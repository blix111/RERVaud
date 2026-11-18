#pragma once

// =====================================================================
// TrainBoard - Pinout matériel validé (ESP32-S3-WROOM-1-N4)
// =====================================================================

// --- Bus LED (WS2812B SMD voies / LEDs THT 5mm gares) via 74AHCT125 ---
#define PIN_BUS_0 4   // BufferVert
#define PIN_BUS_1 5   // BufferBlanc
#define PIN_BUS_2 6   // BufferBleu
#define PIN_BUS_3 7   // BufferViolet

// Nombre max de LEDs par bus (250 pour couvrir la plus longue chaîne)
#define NUM_LEDS_BUS_0 250
#define NUM_LEDS_BUS_1 250
#define NUM_LEDS_BUS_2 250
#define NUM_LEDS_BUS_3 250
#define MAX_LEDS_PER_BUS 250

#define NUM_BUSES 4
#define LED_BRIGHTNESS 15 // Bridage logiciel doux (~15% par défaut) pour confort visuel et protéger l'USB

// --- Interface utilisateur (INPUT_PULLUP, actifs LOW) ---
#define PIN_ENCODER_A   10
#define PIN_ENCODER_B   11
#define PIN_ENCODER_BTN 12
#define PIN_BTN_NAV2    8

// --- Afficheur 14 segments I2C (HT16K33 en 3.3V) ---
#define PIN_I2C_SDA      14
#define PIN_I2C_SCL      13
#define DISPLAY_I2C_ADDR 0x70