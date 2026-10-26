#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/world-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/world-diff/wine.lock
flock 9
if [[ "${1:-}" == --init-breaks ]]; then
    exec python3 -B tools/world_init_breaks.py
fi
if [[ "${1:-}" == --init ]]; then
    exec timeout 120s wine bin/world-diff/worldinitdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --ui ]]; then
    exec timeout 180s wine bin/world-diff/worlduidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-24}"
fi
if [[ "${1:-}" == --ui-breaks ]]; then
    exec python3 -B tools/world_ui_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --smoke ]]; then
    exec python3 -B tools/world_smoke.py "${2:-75}"
fi
if [[ "${1:-}" == --life ]]; then
    exec timeout 120s wine bin/world-diff/worldlifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --life-breaks ]]; then
    exec python3 -B tools/world_life_breaks.py
fi
if [[ "${1:-}" == --breaks ]]; then
    exec python3 -B tools/world_breaks.py
fi
exec timeout 120s wine "${WORLD_DIFF_EXE:-bin/world-diff/worlddiff.exe}" "${1:-2000}" "${2:-0x4a971b63}" "${3:-normal}"
