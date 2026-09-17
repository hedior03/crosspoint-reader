#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../../src/activities/tools/VoiceMemoFilter.h"
#include "../../src/activities/tools/VoiceMemoPcm.h"

int main(int argc, char** argv) {
  assert(argc == 3);
  static_assert(sizeof(VoiceMemoFilter) == 32);
  assert(voiceMemoToPcm(1000) == 1000);
  assert(voiceMemoToPcm(-1000) == -1000);
  assert(voiceMemoToPcm(1e10f) == 32767);
  assert(voiceMemoToPcm(-1e10f) == -32768);
  VoiceMemoFilter filter;
  for (unsigned i = 0; i < 16000; ++i) assert(filter.process(0) == 0);
  auto* inputFile = std::fopen(argv[1], "wb");
  auto* outputFile = std::fopen(argv[2], "wb");
  assert(inputFile && outputFile);
  uint32_t random = 123;
  std::vector<float> input(160000), output(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    random = random * 1664525u + 1013904223u;
    input[i] = i == 0       ? 32767
               : i < 16000  ? 0
               : i < 32000  ? 32767
               : i < 48000  ? -32768
               : i < 64000  ? (i % 2 ? 32767 : -32768)
               : i < 128000 ? int(random >> 16) - 32768
                            : 0;
    output[i] = filter.process(input[i]);
    assert(std::isfinite(output[i]));
  }
  // Same filter lifetime across queue-sized blocks, codec blocks and file boundaries.
  VoiceMemoFilter segmented;
  size_t offset = 0;
  for (size_t block : {1u, 504u, 505u, 506u, 1024u, 4093u, 153367u}) {
    for (size_t end = std::min(input.size(), offset + block); offset < end; ++offset)
      assert(segmented.process(input[offset]) == output[offset]);
  }
  assert(offset == input.size());
  // A new writer on Resume starts from rest, independently of the previous tail.
  VoiceMemoFilter resumed;
  for (size_t i = 0; i < 1024; ++i) assert(resumed.process(input[i]) == output[i]);
  for (size_t i = input.size() - 1000; i < input.size(); ++i) assert(std::abs(output[i]) < 0.01f);
  assert(std::fwrite(input.data(), sizeof(float), input.size(), inputFile) == input.size());
  assert(std::fwrite(output.data(), sizeof(float), output.size(), outputFile) == output.size());
  assert(std::fclose(inputFile) == 0 && std::fclose(outputFile) == 0);
}
