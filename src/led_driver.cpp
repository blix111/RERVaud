#include "led_driver.h"
#include "config.h"
#include <string.h>
#include <Preferences.h>

namespace {
  const uint8_t LED_BUS[NUM_BUSES] = {PIN_BUS_0, PIN_BUS_1, PIN_BUS_2, PIN_BUS_3};
  uint8_t frameBuf[NUM_BUSES][MAX_LEDS_PER_BUS][3];
  rmt_data_t rmtBuf[MAX_LEDS_PER_BUS * 24];

  // Luminosité globale en pourcentage (0..100%).
  // Valeur par défaut réduite (LED_BRIGHTNESS = 15) pour éviter l'éblouissement.
  uint8_t g_brightness = LED_BRIGHTNESS;

  uint8_t scaleBrightness(uint8_t v) {
    if (v == 0 || g_brightness == 0) return 0;
    uint32_t res = ((uint32_t)v * g_brightness) / 100;
    if (res == 0 && v > 0 && g_brightness > 0) res = 1;
    return (uint8_t)res;
  }

  void flushBus(uint8_t bus) {
    int k = 0;
    for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
      uint8_t r = scaleBrightness(frameBuf[bus][i][0]);
      uint8_t g = scaleBrightness(frameBuf[bus][i][1]);
      uint8_t b = scaleBrightness(frameBuf[bus][i][2]);
      uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
      for (int bit = 23; bit >= 0; bit--) {
        if (grb & (1UL << bit)) {
          rmtBuf[k].level0 = 1; rmtBuf[k].duration0 = 8;
          rmtBuf[k].level1 = 0; rmtBuf[k].duration1 = 4;
        } else {
          rmtBuf[k].level0 = 1; rmtBuf[k].duration0 = 4;
          rmtBuf[k].level1 = 0; rmtBuf[k].duration1 = 8;
        }
        k++;
      }
    }
    rmtWrite(LED_BUS[bus], rmtBuf, k, RMT_WAIT_FOR_EVER);
  }
}

namespace LedDriver {
  void begin() {
    Preferences prefs;
    prefs.begin("trainboard", true);
    g_brightness = prefs.getUChar("bright", LED_BRIGHTNESS);
    prefs.end();
    if (g_brightness > 100) g_brightness = 100;

    for (int i = 0; i < NUM_BUSES; i++) {
      rmtInit(LED_BUS[i], RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
    }
    clearAll();
  }

  void setPixel(uint8_t bus, uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (bus >= NUM_BUSES || index >= MAX_LEDS_PER_BUS) return;
    frameBuf[bus][index][0] = r;
    frameBuf[bus][index][1] = g;
    frameBuf[bus][index][2] = b;
  }

  void clearBus(uint8_t bus) {
    if (bus >= NUM_BUSES) return;
    memset(frameBuf[bus], 0, sizeof(frameBuf[bus]));
  }

  void clearAll() {
    for (int bus = 0; bus < NUM_BUSES; bus++) clearBus(bus);
  }

  void show(uint8_t bus) {
    if (bus >= NUM_BUSES) return;
    flushBus(bus);
  }

  void showAll() {
    for (int bus = 0; bus < NUM_BUSES; bus++) flushBus(bus);
  }

  void setBrightness(uint8_t v) {
    setBrightnessPercent(v);
  }

  uint8_t getBrightness() {
    return getBrightnessPercent();
  }

  void setBrightnessPercent(uint8_t percent) {
    g_brightness = (percent > 100) ? 100 : percent;
  }

  uint8_t getBrightnessPercent() {
    return g_brightness;
  }

  void saveBrightness() {
    Preferences prefs;
    prefs.begin("trainboard", false);
    prefs.putUChar("bright", g_brightness);
    prefs.end();
  }
}
