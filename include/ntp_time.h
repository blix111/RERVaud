#pragma once
#include <Arduino.h>
#include <time.h>

namespace NtpTime {
  void begin();   // Phase 2 : configTzTime, TZ Europe/Zurich, à appeler après WifiManager::isConnected()
  bool isSynced();
  time_t now();
  void formatHM(char out[6]); // "HH:MM\0"
}
