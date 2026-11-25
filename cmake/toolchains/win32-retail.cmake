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
#   baselines without it: demodiff-run, advdrvdiff-run, g3ddiff-run and controlsdiff-run each 20000 scenarios, 0 differ.
#   So no baseline needs it; it stays because the byte-identity gates compare the DLLs with it.
# -fno-tree-loop-distribute-patterns, -fno-tree-vectorize
#   sites: the 25 simsui-native objects, SIM1_ours.sim (all.cmake), sim1rdiff.exe, simsuidiff.exe (difftests.cmake),
#          the SIMSUI break build (tools/simsui_breaks.py)
#   without either: code changes in SIM1_ours.sim, sim1rdiff.exe, simsuidiff.exe and in several simsui objects
#   (loop-distribute: text, textart, cdib; vectorize: dib, dibsection, button, dialog, tabs, slider, textart, cdib,
#   native). Baselines without either: sim1rdiff-run 372000 comparisons, 0 differ; simsuidiff-run N=2000, 0 differ.
#   So no baseline needs them; they stay for the byte-identity gates.
# -fno-math-errno
#   sites: SIM1_ours.sim (all.cmake), sim1rdiff.exe (difftests.cmake)
#   without it: no code change in either site (sim1rdiff-run 372000 comparisons, 0 differ); the flag has no effect today.
#
# The machine-readable table, checked on every build by tools/check_codegen_overrides.py (see its docstring):
#   #@ <flag>[,<flag>] | <file> | <output> | <source> | <count> | <reason>
# One row per site: a gcc recipe, named by its file, its -o output and its first .c/.S token (`-` for none, fnmatch
# patterns). A row covers every recipe that matches and states how many that is (three for PANELS_unit.dll), so each is checked. A site
# lacking its flag fails the build, and so does a flag no recipe carries; an unlisted extra use is allowed.
# Runners that delegate to CMake (SIM1R through difftests; ANALYSIS, CONTROLS, G3D, PANELS, REALITY and WINDOW through
# the break-build recipes in cmake/targets.cmake) are covered through those recipes.
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/ADVDRV.DLL | src/advdrv/advdrv.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/ANALYSIS.DLL | src/analysis/analysis.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/CONTROLS.DLL | src/controls/controls.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/DEMO.DLL | src/demo/demo.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/G3D.DLL | src/g3d/g3d.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/PANELS.DLL | src/panels/panels.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/REALITY.DLL | src/reality/reality.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/steps/all.cmake | bin/WINDOW.DLL | src/window/window.c | 1 | module build; byte-identity gates
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/PANELS_unit.dll | src/panels/panels.c | 3 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/G3D_ours.dll | src/g3d/g3d.c | 1 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/G3D_ours.dll | src/g3d/g3d_mesh.c | 1 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/ADVDRV_ours.dll | src/advdrv/advdrv.c | 2 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/ANALYSIS_ours.dll | - | 1 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/CONTROLS_ours.dll | bin/difftest/controls_break.c | 1 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | ${PANELS_OUT:-} | - | 1 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/REALITY_ours.dll | - | 1 | difftest and break build of the module
#@ -fno-ipa-icf | cmake/targets.cmake | bin/difftest/WINDOW_ours.dll | - | 1 | difftest and break build of the module
#@ -fno-tree-loop-distribute-patterns,-fno-tree-vectorize | cmake/steps/all.cmake | bin/simsui-native/*.o | src/simsui/*.c | 26 | the 24 loop-generated units and the two explicit compiles
#@ -fno-tree-loop-distribute-patterns,-fno-tree-vectorize,-fno-math-errno | cmake/steps/all.cmake | bin/sim1-native/SIM1_ours.sim | src/sim1/sim1_native.c | 1 | SIM1 build
#@ -fno-tree-loop-distribute-patterns,-fno-tree-vectorize,-fno-math-errno | cmake/steps/difftests.cmake | bin/sim1-diff/sim1rdiff.exe | tools/difftest/sim1rdiff.c | 1 | SIM1R harness
#@ -fno-tree-loop-distribute-patterns,-fno-tree-vectorize | cmake/steps/difftests.cmake | bin/simsui-diff/simsuidiff.exe | tools/difftest/simsuidiff.c | 1 | SIMSUI harness
#@ -fno-tree-loop-distribute-patterns,-fno-tree-vectorize | tools/simsui_breaks.py | * | tools/difftest/simsuidiff.c | 1 | SIMSUI break runner compiles the unit itself
