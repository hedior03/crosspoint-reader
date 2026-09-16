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
int main(int argc, char** argv) {
  using namespace VoiceMemoConfig;
  fake::reset();
  VoiceMemoSession session;
  assert(session.begin());
  assert(std::string(session.path()) == "/recordings/session-000001");
  const uint64_t total = CHUNK_SAMPLES * 2 + 17;
  feed(session, total);
  assert(session.finishChunk(true));
  assert(session.event("pause"));
  uint64_t offset = 0;
  for (unsigned i = 1; i <= 3; ++i) {
    const auto& wav = fake::files.at(chunkPath(session, i, "wav"));
    const uint32_t samples = i == 3 ? 17 : CHUNK_SAMPLES;
    assert(wav.size() == 44 + samples * 2);
    assert(read32(wav, 24) == SAMPLE_RATE && read32(wav, 28) == SAMPLE_RATE * 2);
    assert(read32(wav, 40) == samples * 2 && read32(wav, 4) == 36 + samples * 2);
    for (uint32_t j = 0; j < samples; ++j) {
      const uint16_t value = wav[44 + j * 2] | uint16_t(wav[45 + j * 2]) << 8;
      assert(value == (offset + j) % 32767);
    }
    offset += samples;
  }
  assert(offset == total && session.samples() == total);
  assert(session.event("resume", 1234));
  feed(session, 3, total);
  assert(session.finishChunk(true));
  assert(session.event("stop"));
  assert(fake::files.at(chunkPath(session, 4, "wav")).size() == 50);
  const auto& journal = fake::files.at(std::string(session.path()) + "/session.jsonl");
  const std::string text(journal.begin(), journal.end());
  assert(text.find("\"event\":\"resume\"") != std::string::npos);
  assert(text.find("\"gap_ms\":1234") != std::string::npos);
  if (argc == 2) {
    std::filesystem::create_directories(argv[1]);
    for (const auto& [path, bytes] : fake::files) {
      std::ofstream out(std::filesystem::path(argv[1]) / std::filesystem::path(path).filename(), std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
      assert(out.good());
    }
  }
  VoiceMemoSession second;
  assert(second.begin());
  assert(std::string(second.path()) != session.path());

  // Exact-boundary Stop doesn't create an empty next chunk; immediate Stop creates none.
  fake::reset();
  VoiceMemoSession exact;
  assert(exact.begin());
  feed(exact, CHUNK_SAMPLES);
  assert(exact.finishChunk(true));
  assert(exact.chunk() == 1);
  assert(!fake::files.count(chunkPath(exact, 2, "part")));
  VoiceMemoSession empty;
  assert(empty.begin());
  assert(empty.finishChunk(true));
  assert(empty.chunk() == 0);

  // A later failed write/finalize leaves earlier WAVs intact and the current chunk partial.
  for (const char* operation : {"write", "seek", "close", "rename"}) {
    fake::reset();
    VoiceMemoSession failing;
    assert(failing.begin());
    feed(failing, CHUNK_SAMPLES + 1);
    fake::fault = operation;
    if (std::strcmp(operation, "write") == 0) {
      const int16_t values[2] = {1, 2};
      assert(!failing.append(values, 2));
      assert(failing.finishChunk(false));
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
  const int16_t value = 1;
  assert(!failedOpen.append(&value, 1));
  assert(failedOpen.finishChunk(false));
  fake::reset();
  VoiceMemoSession failedJournal;
  assert(failedJournal.begin());
  feed(failedJournal, 3);
  fake::fault = "write";
  fake::skip = 1;  // WAV header succeeds, journal write fails after audio publication.
  assert(!failedJournal.finishChunk(true));
  assert(fake::files.count(chunkPath(failedJournal, 1, "wav")));
  fake::reset();
  VoiceMemoSession failedStart;
  fake::fault = "mkdir";
  assert(!failedStart.begin());
  puts("Session checks passed: sample order, rollover, pause/resume, empty stop, unique paths and storage failures.");
}
