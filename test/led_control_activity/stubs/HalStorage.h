#pragma once

struct TestStorage {
  bool exists(const char*) const { return false; }
};
inline TestStorage Storage;
