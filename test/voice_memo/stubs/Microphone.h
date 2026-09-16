#pragma once
#include <cstddef>
#include <cstdint>
inline uint32_t fakeMillis = 1000;
inline uint32_t millis() { return fakeMillis; }
inline void delay(uint32_t ms) { fakeMillis += ms; }
class Microphone {
 public:
  bool begin(uint32_t) { return true; }
  int read(int16_t*, size_t, uint32_t) { return 0; }
  void end() {}
  bool overflowed() const { return false; }
};
