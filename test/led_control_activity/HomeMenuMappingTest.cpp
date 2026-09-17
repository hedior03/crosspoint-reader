#include <RecentBooksStore.h>
#include <gtest/gtest.h>

#include <string>

#include "TestServices.h"
#include "activities/home/HomeActivity.h"

namespace {
// Explicit instantiation permits access to private members without changing
// Home's production API. These pointers call the real inline mapping functions.
template <typename Tag, typename Tag::Type Member>
struct Access {
  friend typename Tag::Type get(Tag) { return Member; }
};
struct ItemToIndex {
  using Type = int (*)(HomeMenuItem, bool);
  friend Type get(ItemToIndex);
};
struct IndexToItem {
  using Type = HomeMenuItem (*)(int, bool);
  friend Type get(IndexToItem);
};
template struct Access<ItemToIndex, &HomeActivity::menuItemToIndex>;
template struct Access<IndexToItem, &HomeActivity::indexToMenuItem>;

struct MenuCase {
  bool leds;
  bool opds;
  std::vector<HomeMenuItem> items;
};
class LedControlHomeTest : public testing::TestWithParam<MenuCase> {};

TEST_P(LedControlHomeTest, VisibleMenuOrderAndRestoredSelectionAgree) {
  test::reset();
  const auto& menu = GetParam();
  test::supported = menu.leds;
  for (size_t index = 0; index < menu.items.size(); ++index) {
    EXPECT_EQ(get(IndexToItem{})(static_cast<int>(index), menu.opds), menu.items[index]);
    EXPECT_EQ(get(ItemToIndex{})(menu.items[index], menu.opds), index);
  }
  EXPECT_EQ(get(IndexToItem{})(-1, menu.opds), HomeMenuItem::NONE);
  EXPECT_EQ(get(IndexToItem{})(static_cast<int>(menu.items.size()), menu.opds), HomeMenuItem::NONE);
  if (!menu.leds) {
    EXPECT_EQ(get(ItemToIndex{})(HomeMenuItem::LED_CONTROL, menu.opds), 0);
  }
  if (!menu.opds) {
    EXPECT_EQ(get(ItemToIndex{})(HomeMenuItem::OPDS_BROWSER, menu.opds), 0);
  }
  EXPECT_EQ(test::beginCalls, 0);
  EXPECT_TRUE(test::colors.empty());
}

using Item = HomeMenuItem;
INSTANTIATE_TEST_SUITE_P(
    LedControlMenus, LedControlHomeTest,
    testing::Values(
        MenuCase{false, false, {Item::FILE_BROWSER, Item::RECENTS, Item::FILE_TRANSFER, Item::SETTINGS_MENU}},
        MenuCase{false,
                 true,
                 {Item::FILE_BROWSER, Item::RECENTS, Item::OPDS_BROWSER, Item::FILE_TRANSFER, Item::SETTINGS_MENU}},
        MenuCase{true,
                 false,
                 {Item::FILE_BROWSER, Item::RECENTS, Item::FILE_TRANSFER, Item::LED_CONTROL, Item::SETTINGS_MENU}},
        MenuCase{true,
                 true,
                 {Item::FILE_BROWSER, Item::RECENTS, Item::OPDS_BROWSER, Item::FILE_TRANSFER, Item::LED_CONTROL,
                  Item::SETTINGS_MENU}}),
    [](const testing::TestParamInfo<MenuCase>& info) {
      return std::string(info.param.leds ? "Supported" : "Unsupported") +
             (info.param.opds ? "WithOpds" : "WithoutOpds");
    });
}  // namespace
