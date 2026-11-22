#!/bin/sh
# Story 1.3: every gcc invocation takes OFS_MODULE_FLAGS, and only the toolchain file spells the six.
cd "$(dirname "$0")/.." || exit 1
fail=0
spelled=$(grep -rnE -- '-f(no-)?(wrapv|strict-aliasing|delete-null-pointer-checks)|-mfpmath|-ffp-contract|-fexcess-precision' \
    cmake CMakeLists.txt tools/*.py tools/*.sh tools/ofs | grep -v '^cmake/toolchains/win32-retail.cmake:' | grep -v '^tools/check_module_flags.sh:')
[ -z "$spelled" ] || { echo "spells a module flag itself:"; echo "$spelled"; fail=1; }
bare=$(grep -n 'mingw32-gcc ' cmake/steps/*.cmake cmake/targets.cmake | grep -v 'mingw32-gcc @MODULE_FLAGS@ ')
[ -z "$bare" ] || { echo "gcc without @MODULE_FLAGS@:"; echo "$bare"; fail=1; }
nopy=$(grep -L 'module_flags' $(grep -l 'mingw32-gcc' tools/*.py))
[ -z "$nopy" ] || { echo "Python gcc site without module_flags:"; echo "$nopy"; fail=1; }
[ $fail = 0 ] && echo "module flags: every site takes OFS_MODULE_FLAGS"
exit $fail
