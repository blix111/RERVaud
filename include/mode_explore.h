#pragma once
#include "button_input.h"

namespace ModeExplore {
  void begin();
  void render();
  // Encodeur : rotation = navigue dans l'arbre lignes/gares ; clic court = valider/entrer ;
  // clic long = retour/annuler (cf. décision §1.4 du plan).
  void handleEncoderDelta(int delta);
  void handleEncoderButton(ButtonEventType type);
}
