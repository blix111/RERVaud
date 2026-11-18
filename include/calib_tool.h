#pragma once
#include <Arduino.h>

namespace CalibTool {
  void begin();
  void loop();
  void run(); // wrapper bloquant: begin() puis while(true) loop();

  // Service non-bloquant utilisable pendant que l'application normale tourne
  // (permet de se connecter en USB ou par IP à tout moment sans devoir rebooter avec BNav2 maintenu)
  void beginBackground();
  void pollBackground();
}
