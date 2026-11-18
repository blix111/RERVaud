#include "api_client.h"
#include "json_filter.h"
#include "colors.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <strings.h>

namespace {
  // Client TLS et HTTPClient statiques réutilisés en Keep-Alive (évite ~900 ms de
  // handshake TLS à chaque gare).
  WiFiClientSecure secureClient;
  HTTPClient http;
  bool clientReady = false;

  // "2026-11-18T19:45:00+0100" -> epoch (heure locale CET/CEST via mktime)
  time_t parseIso(const char* s) {
    if (!s) return 0;
    int Y, M, D, h, m, sec;
    if (sscanf(s, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec) != 6) return 0;
    struct tm t = {};
    t.tm_year = Y - 1900; t.tm_mon = M - 1; t.tm_mday = D;
    t.tm_hour = h; t.tm_min = m; t.tm_sec = sec;
    t.tm_isdst = -1;
    return mktime(&t);
  }

  uint8_t ligneFromCode(const char* code) {
    for (uint8_t i = 0; i < (uint8_t)LigneId::COUNT; i++) {
      if (strcasecmp(code, LIGNE_NOMS[i]) == 0) return i;
    }
    return 0xFF;
  }
}

namespace ApiClient {
  bool fetchStationboard(const char* stationApiName,
                         StationboardEntry out[], uint8_t maxOut, uint8_t* outCount) {
    *outCount = 0;
    if (WiFi.status() != WL_CONNECTED) return false;

    if (!clientReady) {
      secureClient.setInsecure(); // API publique en lecture seule
      secureClient.setTimeout(5);
      http.setReuse(true);        // Réutilise la session TLS entre les requêtes
      http.setTimeout(5000);
      clientReady = true;
    }

    // Encodage minimal du nom de gare (espaces)
    char station[64];
    size_t k = 0;
    for (const char* p = stationApiName; *p && k < sizeof(station) - 4; p++) {
      if (*p == ' ') { station[k++] = '%'; station[k++] = '2'; station[k++] = '0'; }
      else station[k++] = *p;
    }
    station[k] = '\0';

    char url[256];
    snprintf(url, sizeof(url),
             "https://transport.opendata.ch/v1/stationboard?station=%s&limit=8"
             "&fields%%5B%%5D=stationboard/category&fields%%5B%%5D=stationboard/number"
             "&fields%%5B%%5D=stationboard/to&fields%%5B%%5D=stationboard/stop/departure"
             "&fields%%5B%%5D=stationboard/stop/delay",
             station);

    http.useHTTP10(true);
    if (!http.begin(secureClient, url)) return false;

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      http.end();
      return false;
    }

    JsonDocument filter = buildStationboardFilter();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();
    if (err) return false;

    uint8_t n = 0;
    for (JsonObject e : doc["stationboard"].as<JsonArray>()) {
      if (n >= maxOut) break;

      char code2[6];
      snprintf(code2, sizeof(code2), "%s%s",
               e["category"] | "", e["number"] | "");
      uint8_t lg = ligneFromCode(code2);
      if (lg == 0xFF) continue; // ligne non représentée sur la carte

      StationboardEntry& o = out[n];
      memset(&o, 0, sizeof(o));
      o.ligne = lg;
      strlcpy(o.ligneCode, code2, sizeof(o.ligneCode));
      strlcpy(o.destination, e["to"] | "", sizeof(o.destination));
      o.scheduled = parseIso(e["stop"]["departure"] | (const char*)nullptr);
      if (o.scheduled == 0) continue;
      int delay = e["stop"]["delay"] | 0;
      o.delayMin = (int16_t)delay;
      o.realtime = o.scheduled + (time_t)delay * 60;
      o.cancelled = false;
      n++;
    }
    *outCount = n;
    return true;
  }
}
