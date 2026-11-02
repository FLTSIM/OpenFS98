#!/usr/bin/env bash
# Each of our modules alone in a retail game, comparing what its Linkage functions leave in
# the registers with what retail's left: tools/reg_contracts.py says how to read the result.
# Without sound, since a crash's timing would otherwise change which calls happen.
#
#   tools/reg_contracts.sh [module...]     default every module and WORLD
#
# REG_CONTRACTS_RUNS   flight:scenario pairs, - for no input
# REG_CONTRACTS_JOBS   runs at once (default 8; each is three games)
# REG_CONTRACTS_PASSES (default 300)
# REG_CONTRACTS_OUT    output directory (default bin/reg-contracts)
set -uo pipefail
cd "$(dirname "$0")/.."
runs=${REG_CONTRACTS_RUNS:-PARIS:controls LEARLON:avionics NEWYORK:views KAUAI:-}
jobs=${REG_CONTRACTS_JOBS:-8}
passes=${REG_CONTRACTS_PASSES:-300}
out=$(realpath -m "${REG_CONTRACTS_OUT:-bin/reg-contracts}")
rm -rf "$out"; mkdir -p "$out/scripts"
python3 -B tools/fly_scripts.py "$out/scripts"
modules=("$@")
[ ${#modules[@]} -gt 0 ] || modules=($(make -s modules-of) WORLD)

one() {     # module flight:scenario
    local m=$1 flight=${2%%:*} scenario=${2##*:} dir args verdict
    dir=$out/$m/$flight
    args=("Situatio\\$flight.STN" "$passes")
    [ "$scenario" = - ] || args+=("$out/scripts/$scenario.txt")
    mkdir -p "$out/$m"
    FRAMELOCK_REGS=1 HG2D_FRAMES_ONLY=$m HG2D_FRAMES_NOSOUND=1 HG2D_FRAMES_OUT=$dir \
        HG2D_FRAMES_DISPLAY=:$((150 + ${SLOT:-0})) HG2D_FRAMES_TIMEOUT=250 \
        tools/hg2d_frames.sh "${args[@]}" >"$dir.txt" 2>&1
    case $? in 0) verdict=same ;; 1) verdict=DIVERGED ;; *) verdict=NO-VERDICT ;; esac
    echo "$m $flight $verdict $(grep -a 'retail against ours' "$dir.txt" | cut -c24-60)" >>"$out/$m/runs"
    rm -rf "$dir/prefix" "$dir"/*/game.log "$dir"/*/frames.log "$dir"/*/*.png
}
export -f one
export out passes

for m in "${modules[@]}"; do for r in $runs; do echo "$m $r"; done; done |
    xargs -P "$jobs" --process-slot-var=SLOT -L1 bash -c 'one "$1" "$2"' _

status=0
for m in "${modules[@]}"; do
    case $m in WORLD) name=WORLD ;; *) name=$(make -s installed-name-of M="$m"); name=${name%.*}; name=${name^^} ;; esac
    pairs=()
    for r in $runs; do
        flight=${r%%:*}
        [ -f "$out/$m/$flight/ours/regs.txt" ] && pairs+=("$out/$m/$flight/retail-a/regs.txt" "$out/$m/$flight/ours/regs.txt")
    done
    echo "=== $m"
    grep -v ' same ' "$out/$m/runs" | cut -c1-90
    python3 -B tools/reg_contracts.py "${pairs[@]}" --module "$name" || status=1
done
exit $status
