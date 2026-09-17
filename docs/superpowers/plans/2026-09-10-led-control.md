# PaperMono LED Control Implementation Plan

**Goal:** Implement the approved green/blue-only LED app, with host verification
now and an explicitly separate PaperMono compilation/device session later.

**Architecture:** A small `HalLed` owns one foreground control session and uses
the pinned SDK's checked M5IOE1 register helpers. `LedControlActivity` extends
`UiListActivity`, reachable from Home. No SDK fork, RGB library, plugin system,
task, timer, persistence, or simulator changes are required.

**Execution status:** Implementation and host verification complete locally.
The original 191 host tests passed before edits; the final suite passed 233/233.
The HAL's initial stub produced 13 expected failures, the initial activity stub
produced 10, and additional input/menu/geometry regressions failed before their
fixes. Independent review and scoped geometry re-review found no remaining
blocking issues. No target compilation, device test, commit, or push occurred.

Subsequent binary-preparation task: both PaperMono target profiles compiled;
the release application was packaged and fits the recorded slot. See
`docs/papermono/build-baseline.md`. No device test, commit, or push followed.

## Constraints And Approved Behavior

- Work in the existing private Reader checkout on `feature/led-control`; retain
  its existing uncommitted setup documentation and ignore rules.
- No commits, pushes, device/serial operations, firmware upload, partition
  changes, SD formatting, or PMIC recovery changes.
- Choices: Off (green/blue), Green, Blue, Both. Red stays system-managed; do not
  instantiate SDK `LedManager`, whose setters write red as well.
- No brightness, blinking, persistence, or background work. Normal auto-sleep
  remains enabled. A temporary frontlight overlay keeps the steady setting;
  actual activity exit or sleep clears the app-controlled channels.
- Only PaperMono with `FREEINK_CAP_LED` is supported. Do not use `hasLeds()`:
  the pinned helper incorrectly requires a data GPIO for the discrete LED.
- Do not call `configureOutputs()`, `beginBus()`, or change mode/drive/PWM/rail
  registers. Require normal board initialization, GPIO output mode, push-pull,
  and disabled green/blue PWM before claiming the session.
- Under `RenderLock`, masked writes change only IO8/IO9 in `GPIO_OUT`. Read back
  those bits. Errors trigger one bounded best-effort off operation and block
  further on commands until a new session; never claim optical verification.
- `onEnter()` and activation acquire `RenderLock`; `onExit()` uses the lock
  already held by ActivityManager. Never acquire it recursively or render while
  performing I2C operations.

## HAL Contract

New `lib/hal/HalLed.h/.cpp` expose:

```cpp
class HalLed {
 public:
  enum class Color : uint8_t { Off, Green, Blue, Both };
  static bool supported();
  bool begin();
  bool setColor(Color color);
  bool end();
};
```

The object is activity-owned, noncopyable, and allocation-free. Internally its
state is Idle, Ready, or Fault. `begin()` validates existing configuration and
sets green/blue off; it must not reinitialize shared hardware. `setColor()`
rejects invalid enums and calls outside Ready. A failed write/readback latches
Fault and attempts off once. `end()` tries off for any claimed session and only
returns to Idle after verification. A never-claimed session causes no writes.
Unsupported boards return false from begin/set and perform no I2C access.

## Task 1: Host Tests And HAL

- [x] Add isolated native-test tools in `test/requirements.in` and a hashed lock;
  provision Reader's own uv Python 3.13 `.venv`, not a sibling environment.
- [x] Establish the existing native test result with the installed GCC 13, which
  supports the float `from_chars` missing from Apple's installed libc++.
- [x] Add `test/led_control/` compiling the real HAL with a register-model SDK
  substitute. Add it to `test/CMakeLists.txt`.
- [x] Observe failures for a stub HAL, then implement the contract. Cover valid
  mappings, unrelated-bit preservation, absent initialization, mode/drive/PWM
  rejection, failed reads/writes/readbacks, fault latch, cleanup/re-entry,
  invalid enums, and compile-time unsupported targets.
- [x] No `platformio.ini`, SDK, or device layout edits are needed for the HAL.

## Task 2: Activity And Navigation

- [x] Add `src/activities/util/LedControlActivity.h/.cpp` using the existing
  list host, fixed four-row storage, translated labels and an applied marker.
- [x] Complete hardware/session initialization before requesting the first
  list render. Only activation applies a color; moving focus does not.
- [x] Surface unavailable/I2C-error state without an applied-color claim. Failed
  sessions must not keep accepting on commands. Back remains usable.
- [x] Perform off cleanup in `onExit()` without reacquiring `RenderLock`; log
  failure without claiming the LED is dark. No hardware writes in rendering.
- [x] Add `HomeMenuItem::LED_CONTROL` and `goToLedControl()` using checked
  allocation. Gate the menu using `HalLed::supported()` without probing I2C.
- [x] Keep Home count, mappings, label/icon order and restored selection in sync,
  with and without OPDS/recent books. Use a known existing icon.
- [x] Add English source strings only; generate i18n through the existing script
  rather than editing generated files. Other languages use upstream fallback.

## Task 3: Verification And Handoff

- [x] Run all host tests; record actual counts/failures. Run the repository
  formatting wrapper and `git diff --check` for code changes.
- [x] Independently review register masks, no-PMIC invariant, render/lifecycle
  synchronization, error handling, Home integration, and tests.
- [x] Record what is implemented versus tested. Full ESP32/PaperMono compilation
  is deferred per the user; it is not implied by host tests.
- [x] Document the next build command `pio run -e papermono` in the separately
  provisioned firmware environment, image/slot checks, and user-operated boot
  acceptance steps. Do not run any upload command in this implementation task.

## Evidence Behind The Design

- `freeink-sdk/.../BoardConfig/include/M5Ioe1.h:34-55,113-149`: output registers,
  masks, checked reads, masked updates, and shared peripheral pins.
- `freeink-sdk/.../LedManager/src/LedManager.cpp:49-60,94-105`: existing RGB path
  writes red and configures hardware, so it is not the green/blue-only API.
- `src/activities/ActivityManager.cpp:169-190,206-211`: enter outside render lock,
  exit under it; pushes preserve the underlying activity.
- `src/activities/UiListActivity.cpp`: existing input/routing/render lifecycle.
- CrossMux's compiled app catalog informed the approach; no reference firmware
  code or dependencies are copied into this app.
