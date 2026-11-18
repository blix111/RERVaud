#include "wifi_manager.h"

namespace {
  WifiManager::State currentState = WifiManager::State::IDLE;
}

namespace WifiManager {
  void begin() {
    // Phase 2 : Preferences (NVS) -> WiFi.begin(ssid, pass), timeout 15s,
    // sinon bascule vers le portail captif TrainBoard-Setup (formulaire SSID/mdp minimal).
    currentState = State::IDLE;
  }

  bool isConnected() {
    return currentState == State::CONNECTED;
  }

  State state() {
    return currentState;
  }

  void loop() {
    // Phase 2
  }

  void saveCredentials(const char* ssid, const char* pass) {
    (void)ssid; (void)pass; // Phase 2 : Preferences::putString(...)
  }
}
