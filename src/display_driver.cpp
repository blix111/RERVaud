#include "display_driver.h"
#include <Wire.h>
#include "config.h"
#include <string.h>
#include <ctype.h>

namespace {
  uint16_t charTo14Seg(char c) {
    switch (toupper(c)) {
      case '0': return 0x0C3F; case '1': return 0x0006; case '2': return 0x00DB;
      case '3': return 0x008F; case '4': return 0x00E6; case '5': return 0x2069;
      case '6': return 0x00FD; case '7': return 0x0007; case '8': return 0x00FF;
      case '9': return 0x00EF; case 'A': return 0x00F7; case 'B': return 0x128F;
      case 'C': return 0x0039; case 'D': return 0x120F; case 'E': return 0x0079;
      case 'F': return 0x0071; case 'G': return 0x00BD; case 'H': return 0x00F6;
      case 'I': return 0x1209; case 'J': return 0x001E; case 'K': return 0x2470;
      case 'L': return 0x0038; case 'M': return 0x0536; case 'N': return 0x2136;
      case 'O': return 0x003F; case 'P': return 0x00F3; case 'Q': return 0x203F;
      case 'R': return 0x20F3; case 'S': return 0x00ED; case 'T': return 0x1201;
      case 'U': return 0x003E; case 'V': return 0x0C30; case 'W': return 0x2836;
      case 'X': return 0x2D00; case 'Y': return 0x1500; case 'Z': return 0x0C09;
      case '-': return 0x00C0; case ' ': default: return 0x0000;
    }
  }

  void writeRaw(const char* text4) {
    Wire.beginTransmission(DISPLAY_I2C_ADDR);
    Wire.write(0x00);
    for (int i = 0; i < 4; i++) {
      char c = (i < (int)strlen(text4)) ? text4[i] : ' ';
      uint16_t seg = charTo14Seg(c);
      Wire.write(seg & 0xFF);
      Wire.write((seg >> 8) & 0xFF);
    }
    Wire.endTransmission();
  }

  const size_t SCROLL_BUF_SIZE = 64;
  char scrollBuf[SCROLL_BUF_SIZE];
  size_t scrollLen = 0;
  size_t scrollPos = 0;
  uint32_t scrollIntervalMs = 0;
  uint32_t lastScrollStep = 0;
  bool scrolling = false;

  void renderScrollFrame() {
    char frame[5];
    for (int i = 0; i < 4; i++) {
      size_t srcIdx = scrollPos + i;
      frame[i] = (srcIdx < scrollLen) ? scrollBuf[srcIdx] : ' ';
    }
    frame[4] = '\0';
    writeRaw(frame);
  }
}

namespace DisplayDriver {
  void begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(50000);
    Wire.beginTransmission(DISPLAY_I2C_ADDR); Wire.write(0x21); Wire.endTransmission(); delay(20);
    Wire.beginTransmission(DISPLAY_I2C_ADDR); Wire.write(0xA0); Wire.endTransmission();
    Wire.beginTransmission(DISPLAY_I2C_ADDR); Wire.write(0xEA); Wire.endTransmission();
    Wire.beginTransmission(DISPLAY_I2C_ADDR); Wire.write(0x81); Wire.endTransmission();
  }

  void showText(const char* text4) {
    scrolling = false;
    writeRaw(text4);
  }

  void showScrolling(const char* fullText, uint32_t stepIntervalMs) {
    size_t len = strlen(fullText);
    if (len >= SCROLL_BUF_SIZE) len = SCROLL_BUF_SIZE - 1;
    memcpy(scrollBuf, fullText, len);
    scrollBuf[len] = '\0';
    scrollLen = len;
    scrollPos = 0;
    scrollIntervalMs = stepIntervalMs;
    lastScrollStep = millis();
    scrolling = true;
    renderScrollFrame();
  }

  void update() {
    if (!scrolling) return;
    uint32_t now = millis();
    if (now - lastScrollStep < scrollIntervalMs) return;
    lastScrollStep = now;
    scrollPos++;
    if (scrollPos > scrollLen) scrollPos = 0;
    renderScrollFrame();
  }
}
