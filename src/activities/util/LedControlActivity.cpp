#include "LedControlActivity.h"

#include <I18n.h>

#include "components/UITheme.h"

namespace fui = freeink::ui;

LedControlActivity::LedControlActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("LedControl", renderer, mappedInput) {}

void LedControlActivity::onEnter() {
  RenderLock lock(*this);
  appliedIndex = -1;
  errorMessage = nullptr;
  if (!HalLed::supported()) {
    errorMessage = tr(STR_LED_UNAVAILABLE);
  } else if (!led.begin()) {
    errorMessage = tr(STR_LED_ERROR);
    LOG_ERR("LED", "Failed to begin LED control session");
  } else {
    appliedIndex = 0;
  }
  rebuildRowItems();
  // The base resets navigation and requests the first render after HAL setup.
  UiListActivity::onEnter();
}

void LedControlActivity::onExit() {
  // ActivityManager already holds RenderLock, including exit from the stack.
  closeRouting();
  appliedIndex = -1;
  errorMessage = tr(STR_LED_UNAVAILABLE);
  if (!led.end()) {
    errorMessage = tr(STR_LED_ERROR);
    LOG_ERR("LED", "LED cleanup failed; green/blue off could not be verified");
  }
  rebuildRowItems();
  Activity::onExit();
}

void LedControlActivity::rebuildRowItems() {
  const char* labels[] = {tr(STR_LED_OFF), tr(STR_LED_GREEN), tr(STR_LED_BLUE), tr(STR_LED_BOTH)};
  for (int i = 0; i < listCount(); ++i) {
    rowItems[i].label = labels[i];
    rowItems[i].value = i == appliedIndex ? tr(STR_LED_APPLIED) : nullptr;
    rowItems[i].actionValue = static_cast<int16_t>(i);
    rowItems[i].enabled = appliedIndex >= 0;
  }
}

const char* LedControlActivity::headerTitle() const { return tr(STR_LED_CONTROL); }

void LedControlActivity::activateIndex(const int index) {
  {
    RenderLock lock(*this);
    if (index < 0 || index >= listCount() || appliedIndex < 0) return;
    nav.selected = index;
    if (led.setColor(static_cast<HalLed::Color>(index))) {
      appliedIndex = index;
    } else {
      appliedIndex = -1;
      errorMessage = tr(STR_LED_ERROR);
      app.clearTapFlash();
      LOG_ERR("LED", "LED control failed; session disabled until re-entry");
    }
    rebuildRowItems();
  }
  requestUpdate();
}

void LedControlActivity::onRowAction(const fui::ActionEvent& event) {
  // Touch callbacks run without RenderLock; activateIndex owns selection too.
  if (!event.longPress) activateIndex(event.value);
}

void LedControlActivity::navigateButtons() {
  bool moved = false;
  {
    RenderLock lock(*this);
    const auto move = [this, &moved](int index) {
      nav.selected = index;
      nav.follow(listCount());
      moved = true;
    };
    buttonNavigator.onNextRelease([&] { move(ButtonNavigator::nextIndex(nav.selected, listCount())); });
    buttonNavigator.onPreviousRelease([&] { move(ButtonNavigator::previousIndex(nav.selected, listCount())); });
    buttonNavigator.onNextContinuous(
        [&] { move(ButtonNavigator::nextPageIndex(nav.selected, listCount(), nav.pageRows())); });
    buttonNavigator.onPreviousContinuous(
        [&] { move(ButtonNavigator::previousPageIndex(nav.selected, listCount(), nav.pageRows())); });
  }
  if (moved) requestUpdate();
}

void LedControlActivity::onBackButton() {
  app.clearTapFlash();
  activityManager.goHome(HomeMenuItem::LED_CONTROL);
}

void LedControlActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  auto noteStyle = screen.theme().smallText;
  noteStyle.maxLines = 3;
  const char* note = tr(STR_LED_RED_SYSTEM_MANAGED);
  const int16_t noteHeight = fui::measureWrappedText(screen.target(), note, noteStyle, screen.body().width).height;
  screen.target().text(screen.takeTop(noteHeight, screen.theme().spaceMd), note, noteStyle);
  if (errorMessage) {
    noteStyle.bold = true;
    const int16_t errorHeight =
        fui::measureWrappedText(screen.target(), errorMessage, noteStyle, screen.body().width).height;
    screen.target().text(screen.takeTop(errorHeight, screen.theme().spaceMd), errorMessage, noteStyle);
  }

  fui::ListProps props;
  props.items = rowItems;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
