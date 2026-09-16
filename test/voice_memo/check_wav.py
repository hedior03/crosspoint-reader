"""Validate a copied WAV or complete session directory; listening is still required."""
import argparse
import json
from pathlib import Path
import wave


def check_wav(path, expected_rate=None):
    with wave.open(str(path), "rb") as recording:
        assert recording.getnchannels() == 1 and recording.getsampwidth() == 2, "Expected mono 16-bit PCM"
        rate = recording.getframerate()
        if expected_rate is not None:
            assert rate == expected_rate, "Sample rate differs from session metadata"
        frames = recording.getnframes()
        remaining = frames
        while remaining:
            data = recording.readframes(min(remaining, 16384))
            assert data and len(data) % 2 == 0, "Truncated PCM data"
            remaining -= len(data) // 2
        assert not recording.readframes(1), "Unexpected trailing PCM"
    assert path.stat().st_size == 44 + frames * 2, "Unexpected size for recorder WAV"
    print(f"{path.name}: {frames / rate:.3f}s, {rate} Hz")
    return frames


def check_session(path):
    lines = (path / "session.jsonl").read_bytes().splitlines(keepends=True)
    assert lines and all(line.endswith(b"\n") for line in lines), "Interrupted journal entry; session incomplete"
    events = [json.loads(line) for line in lines]
    start = events[0]
    assert start["version"] == 1 and start["event"] == "start", "Unknown session format"
    assert (start["bits"], start["channels"]) == (16, 1)
    total = 0
    chunks = []
    for event in events[1:]:
        if event["event"] == "chunk":
            assert event["chunk"] == len(chunks) + 1, "Missing/reordered chunk"
            chunk = path / f"chunk-{event['chunk']:06d}.wav"
            count = check_wav(chunk, start["sample_rate"])
            assert count == event["samples"], "Chunk length differs from journal"
            total += count
            assert event["end_sample"] == total, "Wrong chunk sample offset"
            chunks.append(chunk)
        else:
            assert event["sample"] == total, "Wrong event sample offset"
            if event["event"] == "resume":
                print(f"Intentional pause: {event['gap_ms'] / 1000:.3f}s")
    assert set(path.glob("*.wav")) == set(chunks), "Unjournaled WAV; session needs inspection"
    assert not list(path.glob("*.part")), "Partial audio remains; session incomplete"
    assert events[-1]["event"] == "stop", "Session did not finish successfully"
    print(f"Complete session: {len(chunks)} chunks, {total / start['sample_rate']:.3f}s of audio")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    if args.path.is_dir():
        check_session(args.path)
    else:
        check_wav(args.path)
    print("Structure valid. Listen to a timed source across boundaries to verify continuity and quality.")
