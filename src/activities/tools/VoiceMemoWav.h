#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "VoiceMemoConfig.h"

inline auto voiceMemoWavHeader(uint32_t dataBytes, uint32_t sampleCount = 0) {
  constexpr size_t size = VoiceMemoConfig::ADPCM ? 60 : 44;
  std::array<uint8_t, size> header{};
  const auto put = [&](size_t offset, uint32_t value, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) header[offset + i] = static_cast<uint8_t>(value >> (8 * i));
  };
  const char tags[] = "RIFFWAVEfmt data";
  for (size_t i = 0; i < 4; ++i) {
    header[i] = tags[i];
    header[8 + i] = tags[4 + i];
    header[12 + i] = tags[8 + i];
    header[size - 8 + i] = tags[12 + i];
  }
  put(4, size - 8 + dataBytes, 4);
  put(16, VoiceMemoConfig::ADPCM ? 20 : 16, 4);
  put(20, VoiceMemoConfig::ADPCM ? 0x11 : 1, 2);
  put(22, 1, 2);
  put(24, VoiceMemoConfig::SAMPLE_RATE, 4);
  if constexpr (VoiceMemoConfig::ADPCM) {
    put(28, VoiceMemoConfig::SAMPLE_RATE * VoiceMemoConfig::CODEC_BLOCK_BYTES / VoiceMemoConfig::CODEC_BLOCK_SAMPLES,
        4);
    put(32, VoiceMemoConfig::CODEC_BLOCK_BYTES, 2);
    put(34, 4, 2);
    put(36, 2, 2);
    put(38, VoiceMemoConfig::CODEC_BLOCK_SAMPLES, 2);
    header[40] = 'f';
    header[41] = 'a';
    header[42] = 'c';
    header[43] = 't';
    put(44, 4, 4);
    put(48, sampleCount, 4);
  } else {
    put(28, VoiceMemoConfig::SAMPLE_RATE * VoiceMemoConfig::BYTES_PER_SAMPLE, 4);
    put(32, 2, 2);
    put(34, 16, 2);
  }
  put(size - 4, dataBytes, 4);
  return header;
}
