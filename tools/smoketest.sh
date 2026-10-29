#!/usr/bin/env bash
# Boot the game under wine and assert it actually comes up.
#
# Usage: tools/smoketest.sh [seconds]
#
# "Still running" is NOT a success criterion.  FS98 reports startup problems in
# *modal* dialogs, so a game wedged behind "Aircraft initialization failure"
# looks exactly like a game that is flying -- which is how an earlier version of
# this script passed a run that never got past the intro.  The signal used here
# is gauge loading: the panel .GAU modules only load once an aircraft has been
# resolved, its visual model found and its panel built, so a single .GAU load
# means startup went all the way through.
#
# Module names are checked against their *installed* names, which is not what is
# on the CD -- setup renames FACIL.DLL to FACILITIES.DLL and MULTIP.DLL to
# MULTIPLAYER.DLL.
set -u
cd "$(dirname "$0")/.."
SECS="${1:-75}"
GAME="${FS98_DIR:-$PWD/game}"
TRACE="$(mktemp)"
trap 'rm -f "$TRACE"' EXIT

[ -f "$GAME/FLTSIM98.EXE" ] || { echo "no FLTSIM98.EXE in $GAME -- run tools/stage_game.py" >&2; exit 2; }

# A second instance started while one is already up produces an empty trace and
# looks exactly like a total failure to load.  Refuse rather than lie.
if pgrep -f FLTSIM98.EXE >/dev/null 2>&1; then
    echo "a FLTSIM98 instance is already running -- close it first" >&2
    exit 2
fi

wineboot -u >/dev/null 2>&1
( cd "$GAME" && timeout "$SECS" env WINEDEBUG=+loaddll wine FLTSIM98.EXE ) >"$TRACE" 2>&1

fail=0
# SEHFIX is a wine compatibility shim, not one of the sim's modules: it is
# left out of the count so the baseline stays the retail module set.
sehfix=0; [ -e "$GAME/Modules/SEHFIX.DLL" ] && sehfix=1
want=$(ls "$GAME/Modules" | grep -vxi 'SEHFIX.DLL' | wc -l)
mods=0; missing=()
for f in "$GAME"/Modules/*; do
    m=$(basename "$f")
    [ "$m" = "SEHFIX.DLL" ] && continue
    if grep -qiE "MODULES.+${m//./\\.}" "$TRACE"; then mods=$((mods+1)); else missing+=("$m"); fi
done
gaus=$(grep -ciE '\.GAU' "$TRACE")

echo "modules loaded: $mods/$want"
echo "gauges loaded:  $gaus"
[ ${#missing[@]} -eq 0 ] || { printf 'FAIL: never loaded: %s\n' "${missing[*]}"; fail=1; }
[ "$gaus" -gt 0 ] || { echo "FAIL: no gauges -- startup stopped before the panel was built"; fail=1; }

# "KiUserCallbackDispatcher ignoring exception" is wine swallowing a divide
# fault the host would have repaired (docs/wine.md).  Without SEHFIX it is
# expected at startup; with SEHFIX installed it means a fault got past it.
ignore='winediag|RandR'
[ $sehfix -eq 1 ] || ignore="$ignore|KiUserCallbackDispatcher"
echo "SEHFIX installed: $([ $sehfix -eq 1 ] && echo yes || echo no)"
errs=$(grep -iE 'err:|unhandled exception' "$TRACE" | grep -viE "$ignore" || true)
[ -n "$errs" ] && { echo "FAIL: errors in trace:"; echo "$errs" | head -8; fail=1; }

for f in bin/*.DLL; do
    [ -e "$f" ] || continue
    m=$(basename "$f")
    # OFSTEST is a test fixture, not one of the sim's modules: it is installed
    # only by `make ofstest-run` and removed again afterwards.
    [ "$m" = "OFSTEST.DLL" ] && continue
    [ "$m" = "SEHFIX.DLL" ] && continue
    # *_TRACE builds are instrumented variants installed only by their own
    # targets (make util-trace), never by make install.
    case "$m" in *_TRACE.DLL) continue ;; esac
    case "$m" in
        FACIL.DLL)  installed=Modules/FACILITIES.DLL ;;
        MULTIP.DLL) installed=Modules/MULTIPLAYER.DLL ;;
        MPCHAT.DLL) installed=Modules/mpchat.DLL ;;
        WORLD.DLL)  installed=English/World.DLL ;;
        *)          installed="Modules/$m" ;;
    esac
    if cmp -s "$f" "$GAME/$installed"; then
        echo "  running the OpenFS98 build of $m"
    else
        echo "  NOTE: $m differs from the installed DLL -- this exact build was not tested."
        echo "        run 'make install' to test what you just built."
        [ "$m" != WORLD.DLL ] || fail=1
    fi
    if [ "$m" = WORLD.DLL ] && ! grep -qiE 'english.+world\.dll' "$TRACE"; then
        echo "FAIL: English/World.DLL was not loaded"
        fail=1
    fi
done

[ $fail -eq 0 ] && echo PASS || echo FAILED
exit $fail
