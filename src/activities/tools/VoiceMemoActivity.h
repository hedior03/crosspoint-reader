#pragma once
#include <BoardConfig.h>
#if FREEINK_DEVICE_PAPERMONO
#include <HalMicrophone.h>
#include <HalStorage.h>

#include <atomic>

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
  bool preventAutoSleep() override { return task != nullptr; }

 private:
  enum class Status { Ready, Starting, Recording, Saving, Saved, Failed };
  static constexpr uint32_t MAX_SECONDS = 300;
  static constexpr freeink::ui::ActionId ACTION_RECORD = 1;
  static constexpr freeink::ui::ActionId ACTION_STOP = 2;
  void startRecording();
  void stopRecording();
  void record();
  void finishWorker();
  void buildScreen(UiScreen& screen);
  bool prepareFile();
  HalMicrophone mic;
  HalFile file;
  TaskHandle_t task = nullptr;
  std::atomic<bool> stop{false};
  std::atomic<bool> done{false};
  std::atomic<uint32_t> samplesWritten{0};
  bool saved = false;
  std::atomic<Status> status{Status::Ready};
  char partialPath[48]{};
  char savedPath[48]{};
  int16_t samples[1024]{};
  uint32_t lastPaint = 0;
};
#endif
