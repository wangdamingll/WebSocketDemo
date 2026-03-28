#!/usr/bin/env python3
"""
Strict variable-length WebSocket text tests (echo + broadcast on same worker).
Use WS_VARLEN_CONCURRENCY=1 (default): parallel clients would see other users' broadcasts first.
Compatible with SO_REUSEPORT workers: echo is always correct per connection; ordering is strict.
"""
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

# Below server WS_MAX_PAYLOAD_BYTES (1MiB); leave margin for framing
MAX_PAYLOAD = int(os.environ.get("WS_VARLEN_MAX", str(768 * 1024)))


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
        chunk = await reader.read(65536)
        if not chunk:
            raise ConnectionError("EOF during HTTP response")
        buf += chunk
        if len(buf) > 262144:
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


def ascii_payload(exact_len: int, salt: int) -> bytes:
    """Exactly `exact_len` bytes, valid UTF-8 (ASCII)."""
    head = f"{salt}:".encode("ascii")
    if exact_len <= 0:
        return b""
    if exact_len <= len(head):
        return head[:exact_len]
    return head + b"*" * (exact_len - len(head))


class Client:
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

    async def send_text_bytes(self, payload: bytes) -> None:
        self.writer.write(build_text_frame(payload))
        await self.writer.drain()

    async def recv_text_bytes(self) -> bytes:
        opcode, payload = await read_ws_frame(self.reader)
        if opcode == 0x08:
            raise ConnectionError("close frame")
        if opcode != 0x01:
            raise RuntimeError(f"unexpected opcode {opcode}")
        return payload

    def close(self) -> None:
        self.writer.close()

    async def wait_closed(self) -> None:
        await self.writer.wait_closed()


async def open_client(host: str, port: int) -> Client:
    reader, writer = await asyncio.wait_for(
        asyncio.open_connection(host, port), timeout=60.0
    )
    c = Client(reader, writer)
    await asyncio.wait_for(c.handshake(host, port), timeout=60.0)
    return c


def default_lengths() -> list[int]:
    xs = [
        0,
        1,
        2,
        124,
        125,
        126,
        127,
        255,
        256,
        1023,
        1024,
        1025,
        8191,
        8192,
        32767,
        32768,
        65535,
        65536,
        100000,
        256 * 1024,
        512 * 1024,
    ]
    return [x for x in xs if x <= MAX_PAYLOAD]


async def run_varlen(host: str, port: int) -> None:
    lengths = default_lengths()
    if not lengths:
        raise RuntimeError("no lengths to test")
    timeout = float(os.environ.get("WS_VARLEN_TIMEOUT", "600"))
    conc = int(os.environ.get("WS_VARLEN_CONCURRENCY", "1"))

    print(f"varlen: {len(lengths)} distinct sizes up to {max(lengths)} bytes (parallelism={conc})")

    sem = asyncio.Semaphore(conc)

    async def one_case(idx: int, n: int) -> None:
        body = ascii_payload(n, idx)
        assert len(body) == n, (idx, n, len(body))
        async with sem:
            c = await open_client(host, port)
        try:
            await asyncio.wait_for(c.send_text_bytes(body), timeout=timeout)
            got = await asyncio.wait_for(c.recv_text_bytes(), timeout=timeout)
            if got != body:
                raise AssertionError(
                    f"len={n} idx={idx}: echo mismatch len(got)={len(got)} expected={n}"
                )
        finally:
            c.close()
            await asyncio.wait_for(c.wait_closed(), timeout=30.0)

    t0 = time.perf_counter()
    await asyncio.gather(*(one_case(i, L) for i, L in enumerate(lengths)))
    t1 = time.perf_counter()
    print(f"varlen: all {len(lengths)} sizes passed in {t1 - t0:.2f}s")

    # Second pass: one connection, chained messages (header length boundaries)
    chain = [0, 125, 126, 65535, 65536, min(200000, MAX_PAYLOAD)]
    chain = sorted(set(x for x in chain if x <= MAX_PAYLOAD))
    c = await open_client(host, port)
    try:
        for j, n in enumerate(chain):
            body = ascii_payload(n, 9000 + j)
            await asyncio.wait_for(c.send_text_bytes(body), timeout=timeout)
            got = await asyncio.wait_for(c.recv_text_bytes(), timeout=timeout)
            if got != body:
                raise AssertionError(f"chain len={n}: mismatch")
        print(f"varlen: chained {len(chain)} messages on one connection OK")
    finally:
        c.close()
        await asyncio.wait_for(c.wait_closed(), timeout=30.0)

    # Parallel clients + different lengths: drain broadcasts until own echo matches.
    if os.environ.get("WS_VARLEN_PARALLEL_ROUND", "1") != "1":
        return
    par_lengths = [1, 15, 125, 126, 1023, 1024, 4095, 4096]
    par_conc = int(os.environ.get("WS_VARLEN_PAR_CONC", "24"))
    sem = asyncio.Semaphore(par_conc)

    async def parallel_echo(i: int, n: int) -> None:
        body = ascii_payload(n, 50000 + i)
        assert len(body) == n
        async with sem:
            c = await open_client(host, port)
        try:
            await asyncio.wait_for(c.send_text_bytes(body), timeout=timeout)
            deadline = time.monotonic() + timeout
            while True:
                left = deadline - time.monotonic()
                if left <= 0:
                    raise TimeoutError(f"parallel idx={i} len={n}: no matching echo")
                got = await asyncio.wait_for(c.recv_text_bytes(), timeout=left)
                if got == body:
                    return
        finally:
            c.close()
            await asyncio.wait_for(c.wait_closed(), timeout=30.0)

    await asyncio.gather(*(parallel_echo(i, L) for i, L in enumerate(par_lengths)))
    print(f"varlen: parallel round ({len(par_lengths)} clients, drain-until-echo) OK")


def main() -> int:
    host = os.environ.get("WS_TEST_HOST", "127.0.0.1")
    port = int(os.environ.get("WS_TEST_PORT", "9000"))
    print(f"varlen test -> {host}:{port}")
    try:
        asyncio.run(run_varlen(host, port))
    except Exception as e:
        print("FAIL:", e, file=sys.stderr)
        return 1
    print("OK: varlen test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
