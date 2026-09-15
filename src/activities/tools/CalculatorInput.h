#pragma once

#include <cstddef>

struct CalculatorInput {
  static constexpr size_t MAX_DIGITS = 12;
  char text[MAX_DIGITS + 2] = "0";
  size_t length = 1;
  bool hasDecimal = false;

  bool append(char digit) {
    if (digit == '.') {
      if (hasDecimal) return false;
      hasDecimal = true;
    } else {
      if (digit < '0' || digit > '9' || full()) return false;
      if (length == 1 && text[0] == '0') {
        text[0] = digit;
        return true;
      }
    }
    text[length++] = digit;
    text[length] = '\0';
    return true;
  }

  void backspace() {
    if (length <= 1) {
      clear();
      return;
    }
    if (text[--length] == '.') hasDecimal = false;
    text[length] = '\0';
  }

  void clear() {
    length = 1;
    text[0] = '0';
    text[1] = '\0';
    hasDecimal = false;
  }

  bool full() const { return length - (hasDecimal ? 1 : 0) == MAX_DIGITS; }
};
