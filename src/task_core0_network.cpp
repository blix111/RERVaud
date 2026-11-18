#include <Arduino.h>
#include "wifi_manager.h"
#include "ntp_time.h"
#include "shared_state.h"

void taskCore0Network(void* param) {
  (void)param;
  WifiManager::begin();

  for (;;) {
    WifiManager::loop();
    if (WifiManager::isConnected() && !NtpTime::isSynced()) {
      NtpTime::begin();
    }
    // Phase 2 : polling round-robin des 7 gares pivots, parsing JSON filtré,
    // écriture dans g_sharedData sous g_dataMutex (jamais tenu pendant l'I/O HTTPS).
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
