#!/bin/bash
# Flights' harnesses in a private prefix behind a lock: the sim and other modules' tests are left alone.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/flights-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/flights-diff/wine.lock
flock 9
if [[ "${1:-}" == --breaks ]]; then
    wineserver -p60      # one server across the mutants' builds
    exec python3 -B tools/flights_breaks.py "${2:-400}" "${@:3}"
fi
exec timeout 600s wine bin/flights-diff/flightseventsdiff.exe "${1:-20000}" "${2:-0x5f1a6c2d}" "${3:-normal}"
