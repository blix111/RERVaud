#include <Arduino.h>
#include "config.h"
#include "shared_state.h"
#include "button_input.h"
#include "mode_manager.h"
#include "led_driver.h"
#include "display_driver.h"
#include "calib_tool.h"

void taskCore0Network(void* param);
void taskCore1Ui(void* param);

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(PIN_BTN_NAV2, INPUT_PULLUP);
  if (digitalRead(PIN_BTN_NAV2) == LOW) {
    // BNav2 maintenu au boot : mode CAL, contourne les tâches FreeRTOS (cf. plan §3)
    CalibTool::run(); // bloquant, ne retourne jamais
  }

  SharedState::begin();
  LedDriver::begin();
  DisplayDriver::begin();
  ButtonInput::begin();
  ModeManager::begin();

  xTaskCreatePinnedToCore(taskCore0Network, "core0_network", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(taskCore1Ui, "core1_ui", 8192, nullptr, 1, nullptr, 1);
}

void loop() {
  // Tout le travail applicatif se fait dans task_core0_network / task_core1_ui.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
