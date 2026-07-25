#include <Wire.h>

const uint8_t BME280_ADDR = 0x76;  // use 0x77 if your scanner found the sensor there instead
const uint8_t REG_CTRL_HUM = 0xF2;

uint8_t read_register(uint8_t reg) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);  // repeated start - keep the bus held

  Wire.requestFrom(BME280_ADDR, (uint8_t)1);
  return Wire.read();
}

void write_register(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();  // full stop - the write is complete, nothing to read after
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial) {}

  uint8_t before = read_register(REG_CTRL_HUM);
  Serial.print("ctrl_hum before write: 0x");
  Serial.println(before, HEX);

  write_register(REG_CTRL_HUM, 0x01);  // osrs_h = 001 (humidity oversampling x1)

  uint8_t after = read_register(REG_CTRL_HUM);
  Serial.print("ctrl_hum after write:  0x");
  Serial.println(after, HEX);

  if (after == 0x01) {
    Serial.println("Write confirmed - chip accepted the value.");
  } else {
    Serial.println("Write did NOT take effect as expected.");
  }
}

void loop() {
  // one-shot test - nothing to repeat
}
