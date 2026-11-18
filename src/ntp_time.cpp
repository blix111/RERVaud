#include "ntp_time.h"

namespace {
  bool synced = false;
}

namespace NtpTime {
  void begin() {
    // Phase 2 : configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
  }

  bool isSynced() {
    return synced;
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
