#include <Arduino.h>
#include "wifi_manager.h"
#include "ntp_time.h"
#include "shared_state.h"
#include "api_client.h"
#include "stations.h"
#include <string.h>

void taskCore0Network(void* param) {
  (void)param;
  WifiManager::begin();

  uint8_t nextGare = 0;
  uint32_t lastPoll = 0;
  uint8_t failStreak = 0;
  bool initialSweepDone = false;

  // Premier balayage ultra-rapide (20 ms entre requêtes) pour allumer toutes les gares au plus vite,
  // puis rafraîchissement continu rapide (150 ms entre requêtes).
  constexpr uint32_t FAST_POLL_INTERVAL_MS   = 20;
  constexpr uint32_t NORMAL_POLL_INTERVAL_MS = 150;

  for (;;) {
    WifiManager::loop();
    if (WifiManager::isConnected() && !NtpTime::isSynced()) {
      NtpTime::begin();
    }

    uint32_t fastMs = g_settings.pollFastMs ? g_settings.pollFastMs : 20;
    uint32_t normMs = g_settings.pollNormalMs ? g_settings.pollNormalMs : 100;
    uint32_t interval = initialSweepDone ? normMs : fastMs;
    if (WifiManager::isConnected() && NtpTime::isSynced() &&
        millis() - lastPoll >= interval) {
      lastPoll = millis();

      // Saute les gares non interrogeables (ex. HORS)
      while (nextGare < NB_GARES && !GARES[nextGare].pivotAPI) nextGare++;
      if (nextGare >= NB_GARES) {
        nextGare = 0;
        initialSweepDone = true;
      }
      while (!GARES[nextGare].pivotAPI) nextGare++;

      StationboardEntry tmp[MAX_BOARD_ENTRIES];
      uint8_t cnt = 0;
      // I/O HTTPS hors mutex
      bool ok = ApiClient::fetchStationboard(GARES[nextGare].nomAPI, tmp, MAX_BOARD_ENTRIES, &cnt);

      if (xSemaphoreTake(g_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (ok) {
          time_t now = NtpTime::now();
          // Conserve les départs récents (< 10 min dans le passé) qui sont encore en circulation
          // entre cette gare et la gare suivante si de la place reste dans tmp[].
          uint8_t prevCnt = g_sharedData.boardCount[nextGare];
          for (uint8_t p = 0; p < prevCnt && cnt < MAX_BOARD_ENTRIES; p++) {
            const StationboardEntry& oldE = g_sharedData.boards[nextGare][p];
            if (oldE.ligne == 0xFF) continue;
            int32_t age = (int32_t)(now - oldE.realtime);
            if (age >= -15 && age <= 600) {
              bool alreadyPresent = false;
              for (uint8_t k = 0; k < cnt; k++) {
                if (tmp[k].ligne == oldE.ligne &&
                    labs((long)(tmp[k].scheduled - oldE.scheduled)) < 120) {
                  alreadyPresent = true;
                  break;
                }
              }
              if (!alreadyPresent) {
                tmp[cnt++] = oldE;
              }
            }
          }

          memcpy(g_sharedData.boards[nextGare], tmp, sizeof(tmp));
          g_sharedData.boardCount[nextGare] = cnt;
          g_sharedData.boardPolled[nextGare] = true;
          g_sharedData.apiHealthy = true;
          g_sharedData.lastGoodPollEpoch = now;
          failStreak = 0;
        } else if (++failStreak >= 5) {
          g_sharedData.apiHealthy = false;
        }
        xSemaphoreGive(g_dataMutex);
      }
      nextGare++;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
