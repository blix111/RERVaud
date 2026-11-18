#include "mode_demo.h"
#include "mode_live.h"

namespace ModeDemo {
  void begin() {
    // Phase 1 : premier mode testable de bout en bout sans réseau (cf. plan Phase 1 item 6)
  }

  void tick() {
    // Phase 1 : écrit des TrainPosition synthétiques dans g_sharedData sous g_dataMutex
  }

  void render() {
    ModeLive::render(); // réutilise le renderer LIVE une fois les données synthétiques posées
  }
}
