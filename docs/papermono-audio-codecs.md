# Codecs, compression and audio formats

Part of [PaperMono audio experiments](papermono-audio-experiments.md).
Status: ADPCM-XQ / block-aligned IMA WAV implemented in `f9d4d6d0`; PCM fallback retained.
FLAC and Opus remain comparisons, not firmware integrations. See the [recorder plan](papermono-voice-memo-plan.md).

## Baseline and candidate sizes

For 16 kHz mono signed 16-bit PCM, the payload rate is 32,000 bytes/sec, or 115.2 MB/hour (decimal MB).

| Representation | Approximate payload per hour | Main trade-off |
| --- | --- | --- |
| 16-bit PCM | 115.2 MB | Simple, exact samples, minimal processing |
| 4-bit IMA ADPCM | About 29.2 MB including recorder block framing | Roughly fourfold reduction; lossy and lightweight |
| FLAC | Signal-dependent | Lossless relative to input; variable compression and encoding cost |
| Opus at 24 kbps | 10.8 MB plus container overhead | Compact speech; more processing and integration |

These are format arithmetic or qualitative expectations. A ten-second microphone reference was also encoded
on the desktop: PCM 320,044 bytes, FLAC level 0 134,718 bytes, Opus 24 kbps CBR in Ogg 30,994 bytes. Extrapolating
that short sample gives about 48.5 MB/hour FLAC and 11.2 MB/hour Opus, including repeated short-file overhead;
FLAC varies with content. These are not device CPU/power measurements. Decoded audio must also be listened to.

At current settings, approximate five-minute sizes are PCM 9.60 MB, ADPCM 2.43 MB, FLAC 4.04 MB (sample-derived),
and Opus 0.92 MB. For 16 GB entirely available, approximate capacities are 139 / 548 / 330 / 1,455 hours respectively;
filesystem overhead and other files reduce these estimates. ADPCM is the accepted storage compromise for now.

## IMA ADPCM

ADPCM encodes prediction differences with adaptive quantization. The common 4-bit IMA form uses simple
operations and small state, making it the leading candidate when low CPU cost matters more than maximal
compression. It is lossy; four coded bits per sample do not mean it behaves like crude four-bit linear PCM.

Espressif's published esp_audio_codec **v2.0.0** benchmark reports 2.69% encoding CPU for ADPCM at 48 kHz stereo
on ESP32-S3R8. This supports investigating it for 16 kHz mono, but does not provide a measured CPU percentage,
stack requirement, binary size or battery benefit for this fork. Its listed heap figures exclude stack.
That benchmark uses a different encoder from our pinned ADPCM-XQ and must not be attributed to this firmware.
[Espressif encoder support and benchmark](https://components.espressif.com/components/espressif/esp_audio_codec/versions/2.0.0?language=en).

ADPCM can be stored in WAV, but PCM and ADPCM WAV headers/block rules differ. Our checker and merger now explicitly support IMA framing. Version-3 recordings contain only complete blocks;
Pause/Stop tails are discarded and journaled. They can merge without re-encoding. Older padded chunks must use
decoded PCM export so internal padding is trimmed.

## FLAC

FLAC preserves the exact samples supplied to it. Encoding level 0 is its fastest preset; higher levels spend
more effort looking for smaller representations without changing decoded quality. Encoding and decoding
performance are different questions: a library offering FLAC playback does not establish recording support.

Microphone noise can reduce compression substantially. Do not promise a ratio before encoding representative
recordings. The codec component version examined lists FLAC decoding, not a FLAC encoder; a practical encoder
integration must be verified independently.

If audio is filtered before FLAC, the FLAC file exactly preserves the filtered signal. It does not restore the
information removed by filtering. Retain raw reference samples during experiments.

Sources: [FLAC encoder settings](https://www.xiph.org/flac/api/group__flac__stream__encoder.html),
[FLAC FAQ and compression variability](https://www.xiph.org/flac/faq.html).

## Opus and MP3

Opus is a serious candidate for a speech recorder with eventual uploads. Espressif supplies an Opus encoder
with adjustable complexity; its v2.0.0 S3 benchmark lists 24.9% CPU at 48 kHz stereo, 90 kbps, complexity 0.
Those settings differ from a prospective 16 kHz mono voice configuration, so the figure is context, not a
prediction. Measure the actual configuration and include stack/buffering costs.

MP3 was mentioned as potentially expensive, but no MP3 encoding benchmark or integration investigation was
performed here. Do not turn that initial assumption into a claim that the S3 cannot encode MP3. Opus is the
more relevant speech candidate to compare first; no codec work is currently implemented in the notebook.

## Containers and macOS playback

WAV and Ogg are containers. PCM, ADPCM, Vorbis and Opus describe coding schemes; an extension alone does not
establish decoder compatibility. ADPCM also has multiple variants with different framing conventions.

A read-only `afconvert -hf` check on the owner's Mac listed these WAV data formats:

- `ms\x00\x11`: IMA ADPCM WAV, format tag 0x0011.
- `ms\x00\x02`: Microsoft ADPCM WAV, format tag 0x0002.

Apple IMA4 and IMA ADPCM WAV are not interchangeable packet formats. Despite that listing, native `afinfo`
and `afconvert` rejected the tested ADPCM files, including an FFmpeg control. FFmpeg decoding works; decoded
PCM is the compatibility path. VLC and QuickTime playback remain unverified.

## Why not just 10–12-bit PCM?

Bit depth controls amplitude quantization; sample rate controls temporal sampling and representable frequency
range. Reducing either is a quality decision as well as a size decision.

In conventional WAV PCM, 10 or 12 meaningful bits commonly occupy a 16-bit container, so reducing precision
alone saves no bytes. Custom packing would save 25% at 12 bits or 37.5% at 10 bits relative to 16-bit PCM,
while adding packing/decoding and compatibility work. Quantization noise becomes more significant, particularly
for quiet/distant speech. [WAV container and valid-bit rules](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html).

The preferred direction is to keep 16-bit capture and compare established codecs, rather than introduce a
custom packed PCM format for modest savings. This is a recommendation, not a claim that 12-bit voice is unusable.

## Filtering, gzip and energy

Filtering may improve lossless compression by removing unpredictable noise. It does not materially reduce
fixed-rate ADPCM size or a constant-bitrate Opus target. It may instead improve quality at that fixed rate.
Gzip after an audio codec is not expected to provide worthwhile extra savings; no measurements justify adding it.

Lower SD/network traffic may save energy, but encoding uses CPU and sometimes extra memory or storage passes.
Net battery impact must be measured. A codec benchmark alone cannot establish it. More RAM buffering can cover
finite processing/storage stalls, but cannot fix sustained processing slower than incoming audio.

## Next comparison

Encode the same raw and optionally filtered samples on the PC, decode them, listen at comparable levels, and
record size and settings. Test macOS playback and transcription if available. Then benchmark the preferred
encoder on PaperMono while watching capture continuity, buffer occupancy, stack and internal heap. For any alternative codec, verify its
library and framing at that point; no generic codec framework is needed to make this comparison.

## Upload packaging and battery decisions

Tar packages files without compression. A tar stream could be generated during upload without storing another
archive on SD, but no archive is currently needed or implemented. Uploading original chunks plus JSONL remains
the proposed first step. Unlike older padded recordings, new block-aligned ADPCM could also be merged while
uploading; that option is not an implemented uploader or a requirement for the first upload.

ZIP/gzip after ADPCM is an unmeasured experiment, not a promised saving. Transcoding ADPCM to Opus would add
another lossy generation and encoding work; direct Opus recording is a different future trade-off.

No reliable mA or runtime estimate exists per codec. PCM minimizes encoding but writes more; ADPCM trades small
encoding work for less SD traffic; Opus trades more processing for even less traffic. None establishes the battery
winner. Compare whole-device power under matched rate, gain, radio, frontlight and recording duration. CPU benchmark
percentages cannot be converted directly to battery hours.
