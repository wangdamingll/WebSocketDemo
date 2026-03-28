#!/usr/bin/env python3
"""Async multi-client WebSocket load test: handshake, per-client echo, broadcast."""
from __future__ import annotations

import asyncio
import base64
import hashlib
import os
import random
import struct
import sys
import time

MAGIC = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


async def read_exact(reader: asyncio.StreamReader, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = await reader.read(n - len(buf))
        if not chunk:
            raise ConnectionError("EOF")
        buf += chunk
    return buf


async def recv_http_headers(reader: asyncio.StreamReader) -> bytes:
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = await reader.read(4096)
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


def build_text_frame(payload: bytes) -> bytes:
    b0 = 0x80 | 0x01
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


async def read_ws_frame(reader: asyncio.StreamReader) -> tuple[int, bytes]:
    hdr = await read_exact(reader, 2)
    b0, b1 = hdr[0], hdr[1]
    opcode = b0 & 0x0F
    masked = (b1 >> 7) & 1
    ln = b1 & 0x7F
    if ln == 126:
        ext = await read_exact(reader, 2)
        ln = struct.unpack("!H", ext)[0]
    elif ln == 127:
        ext = await read_exact(reader, 8)
        ln = struct.unpack("!Q", ext)[0]
    mask_key = b""
    if masked:
        mask_key = await read_exact(reader, 4)
    payload = await read_exact(reader, ln)
    if masked:
        payload = bytes(payload[i] ^ mask_key[i % 4] for i in range(len(payload)))
    return opcode, payload


class AsyncWsClient:
    def __init__(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        self.reader = reader
        self.writer = writer

    async def handshake(self, host: str, port: int) -> None:
        key = base64.b64encode(bytes(random.getrandbits(8) for _ in range(16))).decode("ascii")
        req = (
            f"GET / HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n"
            f"\r\n"
        )
        self.writer.write(req.encode("ascii"))
        await self.writer.drain()
        headers = await recv_http_headers(self.reader)
        status_line = headers.split(b"\r\n", 1)[0]
        if b" 101 " not in status_line:
            raise RuntimeError(f"bad status {status_line!r}")
        accept = parse_accept(headers)
        expected = base64.b64encode(
            hashlib.sha1((key.encode("ascii") + MAGIC)).digest()
        ).decode("ascii")
        if accept != expected:
            raise RuntimeError("Sec-WebSocket-Accept mismatch")

    async def send_text(self, text: str) -> None:
        self.writer.write(build_text_frame(text.encode("utf-8")))
        await self.writer.drain()

    async def recv_text(self) -> str:
        opcode, payload = await read_ws_frame(self.reader)
        if opcode == 0x08:
            raise ConnectionError("close frame")
        if opcode != 0x01:
            raise RuntimeError(f"unexpected opcode {opcode}")
        return payload.decode("utf-8", errors="replace")

    def close(self) -> None:
        self.writer.close()

    async def wait_closed(self) -> None:
        await self.writer.wait_closed()


async def open_client(host: str, port: int) -> AsyncWsClient:
    reader, writer = await asyncio.wait_for(
        asyncio.open_connection(host, port), timeout=30.0
    )
    c = AsyncWsClient(reader, writer)
    await asyncio.wait_for(c.handshake(host, port), timeout=30.0)
    return c


async def run_load(host: str, port: int, n: int) -> None:
    sem = asyncio.Semaphore(int(os.environ.get("WS_CONNECT_CONCURRENCY", "500")))

    async def one_connect(i: int) -> AsyncWsClient:
        async with sem:
            return await open_client(host, port)

    t0 = time.perf_counter()
    clients = await asyncio.gather(*[one_connect(i) for i in range(n)])
    t1 = time.perf_counter()
    print(f"connected {n} clients in {t1 - t0:.2f}s")

    full_mesh_max = int(os.environ.get("WS_FULL_MESH_MAX", "200"))
    timeout = float(os.environ.get("WS_OP_TIMEOUT", "120"))

    async def one_broadcast_round(sender: int, label: str) -> None:
        token = f"{label}-{sender}-{random.randint(0, 1_000_000)}"
        await clients[sender].send_text(token)
        got_self = await asyncio.wait_for(clients[sender].recv_text(), timeout=timeout)
        if got_self != token:
            raise AssertionError(f"client {sender} echo mismatch {got_self!r} != {token!r}")
        for j in range(n):
            if j == sender:
                continue
            got_b = await asyncio.wait_for(clients[j].recv_text(), timeout=timeout)
            if got_b != token:
                raise AssertionError(
                    f"client {j} expected broadcast from {sender}: {got_b!r} != {token!r}"
                )

    if n <= full_mesh_max:
        # O(n^2) frames: every client sends once; order sequential so reads match.
        for i in range(n):
            await one_broadcast_round(i, "mesh")
        print(f"phase A: full mesh ({n} senders) OK")
        if n >= 2:
            mark = f"extra-broadcast-{random.randint(0, 1_000_000)}"
            await clients[0].send_text(mark)
            got0 = await asyncio.wait_for(clients[0].recv_text(), timeout=timeout)
            if got0 != mark:
                raise AssertionError(f"phase B echo {got0!r} != {mark!r}")
            for i in range(1, n):
                g = await asyncio.wait_for(clients[i].recv_text(), timeout=timeout)
                if g != mark:
                    raise AssertionError(f"phase B client {i} {g!r} != {mark!r}")
            print("phase B: extra broadcast OK")
    else:
        # Large n: O(n) — two broadcast rounds from different senders
        await one_broadcast_round(0, "scale")
        if n >= 2:
            await one_broadcast_round(n - 1, "scale")
        print(f"phase A (scale mode, n={n}): two broadcast rounds OK")

    for c in clients:
        c.close()
    await asyncio.gather(*[c.wait_closed() for c in clients], return_exceptions=True)
    print("all clients closed cleanly")


def main() -> int:
    host = os.environ.get("WS_TEST_HOST", "127.0.0.1")
    port = int(os.environ.get("WS_TEST_PORT", "9000"))
    n = int(os.environ.get("WS_LOAD_CLIENTS", "100"))
    if n < 1:
        print("WS_LOAD_CLIENTS must be >= 1", file=sys.stderr)
        return 1
    print(f"load test: {n} clients -> {host}:{port}")
    try:
        asyncio.run(run_load(host, port, n))
    except Exception as e:
        print("FAIL:", e, file=sys.stderr)
        return 1
    print("OK: load test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
