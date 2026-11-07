#!/usr/bin/env bash
# Compare host-driven dialogs and exercise both cancelled and accepted exit.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
out=$(realpath -m "${HOST_UI_OUT:-bin/host-runtime/ui}")
game=$(realpath "${FS98_DIR:-game}")
candidate=$(realpath "${HOST_UI_EXE:-bin/OPENFS98.EXE}")
mkdir -p "$out"
[ -f bin/fsdialogs.exe ] || { echo 'build bin/fsdialogs.exe first' >&2; exit 2; }
[ -d "$out/prefix" ] || cp -a --reflink=auto "${WINEPREFIX:-$HOME/.wine}" "$out/prefix"
export WINEPREFIX=$out/prefix WINEDEBUG=-all DISPLAY=${HOST_UI_DISPLAY:-:71} WINEDLLOVERRIDES=dsound=d
Xvfb "$DISPLAY" -screen 0 1024x768x24 -ac -noreset > "$out/xvfb.log" 2>&1 &
host_xvfb_pid=$!
trap 'wineserver -k 2>/dev/null || true; kill "$host_xvfb_pid" 2>/dev/null || true' EXIT
for _ in {1..20}; do xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
for side in retail ours; do
    [ "${HOST_UI_COMPARE_ONLY:-0}" != 1 ] || break
    copy=$out/game-$side
    [ ! -e "$copy" ] || { echo "refusing to reuse $copy" >&2; exit 2; }
    cp -a --reflink=auto "$game" "$copy"
    mkdir -p "$out/$side"
    executable=$root/orig/FLTSIM98.EXE
    [ "$side" != ours ] || executable=$candidate
    cp "$executable" "$copy/FLTSIM98.EXE"
    sha256sum "$copy/FLTSIM98.EXE" > "$out/$side/executable.sha256"
    rm -f "$copy/Modules/SEHFIX.DLL"
    [ "$side" != retail ] || cp bin/SEHFIX.DLL "$copy/Modules/SEHFIX.DLL"
    (cd "$copy" && timeout 240 wine FLTSIM98.EXE '/STN:Situatio\STGEORGE.STN') > "$out/$side/game.log" 2>&1 &
    host_game_pid=$!
    timeout 200 wine bin/fsdialogs.exe "$(winepath -w "$out/$side")" 12000 cancel 1607/3805 1301 1203 resize:800x600 resize:640x480 exit quit > "$out/$side/driver.log" 2>&1
    wait "$host_game_pid"
    grep -q 'the game has gone' "$out/$side/drive.log"
    if grep -E 'STUCK|GAME GONE|no window|nothing came up|no control' "$out/$side/drive.log"; then exit 1; fi
    if grep -aiE 'Unhandled exception|Unhandled page fault|host runtime: exception|unsupported state' "$out/$side/game.log"; then exit 1; fi
    echo "$side: dialogs and normal process exit completed"
    wineserver -k 2>/dev/null || true
    rm -rf "$copy"
done
# As in main_dialogs.sh, normalize only the four wall-clock edit values.
normal() { sed -E -e 's/[0-9]{2}:[0-9]{2}(:[0-9]{2})?/TIME/g' \
    -e "s/(id=(6909|6910|6904|6905) ')[0-9]*'/\\1CLOCK'/" "$1"; }
diff -u <(normal "$out/retail/drive.log") <(normal "$out/ours/drive.log") > "$out/controls.diff"
for picture in c1607-3805.bmp cexit.bmp; do
    test -s "$out/retail/$picture"
    pixels=$(compare -metric AE "$out/retail/$picture" "$out/ours/$picture" null: 2>&1 || true)
    echo "$picture: $pixels differing pixels"
    test "$pixels" = 0
done
echo PASS
