#include "stations.h"
#include <string.h>
#include <strings.h>

// ===== A REMPLACER par le collage de la sortie "EXPORT CPP" de CalibTool =====
const Gare GARES[] = {};
const uint8_t NB_GARES = 0;

const Troncon TRONCONS[] = {};
const uint8_t NB_TRONCONS = 0;
// ===== Fin de la zone à remplacer =====

int findGareIndexByCode(const char* nomCourt) {
  for (uint8_t i = 0; i < NB_GARES; i++) {
    if (strcasecmp(GARES[i].nomCourt, nomCourt) == 0) return i;
  }
  return -1;
}
