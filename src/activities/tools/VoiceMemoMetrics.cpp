#include "VoiceMemoMetrics.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
namespace cfg = VoiceMemoMetricsConfig;

// Bounded append: truncates rather than overflowing the row buffer.
void append(char* row, size_t rowSize, size_t& used, const char* format, ...) {
  if (used >= rowSize) return;
  va_list args;
  va_start(args, format);
  const int written = vsnprintf(row + used, rowSize - used, format, args);
  va_end(args);
  if (written < 0) return;
  const size_t room = rowSize - used;
  used += std::min(static_cast<size_t>(written), room - 1);
}

void appendCounter(char* row, size_t rowSize, size_t& used, const VoiceMemoCounter& counter) {
  append(row, rowSize, used, ",%u,%llu,%llu,%u,%u,%u,%u,%u", static_cast<unsigned>(counter.count),
         static_cast<unsigned long long>(counter.totalUs), static_cast<unsigned long long>(counter.maxUs),
         static_cast<unsigned>(counter.bins[0]), static_cast<unsigned>(counter.bins[1]),
         static_cast<unsigned>(counter.bins[2]), static_cast<unsigned>(counter.bins[3]),
         static_cast<unsigned>(counter.bins[4]));
}

void appendValue(char* row, size_t rowSize, size_t& used, bool known, uint64_t value) {
  if (known)
    append(row, rowSize, used, ",%llu", static_cast<unsigned long long>(value));
  else
    append(row, rowSize, used, ",");
}

void appendTime(char* row, size_t rowSize, size_t& used, bool sampled, uint64_t atUs, uint64_t startUs) {
  if (sampled)
    append(row, rowSize, used, ",%lld", static_cast<long long>(atUs) - static_cast<long long>(startUs));
  else
    append(row, rowSize, used, ",");
}

void appendStack(char* row, size_t rowSize, size_t& used, const std::atomic<int32_t>& value) {
  const int32_t current = value.load(std::memory_order_relaxed);
  appendValue(row, rowSize, used, current >= 0, current);
}
}  // namespace

void VoiceMemoCounter::add(uint64_t us) {
  ++count;
  totalUs += us;
  if (us > maxUs) maxUs = us;
  ++bins[us < 1000 ? 0 : us < 10000 ? 1 : us < 100000 ? 2 : us < 1000000 ? 3 : 4];
}

void VoiceMemoCounter::reset() {
  count = 0;
  totalUs = 0;
  maxUs = 0;
  std::memset(bins, 0, sizeof(bins));
}

uint16_t VoiceMemoMetrics::acquireBuffer() {
  auto state = bufferState_.load(std::memory_order_relaxed);
  for (;;) {
    const uint32_t held = static_cast<uint16_t>(state) + 1u;
    const uint32_t next = (std::max(state >> 16, held) << 16) | held;
    if (bufferState_.compare_exchange_weak(state, next, std::memory_order_relaxed)) return held;
  }
}

uint16_t VoiceMemoMetrics::takeBufferHighWater() {
  auto state = bufferState_.load(std::memory_order_relaxed);
  for (;;) {
    const uint32_t held = static_cast<uint16_t>(state);
    const uint32_t next = (held << 16) | held;
    if (bufferState_.compare_exchange_weak(state, next, std::memory_order_relaxed)) return state >> 16;
  }
}

void VoiceMemoMetrics::startSession(uint64_t nowUs) {
  sessionStartUs_ = nowUs;
  beginInterval(nowUs);
}

void VoiceMemoMetrics::beginInterval(uint64_t nowUs) {
  intervalStartUs_ = nowUs;
  active_ = true;
  takeBufferHighWater();
  resetActive();
}

void VoiceMemoMetrics::resetActive() {
  filter_.reset();
  encode_.reset();
  write_.reset();
  rollover_.reset();
  diag_.reset();
  batch_.reset();
  samples_ = 0;
}

const char* VoiceMemoMetrics::takeRow(uint64_t nowUs, bool final, const SystemMetrics::Snapshot& system) {
  if (!active_) return nullptr;
  const uint64_t elapsed = nowUs - intervalStartUs_;
  if (!final && elapsed < cfg::INTERVAL_US) return nullptr;
  const bool hasData = samples_ != 0 || !filter_.empty() || !encode_.empty() || !write_.empty() || !batch_.empty() ||
                       !rollover_.empty() || !diag_.empty();
  // A final flush with no elapsed time and no work has nothing to report.
  if (final && !hasData && elapsed == 0) return nullptr;

  const uint64_t elapsedMs = elapsed / 1000;
  const uint32_t missed = elapsed >= cfg::INTERVAL_US ? static_cast<uint32_t>(elapsed / cfg::INTERVAL_US) - 1 : 0;
  size_t used = 0;
  append(row_, sizeof(row_), used, "%llu,%llu,%u,%u,%u",
         static_cast<unsigned long long>((nowUs - sessionStartUs_) / 1000), static_cast<unsigned long long>(elapsedMs),
         static_cast<unsigned>(missed), static_cast<unsigned>(samples_), static_cast<unsigned>(takeBufferHighWater()));
  appendCounter(row_, sizeof(row_), used, filter_);
  appendCounter(row_, sizeof(row_), used, encode_);
  appendCounter(row_, sizeof(row_), used, write_);
  appendCounter(row_, sizeof(row_), used, batch_);
  appendCounter(row_, sizeof(row_), used, rollover_);
  appendCounter(row_, sizeof(row_), used, diag_);
  appendValue(row_, sizeof(row_), used, system.batteryMvKnown, system.batteryMv);
  appendValue(row_, sizeof(row_), used, system.batteryPercentKnown, system.batteryPercent);
  appendValue(row_, sizeof(row_), used, system.externalPowerKnown, system.externalPower);
  appendValue(row_, sizeof(row_), used, system.internal.available, system.internal.freeBytes);
  appendValue(row_, sizeof(row_), used, system.internal.available, system.internal.minFreeBytes);
  appendValue(row_, sizeof(row_), used, system.internal.available, system.internal.largestBlockBytes);
  appendValue(row_, sizeof(row_), used, system.psram.available, system.psram.freeBytes);
  appendValue(row_, sizeof(row_), used, system.psram.available, system.psram.minFreeBytes);
  appendStack(row_, sizeof(row_), used, environment_.writerStackFree);
  appendStack(row_, sizeof(row_), used, environment_.captureStackFree);
  appendValue(row_, sizeof(row_), used, system.coreCount != 0, system.coreCount);
  for (const auto& core : system.cores) appendValue(row_, sizeof(row_), used, core.valid, core.busyBasisPoints);
  for (const auto& core : system.cores) appendValue(row_, sizeof(row_), used, core.valid, core.intervalUs);
  for (const auto& core : system.cores)
    appendTime(row_, sizeof(row_), used, core.valid, core.sampledAtUs, sessionStartUs_);
  appendValue(row_, sizeof(row_), used, system.frequencyKnown, system.frequencyMhz);
  appendTime(row_, sizeof(row_), used, system.frequencyKnown, system.frequencyAtUs, sessionStartUs_);
  appendTime(row_, sizeof(row_), used, system.memorySampled, system.memoryAtUs, sessionStartUs_);
  appendTime(row_, sizeof(row_), used, system.batterySampled, system.batteryAtUs, sessionStartUs_);

  intervalStartUs_ = nowUs;
  const bool truncated = used + 1 >= sizeof(row_);
  resetActive();
  if (truncated) return nullptr;  // Drop an over-long row rather than persist a partial line.
  row_[used] = '\n';
  row_[used + 1] = '\0';
  return row_;
}

const char* VoiceMemoMetrics::csvHeader() {
  return "t_ms,elapsed_ms,missed,samples,buffer_high,"
         "filter_n,filter_us,filter_max_us,filter_b0,filter_b1,filter_b2,filter_b3,filter_b4,"
         "encode_n,encode_us,encode_max_us,encode_b0,encode_b1,encode_b2,encode_b3,encode_b4,"
         "write_n,write_us,write_max_us,write_b0,write_b1,write_b2,write_b3,write_b4,"
         "batch_n,batch_us,batch_max_us,batch_b0,batch_b1,batch_b2,batch_b3,batch_b4,"
         "rollover_n,rollover_us,rollover_max_us,rollover_b0,rollover_b1,rollover_b2,rollover_b3,rollover_b4,"
         "diag_n,diag_us,diag_max_us,diag_b0,diag_b1,diag_b2,diag_b3,diag_b4,"
         "battery_mv,battery_pct,external_power,"
         "heap_internal_free,heap_internal_min,heap_internal_largest,"
         "psram_free,psram_min,writer_stack_free,capture_stack_free,"
         "cpu_cores,cpu0_busy_bp,cpu1_busy_bp,cpu0_interval_us,cpu1_interval_us,cpu0_t_us,cpu1_t_us,"
         "cpu_mhz,cpu_frequency_t_us,memory_t_us,battery_t_us\n";
}
