#pragma once

#include <cstdint>

// Foreground PaperMono green/blue control. Red remains owned by the PMIC.
// Call begin/setColor/end from the activity task while holding RenderLock;
// ActivityManager already holds it when calling an activity's onExit().
class HalLed {
 public:
  enum class Color : uint8_t { Off, Green, Blue, Both };

  HalLed() = default;
  HalLed(const HalLed&) = delete;
  HalLed& operator=(const HalLed&) = delete;

  static bool supported();
  bool begin();
  bool setColor(Color color);
  bool end();

 private:
  enum class State : uint8_t { Idle, Ready, Fault };
  State state = State::Idle;
};
