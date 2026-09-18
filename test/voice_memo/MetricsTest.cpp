// Run with test/voice_memo/run_host_tests.sh. Pure metrics logic, no FreeRTOS or SD.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/activities/tools/VoiceMemoMetrics.h"

static size_t columnCount(const char* line) {
  size_t count = 1;
  for (const char* p = line; *p; ++p)
    if (*p == ',') ++count;
  return count;
}

static void expectPrefix(const char* row, unsigned long tMs, unsigned long elapsedMs, unsigned missed, unsigned samples,
                         unsigned bufferHigh) {
  assert(row);
  unsigned long a = 0, b = 0;
  unsigned c = 0, d = 0, e = 0;
  assert(std::sscanf(row, "%lu,%lu,%u,%u,%u", &a, &b, &c, &d, &e) == 5);
  assert(a == tMs && b == elapsedMs && c == missed && d == samples && e == bufferHigh);
}

int main() {
  // Histogram edges: <1ms, <10ms, <100ms, <1s, >=1s.
  VoiceMemoCounter counter;
  const uint64_t edges[] = {0, 999, 1000, 9999, 10000, 99999, 100000, 999999, 1000000, 5000000};
  for (const uint64_t us : edges) counter.add(us);
  assert(counter.count == 10);
  for (const uint32_t bin : counter.bins) assert(bin == 2);
  assert(counter.maxUs == 5000000);
  uint64_t total = 0;
  for (const uint64_t us : edges) total += us;
  assert(counter.totalUs == total);
  counter.reset();
  assert(counter.count == 0 && counter.totalUs == 0 && counter.maxUs == 0 && counter.bins[0] == 0);

  const size_t headerColumns = columnCount(VoiceMemoMetrics::csvHeader());
  assert(headerColumns == 63);  // 5 base + 6 counters * 8 + 10 environment.

  VoiceMemoMetrics idle;
  assert(idle.takeRow(1000000, false) == nullptr);
  assert(idle.takeRow(1000000, true) == nullptr);

  VoiceMemoMetrics metrics;
  metrics.startSession(0);
  metrics.addFilter(500);
  metrics.addEncode(2000);
  metrics.addWrite(50000);
  metrics.addBatch(200000);
  metrics.addRollover(1500000);
  metrics.addDiag(7000);
  metrics.noteSamples(1024);
  for (int i = 0; i < 5; ++i) assert(metrics.acquireBuffer() == i + 1);
  for (int i = 0; i < 5; ++i) metrics.releaseBuffer();
  const char* row = metrics.takeRow(1000000, false);
  expectPrefix(row, 1000, 1000, 0, 1024, 5);
  assert(columnCount(row) == headerColumns);
  // Taking a row resets the active interval; nothing is due twice.
  assert(metrics.takeRow(1000000, false) == nullptr);
  assert(metrics.takeRow(1000000, true) == nullptr);

  // Capture can fill the pool while the writer is busy, then drain before a report.
  for (int i = 0; i < 32; ++i) metrics.acquireBuffer();
  for (int i = 0; i < 29; ++i) metrics.releaseBuffer();
  expectPrefix(metrics.takeRow(2000000, false), 2000, 1000, 0, 0, 32);
  // Blocks held across an interval boundary remain part of the next interval's peak.
  for (int i = 0; i < 3; ++i) metrics.releaseBuffer();
  expectPrefix(metrics.takeRow(3000000, false), 3000, 1000, 0, 0, 3);
  metrics.beginInterval(4000000);  // Resume after a complete drain starts with zero occupancy.
  expectPrefix(metrics.takeRow(5000000, false), 5000, 1000, 0, 0, 0);

  // A delayed writer reports one longer interval and counts the skipped ones.
  metrics.beginInterval(2000000);
  metrics.noteSamples(10);
  row = metrics.takeRow(7200000, false);
  expectPrefix(row, 7200, 5200, 4, 10, 0);

  // Final flush reports the partial interval with no samples.
  metrics.beginInterval(8000000);
  metrics.addFilter(1000);
  row = metrics.takeRow(8300000, true);
  expectPrefix(row, 8300, 300, 0, 0, 0);
  assert(metrics.takeRow(8300000, true) == nullptr);

  // Unavailable environment is empty, never zero.
  metrics.beginInterval(9000000);
  metrics.noteSamples(1);
  row = metrics.takeRow(9000000, true);
  assert(row && std::string(row).find(",,,,,,,,,,\n") != std::string::npos);

  metrics.environment().batteryMv.store(3900);
  metrics.environment().batteryPct.store(61);
  metrics.environment().externalPower.store(1);
  metrics.environment().internalFree.store(200000);
  metrics.beginInterval(10000000);
  metrics.noteSamples(1);
  row = metrics.takeRow(10000000, true);
  assert(row && std::string(row).find(",3900,61,1,200000,") != std::string::npos);

  std::puts(
      "Metrics checks passed: histogram edges, interval reset, delayed/missed intervals, partial flush and "
      "unknown environment.");
}
