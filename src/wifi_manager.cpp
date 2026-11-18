#include "wifi_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <string.h>

namespace {
  WifiManager::State currentState = WifiManager::State::IDLE;

  char ssid[33] = "";
  char pass[65] = "";

  uint32_t connectStart = 0;
  uint32_t lastReconnect = 0;
  constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
  constexpr uint32_t RECONNECT_PERIOD_MS = 30000;

  WebServer* server = nullptr;
  DNSServer* dns = nullptr;

  const char PORTAL_HTML[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>TrainBoard</title></head><body style='font-family:sans-serif;max-width:360px;margin:30px auto'>"
    "<h2>TrainBoard - Wi-Fi</h2>"
    "<form method='POST' action='/save'>"
    "SSID<br><input name='s' maxlength='32' style='width:100%'><br><br>"
    "Mot de passe<br><input name='p' type='password' maxlength='64' style='width:100%'><br><br>"
    "<button type='submit' style='width:100%;padding:10px'>Enregistrer</button>"
    "</form></body></html>";

  void startConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    connectStart = millis();
    currentState = WifiManager::State::CONNECTING;
  }

  void startPortal() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("TrainBoard-Setup");
    if (!dns) dns = new DNSServer();
    dns->start(53, "*", WiFi.softAPIP());
    if (!server) {
      server = new WebServer(80);
      server->on("/save", HTTP_POST, []() {
        String s = server->arg("s");
        String p = server->arg("p");
        if (s.length() == 0) {
          server->send(400, "text/plain", "SSID vide");
          return;
        }
        WifiManager::saveCredentials(s.c_str(), p.c_str());
        server->send(200, "text/html; charset=utf-8",
                     "<html><body><h3>Enregistr&eacute;. Red&eacute;marrage...</h3></body></html>");
        delay(1000);
        ESP.restart();
      });
      // Toute autre URL (détection portail captif) -> formulaire
      server->onNotFound([]() { server->send(200, "text/html; charset=utf-8", PORTAL_HTML); });
    }
    server->begin();
    currentState = WifiManager::State::PORTAL_ACTIVE;
  }
}

namespace WifiManager {
  void begin() {
    Preferences prefs;
    prefs.begin("trainboard", true);
    String s = prefs.getString("ssid", "");
    String p = prefs.getString("pass", "");
    prefs.end();
    strlcpy(ssid, s.c_str(), sizeof(ssid));
    strlcpy(pass, p.c_str(), sizeof(pass));

    if (ssid[0] == '\0') startPortal();
    else startConnect();
  }

  bool isConnected() {
    return currentState == State::CONNECTED;
  }

  State state() {
    return currentState;
  }

  void loop() {
    uint32_t now = millis();
    switch (currentState) {
      case State::CONNECTING:
        if (WiFi.status() == WL_CONNECTED) {
          currentState = State::CONNECTED;
        } else if (now - connectStart > CONNECT_TIMEOUT_MS) {
          startPortal();
        }
        break;
      case State::CONNECTED:
        if (WiFi.status() != WL_CONNECTED && now - lastReconnect > RECONNECT_PERIOD_MS) {
          lastReconnect = now;
          WiFi.reconnect();
          connectStart = now;
          currentState = State::CONNECTING;
        }
        break;
      case State::PORTAL_ACTIVE:
        if (dns) dns->processNextRequest();
        if (server) server->handleClient();
        break;
      default:
        break;
    }
  }

  void saveCredentials(const char* newSsid, const char* newPass) {
    Preferences prefs;
    prefs.begin("trainboard", false);
    prefs.putString("ssid", newSsid);
    prefs.putString("pass", newPass);
    prefs.end();
  }
}
