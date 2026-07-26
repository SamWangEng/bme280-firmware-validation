# BME280 Firmware + Validation Harness

A small embedded systems project simulating a "firmware + validation" workflow:
an Arduino reads a BME280 temperature sensor over I2C, reports structured
status over UART, and supports fault injection for testing - validated by an
automated Python test suite that mirrors real hardware validation practices.

## Phase 1 - Firmware (Arduino / C++)

- `sketches/firmware/firmware.ino` - the main firmware. Reads temperature over
  I2C, packs sensor health into a single status byte using bit flags (sensor
  detected / value in range / error), and prints structured status lines over
  serial (`STATUS:0x03 TEMP:23.4 ERR:NONE`). Supports a debug command (`F`/`R`)
  to inject and clear a simulated sensor failure.
- `sketches/i2c_raw_*` - a from-scratch I2C driver built directly on `Wire.h`,
  without the Adafruit_BME280 library: raw register reads/writes, calibration
  data parsing, a trigger+poll measurement cycle, and Bosch's temperature
  compensation formula implemented by hand.
- `sketches/i2c_validate` - runs the raw implementation and the Adafruit
  library side by side to confirm they agree.
- `bme280.chip.c` - a Wokwi custom chip simulating a BME280 for testing
  without physical hardware.

## Phase 2 - Validation (Python)

- `python/status_parser.py` - parses firmware status lines into structured
  data.
- `python/test_firmware.py` - a `pytest` suite that connects to the live
  Arduino over serial and validates: status lines parse correctly, temperature
  is in range, status bits are internally consistent, and fault injection
  correctly triggers and clears an `ERR:SENSOR_NOT_FOUND` state.
- `python/conftest.py` - logs every test run's outcome to a local SQLite
  database (`test_results.db`).
- `python/report.py` - prints a pass/fail summary report for the most recent
  test run.

## Running it

1. Flash `sketches/firmware/firmware.ino` to an Arduino wired to a BME280 over
   I2C.
2. `pip install pyserial pytest`
3. Update `PORT` in `python/test_firmware.py` to match your board's serial
   port.
4. `python -m pytest python/test_firmware.py -v`
5. `python python/report.py`
