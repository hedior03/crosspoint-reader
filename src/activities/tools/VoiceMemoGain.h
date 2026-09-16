#pragma once

#include <algorithm>
#include <cstdint>

#include "VoiceMemoConfig.h"

constexpr int16_t voiceMemoApplyGain(int16_t sample) {
  const int32_t amplified = int32_t{sample} * VoiceMemoConfig::GAIN;
  return static_cast<int16_t>(std::clamp(amplified, int32_t{-32768}, int32_t{32767}));
}
