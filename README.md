# BME280 Firmware + Validation Harness

A small embedded systems project simulating a "firmware + validation" workflow:
an Arduino reads a BME280 temperature sensor over I2C, reports structured
status over UART in response to a request/response protocol, and supports
fault injection for testing - validated by an automated Python test suite,
runnable directly or in a Docker container, that mirrors real hardware
validation practices.

## Phase 1 - Firmware (Arduino / C++)

- `sketches/firmware/firmware.ino` - the main firmware. Reads temperature over
  I2C and packs sensor health into a single status byte using bit flags
  (sensor detected / value in range / error). Nothing is sent unprompted -
  the firmware waits for a serial command: `Q` requests one reading, printed
  as `STATUS:0x03 TEMP:23.4 ERR:NONE`; `F`/`R` inject and clear a simulated
  sensor failure.
- `sketches/i2c_raw_*` - a from-scratch I2C driver built directly on `Wire.h`,
  without the Adafruit_BME280 library: raw register reads/writes, calibration
  data parsing, a trigger+poll measurement cycle, and Bosch's temperature
  compensation formula implemented by hand.
- `sketches/i2c_validate` - runs the raw implementation and the Adafruit
  library side by side to confirm they agree.

## Phase 2 - Validation (Python)

- `python/status_parser.py` - parses firmware status lines into structured
  data.
- `python/test_firmware.py` - a `pytest` suite that connects to the live
  Arduino (directly or via `serial_bridge.py`, see below) and validates:
  status lines parse correctly, temperature is in range, status bits are
  internally consistent, and fault injection correctly triggers and clears an
  `ERR:SENSOR_NOT_FOUND` state.
- `python/conftest.py` - logs every test run's outcome to a local SQLite
  database (`test_results.db`).
- `python/report.py` - prints a pass/fail summary report for the most recent
  test run.
- `python/connect_test.py` - a manual diagnostic tool: polls the firmware
  once a second and prints raw responses, for eyeballing that the board is
  alive and sane before trusting the full automated suite.

## Phase 3 - Containerized validation (Docker)

- `python/Dockerfile` / `python/requirements.txt` / `python/.dockerignore` -
  packages the validation harness into a Docker image. The default command
  runs `status_parser.py`'s hardware-independent self-test, since a container
  has no access to the host's serial ports out of the box.
- `python/serial_bridge.py` - run on the host machine physically wired to the
  Arduino. Opens the real COM port and re-exposes it over a plain TCP socket,
  the same pattern real hardware labs use for network-attached serial/USB
  servers, so a containerized test suite can reach the hardware without any
  device passthrough.
- `python/socket_serial.py` - a `pyserial`-compatible adapter backed by a TCP
  socket instead of a COM port, so `test_firmware.py` can talk to
  `serial_bridge.py` without knowing the difference.

## Running it

1. Upload `sketches/firmware/firmware.ino` to an Arduino wired to a BME280
   over I2C.
2. `pip install -r python/requirements.txt`
3. Update `PORT` in `python/test_firmware.py` (and `python/connect_test.py`)
   to match your board's serial port.
4. `python -m pytest python/test_firmware.py -v`
5. `python python/report.py`

### Running it in Docker

Hardware-independent self-test (no Arduino required):
```
docker build -t bme280-validation python/
docker run --rm bme280-validation
```

Full hardware-in-the-loop suite, via the serial bridge. A container's
filesystem is deleted when it exits (`--rm`), so `test_results.db` is written
to a mounted host folder (`python/data/`) instead of living inside the
container, letting it survive past that container's lifetime:
```
python python/serial_bridge.py                          # on the host, wired to the Arduino

docker run --rm \
  -e SERIAL_BRIDGE_HOST=host.docker.internal \
  -e DB_PATH=/app/data/test_results.db \
  -v "$(pwd)/data:/app/data" \
  bme280-validation python -m pytest test_firmware.py -v

docker run --rm \
  -e DB_PATH=/app/data/test_results.db \
  -v "$(pwd)/data:/app/data" \
  bme280-validation python report.py
```
