#pragma once
#include <Arduino.h>
#include <atomic>
#include <time.h>
#include "colors.h"

// État partagé Core0 (producteur réseau) <-> Core1 (consommateur IHM).
// Règle stricte : ne jamais tenir g_dataMutex pendant un I/O RMT/I2C/HTTPS.
// Le producteur le prend seulement pour publier un résultat déjà parsé ;
// le consommateur le prend seulement pour copier les champs nécessaires sur
// la pile, puis le relâche avant de toucher aux LEDs/afficheur.

#define NB_GARES_PIVOT 7
#define MAX_BOARD_ENTRIES 5
#define MAX_TRAINS 32

// Valeurs préfixées PAX_ : Arduino définit LOW/HIGH comme macros (#define LOW 0x0),
// ce qui casse un enum class classique nommé LOW/HIGH (le préprocesseur les remplace
// avant que le compilateur C++ ne voie l'énumération).
enum class PaxLevel : uint8_t { PAX_UNKNOWN, PAX_LOW, PAX_MEDIUM, PAX_HIGH };

struct StationboardEntry {
  char ligneCode[6];
  char destination[24];
  time_t scheduled;
  time_t realtime;
  int16_t delayMin;
  bool cancelled;
};

struct TrainPosition {
  LigneId ligne;
  uint8_t gareAmontIdx;
  uint8_t gareAvalIdx;
  time_t depAmont;
  time_t arrAval;
  int16_t delayMin;
  bool cancelled;
  bool active;
};

struct SharedData {
  StationboardEntry boards[NB_GARES_PIVOT][MAX_BOARD_ENTRIES];
  uint8_t boardCount[NB_GARES_PIVOT];

  TrainPosition trains[MAX_TRAINS];
  uint8_t nbTrains;

  PaxLevel paxByGare[64]; // dimensionné au-delà du nombre de gares attendu

  bool apiHealthy;
  time_t lastGoodPollEpoch;
};

extern SharedData g_sharedData;
extern SemaphoreHandle_t g_dataMutex;
extern std::atomic<uint8_t> g_currentMode;
extern std::atomic<bool> g_demoForced;

namespace SharedState {
  void begin();
}
