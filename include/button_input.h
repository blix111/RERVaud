#pragma once
#include <Arduino.h>

enum class ButtonId : uint8_t { ENCODER, NAV2 };
enum class ButtonEventType : uint8_t { SHORT_PRESS, LONG_PRESS };

struct ButtonEvent {
  ButtonId button;
  ButtonEventType type;
};

namespace ButtonInput {
  void begin();
  void poll();                         // à appeler à chaque tick de task_core1_ui, jamais bloquant
  int takeEncoderDelta();               // delta accumulé depuis le dernier appel, remis à zéro
  bool popEvent(ButtonEvent& outEvent); // dépile le prochain événement court/long, false si aucun
}
