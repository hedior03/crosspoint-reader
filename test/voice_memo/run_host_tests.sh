#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined test/voice_memo/WavTest.cpp -o "$test_dir/wav"
"$test_dir/wav"
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -Itest/voice_memo/stubs \
  test/voice_memo/SessionTest.cpp src/activities/tools/VoiceMemoSession.cpp -o "$test_dir/session"
"$test_dir/session" "$test_dir/recording"
.venv/bin/python test/voice_memo/check_wav.py "$test_dir/recording"
.venv/bin/python test/voice_memo/check_wav.py /tmp/voice-memo-test.wav
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -Itest/voice_memo/stubs -Ilib/hal \
  test/voice_memo/IndicatorTest.cpp lib/hal/HalMicrophone.cpp -o "$test_dir/indicator"
"$test_dir/indicator"
