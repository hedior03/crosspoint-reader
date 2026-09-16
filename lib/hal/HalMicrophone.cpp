#include "HalMicrophone.h"
#if FREEINK_DEVICE_PAPERMONO
#include <Logging.h>
#include <PaperMonoBoard.h>

#include <atomic>

namespace {
std::atomic<bool> indicatorActive{false};
bool indicatorOn = false;  // Main-task-only hardware state.
uint32_t indicatorStarted = 0;
uint32_t lastIndicatorAttempt = 0;
constexpr uint32_t INDICATOR_PERIOD_MS = 2000;
constexpr uint32_t INDICATOR_ON_MS = 150;
bool indicatorDesired() {
  return indicatorActive.load() && uint32_t(millis() - indicatorStarted) % INDICATOR_PERIOD_MS < INDICATOR_ON_MS;
}
}  // namespace

bool HalMicrophone::indicatorNeedsUpdate() {
  return indicatorDesired() != indicatorOn && uint32_t(millis() - lastIndicatorAttempt) >= 100;
}

void HalMicrophone::updateIndicator() {
  const bool on = indicatorDesired();
  if (on == indicatorOn) return;
  lastIndicatorAttempt = millis();
  if (freeink::m5pm1::updateReg(freeink::m5pm1::REG_PWR_CFG, freeink::m5pm1::LED_R_EN,
                                on ? freeink::m5pm1::LED_R_EN : 0)) {
    indicatorOn = on;
  } else {
    LOG_ERR("MIC", "Recording LED update failed");
  }
}

void HalMicrophone::stopIndicator() { indicatorActive.store(false); }

bool HalMicrophone::begin(uint32_t sampleRate) {
  if (!freeink::papermono::ensureBooted()) {
    LOG_ERR("MIC", "Board power unavailable");
    return false;
  }
  powered = true;
  if (!freeink::m5ioe1::write(freeink::m5ioe1::PIN_MIC_POWER, true)) {
    LOG_ERR("MIC", "Power enable failed");
    end();
    return false;
  }
  delay(20);
  if (!mic.begin(sampleRate)) {
    LOG_ERR("MIC", "PDM initialization failed");
    end();
    return false;
  }
  indicatorStarted = millis();
  // Same red-output drive mode as the SDK LED manager; leave green/blue untouched.
  if (freeink::m5pm1::updateReg(freeink::m5pm1::REG_GPIO_DRV, 1u << 5, 0)) {
    indicatorActive.store(true);
  } else {
    LOG_ERR("MIC", "Recording LED unavailable");
  }
  return true;
}

bool HalMicrophone::end() {
  stopIndicator();
  updateIndicator();
  mic.end();
  if (!powered) return true;
  if (!freeink::m5ioe1::write(freeink::m5ioe1::PIN_MIC_POWER, false)) {
    LOG_ERR("MIC", "Power disable failed");
    return false;
  }
  powered = false;
  return true;
}
#endif
