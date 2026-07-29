"""Run this on the host that's physically wired to the Arduino (outside
Docker). It opens the real COM port and re-exposes it as a plain TCP socket,
so a containerized test suite - which has no access to Windows COM ports -
can reach the hardware over the network instead. Same pattern real hardware
labs use for rack-mounted or remote DUTs (device under test).
"""
import socket
import threading

import serial

SERIAL_PORT = "COM3"
BAUD = 9600
LISTEN_HOST = "0.0.0.0"
LISTEN_PORT = 9000

#  the Arduino→network direction
def forward_serial_to_socket(ser, conn, stop_event):
    try:
        while not stop_event.is_set():
            data = ser.read(ser.in_waiting or 1)  # ser has timeout=1, so this re-checks stop_event at least once/sec
            if data:
                conn.sendall(data)
    except (OSError, serial.SerialException):
        pass

# the network→Arduino direction
def forward_socket_to_serial(conn, ser, stop_event):
    try:
        while True:
            data = conn.recv(1024)
            if not data:  # client disconnected
                break
            ser.write(data)
    except OSError:
        pass
    finally:
        stop_event.set()  # tell the other thread this client is done, so it doesn't linger and race the next one


def main():
    ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
    print(f"Opened {SERIAL_PORT} at {BAUD} baud.")

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((LISTEN_HOST, LISTEN_PORT))
    listener.listen(1)
    listener.settimeout(1.0)  # so accept() below periodically gives Ctrl+C a chance to register
    print(f"Listening on {LISTEN_HOST}:{LISTEN_PORT} - waiting for a client...")

    try:
        while True:
            try:
                conn, addr = listener.accept()
            except socket.timeout:
                continue  # nobody connected in the last second - just try again
            print(f"Client connected from {addr}")
            ser.reset_input_buffer()  

            stop_event = threading.Event()
            to_socket = threading.Thread(target=forward_serial_to_socket, args=(ser, conn, stop_event), daemon=True)
            to_serial = threading.Thread(target=forward_socket_to_serial, args=(conn, ser, stop_event), daemon=True)
            to_socket.start()
            to_serial.start()
            to_serial.join()  # blocks until this client disconnects
            stop_event.set()  # in case to_serial ended via an exception before its own finally could set it - harmless if already set
            to_socket.join(timeout=2)  # wait for the old reader to actually exit before accepting a new client
            print("Client disconnected, waiting for next connection...")
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()
        listener.close()    


if __name__ == "__main__":
    main()
