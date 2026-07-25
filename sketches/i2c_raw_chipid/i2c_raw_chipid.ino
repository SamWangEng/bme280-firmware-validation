#include <Wire.h>

#define BME280_ADDR   0x76  
#define REG_CHIP_ID   0xD0
#define EXPECTED_ID   0x60

// Reads one register from the BME280 using the raw two-step I2C sequence:
// 1) write the register address we want (with a repeated start, not a stop)
// 2) request/read the byte living at that register
uint8_t read_register(uint8_t reg) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);  // repeated start - keep the bus held

  Wire.requestFrom(BME280_ADDR, 1);
  return Wire.read();
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial) {}
}

void loop() {
  uint8_t chipId = read_register(REG_CHIP_ID);

  Serial.print("Chip ID: 0x");
  Serial.print(chipId, HEX);

  if (chipId == EXPECTED_ID) {
    Serial.println("  -> MATCH (this is a BME280)");
  } else {
    Serial.println("  -> MISMATCH (wrong device or bad read)");
  }

  delay(1000);
}
