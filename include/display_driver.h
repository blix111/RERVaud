#pragma once
#include <Arduino.h>

namespace DisplayDriver {
  void begin();
  void showText(const char* text4);
  void showScrolling(const char* fullText, uint32_t stepIntervalMs);
  void update();
}
