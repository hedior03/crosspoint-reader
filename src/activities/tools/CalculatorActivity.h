#pragma once

#include <BoardConfig.h>

#if FREEINK_DEVICE_PAPERMONO
#include "CalculatorFeedback.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"

class CalculatorActivity final : public Activity, private UiAppHost {
 public:
  CalculatorActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("Calculator", renderer, input), UiAppHost(renderer) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr freeink::ui::ActionId ACTION_KEY = 1;
  static constexpr int16_t CLEAR = 10;
  static constexpr int16_t DECIMAL = 11;
  void buildScreen(UiScreen& screen);
  void activateKey(int16_t value);

  void publishFeedback(int16_t pressed = -1);
  CalculatorState calculator;
  CalculatorFeedback feedback;
  CalculatorFeedback::Frame displayed;
  portMUX_TYPE feedbackMux = portMUX_INITIALIZER_UNLOCKED;
  freeink::ui::KeyboardKey keys[19]{};
  freeink::ui::KeyboardRow rows[5]{};
  freeink::ui::KeyboardLayout layout{rows, 5};
  freeink::ui::KeyboardNavigator navigator;
  freeink::ui::KeyboardProps keyboard;
};
#endif
