#!/bin/bash
# Private prefix and lock: leave the sim and other module tests alone.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/help-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/help-diff/wine.lock
flock 9
if [[ "${1:-}" == --breaks ]]; then
    exec python3 -B tools/help_breaks.py "${2:-200}"
fi
exec timeout 240s wine bin/help-diff/helpeventsdiff.exe "${1:-20000}" "${2:-0x19a847e3}" "${3:-normal}"
