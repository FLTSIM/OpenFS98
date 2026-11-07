#!/bin/bash
# SIMSUI's harness in a private prefix behind a lock: the sim and other modules' tests are left alone.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/simsui-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/simsui-diff/wine.lock
flock 9
if [[ "${1:-}" == --breaks ]]; then
    wineserver -p60      # one server across the mutants' builds
    exec python3 -B tools/simsui_breaks.py "${2:-400}" "${@:3}"
fi
exec timeout 3600s wine bin/simsui-diff/simsuidiff.exe "${1:-2000}" "${2:-0x51c5e1}" "${@:3}"
