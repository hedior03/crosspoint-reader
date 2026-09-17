#pragma once

#include <GfxRenderer.h>
#include <Logging.h>
#include <MappedInputManager.h>

#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"

struct TestActivityManager {
  HomeMenuItem home = HomeMenuItem::NONE;
  void goHome(HomeMenuItem item = HomeMenuItem::NONE) { home = item; }
  void goToReader(const std::string&) {}
  void goToFileBrowser() {}
  void goToRecentBooks() {}
  void goToSettings() {}
  void goToLedControl() {}
  void goToFileTransfer() {}
  void goToBrowser() {}
};
inline TestActivityManager testActivityManager;
#define activityManager testActivityManager

class Activity {
 protected:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;

 public:
  Activity(const char*, GfxRenderer& renderer, MappedInputManager& input) : renderer(renderer), mappedInput(input) {}
  virtual ~Activity() = default;
  virtual void onEnter() { test::events.push_back("enter"); }
  virtual void onExit() { test::events.push_back("exit"); }
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool preventAutoSleep() { return false; }
  virtual bool isHomeActivity() const { return false; }
  virtual void requestUpdate(bool = false) { test::events.push_back("update"); }
  static void finish() { activityManager.goHome(); }
};
