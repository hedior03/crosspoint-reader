# PaperMono Firmware Build Results

## Prepared Image

Both PaperMono target builds completed successfully on 2026-09-10. The smaller
release-profile application is the hand-off candidate:

```text
build/artifacts/papermono-led-control-20260910-211130/crosspoint-papermono-led-control.bin
```

SHA-256:

```text
d835b6b865caeefefc235b806c2648bc6335d8054198fab5e797052a3585995d
```

This is a **local app-only build, not an official release or merged image**.
It has not been flashed or hardware-tested. The previous CrossPoint startup
failure remains unresolved; successful compilation does not diagnose it.

| Profile | Application bytes | Fits recorded 0x520000 slot? | Headroom |
| --- | ---: | --- | ---: |
| `papermono` (debug logging) | 5,404,704 | No | -30,752 bytes |
| `papermono-gh_release` (info/error logging) | 5,366,352 | Yes | 7,600 bytes |

Both fit the source tree's standalone `0x640000` app partitions. The recorded
device slot is smaller and was last located at `0x1A0000`. The release profile
was selected to fit it without changing app code or the partition table. That
7,600-byte margin is very small; later features must be size-checked again.
The live device table has not been re-read in this build session.

## Validation

- ESP32-S3 image, DIO, 80 MHz, 16 MB flash header, 64 KiB MMU pages.
- Seven executable segments; checksum and appended validation SHA-256 valid.
- The release ELF is Xtensa and contains real `HalLed` and `LedControlActivity`
  symbols. The Simulator backend was not linked.
- Matching release ELF SHA-256:
  `b0b781a753527fe895909dd07cba6099e25dc31a04ee3f91103a3a299b2befb8`.
  This equals the ELF digest embedded in the app image.
- Fresh native host test run: **233/233 passed**.
- No application source, SDK pin, partition table, or `platformio.ini` change
  was needed for compilation or size reduction. Git index remains unstaged.

The ESP app descriptor reports `arduino-lib-builder`, version `ee57070`, and
a July 20 compilation date inherited from the prebuilt core. Those fields do
not identify this derivative. Use the image hash, matching ELF, source patch,
and source revisions recorded below.

Release static RAM summary: 116,416 / 327,680 bytes (35.5%). Flash sections:
5,361,338 / 6,553,600 bytes (81.8%). The actual `.bin` is larger because of image
overhead/padding. Static RAM is not runtime free heap; the simulator's fixed
1,048,576-byte reports do not measure device memory usage or leaks.

## Rebuild

Reader's `.venv` now contains firmware and host-test tools. Initial setup:

```sh
uv venv --managed-python --python 3.13 .venv
uv pip sync --python .venv/bin/python --require-hashes build-requirements.lock
```

To build the release profile from the Reader root:

```sh
./bin/build-papermono
```

The helper fixes `papermono-gh_release` and explicitly requests only
`buildprog` / `checkprogsize`; it does not forward arbitrary targets. Despite the
profile name, this creates no GitHub release. Review any future
`platformio.local.ini` overrides before treating a build as a deployment
candidate; there were no local overrides for this build.

`build-requirements.in` includes existing host-test requirements and constrains
them to `test/requirements.txt`. The generated `build-requirements.lock` freezes
the outer Python package set. Do not sync only the smaller host-test lock into
this environment, which would remove firmware tools.

Platforms, packages and tools are isolated under `.cache/platformio/`, build
cache under `.cache/build/`, and outputs under `.pio/`. The platform additionally
creates a uv-managed `.cache/platformio/penv`; its resolved packages are recorded
but not controlled by the outer lock. No global pip or PlatformIO installation
was changed. Initial downloads/compilation exceeded command time limits; cached
builds resumed after checking no old compiler process remained. No source
workaround was needed. Simulator builds were not run concurrently.

The final dependency audit found Click 8.5.0 in the platform-managed environment,
outside its Core's `<8.4` requirement. It was corrected there to 8.3.3, matching
the outer environment, and both dependency checks pass. The artifact preserves
both build-time and post-check managed inventories; this Python-only adjustment
did not alter the binary. Recheck managed dependencies after platform bootstrap
updates rather than assuming the outer lock covers them.

## Source And Toolchain

| Component | Revision/version |
| --- | --- |
| Reader base HEAD | `b5fb406f505144c6c4b5703664889455ae6a7538` |
| Local source | Uncommitted LED feature on `feature/led-control` |
| FreeInk SDK | `7f6bd0f47a766eea18206dd19f723f3707b6c9d3` |
| SDK Lucide | `c81680e066f45b640743ca78ae36cdedda3f0318` |
| Host | macOS ARM64, uv Python 3.13.2 |
| Outer pioarduino Core | 6.1.19 |
| Platform archive | 55.03.311 |
| Arduino-ESP32 | 3.3.11 |
| ESP-IDF prebuilt libraries | 5.5.5, `b774170ff46` |
| Xtensa GCC | 14.2.0+20260121 |
| esptool image producer | 5.3.0 |
| esptool offline verifier | 5.4.0 |

The artifact directory contains the matching ELF, checksum manifest, source
patch including untracked LED files, requirements, build helper, and resolved
package inventories. The source patch was reverse-check-validated against the
current tree without changing it. This is provenance, not a hermetic archive of
every downloaded package. Artifacts are ignored by Git; nothing was committed
or uploaded.

## Installation Is Separate

Only the release **application** is packaged for hand-off. PlatformIO also
generated bootloader/table/factory binaries in its build directory, but those
must not be used over the existing Launcher layout without a separately reviewed
device plan. Never write the app-only image at address zero.

Before any later write, reconfirm device identity, live layout, security state,
backups and the intended payload target. Preserve Launcher, NVS and SD data. Do
not use CrossPoint's own SD/online firmware updater in the shared layout. The
owner will handle boot/power controls while being guided.
