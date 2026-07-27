#include <Arduino.h>
#include <Wire.h>

constexpr uint8_t BH1750_SDA_PIN = 21;
constexpr uint8_t BH1750_SCL_PIN = 22;
constexpr uint8_t BH1750_ADDRESSES[] = {0x23, 0x5C};
constexpr uint8_t BH1750_POWER_ON = 0x01;
constexpr uint8_t BH1750_CONTINUOUS_HIGH_RES_MODE = 0x10;

uint8_t bh1750Address = 0;

bool sendBh1750Command(uint8_t address, uint8_t command) {
  Wire.beginTransmission(address);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

uint8_t detectBh1750Address() {
  for (const uint8_t address : BH1750_ADDRESSES) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      return address;
    }
  }

  return 0;
}

void printBh1750Address(uint8_t address) {
  Serial.print("0x");
  Serial.print(address, HEX);
}

void discoverAndConfigureBh1750() {
  bh1750Address = detectBh1750Address();
  if (bh1750Address == 0) {
    Serial.println(
        "bh1750_status=error reason=sensor_not_found "
        "addresses_checked=0x23,0x5C");
    return;
  }

  if (!sendBh1750Command(bh1750Address, BH1750_POWER_ON) ||
      !sendBh1750Command(bh1750Address, BH1750_CONTINUOUS_HIGH_RES_MODE)) {
    Serial.print(
        "bh1750_status=error reason=read_failed stage=configuration "
        "i2c_address=");
    printBh1750Address(bh1750Address);
    Serial.println();
    bh1750Address = 0;
    return;
  }

  Serial.print("bh1750_status=ready i2c_address=");
  printBh1750Address(bh1750Address);
  Serial.println();
  delay(180);
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

void setup() {
  Serial.begin(115200);

  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
  Wire.setClock(100000);

  discoverAndConfigureBh1750();
}

void loop() {
  if (bh1750Address == 0) {
    discoverAndConfigureBh1750();
    delay(2000);
    return;
  }

  float lightLux;
  if (!readBh1750(lightLux)) {
    Serial.print(
        "bh1750_status=error reason=read_failed stage=measurement "
        "i2c_address=");
    printBh1750Address(bh1750Address);
    Serial.println();
    bh1750Address = 0;
    delay(2000);
    return;
  }

  Serial.print("light_lux=");
  Serial.println(lightLux, 2);
  delay(2000);
}
