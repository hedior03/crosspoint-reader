#pragma once

#include <algorithm>
#include <cstdint>

inline int16_t voiceMemoToPcm(float sample) {
  const float value = std::clamp(sample, -32768.0f, 32767.0f);
  return static_cast<int16_t>(value + (value < 0 ? -0.5f : 0.5f));
}
