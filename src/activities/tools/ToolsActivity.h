#pragma once

#include <BoardConfig.h>

#if FREEINK_DEVICE_PAPERMONO
#include "activities/UiListActivity.h"

class ToolsActivity final : public UiListActivity {
 public:
  ToolsActivity(GfxRenderer& renderer, MappedInputManager& input) : UiListActivity("Tools", renderer, input) {}

 private:
  int listCount() const override { return 1; }
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override { activityManager.goHome(HomeMenuItem::TOOLS); }
  freeink::ui::ListItem calculatorRow{};
};
#endif
