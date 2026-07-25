import serial

PORT = "COM3"  # change to match whatever port the Arduino IDE showed you
BAUD = 9600

def main():
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Connected to {PORT} at {BAUD} baud. Reading lines (Ctrl+C to stop)...")

    try:
        while True:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if line:
                print(line)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
