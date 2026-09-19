# PaperMono system performance snapshot

Status: implemented locally; host and target checks recorded below. Device acceptance remains open. 2026-09-18.

## Goal

Expose a small, process-wide, read-only performance snapshot to activities and diagnostics.
Keep voice memo's detailed timing, occupancy, stack and persistence metrics.
Sampling frequency and consumption frequency are independent: reading cached data never triggers
hardware access, resets counters or makes old measurements appear new.

Read alongside:
- [Building PaperMono tools](papermono-tools-development.md)
- [Recorder metrics plan](papermono-recorder-metrics-plan.md)
- [Voice memo lifecycle and behavior](papermono-voice-memo-plan.md)
- [Development rules](../AGENTS.md)

## Repository evidence and placement

Line references describe the checkout inspected for this plan; recheck before implementation.

- lib/hal/HalStorage.h:68 provides the existing static-instance/getInstance singleton pattern.
- lib/hal/HalPowerManager.cpp:136 exposes checked battery status through the HAL.
  Its frequency transitions at lines 34-68 remain under power-manager ownership.
- lib/hal/HalMemory.h exposes internal and PSRAM free/minimum/largest-block statistics.
- src/activities/tools/VoiceMemoActivity.cpp:255 currently samples battery, heaps and recorder
  task stack headroom. Reuse its HAL sources and preserve task-lifetime ownership.
- src/main.cpp:750-795 runs activities, tracks loop duration and applies idle power saving.
  Integrate once per eligible main-loop iteration, after initialization and before activity work;
  inspect all early returns and shutdown paths before choosing the final insertion point.
- src/activities/ActivityManager.h:43 owns the current activity. Activity.h:20 holds its name.
  Any activity label must be copied under that owner's rules, never retained as an activity pointer.
- platformio.ini:390-397 selects ESP32-S3 with dio_opi for PaperMono.
- Installed SDK esp32s3/dio_opi/include/sdkconfig.h:1033-1044 enables runtime statistics,
  a uint32_t runtime counter, and the ESP timer timebase.
- SDK freertos/esp_additions/include/freertos/idf_additions.h:166-190 declares
  ulTaskGetIdleRunTimeCounterForCore, guarded by non-SMP/runtime-stat/idle-handle support.

Proposed implementation: lib/SystemMetrics/SystemMetrics.h and .cpp, using getInstance()
and one static instance. Keep OS sampling details private in the implementation. Reuse HALs
for battery/memory; do not access the PMIC or SDK battery classes directly.

## Scope and API

Minimal public operations: begin(), update(), and snapshot().
Only the main task calls begin/update. Other task-context consumers may copy the snapshot.
No ISR access, subscriptions, callbacks, dynamic registration, settings screen, new task,
timer callback, history buffer or generic telemetry framework.

Snapshot groups:
- CPU: configured core count, busy percentage per core, actual interval duration, sample timestamp,
  validity, and observed CPU frequency in MHz. Store percentages as integer basis points (0..10000).
- Memory: internal heap and PSRAM free/minimum/largest-block bytes, timestamp, availability.
  Minimum values retain the allocator's existing semantics; do not claim interval minima.
- Battery: mV, percentage, external-power flag, availability per field, timestamp.
- Optional small current-activity label, copied by the activity owner with explicit truncation.
  Include only if the existing owner can supply it without allocations or widened lifetime access;
  CPU intervals spanning activity changes must not be attributed wholly to the final activity.

The snapshot is a plain fixed-size value. Use existing unavailable semantics or explicit flags;
zero voltage, zero usage and zero free bytes must not be confused with missing data.

## Cadence and freshness contract

| Group | Initial cadence | Semantics |
| --- | --- | --- |
| CPU | 1 second | Interval non-idle scheduler time per core |
| Frequency | With CPU | Instantaneous sampled MHz, not interval average |
| Memory | 1 second | Point-in-time heap snapshot plus allocator minima |
| Battery | 45 seconds | Cached battery measurement, including external power |

These are constants, not a new user configuration surface. Consumers may read more often,
but receive the same measurements until the producer publishes another sample.

Use a 64-bit monotonic microsecond clock. Age is read-time minus measurement-time; calculate
it when consuming rather than incrementing an age field. Each independently sampled group
has its own timestamp. Timestamp a completed measurement consistently; collection is not
simultaneous across groups. A consistent publication does not imply identical measurement times.

On boot, initialize memory and battery when their HALs are ready; establish CPU baselines.
CPU utilization remains unavailable until a second valid reading establishes an interval.
No cadence guarantee: display/SD/main-loop delays produce older snapshots and longer intervals.
When overdue, sample once using actual elapsed time; do not replay missed ticks.
Reading the store never wakes the device or disables power saving.
Deep sleep/reboot starts a fresh history. Rebaseline after unsupported clock/counter discontinuities.

On battery read failure, mark affected fields unavailable for that attempt; do not relabel an
old successful value as fresh. Keep cadence bookkeeping separate from successful data freshness.
A consumer can identify both unavailable data and stale data without receiving a fabricated zero.

## CPU measurement contract

Prefer the verified per-core idle counter API; do not use formatted runtime-stat strings,
task enumeration, idle-hook iteration counts or raw cycle counters.

For each core, subtract consecutive idle counters using their actual unsigned counter width.
With an ESP-timer-based runtime clock, compare idle delta with elapsed time in the same units:
busy = 1 - idle_delta / elapsed. Each core has its own elapsed-time denominator; do not multiply
it by the number of cores. Expose per-core results rather than hiding one saturated core in an average.

Before implementation, inspect the selected SDK implementation or matching source to establish
whether counters include the currently running idle slice, how cross-core reads are synchronized,
and how sleep/interrupt time is accounted. Header availability alone does not prove accuracy.
If this API cannot provide credible interval estimates, resolve that with evidence before adding
a more expensive fallback. Unsupported configurations report unavailable; do not patch SDK build
flags or silently switch to estimated idle-loop counts.

A 32-bit microsecond counter wraps approximately every 71.6 minutes. Modular subtraction
handles ordinary wraparound only when the elapsed interval is shorter than that range.
For longer gaps, zero intervals, invalid deltas or resets, invalidate the interval and rebaseline.
Do not silently clamp large accounting errors into plausible utilization.
Account for any small documented sampling skew explicitly.

This is scheduler non-idle time, not exact instruction cycles, isolated interrupt cost, useful
work, electrical power or energy. Busy waiting counts as busy. Sampled frequency is not average
frequency. Battery voltage and CPU load cannot provide mAh/Wh without current measurement.

## Synchronization and resources

Collect outside the publication lock. Publish or copy only the fixed snapshot under a brief
cross-core-safe lock, following existing ESP32 synchronization patterns. Never hold it during
HAL reads, logging, SD access or scheduler queries. Independently atomic fields alone do not
guarantee a consistent multi-field snapshot.

Use static storage and a statically initialized synchronization primitive where supported.
No recurring heap allocation and no task creation. Keep local variables within the repository's
256-byte stack-variable budget; measure snapshot size and use producer-owned staging if needed.
Document static RAM and stack costs from the final implementation rather than estimating savings.

The sampler consumes CPU itself. Record host-independent collection duration during device
validation; compare enabled/disabled builds. Do not promise negligible overhead before measuring it.

## Voice memo integration and persistence

Replace only the duplicated battery/heap collection with copies from SystemMetrics.
Keep recorder task stack sampling in VoiceMemoActivity while task handles are valid.
Keep existing timing counters, held-buffer accounting, workers, HAL writes and accepted limitations.

The singleton does not write files. The existing recorder writer remains the persistence owner.
To make CPU/freshness data useful in experiments, append explicitly named CSV columns for per-core
CPU utilization, CPU interval and timestamp, sampled MHz, memory timestamp and battery timestamp.
Use session-relative timestamps or a documented session-start uptime mapping; never mix time origins.
Use empty fields for unavailable readings and document units. Avoid assigning a fresh timestamp
merely because a cached value was copied into a row.

Preserve existing columns and their meanings; update the documented column count and host parser
checks. Recalculate the maximum formatted row length before changing ROW_MAX; prove the fixed
2048-byte batch can hold a complete row. Adjust only bounds required by the added fields.
Preserve the accepted flush policy, without implying a four-row batch when the byte limit flushes sooner.
No new global CSV, storage task or SD writes from update/snapshot.

Global historical logging outside recordings and a performance screen are later consumers,
not prerequisites for this increment.

## Implementation sequence

1. Reinspect working tree and referenced ownership/lifecycle code. Preserve ongoing experiments,
   notebook work, .kilo and platformio.local.ini. Verify active SDK guards and counter semantics.
2. Implement fixed snapshot, singleton lifecycle, cadence and pure interval arithmetic.
3. Add main-loop initialization/update at a safe point, including early-return/shutdown inspection.
4. Integrate recorder cached environment and append documented CPU/freshness CSV fields.
5. Add focused checks, update recorder format documentation, run formatting and host/build checks.
6. Report device checks separately. Do not commit, push or upload without an explicit request.

## Verification and acceptance

Host checks, using existing lightweight assert-based tests:
- First CPU sample unavailable; second uses the actual elapsed interval.
- Per-core idle/busy/mixed deltas, wraparound, invalid deltas, zero and excessive intervals.
- One overdue sample, no catch-up burst; independent battery and CPU cadences.
- Repeated reads do not sample hardware, alter baselines or refresh timestamps.
- Unknown PSRAM/battery/CPU and failed reads preserve availability/freshness semantics.
- CSV names/units, empty fields, timestamp origin, maximum row bounds and existing session behavior.
- Code review plus a target stress check establishes cross-core snapshot consistency; arithmetic
  unit tests alone cannot establish synchronization correctness.

Build checks:
- Repository formatting wrapper only; git diff --check.
- Existing recorder host suite, including PCM/ADPCM, merge and filter checks.
- PaperMono firmware build with current local settings preserved.
- Compile a supported single-core/non-runtime-stat configuration to verify unavailable guards;
  no forced framework rebuild just to enable statistics.

Device checks:
- Idle and reading/page-refresh workloads: believable per-core intervals and snapshot age.
- Controlled work pinned to each core: correct core rises, no false combined 100% assumption.
- Normal/low-frequency transitions: correct observed MHz and timebase behavior.
- Recording with PCM and ADPCM: no new overflow, reasonable buffer peaks, valid WAVs, CSV age fields,
  Pause/Resume/Stop/exit behavior and continued global sampling after recorder exit.
- A run longer than 72 minutes crosses runtime-counter wrap without spikes.
- Battery/external-power transitions respect the stated 45-second observation lag.
- Missing/failed sensor reads appear unavailable; blocking the main loop produces visibly older data.
- Enabled/disabled comparison reports collection time, memory/stack cost, recording timing and any
  observed battery impact. No claim of energy savings from voltage alone.

Done means the integrated caller and lifecycle paths satisfy these contracts, not merely a passing
build. Any unsupported counter semantics, unverified hardware behavior or accepted limits must be
listed explicitly in the completion report.

## Implementation evidence and limits

### Selected SDK and interval semantics

Inspected the installed `framework-arduinoespressif32-libs` package `5.5.5+sha.b774170ff46`,
PaperMono's `esp32s3/dio_opi` configuration, and matching ESP-IDF v5.5.5 source:

- [Idle counter implementation, lines 489–504](https://github.com/espressif/esp-idf/blob/v5.5.5/components/freertos/esp_additions/freertos_tasks_c_additions.h#L489)
  takes the kernel lock and copies the idle TCB's accumulated uint32 counter. It **does not** add the
  currently running idle slice. Disassembly of the installed `libfreertos.a` confirms critical-section
  entry, one 32-bit TCB load, and critical-section exit; this is not a header-only inference.
- [Context-switch accounting, lines 3648–3693](https://github.com/espressif/esp-idf/blob/v5.5.5/components/freertos/FreeRTOS-Kernel/tasks.c#L3648)
  uses that same kernel lock. The selected port casts `esp_timer_get_time()` to uint32 microseconds.
  It adds a completed slice only when the new runtime-clock value exceeds the switch-in value.
  Thus the slice crossing **runtime-clock** wrap is discarded by the SDK. The implementation invalidates
  the observation crossing each 2^32-microsecond uptime boundary and rebaselines; it does not invent a
  corrected CPU value. A blank interval around 71.6 minutes is expected, not a claimed measured spike-free value.
  Ordinary **cumulative idle-counter** wrap uses uint32 modular subtraction.
- [Core 0 time slicing, lines 3373–3384](https://github.com/espressif/esp-idf/blob/v5.5.5/components/freertos/FreeRTOS-Kernel/tasks.c#L3373)
  and [other-core ticks, lines 83–143](https://github.com/espressif/esp-idf/blob/v5.5.5/components/freertos/esp_additions/freertos_tasks_c_additions.h#L83)
  request a scheduling switch when the ready list contains more than one equal-priority task.
  Both pinned idle tasks occupy the shared priority-zero ready list. With the selected dual-core,
  preemptive, time-sliced, 1000 Hz, non-tickless configuration, completed idle accounting normally lags
  by at most one tick. Single-core can leave its sole idle task without a switch for much longer.

Consequently utilization is enabled only for this audited IDF version, S3 dual-core configuration,
ESP-timer uint32 runtime stats, preemption/time slicing and no tickless idle. Other configurations keep
core count, frequency, memory and battery but return unavailable utilization; no SDK flags are changed.
No task enumeration, hooks, task creation, forced cross-core yields or alternative CPU estimator was added.

Each core query is bracketed by the 64-bit clock and timestamped on completion. Reads taking over one tick
are invalid and invalidate the baseline. The tolerated idle excess is one tick plus both endpoint query
widths (at most three ticks); only that small excess maps to zero busy. Larger deltas are invalid, not
clamped. Zero/backward/too-long intervals and clock wrap rebaseline. Normal interval estimates can still
have tick-scale error. Prolonged interrupt masking or scheduler suspension can delay idle accounting beyond
one tick, and cannot always be detected through this API; hardware stress validation is required.

Interrupt time is charged to whichever task's slice contains it; idle hooks, idle housekeeping and idle wait
are included in idle time. This measures scheduler non-idle time, not isolated ISR cost or useful work.
Frequency changes retain the ESP-timer timebase; observed MHz is sampled separately with its own timestamp.
Deep sleep restarts all state. No energy, battery-life or negligible-overhead claim is made.

### Integrated lifetime and freshness

`setup()` calls `begin()` after GPIO/power/clock initialization and wake-button verification, before the
SD-initialization early return. `loop()` calls `update()` before input/activity work and all early-return
branches, including USB exclusive-storage mode (the sampler never accesses SD). Once deep-sleep teardown
is committed, updates are suppressed. Blocking rendering, SD work and recorder teardown naturally leave
older cached values; there is no background sampler or catch-up burst.

`begin()` is idempotent; before it, values are unavailable. Memory and battery initialize immediately;
CPU only establishes baselines. Each group schedules its next attempt from its last completion.
Battery failures publish independent unavailable field flags and clear affected values; the completed
attempt timestamp also controls retry cadence. This does not label an old successful value as fresh.
Memory availability uses HAL total capacity, so a present heap with zero free bytes remains available.
Minimum-free values retain the allocator's lifetime semantics.

The writer copies the singleton before its row timestamp, without hardware access. Per-core CPU timestamps,
frequency, memory and battery retain their individual completion times. CSV timestamps use the recorder's
session origin, including negative values for pre-session cached measurements. Pause/Resume preserves that
origin. Detailed timing, occupancy and stack counters remain recorder-owned; final rows still precede audio
finalization, as explicitly accepted. No activity label is included, so intervals are not falsely attributed
to the last activity. See the [74-column CSV contract](papermono-recorder-metrics-plan.md#implemented-format).

### Resources and local verification

The target snapshot is 120 bytes, trivially copyable. Publication copies only this value under the same
statically initialized ESP32 spinlock pattern used by Calculator; all clocks, kernel queries and HAL access
are outside that lock. Producer staging is static. S3 object symbols total 306 bytes of sampler static data
(two 120-byte snapshots, 48-byte baselines, 8-byte lock, 8-byte cadence timestamp, two one-byte flags/instance),
excluding linker alignment. There are no sampler heap allocations. The recorder row grows by 512 bytes and
removes 32 bytes of duplicated environment atomics: 480 bytes added to its existing activity allocation,
not a new allocation or queue. Batch capacity remains 2048 bytes.

S3 GCC 14.2 `-Os -fstack-usage`, with the actual PaperMono compilation flags, reports individual frames:
CPU collection 48 bytes, memory 64, battery 48, publish/snapshot/update/begin 32 each;
recorder snapshot consumer `pollMetrics` 160, row formatting `takeRow` 112. These are individual compiler
frames, **not** transitive task stack headroom. Existing recorder stack-high-water measurements remain.

Local checks:

- `rtk proxy bash test/system_metrics/run_host_tests.sh`: real sampler with deterministic HAL/clock/kernel
  substitutes, supported and single-core/no-statistics modes, ASan/UBSan, arithmetic/wrap/skew/reset guards,
  first sample, cadence, availability, repeated reads, and concurrent host publication/copies.
  Host mutex stress does not validate the ESP32 spinlock or actual scheduler accounting.
- `rtk proxy env UV_CACHE_DIR=/tmp/crosspoint-uv-cache bash test/voice_memo/run_host_tests.sh`: existing
  PCM/ADPCM/session/fault/merge/filter suite plus cached CSV timestamps, empty fields, integer row bounds and
  Python parsing of actual persisted 74-column rows. The temporary UV cache avoids sandbox access to the
  user's cache; no notebook environment was modified.
- The repository formatting wrapper runs with `uv run --no-project --with clang-format==21.1.8` because no
  formatter is on the normal PATH. New files are formatted with that same wrapper in a temporary Git index
  context; the working tree index is untouched. `rtk git diff --check` checks tracked whitespace.
- PaperMono firmware build uses the unchanged local experiment E settings: 24 kHz, filtered IMA ADPCM,
  1024 samples × 16 buffers, 300-second chunks. No commit, push or upload.
- A target **compile-only** guard check uses the same S3 compiler and SDK headers with a temporary sdkconfig
  overlay selecting one core and disabling runtime stats. `SystemMetrics.cpp` compiles; `nm -u` contains no
  idle-counter reference. This is not a linked alternative firmware or a rebuilt framework. The real
  single-core device build and execution remain unverified.

### Outstanding device acceptance

All device checks above remain open: pinned loads on both cores; idle/reading/display and frequency changes;
more than 72 minutes including the expected unavailable wrap interval; partial sensor failures and external
power's 45-second lag; PCM/ADPCM recording across rollover/Pause/Resume/Stop/exit; and cross-core snapshot
stress. Also verify that a deliberately blocked main loop leaves unchanged timestamps/older data.
For cross-core stress, have a temporary task pinned opposite the main task repeatedly copy snapshots while
main sampling runs; assert coherent groups and nondecreasing completed timestamps within one boot.

Measure collection duration with 64-bit `esp_timer_get_time()` around due `update()` calls, retaining/reporting
maxima outside the measurement and publication lock. Compare otherwise identical enabled/disabled builds,
record stack high-water and recorder timing/occupancy, and listen/check WAV boundaries. This instrumentation
and firmware upload have not been run. No target overhead or battery-impact result is available.

The accepted diagnostic persistence limits remain: overflow can write with audio queued; no periodic filesystem
sync; short writes can leave partial CSV; finalization/last diagnostic cost and dropped-row totals are not fully
exported. They were not addressed by adding recovery machinery. Experiment session identifiers are still
first-unused-directory allocations; A–E mappings have not been verified on the device here.
