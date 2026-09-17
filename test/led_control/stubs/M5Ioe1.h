#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace freeink::m5ioe1 {
constexpr uint8_t REG_GPIO_MODE_L = 0x03;
constexpr uint8_t REG_GPIO_OUT_L = 0x05;
constexpr uint8_t REG_GPIO_DRV_L = 0x13;
constexpr uint8_t REG_PWM1_DUTY_L = 0x1B;
constexpr uint8_t REG_PWM2_DUTY_L = 0x1D;
constexpr uint16_t PIN_LED_GREEN = 0x0080;
constexpr uint16_t PIN_LED_BLUE = 0x0100;
inline uint8_t g_addr = 0x4F;
}  // namespace freeink::m5ioe1

namespace fakeLed {
struct Update {
  uint8_t reg;
  uint16_t clearMask;
  uint16_t setMask;
};

inline std::array<uint16_t, 256> registers{};
inline std::vector<Update> updates;
inline int reads = 0;
inline int failReadAt = 0;
inline int failedUpdatesRemaining = 0;
inline bool corruptNextOutputRead = false;

inline void reset() {
  using namespace freeink::m5ioe1;
  registers.fill(0);
  registers[REG_GPIO_MODE_L] = 0xFFFF;
  registers[REG_GPIO_OUT_L] = 0xA6D5;
  updates.clear();
  updates.reserve(32);
  reads = 0;
  failReadAt = 0;
  failedUpdatesRemaining = 0;
  corruptNextOutputRead = false;
  g_addr = 0x4F;
}
}  // namespace fakeLed

namespace freeink::m5ioe1 {
inline bool readReg16(uint8_t reg, uint16_t* value) {
  if (++fakeLed::reads == fakeLed::failReadAt) return false;
  *value = fakeLed::registers[reg];
  if (reg == REG_GPIO_OUT_L && fakeLed::corruptNextOutputRead) {
    *value ^= PIN_LED_GREEN;
    fakeLed::corruptNextOutputRead = false;
  }
  return true;
}

inline bool updateReg16(uint8_t reg, uint16_t clearMask, uint16_t setMask) {
  fakeLed::updates.push_back({reg, clearMask, setMask});
  if (fakeLed::failedUpdatesRemaining > 0) {
    --fakeLed::failedUpdatesRemaining;
    return false;
  }
  auto& value = fakeLed::registers[reg];
  value = static_cast<uint16_t>((value & ~clearMask) | setMask);
  return true;
}
}  // namespace freeink::m5ioe1
