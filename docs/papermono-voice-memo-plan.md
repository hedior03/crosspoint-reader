# PaperMono voice memo: baby-step plan

Status: ten-second hardware capture confirmed by the owner; sample WAV format and levels checked.
Continuous session recording and pause/resume are implemented locally; their device checklist remains open.
Follow [Building PaperMono tools](papermono-tools-development.md). Parent: [roadmap](papermono-roadmap.md).

Desktop research: [audio experiments](papermono-audio-experiments.md) covers the Jupyter lab, biquad
filtering and codec comparisons. Those experiments do not change the firmware recording format.

## Original single-file milestone

Tools → Voice memo → Record → Stop → one playable local WAV file. Copy it using existing File Transfer
and listen on a computer. Start with short recordings; establish reliability before increasing duration.
That initial milestone excluded upload, transcription, playback, pause, chunking and recording while reading.
The current continuous-recording implementation is described below.

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

## Initial milestones (single-file baseline)

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

## Structure used for the single-file baseline

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

## Continuous meeting recording and pause/resume

Status: implemented locally as one increment at the owner's request. One active session; leaving Voice Memo
stops and saves. Pause/resume is included. Background uploads and reopening old sessions remain deferred.

### Observable scope

- Record creates a unique `/recordings/session-NNNN/` directory; numbered `chunk-NNNN.part` files become
  `.wav` only after successful finalization. Existing flat memo files remain untouched.
- Five minutes becomes a chunk duration, not a session cap. Continue until Stop, exit, or an error/resource
  limit. Never overwrite an existing session/chunk; detect identifier and counter exhaustion explicitly.
- Keep existing themed Record/Stop controls; show recording state, current chunk and session path.
- Stop ends capture, drains accepted buffered PCM in order, finalizes the last nonempty chunk, and releases
  tasks/buffers/mic power. ADPCM retains complete codec blocks and journals its incomplete tail as described
  below; PCM retains every sample. Avoid an extra empty chunk when Stop lands exactly at a boundary.
- On overflow or storage failure, preserve completed chunks and the active partial file, report incomplete,
  and release resources safely. Drain buffered audio only while storage remains writable. Forced power loss
  can lose RAM/buffered writes; automatic partial recovery is deferred.

### Implementation order and source evidence

1. Centralize sample rate, digital gain, chunk seconds, block samples and buffer blocks in one small recorder
   config header. Keep mono signed 16-bit PCM fixed. `VoiceMemoWav.h:22–25` and `HalMicrophone.cpp:18` currently
   duplicate format values; pass the selected rate into the HAL and derive WAV rates/lengths from it. Validate
   bounds at compile time and driver startup. Use wide session counters and bounded per-chunk byte counts.
2. Replace flat naming (`VoiceMemoActivity.cpp:14–25`) with unique session directories and numbered chunks.
   Keep filename generation bounded and all file access through HalStorage.
3. Split the existing read/write loop (`VoiceMemoActivity.cpp:80–129`) into capture and writer tasks owned by
   the activity. Use a fixed pool of PCM blocks and FreeRTOS queues of block indices: free blocks to capture,
   filled blocks to writer, recycled only after writing. Allocate once per session with checked failure cleanup.
   Capture never waits on an SD lock; writer alone owns files. No task per chunk or generic job framework.
4. Keep I2S enabled across chunk transitions. Writer splits blocks at exact sample boundaries, finalizes the
   old chunk and opens the next while capture continues into free blocks. WAV header arithmetic is cheap;
   file-system delay is the reason for buffering. Do not defer normal WAV finalization to a server.
5. Make shutdown explicit: publish capture completion, drain all queued blocks, finish the writer, then delete
   tasks and free their resources. Stop/error must remain observable even when data queues are full. No task
   may wait for a render lock during exit; no forced task deletion while it owns files or storage locks.
6. Add compact timing and buffer diagnostics, host checks, then build PaperMono and run the hardware gate below.

### Initial constants and measurement

Proposed trial defaults: 16,000 Hz, mono 16-bit, gain 2x, 300-second chunks, 1,024 samples per block,
32 blocks (65,536 bytes / 2.048 seconds). These are experimental starting values, not a losslessness guarantee.
Justify the pool allocation as protection against measured SD stalls; verify internal RAM versus PSRAM placement
against the actual read/copy path, leaving DMA buffers in driver-compatible memory. Record free internal heap,
PSRAM use and task stack headroom; allocation failure must leave recording idle and hardware released.

Time writer batches with a monotonic timer, including the complete file transition (header/flush/close/rename/open)
when a batch crosses a boundary.
Track maximum and a small fixed latency histogram, queue high-water, sample counts, DMA overflow and clipping.
Aggregate counters in memory; emit a brief periodic/stop summary outside the capture hot path. Existing
`VoiceMemoActivity.cpp:111–133` measures only maximum write time and reports it at completion.

Required throughput = sample rate × channels × bytes/sample. Buffer time = usable queued bytes / byte rate,
with allowance for blocks currently held by capture/writer. The pool must cover stalls with margin; sustained
writer throughput must also exceed capture throughput. Never silently overwrite unread blocks on exhaustion.
Change one variable at a time. Gain affects amplitude/clipping, not byte rate; sample rate affects bandwidth
and bytes/sec; chunk duration affects rollover frequency and file size; block size affects call overhead and
stop granularity; pool size affects stall tolerance. Shorten chunks temporarily to stress transitions quickly.
Do not reduce audio quality as a substitute for measuring storage or fixing lost-sample bugs.

### Verification gate

Host: known numbered samples survive chunk splitting exactly once and in order; test partially filled blocks,
exact-boundary Stop, and injected open/write/finalize failures. Verify WAV lengths and unique paths. Real queue
exhaustion, driver short reads and shutdown/drain scheduling remain hardware checks, not covered by the storage stub.

PaperMono: capture a timed source for 30–60 minutes, inspect every boundary for gaps/duplicates, exercise
screen refresh and repeated Stop near rollover. Test full/missing card, repeated sessions, exit, explicit sleep,
and reader return. Compare telemetry with USB monitor attached and detached; also test battery operation.
Log transition latency, buffer high-water, overflow and heap/stack. Driver sample counts alone cannot prove
that the physical signal had no gaps; include listening/timed-source checks. Repeat with shorter chunks.

### Pause/resume and metadata

Pause drains and finalizes the current chunk, releases microphone capture, and keeps the session and buffer
pool selected. Resume opens the next chunk in the same directory; Stop or leaving the screen ends the session.
Reopening old sessions or resuming after reboot is out of scope. The gap duration measures microphone-stop to
microphone-restart using the monotonic microsecond clock, including drain time; it is not a wall-clock timestamp.

`session.jsonl` is an append-only journal, one small JSON object per line. Its version-1 start event declares
sample rate, mono 16-bit format and gain. Completed chunk events record chunk number, sample count and cumulative
end sample. Recording/resume/pause/stop/incomplete events record the written sample offset; resume also records
`gap_ms`. Fixed internal fields are formatted into bounded buffers; no JSON parser or repeated document rewrite
is needed. Every entry is closed with a checked result. Audio publication precedes its chunk entry: if journal
writing fails, the WAV is retained and the session reports failure. An interrupted trailing entry or absence of
`stop` means the session needs inspection, not that its completed WAVs should be deleted. There is no automatic
repair, resume-after-reboot or guarantee against filesystem damage during power loss.

After local recording is reliable: one streamed WAV upload with acknowledgment, then a durable one-at-a-time queue
within Tools. Separate recording metadata from pending/retry/acknowledged upload state. Decide whether the server
needs a session manifest when its API is scoped. No server, authentication or upload framework in this increment.

## Current workflow and verification

Tools → Voice memo has four existing themed buttons: Record, Pause, Resume, Stop. Invalid actions in the current
state are ignored, so old screen hit targets cannot toggle a new session. Confirm starts, resumes a paused session,
or stops an active capture. Stop overrides a pending Pause. Back and explicit sleep drain/save before leaving;
idle sleep is prevented during capture/finalization and allowed while paused (ending that session on exit).
The frontlight overlay temporarily covers the tool without ending it; the activity manager includes covered
activities in its sleep check, so recording remains awake. Test opening/closing that overlay during capture.
The UI shows Recording…, chunk number and the session directory. It refreshes on state/chunk changes, not every
second; there is no live stopwatch or five-minute session cap. A red indicator pulses for approximately 150 ms
every two seconds while capturing. The microphone HAL owns the PMIC red LED bit, polled by the existing main loop
under RenderLock, including while the frontlight overlay is visible. No new task or capture-thread I2C traffic.
Pause, Stop, capture failure and exit disable the indicator. Blink timing can stretch during a display refresh;
this is visual feedback, not a recording clock. Battery savings have not been measured.

The activity owns two workers: capture has a 4 KiB stack; writer has 6 KiB for ADPCM or 4 KiB for PCM. A single checked pool contains 32 blocks of 1,024 int16
samples plus per-block counts; two bounded FreeRTOS queues transfer block indices without copying audio. The pool
uses the normal nothrow allocator (placement reported in logs), avoiding large task-stack allocations; ESP-IDF
retains ownership of its DMA buffers. The pool and queues are reused while paused and released on Stop/error/exit.
The capture worker has priority 2, writer priority 1, both unpinned. Capture does no SD access or serial logging.
The writer owns all audio files; main-task metadata/power cleanup happens only after workers acknowledge completion.

Config: `src/activities/tools/VoiceMemoConfig.h`. Default 16 kHz mono16, 2x saturating gain, approximately five-minute ADPCM chunks,
64 KiB PCM pool. Storage defaults to IMA ADPCM; PCM remains a compile-time baseline (see below).
Gain amplifies air/rubbing noise too. Filtering, AGC and uploads are deferred.

Commands from the repository:

```sh
bash test/voice_memo/run_host_tests.sh
.venv/bin/pio run -e papermono
.venv/bin/pio device monitor --port /dev/cu.usbmodem101 --baud 115200
.venv/bin/python test/voice_memo/check_wav.py /path/to/copied/session-000001
```

Host checks compile the real session writer with a fault-injecting storage substitute and address/undefined-behavior
sanitizers. They check sample order across nonaligned boundaries, WAV lengths, empty/exact-boundary Stop, session
uniqueness, pause/resume events, and open/write/header/close/rename/journal failures. The Python checker verifies a
copied session's files against its journal. These checks do not simulate FreeRTOS scheduling, real SD stalls or I2S.

On PaperMono, first record past five minutes, Pause/Resume, then Stop and copy the entire session. Check it with
the script and listen across boundaries. Then run the 30–60 minute timed-source test and failure/exit tests above.
Verify the screen stays still between chunk/state changes, and the red LED pulses during capture and turns off
on Pause/Stop/error/exit, including after using the frontlight overlay.
For a faster rollover stress test temporarily lower CHUNK_SECONDS, rebuild, then restore the normal value.
Expected: zero overflow, no unexplained sample gaps/duplicates, headroom in buffer/stack/internal heap, and no
resource loss after repeated sessions. Also try Stop while draining a Pause, rapid invalid taps, and Stop exactly
near a boundary. Forced power loss may leave partial audio and an unfinished journal; earlier WAVs remain valuable.

Serial diagnostics: allocation placement/internal heap/PSRAM; each opening/rollover's complete batch duration and
queued blocks; maximum batch duration and five latency buckets (<1ms, <10ms, <100ms, <1s, >=1s), final close duration,
worker stack headroom, capture peak/clipping and buffer high-water. Batch timing includes any header, close, rename,
journal and next-file open needed by that batch. Durations are microseconds. No per-sample logging. Buffer-full,
DMA/read, setup and storage failures stop with an incomplete result, never a silent overwrite of queued samples.

Build and host checks are local evidence only. Long-session continuity, pause/resume timing, memory placement and
full-card behavior still require device validation. The earlier ten-second hardware recording worked; that alone
does not establish this new pipeline's reliability.

## IMA ADPCM recording increment

Default storage is mono four-bit Microsoft IMA ADPCM WAV using the pinned
[ADPCM-XQ library](../lib/AdpcmXq/README.md). `VoiceMemoConfig::FORMAT` selects `ImaAdpcm4` or `Pcm16`;
`VOICE_MEMO_PCM_BASELINE` also selects PCM for comparison builds. There is no runtime selector or simultaneous
dual recording. Capture remains 16-bit PCM at 16 kHz with the same 2x saturating gain before queueing.
The existing writer assembles codec blocks, encodes and writes them through HalStorage. No additional task,
filter, noise shaping or lookahead is enabled. ADPCM encoding is the compression step.

### Blocks, samples and lifetime

Each 256-byte block holds 505 samples. Normal rollover rounds the five-minute target **up** to the next complete
block: 4,800,025 samples at 16 kHz, or 300.0015625 seconds. No samples are discarded at rollover. The writer stages
incoming PCM, writes complete blocks, and finalizes a chunk when it reaches that threshold. A new file is opened
only when its first complete block is available; no empty WAV or placeholder is created for a shorter recording.

Pause/Stop still stop capture and drain the existing queue. The writer retains all complete blocks and intentionally
discards only the last 0–504 samples (at most 31.5 ms at 16 kHz). It never pads them. Resume starts a fresh chunk in
the same session; no tail crosses the intentional pause. The WAV `fact` count, durations and all session offsets
count retained samples only. PCM fallback retains its exact five-minute splitting and every final PCM sample.
No new task, capture handshake or allocation is needed for alignment.

The context survives Pause and chunk rollover, but every encoded block contains its own predictor/index and can
decode independently. Stop/error/exit releases the context and staging buffers after the workers finish.

New journals use version 3. The start/chunk fields are unchanged from version 2, but `padding_samples` is always
zero. After finalizing a successful Pause/Stop segment, a nonempty discarded tail produces a separate entry:

```json
{"event":"discard","sample":505,"discarded_samples":1,"reason":"incomplete_tail"}
```

`sample` is the retained session offset, not the captured offset. The following `pause`/`stop` entry uses the same
offset. Zero discarded samples needs no entry. A sub-block segment can therefore contain a discard entry and no
chunk entry. Chunk numbering remains consecutive across Resume. Failure to persist the discard entry reports a
storage error; it must not silently report success. The checker sums the discarded counts separately from duration.

Existing version-1 PCM and version-2 padded ADPCM sessions remain readable. Failed writes/finalization retain
existing `.part` files and report failure; these are never relabeled as intentional tail discard. Only successfully
written complete codec blocks advance retained counters. A capture failure preserves any current file as `.part`;
a failure before the first complete block may leave only the unfinished journal. Do not treat partial headers as
finalized or automatically repair them. Earlier completed WAVs remain available. Power-loss guarantees are unchanged.

The encoder adds roughly 2 KiB of checked, session-lifetime heap allocations: 505 PCM samples, 256 encoded bytes,
context and its index-search scratch array. This avoids large local arrays and repeated allocations per block.
The library context allocation is checked before use, including its internal memset. The writer's extra 2 KiB
stack allowance covers the initial recursive index search; it is provisional, not measured hardware headroom.
The existing PCM pool remains 64 KiB, so compression does not expand capture's stall budget. Small 256-byte
storage writes may have different latency from PCM; measure under the actual card workload.

### Desktop validation and merging

```sh
bash test/voice_memo/run_host_tests.sh
.venv/bin/python test/voice_memo/check_wav.py /path/to/session-000001 \
  --merge /path/to/session-000001-merged.wav
```

`--merge` preserves decoded PCM export, including per-chunk trimming of legacy padding. FFmpeg must be on PATH
for ADPCM decoding. To merge compatible **padding-free** ADPCM chunks without decoding or re-encoding:

```sh
.venv/bin/python test/voice_memo/check_wav.py /path/to/session-000001 \
  --merge-adpcm /path/to/session-000001-adpcm.wav
```

The direct merger writes one header with combined RIFF/data/fact lengths, then copies each chunk's encoded blocks
unchanged. It validates matching format/rate/block framing and rejects any padding, including padding in older
recordings. Use decoded `--merge` for those. Both exports omit intentional pauses and discarded tails. A session
with no complete blocks can export an empty PCM WAV, but direct ADPCM export reports that there is no audio to merge.

Output must be a new file outside the session directory. Sources are preserved. Direct merge uses bounded copying;
decoded export also uses a temporary decoded chunk on disk. Both formats have ordinary WAV/RIFF size limits, and
ADPCM `fact` has a 32-bit sample-count limit; split very long exports rather than wrapping lengths. Structural
checking and direct merge require only Python's standard library. Never concatenate legacy padded payloads blindly.

Native macOS `afinfo`/`afconvert` rejected research ADPCM files, including an FFmpeg control. Decoded PCM is the
compatibility path. VLC/QuickTime playback of the firmware representation remains an application/device test.

Host checks exercise both formats with the actual writer, sanitizers, five-minute boundaries, 1/504/505/506-sample
endings, discarded counts, sub-block Pause/Resume, empty Stop, allocation failure and storage faults. A split
capture batch crosses the approximate five-minute boundary without losing samples. FFmpeg decodes the directly
merged ADPCM and must produce exactly the same PCM bytes as decoding the individual chunks in order. Legacy
padded sessions still export their exact true sample counts; direct merging rejects internal padding. The checker
also rejects malformed headers/counts and refuses to overwrite originals. These are not measurements of ESP32 encoding throughput or microphone continuity.

### PaperMono gate

Upload the ADPCM build and record speech past two chunk boundaries, Pause/Resume, Stop, and copy the whole session.
Compare with PCM using the same rate/gain and a repeatable timed source; keep original references.
Expect ordinary ADPCM chunks to last 300.0015625 seconds, zero padding in every published WAV, and only journaled
tail loss on Pause/Stop. Exercise Stop just before/after a full block and around rollover, a sub-block recording,
and repeated short Pause/Resume. Confirm retained totals plus intentional discards match accepted input on clean
runs. Export both PCM and direct ADPCM on the desktop, decode with FFmpeg and compare samples. Real scheduling,
SD stalls, sleep/exit and first-block stack checks still require hardware; host tests do not establish continuity. Listen around
boundaries and to quiet speech, consonants and loud transients. Check zero DMA/queue overflow and stable heap across
repeated sessions, including immediate Stop, exit and the frontlight overlay. Repeat on battery and with USB logging.

Existing batch timing includes encoding and rollover. Each finalized chunk also logs total `encode_us` and
`max_encode_us` (encoding only); existing `writer_stack`, `capture_stack`, `buffer_high`, heap and clipping logs
provide the rest. Verify stack headroom during the first block and queue margin during SD stalls before extending
meeting use. No on-device throughput, power saving or subjective audio-quality claim is made by host tests.
Filtering and the notebook remain separate later experiments.

Earlier padded implementation evidence (before block-aligned tails): both PaperMono PCM/ADPCM builds passed. S3 GCC 14.2 `-Os -fstack-usage`
reports the patched encoder frame at 128 bytes (upstream 848); recursive search frames remain 160 bytes each.
The saved ten-second `.tmp/memo-0001.wav` reference, passed through the actual host session writer without
additional gain, produced 81,212 bytes versus 320,044 bytes PCM (74.6% smaller). FFmpeg plus fact-count trimming
returned exactly 160,000 samples. Sample-aligned SNR was 27.89 dB with peak absolute sample error 5,953;
these numerical results do not establish perceived speech quality. The source recording was preserved.
