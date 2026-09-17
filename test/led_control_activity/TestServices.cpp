#include "TestServices.h"

#include <gtest/gtest.h>

#include <cstdarg>
#include <cstdio>

#include "activities/Activity.h"

void test::reset() {
  lockDepth = 0;
  rendering = false;
  supported = beginOk = setOk = endOk = true;
  beginCalls = endCalls = 0;
  colors.clear();
  events.clear();
  errors.clear();
  beforeLock = nullptr;
  activityManager.home = HomeMenuItem::NONE;
}

void test::logError(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  errors.emplace_back(buffer);
}

RenderLock::RenderLock() {
  if (test::beforeLock) {
    auto hook = std::move(test::beforeLock);
    test::beforeLock = nullptr;
    hook();
  }
  EXPECT_EQ(test::lockDepth, 0) << "Recursive RenderLock acquisition";
  ++test::lockDepth;
  isLocked = true;
}
RenderLock::RenderLock(Activity&) : RenderLock() {}
RenderLock::~RenderLock() { unlock(); }
void RenderLock::unlock() {
  if (isLocked) --test::lockDepth;
  isLocked = false;
}
bool RenderLock::peek() { return test::lockDepth != 0; }

bool HalLed::supported() { return test::supported; }
bool HalLed::begin() {
  EXPECT_EQ(test::lockDepth, 1);
  EXPECT_FALSE(test::rendering);
  ++test::beginCalls;
  test::events.push_back("begin");
  return test::beginOk;
}
bool HalLed::setColor(Color color) {
  EXPECT_EQ(test::lockDepth, 1);
  EXPECT_FALSE(test::rendering);
  test::colors.push_back(color);
  return test::setOk;
}
bool HalLed::end() {
  EXPECT_EQ(test::lockDepth, 1);
  EXPECT_FALSE(test::rendering);
  ++test::endCalls;
  test::events.push_back("end");
  return test::endOk;
}
