#pragma once
#include <cstddef>
#include <cstdint>

namespace VoiceMemoConfig {
enum class RecordingFormat { Pcm16, ImaAdpcm4 };
#ifdef VOICE_MEMO_PCM_BASELINE
inline constexpr RecordingFormat FORMAT = RecordingFormat::Pcm16;
#else
inline constexpr RecordingFormat FORMAT = RecordingFormat::ImaAdpcm4;
#endif
inline constexpr bool ADPCM = FORMAT == RecordingFormat::ImaAdpcm4;
inline constexpr uint32_t CODEC_BLOCK_BYTES = 256;
inline constexpr uint32_t CODEC_BLOCK_SAMPLES = (CODEC_BLOCK_BYTES - 4) * 2 + 1;
inline constexpr uint32_t WRITER_STACK_BYTES = ADPCM ? 6144 : 4096;

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
inline constexpr uint64_t CHUNK_RETAINED_SAMPLES =
    ADPCM ? (CHUNK_SAMPLES + CODEC_BLOCK_SAMPLES - 1) / CODEC_BLOCK_SAMPLES * CODEC_BLOCK_SAMPLES : CHUNK_SAMPLES;
static_assert(CHUNK_RETAINED_SAMPLES <= (UINT32_MAX - 60) / BYTES_PER_SAMPLE);
static_assert(SAMPLE_RATE >= 8000 && SAMPLE_RATE <= 48000);
static_assert(GAIN >= 1 && GAIN <= 64);
static_assert(CHUNK_SAMPLES > 0 && CHUNK_SAMPLES <= (UINT32_MAX - 36) / BYTES_PER_SAMPLE);
static_assert(BLOCK_SAMPLES > 0 && BLOCK_SAMPLES <= 4096);
static_assert(BUFFER_BLOCKS >= 4 && BUFFER_BLOCKS <= 256);
}  // namespace VoiceMemoConfig
