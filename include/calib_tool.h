#pragma once
#include <Arduino.h>

namespace CalibTool {
  void begin();
  void loop();
  void run(); // wrapper bloquant: begin() puis while(true) loop();
}
