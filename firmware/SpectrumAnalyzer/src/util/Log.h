// Minimal serial logging: timestamp, level and tag in front of every line.
//
//   LOG_INFO("leds", "frame took %u us", micros);
//   ->  [   12.345] I leds: frame took 4600 us
#pragma once

#include <Arduino.h>

#define SA_LOG(level, tag, fmt, ...)                                                    \
  Serial.printf("[%5lu.%03lu] %c %s: " fmt "\n", (unsigned long)(millis() / 1000),     \
                (unsigned long)(millis() % 1000), level, tag, ##__VA_ARGS__)

#define LOG_INFO(tag, fmt, ...) SA_LOG('I', tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...) SA_LOG('W', tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) SA_LOG('E', tag, fmt, ##__VA_ARGS__)
