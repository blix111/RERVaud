#include "mode_manager.h"
#include "shared_state.h"

namespace {
  Mode currentMode = Mode::LIVE;
  Mode modeBeforeDemo = Mode::LIVE;

  Mode nextInCycle(Mode m) {
    switch (m) {
      case Mode::LIVE:    return Mode::PAX;
      case Mode::PAX:     return Mode::PMR;
      case Mode::PMR:     return Mode::EXPLORE;
      case Mode::EXPLORE: return Mode::LIVE;
      default:            return Mode::LIVE; // DEMO/CAL hors du cycle de navigation
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

  void handleButtonEvent(const ButtonEvent& ev) {
    if (ev.button == ButtonId::NAV2) {
      if (ev.type == ButtonEventType::SHORT_PRESS) {
        if (currentMode != Mode::DEMO) currentMode = nextInCycle(currentMode);
      } else { // LONG_PRESS : bascule manuelle DEMO
        if (currentMode == Mode::DEMO) {
          currentMode = modeBeforeDemo;
        } else {
          modeBeforeDemo = currentMode;
          currentMode = Mode::DEMO;
        }
        g_demoForced.store(false); // bascule manuelle, distincte du fallback automatique réseau
      }
      g_currentMode.store((uint8_t)currentMode);
    }
    // ButtonId::ENCODER : délégué au mode actif (ex. arbre de navigation EXPLORE)
  }

  void handleEncoderDelta(int delta) {
    (void)delta; // délégué au mode actif — rien à faire au niveau machine à états
  }

  void tick() {
    if (g_demoForced.load() && currentMode != Mode::DEMO) {
      modeBeforeDemo = currentMode;
      currentMode = Mode::DEMO;
      g_currentMode.store((uint8_t)currentMode);
    }
  }
}
