#include "ToolsActivity.h"

#if FREEINK_DEVICE_PAPERMONO
#include <I18n.h>
#include <Memory.h>

#include "CalculatorActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

const char* ToolsActivity::headerTitle() const { return tr(STR_TOOLS); }

void ToolsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  calculatorRow.label = tr(STR_CALCULATOR);
  calculatorRow.actionValue = 0;
  fui::ListProps props;
  props.items = &calculatorRow;
  props.count = 1;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

void ToolsActivity::activateIndex(int index) {
  if (index != 0) return;
  auto calculator = makeUniqueNoThrow<CalculatorActivity>(renderer, mappedInput);
  if (!calculator) {
    LOG_ERR("TOOLS", "OOM: Calculator activity");
    return;
  }
  app.clearTapFlash();
  activityManager.pushActivity(std::move(calculator));
}
#endif
