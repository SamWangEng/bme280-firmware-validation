// Wokwi Custom Chip - Simulated BME280 (I2C, temperature-focused)
// For docs and examples see: https://docs.wokwi.com/chips-api/getting-started
//
// This chip emulates the I2C register interface of a Bosch BME280 well
// enough for real driver code (Wire.h, Adafruit_BME280, etc.) to:
//   - detect the chip by reading the correct chip-id byte at 0xD0 (0x60)
//   - read back a temperature you control live via the "Temperature"
//     slider on the chip (defaults to 23.4C to match a typical status
//     line like "TEMP:23.4")
//   - simulate a missing/dead sensor via the "Sensor Present" slider,
//     which makes the chip NACK the bus entirely when set to 0 - useful
//     for exercising your bit-0 "sensor detected" logic against a real
//     I2C failure, separate from any software-side debug override.
//
// Calibration data uses Bosch's published example coefficients for
// dig_T1 / dig_T2, but forces dig_T3 = 0. That makes the (double-based)
// temperature compensation formula used by most Arduino BME280 drivers
// purely linear in the raw ADC value, so this chip can invert it
// EXACTLY and hand back precisely the temperature set on the slider
// (accurate to about 0.01C), regardless of which library you use.
//
// Pressure and humidity registers are filled with static, NOT
// calibrated placeholder data. They exist so begin()/readCoefficients()
// won't choke, but readPressure()/readHumidity() results are not
// meaningful. Only temperature is modeled - which matches Phase 1 scope
// (read temperature, pack status flags, print the status line).
//
// SPDX-License-Identifier: MIT

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BME280_I2C_ADDR   0x76   // SDO tied to GND. Use 0x77 if SDO=VCC.

#define REG_CHIP_ID       0xD0
#define REG_CTRL_HUM      0xF2
#define REG_STATUS        0xF3
#define REG_CTRL_MEAS     0xF4
#define REG_CONFIG        0xF5

// Bosch's published example calibration constants for dig_T1/dig_T2.
// dig_T3 is forced to 0 so the compensation formula collapses to a
// linear function of the raw ADC value, which we invert exactly below.
static const uint16_t DIG_T1 = 27504;
static const int16_t  DIG_T2 = 26435;
static const int16_t  DIG_T3 = 0;

typedef struct {
  uint8_t  regs[256];
  uint8_t  reg_addr;
  bool     addr_phase;   // true = the next byte written sets the register pointer
  uint32_t temp_attr;
  uint32_t present_attr;
} chip_state_t;

static void put_u16(uint8_t *regs, uint8_t addr, uint16_t v) {
  regs[addr]     = v & 0xFF;
  regs[addr + 1] = (v >> 8) & 0xFF;
}

static void put_s16(uint8_t *regs, uint8_t addr, int16_t v) {
  put_u16(regs, addr, (uint16_t)v);
}

static void init_registers(chip_state_t *chip) {
  memset(chip->regs, 0, sizeof(chip->regs));

  // --- Temperature calibration (0x88-0x8D) - the only block we
  //     actually invert / guarantee accuracy for. ---
  put_u16(chip->regs, 0x88, DIG_T1);
  put_s16(chip->regs, 0x8A, DIG_T2);
  put_s16(chip->regs, 0x8C, DIG_T3);

  // --- Pressure calibration (0x8E-0x9F) - placeholder values so
  //     driver init code doesn't choke. Not modeled/inverted. ---
  put_u16(chip->regs, 0x8E, 36477);   // dig_P1
  put_s16(chip->regs, 0x90, -10685);  // dig_P2
  put_s16(chip->regs, 0x92, 3024);    // dig_P3
  put_s16(chip->regs, 0x94, 2855);    // dig_P4
  put_s16(chip->regs, 0x96, 140);     // dig_P5
  put_s16(chip->regs, 0x98, -7);      // dig_P6
  put_s16(chip->regs, 0x9A, 15500);   // dig_P7
  put_s16(chip->regs, 0x9C, -14600);  // dig_P8
  put_s16(chip->regs, 0x9E, 6000);    // dig_P9

  // --- Humidity calibration (0xA1, 0xE1-0xE7) - placeholders. ---
  chip->regs[0xA1] = 75;  // dig_H1
  put_s16(chip->regs, 0xE1, 382); // dig_H2
  chip->regs[0xE3] = 0;   // dig_H3
  chip->regs[0xE4] = 0;   // dig_H4 high bits
  chip->regs[0xE5] = 0;   // dig_H4/H5 shared nibble byte
  chip->regs[0xE6] = 0;   // dig_H5 high bits
  chip->regs[0xE7] = 30;  // dig_H6

  chip->regs[REG_CHIP_ID] = 0x60; // BME280 chip-id ("WHO_AM_I")

  chip->regs[REG_CTRL_HUM]  = 0x01;
  chip->regs[REG_STATUS]    = 0x00; // always "conversion done"
  chip->regs[REG_CTRL_MEAS] = 0x27;
  chip->regs[REG_CONFIG]    = 0xA0;

  // Static placeholder pressure/humidity raw data. Not calibrated
  // against the placeholder coefficients above - don't trust
  // readPressure()/readHumidity() output.
  chip->regs[0xF7] = 0x5E; chip->regs[0xF8] = 0x2E; chip->regs[0xF9] = 0x00;
  chip->regs[0xFD] = 0x80; chip->regs[0xFE] = 0x00;
}

// Recomputes the raw temperature registers (0xFA-0xFC) so they decode
// back to attr_read_float(chip->temp_attr) degrees C, using the same
// double-precision formula most Arduino BME280 drivers use internally
// (e.g. Adafruit_BME280::readTemperature()):
//
//   T = ((adc_T/16384 - dig_T1/1024) * dig_T2) / 5120      [since dig_T3 == 0]
//
// Solved for adc_T:
//
//   adc_T = 16384 * (T*5120/dig_T2 + dig_T1/1024)
static void update_temperature(chip_state_t *chip) {
  double target_c = attr_read_float(chip->temp_attr);

  double adc_t = 16384.0 * ((target_c * 5120.0) / (double)DIG_T2 + (double)DIG_T1 / 1024.0);

  int32_t adc_t_i = (int32_t)adc_t;
  if (adc_t_i < 0) adc_t_i = 0;
  if (adc_t_i > 0xFFFFF) adc_t_i = 0xFFFFF; // raw value is 20 bits wide

  chip->regs[0xFA] = (adc_t_i >> 12) & 0xFF; // temp_msb
  chip->regs[0xFB] = (adc_t_i >> 4)  & 0xFF; // temp_lsb
  chip->regs[0xFC] = (adc_t_i << 4)  & 0xF0; // temp_xlsb (top nibble only)
}

static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  chip_state_t *chip = (chip_state_t*)user_data;

  // "Sensor Present" slider: set to 0 to simulate an unplugged/dead
  // sensor. We NACK every transaction, just like real hardware that
  // isn't answering on the bus.
  if (attr_read(chip->present_attr) == 0) {
    return false;
  }

  if (read) {
    update_temperature(chip);
  }

  chip->addr_phase = true; // next written byte (if any) sets the register pointer
  return true; // ACK
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  uint8_t value = chip->regs[chip->reg_addr];
  chip->reg_addr++; // auto-increment, matches real BME280 burst reads
  return value;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t*)user_data;

  if (chip->addr_phase) {
    chip->reg_addr = data;
    chip->addr_phase = false;
  } else {
    chip->regs[chip->reg_addr] = data;
    chip->reg_addr++;
  }
  return true; // ACK
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->reg_addr = 0;
  chip->addr_phase = false;

  init_registers(chip);

  chip->temp_attr    = attr_init_float("temperature", 23.4f);
  chip->present_attr = attr_init("sensorPresent", 1);

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = BME280_I2C_ADDR,
    .scl = pin_init("SCL", INPUT_PULLUP),
    .sda = pin_init("SDA", INPUT_PULLUP),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
  };
  i2c_init(&i2c_config);

  printf("BME280 sim chip ready (addr 0x%02X)\n", BME280_I2C_ADDR);
}
