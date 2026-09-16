#pragma once
#include <BoardConfig.h>
#if FREEINK_DEVICE_PAPERMONO
#include <Microphone.h>

// Power transitions run on the input task with RenderLock held, so board I2C owners cannot overlap.
class HalMicrophone {
 public:
  bool begin();
  int read(int16_t* samples, size_t count) { return mic.read(samples, count, 100); }
  bool overflowed() const { return mic.overflowed(); }
  void stopCapture() { mic.end(); }
  bool end();

 private:
  Microphone mic;
  bool powered = false;
};
#endif
