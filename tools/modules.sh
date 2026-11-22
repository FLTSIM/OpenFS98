#!/bin/sh
# The module lists and names from tools/modules.txt (what the retired Makefile's `make -s modules-of` and friends printed).
#   tools/modules.sh modules-of | english-modules-of | english-candidates-of | shared-modules-of | shared-candidates-of
#   tools/modules.sh installed-name-of | installed-path-of | orig-module-of NAME
list="$(dirname "$0")/modules.txt"
case "$1" in
    modules-of)            kind=module ;;
    english-modules-of)    kind=english ;;
    english-candidates-of) kind=english-candidate ;;
    shared-modules-of)     kind=shared ;;
    shared-candidates-of)  kind=shared-candidate ;;
    installed-name-of|installed-path-of|orig-module-of)
        [ -n "$2" ] || { echo "usage: $0 $1 NAME" >&2; exit 2; }
        line=$(grep -v '^#' "$list" | awk -v m="$2" '$1 == m')
        [ -n "$line" ] || { echo "$0: no module $2 in $list" >&2; exit 2; }
        set -- "$1" $line
        case "$1" in
            installed-path-of) echo "$4" ;;
            orig-module-of)    echo "orig/$5" ;;
            # the Makefile's installed_name: the file name for Modules/, NAME.DLL for the rest
            installed-name-of) case "$4" in Modules/*) echo "${4#Modules/}" ;; *) echo "$2.DLL" ;; esac ;;
        esac
        exit 0 ;;
    *) echo "usage: $0 modules-of|english-modules-of|...|installed-path-of NAME" >&2; exit 2 ;;
esac
grep -v '^#' "$list" | awk -v k="$kind" '$2 == k { printf "%s%s", sep, $1; sep = " " } END { print "" }'
