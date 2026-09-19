#pragma once

#include <cstdint>

class SystemMetrics {
 public:
  struct Core {
    uint64_t sampledAtUs = 0;
    uint64_t intervalUs = 0;
    uint16_t busyBasisPoints = 0;
    bool valid = false;
  };
  struct Heap {
    uint32_t freeBytes = 0;
    uint32_t minFreeBytes = 0;
    uint32_t largestBlockBytes = 0;
    bool available = false;
  };
  struct Snapshot {
    Core cores[2];
    uint64_t frequencyAtUs = 0;
    uint64_t memoryAtUs = 0;
    uint64_t batteryAtUs = 0;
    Heap internal, psram;
    uint32_t frequencyMhz = 0;
    uint16_t batteryMv = 0;
    uint16_t batteryPercent = 0;
    uint8_t coreCount = 0;
    bool frequencyKnown = false;
    bool memorySampled = false;
    bool batterySampled = false;
    bool batteryMvKnown = false;
    bool batteryPercentKnown = false;
    bool externalPowerKnown = false;
    bool externalPower = false;
  };

  static SystemMetrics& getInstance();
  // Main task only, after HAL initialization. Repeated begin() is a no-op.
  void begin();
  void update();
  // Task context only. Copies cached data; never samples hardware.
  Snapshot snapshot() const;

 private:
  SystemMetrics() = default;
  SystemMetrics(const SystemMetrics&) = delete;
  SystemMetrics& operator=(const SystemMetrics&) = delete;
};
