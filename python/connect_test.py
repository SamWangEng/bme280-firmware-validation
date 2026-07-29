import time

import serial

PORT = "COM3"  # change to match whatever port the Arduino IDE showed you
BAUD = 9600

def main():
    # TODO: retry with backoff if the port is busy/locked instead of just
    # crashing - not finished, ran out of time to test this properly.
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Connected to {PORT} at {BAUD} baud. Polling for readings (Ctrl+C to stop)...")

    try:
        while True:
            # firmware.ino only sends a reading in response to 'Q'
            ser.write(b"Q")
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                print(line)
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
