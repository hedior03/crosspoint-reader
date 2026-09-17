# PaperMono Build Baseline Implementation Plan

> **For agentic workers:** Use executing-plans to implement this plan task by
> task after the user approves execution. Independent read-only review may be
> delegated; no worker may connect to the device. Steps use checkbox syntax.

**Goal:** Establish a measured macOS ARM64 build and host-test baseline for the
unchanged PaperMono Reader source, before adding applications.

**Architecture:** Build the existing `papermono` PlatformIO environment with the
upstream-pinned SDK and platform. Isolate Python and PlatformIO state within the
Reader checkout, run native CMake tests, and record image identity and size.
Simulator and multiboot remain separate, optional projects.

**Tech Stack:** uv-managed Python 3.13, pioarduino Core 6.1.19, platform-espressif32
55.03.311, AppleClang ARM64, CMake/Ninja, GoogleTest 1.17.0.

**Status:** This was the original pre-feature plan. The owner subsequently
approved implementing LED Control before the ESP32 build. Its native test
baseline and feature tests were executed with GCC 13. Subsequent development and
release target builds now pass, and the release app fits the recorded slot. See
the [actual build report](../../papermono/build-baseline.md) and
[LED guide](../../papermono/led-control.md) for current status. Reader `.venv` now
contains both host and firmware tooling. Do not blindly run the old
unchanged-source preflight or replace the combined dependency manifest below.
These commands are a record/reference for the original plan; hardware
installation remains unperformed.

## Global Constraints

- Work from `crosspoint-reader/`, not its non-Git parent or `../multiboot/`.
- No device connection, serial monitor, upload, flashing, erase, repartition,
  SD cache deletion/format, PMIC recovery modification, or eFuse operation.
- Do not create a Simulator checkout, fork, dependency, or build environment.
- Keep `src/`, `lib/`, `test/`, `platformio.ini`, `partitions.csv`, `.gitmodules`,
  and the SDK gitlink/source unchanged for the baseline. Document failures before
  proposing narrow fixes; do not mix feature development into setup.
- Do not upgrade the platform or SDK opportunistically or fetch SDK `main` as
  a replacement for the checked-in gitlink.
- Use uv isolation; no global pip, `uv pip --system`, shared programmer venv,
  global PlatformIO package modifications, or global Git configuration changes.
- Keep generated binaries, logs, tool caches, and private data out of Git.
- GitHub Actions remain disabled. No tags, release, PR, commit, or push without
  separate explicit approval. Existing setup documentation must be preserved.
- Format C/C++ only through `./bin/clang-format-fix -g` if a later approved code
  change requires it. Do not reformat upstream for a documentation/build task.
- Passing host tests or image integrity checks does not establish hardware boot
  compatibility. The current device's CrossPoint startup failure is unresolved.

## Inputs And Deliverables

| Item | Path or revision |
| --- | --- |
| Reader source baseline | `b5fb406f505144c6c4b5703664889455ae6a7538` |
| SDK | `7f6bd0f47a766eea18206dd19f723f3707b6c9d3` |
| SDK Lucide assets | `c81680e066f45b640743ca78ae36cdedda3f0318` |
| Create after approval | `build-requirements.in`, `build-requirements.lock` |
| Create after approval | `docs/papermono/build-baseline.md`, a measured results report |
| Update after approval | `docs/papermono/README.md`, link actual results |
| Generated and ignored | `.venv/`, `.cache/`, `.pio/`, `build/test/` |
| Application output | `.pio/build/papermono/firmware.bin` |

The application image is not the ELF and not a merged bootloader/table package.
The embedded `1.6.0-papermono` version is not a unique source revision, so retain
the Reader SHA and the complete image SHA-256 in the report.

## Task 1: Verify The Checkout And Host

**Files:** Read only: `AGENTS.md`, `docs/papermono/README.md`, `platformio.ini`,
`.github/workflows/ci.yml`, `test/CMakeLists.txt`, `.gitmodules`.

**Consumes:** Private Reader checkout and inherited submodule pins.
**Produces:** Confirmed source/host identity before any tool installation.

- [ ] Run from the Reader repository:

```sh
uname -sm
xcode-select -p
/usr/bin/clang --version
uv --version
git branch --show-current
git remote -v
git status --short
git rev-parse HEAD
git submodule status --recursive
git diff b5fb406f505144c6c4b5703664889455ae6a7538 -- src lib test platformio.ini partitions.csv .gitmodules freeink-sdk
test ! -e platformio.local.ini
```

- [ ] Require Darwin ARM64 for the commands below, the private repository as
  `origin`, public original as `upstream`, and no baseline-source differences.
  Existing `.gitignore`, `AGENTS.md`, and documentation edits are intentional.
  If the source has advanced, stop and rebase this plan on the new evidence;
  never reset user work just to match the recorded SHA.
- [ ] The last command checks the ignored local configuration, which is absent
  in the initial checkout. If it exists, inspect it before proceeding:
  `platformio.ini` loads it automatically, and `git diff` does not cover ignored
  overrides. Review target, platform, build flags, scripts, and dependency/path
  overrides; proceed only if they are baseline-neutral and documented. Do not
  delete, overwrite, or disable a user's local file automatically.
- [ ] Require initialized submodules at the listed pins. If uninitialized,
  `git submodule update --init --recursive` is allowed; never add `--remote`.
- [ ] If Apple Command Line Tools are absent, request installation through
  `xcode-select --install` and wait for completion. Do not substitute an ESP32
  toolchain for the native compiler used by host tests.

## Task 2: Provision An Isolated Build Environment

**Files:** Create `build-requirements.in` and generated `build-requirements.lock`.
**Consumes:** Task 1's checkout/host identity and CI bootstrap dependencies.
**Produces:** Local `.venv` and hashed outer Python dependency lock.

- [ ] Create `build-requirements.in` using this content:

```text
# pioarduino Core source pinned by Reader CI, not stock PyPI PlatformIO.
https://github.com/pioarduino/platformio-core/archive/refs/tags/v6.1.19.zip

# CI's platform bootstrap dependencies.
littlefs-python>=0.16.0
fatfs-ng>=0.1.14
pyyaml>=6.0.2
rich-click>=1.8.6
zopfli>=0.2.2
intelhex>=2.3.0
rich>=14.0.0
urllib3<2
cryptography>=45.0.3
certifi>=2025.8.3
ecdsa>=0.19.1
bitstring>=4.3.1
reedsolo>=1.5.3,<1.8
esp-idf-size>=2.0.0
esp-coredump>=1.14.0

# Native host-test tools; --test-dir requires CMake 3.20 or later.
cmake>=3.20
ninja
```

The source constraints match `.github/workflows/ci.yml:113-122`; resolve exact
versions once and retain the generated lock. Do not install root
`requirements.txt` for this baseline: it includes optional font/image/debug
tooling. No font/icon asset rebuild or external simulator libraries are needed.

- [ ] In a fresh shell with no active ESP-IDF environment, initialize paths from
  the Reader root. Use these same values in every subsequent command session:

```sh
export REPO="$PWD"
export VIRTUAL_ENV="$REPO/.venv"
export PATH="$VIRTUAL_ENV/bin:$PATH"
unset PYTHONHOME PYTHONPATH
export PYTHONNOUSERSITE=1
export UV_CACHE_DIR="$REPO/.cache/uv"
export UV_PYTHON_INSTALL_DIR="$REPO/.cache/uv-python"
export PLATFORMIO_CORE_DIR="$REPO/.cache/platformio"
export PLATFORMIO_PLATFORMS_DIR="$PLATFORMIO_CORE_DIR/platforms"
export PLATFORMIO_PACKAGES_DIR="$PLATFORMIO_CORE_DIR/packages"
export PLATFORMIO_GLOBALLIB_DIR="$PLATFORMIO_CORE_DIR/lib"
export PLATFORMIO_CACHE_DIR="$PLATFORMIO_CORE_DIR/.cache"
export PLATFORMIO_WORKSPACE_DIR="$REPO/.pio"
export PLATFORMIO_BUILD_DIR="$REPO/.pio/build"
export PLATFORMIO_LIBDEPS_DIR="$REPO/.pio/libdeps"
export PLATFORMIO_BUILD_CACHE_DIR="$REPO/.cache"
export PLATFORMIO_DEFAULT_ENVS=papermono
```

`PLATFORMIO_CORE_DIR` matters: selecting a local Python executable alone still
allows global `~/.platformio` state. Retain the normal `.pio/libdeps` location
because the Reader dependency-patching hooks search it explicitly.

- [ ] Verify this Reader `.venv` is not already an unrelated environment. Do not
  clear an existing environment without inspecting it. For first creation:

```sh
uv venv --managed-python --python 3.13 .venv
uv pip compile --python .venv/bin/python --generate-hashes --no-header --output-file build-requirements.lock build-requirements.in
uv pip sync --python .venv/bin/python --require-hashes build-requirements.lock
.venv/bin/python --version
.venv/bin/pio --version
.venv/bin/cmake --version
.venv/bin/ninja --version
uv pip check --python .venv/bin/python
.venv/bin/python -c 'import sys; print(sys.executable); print(sys.prefix); assert sys.prefix != sys.base_prefix'
```

- [ ] Require Python 3.13.x, Core 6.1.19, compatible installed dependencies, and
  the executable prefix inside this checkout. Record actual patch/tool versions.
  Do not regenerate the lock on every later run; sync it with hash verification.

The pioarduino platform can create a managed `penv` under the isolated Core
directory. The outer lock does not freeze that environment, toolchains, or all
transitive PlatformIO libraries. Record their resolved inventories after build;
do not claim a hermetic firmware build based only on the Python lock.

## Task 3: Run Native Host Tests

**Files:** Existing `test/`; generated output in `build/test/` only.
**Consumes:** Task 2's CMake/Ninja and Task 1's AppleClang.
**Produces:** A measured CTest count and pass/failure report.

- [ ] Configure and build the existing tests directly, without PlatformIO:

```sh
.venv/bin/cmake -S test -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_OSX_ARCHITECTURES=arm64
.venv/bin/cmake --build build/test --parallel 4
.venv/bin/ctest --test-dir build/test --show-only
.venv/bin/ctest --test-dir build/test --output-on-failure --parallel 4
```

- [ ] Confirm CTest discovers tests, exits successfully, and reports zero failing
  cases. The source currently defines 19 test executables; record the actual
  discovered case count rather than assuming it is 19.
- [ ] GoogleTest v1.17.0 is fetched at configure time. One parser target links
  native expat; the Apple SDK currently supplies it. Diagnose an actual missing
  header/library before adding a Homebrew dependency.
- [ ] If tests fail, capture the exact command, compiler diagnostics, and test
  names. Investigate environment/source provenance first. Do not edit source,
  disable tests, or weaken assertions just to obtain a green baseline.

## Task 4: Build And Inspect PaperMono Firmware

**Files:** Existing configuration/source; generated output in `.pio/` and `.cache/`.
**Consumes:** The pinned platform/SDK and isolated Core from Task 2.
**Produces:** A PaperMono application image and a separate slot-fit assessment.

- [ ] With Task 2's environment still active, run exactly the development target:

```sh
.venv/bin/pio run -e papermono
```

Do not run bare `pio run`: `default_envs = default` selects the C3 custom-core
build. `papermono` extends `base`, not `firmware_tuned`, for the prebuilt TinyUSB
graph. Do not apply the C3 custom-core CMake workaround to PaperMono preemptively.
The first build downloads sizeable platform/toolchain dependencies; this is not
permission to install or flash a standalone ESP-IDF project.

- [ ] Require a successful compiler/linker exit, nonempty `firmware.bin`, and
  record the RAM/flash summary and warnings. Do not rebuild a passing target
  solely after documentation edits or clean caches by default.
- [ ] Inspect only the local application image and calculate both size budgets:

```sh
.venv/bin/python -m esptool --chip esp32s3 image-info .pio/build/papermono/firmware.bin
shasum -a 256 .pio/build/papermono/firmware.bin
.venv/bin/python -c 'from pathlib import Path; p = Path(".pio/build/papermono/firmware.bin"); n = p.stat().st_size; assert n > 0; print("Application bytes:", n); print("Recorded multiboot slot:", "FITS" if n <= 0x520000 else "DOES NOT FIT", "headroom:", 0x520000 - n); print("Standalone slot:", "FITS" if n <= 0x640000 else "DOES NOT FIT", "headroom:", 0x640000 - n)'
.venv/bin/pio pkg list -e papermono
uv pip freeze --python .venv/bin/python
```

If esptool is only in the platform-managed `penv`, use its confirmed Python
executable under `$PLATFORMIO_CORE_DIR/penv` instead; do not borrow the programmer
environment or install globally. Record that executable and esptool version.
Inspect whether that managed environment exists and record its package inventory
with `uv pip freeze --python "$PLATFORMIO_CORE_DIR/penv/bin/python"` if present.

- [ ] Require the image to identify ESP32-S3 and report valid checksum/digest.
  Keep its source SHA, image SHA, and embedded version distinct.
- [ ] Report **both** limits: recorded multiboot slot `0x520000` = 5,373,952 bytes;
  standalone slot `0x640000` = 6,553,600 bytes. The recorded multiboot address is
  `0x1A0000`; source defaults use different app addresses. A 16 MB board-size
  value is not the application budget.
- [ ] A successful build that exceeds the smaller slot is a valid build baseline
  with a deployment blocker. Record the blocker; do not shrink features or alter
  partitions in this task. Even a fitting binary is not approved for flashing.

## Task 5: Record Results And Handoff

**Files:** Create `docs/papermono/build-baseline.md`; update the fork guide.
**Consumes:** The measured results of Tasks 1-4.
**Produces:** Reproducible build instructions and an honest acceptance report.

- [ ] Optionally run the existing cppcheck baseline if analysis is requested:

```sh
.venv/bin/pio check -e papermono --fail-on-defect low --fail-on-defect medium --fail-on-defect high
```

Keep configured suppressions and source scope. Do not install CI's Linux-only
`libpcre3` package on macOS or remediate unrelated findings in this baseline.

- [ ] Create the results report with actual source/submodule SHAs, host/tool
  versions, reproducible environment exports, lock/install commands, resolved
  package inventories, build exit status/warnings, test counts/failures, image
  descriptor/hash/size, and separate fit results. If a task could not finish,
  state the exact blocker instead of filling in an expected result.
- [ ] Check remaining tracked changes and exclusions:

```sh
git diff --check
git status --short --untracked-files=all
git diff b5fb406f505144c6c4b5703664889455ae6a7538 -- src lib test platformio.ini partitions.csv .gitmodules freeink-sdk
git submodule status --recursive
git check-ignore .venv/bin/python .cache/platformio .pio/build/papermono/firmware.bin build/test
```

- [ ] Require unchanged upstream application source/config/SDK, ignored generated
  outputs, and only intended build-support/documentation changes. Link the result
  from `docs/papermono/README.md`. Do not stage or commit without explicit approval.
- [ ] Hand off separately: local build status; host-test status; slot-fit status;
  hardware status **not tested, prior failure unresolved**. Next feature planning
  can target Calculator, but no feature implementation or device repair is
  implied by completing this baseline.

## Source References And Self-Review

- `platformio.ini:1-12,90-129,381-406`: default target, platform, dependencies,
  hooks, and PaperMono prebuilt-core requirements.
- `.github/workflows/ci.yml:103-122,156-181`: Python/Core/bootstrap and host tests.
- `test/CMakeLists.txt`: C++20, GoogleTest pin, host-test targets.
- `partitions.csv`: standalone app sizes; the separate hardware record supplies
  the smaller recorded installed slot, not a build dependency.
- `scripts/patch_wolfssl.py`, `scripts/patch_jpegdec.py`: default libdeps paths.
- [Pinned platform bootstrap](https://github.com/pioarduino/platform-espressif32/blob/55.03.311/builder/penv_setup.py): managed tool environment.

Plan self-review: source identity, tool isolation, explicit target, host tests,
image validation, both size budgets, result recording, unchanged source, and
no-device/no-simulator gates each have an explicit step above. All paths refer
to this firmware checkout unless explicitly marked as separate history. No
build success or runtime compatibility has been inferred from source inspection.
