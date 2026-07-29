FROM python:3.12-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

# Default: run the hardware-independent parser self-test, which needs no
# serial device access and works out of the box with a plain `docker run`.
#
# The hardware-in-the-loop suite (test_firmware.py) needs a real Arduino.
# Run serial_bridge.py on the host machine that's physically wired to the
# board first (it re-exposes the COM port over TCP), then point the
# container at it - no device passthrough needed:
#   python serial_bridge.py                        (on the host, outside Docker)
#   docker run -e SERIAL_BRIDGE_HOST=host.docker.internal <image> python -m pytest test_firmware.py -v
CMD ["python", "status_parser.py"]
