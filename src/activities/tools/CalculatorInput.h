#pragma once

#include <cstddef>

struct CalculatorInput {
  static constexpr size_t MAX_DIGITS = 12;
  char text[MAX_DIGITS + 1]{};
  size_t length = 0;

  bool append(char digit) {
    if (digit < '0' || digit > '9' || full()) return false;
    text[length++] = digit;
    text[length] = '\0';
    return true;
  }

  void clear() {
    length = 0;
    text[0] = '\0';
  }

  bool full() const { return length == MAX_DIGITS; }
};
