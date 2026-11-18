#include "mode_explore.h"

namespace ModeExplore {
  void begin() {
    // Phase 3
  }

  void render() {
    // Phase 3
  }

  void handleEncoderDelta(int delta) {
    (void)delta; // Phase 3 : navigation dans l'arbre lignes/gares
  }

  void handleEncoderButton(ButtonEventType type) {
    (void)type; // Phase 3 : SHORT_PRESS = valider/entrer, LONG_PRESS = retour/annuler
  }
}
