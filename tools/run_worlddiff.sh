#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/world-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/world-diff/wine.lock
flock 9
if [[ "${1:-}" == --advanced-native-breaks ]]; then
    exec python3 -B tools/world_advanced_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --game-ui ]]; then
    exec python3 -B tools/world_game.py "${@:2}"
fi
if [[ "${1:-}" == --advanced-ui ]]; then
    exec timeout 600s wine bin/world-diff/worldadvanceduidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-8}"
fi
if [[ "${1:-}" == --advanced-cloud ]]; then
    exec timeout 180s wine bin/world-diff/worldadvancedclouddiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --advanced-wind ]]; then
    exec timeout 180s wine bin/world-diff/worldadvancedwinddiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --advanced-core-breaks ]]; then
    exec python3 -B tools/world_advanced_core_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --advanced-temperature ]]; then
    exec timeout 180s wine bin/world-diff/worldadvancedtemperaturediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --advanced-parent ]]; then
    exec timeout 180s wine bin/world-diff/worldadvancedparentdiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --advanced-life-breaks ]]; then
    exec python3 -B tools/world_advanced_life_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --advanced-life ]]; then
    exec timeout 180s wine bin/world-diff/worldadvancedlifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --wareas-breaks ]]; then
    exec python3 -B tools/world_wareas_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --wareas-native-breaks ]]; then
    exec python3 -B tools/world_wareas_breaks.py --native "${@:2}"
fi
if [[ "${1:-}" == --wareas-init ]]; then
    exec timeout 180s wine bin/world-diff/worldwareasinitdiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --wareas-core ]]; then
    exec timeout 180s wine bin/world-diff/worldwareasdiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --wareas-life ]]; then
    exec timeout 180s wine bin/world-diff/worldwareaslifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --wareas-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldwareasuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-12}"
fi
if [[ "${1:-}" == --time-breaks ]]; then
    exec python3 -B tools/world_time_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --time-native-breaks ]]; then
    exec python3 -B tools/world_time_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --time-life || "${1:-}" == --time-core ]]; then
    kind="${1#--time-}"
    if [[ "$kind" == life ]]; then executable=worldtimelifediff.exe; else executable=worldtimediff.exe; fi
    exec timeout 180s wine "bin/world-diff/$executable" "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --time-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldtimeuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-16}"
fi
if [[ "${1:-}" == --weather-breaks ]]; then
    exec python3 -B tools/world_weather_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --weather-native-breaks ]]; then
    exec python3 -B tools/world_weather_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --weather-init ]]; then
    exec timeout 180s wine bin/world-diff/worldweatherinitdiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --weather-life || "${1:-}" == --weather-core ]]; then
    kind="${1#--weather-}"
    if [[ "$kind" == life ]]; then executable=worldweatherlifediff.exe; else executable=worldweatherdiff.exe; fi
    exec timeout 180s wine "bin/world-diff/$executable" "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --weather-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldweatheruidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-20}"
fi
if [[ "${1:-}" == --copy-breaks ]]; then
    exec python3 -B tools/world_copy_breaks.py
fi
if [[ "${1:-}" == --copy-life-breaks ]]; then
    exec python3 -B tools/world_copy_life_breaks.py
fi
if [[ "${1:-}" == --copy-native-breaks ]]; then
    exec python3 -B tools/world_copy_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --copy-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldcopyuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-16}"
fi
if [[ "${1:-}" == --copy-core || "${1:-}" == --copy-life ]]; then
    kind="${1#--copy-}"
    if [[ "$kind" == core ]]; then executable=worldcopydiff.exe; else executable=worldcopylifediff.exe; fi
    exec timeout 120s wine "bin/world-diff/$executable" "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --dynamic-breaks ]]; then
    exec python3 -B tools/world_dynamic_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --dynamic-life-breaks ]]; then
    exec python3 -B tools/world_dynamic_life_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --dynamic-init-breaks ]]; then
    exec python3 -B tools/world_dynamic_init_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --dynamic-native-breaks ]]; then
    exec python3 -B tools/world_dynamic_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --dynamic-ui ]]; then
    exec timeout 300s wine bin/world-diff/worlddynamicuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-16}"
fi
if [[ "${1:-}" == --dynamic-core ]]; then
    exec timeout 120s wine bin/world-diff/worlddynamicdiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --dynamic-life ]]; then
    exec timeout 120s wine bin/world-diff/worlddynamiclifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --dynamic-init ]]; then
    exec timeout 120s wine bin/world-diff/worlddynamicinitdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --complexity-native-breaks ]]; then
    exec python3 -B tools/world_complexity_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --complexity-init-breaks ]]; then
    exec python3 -B tools/world_complexity_init_breaks.py
fi
if [[ "${1:-}" == --complexity-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldcomplexityuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-16}"
fi
if [[ "${1:-}" == --complexity-init ]]; then
    exec timeout 120s wine bin/world-diff/worldcomplexityinitdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --complexity-life ]]; then
    exec timeout 120s wine bin/world-diff/worldcomplexitylifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --complexity-life-breaks ]]; then
    exec python3 -B tools/world_complexity_life_breaks.py
fi
if [[ "${1:-}" == --complexity-core ]]; then
    exec timeout 120s wine bin/world-diff/worldcomplexitydiff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --complexity-breaks ]]; then
    exec python3 -B tools/world_complexity_breaks.py
fi
if [[ "${1:-}" == --airport-native-breaks ]]; then
    exec python3 -B tools/world_airport_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --airport-breaks ]]; then
    exec python3 -B tools/world_airport_breaks.py
fi
if [[ "${1:-}" == --airport-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldairportuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-12}"
fi
if [[ "${1:-}" == --airport-core ]]; then
    exec timeout 180s wine bin/world-diff/worldairportdiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --airport-life ]]; then
    exec timeout 120s wine bin/world-diff/worldairportlifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --airport-life-breaks ]]; then
    exec python3 -B tools/world_airport_life_breaks.py
fi
if [[ "${1:-}" == --facilities-breaks ]]; then
    exec python3 -B tools/world_facilities_breaks.py
fi
if [[ "${1:-}" == --facilities-native-breaks ]]; then
    exec python3 -B tools/world_facilities_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --facilities-core ]]; then
    exec timeout 180s wine bin/world-diff/worldfacilitiesdiff.exe "${2:-200}" "${3:-normal}"
fi
if [[ "${1:-}" == --facilities-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldfacilitiesuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-16}"
fi
if [[ "${1:-}" == --facilities-life ]]; then
    exec timeout 120s wine bin/world-diff/worldfacilitieslifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
fi
if [[ "${1:-}" == --facilities-life-breaks ]]; then
    exec python3 -B tools/world_facilities_life_breaks.py
fi
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
if [[ "${1:-}" == --position-core ]]; then
    exec timeout 120s wine bin/world-diff/worldpositiondiff.exe "${2:-2000}" "${3:-normal}"
fi
if [[ "${1:-}" == --position-ui ]]; then
    exec timeout 300s wine bin/world-diff/worldpositionuidiff.exe "${2:-bin/world-diff/WORLD_area.dll}" "${3:-16}"
fi
if [[ "${1:-}" == --position-breaks ]]; then
    exec python3 -B tools/world_position_breaks.py
fi
if [[ "${1:-}" == --position-life-breaks ]]; then
    exec python3 -B tools/world_position_life_breaks.py
fi
if [[ "${1:-}" == --position-native-breaks ]]; then
    exec python3 -B tools/world_position_native_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --position-life ]]; then
    exec timeout 120s wine bin/world-diff/worldpositionlifediff.exe "${2:-2000}" "${3:-0x4a971b63}" "${4:-normal}"
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
