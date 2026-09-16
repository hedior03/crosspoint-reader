#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

inline std::array<uint8_t, 44> voiceMemoWavHeader(uint32_t dataBytes) {
  std::array<uint8_t, 44> header{};
  const auto put = [&](size_t offset, uint32_t value, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) header[offset + i] = static_cast<uint8_t>(value >> (8 * i));
  };
  const char tags[] = "RIFFWAVEfmt data";
  for (size_t i = 0; i < 4; ++i) {
    header[i] = tags[i];
    header[8 + i] = tags[4 + i];
    header[12 + i] = tags[8 + i];
    header[36 + i] = tags[12 + i];
  }
  put(4, 36 + dataBytes, 4);
  put(16, 16, 4);
  put(20, 1, 2);
  put(22, 1, 2);
  put(24, 16000, 4);
  put(28, 32000, 4);
  put(32, 2, 2);
  put(34, 16, 2);
  put(40, dataBytes, 4);
  return header;
}
