#pragma once
#include <cstdint>
#include <mutex>
#ifndef METRICS_UNSUPPORTED
#define CONFIG_IDF_TARGET_ESP32S3 1
#define configNUMBER_OF_CORES 2
#define configGENERATE_RUN_TIME_STATS 1
#else
#define configNUMBER_OF_CORES 1
#define configGENERATE_RUN_TIME_STATS 0
#endif
#define INCLUDE_xTaskGetIdleTaskHandle 1
#define CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER 1
#define CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U32 1
#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configUSE_TICKLESS_IDLE 0
#define configTICK_RATE_HZ 1000
#define configRUN_TIME_COUNTER_TYPE uint32_t
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED \
  {                                  \
  }
#define portENTER_CRITICAL(mux) (mux)->lock()
#define portEXIT_CRITICAL(mux) (mux)->unlock()
