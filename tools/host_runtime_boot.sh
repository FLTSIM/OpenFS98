#!/usr/bin/env bash
# Bounded development boot of the candidate in a disposable game and prefix.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
out=$root/bin/host-runtime
mkdir -p "$out"
[ -d "$out/game" ] || cp -a --reflink=auto "${FS98_DIR:-game}" "$out/game"
[ -d "$out/prefix" ] || cp -a --reflink=auto "${WINEPREFIX:-$HOME/.wine}" "$out/prefix"
cp bin/OPENFS98.EXE "$out/game/OPENFS98.EXE"
export WINEPREFIX=$out/prefix WINEDEBUG=${HOST_RUNTIME_DEBUG:--all} DISPLAY=${HOST_RUNTIME_DISPLAY:-:69}
export WINEDLLOVERRIDES=dsound=d
Xvfb "$DISPLAY" -screen 0 1024x768x24 -ac -noreset > "$out/xvfb.log" 2>&1 &
host_xvfb_pid=$!
trap 'wineserver -k 2>/dev/null || true; kill "$host_xvfb_pid" 2>/dev/null || true' EXIT
for _ in {1..20}; do xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
set +e
(cd "$out/game" && timeout "${HOST_RUNTIME_TIMEOUT:-40}" wine OPENFS98.EXE '/STN:Situatio\STGEORGE.STN') > "$out/boot.log" 2>&1
host_status=$?
set -e
cat "$out/boot.log"
exit "$host_status"
