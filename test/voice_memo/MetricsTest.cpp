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
  assert(headerColumns == 74);  // 5 base + 6 counters * 8 + 10 environment + 11 system.

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

  SystemMetrics::Snapshot system;
  system.batteryMvKnown = system.batteryPercentKnown = system.externalPowerKnown = true;
  system.batteryMv = 3900;
  system.batteryPercent = 61;
  system.externalPower = true;
  system.internal.available = true;
  system.internal.freeBytes = 200000;
  metrics.beginInterval(10000000);
  metrics.noteSamples(1);
  row = metrics.takeRow(10000000, true, system);
  assert(row && std::string(row).find(",3900,61,1,200000,") != std::string::npos);

  // Cached pre-session readings keep negative session-relative timestamps on every copy.
  metrics.startSession(20000000);
  system.coreCount = 2;
  system.cores[0] = {19500000, 1000000, 0, true};
  system.cores[1] = {19500010, 1000010, 10000, true};
  system.frequencyKnown = system.memorySampled = system.batterySampled = true;
  system.frequencyMhz = 80;
  system.frequencyAtUs = 19500020;
  system.memoryAtUs = 19500030;
  system.batteryAtUs = 1000000;
  row = metrics.takeRow(21000000, false, system);
  const std::string suffix = ",2,0,10000,1000000,1000010,-500000,-499990,80,-499980,-499970,-19000000\n";
  assert(row && std::string(row).ends_with(suffix));
  row = metrics.takeRow(22000000, false, system);
  assert(row && std::string(row).ends_with(suffix));
  assert(columnCount(row) == 74);
  system.cores[0].valid = system.cores[1].valid = false;
  system.frequencyKnown = false;
  row = metrics.takeRow(23000000, false, system);
  assert(row && std::string(row).ends_with(",2,,,,,,,,,-499970,-19000000\n"));

  // Full integer widths fit even when all histogram/count fields reach their type maxima.
  // 5 base fields + 48 timing fields + 10 environment + 11 system, commas, LF and NUL.
  const size_t worst = 2 * 20 + 2 * 10 + 5 + 4 + 6 * (6 * 10 + 2 * 20 + 8) + 10 * (10 + 1) + (3 + 1) + 2 * (5 + 1) +
                       4 * (20 + 1) + (10 + 1) + 3 * (20 + 1) + 2;
  assert(worst == VoiceMemoMetricsConfig::ROW_BOUND && worst == 1003);
  assert(worst <= VoiceMemoMetricsConfig::ROW_MAX && worst <= VoiceMemoMetricsConfig::BATCH_BYTES);
  metrics.startSession(0);
  system.internal = system.psram = {UINT32_MAX, UINT32_MAX, UINT32_MAX, true};
  system.frequencyKnown = true;
  system.frequencyMhz = UINT32_MAX;
  system.cores[0] = system.cores[1] = {INT64_MAX, UINT32_MAX, 10000, true};
  system.frequencyAtUs = system.memoryAtUs = system.batteryAtUs = INT64_MAX;
  metrics.addFilter(UINT64_MAX);
  metrics.addEncode(UINT64_MAX);
  metrics.addWrite(UINT64_MAX);
  metrics.addBatch(UINT64_MAX);
  metrics.addRollover(UINT64_MAX);
  metrics.addDiag(UINT64_MAX);
  metrics.noteSamples(UINT32_MAX);
  row = metrics.takeRow(INT64_MAX, true, system);
  assert(row && columnCount(row) == 74 && std::strlen(row) + 1 <= worst);

  std::puts(
      "Metrics checks passed: histogram edges, interval reset, delayed/missed intervals, partial flush and "
      "unknown environment.");
}
