#include "shared_state.h"
#include <string.h>

SharedData g_sharedData;
SemaphoreHandle_t g_dataMutex = nullptr;
std::atomic<uint8_t> g_currentMode{0};
std::atomic<bool> g_demoForced{false};

namespace SharedState {
  void begin() {
    memset(&g_sharedData, 0, sizeof(g_sharedData));
    g_dataMutex = xSemaphoreCreateMutex();
  }
}
