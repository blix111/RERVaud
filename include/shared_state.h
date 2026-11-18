#pragma once
#include <Arduino.h>
#include <atomic>
#include <time.h>
#include "colors.h"

#define NB_GARES_PIVOT 29   // indexé par index GARES[] (28 gares + HORS)
#define MAX_BOARD_ENTRIES 10
#define MAX_TRAINS 32

enum class PaxLevel : uint8_t { PAX_UNKNOWN, PAX_LOW, PAX_MEDIUM, PAX_HIGH };

struct StationboardEntry {
  uint8_t ligne;          // (LigneId) ou 0xFF si ligne non représentée sur la carte
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
  bool boardPolled[NB_GARES_PIVOT];
  uint32_t boardPollMs[NB_GARES_PIVOT];

  TrainPosition trains[MAX_TRAINS];
  uint8_t nbTrains;

  PaxLevel paxByGare[64];

  bool apiHealthy;
  time_t lastGoodPollEpoch;
  uint32_t totalPollsOk;
  uint32_t totalPollsFail;
};

// Paramètres configurables en direct depuis l'interface Web / USB (persistés en NVS)
struct AppSettings {
  uint16_t pollFastMs;          // intervalle premier balayage (défaut: 20 ms)
  uint16_t pollNormalMs;        // intervalle régime établi (défaut: 100 ms)
  uint8_t  apiLimit;            // nb départs demandés à transport.opendata.ch (défaut: 20)
  uint16_t stationWindowSec;    // fenêtre +/- secondes "train à quai" (défaut: 35 s)
  uint16_t gareLookaheadSec;    // fenêtre max prochain train affiché sur une gare en LIVE (défaut: 1800 s)
  uint8_t  gareNoTrainMode;     // 0 = éteinte si aucun train dans la fenêtre, 1 = vert doux
  uint16_t blinkBaseHalfMs;     // demi-période clignotement à 0 min (défaut: 660 ms)
  uint16_t blinkStepPerMinMs;   // accélération par minute de retard (défaut: 108 ms)
  uint16_t scrollSpeedMs;       // vitesse défilement 14 segments (défaut: 220 ms)
  uint16_t travelScalePct;      // facteur d'échelle temps de trajet trains (défaut: 100 %)
};

extern SharedData g_sharedData;
extern AppSettings g_settings;
extern SemaphoreHandle_t g_dataMutex;
extern std::atomic<uint8_t> g_currentMode;
extern std::atomic<int8_t> g_forcePollGare; // -1 = normal, >=0 = forcer poll immédiat de cette gare, 99 = tout re-poller
extern std::atomic<bool> g_studioLedOverride; // true = le Studio Web/USB contrôle directement les LEDs

namespace SharedState {
  void begin();
  void saveSettings();
  void resetSettings();
}
