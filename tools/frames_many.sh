#!/usr/bin/env bash
# The whole game, all retail against all ours, over many flights at once: one
# tools/hg2d_frames.sh per flight, each with its own Xvfb, prefix copy and game copies.
#
#   tools/frames_many.sh [script] [flight...]
#
# script   a FRAMELOCK_SCRIPT file, or - for no input (tools/fly_scripts.py writes them)
# flight   names under Situatio\ without .STN (default every flight the game has)
#
# FRAMES_MANY_JOBS   flights at once (default 4; each is three games in turn)
# FRAMES_MANY_PASSES passes a flight (default 900)
# FRAMES_MANY_OUT    output directory (default bin/frames-many); each flight's verdict is
#                    in <flight>/verdict, and only a failing flight keeps its logs
#
# Prints one line a flight, then the counts.  A flight that crashes does not repeat with
# sound (see tools/hg2d_frames.sh), and two of the three runs can agree by chance, so any
# flight that fails is run again without sound and says so: "match, no sound".  A real
# difference still differs.  If retail still does not repeat it is NONDETERMINISTIC, which
# is not a pass.  One that logs too little to mean anything is flagged FEW FRAMES.
set -uo pipefail
cd "$(dirname "$0")/.."
script=${1:--}
[ $# -gt 0 ] && shift
jobs=${FRAMES_MANY_JOBS:-4}
passes=${FRAMES_MANY_PASSES:-900}
out=$(realpath -m "${FRAMES_MANY_OUT:-bin/frames-many}")
game=$(realpath "${FS98_DIR:-$PWD/game}")
[ "$script" = - ] || script=$(realpath "$script")
mkdir -p "$out"

if [ $# -gt 0 ]; then
    flights=("$@")
else
    mapfile -t flights < <(ls "$game/Situatio" | grep -i '\.stn$' | sed 's/\.[Ss][Tt][Nn]$//' | sort -f)
fi

one() {     # flight
    local flight=$1 dir=$out/$1 display=:$((80 + ${SLOT:-0})) distinct verdict
    local args=("Situatio\\$flight.STN" "$passes")
    [ "$script" = - ] || args+=("$script")
    HG2D_FRAMES_ALL=1 HG2D_FRAMES_OUT="$dir" HG2D_FRAMES_DISPLAY=$display \
        HG2D_FRAMES_TIMEOUT=${FRAMES_MANY_TIMEOUT:-300} \
        tools/hg2d_frames.sh "${args[@]}" >"$dir.txt" 2>&1
    local status=$? quiet=
    if [ "$status" != 0 ] && [ -s "$dir/retail-a/frames.log" ]; then
        quiet=", no sound"
        HG2D_FRAMES_NOSOUND=1 HG2D_FRAMES_ALL=1 HG2D_FRAMES_OUT="$dir" HG2D_FRAMES_DISPLAY=$display \
            HG2D_FRAMES_TIMEOUT=${FRAMES_MANY_TIMEOUT:-300} \
            tools/hg2d_frames.sh "${args[@]}" >"$dir.txt" 2>&1
        status=$?
    fi
    distinct=$(awk '$3 != "" && $1 != "end" && $1 != "fault" { print $9 }' "$dir/retail-a/frames.log" 2>/dev/null | sort -u | wc -l)
    case $status in
        0) verdict=match ;;
        1) verdict=DIFFERS ;;
        *) verdict=NONDETERMINISTIC ;;
    esac
    [ "$status" = 2 ] && [ ! -s "$dir/retail-a/frames.log" ] && verdict=NO-RUN
    [ "$verdict" = match ] && [ "$distinct" -lt 20 ] && verdict="match, FEW FRAMES"
    verdict=$verdict${verdict:+$quiet}
    local faults=$(grep -c '^fault' "$dir/ours/frames.log" 2>/dev/null)
    local rfaults=$(grep -c '^fault' "$dir/retail-a/frames.log" 2>/dev/null)
    echo "$flight: $verdict ($distinct distinct frames, faults retail ${rfaults:-0} ours ${faults:-0}) $(grep -a 'retail against ours' "$dir.txt" | cut -c1-110)"
    mv "$dir.txt" "$dir/result.txt" 2>/dev/null
    case $verdict in match*) rm -rf "$dir/retail-a" "$dir/retail-b" "$dir/ours" ;; esac
    return 0
}
export -f one
export out passes script game
export FS98_DIR="$game"

printf '%s\n' "${flights[@]}" | xargs -P "$jobs" --process-slot-var=SLOT -I{} bash -c 'one "$1"' _ {} | tee "$out/summary.txt"
echo "--- $(grep -c ': match' "$out/summary.txt") match, $(grep -c ': DIFFERS' "$out/summary.txt") differ, $(grep -c ': NONDETERMINISTIC' "$out/summary.txt") nondeterministic, $(grep -c ': NO-RUN' "$out/summary.txt") did not run, of ${#flights[@]}"
! grep -q ': DIFFERS\|: NO-RUN' "$out/summary.txt"
