#pragma once
#include <Arduino.h>
#include "button_input.h"

enum class Mode : uint8_t { LIVE, PAX, PMR, EXPLORE, DEMO, CAL };

namespace ModeManager {
  void begin();
  Mode current();
  void handleButtonEvent(const ButtonEvent& ev); // NAV2 court = cycle, NAV2 long = bascule DEMO
  void handleEncoderDelta(int delta);             // délégué au mode actif (ex. EXPLORE)
  void tick();                                    // bascule auto-DEMO si g_demoForced
}
