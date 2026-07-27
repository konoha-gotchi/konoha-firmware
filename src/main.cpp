#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t SOIL_MOISTURE_PIN = 34;
constexpr int PROVISIONAL_SOIL_DRY_RAW = 3200;
constexpr int SOIL_WET_RAW = 2162;

constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;
constexpr uint8_t SHT31_ADDRESSES[] = {0x44, 0x45};
constexpr size_t SHT31_ADDRESS_COUNT =
    sizeof(SHT31_ADDRESSES) / sizeof(SHT31_ADDRESSES[0]);
constexpr uint8_t BH1750_ADDRESSES[] = {0x23, 0x5C};
constexpr size_t BH1750_ADDRESS_COUNT =
    sizeof(BH1750_ADDRESSES) / sizeof(BH1750_ADDRESSES[0]);
constexpr uint8_t BH1750_POWER_ON = 0x01;
constexpr uint8_t BH1750_CONTINUOUS_HIGH_RES_MODE = 0x10;

uint8_t sht31Address = 0;
uint8_t bh1750Address = 0;

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

uint8_t detectI2cAddress(const uint8_t *addresses, size_t addressCount) {
  for (size_t index = 0; index < addressCount; ++index) {
    const uint8_t address = addresses[index];
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      return address;
    }
  }

  return 0;
}

bool sendI2cCommand(uint8_t address, uint8_t command) {
  Wire.beginTransmission(address);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

void printI2cAddress(uint8_t address) {
  Serial.print("0x");
  Serial.print(address, HEX);
}

bool discoverSht31() {
  sht31Address = detectI2cAddress(SHT31_ADDRESSES, SHT31_ADDRESS_COUNT);
  if (sht31Address == 0) {
    Serial.println(
        "sht31_status=error reason=sensor_not_found "
        "addresses_checked=0x44,0x45");
    return false;
  }

  Serial.print("sht31_status=ready i2c_address=");
  printI2cAddress(sht31Address);
  Serial.println();
  return true;
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

bool discoverAndConfigureBh1750() {
  bh1750Address = detectI2cAddress(BH1750_ADDRESSES, BH1750_ADDRESS_COUNT);
  if (bh1750Address == 0) {
    Serial.println(
        "bh1750_status=error reason=sensor_not_found "
        "addresses_checked=0x23,0x5C");
    return false;
  }

  if (!sendI2cCommand(bh1750Address, BH1750_POWER_ON) ||
      !sendI2cCommand(bh1750Address, BH1750_CONTINUOUS_HIGH_RES_MODE)) {
    Serial.print(
        "bh1750_status=error reason=read_failed stage=configuration "
        "i2c_address=");
    printI2cAddress(bh1750Address);
    Serial.println();
    bh1750Address = 0;
    return false;
  }

  Serial.print("bh1750_status=ready i2c_address=");
  printI2cAddress(bh1750Address);
  Serial.println();
  delay(180);
  return true;
}

bool readBh1750(float &lightLux) {
  if (Wire.requestFrom(bh1750Address, static_cast<uint8_t>(2)) != 2) {
    return false;
  }

  const uint16_t rawReading =
      (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
  lightLux = rawReading / 1.2f;
  return true;
}

void printCombinedReading(int rawReading, float temperatureC,
                          float humidityPercent, float lightLux) {
  Serial.print("{\"soil_moisture_raw\":");
  Serial.print(rawReading);
  Serial.print(",\"soil_moisture_percent\":");
  Serial.print(soilMoisturePercent(rawReading), 1);
  Serial.print(",\"temperature_c\":");
  Serial.print(temperatureC, 2);
  Serial.print(",\"humidity_percent\":");
  Serial.print(humidityPercent, 2);
  Serial.print(",\"light_lux\":");
  Serial.print(lightLux, 2);
  Serial.println("}");
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_MOISTURE_PIN, ADC_11db);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);

  discoverSht31();
  discoverAndConfigureBh1750();
}

void loop() {
  if (sht31Address == 0 && !discoverSht31()) {
    delay(2000);
    return;
  }

  if (bh1750Address == 0 && !discoverAndConfigureBh1750()) {
    delay(2000);
    return;
  }

  const int rawReading = analogRead(SOIL_MOISTURE_PIN);

  float temperatureC;
  float humidityPercent;
  if (!readSht31(temperatureC, humidityPercent)) {
    Serial.println("sht31_status=error reason=read_failed");
    delay(2000);
    return;
  }

  float lightLux;
  if (!readBh1750(lightLux)) {
    Serial.println("bh1750_status=error reason=read_failed");
    bh1750Address = 0;
    delay(2000);
    return;
  }

  printCombinedReading(rawReading, temperatureC, humidityPercent, lightLux);
  delay(2000);
}
