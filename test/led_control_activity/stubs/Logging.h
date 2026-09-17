#pragma once
#include "TestServices.h"

#define LOG_ERR(module, ...) test::logError(__VA_ARGS__)
#define LOG_DBG(...) ((void)0)
