#include "mode_live.h"
#include "led_driver.h"
#include "display_driver.h"
#include "stations.h"
#include "colors.h"
#include "shared_state.h"
#include "wifi_manager.h"
#include "ntp_time.h"
#include <Arduino.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

namespace {
  // --- Trains actifs positionnés en temps réel sur leur LED correspondante ---
  struct ActiveTrainLed {
    LedRef led;
    uint8_t ligne;
    int16_t delayMin;
    bool cancelled;
    uint8_t gareAmont;
    uint8_t gareAval;
    char destination[24];
  };
  constexpr uint8_t MAX_ACTIVE_TRAIN_LEDS = 48;
  ActiveTrainLed g_trainLeds[MAX_ACTIVE_TRAIN_LEDS];
  uint8_t g_nbTrainLeds = 0;

  // Sélection d'un train via l'encodeur (-1 = aucune sélection, vue globale)
  int8_t g_selectedTrainIdx = -1;
  uint32_t g_lastEncoderMs = 0;
  constexpr uint32_t SELECTION_TIMEOUT_MS = 10000;

  // Copie locale thread-safe des tableaux de départs
  StationboardEntry g_localBoards[NB_GARES_PIVOT][MAX_BOARD_ENTRIES];
  uint8_t g_localCounts[NB_GARES_PIVOT];
  bool g_localPolled[NB_GARES_PIVOT];

  uint32_t lastRefresh = 0;
  uint32_t lastClockMin = 0xFFFFFFFF;

  // Bannière défilante "VUE LIVE" à l'entrée du mode puis affichage de l'heure
  bool bannerActive = true;
  uint32_t bannerUntil = 0;
  constexpr uint32_t BANNER_DURATION_MS = 3400;

  // Temps de parcours CFF réels (en secondes) entre chaque paire de gares adjacentes sur le PCB
  int32_t realTravelSeconds(uint8_t gareA, uint8_t gareB, uint8_t nbLeds) {
    uint8_t u = (gareA < gareB) ? gareA : gareB;
    uint8_t v = (gareA < gareB) ? gareB : gareA;
    int32_t sec = 0;
    if      (u == 0  && v == 1)  sec = 2460; // Le Brassus <-> Vallorbe (41 min)
    else if (u == 1  && v == 11) sec = 1980; // Vallorbe <-> Renens (33 min)
    else if (u == 11 && v == 27) sec = 1380; // Yverdon <-> Renens (23 min)
    else if (u == 26 && v == 27) sec = 300;  // Grandson <-> Yverdon (5 min)
    else if (u == 25 && v == 26) sec = 1680; // Grandson <-> Neuchâtel (28 min)
    else if (u == 6  && v == 7)  sec = 240;  // Nyon <-> Gland (4 min)
    else if (u == 7  && v == 8)  sec = 300;  // Gland <-> Rolle (5 min)
    else if (u == 8  && v == 9)  sec = 300;  // Rolle <-> Allaman (5 min)
    else if (u == 9  && v == 10) sec = 420;  // Allaman <-> Morges (7 min)
    else if (u == 10 && v == 11) sec = 450;  // Morges <-> Renens (7.5 min)
    else if (u == 11 && v == 12) sec = 360;  // Renens <-> Lausanne (6 min)
    else if (u == 12 && v == 13) sec = 900;  // Lausanne <-> Vevey (15 min)
    else if (u == 13 && v == 14) sec = 420;  // Vevey <-> Montreux (7 min)
    else if (u == 14 && v == 15) sec = 300;  // Montreux <-> Villeneuve (5 min)
    else if (u == 15 && v == 16) sec = 540;  // Villeneuve <-> Aigle (9 min)
    else if (u == 16 && v == 17) sec = 360;  // Aigle <-> Bex (6 min)
    else if (u == 17 && v == 18) sec = 240;  // Bex <-> St-Maurice (4 min)
    else if (u == 2  && v == 12) sec = 720;  // Lausanne <-> Puidoux (12 min)
    else if (u == 2  && v == 13) sec = 780;  // Puidoux <-> Vevey (13 min)
    else if (u == 2  && v == 3)  sec = 420;  // Puidoux <-> Palézieux (7 min)
    else if (u == 3  && v == 4)  sec = 900;  // Palézieux <-> Romont (15 min)
    else if (u == 4  && v == 5)  sec = 1320; // Romont <-> Fribourg (22 min)
    else if (u == 3  && v == 21) sec = 2160; // Palézieux <-> Payerne (36 min)
    else if (u == 20 && v == 21) sec = 540;  // Payerne <-> Avenches (9 min)
    else if (u == 19 && v == 20) sec = 480;  // Avenches <-> Morat (8 min)
    else if (u == 19 && v == 23) sec = 540;  // Morat <-> Kerzers (9 min)
    else if (u == 23 && v == 28) sec = 1080; // Kerzers <-> Berne/HORS (18 min)
    else if (u == 23 && v == 24) sec = 420;  // Kerzers <-> Ins (7 min)
    else if (u == 24 && v == 25) sec = 1080; // Ins <-> Neuchâtel (18 min)
    else if (u == 22 && v == 27) sec = 1140; // Yverdon <-> Estavayer (19 min)
    else if (u == 21 && v == 22) sec = 600;  // Estavayer <-> Payerne (10 min)
    else if (u == 5  && v == 27) sec = 3120; // Yverdon <-> Fribourg (52 min)
    else {
      sec = (int32_t)nbLeds * 45;
      if (sec < 180) sec = 180;
    }
    uint16_t scale = g_settings.travelScalePct ? g_settings.travelScalePct : 100;
    return (sec * (int32_t)scale) / 100;
  }

  // Récupère la séquence ordonnée des LEDs physiques reliant gareA -> gareB
  uint8_t getOrderedSegmentLeds(uint8_t gareA, uint8_t gareB, LedRef out[], uint8_t maxLeds) {
    uint8_t n = 0;

    // Cas particulier : YVER(27) <-> RENE(11) s'étend sur Bus 3 (40..52), Bus 0 (22..30), Bus 1 (18)
    if ((gareA == 27 && gareB == 11) || (gareA == 11 && gareB == 27)) {
      const uint8_t busOrder[3] = {3, 0, 1};
      for (uint8_t b = 0; b < 3; b++) {
        for (uint8_t t = 0; t < NB_TRONCONS && n < maxLeds; t++) {
          if (TRONCONS[t].led.bus != busOrder[b]) continue;
          for (uint8_t l = 0; l < TRONCONS[t].nbLignes; l++) {
            uint8_t a = TRONCONS[t].lignes[l].idxGareAmont;
            uint8_t v = TRONCONS[t].lignes[l].idxGareAval;
            if ((a == 27 && v == 11) || (a == 11 && v == 27)) {
              out[n++] = TRONCONS[t].led;
              break;
            }
          }
        }
      }
      if (gareA == 11) {
        for (uint8_t i = 0; i < n / 2; i++) {
          LedRef tmp = out[i]; out[i] = out[n - 1 - i]; out[n - 1 - i] = tmp;
        }
      }
      return n;
    }

    // Collecte toutes les LEDs de TRONCONS reliant {gareA, gareB}
    for (uint8_t t = 0; t < NB_TRONCONS && n < maxLeds; t++) {
      for (uint8_t l = 0; l < TRONCONS[t].nbLignes; l++) {
        uint8_t a = TRONCONS[t].lignes[l].idxGareAmont;
        uint8_t v = TRONCONS[t].lignes[l].idxGareAval;
        if ((a == gareA && v == gareB) || (a == gareB && v == gareA)) {
          out[n++] = TRONCONS[t].led;
          break;
        }
      }
    }
    if (n <= 1) return n;

    // Vérifie si l'ordre collecté va de gareA vers gareB ou de gareB vers gareA
    bool reverse = false;
    LedRef ledA = GARES[gareA].led;
    LedRef ledB = GARES[gareB].led;
    if (out[0].bus == ledA.bus && out[n - 1].bus == ledA.bus) {
      int d0 = abs((int)out[0].index - (int)ledA.index);
      int d1 = abs((int)out[n - 1].index - (int)ledA.index);
      if (d0 > d1) reverse = true;
    } else if (out[n - 1].bus == ledA.bus && abs((int)out[n - 1].index - (int)ledA.index) <= 4) {
      reverse = true;
    } else if (out[0].bus == ledB.bus && abs((int)out[0].index - (int)ledB.index) <= 4) {
      reverse = true;
    } else if (gareA == 13 && gareB == 2) {
      reverse = true;
    }

    if (reverse) {
      for (uint8_t i = 0; i < n / 2; i++) {
        LedRef tmp = out[i]; out[i] = out[n - 1 - i]; out[n - 1 - i] = tmp;
      }
    }
    return n;
  }

  int matchDestinationGare(const char* dest) {
    if (!dest || !dest[0]) return -1;
    for (uint8_t g = 0; g < NB_GARES; g++) {
      if (GARES[g].nomAPI[0] && strcasestr(dest, GARES[g].nomAPI) != nullptr) return g;
    }
    if (strcasestr(dest, "Gen") || strcasestr(dest, "Annemasse") || strcasestr(dest, "Coppet")) return 6;
    if (strcasestr(dest, "Bern")) return 28;
    if (strcasestr(dest, "Murten")) return 19;
    if (strcasestr(dest, "Freiburg")) return 5;
    if (strcasestr(dest, "Pal")) return 3;
    if (strcasestr(dest, "Neuch")) return 25;
    if (strcasestr(dest, "Yverdon")) return 27;
    if (strcasestr(dest, "Estavayer")) return 22;
    if (strcasestr(dest, "Maurice") || strcasestr(dest, "Martigny")) return 18;
    if (strcasestr(dest, "Brassus")) return 0;
    return -1;
  }

  struct LineChain {
    uint8_t ligne;
    uint8_t gares[14];
    uint8_t len;
  };

  const LineChain LINE_CHAINS[] = {
    { (uint8_t)LigneId::R1,   {26, 27, 11, 12, 13, 14, 15, 16, 17, 18}, 10 },
    { (uint8_t)LigneId::R2,   {27, 11, 12, 13, 14, 15, 16, 17}, 8 },
    { (uint8_t)LigneId::R3,   {1, 11, 12, 13}, 4 },
    { (uint8_t)LigneId::R4,   {0, 1, 11, 12, 13}, 5 },
    { (uint8_t)LigneId::R7,   {2, 13}, 2 },
    { (uint8_t)LigneId::R8,   {9, 10, 11, 12, 2, 3, 21, 20}, 8 },
    { (uint8_t)LigneId::R9,   {9, 10, 11, 12, 2, 3, 21, 20, 19}, 9 },
    { (uint8_t)LigneId::R13,  {27, 26, 25}, 3 },
    { (uint8_t)LigneId::RE33, {6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18}, 13 },
    { (uint8_t)LigneId::S5,   {25, 24, 23, 28}, 4 },
    { (uint8_t)LigneId::S5,   {20, 19, 23, 28}, 4 },
    { (uint8_t)LigneId::S30,  {27, 22, 21}, 3 },
    { (uint8_t)LigneId::S30,  {27, 5}, 2 },
    { (uint8_t)LigneId::S40,  {2, 3, 4, 5}, 4 },
    { (uint8_t)LigneId::S41,  {2, 3, 4, 5}, 4 },
    { (uint8_t)LigneId::S52,  {21, 20, 19, 23, 28}, 5 },
    { (uint8_t)LigneId::S52,  {24, 23, 28}, 3 },
  };
  constexpr uint8_t NB_CHAINS = sizeof(LINE_CHAINS) / sizeof(LINE_CHAINS[0]);

  void addActiveTrainLed(LedRef led, uint8_t ligne, int16_t delayMin, bool cancelled,
                         uint8_t gareAmont, uint8_t gareAval, const char* destination) {
    for (uint8_t i = 0; i < g_nbTrainLeds; i++) {
      if (g_trainLeds[i].led.bus == led.bus &&
          g_trainLeds[i].led.index == led.index &&
          g_trainLeds[i].ligne == ligne) {
        if (delayMin > g_trainLeds[i].delayMin) g_trainLeds[i].delayMin = delayMin;
        return;
      }
    }
    if (g_nbTrainLeds < MAX_ACTIVE_TRAIN_LEDS) {
      ActiveTrainLed& dst = g_trainLeds[g_nbTrainLeds++];
      dst.led = led;
      dst.ligne = ligne;
      dst.delayMin = delayMin;
      dst.cancelled = cancelled;
      dst.gareAmont = gareAmont;
      dst.gareAval = gareAval;
      strlcpy(dst.destination, destination ? destination : "", sizeof(dst.destination));
    }
  }

  void computeTrainPositions(time_t now) {
    g_nbTrainLeds = 0;
    if (now == 0) return;

    int32_t winSec = g_settings.stationWindowSec ? g_settings.stationWindowSec : 35;

    // 1. Trains actuellement à quai dans une gare (départ dans [-winSec, +winSec])
    for (uint8_t g = 0; g < NB_GARES; g++) {
      if (!GARES[g].pivotAPI || !g_localPolled[g]) continue;
      for (uint8_t i = 0; i < g_localCounts[g]; i++) {
        const StationboardEntry& e = g_localBoards[g][i];
        if (e.ligne == 0xFF) continue;
        int32_t dt = (int32_t)(e.realtime - now);
        if (dt >= -winSec && dt <= winSec) {
          addActiveTrainLed(GARES[g].led, e.ligne, e.delayMin, e.cancelled, g, g, e.destination);
        }
      }
    }

    // 2. Trains en circulation sur chaque tronçon entre deux gares successives (A -> B)
    LedRef segLeds[32];
    for (uint8_t c = 0; c < NB_CHAINS; c++) {
      const LineChain& ch = LINE_CHAINS[c];
      uint8_t lg = ch.ligne;

      for (uint8_t s = 0; s + 1 < ch.len; s++) {
        for (int dir = 0; dir < 2; dir++) {
          uint8_t posA = (dir == 0) ? s : (s + 1);
          uint8_t posB = (dir == 0) ? (s + 1) : s;
          uint8_t gareA = ch.gares[posA];
          uint8_t gareB = ch.gares[posB];

          uint8_t nbLeds = getOrderedSegmentLeds(gareA, gareB, segLeds, 32);
          if (nbLeds == 0) continue;

          int32_t estTravelSec = realTravelSeconds(gareA, gareB, nbLeds);

          // A) Recherche via les départs à venir en gareB
          if (g_localPolled[gareB]) {
            for (uint8_t ib = 0; ib < g_localCounts[gareB]; ib++) {
              const StationboardEntry& eB = g_localBoards[gareB][ib];
              if (eB.ligne != lg) continue;
              int32_t dtB = (int32_t)(eB.realtime - now);
              if (dtB <= winSec || dtB > estTravelSec + 60) continue;

              int destGare = matchDestinationGare(eB.destination);
              int destPosInChain = -1;
              for (uint8_t k = 0; k < ch.len; k++) {
                if (ch.gares[k] == destGare) { destPosInChain = k; break; }
              }
              if (destPosInChain >= 0) {
                if (dir == 0 && destPosInChain < (int)posB) continue;
                if (dir == 1 && destPosInChain > (int)posB) continue;
              }

              time_t tA = eB.realtime - estTravelSec;
              bool blockedAtA = false;
              if (g_localPolled[gareA]) {
                for (uint8_t ia = 0; ia < g_localCounts[gareA]; ia++) {
                  const StationboardEntry& eA = g_localBoards[gareA][ia];
                  if (eA.ligne != lg) continue;
                  int32_t diffAB = (int32_t)(eB.realtime - eA.realtime);
                  if (diffAB > 30 && diffAB <= estTravelSec + 300) {
                    if (eA.realtime > now - 15) {
                      blockedAtA = true;
                    } else {
                      tA = eA.realtime;
                    }
                    break;
                  }
                }
              }
              if (blockedAtA) continue;

              int32_t totalDur = (int32_t)((eB.realtime - winSec) - tA);
              int32_t elapsed  = (int32_t)(now - tA);
              if (totalDur > 0 && elapsed >= 0 && elapsed < totalDur) {
                uint8_t ledIdx = (uint8_t)((elapsed * nbLeds) / totalDur);
                if (ledIdx >= nbLeds) ledIdx = nbLeds - 1;
                addActiveTrainLed(segLeds[ledIdx], lg, eB.delayMin, eB.cancelled, gareA, gareB, eB.destination);
              }
            }
          }

          // B) Train ayant quitté gareA vers un terminus gareB
          bool isTerminusB = (posB == 0 || posB == ch.len - 1);
          if (isTerminusB && g_localPolled[gareA]) {
            for (uint8_t ia = 0; ia < g_localCounts[gareA]; ia++) {
              const StationboardEntry& eA = g_localBoards[gareA][ia];
              if (eA.ligne != lg) continue;
              int32_t elapsed = (int32_t)(now - eA.realtime);
              if (elapsed <= winSec || elapsed >= estTravelSec) continue;
              int destGare = matchDestinationGare(eA.destination);
              if (destGare == (int)gareB) {
                uint8_t ledIdx = (uint8_t)(((elapsed - winSec) * nbLeds) / (estTravelSec - winSec));
                if (ledIdx >= nbLeds) ledIdx = nbLeds - 1;
                addActiveTrainLed(segLeds[ledIdx], lg, eA.delayMin, eA.cancelled, gareA, gareB, eA.destination);
              }
            }
          }
        }
      }
    }

    if (g_selectedTrainIdx >= (int8_t)g_nbTrainLeds) {
      g_selectedTrainIdx = (g_nbTrainLeds > 0) ? (int8_t)(g_nbTrainLeds - 1) : -1;
    }
  }

  void refreshData() {
    time_t now = NtpTime::isSynced() ? NtpTime::now() : 0;

    if (g_dataMutex && xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      memcpy(g_localBoards, g_sharedData.boards, sizeof(g_localBoards));
      memcpy(g_localCounts, g_sharedData.boardCount, sizeof(g_localCounts));
      memcpy(g_localPolled, g_sharedData.boardPolled, sizeof(g_localPolled));
      xSemaphoreGive(g_dataMutex);
    }

    computeTrainPositions(now);
  }

  // Couleur fixe selon le retard du train (sans clignotement ni couleur de ligne) :
  //   - <= 0 min (à l'heure) : Vert (0, 230, 0)
  //   - 1 à 2 min de retard  : Jaune/Ambre (255, 180, 0)
  //   - 3 à 4 min de retard  : Orange vif (255, 65, 0)
  //   - >= 5 min ou supprimé : Rouge (255, 0, 0)
  LineColor colorForDelay(int16_t delayMin, bool cancelled) {
    if (cancelled || delayMin >= 5) return LineColor{255, 0, 0};
    if (delayMin >= 3)              return LineColor{255, 65, 0};
    if (delayMin >= 1)              return LineColor{255, 180, 0};
    return LineColor{0, 230, 0};
  }

  // Convertit une chaîne UTF-8 (ex: "Genève", "Palézieux", "Neuchâtel") en ASCII majuscule
  // compatible avec l'afficheur 14 segments.
  void normalizeAscii14Seg(const char* in, char* out, size_t maxOut) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < maxOut; i++) {
      uint8_t c = (uint8_t)in[i];
      if (c == 0xC3 && in[i + 1]) {
        uint8_t n = (uint8_t)in[++i];
        if (n >= 0x80 && n <= 0x86) out[o++] = 'A';
        else if (n == 0x87)         out[o++] = 'C';
        else if (n >= 0x88 && n <= 0x8B) out[o++] = 'E';
        else if (n >= 0x8C && n <= 0x8F) out[o++] = 'I';
        else if (n >= 0x92 && n <= 0x96) out[o++] = 'O';
        else if (n >= 0x99 && n <= 0x9C) out[o++] = 'U';
        else if (n >= 0xA0 && n <= 0xA6) out[o++] = 'A';
        else if (n == 0xA7)         out[o++] = 'C';
        else if (n >= 0xA8 && n <= 0xAB) out[o++] = 'E';
        else if (n >= 0xAC && n <= 0xAF) out[o++] = 'I';
        else if (n >= 0xB2 && n <= 0xB6) out[o++] = 'O';
        else if (n >= 0xB9 && n <= 0xBC) out[o++] = 'U';
      } else if (c < 0x80) {
        out[o++] = (char)toupper(c);
      }
    }
    out[o] = '\0';
  }

  void showClock() {
    if (!NtpTime::isSynced()) return;
    time_t t = NtpTime::now();
    struct tm tmS;
    localtime_r(&t, &tmS);
    char buf[5];
    snprintf(buf, sizeof(buf), "%02d%02d", tmS.tm_hour, tmS.tm_min);
    DisplayDriver::showText(buf);
    lastClockMin = tmS.tm_hour * 60 + tmS.tm_min;
  }

  void showSelectedTrainBanner() {
    uint16_t speed = g_settings.scrollSpeedMs ? g_settings.scrollSpeedMs : 220;
    if (g_nbTrainLeds == 0 || g_selectedTrainIdx < 0) {
      DisplayDriver::showScrolling("AUCUN TRAIN", speed);
      return;
    }
    const ActiveTrainLed& tr = g_trainLeds[g_selectedTrainIdx];
    const char* nom = LIGNE_NOMS[tr.ligne];
    char termAscii[24];
    normalizeAscii14Seg(tr.destination[0] ? tr.destination : GARES[tr.gareAval].nomCourt,
                        termAscii, sizeof(termAscii));

    char buf[56];
    if (tr.cancelled) {
      snprintf(buf, sizeof(buf), "%s %s SUPPR", nom, termAscii);
    } else if (tr.delayMin > 0) {
      snprintf(buf, sizeof(buf), "%s %s +%dMIN", nom, termAscii, (int)tr.delayMin);
    } else {
      snprintf(buf, sizeof(buf), "%s %s +0MIN", nom, termAscii);
    }
    DisplayDriver::showScrolling(buf, speed);
  }

  void paintRealtimeTrains(uint32_t nowMs) {
    // Mode LIVE : affiche chaque train sur sa LED correspondante avec une couleur fixe
    // fonction de son retard (Vert = à l'heure, Jaune = +1..2 min, Orange = +3..4 min, Rouge = >= +5 min).
    // Seul le train sélectionné à l'encodeur alterne avec du blanc pour être repéré sur la carte.
    LedDriver::clearAll();

    for (uint8_t i = 0; i < g_nbTrainLeds; i++) {
      const ActiveTrainLed& tr = g_trainLeds[i];
      LineColor c = colorForDelay(tr.delayMin, tr.cancelled);

      if (g_selectedTrainIdx >= 0 && (uint8_t)g_selectedTrainIdx == i) {
        bool whitePhase = ((nowMs / 200) % 2) == 0;
        if (whitePhase) {
          LedDriver::setPixel(tr.led.bus, tr.led.index, 255, 255, 255);
        } else {
          LedDriver::setPixel(tr.led.bus, tr.led.index, c.r, c.g, c.b);
        }
      } else {
        LedDriver::setPixel(tr.led.bus, tr.led.index, c.r, c.g, c.b);
      }
    }

    LedDriver::showAll();
  }
}

namespace ModeLive {
  void begin(bool trainMode) {
    (void)trainMode;
    lastRefresh = 0;
    lastClockMin = 0xFFFFFFFF;
    g_selectedTrainIdx = -1;
    g_lastEncoderMs = 0;

    bannerActive = true;
    bannerUntil = millis() + BANNER_DURATION_MS;
    uint16_t speed = g_settings.scrollSpeedMs ? g_settings.scrollSpeedMs : 220;
    DisplayDriver::showScrolling("VUE LIVE", speed);

    refreshData();
    paintRealtimeTrains(millis());
  }

  void render() {
    uint32_t now = millis();

    if (now - lastRefresh >= 400) {
      lastRefresh = now;
      refreshData();
    }

    // Désélection automatique après 10s sans tourner l'encodeur -> retour à l'heure
    if (g_selectedTrainIdx >= 0 && (now - g_lastEncoderMs >= SELECTION_TIMEOUT_MS)) {
      g_selectedTrainIdx = -1;
      lastClockMin = 0xFFFFFFFF;
      if (NtpTime::isSynced()) {
        showClock();
      } else {
        DisplayDriver::showScrolling("VUE LIVE", g_settings.scrollSpeedMs ? g_settings.scrollSpeedMs : 220);
      }
    }

    if (g_selectedTrainIdx < 0) {
      if (bannerActive) {
        if (now >= bannerUntil && NtpTime::isSynced()) {
          bannerActive = false;
          showClock();
        }
      } else if (NtpTime::isSynced()) {
        time_t t = NtpTime::now();
        struct tm tmS;
        localtime_r(&t, &tmS);
        if ((uint32_t)(tmS.tm_hour * 60 + tmS.tm_min) != lastClockMin) {
          showClock();
        }
      }
    }

    static uint32_t lastPaint = 0;
    if (now - lastPaint >= 50) {
      lastPaint = now;
      paintRealtimeTrains(now);
    }

    DisplayDriver::update();
  }

  void handleEncoderDelta(int delta) {
    g_lastEncoderMs = millis();
    bannerActive = false;

    if (g_nbTrainLeds == 0) {
      g_selectedTrainIdx = -1;
      showSelectedTrainBanner();
      return;
    }

    int next;
    if (g_selectedTrainIdx < 0) {
      next = (delta >= 0) ? 0 : ((int)g_nbTrainLeds - 1);
    } else {
      next = (int)g_selectedTrainIdx + delta;
    }
    next = ((next % (int)g_nbTrainLeds) + (int)g_nbTrainLeds) % (int)g_nbTrainLeds;
    g_selectedTrainIdx = (int8_t)next;

    showSelectedTrainBanner();
    paintRealtimeTrains(millis());
  }

  void handleEncoderButton() {
    // Un clic court sur l'encodeur quitte la sélection du train et réaffiche l'heure
    if (g_selectedTrainIdx >= 0) {
      g_selectedTrainIdx = -1;
      lastClockMin = 0xFFFFFFFF;
      if (NtpTime::isSynced()) showClock();
      paintRealtimeTrains(millis());
    }
  }
}
