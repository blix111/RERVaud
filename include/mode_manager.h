#pragma once
#include <Arduino.h>
#include "button_input.h"

enum class Mode : uint8_t { LIVE, LIVE_TRAINS, PAX, PMR, EXPLORE, CAL, BRIGHTNESS };

namespace ModeManager {
  void begin();
  Mode current();
  uint8_t brightnessPage();                       // menu luminosité : 0 = LEDs, 1 = afficheur
  void handleButtonEvent(const ButtonEvent& ev);  // NAV2 court = cycle, maintien+rotation = LIVE_TRAINS
  void handleEncoderDelta(int delta);             // délégué au mode actif
  void tick();
}
