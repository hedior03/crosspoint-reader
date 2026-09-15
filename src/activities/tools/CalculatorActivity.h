#pragma once

#include <BoardConfig.h>

#if FREEINK_DEVICE_PAPERMONO
#include "CalculatorInput.h"
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
  void buildScreen(UiScreen& screen);
  void activateKey(int16_t value);

  CalculatorInput input;
  freeink::ui::KeyboardKey keys[11]{};
  freeink::ui::KeyboardRow rows[4]{};
  freeink::ui::KeyboardLayout layout{rows, 4};
  freeink::ui::KeyboardNavigator navigator;
  freeink::ui::KeyboardProps keyboard;
};
#endif
