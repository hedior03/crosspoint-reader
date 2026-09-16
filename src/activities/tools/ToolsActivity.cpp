#include "ToolsActivity.h"

#if FREEINK_DEVICE_PAPERMONO
#include <I18n.h>
#include <Memory.h>

#include "CalculatorActivity.h"
#include "VoiceMemoActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

const char* ToolsActivity::headerTitle() const { return tr(STR_TOOLS); }

void ToolsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  rows[0].label = tr(STR_CALCULATOR);
  rows[0].actionValue = 0;
  rows[1].label = tr(STR_VOICE_MEMO);
  rows[1].actionValue = 1;
  fui::ListProps props;
  props.items = rows;
  props.count = 2;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void ToolsActivity::activateIndex(int index) {
  std::unique_ptr<Activity> next;
  if (index == 0)
    next = makeUniqueNoThrow<CalculatorActivity>(renderer, mappedInput);
  else if (index == 1)
    next = makeUniqueNoThrow<VoiceMemoActivity>(renderer, mappedInput);
  else
    return;
  if (!next) {
    LOG_ERR("TOOLS", "OOM: tool activity");
    return;
  }
  app.clearTapFlash();
  activityManager.pushActivity(std::move(next));
}
#endif
