#!/usr/bin/env bash
# Open every MAIN menu command in the real game, on retail MAIN and the retail
# English dialog DLLs and on ours, and compare what each shows: controls and their text, and the dialog's pixels.  It
# presses Cancel through all of them, then OK.  Each run has its own copy of the
# game and of the wine prefix, so no other wine process is disturbed and the game
# tree is not written to; a nested X server keeps it off a screen that may be locked
# (a capture of a locked desktop shows the lock screen, and all of them match).
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
game=$(realpath "${FS98_DIR:-$PWD/game}")
out=$root/bin/main-dialogs
display=:97
commands="1308 1309 1607 1203 1201 1202 1204 1205 1301 1306 1307 1407 1409 1502 1101 300 1104"
# Then the English dialogs the first list does not reach: About's Credits (Help); the
# crash analysis (Sim), after setting "Detect crash and show graph" on the Crash / Damage
# tab, clicked by position since its tab and buttons are owner drawn; the lesson picker
# (Help, through ADVDRV), after the crash since OK may start a lesson; and the Exit
# confirmation (Alerts), answered No.  After the crash, so that loading a flight cannot
# unset the crash setting first, Flights' dialogs: Select Flight, the welcome dialog and
# its two menus (and through them, back into MAIN, Select Flight and Challenges), and the
# two pickers, whose OK starts a flight.  Save (1104) is in the first list: while a flight
# loads it opens nothing.  Options' dialogs: the Logbook's two (Add Logbook, Add Entry) and
# Preferences' Select Video (MAIN's Select demo) early; Flight video, Flight analysis,
# Customise controls and Instant replay last, since OK on Instant replay starts a replay;
# between them each tab page of Customise controls and Preferences, clicked by position (owner
# drawn), and Display options through the Display page's button.
# See tools/drive/fsdialogs.c.
steps="1607/3805 1407/4443 1407/4439 1409/4467 set:1205/@505,190/@80,174 crash 1102 1109 1109/502 1109/504 1109/502/3257 1109/502/3202 1106 1107 1105 exit 1404 1405 140a 140a/@530,158 140a/@530,197 1409/@500,157 1409/@500,196 1409/@500,235 1409/@500,274 1409/@500,313 1409/@500,235/4417 1403"
english="$(make -s english-modules-of) $(make -s english-candidates-of)"
for m in MAIN $english; do
    [ -f "bin/$m.DLL" ] || { echo "needs bin/$m.DLL" >&2; exit 2; }
done
[ -f bin/fsdialogs.exe ] && [ -f "$game/FLTSIM98.EXE" ] || {
    echo "needs bin/fsdialogs.exe and the staged game" >&2; exit 2;
}
command -v Xephyr >/dev/null || { echo "needs Xephyr" >&2; exit 2; }

rm -rf "$out"
mkdir -p "$out"
cp -a "${WINEPREFIX:-$HOME/.wine}" "$out/prefix"
# Without DirectSound: a crash waits for its sound before the analysis comes up
export WINEPREFIX=$out/prefix WINEDEBUG=-all WINEDLLOVERRIDES=dsound=d

Xephyr "$display" -screen 1024x768x24 -ac -noreset >"$out/xephyr.log" 2>&1 &
xephyr=$!
trap 'kill $xephyr 2>/dev/null || true' EXIT
export DISPLAY=$display
for _ in $(seq 20); do xdpyinfo >/dev/null 2>&1 && break; sleep 0.5; done

run() {     # tag dll button
    local dir=$out/$1 copy=$out/game-$1
    mkdir -p "$dir"
    cp -a "$game" "$copy"
    cp "$2" "$copy/Modules/MAIN.DLL"
    local m
    for m in $english; do      # the English DLLs and Flights: retail's or ours with MAIN
        if [[ $1 == ours-* ]]; then cp "bin/$m.DLL" "$copy/$(make -s installed-path-of M=$m)"
        else cp "$(make -s orig-module-of M=$m)" "$copy/$(make -s installed-path-of M=$m)"; fi
    done
    ( cd "$copy" && WINEDEBUG=err+all timeout 720 wine FLTSIM98.EXE '/STN:Situatio\STGEORGE.STN' >"$dir/game.log" 2>&1 ) &
    local pid=$!
    # shellcheck disable=SC2086
    wine bin/fsdialogs.exe "$(winepath -w "$dir")" 25000 "$3" $commands $steps || echo "$1: the drive exited $?"
    wineserver -k 2>/dev/null || true
    wait "$pid" || true
}

# The wall clock, on the clock dialog and the Create a Flight summary, and in Time and
# Season's hour and minute edits (the game takes its time of day from the computer's at
# startup, and ours starts minutes after retail); and the crash's speeds at impact, which
# depend on how a real-time dive went
normal() { sed -E -e 's/[0-9]{2}:[0-9]{2}(:[0-9]{2})?/TIME/g' -e "s/(id=(5094|5071) ')[-+0-9]*'/\1IMPACT'/" \
    -e "s/(id=(6909|6910|6904|6905) ')[0-9]*'/\1CLOCK'/" -e "s/(id=4508 ')[0-9]*'/\1REPLAY'/" "$1"; }

fail=0
for button in cancel ok; do
    run "retail-$button" orig/Modules/MAIN.DLL "$button"
    run "ours-$button" bin/MAIN.DLL "$button"
    r=$out/retail-$button o=$out/ours-$button
    if ! diff <(normal "$r/drive.log") <(normal "$o/drive.log") >"$out/$button.diff"; then
        echo "$button: the controls differ, see $out/$button.diff"; fail=1
    fi
    grep -E 'STUCK|GAME GONE|no window|nothing came up|no control' "$o/drive.log" && fail=1
    grep -aE '^wine: Unhandled|Unhandled exception|Unhandled page fault' "$o/game.log" && fail=1
    same=0 total=0 moved=""
    for picture in "$o"/c*.bmp; do
        name=$(basename "$picture")
        total=$((total + 1))
        n=$(compare -metric AE "$r/$name" "$picture" null: 2>&1 | head -1 || true)
        if [ "$n" = 0 ]; then same=$((same + 1)); else moved="$moved ${name%.bmp}($n)"; fi
    done
    echo "$button: $(grep -c '^== command' "$o/drive.log") commands, $same of $total pictures identical; moved:$moved"
done
wineserver -k 2>/dev/null || true
rm -rf "$out/prefix" "$out"/game-*      # 3 GB; the logs and pictures stay
[ "$fail" = 0 ] && echo PASS || { echo FAIL; exit 1; }
