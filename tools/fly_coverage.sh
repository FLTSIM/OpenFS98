#!/usr/bin/env bash
# Does each step of a flying scenario reach the picture?  Runs retail with the whole
# script, then once without each step, and names the steps whose absence changes no frame:
# those are not tested by comparing frames, whatever the comparison says.  Without sound,
# so that a crash cannot make a step look effective (tools/hg2d_frames.sh).
#
#   tools/fly_coverage.sh flight scenario...
#
# FLY_COVERAGE_JOBS  runs at once (default 6); FLY_COVERAGE_PASSES (default 900)
set -uo pipefail
cd "$(dirname "$0")/.."
flight=$1; shift
jobs=${FLY_COVERAGE_JOBS:-6}
passes=${FLY_COVERAGE_PASSES:-900}
out=$(realpath -m "${FLY_COVERAGE_OUT:-bin/fly-coverage}")
rm -rf "$out"; mkdir -p "$out"
game=$(realpath "${FS98_DIR:-$PWD/game}")

base=$out/base; mkdir -p "$base"
mkdir -p "$base/Modules" "$base/English"
for m in $(make -s modules-of) $(make -s english-modules-of); do
    cp "$(make -s orig-module-of M=$m)" "$base/$(make -s installed-path-of M=$m)"
done

run_one() {     # tag script
    local tag=$1 script=$2 slot=$out/slot${SLOT:-0} display=:$((110 + ${SLOT:-0})) copy
    [ -d "$slot/prefix" ] || { mkdir -p "$slot"; cp -a "${WINEPREFIX:-$HOME/.wine}" "$slot/prefix"; }
    copy=$slot/game; rm -rf "$copy"; cp -a "$game" "$copy"
    cp "$base"/Modules/* "$copy/Modules/"; cp "$base"/English/* "$copy/English/"
    cp bin/SEHFIX.DLL bin/hg2d-diff/FRAMELOCK.DLL "$copy/Modules/"
    Xvfb "$display" -screen 0 1024x768x24 +extension GLX -ac -noreset >/dev/null 2>&1 &
    local xvfb=$!
    sleep 1
    ( export WINEPREFIX=$slot/prefix DISPLAY=$display WINEDEBUG=-all WINEDLLOVERRIDES="dsound=d"
      cd "$copy" && FRAMELOCK_LOG="$(winepath -w "$out/$tag.log")" FRAMELOCK_PASSES=$passes \
          FRAMELOCK_SCRIPT="$(winepath -w "$script")" \
          timeout 300 wine FLTSIM98.EXE "/STN:Situatio\\$flight.STN" >/dev/null 2>&1
      wineserver -k 2>/dev/null )
    kill $xvfb 2>/dev/null
}
export -f run_one
export out passes game flight base

work=$out/work; mkdir -p "$work"
for scenario in "$@"; do
    python3 -B tools/fly_scripts.py "$work"
    echo "$scenario-all $work/$scenario.txt" >>"$work/jobs"
    count=$(python3 -B tools/fly_scripts.py --steps "$scenario" | wc -l)
    for step in $(seq 0 $((count - 1))); do
        python3 -B tools/fly_scripts.py --drop "$scenario" "$step" >"$work/$scenario-$step.txt"
        echo "$scenario-$step $work/$scenario-$step.txt" >>"$work/jobs"
    done
done
xargs -P "$jobs" --process-slot-var=SLOT -L1 bash -c 'run_one "$1" "$2"' _ <"$work/jobs"
rm -rf "$out"/slot*

for scenario in "$@"; do
    python3 -B tools/fly_scripts.py --steps "$scenario" | while read -r step name _ at; do
        cmp -s "$out/$scenario-all.log" "$out/$scenario-$step.log" && echo "$scenario $name at $at: NO EFFECT" || echo "$scenario $name at $at: reaches the frames"
    done
done
