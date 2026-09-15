#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "CalculatorInput.h"

struct CalculatorState {
  CalculatorInput input;
  double total = 0;
  char operation = 0;
  bool fresh = false;
  bool error = false;
  char result[32] = "0";

  void clear() { *this = CalculatorState{}; }

  void clearEntry() {
    if (error) {
      clear();
      return;
    }
    input.clear();
    fresh = false;
  }

  void append(char digit) {
    if (error) clear();
    if (fresh) input.clear();
    fresh = false;
    input.append(digit);
  }

  void backspace() {
    if (!fresh && !error) input.backspace();
  }

  const char* display() const { return fresh ? result : input.text; }

  void calculate(char next) {
    if (error) return;
    if (fresh) {
      if (next != '=') operation = next;
      return;
    }
    const double value = std::strtod(input.text, nullptr);
    switch (operation) {
      case '+':
        total += value;
        break;
      case '-':
        total -= value;
        break;
      case '*':
        total *= value;
        break;
      case '/':
        if (value == 0) {
          error = true;
          return;
        }
        total /= value;
        break;
      default:
        total = value;
        break;
    }
    if (!std::isfinite(total)) {
      error = true;
      return;
    }
    // Round stored results to the displayed precision so chained operations use what is visible.
    std::snprintf(result, sizeof(result), "%.12g", total == 0 ? 0.0 : total);
    total = std::strtod(result, nullptr);
    error = !std::isfinite(total);
    operation = next == '=' ? 0 : next;
    fresh = true;
  }
};
