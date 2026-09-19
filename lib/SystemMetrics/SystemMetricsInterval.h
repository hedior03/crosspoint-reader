#pragma once

#include <cstdint>

namespace SystemMetricsInterval {
// uint32_t cumulative idle counters wrap independently of the runtime clock.
inline bool busy(uint32_t previousIdle, uint32_t idle, uint64_t elapsedUs, uint64_t skewUs, uint16_t& basisPoints) {
  if (elapsedUs == 0 || elapsedUs >= (uint64_t{1} << 32)) return false;
  const uint64_t delta = uint32_t(idle - previousIdle);
  if (delta > elapsedUs + skewUs) return false;
  // Only the explicitly bounded read/tick skew may round an idle interval to zero busy.
  basisPoints = delta >= elapsedUs ? 0 : ((elapsedUs - delta) * 10000 / elapsedUs);
  return true;
}
}  // namespace SystemMetricsInterval
