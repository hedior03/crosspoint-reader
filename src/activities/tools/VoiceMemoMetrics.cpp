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

void appendEnvironment(char* row, size_t rowSize, size_t& used, const std::atomic<int32_t>& value) {
  const int32_t current = value.load(std::memory_order_relaxed);
  if (current < 0)
    append(row, rowSize, used, ",");
  else
    append(row, rowSize, used, ",%ld", static_cast<long>(current));
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

const char* VoiceMemoMetrics::takeRow(uint64_t nowUs, bool final) {
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
  appendEnvironment(row_, sizeof(row_), used, environment_.batteryMv);
  appendEnvironment(row_, sizeof(row_), used, environment_.batteryPct);
  appendEnvironment(row_, sizeof(row_), used, environment_.externalPower);
  appendEnvironment(row_, sizeof(row_), used, environment_.internalFree);
  appendEnvironment(row_, sizeof(row_), used, environment_.internalMin);
  appendEnvironment(row_, sizeof(row_), used, environment_.internalLargest);
  appendEnvironment(row_, sizeof(row_), used, environment_.psramFree);
  appendEnvironment(row_, sizeof(row_), used, environment_.psramMin);
  appendEnvironment(row_, sizeof(row_), used, environment_.writerStackFree);
  appendEnvironment(row_, sizeof(row_), used, environment_.captureStackFree);

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
         "psram_free,psram_min,writer_stack_free,capture_stack_free\n";
}
