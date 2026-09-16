"""Validate a copied WAV or complete session directory; listening is still required."""
import argparse
import json
from pathlib import Path
import wave
import struct
import subprocess
import tempfile


def wav_info(path):
    """Read recorder PCM/IMA framing without loading the audio into RAM."""
    with path.open("rb") as source:
        header = source.read(12)
        assert len(header) == 12 and header[:4] == b"RIFF" and header[8:] == b"WAVE"
        assert int.from_bytes(header[4:8], "little") + 8 == path.stat().st_size, "Truncated/trailing RIFF"
        chunks = {}
        while source.tell() < path.stat().st_size:
            chunk = source.read(8)
            assert len(chunk) == 8, "Truncated chunk header"
            name, size = struct.unpack("<4sI", chunk)
            assert source.tell() + size + (size & 1) <= path.stat().st_size, "Truncated chunk"
            assert name not in chunks, "Duplicate WAV chunk"
            chunks[name] = (source.tell(), size)
            source.seek(size + (size & 1), 1)
        offset, size = chunks[b"fmt "]
        assert size in (16, 20), "Unsupported recorder fmt"
        source.seek(offset)
        fmt = source.read(size)
        tag, channels, rate, byte_rate, align, bits = struct.unpack_from("<HHIIHH", fmt)
        assert channels == 1 and rate > 0 and align > 0
        data_offset, data_bytes = chunks[b"data"]
        assert data_bytes % align == 0, "Incomplete audio block"
        if tag == 1:
            assert (bits, align, byte_rate) == (16, 2, rate * 2)
            samples, padding = data_bytes // 2, 0
        else:
            assert tag == 0x11 and bits == 4 and size == 20, "Expected PCM16 or IMA ADPCM4"
            extension, per_block = struct.unpack_from("<HH", fmt, 16)
            assert (extension, align, per_block) == (2, 256, 505)
            assert byte_rate == rate * align // per_block
            offset, fact_size = chunks[b"fact"]
            assert fact_size == 4
            source.seek(offset)
            samples = struct.unpack("<I", source.read(4))[0]
            padding = data_bytes // align * per_block - samples
            assert 0 <= padding < per_block and samples > 0, "Invalid true sample count"
            for pos in range(data_offset, data_offset + data_bytes, align):
                source.seek(pos + 2)
                index, reserved = source.read(2)
                assert index <= 88 and reserved == 0, "Invalid IMA block header"
    return dict(samples=samples, rate=rate, padding=padding, data_bytes=data_bytes,
                data_offset=data_offset, fmt=fmt, format="pcm16" if tag == 1 else "ima_adpcm4")


def check_wav(path, expected_rate=None):
    info = wav_info(path)
    if expected_rate is not None:
        assert info["rate"] == expected_rate, "Sample rate differs from session metadata"
    print(f"{path.name}: {info['samples'] / info['rate']:.3f}s, {info['rate']} Hz, "
          f"{info['format']}, {info['padding']} padding samples")
    return info["samples"]


def merge_wav(chunks, output, rate):
    """Decode each chunk, discard declared codec padding, omit intentional pauses."""
    # Exclusive creation prevents overwriting the original recordings.
    with output.open("xb") as destination:
        try:
            with wave.open(destination, "wb") as merged:
                merged.setparams((1, 2, rate, 0, "NONE", "not compressed"))
                for path in chunks:
                    info = wav_info(path)
                    with tempfile.TemporaryFile() as pcm:
                        if info["format"] == "ima_adpcm4":
                            subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-f", "s16le",
                                            "-acodec", "pcm_s16le", "-"], stdout=pcm, check=True)
                            decoded_bytes = pcm.tell()
                            assert info["samples"] * 2 <= decoded_bytes <= (info["samples"] + info["padding"]) * 2
                            assert decoded_bytes % 2 == 0
                            pcm.seek(0)
                        else:
                            with path.open("rb") as source:
                                source.seek(info["data_offset"])
                                remaining = info["data_bytes"]
                                while remaining:
                                    data = source.read(min(65536, remaining))
                                    assert data, "Truncated PCM"
                                    pcm.write(data)
                                    remaining -= len(data)
                            pcm.seek(0)
                        remaining = info["samples"] * 2
                        while remaining:
                            data = pcm.read(min(65536, remaining))
                            assert data, "Decoder returned too few samples"
                            merged.writeframesraw(data)
                            remaining -= len(data)
            print(f"Merged PCM: {output} (intentional pauses omitted)")
        except BaseException:
            output.unlink(missing_ok=True)
            raise


def merge_adpcm(chunks, output):
    """Copy compatible complete IMA blocks unchanged into one WAV."""
    assert chunks, "No complete ADPCM blocks to merge"
    infos = [wav_info(path) for path in chunks]
    for info in infos:
        assert info["format"] == "ima_adpcm4", "Direct merge requires ADPCM"
        assert info["fmt"] == infos[0]["fmt"], "Incompatible ADPCM formats"
        assert info["padding"] == 0, "Padded recording: use --merge for decoded PCM export"
    samples = sum(info["samples"] for info in infos)
    size = sum(info["data_bytes"] for info in infos)
    assert samples <= 0xffffffff and size <= 0xffffffff - 52, "Merged WAV exceeds RIFF/fact limits"
    header = (b"RIFF" + struct.pack("<I", 52 + size) + b"WAVEfmt " + struct.pack("<I", 20)
              + infos[0]["fmt"] + b"fact" + struct.pack("<II", 4, samples)
              + b"data" + struct.pack("<I", size))
    with output.open("xb") as destination:
        try:
            destination.write(header)
            for path, info in zip(chunks, infos):
                with path.open("rb") as source:
                    source.seek(info["data_offset"])
                    remaining = info["data_bytes"]
                    while remaining:
                        data = source.read(min(65536, remaining))
                        assert data, "Truncated ADPCM data"
                        destination.write(data)
                        remaining -= len(data)
        except BaseException:
            output.unlink(missing_ok=True)
            raise
    print(f"Merged ADPCM: {output} ({samples} retained samples; intentional pauses omitted)")


def check_session(path):
    lines = (path / "session.jsonl").read_bytes().splitlines(keepends=True)
    assert lines and all(line.endswith(b"\n") for line in lines), "Interrupted journal entry; session incomplete"
    events = [json.loads(line) for line in lines]
    start = events[0]
    assert start["version"] in (1, 2, 3) and start["event"] == "start", "Unknown session format"
    assert (start["bits"], start["channels"]) == (16, 1)
    assert type(start["sample_rate"]) is int and start["sample_rate"] > 0
    if start["version"] >= 2:
        assert start["format"] in ("pcm16", "ima_adpcm4"), "Unknown recording codec"
        expected = (256, 505) if start["format"] == "ima_adpcm4" else (2, 1)
        assert (start["block_bytes"], start["samples_per_block"]) == expected
    total = discarded = 0
    chunks = []
    for index, event in enumerate(events[1:], 1):
        if event["event"] == "chunk":
            assert event["chunk"] == len(chunks) + 1, "Missing/reordered chunk"
            chunk = path / f"chunk-{event['chunk']:06d}.wav"
            count = check_wav(chunk, start["sample_rate"])
            assert count == event["samples"], "Chunk length differs from journal"
            info = wav_info(chunk)
            assert info["format"] == start.get("format", "pcm16"), "Format differs from journal"
            if start["version"] >= 2:
                assert event["encoded_bytes"] == info["data_bytes"]
                assert event["padding_samples"] == info["padding"]
                if start["version"] == 3:
                    assert info["padding"] == 0, "Version 3 must contain complete codec blocks"
            total += count
            assert event["end_sample"] == total, "Wrong chunk sample offset"
            chunks.append(chunk)
        else:
            assert event["sample"] == total, "Wrong event sample offset"
            if event["event"] == "discard":
                assert start["version"] == 3 and start["format"] == "ima_adpcm4"
                assert event["reason"] == "incomplete_tail"
                count = event["discarded_samples"]
                assert type(count) is int and 0 < count < start["samples_per_block"], "Invalid tail discard"
                assert index + 1 < len(events) and events[index + 1]["event"] in ("pause", "stop"), "Discard outside Pause/Stop"
                discarded += count
            if event["event"] == "resume":
                print(f"Intentional pause: {event['gap_ms'] / 1000:.3f}s")
    assert set(path.glob("*.wav")) == set(chunks), "Unjournaled WAV; session needs inspection"
    assert not list(path.glob("*.part")), "Partial audio remains; session incomplete"
    assert events[-1]["event"] == "stop", "Session did not finish successfully"
    print(f"Complete session: {len(chunks)} chunks, {total / start['sample_rate']:.3f}s of audio")

    if start["version"] == 3:
        print(f"Intentional tail discard: {discarded} samples total ({discarded / start['sample_rate']:.6f}s)")
    return chunks, start["sample_rate"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    exports = parser.add_mutually_exclusive_group()
    exports.add_argument("--merge", type=Path, help="Write decoded PCM WAV; requires FFmpeg for ADPCM")
    exports.add_argument("--merge-adpcm", type=Path, help="Copy padding-free ADPCM blocks without re-encoding")
    args = parser.parse_args()
    if args.path.is_dir():
        chunks, rate = check_session(args.path)
    else:
        check_wav(args.path)
        chunks, rate = [args.path], wav_info(args.path)["rate"]
    if args.merge:
        merge_wav(chunks, args.merge, rate)
    if args.merge_adpcm:
        merge_adpcm(chunks, args.merge_adpcm)
    print("Structure valid. Listen to a timed source across boundaries to verify continuity and quality.")
