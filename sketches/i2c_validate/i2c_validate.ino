#include <Wire.h>
#include <Adafruit_BME280.h>

/*
one is an address on the bus (which chip you're talking to), 
the rest are addresses inside the chip (which internal register you're reading/writing). 
It helps to think of the sensor chip like a tiny filing cabinet with numbered drawers, 
and I2C as how you tell it "open drawer 0xFA and hand me what's in it.

The register map — where things like 0x88 (calibration data), 0xF4 (control), 0xF3 (status), 0xFA (temperature) live — is Bosch's design.
 When they designed the chip's internal circuitry, they decided "temperature data will live at this memory location, control settings at that one," and they published that layout in the BME280 datasheet

REG_CTRL_MEAS = 0xF4 — the "control measurement" register. 
You write to this one (not read) to configure and trigger a measurement — 
things like oversampling settings and putting the sensor into forced or normal measurement mode.

REG_STATUS: a status register you read to check what the chip is currently doing, 
e.g., whether it's still busy taking a measurement or 
updating its internal calibration/NVM data — 
useful for knowing when it's safe to read the result instead of grabbing a half-finished measurement

REG_TEMP_MSB = 0xFA — this is where the raw temperature reading itself starts. 
Temperature is stored across three consecutive registers 
(0xFA, 0xFB, 0xFC — most significant, least significant, and extra low bits), 
which together form one raw 20-bit ADC value. 
That raw value is what gets fed into the compensation formula along with the calibration coefficients from 0x88 to produce an actual °C reading.
*/

const uint8_t BME280_ADDR     = 0x76;  // use 0x77 if your scanner found the sensor there instead
const uint8_t REG_CALIB_START = 0x88;
const uint8_t REG_CTRL_MEAS   = 0xF4;
const uint8_t REG_STATUS      = 0xF3;
const uint8_t REG_TEMP_MSB    = 0xFA;

const uint8_t STATUS_MEASURING_BIT = 0x08;

const float TEMP_TOLERANCE = 0.1;  // deg C - max allowed |RAW - LIB| before flagging a mismatch

uint16_t dig_T1;
int16_t  dig_T2;
int16_t  dig_T3;

Adafruit_BME280 bme;

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

bool trigger_and_wait() {
  write_register(REG_CTRL_MEAS, 0x21);
  const unsigned long TIMEOUT_MS = 100;
  unsigned long start = millis();
  uint8_t status;
  do {
    status = read_register(REG_STATUS);
    if (millis() - start > TIMEOUT_MS) return false;
  } while (status & STATUS_MEASURING_BIT);
  return true;
}

int32_t read_raw_adc_t() {
  uint8_t raw[3];
  read_registers(REG_TEMP_MSB, raw, 3);
  return ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
}

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

  read_calibration();  // for the raw path

  if (!bme.begin(BME280_ADDR)) {  // for the library path
    Serial.println("Adafruit_BME280::begin() failed!");
    while (1) {}
  }

  Serial.println("RAW vs LIB comparison starting...");
}

void loop() {
  float rawTemp = NAN;
  if (trigger_and_wait()) {
    int32_t adc_T = read_raw_adc_t();
    rawTemp = compensate_temperature(adc_T);
  }

  float libTemp = bme.readTemperature();
  float diff = rawTemp - libTemp;

  Serial.print("RAW:");
  Serial.print(rawTemp, 2);
  Serial.print("  LIB:");
  Serial.print(libTemp, 2);
  Serial.print("  DIFF:");
  Serial.print(diff, 3);

  if (isnan(rawTemp)) {
    Serial.println("  STATUS:SKIP (measurement timeout)");
  } else if (fabs(diff) <= TEMP_TOLERANCE) {
    Serial.println("  STATUS:PASS");
  } else {
    Serial.println("  STATUS:FAIL");
  }

  delay(1000);
}
