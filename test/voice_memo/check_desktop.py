"""Exercise checker rejection and exact decoded merge lengths on host-generated sessions."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import wave

from check_wav import wav_info, merge_wav, merge_adpcm, check_session
import subprocess
import shutil

session, merged = map(Path, sys.argv[1:])
events = [json.loads(line) for line in (session / "session.jsonl").read_text().splitlines()]
with wave.open(str(merged)) as audio:
    assert audio.getnframes() == sum(e["samples"] for e in events if e["event"] == "chunk")
for directory in filter(Path.is_dir, session.glob("short-*")):
    with wave.open(str(directory) + ".wav") as audio:
        count = int(directory.name.split("-")[1])
        assert audio.getnframes() == (count // 505 * 505 if events[0]["format"] == "ima_adpcm4" else count)
source = session / "chunk-000001.wav"
original = source.read_bytes()
try:
    merge_wav([source], source, 16000)
except FileExistsError:
    pass
else:
    raise AssertionError("Allowed source overwrite")
assert source.read_bytes() == original
with tempfile.TemporaryDirectory() as directory:
    broken = Path(directory) / "bad.wav"
    variants = [original[:-1], original + b"extra"]
    if wav_info(source)["format"] == "ima_adpcm4":
        invalid = bytearray(original)
        struct.pack_into("<I", invalid, 48, 0)
        variants.append(invalid)
        invalid = bytearray(original)
        invalid[62] = 89
        variants.append(invalid)
    for data in variants:
        broken.write_bytes(data)
        try:
            wav_info(broken)
        except AssertionError:
            pass
        else:
            raise AssertionError("Accepted malformed WAV")
print("Desktop checks passed: exact merged counts, malformed WAVs rejected, originals preserved.")

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    chunks = sorted(session.glob("chunk-*.wav"))
    if events[0]["format"] == "ima_adpcm4":
        assert sum(e["discarded_samples"] for e in events if e["event"] == "discard") == 18
        output = root / "merged-adpcm.wav"
        merge_adpcm(chunks, output)
        info = wav_info(output)
        assert info["padding"] == 0
        expected = b"".join(p.read_bytes()[wav_info(p)["data_offset"]:] for p in chunks)
        assert output.read_bytes()[info["data_offset"]:] == expected
        decoded = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(output), "-f", "s16le", "-"])
        with wave.open(str(merged)) as audio:
            assert decoded == audio.readframes(audio.getnframes())
        assert len(decoded) == info["samples"] * 2
        tiny, rate = check_session(session / "tiny-pause")
        merge_adpcm(tiny, root / "tiny.wav")

        # Older version-2 files retain true counts but have internal codec padding.
        old = root / "old"
        old.mkdir()
        old_events = [dict(events[0], version=2)]
        total = 0
        for i, path in enumerate(chunks[:2], 1):
            data = bytearray(path.read_bytes())
            count = wav_info(path)["samples"] - 1
            struct.pack_into("<I", data, 48, count)
            (old / path.name).write_bytes(data)
            total += count
            old_events.append(dict(event="chunk", chunk=i, samples=count, end_sample=total,
                                   encoded_bytes=len(data)-60, padding_samples=1))
        old_events.append(dict(event="stop", sample=total, gap_ms=0))
        (old / "session.jsonl").write_text("".join(json.dumps(e)+"\n" for e in old_events))
        old_chunks, rate = check_session(old)
        try:
            merge_adpcm(old_chunks, root / "unsafe.wav")
        except AssertionError as error:
            assert "Padded" in str(error)
        else:
            raise AssertionError("Concatenated internal padding")
        assert not (root / "unsafe.wav").exists()
        merge_wav(old_chunks, root / "old-pcm.wav", rate)
        with wave.open(str(root / "old-pcm.wav")) as audio:
            assert audio.getnframes() == total

        # Invalid discard counts and incompatible sample rates must fail before export.
        old_events[0]["version"] = 3
        bad = dict(event="discard", sample=total, discarded_samples=505, reason="incomplete_tail")
        bad_session = root / "bad-session"
        bad_session.mkdir()
        (bad_session / "session.jsonl").write_text("\n".join(json.dumps(e) for e in
            [events[0], dict(bad, sample=0), dict(event="stop", sample=0)]) + "\n")
        try:
            check_session(bad_session)
        except AssertionError:
            pass
        else:
            raise AssertionError("Accepted whole-block discard")
        different = root / "different.wav"
        data = bytearray(chunks[0].read_bytes())
        struct.pack_into("<II", data, 24, 8000, 8000 * 256 // 505)
        different.write_bytes(data)
        try:
            merge_adpcm([chunks[0], different], root / "incompatible.wav")
        except AssertionError:
            pass
        else:
            raise AssertionError("Merged incompatible rates")
    else:
        # Existing version-1 PCM journals remain readable.
        old = root / "pcm-v1"
        old.mkdir()
        for chunk in chunks:
            shutil.copyfile(chunk, old / chunk.name)
        old_events = [dict(events[0], version=1)] + events[1:]
        (old / "session.jsonl").write_text("".join(json.dumps(e)+"\n" for e in old_events))
        check_session(old)
print("Block-aligned merge checks passed: identical decoded audio, legacy padding handled, discard metadata checked.")
