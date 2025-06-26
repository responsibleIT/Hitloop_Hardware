#include <Wire.h>
#include "SparkFun_LIS2DH12.h"

SPARKFUN_LIS2DH12 accel;

int rawX = 0, rawY = 0, rawZ = 0;
int prevX = 0, prevY = 0, prevZ = 0;
bool firstRead = true;

float magnitudeHistory[10];
int historyIndex = 0;

float prevMagnitude = 0;
float prevPrevMagnitude = 0;
float smoothedMagnitude = 0;

#define MAX_PEAKS 1000
unsigned long peakTimestamps[MAX_PEAKS];
int peakCount = 0;

const int smoothWindowSize = 5;
const unsigned long minPeakDistance = 300; // ms
const float minPeakHeight = 100.0;
const unsigned long BPM_AVG_WINDOW = 10000; // ms

unsigned long lastPeakTime = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!accel.begin()) {
    Serial.println("Accelerometer not detected. Freezing...");
    while (1);
  }

  for (int i = 0; i < 10; i++) magnitudeHistory[i] = 0;
  for (int i = 0; i < MAX_PEAKS; i++) peakTimestamps[i] = 0;
}

void loop() {
  if (accel.available()) {
    rawX = accel.getX();
    rawY = accel.getY();
    rawZ = accel.getZ();

    if (!firstRead) {
      int dx = rawX - prevX;
      int dy = rawY - prevY;
      int dz = rawZ - prevZ;

      float rawMagnitude = sqrt(dx * dx + dy * dy + dz * dz);

      // Smooth with moving average
      magnitudeHistory[historyIndex] = rawMagnitude;
      historyIndex = (historyIndex + 1) % smoothWindowSize;

      float sum = 0;
      for (int i = 0; i < smoothWindowSize; i++) sum += magnitudeHistory[i];
      smoothedMagnitude = sum / smoothWindowSize;

      // Peak detection using derivative
      bool isPeak = prevPrevMagnitude < prevMagnitude &&
                    prevMagnitude > smoothedMagnitude &&
                    prevMagnitude > minPeakHeight &&
                    (millis() - lastPeakTime > minPeakDistance);

      if (isPeak) {
        lastPeakTime = millis();
        if (peakCount < MAX_PEAKS) {
          peakTimestamps[peakCount++] = lastPeakTime;
        } else {
          for (int i = 1; i < MAX_PEAKS; i++) {
            peakTimestamps[i - 1] = peakTimestamps[i];
          }
          peakTimestamps[MAX_PEAKS - 1] = lastPeakTime;
        }
      }

      // Calculate BPM from peaks in last 10 seconds
      float bpm = 0;
      int validBeats = 0;
      unsigned long now = millis();

      for (int i = peakCount - 1; i > 0; i--) {
        if (now - peakTimestamps[i] > BPM_AVG_WINDOW) break;
        if (peakTimestamps[i] > 0 && peakTimestamps[i - 1] > 0) {
          unsigned long interval = peakTimestamps[i] - peakTimestamps[i - 1];
          if (interval > 0 && interval < 2000) {
            bpm += 60000.0 / interval;
            validBeats++;
          }
        }
      }

      if (validBeats > 0) {
        bpm /= validBeats;
      } else {
        bpm = 0;
      }

      Serial.print(smoothedMagnitude);
      Serial.print(",");
      Serial.println(bpm);

      // Update history
      prevPrevMagnitude = prevMagnitude;
      prevMagnitude = smoothedMagnitude;
    } else {
      firstRead = false;
    }

    prevX = rawX;
    prevY = rawY;
    prevZ = rawZ;
  }

  delay(10); // 100 Hz loop
}
