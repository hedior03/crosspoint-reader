#pragma once

#include <FreeInkUI.h>

#include <algorithm>
#include <cstdint>

inline uint32_t millis() { return 1000; }

class MappedInputManager {
 public:
  enum class Button { None, Back, Confirm, NavNext, NavPrevious };
  enum class SwipeDir { None, Up, Down };
  enum class RowTouch { None, Down, Up };
  Button released = Button::None;
  Button held = Button::None;
  SwipeDir swipe = SwipeDir::None;
  freeink::ui::InputSnapshot touch{};
  bool touchEnabled = true;
  int menuTouchTop = -1;
  int menuTouchRowHeight = 0;
  int menuTouchRowStep = 0;
  int menuTouchCount = 0;

  bool wasReleased(Button button) const { return released == button; }
  bool wasPressed(Button) const { return false; }
  bool isPressed(Button button) const { return held == button; }
  uint32_t getHeldTime() const { return held == Button::None ? 0 : 1000; }
  SwipeDir wasSwipe() const { return swipe; }
  bool hasTouch() const { return touchEnabled; }
  RowTouch colTouch(int&, int, int, int, int, int, int) const { return RowTouch::None; }
  RowTouch rowTouch(int&, int top, int rowStep, int count, int, int, int rowHeight) {
    menuTouchTop = top;
    menuTouchRowHeight = rowHeight;
    menuTouchRowStep = rowStep;
    menuTouchCount = count;
    return RowTouch::None;
  }
  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };
  Labels mapLabels(const char* a, const char* b, const char* c, const char* d) const { return {a, b, c, d}; }
};
