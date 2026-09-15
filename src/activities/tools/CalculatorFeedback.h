#pragma once

#include <cstdint>

#include "CalculatorState.h"

struct CalculatorFeedback {
  struct Frame {
    CalculatorState calculator;
    int16_t selected = -1;
    uint32_t pressed = 0;
  };
  static constexpr uint32_t HIGHLIGHT_MS = 250;
  static constexpr size_t KEY_COUNT = 19;
  uint32_t shownAt[KEY_COUNT]{};
  uint32_t active = 0;

  void shown(uint32_t now, uint32_t pressed) {
    for (size_t key = 0; key < KEY_COUNT; ++key) {
      const uint32_t bit = uint32_t{1} << key;
      if (pressed & bit) {
        shownAt[key] = now;
        active |= bit;
      }
    }
  }

  bool clearDue(uint32_t now) {
    const uint32_t previous = active;
    for (size_t key = 0; key < KEY_COUNT; ++key) {
      const uint32_t bit = uint32_t{1} << key;
      if ((active & bit) && now - shownAt[key] >= HIGHLIGHT_MS) active &= ~bit;
    }
    return active != previous;
  }

  Frame latest;

  void publish(const Frame& frame) {
    const uint32_t pressed = latest.pressed | frame.pressed;
    latest = frame;
    latest.pressed = pressed;
  }

  void take(Frame& frame) {
    frame = latest;
    latest.pressed = 0;
  }
};
