#include "ntp_time.h"
#include <Arduino.h>

namespace {
  bool started = false;
}

namespace NtpTime {
  void begin() {
    if (started) return;
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
    started = true;
  }

  bool isSynced() {
    // Synchronisé quand l'heure dépasse 2020 (epoch par défaut = 1970)
    return started && time(nullptr) > 1577836800;
  }

  time_t now() {
    return time(nullptr);
  }

  void formatHM(char out[6]) {
    time_t t = now();
    struct tm tmStruct;
    localtime_r(&t, &tmStruct);
    snprintf(out, 6, "%02d:%02d", tmStruct.tm_hour, tmStruct.tm_min);
  }
}
