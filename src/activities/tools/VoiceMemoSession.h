#pragma once
#include <HalStorage.h>
#include <adpcm-lib.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "VoiceMemoConfig.h"
#include "VoiceMemoMetrics.h"

// File ownership belongs to the writer while it runs, otherwise to the activity.
class VoiceMemoSession {
 public:
  bool begin();
  bool append(const int16_t* samples, size_t count);
  bool finishChunk(bool complete);
  void releaseEncoder() { encoder.reset(); }
  bool event(const char* name, uint64_t gapMs = 0);
  const char* path() const { return directory; }
  uint64_t samples() const { return totalSamples; }
  uint32_t chunk() const { return chunkNumber; }

  // Diagnostic CSV is best-effort: failures never affect audio results. The timing
  // sink may be null (host checks without metrics).
  void setMetrics(VoiceMemoMetrics* metrics) { metrics_ = metrics; }
  bool openMetrics();
  bool appendMetrics(const char* row);
  bool flushMetrics();
  bool closeMetrics();
  bool metricsOpen() const { return metricsOpen_; }
  size_t pendingMetricsRows() const { return metricsPendingRows; }

 private:
  struct Encoder {
    std::unique_ptr<void, decltype(&adpcm_free_context)> context{nullptr, adpcm_free_context};
    int16_t pcm[VoiceMemoConfig::CODEC_BLOCK_SAMPLES]{};
    uint8_t encoded[VoiceMemoConfig::CODEC_BLOCK_BYTES]{};
    size_t pending = 0;
  };
  std::unique_ptr<Encoder> encoder;
  bool writeSamples(const int16_t* samples, size_t count);
  bool writeCodecBlock();
  bool closeChunk(bool complete);
  bool openChunk();
  bool journal(const char* line);
  // Times the write (including HalStorage's mutex wait) into the metrics sink.
  bool writeAudio(const void* data, size_t bytes);
  HalFile file;
  HalFile journalFile;
  HalFile metricsFile;
  VoiceMemoMetrics* metrics_ = nullptr;
  char directory[40]{};
  char partialPath[64]{};
  char savedPath[64]{};
  char metricsBuffer[VoiceMemoMetricsConfig::BATCH_BYTES]{};
  size_t metricsUsed = 0;
  size_t metricsPendingRows = 0;
  uint32_t metricsDropped = 0;
  bool metricsOpen_ = false;
  bool metricsFailureLogged = false;
  uint64_t totalSamples = 0;
  uint32_t chunkSamples = 0;
  uint32_t chunkNumber = 0;
  uint32_t dataBytes = 0;
  bool writeFailed = false;
  uint64_t encodeUs = 0;
  uint64_t maxEncodeUs = 0;
};
