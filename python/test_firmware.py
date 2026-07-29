import os
import time

import pytest
import serial

from socket_serial import SocketSerial
from status_parser import StatusLine, parse_status_line

PORT = "COM3"  # change to match whatever port the Arduino IDE showed you
BAUD = 9600

# If set, connect to serial_bridge.py over TCP instead of opening a local COM
# port - this is how the container reaches hardware attached to the host.
BRIDGE_HOST = os.environ.get("SERIAL_BRIDGE_HOST")
BRIDGE_PORT = int(os.environ.get("SERIAL_BRIDGE_PORT", "9000"))

BIT_SENSOR_DETECTED = 0x01
BIT_VALUE_IN_RANGE = 0x02
BIT_ERROR = 0x04


@pytest.fixture(scope="module")
def serial_conn():
    if BRIDGE_HOST:
        ser = SocketSerial(BRIDGE_HOST, BRIDGE_PORT, timeout=2)
    else:
        ser = serial.Serial(PORT, BAUD, timeout=2)
    time.sleep(2)  # give time to let the Arduino finish resetting after the port opens
    ser.reset_input_buffer()  # discard any stale/partial data from before we connected
    yield ser #Hands the now-ready ser object over to whichever test function requested it — this is the "setup is done, here's the resource" moment.
    ser.close() # Runs only after all tests using this fixture have finished — closes the serial connection cleanly, freeing up the COM port
"""
With scope="module", the setup/teardown cycle runs only once total, shared across all test functions in that file — not once per test. Setup runs the first time any test in the file needs serial_conn, and teardown only runs once, after the last test in the file finishes.

So the actual sequence for your file is:
setup (open port, wait, clear buffer)   ← runs once
  → test_status_line_is_parseable        ← reuses the same connection
  → test_temperature_in_plausible_range  ← reuses the same connection
  → test_sensor_detected_bit_matches_err_field ← reuses the same connection
  → test_healthy_reading_has_consistent_bits   ← reuses the same connection
teardown (close port)                    ← runs once, at the very end

If you'd used the default scope="function" instead, that's when you'd get "every test function runs through its own full cycle" — pytest would open a brand-new serial connection, wait 2 seconds, clear the buffer, run one test, then immediately close it — and repeat that entire process from scratch for each of the 4 tests. That would work too, but be 4x slower (four separate 2-second Arduino reboots) and unnecessarily reopen a resource that's perfectly fine to share.
"""

def read_parsed_line(ser) -> StatusLine:
    # firmware.ino only sends a reading in response to 'Q' - nothing is pushed unprompted
    ser.write(b"Q")
    raw = ser.readline().decode("utf-8", errors="replace").strip() # Converts those raw bytes into an actual readable Python string
    return parse_status_line(raw)


def test_status_line_is_parseable(serial_conn, record_property):
    line = read_parsed_line(serial_conn)
    record_property("raw_reading", repr(line)) # "put this content (repr(line)) into a jar, and write 'raw_reading' on the label."
    assert isinstance(line, StatusLine)


def test_temperature_in_plausible_range(serial_conn, record_property):
    line = read_parsed_line(serial_conn)
    record_property("raw_reading", repr(line))
    assert line.temp is not None
    # matches the same operating-spec sanity check firmware.ino itself uses
    assert -40.0 <= line.temp <= 85.0


def test_sensor_detected_bit_matches_err_field(serial_conn, record_property):
    line = read_parsed_line(serial_conn)
    record_property("raw_reading", repr(line))
    sensor_detected = bool(line.status & BIT_SENSOR_DETECTED)
    if sensor_detected:
        assert line.err != "SENSOR_NOT_FOUND"
    else:
        assert line.err == "SENSOR_NOT_FOUND"


def test_healthy_reading_has_consistent_bits(serial_conn, record_property):
    line = read_parsed_line(serial_conn)
    record_property("raw_reading", repr(line))
    if line.err == "NONE":
        assert line.status & BIT_SENSOR_DETECTED
        assert line.status & BIT_VALUE_IN_RANGE
        assert not (line.status & BIT_ERROR)


def test_consecutive_readings_are_stable(serial_conn, record_property):
    """Catches a sensor glitching wildly between readings, one second apart."""
    readings = []
    for _ in range(3):
        readings.append(read_parsed_line(serial_conn))
        time.sleep(1)  # readings are now polled on demand, so space them out ourselves
    record_property("raw_reading", ", ".join(repr(r) for r in readings))

    temps = [r.temp for r in readings if r.temp is not None]
    assert len(temps) >= 2, "not enough healthy readings to compare"

    for earlier, later in zip(temps, temps[1:]):
        assert abs(later - earlier) < 2.0, f"temperature jumped from {earlier} to {later}"


def wait_for_err(ser, expected_err, max_lines=10):
    """Polls for readings until one matches expected_err, or fails after max_lines tries.

    firmware.ino applies 'F'/'R' synchronously, so a 'Q' sent right after should
    already reflect it - this retry loop is just defensive margin against
    latency, not a requirement of the protocol. Non-STATUS lines (e.g. the
    firmware's own "[debug] ..." prints, echoed back right after we send a
    command) are skipped rather than treated as failures.
    """
    for _ in range(max_lines):
        ser.write(b"Q")
        raw = ser.readline().decode("utf-8", errors="replace").strip()
        try:
            line = parse_status_line(raw)
        except ValueError:
            continue  # not a STATUS line (e.g. a "[debug] ..." line) - skip it
        if line.err == expected_err:
            return line
    pytest.fail(f"did not observe ERR:{expected_err} within {max_lines} lines")


# NOTE: these two tests are stateful and order-dependent (fault injection
# persists on the device until cleared) - they rely on running in this order,
# top to bottom, which is pytest's default within a single file.

def test_fault_injection_forces_sensor_not_found(serial_conn, record_property):
    serial_conn.write(b"F")
    line = wait_for_err(serial_conn, "SENSOR_NOT_FOUND")
    record_property("raw_reading", repr(line))

    assert line.temp is None
    assert not (line.status & BIT_SENSOR_DETECTED)
    assert line.status & BIT_ERROR


def test_fault_clear_restores_normal_reading(serial_conn, record_property):
    serial_conn.write(b"R")
    line = wait_for_err(serial_conn, "NONE")
    record_property("raw_reading", repr(line))

    assert line.temp is not None
    assert line.status & BIT_SENSOR_DETECTED
    assert line.status & BIT_VALUE_IN_RANGE
    assert not (line.status & BIT_ERROR)
