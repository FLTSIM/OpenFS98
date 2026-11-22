# win32-retail: mingw-w64 i686, for retail-ABI modules, difftests and reference recording (AD-11).
# OFS_MODULE_FLAGS is the one semantic flag set: signed overflow, aliasing, null checks and x87 arithmetic
# as retail's MSVC code relies on them. Every gcc invocation takes it from here: CMake steps through the
# @MODULE_FLAGS@ placeholder (cmake/OpenFS98.cmake), the OpenFS98::module interface target (CMakeLists.txt),
# Python scripts through tools/module_flags.py, which parses the set() line below (keep it on one line).
# Optimisation levels, -std, freestanding, -nostdlib, image bases and link flags stay in each build rule.
set(OFS_MODULE_FLAGS -fwrapv -fno-strict-aliasing -fno-delete-null-pointer-checks -mfpmath=387 -ffp-contract=off -fexcess-precision=standard)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(OFS_TRIPLE i686-w64-mingw32)
set(CMAKE_C_COMPILER ${OFS_TRIPLE}-gcc)
set(CMAKE_RC_COMPILER ${OFS_TRIPLE}-windres)
set(OFS_TOOLCHAIN win32-retail)

# Codegen overrides that stay in the build rules, not in OFS_MODULE_FLAGS. Each was removed in a copy of the tree
# (2026-11-19, Story 1.3), the affected units rebuilt, their sections compared with the build that has it, and
# the unit's difftest baseline run without it. "Code changes" = .text/.data/.rdata of the unit differ.
#
# -fno-ipa-icf
#   sites: ADVDRV, ANALYSIS, CONTROLS, DEMO, G3D, PANELS, REALITY, WINDOW (cmake/steps/all.cmake) and their
#          difftest builds (*_ours.dll, PANELS_unit.dll in cmake/targets.cmake)
#   without it: code changes in ADVDRV, CONTROLS, DEMO, G3D; ANALYSIS, PANELS, REALITY, WINDOW unchanged.
#   baselines without it: demodiff-run 20000 scenarios, 0 differ (so DEMO's baseline does not need it).
#   see the spec's Implementation Notes for g3ddiff-run, advdrvdiff-run and controlsdiff-run.
# -fno-tree-loop-distribute-patterns, -fno-tree-vectorize
#   sites: the 25 simsui-native objects, SIM1_ours.sim (all.cmake), sim1rdiff.exe, simsuidiff.exe (difftests.cmake),
#          the SIMSUI break build (tools/simsui_breaks.py)
#   without either: code changes in SIM1_ours.sim, sim1rdiff.exe, simsuidiff.exe and in several simsui objects
#   (loop-distribute: text, textart, cdib; vectorize: dib, dibsection, button, dialog, tabs, slider, textart, cdib,
#   native). Baselines: see the spec's Implementation Notes (sim1rdiff-run, simsuidiff-run).
# -fno-math-errno
#   sites: SIM1_ours.sim (all.cmake), sim1rdiff.exe (difftests.cmake)
#   without it: no code change in either site; nothing breaks, so the flag has no effect today.
