#pragma once
#include <Arduino.h>

namespace ModePmr {
  void begin();
  void render();
  void handleEncoderDelta(int delta);
}
