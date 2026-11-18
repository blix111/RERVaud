#pragma once
#include <Arduino.h>

namespace ModePax {
  void begin();
  void render();
  void handleEncoderDelta(int delta);
}
