#pragma once

#include <algorithm>
#include <cstdint>

// Fixed digital gain; tune from recorded speech and clipping measurements.
constexpr int32_t VOICE_MEMO_GAIN = 2;

constexpr int16_t voiceMemoApplyGain(int16_t sample) {
  const int32_t amplified = int32_t{sample} * VOICE_MEMO_GAIN;
  return static_cast<int16_t>(std::clamp(amplified, int32_t{-32768}, int32_t{32767}));
}
