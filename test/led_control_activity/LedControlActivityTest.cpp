#include <gtest/gtest.h>

#include "TestServices.h"
#include "activities/util/LedControlActivity.h"

namespace fui = freeink::ui;
using Button = MappedInputManager::Button;

// Exercise protected virtual entry points without adding a test API to the app.
class ListAccess : public UiListActivity {
 public:
  static fui::ActionEvent lastEvent(UiListActivity& activity) { return (activity.*&ListAccess::app).lastEvent(); }
  static void activate(UiListActivity& activity, int index) { (activity.*&ListAccess::activateIndex)(index); }
  static void row(UiListActivity& activity, int index, bool longPress = false) {
    fui::ActionEvent event;
    event.action = ACTION_ROW;
    event.value = static_cast<int16_t>(index);
    event.longPress = longPress;
    (activity.*&ListAccess::onRowAction)(event);
  }
};

class LedControlActivityTest : public testing::Test {
 protected:
  GfxRenderer renderer;
  MappedInputManager input;
  LedControlActivity activity{renderer, input};

  void SetUp() override {
    test::reset();
    ButtonNavigator::setMappedInputManager(input);
  }
  void TearDown() override { EXPECT_EQ(test::lockDepth, 0); }
  void render() {
    test::rendering = true;
    {
      RenderLock lock;
      activity.render(std::move(lock));
    }
    test::rendering = false;
  }
  const GfxRenderer::Text* text(const char* value) const {
    for (const auto& drawn : renderer.texts) {
      if (drawn.value == value) return &drawn;
    }
    return nullptr;
  }
  void appliedAt(const char* label) const {
    const auto* applied = text("STR_LED_APPLIED");
    ASSERT_NE(applied, nullptr);
    const auto* row = text(label);
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(applied->rect.y, row->rect.y);
    EXPECT_EQ(std::count_if(renderer.texts.begin(), renderer.texts.end(),
                            [](const auto& drawn) { return drawn.value == "STR_LED_APPLIED"; }),
              1);
  }
  void button(Button button) {
    input.released = button;
    activity.loop();
    input.released = Button::None;
  }
  void tap(const char* label, bool release = true) {
    const auto* row = text(label);
    ASSERT_NE(row, nullptr);
    input.touch.touchX = row->rect.x + 2;
    input.touch.touchY = row->rect.y + row->rect.height / 2;
    input.touch.touchReleased = release;
    input.touch.touchPressed = !release;
    activity.loop();
    input.touch = {};
  }
  void exit() {
    RenderLock lock;
    activity.onExit();
  }
};

TEST_F(LedControlActivityTest, InitializesOffBeforeFirstUpdateAndRendersFourChoices) {
  activity.onEnter();
  EXPECT_EQ(test::beginCalls, 1);
  auto begin = std::find(test::events.begin(), test::events.end(), "begin");
  auto update = std::find(test::events.begin(), test::events.end(), "update");
  EXPECT_LT(begin, update);
  render();
  appliedAt("STR_LED_OFF");
  EXPECT_NE(text("STR_LED_GREEN"), nullptr);
  EXPECT_NE(text("STR_LED_BLUE"), nullptr);
  EXPECT_NE(text("STR_LED_BOTH"), nullptr);
  EXPECT_NE(text("STR_LED_RED_SYSTEM_MANAGED"), nullptr);
  EXPECT_NE(text("STR_LED_CONTROL"), nullptr);
  EXPECT_TRUE(test::colors.empty());
  EXPECT_FALSE(activity.preventAutoSleep());
}

TEST_F(LedControlActivityTest, FocusAndSwipeDoNotApplyButConfirmAppliesExactlyOnce) {
  activity.onEnter();
  render();
  button(Button::NavNext);
  render();
  appliedAt("STR_LED_OFF");
  input.swipe = MappedInputManager::SwipeDir::Up;
  activity.loop();
  input.swipe = MappedInputManager::SwipeDir::None;
  EXPECT_TRUE(test::colors.empty());
  button(Button::Confirm);
  render();
  appliedAt("STR_LED_GREEN");
  EXPECT_EQ(test::colors, (std::vector{HalLed::Color::Green}));
  button(Button::NavNext);
  render();
  appliedAt("STR_LED_GREEN");
  ASSERT_NE(text("STR_LED_BLUE"), nullptr);
  EXPECT_EQ(text("STR_LED_BLUE")->style.color, fui::Color::White);
}

TEST_F(LedControlActivityTest, TouchPressDoesNotApplyAndReleaseAppliesOnceForEachChoice) {
  activity.onEnter();
  render();
  tap("STR_LED_BLUE", false);
  EXPECT_TRUE(test::colors.empty());
  tap("STR_LED_BLUE");
  render();
  appliedAt("STR_LED_BLUE");
  tap("STR_LED_BOTH");
  render();
  appliedAt("STR_LED_BOTH");
  tap("STR_LED_GREEN");
  render();
  appliedAt("STR_LED_GREEN");
  tap("STR_LED_OFF");
  render();
  appliedAt("STR_LED_OFF");
  EXPECT_EQ(test::colors,
            (std::vector{HalLed::Color::Blue, HalLed::Color::Both, HalLed::Color::Green, HalLed::Color::Off}));
}

TEST_F(LedControlActivityTest, InvalidAndLongPressActionsCannotApplyOrChangeFocus) {
  activity.onEnter();
  for (int index : {-1, 4, 255}) {
    ListAccess::activate(activity, index);
    ListAccess::row(activity, index);
  }
  ListAccess::row(activity, 2, true);
  EXPECT_TRUE(test::colors.empty());
  button(Button::Confirm);
  EXPECT_EQ(test::colors, (std::vector{HalLed::Color::Off}));
}

TEST_F(LedControlActivityTest, ApplyFailureClearsMarkerBlocksStaleTouchAndButtonsUntilReentry) {
  activity.onEnter();
  render();
  tap("STR_LED_GREEN");
  render();
  test::setOk = false;
  tap("STR_LED_BLUE");
  // The old published table still has enabled rows until the next render.
  tap("STR_LED_BOTH");
  button(Button::Confirm);
  render();
  EXPECT_EQ(text("STR_LED_APPLIED"), nullptr);
  EXPECT_NE(text("STR_LED_ERROR"), nullptr);
  EXPECT_NE(text("STR_BACK"), nullptr);
  tap("STR_LED_GREEN");
  EXPECT_FALSE(ListAccess::lastEvent(activity));
  ListAccess::activate(activity, 0);
  EXPECT_EQ(test::colors, (std::vector{HalLed::Color::Green, HalLed::Color::Blue}));
  button(Button::Back);
  EXPECT_EQ(activityManager.home, HomeMenuItem::LED_CONTROL);
  exit();
  EXPECT_EQ(test::endCalls, 1);
  test::setOk = true;
  activity.onEnter();
  render();
  appliedAt("STR_LED_OFF");
  tap("STR_LED_BOTH");
  ASSERT_FALSE(test::colors.empty());
  EXPECT_EQ(test::colors.back(), HalLed::Color::Both);
}

TEST_F(LedControlActivityTest, BeginFailureDisablesChoicesWithoutAppliedClaimAndStillCleansUp) {
  test::beginOk = false;
  activity.onEnter();
  render();
  EXPECT_EQ(text("STR_LED_APPLIED"), nullptr);
  EXPECT_NE(text("STR_LED_ERROR"), nullptr);
  tap("STR_LED_GREEN");
  button(Button::Confirm);
  EXPECT_TRUE(test::colors.empty());
  button(Button::Back);
  EXPECT_EQ(activityManager.home, HomeMenuItem::LED_CONTROL);
  exit();
  EXPECT_EQ(test::endCalls, 1);
}

TEST_F(LedControlActivityTest, UnsupportedEntryShowsUnavailableAndKeepsBackUsable) {
  test::supported = false;
  activity.onEnter();
  render();
  EXPECT_EQ(test::beginCalls, 0);
  EXPECT_EQ(text("STR_LED_APPLIED"), nullptr);
  EXPECT_NE(text("STR_LED_UNAVAILABLE"), nullptr);
  button(Button::NavNext);
  button(Button::Confirm);
  EXPECT_TRUE(test::colors.empty());
  button(Button::Back);
  EXPECT_EQ(activityManager.home, HomeMenuItem::LED_CONTROL);
  exit();
  EXPECT_EQ(test::endCalls, 1);
}

TEST_F(LedControlActivityTest, ExitUsesHeldLockLogsCleanupFailureAndRejectsLateActions) {
  activity.onEnter();
  ListAccess::activate(activity, 3);
  test::endOk = false;
  exit();
  EXPECT_EQ(test::endCalls, 1);
  EXPECT_FALSE(test::errors.empty());
  ListAccess::activate(activity, 1);
  EXPECT_EQ(test::colors, (std::vector{HalLed::Color::Both}));
  render();
  EXPECT_EQ(text("STR_LED_APPLIED"), nullptr);
}

TEST_F(LedControlActivityTest, RepaintingAndIdleLoopDoNotWriteOrEndSession) {
  activity.onEnter();
  ListAccess::activate(activity, 3);
  for (int i = 0; i < 4; ++i) {
    render();
    activity.loop();
  }
  EXPECT_EQ(test::beginCalls, 1);
  EXPECT_EQ(test::colors, (std::vector{HalLed::Color::Both}));
  EXPECT_EQ(test::endCalls, 0);
  exit();
  EXPECT_EQ(test::endCalls, 1);
}

TEST_F(LedControlActivityTest, HeldButtonPagingAndPreviousWrapKeepAppliedMarkerSeparate) {
  activity.onEnter();
  render();
  button(Button::NavPrevious);
  button(Button::Confirm);
  EXPECT_EQ(test::colors, (std::vector{HalLed::Color::Both}));
  input.held = Button::NavNext;
  activity.loop();
  input.held = Button::None;
  button(Button::NavNext);  // release after a hold must not take another step
  render();
  appliedAt("STR_LED_BOTH");
  button(Button::Confirm);
  ASSERT_FALSE(test::colors.empty());
  EXPECT_EQ(test::colors.back(), HalLed::Color::Off);
}

TEST_F(LedControlActivityTest, TouchSelectionRemainsUnchangedUntilRenderLockIsAcquired) {
  activity.onEnter();
  render();
  test::beforeLock = [this] {
    render();
    appliedAt("STR_LED_OFF");
    ASSERT_NE(text("STR_LED_OFF"), nullptr);
    EXPECT_EQ(text("STR_LED_OFF")->style.color, fui::Color::White);
    ASSERT_NE(text("STR_LED_BLUE"), nullptr);
    EXPECT_EQ(text("STR_LED_BLUE")->style.color, fui::Color::Black);
  };
  ListAccess::row(activity, 2);
  EXPECT_FALSE(test::beforeLock);
  render();
  appliedAt("STR_LED_BLUE");
}
