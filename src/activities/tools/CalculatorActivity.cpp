#include "CalculatorActivity.h"

#if FREEINK_DEVICE_PAPERMONO
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr char DIGIT_LABELS[10][2] = {{'0'}, {'1'}, {'2'}, {'3'}, {'4'}, {'5'}, {'6'}, {'7'}, {'8'}, {'9'}};
constexpr uint8_t DIGIT_ORDER[] = {7, 8, 9, 4, 5, 6, 1, 2, 3, 0};
}  // namespace

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  input.clear();
  uiTarget.setFont(fui::GfxRendererTarget::FONT_BODY, UI_12_FONT_ID);
  resetUi();
  for (size_t i = 0; i < 10; ++i) {
    keys[i].label = DIGIT_LABELS[DIGIT_ORDER[i]];
    keys[i].value = DIGIT_ORDER[i];
  }
  keys[9].widthUnits = 2;
  keys[10].label = tr(STR_CALCULATOR_CLEAR);
  keys[10].value = CLEAR;
  for (size_t i = 0; i < 3; ++i) rows[i] = fui::KeyboardRow{keys + i * 3, 3};
  rows[3] = fui::KeyboardRow{keys + 9, 2};
  keyboard.layout = &layout;
  keyboard.keyAction = ACTION_KEY;
  keyboard.inputMask = fui::InputTouch;
  keyboard.labelText.font = fui::GfxRendererTarget::FONT_BODY;
  keyboard.padding = fui::Insets{0, 0, 0, 0};
  app.on(
      ACTION_KEY,
      [](const fui::ActionEvent& event, void* user) {
        static_cast<CalculatorActivity*>(user)->activateKey(event.value);
      },
      this);
  app.setScreen([](UiScreen& screen, void* user) { static_cast<CalculatorActivity*>(user)->buildScreen(screen); },
                this);
  requestUpdate();
}

void CalculatorActivity::activateKey(int16_t value) {
  {
    RenderLock lock;
    navigator.syncToValue(layout, value);
    if (value == CLEAR) {
      input.clear();
    } else if (value >= 0 && value <= 9) {
      input.append(static_cast<char>('0' + value));
    }
  }
  requestUpdate();
}

void CalculatorActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.routed && app.invalidated()) requestUpdate();
  if (touch) return;

  if (mappedInput.wasReleased(Button::Confirm)) {
    const auto* key = navigator.selected(layout);
    if (key) activateKey(key->value);
    return;
  }
  const int horizontal = mappedInput.wasReleased(Button::Right) - mappedInput.wasReleased(Button::Left);
  const int vertical = mappedInput.wasReleased(Button::Down) - mappedInput.wasReleased(Button::Up);
  if (horizontal || vertical) {
    {
      RenderLock lock;
      if (horizontal) navigator.moveCol(layout, horizontal);
      if (vertical) navigator.moveRow(layout, vertical);
    }
    requestUpdate();
  }
}

void CalculatorActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(metrics.contentSidePadding),
      static_cast<int16_t>(metrics.buttonHintsHeight), static_cast<int16_t>(metrics.contentSidePadding)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  auto textStyle = screen.theme().bodyText;
  textStyle.align = fui::TextAlign::Right;
  textStyle.maxLines = 1;
  uiTarget.text(screen.takeTop(screen.theme().rowHeight, static_cast<int16_t>(metrics.verticalSpacing)), input.text,
                textStyle);
  for (size_t i = 0; i < 10; ++i) keys[i].enabled = !input.full();
  keyboard.gap = static_cast<int16_t>(metrics.keyboardKeySpacing);
  keyboard.selectedIndex = navigator.logicalIndex(layout);
  fui::keyboard(screen.frame(), screen.body(), keyboard);
}

void CalculatorActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_CALCULATOR));
  renderUi();
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
#endif
