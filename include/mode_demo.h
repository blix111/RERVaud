#pragma once

namespace ModeDemo {
  void begin();
  void tick();   // génère des TrainPosition synthétiques dans g_sharedData (sans dépendance réseau)
  void render(); // Phase 1 : réutilise ModeLive::render() sur les données synthétiques
}
