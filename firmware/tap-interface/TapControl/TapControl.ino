#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "SparkFun_LIS2DH12.h"

SPARKFUN_LIS2DH12 accel;

#define LED_PIN D1
#define LED_COUNT 6
#define TPM_WINDOW 10000  // ms

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long tapTimestamps[50];
int tapIndex = 0;

int currentRippleIndex = 0;        // Current "head" of the ripple
uint16_t currentHue = 0;           // Color based on tempo

float brightness[LED_COUNT];       // Individual brightness levels (0.0 - 1.0)
const float fadeRate = 0.98;       // Multiplier per frame for trail fading

void setup() {
  strip.begin();
  strip.show();
  strip.setBrightness(255);

  Serial.begin(115200);
  Serial.println("Rotating LED per Tap (Afterimage Effect)");

  Wire.begin();

  if (!accel.begin()) {
    Serial.println("Accelerometer not detected. Freezing...");
    while (1);
  }

  accel.enableTapDetection();
  accel.setTapThreshold(40);

  while (accel.isTapped()) delay(10); // clear any startup tap

  // Init all brightness to 0
  for (int i = 0; i < LED_COUNT; i++) {
    brightness[i] = 0.0;
  }
}

void loop() {
  unsigned long now = millis();

  // TAP DETECTION
  if (accel.isTapped()) {
    tapTimestamps[tapIndex] = now;
    tapIndex = (tapIndex + 1) % 50;

    currentRippleIndex = (currentRippleIndex + 1) % LED_COUNT;

    float tpm = recentTPM();
    currentHue = map((int)tpm, 0, 120, 0, 65535);

    brightness[currentRippleIndex] = 1.0;

    while (accel.isTapped()) delay(10);
  }

  // 🛑 Early cutoff if all are lit
  bool allLit = true;
  for (int i = 0; i < LED_COUNT; i++) {
    if (brightness[i] < 0.9) {
      allLit = false;
      break;
    }
  }

  if (allLit) {
    for (int i = 0; i < LED_COUNT; i++) {
      brightness[i] = 0.0;
    }
    brightness[currentRippleIndex] = 1.0;
  }

  // ✨ Then apply fade & render
  strip.clear();
  for (int i = 0; i < LED_COUNT; i++) {
    if (i != currentRippleIndex) {
      brightness[i] *= fadeRate;
    }

    if (brightness[i] > 0.01) {
      uint32_t color = dimColor(strip.ColorHSV(currentHue), brightness[i]);
      strip.setPixelColor(i, strip.gamma32(color));
    } else {
      brightness[i] = 0.0;
    }
  }

  strip.show();
  delay(10);
}


// ========== HELPERS ==========

float recentTPM() {
  unsigned long now = millis();
  int recentTaps = 0;
  for (int i = 0; i < 50; i++) {
    if (now - tapTimestamps[i] <= TPM_WINDOW) {
      recentTaps++;
    }
  }
  return recentTaps * (60000.0 / TPM_WINDOW);
}

uint32_t dimColor(uint32_t color, float factor) {
  uint8_t r = (uint8_t)(((color >> 16) & 0xFF) * factor);
  uint8_t g = (uint8_t)(((color >> 8) & 0xFF) * factor);
  uint8_t b = (uint8_t)((color & 0xFF) * factor);
  return strip.Color(r, g, b);
}
