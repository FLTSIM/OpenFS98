#!/usr/bin/env bash
# Temporarily test MAIN under its installed name; restore the exact prior DLL.
set -euo pipefail
cd "$(dirname "$0")/.."
game_dir="${FS98_DIR:-$PWD/game}"
game_dir="$(realpath "$game_dir")"
module_path="$game_dir/Modules/MAIN.DLL"
[ -f bin/MAIN.DLL ] && [ -f "$module_path" ] && [ -f "$game_dir/FLTSIM98.EXE" ] || {
    echo "build MAIN and stage the game before running main-smoke" >&2; exit 2;
}
if pgrep -f FLTSIM98.EXE >/dev/null 2>&1; then
    echo "a game instance is running; close it before main-smoke" >&2; exit 2
fi
backup_dir="$(mktemp -d /tmp/openfs98-main-smoke.XXXXXX)"
cp -p "$module_path" "$backup_dir/MAIN.DLL"
restore_module() {
    if cp -p "$backup_dir/MAIN.DLL" "$module_path.restore.$$" &&
       mv -f "$module_path.restore.$$" "$module_path" &&
       cmp -s "$backup_dir/MAIN.DLL" "$module_path"; then
        rm -rf "$backup_dir"
        echo "restored the previous MAIN.DLL (byte-for-byte)"
    else
        echo "restore failed; the original DLL is in $backup_dir" >&2
        return 1
    fi
}
trap restore_module EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
cp bin/MAIN.DLL "$module_path.main-smoke.$$"
mv -f "$module_path.main-smoke.$$" "$module_path"
cmp -s bin/MAIN.DLL "$module_path"
FS98_DIR="$game_dir" tools/smoketest.sh "${1:-75}" | tee "$backup_dir/smoke.log"
grep -q 'running the OpenFS98 build of MAIN.DLL' "$backup_dir/smoke.log"
grep -qx PASS "$backup_dir/smoke.log"
