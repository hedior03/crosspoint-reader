#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
c++ -DVOICE_MEMO_PCM_BASELINE -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined test/voice_memo/WavTest.cpp -o "$test_dir/wav"
"$test_dir/wav"
for source in adpcm-lib adpcm-dns; do
  cc -std=c99 -fsanitize=address,undefined -c "lib/AdpcmXq/$source.c" -o "$test_dir/$source.o"
done
cc -std=c99 -fsanitize=address,undefined test/voice_memo/AllocationTest.c \
  "$test_dir/adpcm-dns.o" -o "$test_dir/allocation"
"$test_dir/allocation"
for format in pcm adpcm; do
  flags=()
  if [[ "$format" == pcm ]]; then flags+=(-DVOICE_MEMO_PCM_BASELINE); fi
  c++ "${flags[@]}" -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Itest/voice_memo/stubs -Ilib/Memory -Ilib/AdpcmXq \
    test/voice_memo/SessionTest.cpp src/activities/tools/VoiceMemoSession.cpp \
    "$test_dir/adpcm-lib.o" "$test_dir/adpcm-dns.o" -o "$test_dir/session"
  "$test_dir/session" "$test_dir/$format"
  .venv/bin/python test/voice_memo/check_wav.py "$test_dir/$format" --merge "$test_dir/$format.wav"
  for recording in "$test_dir/$format"/short-*; do
    .venv/bin/python test/voice_memo/check_wav.py "$recording" --merge "$recording.wav"
  done
  .venv/bin/python test/voice_memo/check_desktop.py "$test_dir/$format" "$test_dir/$format.wav"
done
.venv/bin/python test/voice_memo/check_wav.py /tmp/voice-memo-test.wav
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -Itest/voice_memo/stubs -Ilib/hal \
  test/voice_memo/IndicatorTest.cpp lib/hal/HalMicrophone.cpp -o "$test_dir/indicator"
"$test_dir/indicator"
