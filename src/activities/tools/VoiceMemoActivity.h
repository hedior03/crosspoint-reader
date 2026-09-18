#pragma once
#include <BoardConfig.h>
#if FREEINK_DEVICE_PAPERMONO
#include <HalMicrophone.h>
#include <freertos/queue.h>

#include <atomic>
#include <memory>

#include "VoiceMemoConfig.h"
#include "VoiceMemoMetrics.h"
#include "VoiceMemoSession.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"

class VoiceMemoActivity final : public Activity, private UiAppHost {
 public:
  VoiceMemoActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("VoiceMemo", renderer, input), UiAppHost(renderer) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return captureTask || writerTask; }

 private:
  enum class Status { Ready, Starting, Recording, Saving, Paused, Saved, Failed };
  enum class Error { None, Setup, Capture, BufferFull, StorageIo, Processing };
  struct Block {
    size_t count = 0;
    int16_t samples[VoiceMemoConfig::BLOCK_SAMPLES];
  };
  static constexpr freeink::ui::ActionId ACTION_RECORD = 1;
  static constexpr freeink::ui::ActionId ACTION_STOP = 2;
  static constexpr freeink::ui::ActionId ACTION_PAUSE = 3;
  static constexpr freeink::ui::ActionId ACTION_RESUME = 4;
  void startRecording(bool resume = false);
  void stopRecording(bool pause = false);
  void capture();
  void write();
  void pollMetrics(bool final);
  void sampleEnvironment();
  void finishWorkers();
  void releaseBuffers();
  void fail(Error reason);
  void buildScreen(UiScreen& screen);
  HalMicrophone mic;
  VoiceMemoSession session;
  VoiceMemoMetrics telemetry;
  std::unique_ptr<Block[]> blocks;
  QueueHandle_t freeBlocks = nullptr;
  QueueHandle_t filledBlocks = nullptr;
  TaskHandle_t captureTask = nullptr;
  TaskHandle_t writerTask = nullptr;
  std::atomic<bool> stop{false};
  std::atomic<bool> captureDone{true};
  std::atomic<bool> writerDone{true};
  std::atomic<Error> error{Error::None};
  std::atomic<Status> status{Status::Ready};
  std::atomic<uint32_t> currentChunk{0};
  bool sessionActive = false;
  bool pauseRequested = false;
  bool resuming = false;
  int64_t stoppedAt = 0;
  uint64_t resumeGapMs = 0;
  uint64_t clipped = 0;
  uint32_t peak = 0;
  uint32_t highWater = 0;
  uint32_t displayedChunk = 0;
  uint32_t nextEnvironmentMs = 0;
  bool metricsLossReported = false;
};
#endif
