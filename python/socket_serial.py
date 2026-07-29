import socket


class SocketSerial:
    """Stands in for serial.Serial, backed by a plain TCP socket instead of a
    local COM port. Talks to serial_bridge.py, which is the thing actually
    holding the real serial connection on the host. Only implements the
    handful of pyserial methods test_firmware.py actually calls.
    """

    def __init__(self, host, port, timeout=2):
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._sock.settimeout(timeout)
        self._buf = b""

    def write(self, data):
        self._sock.sendall(data)

    def readline(self):
        while b"\n" not in self._buf:
            try:
                chunk = self._sock.recv(1024)
            except socket.timeout:
                line, self._buf = self._buf, b""
                return line
            if not chunk:  # bridge closed the connection
                line, self._buf = self._buf, b""
                return line
            self._buf += chunk
        line, sep, self._buf = self._buf.partition(b"\n")
        return line + sep

    def reset_input_buffer(self):
        self._buf = b""

    def close(self):
        self._sock.close()
