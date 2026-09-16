// Run with test/voice_memo/run_host_tests.sh (actual session writer, fault-injected storage).
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../../src/activities/tools/VoiceMemoConfig.h"
#include "../../src/activities/tools/VoiceMemoSession.h"

static uint32_t read32(const std::vector<uint8_t>& bytes, size_t offset) {
  return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 | uint32_t(bytes[offset + 2]) << 16 |
         uint32_t(bytes[offset + 3]) << 24;
}
static void feed(VoiceMemoSession& session, uint64_t count, uint64_t start = 0) {
  std::vector<int16_t> block(4093);
  while (count) {
    const size_t take = std::min<uint64_t>(count, block.size());
    for (size_t i = 0; i < take; ++i) block[i] = static_cast<int16_t>((start + i) % 32767);
    assert(session.append(block.data(), take));
    start += take;
    count -= take;
  }
}
static std::string chunkPath(const VoiceMemoSession& session, unsigned index, const char* suffix) {
  char path[80];
  snprintf(path, sizeof(path), "%s/chunk-%06u.%s", session.path(), index, suffix);
  return path;
}
static void exportSession(const char* directory) {
  std::filesystem::create_directories(directory);
  for (const auto& [path, bytes] : fake::files) {
    std::ofstream out(std::filesystem::path(directory) / std::filesystem::path(path).filename(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    assert(out.good());
  }
}
int main(int argc, char** argv) {
  using namespace VoiceMemoConfig;
  fake::reset();
  if (argc == 4 && std::string(argv[1]) == "--encode") {
    VoiceMemoSession recording;
    assert(recording.begin());
    std::ifstream input(argv[2], std::ios::binary);
    assert(input.good());
    std::vector<int16_t> pcm(1024);
    while (input.read(reinterpret_cast<char*>(pcm.data()), pcm.size() * 2) || input.gcount()) {
      assert(input.gcount() % 2 == 0);
      assert(recording.append(pcm.data(), input.gcount() / 2));
    }
    assert(input.eof());
    assert(recording.finishChunk(true) && recording.event("stop"));
    exportSession(argv[3]);
    return 0;
  }
  VoiceMemoSession session;
  assert(session.begin());
  assert(std::string(session.path()) == "/recordings/session-000001");
  const uint64_t total = CHUNK_RETAINED_SAMPLES * 2 + CODEC_BLOCK_SAMPLES + 17;
  feed(session, total);
  assert(session.finishChunk(true));
  assert(session.event("pause"));
  uint64_t offset = 0;
  for (unsigned i = 1; i <= 3; ++i) {
    const auto& wav = fake::files.at(chunkPath(session, i, "wav"));
    const uint32_t samples = i == 3 ? CODEC_BLOCK_SAMPLES + (ADPCM ? 0 : 17) : CHUNK_RETAINED_SAMPLES;
    if constexpr (ADPCM) {
      const uint32_t bytes = (samples + CODEC_BLOCK_SAMPLES - 1) / CODEC_BLOCK_SAMPLES * CODEC_BLOCK_BYTES;
      assert(wav.size() == 60 + bytes && read32(wav, 4) == 52 + bytes);
      assert(read32(wav, 48) == samples && read32(wav, 56) == bytes);
      for (size_t pos = 60; pos < wav.size(); pos += CODEC_BLOCK_BYTES) {
        assert(wav[pos + 2] <= 88 && wav[pos + 3] == 0);
        const uint16_t first = wav[pos] | uint16_t(wav[pos + 1]) << 8;
        assert(first == (offset + (pos - 60) / CODEC_BLOCK_BYTES * CODEC_BLOCK_SAMPLES) % 32767);
      }
    } else {
      assert(wav.size() == 44 + samples * 2);
      assert(read32(wav, 24) == SAMPLE_RATE && read32(wav, 28) == SAMPLE_RATE * 2);
      assert(read32(wav, 40) == samples * 2 && read32(wav, 4) == 36 + samples * 2);
      for (uint32_t j = 0; j < samples; ++j) {
        const uint16_t value = wav[44 + j * 2] | uint16_t(wav[45 + j * 2]) << 8;
        assert(value == (offset + j) % 32767);
      }
    }
    offset += samples;
  }
  assert(offset == total - (ADPCM ? 17 : 0) && session.samples() == offset);
  assert(session.event("resume", 1234));
  feed(session, CODEC_BLOCK_SAMPLES + 1, total);
  assert(session.finishChunk(true));
  assert(session.event("stop"));
  assert(fake::files.at(chunkPath(session, 4, "wav")).size() == (ADPCM ? 316 : 44 + (CODEC_BLOCK_SAMPLES + 1) * 2));
  const auto& journal = fake::files.at(std::string(session.path()) + "/session.jsonl");
  const std::string text(journal.begin(), journal.end());
  assert(text.find("\"event\":\"resume\"") != std::string::npos);
  assert(text.find("\"gap_ms\":1234") != std::string::npos);
  if (argc == 2) exportSession(argv[1]);
  VoiceMemoSession second;
  assert(second.begin());
  assert(std::string(second.path()) != session.path());

  // Exact-boundary Stop doesn't create an empty next chunk; immediate Stop creates none.
  fake::reset();
  VoiceMemoSession exact;
  assert(exact.begin());
  feed(exact, CHUNK_RETAINED_SAMPLES);
  assert(exact.finishChunk(true));
  assert(exact.chunk() == 1);
  assert(!fake::files.count(chunkPath(exact, 2, "part")));
  VoiceMemoSession empty;
  assert(empty.begin());
  assert(empty.finishChunk(true));
  assert(empty.chunk() == 0);

  for (uint32_t count : {1u, CODEC_BLOCK_SAMPLES - 1, CODEC_BLOCK_SAMPLES, CODEC_BLOCK_SAMPLES + 1}) {
    fake::reset();
    VoiceMemoSession shortMemo;
    assert(shortMemo.begin());
    feed(shortMemo, count);
    assert(shortMemo.finishChunk(true) && shortMemo.event("stop"));
    const auto retained = ADPCM ? count / CODEC_BLOCK_SAMPLES * CODEC_BLOCK_SAMPLES : count;
    assert(shortMemo.samples() == retained);
    assert(shortMemo.chunk() == (retained ? 1 : 0));
    assert(!fake::files.count(chunkPath(shortMemo, 1, "part")));
    const auto& entries = fake::files.at(std::string(shortMemo.path()) + "/session.jsonl");
    const std::string journal(entries.begin(), entries.end());
    if (count != retained)
      assert(journal.find("\"discarded_samples\":" + std::to_string(count - retained) + ",") != std::string::npos);
    else
      assert(journal.find("\"event\":\"discard\"") == std::string::npos);
    if (argc == 2) {
      const auto directory = std::filesystem::path(argv[1]) / ("short-" + std::to_string(count));
      exportSession(directory.c_str());
    }
  }

  // A later failed write/finalize leaves earlier WAVs intact and the current chunk partial.
  for (const char* operation : {"write", "seek", "close", "rename"}) {
    fake::reset();
    VoiceMemoSession failing;
    assert(failing.begin());
    feed(failing, CHUNK_RETAINED_SAMPLES + CODEC_BLOCK_SAMPLES);
    fake::fault = operation;
    if (std::strcmp(operation, "write") == 0) {
      const std::vector<int16_t> values(CODEC_BLOCK_SAMPLES, 1);
      assert(!failing.append(values.data(), values.size()));
      assert(!failing.finishChunk(false));
    } else
      assert(!failing.finishChunk(true));
    assert(fake::files.count(chunkPath(failing, 1, "wav")));
    assert(fake::files.count(chunkPath(failing, 2, "part")));
    assert(!fake::files.count(chunkPath(failing, 2, "wav")));
  }
  fake::reset();
  VoiceMemoSession failedOpen;
  assert(failedOpen.begin());
  fake::fault = "open";
  const std::vector<int16_t> values(CODEC_BLOCK_SAMPLES, 1);
  assert(!failedOpen.append(values.data(), values.size()));
  assert(failedOpen.finishChunk(false));
  fake::reset();
  VoiceMemoSession failedJournal;
  assert(failedJournal.begin());
  feed(failedJournal, CODEC_BLOCK_SAMPLES);
  fake::fault = "write";
  fake::skip = 1;  // WAV header succeeds, journal write fails after audio publication.
  assert(!failedJournal.finishChunk(true));
  assert(fake::files.count(chunkPath(failedJournal, 1, "wav")));
  // Capture failures keep completed blocks partial, and never claim intentional discard.
  fake::reset();
  VoiceMemoSession captureError;
  assert(captureError.begin());
  feed(captureError, CODEC_BLOCK_SAMPLES + 1);
  assert(captureError.finishChunk(false));
  assert(captureError.event("incomplete"));
  assert(fake::files.count(chunkPath(captureError, 1, "part")));
  assert(!fake::files.count(chunkPath(captureError, 1, "wav")));
  const auto& failureJournal = fake::files.at(std::string(captureError.path()) + "/session.jsonl");
  assert(std::string(failureJournal.begin(), failureJournal.end()).find("discard") == std::string::npos);

  if constexpr (ADPCM) {
    // A sub-block Pause publishes nothing; Resume starts fresh without carrying the old tail.
    fake::reset();
    VoiceMemoSession tinyPause;
    assert(tinyPause.begin());
    feed(tinyPause, 504);
    assert(tinyPause.finishChunk(true) && tinyPause.event("pause"));
    assert(tinyPause.chunk() == 0 && tinyPause.samples() == 0);
    assert(tinyPause.event("resume", 100));
    feed(tinyPause, 506, 1000);
    assert(tinyPause.finishChunk(true) && tinyPause.event("stop"));
    assert(tinyPause.chunk() == 1 && tinyPause.samples() == 505);
    const auto& wav = fake::files.at(chunkPath(tinyPause, 1, "wav"));
    assert((wav[60] | uint16_t(wav[61]) << 8) == 1000);
    if (argc == 2) exportSession((std::filesystem::path(argv[1]) / "tiny-pause").c_str());

    // Losing the discard journal entry is a storage failure, not a successful tiny recording.
    for (uint32_t count : {1u, 506u}) {
      fake::reset();
      VoiceMemoSession failedDiscard;
      assert(failedDiscard.begin());
      feed(failedDiscard, count);
      fake::fault = "write";
      fake::skip = count >= CODEC_BLOCK_SAMPLES ? 2 : 0;  // Header and chunk entry, then discard entry.
      assert(!failedDiscard.finishChunk(true));
      assert(failedDiscard.samples() == count / CODEC_BLOCK_SAMPLES * CODEC_BLOCK_SAMPLES);
      assert(fake::files.count(chunkPath(failedDiscard, 1, "wav")) == (count >= CODEC_BLOCK_SAMPLES));
    }

    // Five minutes alone cannot force a partial block to be padded or discarded at rollover.
    fake::reset();
    VoiceMemoSession boundary;
    assert(boundary.begin());
    feed(boundary, CHUNK_SAMPLES);
    assert(!fake::files.count(chunkPath(boundary, 1, "wav")));
    assert(boundary.samples() == CHUNK_SAMPLES / CODEC_BLOCK_SAMPLES * CODEC_BLOCK_SAMPLES);
    feed(boundary, CHUNK_RETAINED_SAMPLES - CHUNK_SAMPLES, CHUNK_SAMPLES);
    assert(boundary.samples() == CHUNK_RETAINED_SAMPLES);
    assert(fake::files.count(chunkPath(boundary, 1, "wav")));
    assert(boundary.finishChunk(true));
  }
  fake::reset();
  VoiceMemoSession failedStart;
  fake::fault = "mkdir";
  assert(!failedStart.begin());
  puts("Session checks passed: sample order, rollover, pause/resume, empty stop, unique paths and storage failures.");
}
