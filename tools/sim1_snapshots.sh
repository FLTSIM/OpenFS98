#!/usr/bin/env bash
# SIM1's in-flight states for tools/difftest/sim1rdiff.c: eight retail flights, one per stock
# aircraft type, 1,200 passes of the controls script, SIM1's .data and GLOBAL every 40 passes
# (FRAMELOCK_SIM1_SNAPSHOT), joined into bin/sim1-diff/snapshots.bin (8 + 0x43f8 + 0x12a8 bytes
# a record). Each flight runs through tools/hg2d_frames.sh on its own Xvfb; only the retail-a
# side's snapshots are kept, and they must equal retail-b's (the run repeats).
#
#   tools/sim1_snapshots.sh        (needs bin/sim1-diff/SIM1_COMPLETE.DLL and the
#                                   bin/sim1-diff/scripts made by make -f tools/sim1.mk sim1flights-run)
set -uo pipefail
cd "$(dirname "$0")/.."
out=bin/sim1-diff/snapshots
mkdir -p "$out"
flights=(STGEORGE BELLSFR LEARLON EXTRARIO CESSROME SAIL1 KENNEDY PARIS)
i=0
for flight in "${flights[@]}"; do
    ( HG2D_FRAMES_ONLY=SIM1 HG2D_FRAMES_SIM1="$PWD/bin/sim1-diff/SIM1_COMPLETE.DLL" HG2D_FRAMES_NOSOUND=1 \
      HG2D_FRAMES_SNAPSHOT=40 HG2D_FRAMES_OUT=$out/$flight HG2D_FRAMES_DISPLAY=:$((91 + i)) FS98_DIR="$PWD/game" \
      tools/hg2d_frames.sh "Situatio\\$flight.STN" 1200 bin/sim1-diff/scripts/controls.txt > "$out/$flight.log" 2>&1 ) &
    i=$((i + 1))
    [ $((i % 4)) -eq 0 ] && wait
done
wait
: > bin/sim1-diff/snapshots.bin
for flight in "${flights[@]}"; do
    a=$out/$flight/retail-a/sim1.snap b=$out/$flight/retail-b/sim1.snap
    [ -s "$a" ] && cmp -s "$a" "$b" || { echo "$flight: no snapshots, or the two retail runs differ" >&2; exit 2; }
    cat "$a" >> bin/sim1-diff/snapshots.bin
done
echo "bin/sim1-diff/snapshots.bin: $(( $(stat -c %s bin/sim1-diff/snapshots.bin) / (8 + 0x43f8 + 0x12a8) )) snapshots"
