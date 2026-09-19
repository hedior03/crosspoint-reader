#pragma once
#include <SystemMetrics.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace VoiceMemoMetricsConfig {
inline constexpr uint64_t INTERVAL_US = 1000000;      // Scheduled interval report length.
inline constexpr size_t HISTOGRAM_BUCKETS = 5;        // <1ms, <10ms, <100ms, <1s, >=1s.
inline constexpr size_t ROW_MAX = 1024;               // Bounds one formatted CSV row.
inline constexpr size_t BATCH_BYTES = 2048;           // Bounds rows buffered before one SD write.
inline constexpr uint32_t BATCH_ROWS = 4;             // Flush target while the audio backlog is empty.
inline constexpr uint32_t STACK_INTERVAL_MS = 45000;  // Recorder task-stack sampling cadence.
// Conservative decimal widths: base, six counters, ten environment fields, eleven new fields, LF/NUL.
inline constexpr size_t ROW_BOUND = 69 + 6 * 108 + 10 * 11 + 174 + 2;
static_assert(ROW_BOUND <= ROW_MAX && ROW_MAX <= BATCH_BYTES);
}  // namespace VoiceMemoMetricsConfig

// Writer-task-only timing accumulator: count, total, maximum and fixed latency buckets.
struct VoiceMemoCounter {
  uint32_t count = 0;
  uint64_t totalUs = 0;
  uint64_t maxUs = 0;
  uint32_t bins[VoiceMemoMetricsConfig::HISTOGRAM_BUCKETS]{};
  void add(uint64_t us);
  void reset();
  bool empty() const { return count == 0; }
};

// One-second interval accumulator. Timing counters reset each time a row is taken; the
// task-stack headroom is refreshed by the main task while handles remain valid.
// Negative environment values mean "unknown", never zero.
class VoiceMemoMetrics {
 public:
  struct Environment {
    std::atomic<int32_t> writerStackFree{-1};
    std::atomic<int32_t> captureStackFree{-1};
  };

  void startSession(uint64_t nowUs);
  void beginInterval(uint64_t nowUs);
  void addFilter(uint64_t us) { filter_.add(us); }
  void addEncode(uint64_t us) { encode_.add(us); }
  void addWrite(uint64_t us) { write_.add(us); }
  void addRollover(uint64_t us) { rollover_.add(us); }
  void addDiag(uint64_t us) { diag_.add(us); }
  void addBatch(uint64_t us) { batch_.add(us); }
  void noteSamples(uint32_t samples) { samples_ += samples; }
  // Capture acquires; capture or writer releases before returning a block to the free queue.
  uint16_t acquireBuffer();
  void releaseBuffer() { bufferState_.fetch_sub(1, std::memory_order_relaxed); }

  // Formats a due interval into the internal row buffer. Returns nullptr when no row is
  // due; `final` flushes the remaining partial interval at drain/Pause/Stop.
  const char* takeRow(uint64_t nowUs, bool final, const SystemMetrics::Snapshot& system = {});

  Environment& environment() { return environment_; }
  static const char* csvHeader();

 private:
  void resetActive();
  uint16_t takeBufferHighWater();
  VoiceMemoCounter filter_, encode_, write_, rollover_, diag_, batch_;
  uint32_t samples_ = 0;
  // Low 16 bits: held blocks. High 16 bits: interval maximum. One atomic reset preserves both.
  std::atomic<uint32_t> bufferState_{0};
  uint64_t sessionStartUs_ = 0;
  uint64_t intervalStartUs_ = 0;
  bool active_ = false;
  Environment environment_;
  char row_[VoiceMemoMetricsConfig::ROW_MAX]{};
};
