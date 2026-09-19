#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
for mode in supported unsupported; do
  flags=()
  if [[ "$mode" == unsupported ]]; then flags+=(-DMETRICS_UNSUPPORTED); fi
  c++ "${flags[@]}" -std=c++20 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
    -Itest/system_metrics/stubs -Ilib/SystemMetrics -Ilib/hal \
    test/system_metrics/SystemMetricsTest.cpp lib/SystemMetrics/SystemMetrics.cpp -o "$test_dir/$mode"
  "$test_dir/$mode"
done
