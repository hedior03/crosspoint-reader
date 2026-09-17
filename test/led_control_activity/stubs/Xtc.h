#pragma once

#include <string>

struct Xtc {
  Xtc(const std::string&, const char*) {}
  bool load() { return false; }
  bool generateThumbBmp(int) { return false; }
};
