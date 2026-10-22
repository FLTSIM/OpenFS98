#!/usr/bin/env bash
# Temporarily test MULTIP under its installed name; restore the exact prior DLL.
set -euo pipefail
cd "$(dirname "$0")/.."
game_dir="${FS98_DIR:-$PWD/game}"
game_dir="$(realpath "$game_dir")"
module_path="$game_dir/Modules/MULTIPLAYER.DLL"
[ -f bin/MULTIP.DLL ] && [ -f "$module_path" ] && [ -f "$game_dir/FLTSIM98.EXE" ] || {
    echo "build MULTIP and stage the game before running multip-smoke" >&2; exit 2;
}
if pgrep -f FLTSIM98.EXE >/dev/null 2>&1; then
    echo "a game instance is running; close it before multip-smoke" >&2; exit 2
fi
backup_dir="$(mktemp -d /tmp/openfs98-multip-smoke.XXXXXX)"
cp -p "$module_path" "$backup_dir/MULTIPLAYER.DLL"
restore_module() {
    if cp -p "$backup_dir/MULTIPLAYER.DLL" "$module_path.restore.$$" &&
       mv -f "$module_path.restore.$$" "$module_path" &&
       cmp -s "$backup_dir/MULTIPLAYER.DLL" "$module_path"; then
        rm -rf "$backup_dir"
        echo "restored the previous MULTIPLAYER.DLL (byte-for-byte)"
    else
        echo "restore failed; the original DLL is in $backup_dir" >&2
        return 1
    fi
}
trap restore_module EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
cp bin/MULTIP.DLL "$module_path.multip-smoke.$$"
mv -f "$module_path.multip-smoke.$$" "$module_path"
cmp -s bin/MULTIP.DLL "$module_path"
FS98_DIR="$game_dir" tools/smoketest.sh "${1:-75}" | tee "$backup_dir/smoke.log"
grep -q 'running the OpenFS98 build of MULTIP.DLL' "$backup_dir/smoke.log"
grep -qx PASS "$backup_dir/smoke.log"
