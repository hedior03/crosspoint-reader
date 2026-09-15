// Run: c++ -std=c++20 test/calculator_input/CalculatorInputTest.cpp -o /tmp/calculator-input-test &&
// /tmp/calculator-input-test
#include <cassert>
#include <cstring>

#include "../../src/activities/tools/CalculatorInput.h"

int main() {
  CalculatorInput input;
  assert(input.text[0] == '\0');
  constexpr char digits[] = {'0', '0', '7'};
  for (char digit : digits) assert(input.append(digit));
  assert(std::strcmp(input.text, "007") == 0);
  input.clear();
  input.clear();
  assert(input.length == 0 && input.text[0] == '\0');
  assert(!input.append('C'));
  for (size_t i = 0; i < CalculatorInput::MAX_DIGITS; ++i) assert(input.append('8'));
  assert(input.full());
  assert(!input.append('9'));
  assert(std::strcmp(input.text, "888888888888") == 0);
  input.clear();
  assert(input.append('1'));
  assert(std::strcmp(input.text, "1") == 0);
}
