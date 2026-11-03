#!/usr/bin/env bash
# Start flights through Flights' dialogs in the real game and save each, on retail MAIN and
# the retail English DLLs and on ours, then compare the saved flights (tools/stn_compare.py)
# and what the dialogs showed.  Where main-dialogs opens each dialog and presses OK or
# Cancel, this uses them: it saves the situation the game starts in (Save, with a title
# typed in), loads a flight from Select Flight, a challenge and an adventure from the
# pickers, and the flight it saved first, each chosen in its list, pauses each (PAUSE_ON)
# and saves it.  Then it sets things in the dialogs and saves what they did: another
# aircraft in Select Aircraft; Navigation's NAV radios, then on its other two tabs the
# transponder, the ADF and the EFIS (master switch, VOR lock, altitude); Autopilot switched
# on with heading and altitude holds and a navigation mode; Communications' COM 1; then
# Select Flight loads a saved flight marked as the default, and deletes one saved for it
# (Yes to the question).  The tabs are clicked where they are drawn (control 3345): the
# &N, &T and &E buttons are only their keyboard shortcuts.  EFIS's lock radios stay
# unavailable at St George, where no VOR or ILS is received, in retail too.  Last it quits (Yes to Exit), so the game writes its settings.  Everything
# the game wrote is kept and compared: the saved flights, the files it changed, and which
# flights are left.  Retail runs twice, to tell the records that differ with timing (the sim
# runs between a load and the pause for however long wine takes) from the ones that must
# be the same; ours runs twice.  All four at once, each on its own Xvfb and with its own
# copy of the game and the wine prefix, so nothing reaches a screen or the shared prefix.
# The modules other than MAIN and the English DLLs are those installed in game/.
# FLIGHTS_DLL=path runs ours with that Flights.DLL instead of bin/FLIGHTS.DLL, to show
# that a broken one is caught; FLIGHTS_PLAY_OUT and FLIGHTS_PLAY_DISPLAY (the first of four
# display numbers, 81) let two runs go at once.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
game=$(realpath "${FS98_DIR:-$PWD/game}")
out=${FLIGHTS_PLAY_OUT:-$root/bin/flights-play}
d=${FLIGHTS_PLAY_DISPLAY:-81}
# Save writes the flight a moment after its dialog closes (Pilots\<title>.STN), so each
# save is followed by a wait; a load takes up to half a minute before Save will open.
steps="1104/3272=DriveStart sleep:15
1102/#3267=A_Walk_in_the_Park sleep:30 pause 1104/3272=DriveSelect sleep:15
1106/#3266=2 sleep:30 pause 1104/3272=DriveChallenge sleep:15
1107/#3266=1 sleep:45 pause 1104/3272=DriveAdventure sleep:15
1102/#3267=DriveStart sleep:30 pause 1104/3272=DriveReload sleep:15
1201/#3265=0 sleep:20 pause 1104/3272=DriveAircraft sleep:15
1202/3257=113.00/3258=90/3259=110.50/3260=270/@505,155/3256=4321/3255=350/@505,190/!3219 sleep:5 1104/3272=DriveRadios sleep:15
1204/!3233/!3230/3249=270/!3228/3248=5000/%3246=1 sleep:5 1104/3272=DriveAutopilot sleep:15
1203/3251=121.50 sleep:5 1104/3272=DriveComms sleep:15
1102/#3267=DriveRadios/!3346 sleep:30 pause 1104/3272=DriveDefault sleep:15
1104/3272=DriveDelete sleep:15 1102/#3267=DriveDelete/3221/2004 sleep:5
quit"
english=$(make -s english-modules-of)
for m in MAIN $english; do
    [ -f "bin/$m.DLL" ] || { echo "needs bin/$m.DLL" >&2; exit 2; }
done
[ -f bin/fsdialogs.exe ] && [ -f "$game/FLTSIM98.EXE" ] || {
    echo "needs bin/fsdialogs.exe and the staged game" >&2; exit 2;
}
command -v Xvfb >/dev/null || { echo "needs Xvfb" >&2; exit 2; }

rm -rf "$out"
mkdir -p "$out"

run() {     # tag display
    local tag=$1 display=$2 dir=$out/$1 copy=$out/game-$1 prefix=$out/prefix-$1 m
    mkdir -p "$dir"
    cp -a "$game" "$copy"
    cp -a "${WINEPREFIX:-$HOME/.wine}" "$prefix"
    for m in MAIN $english; do
        if [[ $tag == ours-* && $m == FLIGHTS && -n ${FLIGHTS_DLL:-} ]]; then cp "$FLIGHTS_DLL" "$copy/$(make -s installed-path-of M=$m)"
        elif [[ $tag == ours-* ]]; then cp "bin/$m.DLL" "$copy/$(make -s installed-path-of M=$m)"
        else cp "$(make -s orig-module-of M=$m)" "$copy/$(make -s installed-path-of M=$m)"; fi
    done
    touch "$dir/marker"
    export WINEPREFIX=$prefix WINEDEBUG=-all DISPLAY=$display WINEDLLOVERRIDES=dsound=d
    Xvfb "$display" -screen 0 1024x768x24 -ac -noreset >"$dir/xvfb.log" 2>&1 &
    local xvfb=$!
    for _ in $(seq 20); do xdpyinfo >/dev/null 2>&1 && break; sleep 0.5; done
    ( cd "$copy" && WINEDEBUG=err+all timeout 900 wine FLTSIM98.EXE '/STN:Situatio\STGEORGE.STN' >"$dir/game.log" 2>&1 ) &
    local pid=$!
    # shellcheck disable=SC2086
    timeout 840 wine bin/fsdialogs.exe "$(winepath -w "$dir")" 25000 ok $steps || echo "$tag: the drive exited $?"
    wineserver -k 2>/dev/null || true
    wait "$pid" || true
    kill "$xvfb" 2>/dev/null || true
    wait "$xvfb" 2>/dev/null || true
    # what the game wrote: the saved flights apart, every other file it changed, and the
    # flights left after the delete
    mkdir -p "$dir/Pilots" "$dir/changed"
    find "$copy/Pilots" -name '*.STN' -newer "$dir/marker" -exec cp -t "$dir/Pilots/" {} +
    ( cd "$copy" && find . -type f -newer "$dir/marker" ! -path './Pilots/*.STN' -print0 |
          xargs -0r cp --parents -t "$dir/changed/" )
    ls "$copy/Pilots" >"$dir/pilots.txt"
    rm -rf "$copy" "$prefix"
}

run retail-1 :$d & sleep 3
run retail-2 :$((d + 1)) & sleep 3
run ours-1 :$((d + 2)) & sleep 3
run ours-2 :$((d + 3)) &
wait

# The wall clock, should a dialog show it
normal() { tr -d '\r' <"$1" | sed -E 's/[0-9]{2}:[0-9]{2}(:[0-9]{2})?/TIME/g'; }

fail=0
for o in ours-1 ours-2; do
    if ! diff <(normal "$out/retail-1/drive.log") <(normal "$out/$o/drive.log") >"$out/$o.diff"; then
        echo "$o: the dialogs differ, see $out/$o.diff"; fail=1
    fi
done
for r in retail-1 retail-2 ours-1 ours-2; do
    grep -E 'STUCK|GAME GONE|no window|no control' "$out/$r/drive.log" && { echo "$r: the drive did not get through"; fail=1; }
    grep -aE '^wine: Unhandled|Unhandled exception|Unhandled page fault' "$out/$r/game.log" && fail=1
done
for o in ours-1 ours-2; do
    diff "$out/retail-1/pilots.txt" "$out/$o/pilots.txt" >/dev/null ||
        { echo "$o: other flights are left in Pilots"; diff "$out/retail-1/pilots.txt" "$out/$o/pilots.txt"; fail=1; }
    changed=$(cd "$out/$o/changed" && find . -type f | sort)
    [ "$changed" = "$(cd "$out/retail-1/changed" && find . -type f | sort)" ] ||
        { echo "$o: the game changed other files:" $changed; fail=1; }
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        cmp -s "$out/retail-1/changed/$f" "$out/$o/changed/$f" || cmp -s "$out/retail-2/changed/$f" "$out/$o/changed/$f" ||
            { echo "$o: $f differs from retail's"; fail=1; }
    done <<<"$changed"
done
python3 -B tools/stn_compare.py "$out"/retail-1/Pilots "$out"/retail-2/Pilots "$out"/ours-1/Pilots "$out"/ours-2/Pilots || fail=1
same=0 total=0 moved=""
for picture in "$out"/ours-1/c*.bmp; do
    name=$(basename "$picture")
    total=$((total + 1))
    n=$(compare -metric AE "$out/retail-1/$name" "$picture" null: 2>&1 | head -1 || true)
    if [ "$n" = 0 ]; then same=$((same + 1)); else moved="$moved ${name%.bmp}($n)"; fi
done
echo "$(ls "$out"/retail-1/Pilots | wc -l) flights saved, $(ls "$out"/retail-1/pilots.txt >/dev/null && wc -l <"$out/retail-1/pilots.txt") left; changed: $(cd "$out/retail-1/changed" && find . -type f | sort | tr '\n' ' ')"
echo "$(grep -c '^== command' "$out/retail-1/drive.log") commands, $same of $total pictures identical; moved:$moved"
[ "$fail" = 0 ] && echo PASS || { echo FAIL; exit 1; }
