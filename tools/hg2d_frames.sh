#!/usr/bin/env bash
# Frame-exact comparison of the running game: retail HG2D twice, which shows the run
# is deterministic at all, then ours.  Retail always runs with SEHFIX.DLL and ours
# without: under wine retail's 3D view freezes at the first divide fault it takes
# (docs/wine.md), so without it the reference is wrong.  Each run has its own copy of the game and one
# wine prefix copy is shared between them, on a private Xvfb, so neither game/ nor
# another wine process nor the desktop is disturbed.  The clock and the log are FRAMELOCK's
# (tools/drive/framelock.c).
#
#   tools/hg2d_frames.sh [flight] [passes] [script]
#
# flight   a situation under the game's directory (default Situatio\STGEORGE.STN)
# passes   main-loop passes to run, a frame each once flying (default 900)
# script   FRAMELOCK_SCRIPT file: messages posted to the main window (optional)
#
# HG2D_FRAMES_OUT sets the output directory (default bin/hg2d-frames),
# HG2D_FRAMES_DISPLAY the Xvfb (default :62), and HG2D_FRAMES_OURS the DLL to test
# (default bin/HG2D.DLL), which is how a deliberate break is checked.
#
# HG2D_FRAMES_NOSOUND=1 runs without DirectSound.  A crash pauses the sim until its sound
# ends (docs/modules/reality.md), which is real time, so a flight that crashes does not
# repeat with sound: the pass it resumes on varies.  Without it, it does.
#
# HG2D_FRAMES_BIN is the directory of our DLLs (default bin), for testing a broken build.
#
# HG2D_FRAMES_ALL=1 compares the whole game instead: every module and World.DLL
# retail against every one ours, and the verdict is on all of them.
#
# HG2D_FRAMES_DEVICE and HG2D_FRAMES_MODE set [HARDWARE] Device= and Mode= in each
# copy's FLTSIM98.CFG, for the Direct3D path.  Its frames do not pass through the
# logged blits, so HG2D_FRAMES_HOLD=n also takes a screenshot every n passes, and
# compares those: identical pixels, or the count of those that differ.  HG2D only
# makes a device for a DirectDraw surface, which means full screen: post command
# 0x1501 from a script ("32 111 1501 0"), and set HG2D_FRAMES_SCREEN=640x480, since
# wine's mode change fails on an Xvfb without that mode.  The run fails if a side drew
# no Direct3D scenes, because otherwise it silently compares the software renderer.
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
ours_bin=${HG2D_FRAMES_BIN:-bin}
ours=${HG2D_FRAMES_OURS:-$ours_bin/HG2D.DLL}
[ -f "$ours" ] && [ -f "$framelock" ] && [ -f bin/SEHFIX.DLL ] && [ -f "$game/FLTSIM98.EXE" ] || {
    echo "needs $ours, $framelock, bin/SEHFIX.DLL and the staged game" >&2; exit 2;
}
command -v Xvfb >/dev/null || { echo "needs Xvfb" >&2; exit 2; }

rm -rf "$out"
mkdir -p "$out"
cp -a "${WINEPREFIX:-$HOME/.wine}" "$out/prefix"
export WINEPREFIX=$out/prefix
Xvfb "$display" -screen 0 "${HG2D_FRAMES_SCREEN:-1024x768}x24" +extension GLX -ac -noreset >"$out/xvfb.log" 2>&1 &
xvfb=$!
trap 'wineserver -k 2>/dev/null; kill $xvfb 2>/dev/null || true' EXIT
export DISPLAY=$display
for _ in $(seq 20); do xdpyinfo >/dev/null 2>&1 && break; sleep 0.5; done
xdpyinfo >/dev/null 2>&1 || { echo "Xvfb did not start" >&2; exit 2; }

debug=-all
[ -z "${HG2D_FRAMES_DEVICE:-}" ] || debug=+ddraw
export WINEDEBUG=-all
[ -z "${HG2D_FRAMES_NOSOUND:-}" ] || export WINEDLLOVERRIDES="dsound=d"

run() {     # tag hg2d
    local tag=$1 copy=$out/game-$1
    cp -a "$game" "$copy"
    if [ -n "${HG2D_FRAMES_ALL:-}" ]; then
        local m
        for m in $(make -s modules-of); do
            if [ "$tag" = ours ]; then cp "$ours_bin/$m.DLL" "$copy/Modules/$(make -s installed-name-of M=$m)"
            else cp "$(make -s orig-module-of M=$m)" "$copy/Modules/$(make -s installed-name-of M=$m)"; fi
        done
        if [ "$tag" = ours ]; then cp "$ours_bin/WORLD.DLL" "$copy/English/World.DLL"
        else cp orig/English/World.DLL "$copy/English/World.DLL"; fi
    fi
    cp "$2" "$copy/Modules/HG2D.DLL"
    cp "$framelock" "$copy/Modules/FRAMELOCK.DLL"
    rm -f "$copy/Modules/SEHFIX.DLL"
    [ "$tag" = ours ] || cp bin/SEHFIX.DLL "$copy/Modules/SEHFIX.DLL"
    mkdir -p "$out/$tag"
    [ -z "${HG2D_FRAMES_DEVICE+x}" ] || sed -i "s/^Device=.*/Device=$HG2D_FRAMES_DEVICE\r/" "$copy/FLTSIM98.CFG"
    [ -z "${HG2D_FRAMES_MODE+x}" ] || sed -i "s/^Mode=.*/Mode=$HG2D_FRAMES_MODE\r/" "$copy/FLTSIM98.CFG"
    ( while :; do
          if [ -e "$out/$tag/hold" ]; then
              sleep 0.5
              shots=$((${shots:-0} + 1))
              import -display "$display" -window root "$out/$tag/shot$(printf %04d $shots).png" 2>/dev/null
              rm -f "$out/$tag/hold"
          fi
          sleep 0.05
      done ) &
    local watcher=$!
    ( cd "$copy" &&
      FRAMELOCK_LOG="$(winepath -w "$out/$tag/frames.log")" FRAMELOCK_PASSES=$passes \
      FRAMELOCK_DUMP=${HG2D_FRAMES_DUMP:-0} FRAMELOCK_DUMP_AT=${HG2D_FRAMES_DUMP_AT:-0} \
      FRAMELOCK_HOLD=${HG2D_FRAMES_HOLD:-0} \
      FRAMELOCK_SCRIPT=${script:+$(winepath -w "$script")} \
      WINEDEBUG=$debug \
      timeout "${HG2D_FRAMES_TIMEOUT:-600}" wine FLTSIM98.EXE "/STN:$flight" >"$out/$tag/game.log" 2>&1 )
    local status=$?
    kill $watcher 2>/dev/null
    [ -s "$out/$tag/frames.log" ] || { echo "$tag: exit $status and no frames logged" >&2; exit 2; }
    echo "$tag: exit $status, $(grep -c . "$out/$tag/frames.log") lines, $(tail -1 "$out/$tag/frames.log")"
    if [ -n "${HG2D_FRAMES_DEVICE:-}" ]; then
        local scenes draws
        scenes=$(grep -ac d3d_device7_BeginScene "$out/$tag/game.log")
        draws=$(grep -ac d3d_device7_DrawPrimitive "$out/$tag/game.log")
        echo "$tag: $scenes Direct3D scenes, $draws draws"
        echo "$scenes $draws" >"$out/$tag/scenes"
        rm -f "$out/$tag/game.log"
        [ "$scenes" -gt 1 ] || no_scenes=1
    fi
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
for shot in "$out"/retail-a/shot*.png; do
    [ -e "$shot" ] || continue
    name=$(basename "$shot")
    a=$(compare -metric AE "$shot" "$out/retail-b/$name" null: 2>&1 | head -1)
    b=$(compare -metric AE "$shot" "$out/ours/$name" null: 2>&1 | head -1)
    echo "$name: retail against retail $a, retail against ours $b"
    [ "$b" = 0 ] || fail=${fail/0/1}
done
if [ -n "${HG2D_FRAMES_DEVICE:-}" ]; then
    cmp -s "$out/retail-a/scenes" "$out/ours/scenes" || { echo "the scene and draw counts differ"; fail=${fail/0/1}; }
fi
[ -z "${no_scenes:-}" ] || { echo "a side drew no Direct3D scenes: no verdict"; fail=2; }
[ "$fail" = 0 ] && echo PASS || echo FAIL
exit "$fail"
