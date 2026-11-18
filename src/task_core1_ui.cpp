#include <Arduino.h>
#include "button_input.h"
#include "mode_manager.h"
#include "display_driver.h"

void taskCore1Ui(void* param) {
  (void)param;

  for (;;) {
    ButtonInput::poll();

    ButtonEvent ev;
    while (ButtonInput::popEvent(ev)) {
      ModeManager::handleButtonEvent(ev);
    }
    int delta = ButtonInput::takeEncoderDelta();
    if (delta != 0) ModeManager::handleEncoderDelta(delta);

    ModeManager::tick();

    // Phase 1 : affiche juste le nom du mode courant (cf. plan Phase 1, item 4).
    // Sera remplacé par le dispatch vers mode_live/mode_pax/... en Phase 3.
    switch (ModeManager::current()) {
      case Mode::LIVE:    DisplayDriver::showText("LIVE"); break;
      case Mode::PAX:     DisplayDriver::showText("PAX "); break;
      case Mode::PMR:     DisplayDriver::showText("PMR "); break;
      case Mode::EXPLORE: DisplayDriver::showText("EXPL"); break;
      case Mode::DEMO:    DisplayDriver::showText("DEMO"); break;
      case Mode::CAL:     DisplayDriver::showText("CAL "); break;
    }
    DisplayDriver::update();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
