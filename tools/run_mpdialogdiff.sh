#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
if [[ "${1:-}" == --smoke ]]; then
    export WINEPREFIX="${MPD_SMOKE_WINEPREFIX:-$PWD/bin/mpdialog-diff/wineprefix-smoke}"
else
    export WINEPREFIX="${MPD_WINEPREFIX:-$PWD/bin/mpdialog-diff/wineprefix}"
fi
export WINEARCH=win32
export WINEDEBUG=-all
mkdir -p -- bin/mpdialog-diff
mkdir -p -- "$WINEPREFIX"
exec 8>"$PWD/bin/mpdialog-diff/wine-run.lock"
flock 8
exec 9>"${WINEPREFIX}.lock"
flock 9
if [[ "${1:-}" == --smoke ]]; then
    shift
    exec python3 -B tools/mpdialog_smoke.py "${@}"
fi
if [[ "${1:-}" == --ui-breaks ]]; then
    exec python3 -B tools/mpdialog_ui_breaks.py "${@:2}"
fi
if [[ "${1:-}" == --ui ]]; then
    shift
fi
if (($# == 0)); then
    set -- MP_DIALOG_ours.dll 12
elif (($# == 1)); then
    set -- "$1" 12
fi
exec timeout 300s wine bin/mpdialog-diff/mpdialoguidiff.exe "$@"
