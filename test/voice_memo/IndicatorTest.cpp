#include <HalMicrophone.h>
#include <PaperMonoBoard.h>

#include <cassert>

int main() {
  using namespace freeink::m5pm1;
  HalMicrophone mic;
  assert(mic.begin(16000));
  const uint32_t started = fakeMillis;
  assert(HalMicrophone::indicatorNeedsUpdate());
  HalMicrophone::updateIndicator();
  assert(registers[REG_PWR_CFG] == (4 | LED_R_EN));
  fakeMillis = started + 150;
  assert(HalMicrophone::indicatorNeedsUpdate());
  HalMicrophone::updateIndicator();
  assert(registers[REG_PWR_CFG] == 4);
  fakeMillis = started + 1000;
  assert(!HalMicrophone::indicatorNeedsUpdate());
  fakeMillis = started + 2000;
  assert(HalMicrophone::indicatorNeedsUpdate());
  HalMicrophone::updateIndicator();
  assert(registers[REG_PWR_CFG] & LED_R_EN);
  HalMicrophone::stopIndicator();
  failNextUpdate = true;
  HalMicrophone::updateIndicator();
  assert(registers[REG_PWR_CFG] & LED_R_EN);
  fakeMillis += 100;
  assert(HalMicrophone::indicatorNeedsUpdate());
  HalMicrophone::updateIndicator();
  assert(registers[REG_PWR_CFG] == 4);
  fakeMillis += 2000;
  assert(!HalMicrophone::indicatorNeedsUpdate());
  assert(mic.begin(16000));
  HalMicrophone::updateIndicator();
  assert(registers[REG_PWR_CFG] & LED_R_EN);
  assert(mic.end());
  assert(registers[REG_PWR_CFG] == 4);
}
