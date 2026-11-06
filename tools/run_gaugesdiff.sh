#!/usr/bin/env bash
# Gauge work has a private prefix and lock; never use another agent's Wine server.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/gauges-diff/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/gauges-diff/wine.lock
flock 9
if [[ "${1:-}" == --all ]]; then
    shift
    exec timeout 1800s wine bin/gauges-all/allgaugesdiff.exe "$@"
fi
if [[ "${1:-}" == --all-batch ]]; then
    shift
    exec python3 -B tools/check_gauges.py "$@"
fi
if [[ "${1:-}" == --all-panels ]]; then
    export PANELS_STRICT=1
    cd bin/gauges-all
    exec timeout 7200s wine allpaneldiff.exe "${2:-1000}" "${3:-0}"
fi
if [[ "${1:-}" == --all-mutations ]]; then
    exec python3 -B tools/mutate_gauges.py
fi
if [[ "${1:-}" == --suction ]]; then
    exec timeout 180s wine bin/gauges-diff/suctiondiff.exe "${2:-20000}" "${3:-bin/gauges-diff/CESSNA_182.SUCTION.GAU}"
fi
if [[ "${1:-}" == --breaks ]]; then
    exec python3 -B tools/gauges_breaks.py
fi
if [[ "${1:-}" == --suction-breaks ]]; then
    exec python3 -B tools/gauges_breaks.py --suction
fi
if [[ "${1:-}" == --panels ]]; then
    export PANELS_STRICT=1
    cd bin/gauges-diff
    exec timeout 1800s wine gaugespanelsdiff.exe "${2:-200}" "${3:-0}"
fi
exec timeout 180s wine bin/gauges-diff/gaugesdiff.exe "${1:-20000}" "${2:-bin/gauges-diff/737-400.WARNING_ENUNCIATORS.GAU}"
