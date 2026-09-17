# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2,<3", "matplotlib>=3.9,<4"]
# ///
"""Plot a whole-file FFT and spectrogram of PCM or PaperMono IMA ADPCM WAV recordings.

uv run scripts/audio/analyze_recording.py path.wav --output-dir .tmp/audio-analysis
A session directory is also accepted; numbered chunks are joined without pauses.
"""
from pathlib import Path
import argparse
import json
import wave
import subprocess
import importlib.util

import matplotlib
if __name__ == "__main__":
    matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_pcm(file):
    try:
        recording = wave.open(str(file), "rb")
    except wave.Error:
        # Reuse the recorder's framing/count checks, including legacy final-block padding.
        checker_path = Path(__file__).resolve().parents[2] / "test/voice_memo/check_wav.py"
        spec = importlib.util.spec_from_file_location("memo_wav", checker_path)
        checker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(checker)
        info = checker.wav_info(file)
        if info["format"] != "ima_adpcm4":
            raise ValueError(f"Expected PCM16 or PaperMono IMA ADPCM WAV: {file}")
        try:
            data = subprocess.check_output(
                ["ffmpeg", "-v", "error", "-i", str(file), "-f", "s16le", "-acodec", "pcm_s16le", "-"],
                stderr=subprocess.PIPE)
        except FileNotFoundError as error:
            raise RuntimeError("ADPCM loading requires FFmpeg on PATH (macOS: brew install ffmpeg). Restart the kernel after installing.") from error
        except subprocess.CalledProcessError as error:
            raise ValueError(f"FFmpeg could not decode {file}: {error.stderr.decode(errors='replace')}") from error
        frames = info["samples"]
        if len(data) % 2 or not frames * 2 <= len(data) <= (frames + info["padding"]) * 2:
            raise ValueError(f"Decoded sample count differs from WAV metadata: {file}")
        return data[:frames * 2], info["rate"], 1, frames
    with recording as wav:
        if wav.getsampwidth() != 2 or wav.getcomptype() != "NONE":
            raise ValueError(f"Expected 16-bit PCM: {file}")
        frames = wav.getnframes()
        data = wav.readframes(frames)
        if len(data) != frames * wav.getnchannels() * 2:
            raise ValueError(f"Truncated WAV: {file}")
        return data, wav.getframerate(), wav.getnchannels(), frames


def load_audio(path):
    files = sorted(path.glob("chunk-*.wav")) if path.is_dir() else [path]
    if not files:
        raise ValueError("No chunk WAVs found")
    audio, rate = [], None
    boundaries = []
    count = 0
    for file in files:
        data, file_rate, channels, frames = read_pcm(file)
        if rate is not None and rate != file_rate:
            raise ValueError("Chunk sample rates differ")
        rate = file_rate
        samples = np.frombuffer(data, dtype="<i2").astype(np.float64) / 32768
        samples = samples.reshape(-1, channels).mean(axis=1)
        if count:
            boundaries.append(count / rate)
        audio.append(samples)
        count += frames
    samples = np.concatenate(audio)
    if len(samples) < 4:
        raise ValueError("Recording is too short")
    return samples, rate, boundaries, len(files)


def spectrum(samples, rate):
    window = np.hanning(len(samples))
    amplitude = np.abs(np.fft.rfft((samples - samples.mean()) * window)) * 2 / window.sum()
    amplitude[0] /= 2
    if len(samples) % 2 == 0:
        amplitude[-1] /= 2
    return np.fft.rfftfreq(len(samples), 1 / rate), 20 * np.log10(np.maximum(amplitude, 1e-8))


def spectrogram(samples, rate, window_ms, hop_ms, columns=1800):
    length = max(4, round(rate * window_ms / 1000))
    hop = max(1, round(rate * hop_ms / 1000))
    if hop > length:
        raise ValueError("Hop must not exceed the window length")
    if len(samples) < length:
        samples = np.pad(samples, (0, length - len(samples)))
    # Pad the tail so the last samples participate in a frame.
    frames = 1 + int(np.ceil((len(samples) - length) / hop))
    samples = np.pad(samples, (0, (frames - 1) * hop + length - len(samples)))
    window = np.hanning(length)
    group = max(1, int(np.ceil(frames / columns)))
    bins = int(np.ceil(frames / group))
    power = np.zeros((length // 2 + 1, bins))
    counts = np.zeros(bins)
    views = np.lib.stride_tricks.sliding_window_view(samples, length)[::hop]
    for first in range(0, frames, 128):
        block = views[first:first + 128]
        block = (block - block.mean(axis=1, keepdims=True)) * window
        psd = np.abs(np.fft.rfft(block, axis=1)) ** 2 / (rate * np.sum(window ** 2))
        psd[:, 1:-1 if length % 2 == 0 else None] *= 2
        for offset, row in enumerate(psd):
            index = (first + offset) // group
            power[:, index] += row
            counts[index] += 1
    power /= counts
    # Cell boundaries halfway between adjacent groups of frame centres.
    starts = np.arange(bins) * group
    ends = np.minimum(starts + group, frames)
    centres = ((starts + ends - 1) / 2 * hop + length / 2) / rate
    times = np.r_[0, (centres[:-1] + centres[1:]) / 2, len(samples) / rate]
    frequencies = np.fft.rfftfreq(length, 1 / rate)
    return frequencies, times, 10 * np.log10(np.maximum(power, 1e-16)), length, hop, group


def self_test():
    rate = 16000
    t = np.arange(rate * 2) / rate
    samples = 0.5 * np.sin(2 * np.pi * 1000 * t)
    freq, db = spectrum(samples, rate)
    assert abs(freq[np.argmax(db)] - 1000) < 1
    assert abs(db.max() - 20 * np.log10(0.5)) < 0.02
    samples[rate:] = 0.5 * np.sin(2 * np.pi * 3000 * t[rate:])
    freq, _, db, *_ = spectrogram(samples, rate, 64, 16)
    assert abs(freq[np.argmax(db[:, 10])] - 1000) < 20
    assert abs(freq[np.argmax(db[:, -10])] - 3000) < 20
    assert np.isfinite(spectrogram(np.zeros(100), rate, 64, 16)[2]).all()
    import struct
    import tempfile
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "recording.wav"
        with wave.open(str(path), "wb") as wav:
            wav.setparams((1, 2, rate, 0, "NONE", "not compressed"))
            wav.writeframes(np.rint(samples * 32768).astype("<i2").tobytes())
        loaded, loaded_rate, boundaries, files = load_audio(path)
        assert loaded_rate == rate and len(loaded) == len(samples) and not boundaries and files == 1
        # Two independent silent IMA blocks; also exercise legacy one-sample end padding.
        for frames in (1010, 1009):
            header = (b"RIFF" + struct.pack("<I", 52 + 512) + b"WAVEfmt " + struct.pack("<I", 20)
                      + struct.pack("<HHIIHHHH", 0x11, 1, rate, rate * 256 // 505, 256, 4, 2, 505)
                      + b"fact" + struct.pack("<II", 4, frames) + b"data" + struct.pack("<I", 512))
            original = header + bytes(512)
            path.write_bytes(original)
            loaded, loaded_rate, boundaries, files = load_audio(path)
            assert len(loaded) == frames and loaded_rate == rate and not loaded.any()
            assert files == 1 and not boundaries and path.read_bytes() == original
        path.write_bytes(original[:-1])
        try:
            load_audio(path)
        except (AssertionError, ValueError):
            pass
        else:
            raise AssertionError("Truncated ADPCM accepted")
    print("Self-test passed: spectra, PCM/ADPCM loading, exact counts, legacy padding and source preservation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, nargs="?")
    parser.add_argument("--output-dir", type=Path, default=Path(".tmp/audio-analysis"))
    parser.add_argument("--window-ms", type=float, default=64)
    parser.add_argument("--hop-ms", type=float, default=16)
    parser.add_argument("--max-hz", type=float)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if args.input is None:
        parser.error("Provide a WAV or session directory")
    if args.window_ms <= 0 or args.hop_ms <= 0:
        parser.error("Window and hop must be positive")
    samples, rate, boundaries, files = load_audio(args.input)
    max_hz = args.max_hz if args.max_hz is not None else rate / 2
    if not 0 < max_hz <= rate / 2:
        parser.error("max-hz must be between 0 and half the sample rate")
    output = args.output_dir
    output.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"figure.facecolor": "#fafafa", "axes.facecolor": "#fafafa", "font.size": 11})
    freq, db = spectrum(samples, rate)
    keep = freq <= max_hz
    freq, db = freq[keep], db[keep]
    # Retain peak amplitudes when reducing millions of FFT bins for plotting.
    step = max(1, int(np.ceil(len(freq) / 12000)))
    starts = np.arange(0, len(freq), step)
    plotted_db = np.maximum.reduceat(db, starts)
    fig, ax = plt.subplots(figsize=(13, 5), layout="constrained")
    ax.plot(freq[starts], plotted_db, color="#177e89", linewidth=0.65)
    ax.set(xlabel="Frequency (Hz)", ylabel="Amplitude (dBFS)", xlim=(0, max_hz), ylim=(-140, 0),
           title=f"Whole-recording FFT · {args.input.name}")
    ax.grid(alpha=0.2)
    fig.text(0.01, 0.01, "DC removed · Hann window · 0 dBFS = full-scale sine · display retains peak FFT bins", fontsize=9)
    fig.savefig(output / "spectrum.png", dpi=160)
    plt.close(fig)
    freq, times, db, length, hop, group = spectrogram(samples, rate, args.window_ms, args.hop_ms)
    edges = np.r_[0, (freq[:-1] + freq[1:]) / 2, rate / 2]
    fig, ax = plt.subplots(figsize=(13, 6), layout="constrained")
    mesh = ax.pcolormesh(times / 60, edges, db, shading="flat", cmap="magma", vmin=-110, vmax=-30, rasterized=True)
    for boundary in boundaries:
        ax.axvline(boundary / 60, color="cyan", linewidth=0.6, alpha=0.7)
    ax.set(xlabel="Recorded time (minutes; pauses omitted)", ylabel="Frequency (Hz)", ylim=(0, max_hz),
           xlim=(0, len(samples) / rate / 60), title=f"Spectrogram · {args.input.name}")
    fig.colorbar(mesh, ax=ax, label="Power spectral density (dBFS/Hz)")
    fig.text(0.01, 0.01, f"Hann {length / rate * 1000:g} ms · hop {hop / rate * 1000:g} ms · "
             f"display averages {group} frames per column · cyan = file boundaries", fontsize=9)
    fig.savefig(output / "spectrogram.png", dpi=160)
    plt.close(fig)
    summary = {"source": str(args.input.resolve()), "files": files, "sample_rate": rate,
               "duration_seconds": len(samples) / rate, "dc_offset": float(samples.mean()),
               "peak_dbfs": float(20 * np.log10(max(np.max(np.abs(samples)), 1e-8))),
               "rms_dbfs": float(20 * np.log10(max(np.sqrt(np.mean(samples ** 2)), 1e-8))),
               "chunk_boundaries_seconds": boundaries, "window_samples": length, "hop_samples": hop}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    print(f"Plots saved in {output.resolve()}")


if __name__ == "__main__":
    main()
