#include "HalMicrophone.h"
#if FREEINK_DEVICE_PAPERMONO
#include <Logging.h>
#include <PaperMonoBoard.h>

bool HalMicrophone::begin() {
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
  if (!mic.begin(16000)) {
    LOG_ERR("MIC", "PDM initialization failed");
    end();
    return false;
  }
  return true;
}

bool HalMicrophone::end() {
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
