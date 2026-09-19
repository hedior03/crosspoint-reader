#pragma once
#include <cstdint>
class HalPowerManager {
 public:
  struct BatteryStatus {
    bool millivoltsKnown = false;
    uint16_t millivolts = 0;
    bool percentageKnown = false;
    uint16_t percentage = 0;
    bool externalPowerKnown = false;
    bool externalPower = false;
  };
  BatteryStatus getBatteryStatus() const;
};
extern HalPowerManager powerManager;
