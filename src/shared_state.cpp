#include "shared_state.h"
#include <Preferences.h>
#include <string.h>

SharedData g_sharedData;
AppSettings g_settings;
SemaphoreHandle_t g_dataMutex = nullptr;
std::atomic<uint8_t> g_currentMode{0};
std::atomic<int8_t> g_forcePollGare{-1};
std::atomic<bool> g_studioLedOverride{false};

namespace {
  void setDefaultSettings() {
    g_settings.pollFastMs        = 20;
    g_settings.pollNormalMs      = 100;
    g_settings.apiLimit          = 20;
    g_settings.stationWindowSec  = 35;
    g_settings.gareLookaheadSec  = 1800;
    g_settings.gareNoTrainMode   = 1;
    g_settings.blinkBaseHalfMs   = 660;
    g_settings.blinkStepPerMinMs = 108;
    g_settings.scrollSpeedMs     = 220;
    g_settings.travelScalePct    = 100;
  }
}

namespace SharedState {
  void begin() {
    memset(&g_sharedData, 0, sizeof(g_sharedData));
    g_dataMutex = xSemaphoreCreateMutex();
    setDefaultSettings();
    Preferences prefs;
    if (prefs.begin("tb_cfg", true)) {
      if (prefs.getBytesLength("cfg") == sizeof(AppSettings)) {
        prefs.getBytes("cfg", &g_settings, sizeof(AppSettings));
      }
      prefs.end();
    }
    Colors::begin();
  }

  void saveSettings() {
    Preferences prefs;
    if (prefs.begin("tb_cfg", false)) {
      prefs.putBytes("cfg", &g_settings, sizeof(AppSettings));
      prefs.end();
    }
  }

  void resetSettings() {
    setDefaultSettings();
    saveSettings();
  }
}
