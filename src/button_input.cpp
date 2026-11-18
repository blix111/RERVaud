#include "button_input.h"
#include "config.h"

namespace {
  const uint32_t LONG_PRESS_MS = 600;
  const uint32_t HOLD_3S_MS = 2500; // 2.5s pour confort et réactivité
  const uint8_t EVENT_QUEUE_SIZE = 8;

  ButtonEvent eventQueue[EVENT_QUEUE_SIZE];
  uint8_t queueHead = 0, queueTail = 0, queueCount = 0;

  void pushEvent(ButtonId id, ButtonEventType type) {
    if (queueCount >= EVENT_QUEUE_SIZE) return;
    eventQueue[queueTail] = {id, type};
    queueTail = (queueTail + 1) % EVENT_QUEUE_SIZE;
    queueCount++;
  }

  // --- Anti-rebond Encodeur rotatif avec verrouillage de cran ---
  int16_t encoderDelta = 0;
  int lastA = HIGH;
  uint32_t lastEncStepTime = 0;
  bool encoderArmed = true;

  // --- Gestion Boutons et Maintien pour Menu caché ---
  uint32_t bothDownSince = 0;
  bool hold3sTriggered = false;
  bool holdRotateTriggered = false;
  uint32_t lockoutUntil = 0;

  bool encWasDown = false;
  uint32_t encDownTime = 0;

  bool navWasDown = false;
  uint32_t navDownTime = 0;
  bool navLongFired = false;

  void pollEncoder() {
    int curA = digitalRead(PIN_ENCODER_A);
    int curB = digitalRead(PIN_ENCODER_B);

    // Une fois qu'un pas a été déclenché, on attend obligatoirement que l'encodeur
    // soit revenu au repos dans un cran (A=HIGH et B=HIGH) stabilisé
    // avant de réarmer la détection. Empêche tout double saut de cran.
    if (!encoderArmed) {
      if (curA == HIGH && curB == HIGH) {
        static uint32_t restSince = 0;
        if (restSince == 0) restSince = millis();
        if (millis() - restSince >= 15) {
          encoderArmed = true;
          restSince = 0;
        }
      } else {
        // En transition
      }
      lastA = curA;
      return;
    }

    // Détection front descendant sur A
    if (lastA == HIGH && curA == LOW) {
      uint32_t now = millis();
      if (now - lastEncStepTime >= 35) {
        lastEncStepTime = now;
        int dir = (curB == HIGH) ? 1 : -1;
        bool btnHeld = (digitalRead(PIN_BTN_NAV2) == LOW) || (digitalRead(PIN_ENCODER_BTN) == LOW);
        if (btnHeld) {
          if (!holdRotateTriggered) {
            holdRotateTriggered = true;
            hold3sTriggered = true;
            navLongFired = true;
            encWasDown = false;
            navWasDown = false;
            lockoutUntil = now + 500;
            pushEvent(ButtonId::BOTH, ButtonEventType::HOLD_ROTATE);
          }
        } else {
          encoderDelta += dir;
        }
        encoderArmed = false; // Désarmé jusqu'au retour complet dans le cran suivant !
      }
    }
    lastA = curA;
  }

  void pollButtons() {
    uint32_t now = millis();
    bool encDown = (digitalRead(PIN_ENCODER_BTN) == LOW);
    bool navDown = (digitalRead(PIN_BTN_NAV2) == LOW);

    if (!encDown && !navDown) {
      holdRotateTriggered = false;
    }

    if (holdRotateTriggered) {
      bothDownSince = 0;
      encWasDown = false;
      navWasDown = false;
      return;
    }

    // Verrou post-déclenchement pour ne jamais émettre de faux clics au relâchement
    if (now < lockoutUntil) {
      encWasDown = false;
      navWasDown = false;
      return;
    }

    // 1. Détection maintien pour Menu caché (2.5s) :
    //    Déclenché soit par les 2 boutons enfoncés, soit par le clic encodeur maintenu !
    bool comboPressed = (encDown && navDown) || encDown;
    if (comboPressed) {
      if (bothDownSince == 0) {
        bothDownSince = now;
        hold3sTriggered = false;
      } else if (!hold3sTriggered && (now - bothDownSince >= HOLD_3S_MS)) {
        hold3sTriggered = true;
        lockoutUntil = now + 800; // 800 ms de verrouillage pour relâcher tranquillement
        pushEvent(ButtonId::BOTH, ButtonEventType::HOLD_3S);
        bothDownSince = 0;
        encWasDown = false;
        navWasDown = false;
        return;
      }
    } else {
      bothDownSince = 0;
      hold3sTriggered = false;
    }

    // 2. Gestion bouton Nav2 individuel (changement de mode au clic court)
    if (navDown && !encDown) {
      if (!navWasDown) {
        navWasDown = true;
        navDownTime = now;
        navLongFired = false;
      } else if (!navLongFired && (now - navDownTime >= LONG_PRESS_MS)) {
        navLongFired = true;
        pushEvent(ButtonId::NAV2, ButtonEventType::LONG_PRESS);
      }
    } else if (!navDown && navWasDown) {
      navWasDown = false;
      if (!navLongFired && (now - navDownTime >= 30) && (now - navDownTime < LONG_PRESS_MS)) {
        pushEvent(ButtonId::NAV2, ButtonEventType::SHORT_PRESS);
      }
    }

    // 3. Gestion bouton Encodeur individuel (clic court)
    if (encDown && !navDown) {
      if (!encWasDown) {
        encWasDown = true;
        encDownTime = now;
      }
    } else if (!encDown && encWasDown) {
      encWasDown = false;
      if (!hold3sTriggered && (now - encDownTime >= 30) && (now - encDownTime < HOLD_3S_MS)) {
        pushEvent(ButtonId::ENCODER, ButtonEventType::SHORT_PRESS);
      }
    }
  }
}

namespace ButtonInput {
  void begin() {
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
    pinMode(PIN_BTN_NAV2, INPUT_PULLUP);
    lastA = digitalRead(PIN_ENCODER_A);
    encoderArmed = true;
  }

  void poll() {
    pollEncoder();
    pollButtons();
  }

  int takeEncoderDelta() {
    int d = encoderDelta;
    encoderDelta = 0;
    return d;
  }

  bool popEvent(ButtonEvent& outEvent) {
    if (queueCount == 0) return false;
    outEvent = eventQueue[queueHead];
    queueHead = (queueHead + 1) % EVENT_QUEUE_SIZE;
    queueCount--;
    return true;
  }
}
