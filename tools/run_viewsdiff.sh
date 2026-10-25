#!/bin/bash
# Views has its own Wine prefix and lock; never changes the game's prefix.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/views-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/views-diff/wine.lock
flock 9
if [[ "${1:-}" == --breaks ]]; then
    exec python3 -B tools/views_breaks.py "${2:-2000}"
fi
if [[ "${1:-}" == --ui-breaks ]]; then
    exec python3 -B tools/views_ui_breaks.py
fi
if [[ "${1:-}" == --smoke ]]; then
    exec python3 -B tools/views_smoke.py "${2:-75}"
fi
if [[ "${1:-}" == --ui ]]; then
    exec timeout 360s wine bin/views-diff/viewsuidiff.exe "${2:-bin/views-diff/VIEWS_ours.dll}" "${3:-24}"
fi
exec timeout 180s wine bin/views-diff/viewseventsdiff.exe "${1:-20000}" "${2:-0x7e51a23b}" "${3:-normal}"
