#pragma once

#include <HalLed.h>

#include "activities/UiListActivity.h"

class LedControlActivity final : public UiListActivity {
 public:
  explicit LedControlActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;

 private:
  HalLed led;
  freeink::ui::ListItem rowItems[4]{};
  int appliedIndex = -1;
  const char* errorMessage = nullptr;

  void rebuildRowItems();
  int listCount() const override { return 4; }
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  void navigateButtons() override;
  void onBackButton() override;
};
