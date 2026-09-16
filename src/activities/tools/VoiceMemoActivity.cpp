#include "VoiceMemoActivity.h"
#if FREEINK_DEVICE_PAPERMONO
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "VoiceMemoGain.h"
#include "VoiceMemoWav.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

bool VoiceMemoActivity::prepareFile() {
  if (!Storage.ready() || !Storage.ensureDirectoryExists("/recordings")) return false;
  for (unsigned number = 1; number <= 9999; ++number) {
    snprintf(partialPath, sizeof(partialPath), "/recordings/memo-%04u.part", number);
    snprintf(savedPath, sizeof(savedPath), "/recordings/memo-%04u.wav", number);
    if (Storage.exists(partialPath) || Storage.exists(savedPath)) continue;
    file = Storage.open(partialPath, O_RDWR | O_CREAT | O_EXCL);
    if (!file) return false;
    const auto header = voiceMemoWavHeader(0);
    return file.write(header.data(), header.size()) == header.size();
  }
  return false;
}

void VoiceMemoActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock;
  resetUi();
  app.setScreen([](UiScreen& screen, void* user) { static_cast<VoiceMemoActivity*>(user)->buildScreen(screen); }, this);
  app.on(
      ACTION_RECORD,
      [](const fui::ActionEvent&, void* user) { static_cast<VoiceMemoActivity*>(user)->startRecording(); }, this);
  app.on(
      ACTION_STOP, [](const fui::ActionEvent&, void* user) { static_cast<VoiceMemoActivity*>(user)->stopRecording(); },
      this);
  requestUpdate();
}

void VoiceMemoActivity::startRecording() {
  if (task) return;
  RenderLock lock;
  status.store(Status::Starting);
  stop.store(false);
  done.store(false);
  samplesWritten.store(0);
  saved = false;
  partialPath[0] = savedPath[0] = '\0';
  LOG_INF("MEMO", "Starting recording; heap=%u", ESP.getFreeHeap());
  if (!prepareFile()) {
    LOG_ERR("MEMO", "Cannot prepare recording file");
  } else if (mic.begin()) {
    // Activity-owned 2 KB buffer; the worker stack is reserved for control and HAL calls.
    if (xTaskCreate([](void* user) { static_cast<VoiceMemoActivity*>(user)->record(); }, "VoiceMemo", 4096, this, 1,
                    &task) == pdPASS) {
      status = Status::Recording;
    } else {
      task = nullptr;
      LOG_ERR("MEMO", "Cannot allocate recording task");
    }
  }
  if (!task) {
    status.store(Status::Failed);
    mic.end();
    if (file) file.close();
  }
  requestUpdate();
}

void VoiceMemoActivity::stopRecording() {
  if (!task || done.load()) return;
  stop.store(true);
  status.store(Status::Saving);
  requestUpdate();
}

void VoiceMemoActivity::record() {
  constexpr uint32_t TARGET_SAMPLES = MAX_SECONDS * 16000;
  const uint32_t started = millis();
  uint32_t lastData = started;
  uint32_t count = 0;
  uint32_t maxWriteMs = 0;
  int32_t peak = 0;
  int64_t sum = 0;
  uint32_t clipped = 0;
  bool ok = true;
  while (count < TARGET_SAMPLES && !stop.load()) {
    if (millis() - lastData > 2000 || millis() - started > MAX_SECONDS * 1000 + 5000) {
      LOG_ERR("MEMO", "Capture timed out");
      ok = false;
      break;
    }
    const int received = mic.read(samples, std::min(uint32_t{1024}, TARGET_SAMPLES - count));
    if (received < 0 || mic.overflowed()) {
      LOG_ERR("MEMO", "Capture error or DMA overflow");
      ok = false;
      break;
    }
    if (received == 0) continue;
    lastData = millis();
    for (int i = 0; i < received; ++i) {
      samples[i] = voiceMemoApplyGain(samples[i]);
      const int32_t value = samples[i];
      sum += value;
      peak = std::max(peak, value < 0 ? -value : value);
      if (value == -32768 || value == 32767) ++clipped;
    }
    const size_t bytes = static_cast<size_t>(received) * sizeof(int16_t);
    const uint32_t writeStarted = millis();
    if (file.write(reinterpret_cast<const uint8_t*>(samples), bytes) != bytes) {
      LOG_ERR("MEMO", "Short SD write; retaining partial file");
      ok = false;
      break;
    }
    maxWriteMs = std::max(maxWriteMs, uint32_t(millis() - writeStarted));
    count += received;
    samplesWritten.store(count);
  }
  mic.stopCapture();
  status.store(Status::Saving);
  requestUpdate(true);
  ok = ok && !mic.overflowed();
  const auto header = voiceMemoWavHeader(count * sizeof(int16_t));
  const bool headerOk = file.seek(0) && file.write(header.data(), header.size()) == header.size();
  file.flush();
  const bool closed = file.close();
  saved = ok && headerOk && closed && Storage.rename(partialPath, savedPath);
  LOG_INF("MEMO", "samples=%lu duration=%lums peak=%ld dc=%ld clipped=%lu max_write=%lums stack=%u saved=%d",
          static_cast<unsigned long>(count), static_cast<unsigned long>(millis() - started), static_cast<long>(peak),
          static_cast<long>(count ? sum / count : 0), static_cast<unsigned long>(clipped),
          static_cast<unsigned long>(maxWriteMs), uxTaskGetStackHighWaterMark(nullptr), saved);
  if (!saved) LOG_ERR("MEMO", "Incomplete recording retained at %s", partialPath);
  // No activity or file access after publication; the input task owns task deletion.
  done.store(true);
  vTaskSuspend(nullptr);
}

void VoiceMemoActivity::finishWorker() {
  if (!task) return;
  stop.store(true);
  while (!done.load()) vTaskDelay(pdMS_TO_TICKS(10));
  vTaskDelete(task);
  task = nullptr;
  const bool powerOff = mic.end();
  status = saved && powerOff ? Status::Saved : Status::Failed;
  LOG_INF("MEMO", "Released capture; heap=%u power_off=%d", ESP.getFreeHeap(), powerOff);
}

void VoiceMemoActivity::onExit() {
  finishWorker();
  if (!mic.end()) LOG_ERR("MEMO", "Mic rail still needs power-off");
  Activity::onExit();
}

void VoiceMemoActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    stop.store(true);
    finish();
    return;
  }
  const auto touch = routeTouch(mappedInput);
  if (touch.routed && app.invalidated()) requestUpdate();
  if (!touch && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (task)
      stopRecording();
    else
      startRecording();
  }
  if (task && done.load()) {
    RenderLock lock;
    finishWorker();
    requestUpdate();
  } else if (task && millis() - lastPaint >= 1000) {
    lastPaint = millis();
    requestUpdate();
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
  char progress[48];
  const uint32_t seconds = samplesWritten.load() / 16000;
  snprintf(progress, sizeof(progress), tr(STR_MEMO_PROGRESS), static_cast<unsigned long>(seconds / 60),
           static_cast<unsigned long>(seconds % 60));
  uiTarget.text(screen.takeTop(screen.theme().rowHeight), progress, style);
  uiTarget.text(screen.takeTop(screen.theme().rowHeight), current == Status::Saved ? savedPath : partialPath, style);
  // Stable hit targets also accept Stop while an older screen is still displayed.
  screen.button(tr(STR_MEMO_RECORD), ACTION_RECORD);
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
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), busy ? tr(STR_MEMO_STOP) : tr(STR_MEMO_RECORD), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
#endif
