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
export MPD_PROVENANCE_DIR
MPD_PROVENANCE_DIR=$(python3 -B tools/mpdialog_provenance.py begin "$@")
echo "MP_DIALOG provenance: $MPD_PROVENANCE_DIR"
finish_provenance() {
    local result=$?
    python3 -B tools/mpdialog_provenance.py finish "$result" || true
}
trap finish_provenance EXIT
if [[ "${1:-}" == --smoke ]]; then
    shift
    python3 -B tools/mpdialog_smoke.py "${@}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --ui-breaks ]]; then
    python3 -B tools/mpdialog_ui_breaks.py "${@:2}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --review-breaks ]]; then
    python3 -B tools/mpdialog_review_breaks.py "${@:2}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --common-breaks ]]; then
    python3 -B tools/mpdialog_common_breaks.py "${@:2}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --retained-breaks ]]; then
    python3 -B tools/mpdialog_retained_breaks.py "${@:2}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --create-failure-breaks ]]; then
    python3 -B tools/mpdialog_create_failure_breaks.py "${@:2}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --exception-breaks ]]; then
    python3 -B tools/mpdialog_exception_breaks.py "${@:2}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --construction-breaks ]]; then
    python3 -B tools/mpdialog_construction_breaks.py "${@:2}" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
    exit $?
fi
if [[ "${1:-}" == --ui ]]; then
    shift
fi
if (($# == 0)); then
    set -- MP_DIALOG_ours.dll 12
elif (($# == 1)); then
    set -- "$1" 12
fi
python3 -B tools/mpdialog_provenance.py record-ui "$@"
timeout 300s wine bin/mpdialog-diff/mpdialoguidiff.exe "$@" 2>&1 | tee "$MPD_PROVENANCE_DIR/output.log"
