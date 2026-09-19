// Exercises the real singleton with deterministic HAL/clock/kernel substitutes.
#include <HalMemory.h>
#include <HalPowerManager.h>
#include <SystemMetrics.h>
#include <SystemMetricsInterval.h>

#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>

namespace {
uint64_t now = 100;
uint32_t idle[2]{};
unsigned idleReads = 0, heapReads = 0, batteryReads = 0, frequencyReads = 0;
uint32_t readDelay = 0, mhz = 240;
bool psramPresent = true, batteryOk = true, externalOnly = false;
uint32_t heapFree = 12345;
}  // namespace
HalPowerManager powerManager;
int64_t esp_timer_get_time() { return now; }
uint32_t getCpuFrequencyMhz() {
  ++frequencyReads;
  return mhz;
}
uint32_t ulTaskGetIdleRunTimeCounterForCore(unsigned core) {
  ++idleReads;
  now += readDelay;
  return idle[core];
}
HalMemory::HeapStats HalMemory::getInternalHeap() {
  ++heapReads;
  return {heapFree, 100000, 123, heapFree};
}
HalMemory::HeapStats HalMemory::getPsramHeap() {
  ++heapReads;
  return psramPresent ? HeapStats{0, 8000000, 0, 0} : HeapStats{};
}
HalPowerManager::BatteryStatus HalPowerManager::getBatteryStatus() const {
  ++batteryReads;
  if (externalOnly) return BatteryStatus{false, 0, false, 0, true, true};
  return batteryOk ? BatteryStatus{true, 3900, true, 60, true, false} : BatteryStatus{};
}

int main() {
  uint16_t busy = 123;
  assert(!SystemMetricsInterval::busy(0, 0, 0, 0, busy));
  assert(!SystemMetricsInterval::busy(0, 0, uint64_t{1} << 32, 0, busy));
  assert(!SystemMetricsInterval::busy(0, 1000002, 1000000, 1, busy));
  assert(SystemMetricsInterval::busy(0, 1000001, 1000000, 1, busy) && busy == 0);
  assert(SystemMetricsInterval::busy(UINT32_MAX - 499, 500, 2000, 0, busy) && busy == 5000);
  assert(!SystemMetricsInterval::busy(1000000, 0, 1000000, 0, busy));

  auto& metrics = SystemMetrics::getInstance();
  metrics.update();
  assert(heapReads == 0 && batteryReads == 0);
  assert(!metrics.snapshot().memorySampled);
  metrics.begin();
  auto value = metrics.snapshot();
  assert(!value.cores[0].valid && !value.cores[1].valid);
  assert(value.memorySampled && value.batterySampled && value.frequencyMhz == 240);
  assert(value.psram.available && value.psram.freeBytes == 0);
  assert(value.memoryAtUs == 100 && value.batteryAtUs == 100);
  for (int i = 0; i < 10; ++i) {
    metrics.snapshot();
    metrics.update();
    metrics.begin();
  }
  assert(heapReads == 2 && batteryReads == 1 && frequencyReads == 1);

  now += 2500000;  // One overdue sample; no catch-up loop.
  idle[0] = 2500000;
  idle[1] = 625000;
  metrics.update();
  value = metrics.snapshot();
#ifndef METRICS_UNSUPPORTED
  assert(value.coreCount == 2);
  assert(value.cores[0].valid && value.cores[0].busyBasisPoints == 0);
  assert(value.cores[1].valid && value.cores[1].busyBasisPoints == 7500);
  assert(value.cores[0].intervalUs == 2500000 && value.cores[1].intervalUs == 2500000);
#else
  assert(value.coreCount == 1 && idleReads == 0);
  assert(!value.cores[0].valid && !value.cores[1].valid);
#endif
  metrics.update();
  assert(frequencyReads == 2 && heapReads == 4 && batteryReads == 1);
  assert(value.batteryAtUs == 100);
  now += 1000000;
  metrics.update();
#ifndef METRICS_UNSUPPORTED
  assert(metrics.snapshot().cores[0].busyBasisPoints == 10000);
#endif

  now = 45000100;
  batteryOk = false;
  psramPresent = false;
  mhz = 80;
  metrics.update();
  value = metrics.snapshot();
  assert(batteryReads == 2 && !value.batteryMvKnown && !value.batteryPercentKnown && !value.externalPowerKnown);
  assert(value.batteryMv == 0 && value.batteryAtUs == now);
  assert(!value.psram.available && value.frequencyMhz == 80);
  now += 1000000;
  batteryOk = true;
  metrics.update();
  assert(batteryReads == 2 && metrics.snapshot().batteryAtUs == 45000100);

#ifndef METRICS_UNSUPPORTED
  idle[0] += 2000000;  // Invalid delta, then automatic recovery from the new baseline.
  now += 1000000;
  metrics.update();
  assert(!metrics.snapshot().cores[0].valid);
  now += 1000000;
  metrics.update();
  assert(metrics.snapshot().cores[0].valid);
  readDelay = 2000;  // Delayed query cannot silently become a precise endpoint.
  now += 1000000;
  metrics.update();
  assert(!metrics.snapshot().cores[0].valid);
  readDelay = 0;
  now += 1000000;
  metrics.update();
  assert(!metrics.snapshot().cores[0].valid);
  now += 1000000;
  metrics.update();
  assert(metrics.snapshot().cores[0].valid);

  now = (uint64_t{1} << 32) - 500000;
  metrics.update();
  now += 1000000;
  metrics.update();
  assert(!metrics.snapshot().cores[0].valid);  // SDK discards a runtime-clock wrap slice.
  now += 1000000;
  metrics.update();
  assert(metrics.snapshot().cores[0].valid);
  now += uint64_t{1} << 32;
  metrics.update();
  assert(!metrics.snapshot().cores[0].valid);
  now = 1;  // Unsupported clock reset rebaselines, never unsigned fake uptime.
  metrics.update();
  assert(!metrics.snapshot().cores[0].valid);
  now += 1000000;
  metrics.update();
  assert(metrics.snapshot().cores[0].valid);
#endif

  now += 45000000;
  externalOnly = true;
  mhz = 0;
  metrics.update();
  value = metrics.snapshot();
  assert(!value.batteryMvKnown && !value.batteryPercentKnown);
  assert(value.externalPowerKnown && value.externalPower && !value.frequencyKnown);
  const auto attemptedAt = value.batteryAtUs;
  const unsigned readsBefore = batteryReads;
  now += 10000000;
  for (unsigned i = 0; i < 10; ++i) {
    value = metrics.snapshot();
    assert(value.batteryAtUs == attemptedAt && batteryReads == readsBefore);
  }
#ifndef METRICS_UNSUPPORTED
  const auto previous = metrics.snapshot();
  readDelay = 10;
  metrics.update();
  value = metrics.snapshot();
  assert(value.cores[1].sampledAtUs == value.cores[0].sampledAtUs + 10);
  for (unsigned core = 0; core < 2; ++core)
    assert(value.cores[core].intervalUs == value.cores[core].sampledAtUs - previous.cores[core].sampledAtUs);
  readDelay = 0;
#endif

  // Real host threads test publication, not ESP32's lock implementation.
  std::atomic<bool> done{false};
  std::thread consumer([&] {
    while (!done.load()) {
      const auto copy = metrics.snapshot();
      assert(copy.internal.freeBytes == copy.internal.largestBlockBytes);
    }
  });
  for (unsigned i = 0; i < 10000; ++i) {
    heapFree = i;
    now += 1000000;
    metrics.update();
  }
  done = true;
  consumer.join();
  std::printf("SystemMetrics checks passed; snapshot=%zu bytes\n", sizeof(SystemMetrics::Snapshot));
}
