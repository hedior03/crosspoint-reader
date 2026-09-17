# Voice filtering with biquads

Part of [PaperMono audio experiments](papermono-audio-experiments.md).
Use the [Jupyter lab](papermono-audio-jupyter.md) to evaluate settings before proposing a firmware change.

## Purpose and limitations

The immediate question is whether gentle filtering makes actual PaperMono recordings more useful for
listening and transcription. It is not a request to implement a general audio effects engine.

Start with three comparisons: unchanged audio, high-pass only, and high-pass plus low-pass. A high-pass can
reduce DC and low-frequency rumble. A low-pass can reduce high-frequency noise, but also removes speech detail.
Avoid choosing a narrow telephone-style passband merely because the intended content is voice.

Handling noise and wind are not confined to one frequency band. Rubbing can overlap speech, and a filter
cannot reconstruct speech lost through microphone/ADC overload or digital clipping. Gain amplifies signal
and noise together; it is not itself a sensitivity or signal-to-noise improvement.

## What the notebook implements

The notebook offers Butterworth, Chebyshev I, Chebyshev II and elliptic designs, all through the same
independent high-pass/low-pass controls. Each edge can be order 2, 4 or 6. Both enabled doubles the total:
order 4 per edge means order 8 total, implemented as four biquads. This is a cascade of HP and LP sections,
not a special family-specific bandpass mode. There is no separate Q control.

The defaults retain Butterworth and fourth order per edge. Chebyshev/elliptic parameters are 0.5 dB passband
ripple and 40 dB stopband attenuation where applicable. They can be edited above the design function.
Butterworth's cutoff is its −3 dB point; Chebyshev I and elliptic use the passband ripple boundary; Chebyshev II
uses the stopband attenuation boundary. Identical Hz settings therefore do not imply equal usable bandwidth.

Poles outside or on the unit circle and non-finite coefficients/output are rejected. Tests exercise all
families, orders and enabled-edge combinations, plus extreme cutoffs. Stability does not rule out audible
ringing, overshoot, loss of speech detail or differences at embedded precision. Original audio is unchanged.

A biquad can be written, with `a0 = 1`, as:

```text
y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
```

The coefficients determine the response; `z^-4` on its own represents four samples of delay. Different
implementation forms store different small sets of state variables. SciPy's SOS representation contains
`[b0, b1, b2, a0, a1, a2]` for each section. Check coefficient normalization and feedback signs before using
these arrays in another library.

The notebook uses `iirfilter(..., ftype=family, output="sos", fs=sample_rate)` and causal `sosfilt`. It does not use
forward/backward `sosfiltfilt`, which needs future data and changes the response. Causal filtering introduces
frequency-dependent phase/delay, as a streaming device implementation would.

Sources: [SciPy Butterworth design](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.butter.html),
[SOS filtering](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.sosfilt.html).

## Tuning with useful samples

Record close speech, distant speech, a quiet room, and handling/air noise. Note microphone placement and
firmware gain. Use both quiet and loud speakers if available. Compare identical excerpts and retain originals.

The notebook starts with high-pass at 100 Hz and low-pass disabled. These are trial controls, not calibrated
recommendations for the microphone. Raise/lower the high-pass while listening for reduced rumble versus thinner
voices. Enable low-pass only if it removes a real nuisance without harming consonants or distant speech.

Apply extra gain after filtering and assess clipping. A recording already captured with gain/clipping cannot
be used to demonstrate that a new filter would have prevented upstream overload. For firmware evaluation,
filter before additional gain and the final signed-16-bit saturation step.

Compare transcription too when available: pleasant sound does not guarantee fewer word errors. Preserve enough
speaker information if later diarization is important.

## CPU and memory expectations

A small biquad cascade needs only coefficients and a few state values per section, not a large audio history.
Its working state can remain fixed while it processes existing audio blocks. An extra task and queue are not
necessary simply because this is another processing stage.

Espressif publishes an S3 biquad benchmark of roughly 17,500 cycles per 1,024 input samples. Applying two such
sections at 16 kHz gives an illustrative estimate:

```text
17,500 cycles / 1,024 samples × 16,000 samples/sec × 2 sections
≈ 547,000 cycles/sec ≈ 0.23% of one 240 MHz core
```

This is arithmetic based on a vendor kernel benchmark, not measured CPU utilization in this recorder. It
excludes conversion, memory access, scheduling and other code. The actual CPU frequency and chosen function
matter. Desktop notebook execution is not an embedded benchmark.
[ESP-DSP benchmark table](https://documentation.espressif.com/esp-dsp/en/latest/esp32/esp-dsp-library.html).

ESP-DSP is a possible implementation source; do not assume it is already integrated or bring in a whole audio
framework for two sections. Check existing dependencies and the precise API when implementation is scoped.

## State, boundaries and numerical checks

- Preserve state across consecutive blocks and continuous file rollovers. Resetting at every block introduces
  repeated startup transients and would invalidate the notebook's continuous result.
- Decide reset behavior separately for a new session and for Pause/Resume. The notebook joins chunks without
  gaps; it does not yet reproduce an explicit firmware pause-reset policy.
- Coefficients depend on sample rate. A sample-rate change requires redesign, not just a changed WAV header.
- Desktop float64 results must be checked against the selected embedded precision, especially for low cutoffs.
  Verify stability, finite values, silence, loud transients and saturation behavior.
- Measure processing duration and buffer headroom on PaperMono before claiming negligible impact on recording.

Filtering can make a signal more predictable for lossless coding, but compression improvement is an experiment,
not a filter-design objective that should override speech quality. See [codec notes](papermono-audio-codecs.md).

## Implemented owner-selected trial

The owner-supplied coefficients are preserved in [voice-ellip-150-4500.json](audio-presets/voice-ellip-150-4500.json):
16 kHz, elliptic high-pass 150 Hz plus low-pass 4500 Hz, fourth order per edge (four SOS sections total),
0.5 dB ripple and 40 dB stopband attenuation. Coefficients are used in the supplied order and rounded to float32.
They are not redesigned from the descriptive cutoff settings at runtime.

`VoiceMemoFilter.h` uses transposed direct form II, the standard structure used by SciPy `sosfilt`. Each section
requires five multiplications, four additions/subtractions and two state floats per input sample. Four sections
therefore need 32 bytes of state; 24 float coefficients occupy 96 bytes of read-only data. There is no allocation,
FFT, convolution window, float block buffer or new task. One sample passes through all four sections before
conversion back to int16; no intermediate int16 clipping occurs.

ESP-DSP was investigated: its `dsps_biquad_f32_aes3` is optimized S3 assembly using ordinary direct form II,
with float input/output buffers and two state floats. It is not already a project dependency. The small transposed
implementation matches the desktop algorithm and processes existing int16 queue blocks without another float
buffer or dependency. Investigate the optimized kernel only if device measurements show a need.
[ESP-DSP C reference](https://github.com/espressif/esp-dsp/blob/master/modules/iir/biquad/dsps_biquad_f32_ansi.c),
[S3 implementation](https://github.com/espressif/esp-dsp/blob/master/modules/iir/biquad/dsps_biquad_f32_aes3.S).

Processing order is **raw PCM capture → existing queue → filter → rounding/saturation → ADPCM or PCM → SD**.
The writer owns filter state, keeps it across batches and chunk rollovers, and starts from zero on each Record
or Resume. No filter state spans an intentional pause. A start transient is expected; there is no added window,
padding or delay trimming. Existing ADPCM final-tail discard is unchanged.

`VoiceMemoConfig.h` sets `FILTER_ENABLED`. The supplied SOS coefficients are applied unchanged, with no
separate gain multiplier. Output is rounded and saturated once to signed 16-bit PCM. Bypassing the filter
preserves the captured PCM values. No automatic normalization or AGC is applied.
The compiled preset rejects a different sample rate while enabled; redesign coefficients before changing it.

The version-3 start journal additionally records a filter identifier and `gain_db`, with `gain` retained as the
linear multiplier (1.0, with `gain_db` 0: no separate gain). A non-finite filter output stops with a processing error and preserves the current partial
recording. Output peak/full-scale counters now belong to the writer. Logs include `max_filter_us` per input
batch alongside existing encode, full-batch, buffer and stack diagnostics; capture performs no DSP or SD work.

### Verification

`bash test/voice_memo/run_host_tests.sh` runs the real float32 kernel with sanitizers and compares it with SciPy
using the saved JSON coefficients. The comparison's pinned SciPy dependency is supplied by `uv run --script`;
no notebook edits or notebook environment are required. Tests cover silence, impulses, full-scale DC, alternating
extremes, deterministic noise, decay, segmented input, fresh-state Resume and rounding/saturation.

On the host, maximum filter error against SciPy float64 was **0.487622 PCM units**, RMS **0.074197**, for the test
signal. Float32 poles remain inside the unit circle. S3 GCC 14.2 at `-Os` emits `mul.s`/`madd.s`/`msub.s` hardware
instructions for the kernel, with no helper calls; a loop probe has a 32-byte stack frame. These are numerical and
compiler checks, not firmware CPU or battery measurements. S3 rounding can differ from the host.

On PaperMono, compare bypass/filter with a repeatable speech/handling-noise source. Listen for
lost consonants, ringing, start/resume transients and loud-input clipping. Record across rollover, Pause/Resume,
Stop and exit, then verify the session and merge as before. Inspect processing/batch maxima, buffer high-water,
DMA overflow, writer stack and heap; keep comfortable margin within each 1024-sample batch's 64 ms of audio.
Timing on the real device remains open. Filtering still does not reduce fixed-rate ADPCM file size or guarantee
better transcription, and it is not a prerequisite for future uploads.
