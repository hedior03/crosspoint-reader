#include "CalculatorActivity.h"

#if FREEINK_DEVICE_PAPERMONO
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr char DIGIT_LABELS[10][2] = {{'0'}, {'1'}, {'2'}, {'3'}, {'4'}, {'5'}, {'6'}, {'7'}, {'8'}, {'9'}};
constexpr int16_t KEY_VALUES[] = {10, 18, 17, 12, 7, 8, 9, 13, 4, 5, 6, 14, 1, 2, 3, 15, 0, 11, 16};
}  // namespace

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  calculator.clear();
  navigator.reset(-1, -1);
  uiTarget.setFont(fui::GfxRendererTarget::FONT_BODY, UI_12_FONT_ID);
  uiTarget.setFont(fui::GfxRendererTarget::FONT_TITLE, NOTOSANS_18_FONT_ID);
  resetUi();
  for (size_t i = 0; i < 19; ++i) {
    keys[i].value = KEY_VALUES[i];
    if (keys[i].value < CLEAR) keys[i].label = DIGIT_LABELS[keys[i].value];
    keys[i].enabled = true;
  }
  keys[0].label = tr(STR_CALCULATOR_CLEAR);
  keys[1].label = tr(STR_CALCULATOR_CLEAR_ENTRY);
  keys[2].label = tr(STR_CALCULATOR_BACKSPACE);
  keys[3].label = tr(STR_CALCULATOR_DIVIDE);
  keys[7].label = tr(STR_CALCULATOR_MULTIPLY);
  keys[11].label = tr(STR_CALCULATOR_SUBTRACT);
  keys[15].label = tr(STR_CALCULATOR_ADD);
  keys[16].widthUnits = 2;
  keys[17].label = tr(STR_CALCULATOR_DECIMAL);
  keys[18].label = tr(STR_CALCULATOR_EQUALS);
  rows[0] = fui::KeyboardRow{keys, 4};
  for (size_t i = 0; i < 3; ++i) rows[i + 1] = fui::KeyboardRow{keys + 4 + i * 4, 4};
  rows[4] = fui::KeyboardRow{keys + 16, 3};
  keyboard.layout = &layout;
  keyboard.keyAction = ACTION_KEY;
  keyboard.inputMask = fui::InputTouch;
  keyboard.labelText.font = fui::GfxRendererTarget::FONT_BODY;
  keyboard.padding = fui::Insets{0, 0, 0, 0};
  keyboard.keyStyles = fui::defaultButtonStyles();
  fui::BoxStyle* states[] = {&keyboard.keyStyles.normal, &keyboard.keyStyles.selected, &keyboard.keyStyles.focused,
                             &keyboard.keyStyles.active, &keyboard.keyStyles.disabled};
  for (auto* style : states) {
    style->border = fui::Paint::solid(fui::Color::Black);
    style->borderWidth = 1;
  }
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

void CalculatorActivity::publishFeedback(int16_t pressed) {
  const CalculatorFeedback::Frame frame{calculator, navigator.logicalIndex(layout),
                                        pressed >= 0 ? uint32_t{1} << pressed : 0};
  portENTER_CRITICAL(&feedbackMux);
  feedback.publish(frame);
  portEXIT_CRITICAL(&feedbackMux);
  requestUpdate();
}

void CalculatorActivity::activateKey(int16_t value) {
  if (value < 0 || value > 18) return;
  if (value == CLEAR) {
    calculator.clear();
  } else if (value == DECIMAL) {
    calculator.append('.');
  } else if (value == 18) {
    calculator.clearEntry();
  } else if (value == 17) {
    calculator.backspace();
  } else if (value >= 12 && value <= 16) {
    static constexpr char OPERATIONS[] = {'/', '*', '-', '+', '='};
    calculator.calculate(OPERATIONS[value - 12]);
  } else if (value >= 0 && value <= 9) {
    calculator.append(static_cast<char>('0' + value));
  }
  app.clearTapFlash();
  publishFeedback(value);
}

void CalculatorActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  portENTER_CRITICAL(&feedbackMux);
  const bool clearHighlight = feedback.clearDue(millis());
  portEXIT_CRITICAL(&feedbackMux);
  if (clearHighlight) requestUpdate();
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
    if (horizontal) navigator.moveCol(layout, horizontal);
    if (vertical) navigator.moveRow(layout, vertical);
    publishFeedback();
  }
}

void CalculatorActivity::buildScreen(UiScreen& screen) {
  const auto& calculator = displayed.calculator;
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(metrics.contentSidePadding),
      static_cast<int16_t>(metrics.buttonHintsHeight), static_cast<int16_t>(metrics.contentSidePadding)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  const char* pending = "";
  if (!calculator.error) {
    switch (calculator.operation) {
      case '+':
        pending = tr(STR_CALCULATOR_ADD);
        break;
      case '-':
        pending = tr(STR_CALCULATOR_SUBTRACT);
        break;
      case '*':
        pending = tr(STR_CALCULATOR_MULTIPLY);
        break;
      case '/':
        pending = tr(STR_CALCULATOR_DIVIDE);
        break;
    }
  }
  auto pendingStyle = screen.theme().bodyText;
  pendingStyle.align = fui::TextAlign::Right;
  pendingStyle.maxLines = 1;
  uiTarget.text(screen.takeTop(uiTarget.lineHeight(pendingStyle.font)), pending, pendingStyle);
  auto textStyle = screen.theme().bodyText;
  textStyle.font = fui::GfxRendererTarget::FONT_TITLE;
  textStyle.align = fui::TextAlign::Right;
  textStyle.maxLines = 1;
  const auto readoutHeight = std::max(screen.theme().rowHeight, uiTarget.lineHeight(textStyle.font));
  uiTarget.text(screen.takeTop(readoutHeight, static_cast<int16_t>(metrics.verticalSpacing)),
                calculator.error ? tr(STR_CALCULATOR_ERROR) : calculator.display(), textStyle);
  for (auto& key : keys) {
    key.state = (displayed.pressed & (uint32_t{1} << key.value)) ? fui::StateSelected : fui::StateNormal;
  }
  keyboard.gap = static_cast<int16_t>(metrics.keyboardKeySpacing);
  keyboard.selectedIndex = displayed.selected;
  fui::keyboard(screen.frame(), screen.body(), keyboard);
}

void CalculatorActivity::render(RenderLock&&) {
  portENTER_CRITICAL(&feedbackMux);
  feedback.take(displayed);
  const uint32_t newPresses = displayed.pressed;
  displayed.pressed |= feedback.active;
  portEXIT_CRITICAL(&feedbackMux);
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_CALCULATOR));
  renderUi();
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
  portENTER_CRITICAL(&feedbackMux);
  feedback.shown(millis(), newPresses);
  portEXIT_CRITICAL(&feedbackMux);
}
#endif
