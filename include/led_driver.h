#pragma once
#include <Arduino.h>

namespace LedDriver {
  void begin();
  void setPixel(uint8_t bus, uint16_t index, uint8_t r, uint8_t g, uint8_t b);
  void clearBus(uint8_t bus);
  void clearAll();
  void show(uint8_t bus);
  void showAll();

  // Plafond de luminosite par canal, ajustable a l'execution (defaut: LED_BRIGHTNESS).
  // N'affecte que les futurs setPixel() ; pas persiste (RAM uniquement).
  void setBrightness(uint8_t v);
  uint8_t getBrightness();
}
