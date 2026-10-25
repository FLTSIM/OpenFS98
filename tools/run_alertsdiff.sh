#!/bin/bash
# Dedicated Wine state: never shares the game's prefix or other harness output.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/alerts-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
if [[ "${1:-}" == --breaks ]]; then
    exec python3 -B tools/alerts_breaks.py "${2:-2000}"
fi
exec timeout 180s wine bin/alerts-diff/alertseventsdiff.exe "${1:-20000}" "${2:-0x41e2753b}"
