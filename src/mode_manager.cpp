#include "mode_manager.h"
#include "shared_state.h"
#include "led_driver.h"
#include "display_driver.h"

namespace {
  uint8_t brightPage = 0; // 0 = LEDs, 1 = afficheur 14 segments
  Mode currentMode = Mode::LIVE;
  Mode modeBeforeBrightness = Mode::LIVE;

  Mode nextInCycle(Mode m) {
    switch (m) {
      case Mode::LIVE:        return Mode::EXPLORE;
      case Mode::LIVE_TRAINS: return Mode::EXPLORE;
      case Mode::EXPLORE:     return Mode::PAX;
      case Mode::PAX:         return Mode::PMR;
      case Mode::PMR:         return Mode::LIVE;
      default:                return Mode::LIVE;
    }
  }
}

namespace ModeManager {
  void begin() {
    currentMode = Mode::LIVE;
    g_currentMode.store((uint8_t)currentMode);
  }

  Mode current() {
    return currentMode;
  }

  uint8_t brightnessPage() {
    return brightPage;
  }

  void handleButtonEvent(const ButtonEvent& ev) {
    // Deuxième mode LIVE (trains en temps réel sur leur LED rail/gare) :
    // accessible uniquement en maintenant le bouton + tournant l'encodeur.
    if (ev.type == ButtonEventType::HOLD_ROTATE) {
      if (currentMode == Mode::BRIGHTNESS) {
        LedDriver::saveBrightness();
        DisplayDriver::saveBrightness();
      }
      if (currentMode != Mode::LIVE_TRAINS) {
        currentMode = Mode::LIVE_TRAINS;
      } else {
        currentMode = Mode::LIVE;
      }
      g_currentMode.store((uint8_t)currentMode);
      return;
    }

    // Menu caché luminosité : maintien (clic encodeur, ou encodeur + Nav)
    if (ev.button == ButtonId::BOTH && ev.type == ButtonEventType::HOLD_3S) {
      if (currentMode != Mode::BRIGHTNESS) {
        modeBeforeBrightness = currentMode;
        currentMode = Mode::BRIGHTNESS;
        brightPage = 0;
      } else {
        currentMode = modeBeforeBrightness;
        LedDriver::saveBrightness();
        DisplayDriver::saveBrightness();
      }
      g_currentMode.store((uint8_t)currentMode);
      return;
    }

    // Menu luminosité : clic court = page suivante (LEDs -> afficheur), puis valide et quitte
    if (currentMode == Mode::BRIGHTNESS) {
      if (ev.type == ButtonEventType::SHORT_PRESS) {
        if (brightPage == 0) {
          brightPage = 1;
        } else {
          currentMode = modeBeforeBrightness;
          LedDriver::saveBrightness();
          DisplayDriver::saveBrightness();
          g_currentMode.store((uint8_t)currentMode);
        }
      }
      return;
    }

    if (ev.button == ButtonId::NAV2 && ev.type == ButtonEventType::SHORT_PRESS) {
      currentMode = nextInCycle(currentMode);
      g_currentMode.store((uint8_t)currentMode);
    }
    // ButtonId::ENCODER : délégué au mode actif (ex. arbre de navigation EXPLORE)
  }

  void handleEncoderDelta(int delta) {
    (void)delta;
  }

  void tick() {}
}
