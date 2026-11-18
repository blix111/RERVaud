#pragma once
#include <Arduino.h>

namespace WifiManager {
  enum class State { IDLE, SCANNING, CONNECTING, CONNECTED, PORTAL_ACTIVE, FAILED };

  void begin();          // Phase 2 : charge SSID/pass depuis Preferences, tente connexion (timeout 15s)
  bool isConnected();
  State state();
  void loop();           // Phase 2 : portail captif TrainBoard-Setup + reconnexion si nécessaire
  void saveCredentials(const char* ssid, const char* pass);
}
