#include "calib_tool.h"
#include "led_driver.h"
#include "display_driver.h"
#include "colors.h"
#include "stations.h"
#include "shared_state.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

namespace {
  struct CalibGareEntry {
    bool used;
    char code[5];
    char apiName[32];
    bool pivot;
  };

  // amont/aval ne peuvent PAS etre deduits de l'ordre des index sur le bus :
  // le cablage physique ne suit pas forcement le trace de la voie (deux LEDs
  // consecutives en index peuvent appartenir a deux lignes/troncons differents).
  // Ils sont donc resolus en suivant le CHEMIN reellement parcouru a l'encodeur
  // pendant qu'une ligne est armee (cf. ArmedLine / applyArmedCodesToCurrent) et
  // stockes directement sur chaque entree. "" = pas encore resolu ("?" a l'affichage).
  //
  // amont/aval sont stockes PAR LIGNE (et non une seule fois par LED) : sur un
  // embranchement (ex. une LED partagee par deux lignes qui divergent juste
  // apres), deux lignes qui traversent la meme LED physique peuvent legitimement
  // avoir des gares encadrantes differentes.
  struct CalibTronconEntry {
    bool used;
    char ligneCodes[4][6];
    char gareAmontCodes[4][5];
    char gareAvalCodes[4][5];
    uint8_t nbLignes;
  };

  CalibGareEntry calibGares[NUM_BUSES][MAX_LEDS_PER_BUS];
  CalibTronconEntry calibTroncons[NUM_BUSES][MAX_LEDS_PER_BUS];

  // Une ligne "armee" (LINE <code> START) tague automatiquement chaque LED
  // traversee par l'encodeur (sauf les gares) et resout amont/aval au fil de
  // l'eau : `pending` accumule les LEDs de troncon taguees depuis la derniere
  // gare croisee ; des qu'une nouvelle gare est atteinte, elle devient l'aval
  // de tout ce qui est en attente, et l'amont des prochaines LEDs taguees.
  const uint8_t PENDING_MAX = 64;
  struct PendingLed { uint8_t bus; uint16_t index; };
  struct ArmedLine {
    bool active;
    char code[6];
    char lastGare[5];   // "" = aucune gare croisee depuis l'armement
    uint8_t nbPending;
    PendingLed pending[PENDING_MAX];
  };

  ArmedLine armedLines[4];

  char reviewCode[6] = ""; // vide = pas de mode revue actif

  const uint32_t LONG_PRESS_MS = 600;
  const size_t MAX_EXPORT_GARES = 64;

  uint8_t busActif = 0;
  uint16_t ledPos = 0;
  bool modeToutAllume = false;
  uint8_t couleurIdx = 0;
  int lastEncA = HIGH;
  uint32_t lastStepCalib = 0;

  bool encBtnDown = false; uint32_t encBtnDownAt = 0; bool encBtnLongFired = false;
  bool navBtnDown = false; uint32_t navBtnDownAt = 0; bool navBtnLongFired = false;

  char lineBuf[160];
  size_t lineLen = 0;

  WebServer* calibServer = nullptr;

  void loadFromCompiledStations() {
    memset(calibGares, 0, sizeof(calibGares));
    memset(calibTroncons, 0, sizeof(calibTroncons));
    for (uint8_t g = 0; g < NB_GARES; g++) {
      const Gare& ga = GARES[g];
      if (ga.led.bus >= NUM_BUSES || ga.led.index >= MAX_LEDS_PER_BUS) continue;
      CalibGareEntry& dst = calibGares[ga.led.bus][ga.led.index];
      dst.used = true;
      strlcpy(dst.code, ga.nomCourt, sizeof(dst.code));
      strlcpy(dst.apiName, ga.nomAPI, sizeof(dst.apiName));
      dst.pivot = ga.pivotAPI;
    }
    for (uint8_t t = 0; t < NB_TRONCONS; t++) {
      const Troncon& tr = TRONCONS[t];
      if (tr.led.bus >= NUM_BUSES || tr.led.index >= MAX_LEDS_PER_BUS) continue;
      if (calibGares[tr.led.bus][tr.led.index].used) continue;
      CalibTronconEntry& dst = calibTroncons[tr.led.bus][tr.led.index];
      dst.used = (tr.nbLignes > 0);
      dst.nbLignes = (tr.nbLignes <= 4) ? tr.nbLignes : 4;
      for (uint8_t l = 0; l < dst.nbLignes; l++) {
        uint8_t lid = (uint8_t)tr.lignes[l].ligne;
        strlcpy(dst.ligneCodes[l], (lid < (uint8_t)LigneId::COUNT) ? LIGNE_NOMS[lid] : "R1", sizeof(dst.ligneCodes[l]));
        uint8_t am = tr.lignes[l].idxGareAmont;
        uint8_t av = tr.lignes[l].idxGareAval;
        strlcpy(dst.gareAmontCodes[l], (am < NB_GARES) ? GARES[am].nomCourt : "", sizeof(dst.gareAmontCodes[l]));
        strlcpy(dst.gareAvalCodes[l],  (av < NB_GARES) ? GARES[av].nomCourt : "", sizeof(dst.gareAvalCodes[l]));
      }
    }
  }

  String buildColorsJsonString() {
    String s = "{\"altMs\":";
    s += String(g_ligneAlternationPeriodMs);
    s += ",\"lines\":[";
    for (size_t i = 0; i < (size_t)LigneId::COUNT; i++) {
      if (i > 0) s += ",";
      const LigneSpec& sp = LIGNE_SPECS[i];
      char buf[128];
      snprintf(buf, sizeof(buf),
               "{\"code\":\"%s\",\"r1\":%u,\"g1\":%u,\"b1\":%u,\"r2\":%u,\"g2\":%u,\"b2\":%u,\"alt\":%s}",
               LIGNE_NOMS[i],
               sp.primaire.r, sp.primaire.g, sp.primaire.b,
               sp.secondaire.r, sp.secondaire.g, sp.secondaire.b,
               sp.alterne ? "true" : "false");
      s += buf;
    }
    s += "]}";
    return s;
  }

  String buildSettingsJsonString() {
    String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    char buf[384];
    snprintf(buf, sizeof(buf),
             "{\"pollFastMs\":%u,\"pollNormalMs\":%u,\"apiLimit\":%u,"
             "\"stationWindowSec\":%u,\"gareLookaheadSec\":%u,\"gareNoTrainMode\":%u,"
             "\"blinkBaseHalfMs\":%u,\"blinkStepPerMinMs\":%u,\"scrollSpeedMs\":%u,"
             "\"travelScalePct\":%u,\"ledBright\":%u,\"dispBright\":%u,\"ip\":\"%s\"}",
             g_settings.pollFastMs, g_settings.pollNormalMs, g_settings.apiLimit,
             g_settings.stationWindowSec, g_settings.gareLookaheadSec, g_settings.gareNoTrainMode,
             g_settings.blinkBaseHalfMs, g_settings.blinkStepPerMinMs, g_settings.scrollSpeedMs,
             g_settings.travelScalePct, LedDriver::getBrightnessPercent(), DisplayDriver::getBrightness(),
             ip.c_str());
    return String(buf);
  }

  void underscoreToSpace(char* s) {
    for (char* p = s; *p; p++) if (*p == '_') *p = ' ';
  }

  void jsonEscape(const char* in, char* out, size_t outSize) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o < outSize - 2; i++) {
      char c = in[i];
      if (c == '"' || c == '\\') out[o++] = '\\';
      out[o++] = c;
    }
    out[o] = '\0';
  }

  // Affichage "?" pour un code de gare pas encore resolu (chaine vide en interne).
  const char* fmtGare(const char* code) {
    return (code[0] == '\0') ? "?" : code;
  }

  void countTags(uint16_t& gares, uint16_t& troncons) {
    gares = 0; troncons = 0;
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        if (calibGares[bus][i].used) gares++;
        if (calibTroncons[bus][i].used) troncons++;
      }
    }
  }

  bool ligneIdFromCode(const char* code, LigneId& out) {
    for (size_t i = 0; i < (size_t)LigneId::COUNT; i++) {
      if (strcasecmp(LIGNE_NOMS[i], code) == 0) { out = (LigneId)i; return true; }
    }
    return false;
  }

  int findArmedIndex(const char* code) {
    for (int i = 0; i < 4; i++) if (armedLines[i].active && strcasecmp(armedLines[i].code, code) == 0) return i;
    return -1;
  }

  // Index de <code> dans ligneCodes[] (et donc dans gareAmontCodes[]/gareAvalCodes[]
  // au meme indice) pour une entree de troncon donnee, ou -1 si absente.
  int findLigneSlot(const CalibTronconEntry& e, const char* code) {
    for (int j = 0; j < e.nbLignes; j++) if (strcasecmp(e.ligneCodes[j], code) == 0) return j;
    return -1;
  }

  // Une gare vient d'etre atteinte (code gareCode) : pour chaque ligne armee,
  // tout ce qui est "en attente" depuis la derniere gare obtient cette gare
  // comme aval, puis la file d'attente est videe et lastGare mis a jour.
  void resolveGareCrossing(const char* gareCode) {
    for (int a = 0; a < 4; a++) {
      ArmedLine& al = armedLines[a];
      if (!al.active) continue;
      for (uint8_t p = 0; p < al.nbPending; p++) {
        CalibTronconEntry& e = calibTroncons[al.pending[p].bus][al.pending[p].index];
        int slot = findLigneSlot(e, al.code);
        if (slot >= 0 && e.gareAvalCodes[slot][0] == '\0') strlcpy(e.gareAvalCodes[slot], gareCode, sizeof(e.gareAvalCodes[slot]));
      }
      al.nbPending = 0;
      strlcpy(al.lastGare, gareCode, sizeof(al.lastGare));
    }
  }

  void applyArmedCodesToCurrent() {
    bool anyArmed = false;
    for (int a = 0; a < 4; a++) if (armedLines[a].active) anyArmed = true;
    if (!anyArmed) return;

    if (calibGares[busActif][ledPos].used) {
      // LED de gare : resout l'aval de tout ce qui etait en attente pour chaque
      // ligne armee, sans jamais taguer la gare elle-meme comme troncon.
      resolveGareCrossing(calibGares[busActif][ledPos].code);
      return;
    }

    CalibTronconEntry& e = calibTroncons[busActif][ledPos];
    for (int a = 0; a < 4; a++) {
      ArmedLine& al = armedLines[a];
      if (!al.active) continue;

      int slot = findLigneSlot(e, al.code);
      if (slot < 0 && e.nbLignes < 4) {
        slot = e.nbLignes;
        strlcpy(e.ligneCodes[slot], al.code, sizeof(e.ligneCodes[slot]));
        e.gareAmontCodes[slot][0] = '\0';
        e.gareAvalCodes[slot][0] = '\0';
        e.nbLignes++;
      }
      e.used = (e.nbLignes > 0);
      if (slot < 0) continue; // 4 lignes deja presentes sur cette LED, pas de place pour celle-ci

      // Idempotent vis-a-vis des allers-retours a l'encodeur : on ne pose
      // l'amont que s'il est encore vide, pour ne jamais ecraser une valeur
      // deja resolue par un passage precedent.
      if (e.gareAmontCodes[slot][0] == '\0' && al.lastGare[0] != '\0') {
        strlcpy(e.gareAmontCodes[slot], al.lastGare, sizeof(e.gareAmontCodes[slot]));
      }

      // Idem pour l'aval : on ne remet cette LED en attente que si elle n'a
      // pas deja un aval connu, et qu'elle n'y est pas deja.
      if (e.gareAvalCodes[slot][0] == '\0') {
        bool inPending = false;
        for (uint8_t p = 0; p < al.nbPending; p++) {
          if (al.pending[p].bus == busActif && al.pending[p].index == ledPos) inPending = true;
        }
        if (!inPending) {
          if (al.nbPending < PENDING_MAX) {
            al.pending[al.nbPending].bus = busActif;
            al.pending[al.nbPending].index = ledPos;
            al.nbPending++;
          } else {
            Serial.printf("ATTENTION: file d'attente pleine pour %s, aval non garanti pour bus %d index %d\n", al.code, busActif, ledPos);
          }
        }
      }
    }
  }

  void lineArm(const char* code) {
    if (findArmedIndex(code) >= 0) { Serial.println("Erreur: deja armee."); return; }
    int slot = -1;
    for (int i = 0; i < 4; i++) if (!armedLines[i].active) { slot = i; break; }
    if (slot < 0) { Serial.println("Erreur: 4 lignes armees au maximum."); return; }
    ArmedLine& al = armedLines[slot];
    al.active = true;
    strlcpy(al.code, code, sizeof(al.code));
    al.lastGare[0] = '\0';
    al.nbPending = 0;
    // Si on demarre deja positionne sur une gare taguee, elle sert d'amont de depart.
    if (calibGares[busActif][ledPos].used) strlcpy(al.lastGare, calibGares[busActif][ledPos].code, sizeof(al.lastGare));
    applyArmedCodesToCurrent();
    Serial.printf("OK: %s armee (peinture automatique en se deplacant, amont/aval resolus en suivant le trajet)\n", code);
  }

  void lineDisarm(const char* code) {
    int idx = findArmedIndex(code);
    if (idx < 0) { Serial.println("Erreur: cette ligne n'est pas armee."); return; }
    if (armedLines[idx].nbPending > 0) {
      Serial.printf("Avertissement: %u LED(s) de %s restent sans aval connu (pas de gare atteinte apres elles).\n", armedLines[idx].nbPending, code);
    }
    armedLines[idx].active = false;
    Serial.printf("OK: %s desarmee\n", code);
  }

  void lineAdd(const char* code) {
    CalibTronconEntry& e = calibTroncons[busActif][ledPos];
    if (findLigneSlot(e, code) >= 0) { Serial.println("Deja presente sur cette LED."); return; }
    if (e.nbLignes >= 4) { Serial.println("Erreur: maximum 4 lignes par LED."); return; }
    uint8_t slot = e.nbLignes;
    strlcpy(e.ligneCodes[slot], code, sizeof(e.ligneCodes[slot]));
    e.gareAmontCodes[slot][0] = '\0';
    e.gareAvalCodes[slot][0] = '\0';
    e.nbLignes++;
    e.used = true;
    Serial.printf("OK: %s ajoutee a bus %d index %d (amont/aval a renseigner via SETBORDERS %s <amont> <aval>)\n", code, busActif, ledPos, code);
  }

  void lineDel(const char* code) {
    CalibTronconEntry& e = calibTroncons[busActif][ledPos];
    int j = findLigneSlot(e, code);
    if (j < 0) { Serial.println("Erreur: cette ligne n'est pas presente sur la LED courante."); return; }
    for (int k = j; k < e.nbLignes - 1; k++) {
      strlcpy(e.ligneCodes[k], e.ligneCodes[k + 1], sizeof(e.ligneCodes[k]));
      strlcpy(e.gareAmontCodes[k], e.gareAmontCodes[k + 1], sizeof(e.gareAmontCodes[k]));
      strlcpy(e.gareAvalCodes[k], e.gareAvalCodes[k + 1], sizeof(e.gareAvalCodes[k]));
    }
    e.nbLignes--;
    if (e.nbLignes == 0) e.used = false;
    Serial.printf("OK: %s retiree de bus %d index %d\n", code, busActif, ledPos);
  }

  uint16_t countForLigne(const char* code) {
    uint16_t n = 0;
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibTronconEntry& t = calibTroncons[bus][i];
        for (int j = 0; j < t.nbLignes; j++) {
          if (strcasecmp(t.ligneCodes[j], code) == 0) { n++; break; }
        }
      }
    }
    return n;
  }

  void renderReview() {
    LigneId id;
    bool known = ligneIdFromCode(reviewCode, id);
    LineColor c = known ? colorForLigne(id, millis()) : LineColor{255, 255, 255};
    for (uint8_t bus = 0; bus < NUM_BUSES; bus++) {
      LedDriver::clearBus(bus);
      for (uint16_t i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibTronconEntry& t = calibTroncons[bus][i];
        for (int j = 0; j < t.nbLignes; j++) {
          if (strcasecmp(t.ligneCodes[j], reviewCode) == 0) { LedDriver::setPixel(bus, i, c.r, c.g, c.b); break; }
        }
      }
      if (bus == busActif) LedDriver::setPixel(bus, ledPos, 255, 255, 255); // curseur = reperage, meme non tague
      LedDriver::show(bus);
    }
    uint16_t n = countForLigne(reviewCode);
    char buf[5]; snprintf(buf, sizeof(buf), "%4u", n < 10000 ? n : 9999);
    DisplayDriver::showText(buf);
    Serial.printf("REVIEW %s: %u LED taguees | curseur en blanc: bus %d index %d\n", reviewCode, n, busActif, ledPos);
  }

  void refreshPreview() {
    if (reviewCode[0] != '\0') { renderReview(); return; }
    uint8_t bright = LedDriver::getBrightness();
    uint8_t r = (couleurIdx == 0 || couleurIdx == 3) ? bright : 0;
    uint8_t g = (couleurIdx == 1 || couleurIdx == 3) ? bright : 0;
    uint8_t b = (couleurIdx == 2 || couleurIdx == 3) ? bright : 0;
    for (uint8_t bus = 0; bus < NUM_BUSES; bus++) {
      LedDriver::clearBus(bus);
      if (bus == busActif) {
        if (modeToutAllume) {
          for (uint16_t i = 0; i < MAX_LEDS_PER_BUS; i++) LedDriver::setPixel(bus, i, r, g, b);
        } else {
          LedDriver::setPixel(bus, ledPos, r, g, b);
        }
      }
      LedDriver::show(bus);
    }
    char buf[5];
    if (modeToutAllume) snprintf(buf, sizeof(buf), "%dALL", busActif);
    else snprintf(buf, sizeof(buf), "%d%03d", busActif, ledPos);
    DisplayDriver::showText(buf);
    Serial.printf("Bus %d | LED %03d | Ecran: [%s]\n", busActif, ledPos, buf);
  }

  void cmdStatus() {
    uint16_t nbG, nbT; countTags(nbG, nbT);
    Serial.printf("Position: bus %d, index %d\n", busActif, ledPos);
    CalibGareEntry& g = calibGares[busActif][ledPos];
    CalibTronconEntry& t = calibTroncons[busActif][ledPos];
    if (g.used) Serial.printf("  GARE: code=%s api=%s pivot=%d\n", g.code, g.apiName, g.pivot);
    if (t.used) {
      Serial.println("  TRONCON:");
      for (int j = 0; j < t.nbLignes; j++) {
        Serial.printf("    %s: amont=%s aval=%s\n", t.ligneCodes[j], fmtGare(t.gareAmontCodes[j]), fmtGare(t.gareAvalCodes[j]));
      }
    }
    if (!g.used && !t.used) Serial.println("  (non tague)");
    Serial.printf("Total: %u gares, %u troncons tagues.\n", nbG, nbT);
  }

  void listStations() {
    Serial.println("code,api_name,pivot,bus,index");
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibGareEntry& g = calibGares[bus][i];
        if (g.used) Serial.printf("%s,%s,%d,%d,%d\n", g.code, g.apiName, g.pivot, bus, i);
      }
    }
  }

  void listSegments(const char* filterCode) {
    Serial.println("ligne,gare_amont,gare_aval,bus,index");
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibTronconEntry& t = calibTroncons[bus][i];
        if (!t.used) continue;
        for (int j = 0; j < t.nbLignes; j++) {
          if (filterCode && strcasecmp(t.ligneCodes[j], filterCode) != 0) continue;
          Serial.printf("%s,%s,%s,%d,%d\n", t.ligneCodes[j], fmtGare(t.gareAmontCodes[j]), fmtGare(t.gareAvalCodes[j]), bus, i);
        }
      }
    }
  }

  void listLignesRecap() {
    Serial.println("code,count");
    for (size_t li = 0; li < (size_t)LigneId::COUNT; li++) {
      Serial.printf("%s,%u\n", LIGNE_NOMS[li], countForLigne(LIGNE_NOMS[li]));
    }
  }

  void collectUsedGares(uint16_t* busOut, uint16_t* idxOut, uint16_t* count, uint16_t maxCount) {
    *count = 0;
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        if (calibGares[bus][i].used && *count < maxCount) {
          busOut[*count] = bus; idxOut[*count] = i; (*count)++;
        }
      }
    }
  }

  int findGareExportIndex(uint16_t* busArr, uint16_t* idxArr, uint16_t count, const char* code) {
    if (code[0] == '\0') return -1;
    for (uint16_t k = 0; k < count; k++) {
      if (strcasecmp(calibGares[busArr[k]][idxArr[k]].code, code) == 0) return k;
    }
    return -1;
  }

  void exportCpp() {
    static uint16_t gBus[MAX_EXPORT_GARES], gIdx[MAX_EXPORT_GARES]; uint16_t gCount;
    collectUsedGares(gBus, gIdx, &gCount, MAX_EXPORT_GARES);

    Serial.println("// ===== AUTO-GENERE par TrainBoard CAL - a coller dans src/stations.cpp =====");
    Serial.println("const Gare GARES[] = {");
    for (uint16_t k = 0; k < gCount; k++) {
      CalibGareEntry& g = calibGares[gBus[k]][gIdx[k]];
      Serial.printf("  { \"%s\", \"%s\", {%d, %d}, %s, 0 },\n",
                    g.code, g.apiName, gBus[k], gIdx[k], g.pivot ? "true" : "false");
    }
    Serial.println("};");
    Serial.printf("const uint8_t NB_GARES = %u;\n\n", gCount);

    Serial.println("const Troncon TRONCONS[] = {");
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibTronconEntry& t = calibTroncons[bus][i];
        if (!t.used) continue;
        Serial.printf("  { {%d, %d}, {", bus, i);
        bool missing = false;
        for (uint8_t j = 0; j < t.nbLignes; j++) {
          int amontIdx = findGareExportIndex(gBus, gIdx, gCount, t.gareAmontCodes[j]);
          int avalIdx = findGareExportIndex(gBus, gIdx, gCount, t.gareAvalCodes[j]);
          if (amontIdx < 0 || avalIdx < 0) missing = true;
          Serial.printf("{LigneId::%s, %d, %d}%s", t.ligneCodes[j], amontIdx, avalIdx, (j < t.nbLignes - 1) ? ", " : "");
        }
        Serial.printf("}, %u },", t.nbLignes);
        if (missing) Serial.print("  // ATTENTION: gare amont/aval introuvable pour au moins une ligne, verifier/completer (SETBORDERS)");
        Serial.println();
      }
    }
    Serial.println("};");
    uint16_t nbG, nbT; countTags(nbG, nbT);
    Serial.printf("const uint8_t NB_TRONCONS = %u;\n", nbT);
  }

  void exportCsv() {
    Serial.println("bus,index,type,code,api_name,pivot");
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibGareEntry& g = calibGares[bus][i];
        if (g.used) Serial.printf("%d,%d,GARE,%s,%s,%d\n", bus, i, g.code, g.apiName, g.pivot ? 1 : 0);
      }
    }
    Serial.println("bus,index,type,ligne,gare_amont,gare_aval");
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibTronconEntry& t = calibTroncons[bus][i];
        if (!t.used) continue;
        for (uint8_t j = 0; j < t.nbLignes; j++) {
          Serial.printf("%d,%d,TRONCON,%s,%s,%s\n", bus, i, t.ligneCodes[j], fmtGare(t.gareAmontCodes[j]), fmtGare(t.gareAvalCodes[j]));
        }
      }
    }
  }

  // ----- Sorties JSON pour outils externes (interface web de calibration) -----
  // Prefixees par un marqueur @XXX@ pour que le parseur cote navigateur distingue
  // sans ambiguite une ligne JSON des lignes texte humaines (logs encodeur, "OK: ...", etc.)
  // qui continuent de s'imprimer normalement sur Serial.

  void printTagJson(uint8_t bus, uint16_t idx) {
    CalibGareEntry& g = calibGares[bus][idx];
    CalibTronconEntry& t = calibTroncons[bus][idx];
    if (g.used) {
      char esc[64]; jsonEscape(g.apiName, esc, sizeof(esc));
      Serial.printf("{\"type\":\"GARE\",\"code\":\"%s\",\"api\":\"%s\",\"pivot\":%s}",
                    g.code, esc, g.pivot ? "true" : "false");
    } else if (t.used) {
      Serial.print("{\"type\":\"TRONCON\",\"lignes\":[");
      for (int j = 0; j < t.nbLignes; j++) {
        Serial.printf("{\"code\":\"%s\",\"amont\":\"%s\",\"aval\":\"%s\"}%s",
                      t.ligneCodes[j], fmtGare(t.gareAmontCodes[j]), fmtGare(t.gareAvalCodes[j]), (j < t.nbLignes - 1) ? "," : "");
      }
      Serial.print("]}");
    } else {
      Serial.print("{\"type\":\"NONE\"}");
    }
  }

  void cmdState() {
    uint16_t nbG, nbT; countTags(nbG, nbT);
    Serial.printf("@STATE@{\"bus\":%d,\"index\":%d,\"numBuses\":%d,\"ledsPerBus\":%d,\"review\":\"%s\",\"brightness\":%u,\"tag\":",
                  busActif, ledPos, NUM_BUSES, MAX_LEDS_PER_BUS, reviewCode, LedDriver::getBrightness());
    printTagJson(busActif, ledPos);
    Serial.printf(",\"totals\":{\"gares\":%u,\"troncons\":%u}}\n", nbG, nbT);
  }

  void listJson() {
    Serial.print("@LISTJSON@{\"gares\":[");
    bool first = true;
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibGareEntry& g = calibGares[bus][i];
        if (!g.used) continue;
        if (!first) Serial.print(",");
        first = false;
        char esc[64]; jsonEscape(g.apiName, esc, sizeof(esc));
        Serial.printf("{\"bus\":%d,\"index\":%d,\"code\":\"%s\",\"api\":\"%s\",\"pivot\":%s}",
                      bus, i, g.code, esc, g.pivot ? "true" : "false");
      }
    }
    Serial.print("],\"troncons\":[");
    first = true;
    for (int bus = 0; bus < NUM_BUSES; bus++) {
      for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
        CalibTronconEntry& t = calibTroncons[bus][i];
        if (!t.used) continue;
        if (!first) Serial.print(",");
        first = false;
        Serial.printf("{\"bus\":%d,\"index\":%d,\"lignes\":[", bus, i);
        for (int j = 0; j < t.nbLignes; j++) {
          Serial.printf("{\"code\":\"%s\",\"amont\":\"%s\",\"aval\":\"%s\"}%s",
                        t.ligneCodes[j], fmtGare(t.gareAmontCodes[j]), fmtGare(t.gareAvalCodes[j]), (j < t.nbLignes - 1) ? "," : "");
        }
        Serial.print("]}");
      }
    }
    Serial.println("]}");
  }

  void cmdSessions() {
    Serial.print("@SESSIONS@{\"active\":[");
    bool first = true;
    for (int i = 0; i < 4; i++) {
      if (!armedLines[i].active) continue;
      if (!first) Serial.print(",");
      first = false;
      Serial.printf("\"%s\"", armedLines[i].code);
    }
    Serial.println("]}");
  }

  void printHelp() {
    Serial.println("=== TrainBoard CAL - commandes disponibles ===");
    Serial.println("HELP                                   - cette liste");
    Serial.println("STATUS / S                             - position courante + tag + totaux");
    Serial.println("STATION <code> <api_name> [PIVOT]       - tague la position courante comme gare");
    Serial.println("UNTAG                                   - retire le tag de la position courante");
    Serial.println("LINE <code> START                       - arme cette ligne : chaque deplacement d'index la tague automatiquement (gares ignorees),");
    Serial.println("                                           amont/aval resolus en suivant le trajet reel (pas l'ordre des index sur le bus)");
    Serial.println("LINE <code> STOP                        - desarme cette ligne");
    Serial.println("LINE <code> ADD                         - tague uniquement la LED courante avec cette ligne (amont/aval a regler via SETBORDERS)");
    Serial.println("LINE <code> DEL                         - retire cette ligne de la LED courante");
    Serial.println("SETBORDERS <ligne> <amont> <aval>       - force amont/aval de cette ligne sur la LED courante (utiliser ? pour effacer),");
    Serial.println("                                           la ligne doit deja etre presente sur la LED (LINE <code> ADD d'abord). Sert a");
    Serial.println("                                           corriger une erreur ou a tagger un embranchement ou plusieurs lignes qui traversent");
    Serial.println("                                           la meme LED divergent vers des gares differentes.");
    Serial.println("BRIGHTNESS [0-255]                      - sans argument: affiche la luminosite actuelle ; avec argument: la regle (RAM, non persiste)");
    Serial.println("REVIEW <code> / REVIEW OFF              - allume sur le PCB toutes les LED taguees de cette ligne (verif visuelle)");
    Serial.println("LIST [STATIONS|SEGMENTS|JSON|LIGNES|LINE <code>] - dump de verification (LIGNES = recap par ligne)");
    Serial.println("GOTO <bus> <index>                      - change la selection courante");
    Serial.println("EXPORT CPP                              - exporte le C++ pour stations.cpp");
    Serial.println("EXPORT CSV                              - exporte un dump CSV plat");
    Serial.println("CLEAR ALL CONFIRM                       - efface tout le tagging (irreversible)");
    Serial.println("STATE / LIST JSON / SESSIONS            - sorties JSON pour outils externes (interface web)");
    Serial.println();
    Serial.println("Note: le cablage ne suit pas forcement l'ordre physique de la voie. Amont/aval ne sont donc PAS devines");
    Serial.println("depuis l'index des LEDs : ils sont resolus en suivant le trajet reellement parcouru a l'encodeur pendant");
    Serial.println("qu'une ligne est armee (chaque gare croisee cloture l'aval des LED en attente et devient l'amont des suivantes).");
    Serial.println("Encodeur: rotation = deplace index | clic court = change de bus | clic long = aperçu bus entier");
    Serial.println("BNav2: appui court = change couleur d'apercu | appui long = raccourci EXPORT CSV");
    Serial.println("Espace dans un nom de gare API: utiliser un underscore (Renens_VD -> \"Renens VD\")");
  }

  void processLine(char* line) {
    char* cmd = strtok(line, " ");
    if (!cmd) return;

    if (strcasecmp(cmd, "HELP") == 0) {
      printHelp();
    } else if (strcasecmp(cmd, "STATUS") == 0 || strcasecmp(cmd, "S") == 0) {
      cmdStatus();
    } else if (strcasecmp(cmd, "STATE") == 0) {
      cmdState();
    } else if (strcasecmp(cmd, "SESSIONS") == 0) {
      cmdSessions();
    } else if (strcasecmp(cmd, "STATION") == 0) {
      char* code = strtok(NULL, " ");
      char* apiName = strtok(NULL, " ");
      char* flag = strtok(NULL, " ");
      if (!code || !apiName) { Serial.println("Usage: STATION <code> <api_name> [PIVOT]"); return; }
      underscoreToSpace(apiName);
      CalibGareEntry& g = calibGares[busActif][ledPos];
      g.used = true;
      strlcpy(g.code, code, sizeof(g.code));
      strlcpy(g.apiName, apiName, sizeof(g.apiName));
      g.pivot = flag && strcasecmp(flag, "PIVOT") == 0;
      // une LED est soit une gare, soit un troncon, jamais les deux
      calibTroncons[busActif][ledPos].used = false;
      calibTroncons[busActif][ledPos].nbLignes = 0;
      // Si des lignes etaient armees et deja en attente avant que cette LED ne
      // soit declaree gare, elles doivent etre resolues maintenant (sinon leur
      // aval resterait "?" alors qu'une gare vient d'etre posee ici meme).
      resolveGareCrossing(g.code);
      Serial.printf("OK: GARE %s (%s)%s taguee sur bus %d index %d\n", g.code, g.apiName, g.pivot ? " [PIVOT]" : "", busActif, ledPos);
    } else if (strcasecmp(cmd, "UNTAG") == 0) {
      calibGares[busActif][ledPos].used = false;
      calibTroncons[busActif][ledPos].used = false;
      calibTroncons[busActif][ledPos].nbLignes = 0;
      Serial.printf("OK: tag retire sur bus %d index %d\n", busActif, ledPos);
    } else if (strcasecmp(cmd, "SETBORDERS") == 0) {
      char* ligne = strtok(NULL, " ");
      char* amont = strtok(NULL, " ");
      char* aval = strtok(NULL, " ");
      if (!ligne || !amont || !aval) { Serial.println("Usage: SETBORDERS <ligne> <amont>|? <aval>|?"); return; }
      CalibTronconEntry& e = calibTroncons[busActif][ledPos];
      int slot = findLigneSlot(e, ligne);
      if (slot < 0) { Serial.printf("Erreur: %s n'est pas presente sur cette LED (utiliser LINE %s ADD d'abord).\n", ligne, ligne); return; }
      strlcpy(e.gareAmontCodes[slot], strcmp(amont, "?") == 0 ? "" : amont, sizeof(e.gareAmontCodes[slot]));
      strlcpy(e.gareAvalCodes[slot], strcmp(aval, "?") == 0 ? "" : aval, sizeof(e.gareAvalCodes[slot]));
      Serial.printf("OK: bordures forcees sur bus %d index %d [%s] -> amont=%s aval=%s\n", busActif, ledPos, e.ligneCodes[slot], fmtGare(e.gareAmontCodes[slot]), fmtGare(e.gareAvalCodes[slot]));
    } else if (strcasecmp(cmd, "BRIGHTNESS") == 0) {
      char* arg = strtok(NULL, " ");
      if (!arg) {
        Serial.printf("Luminosite actuelle: %u/255\n", LedDriver::getBrightness());
        return;
      }
      int v = atoi(arg);
      if (v < 0 || v > 255) { Serial.println("Usage: BRIGHTNESS <0-255>"); return; }
      LedDriver::setBrightness((uint8_t)v);
      Serial.printf("OK: luminosite reglee a %d/255\n", v);
      refreshPreview();
    } else if (strcasecmp(cmd, "REVIEW") == 0) {
      char* arg = strtok(NULL, " ");
      if (!arg) { Serial.println("Usage: REVIEW <code>|OFF"); return; }
      if (strcasecmp(arg, "OFF") == 0) {
        reviewCode[0] = '\0';
        g_studioLedOverride.store(false);
        Serial.println("OK: mode revue desactive.");
      } else {
        strlcpy(reviewCode, arg, sizeof(reviewCode));
        g_studioLedOverride.store(true);
        Serial.printf("OK: mode revue active pour %s\n", reviewCode);
        refreshPreview();
      }
      return;
    } else if (strcasecmp(cmd, "LINE") == 0) {
      char* code = strtok(NULL, " ");
      char* verb = strtok(NULL, " ");
      if (!code || !verb) { Serial.println("Usage: LINE <code> START|STOP|ADD|DEL"); return; }
      if (strcasecmp(verb, "START") == 0) {
        lineArm(code);
      } else if (strcasecmp(verb, "STOP") == 0) {
        lineDisarm(code);
      } else if (strcasecmp(verb, "ADD") == 0) {
        lineAdd(code);
      } else if (strcasecmp(verb, "DEL") == 0) {
        lineDel(code);
      } else {
        Serial.println("Usage: LINE <code> START|STOP|ADD|DEL");
      }
    } else if (strcasecmp(cmd, "LIST") == 0) {
      char* sub = strtok(NULL, " ");
      if (!sub) { listStations(); listSegments(nullptr); }
      else if (strcasecmp(sub, "STATIONS") == 0) listStations();
      else if (strcasecmp(sub, "SEGMENTS") == 0) listSegments(nullptr);
      else if (strcasecmp(sub, "JSON") == 0) listJson();
      else if (strcasecmp(sub, "LIGNES") == 0) listLignesRecap();
      else if (strcasecmp(sub, "LINE") == 0) {
        char* code = strtok(NULL, " ");
        if (!code) { Serial.println("Usage: LIST LINE <code>"); return; }
        listSegments(code);
      } else {
        Serial.println("Usage: LIST [STATIONS|SEGMENTS|JSON|LIGNES|LINE <code>]");
      }
    } else if (strcasecmp(cmd, "GOTO") == 0) {
      char* busStr = strtok(NULL, " ");
      char* idxStr = strtok(NULL, " ");
      if (!busStr || !idxStr) { Serial.println("Usage: GOTO <bus> <index>"); return; }
      int bus = atoi(busStr);
      int idx = atoi(idxStr);
      if (bus < 0 || bus >= NUM_BUSES || idx < 0 || idx >= MAX_LEDS_PER_BUS) { Serial.println("Erreur: hors limites."); return; }
      busActif = (uint8_t)bus; ledPos = (uint16_t)idx;
      g_studioLedOverride.store(true);
      applyArmedCodesToCurrent();
      refreshPreview();
    } else if (strcasecmp(cmd, "EXPORT") == 0) {
      char* sub = strtok(NULL, " ");
      if (sub && strcasecmp(sub, "CPP") == 0) { Serial.println("@EXPORTSTART@"); exportCpp(); Serial.println("@EXPORTEND@"); }
      else if (sub && strcasecmp(sub, "CSV") == 0) { Serial.println("@EXPORTSTART@"); exportCsv(); Serial.println("@EXPORTEND@"); }
      else Serial.println("Usage: EXPORT CPP|CSV");
    } else if (strcasecmp(cmd, "CLEAR") == 0) {
      char* a = strtok(NULL, " ");
      char* b = strtok(NULL, " ");
      if (a && b && strcasecmp(a, "ALL") == 0 && strcasecmp(b, "CONFIRM") == 0) {
        memset(calibGares, 0, sizeof(calibGares));
        memset(calibTroncons, 0, sizeof(calibTroncons));
        memset(armedLines, 0, sizeof(armedLines));
        reviewCode[0] = '\0';
        Serial.println("OK: toutes les donnees de calibration effacees.");
      } else {
        Serial.println("Usage: CLEAR ALL CONFIRM  (effacement total, action irreversible)");
      }
    } else if (strcasecmp(cmd, "LOADDEFAULT") == 0) {
      loadFromCompiledStations();
      Serial.println("OK: gares et troncons recharges depuis stations.cpp.");
      refreshPreview();
    } else if (strcasecmp(cmd, "COLORSJSON") == 0) {
      Serial.print("@COLORS@");
      Serial.println(buildColorsJsonString());
    } else if (strcasecmp(cmd, "COLOR") == 0) {
      char* code = strtok(NULL, " ");
      char* r1s  = strtok(NULL, " ");
      char* g1s  = strtok(NULL, " ");
      char* b1s  = strtok(NULL, " ");
      char* r2s  = strtok(NULL, " ");
      char* g2s  = strtok(NULL, " ");
      char* b2s  = strtok(NULL, " ");
      char* alts = strtok(NULL, " ");
      LigneId id;
      if (!code || !r1s || !g1s || !b1s || !ligneIdFromCode(code, id)) {
        Serial.println("Usage: COLOR <ligne> <r1> <g1> <b1> [r2 g2 b2 0|1]");
        return;
      }
      LineColor c1{(uint8_t)atoi(r1s), (uint8_t)atoi(g1s), (uint8_t)atoi(b1s)};
      LineColor c2 = c1;
      bool alt = false;
      if (r2s && g2s && b2s) {
        c2 = {(uint8_t)atoi(r2s), (uint8_t)atoi(g2s), (uint8_t)atoi(b2s)};
        alt = (alts && atoi(alts) != 0);
      }
      Colors::setSpec(id, c1, c2, alt);
      Serial.printf("OK: couleur %s mise a jour (%u,%u,%u)\n", code, c1.r, c1.g, c1.b);
      if (reviewCode[0] != '\0') renderReview();
    } else if (strcasecmp(cmd, "ALTPERIOD") == 0) {
      char* ms = strtok(NULL, " ");
      if (ms) {
        uint32_t v = (uint32_t)atoi(ms);
        if (v >= 200 && v <= 10000) g_ligneAlternationPeriodMs = v;
        Serial.printf("OK: periode d'alternance = %u ms\n", (unsigned)g_ligneAlternationPeriodMs);
      }
    } else if (strcasecmp(cmd, "COLORSAVE") == 0) {
      Colors::saveToNvs();
      Serial.println("OK: couleurs sauvegardees en NVS.");
    } else if (strcasecmp(cmd, "COLORRESET") == 0) {
      Colors::resetDefaults();
      Serial.println("OK: couleurs reinitialisees par defaut.");
      if (reviewCode[0] != '\0') renderReview();
    } else if (strcasecmp(cmd, "SETTINGSJSON") == 0) {
      Serial.print("@SETTINGS@");
      Serial.println(buildSettingsJsonString());
    } else if (strcasecmp(cmd, "SET") == 0) {
      char* key = strtok(NULL, " ");
      char* val = strtok(NULL, " ");
      if (!key || !val) { Serial.println("Usage: SET <key> <val>"); return; }
      int v = atoi(val);
      if      (strcasecmp(key, "pollFastMs") == 0)        g_settings.pollFastMs = (uint16_t)constrain(v, 5, 5000);
      else if (strcasecmp(key, "pollNormalMs") == 0)      g_settings.pollNormalMs = (uint16_t)constrain(v, 20, 10000);
      else if (strcasecmp(key, "apiLimit") == 0)          g_settings.apiLimit = (uint8_t)constrain(v, 5, 30);
      else if (strcasecmp(key, "stationWindowSec") == 0)  g_settings.stationWindowSec = (uint16_t)constrain(v, 10, 300);
      else if (strcasecmp(key, "gareLookaheadSec") == 0)  g_settings.gareLookaheadSec = (uint16_t)constrain(v, 60, 7200);
      else if (strcasecmp(key, "gareNoTrainMode") == 0)   g_settings.gareNoTrainMode = (uint8_t)constrain(v, 0, 2);
      else if (strcasecmp(key, "blinkBaseHalfMs") == 0)   g_settings.blinkBaseHalfMs = (uint16_t)constrain(v, 150, 2000);
      else if (strcasecmp(key, "blinkStepPerMinMs") == 0) g_settings.blinkStepPerMinMs = (uint16_t)constrain(v, 10, 400);
      else if (strcasecmp(key, "scrollSpeedMs") == 0)     g_settings.scrollSpeedMs = (uint16_t)constrain(v, 60, 1000);
      else if (strcasecmp(key, "travelScalePct") == 0)    g_settings.travelScalePct = (uint16_t)constrain(v, 25, 300);
      else if (strcasecmp(key, "ledBright") == 0) {
        LedDriver::setBrightnessPercent((uint8_t)constrain(v, 0, 100));
        LedDriver::showAll();
      } else if (strcasecmp(key, "dispBright") == 0) {
        DisplayDriver::setBrightness((uint8_t)constrain(v, 0, 15));
      }
      Serial.printf("OK: %s = %d\n", key, v);
    } else if (strcasecmp(cmd, "SAVESETTINGS") == 0) {
      SharedState::saveSettings();
      LedDriver::saveBrightness();
      DisplayDriver::saveBrightness();
      Serial.println("OK: parametres sauvegardes en NVS.");
    } else if (strcasecmp(cmd, "RESETSETTINGS") == 0) {
      SharedState::resetSettings();
      Serial.println("OK: parametres reinitialises.");
    } else if (strcasecmp(cmd, "PIXEL") == 0) {
      char* bs = strtok(NULL, " ");
      char* is = strtok(NULL, " ");
      char* rs = strtok(NULL, " ");
      char* gs = strtok(NULL, " ");
      char* bls = strtok(NULL, " ");
      if (!bs || !is || !rs || !gs || !bls) return;
      uint8_t b = (uint8_t)atoi(bs);
      uint16_t idx = (uint16_t)atoi(is);
      g_studioLedOverride.store(true);
      LedDriver::setPixel(b, idx, (uint8_t)atoi(rs), (uint8_t)atoi(gs), (uint8_t)atoi(bls));
      LedDriver::show(b);
      Serial.printf("OK: PIXEL %u %u\n", b, idx);
    } else if (strcasecmp(cmd, "CLEARLEDS") == 0) {
      reviewCode[0] = '\0';
      g_studioLedOverride.store(true);
      LedDriver::clearAll();
      LedDriver::showAll();
      Serial.println("OK: LEDs eteintes.");
    } else if (strcasecmp(cmd, "REBOOT") == 0) {
      Serial.println("OK: redemarrage...");
      delay(200);
      ESP.restart();
    } else {
      Serial.println("Commande inconnue. Tapez HELP.");
    }
  }

  void sendCorsJson(int code, const String& body) {
    if (!calibServer) return;
    calibServer->sendHeader("Access-Control-Allow-Origin", "*");
    calibServer->sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    calibServer->sendHeader("Access-Control-Allow-Headers", "Content-Type");
    calibServer->send(code, "application/json; charset=utf-8", body);
  }

  void startCalibHttpServer() {
    Preferences prefs;
    prefs.begin("trainboard", true);
    String s = prefs.getString("ssid", "");
    String p = prefs.getString("pass", "");
    prefs.end();

    if (s.length() > 0) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(s.c_str(), p.c_str());
      uint32_t t0 = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
        delay(100);
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_AP);
      WiFi.softAP("TrainBoard-Calib");
      Serial.printf("Wi-Fi AP Calibration: TrainBoard-Calib | IP: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
      Serial.printf("Wi-Fi Connecte (%s) | IP Calibration: %s\n", s.c_str(), WiFi.localIP().toString().c_str());
    }

    if (!calibServer) calibServer = new WebServer(80);

    calibServer->on("/api/all", HTTP_GET, []() {
      uint16_t nbG, nbT; countTags(nbG, nbT);
      String out = "{\"bus\":" + String(busActif) +
                   ",\"index\":" + String(ledPos) +
                   ",\"review\":\"" + String(reviewCode) + "\"" +
                   ",\"totals\":{\"gares\":" + String(nbG) + ",\"troncons\":" + String(nbT) + "}" +
                   ",\"colors\":" + buildColorsJsonString() +
                   ",\"settings\":" + buildSettingsJsonString() + "}";
      sendCorsJson(200, out);
    });

    calibServer->on("/api/map", HTTP_GET, []() {
      calibServer->sendHeader("Access-Control-Allow-Origin", "*");
      calibServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
      calibServer->send(200, "application/json; charset=utf-8", "");
      String chunk = "{\"gares\":[";
      bool first = true;
      for (int bus = 0; bus < NUM_BUSES; bus++) {
        for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
          CalibGareEntry& g = calibGares[bus][i];
          if (!g.used) continue;
          if (!first) chunk += ",";
          first = false;
          char esc[64]; jsonEscape(g.apiName, esc, sizeof(esc));
          char b[160];
          snprintf(b, sizeof(b), "{\"bus\":%d,\"index\":%d,\"code\":\"%s\",\"api\":\"%s\",\"pivot\":%s}",
                   bus, i, g.code, esc, g.pivot ? "true" : "false");
          chunk += b;
          if (chunk.length() > 1024) { calibServer->sendContent(chunk); chunk = ""; }
        }
      }
      chunk += "],\"troncons\":[";
      first = true;
      for (int bus = 0; bus < NUM_BUSES; bus++) {
        for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
          CalibTronconEntry& t = calibTroncons[bus][i];
          if (!t.used) continue;
          if (!first) chunk += ",";
          first = false;
          char b[64];
          snprintf(b, sizeof(b), "{\"bus\":%d,\"index\":%d,\"lignes\":[", bus, i);
          chunk += b;
          for (int j = 0; j < t.nbLignes; j++) {
            char lb[96];
            snprintf(lb, sizeof(lb), "{\"code\":\"%s\",\"amont\":\"%s\",\"aval\":\"%s\"}%s",
                     t.ligneCodes[j], fmtGare(t.gareAmontCodes[j]), fmtGare(t.gareAvalCodes[j]),
                     (j < t.nbLignes - 1) ? "," : "");
            chunk += lb;
          }
          chunk += "]}";
          if (chunk.length() > 1024) { calibServer->sendContent(chunk); chunk = ""; }
        }
      }
      chunk += "]}";
      if (chunk.length() > 0) calibServer->sendContent(chunk);
    });

    calibServer->on("/api/cmd", HTTP_ANY, []() {
      if (calibServer->method() == HTTP_OPTIONS) {
        sendCorsJson(200, "{}");
        return;
      }
      String c = calibServer->arg("c");
      if (c.length() == 0 && calibServer->hasArg("plain")) {
        c = calibServer->arg("plain");
      }
      int start = 0;
      while (start < (int)c.length()) {
        int nl = c.indexOf('\n', start);
        if (nl < 0) nl = c.length();
        String sub = c.substring(start, nl);
        sub.trim();
        if (sub.length() > 0) {
          char buf[160];
          strlcpy(buf, sub.c_str(), sizeof(buf));
          processLine(buf);
        }
        start = nl + 1;
      }
      sendCorsJson(200, "{\"ok\":true}");
    });

    calibServer->onNotFound([]() {
      if (calibServer->method() == HTTP_OPTIONS) {
        sendCorsJson(200, "{}");
        return;
      }
      calibServer->sendHeader("Access-Control-Allow-Origin", "*");
      calibServer->send(200, "text/html; charset=utf-8",
        "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TrainBoard Studio</title></head><body style='background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:24px'>"
        "<h2>TrainBoard Calibration & Live Studio Actif</h2>"
        "<p>Serveur HTTP API prêt sur cette adresse IP. Ouvrez <code>tools/calib_web/index.html</code> et connectez-vous à cette IP (ou en USB).</p>"
        "</body></html>");
    });

    calibServer->begin();
  }

  void pollSerial() {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        lineBuf[lineLen] = '\0';
        if (lineLen > 0) processLine(lineBuf);
        lineLen = 0;
      } else if (lineLen < sizeof(lineBuf) - 1) {
        lineBuf[lineLen++] = c;
      }
    }
  }
}

namespace CalibTool {
  void begin() {
    SharedState::begin();
    loadFromCompiledStations();
    memset(armedLines, 0, sizeof(armedLines));
    reviewCode[0] = '\0';

    LedDriver::begin();
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
    pinMode(PIN_BTN_NAV2, INPUT_PULLUP);
    lastEncA = digitalRead(PIN_ENCODER_A);

    DisplayDriver::begin();
    DisplayDriver::showText("CAL ");
    startCalibHttpServer();

    Serial.println("=== TRAINBOARD CAL : STUDIO CALIBRATION / COULEURS / SETTINGS ===");
    Serial.println("Tapez HELP pour la liste des commandes.");
    refreshPreview();
  }

  void loop() {
    int curA = digitalRead(PIN_ENCODER_A);
    if (lastEncA == HIGH && curA == LOW) {
      uint32_t now = millis();
      if (now - lastStepCalib >= 25) {
        lastStepCalib = now;
        int dir = (digitalRead(PIN_ENCODER_B) == HIGH) ? 1 : -1;
        if (!modeToutAllume) {
          ledPos = (ledPos + dir + MAX_LEDS_PER_BUS) % MAX_LEDS_PER_BUS;
          applyArmedCodesToCurrent();
        } else {
          couleurIdx = (couleurIdx + dir + 4) % 4;
        }
        refreshPreview();
      }
    }
    lastEncA = curA;

    bool encRead = (digitalRead(PIN_ENCODER_BTN) == LOW);
    if (encRead && !encBtnDown) {
      encBtnDown = true; encBtnDownAt = millis(); encBtnLongFired = false;
    } else if (encRead && encBtnDown && !encBtnLongFired && (millis() - encBtnDownAt > LONG_PRESS_MS)) {
      encBtnLongFired = true;
      modeToutAllume = !modeToutAllume;
      refreshPreview();
    } else if (!encRead && encBtnDown) {
      encBtnDown = false;
      if (!encBtnLongFired) {
        busActif = (busActif + 1) % NUM_BUSES;
        applyArmedCodesToCurrent();
        refreshPreview();
      }
    }

    bool navRead = (digitalRead(PIN_BTN_NAV2) == LOW);
    if (navRead && !navBtnDown) {
      navBtnDown = true; navBtnDownAt = millis(); navBtnLongFired = false;
    } else if (navRead && navBtnDown && !navBtnLongFired && (millis() - navBtnDownAt > LONG_PRESS_MS)) {
      navBtnLongFired = true;
      Serial.println("--- EXPORT CSV (raccourci bouton nav) ---");
      Serial.println("@EXPORTSTART@");
      exportCsv();
      Serial.println("@EXPORTEND@");
    } else if (!navRead && navBtnDown) {
      navBtnDown = false;
      if (!navBtnLongFired) {
        couleurIdx = (couleurIdx + 1) % 4;
        refreshPreview();
      }
    }

    pollSerial();
    if (calibServer) calibServer->handleClient();
  }

  void run() {
    begin();
    while (true) loop();
  }

  void beginBackground() {
    loadFromCompiledStations();
    memset(armedLines, 0, sizeof(armedLines));
    reviewCode[0] = '\0';
  }

  void pollBackground() {
    pollSerial();
  }
}
