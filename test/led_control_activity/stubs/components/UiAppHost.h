#pragma once

#include <FreeInkApp.h>
#include <GfxRenderer.h>
#include <MappedInputManager.h>

// Only device/theme binding is substituted. The SDK screen, list, interaction
// table and callback dispatch are real, including callbacks without RenderLock.
class UiAppHost {
  GfxRenderer& target;
  bool ready = false;

 public:
  using UiApp = freeink::ui::FreeInkApp<24, 6>;
  using UiScreen = UiApp::ScreenType;
  UiApp app;

  explicit UiAppHost(GfxRenderer& renderer) : target(renderer), app(renderer, {}) {}
  void resetUi() { ready = false; }
  void closeRouting() { ready = false; }
  void renderUi() {
    freeink::ui::DeviceContext device;
    device.width = static_cast<int16_t>(target.width);
    device.height = static_cast<int16_t>(target.height);
    device.hasTouch = true;
    app.setDevice(device);
    app.render();
    ready = true;
  }
  struct TouchRoute {
    freeink::ui::ActionEvent event{};
    freeink::ui::InputSnapshot snap{};
    bool routed = false;
    explicit operator bool() const { return static_cast<bool>(event); }
  };
  TouchRoute routeTouch(const MappedInputManager& input, bool = false, bool = false) {
    if (!ready) return {};
    return {app.route(input.touch), input.touch, true};
  }
};
