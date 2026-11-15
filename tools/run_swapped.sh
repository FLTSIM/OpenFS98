#!/usr/bin/env bash
# run_swapped.sh TARGET REPLACEMENT CMD...: put REPLACEMENT at TARGET, run CMD, and put back
# whatever was at TARGET (byte-checked), also on Ctrl-C or a failing CMD.  The swap is
# copy-then-mv so a running game never sees a half-written DLL.
set -u
target=$1 replacement=$2; shift 2
backup=$(mktemp /tmp/openfs98-swap.XXXXXX)
cp -p "$target" "$backup"
restore() {
    if cp -p "$backup" "$target.restore.$$" && mv -f "$target.restore.$$" "$target" && cmp -s "$backup" "$target"; then
        rm -f "$backup"
    else
        echo "restore failed; the original is in $backup" >&2
    fi
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
cp "$replacement" "$target.swap.$$" && mv -f "$target.swap.$$" "$target"
"$@"
