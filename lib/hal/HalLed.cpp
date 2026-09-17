#include "HalLed.h"

#include <BoardConfig.h>

#if FREEINK_DEVICE_PAPERMONO && FREEINK_CAP_LED
#include <M5Ioe1.h>

namespace {
namespace ioe = freeink::m5ioe1;
constexpr uint16_t LED_MASK = ioe::PIN_LED_GREEN | ioe::PIN_LED_BLUE;
constexpr uint16_t PWM_ENABLED = 1u << 15;

bool writeChannels(const uint16_t channels) {
  uint16_t output = 0;
  return ioe::updateReg16(ioe::REG_GPIO_OUT_L, LED_MASK, channels) && ioe::readReg16(ioe::REG_GPIO_OUT_L, &output) &&
         (output & LED_MASK) == channels;
}
}  // namespace
#endif

bool HalLed::supported() {
#if FREEINK_DEVICE_PAPERMONO && FREEINK_CAP_LED
  return true;
#else
  return false;
#endif
}

bool HalLed::begin() {
#if FREEINK_DEVICE_PAPERMONO && FREEINK_CAP_LED
  if (state != State::Idle) return state == State::Ready;

  // Normal board bring-up owns bus, mode and PWM configuration. Never redo it
  // here: the same output bank powers the SD card, display and touch controller.
  if (ioe::g_addr == 0 || ioe::g_addr == 0xFF) return false;
  uint16_t mode = 0;
  uint16_t drive = 0;
  uint16_t bluePwm = 0;
  uint16_t greenPwm = 0;
  if (!ioe::readReg16(ioe::REG_GPIO_MODE_L, &mode) || !ioe::readReg16(ioe::REG_GPIO_DRV_L, &drive) ||
      !ioe::readReg16(ioe::REG_PWM1_DUTY_L, &bluePwm) || !ioe::readReg16(ioe::REG_PWM2_DUTY_L, &greenPwm) ||
      (mode & LED_MASK) != LED_MASK || (drive & LED_MASK) != 0 || ((bluePwm | greenPwm) & PWM_ENABLED) != 0) {
    return false;
  }
  state = State::Ready;
  return setColor(Color::Off);
#else
  return false;
#endif
}

bool HalLed::setColor(const Color color) {
#if FREEINK_DEVICE_PAPERMONO && FREEINK_CAP_LED
  if (state != State::Ready) return false;
  uint16_t channels = 0;
  switch (color) {
    case Color::Off:
      break;
    case Color::Green:
      channels = ioe::PIN_LED_GREEN;
      break;
    case Color::Blue:
      channels = ioe::PIN_LED_BLUE;
      break;
    case Color::Both:
      channels = LED_MASK;
      break;
    default:
      return false;
  }
  if (writeChannels(channels)) return true;
  state = State::Fault;
  // A lost acknowledgement/readback can leave a channel on. Attempt cleanup,
  // but retain the fault even if cleanup succeeds; do not infer optical state.
  writeChannels(0);
  return false;
#else
  (void)color;
  return false;
#endif
}

bool HalLed::end() {
#if FREEINK_DEVICE_PAPERMONO && FREEINK_CAP_LED
  if (state == State::Idle) return true;
  const bool cleared = writeChannels(0);
  state = cleared ? State::Idle : State::Fault;
  return cleared;
#else
  return true;
#endif
}
