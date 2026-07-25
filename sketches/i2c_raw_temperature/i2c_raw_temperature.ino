#include <Wire.h>

const uint8_t BME280_ADDR   = 0x76;  // use 0x77 if your scanner found the sensor there instead
const uint8_t REG_CALIB_START = 0x88;  // dig_T1..dig_T3, 6 bytes
const uint8_t REG_CTRL_MEAS = 0xF4;
const uint8_t REG_STATUS    = 0xF3;
const uint8_t REG_TEMP_MSB  = 0xFA;   // raw temp data starts here (0xFA-0xFC, 3 bytes)

const uint8_t STATUS_MEASURING_BIT = 0x08;  // bit 3: 1 = conversion in progress

uint16_t dig_T1;
int16_t  dig_T2;
int16_t  dig_T3;

uint8_t read_register(uint8_t reg) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);

  Wire.requestFrom(BME280_ADDR, (uint8_t)1);
  return Wire.read();
}

void read_registers(uint8_t start_reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(start_reg);
  Wire.endTransmission(false);

  Wire.requestFrom(BME280_ADDR, len);
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
}

void write_register(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void read_calibration() {
  uint8_t raw[6];
  read_registers(REG_CALIB_START, raw, 6);

  dig_T1 = (uint16_t)(raw[1] << 8) | raw[0];
  dig_T2 = (int16_t)((raw[3] << 8) | raw[2]);
  dig_T3 = (int16_t)((raw[5] << 8) | raw[4]);
}

// Triggers one forced-mode measurement and waits for it to finish.
// Returns false if the sensor didn't respond in time.
bool trigger_and_wait() {
  write_register(REG_CTRL_MEAS, 0x21);  // osrs_t=x1, osrs_p=skip, mode=forced

  const unsigned long TIMEOUT_MS = 100;
  unsigned long start = millis();
  uint8_t status;

  do {
    status = read_register(REG_STATUS);
    if (millis() - start > TIMEOUT_MS) {
      return false;
    }
  } while (status & STATUS_MEASURING_BIT);

  return true;
}

int32_t read_raw_adc_t() {
  uint8_t raw[3];
  read_registers(REG_TEMP_MSB, raw, 3);

  return ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
}

// Bosch's official double-precision compensation formula (matches what
// Adafruit_BME280::readTemperature() computes internally), using the real
// dig_T3 term this time - not the zeroed-out simplified version.
float compensate_temperature(int32_t adc_T) {
  double var1, var2, T;

  var1 = (((double)adc_T) / 16384.0 - ((double)dig_T1) / 1024.0) * ((double)dig_T2);
  var2 = ((((double)adc_T) / 131072.0 - ((double)dig_T1) / 8192.0) *
          (((double)adc_T) / 131072.0 - ((double)dig_T1) / 8192.0)) * ((double)dig_T3);

  T = (var1 + var2) / 5120.0;
  return (float)T;
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial) {}

  read_calibration();
  Serial.print("dig_T1=");
  Serial.print(dig_T1);
  Serial.print(" dig_T2=");
  Serial.print(dig_T2);
  Serial.print(" dig_T3=");
  Serial.println(dig_T3);
}

void loop() {
  if (!trigger_and_wait()) {
    Serial.println("Timed out waiting for conversion.");
    delay(1000);
    return;
  }

  int32_t adc_T = read_raw_adc_t();
  float tempC = compensate_temperature(adc_T);

  Serial.print("raw_adc=");
  Serial.print(adc_T);
  Serial.print(" TEMP:");
  Serial.println(tempC, 2);

  delay(1000);
}
