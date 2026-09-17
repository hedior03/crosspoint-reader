#include "VoiceMemoSession.h"

#include <Logging.h>
#include <Memory.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "VoiceMemoConfig.h"
#include "VoiceMemoWav.h"

bool VoiceMemoSession::journal(const char* line) {
  char path[64];
  snprintf(path, sizeof(path), "%s/session.jsonl", directory);
  journalFile = Storage.open(path, O_WRONLY | O_CREAT | O_APPEND);
  if (!journalFile) {
    LOG_ERR("MEMO", "Cannot open session journal");
    return false;
  }
  const size_t length = strlen(line);
  const bool written = journalFile.write(reinterpret_cast<const uint8_t*>(line), length) == length;
  // Check close before claiming this state was persisted.
  const bool closed = journalFile.close();
  if (!written || !closed) LOG_ERR("MEMO", "Session journal write/close failed");
  return written && closed;
}

bool VoiceMemoSession::begin() {
  directory[0] = '\0';
  releaseEncoder();
  if constexpr (VoiceMemoConfig::ADPCM) {
    // Once per session: codec staging and search scratch cannot live on the writer stack.
    encoder = makeUniqueNoThrow<Encoder>();
    if (encoder) encoder->context.reset(adpcm_create_context(1, VoiceMemoConfig::SAMPLE_RATE, 0, NOISE_SHAPING_OFF));
    if (!encoder || !encoder->context) {
      LOG_ERR("MEMO", "Cannot allocate ADPCM encoder");
      releaseEncoder();
      return false;
    }
  }
  totalSamples = 0;
  chunkSamples = 0;
  chunkNumber = 0;
  if (!Storage.ready() || !Storage.ensureDirectoryExists("/recordings")) {
    LOG_ERR("MEMO", "Recordings directory unavailable");
    return false;
  }
  // ponytail: scan existing sessions; persist a next-ID hint if large directories make startup slow.
  for (unsigned number = 1; number <= 999999; ++number) {
    snprintf(directory, sizeof(directory), "/recordings/session-%06u", number);
    if (Storage.exists(directory)) continue;
    if (!Storage.mkdir(directory)) {
      LOG_ERR("MEMO", "Cannot create session directory");
      return false;
    }
    char line[224];
    snprintf(line, sizeof(line),
             "{\"version\":3,\"event\":\"start\",\"sample_rate\":%lu,\"bits\":16,\"channels\":1,\"gain\":%.9g,"
             "\"format\":\"%s\",\"block_bytes\":%u,\"samples_per_block\":%u,\"filter\":\"%s\",\"gain_db\":%g}\n",
             static_cast<unsigned long>(VoiceMemoConfig::SAMPLE_RATE), 1.0,
             VoiceMemoConfig::ADPCM ? "ima_adpcm4" : "pcm16",
             VoiceMemoConfig::ADPCM ? VoiceMemoConfig::CODEC_BLOCK_BYTES : 2u,
             VoiceMemoConfig::ADPCM ? VoiceMemoConfig::CODEC_BLOCK_SAMPLES : 1u,
             VoiceMemoConfig::FILTER_ENABLED ? "ellip_hp150_lp4500_v1" : "none", 0.0);
    return journal(line);
  }
  LOG_ERR("MEMO", "Session identifiers exhausted");
  directory[0] = '\0';
  return false;
}

bool VoiceMemoSession::openChunk() {
  if (chunkNumber == 999999) {
    LOG_ERR("MEMO", "Chunk identifiers exhausted");
    return false;
  }
  ++chunkNumber;
  chunkSamples = dataBytes = 0;
  encodeUs = maxEncodeUs = 0;
  writeFailed = false;
  snprintf(partialPath, sizeof(partialPath), "%s/chunk-%06lu.part", directory, static_cast<unsigned long>(chunkNumber));
  snprintf(savedPath, sizeof(savedPath), "%s/chunk-%06lu.wav", directory, static_cast<unsigned long>(chunkNumber));
  if (Storage.exists(savedPath)) {
    LOG_ERR("MEMO", "Refusing to overwrite chunk");
    return false;
  }
  file = Storage.open(partialPath, O_RDWR | O_CREAT | O_EXCL);
  if (!file) {
    LOG_ERR("MEMO", "Cannot open chunk");
    return false;
  }
  const auto header = voiceMemoWavHeader(0);
  if (file.write(header.data(), header.size()) != header.size()) {
    writeFailed = true;
    LOG_ERR("MEMO", "Cannot reserve WAV header");
    return false;
  }
  return true;
}

bool VoiceMemoSession::append(const int16_t* samples, size_t count) {
  if constexpr (VoiceMemoConfig::ADPCM) return writeSamples(samples, count);
  while (count) {
    if (file && chunkSamples == VoiceMemoConfig::CHUNK_SAMPLES && !finishChunk(true)) return false;
    if (!file && !openChunk()) return false;
    const auto take = std::min(count, static_cast<size_t>(VoiceMemoConfig::CHUNK_SAMPLES - chunkSamples));
    if (!writeSamples(samples, take)) return false;
    samples += take;
    count -= take;
  }
  return true;
}

bool VoiceMemoSession::writeCodecBlock() {
  if (!file && !openChunk()) return false;
  size_t bytes = 0;
  const auto started = esp_timer_get_time();
  const bool encoded = adpcm_encode_block(encoder->context.get(), encoder->encoded, &bytes, encoder->pcm,
                                          VoiceMemoConfig::CODEC_BLOCK_SAMPLES);
  const uint64_t elapsed = esp_timer_get_time() - started;
  encodeUs += elapsed;
  maxEncodeUs = std::max(maxEncodeUs, elapsed);
  if (!encoded || bytes != VoiceMemoConfig::CODEC_BLOCK_BYTES || file.write(encoder->encoded, bytes) != bytes) {
    LOG_ERR("MEMO", "ADPCM encoding/write failed; retaining partial chunk");
    writeFailed = true;
    return false;
  }
  dataBytes += bytes;
  chunkSamples += VoiceMemoConfig::CODEC_BLOCK_SAMPLES;
  totalSamples += VoiceMemoConfig::CODEC_BLOCK_SAMPLES;
  encoder->pending = 0;
  return chunkSamples < VoiceMemoConfig::CHUNK_RETAINED_SAMPLES || closeChunk(true);
}

bool VoiceMemoSession::writeSamples(const int16_t* samples, size_t count) {
  if constexpr (!VoiceMemoConfig::ADPCM) {
    const size_t bytes = count * sizeof(int16_t);
    if (file.write(reinterpret_cast<const uint8_t*>(samples), bytes) != bytes) {
      LOG_ERR("MEMO", "Short SD write; retaining partial chunk");
      writeFailed = true;
      return false;
    }
    dataBytes += bytes;
    chunkSamples += count;
    totalSamples += count;
    return true;
  }
  while (count) {
    const size_t take = std::min<size_t>(count, VoiceMemoConfig::CODEC_BLOCK_SAMPLES - encoder->pending);
    std::copy_n(samples, take, encoder->pcm + encoder->pending);
    encoder->pending += take;
    samples += take;
    count -= take;
    if (encoder->pending == VoiceMemoConfig::CODEC_BLOCK_SAMPLES && !writeCodecBlock()) return false;
  }
  return true;
}

bool VoiceMemoSession::finishChunk(bool complete) {
  if (!closeChunk(complete)) return false;
  if constexpr (VoiceMemoConfig::ADPCM) {
    if (complete && encoder->pending) {
      char line[144];
      snprintf(line, sizeof(line),
               "{\"event\":\"discard\",\"sample\":%llu,\"discarded_samples\":%u,\"reason\":\"incomplete_tail\"}\n",
               static_cast<unsigned long long>(totalSamples), static_cast<unsigned>(encoder->pending));
      if (!journal(line)) return false;
      LOG_INF("MEMO", "Discarded incomplete tail: %u samples", static_cast<unsigned>(encoder->pending));
      encoder->pending = 0;
    }
  }
  return true;
}

bool VoiceMemoSession::closeChunk(bool complete) {
  if (!file) return true;
  const auto header = voiceMemoWavHeader(dataBytes, chunkSamples);
  const bool headerOk = file.seek(0) && file.write(header.data(), header.size()) == header.size();
  file.flush();
  const bool closed = file.close();
  if (!headerOk || !closed) {
    LOG_ERR("MEMO", "WAV finalization failed");
    return false;
  }
  if (writeFailed) return false;
  if (!complete) return true;  // Preserve .part after capture/storage errors.
  if (!Storage.rename(partialPath, savedPath)) {
    LOG_ERR("MEMO", "Cannot publish WAV chunk");
    return false;
  }
  LOG_INF("MEMO", "codec=%s samples=%lu bytes=%lu encode_us=%llu max_encode_us=%llu",
          VoiceMemoConfig::ADPCM ? "ima_adpcm4" : "pcm16", static_cast<unsigned long>(chunkSamples),
          static_cast<unsigned long>(dataBytes), static_cast<unsigned long long>(encodeUs),
          static_cast<unsigned long long>(maxEncodeUs));
  char line[160];
  snprintf(line, sizeof(line),
           "{\"event\":\"chunk\",\"chunk\":%lu,\"samples\":%lu,\"end_sample\":%llu,"
           "\"encoded_bytes\":%lu,\"padding_samples\":0}\n",
           static_cast<unsigned long>(chunkNumber), static_cast<unsigned long>(chunkSamples),
           static_cast<unsigned long long>(totalSamples), static_cast<unsigned long>(dataBytes));
  return journal(line);
}

bool VoiceMemoSession::event(const char* name, uint64_t gapMs) {
  if (!directory[0]) return false;
  char line[192];
  // Names are fixed internal literals, never user input.
  snprintf(line, sizeof(line), "{\"event\":\"%s\",\"sample\":%llu,\"gap_ms\":%llu}\n", name,
           static_cast<unsigned long long>(totalSamples), static_cast<unsigned long long>(gapMs));
  return journal(line);
}
