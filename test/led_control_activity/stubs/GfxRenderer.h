#pragma once

#include <FreeInkUI.h>

#include <cstring>
#include <string>
#include <vector>

struct EpdFontFamily {
  enum Style { REGULAR };
};

class GfxRenderer : public freeink::ui::DrawTarget {
 public:
  struct Text {
    freeink::ui::Rect rect;
    std::string value;
    freeink::ui::TextStyle style;
  };
  std::vector<Text> texts;
  int width = 480;
  int height = 800;
  int bottomInset = 3;

  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return height; }
  void clearScreen() { texts.clear(); }
  void displayBuffer(int = 0) {}
  void getOrientedViewableTRBL(int* top, int* right, int* bottom, int* left) const {
    *top = *right = *left = 3;
    *bottom = bottomInset;
  }
  size_t getRegionByteSize(int, int, int, int) const { return 0; }
  bool copyRegionToBuffer(int, int, int, int, uint8_t*, size_t) const { return false; }
  bool copyBufferToRegion(int, int, int, int, uint8_t*, size_t) const { return false; }
  freeink::ui::Size measureText(freeink::ui::FontId, const char* text, freeink::ui::TextStyle) const override {
    return {static_cast<int16_t>(std::strlen(text) * 6), 20};
  }
  int16_t lineHeight(freeink::ui::FontId) const override { return 20; }
  void fill(freeink::ui::Rect, freeink::ui::Paint, uint8_t, uint8_t) override {}
  void stroke(freeink::ui::Rect, freeink::ui::Paint, uint8_t, uint8_t, uint8_t) override {}
  void line(freeink::ui::Point, freeink::ui::Point, uint8_t, freeink::ui::Paint) override {}
  void triangle(freeink::ui::Point, freeink::ui::Point, freeink::ui::Point, freeink::ui::Paint) override {}
  void text(freeink::ui::Rect rect, const char* text, freeink::ui::TextStyle style) override {
    texts.push_back({rect, text ? text : "", style});
  }
  void bitmap(freeink::ui::Rect, freeink::ui::BitmapRef, freeink::ui::BitmapMode, freeink::ui::Paint,
              freeink::ui::Rotation) override {}
};
