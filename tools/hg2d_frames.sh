#!/usr/bin/env bash
# Frame-exact comparison of the running game: retail HG2D twice, which shows the run
# is deterministic at all, then ours.  Retail always runs with SEHFIX.DLL, as does any game
# with a retail module in it, and all ours without: under wine retail's 3D view freezes at
# the first divide fault it takes (docs/wine.md).  Each run has its own copy of the game and one
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
# HG2D_FRAMES_GAUGE and HG2D_FRAMES_GAUGE_RETAIL optionally compare one rebuilt
# .GAU too. Both paths are required; the rebuilt basename is its installed name.
# Each run must actually load that gauge. Set HG2D_FRAMES_OURS to retail HG2D
# when testing the gauge alone.
# HG2D_FRAMES_GAUGES=1 compares the entire rebuilt collection instead;
# tools/stage_gauges.py restores retail inputs on the baseline sides.
# HG2D_FRAMES_MDLS=1 compares the eight rebuilt normal aircraft MDLs too,
# restoring retail on baseline sides and requiring a normal MDL load in each run.
# HG2D_FRAMES_MDL_MANIFEST selects an alternate generated set for deliberate breaks.
# HG2D_FRAMES_EXPECT_MDL additionally requires that specific model to load.
# HG2D_FRAMES_HOST selects a rebuilt executable for ours; baselines use retail.
# Its observation export provides the runtime clock, imports and HWND.
# HG2D_FRAMES_SIM1 selects a rebuilt SIM1.sim for the private ours copy;
# both baseline copies use orig/Sim/SIM1.sim. Its load is required.
# HG2D_FRAMES_CRASH_TEST=1 enables crash detection without a modal graph in
# the selected flight of each private copy (the source game stays unchanged).
# HG2D_FRAMES_CRASH_ALTITUDE optionally sets that private fixture's altitude in metres.
# HG2D_FRAMES_CRASH_COCKPIT_ONLY removes saved non-cockpit views from that fixture.
#
# HG2D_FRAMES_ALL=1 compares the whole game instead: every module and the English DLLs
# retail against every one ours, and the verdict is on all of them.
# HG2D_FRAMES_ONLY="UTIL MAIN" compares those modules alone: ours has them in an otherwise
# retail game, with retail's HG2D unless HG2D is named.  It finds which module a difference
# comes from, and what one module expects of another's registers.
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
# Bash may otherwise read later commands from a file edited during a long
# run. Execute a complete in-memory snapshot before starting any games.
if [ -z "${BASH_EXECUTION_STRING:-}" ]; then
    exec bash -c "$(cat -- "$0")" "$0" "$@"
fi
cd "$(dirname "$0")/.."
root=$PWD
flight=${1:-'Situatio\STGEORGE.STN'}
passes=${2:-900}
script=${3:-}
if [ -n "$script" ]; then
    script=$(realpath "$script")
    [ -f "$script" ] || { echo "no FRAMELOCK script: $script" >&2; exit 2; }
fi
out=$(realpath -m "${HG2D_FRAMES_OUT:-bin/hg2d-frames}")
display=${HG2D_FRAMES_DISPLAY:-:62}
game=$(realpath "${FS98_DIR:-$PWD/game}")
framelock=bin/hg2d-diff/FRAMELOCK.DLL
ours_bin=${HG2D_FRAMES_BIN:-bin}
ours=${HG2D_FRAMES_OURS:-$ours_bin/HG2D.DLL}
gauge_name=
if [ -n "${HG2D_FRAMES_GAUGE:-}${HG2D_FRAMES_GAUGE_RETAIL:-}" ]; then
    [ -f "${HG2D_FRAMES_GAUGE:-}" ] && [ -f "${HG2D_FRAMES_GAUGE_RETAIL:-}" ] || {
        echo "gauge comparison needs both rebuilt and retail .GAU paths" >&2; exit 2;
    }
    gauge_name=$(basename "$HG2D_FRAMES_GAUGE")
fi
[ -f "$ours" ] && [ -f "$framelock" ] && [ -f bin/SEHFIX.DLL ] && [ -f "$game/FLTSIM98.EXE" ] || {
    echo "needs $ours, $framelock, bin/SEHFIX.DLL and the staged game" >&2; exit 2;
}
command -v Xvfb >/dev/null || { echo "needs Xvfb" >&2; exit 2; }

rm -rf "$out"
mkdir -p "$out"
cp "$framelock" "$out/FRAMELOCK.DLL"
framelock=$out/FRAMELOCK.DLL
if [ -n "${HG2D_FRAMES_SIM1:-}" ]; then
    cp "$HG2D_FRAMES_SIM1" "$out/SIM1_candidate.sim" || exit 2
    export HG2D_FRAMES_SIM1=$out/SIM1_candidate.sim
    sha256sum "$HG2D_FRAMES_SIM1" > "$out/sim1-build.sha256"
fi
cp -a "${WINEPREFIX:-$HOME/.wine}" "$out/prefix"
export WINEPREFIX=$out/prefix
Xvfb "$display" -screen 0 "${HG2D_FRAMES_SCREEN:-1024x768}x24" +extension GLX -ac -noreset >"$out/xvfb.log" 2>&1 &
xvfb=$!
trap 'wineserver -k 2>/dev/null; kill $xvfb 2>/dev/null || true' EXIT
export DISPLAY=$display
for _ in $(seq 20); do xdpyinfo >/dev/null 2>&1 && break; sleep 0.5; done
xdpyinfo >/dev/null 2>&1 || { echo "Xvfb did not start" >&2; exit 2; }

debug=-all
[ -z "$gauge_name" ] || debug=+loaddll
[ -z "${HG2D_FRAMES_GAUGES:-}" ] || debug=+loaddll
[ -z "${HG2D_FRAMES_MDLS:-}" ] || debug=+loaddll
[ -z "${HG2D_FRAMES_SIM1:-}" ] || debug=+loaddll
[ -z "${HG2D_FRAMES_DEVICE:-}" ] || debug=+ddraw
[ -z "${gauge_name}${HG2D_FRAMES_GAUGES:-}${HG2D_FRAMES_MDLS:-}${HG2D_FRAMES_SIM1:-}" ] || [ -z "${HG2D_FRAMES_DEVICE:-}" ] || debug=+loaddll,+ddraw
export WINEDEBUG=-all
[ -z "${HG2D_FRAMES_NOSOUND:-}" ] || export WINEDLLOVERRIDES="dsound=d"

run() {     # tag hg2d
    local tag=$1 copy=$out/game-$1
    cp -a "$game" "$copy"
    local hg2d=$2
    if [ -n "${HG2D_FRAMES_ALL:-}${HG2D_FRAMES_ONLY:-}${HG2D_FRAMES_MDLS:-}" ]; then
        local m
        for m in $(make -s modules-of) $(make -s english-modules-of); do
            local to from
            to=$(make -s installed-path-of M=$m) from=$(make -s orig-module-of M=$m)
            if [ "$tag" = ours ]; then
                if [ -n "${HG2D_FRAMES_ALL:-}${HG2D_FRAMES_ONLY:-}" ] && {
                    [ -z "${HG2D_FRAMES_ONLY:-}" ] || [[ " $HG2D_FRAMES_ONLY " == *" $m "* ]]
                }; then from=$ours_bin/$m.DLL; fi
            fi
            cp "$from" "$copy/$to"
            [ "$m" = HG2D ] && hg2d=$from
        done
    fi
    cp "$hg2d" "$copy/Modules/HG2D.DLL"
    if [ -n "${HG2D_FRAMES_SIM1:-}" ]; then
        local sim1=orig/Sim/SIM1.sim
        [ "$tag" != ours ] || sim1=$HG2D_FRAMES_SIM1
        cp "$sim1" "$copy/Sim/SIM1.sim" || exit 2
        cmp -s "$sim1" "$copy/Sim/SIM1.sim" || exit 2
        # Preserve detection settings while preventing retail's modal crash
        # graph from blocking an unattended simulation trajectory comparison.
        python3 -B tools/mdl_crash_fixture.py "$copy" "$flight" --suppress-analysis-only || exit 2
    fi
    if [ -n "${HG2D_FRAMES_CRASH_TEST:-}" ]; then
        local fixture_altitude=()
        [ -z "${HG2D_FRAMES_CRASH_ALTITUDE:-}" ] || fixture_altitude=(--altitude "$HG2D_FRAMES_CRASH_ALTITUDE")
        [ -z "${HG2D_FRAMES_CRASH_COCKPIT_ONLY:-}" ] || fixture_altitude+=(--cockpit-only)
        python3 -B tools/mdl_crash_fixture.py "$copy" "$flight" "${fixture_altitude[@]}" || exit 2
    fi
    if [ -n "${HG2D_FRAMES_GAUGES:-}" ]; then
        local gauge_mode=(--retail)
        [ "$tag" != ours ] || gauge_mode=()
        python3 -B tools/stage_gauges.py "$copy" "${gauge_mode[@]}" || exit 2
    fi
    if [ -n "${HG2D_FRAMES_MDLS:-}" ]; then
        local mdl_mode=(--retail)
        [ "$tag" != ours ] || mdl_mode=()
        python3 -B tools/stage_mdls.py "$copy" "${mdl_mode[@]}" \
            --manifest "${HG2D_FRAMES_MDL_MANIFEST:-bin/mdl-diff/modules.json}" || exit 2
    fi
    if [ -n "$gauge_name" ]; then
        local gauge=$HG2D_FRAMES_GAUGE_RETAIL
        [ "$tag" != ours ] || gauge=$HG2D_FRAMES_GAUGE
        cp "$gauge" "$copy/Gauges/$gauge_name"
        cmp -s "$gauge" "$copy/Gauges/$gauge_name" || exit 2
    fi
    if [ -n "${HG2D_FRAMES_HOST:-}" ]; then
        local host=orig/FLTSIM98.EXE
        [ "$tag" != ours ] || host=$HG2D_FRAMES_HOST
        cp "$host" "$copy/FLTSIM98.EXE" || exit 2
        cmp -s "$host" "$copy/FLTSIM98.EXE" || exit 2
        sha256sum "$copy/FLTSIM98.EXE" > "$out/host-$tag.sha256"
    fi
    cp "$framelock" "$copy/Modules/FRAMELOCK.DLL"
    rm -f "$copy/Modules/SEHFIX.DLL"
    # only all ours is free of the faults: retail's G3D and WINDOW divide into the repair too
    [ "$tag" = ours ] && [ -n "${HG2D_FRAMES_ALL:-}${HG2D_FRAMES_HOST:-}" ] || cp bin/SEHFIX.DLL "$copy/Modules/SEHFIX.DLL"
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
      FRAMELOCK_SIM1=${HG2D_FRAMES_SIM1:+1} \
      FRAMELOCK_SCRIPT=${script:+$(winepath -w "$script")} \
      WINEDEBUG=$debug \
      timeout "${HG2D_FRAMES_TIMEOUT:-600}" wine FLTSIM98.EXE "/STN:$flight" >"$out/$tag/game.log" 2>&1 )
    local status=$?
    kill $watcher 2>/dev/null
    [ -s "$out/$tag/frames.log" ] || { echo "$tag: exit $status and no frames logged" >&2; exit 2; }
    echo "$tag: exit $status, $(grep -c . "$out/$tag/frames.log") lines, $(tail -1 "$out/$tag/frames.log")"
    local expected_end
    expected_end=$(printf 'end %08x' "$passes")
    [ "$status" -eq 0 ] && [ "$(tail -1 "$out/$tag/frames.log")" = "$expected_end" ] || {
        echo "$tag: game did not complete the requested passes cleanly" >&2; exit 2;
    }
    if [ -n "${HG2D_FRAMES_SIM1:-}" ]; then
        local sim1_loads
        sim1_loads=$(grep -ai 'Loaded L"' "$out/$tag/game.log" | grep -ic 'sim1\.sim"')
        echo "$tag: successful SIM1 loads $sim1_loads"
        [ "$sim1_loads" -gt 0 ] || { echo "SIM1 never loaded" >&2; exit 2; }
        local sim1_samples
        sim1_samples=$(grep -Ec '^sim1 [0-9a-f]{8} [0-9a-f]{16}' "$out/$tag/frames.log")
        echo "$tag: SIM1 numeric state samples $sim1_samples"
        [ "$sim1_samples" -gt 0 ] || { echo "no SIM1 state observed" >&2; exit 2; }
    fi
    if [ -n "${HG2D_FRAMES_MDLS:-}" ]; then
        local mdl_loads
        mdl_loads=$(grep -ai 'Loaded L"' "$out/$tag/game.log" | grep -ic '_N\.mdl"')
        echo "$tag: successful normal MDL loads $mdl_loads"
        [ "$mdl_loads" -gt 0 ] || { echo "no normal MDL loaded" >&2; exit 2; }
        if [ -n "${HG2D_FRAMES_EXPECT_MDL:-}" ]; then
            mdl_loads=$(grep -ai 'Loaded L"' "$out/$tag/game.log" | grep -icF "$HG2D_FRAMES_EXPECT_MDL")
            echo "$tag: $HG2D_FRAMES_EXPECT_MDL successful loads $mdl_loads"
            [ "$mdl_loads" -gt 0 ] || { echo "expected MDL never loaded" >&2; exit 2; }
        fi
    fi
    if [ -n "${HG2D_FRAMES_GAUGES:-}" ]; then
        local gauge_loads
        gauge_loads=$(grep -ai 'Loaded L"' "$out/$tag/game.log" | grep -ic '\.gau"')
        echo "$tag: collection successful gauge loads $gauge_loads"
        [ "$gauge_loads" -gt 0 ] || { echo "no collection gauge loaded" >&2; exit 2; }
    fi
    if [ -n "$gauge_name" ]; then
        local loads
        loads=$(grep -ai 'Loaded L"' "$out/$tag/game.log" | grep -icF "$gauge_name")
        echo "$tag: $gauge_name successful loads $loads"
        [ "$loads" -gt 0 ] || { echo "selected gauge never loaded" >&2; exit 2; }
    fi
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
