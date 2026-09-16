# ADPCM-XQ

Vendored library sources from David Bryant's ADPCM-XQ v0.5, commit
`b4a825e8ed4e0aed0984759235b5e425b32b9f9a`.
Upstream: https://github.com/dbry/adpcm-xq/tree/b4a825e8ed4e0aed0984759235b5e425b32b9f9a
License: BSD-3-Clause; retain license.txt with distributions.

Local safety changes in adpcm-lib.c:
- Check context allocation before memset and optional dynamic-shaping allocation before use.
- Move the 89-entry initial-index error array from stack to the allocated encoder context.

Recorder settings: mono, four-bit Microsoft IMA WAV blocks, lookahead 0, noise shaping off.
The first block still performs an index search; measure stack headroom and timing on the device.
Full 505-sample blocks avoid upstream's fresh-context single-sample invalid step index. The recorder only passes complete blocks; incomplete final tails are journaled and discarded. No per-block allocation with shaping off.
Do not format vendor sources with the project style; keep upstream diffs reviewable.
