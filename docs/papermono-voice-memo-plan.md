# PaperMono voice memo: baby-step plan

Status: ten-second hardware capture confirmed by the owner; sample WAV format and levels checked.
Record/Stop with a five-minute cap is implemented locally; its device checklist remains open.
Follow [Building PaperMono tools](papermono-tools-development.md). Parent: [roadmap](papermono-roadmap.md).

## First useful outcome

Tools → Voice memo → Record → Stop → one playable local WAV file. Copy it using existing File Transfer
and listen on a computer. Start with short recordings; establish reliability before increasing duration.
No upload, transcription, playback on-device, pause, chunking, or recording while reading in this increment.

## Hardware and reuse evidence

The [official PaperMono specification](https://docs.m5stack.com/en/core/PaperMono) identifies ESP32-S3R8,
16 MB flash, 8 MB PSRAM, a PDM microphone, M5PM1 power management, M5IOE1 expander and microSD.
The C3's RAM limits are not the PaperMono's physical specification; still measure free internal RAM, DMA
allocation and PSRAM rather than treating all memory as interchangeable.

| Concern | Existing source and decision |
| --- | --- |
| Build | `platformio.ini:389`: PaperMono S3, PSRAM, native SDMMC and USB MSC. Use this target; add only the needed Microphone dependency to its configuration. |
| Capture | `freeink-sdk/libs/hardware/Microphone/include/Microphone.h:24`: `begin/read/end`, 16-bit mono PCM, default 16 kHz. Prefer this existing ESP-IDF PDM implementation behind a narrow HAL entry, not a new audio framework. It is not currently listed in the root library dependencies. |
| Pins | `freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h:1004`: clock GPIO45, data GPIO46; mic enable is unassigned because it is not an ESP GPIO. |
| Mic power | `freeink-sdk/libs/hardware/BoardConfig/include/M5Ioe1.h:53`: IO12 controls the microphone rail. `PaperMonoBoard.h:35` owns boot sequencing. Extend that existing ownership with mic power control; never repeat expander-wide initialization. |
| M5PM1 | `freeink-sdk/libs/hardware/BoardConfig/include/M5Pm1.h:86` and `PaperMonoBoard.h:83`: existing PMIC/bus and power-button ownership. M5PM1 is not the audio capture engine; preserve its existing power policy. |
| Storage | `lib/hal/HalStorage.h:63` and `:95`: open, write, seek, flush through `HalFile`; `Storage.rename` publishes the completed file. No raw SdFat or second SD mount. Check short writes and seek/rename errors; flush currently has no success return, so do not claim stronger durability than the API supplies. |
| Sleep | `src/activities/Activity.h:45` and `src/main.cpp:640`: `preventAutoSleep()` keeps an active recording from timing out. `HalPowerManager::Lock` is a CPU power-saving mechanism, not a substitute for the sleep lifecycle. Explicit power-off/exit must also stop capture and finish the file. |
| UI | `src/activities/tools/ToolsActivity.cpp`, `CalculatorActivity.cpp`, `src/components/UiAppHost.h`: existing Tools list, themed text/buttons and rendering/input separation. No new artwork or waveform display. |

### Known gaps to resolve in the capture experiment

`Microphone.cpp:16` only toggles an ordinary GPIO and does nothing for the PaperMono's unassigned enable.
Add the narrow board-specific power path, checking rail failures and cleanup. Audit shared I2C transactions
with touch/PMIC polling; do not assume individual Wire calls make a multi-call transaction atomic.

The [official microphone example](https://docs.m5stack.com/en/arduino/papermono/mic) uses the same clock/data
pins, IO12 power, I2S0 and right-channel input. The pinned Microphone implementation uses automatic port
selection and default mono slot configuration (`Microphone.cpp:37–48`). The installed S3 `driver/i2s_pdm.h:36`
defaults mono to left. Verify the port/channel explicitly before judging silence or noise as broken hardware.

`Microphone.cpp:70–77` converts timeout milliseconds to ticks before passing them to `i2s_channel_read`,
whose installed `driver/i2s_common.h:223` accepts milliseconds. It also discards `bytesRead` on timeout.
Correct these semantics if confirmed by the capture experiment/API contract; preserve partial samples and
instrument read failures/overflow. A successful `begin()` alone is not evidence of useful audio.

### Library choice

Start with FreeInk Microphone + the already-installed ESP-IDF driver. Fix only the demonstrated integration
gaps through reproducible SDK changes, and add a thin microphone HAL so activities do not access SDK hardware
directly. Do not clone the entire driver into the activity.

M5Unified's `Mic_Class` is the official fallback/reference, not an extra simultaneous capture owner. If the
FreeInk path proves unsuitable, compare a narrowly configured Mic_Class instance, pin its dependency version,
and measure its RAM/flash cost and task lifecycle. Do not call a second `M5.begin()` or take over display,
SD, I2C, PMIC or expander initialization. No additional M5PM1/M5IOE1 library is presently justified.
The official example samples periodically for an amplitude display; it is not a continuous recorder to copy.
No reusable voice-recorder implementation was identified in the inspected CrossPlay source; keep its upload
patterns for the later server milestone.

## Small increments

### 0. Microphone bring-up and release — recommended starting point

**Does:** developer-only, bounded capture into a reusable small PCM buffer, reporting sample counts, peak/DC
levels, errors and timing. Exercise the existing board owner, the chosen PDM channel, and begin/end.

**Out of scope:** permanent Tools entry, SD recording, level-meter UI, networking.

**PaperMono test:** silence, normal speech and a clap; compare left/right only if needed. Repeat start/stop ten
times, check the rail turns off and internal heap/stack recover, then test touch, frontlight and reader return.
Exercise capture during a display refresh. Record actual settings and observations; do not retain a diagnostic
screen or parallel driver after it has served its purpose.

### 1. Save a fixed-duration test WAV

**Does:** capture approximately ten seconds into one file, stop automatically, finalize it, and expose its path
through existing text/log output. Proposed format: 16 kHz mono signed 16-bit PCM WAV, little-endian.
That is 32,000 bytes/sec, 320 KB for ten seconds, 1.92 MB/minute, excluding the small header.

**Out of scope:** open-ended recording, custom browser, upload, compression, metadata database.

**PaperMono test:** copy the file after capture has stopped, inspect WAV fields/sample count with Python's
standard `wave` module (use uv for any Python environment), and listen. Compare captured duration to a timed
speech/tone source; look for missing audio at refreshes and SD stalls. Count bytes actually written, detect
short writes, and test missing/full SD and a second recording without overwriting the first.

Use a unique temporary filename, reserve a WAV header, stream PCM, then patch lengths, close and rename to
`.wav` only after finalization succeeds. Keep unfinished files recognizable on error. Do not delete a partial
recording or mark it Saved on failure. Unexpected power loss can still lose buffered audio; automatic repair
is deferred, not silently claimed. Bound the experiment's duration and avoid unbounded RAM accumulation.

### 2. Small usable Record/Stop screen

**Does:** add Voice memo to Tools using existing list/navigation and themed buttons/text. Show Ready,
Starting, Recording, Saving, Saved or Error. Use separate Record and Stop actions so rapid repeated taps
cannot toggle into an unintended new session. An accepted stop request cannot be lost to UI refresh.
Show elapsed time from captured samples at a modest refresh cadence (initially no faster than once/second).
No animation replay, live waveform, or new pixel design. Stop feedback changes state promptly even if the
panel takes longer to show it. Start with an explicit short cap (proposed five minutes) until soak testing.

**Out of scope:** pause/resume, background recording outside this screen, transcription and uploads.

**PaperMono test:** tap Record repeatedly, then Stop during refresh; verify one file and correct transitions.
Back while active means stop-and-save before leaving. Verify auto-sleep is inhibited during capture/finalize,
explicit power-button sleep performs orderly cleanup, errors preserve partial files, and idle sleep works
again afterward. Do not enable USB Drive while a recorder owns an SD handle. Record several minutes on
battery and USB, repeat sessions, and return to a book without changing its reading position.

## Minimum internal structure for those increments

- **Activity:** owns the recorder lifetime, handles commands, renders a small latest-status snapshot. It never
  captures or writes audio under the rendering lock. No generic service registry or event bus.
- **Capture/file worker:** one owner starts/stops the microphone and writes/finalizes the file. The existing
  I2S driver provides DMA buffering. Try a single read/write worker first; keep read timeouts bounded so Stop
  is noticed. Activity teardown requests stop and waits for acknowledged cleanup before freeing its memory;
  never kill a worker while it owns an SD lock. Account for exit being called with the render lock held.
- **Buffers:** choose a fixed block after measuring. For scale, a 2 KB PCM block represents 64 ms of audio;
  DMA reserve must cover measured SD write stalls. Account separately for DMA, block buffer and task stack,
  allocate once with checked failure handling, and report high-water marks. Add a bounded capture/writer
  ring only if measurements require it. Overflow must stop/report an incomplete recording, not silently
  discard samples. Unlike calculator feedback, audio cannot be coalesced.
- **Power/control:** keep mic rail ownership centralized and start/end sequencing coordinated with existing
  I2C clients. Inspect the CPU power lock's behavior with rendering before holding it across a session.
  No new board-wide initialization or assumption that preventAutoSleep blocks forced physical power loss.

Host checks should cover WAV lengths, write/finalization failures, repeated start/stop and partial-file policy.
Build PaperMono after implementation; then use actual capture/listening/stress results to set duration and
buffer bounds. Source review and a compile are not a continuity test.

## Later, kept broad

First increase single-file reliability/duration and add browsing/recovery if needed. Then chunk recordings
without missing samples; establish overlap/gap evidence before meeting use. Next introduce a durable upload
queue with server acknowledgment and retry, followed by transcription, optional BLE text delivery, speaker
annotation, AI cleanup, agentic processing and a personal transcription dictionary. Choose server/API,
formats and queue architecture when those increments start. No such framework is needed for local WAVs.

## Next increment: continuous meeting recording

The owner wants capture to continue uninterrupted across file transitions. The current five-minute
Record/Stop recorder is the baseline; continuous chunks are not implemented yet.

Proposed smallest structure: a capture task drains the microphone into a bounded, reusable PCM buffer;
a separate SD writer consumes samples in order and owns chunk headers, writes, close and rename.
Keep the microphone running across boundaries. The writer can finalize one chunk while capture fills
the buffer, then continue into the next numbered chunk in the same session. Split at exact sample counts.
Do not spawn a task per chunk or introduce a general job framework.

WAV header calculation is a few integer operations; SD latency is the uncertainty. Retain locally playable
WAV chunks and the existing `.part` until successfully finalized policy. Deferring headers to the server
would not eliminate write/open stalls and is not presently needed. Previously completed chunks must remain
usable if a later chunk fails. Recovery of the active partial file remains a separate increment.

Size the buffer from measured worst-case write/finalization/open latency with margin, and verify memory
placement and allocation failure handling on PaperMono. At 32,000 bytes/second, 64,000 bytes covers two
seconds; this is a sizing example, not a selected allocation. No finite buffer guarantees lossless capture
through arbitrary SD stalls. Detect exhaustion, preserve stored audio, and report an incomplete session;
never silently drop or overwrite unread samples. Stop must drain accepted buffered samples before finalizing.

Hardware gate: record a timed source for 30–60 minutes, check every boundary for missing/duplicated samples,
exercise display refresh and Stop near rollover, and test full-card/write failures. Log buffer high-water,
SD transition latency, overflow, heap and task stack. Retain stop-on-exit and existing UI components.
Uploads, recording while reading, and a new file browser are out of scope.

After this: upload one closed WAV with server acknowledgment, then add a durable one-at-a-time upload
queue within Tools. Start uploads while recording is idle; concurrent upload/capture needs its own SD/radio
contention test. Endpoint/authentication and retry/duplicate semantics are decisions for that increment.

## Current Record/Stop workflow

Tools → Voice memo opens idle. Record creates a new `/recordings/memo-NNNN.part` and starts capture;
Stop saves a WAV of the captured duration. Repeated Record presses while busy are ignored. The physical
Confirm button starts while idle and stops while busy. Back or power-button sleep stops and finalizes
before leaving. An immediate stop may produce a valid empty WAV. The five-minute cap stops and saves
automatically. Capture/write errors retain a `.part`; Saved requires successful finalization and rename.
Forced physical power loss can still leave incomplete data.

The existing themed screen supplies separate Record/Stop buttons, progress, status and file path. Stop
sets an atomic request without acquiring the render lock; capture polls it between bounded reads/writes.
The task closes its file and stops capture before acknowledging cleanup. The input task then powers off
the mic and releases the worker. Startup resets session state so a new recording cannot inherit a stop/error.
One 4 KB worker stack, an activity-owned 2 KB sample buffer and SDK I2S DMA buffers remain; no upload,
playback, filtering, second capture task or new board initialization has been added.

### Verification

- Host: WAV headers cover empty, short, arbitrary and five-minute lengths; Python checks actual PCM size.
- Device: Record, speak, Stop after a few seconds; copy the WAV with File Transfer and run
  `.venv/bin/python test/voice_memo/check_wav.py /path/to/memo-NNNN.wav`, then listen.
- Rapid Record/Stop taps must create only one session until Saving finishes. Repeat sessions without leaving.
- Stop during a display refresh, Back during capture, and explicit sleep must save and release the mic.
  Idle auto-sleep must work again afterward. Confirm reader/touch/frontlight operation.
- Record to the five-minute cap, check duration/continuity, and test a full/missing card. Keep `.part` and
  `MIC`/`MEMO` serial messages if a run fails. These new hardware checks have not been completed.

The reproducible `patch_freeink_microphone.py` SDK patch selects PaperMono's I2S0/right slot, preserves partial
reads on timeout using millisecond units, and exposes RX queue overflow through an ISR-safe internal-RAM flag.
Overflow marks the recording incomplete. `HalMicrophone` owns the existing expander mic rail and checks power
transitions. Keep the captured evidence before changing buffer sizes or adding a second worker.

The first ten-second test worked on device. Its WAV had the expected 160,000 mono samples at 16 kHz,
no clipped samples and a -6 dBFS peak. Default recording gain is now 2x (about +6 dB), applied in place with signed 16-bit saturation.
Peak/DC/clipping diagnostics describe the amplified samples. Gain also increases air noise; filtering
and automatic gain control remain deferred. Verify speech and loud/clap samples before increasing gain further;
these observations do not establish long-duration or failure-path reliability.
