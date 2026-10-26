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
if [[ "${1:-}" == --ui ]]; then
    exec timeout 360s wine bin/help-diff/helpuidiff.exe "${2:-bin/help-diff/HELP_ours.dll}" "${3:-30}"
fi
if [[ "${1:-}" == --ui-breaks ]]; then
    exec python3 -B tools/help_ui_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --smoke ]]; then
    exec python3 -B tools/help_smoke.py "${2:-75}"
fi
if [[ "${1:-}" == --breaks ]]; then
    # Keep this private server alive across compiler gaps between mutants.
    wineserver -p60
    exec python3 -B tools/help_breaks.py "${2:-200}"
fi
exec timeout 240s wine bin/help-diff/helpeventsdiff.exe "${1:-20000}" "${2:-0x19a847e3}" "${3:-normal}"
