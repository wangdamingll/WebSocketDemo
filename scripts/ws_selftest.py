#!/usr/bin/env python3
"""Minimal RFC6455 client (stdlib only): handshake + text echo against localhost:9000."""
from __future__ import annotations

import base64
import hashlib
import random
import socket
import struct
import sys
import time

MAGIC = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
HOST = "127.0.0.1"
PORT = 9000


def recv_http_headers(sock: socket.socket) -> bytes:
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("EOF during HTTP response")
        buf += chunk
        if len(buf) > 65536:
            raise ValueError("HTTP response too large")
    return buf


def parse_accept(headers: bytes) -> str | None:
    text = headers.decode("latin-1", errors="replace")
    for line in text.split("\r\n"):
        if line.lower().startswith("sec-websocket-accept:"):
            return line.split(":", 1)[1].strip()
    return None


def build_text_frame_client(payload: bytes) -> bytes:
    """Client->server frame: FIN+text, masked."""
    b0 = 0x80 | 0x01  # FIN, opcode text
    n = len(payload)
    if n < 126:
        b1 = 0x80 | n
        header = struct.pack("!BB", b0, b1)
    elif n < 65536:
        b1 = 0x80 | 126
        header = struct.pack("!BBH", b0, b1, n)
    else:
        b1 = 0x80 | 127
        header = struct.pack("!BBQ", b0, b1, n)
    mask = bytes(random.getrandbits(8) for _ in range(4))
    masked = bytes(payload[i] ^ mask[i % 4] for i in range(len(payload)))
    return header + mask + masked


def read_ws_frame(sock: socket.socket) -> tuple[int, bytes]:
    """Read one server->client frame (assumes small payload, unmasked)."""
    hdr = b""
    while len(hdr) < 2:
        c = sock.recv(2 - len(hdr))
        if not c:
            raise ConnectionError("EOF reading frame header")
        hdr += c
    b0, b1 = hdr[0], hdr[1]
    opcode = b0 & 0x0F
    masked = (b1 >> 7) & 1
    ln = b1 & 0x7F
    off = 2
    if ln == 126:
        ext = sock.recv(2)
        if len(ext) != 2:
            raise ConnectionError("EOF extended length 16")
        ln = struct.unpack("!H", ext)[0]
    elif ln == 127:
        ext = sock.recv(8)
        if len(ext) != 8:
            raise ConnectionError("EOF extended length 64")
        ln = struct.unpack("!Q", ext)[0]
    mask_key = b""
    if masked:
        mask_key = sock.recv(4)
        if len(mask_key) != 4:
            raise ConnectionError("EOF mask")
    payload = b""
    while len(payload) < ln:
        c = sock.recv(ln - len(payload))
        if not c:
            raise ConnectionError("EOF payload")
        payload += c
    if masked:
        payload = bytes(payload[i] ^ mask_key[i % 4] for i in range(len(payload)))
    return opcode, payload


def main() -> int:
    deadline = time.time() + 5.0
    while time.time() < deadline:
        try:
            s = socket.create_connection((HOST, PORT), timeout=2.0)
            break
        except OSError:
            time.sleep(0.05)
    else:
        print("FAIL: could not connect to", HOST, PORT, file=sys.stderr)
        return 1

    try:
        s.settimeout(5.0)
        key = base64.b64encode(bytes(random.getrandbits(8) for _ in range(16))).decode("ascii")
        req = (
            f"GET / HTTP/1.1\r\n"
            f"Host: {HOST}:{PORT}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n"
            f"\r\n"
        )
        s.sendall(req.encode("ascii"))
        headers = recv_http_headers(s)
        status_line = headers.split(b"\r\n", 1)[0]
        if b" 101 " not in status_line:
            print("FAIL: expected 101, got:", status_line, file=sys.stderr)
            return 1
        accept = parse_accept(headers)
        expected = base64.b64encode(
            hashlib.sha1((key.encode("ascii") + MAGIC)).digest()
        ).decode("ascii")
        if accept != expected:
            print("FAIL: Sec-WebSocket-Accept mismatch", accept, expected, file=sys.stderr)
            return 1

        msg = b"echo-check-\xe2\x9c\x93"  # UTF-8 to ensure binary path OK
        s.sendall(build_text_frame_client(msg))
        opcode, payload = read_ws_frame(s)
        if opcode != 0x01:
            print("FAIL: expected text opcode 1, got", opcode, file=sys.stderr)
            return 1
        if payload != msg:
            print("FAIL: echo mismatch", payload, msg, file=sys.stderr)
            return 1

        print("OK: handshake + text echo")
        return 0
    finally:
        try:
            s.close()
        except OSError:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
