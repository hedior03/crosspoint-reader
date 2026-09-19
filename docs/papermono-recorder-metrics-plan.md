# PaperMono recorder metrics: implementation handoff

Status: implemented locally. Host checks pass and both PaperMono builds succeed (local 24 kHz PCM and shared 16 kHz
filtered ADPCM). Device measurements remain. Updated 2026-09-17.

## Goal and agreed decisions

Collect enough on-device evidence to choose recording rate, codec and buffer capacity without relying on
an attached serial monitor. Preserve recording reliability and keep the implementation local to the recorder.
Use ponytail full: small functions, fixed-size data, existing workers and HALs; no telemetry framework or new task.

- Collect **one-second summaries**, not individual sample/block log records.
- Batch several summary rows into each SD write. Sampling/report intervals and storage-write cadence are separate.
- Audio takes priority over diagnostic persistence. Keep bounded memory and explicitly count missed/dropped reports.
- Keep 1024 samples per capture block and 32 blocks until measured backlog justifies changing them.
- No automatic buffer sizing, new filters, uploads, runtime settings UI or codec changes in this increment.
- No commits, pushes or firmware uploads without a fresh explicit request.

## Read first

1. [Tools development principles](papermono-tools-development.md).
2. [Recorder behavior, ownership and checks](papermono-voice-memo-plan.md).
3. [Current roadmap](papermono-roadmap.md).
4. [Audio experiments](papermono-audio-experiments.md), [codec evidence](papermono-audio-codecs.md),
   [filter notes](papermono-audio-filters.md) and [Jupyter workflow](papermono-audio-jupyter.md).

Inspect branch, remotes, working tree and current code before editing. The recorder branch `codex/voice-memo` has been fast-forwarded into local `develop` through `25d7f3f9`.
Recorder configuration overrides and tail logging are committed. The documentation and audio lab are being
saved separately on `develop`; inspect current history/status rather than assuming a clean tree.
Preserve local experiments and gitignored recordings; do not stage broad directories.
The user opted out of GitButler. Follow AGENTS.md, use `rtk`, prefer `uv`, and use the repository formatting wrapper.

## Current configuration and behavior

Shared C++ defaults: 16 kHz mono16, IMA ADPCM, selected 150–4500 Hz filter enabled, **no separate gain**.
The supplied SOS coefficients remain unchanged: the original preset is for 16 kHz; the additional
[24 kHz preset](audio-presets/voice-owner-24000.json) is selected for filtered 24 kHz builds.

The original gitignored `platformio.local.ini` trial used these `[voice_memo]` settings.
The current shared setup is now committed in `platformio.papermono.ini` (see the integration note below):

```ini
format = Pcm16
sample_rate = 24000
filter_enabled = false
chunk_seconds = 300
block_samples = 1024
buffer_blocks = 32
read_timeout_ms = 100
no_audio_timeout_ms = 2000
```

At 24 kHz this PCM pool holds at most 1.365 seconds / 64 KiB of audio (excluding bookkeeping).
One full capture block represents 42.667 ms. These are capacities, not timers or guaranteed stall tolerance.
The local 24 kHz flags were resolved and checked with C++ static assertions, and this configuration now builds
locally (device testing remains). The earlier 32 kHz PCM configuration also built successfully.

Capture feeds a bounded pool through index queues. The writer consumes immediately and writes PCM batches
or full ADPCM blocks; it does not wait for the pool to fill. Filtering is per sample and needs only SOS state.
Codec staging is separate: 505 PCM samples become 256 encoded bytes. At 24 kHz, normal Pause/Stop may discard
0–504 samples (maximum 21 ms); PCM discards no codec tail. Rollover discards none.
Startup now logs the maximum discard; Pause/Stop logs actual nonzero discard in samples and milliseconds.
The journal already records actual discard. Preserve this behavior and storage-error/.part semantics.

A free-block wait currently fails after 20 ms when no block returns. The microphone read timeout and
no-audio timeout are separate. HalStorage's mutex wait is indefinite; there is no recorder-level SD-write
cancellation deadline. Do not describe buffer duration as a hard I/O timeout or promise bounded Stop time.

## Reuse these existing code paths

- `src/activities/tools/VoiceMemoConfig.h`: compile-time defaults and local overrides; derived sizes stay derived.
- `VoiceMemoActivity.cpp`: capture/write loops, batch/filter elapsed maxima, latency buckets, buffer high-water,
  capture/writer stack reporting, orderly drain and worker teardown.
- `VoiceMemoSession.cpp`: encoder elapsed total/max, storage operations, chunk finalization, journal and tail logs.
- `lib/hal/HalStorage.*`: mandatory serialized storage through HalFile; do not bypass with raw SDK/SdFat access.
- `lib/hal/HalMemory.*`: free, minimum-free and largest-block statistics for internal RAM and PSRAM separately.
- `lib/hal/HalPowerManager.*`: current battery percentage access.
- FreeInk `BatteryMonitor::readM5Pm1Status`: existing voltage/external-power readings; percentage is voltage-derived.
  Expose only the minimal needed snapshot through HAL if necessary. Reuse I2C ownership; no direct app SDK access.

## Small implementation scope

1. Extend existing timing counters with count, total, maximum and fixed histogram buckets for filter processing,
   ADPCM encoding, SD writes and whole writer batches. Include storage lock wait in observed write latency;
   identify rollover/finalization work separately. Reuse timestamps where possible. No per-sample instrumentation.
2. Snapshot these counters into one-second interval records with elapsed-time timestamps, actual interval duration,
   sample counts and interval buffer high-water. A delayed writer cannot manufacture exact one-second reports:
   explicitly represent longer/missed intervals. Keep arithmetic and memory bounded and define counter ownership.
3. Persist records to a separate `metrics.csv` inside each session. The existing writer should batch multiple rows
   when audio backlog is low. Choose and document a small fixed buffer, batch threshold and bounded overflow policy;
   losing counted telemetry is preferable to blocking audio indefinitely. No unbounded queue or heap churn.
4. Preserve summaries across Pause/Resume, save pending data after normal drain on Pause/Stop, and close handles
   before activity destruction. Diagnostic failure must not falsely mark good audio as corrupt or successful audio
   writes as failed. Report telemetry loss/failure once and keep audio errors distinct.
5. Sample battery voltage, percentage and external-power status at a slow cadence (suggest 30–60 seconds), using
   the main task/HAL rather than capture. Record unavailable readings as unknown, not zero. Sample memory/stack
   at a modest cadence; distinguish internal RAM from PSRAM. Do not add cross-task data races or read deleted tasks.
6. Document columns, units, histogram boundaries, flushing policy and known measurement limitations. Include
   enough configuration metadata to compare sessions: rate, format, filter, capture block samples and pool block
   count, codec framing and firmware identity. Record these once in the session start entry.

The histogram provides approximate p90/p99 ranges, not exact percentiles. Count + total gives a mean;
maximum and occupancy expose dangerous stalls. Variance and full event traces are deferred.
Elapsed filter/encode time includes task preemption: it is **not** CPU utilization. Defer FreeRTOS runtime/idle
statistics until the pinned build's support and overhead are verified and a specific question requires them.
The current PaperMono battery path does not expose measured current. Voltage trends are not precise mAh/Wh;
accurate power comparisons may require an external instrument. Do not invent a current measurement.

A one-second row rate is 3600 rows/hour; SD write rate must be lower through batching. Avoid flushing every row.
State what recent telemetry may be lost on power failure. Measure diagnostic-write duration and compare enabled
versus disabled runs; logging is not free. Do not extend the audio journal with high-volume diagnostic rows.

## Verification and done criteria

- Focused host checks: held-block peaks across busy-writer and interval boundaries, startup capture configuration,
  interval accumulation/reset, histogram edges, delayed intervals, bounded telemetry overflow,
  final partial report, Pause/Resume lifecycle and diagnostic I/O failure distinct from audio failure.
- Preserve recorder host tests, exact sample counts, chunk rollover and legacy PCM/ADPCM merge behavior.
- Build local 24 kHz unfiltered PCM and shared 16 kHz filtered ADPCM without overwriting the user's local settings.
- Device gate: record across multiple rollovers, Pause/Resume/Stop/exit; inspect backlog, stack and memory headroom,
  confirm no audio overflow, verify WAVs with the existing checker and listen to a timed source at boundaries.
- Compare logging enabled/disabled under the same recording conditions. Test slow-storage/backlog behavior and
  show that missed telemetry is visible rather than silently overwriting counters or stalling capture.
- Battery comparisons must record external-power state and use consistent frontlight/radio conditions; use
  unplugged runs for discharge trends. Serial remains optional, not the only place to retrieve results.
- CSV must be directly loadable on the desktop. Plotting/dashboard work is a follow-up; do not build a framework.

After this evidence, compare 32 versus 16 capture blocks at 24 kHz. Choose capacity from worst observed backlog
plus margin, not average SD speed. No finite buffer fixes sustained writer throughput below capture throughput.

## Implemented format

`metrics.csv` lives beside `session.jsonl` in every new session directory. Configuration needed to compare
sessions stays in the version-3 `session.jsonl` start event: `sample_rate`, `format`, `filter`, `block_bytes`,
`samples_per_block`, `gain_db`, `firmware`, `block_samples` and `buffer_blocks`.
`block_samples` and `buffer_blocks` are the effective compiled capture-pool settings; `block_bytes` and
`samples_per_block` describe codec framing, not capture buffering. There is no separate telemetry
framework, task or queue: the existing writer task folds its own timings, formats a row, and appends it to a
bounded buffer that is flushed through HalStorage.

Columns (74, comma separated, one header row written once):

- `t_ms` — milliseconds since session start (monotonic `esp_timer`, not wall clock).
- `elapsed_ms` — actual duration covered by this row.
- `missed` — whole scheduled intervals not separately reported because the writer was delayed.
- `samples` — captured samples folded into the interval.
- `buffer_high` — maximum blocks held by capture/queues/writer during the interval (0..`BUFFER_BLOCKS`).
  Capture increments ownership immediately after taking a free block. Capture's unused/error returns and the
  writer's processed/error-drain returns decrement ownership before returning the index to the free queue.
  One atomic word holds current occupancy and its peak; reporting resets only the peak to current occupancy.
  This preserves peaks while the writer is busy and includes blocks held across interval boundaries. Each
  Record/Resume initializes the interval before starting workers; timing counters remain writer-owned.
- Six timing counters, each `n` (events), `us` (total), `max_us` (maximum) and `b0..b4` (buckets):
  `filter` per-block filter/convert, `encode` ADPCM block encodes, `write` audio SD writes (including the
  HalStorage mutex wait), `batch` whole writer batches (filter + encode + write + rollover),
  `rollover` chunk open/close/rename/finalize transitions, `diag` `metrics.csv` writes.
- Buckets are `<1ms`, `<10ms`, `<100ms`, `<1s`, `>=1s`. They give approximate p90/p99 ranges and occupancy,
  not exact percentiles. Count plus total gives a mean.
- `battery_mv`, `battery_pct`, `external_power` (0/1), sampled on the main task every
  SystemMetrics (45 s) through `HalPowerManager::getBatteryStatus`. The writer only copies the cache.
- `heap_internal_free`, `heap_internal_min`, `heap_internal_largest`, `psram_free`, `psram_min` in bytes,
  internal RAM and PSRAM separately (unavailable heaps are empty). SystemMetrics samples these every second;
  minima are allocator lifetime minima, not recording/row minima. The global snapshot also exposes PSRAM largest block.
- `writer_stack_free`, `capture_stack_free` — FreeRTOS stack high-water marks in bytes; smaller means less
  headroom. Sampled by the activity every `STACK_INTERVAL_MS` (45 s), only while task handles exist.
- Appended system fields (the original 63 columns retain their order and units):
  `cpu_cores`; `cpu0_busy_bp`, `cpu1_busy_bp` (0..10000 basis points, 10000 = 100%);
  `cpu0_interval_us`, `cpu1_interval_us` (each core's actual measurement interval);
  `cpu0_t_us`, `cpu1_t_us`; `cpu_mhz` (instantaneous frequency, not interval average);
  `cpu_frequency_t_us`, `memory_t_us`, `battery_t_us`.
- Every appended `*_t_us` is signed microseconds relative to the same session start as `t_ms`.
  Cached measurements before Record have negative timestamps; Resume does not change the origin.
  Repeated rows can retain identical timestamps. At row time, approximate age in microseconds is
  `t_ms * 1000 - group_t_us` (the row timestamp has millisecond resolution).
  CPU timestamps/intervals are empty when utilization is invalid; frequency remains independently available.
  Memory/battery timestamps identify the completed attempt, including failures; availability comes from
  each value's empty field. A failed battery attempt clears affected values rather than refreshing old data.
- An empty field means unknown/unavailable, never zero.

Flushing policy: rows are appended to a fixed `BATCH_BYTES` (2048) buffer. The writer flushes at
`BATCH_ROWS` (4) rows when the filled-block queue is empty, at Pause/Stop drain, and when a row would
overflow the buffer. Four rows are a target, not a guarantee: the byte limit can flush earlier.
`ROW_MAX` is 1024 bytes including NUL. A conservative full-width decimal bound is 1003 bytes:
69 base + 6 × 108 timing + 110 environment + 174 appended system + 2 LF/NUL. The unchanged
2048-byte batch can hold a complete row; compile-time and host checks enforce these bounds.
A failed flush drops the buffered rows, is logged once, and never changes the audio
result. Here “flush” means writing the RAM batch through HalFile, not calling `HalFile::flush()` to sync
filesystem state. Pause writes pending rows but keeps the handle open; Stop/exit closes it. Power loss before
close can lose access to the session's metrics, not just the latest RAM batch. `metrics.csv` is opened with
`O_EXCL`; an existing session is never overwritten.

### Accepted trade-offs and remaining gaps

The owner accepted the current overflow-write policy, lack of periodic filesystem synchronization, and
best-effort diagnostic error handling for this increment. A full metrics buffer can trigger an SD write even
with audio queued; neither small writes nor bounded RAM imply bounded SD latency. A short diagnostic write
can leave a partial CSV row, and subsequent appends can join onto it. Recovery for these cases is out of scope.

Two review gaps remain unchanged: the final report precedes chunk finalization, so final close/header/rename
and last diagnostic-write timings are omitted; dropped-row counts are not exported (the `missed` column only
counts delayed intervals). Serial can report diagnostic failure, but the saved CSV alone does not provide a
complete loss count. These limitations qualify the original acceptance criteria above; do not report them as
verified behavior.

Known limitations: elapsed filter/encode time includes task preemption, so it is not CPU utilization.
`write` covers audio writes only; `diag` shows the diagnostic write cost separately. Battery voltage trends
are not precise mAh/Wh and no current measurement exists. The first row of a segment starts at Resume, so a
pause gap is not reported as an interval. Histograms are ranges, and there is no variance or event trace.

## Device checks remaining

Build and host checks do not exercise FreeRTOS scheduling, real SD stalls or I2S. On PaperMono, record across
several rollovers and Pause/Resume/Stop/exit, then copy the whole session and confirm:

- `metrics.csv` loads directly on the desktop and its row count roughly matches recorded seconds.
- `buffer_high` stays well below `BUFFER_BLOCKS`; `write` and `rollover` maxima show plausible stalls.
- Zero audio overflow and correct WAVs (existing checker) with listening across boundaries.
- `heap_internal_*`, `psram_*` and stack columns show headroom; compare a logging-enabled and a
  logging-disabled build under identical conditions to measure the diagnostic cost.
- Slow-storage/backlog behavior shows missed intervals in `missed` rather than stalled capture.
- Battery runs record `external_power` and use consistent frontlight/radio conditions; unplugged runs give
  discharge trends.

## Shared sampler integration (2026-09-18)

See [SystemMetrics implementation and SDK evidence](papermono-system-metrics-plan.md#implementation-evidence-and-limits).
Main-task sampling continues during Pause, overlays and after recorder exit. The writer copies a consistent
cached system snapshot before formatting each candidate report; sampling never occurs in the writer or capture.
Task-stack polling and worker teardown remain activity-owned. The audio pipeline, supplied filter coefficients,
recorder counters, overflow-write policy, sync/close behavior and accepted diagnostic-loss gaps above are unchanged.
The committed `platformio.papermono.ini` settings are 24 kHz filtered IMA ADPCM, 1024 × 16 buffers,
300-second chunks (experiment E). Machine-specific overrides remain in ignored `platformio.local.ini`.
This is a configuration observation, not a device/session-number verification.
