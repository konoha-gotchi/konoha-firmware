#include <Arduino.h>

constexpr uint8_t SOIL_MOISTURE_PIN = 34;
constexpr int PROVISIONAL_SOIL_DRY_RAW = 3200;
constexpr int SOIL_WET_RAW = 2162;

float soilMoisturePercent(int rawReading) {
  const float percent =
      100.0f * (PROVISIONAL_SOIL_DRY_RAW - rawReading) /
      (PROVISIONAL_SOIL_DRY_RAW - SOIL_WET_RAW);

  return constrain(percent, 0.0f, 100.0f);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_MOISTURE_PIN, ADC_11db);
}

void loop() {
  const int rawReading = analogRead(SOIL_MOISTURE_PIN);
  const float percent = soilMoisturePercent(rawReading);

  Serial.print("soil_moisture_raw=");
  Serial.print(rawReading);
  Serial.print(" soil_moisture_percent=");
  Serial.println(percent, 1);
  delay(1000);
}
