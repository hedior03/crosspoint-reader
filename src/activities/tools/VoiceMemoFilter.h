#pragma once

#include "VoiceMemoConfig.h"

// Owner-selected SciPy SOS presets: docs/audio-presets/.
// Transposed direct form II; state survives blocks/rollovers, resets with each writer run.
class VoiceMemoFilter {
 public:
  float process(float input) {
    if constexpr (!VoiceMemoConfig::FILTER_ENABLED) return input;
    for (unsigned i = 0; i < 4; ++i) {
      const auto& c = (VoiceMemoConfig::SAMPLE_RATE == 24000 ? SOS_24000 : SOS_16000)[i];
      const float output = c[0] * input + state[i][0];
      state[i][0] = c[1] * input - c[4] * output + state[i][1];
      state[i][1] = c[2] * input - c[5] * output;
      input = output;
    }
    return input;
  }

 private:
  // [b0, b1, b2, a0, a1, a2]; a0 is normalized to 1 in every section.
  static constexpr float SOS_16000[4][6] = {{0.88219343f, -1.76418145f, 0.88219343f, 1.0f, -1.88026294f, 0.88756655f},
                                            {1.0f, -1.99884655f, 1.0f, 1.0f, -1.98178591f, 0.98502981f},
                                            {0.15709573f, 0.28703343f, 0.15709573f, 1.0f, -0.24200509f, 0.20200323f},
                                            {1.0f, 1.26843099f, 1.0f, 1.0f, 0.39617113f, 0.7720615f}};
  static constexpr float SOS_24000[4][6] = {{0.9127299f, -1.82540669f, 0.9127299f, 1.0f, -1.94020743f, 0.94208799f},
                                            {1.0f, -1.9997117f, 1.0f, 1.0f, -1.99166965f, 0.99248382f},
                                            {0.03472436f, 0.03741045f, 0.03472436f, 1.0f, -1.17689793f, 0.4389028f},
                                            {1.0f, -0.39075291f, 1.0f, 1.0f, -1.11648208f, 0.81170667f}};
  static_assert(!VoiceMemoConfig::FILTER_ENABLED || VoiceMemoConfig::SAMPLE_RATE == 16000 ||
                    VoiceMemoConfig::SAMPLE_RATE == 24000,
                "Redesign filter coefficients before changing sample rate");
  float state[4][2]{};
};
