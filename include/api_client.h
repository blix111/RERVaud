#pragma once
#include <Arduino.h>
#include "shared_state.h"

namespace ApiClient {
  // Phase 2 : WiFiClientSecure statique réutilisé, deserializeJson(doc, client, Filter(...)),
  // limit=5, recopie strlcpy immédiate dans out[] (le JsonDocument ne doit jamais survivre
  // à la fonction). Voir plan §5 pour les 7 règles de sécurité mémoire.
  bool fetchStationboard(const char* stationApiName,
                          StationboardEntry out[], uint8_t maxOut, uint8_t* outCount);
}
