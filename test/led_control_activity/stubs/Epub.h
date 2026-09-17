#pragma once

#include <string>

struct Epub {
  Epub(const std::string&, const char*) {}
  bool load(bool, bool) { return false; }
  bool generateThumbBmp(int) { return false; }
};
