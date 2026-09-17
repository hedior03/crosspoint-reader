#include <OpdsServerStore.h>
#include <RecentBooksStore.h>
#include <gtest/gtest.h>

#include "TestServices.h"
#include "activities/home/HomeActivity.h"
#include "components/UITheme.h"

class LedControlHomeGeometryTest : public testing::Test {
 protected:
  GfxRenderer renderer;
  MappedInputManager input;
  HomeActivity home{renderer, input};

  void SetUp() override {
    test::reset();
    GUI.metrics = {};
    GUI.menuRowHeight = 49;
    OPDS_STORE.enabled = true;
    RECENT_BOOKS.books = {{"/book.epub", "Recent book", "Author", ""}};
    ButtonNavigator::setMappedInputManager(input);
  }
  void TearDown() override {
    RenderLock lock;
    home.onExit();
  }
  void enterAndRender() {
    home.onEnter();
    RenderLock lock;
    home.render(std::move(lock));
  }
  void expectTouchMatchesDrawing() {
    home.loop();
    EXPECT_EQ(input.menuTouchTop, GUI.menuRect.y);
    EXPECT_EQ(input.menuTouchRowHeight, 49);
    EXPECT_EQ(input.menuTouchRowStep, 55);
    EXPECT_EQ(input.menuTouchCount, GUI.menuCount);
  }
};

TEST_F(LedControlHomeGeometryTest, PaperMonoRecentAndOpdsLastCardFitsViewableBottom) {
  enterAndRender();
  ASSERT_EQ(renderer.getScreenWidth(), 480);
  ASSERT_EQ(renderer.getScreenHeight(), 800);
  ASSERT_EQ(GUI.menuCount, 7);
  EXPECT_EQ(GUI.menuLabels.front(), "STR_CONTINUE_READING");
  EXPECT_EQ(GUI.menuLabels[3], "STR_OPDS_BROWSER");
  EXPECT_EQ(GUI.menuLabels[5], "STR_LED_CONTROL");
  EXPECT_EQ(GUI.menuLabels.back(), "STR_SETTINGS_TITLE");
  EXPECT_EQ(GUI.coverRect.y + GUI.coverRect.height, 405);
  EXPECT_EQ(GUI.menuRect.y, 418);
  EXPECT_GE(GUI.menuRect.y - 405, 6);
  // Seven 49px cards, with six 6px gaps; no trailing gap is required.
  EXPECT_LE(GUI.menuRect.y + 6 * 55 + 49, 797);
  expectTouchMatchesDrawing();
}

TEST_F(LedControlHomeGeometryTest, AlreadyFittingMenuKeepsItsOriginalGap) {
  OPDS_STORE.enabled = false;
  enterAndRender();
  ASSERT_EQ(GUI.menuCount, 6);
  EXPECT_EQ(GUI.menuRect.y, 425);
  expectTouchMatchesDrawing();
}

TEST_F(LedControlHomeGeometryTest, NoRecentBookKeepsItsOriginalGap) {
  RECENT_BOOKS.books.clear();
  enterAndRender();
  ASSERT_EQ(GUI.menuCount, 6);
  EXPECT_EQ(GUI.menuRect.y, 425);
  expectTouchMatchesDrawing();
}

TEST_F(LedControlHomeGeometryTest, UnsupportedBoardKeepsItsOriginalLayoutEvenWhenShort) {
  test::supported = false;
  renderer.height = 740;
  enterAndRender();
  ASSERT_EQ(GUI.menuCount, 6);
  EXPECT_EQ(GUI.menuRect.y, 425);
  expectTouchMatchesDrawing();
}

TEST_F(LedControlHomeGeometryTest, ShortViewportCannotConsumeTheMinimumCoverGap) {
  renderer.height = 780;
  enterAndRender();
  EXPECT_EQ(GUI.menuRect.y, 411);
  EXPECT_EQ(GUI.menuRect.y - (GUI.coverRect.y + GUI.coverRect.height), 6);
  expectTouchMatchesDrawing();
}

TEST_F(LedControlHomeGeometryTest, BezelInsetDeterminesTheRequiredShift) {
  renderer.bottomInset = 8;
  enterAndRender();
  EXPECT_EQ(GUI.menuRect.y, 413);
  EXPECT_LE(GUI.menuRect.y + 6 * 55 + 49, 792);
  expectTouchMatchesDrawing();
}
