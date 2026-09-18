#include "VoiceMemoSession.h"

#include <Logging.h>
#include <Memory.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "VoiceMemoConfig.h"
#include "VoiceMemoWav.h"

#ifndef CROSSPOINT_VERSION
#define CROSSPOINT_VERSION "unknown"
#endif

namespace {
// Times chunk open/close/rename/finalize transitions into the rollover counter.
struct RolloverTimer {
  VoiceMemoMetrics* metrics;
  int64_t started = esp_timer_get_time();
  explicit RolloverTimer(VoiceMemoMetrics* sink) : metrics(sink) {}
  ~RolloverTimer() {
    if (metrics) metrics->addRollover(esp_timer_get_time() - started);
  }
};
}  // namespace

bool VoiceMemoSession::writeAudio(const void* data, size_t bytes) {
  const auto started = esp_timer_get_time();
  const size_t written = file.write(static_cast<const uint8_t*>(data), bytes);
  if (metrics_) metrics_->addWrite(esp_timer_get_time() - started);
  return written == bytes;
}

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
  closeMetrics();
  metricsUsed = metricsPendingRows = metricsDropped = 0;
  metricsFailureLogged = false;
  LOG_INF("MEMO", "Pause/Stop tail discard limit: %u samples (%.3f ms)",
          unsigned(VoiceMemoConfig::ADPCM ? VoiceMemoConfig::CODEC_BLOCK_SAMPLES - 1 : 0),
          VoiceMemoConfig::ADPCM ? 1000.0 * (VoiceMemoConfig::CODEC_BLOCK_SAMPLES - 1) / VoiceMemoConfig::SAMPLE_RATE
                                 : 0.0);
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
    // Reuse the diagnostic buffer before rows exist; keep startup metadata off the task stack.
    const int length = snprintf(
        metricsBuffer, sizeof(metricsBuffer),
        "{\"version\":3,\"event\":\"start\",\"sample_rate\":%lu,\"bits\":16,\"channels\":1,\"gain\":%.9g,"
        "\"format\":\"%s\",\"block_bytes\":%u,\"samples_per_block\":%u,\"filter\":\"%s\",\"gain_db\":%g,"
        "\"firmware\":\"%s\",\"block_samples\":%u,\"buffer_blocks\":%u}\n",
        static_cast<unsigned long>(VoiceMemoConfig::SAMPLE_RATE), 1.0, VoiceMemoConfig::ADPCM ? "ima_adpcm4" : "pcm16",
        VoiceMemoConfig::ADPCM ? VoiceMemoConfig::CODEC_BLOCK_BYTES : 2u,
        VoiceMemoConfig::ADPCM ? VoiceMemoConfig::CODEC_BLOCK_SAMPLES : 1u,
        VoiceMemoConfig::FILTER_ENABLED
            ? (VoiceMemoConfig::SAMPLE_RATE == 24000 ? "owner_sos_24000_v1" : "ellip_hp150_lp4500_v1")
            : "none",
        0.0, CROSSPOINT_VERSION, unsigned(VoiceMemoConfig::BLOCK_SAMPLES), unsigned(VoiceMemoConfig::BUFFER_BLOCKS));
    if (length < 0 || static_cast<size_t>(length) >= sizeof(metricsBuffer)) {
      LOG_ERR("MEMO", "Session metadata exceeds buffer");
      return false;
    }
    return journal(metricsBuffer);
  }
  LOG_ERR("MEMO", "Session identifiers exhausted");
  directory[0] = '\0';
  return false;
}

bool VoiceMemoSession::openChunk() {
  RolloverTimer rollover(metrics_);
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
  if (!writeAudio(header.data(), header.size())) {
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
  if (metrics_) metrics_->addEncode(elapsed);
  if (!encoded || bytes != VoiceMemoConfig::CODEC_BLOCK_BYTES || !writeAudio(encoder->encoded, bytes)) {
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
    if (!writeAudio(samples, bytes)) {
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
      LOG_INF("MEMO", "Discarded incomplete tail: %u samples (%.3f ms)", static_cast<unsigned>(encoder->pending),
              1000.0 * encoder->pending / VoiceMemoConfig::SAMPLE_RATE);
      encoder->pending = 0;
    }
  }
  return true;
}

bool VoiceMemoSession::closeChunk(bool complete) {
  if (!file) return true;
  RolloverTimer rollover(metrics_);
  const auto header = voiceMemoWavHeader(dataBytes, chunkSamples);
  const bool headerOk = file.seek(0) && writeAudio(header.data(), header.size());
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

bool VoiceMemoSession::openMetrics() {
  if (!directory[0]) return false;
  char path[64];
  snprintf(path, sizeof(path), "%s/metrics.csv", directory);
  metricsFile = Storage.open(path, O_WRONLY | O_CREAT | O_EXCL);
  if (!metricsFile) {
    LOG_ERR("MEMO", "Cannot open metrics.csv");
    return false;
  }
  const char* header = VoiceMemoMetrics::csvHeader();
  const size_t length = strlen(header);
  const auto started = esp_timer_get_time();
  const size_t written = metricsFile.write(reinterpret_cast<const uint8_t*>(header), length);
  if (metrics_) metrics_->addDiag(esp_timer_get_time() - started);
  if (written != length) {
    LOG_ERR("MEMO", "Cannot write metrics.csv header");
    metricsFile.close();
    return false;
  }
  metricsUsed = metricsPendingRows = 0;
  metricsFailureLogged = false;
  metricsOpen_ = true;
  return true;
}

bool VoiceMemoSession::appendMetrics(const char* row) {
  if (!metricsOpen_) return false;
  const size_t length = strlen(row);
  if (length == 0 || length + 1 > VoiceMemoMetricsConfig::ROW_MAX) {
    ++metricsDropped;
    return false;
  }
  // Buffer-full is the overflow policy: attempt one bounded flush, otherwise drop the row.
  if (metricsUsed + length > VoiceMemoMetricsConfig::BATCH_BYTES && !flushMetrics()) {
    ++metricsDropped;
    return false;
  }
  memcpy(metricsBuffer + metricsUsed, row, length);
  metricsUsed += length;
  ++metricsPendingRows;
  return true;
}

bool VoiceMemoSession::flushMetrics() {
  if (!metricsUsed) return true;
  if (!metricsOpen_) {
    metricsDropped += metricsPendingRows;
    metricsUsed = metricsPendingRows = 0;
    return false;
  }
  const auto started = esp_timer_get_time();
  const size_t written = metricsFile.write(reinterpret_cast<const uint8_t*>(metricsBuffer), metricsUsed);
  if (metrics_) metrics_->addDiag(esp_timer_get_time() - started);
  const bool ok = written == metricsUsed;
  if (!ok) {
    metricsDropped += metricsPendingRows;
    if (!metricsFailureLogged) {
      LOG_ERR("MEMO", "metrics.csv write failed; telemetry incomplete");
      metricsFailureLogged = true;
    }
  }
  metricsUsed = metricsPendingRows = 0;
  return ok;
}

bool VoiceMemoSession::closeMetrics() {
  if (!metricsOpen_) return true;
  const bool flushed = flushMetrics();
  const bool closed = metricsFile.close();
  metricsOpen_ = false;
  return flushed && closed;
}
