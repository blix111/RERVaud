#include "mode_pax.h"
#include "led_driver.h"
#include "display_driver.h"
#include "stations.h"

namespace {
  int8_t selectedGareIdx = -1; // -1 = vue globale "PAX "
  uint32_t lastSelectedTime = 0;
  uint32_t lastBlinkTime = 0;
  bool blinkState = false;

  void paintPax(bool blinkOn) {
    LedDriver::clearAll();

    // Rails éteints : seules les gares sont allumées avec une courbe gamma
    // accentuée pour un fort contraste visuel entre les paliers d'affluence.
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
        // Courbe exponentielle (gamma) sur les 6 paliers officiels :
        // 100% (>100k), 80% (>30k), 60% (>25k), 40% (>20k), 20% (>10k), 10% (<10k)
        uint8_t pct = affluencePercentForGare(ga);
        uint8_t r, gVal, bVal;
        switch (pct) {
          case 100: r = 255; gVal = 225; bVal = 150; break; // > 100k (Lausanne) : éclat maximal
          case 80:  r = 160; gVal = 138; bVal =  88; break; // > 30k  (Renens, Fribourg) : fort
          case 60:  r =  90; gVal =  76; bVal =  48; break; // > 25k  (Morges, Neuchâtel) : moyen+
          case 40:  r =  44; gVal =  37; bVal =  22; break; // > 20k  (Nyon, Vevey) : moyen
          case 20:  r =  18; gVal =  15; bVal =   9; break; // > 10k  (Yverdon, Montreux, Gland, Palézieux) : faible
          default:  r =   5; gVal =   4; bVal =   2; break; // < 10k  (petites gares) : lueur minimale
        }
        LedDriver::setPixel(ga.led.bus, ga.led.index, r, gVal, bVal);
      }
    }

    LedDriver::showAll();
  }
}

namespace ModePax {
  void begin() {
    selectedGareIdx = -1;
    lastSelectedTime = 0;
    lastBlinkTime = millis();
    blinkState = true;
    DisplayDriver::showScrolling("INFLUENCE", 220);
    paintPax(true);
  }

  void render() {
    uint32_t now = millis();

    // Retour automatique au défilement "INFLUENCE" après 6s sans toucher l'encodeur
    if (selectedGareIdx >= 0 && (now - lastSelectedTime > 6000)) {
      selectedGareIdx = -1;
      DisplayDriver::showScrolling("INFLUENCE", 220);
      paintPax(true);
    }

    if (now - lastBlinkTime >= 300) {
      lastBlinkTime = now;
      blinkState = !blinkState;
      if (selectedGareIdx >= 0) {
        paintPax(blinkState);
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
    DisplayDriver::showScrolling(GARES[selectedGareIdx].nomAPI, 220);
    paintPax(true);
  }
}
