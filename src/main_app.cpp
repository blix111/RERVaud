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
  Serial.setTxTimeoutMs(0); // Évite tout blocage USB CDC natif ESP32-S3 si DTR non actif
  delay(1500);

  pinMode(PIN_BTN_NAV2, INPUT_PULLUP);
  if (digitalRead(PIN_BTN_NAV2) == LOW) {
    // BNav2 maintenu au boot : mode CAL dédié, contourne les tâches FreeRTOS
    CalibTool::run(); // bloquant, ne retourne jamais
  }

  SharedState::begin();
  LedDriver::begin();
  DisplayDriver::begin();
  ButtonInput::begin();
  ModeManager::begin();
  CalibTool::beginBackground();

  xTaskCreatePinnedToCore(taskCore0Network, "core0_network", 16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(taskCore1Ui, "core1_ui", 8192, nullptr, 1, nullptr, 1);
}

void loop() {
  // Écoute en permanence les commandes USB WebSerial du Studio (couleurs, settings, remap, test)
  // même sans avoir démarré avec BNav2 enfoncé.
  CalibTool::pollBackground();
  vTaskDelay(pdMS_TO_TICKS(20));
}
