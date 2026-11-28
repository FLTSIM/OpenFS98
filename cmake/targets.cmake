# One CMake target per make target named in docs/tools.md (Story 1.2). Commands are `make -n -B <target>`'s,
# minus the build steps in cmake/steps/ (those become dependencies). Parameters come from the environment with
# the Makefile's defaults: `SUITE=nav cmake --build <dir> --target maindiff-run`. Each target also runs its own
# helper builds every time (make -B), where the Makefile rebuilt only when out of date.

ofs_target(difftest-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/SLEW.DLL bin/difftest/SLEW_retail.dll]==]
    [==[cp bin/SLEW.DLL bin/difftest/SLEW_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine slewdiff.exe ${N:-}]==]
  )

ofs_target(utildiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp bin/UTIL.DLL bin/difftest/UTIL_ours.dll]==]
    [==[cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine utildiff.exe]==]
  )

ofs_target(utildiff-portable
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp bin/UTIL.DLL bin/difftest/UTIL_ours.dll]==]
    [==[cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine utildiff_portable.exe]==]
  )

ofs_target(optionseventsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_optionsdiff.sh ${N:-20000} ${SEED:-0x5f1a6c2d} ${MODE:-normal}]==]
  )

ofs_target(options-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_optionsdiff.sh --breaks ${N:-20000}]==]
  )

ofs_target(options
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(numericdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine numericdiff.exe]==]
  )

# The two hand-written test gauges, built once for every target that runs them (bin/gauges-diff/; the data is committed in src/gauges/).
ofs_target(test-gauges
  COMMANDS
    [==[python3 -B tools/gen_warning_data.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/gauges-diff/warning.res bin/gauges-diff/warning-res.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -Ibin/gauges-diff -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x32000000 -o bin/gauges-diff/737-400.WARNING_ENUNCIATORS.GAU src/gauges/warning.c src/gauges/warning.def bin/gauges-diff/warning-res.o -lkernel32]==]
    [==[python3 -B tools/gen_suction_data.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/gauges-diff/suction.res bin/gauges-diff/suction-res.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -Ibin/gauges-diff -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x32010000 -o bin/gauges-diff/CESSNA_182.SUCTION.GAU src/gauges/suction.c src/gauges/suction.def bin/gauges-diff/suction-res.o -lkernel32]==]
    [==[python3 -B tools/check_data_image.py --hash WARNING bin/gauges-diff/737-400.WARNING_ENUNCIATORS.GAU]==]
    [==[python3 -B tools/check_data_image.py --hash SUCTION bin/gauges-diff/CESSNA_182.SUCTION.GAU]==]
  )

ofs_target(data-images
  DEPENDS openfs98 test-gauges
  COMMANDS
    [==[python3 -B tools/check_data_image.py --orig orig --manifest tools/data_images.txt --self-test]==]
)

ofs_target(atcdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ATC.DLL bin/difftest/ATC_retail.dll]==]
    [==[cp bin/ATC.DLL bin/difftest/ATC_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 900 wine atcdiff.exe ${N:-}]==]
  )

ofs_target(acontaindiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ACONTAIN.DLL bin/difftest/ACONTAIN_retail.dll]==]
    [==[cp bin/ACONTAIN.DLL bin/difftest/ACONTAIN_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/ENGLISH.FLL bin/difftest/ENGLISH.FLL]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 1800 wine acontaindiff.exe ${N:-}]==]
  )

ofs_target(sim1
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(sim1-regression-run
  DEPENDS difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o bin/sim1-diff/SIM1_PARTIAL.DLL src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S src/sim1/sim1.def]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o bin/sim1-diff/SIM1_STATE_TEST.DLL src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S tools/sim1_state_test.def]==]
    [==[for order in retail-first ours-first relocate@SEMI@ do WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1diff.exe ${N:-20000} $order bin/sim1-diff/SIM1_PARTIAL.DLL || exit $?@SEMI@ WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1statediff.exe ${N:-20000} $order bin/sim1-diff/SIM1_STATE_TEST.DLL || exit $?@SEMI@ done]==]
  )

ofs_target(sim1rdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[WINEDEBUG=-all timeout 7200 wine bin/sim1-diff/sim1rdiff.exe ${SIM1R_N-2000} ${SIM1R_SEED-0x51e1}]==]
  )

ofs_target(sim1native-flights
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--image-base=0x20970000 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -o bin/hg2d-diff/FRAMELOCK.DLL tools/drive/framelock.c tools/drive/framelock.def -lkernel32 -luser32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
    [==[python3 -B tools/fly_scripts.py bin/sim1-native/scripts]==]
    [==[FS98_DIR="$PWD/game" HG2D_FRAMES_ONLY=SIM1 HG2D_FRAMES_SIM1="$PWD/bin/sim1-native/SIM1_ours.sim" HG2D_FRAMES_NOSOUND=1 FRAMES_MANY_OUT=bin/sim1-native/flights FRAMES_MANY_PASSES=1200 FRAMES_MANY_JOBS=8 tools/frames_many.sh bin/sim1-native/scripts/controls.txt]==]
    [==[test "$(grep -c ': match' bin/sim1-native/flights/summary.txt)" -eq 74]==]
  )

# Story 1.6: retail's reference logs (tools/record_references.sh); CASES="flight/PARIS ..." and REFERENCES_OUT for a subset
ofs_target(references
  DEPENDS framelock sehfix
  COMMANDS
    [==[tools/record_references.sh ${CASES:-}]==]
  )

ofs_target(sim1-install
  DEPENDS openfs98
  COMMANDS
    [==[python3 -B tools/stage_sim1.py "${FS98_DIR:-$HOME/.wine/drive_c/FS98}"]==]
  )

ofs_target(mdls
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(mdls-install
  DEPENDS openfs98
  COMMANDS
    [==[python3 -B tools/stage_mdls.py "${FS98_DIR:-$HOME/.wine/drive_c/FS98}" --all]==]
  )

ofs_target(mdls-revert
  DEPENDS openfs98
  COMMANDS
    [==[python3 -B tools/stage_mdls.py "${FS98_DIR:-$HOME/.wine/drive_c/FS98}" --all --retail]==]
  )

ofs_target(run
  COMMANDS
    [==[echo "modules installed:"@SEMI@ for m in $(tools/modules.sh modules-of) $(tools/modules.sh english-modules-of) $(tools/modules.sh shared-modules-of)@SEMI@ do n=$(tools/modules.sh installed-path-of $m)@SEMI@ o=$(tools/modules.sh orig-module-of $m)@SEMI@ if cmp -s "$o" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/$n"@SEMI@ then echo " $n retail"@SEMI@ elif cmp -s bin/$m.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/$n"@SEMI@ then echo " $n OpenFS98"@SEMI@ else echo " $n unknown"@SEMI@ fi@SEMI@ done]==]
    [==[cd ${FS98_DIR:-$HOME/.wine/drive_c/FS98} && WINEDEBUG=-all wine $(test -n "${VD-1024x768}" && echo explorer /desktop=FS98,${VD-1024x768}) FLTSIM98.EXE]==]
  )

ofs_target(mdl-normal-diff-run
  DEPENDS difftests
  COMMANDS
    [==[WINEPREFIX="$PWD/bin/mdl-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine bin/mdl-diff/mdldiff.exe ${N:-70000}]==]
    [==[WINEPREFIX="$PWD/bin/mdl-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine bin/mdl-diff/mdldiff.exe ${N:-70000} relocate]==]
  )

ofs_target(mdl-normal-breaks
  DEPENDS difftests
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_breaks.py]==]
  )

ofs_target(mdl-normal-modules
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(mdl-normal-module-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_module_breaks.py]==]
  )

ofs_target(mdl-normal-game
  DEPENDS openfs98 difftests
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_frames.py --passes ${N:-600} --jobs ${JOBS:-4}]==]
  )

ofs_target(mdl-crash-modules
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(mdl-crash-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_crash_breaks.py]==]
  )

ofs_target(host-rt-tables-check
  COMMANDS
    [==[python3 -B tools/gen_host_rt_tables.py]==]
  )

ofs_target(simsui-tables-check
  COMMANDS
    [==[python3 -B tools/gen_simsui_tables.py]==]
  )

ofs_target(hostrtdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[WINEDEBUG=-all timeout 1800 wine bin/host-rt/hostrtdiff.exe ${N:-2000}]==]
  )

ofs_target(dividediff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[WINEDEBUG=-all timeout 1800 wine bin/difftest/dividediff.exe ${N:-2000}]==]
  )

ofs_target(host-exe
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(host-rt-exe
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(mdl-crash-game
  DEPENDS openfs98 difftests
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_crash_frames.py --passes ${N:-1200} --jobs ${JOBS:-3}]==]
  )

ofs_target(sounddiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/SOUND.DLL bin/difftest/SOUND_retail.dll]==]
    [==[cp bin/SOUND.DLL bin/difftest/SOUND_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/ENGLISH.FLL bin/difftest/ENGLISH.FLL]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[mkdir -p bin/difftest/cdsound]==]
    [==[for a in B737_400 Extra300 Bell206B Schweizr Lear45 Sopcamel C182@SEMI@ do cp orig/Aircraft/$a/sound/sound.cfg bin/difftest/cdsound/$a.cfg@SEMI@ done]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine sounddiff.exe ${N:-}]==]
  )

ofs_target(dynamicdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/DYNAMIC.DLL bin/difftest/DYNAMIC_retail.dll]==]
    [==[cp bin/DYNAMIC.DLL bin/difftest/DYNAMIC_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[mkdir -p bin/difftest/cdscenery]==]
    [==[cp orig/Scenery/*5.bgl bin/difftest/cdscenery/]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine dynamicdiff.exe ${N:-}]==]
  )

ofs_target(fegriddiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_retail.dll]==]
    [==[cp bin/FE.DLL bin/difftest/FE_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine fegriddiff.exe ${N:-}]==]
  )

ofs_target(fediff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_retail.dll]==]
    [==[cp bin/FE.DLL bin/difftest/FE_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cd bin/difftest && test -n "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}" || { echo "FE fixture missing: run tools/extract.sh again or tools/ofs stage" >&2@SEMI@ exit 2@SEMI@ }]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine fediff.exe ${N:-20000} "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}"]==]
  )

ofs_target(landmediff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/LANDME.DLL bin/difftest/LANDME_retail.dll]==]
    [==[cp bin/LANDME.DLL bin/difftest/LANDME_ours.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_landme_retail.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_landme_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cd bin/difftest && test -n "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}" || { echo "landmediff-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"@SEMI@ exit 2@SEMI@ }]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine landmediff.exe ${N:-20000} "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}"]==]
  )

ofs_target(facildiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/facil.dll bin/difftest/FACIL_retail.dll]==]
    [==[cp bin/FACIL.DLL bin/difftest/FACIL_ours.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_facil_retail.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_facil_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cd bin/difftest && test -n "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}" || { echo "facildiff-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"@SEMI@ exit 2@SEMI@ }]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine facildiff.exe FACIL_retail.dll FACIL_ours.dll "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}"]==]
  )

ofs_target(boosterdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/BOOSTER.DLL bin/difftest/BOOSTER_retail.dll]==]
    [==[cp bin/BOOSTER.DLL bin/difftest/BOOSTER_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[cd bin/difftest && test -n "${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}" || { echo "boosterdiff-run needs the CD tree in cd/ (cd/Boosters)"@SEMI@ exit 2@SEMI@ }]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 7200 wine boosterdiff.exe ${N:-20000} ${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}]==]
  )

ofs_target(windowdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll]==]
    [==[cp bin/WINDOW.DLL bin/difftest/WINDOW_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 7200 wine windowdiff.exe ${N:-20000} $(f=${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}@SEMI@ echo ${f:-.})]==]
  )

ofs_target(window-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll]==]
    [==[cp bin/WINDOW.DLL bin/difftest/WINDOW_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[python3 -B tools/window_breaks.py ${N:-2000}]==]
  )

ofs_target(mpchatdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/mpchat.DLL bin/difftest/MPCHAT_retail.dll]==]
    [==[cp bin/MPCHAT.DLL bin/difftest/MPCHAT_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine mpchatdiff.exe ${N:-20000}]==]
  )

ofs_target(mpchat-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/mpchat.DLL bin/difftest/MPCHAT_retail.dll]==]
    [==[cp bin/MPCHAT.DLL bin/difftest/MPCHAT_ours.dll]==]
    [==[python3 -B tools/mpchat_breaks.py ${N:-2000}]==]
  )

ofs_target(winmsgdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WINMSG.DLL bin/difftest/WINMSG_retail.dll]==]
    [==[cp bin/WINMSG.DLL bin/difftest/WINMSG_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine winmsgdiff.exe ${N:-20000}]==]
  )

ofs_target(winmsg-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WINMSG.DLL bin/difftest/WINMSG_retail.dll]==]
    [==[cp bin/WINMSG.DLL bin/difftest/WINMSG_ours.dll]==]
    [==[python3 -B tools/winmsg_breaks.py ${N:-2000}]==]
  )

ofs_target(realitydiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/REALITY.DLL bin/difftest/REALITY_retail.dll]==]
    [==[cp bin/REALITY.DLL bin/difftest/REALITY_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine realitydiff.exe ${N:-20000}]==]
  )

ofs_target(reality-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/REALITY.DLL bin/difftest/REALITY_retail.dll]==]
    [==[cp bin/REALITY.DLL bin/difftest/REALITY_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[python3 -B tools/reality_breaks.py ${N:-2000}]==]
  )

ofs_target(analysisdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ANALYSIS.DLL bin/difftest/ANALYSIS_retail.dll]==]
    [==[cp bin/ANALYSIS.DLL bin/difftest/ANALYSIS_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine analysisdiff.exe ${N:-20000}]==]
  )

ofs_target(analysis-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ANALYSIS.DLL bin/difftest/ANALYSIS_retail.dll]==]
    [==[cp bin/ANALYSIS.DLL bin/difftest/ANALYSIS_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[python3 -B tools/analysis_breaks.py ${N:-2000}]==]
  )

ofs_target(controlsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/CONTROLS.DLL bin/difftest/CONTROLS_retail.dll]==]
    [==[cp bin/CONTROLS.DLL bin/difftest/CONTROLS_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cp orig/ENGLISH.FLL bin/difftest/ENGLISH.FLL]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout ${CONTROLS_TIMEOUT-3600} wine controlsdiff.exe ${N:-20000}]==]
  )

ofs_target(controls-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/CONTROLS.DLL bin/difftest/CONTROLS_retail.dll]==]
    [==[cp bin/CONTROLS.DLL bin/difftest/CONTROLS_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cp orig/ENGLISH.FLL bin/difftest/ENGLISH.FLL]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[python3 -B tools/controls_breaks.py ${N:-2000}]==]
  )

ofs_target(panelsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/PANELS.DLL bin/difftest/PANELS_retail.dll]==]
    [==[cp bin/PANELS.DLL bin/difftest/PANELS_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 7200 wine panelsdiff.exe ${N:-200} ${START:-}]==]
  )

ofs_target(panelsunit-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DPANELS_UNIT -fno-ipa-icf -Isrc/panels -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x10000000 -o bin/difftest/PANELS_unit.dll src/panels/panels.c bin/panels.res src/panels/panels_unit.def -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc]==]
    [==[cp orig/Modules/PANELS.DLL bin/difftest/PANELS_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine panelsdiff.exe unit ${N:-300} ${START:-}]==]
  )

ofs_target(panels-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/PANELS.DLL bin/difftest/PANELS_retail.dll]==]
    [==[cp bin/PANELS.DLL bin/difftest/PANELS_ours.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DPANELS_UNIT -fno-ipa-icf -Isrc/panels -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x10000000 -o bin/difftest/PANELS_unit.dll src/panels/panels.c bin/panels.res src/panels/panels_unit.def -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc]==]
    [==[cp orig/Modules/PANELS.DLL bin/difftest/PANELS_retail.dll]==]
    [==[python3 -B tools/panels_breaks.py ${N:-150} ${COUNT:-200} ${SEED:-1}]==]
  )

ofs_target(gauges
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(gauges-install
  DEPENDS openfs98
  COMMANDS
    [==[python3 -B tools/stage_gauges.py "${FS98_DIR:-$HOME/.wine/drive_c/FS98}"]==]
  )

ofs_target(gauges-revert
  DEPENDS openfs98
  COMMANDS
    [==[python3 -B tools/stage_gauges.py "${FS98_DIR:-$HOME/.wine/drive_c/FS98}" --retail]==]
  )

ofs_target(gauges-check
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_gaugesdiff.sh --all-batch --count ${N:-20000}]==]
  )

ofs_target(gauges-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[python3 -B tools/gauge_breaks.py]==]
  )

ofs_target(gauges-panels
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp bin/PANELS.DLL bin/gauges-all/PANELS_retail.dll]==]
    [==[cp bin/PANELS.DLL bin/gauges-all/PANELS_ours.dll]==]
    [==[bash tools/run_gaugesdiff.sh --all-panels ${N:-1000} ${START:-}]==]
  )

ofs_target(all-gauges-hangs
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_gaugesdiff.sh --all-batch --hangs]==]
  )

ofs_target(gaugesdiff-run
  DEPENDS difftests test-gauges
  COMMANDS
    [==[bash tools/run_gaugesdiff.sh ${N:-20000}]==]
    [==[GAUGES_RELOCATE=1 bash tools/run_gaugesdiff.sh ${N:-20000}]==]
  )

ofs_target(suctiondiff-run
  DEPENDS difftests test-gauges
  COMMANDS
    [==[bash tools/run_gaugesdiff.sh --suction ${N:-20000}]==]
    [==[GAUGES_RELOCATE=1 bash tools/run_gaugesdiff.sh --suction ${N:-20000}]==]
  )

ofs_target(gauges-panels-run
  DEPENDS openfs98 difftests test-gauges
  COMMANDS
    [==[cp bin/PANELS.DLL bin/gauges-diff/PANELS_retail.dll]==]
    [==[cp bin/PANELS.DLL bin/gauges-diff/PANELS_ours.dll]==]
    [==[bash tools/run_gaugesdiff.sh --panels ${N:-1000} ${START:-}]==]
  )

ofs_target(gauges-game-run
  DEPENDS difftests test-gauges
  COMMANDS
    [==[HG2D_FRAMES_OUT="$PWD/bin/gauges-frames" HG2D_FRAMES_DISPLAY=:176 HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL HG2D_FRAMES_GAUGE=bin/gauges-diff/737-400.WARNING_ENUNCIATORS.GAU HG2D_FRAMES_GAUGE_RETAIL=orig/Gauges/737WARNG.GAU bash tools/hg2d_frames.sh 'Situatio\HONGKONG.STN' 900]==]
  )

ofs_target(suction-game-run
  DEPENDS difftests test-gauges
  COMMANDS
    [==[HG2D_FRAMES_OUT="$PWD/bin/suction-frames" HG2D_FRAMES_DISPLAY=:177 HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL HG2D_FRAMES_GAUGE=bin/gauges-diff/CESSNA_182.SUCTION.GAU HG2D_FRAMES_GAUGE_RETAIL=orig/Gauges/CESSSUC.GAU bash tools/hg2d_frames.sh 'Situatio\STGEORGE.STN' 900]==]
  )

ofs_target(g3ddiff-run
  DEPENDS g3ddiff
  COMMANDS
    [==[cd bin/difftest && WINEDEBUG=-all timeout 7200 wine g3ddiff.exe ${N:-20000}]==]
  )

ofs_target(g3d-breaks
  DEPENDS g3ddiff
  COMMANDS
    [==[python3 -B tools/g3d_breaks.py ${N:-2000}]==]
  )

ofs_target(g3d-portable-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[python3 -B tools/g3d_breaks.py --portable ${N:-2000}]==]
  )

ofs_target(g3dmeshdiff-run
  DEPENDS difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_mesh_retail.dll]==]
    [==[cp "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/MSVCRT.DLL" bin/difftest/MSVCRT_cd.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine g3dmeshdiff.exe ${N:-100000}]==]
  )

ofs_target(g3ddiff-meshc-run
  DEPENDS g3ddiff
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DFS98_PORTABLE_FP -DG3D_MESH_PRECISION_DEFAULT=64 -c -o bin/difftest/g3d_mesh_portable.o src/g3d/g3d_mesh.c]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/g3d -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x206e0000 -o bin/difftest/G3D_ours.dll src/g3d/g3d.c bin/difftest/g3d_mesh_portable.o bin/g3d.res src/g3d/g3d.def -lkernel32 -luser32 -lgcc]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 7200 wine g3ddiff.exe ${N:-20000}]==]
  )

ofs_target(multipdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine multipdiff.exe ${N:-20000}]==]
  )

ofs_target(multip-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
    [==[python3 -B tools/multip_breaks.py]==]
  )

ofs_target(multip-smoke
  DEPENDS openfs98
  COMMANDS
    [==[tools/multip_smoke.sh]==]
  )

ofs_target(multip-acceptance
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine multipdiff.exe 20000]==]
    [==[python3 -B tools/multip_breaks.py]==]
    [==[git diff --check]==]
    [==[tools/multip_smoke.sh]==]
    [==[echo "MULTIP acceptance: PASS"]==]
  )

ofs_target(demodiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/DEMO.DLL bin/difftest/DEMO_retail.dll]==]
    [==[cp bin/DEMO.DLL bin/difftest/DEMO_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[cd bin/difftest && test -n "${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}" || { echo "demodiff-run needs the CD tree in cd/"@SEMI@ exit 2@SEMI@ }]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine demodiff.exe ${N:-20000} ${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}]==]
  )

ofs_target(fesldiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/fe -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20c00000 -o bin/difftest/FE_sl_ours.dll src/fe/fe.c bin/fe.res src/fe/fe.def -lkernel32]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cd bin/difftest && test -n "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}" || { echo "fesldiff-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"@SEMI@ exit 2@SEMI@ }]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine fesldiff.exe ${N:-5000} "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}" ${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}]==]
  )

ofs_target(facildiff-fe-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/facil.dll bin/difftest/FACIL_retail.dll]==]
    [==[cp bin/FACIL.DLL bin/difftest/FACIL_ours.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_facil_retail.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_facil_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cd bin/difftest && test -n "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}" || { echo "facildiff-fe-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"@SEMI@ exit 2@SEMI@ }]==]
    [==[cp bin/FE.DLL bin/difftest/FE_facil_ours.dll]==]
    [==[(cd bin/difftest && WINEDEBUG=-all timeout 3600 wine facildiff.exe FACIL_retail.dll FACIL_ours.dll "${FE_FIXTURE-$(for d in orig game@SEMI@ do test -f ../../$d/Scenery/world.vis && test -f ../../$d/Texture/fs5.pal && { echo ../../$d@SEMI@ break@SEMI@ }@SEMI@ done)}" ${Q:-2000} ${D:-2000})@SEMI@ status=$?@SEMI@ cp orig/Modules/FE.DLL bin/difftest/FE_facil_ours.dll@SEMI@ exit $status]==]
  )

ofs_target(weathermdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WEATHERM.DLL bin/difftest/WEATHERM_retail.dll]==]
    [==[cp bin/WEATHERM.DLL bin/difftest/WEATHERM_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 900 wine weathermdiff.exe ${N:-}]==]
  )

ofs_target(chaindiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp bin/CHAIN.DLL bin/difftest/CHAIN_ours.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 600 wine chaindiff.exe ${N:-}]==]
  )

ofs_target(ofstest-run
  DEPENDS openfs98
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/util -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20950000 -o bin/OFSTEST.DLL src/test/ofstest.c src/util/util_math.c src/util/util_files.c src/util/util_exchange.c src/util/util_alert.c src/test/ofstest.def -lkernel32 -lgcc]==]
    [==[cp bin/OFSTEST.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/Modules/OFSTEST.DLL"]==]
    [==[rm -f "${WINEPREFIX:-$HOME/.wine}/drive_c/openfs98-test.log"]==]
    [==[tools/run_swapped.sh "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/Modules/UTIL.DLL" orig/Modules/UTIL.DLL sh -c 'cd ${FS98_DIR:-$HOME/.wine/drive_c/FS98} && WINEDEBUG=-all timeout 45 wine FLTSIM98.EXE >/dev/null 2>&1 || true']==]
    [==[echo "--- ${WINEPREFIX:-$HOME/.wine}/drive_c/openfs98-test.log ---"]==]
    [==[cat "${WINEPREFIX:-$HOME/.wine}/drive_c/openfs98-test.log" 2>/dev/null || echo "(no log -- init was not reached)"]==]
  )

ofs_target(world
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL" || { echo "no English/World.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL.orig"]==]
    [==[cp bin/WORLD.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL"]==]
  )

ofs_target(world-revert
  COMMANDS
    [==[cp "orig/English/World.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/World.DLL"]==]
  )

ofs_target(world-game-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[FS98_DIR="${FS98_DIR:-$HOME/.wine/drive_c/FS98}" bash tools/run_worlddiff.sh --game-ui]==]
  )

ofs_target(sehfix-install
  DEPENDS openfs98
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
    [==[cp bin/SEHFIX.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/Modules/SEHFIX.DLL"]==]
    [==[echo "SEHFIX.DLL installed@SEMI@ its log is $HOME/.wine/drive_c/openfs98-sehfix.log"]==]
  )

ofs_target(util-trace
  DEPENDS openfs98
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DFS98_UTIL_MODULE -DFS98_UTIL_TRACE -Isrc/util -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x208e0000 -o bin/UTIL_TRACE.DLL src/util/util.c src/util/util_math.c src/util/util_files.c src/util/util_exchange.c src/util/util_alert.c bin/util.res src/util/util.def -lkernel32 -lgcc]==]
    [==[rm -f "${WINEPREFIX:-$HOME/.wine}/drive_c/openfs98-util-trace.log"]==]
    [==[echo "tracing UTIL for up to ${UTIL_TRACE_SECS-90}s -- use the sim, or just wait"]==]
    [==[tools/run_swapped.sh "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/Modules/UTIL.DLL" bin/UTIL_TRACE.DLL sh -c 'cd ${FS98_DIR:-$HOME/.wine/drive_c/FS98} && WINEDEBUG=-all timeout ${UTIL_TRACE_SECS-90} wine FLTSIM98.EXE >/dev/null 2>&1 || true']==]
    [==[echo "--- services called (slot, count, hex) ---"]==]
    [==[grep -v ' 00000000' "${WINEPREFIX:-$HOME/.wine}/drive_c/openfs98-util-trace.log" 2>/dev/null || echo "(no log)"]==]
    [==[echo "--- never called ---"]==]
    [==[grep ' 00000000' "${WINEPREFIX:-$HOME/.wine}/drive_c/openfs98-util-trace.log" 2>/dev/null | cut -d' ' -f1 | tr '\n' ' '@SEMI@ echo]==]
  )

ofs_target(graph
  COMMANDS
    [==[python3 -B tools/modgraph.py orig/Modules]==]
  )

ofs_target(hostmap
  COMMANDS
    [==[python3 -B tools/hostmap.py orig/FLTSIM98.EXE]==]
  )

ofs_target(compare
  DEPENDS openfs98
  COMMANDS
    [==[for m in $(tools/modules.sh modules-of) $(tools/modules.sh english-modules-of) $(tools/modules.sh shared-modules-of)@SEMI@ do echo "== $m"@SEMI@ python3 -B tools/pesurvey.py $(tools/modules.sh orig-module-of $m) bin/$m.DLL 2>/dev/null@SEMI@ done]==]
  )

ofs_target(advdrvdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ADVDRV.DLL bin/difftest/ADVDRV_retail.dll]==]
    [==[cp bin/ADVDRV.DLL bin/difftest/ADVDRV_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[cd bin/difftest && test -n "${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}" || { echo "advdrvdiff-run needs the CD tree in cd/"@SEMI@ exit 2@SEMI@ }]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 7200 wine advdrvdiff.exe ${N:-20000} ${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}]==]
  )

ofs_target(maindiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/MAIN.DLL bin/difftest/MAIN_retail.dll]==]
    [==[cp bin/MAIN.DLL bin/difftest/MAIN_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 3600 wine maindiff.exe ${N:-20000} ${SUITE:-records}]==]
  )

ofs_target(main-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/MAIN.DLL bin/difftest/MAIN_retail.dll]==]
    [==[cp bin/MAIN.DLL bin/difftest/MAIN_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[python3 -B tools/main_breaks.py ${N:-2000} ${SUITE:-records}]==]
  )

ofs_target(main-smoke
  DEPENDS openfs98
  COMMANDS
    [==[tools/main_smoke.sh]==]
  )

ofs_target(flights-play
  DEPENDS openfs98 difftests
  COMMANDS
    [==[tools/flights_play.sh]==]
  )

ofs_target(simsuidiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_simsuidiff.sh ${N:-2000} ${SEED:-0x51c5e1}]==]
  )

ofs_target(simsui-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_simsuidiff.sh --breaks ${N:-2000}]==]
  )

ofs_target(sim1r-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[python3 -B tools/sim1r_breaks.py]==]
  )

ofs_target(simsui
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(main-dialogs
  DEPENDS openfs98 difftests
  COMMANDS
    [==[tools/main_dialogs.sh]==]
  )

ofs_target(hostdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_ddraw.c -o bin/host-rt-standalone/host_rt_ddraw.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_config.c -o bin/host-rt-standalone/host_rt_config.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_modules.c -o bin/host-rt-standalone/host_rt_modules.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_display.c -o bin/host-rt-standalone/host_rt_display.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -Iinclude -c src/host/runtime/host_rt_common.c -o bin/host-rt-standalone/host_rt_common.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x41000000 -o bin/HOST.DLL src/host/host.c bin/host-rt-standalone/host_rt_ddraw.o bin/host-rt-standalone/host_rt_config.o bin/host-rt-standalone/host_rt_modules.o bin/host-rt-standalone/host_rt_display.o bin/host-rt-standalone/host_rt_common.o src/host/host.def -lmsvcrt -lgdi32 -luser32 -lwinmm -ladvapi32 -lmsacm32 -lddraw -lkernel32]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98.EXE]==]
    [==[cp bin/HOST.DLL bin/difftest/HOST_ours.dll]==]
    [==[cp orig/ENGLISH.FLL orig/SIMSUI.DLL orig/DIALOG.DLL bin/difftest/]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 600 wine hostdiff.exe ${N:-1}]==]
  )

ofs_target(host-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_ddraw.c -o bin/host-rt-standalone/host_rt_ddraw.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_config.c -o bin/host-rt-standalone/host_rt_config.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_modules.c -o bin/host-rt-standalone/host_rt_modules.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_display.c -o bin/host-rt-standalone/host_rt_display.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -Iinclude -c src/host/runtime/host_rt_common.c -o bin/host-rt-standalone/host_rt_common.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x41000000 -o bin/HOST.DLL src/host/host.c bin/host-rt-standalone/host_rt_ddraw.o bin/host-rt-standalone/host_rt_config.o bin/host-rt-standalone/host_rt_modules.o bin/host-rt-standalone/host_rt_display.o bin/host-rt-standalone/host_rt_common.o src/host/host.def -lmsvcrt -lgdi32 -luser32 -lwinmm -ladvapi32 -lmsacm32 -lddraw -lkernel32]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98.EXE]==]
    [==[cp bin/HOST.DLL bin/difftest/HOST_ours.dll]==]
    [==[cp orig/ENGLISH.FLL orig/SIMSUI.DLL orig/DIALOG.DLL bin/difftest/]==]
    [==[python3 -B tools/host_breaks.py ${N:-2000}]==]
  )

ofs_target(mdl-crash-diff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[WINEPREFIX="$PWD/bin/mdl-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 300s wine bin/mdl-crash/mdlcrashdiff.exe ${N:-2000} retail-first]==]
    [==[WINEPREFIX="$PWD/bin/mdl-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 300s wine bin/mdl-crash/mdlcrashdiff.exe ${N:-2000} relocate]==]
  )

ofs_target(hg2ddiff-run
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--image-base=0x30000000 -Wl,--disable-dynamicbase -o bin/hg2d-diff/HG2D_ours.dll src/hg2d/hg2d.c src/hg2d/hg2d.def -lgcc -lkernel32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -o bin/hg2d-diff/hg2ddiff.exe tools/difftest/hg2ddiff.c]==]
    [==[cp orig/Modules/HG2D.DLL bin/hg2d-diff/HG2D_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/hg2d-diff/UTIL_retail.dll]==]
    [==[cd bin/hg2d-diff && WINEDEBUG=-all timeout ${HG2D_TIMEOUT-7200} wine hg2ddiff.exe ${N:-20000} ${SUITE:-all}]==]
  )

ofs_target(bin
  COMMANDS
  )

ofs_target(smoke
  DEPENDS openfs98
  COMMANDS
    [==[tools/smoketest.sh]==]
  )

# The rest of the Makefile's and tools/*.mk's targets (Story 1.2, piece 3), converted the same way. The
# module-list queries (modules-of, installed-path-of, ...) are tools/modules.sh instead.

ofs_target(acontaindiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ACONTAIN.DLL bin/difftest/ACONTAIN_retail.dll]==]
    [==[cp bin/ACONTAIN.DLL bin/difftest/ACONTAIN_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/ENGLISH.FLL bin/difftest/ENGLISH.FLL]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
  )

ofs_target(advdrv-cover
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ADVDRV.DLL bin/difftest/ADVDRV_retail.dll]==]
    [==[cp bin/ADVDRV.DLL bin/difftest/ADVDRV_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DADVDRV_COVER -fno-ipa-icf -Isrc/advdrv -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20610000 -o bin/difftest/ADVDRV_ours.dll src/advdrv/advdrv.c bin/advdrv.res src/advdrv/advdrv_cover.def -lkernel32 -lgcc]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine advdrvdiff.exe ${N:-1000} ${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}]==]
    [==[cp bin/ADVDRV.DLL bin/difftest/ADVDRV_ours.dll]==]
  )

# the Makefile's pattern rule advdrv-break-N: ADVDRV built with one deliberate break (#if BREAK==N in
# src/advdrv/advdrv.c), run over N scenarios (default 300); BREAK=N selects it
ofs_target(advdrv-break
  DEPENDS advdrvdiff
  COMMANDS
    [==[test -n "${BREAK:-}" || { echo "advdrv-break needs BREAK=N"@SEMI@ exit 2@SEMI@ }]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DBREAK=$BREAK -fno-ipa-icf -Isrc/advdrv -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20610000 -o bin/difftest/ADVDRV_ours.dll src/advdrv/advdrv.c bin/advdrv.res src/advdrv/advdrv.def -lkernel32 -lgcc]==]
    [==[cd bin/difftest && WINEDEBUG=-all wine advdrvdiff.exe ${N:-300} ${FE_CD-$(test -d ../../cd/Boosters && echo ../../cd)}@SEMI@ test $? -ne 0 && echo "BREAK $BREAK caught" || echo "BREAK $BREAK MISSED"]==]
    [==[cp bin/ADVDRV.DLL bin/difftest/ADVDRV_ours.dll]==]
  )

# the Makefile's pattern rule disasm-NAME: side-by-side disassembly of a module's .text; MOD=NAME selects it
ofs_target(disasm
  DEPENDS openfs98
  COMMANDS
    [==[test -n "${MOD:-}" || { echo "disasm needs MOD=NAME"@SEMI@ exit 2@SEMI@ }]==]
    [==[echo "=== retail $MOD ==="@SEMI@ i686-w64-mingw32-objdump -d -M intel orig/Modules/$MOD.DLL | sed -n '/^[0-9a-f]* <.text>/,$p']==]
    [==[echo "=== openfs98 $MOD ==="@SEMI@ i686-w64-mingw32-objdump -d -M intel bin/$MOD.DLL | sed -n '/^[0-9a-f]* <.text>/,$p']==]
  )

ofs_target(advdrvdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ADVDRV.DLL bin/difftest/ADVDRV_retail.dll]==]
    [==[cp bin/ADVDRV.DLL bin/difftest/ADVDRV_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
  )

ofs_target(alerts
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(alerts-build-check
  COMMANDS
  )

ofs_target(alerts-events-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_alertsdiff.sh --breaks ${N:-20000}]==]
  )

ofs_target(alerts-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL" || { echo "no English/Alerts.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL.orig"]==]
    [==[cp bin/ALERTS.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL"]==]
  )

ofs_target(alerts-object-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_alertsdiff.sh --object-breaks ${N:-20000}]==]
  )

ofs_target(alerts-revert
  COMMANDS
    [==[cp "orig/English/Alerts.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Alerts.DLL"]==]
  )

ofs_target(alerts-smoke
  DEPENDS openfs98
  COMMANDS
    [==[bash tools/run_alertsdiff.sh --smoke]==]
  )

ofs_target(alerts-ui-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_alertsdiff.sh --ui-breaks]==]
  )

ofs_target(alertsevents
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(alertseventsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_alertsdiff.sh ${N:-20000}]==]
  )

ofs_target(alertsobjects
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(alertsobjectsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_alertsdiff.sh --objects ${N:-20000}]==]
  )

ofs_target(alertsuidiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_alertsdiff.sh --ui]==]
  )

# the committed gauge tables (src/gauges/images/, gauges_manifest.json) against retail's full analysis (needs objdump and cc)
ofs_target(gauges-tables-check
  COMMANDS
    [==[python3 -B tools/gen_gauges.py --check]==]
  )

# the committed MDL images (src/mdl/images/, mdl_manifest.json) against retail (needs cc)
ofs_target(mdl-images-check
  COMMANDS
    [==[python3 -B tools/gen_mdl_images.py --check]==]
  )

ofs_target(all-gauges
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(all-gauges-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[python3 -B tools/gauge_breaks.py]==]
  )

ofs_target(all-gauges-check
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_gaugesdiff.sh --all-batch --count ${N:-20000}]==]
  )

ofs_target(all-gauges-panels
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp bin/PANELS.DLL bin/gauges-all/PANELS_retail.dll]==]
    [==[cp bin/PANELS.DLL bin/gauges-all/PANELS_ours.dll]==]
    [==[bash tools/run_gaugesdiff.sh --all-panels ${N:-} ${START:-}]==]
  )

ofs_target(analysis-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/analysis -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20630000 -o bin/difftest/ANALYSIS_ours.dll ${ANALYSIS_SRC:-} bin/analysis.res src/analysis/analysis.def -lkernel32 -lgcc]==]
  )

ofs_target(analysisdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ANALYSIS.DLL bin/difftest/ANALYSIS_retail.dll]==]
    [==[cp bin/ANALYSIS.DLL bin/difftest/ANALYSIS_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
  )

ofs_target(atcdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/ATC.DLL bin/difftest/ATC_retail.dll]==]
    [==[cp bin/ATC.DLL bin/difftest/ATC_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
  )

ofs_target(boosterdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/BOOSTER.DLL bin/difftest/BOOSTER_retail.dll]==]
    [==[cp bin/BOOSTER.DLL bin/difftest/BOOSTER_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
  )

ofs_target(chaindiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp bin/CHAIN.DLL bin/difftest/CHAIN_ours.dll]==]
  )

ofs_target(ofs_clean
  COMMANDS
    [==[rm -rf bin]==]
  )

ofs_target(controls-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/controls -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20670000 -o bin/difftest/CONTROLS_ours.dll bin/difftest/controls_break.c bin/difftest/controls_input_break.c bin/controls.res src/controls/controls.def -lkernel32 -luser32 -lgcc]==]
  )

ofs_target(controlsdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/CONTROLS.DLL bin/difftest/CONTROLS_retail.dll]==]
    [==[cp bin/CONTROLS.DLL bin/difftest/CONTROLS_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
    [==[cp orig/ENGLISH.FLL bin/difftest/ENGLISH.FLL]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
  )

ofs_target(ddraw-default
  COMMANDS
    [==[wine reg delete 'HKCU\Software\Wine\Direct3D' /v renderer /f]==]
    [==[echo "wine ddraw back to its default renderer."]==]
  )

ofs_target(ddraw-gdi
  COMMANDS
    [==[wine reg add 'HKCU\Software\Wine\Direct3D' /v renderer /d gdi /f]==]
    [==[echo "wine ddraw now uses GDI. 'tools/ofs ddraw-default' puts it back."]==]
  )

ofs_target(demodiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/DEMO.DLL bin/difftest/DEMO_retail.dll]==]
    [==[cp bin/DEMO.DLL bin/difftest/DEMO_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
  )

ofs_target(difftest
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/SLEW.DLL bin/difftest/SLEW_retail.dll]==]
    [==[cp bin/SLEW.DLL bin/difftest/SLEW_ours.dll]==]
  )

ofs_target(dynamicdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/DYNAMIC.DLL bin/difftest/DYNAMIC_retail.dll]==]
    [==[cp bin/DYNAMIC.DLL bin/difftest/DYNAMIC_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[mkdir -p bin/difftest/cdscenery]==]
    [==[cp orig/Scenery/*5.bgl bin/difftest/cdscenery/]==]
  )

ofs_target(facildiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/facil.dll bin/difftest/FACIL_retail.dll]==]
    [==[cp bin/FACIL.DLL bin/difftest/FACIL_ours.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_facil_retail.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_facil_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
  )

ofs_target(fediff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_retail.dll]==]
    [==[cp bin/FE.DLL bin/difftest/FE_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
  )

ofs_target(fegriddiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_retail.dll]==]
    [==[cp bin/FE.DLL bin/difftest/FE_ours.dll]==]
  )

ofs_target(fesldiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/fe -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20c00000 -o bin/difftest/FE_sl_ours.dll src/fe/fe.c bin/fe.res src/fe/fe.def -lkernel32]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
  )

ofs_target(flights
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(flights-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_flightsdiff.sh --breaks ${N:-20000}]==]
  )

ofs_target(flights-build-check
  COMMANDS
  )

ofs_target(flights-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL" || { echo "no English/Flights.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL.orig"]==]
    [==[cp bin/FLIGHTS.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL"]==]
  )

ofs_target(flights-revert
  COMMANDS
    [==[cp "orig/English/Flights.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Flights.DLL"]==]
  )

ofs_target(flights-runtime-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(flights-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(flightsevents
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(flightseventsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_flightsdiff.sh ${N:-20000} ${SEED:-0x5f1a6c2d} ${MODE:-normal}]==]
  )

ofs_target(g3d-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/g3d -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x206e0000 -o bin/difftest/G3D_ours.dll ${G3D_SRC:-src/g3d/g3d.c} src/g3d/g3d_mesh.c bin/g3d.res src/g3d/g3d.def -lkernel32 -luser32 -lgcc]==]
  )

ofs_target(g3ddiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll]==]
    [==[cp bin/G3D.DLL bin/difftest/G3D_ours.dll]==]
  )

ofs_target(g3dmeshdiff
  DEPENDS difftests
  COMMANDS
  )

ofs_target(ofs_help
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(help-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_helpdiff.sh --breaks ${N:-20000}]==]
  )

ofs_target(help-build-check
  COMMANDS
  )

ofs_target(help-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL" || { echo "no English/Help.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL.orig"]==]
    [==[cp bin/HELP.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL"]==]
  )

ofs_target(help-revert
  COMMANDS
    [==[cp "orig/English/Help.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Help.DLL"]==]
  )

ofs_target(help-runtime-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(help-smoke
  DEPENDS openfs98
  COMMANDS
    [==[bash tools/run_helpdiff.sh --smoke]==]
  )

ofs_target(help-ui-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_helpdiff.sh --ui-breaks]==]
  )

ofs_target(helpevents
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(helpeventsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_helpdiff.sh ${N:-20000} ${SEED:-0x19a847e3} ${MODE:-normal}]==]
  )

ofs_target(helpuidiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_helpdiff.sh --ui]==]
  )

ofs_target(hg2d
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--image-base=0x30000000 -Wl,--disable-dynamicbase -o bin/hg2d-diff/HG2D_ours.dll src/hg2d/hg2d.c src/hg2d/hg2d.def -lgcc -lkernel32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -o bin/hg2d-diff/hg2ddiff.exe tools/difftest/hg2ddiff.c]==]
  )

ofs_target(hg2dmodeldiff-run
  DEPENDS difftests
  COMMANDS
    [==[cp orig/Modules/HG2D.DLL bin/difftest/HG2D_retail.dll]==]
    [==[cd bin/difftest && WINEDEBUG=-all timeout 1800 wine hg2dmodeldiff.exe ${N:-20000}]==]
  )

ofs_target(host-break-build
  COMMANDS
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_ddraw.c -o bin/host-rt-standalone/host_rt_ddraw.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_config.c -o bin/host-rt-standalone/host_rt_config.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_modules.c -o bin/host-rt-standalone/host_rt_modules.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_display.c -o bin/host-rt-standalone/host_rt_display.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -Iinclude -c src/host/runtime/host_rt_common.c -o bin/host-rt-standalone/host_rt_common.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/host -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x41000000 -o bin/difftest/HOST_ours.dll ${HOST_BREAK:-} bin/host-rt-standalone/host_rt_ddraw.o bin/host-rt-standalone/host_rt_config.o bin/host-rt-standalone/host_rt_modules.o bin/host-rt-standalone/host_rt_display.o bin/host-rt-standalone/host_rt_common.o src/host/host.def -lmsvcrt -lgdi32 -luser32 -lwinmm -ladvapi32 -lmsacm32 -lddraw -lkernel32]==]
  )

ofs_target(host-install
  DEPENDS openfs98
  COMMANDS
    [==[cp bin/OPENFS98.EXE "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/OPENFS98.EXE"]==]
  )

ofs_target(host-run
  DEPENDS openfs98
  COMMANDS
    [==[cp bin/OPENFS98.EXE "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/OPENFS98.EXE"]==]
    [==[cd "${FS98_DIR:-$HOME/.wine/drive_c/FS98}" && WINEDEBUG=-all wine OPENFS98.EXE]==]
  )

ofs_target(host-runtime-boot
  DEPENDS openfs98
  COMMANDS
    [==[FS98_DIR="${FS98_DIR:-$HOME/.wine/drive_c/FS98}" tools/host_runtime_boot.sh]==]
  )

ofs_target(host-runtime-d3d
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--image-base=0x20970000 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -o bin/hg2d-diff/FRAMELOCK.DLL tools/drive/framelock.c tools/drive/framelock.def -lkernel32 -luser32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
    [==[python3 -B tools/fly_scripts.py bin/host-runtime/scripts]==]
    [==[python3 -c 'from pathlib import Path@SEMI@ p=Path("bin/host-runtime/scripts")@SEMI@ (p/"direct3d.txt").write_text("20 111 1501 0\n"+(p/"controls.txt").read_text())']==]
    [==[FS98_DIR="${FS98_DIR:-$HOME/.wine/drive_c/FS98}" HG2D_FRAMES_HOST="$PWD/bin/OPENFS98.EXE" HG2D_FRAMES_ONLY=HOST HG2D_FRAMES_SIM1="$PWD/orig/Sim/SIM1.sim" HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL HG2D_FRAMES_NOSOUND=1 HG2D_FRAMES_OUT=bin/host-runtime/direct3d HG2D_FRAMES_DISPLAY=:70 HG2D_FRAMES_TIMEOUT=300 HG2D_FRAMES_DEVICE='DirectDraw HAL (Direct3D)' HG2D_FRAMES_MODE=640x480x16 HG2D_FRAMES_SCREEN=640x480 HG2D_FRAMES_HOLD=150 tools/hg2d_frames.sh 'Situatio\PARIS.STN' 900 bin/host-runtime/scripts/direct3d.txt]==]
  )

ofs_target(host-runtime-flights
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--image-base=0x20970000 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -o bin/hg2d-diff/FRAMELOCK.DLL tools/drive/framelock.c tools/drive/framelock.def -lkernel32 -luser32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
    [==[python3 -B tools/fly_scripts.py bin/host-runtime/scripts]==]
    [==[FS98_DIR="${FS98_DIR:-$HOME/.wine/drive_c/FS98}" HG2D_FRAMES_HOST="$PWD/bin/OPENFS98.EXE" HG2D_FRAMES_ONLY=HOST HG2D_FRAMES_SIM1="$PWD/orig/Sim/SIM1.sim" HG2D_FRAMES_NOSOUND=1 FRAMES_MANY_OUT=bin/host-runtime/flights FRAMES_MANY_PASSES=900 FRAMES_MANY_JOBS=${HOST_RUNTIME_JOBS:-4} tools/frames_many.sh bin/host-runtime/scripts/controls.txt]==]
    [==[! grep -E ': (DIFFERS|NONDETERMINISTIC|NO-RUN)|FEW FRAMES' bin/host-runtime/flights/summary.txt]==]
  )

ofs_target(host-runtime-frame-driver
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--image-base=0x20970000 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -o bin/hg2d-diff/FRAMELOCK.DLL tools/drive/framelock.c tools/drive/framelock.def -lkernel32 -luser32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
  )

ofs_target(host-runtime-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
    [==[FS98_DIR="${FS98_DIR:-$HOME/.wine/drive_c/FS98}" tools/host_runtime_ui.sh]==]
  )

ofs_target(hostdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_ddraw.c -o bin/host-rt-standalone/host_rt_ddraw.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_config.c -o bin/host-rt-standalone/host_rt_config.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_modules.c -o bin/host-rt-standalone/host_rt_modules.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c src/host/runtime/host_rt_display.c -o bin/host-rt-standalone/host_rt_display.o]==]
    [==[mkdir -p bin/host-rt-standalone]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -Iinclude -c src/host/runtime/host_rt_common.c -o bin/host-rt-standalone/host_rt_common.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x41000000 -o bin/HOST.DLL src/host/host.c bin/host-rt-standalone/host_rt_ddraw.o bin/host-rt-standalone/host_rt_config.o bin/host-rt-standalone/host_rt_modules.o bin/host-rt-standalone/host_rt_display.o bin/host-rt-standalone/host_rt_common.o src/host/host.def -lmsvcrt -lgdi32 -luser32 -lwinmm -ladvapi32 -lmsacm32 -lddraw -lkernel32]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98.EXE]==]
    [==[cp bin/HOST.DLL bin/difftest/HOST_ours.dll]==]
    [==[cp orig/ENGLISH.FLL orig/SIMSUI.DLL orig/DIALOG.DLL bin/difftest/]==]
  )

ofs_target(hostrtdiff
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(dividediff
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(kill
  COMMANDS
    [==[wineserver -k 2>/dev/null@SEMI@ true]==]
    [==[echo "wine processes killed"]==]
  )

ofs_target(landmediff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/LANDME.DLL bin/difftest/LANDME_retail.dll]==]
    [==[cp bin/LANDME.DLL bin/difftest/LANDME_ours.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_landme_retail.dll]==]
    [==[cp orig/Modules/FE.DLL bin/difftest/FE_landme_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
  )

ofs_target(main-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -I${MAIN_BREAK_DIR:-} -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x207e0000 -o ${DIFFDIR:-bin/difftest}/MAIN_ours.dll ${MAIN_BREAK_DIR:-}/main.c bin/main.res src/main/main.def -lmsvcrt -lkernel32 -luser32 -lgdi32 -lgcc]==]
  )

ofs_target(maindiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/MAIN.DLL bin/difftest/MAIN_retail.dll]==]
    [==[cp bin/MAIN.DLL bin/difftest/MAIN_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
  )

ofs_target(mdl-crash-render-break
  DEPENDS openfs98 difftests
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_crash_render_break.py]==]
  )

ofs_target(mdl-normal-diff
  DEPENDS difftests
  COMMANDS
  )

ofs_target(mdl-normal-modules-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[WINEPREFIX="$PWD/bin/mdl-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine bin/mdl-diff/mdldlldiff.exe ${N:-70000} retail-first]==]
    [==[WINEPREFIX="$PWD/bin/mdl-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine bin/mdl-diff/mdldlldiff.exe ${N:-70000} ours-first]==]
    [==[WINEPREFIX="$PWD/bin/mdl-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine bin/mdl-diff/mdldlldiff.exe ${N:-70000} relocate]==]
  )

ofs_target(mdl-normal-render-break
  DEPENDS openfs98 difftests
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_render_break.py]==]
  )

ofs_target(mode16
  COMMANDS
    [==[sed -i 's/^Mode=.*/Mode=640x480x16/' "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/FLTSIM98.CFG"]==]
    [==[grep '^Mode=' "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/FLTSIM98.CFG"]==]
  )

ofs_target(mode8
  COMMANDS
    [==[sed -i 's/^Mode=.*/Mode=640x480x8/' "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/FLTSIM98.CFG"]==]
    [==[grep '^Mode=' "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/FLTSIM98.CFG"]==]
  )

ofs_target(mpchat-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/mpchat -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20840000 -o bin/difftest/MPCHAT_ours.dll ${MPCHAT_SRC:-} bin/mpchat.res src/mpchat/mpchat.def -lkernel32 -luser32 -lgdi32 -lgcc]==]
  )

ofs_target(mpchat-modeldiff-run
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/mpchat-ui]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/mpchat-ui/mpchatdiff.exe tools/difftest/mpchatdiff.c]==]
    [==[i686-w64-mingw32-windres src/mpchat/mpchat.rc -O coff -o bin/mpchat-ui/mpchat.res]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Werror -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20840000 -o bin/mpchat-ui/MPCHAT_ours.dll src/mpchat/mpchat.c bin/mpchat-ui/mpchat.res src/mpchat/mpchat.def -lkernel32 -luser32 -lgdi32 -lgcc]==]
    [==[bash tools/run_mpchatuidiff.sh --model]==]
  )

ofs_target(mpchat-ui
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/mpchat-ui]==]
    [==[i686-w64-mingw32-windres src/mpchat/mpchat.rc -O coff -o bin/mpchat-ui/mpchat.res]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Werror -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20840000 -o bin/mpchat-ui/MPCHAT_ours.dll src/mpchat/mpchat.c bin/mpchat-ui/mpchat.res src/mpchat/mpchat.def -lkernel32 -luser32 -lgdi32 -lgcc]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/mpchat-ui/mpchatuidiff.exe tools/difftest/mpchatuidiff.c -lgdi32]==]
  )

ofs_target(mpchat-ui-breaks
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/mpchat-ui]==]
    [==[i686-w64-mingw32-windres src/mpchat/mpchat.rc -O coff -o bin/mpchat-ui/mpchat.res]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Werror -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20840000 -o bin/mpchat-ui/MPCHAT_ours.dll src/mpchat/mpchat.c bin/mpchat-ui/mpchat.res src/mpchat/mpchat.def -lkernel32 -luser32 -lgdi32 -lgcc]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/mpchat-ui/mpchatuidiff.exe tools/difftest/mpchatuidiff.c -lgdi32]==]
    [==[bash tools/run_mpchatuidiff.sh --breaks]==]
  )

ofs_target(mpchatdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/mpchat.DLL bin/difftest/MPCHAT_retail.dll]==]
    [==[cp bin/MPCHAT.DLL bin/difftest/MPCHAT_ours.dll]==]
  )

ofs_target(mpchatuidiff-run
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/mpchat-ui]==]
    [==[i686-w64-mingw32-windres src/mpchat/mpchat.rc -O coff -o bin/mpchat-ui/mpchat.res]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Werror -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20840000 -o bin/mpchat-ui/MPCHAT_ours.dll src/mpchat/mpchat.c bin/mpchat-ui/mpchat.res src/mpchat/mpchat.def -lkernel32 -luser32 -lgdi32 -lgcc]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/mpchat-ui/mpchatuidiff.exe tools/difftest/mpchatuidiff.c -lgdi32]==]
    [==[bash tools/run_mpchatuidiff.sh --ui bin/mpchat-ui/MPCHAT_ours.dll ${N:-24}]==]
  )

ofs_target(mpdialog
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(mpdialog-build-check
  COMMANDS
  )

ofs_target(mpdialog-exception-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_mpdialogdiff.sh --exception-breaks]==]
  )

ofs_target(mpdialog-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL" || { echo "no English/MP_DIALOG.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL.orig"]==]
    [==[cp bin/MP_DIALOG.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL"]==]
  )

ofs_target(mpdialog-owner-acceptance
  DEPENDS openfs98
  COMMANDS
    [==[python3 -B tools/mpdialog_owner_acceptance.py]==]
  )

ofs_target(mpdialog-revert
  COMMANDS
    [==[cp "orig/English/mpdialog.dll" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/MP_DIALOG.DLL"]==]
  )

ofs_target(mpdialog-runtime-tables
  COMMANDS
  )

ofs_target(mpdialog-smoke
  DEPENDS openfs98
  COMMANDS
    [==[bash tools/run_mpdialogdiff.sh --smoke]==]
  )

ofs_target(mpdialog-ui-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_mpdialogdiff.sh --ui-breaks]==]
  )

ofs_target(mpdialoguidiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_mpdialogdiff.sh --ui]==]
  )

ofs_target(multip-directplay-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
    [==[python3 -B tools/multip_breaks.py --slice directplay]==]
  )

ofs_target(multip-receive-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
    [==[python3 -B tools/multip_breaks.py --slice receive]==]
  )

ofs_target(multip-send-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
    [==[python3 -B tools/multip_breaks.py --slice send]==]
  )

ofs_target(multip-worker-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
    [==[python3 -B tools/multip_breaks.py --slice workers]==]
  )

ofs_target(multipdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/G3D.DLL bin/difftest/G3D_retail.dll.tmp]==]
    [==[mv -f bin/difftest/G3D_retail.dll.tmp bin/difftest/G3D_retail.dll]==]
    [==[cp orig/Modules/multip.dll bin/difftest/MULTIP_retail.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_retail.dll.tmp bin/difftest/MULTIP_retail.dll]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DMULTIP_DIFFTEST -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/difftest/MULTIP_test.dll src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[cp bin/difftest/MULTIP_test.dll bin/difftest/MULTIP_ours.dll.tmp]==]
    [==[mv -f bin/difftest/MULTIP_ours.dll.tmp bin/difftest/MULTIP_ours.dll]==]
  )

ofs_target(numericdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
  )

ofs_target(ofstest
  DEPENDS openfs98
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/util -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20950000 -o bin/OFSTEST.DLL src/test/ofstest.c src/util/util_math.c src/util/util_files.c src/util/util_exchange.c src/util/util_alert.c src/test/ofstest.def -lkernel32 -lgcc]==]
  )

ofs_target(ofstest-remove
  COMMANDS
    [==[rm -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/Modules/OFSTEST.DLL"]==]
    [==[echo "OFSTEST.DLL removed from Modules"]==]
  )

ofs_target(options-build-check
  COMMANDS
  )

ofs_target(options-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL" || { echo "no English/Options.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL.orig"]==]
    [==[cp bin/OPTIONS.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL"]==]
  )

ofs_target(options-revert
  COMMANDS
    [==[cp "orig/English/Options.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Options.DLL"]==]
  )

ofs_target(options-runtime-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(options-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(optionsevents
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(panels-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer ${PANELS_BREAK_FLAGS:-} -fno-ipa-icf -Isrc/panels -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x10000000 -o ${PANELS_OUT:-} ${PANELS_SRC:-} bin/panels.res ${PANELS_DEF:-} -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc]==]
  )

ofs_target(panelsdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/PANELS.DLL bin/difftest/PANELS_retail.dll]==]
    [==[cp bin/PANELS.DLL bin/difftest/PANELS_ours.dll]==]
  )

ofs_target(panelsunit
  DEPENDS openfs98 difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DPANELS_UNIT -fno-ipa-icf -Isrc/panels -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x10000000 -o bin/difftest/PANELS_unit.dll src/panels/panels.c bin/panels.res src/panels/panels_unit.def -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc]==]
    [==[cp orig/Modules/PANELS.DLL bin/difftest/PANELS_retail.dll]==]
  )

ofs_target(reality-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/reality -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x208b0000 -o bin/difftest/REALITY_ours.dll ${REALITY_SRC:-} bin/reality.res src/reality/reality.def -lkernel32 -lgcc]==]
  )

ofs_target(realitydiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/REALITY.DLL bin/difftest/REALITY_retail.dll]==]
    [==[cp bin/REALITY.DLL bin/difftest/REALITY_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/Modules/NUMERIC.DLL bin/difftest/NUMERIC_retail.dll]==]
  )

# tools/hg2d_frames.sh's frame clock (the Makefile built it as the file target bin/hg2d-diff/FRAMELOCK.DLL)
ofs_target(framelock
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--image-base=0x20970000 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -o bin/hg2d-diff/FRAMELOCK.DLL tools/drive/framelock.c tools/drive/framelock.def -lkernel32 -luser32 -lgdi32]==]
  )

ofs_target(sehfix
  DEPENDS openfs98
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
  )

ofs_target(sehfix-log
  COMMANDS
    [==[cat "$HOME/.wine/drive_c/openfs98-sehfix.log" 2>/dev/null || echo "(no log -- SEHFIX was not loaded)"]==]
  )

ofs_target(sehfix-remove
  COMMANDS
    [==[rm -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/Modules/SEHFIX.DLL"]==]
    [==[echo "SEHFIX.DLL removed from Modules"]==]
  )

ofs_target(setup-map
  COMMANDS
    [==[python3 -B tools/extract_setup_map.py cd/Setup.exe > docs/setup_map.tsv]==]
    [==[wc -l < docs/setup_map.tsv | xargs echo "entries:"]==]
  )

ofs_target(sim
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(sim-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_simdiff.sh --breaks ${N:-20000}]==]
  )

ofs_target(sim-build-check
  COMMANDS
  )

ofs_target(sim-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL" || { echo "no English/Sim.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL.orig"]==]
    [==[cp bin/SIM.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL"]==]
  )

ofs_target(sim-revert
  COMMANDS
    [==[cp "orig/English/Sim.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Sim.DLL"]==]
  )

ofs_target(sim-runtime-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(sim-smoke
  DEPENDS openfs98
  COMMANDS
    [==[bash tools/run_simdiff.sh --smoke]==]
  )

ofs_target(sim-ui-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_simdiff.sh --ui-breaks]==]
  )

ofs_target(sim1-breaks
  DEPENDS difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o bin/sim1-diff/SIM1_PARTIAL.DLL src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S src/sim1/sim1.def]==]
    [==[python3 -B tools/sim1_breaks.py]==]
  )

ofs_target(sim1-frame-observer
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--image-base=0x20970000 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -o bin/hg2d-diff/FRAMELOCK.DLL tools/drive/framelock.c tools/drive/framelock.def -lkernel32 -luser32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
  )

ofs_target(sim1-native
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(sim1-reference
  COMMANDS
    [==[python3 -B tools/gen_sim1_data.py]==]
  )

ofs_target(sim1state-build
  DEPENDS difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o bin/sim1-diff/SIM1_STATE_TEST.DLL src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S tools/sim1_state_test.def]==]
  )

ofs_target(sim1air-build
  DEPENDS difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -DSIM1_AIR_TEST -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o bin/sim1-diff/SIM1_AIR_TEST.DLL src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S tools/sim1_air_test.def]==]
  )

ofs_target(sim1-partial
  DEPENDS difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o bin/sim1-diff/SIM1_PARTIAL.DLL src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S src/sim1/sim1.def]==]
  )

ofs_target(sim1-revert
  DEPENDS difftests
  COMMANDS
    [==[python3 -B tools/stage_sim1.py "${FS98_DIR:-$HOME/.wine/drive_c/FS98}" --retail]==]
  )

ofs_target(sim1-snapshots
  DEPENDS openfs98
  COMMANDS
    [==[mkdir -p bin/hg2d-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--image-base=0x20970000 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -o bin/hg2d-diff/FRAMELOCK.DLL tools/drive/framelock.c tools/drive/framelock.def -lkernel32 -luser32 -lgdi32]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -D_WIN32_WINNT=0x0501 -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20960000 -o bin/SEHFIX.DLL src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32]==]
    [==[python3 -B tools/fly_scripts.py bin/sim1-diff/scripts]==]
    [==[bash tools/sim1_snapshots.sh]==]
  )

ofs_target(sim1air-breaks
  DEPENDS difftests sim1air-build
  COMMANDS
    [==[python3 -B tools/sim1_air_breaks.py]==]
  )

ofs_target(sim1air-run
  DEPENDS difftests sim1air-build
  COMMANDS
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1airdiff.exe ${N:-20000} retail-first]==]
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1airdiff.exe ${N:-20000} ours-first]==]
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1airdiff.exe ${N:-20000} relocate]==]
  )

ofs_target(sim1diff-run
  DEPENDS difftests
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o bin/sim1-diff/SIM1_PARTIAL.DLL src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S src/sim1/sim1.def]==]
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1diff.exe ${N:-20000} retail-first]==]
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1diff.exe ${N:-20000} ours-first]==]
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1diff.exe ${N:-20000} relocate]==]
  )

ofs_target(sim1rdiff
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(sim1state-breaks
  DEPENDS difftests sim1state-build
  COMMANDS
    [==[python3 -B tools/sim1_state_breaks.py]==]
  )

ofs_target(sim1state-run
  DEPENDS difftests sim1state-build
  COMMANDS
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1statediff.exe ${N:-20000} retail-first]==]
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1statediff.exe ${N:-20000} ours-first]==]
    [==[WINEPREFIX="$PWD/bin/sim1-diff/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock bin/sim1-diff/wine.lock timeout 180s wine bin/sim1-diff/sim1statediff.exe ${N:-20000} relocate]==]
  )

ofs_target(simdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_simdiff.sh ${N:-20000} ${SEED:-0x73a915e2} ${MODE:-normal}]==]
  )

ofs_target(simevents
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(simsui-build-check
  COMMANDS
  )

ofs_target(simsui-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL" || { echo "no SIMSUI.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL.orig"]==]
    [==[cp bin/SIMSUI.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL"]==]
  )

ofs_target(simsui-native-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(simsui-revert
  COMMANDS
    [==[cp "orig/SIMSUI.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/SIMSUI.DLL"]==]
  )

ofs_target(simsui-tables
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(simsuidiff
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(simuidiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_simdiff.sh --ui]==]
  )

ofs_target(sounddiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/SOUND.DLL bin/difftest/SOUND_retail.dll]==]
    [==[cp bin/SOUND.DLL bin/difftest/SOUND_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp orig/ENGLISH.FLL bin/difftest/ENGLISH.FLL]==]
    [==[cp orig/FLTSIM98.EXE bin/difftest/FLTSIM98_host.exe]==]
    [==[mkdir -p bin/difftest/cdsound]==]
    [==[for a in B737_400 Extra300 Bell206B Schweizr Lear45 Sopcamel C182@SEMI@ do cp orig/Aircraft/$a/sound/sound.cfg bin/difftest/cdsound/$a.cfg@SEMI@ done]==]
  )

ofs_target(stage
  COMMANDS
    [==[python3 -B tools/stage_game.py]==]
  )

ofs_target(stage-scenery
  COMMANDS
    [==[python3 -B tools/stage_scenery.py]==]
  )

ofs_target(suction-breaks
  DEPENDS difftests test-gauges
  COMMANDS
    [==[bash tools/run_gaugesdiff.sh --suction-breaks]==]
  )

ofs_target(trace
  COMMANDS
    [==[cd ${FS98_DIR:-$HOME/.wine/drive_c/FS98} && WINEDEBUG=+loaddll wine FLTSIM98.EXE 2>&1 | grep -i Modules]==]
  )

ofs_target(trace-ddraw
  COMMANDS
    [==[echo "logging to /tmp/fs98-ddraw.log -- fly until the view freezes, then quit"]==]
    [==[cd ${FS98_DIR:-$HOME/.wine/drive_c/FS98} && WINEDEBUG=+ddraw,fixme+all,err+all wine FLTSIM98.EXE > /tmp/fs98-ddraw.log 2>&1 || true]==]
    [==[echo "log size: $(du -h /tmp/fs98-ddraw.log | cut -f1)"]==]
    [==[echo "--- last 40 distinct ddraw calls ---"]==]
    [==[grep -oE 'ddraw:[a-zA-Z0-9_]+' /tmp/fs98-ddraw.log | tail -2000 | uniq | tail -40]==]
  )

ofs_target(trace-render
  COMMANDS
    [==[echo "logging to /tmp/fs98-render.log -- fly, then quit the sim"]==]
    [==[cd ${FS98_DIR:-$HOME/.wine/drive_c/FS98} && WINEDEBUG=fixme+all,err+all wine FLTSIM98.EXE > /tmp/fs98-render.log 2>&1 || true]==]
    [==[echo "--- most common complaints ---"]==]
    [==[grep -oE '(fixme|err):[a-z0-9_]+:[A-Za-z0-9_]+' /tmp/fs98-render.log | sort | uniq -c | sort -rn | head -20]==]
  )

ofs_target(utildiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
    [==[cp bin/UTIL.DLL bin/difftest/UTIL_ours.dll]==]
    [==[cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll]==]
  )

ofs_target(vd-off
  COMMANDS
    [==[wine reg delete 'HKCU\Software\Wine\AppDefaults\FLTSIM98.EXE\Explorer' /v Desktop /f >/dev/null 2>&1]==]
    [==[wine reg delete 'HKCU\Software\Wine\Explorer' /v Desktop /f >/dev/null 2>&1]==]
    [==[echo "tools/ofs run uses a ${VD:-1024x768} virtual desktop by itself now@SEMI@ tools/ofs run VD= for none"]==]
  )

ofs_target(vd-on
  COMMANDS
    [==[wine reg delete 'HKCU\Software\Wine\AppDefaults\FLTSIM98.EXE\Explorer' /v Desktop /f >/dev/null 2>&1]==]
    [==[wine reg delete 'HKCU\Software\Wine\Explorer' /v Desktop /f >/dev/null 2>&1]==]
    [==[echo "tools/ofs run uses a ${VD:-1024x768} virtual desktop by itself now@SEMI@ tools/ofs run VD= for none"]==]
  )

ofs_target(views
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(views-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_viewsdiff.sh --breaks ${N:-20000}]==]
  )

ofs_target(views-build-check
  COMMANDS
  )

ofs_target(views-install
  DEPENDS openfs98
  COMMANDS
    [==[test -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL" || { echo "no English/Views.DLL -- set FS98_DIR"@SEMI@ exit 1@SEMI@ }]==]
    [==[test -e "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL.orig" || cp -p "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL.orig"]==]
    [==[cp bin/VIEWS.DLL "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL.openfs98"]==]
    [==[mv -f "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL.openfs98" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL"]==]
  )

ofs_target(views-revert
  COMMANDS
    [==[cp "orig/English/Views.DLL" "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/English/Views.DLL"]==]
  )

ofs_target(views-smoke
  DEPENDS openfs98
  COMMANDS
    [==[bash tools/run_viewsdiff.sh --smoke]==]
  )

ofs_target(views-ui-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_viewsdiff.sh --ui-breaks]==]
  )

ofs_target(viewsevents
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(viewseventsdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_viewsdiff.sh ${N:-20000}]==]
  )

ofs_target(viewsuidiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_viewsdiff.sh --ui]==]
  )

ofs_target(weathermdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WEATHERM.DLL bin/difftest/WEATHERM_retail.dll]==]
    [==[cp bin/WEATHERM.DLL bin/difftest/WEATHERM_ours.dll]==]
    [==[cp orig/Modules/CHAIN.DLL bin/difftest/CHAIN_retail.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
  )

ofs_target(window-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/window -Ibin/window -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20900000 -o bin/difftest/WINDOW_ours.dll ${WINDOW_SRC:-} bin/window.res src/window/window.def -lkernel32 -lgcc]==]
  )

ofs_target(window-reset
  COMMANDS
    [==[sed -i 's/^Location=.*/Location=0,0,648,533/' "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/FLTSIM98.CFG"]==]
    [==[sed -i 's/^Maximized=.*/Maximized=0/' "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/FLTSIM98.CFG"]==]
    [==[grep -E '^Location=|^Maximized=' "${FS98_DIR:-$HOME/.wine/drive_c/FS98}/FLTSIM98.CFG"]==]
    [==[echo "window set to its native 640x480 client size -- do not resize it"]==]
  )

ofs_target(windowdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WINDOW.DLL bin/difftest/WINDOW_retail.dll]==]
    [==[cp bin/WINDOW.DLL bin/difftest/WINDOW_ours.dll]==]
    [==[cp orig/Modules/UTIL.DLL bin/difftest/UTIL_retail.dll]==]
  )

ofs_target(winmsg-break-build
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/winmsg -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20910000 -o bin/difftest/WINMSG_ours.dll ${WINMSG_SRC:-} bin/winmsg.res src/winmsg/winmsg.def -lkernel32 -luser32 -lgcc]==]
  )

ofs_target(winmsgdiff
  DEPENDS openfs98 difftests
  COMMANDS
    [==[cp orig/Modules/WINMSG.DLL bin/difftest/WINMSG_retail.dll]==]
    [==[cp bin/WINMSG.DLL bin/difftest/WINMSG_ours.dll]==]
  )

ofs_target(wm-managed
  COMMANDS
    [==[wine reg delete 'HKCU\Software\Wine\X11 Driver' /v Managed /f]==]
    [==[wine reg delete 'HKCU\Software\Wine\X11 Driver' /v Decorated /f]==]
    [==[echo "window manager control restored (wine default)"]==]
  )

ofs_target(wm-unmanaged
  COMMANDS
    [==[wine reg add 'HKCU\Software\Wine\X11 Driver' /v Managed /d N /f]==]
    [==[wine reg add 'HKCU\Software\Wine\X11 Driver' /v Decorated /d N /f]==]
    [==[echo "window manager no longer controls wine windows"]==]
  )

ofs_target(world-advanced-cloud
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-cloud ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-advanced-core-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-core-breaks]==]
  )

ofs_target(world-advanced-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-advanced-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-life-breaks]==]
  )

ofs_target(world-advanced-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-advanced-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-native-breaks]==]
  )

ofs_target(world-advanced-parent
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-parent ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-advanced-temperature
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-temperature ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-advanced-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-ui]==]
  )

ofs_target(world-advanced-wind
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --advanced-wind ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-airport-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --airport-breaks]==]
  )

ofs_target(world-airport-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --airport-core ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-airport-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --airport-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-airport-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --airport-life-breaks]==]
  )

ofs_target(world-airport-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-airport-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --airport-native-breaks]==]
  )

ofs_target(world-airport-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --airport-ui]==]
  )

ofs_target(world-booster-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --booster-breaks]==]
  )

ofs_target(world-booster-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --booster-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-booster-init
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --booster-init ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-booster-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --booster-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-booster-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --booster-life-breaks]==]
  )

ofs_target(world-booster-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --booster-native-breaks]==]
  )

ofs_target(world-booster-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --booster-ui]==]
  )

ofs_target(world-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --breaks]==]
  )

ofs_target(world-build-check
  COMMANDS
  )

ofs_target(world-cache-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --cache-breaks]==]
  )

ofs_target(world-cache-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --cache-core ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-cache-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --cache-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-cache-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --cache-life-breaks]==]
  )

ofs_target(world-cache-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-cache-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --cache-native-breaks]==]
  )

ofs_target(world-cache-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --cache-ui]==]
  )

ofs_target(world-complexity-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-breaks]==]
  )

ofs_target(world-complexity-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-complexity-init
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-init ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-complexity-init-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-init-breaks]==]
  )

ofs_target(world-complexity-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-complexity-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-life-breaks]==]
  )

ofs_target(world-complexity-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-complexity-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-native-breaks]==]
  )

ofs_target(world-complexity-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --complexity-ui]==]
  )

ofs_target(world-copy-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --copy-breaks]==]
  )

ofs_target(world-copy-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --copy-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-copy-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --copy-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-copy-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --copy-life-breaks]==]
  )

ofs_target(world-copy-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-copy-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --copy-native-breaks]==]
  )

ofs_target(world-copy-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --copy-ui]==]
  )

ofs_target(world-dynamic-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-breaks]==]
  )

ofs_target(world-dynamic-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-dynamic-init
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-init ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-dynamic-init-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-init-breaks]==]
  )

ofs_target(world-dynamic-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-dynamic-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-life-breaks]==]
  )

ofs_target(world-dynamic-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-dynamic-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-native-breaks]==]
  )

ofs_target(world-dynamic-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --dynamic-ui]==]
  )

ofs_target(world-facilities-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --facilities-breaks]==]
  )

ofs_target(world-facilities-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --facilities-core ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-facilities-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --facilities-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-facilities-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --facilities-life-breaks]==]
  )

ofs_target(world-facilities-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-facilities-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --facilities-native-breaks]==]
  )

ofs_target(world-facilities-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --facilities-ui]==]
  )

ofs_target(world-files-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --files-breaks]==]
  )

ofs_target(world-files-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --files-core ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-files-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --files-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-files-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --files-life-breaks]==]
  )

ofs_target(world-files-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-files-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --files-native-breaks]==]
  )

ofs_target(world-files-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --files-ui]==]
  )

ofs_target(world-init-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --init-breaks]==]
  )

ofs_target(world-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --life-breaks]==]
  )

ofs_target(world-list-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --list-breaks]==]
  )

ofs_target(world-list-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --list-core ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-list-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --list-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-list-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --list-life-breaks]==]
  )

ofs_target(world-list-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-list-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --list-native-breaks]==]
  )

ofs_target(world-list-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --list-ui]==]
  )

ofs_target(world-position-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --position-breaks]==]
  )

ofs_target(world-position-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --position-core ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-position-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --position-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-position-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --position-life-breaks]==]
  )

ofs_target(world-position-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-position-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --position-native-breaks]==]
  )

ofs_target(world-position-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --position-ui]==]
  )

ofs_target(world-priority-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --priority-breaks]==]
  )

ofs_target(world-priority-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --priority-core ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-priority-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --priority-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-priority-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --priority-ui]==]
  )

ofs_target(world-search-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-breaks]==]
  )

ofs_target(world-search-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-search-init
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-init ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(world-search-init-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-init-breaks]==]
  )

ofs_target(world-search-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-search-life-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-life-breaks]==]
  )

ofs_target(world-search-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-search-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-native-breaks]==]
  )

ofs_target(world-search-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --search-ui]==]
  )

ofs_target(world-smoke
  DEPENDS openfs98
  COMMANDS
    [==[bash tools/run_worlddiff.sh --smoke]==]
  )

ofs_target(world-time-breaks
  COMMANDS
    [==[bash tools/run_worlddiff.sh --time-breaks]==]
  )

ofs_target(world-time-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --time-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-time-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --time-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-time-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-time-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --time-native-breaks]==]
  )

ofs_target(world-time-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --time-ui]==]
  )

ofs_target(world-ui-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --ui-breaks]==]
  )

ofs_target(world-wareas-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --wareas-breaks]==]
  )

ofs_target(world-wareas-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --wareas-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-wareas-init
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --wareas-init ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-wareas-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --wareas-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-wareas-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-wareas-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --wareas-native-breaks]==]
  )

ofs_target(world-wareas-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --wareas-ui]==]
  )

ofs_target(world-weather-breaks
  COMMANDS
    [==[bash tools/run_worlddiff.sh --weather-breaks]==]
  )

ofs_target(world-weather-core
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --weather-core ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-weather-init
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --weather-init ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-weather-life
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --weather-life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(world-weather-metadata
  DEPENDS openfs98
  COMMANDS
  )

ofs_target(world-weather-native-breaks
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --weather-native-breaks]==]
  )

ofs_target(world-weather-ui
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --weather-ui]==]
  )

ofs_target(worldcore
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(worlddiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(worldinitdiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --init ${N:-2000} ${MODE:-normal}]==]
  )

ofs_target(worldlife
  DEPENDS openfs98 difftests
  COMMANDS
  )

ofs_target(worldlifediff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --life ${N:-2000} ${SEED:-0x4a971b63} ${MODE:-normal}]==]
  )

ofs_target(worlduidiff-run
  DEPENDS openfs98 difftests
  COMMANDS
    [==[bash tools/run_worlddiff.sh --ui]==]
  )
