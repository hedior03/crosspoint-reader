#include "SystemMetrics.h"

#include <HalMemory.h>
#include <HalPowerManager.h>
#include <esp32-hal-cpu.h>
#include <esp_idf_version.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <type_traits>

#include "SystemMetricsInterval.h"

// Audited IDF kernel: completed idle slices, dual-core tick time slicing, ESP timer.
// Single-core can leave idle running without an accounting switch; report unavailable.
#if ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 5) && CONFIG_IDF_TARGET_ESP32S3 && !CONFIG_FREERTOS_SMP &&    \
    configNUMBER_OF_CORES == 2 && configGENERATE_RUN_TIME_STATS == 1 && INCLUDE_xTaskGetIdleTaskHandle == 1 && \
    CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER && CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U32 &&             \
    configUSE_PREEMPTION == 1 && configUSE_TIME_SLICING == 1 && configUSE_TICKLESS_IDLE == 0
#define SYSTEM_METRICS_IDLE_SUPPORTED 1
#else
#define SYSTEM_METRICS_IDLE_SUPPORTED 0
#endif

namespace {
constexpr uint64_t FAST_INTERVAL_US = 1000000;
constexpr uint64_t BATTERY_INTERVAL_US = 45000000;
SystemMetrics::Snapshot published, staging;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
bool started = false;
uint64_t lastCpuUs = 0;
#if SYSTEM_METRICS_IDLE_SUPPORTED
static_assert(std::is_same_v<configRUN_TIME_COUNTER_TYPE, uint32_t>);
constexpr uint64_t TICK_US = 1000000 / configTICK_RATE_HZ;
struct Baseline {
  uint64_t atUs = 0;
  uint64_t readUs = 0;
  uint32_t idle = 0;
  bool valid = false;
} baselines[2];
#endif
static_assert(std::is_trivially_copyable_v<SystemMetrics::Snapshot>);
static_assert(sizeof(SystemMetrics::Snapshot) <= 192);

bool due(uint64_t now, uint64_t previous, uint64_t interval) { return now < previous || now - previous >= interval; }

void sampleCpu() {
  staging.coreCount = configNUMBER_OF_CORES;
#if SYSTEM_METRICS_IDLE_SUPPORTED
  for (unsigned core = 0; core < 2; ++core) {
    auto& value = staging.cores[core];
    auto& previous = baselines[core];
    const uint64_t before = esp_timer_get_time();
    const uint32_t idle = ulTaskGetIdleRunTimeCounterForCore(core);
    const uint64_t now = esp_timer_get_time();
    const uint64_t readUs = now - before;
    const bool readable = now >= before && readUs <= TICK_US && (before >> 32) == (now >> 32);
    value.sampledAtUs = now;
    value.intervalUs = now >= previous.atUs ? now - previous.atUs : 0;
    value.busyBasisPoints = 0;
    // The kernel skips the running slice at runtime-clock wrap, so discard that interval.
    value.valid = previous.valid && readable && (now >> 32) == (previous.atUs >> 32) &&
                  SystemMetricsInterval::busy(previous.idle, idle, value.intervalUs, TICK_US + previous.readUs + readUs,
                                              value.busyBasisPoints);
    previous = {now, readUs, idle, readable};
  }
#endif
  staging.frequencyMhz = getCpuFrequencyMhz();
  staging.frequencyKnown = staging.frequencyMhz != 0;
  staging.frequencyAtUs = esp_timer_get_time();
  lastCpuUs = staging.frequencyAtUs;
}

void sampleMemory() {
  const auto internal = HalMemory::getInternalHeap();
  const auto psram = HalMemory::getPsramHeap();
  staging.internal = {static_cast<uint32_t>(internal.freeBytes), static_cast<uint32_t>(internal.minFreeBytes),
                      static_cast<uint32_t>(internal.largestBlockBytes), internal.totalBytes != 0};
  staging.psram = {static_cast<uint32_t>(psram.freeBytes), static_cast<uint32_t>(psram.minFreeBytes),
                   static_cast<uint32_t>(psram.largestBlockBytes), psram.totalBytes != 0};
  staging.memoryAtUs = esp_timer_get_time();
  staging.memorySampled = true;
}

void sampleBattery() {
  const auto battery = powerManager.getBatteryStatus();
  staging.batteryMvKnown = battery.millivoltsKnown;
  staging.batteryMv = battery.millivoltsKnown ? battery.millivolts : 0;
  staging.batteryPercentKnown = battery.percentageKnown;
  staging.batteryPercent = battery.percentageKnown ? battery.percentage : 0;
  staging.externalPowerKnown = battery.externalPowerKnown;
  staging.externalPower = battery.externalPowerKnown && battery.externalPower;
  staging.batteryAtUs = esp_timer_get_time();
  staging.batterySampled = true;
}

void publish() {
  portENTER_CRITICAL(&snapshotMux);
  published = staging;
  portEXIT_CRITICAL(&snapshotMux);
}
}  // namespace

SystemMetrics& SystemMetrics::getInstance() {
  static SystemMetrics instance;
  return instance;
}

void SystemMetrics::begin() {
  if (started) return;
  sampleMemory();
  sampleBattery();
  sampleCpu();
  publish();
  started = true;
}

void SystemMetrics::update() {
  if (!started) return;
  bool changed = false;
  if (due(esp_timer_get_time(), lastCpuUs, FAST_INTERVAL_US)) {
    sampleCpu();
    changed = true;
  }
  if (due(esp_timer_get_time(), staging.memoryAtUs, FAST_INTERVAL_US)) {
    sampleMemory();
    changed = true;
  }
  if (due(esp_timer_get_time(), staging.batteryAtUs, BATTERY_INTERVAL_US)) {
    sampleBattery();
    changed = true;
  }
  if (changed) publish();
}

SystemMetrics::Snapshot SystemMetrics::snapshot() const {
  portENTER_CRITICAL(&snapshotMux);
  const Snapshot value = published;
  portEXIT_CRITICAL(&snapshotMux);
  return value;
}
