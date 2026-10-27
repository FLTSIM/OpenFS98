#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/world-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/world-diff/wine.lock
flock 9
if [[ "${1:-}" == --files-breaks ]]; then
    exec python3 -B tools/world_files_breaks.py
fi
if [[ "${1:-}" == --files-life-breaks ]]; then
    exec python3 -B tools/world_files_life_breaks.py
fi
if [[ "${1:-}" == --files-native-breaks ]]; then
    exec python3 -B tools/world_files_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --files-ui ]]; then
    exec timeout 240s wine bin/world-diff/worldfilesuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-28}"
fi
if [[ "${1:-}" == --files-core ]]; then
    exec timeout 120s wine bin/world-diff/worldfilesdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --files-life ]]; then
    exec timeout 120s wine bin/world-diff/worldfileslifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --cache-breaks ]]; then
    exec python3 -B tools/world_cache_breaks.py
fi
if [[ "${1:-}" == --cache-life-breaks ]]; then
    exec python3 -B tools/world_cache_life_breaks.py
fi
if [[ "${1:-}" == --cache-native-breaks ]]; then
    exec python3 -B tools/world_cache_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --cache-ui ]]; then
    exec timeout 240s wine bin/world-diff/worldcacheuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-28}"
fi
if [[ "${1:-}" == --cache-life ]]; then
    exec timeout 120s wine bin/world-diff/worldcachelifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --cache-core ]]; then
    exec timeout 120s wine bin/world-diff/worldcachediff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --list-ui ]]; then
    exec timeout 240s wine bin/world-diff/worldlistuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-21}"
fi
if [[ "${1:-}" == --list-native-breaks ]]; then
    exec python3 -B tools/world_list_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --list-life ]]; then
    exec timeout 120s wine bin/world-diff/worldlistlifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --list-life-breaks ]]; then
    exec python3 -B tools/world_list_life_breaks.py
fi
if [[ "${1:-}" == --list-core ]]; then
    exec timeout 120s wine bin/world-diff/worldlistdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --list-breaks ]]; then
    exec python3 -B tools/world_list_breaks.py
fi
if [[ "${1:-}" == --search-ui ]]; then
    exec timeout 180s wine bin/world-diff/worldsearchuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-20}"
fi
if [[ "${1:-}" == --search-native-breaks ]]; then
    exec python3 -B tools/world_search_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --search-init ]]; then
    exec timeout 120s wine bin/world-diff/worldsearchinitdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --search-init-breaks ]]; then
    exec python3 -B tools/world_search_init_breaks.py
fi
if [[ "${1:-}" == --search-life ]]; then
    exec timeout 120s wine bin/world-diff/worldsearchlifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --search-life-breaks ]]; then
    exec python3 -B tools/world_search_life_breaks.py
fi
if [[ "${1:-}" == --search-core ]]; then
    exec timeout 120s wine bin/world-diff/worldsearchdiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --search-breaks ]]; then
    exec python3 -B tools/world_search_breaks.py
fi
if [[ "${1:-}" == --booster-native-breaks ]]; then
    exec python3 -B tools/world_booster_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --booster-ui ]]; then
    exec timeout 180s wine bin/world-diff/worldboosteruidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-16}"
fi
if [[ "${1:-}" == --booster-init ]]; then
    exec timeout 120s wine bin/world-diff/worldboosterinitdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --booster-life-breaks ]]; then
    exec python3 -B tools/world_booster_life_breaks.py
fi
if [[ "${1:-}" == --booster-life ]]; then
    exec timeout 120s wine bin/world-diff/worldboosterlifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --booster-core ]]; then
    exec timeout 120s wine bin/world-diff/worldboosterdiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --booster-breaks ]]; then
    exec python3 -B tools/world_booster_breaks.py
fi
if [[ "${1:-}" == --priority-breaks ]]; then
    exec python3 -B tools/world_priority_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --priority-ui ]]; then
    exec timeout 180s wine bin/world-diff/worldpriorityuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-21}"
fi
if [[ "${1:-}" == --priority-core ]]; then
    exec timeout 120s wine bin/world-diff/worldprioritydiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --priority-life ]]; then
    exec timeout 120s wine bin/world-diff/worldprioritylifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
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
