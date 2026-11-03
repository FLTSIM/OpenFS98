#!/bin/bash
# Options' harnesses in a private prefix behind a lock: the sim and other modules' tests are left alone.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/options-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/options-diff/wine.lock
flock 9
if [[ "${1:-}" == --breaks ]]; then
    wineserver -p60      # one server across the mutants' builds
    exec python3 -B tools/options_breaks.py "${2:-400}" "${@:3}"
fi
exec timeout 600s wine bin/options-diff/optionseventsdiff.exe "${1:-20000}" "${2:-0x5f1a6c2d}" "${3:-normal}"
