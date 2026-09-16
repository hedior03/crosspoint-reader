#include "VoiceMemoSession.h"

#include <Logging.h>

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
    char line[192];
    snprintf(line, sizeof(line),
             "{\"version\":1,\"event\":\"start\",\"sample_rate\":%lu,\"bits\":16,\"channels\":1,\"gain\":%ld}\n",
             static_cast<unsigned long>(VoiceMemoConfig::SAMPLE_RATE), static_cast<long>(VoiceMemoConfig::GAIN));
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
  chunkSamples = 0;
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
    LOG_ERR("MEMO", "Cannot reserve WAV header");
    return false;
  }
  return true;
}

bool VoiceMemoSession::append(const int16_t* samples, size_t count) {
  while (count) {
    if (file && chunkSamples == VoiceMemoConfig::CHUNK_SAMPLES && !finishChunk(true)) return false;
    if (!file && !openChunk()) return false;
    const auto take = std::min(count, static_cast<size_t>(VoiceMemoConfig::CHUNK_SAMPLES - chunkSamples));
    const size_t bytes = take * sizeof(int16_t);
    if (file.write(reinterpret_cast<const uint8_t*>(samples), bytes) != bytes) {
      LOG_ERR("MEMO", "Short SD write; retaining partial chunk");
      return false;
    }
    chunkSamples += take;
    totalSamples += take;
    samples += take;
    count -= take;
  }
  return true;
}

bool VoiceMemoSession::finishChunk(bool complete) {
  if (!file) return true;
  const auto header = voiceMemoWavHeader(chunkSamples * sizeof(int16_t));
  const bool headerOk = file.seek(0) && file.write(header.data(), header.size()) == header.size();
  file.flush();
  const bool closed = file.close();
  if (!headerOk || !closed) {
    LOG_ERR("MEMO", "WAV finalization failed");
    return false;
  }
  if (!complete) return true;  // Preserve .part after capture/storage errors.
  if (!Storage.rename(partialPath, savedPath)) {
    LOG_ERR("MEMO", "Cannot publish WAV chunk");
    return false;
  }
  char line[192];
  snprintf(line, sizeof(line), "{\"event\":\"chunk\",\"chunk\":%lu,\"samples\":%lu,\"end_sample\":%llu}\n",
           static_cast<unsigned long>(chunkNumber), static_cast<unsigned long>(chunkSamples),
           static_cast<unsigned long long>(totalSamples));
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
