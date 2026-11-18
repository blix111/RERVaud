#pragma once
#include <Arduino.h>
#include "colors.h"

// Modèle de données du réseau physique (gares/tronçons) mappé sur les LEDs.
// Les tables GARES[]/TRONCONS[] sont vides tant que l'export de l'outil de
// calibration (CalibTool, voir calib_tool.h) n'a pas été collé dans stations.cpp.

struct LedRef {
  uint8_t bus;
  uint8_t index;
};

enum class LHandStatus : uint8_t {
  CONFORME = 0,      // Vert : conforme entièrement à la LHand
  PARTIEL = 1,       // Orange : partiellement conforme à la LHand
  NON_CONFORME = 2,  // Rouge : non conforme à la LHand
  INCONNU = 3        // Bleu : statut inconnu
};

struct Gare {
  const char* nomCourt;   // code 4 car. max, affiché sur le 14 segments
  const char* nomAPI;     // nom exact attendu par transport.opendata.ch
  LedRef led;
  bool pivotAPI;          // fait partie des gares gérées
  uint8_t niveauPMR;      // 0 = inconnu (conservé pour rétrocompatibilité)
  uint32_t voyageurs;     // Fréquentation voyageurs / jour
  LHandStatus lhand;      // Statut d'accessibilité LHand
};

// idxGareAmont/idxGareAval sont par ligne, et non par LED : sur un embranchement,
// une meme LED physique peut etre traversee par plusieurs lignes qui divergent
// juste apres, chacune avec ses propres gares encadrantes.
struct TronconLigne {
  LigneId ligne;
  uint8_t idxGareAmont;
  uint8_t idxGareAval;
};

struct Troncon {
  LedRef led;
  TronconLigne lignes[4];
  uint8_t nbLignes;
};

extern const Gare GARES[];
extern const uint8_t NB_GARES;

extern const Troncon TRONCONS[];
extern const uint8_t NB_TRONCONS;

int findGareIndexByCode(const char* nomCourt);
uint8_t affluencePercentForGare(const Gare& g);
LineColor colorForLHand(LHandStatus status);
