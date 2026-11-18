#pragma once
#include <Arduino.h>

namespace LedDriver {
  void begin();
  void setPixel(uint8_t bus, uint16_t index, uint8_t r, uint8_t g, uint8_t b);
  void clearBus(uint8_t bus);
  void clearAll();
  void show(uint8_t bus);
  void showAll();

  // Luminosité globale (0..100%)
  void setBrightness(uint8_t v);
  uint8_t getBrightness();
  void setBrightnessPercent(uint8_t percent);
  uint8_t getBrightnessPercent();
  void saveBrightness();
}
