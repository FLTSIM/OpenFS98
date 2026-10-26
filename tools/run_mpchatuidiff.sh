#!/bin/bash
# A private prefix, lock and output tree; no game/ or shared test binaries.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
export WINEPREFIX="$PWD/bin/mpchat-ui/wineprefix"
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- "$WINEPREFIX"
exec 9>bin/mpchat-ui/wine.lock
flock 9
if [[ "${1:-}" == --breaks ]]; then
    exec python3 -B tools/mpchat_ui_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --model ]]; then
    cp orig/Modules/mpchat.DLL bin/mpchat-ui/MPCHAT_retail.dll
    cd bin/mpchat-ui
    exec timeout 180s wine mpchatdiff.exe 20000
fi
exec timeout 720s wine bin/mpchat-ui/mpchatuidiff.exe "${2:-bin/mpchat-ui/MPCHAT_ours.dll}" "${3:-24}"
