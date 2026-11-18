#include <Arduino.h>
#include "calib_tool.h"

void setup() {
  Serial.begin(115200);
  delay(1500);
  CalibTool::begin();
}

void loop() {
  CalibTool::loop();
}
