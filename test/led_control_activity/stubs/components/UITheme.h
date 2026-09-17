#pragma once

#include <GfxRenderer.h>
#include <RecentBooksStore.h>

#include <functional>

struct Rect {
  int x;
  int y;
  int width;
  int height;
};

enum UIIcon { Folder, Recent, Transfer, Settings, Library, Book };

class UITheme {
 public:
  struct Metrics {
    int topPadding = 5;
    int headerHeight = 45;
    int verticalSpacing = 10;
    int listRowHeight = 30;
    int listWithSubtitleRowHeight = 50;
    int contentSidePadding = 20;
    int homeRecentBooksCount = 1;
    int homeTopPadding = 55;
    int homeCoverTileHeight = 350;
    int homeCoverHeight = 300;
    int homeMenuTopOffset = 20;
    bool homeContinueReadingInMenu = true;
    int menuSpacing = 6;
    int buttonHintsHeight = 0;
  };
  Metrics metrics;
  Rect menuRect{};
  Rect coverRect{};
  int menuRowHeight = 49;  // RoundedRaff's UI_12 line height (29) plus 20px padding.
  int menuCount = 0;
  std::vector<std::string> menuLabels;
  static UITheme& getInstance() {
    static UITheme theme;
    return theme;
  }
  const Metrics& getMetrics() const { return metrics; }
  Rect getScreenSafeArea(const GfxRenderer& renderer, bool frontHints, bool) const {
    return {20, 0, renderer.width - 40, renderer.height - (frontHints ? metrics.buttonHintsHeight : 0)};
  }
  void drawHeader(GfxRenderer& renderer, Rect, const char* title) { renderer.text({}, title, {}); }
  void drawButtonHints(GfxRenderer& renderer, const char* back, const char*, const char*, const char*) {
    renderer.text({}, back, {});
  }
  int getMenuRowHeight(const GfxRenderer&) const { return menuRowHeight; }
  void drawButtonMenu(GfxRenderer&, Rect rect, int count, int, const std::function<std::string(int)>& label,
                      const std::function<UIIcon(int)>&) {
    menuRect = rect;
    menuCount = count;
    menuLabels.clear();
    for (int i = 0; i < count; ++i) menuLabels.push_back(label(i));
  }
  void drawRecentBookCover(GfxRenderer&, Rect rect, const std::vector<RecentBook>&, int, bool&, bool&, bool&,
                           std::function<bool()>) {
    coverRect = rect;
  }
  static std::string getCoverThumbPath(const std::string& path, int) { return path; }
  Rect drawPopup(GfxRenderer&, const char*) { return {}; }
  void fillPopupProgress(GfxRenderer&, Rect, int) {}
};

#define GUI UITheme::getInstance()
