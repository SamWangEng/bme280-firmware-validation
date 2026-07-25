#include <Wire.h>

const uint8_t BME280_ADDR     = 0x76;  // use 0x77 if your scanner found the sensor there instead
const uint8_t REG_CALIB_START = 0x88;  // dig_T1 starts here; dig_T1..dig_T3 span 6 bytes ; Calibration is what turns "some raw electrical signal" into an accurate real-world measurement.

// Reads `len` consecutive registers starting at `start_reg` in one burst,
// instead of one read_register() call per byte.
void read_registers(uint8_t start_reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(start_reg); //point your register pointer at start_reg,
  Wire.endTransmission(false);  // repeated start - keep the bus held

  Wire.requestFrom(BME280_ADDR, len);
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
}

uint16_t dig_T1; //digital calibration data, Temperature
int16_t  dig_T2;
int16_t  dig_T3;

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial) {}

  uint8_t raw[6]; // calibration is a 16-bit(2 byte) number, 3 number means 6 byte 
  read_registers(REG_CALIB_START, raw, 6);

  // Each value is little-endian: low byte first, high byte second.
  dig_T1 = (uint16_t)(raw[1] << 8) | raw[0];
  dig_T2 = (int16_t)((raw[3] << 8) | raw[2]);
  dig_T3 = (int16_t)((raw[5] << 8) | raw[4]);

  Serial.print("dig_T1 = ");
  Serial.println(dig_T1);
  Serial.print("dig_T2 = ");
  Serial.println(dig_T2);
  Serial.print("dig_T3 = ");
  Serial.println(dig_T3);
}

void loop() {
  // Calibration data is read once at boot - nothing to repeat here.
}
