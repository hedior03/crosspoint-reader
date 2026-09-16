#pragma once
#include <Microphone.h>
namespace freeink {
namespace papermono {
inline bool ensureBooted() { return true; }
}  // namespace papermono
namespace m5ioe1 {
inline constexpr uint16_t PIN_MIC_POWER = 1;
inline bool write(uint16_t, bool) { return true; }
}  // namespace m5ioe1
namespace m5pm1 {
inline constexpr uint8_t REG_PWR_CFG = 0, REG_GPIO_DRV = 1, LED_R_EN = 16;
inline uint8_t registers[2] = {4, 255};
inline bool failNextUpdate = false;
inline bool updateReg(uint8_t reg, uint8_t clear, uint8_t set) {
  if (failNextUpdate) {
    failNextUpdate = false;
    return false;
  }
  registers[reg] = (registers[reg] & ~clear) | set;
  return true;
}
}  // namespace m5pm1

}  // namespace freeink
