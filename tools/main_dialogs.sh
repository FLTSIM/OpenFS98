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
commands="1308 1309 1607 1203 1201 1202 1204 1205 1301 1306 1307 1407 1409 1502 1101 300"
english=$(make -s english-modules-of)
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
export WINEPREFIX=$out/prefix WINEDEBUG=-all

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
    for m in $english; do      # Alerts, Help, Sim, Views and World: retail's or ours with MAIN
        if [[ $1 == ours-* ]]; then cp "bin/$m.DLL" "$copy/$(make -s installed-path-of M=$m)"
        else cp "$(make -s orig-module-of M=$m)" "$copy/$(make -s installed-path-of M=$m)"; fi
    done
    ( cd "$copy" && WINEDEBUG=err+all timeout 480 wine FLTSIM98.EXE '/STN:Situatio\STGEORGE.STN' >"$dir/game.log" 2>&1 ) &
    local pid=$!
    # shellcheck disable=SC2086
    wine bin/fsdialogs.exe "$(winepath -w "$dir")" 25000 "$3" $commands || echo "$1: the drive exited $?"
    wineserver -k 2>/dev/null || true
    wait "$pid" || true
}

# The wall clock, on the clock dialog and the Create a Flight summary
normal() { sed -E 's/[0-9]{2}:[0-9]{2}(:[0-9]{2})?/TIME/g' "$1"; }

fail=0
for button in cancel ok; do
    run "retail-$button" orig/Modules/MAIN.DLL "$button"
    run "ours-$button" bin/MAIN.DLL "$button"
    r=$out/retail-$button o=$out/ours-$button
    if ! diff <(normal "$r/drive.log") <(normal "$o/drive.log") >"$out/$button.diff"; then
        echo "$button: the controls differ, see $out/$button.diff"; fail=1
    fi
    grep -E 'STUCK|GAME GONE|no window' "$o/drive.log" && fail=1
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
