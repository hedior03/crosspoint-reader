// Run: c++ -std=c++20 test/voice_memo/WavTest.cpp -o /tmp/wav-test && /tmp/wav-test
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/activities/tools/VoiceMemoGain.h"
#include "../../src/activities/tools/VoiceMemoWav.h"
int main() {
  for (int sample : {0, 1, -1, 1000, -1000, 16383, 16384, -16384, -16385, 32767, -32768}) {
    const int expected = std::clamp(sample * VoiceMemoConfig::GAIN, -32768, 32767);
    assert(voiceMemoApplyGain(static_cast<int16_t>(sample)) == expected);
  }
  const auto empty = voiceMemoWavHeader(0);
  assert(empty[4] == 36 && empty[40] == 0);
  for (uint32_t sampleCount : {0u, 1u, 16000u, 123456u, 4800000u}) {
    const auto variable = voiceMemoWavHeader(sampleCount * 2);
    const auto read32 = [&](size_t offset) {
      return uint32_t(variable[offset]) | (uint32_t(variable[offset + 1]) << 8) |
             (uint32_t(variable[offset + 2]) << 16) | (uint32_t(variable[offset + 3]) << 24);
    };
    assert(read32(40) == sampleCount * 2);
    assert(read32(4) == sampleCount * 2 + 36);
  }
  const auto header = voiceMemoWavHeader(320000);
  assert(std::memcmp(header.data(), "RIFF", 4) == 0);
  assert(std::memcmp(header.data() + 8, "WAVEfmt ", 8) == 0);
  assert(std::memcmp(header.data() + 36, "data", 4) == 0);
  auto file = std::fopen("/tmp/voice-memo-test.wav", "wb");
  assert(file);
  assert(std::fwrite(header.data(), 1, header.size(), file) == 44);
  for (unsigned i = 0; i < 160000; ++i) {
    const unsigned char zero[2]{};
    assert(std::fwrite(zero, 1, 2, file) == 2);
  }
  assert(std::fclose(file) == 0);
}
