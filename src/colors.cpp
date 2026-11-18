#include "colors.h"
#include <Preferences.h>
#include <string.h>

namespace {
  constexpr LineColor ROSE = {0xFF, 0x33, 0x99};
  constexpr LineColor CYAN = {0x00, 0xCC, 0xCC};

  const LigneSpec DEFAULT_SPECS[(size_t)LigneId::COUNT] = {
    { {0xFF, 0x66, 0x00}, {0xFF, 0x66, 0x00}, false }, // R1    orange (Grandson-St Maurice)
    { {0xFF, 0x00, 0x00}, {0xFF, 0x00, 0x00}, false }, // R2    rouge
    { {0x00, 0x33, 0x99}, {0x00, 0x33, 0x99}, false }, // R3    bleu foncé
    { {0x00, 0x99, 0xFF}, {0x00, 0x99, 0xFF}, false }, // R4    bleu clair
    { {0xFF, 0xCC, 0x00}, {0xFF, 0xCC, 0x00}, false }, // R7    jaune
    { {0x66, 0xCC, 0x33}, {0x66, 0xCC, 0x33}, false }, // R8    vert clair
    { {0x00, 0x66, 0x22}, {0x00, 0x66, 0x22}, false }, // R9    vert foncé
    { {0x99, 0x33, 0xCC}, {0x99, 0x33, 0xCC}, false }, // R13   violet
    { {0x8B, 0x45, 0x13}, {0x8B, 0x45, 0x13}, false }, // RE33  marron
    { {0x80, 0x00, 0x20}, {0x80, 0x00, 0x20}, false }, // S5    bordeaux
    { ROSE,               CYAN,               true  }, // S30   alterne rose/cyan
    { ROSE,               CYAN,               true  }, // S40   alterne rose/cyan
    { ROSE,               CYAN,               true  }, // S41   alterne rose/cyan
    { {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF}, false }, // S52   blanc
  };
}

LigneSpec LIGNE_SPECS[(size_t)LigneId::COUNT] = {
  { {0xFF, 0x66, 0x00}, {0xFF, 0x66, 0x00}, false },
  { {0xFF, 0x00, 0x00}, {0xFF, 0x00, 0x00}, false },
  { {0x00, 0x33, 0x99}, {0x00, 0x33, 0x99}, false },
  { {0x00, 0x99, 0xFF}, {0x00, 0x99, 0xFF}, false },
  { {0xFF, 0xCC, 0x00}, {0xFF, 0xCC, 0x00}, false },
  { {0x66, 0xCC, 0x33}, {0x66, 0xCC, 0x33}, false },
  { {0x00, 0x66, 0x22}, {0x00, 0x66, 0x22}, false },
  { {0x99, 0x33, 0xCC}, {0x99, 0x33, 0xCC}, false },
  { {0x8B, 0x45, 0x13}, {0x8B, 0x45, 0x13}, false },
  { {0x80, 0x00, 0x20}, {0x80, 0x00, 0x20}, false },
  { ROSE,               CYAN,               true  },
  { ROSE,               CYAN,               true  },
  { ROSE,               CYAN,               true  },
  { {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF}, false },
};

uint32_t g_ligneAlternationPeriodMs = LIGNE_ALTERNATION_PERIOD_MS;

const char* const LIGNE_NOMS[(size_t)LigneId::COUNT] = {
  "R1", "R2", "R3", "R4", "R7", "R8", "R9", "R13", "RE33", "S5", "S30", "S40", "S41", "S52",
};

const LigneSpec& specForLigne(LigneId id) {
  return LIGNE_SPECS[(size_t)id];
}

const char* nomForLigne(LigneId id) {
  return LIGNE_NOMS[(size_t)id];
}

LineColor colorForLigne(LigneId id, uint32_t nowMs) {
  const LigneSpec& spec = specForLigne(id);
  if (!spec.alterne) return spec.primaire;
  uint32_t period = (g_ligneAlternationPeriodMs >= 200) ? g_ligneAlternationPeriodMs : 1500;
  bool phase = (nowMs / period) % 2 == 0;
  return phase ? spec.primaire : spec.secondaire;
}

namespace Colors {
  void begin() {
    memcpy(LIGNE_SPECS, DEFAULT_SPECS, sizeof(LIGNE_SPECS));
    Preferences prefs;
    if (prefs.begin("tb_colors", true)) {
      if (prefs.getBytesLength("specs") == sizeof(LIGNE_SPECS)) {
        prefs.getBytes("specs", LIGNE_SPECS, sizeof(LIGNE_SPECS));
      }
      g_ligneAlternationPeriodMs = prefs.getUInt("alt_ms", LIGNE_ALTERNATION_PERIOD_MS);
      prefs.end();
    }
  }

  void setSpec(LigneId id, LineColor primaire, LineColor secondaire, bool alterne) {
    if ((size_t)id >= (size_t)LigneId::COUNT) return;
    LIGNE_SPECS[(size_t)id].primaire = primaire;
    LIGNE_SPECS[(size_t)id].secondaire = alterne ? secondaire : primaire;
    LIGNE_SPECS[(size_t)id].alterne = alterne;
  }

  void saveToNvs() {
    Preferences prefs;
    if (prefs.begin("tb_colors", false)) {
      prefs.putBytes("specs", LIGNE_SPECS, sizeof(LIGNE_SPECS));
      prefs.putUInt("alt_ms", g_ligneAlternationPeriodMs);
      prefs.end();
    }
  }

  void resetDefaults() {
    memcpy(LIGNE_SPECS, DEFAULT_SPECS, sizeof(LIGNE_SPECS));
    g_ligneAlternationPeriodMs = LIGNE_ALTERNATION_PERIOD_MS;
    saveToNvs();
  }
}
