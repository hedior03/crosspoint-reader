# Audio lab

From this directory:

```sh
uv sync
uv run jupyter lab voice_filters.ipynb
```

This uses its own `.venv` and lockfile, separate from firmware tooling. No global kernel installation needed.
In the notebook, set `RECORDING` to a merged PCM16 or PaperMono IMA ADPCM WAV, or a session directory and run the cells in order.
ADPCM loading requires FFmpeg on PATH (`brew install ffmpeg` on macOS); WAV sample counts are checked
and declared final-block padding is trimmed. Originals are not rewritten. Filtered exports remain PCM.
The load cell keeps your decoded recording in memory. Plots reuse `scripts/audio/analyze_recording.py`.

The filter controls offer Butterworth, Chebyshev I/II and elliptic families with order 2, 4 or 6 per edge.
High-pass and low-pass can be enabled independently. Both enabled at order 4 means order 8 total (four biquads).
Change the family, order, cutoffs, gain and preview range, then click **Apply & compare**. Designed poles and
finite output are checked; stable filters can still ring or overshoot. Cutoff meanings differ by family;
read the notebook note and inspect the response curve when comparing bandwidths.
Full-recording filtering preserves state across blocks; plots/playback compare the chosen excerpt.
The original overview still shows the entire recording. Playback does not independently normalize each signal:
relative level differences are real. If needed, both players receive the same attenuation to avoid clipping.

This is causal filtering (`sosfilt`), suitable for experimenting toward a streaming implementation. It is not
forward/backward zero-phase processing. Float64 desktop results do not validate firmware timing or float32 behavior.
Pause gaps are omitted when joining session chunks. Existing gain/clipping in a recording cannot be undone.

Put private recordings in `data/` if desired. Generated exports go to ignored `outputs/` and are opt-in.
Clear notebook outputs before committing: embedded playback contains audio from your recording. The checked-in
notebook is output-free. No firmware settings are changed by this lab.

References: [SciPy filter design](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.butter.html),
[causal SOS filtering](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.sosfilt.html).

## Further reading

See the [audio experiments overview](../../docs/papermono-audio-experiments.md),
[Jupyter guide](../../docs/papermono-audio-jupyter.md),
[filtering notes](../../docs/papermono-audio-filters.md), and
[codec comparison](../../docs/papermono-audio-codecs.md).
