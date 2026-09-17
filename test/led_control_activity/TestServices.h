#pragma once

#include <HalLed.h>

#include <functional>
#include <string>
#include <vector>

namespace test {
inline int lockDepth = 0;
inline bool rendering = false;
inline bool supported = true;
inline bool beginOk = true;
inline bool setOk = true;
inline bool endOk = true;
inline int beginCalls = 0;
inline int endCalls = 0;
inline std::vector<HalLed::Color> colors;
inline std::vector<std::string> events;
inline std::vector<std::string> errors;
inline std::function<void()> beforeLock;

void reset();
void logError(const char* format, ...);
}  // namespace test
