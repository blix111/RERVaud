#pragma once
#include <Arduino.h>

namespace DisplayDriver {
  void begin();
  void showText(const char* text4);
  void showScrolling(const char* fullText, uint32_t stepIntervalMs);
  void update();
  void setBrightness(uint8_t level0to15);
  uint8_t getBrightness();      // niveau matériel 0..15
  void saveBrightness();        // sauvegarde en NVS
  void setBrightnessPercent(uint8_t percent);
}
