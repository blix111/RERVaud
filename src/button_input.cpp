#include "button_input.h"
#include "config.h"

namespace {
  const uint32_t LONG_PRESS_MS = 600;
  const uint8_t EVENT_QUEUE_SIZE = 8;

  int lastEncA = HIGH;
  int16_t encoderDelta = 0;

  struct BtnState { bool down; uint32_t downAt; bool longFired; };
  BtnState encBtn{false, 0, false};
  BtnState navBtn{false, 0, false};

  ButtonEvent eventQueue[EVENT_QUEUE_SIZE];
  uint8_t queueHead = 0, queueTail = 0, queueCount = 0;

  void pushEvent(ButtonId id, ButtonEventType type) {
    if (queueCount >= EVENT_QUEUE_SIZE) return; // file pleine: événement perdu (ne doit pas arriver en usage normal)
    eventQueue[queueTail] = {id, type};
    queueTail = (queueTail + 1) % EVENT_QUEUE_SIZE;
    queueCount++;
  }

  void pollButton(uint8_t pin, ButtonId id, BtnState& st) {
    bool pressed = (digitalRead(pin) == LOW);
    if (pressed && !st.down) {
      st.down = true; st.downAt = millis(); st.longFired = false;
    } else if (pressed && st.down && !st.longFired && (millis() - st.downAt > LONG_PRESS_MS)) {
      st.longFired = true;
      pushEvent(id, ButtonEventType::LONG_PRESS);
    } else if (!pressed && st.down) {
      st.down = false;
      if (!st.longFired) pushEvent(id, ButtonEventType::SHORT_PRESS);
    }
  }
}

namespace ButtonInput {
  void begin() {
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
    pinMode(PIN_BTN_NAV2, INPUT_PULLUP);
    lastEncA = digitalRead(PIN_ENCODER_A);
  }

  void poll() {
    static uint32_t lastEncTime = 0;
    int curA = digitalRead(PIN_ENCODER_A);
    if (curA != lastEncA && curA == LOW && (millis() - lastEncTime > 5)) {
      lastEncTime = millis();
      int dir = (digitalRead(PIN_ENCODER_B) == HIGH) ? 1 : -1;
      encoderDelta += dir;
    }
    lastEncA = curA;

    pollButton(PIN_ENCODER_BTN, ButtonId::ENCODER, encBtn);
    pollButton(PIN_BTN_NAV2, ButtonId::NAV2, navBtn);
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
