# PaperMono audio experiments

Status: updated 2026-09-16. Block-aligned ADPCM WAV is implemented and committed (`f9d4d6d0`);
PCM remains the comparison fallback. Filtering, uploads and Opus remain separate future work.
See the [current roadmap shortlist](papermono-roadmap.md#current-next-step-shortlist) for proposed ordering.

The goal is a useful, reliable personal voice recorder with a possible Plaud-like workflow: record locally,
upload later, then transcribe and process on a server. Preserve the reader and existing peripheral ownership.
Follow [Building PaperMono tools](papermono-tools-development.md); the
[voice memo plan](papermono-voice-memo-plan.md) describes recording implementation and device checks.

## Read by topic

- [Jupyter audio lab](papermono-audio-jupyter.md): launch, load recordings, interpret plots, listen and export.
- [Voice filtering](papermono-audio-filters.md): biquads, tuning, numerical behavior and CPU estimates.
- [Codecs and compression](papermono-audio-codecs.md): PCM, ADPCM, FLAC, Opus, containers and compatibility.

## What we know

The owner tested a session with four chunks: 2.816, 300, 300 and 15.552 seconds, totaling **618.368 seconds**
of audio, plus an intentional 2.918-second pause. The session checker accepted its structure. The owner then
merged the chunks on the computer and reported excellent results. This is useful functional evidence, not a
measurement of worst-case SD latency, battery life or every sample boundary under load.

The desktop lab has been created and executed with an earlier short recording. It generates spectra,
spectrograms and original/filtered audio players. Numerical checks cover filter response and state continuity
across blocks. The ten-minute recording has not been independently analyzed in this side conversation.

Capture remains 16 kHz mono signed 16-bit PCM with 2x saturating gain. Storage defaults to ADPCM WAV.
The checker supports legacy padded files and the current padding-free format; direct ADPCM merge and decoded
PCM export are implemented. See the recorder plan for tail discard and journal semantics.

## How processing and compression fit together

The current path is capture → gain/saturation → bounded PCM queue → ADPCM encoding → SD.
No filter is active. If filtering proves useful, it should precede additional gain/clipping; its ownership
and any movement of gain must be decided in that feature, rather than simply filtering already-clipped PCM.
A conceptual future data path is:

**Microphone capture → bounded audio buffer → filter → gain → encode or retain PCM → SD**

Keep capture independent of slow storage. A small filter can process an existing block before writing or
encoding it; it does not automatically need another worker, queue, or audio framework. The existing two-task
recorder is a starting point, not a commitment to place every future operation in one particular task.

Filtering controls which signal components are retained. Gain controls amplitude. A codec controls how that
signal is represented in a file. These solve different problems:

| Change | Potential benefit | What it does not guarantee |
| --- | --- | --- |
| Gentle high-pass | Less DC/low rumble | Removal of all handling noise or recovery of clipped speech |
| Optional low-pass | Less high-frequency noise | Better transcription or smaller fixed-rate files |
| Gain | More audible quiet material | Improved signal-to-noise ratio |
| ADPCM | Predictable, inexpensive size reduction | Exact preservation of the original samples |
| FLAC | Exact preservation of its input | Large savings on noisy microphone recordings |
| Opus | Compact speech recordings/uploads | Negligible CPU cost or transparent integration |

Filtering may make FLAC's input more predictable, but savings must be measured. FLAC following a filter is
lossless only relative to the filtered signal: the complete process has discarded information from the original.
Fixed-rate ADPCM or constant-bitrate Opus does not become materially smaller simply because the audio is smoother.
Do not plan around useful gzip savings after an audio codec.

## A small experimental progression

1. **Keep reference recordings.** Include close speech, distant speech, room noise, handling and air movement.
   Record the distance, environment and existing firmware gain. Do not overwrite the originals.
2. **Compare filtering on the PC.** Compare bypass and the proposed 4 kHz / 6 kHz low-pass at 16 kHz sampling.
   A gentle high-pass is a separate option for low rumble, not an agreed prerequisite.
   Listen at consistent levels and inspect the same excerpts and color scales.
3. **Compare codecs using the same input.** Measure file sizes and listen to decoded output. If transcription
   is available, compare word errors and speaker handling too. Do not confuse a codec comparison with a filter change.
4. **Check intended playback.** Test the actual files in macOS applications and the future server decoder.
5. **Benchmark the preferred candidate on PaperMono.** Measure processing time, buffer high-water, internal
   heap, stack, file-transition latency and continuity while recording. Measure power if battery claims matter.

A useful trial record contains source name, sample rate, filter settings, gain, codec/version/settings, file
size, listening observations and the relevant device measurements. The notebook exports filter settings when
requested; it does not yet perform codec comparisons.

## Decisions deliberately left open

- Filter cutoffs and whether low-pass is worthwhile for this microphone and environment.
- Whether optional Opus savings justify a later device experiment; ADPCM-XQ and IMA WAV are already selected.
- Whether any later upload-time compression is worthwhile. Current ADPCM encoding runs in the existing writer.
  Post-processing adds reads/writes, latency and energy; no gzip benefit has been measured.
- Raw-reference retention, upload protocol and server-side processing.

Prefer the next measured improvement over designing the entire eventual recorder pipeline now.
