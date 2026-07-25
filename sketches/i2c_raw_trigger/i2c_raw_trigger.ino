#include <Wire.h>

const uint8_t BME280_ADDR   = 0x76;  // use 0x77 if your scanner found the sensor there instead
const uint8_t REG_CTRL_MEAS = 0xF4; // Ctrlol Measurement register
const uint8_t REG_STATUS    = 0xF3;  // reports the chip's current state (is a conversion running right now? is internal calibration data still being copied?)
const uint8_t REG_TEMP_MSB  = 0xFA;  // raw temp data starts here (0xFA-0xFC, 3 bytes) //  Temperature Most Significant Byte

const uint8_t STATUS_MEASURING_BIT = 0x08;  // bit 3: 1 = conversion in progress

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

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial) {}

  // Trigger one forced-mode measurement 0x21: osrs_t=x1, osrs_p=skip, mode=forced
  /*
1. I2C transaction happens: your Arduino sends a start condition, addresses the chip at 0x76, writes the register address 0xF4 (ctrl_meas), then writes the data byte 0x21, then stops. The BME280 acknowledges each byte along the way.
2. The chip decodes that byte into its three settings: osrs_t = 001 (temperature oversampling ×1), osrs_p = 000 (skip pressure), mode = 01 (forced).
3. The chip wakes up from sleep (it was idle/asleep before this write) and begins a measurement cycle, since mode = forced is a command to run exactly one conversion right now.
4. Internally, it sets its own measuring bit to 1 in the status register (0xF3, bit 3) — this is the chip signaling "I'm busy," which is exactly what your polling loop is watching for.
5. The actual physical sensing happens: the chip's temperature-sensing element produces an analog signal, the ADC samples it (once, since oversampling is ×1) and digitizes it into a raw 20-bit value.
6. That raw value gets written into the raw temperature registers (0xFA–0xFC) — this is new data, overwriting whatever was there before.
7. The chip clears the measuring bit back to 0 — signaling "done," which is what breaks your polling loop out of its wait.
8. The chip automatically returns to sleep mode — this is specific to forced mode: unlike normal mode (which would keep measuring repeatedly on a timer), forced mode is a one-shot — after finishing this single conversion, the chip goes back to idle and won't measure again until you write ctrl_meas with mode = forced a second time.
  */
  write_register(REG_CTRL_MEAS, 0x21); 
  Serial.println("Measurement triggered.");

  // Poll the status register until the "measuring" bit clears, with a timeout
  // so a misbehaving sensor can't hang the firmware forever.
  const unsigned long TIMEOUT_MS = 100;
  unsigned long start = millis(); //millis() returns how many milliseconds have passed since the Arduino booted up.
  uint8_t status;
  bool timed_out = false;

  do {
    status = read_register(REG_STATUS);
    if (millis() - start > TIMEOUT_MS) {
      timed_out = true;
      break;
    }
  } while (status & STATUS_MEASURING_BIT); 

  if (timed_out) {
    Serial.println("Timed out waiting for conversion - sensor not responding as expected.");
    return;
  }

  unsigned long elapsed = millis() - start;
  Serial.print("Conversion complete after ");
  Serial.print(elapsed);
  Serial.println(" ms.");

  // Peek at the raw (not-yet-compensated) temperature bytes to confirm fresh data exists.
  uint8_t raw[3];
  read_registers(REG_TEMP_MSB, raw, 3);

  Serial.print("Raw temp bytes: 0x");
  Serial.print(raw[0], HEX);
  Serial.print(" 0x");
  Serial.print(raw[1], HEX);
  Serial.print(" 0x");
  Serial.println(raw[2], HEX);
}

void loop() {
  // one-shot test - nothing to repeat
}
