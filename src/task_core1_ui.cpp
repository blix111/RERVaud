#include <Arduino.h>
#include "button_input.h"
#include "mode_manager.h"
#include "display_driver.h"
#include "led_driver.h"
#include "mode_explore.h"
#include "mode_pax.h"
#include "mode_pmr.h"
#include "mode_live.h"
#include "shared_state.h"

void taskCore1Ui(void* param) {
  (void)param;

  Mode lastMode = (Mode)0xFF; // sentinelle : force l'initialisation du mode de départ (LIVE)
  uint32_t lastRenderTime = 0;
  uint32_t lastBrightnessActivity = 0;
  uint8_t lastBrightPage = 0;

  auto showBrightnessPage = []() {
    char buf[8];
    if (ModeManager::brightnessPage() == 0) {
      uint8_t p = LedDriver::getBrightnessPercent();
      if (p >= 100) snprintf(buf, sizeof(buf), "100%%");
      else snprintf(buf, sizeof(buf), "b%2d%%", p);
    } else {
      snprintf(buf, sizeof(buf), "d%2d ", DisplayDriver::getBrightness());
    }
    DisplayDriver::showText(buf);
  };

  for (;;) {
    ButtonInput::poll();

    // --- Lecture des événements bouton ---
    ButtonEvent ev;
    while (ButtonInput::popEvent(ev)) {
      if (g_studioLedOverride.load()) {
        g_studioLedOverride.store(false);
        lastMode = (Mode)0xFF; // force le rafraîchissement du mode normal dès qu'on touche un bouton
      }
      ModeManager::handleButtonEvent(ev);
      // Déléguer les événements de l'encodeur au mode actif
      if (ev.button == ButtonId::ENCODER) {
        if (ModeManager::current() == Mode::EXPLORE) {
          ModeExplore::handleEncoderButton(ev.type);
        } else if (ModeManager::current() == Mode::LIVE ||
                   ModeManager::current() == Mode::LIVE_TRAINS) {
          ModeLive::handleEncoderButton();
        }
      }
    }

    // --- Rotation encodeur ---
    int delta = ButtonInput::takeEncoderDelta();
    if (delta != 0) {
      if (ModeManager::current() == Mode::BRIGHTNESS) {
        lastBrightnessActivity = millis();
        if (ModeManager::brightnessPage() == 0) {
          // Page 0 : LEDs, 1 % par cran, accéléré (x4) si rotation rapide
          int newP = (int)LedDriver::getBrightnessPercent() + delta * ((abs(delta) >= 2) ? 4 : 1);
          if (newP < 0) newP = 0;
          if (newP > 100) newP = 100;
          LedDriver::setBrightnessPercent((uint8_t)newP);
          LedDriver::showAll();
        } else {
          // Page 1 : afficheur 14 segments, niveau matériel 0..15
          int newL = (int)DisplayDriver::getBrightness() + delta;
          if (newL < 0) newL = 0;
          if (newL > 15) newL = 15;
          DisplayDriver::setBrightness((uint8_t)newL);
        }
        showBrightnessPage();
      } else if (ModeManager::current() == Mode::LIVE ||
                 ModeManager::current() == Mode::LIVE_TRAINS) {
        ModeLive::handleEncoderDelta(delta);
      } else if (ModeManager::current() == Mode::EXPLORE) {
        ModeExplore::handleEncoderDelta(delta);
      } else if (ModeManager::current() == Mode::PAX) {
        ModePax::handleEncoderDelta(delta);
      } else if (ModeManager::current() == Mode::PMR) {
        ModePmr::handleEncoderDelta(delta);
      }
    }

    ModeManager::tick();

    // --- Détection changement de mode ---
    Mode cur = ModeManager::current();
    if (cur != lastMode) {
      Mode prevMode = lastMode;
      lastMode = cur;

      if (cur != Mode::BRIGHTNESS && prevMode != Mode::BRIGHTNESS) {
        LedDriver::clearAll();
      }
      switch (cur) {
        case Mode::EXPLORE:
          if (prevMode != Mode::BRIGHTNESS) {
            ModeExplore::begin();
          } else {
            ModeExplore::render();
          }
          break;
        case Mode::LIVE:
          ModeLive::begin(false);
          break;
        case Mode::LIVE_TRAINS:
          ModeLive::begin(true);
          break;
        case Mode::PAX:
          ModePax::begin();
          break;
        case Mode::PMR:
          ModePmr::begin();
          break;
        case Mode::CAL:
          DisplayDriver::showText("CAL ");
          LedDriver::showAll();
          break;
        case Mode::BRIGHTNESS: {
          lastBrightnessActivity = millis();
          lastBrightPage = ModeManager::brightnessPage();
          showBrightnessPage();
          LedDriver::showAll();
          break;
        }
      }
    }

    // Sortie automatique du menu luminosité après 15s d'inactivité
    // (placé APRÈS la détection de changement de mode, sinon lastBrightnessActivity
    //  n'est pas encore initialisé à l'entrée et le menu se ferme immédiatement)
    if (cur == Mode::BRIGHTNESS && lastMode == Mode::BRIGHTNESS) {
      if (ModeManager::brightnessPage() != lastBrightPage) { // changement de page par clic
        lastBrightPage = ModeManager::brightnessPage();
        lastBrightnessActivity = millis();
        showBrightnessPage();
      }
      if (millis() - lastBrightnessActivity > 15000) {
        ButtonEvent fakeExit{ButtonId::BOTH, ButtonEventType::HOLD_3S}; // quitte + sauvegarde
        ModeManager::handleButtonEvent(fakeExit);
      }
    }

    // --- Render du mode courant (à 50 Hz max) ---
    uint32_t now = millis();
    if (!g_studioLedOverride.load() && (now - lastRenderTime >= 20)) {
      lastRenderTime = now;
      switch (cur) {
        case Mode::LIVE:
        case Mode::LIVE_TRAINS: ModeLive::render(); break;
        case Mode::EXPLORE:     ModeExplore::render(); break;
        case Mode::PAX:         ModePax::render(); break;
        case Mode::PMR:         ModePmr::render(); break;
        case Mode::BRIGHTNESS: DisplayDriver::update(); break;
        default:               DisplayDriver::update(); break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5)); // Échantillonnage boutons/encodeur à 200 Hz
  }
}
