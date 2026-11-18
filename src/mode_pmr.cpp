#include "mode_pmr.h"
#include "led_driver.h"
#include "display_driver.h"
#include "stations.h"

namespace {
  int8_t selectedGareIdx = -1; // -1 = vue globale "PMR "
  uint32_t lastSelectedTime = 0;
  uint32_t lastBlinkTime = 0;
  bool blinkState = false;

  void paintPmr(bool blinkOn) {
    LedDriver::clearAll();

    // Rails éteints : seules les gares sont allumées dans leur couleur LHand
    // Vert = Conforme entièrement, Orange = Partiellement conforme,
    // Rouge = Non conforme, Bleu = Statut inconnu
    for (uint8_t g = 0; g < NB_GARES; g++) {
      const Gare& ga = GARES[g];
      if (!ga.pivotAPI) continue;

      if (selectedGareIdx >= 0 && (uint8_t)selectedGareIdx == g) {
        // Gare sélectionnée : clignote en blanc
        if (blinkOn) {
          LedDriver::setPixel(ga.led.bus, ga.led.index, 255, 255, 255);
        } else {
          LedDriver::setPixel(ga.led.bus, ga.led.index, 0, 0, 0);
        }
      } else {
        LineColor c = colorForLHand(ga.lhand);
        LedDriver::setPixel(ga.led.bus, ga.led.index, c.r, c.g, c.b);
      }
    }

    LedDriver::showAll();
  }
}

namespace ModePmr {
  void begin() {
    selectedGareIdx = -1;
    lastSelectedTime = 0;
    lastBlinkTime = millis();
    blinkState = true;
    DisplayDriver::showScrolling("ACCESIBILITE", 220);
    paintPmr(true);
  }

  void render() {
    uint32_t now = millis();

    // Retour automatique au défilement "ACCESIBILITE" après 4s sans toucher l'encodeur
    if (selectedGareIdx >= 0 && (now - lastSelectedTime > 4000)) {
      selectedGareIdx = -1;
      DisplayDriver::showScrolling("ACCESIBILITE", 220);
      paintPmr(true);
    }

    if (now - lastBlinkTime >= 300) {
      lastBlinkTime = now;
      blinkState = !blinkState;
      if (selectedGareIdx >= 0) {
        paintPmr(blinkState);
      }
    }

    DisplayDriver::update();
  }

  void handleEncoderDelta(int delta) {
    if (NB_GARES == 0) return;
    int next = (selectedGareIdx < 0) ? 0 : (selectedGareIdx + delta);
    next = ((next % (int)NB_GARES) + (int)NB_GARES) % (int)NB_GARES;
    while (!GARES[next].pivotAPI) {
      next = (next + (delta >= 0 ? 1 : -1) + (int)NB_GARES) % (int)NB_GARES;
    }
    selectedGareIdx = (int8_t)next;
    lastSelectedTime = millis();
    DisplayDriver::showText(GARES[selectedGareIdx].nomCourt);
    paintPmr(true);
  }
}
