#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
BIN="$BUILD/src/websocket_master/websocketserver"
mkdir -p "$BUILD"
cd "$BUILD"
cmake "$ROOT" >/dev/null
make -j"$(nproc 2>/dev/null || echo 4)"

cleanup() {
  kill "${SRV_PID:-}" 2>/dev/null || true
  pkill -f '/websocketserver$' 2>/dev/null || true
  fuser -k 9000/tcp 2>/dev/null || true
  wait "${SRV_PID:-}" 2>/dev/null || true
}
trap cleanup EXIT

pkill -f '/websocketserver$' 2>/dev/null || true
fuser -k 9000/tcp 2>/dev/null || true
sleep 0.2

ulimit -n 65535 2>/dev/null || ulimit -n 32768 2>/dev/null || true

# Phase 1: SO_REUSEPORT multi-worker (default 4). Echo/varlen only — broadcast is per-process.
export WS_MP_WORKERS="${WS_MP_WORKERS:-4}"
export WS_WORKERS="$WS_MP_WORKERS"
"$BIN" &
SRV_PID=$!
sleep "0.3"
# wait for all workers to bind (parent forks quickly)
for _ in {1..50}; do
  if python3 -c "import socket; s=socket.create_connection(('127.0.0.1',9000),2); s.close()" 2>/dev/null; then
    break
  fi
  sleep 0.05
done

python3 "$ROOT/scripts/ws_selftest.py"
chmod +x "$ROOT/scripts/ws_varlen_test.py" 2>/dev/null || true
python3 "$ROOT/scripts/ws_varlen_test.py"

kill "$SRV_PID" 2>/dev/null || true
pkill -f '/websocketserver$' 2>/dev/null || true
fuser -k 9000/tcp 2>/dev/null || true
wait "$SRV_PID" 2>/dev/null || true
sleep 0.25

# Phase 2: single worker — global broadcast tests
export WS_WORKERS=1
"$BIN" &
SRV_PID=$!
sleep 0.5
export WS_LOAD_CLIENTS="${WS_LOAD_CLIENTS:-100}"
python3 "$ROOT/scripts/ws_loadtest.py"

echo "All checks passed."
