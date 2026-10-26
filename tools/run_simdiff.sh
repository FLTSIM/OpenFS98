#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/sim-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/sim-diff/wine.lock
flock 9
if [[ "${1:-}" == --breaks ]]; then
    wineserver -p60
    exec python3 -B tools/sim_breaks.py "${2:-100}" "${@:3}"
fi
exec timeout 300s wine bin/sim-diff/simdiff.exe "${1:-20000}" "${2:-0x73a915e2}" "${3:-normal}"
