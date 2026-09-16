#include "VoiceMemoActivity.h"
#if FREEINK_DEVICE_PAPERMONO
#include <I18n.h>
#include <Memory.h>
#include <esp_memory_utils.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdio>

#include "VoiceMemoGain.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;
namespace cfg = VoiceMemoConfig;

void VoiceMemoActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock;
  resetUi();
  app.setScreen([](UiScreen& screen, void* user) { static_cast<VoiceMemoActivity*>(user)->buildScreen(screen); }, this);
  app.on(
      ACTION_RECORD, [](const fui::ActionEvent&, void* p) { static_cast<VoiceMemoActivity*>(p)->startRecording(); },
      this);
  app.on(
      ACTION_STOP, [](const fui::ActionEvent&, void* p) { static_cast<VoiceMemoActivity*>(p)->stopRecording(); }, this);
  app.on(
      ACTION_PAUSE, [](const fui::ActionEvent&, void* p) { static_cast<VoiceMemoActivity*>(p)->stopRecording(true); },
      this);
  app.on(
      ACTION_RESUME, [](const fui::ActionEvent&, void* p) { static_cast<VoiceMemoActivity*>(p)->startRecording(true); },
      this);
  requestUpdate();
}

void VoiceMemoActivity::fail(Error reason) {
  auto expected = Error::None;
  error.compare_exchange_strong(expected, reason);
  stop.store(true);
  HalMicrophone::stopIndicator();
}

void VoiceMemoActivity::releaseBuffers() {
  if (freeBlocks) vQueueDelete(freeBlocks);
  if (filledBlocks) vQueueDelete(filledBlocks);
  freeBlocks = filledBlocks = nullptr;
  blocks.reset();
}

void VoiceMemoActivity::startRecording(bool resume) {
  if (captureTask || writerTask || (resume ? status != Status::Paused : sessionActive)) return;
  RenderLock lock;
  status = Status::Starting;
  resuming = resume;
  pauseRequested = false;
  stop = false;
  error = Error::None;
  captureDone = writerDone = true;
  peak = highWater = 0;
  clipped = 0;
  if (!resume) {
    currentChunk = displayedChunk = 0;
    // One bounded pool per session, reused across pauses; too large for the task stack.
    blocks = makeUniqueNoThrow<Block[]>(cfg::BUFFER_BLOCKS);
    freeBlocks = xQueueCreate(cfg::BUFFER_BLOCKS, sizeof(uint16_t));
    filledBlocks = xQueueCreate(cfg::BUFFER_BLOCKS, sizeof(uint16_t));
    if (!blocks || !freeBlocks || !filledBlocks) {
      LOG_ERR("MEMO", "Cannot allocate audio pool/queues");
      fail(Error::Setup);
    } else if (!session.begin()) {
      fail(Error::StorageIo);
    } else {
      sessionActive = true;
    }
  }
  if (error == Error::None) {
    xQueueReset(freeBlocks);
    xQueueReset(filledBlocks);
    for (uint16_t i = 0; i < cfg::BUFFER_BLOCKS; ++i) xQueueSend(freeBlocks, &i, 0);
    LOG_INF("MEMO", "pool=%u bytes external=%d internal_free=%u psram_free=%u",
            unsigned(sizeof(Block) * cfg::BUFFER_BLOCKS), esp_ptr_external_ram(blocks.get()), ESP.getFreeHeap(),
            ESP.getFreePsram());
    if (!mic.begin(cfg::SAMPLE_RATE)) {
      fail(Error::Setup);
    } else {
      resumeGapMs = resume ? (esp_timer_get_time() - stoppedAt) / 1000 : 0;
      captureDone = writerDone = false;
      // Writer may block on SD; capture has higher priority and blocks only on audio/free buffers.
      if (xTaskCreate([](void* p) { static_cast<VoiceMemoActivity*>(p)->write(); }, "MemoWrite", 4096, this, 1,
                      &writerTask) != pdPASS) {
        writerDone = true;
        captureDone = true;
        fail(Error::Setup);
      } else if (xTaskCreate([](void* p) { static_cast<VoiceMemoActivity*>(p)->capture(); }, "MemoCapture", 4096, this,
                             2, &captureTask) != pdPASS) {
        fail(Error::Setup);
        captureDone = true;
      }
    }
  }
  if (error != Error::None) {
    LOG_ERR("MEMO", "Cannot start recording");
    finishWorkers();
  } else {
    status = Status::Recording;
  }
  requestUpdate();
}

void VoiceMemoActivity::stopRecording(bool pause) {
  if (captureTask || writerTask) {
    if (pause && status != Status::Recording) return;
    pauseRequested = pause;
    stop = true;
    HalMicrophone::stopIndicator();
    status = Status::Saving;
  } else if (!pause && sessionActive) {
    RenderLock lock;
    pauseRequested = false;
    finishWorkers();
  }
  requestUpdate();
}

void VoiceMemoActivity::capture() {
  uint32_t lastData = millis();
  while (!stop.load()) {
    uint16_t index;
    if (xQueueReceive(freeBlocks, &index, pdMS_TO_TICKS(20)) != pdPASS) {
      if (!stop.load()) {
        highWater = cfg::BUFFER_BLOCKS;
        fail(Error::BufferFull);
      }
      break;
    }
    auto& block = blocks[index];
    const int received = mic.read(block.samples, cfg::BLOCK_SAMPLES, cfg::READ_TIMEOUT_MS);
    if (received < 0 || mic.overflowed() || millis() - lastData > cfg::NO_AUDIO_TIMEOUT_MS) {
      xQueueSend(freeBlocks, &index, 0);
      fail(Error::Capture);
      break;
    }
    if (!received) {
      xQueueSend(freeBlocks, &index, 0);
      continue;
    }
    lastData = millis();
    block.count = received;
    for (int i = 0; i < received; ++i) {
      block.samples[i] = voiceMemoApplyGain(block.samples[i]);
      const int32_t value = block.samples[i];
      peak = std::max(peak, uint32_t(value < 0 ? -value : value));
      if (value == -32768 || value == 32767) ++clipped;
    }
    if (xQueueSend(filledBlocks, &index, 0) != pdPASS) {
      xQueueSend(freeBlocks, &index, 0);
      fail(Error::BufferFull);
      break;
    }
    highWater = std::max(highWater, uint32_t(cfg::BUFFER_BLOCKS - uxQueueMessagesWaiting(freeBlocks)));
  }
  if (mic.overflowed()) fail(Error::Capture);
  mic.stopCapture();
  stoppedAt = esp_timer_get_time();
  // The consumer checks completion before checking queue emptiness.
  captureDone.store(true);
  vTaskSuspend(nullptr);
}

void VoiceMemoActivity::write() {
  uint64_t maxBatchUs = 0;
  uint32_t histogram[5]{};  // <1ms, <10ms, <100ms, <1s, >=1s; includes rollover work.
  bool writable = session.event(resuming ? "resume" : "recording", resumeGapMs);
  if (!writable) fail(Error::StorageIo);
  for (;;) {
    uint16_t index;
    if (xQueueReceive(filledBlocks, &index, pdMS_TO_TICKS(20)) == pdPASS) {
      if (writable) {
        const auto started = esp_timer_get_time();
        const auto previousChunk = session.chunk();
        writable = session.append(blocks[index].samples, blocks[index].count);
        if (!writable) fail(Error::StorageIo);
        const uint64_t elapsed = esp_timer_get_time() - started;
        maxBatchUs = std::max(maxBatchUs, elapsed);
        ++histogram[elapsed < 1000 ? 0 : elapsed < 10000 ? 1 : elapsed < 100000 ? 2 : elapsed < 1000000 ? 3 : 4];
        currentChunk = session.chunk();
        if (previousChunk != session.chunk())
          LOG_INF("MEMO", "chunk=%lu rollover_batch_us=%llu queued=%u", static_cast<unsigned long>(session.chunk()),
                  static_cast<unsigned long long>(elapsed), unsigned(uxQueueMessagesWaiting(filledBlocks)));
      }
      xQueueSend(freeBlocks, &index, 0);
    }
    if (captureDone.load() && uxQueueMessagesWaiting(filledBlocks) == 0) break;
  }
  const auto started = esp_timer_get_time();
  if (!session.finishChunk(error == Error::None)) fail(Error::StorageIo);
  LOG_INF("MEMO", "max_batch_us=%llu finalize_us=%llu latency_bins=%lu/%lu/%lu/%lu/%lu writer_stack=%u",
          static_cast<unsigned long long>(maxBatchUs), static_cast<unsigned long long>(esp_timer_get_time() - started),
          static_cast<unsigned long>(histogram[0]), static_cast<unsigned long>(histogram[1]),
          static_cast<unsigned long>(histogram[2]), static_cast<unsigned long>(histogram[3]),
          static_cast<unsigned long>(histogram[4]), uxTaskGetStackHighWaterMark(nullptr));
  writerDone.store(true);
  vTaskSuspend(nullptr);
}

void VoiceMemoActivity::finishWorkers() {
  stop = true;
  while (!captureDone.load() || !writerDone.load()) vTaskDelay(pdMS_TO_TICKS(10));
  if (captureTask) {
    LOG_INF("MEMO", "capture_stack=%u peak=%lu clipped=%llu buffer_high=%lu/%u",
            uxTaskGetStackHighWaterMark(captureTask), static_cast<unsigned long>(peak),
            static_cast<unsigned long long>(clipped), static_cast<unsigned long>(highWater), cfg::BUFFER_BLOCKS);
    vTaskDelete(captureTask);
    captureTask = nullptr;
  }
  if (writerTask) {
    vTaskDelete(writerTask);
    writerTask = nullptr;
  }
  if (!mic.end()) fail(Error::Setup);
  if (sessionActive) {
    const char* event = error != Error::None ? "incomplete" : pauseRequested ? "pause" : "stop";
    if (!session.event(event)) fail(Error::StorageIo);
  }
  if (error != Error::None) {
    LOG_ERR("MEMO", "Incomplete session, error=%u (1=setup 2=capture 3=buffer 4=storage)", unsigned(error.load()));
    status = Status::Failed;
  } else {
    status = pauseRequested ? Status::Paused : Status::Saved;
  }
  if (status != Status::Paused) {
    sessionActive = false;
    releaseBuffers();
  }
}

void VoiceMemoActivity::onExit() {
  pauseRequested = false;
  if (sessionActive || captureTask || writerTask) finishWorkers();
  releaseBuffers();
  if (!mic.end()) LOG_ERR("MEMO", "Mic rail still needs power-off");
  Activity::onExit();
}

void VoiceMemoActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    pauseRequested = false;
    stop = true;
    finish();
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.routed && app.invalidated()) requestUpdate();
  if (!touch && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (captureTask || writerTask)
      stopRecording();
    else
      startRecording(status == Status::Paused);
  }
  if ((captureTask || writerTask) && captureDone.load() && writerDone.load()) {
    RenderLock lock;
    finishWorkers();
    requestUpdate();
  } else if (captureTask || writerTask) {
    const auto chunk = currentChunk.load();
    const bool saving = stop.load() && status != Status::Saving;
    if (saving) status = Status::Saving;
    if (saving || chunk != displayedChunk) {
      displayedChunk = chunk;
      requestUpdate();
    }
  }
}

void VoiceMemoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(metrics.contentSidePadding),
      static_cast<int16_t>(metrics.buttonHintsHeight), static_cast<int16_t>(metrics.contentSidePadding)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  const auto style = screen.theme().bodyText;
  const auto current = status.load();
  const char* message = tr(STR_MEMO_READY);
  switch (current) {
    case Status::Starting:
      message = tr(STR_MEMO_STARTING);
      break;
    case Status::Recording:
      message = tr(STR_MEMO_RECORDING);
      break;
    case Status::Saving:
      message = tr(STR_MEMO_SAVING);
      break;
    case Status::Paused:
      message = tr(STR_MEMO_PAUSED);
      break;
    case Status::Saved:
      message = tr(STR_MEMO_SAVED);
      break;
    case Status::Failed:
      message = tr(STR_MEMO_FAILED);
      break;
    case Status::Ready:
      break;
  }
  uiTarget.text(screen.takeTop(screen.theme().rowHeight), message, style);
  char progress[80];
  snprintf(progress, sizeof(progress), tr(STR_MEMO_PROGRESS), static_cast<unsigned long>(currentChunk.load()));
  uiTarget.text(screen.takeTop(screen.theme().rowHeight), progress, style);
  uiTarget.text(screen.takeTop(screen.theme().rowHeight), session.path(), style);
  // Stable hit targets also accept Stop while an older screen is still displayed.
  screen.button(tr(STR_MEMO_RECORD), ACTION_RECORD);
  screen.button(tr(STR_MEMO_PAUSE), ACTION_PAUSE);
  screen.button(tr(STR_RESUME), ACTION_RESUME);
  screen.button(tr(STR_MEMO_STOP), ACTION_STOP);
}

void VoiceMemoActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 tr(STR_VOICE_MEMO));
  renderUi();
  const auto current = status.load();
  const bool busy = current == Status::Starting || current == Status::Recording || current == Status::Saving;
  const auto labels = mappedInput.mapLabels(
      tr(STR_BACK), busy ? tr(STR_MEMO_STOP) : (current == Status::Paused ? tr(STR_RESUME) : tr(STR_MEMO_RECORD)), "",
      "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
#endif
