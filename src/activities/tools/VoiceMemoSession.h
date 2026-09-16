#pragma once
#include <HalStorage.h>

#include <cstddef>
#include <cstdint>

// File ownership belongs to the writer while it runs, otherwise to the activity.
class VoiceMemoSession {
 public:
  bool begin();
  bool append(const int16_t* samples, size_t count);
  bool finishChunk(bool complete);
  bool event(const char* name, uint64_t gapMs = 0);
  const char* path() const { return directory; }
  uint64_t samples() const { return totalSamples; }
  uint32_t chunk() const { return chunkNumber; }

 private:
  bool openChunk();
  bool journal(const char* line);
  HalFile file;
  HalFile journalFile;
  char directory[40]{};
  char partialPath[64]{};
  char savedPath[64]{};
  uint64_t totalSamples = 0;
  uint32_t chunkSamples = 0;
  uint32_t chunkNumber = 0;
};
