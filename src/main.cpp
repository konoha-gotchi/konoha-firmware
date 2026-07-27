#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t SOIL_MOISTURE_PIN = 34;
constexpr int PROVISIONAL_SOIL_DRY_RAW = 3200;
constexpr int SOIL_WET_RAW = 2162;

constexpr uint8_t SHT31_SDA_PIN = 21;
constexpr uint8_t SHT31_SCL_PIN = 22;
constexpr uint8_t SHT31_ADDRESSES[] = {0x44, 0x45};

uint8_t sht31Address = 0;

float soilMoisturePercent(int rawReading) {
  const float percent =
      100.0f * (PROVISIONAL_SOIL_DRY_RAW - rawReading) /
      (PROVISIONAL_SOIL_DRY_RAW - SOIL_WET_RAW);

  return constrain(percent, 0.0f, 100.0f);
}

uint8_t calculateCrc(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;

  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
    }
  }

  return crc;
}

uint8_t detectSht31Address() {
  for (const uint8_t address : SHT31_ADDRESSES) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      return address;
    }
  }

  return 0;
}

bool readSht31(float &temperatureC, float &humidityPercent) {
  Wire.beginTransmission(sht31Address);
  Wire.write(0x24);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(20);

  uint8_t data[6];
  if (Wire.requestFrom(sht31Address, static_cast<uint8_t>(sizeof(data))) !=
      sizeof(data)) {
    return false;
  }

  for (uint8_t &value : data) {
    value = Wire.read();
  }

  if (calculateCrc(data, 2) != data[2] ||
      calculateCrc(data + 3, 2) != data[5]) {
    return false;
  }

  const uint16_t rawTemperature =
      (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t rawHumidity =
      (static_cast<uint16_t>(data[3]) << 8) | data[4];

  temperatureC = -45.0f + 175.0f * rawTemperature / 65535.0f;
  humidityPercent = 100.0f * rawHumidity / 65535.0f;
  return true;
}

void printSoilReading(int rawReading) {
  Serial.print("soil_moisture_raw=");
  Serial.print(rawReading);
  Serial.print(" soil_moisture_percent=");
  Serial.print(soilMoisturePercent(rawReading), 1);
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_MOISTURE_PIN, ADC_11db);

  Wire.begin(SHT31_SDA_PIN, SHT31_SCL_PIN);
  Wire.setClock(100000);

  sht31Address = detectSht31Address();
  if (sht31Address == 0) {
    Serial.println(
        "sht31_status=error reason=sensor_not_found "
        "addresses_checked=0x44,0x45");
    return;
  }

  Serial.print("sht31_status=ready i2c_address=0x");
  Serial.println(sht31Address, HEX);
}

void loop() {
  if (sht31Address == 0) {
    sht31Address = detectSht31Address();
  }

  const int rawReading = analogRead(SOIL_MOISTURE_PIN);
  printSoilReading(rawReading);

  if (sht31Address == 0) {
    Serial.println(" sht31_status=error reason=sensor_not_found");
    delay(2000);
    return;
  }

  float temperatureC;
  float humidityPercent;
  if (!readSht31(temperatureC, humidityPercent)) {
    Serial.println(" sht31_status=error reason=read_failed");
    delay(2000);
    return;
  }

  Serial.print(" temperature_c=");
  Serial.print(temperatureC, 2);
  Serial.print(" humidity_percent=");
  Serial.println(humidityPercent, 2);
  delay(2000);
}
