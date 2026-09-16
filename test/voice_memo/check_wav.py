"""Inspect the completed voice memo: python check_wav.py path.wav."""
import sys
import wave

with wave.open(sys.argv[1], "rb") as recording:
    assert (recording.getnchannels(), recording.getsampwidth(), recording.getframerate()) == (1, 2, 16000)
    frames = recording.getnframes()
    assert frames <= 16000 * 300, "Recording exceeds five-minute cap"
    assert len(recording.readframes(frames + 1)) == frames * 2, "Truncated PCM data"
print(f"Valid {frames / 16000:.3f}-second mono PCM WAV. Listen to verify signal quality and continuity.")
