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

## Next proposed comparison

No firmware filter is enabled. Compare bypass, a 4 kHz low-pass and a 6 kHz low-pass at 16 kHz sampling on the
same representative speech. These are candidates, not selected cutoffs. Listen before adopting a filter;
smoother input may reduce ADPCM prediction error but will not materially change its fixed-rate file size.
Filtering is not a prerequisite for session uploads. Preserve raw references and keep additional gain/clipping
after filtering if a later firmware experiment is justified.
