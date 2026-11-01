#!/usr/bin/env bash
# Frame-exact comparison of the running game: retail HG2D twice, which shows the run
# is deterministic at all, then ours.  Each run has its own copy of the game and one
# wine prefix copy is shared between them, on a nested X server, so neither game/
# nor another wine process is disturbed.  The clock and the log are FRAMELOCK's
# (tools/drive/framelock.c).
#
#   tools/hg2d_frames.sh [flight] [passes] [script]
#
# flight   a situation under the game's directory (default Situatio\STGEORGE.STN)
# passes   main-loop passes to run, a frame each once flying (default 900)
# script   FRAMELOCK_SCRIPT file: messages posted to the main window (optional)
#
# HG2D_FRAMES_OUT sets the output directory (default bin/hg2d-frames),
# HG2D_FRAMES_DISPLAY the nested server (default :62), and HG2D_FRAMES_OURS the DLL
# to test (default bin/HG2D.DLL), which is how a deliberate break is checked.
set -uo pipefail
cd "$(dirname "$0")/.."
root=$PWD
flight=${1:-'Situatio\STGEORGE.STN'}
passes=${2:-900}
script=${3:-}
out=$(realpath -m "${HG2D_FRAMES_OUT:-bin/hg2d-frames}")
display=${HG2D_FRAMES_DISPLAY:-:62}
game=$(realpath "${FS98_DIR:-$PWD/game}")
framelock=bin/hg2d-diff/FRAMELOCK.DLL
ours=${HG2D_FRAMES_OURS:-bin/HG2D.DLL}
[ -f "$ours" ] && [ -f "$framelock" ] && [ -f "$game/FLTSIM98.EXE" ] || {
    echo "needs $ours, $framelock and the staged game" >&2; exit 2;
}
command -v Xephyr >/dev/null || { echo "needs Xephyr" >&2; exit 2; }

rm -rf "$out"
mkdir -p "$out"
cp -a "${WINEPREFIX:-$HOME/.wine}" "$out/prefix"
export WINEPREFIX=$out/prefix WINEDEBUG=-all
Xephyr "$display" -screen 1024x768x24 -ac -noreset >"$out/xephyr.log" 2>&1 &
xephyr=$!
trap 'wineserver -k 2>/dev/null; kill $xephyr 2>/dev/null || true' EXIT
export DISPLAY=$display
for _ in $(seq 20); do xdpyinfo >/dev/null 2>&1 && break; sleep 0.5; done
xdpyinfo >/dev/null 2>&1 || { echo "the nested X server did not start" >&2; exit 2; }

run() {     # tag hg2d
    local tag=$1 copy=$out/game-$1
    cp -a "$game" "$copy"
    cp "$2" "$copy/Modules/HG2D.DLL"
    cp "$framelock" "$copy/Modules/FRAMELOCK.DLL"
    mkdir -p "$out/$tag"
    ( cd "$copy" &&
      FRAMELOCK_LOG="$(winepath -w "$out/$tag/frames.log")" FRAMELOCK_PASSES=$passes \
      FRAMELOCK_DUMP=${HG2D_FRAMES_DUMP:-0} FRAMELOCK_DUMP_AT=${HG2D_FRAMES_DUMP_AT:-0} \
      FRAMELOCK_SCRIPT=${script:+$(winepath -w "$script")} \
      timeout "${HG2D_FRAMES_TIMEOUT:-600}" wine FLTSIM98.EXE "/STN:$flight" >"$out/$tag/game.log" 2>&1 )
    local status=$?
    [ -s "$out/$tag/frames.log" ] || { echo "$tag: exit $status and no frames logged" >&2; exit 2; }
    echo "$tag: exit $status, $(grep -c . "$out/$tag/frames.log") lines, $(tail -1 "$out/$tag/frames.log")"
    wineserver -k 2>/dev/null
    rm -rf "$copy"
}

first_difference() {    # a b
    cmp "$1" "$2" >/dev/null && { echo "identical"; return 0; }
    paste -d'|' "$1" "$2" | awk -F'|' '$1 != $2 { print "line " NR ": " $1 " / " $2; exit }'
    return 1
}

run retail-a orig/Modules/HG2D.DLL
run retail-b orig/Modules/HG2D.DLL
run ours "$ours"
rm -rf "$out/prefix"

fail=0
echo "retail against retail: $(first_difference "$out/retail-a/frames.log" "$out/retail-b/frames.log" || true)"
cmp -s "$out/retail-a/frames.log" "$out/retail-b/frames.log" || { echo "the run is not deterministic: no verdict on ours"; fail=2; }
echo "retail against ours:   $(first_difference "$out/retail-a/frames.log" "$out/ours/frames.log" || true)"
cmp -s "$out/retail-a/frames.log" "$out/ours/frames.log" || fail=${fail/0/1}
[ "$fail" = 0 ] && echo PASS || echo FAIL
exit "$fail"
