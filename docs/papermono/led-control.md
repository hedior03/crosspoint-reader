# LED Control

## Implementation Status

Implemented locally on `feature/led-control`. Both PaperMono target builds now
pass; the release-profile app is packaged and fits the recorded slot with only
7,600 bytes remaining. See [build results and binary location](build-baseline.md).
The later, separately approved standalone installation wrote the merged image
and passed flash verification, replacing Launcher. The owner declined a fresh
backup and will boot/test it manually. Physical behavior is not yet verified.
No device operations occurred during the earlier implementation/build phases.

This is a built-in activity, not a dynamically installed app. Home shows
**LED Control** before Settings on PaperMono builds with LED capability enabled.
Other boards keep their existing Home menu.

The separately requested desktop preview now builds this same app source with
a simulated `HalLed` backend. Run `./simulator/run.sh` from the workspace root.
Its Applied marker and `[SIM LED]` logs represent host state, not I2C readback.
The four physical HAL/activity source files were unchanged by preview integration.
Keyboard/touch selection, Back cleanup, and simulated sleep/stack cleanup were
exercised. Actual ESP32 compilation has since passed; LED hardware tests remain
pending.

## Behavior

- Off (green/blue), Green, Blue, and Both are the four choices.
- Moving the selection does not change the LED. Confirm or a touch release
  applies the choice; a separate **Applied** marker shows successful readback.
- Red remains system-managed. Off only refers to green and blue and does not
  promise the entire indicator is dark.
- Entry starts the controlled channels off. Actual activity exit and sleep
  attempt to clear them. A temporary frontlight/control-center overlay retains
  the steady setting because the RGB activity has not exited.
- No brightness, blinking, background task, persisted preference, or auto-sleep
  override is added.
- On an I2C/write/readback failure, the app removes the Applied marker, disables
  further color commands, and displays an error. The HAL attempts off once; exit
  attempts cleanup again. Failed cleanup is logged, not reported as darkness.

## Hardware Boundary

`lib/hal/HalLed.h/.cpp` use the existing SDK's M5IOE1 helpers. They require the
normal board initialization to have completed, IO8/IO9 to be push-pull outputs,
and their PWM channels to be disabled. Failure does not trigger bus or peripheral
reinitialization.

Writes are limited to the combined green/blue mask `0x0180` in M5IOE1
`REG_GPIO_OUT_L`. Each update preserves current unrelated output bits and reads
back the commanded bits. There are no app-added red, PMIC-policy, mode, drive,
PWM, display/touch/SD power, or download-lock writes.

The activity holds `RenderLock` for initialization and activation, excluding
render-task access during the read-modify-write. `onExit()` uses the lock already
held by ActivityManager. This is a foreground-app contract, not a new general
I2C arbitration service. Future background LED ownership would need a separate
design shared by every relevant peripheral owner.

`LedManager` was deliberately not added as a dependency: its RGB path also writes
red. No SDK modifications or reference-project code copies were required.

## Host Verification

Reader's own uv `.venv` contains host-test/formatting and firmware build tools.
The combined `build-requirements.lock` retains the host versions from
`test/requirements.txt`. Do not sync the smaller host-only manifest over the
combined environment, or reuse the multiboot/simulator Python environments.

Native configuration used the installed Homebrew GCC 13 because the installed
Apple libc++ lacks floating-point `std::from_chars` used by existing Reader code:

```sh
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake -S test -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/opt/homebrew/bin/gcc-13 -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/g++-13
PATH="$PWD/.venv/bin:$PATH" .venv/bin/cmake --build build/test --parallel 4
.venv/bin/ctest --test-dir build/test --output-on-failure --parallel 4
```

The existing baseline passed 191 tests before the feature. The final host run
passed **233/233 tests**, including 42 LED/lifecycle/Home regressions. New
regression tests were observed failing before their implementations. Independent
review found one RoundedRaff clipping issue, which was fixed and re-reviewed.
The repository formatting wrapper and `git diff --check` passed; the real Git
index remains unstaged. Existing upstream compiler warnings are not claimed fixed.
The LED suites exercise:

- The real HAL with a register-model SDK substitute: capability gates, four
  presets, preservation of unrelated bits, configuration checks, read/write
  faults, readback mismatch, bounded cleanup, fault latch and re-entry.
- The real LED activity, list base, navigation helper and FreeInkUI with fake
  hardware/theme services: focus versus activation, touch release, errors,
  late input, applied marker, render-lock ownership, idle rendering and cleanup.
- The real Home mappings/rendering/input call sites with fake theme services:
  support and OPDS combinations, restored selection, and PaperMono geometry.
  RoundedRaff's seven-row menu now uses spare cover-to-menu space to keep its
  last card within the 797-pixel viewable bottom; draw and touch origins agree.

These tests do not execute real I2C transfers, prove optical color, or replace
target compilation and physical testing. The full ActivityManager transition
stack and every theme/orientation combination are not hardware-validated.

## Binary Preparation

The development image is 5,404,704 bytes and exceeds the recorded multiboot slot
by 30,752 bytes. The existing release profile removes debug logging and produces
a 5,366,352-byte app, which fits without changing code or partitions. It retains
info/error logs. Use `./bin/build-papermono` to rebuild this profile; see the
[build report](build-baseline.md) for validation and artifact provenance.

Fitting the slot does not establish runtime compatibility. Resolve or characterize
the existing reader startup failure before calling this a working hardware
baseline. Do not replace the bootloader/table as an unexplained workaround.

## Later Device Check

No device connection is needed for implementation or local compilation. For a
separately approved device session, the owner will operate power/boot controls
while being guided. Reconfirm device identity, recovery access, backups, and the
exact flash layout before selecting any write operation.

After an approved install and successful reader boot, check Home -> LED Control,
all four choices, unchanged red/system indication, display/touch/SD operation,
Back/Home cleanup, frontlight overlay behavior, and sleep/wake. Stop on errors;
do not force retries, clear SD caches, change PMIC settings, or assume a failed
off command succeeded. The earlier multiboot startup failure remains unresolved
by this source implementation.
