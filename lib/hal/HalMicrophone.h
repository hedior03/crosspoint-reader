#pragma once
#include <BoardConfig.h>
#if FREEINK_DEVICE_PAPERMONO
#include <Microphone.h>

// Power transitions run on the input task with RenderLock held, so board I2C owners cannot overlap.
class HalMicrophone {
 public:
  bool begin(uint32_t sampleRate);
  int read(int16_t* samples, size_t count, uint32_t timeoutMs) { return mic.read(samples, count, timeoutMs); }
  bool overflowed() const { return mic.overflowed(); }
  void stopCapture() {
    stopIndicator();
    mic.end();
  }
  // Main-loop polling also covers temporary overlays. updateIndicator requires RenderLock.
  static bool indicatorNeedsUpdate();
  static void updateIndicator();
  static void stopIndicator();
  bool end();

 private:
  Microphone mic;
  bool powered = false;
};
#endif
