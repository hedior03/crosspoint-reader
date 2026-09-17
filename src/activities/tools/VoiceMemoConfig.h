#pragma once
#include <cstddef>
#include <cstdint>

// Defaults may be overridden by PlatformIO build flags in platformio.local.ini.
#ifndef VOICE_MEMO_FORMAT
#define VOICE_MEMO_FORMAT ImaAdpcm4
#endif
#ifndef VOICE_MEMO_SAMPLE_RATE
#define VOICE_MEMO_SAMPLE_RATE 16000
#endif
#ifndef VOICE_MEMO_FILTER_ENABLED
#define VOICE_MEMO_FILTER_ENABLED true
#endif
#ifndef VOICE_MEMO_CHUNK_SECONDS
#define VOICE_MEMO_CHUNK_SECONDS 300
#endif
#ifndef VOICE_MEMO_BLOCK_SAMPLES
#define VOICE_MEMO_BLOCK_SAMPLES 1024
#endif
#ifndef VOICE_MEMO_BUFFER_BLOCKS
#define VOICE_MEMO_BUFFER_BLOCKS 32
#endif
#ifndef VOICE_MEMO_READ_TIMEOUT_MS
#define VOICE_MEMO_READ_TIMEOUT_MS 100
#endif
#ifndef VOICE_MEMO_NO_AUDIO_TIMEOUT_MS
#define VOICE_MEMO_NO_AUDIO_TIMEOUT_MS 2000
#endif

namespace VoiceMemoConfig {
enum class RecordingFormat { Pcm16, ImaAdpcm4 };
#ifdef VOICE_MEMO_PCM_BASELINE
inline constexpr RecordingFormat FORMAT = RecordingFormat::Pcm16;
#else
inline constexpr RecordingFormat FORMAT = RecordingFormat::VOICE_MEMO_FORMAT;
#endif
inline constexpr bool ADPCM = FORMAT == RecordingFormat::ImaAdpcm4;
inline constexpr uint32_t CODEC_BLOCK_BYTES = 256;
inline constexpr uint32_t CODEC_BLOCK_SAMPLES = (CODEC_BLOCK_BYTES - 4) * 2 + 1;
inline constexpr uint32_t WRITER_STACK_BYTES = ADPCM ? 6144 : 4096;

// Tune these together with MEMO latency, buffer high-water and clipping logs.
inline constexpr uint32_t SAMPLE_RATE = VOICE_MEMO_SAMPLE_RATE;
inline constexpr bool FILTER_ENABLED = VOICE_MEMO_FILTER_ENABLED;
inline constexpr uint32_t CHUNK_SECONDS = VOICE_MEMO_CHUNK_SECONDS;
inline constexpr size_t BLOCK_SAMPLES = VOICE_MEMO_BLOCK_SAMPLES;
inline constexpr uint16_t BUFFER_BLOCKS = VOICE_MEMO_BUFFER_BLOCKS;
inline constexpr uint32_t READ_TIMEOUT_MS = VOICE_MEMO_READ_TIMEOUT_MS;
inline constexpr uint32_t NO_AUDIO_TIMEOUT_MS = VOICE_MEMO_NO_AUDIO_TIMEOUT_MS;
inline constexpr uint32_t BYTES_PER_SAMPLE = sizeof(int16_t);  // Mono signed PCM stays 16-bit.
inline constexpr uint64_t CHUNK_SAMPLES = uint64_t{SAMPLE_RATE} * CHUNK_SECONDS;
inline constexpr uint64_t CHUNK_RETAINED_SAMPLES =
    ADPCM ? (CHUNK_SAMPLES + CODEC_BLOCK_SAMPLES - 1) / CODEC_BLOCK_SAMPLES * CODEC_BLOCK_SAMPLES : CHUNK_SAMPLES;
static_assert(CHUNK_RETAINED_SAMPLES <= (UINT32_MAX - 60) / BYTES_PER_SAMPLE);
static_assert(SAMPLE_RATE >= 8000 && SAMPLE_RATE <= 48000);
static_assert(CHUNK_SAMPLES > 0 && CHUNK_SAMPLES <= (UINT32_MAX - 36) / BYTES_PER_SAMPLE);
static_assert(BLOCK_SAMPLES > 0 && BLOCK_SAMPLES <= 4096);
static_assert(BUFFER_BLOCKS >= 4 && BUFFER_BLOCKS <= 256);
}  // namespace VoiceMemoConfig
