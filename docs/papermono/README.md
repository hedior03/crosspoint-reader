# PaperMono Development Fork

## Repository And Scope

Private repository: https://github.com/hedior03/crosspoint-reader

Public upstream: https://github.com/crosspoint-reader/crosspoint-reader

This is a private derivative with upstream Git history, not a GitHub fork of
the public repository. `origin` is private; `upstream` tracks the original.
Only the upstream `develop` branch history was seeded, without pushing tags or
creating releases. Actions are disabled until a CI task is explicitly approved.
Initial setup used `docs/papermono-setup`. Current implementation work is on
`feature/led-control`; setup notes and the LED feature remain local, uncommitted.

The parent directory is a non-Git workspace. `../multiboot/` is a separate
programming/history project, not a firmware dependency or part of this repo.
The separately requested native preview lives in `../simulator/`, with its own
upstream checkouts and compatibility fixes. It is not a dependency of this
firmware checkout, and was not changed to implement the LED app.

Future scope is a PaperMono reader with compiled-in activities: a basic
calculator, an opt-in BLE HID mouse utility with configurable interval, short
voice notes on SD, and a redesigned home screen. No plugin installer, dynamic
loader, generic app framework, or background-service platform is planned.
Upstream `SCOPE.md` excludes interactive apps; this derivative intentionally
differs while preserving upstream reading features and engineering constraints.

## Source Baseline

| Component | Pinned revision |
| --- | --- |
| Reader `develop` at initial clone | `b5fb406f505144c6c4b5703664889455ae6a7538` |
| Existing FreeInk SDK submodule | `7f6bd0f47a766eea18206dd19f723f3707b6c9d3` |
| SDK's existing Lucide submodule | `c81680e066f45b640743ca78ae36cdedda3f0318` |

No SDK or Simulator fork was created. Keep the gitlink pins; do not use
`git submodule update --remote` to silently advance dependencies. The checked-in
PaperMono environment uses pioarduino platform `55.03.311`, the `dio_opi` Arduino
variant, and the prebuilt TinyUSB graph. Source baseline, platform version,
binary descriptor, and downloaded release version are different identifiers.

The existing native host-test baseline passed before the approved first hardware
app was implemented. [LED Control](led-control.md) now provides green/blue-only
control, leaving red system-managed. Full ESP32/PaperMono compilation has since
passed for development and release profiles; the release app fits the recorded
slot. See [measured build results and the prepared binary](build-baseline.md).
Hardware validation remains pending. The earlier
[baseline plan](../superpowers/plans/2026-09-10-papermono-build-baseline.md) is a
reference, not the current execution record. Calculator, BLE, audio, and a
home-screen redesign remain future work.

## Development Map

| Concern | Start here |
| --- | --- |
| Agent guidance | `AGENTS.md`; `CLAUDE.md` remains its upstream symlink |
| Existing lifecycle and navigation | `docs/activity-manager.md`, `src/activities/Activity.h`, `src/activities/ActivityManager.cpp` |
| UI host and touch conventions | `docs/contributing/touch-and-ui.md`, `src/components/UiAppHost.h` |
| Current home | `src/activities/home/HomeActivity.cpp` |
| Settings and SD persistence | `src/CrossPointSettings.cpp`, `lib/Serialization/PersistableStore.h` |
| Hardware boundary and serialized SD access | `lib/hal/`, especially `HalStorage` / `HalFile` |
| Firmware profile | `platformio.ini:388-406` |
| Build dependencies / existing tests | `.github/workflows/ci.yml:103-181`, `test/CMakeLists.txt` |

The original guide includes older C3-only constraints and obsolete lifecycle
examples. Prefer the actual current implementations and newer activity/UI
guides when they disagree. Use the repository formatting wrapper, never a
separate raw clang-format invocation. Avoid reformatting upstream files.

New activities should use the existing manager/render path and HAL boundary.
Audio/BLE must have explicit resource ownership and teardown; foreground-only
behavior is the initial design. The web server is owned by File Transfer, not
an always-running background service. SDK BLE keyboard-host support is not a
mouse peripheral implementation. SDK PDM capture also needs PaperMono's IO
expander microphone rail handling before a recorder can work.

## Hardware Boundary

The separate multiboot task installed Launcher 2.9.1 and CrossPoint 1.6.0 with
verified flash readback. **The owner reports Launcher works but CrossPoint does
not. The cause remains unresolved.** No working-device claim follows from a
future build, image checksum, or unit test. The last capture showed ROM download
mode and did not diagnose a CrossPoint crash.

The recorded CrossPoint slot is `0x520000` bytes at `0x1A0000`, whereas this
repository's standalone layout provides `0x640000` per app at different offsets.
Default upload targets can replace the multiboot bootloader/table. Keep builds
and hardware flashing separate; never flash merely because an image fits.

No SD backup was taken for the prior installation, with the owner's explicit
acceptance. Do not clear `/.crosspoint/`, format SD, or overwrite device data as
part of source setup. The original full-flash backups remain outside this repo.

## Environments And Distribution

Reader build tooling now uses a dedicated uv `.venv` and project-local
PlatformIO state. Do not reuse the sibling project's Python 3.12 programmer
environment. `build-requirements.lock` and `bin/build-papermono` reproduce the
local setup without installing global ESP-IDF or Arduino tools.

Retain upstream licenses, notices, fonts, and attribution. Reader/SDK are MIT,
but linked dependencies have separate terms, including Arduino-wolfSSL's
GPL/commercial choice and font licenses. Review those before distributing a
binary; do not describe the entire firmware bundle as MIT-only.
