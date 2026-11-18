#include <Arduino.h>
#include <Wire.h>
#include "config.h"

const uint8_t LED_BUS[NUM_BUSES] = {PIN_BUS_0, PIN_BUS_1, PIN_BUS_2, PIN_BUS_3};

int ledPos = 0;
int busActif = 0;
bool modeToutAllume = false;
int couleurIdx = 0;
int lastEncA = HIGH;

// Table de caractères 14 segments (HT16K33)
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

void initDisplay() {
  Wire.beginTransmission(DISPLAY_I2C_ADDR);
  Wire.write(0x21); // Oscillateur ON
  Wire.endTransmission();
  delay(20);

  Wire.beginTransmission(DISPLAY_I2C_ADDR);
  Wire.write(0xA0); // ROW/INT en sortie row driver
  Wire.endTransmission();

  Wire.beginTransmission(DISPLAY_I2C_ADDR);
  Wire.write(0xEA); // Luminosité 10/15
  Wire.endTransmission();

  Wire.beginTransmission(DISPLAY_I2C_ADDR);
  Wire.write(0x81); // Affichage ON
  Wire.endTransmission();
}

void afficherTexte(const char* txt) {
  Wire.beginTransmission(DISPLAY_I2C_ADDR);
  Wire.write(0x00);
  for (int i = 0; i < 4; i++) {
    char c = (i < (int)strlen(txt)) ? txt[i] : ' ';
    uint16_t seg = charTo14Seg(c);
    Wire.write(seg & 0xFF);
    Wire.write((seg >> 8) & 0xFF);
  }
  Wire.endTransmission();
}

void envoyerRuban(uint8_t pin, int indexCible, uint8_t r, uint8_t g, uint8_t b, bool tout) {
  static rmt_data_t buffer[MAX_LEDS_PER_BUS * 24];
  int k = 0;

  for (int i = 0; i < MAX_LEDS_PER_BUS; i++) {
    bool allume = tout || (i == indexCible);
    uint32_t grb = allume ? (((uint32_t)g << 16) | ((uint32_t)r << 8) | b) : 0;

    for (int bit = 23; bit >= 0; bit--) {
      if (grb & (1 << bit)) {
        buffer[k].level0 = 1; buffer[k].duration0 = 8;
        buffer[k].level1 = 0; buffer[k].duration1 = 4;
      } else {
        buffer[k].level0 = 1; buffer[k].duration0 = 4;
        buffer[k].level1 = 0; buffer[k].duration1 = 8;
      }
      k++;
    }
  }
  rmtWrite(pin, buffer, k, RMT_WAIT_FOR_EVER);
}

void rafraichirTout() {
  uint8_t r = (couleurIdx == 0 || couleurIdx == 3) ? LED_BRIGHTNESS : 0;
  uint8_t g = (couleurIdx == 1 || couleurIdx == 3) ? LED_BRIGHTNESS : 0;
  uint8_t b = (couleurIdx == 2 || couleurIdx == 3) ? LED_BRIGHTNESS : 0;

  for (int bus = 0; bus < NUM_BUSES; bus++) {
    if (bus == busActif) {
      envoyerRuban(LED_BUS[bus], ledPos, r, g, b, modeToutAllume);
    } else {
      envoyerRuban(LED_BUS[bus], -1, 0, 0, 0, false);
    }
  }

  char buf[5];
  if (modeToutAllume) {
    snprintf(buf, sizeof(buf), "%dALL", busActif);
  } else {
    snprintf(buf, sizeof(buf), "%d%03d", busActif, ledPos);
  }
  afficherTexte(buf);
  Serial.printf("Bus %d | LED %03d | Ecran: [%s]\n", busActif, ledPos, buf);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("=== TRAINBOARD PLATFORMIO : TEST HARDWARE ===");

  for (int i = 0; i < NUM_BUSES; i++) {
    rmtInit(LED_BUS[i], RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
  }

  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_ENCODER_BTN, INPUT_PULLUP);
  pinMode(PIN_BTN_NAV2, INPUT_PULLUP);
  lastEncA = digitalRead(PIN_ENCODER_A);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(50000);
  initDisplay();

  afficherTexte("CFF ");
  delay(1000);
  rafraichirTout();
}

void loop() {
  static uint32_t lastEncTime = 0;
  int curA = digitalRead(PIN_ENCODER_A);

  if (curA != lastEncA && curA == LOW && (millis() - lastEncTime > 5)) {
    lastEncTime = millis();
    int dir = (digitalRead(PIN_ENCODER_B) == HIGH) ? 1 : -1;
    if (!modeToutAllume) {
      ledPos = (ledPos + dir + MAX_LEDS_PER_BUS) % MAX_LEDS_PER_BUS;
    } else {
      couleurIdx = (couleurIdx + dir + 4) % 4;
    }
    rafraichirTout();
  }
  lastEncA = curA;

  static uint32_t lastDebounce = 0;
  if (digitalRead(PIN_ENCODER_BTN) == LOW && (millis() - lastDebounce > 250)) {
    lastDebounce = millis();
    busActif = (busActif + 1) % NUM_BUSES;
    rafraichirTout();
  }

  if (digitalRead(PIN_BTN_NAV2) == LOW && (millis() - lastDebounce > 250)) {
    lastDebounce = millis();
    modeToutAllume = !modeToutAllume;
    rafraichirTout();
  }
}