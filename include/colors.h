#pragma once
#include <Arduino.h>

enum class LigneId : uint8_t {
  R1, R2, R3, R4, R7, R8, R9, R13, RE33, S5, S30, S40, S41, S52, COUNT
};

struct LineColor { uint8_t r, g, b; };

// S30/S40/S41 alternent entre deux couleurs dans le temps au lieu d'avoir une
// couleur fixe ; `secondaire` == `primaire` pour une ligne a couleur fixe
// (alterne == false), ce qui permet a colorForLigne() de traiter les deux cas
// avec la meme formule de phase.
struct LigneSpec {
  LineColor primaire;
  LineColor secondaire;
  bool alterne;
};

constexpr uint32_t LIGNE_ALTERNATION_PERIOD_MS = 1500;

extern LigneSpec LIGNE_SPECS[(size_t)LigneId::COUNT];
extern uint32_t g_ligneAlternationPeriodMs;
extern const char* const LIGNE_NOMS[(size_t)LigneId::COUNT];

const LigneSpec& specForLigne(LigneId id);
const char* nomForLigne(LigneId id);

// Couleur a afficher a l'instant nowMs (gere l'alternance des lignes concernees).
LineColor colorForLigne(LigneId id, uint32_t nowMs);

namespace Colors {
  void begin();
  void setSpec(LigneId id, LineColor primaire, LineColor secondaire, bool alterne);
  void saveToNvs();
  void resetDefaults();
}
