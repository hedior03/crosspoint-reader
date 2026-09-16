#pragma once
#include <cstddef>
#include <cstdint>

namespace VoiceMemoConfig {
// Tune these together with MEMO latency, buffer high-water and clipping logs.
inline constexpr uint32_t SAMPLE_RATE = 16000;
inline constexpr int32_t GAIN = 2;
inline constexpr uint32_t CHUNK_SECONDS = 300;
inline constexpr size_t BLOCK_SAMPLES = 1024;
inline constexpr uint16_t BUFFER_BLOCKS = 32;
inline constexpr uint32_t READ_TIMEOUT_MS = 100;
inline constexpr uint32_t NO_AUDIO_TIMEOUT_MS = 2000;
inline constexpr uint32_t BYTES_PER_SAMPLE = sizeof(int16_t);  // Mono signed PCM stays 16-bit.
inline constexpr uint64_t CHUNK_SAMPLES = uint64_t{SAMPLE_RATE} * CHUNK_SECONDS;
static_assert(SAMPLE_RATE >= 8000 && SAMPLE_RATE <= 48000);
static_assert(GAIN >= 1 && GAIN <= 64);
static_assert(CHUNK_SAMPLES > 0 && CHUNK_SAMPLES <= (UINT32_MAX - 36) / BYTES_PER_SAMPLE);
static_assert(BLOCK_SAMPLES > 0 && BLOCK_SAMPLES <= 4096);
static_assert(BUFFER_BLOCKS >= 4 && BUFFER_BLOCKS <= 256);
}  // namespace VoiceMemoConfig
