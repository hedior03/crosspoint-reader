#pragma once

#include "VoiceMemoConfig.h"

// Owner-selected SciPy SOS preset: docs/audio-presets/voice-ellip-150-4500.json.
// Transposed direct form II; state survives blocks/rollovers, resets with each writer run.
class VoiceMemoFilter {
 public:
  float process(float input) {
    if constexpr (!VoiceMemoConfig::FILTER_ENABLED) return input;
    for (unsigned i = 0; i < 4; ++i) {
      const auto& c = SOS[i];
      const float output = c[0] * input + state[i][0];
      state[i][0] = c[1] * input - c[4] * output + state[i][1];
      state[i][1] = c[2] * input - c[5] * output;
      input = output;
    }
    return input;
  }

 private:
  // [b0, b1, b2, a0, a1, a2]; a0 is normalized to 1 in every section.
  static constexpr float SOS[4][6] = {{0.88219343f, -1.76418145f, 0.88219343f, 1.0f, -1.88026294f, 0.88756655f},
                                      {1.0f, -1.99884655f, 1.0f, 1.0f, -1.98178591f, 0.98502981f},
                                      {0.15709573f, 0.28703343f, 0.15709573f, 1.0f, -0.24200509f, 0.20200323f},
                                      {1.0f, 1.26843099f, 1.0f, 1.0f, 0.39617113f, 0.7720615f}};
  static_assert(!VoiceMemoConfig::FILTER_ENABLED || VoiceMemoConfig::SAMPLE_RATE == 16000,
                "Redesign filter coefficients before changing sample rate");
  float state[4][2]{};
};
