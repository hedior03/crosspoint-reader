# PaperMono personal tools roadmap

Status: **proposal for discussion, not an implementation specification**. Reviewed 2026-09-15.
Agreed direction: one Tools corner on Home, with Calculator as its first tool. The first calculator increment
covers numeric input and clear, with decimal entry, backspace and sequential arithmetic as a follow-up, scoped in the [calculator input plan](papermono-calculator-input-plan.md).
Its list submenu and keyboard-based arrangement are implemented; the owner confirmed successful upload and
basic operation on PaperMono. The full device checklist remains open.
Later feature ordering remains proposed.

Development approach: [Building PaperMono tools](papermono-tools-development.md).

## Direction

Keep CrossPoint as a good reader and gradually add useful personal tools to this PaperMono-only fork.
The reader remains available, with its books, progress, input, and sleep behavior protected.
This fork deliberately permits tools and server connections excluded by upstream's [scope](../SCOPE.md)
and [roadmap](../ROADMAP.md). Those documents describe upstream policy, not a veto on this work.

- Ship one useful feature at a time. Add its entry point with the feature; an empty launcher is not a milestone.
- Reuse current activities, UITheme/FreeInkUI, input, storage, and Wi-Fi setup. Borrow small pieces from CrossPlay.
- Official M5Stack libraries are allowed where they help. Keep one owner of each peripheral and shared bus;
  do not run a second board-wide initialization alongside FreeInk's display, touch, SD, or power control.
- No plugin system, app framework, generalized job engine, or whole-architecture redesign now.
- New tools target PaperMono only. Do not remove other board code as a prerequisite.
- Measure internal heap, task stacks, and PSRAM separately on the device. PaperMono's PSRAM does not make
  DMA, radio allocations, or concurrent SD access free. No promised memory/performance gains without measurements.

## What already exists

Source review only: no firmware was built, flashed, or tested for this plan.
Snapshots: this fork `b5fb406f`, reference CrossPoint `6c83eddb`, CrossPlay `39c5ecb6`.
At the initial review the fork's `freeink-sdk` checkout was uninitialized. Its pinned commit `7f6bd0f` was inspected through
the reference CrossPoint SDK's Git objects; a newer reference checkout is not assumed to be the pinned SDK.
Local reference links below require the ignored `.reference-projects` symlinks.

| Reuse candidate | Evidence and limit |
| --- | --- |
| Existing activity navigation | [Activity.h](../src/activities/Activity.h), lines 31–64, provides lifecycle, child results, and return navigation. [HomeActivity.cpp](../src/activities/home/HomeActivity.cpp), lines 175–199, is an existing launch point. Add a small Tools entry with the first feature. |
| Touch UI and text entry | [UiAppHost.h](../src/components/UiAppHost.h), lines 10–29 and 34–60, already handles UI/input coordination. [KeyboardEntryActivity.h](../src/activities/util/KeyboardEntryActivity.h), lines 13–28, supplies editable text with a length limit. Reuse it for compose-and-send; no new keyboard. |
| Safe SD access | [HalStorage.cpp](../lib/hal/HalStorage.cpp), lines 213–240, locks reads/writes through `HalFile`. Reuse this for recording and pending uploads; serialization prevents races but does not prove sufficient recording throughput. |
| Wi-Fi selection | [WifiSelectionActivity.h](../src/activities/network/WifiSelectionActivity.h), lines 26–49, already covers connection and saved credentials. It is not an upload queue. |
| CrossPlay's app entry pattern | [Shelf.cpp](../.reference-projects/crossplay/src/apps_local/Shelf.cpp), lines 74–91, adds Apps/Games beside the reader; [SampleActivity.cpp](../.reference-projects/crossplay/src/apps_local/sample/SampleActivity.cpp), lines 9–43, demonstrates a small activity. Borrow the integration idea, not its full shelf, game UI, or hardcoded strings. Keep this fork's `tr()` conventions. |
| CrossPlay's durable submission intent | [InstapaperActivity.cpp](../.reference-projects/crossplay/src/apps_local/instapaper/InstapaperActivity.cpp), lines 734–743, saves pending actions on SD until acknowledged. This is a useful retry pattern, not a ready-made audio uploader. Its exit path, lines 83–93, can disconnect Wi-Fi and reboot: do not transplant that into background work. |
| CrossPlay's server boundary | [BridgeHttp.h](../.reference-projects/crossplay/src/apps_local/bridge/BridgeHttp.h), lines 49–62, exposes buffered requests and streamed downloads. Learn from its TLS/error handling; it does not provide streamed audio uploads as-is. Keep service-specific roots, accounts, and endpoints out of this fork. |
| SDK microphone candidate | At pinned SDK `7f6bd0f`: `libs/hardware/Microphone/include/Microphone.h`, lines 23–41, provides mono PCM capture. `BoardConfig/include/BoardConfig.h`, lines 1001–1004, assigns PaperMono CLK 45 / DATA 46 but explicitly leaves expander power to board bring-up. `Microphone.cpp`, lines 15–20, only toggles a normal GPIO. This needs a focused power/capture experiment, not a claim that recording already works. |
| BLE starting point | At pinned SDK `7f6bd0f`: `libs/network/BleKeyboardHost/include/BleKeyboardHost.h`, lines 3–13, implements a **central receiving keyboard input**, one peripheral at a time. Our HID tools need PaperMono to **send input to a host**. [platformio.ini](../platformio.ini), lines 99–124, does not include this SDK library and ignores Arduino `BLE`; choose a compatible peripheral stack during the BLE step. |
| Sleep boundaries | [ActivityManager.cpp](../src/activities/ActivityManager.cpp), line 336, checks only the current activity. [main.cpp](../src/main.cpp), lines 637–642, resets the idle timer; lines 258–296 and 720–728 still allow explicit sleep, tearing down activity/Wi-Fi/storage. Background work needs deliberate ownership and stop behavior when that feature arrives. |

The quick source/catalog scan found no ready calculator, voice recorder, outgoing HID utility, or personal
calendar/task dashboard in these firmware trees. CrossPlay's Connections calendar is a puzzle selector,
not a calendar integration. The reference CrossPoint is useful for SDK/source comparison, not a tools bundle.

Vendor references: [PaperMono hardware](https://docs.m5stack.com/en/core/PaperMono) lists ESP32-S3R8,
16 MB flash, 8 MB PSRAM, and a PDM microphone. The official [microphone example](https://docs.m5stack.com/en/arduino/papermono/mic)
uses M5Unified's `Mic_Class` and M5IOE1 power control. It is a reference for a focused experiment;
its full `M5.begin()`/display setup is not an integration recipe for this reader.

## Practical order — complete idea inventory

These are independent tracks, not mandatory phases. Calculator is a low-risk starting recommendation,
not a dependency of recording or BLE. The first recorder stands alone even if server work never happens.

| When | Idea / useful outcome | Dependency or uncertainty |
| --- | --- | --- |
| First recommendation | Simple calculator | Existing UI/navigation only. |
| Early | Voice memo: start/stop, save one local file | PaperMono mic power/capture, SD writes, and safe exit experiment. |
| Early alternative | BLE multimedia control, one host | Pairing, HID reports, disconnect/reconnect, and sleep experiment. |
| Small BLE follow-up | Keyboard: compose text, then explicitly send | Working one-host BLE; host keyboard layout/character coverage. |
| Next BLE option | Mouse jiggler, visible start/stop and adjustable interval | Mouse HID report support; prove start/stop/disconnect behavior before background use. |
| After one-file capture | Meeting recording in consecutive chunks without lost time | Continuous capture across file boundaries; measure gaps under SD stalls and screen updates. |
| After reliable local capture | Upload a selected closed recording to a server | Agree one endpoint/authentication and acknowledgment; stream from SD, retain the original on failure. Can be tested before chunking. |
| When uploads are useful | Persistent upload queue, progressing in the background within our Tools area | Working single-file upload; retries/duplicate handling, navigation ownership, sleep and radio/SD coexistence. Reader-wide background work is a separate later decision. |
| Later BLE | Remember multiple devices and select the active target | Establish single-host correctness first; saved pairings are distinct from live connections. |
| Hardware-gated BLE | Keep several devices connected; route HID to exactly one selected device | Explicit multi-host experiment: connection limits, per-host report routing, switching, battery and Wi-Fi coexistence. Do not silently replace this goal with reconnect-only switching. |
| Later / exploratory | Responsive mouse or touchpad, possibly gestures | Input latency and collision with existing Back/Home gestures; keep only if useful on real hardware. |
| Later voice pipeline | Server transcription; optional transcript typed over Bluetooth | Reliable uploads; compose/review/send path and host character support for BLE output. |
| Later meeting pipeline | Server transcription with speaker annotations (diarization) | Good continuous recordings; evaluate actual speaker separation before promising accuracy. |
| Later processing | AI cleanup of transcripts, then agentic passes; possible Hermes handoff | Useful raw transcription first; server-side processing, retaining original audio/raw transcript. |
| Later personalization | Proactively build a user transcription dictionary | Actual corrections and vocabulary needs; scope review/update behavior when this becomes useful. |
| Independent later track | Portrait “what I'm working on” view: current work, calendar events, to-do list | First agree content/source. Start read-only with a cached snapshot; defer editing, provider sync, and refresh scheduling. |

Audio progression: **one file → continuous chunks → transcription → AI cleanup → agentic/Hermes processing
→ personal dictionary**. Upload/queue work supports that progression; it need not wait for every recording feature.
No on-device transcription or AI runtime is a prerequisite.

## Next few baby steps

### 1. Basic calculator, with its own entry and a way back

- **Does:** Home → Tools → Calculator; decimal entry, sequential arithmetic, `C`, `CE`, backspace,
  and independent key feedback while input continues through screen refreshes.
- **Out of scope:** expression parsing/precedence, history, persistence, and custom key design.
- **Design source:** existing CrossPoint/FreeInkUI keyboard keys. Discuss composition before implementation.
- **PaperMono test:** digit entry, clear, length bound, rapid/held taps, navigation, sleep/wake, and reader return.
- Detailed scope and source references: [calculator input plan](papermono-calculator-input-plan.md).

### 2. Single-file voice memo

Detailed baby steps and hardware findings: [voice memo plan](papermono-voice-memo-plan.md).

- **Does:** start/stop capture to a new WAV on SD, show recording/saved/error state and the filename.
  Default candidate: 16 kHz, 16-bit mono; 32,000 bytes/second, about 1.92 MB/minute before the small header.
  Stream through a bounded reusable buffer; do not hold the recording in RAM. Keep recordings outside disposable
  reader caches. Stop/finalize before leaving the tool or entering explicit sleep; prevent idle sleep during capture.
- **Prerequisite experiment within this step:** verify mic power, correct PDM channel/samples, and start/end/restart
  through existing hardware ownership. Try the pinned SDK path first; official M5Stack components remain an option.
  If a buffer/task allocation is needed, justify its measured size and internal-memory/PSRAM placement at implementation.
- **Out of scope:** recording while reading, pause, playback on-device, meeting chunks, upload, transcription, queue.
- **PaperMono test:** record speech plus silence for 30 seconds, then several minutes; copy to a computer, listen,
  and compare WAV sample duration with elapsed time. Repeat start/stop, Back/Home, idle timeout, explicit sleep,
  wake, and return to the reader. Test missing/full SD and short-write handling: never report a failed save as saved.
  Forced power loss may leave the current file incomplete; preserve it as such and keep earlier recordings intact.
  Log heap/stack across repeated sessions and check mic power is released afterward.

### 3. One-host BLE media remote

- **Does:** pair PaperMono with one computer/phone; show connection state; send play/pause and volume commands
  while this tool is open. Include a way to disconnect/forget it. Each command must release its HID report.
- **Prerequisite experiment within this step:** select a compatible outgoing HID implementation, pair to the
  user's actual host, and measure the radio's heap/lifecycle cost. Confirm cleanup on tool exit and sleep.
- **Out of scope:** multiple hosts, permanent background connection, text, mouse, jiggler, simultaneous Wi-Fi.
- **PaperMono test:** issue individual and repeated commands; disconnect the host mid-command; reconnect;
  sleep/wake; leave/re-enter and check no stuck input or accumulating heap loss. Verify book input still works.
  Agree the first host OS before implementation; do not claim universal compatibility from one host.

### 4. Compose and send a short text message

- **Does:** reuse the existing keyboard, preview text, and explicitly send it to the connected host, with cancel.
  Start with one declared keyboard layout and a small length bound; visibly reject unsupported characters.
- **Depends on:** step 3's connection/report lifecycle, not its media buttons.
- **Out of scope:** live per-tap typing, arbitrary Unicode input, macros, transcription, automatic Enter/submission.
- **PaperMono test:** send letters, digits, shifted punctuation, and spaces into a scratch document; compare exactly.
  Test unsupported characters, cancel, and disconnect halfway through. Show partial/failed transmission rather
  than automatically resending the whole text. Verify every key/modifier is released and navigation still works.

For each implementation: build the relevant target once after the final edit (`pio run -e papermono`,
through the configured RTK wrapper), add only the smallest useful logic check, then run its device checklist.
Host tests/build success do not substitute for PaperMono validation. Capture a reader baseline before the first change.

## Decisions to make only when needed

- **Now:** finish PaperMono checks for the calculator's independent key expiry and reader return.
  Basic operation and responsive input were confirmed by the owner; later voice/BLE ordering is open.
- **Before uploading:** choose the server/auth and define what acknowledgment means. Keep files until confirmed;
  handle the “server saved it but the reply was lost” case without duplicate submissions or silent deletion.
- **Before background queue work:** define the Tools-area lifetime, explicit stop/pause behavior, reboot recovery,
  and USB Drive exclusion. Begin with one upload at a time and durable pending/failed state; only introduce shared
  machinery when a second real submission feature needs it. Do not assume deep sleep can keep uploads running.
- **Before meeting chunks:** use a timed audio source and inspect boundaries for missing/duplicated samples,
  including under UI/SD load. Choose chunk length after measuring, not as architecture now.
- **Before multi-host or pointer control:** settle target OS/device combinations and run the relevant experiment.
- **Before the portrait view:** clarify what “working on” means and where calendar/tasks come from.

Record layout agreement before implementation. Scope each subsequent feature when
the previous useful increment has been tried on PaperMono; keep the rest as ideas rather than promises.
