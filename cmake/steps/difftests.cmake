# Build steps for the "difftests" group, in the Makefile's own order. Each step is the command list
# `make -n -B` printed for that output (win32-retail, run from the source root). Outputs land in
# bin/, as the Makefile's do. Edit by hand when a rule changes.

ofs_step(difftests
  OUTPUTS bin/alerts-diff/alertseventsdiff.exe
  COMMANDS
    [==[mkdir -p bin/alerts-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -o bin/alerts-diff/alertseventsdiff.exe tools/difftest/alertseventsdiff.c src/alerts/alerts_events.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/alerts-diff/alertsobjectsdiff.exe
  COMMANDS
    [==[mkdir -p bin/alerts-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -o bin/alerts-diff/alertsobjectsdiff.exe tools/difftest/alertsobjectsdiff.c src/alerts/alerts_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/alerts-diff/alertsuidiff.exe
  COMMANDS
    [==[mkdir -p bin/alerts-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/alerts-diff/alertsuidiff.exe tools/difftest/alertsuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/acontaindiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/acontaindiff.exe tools/difftest/acontaindiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/advdrvdiff.exe
  DEPENDS_FILES src/advdrv/advdrv_relocs.inc
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/host/runtime -o bin/difftest/advdrvdiff.exe tools/difftest/advdrvdiff.c src/host/runtime/host_rt_divide.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/analysisdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/analysisdiff.exe tools/difftest/analysisdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/atcdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/atcdiff.exe tools/difftest/atcdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/boosterdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/host/runtime -o bin/difftest/boosterdiff.exe tools/difftest/boosterdiff.c src/host/runtime/host_rt_divide.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/chaindiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/chaindiff.exe tools/difftest/chaindiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/controlsdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/controlsdiff.exe tools/difftest/controlsdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/demodiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/demodiff.exe tools/difftest/demodiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/dynamicdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/host/runtime -o bin/difftest/dynamicdiff.exe tools/difftest/dynamicdiff.c src/host/runtime/host_rt_divide.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/facildiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe -o bin/difftest/facildiff.exe tools/difftest/facildiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/fediff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe -o bin/difftest/fediff.exe tools/difftest/fediff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/fegriddiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/fegriddiff.exe tools/difftest/fegriddiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/fesldiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe -o bin/difftest/fesldiff.exe tools/difftest/fesldiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/g3ddiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/g3ddiff.exe tools/difftest/g3ddiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/g3dmeshdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -c -DG3D_MESH_NAME=g3d_mesh_hw -o bin/difftest/g3d_mesh_hw.o src/g3d/g3d_mesh.c]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -c -DFS98_PORTABLE_FP -DG3D_MESH_NAME=g3d_mesh_soft -o bin/difftest/g3d_mesh_soft.o src/g3d/g3d_mesh.c]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/g3dmeshdiff.exe tools/difftest/g3dmeshdiff.c bin/difftest/g3d_mesh_hw.o bin/difftest/g3d_mesh_soft.o]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/hg2dmodeldiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/hg2dmodeldiff.exe tools/difftest/hg2dmodeldiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/hostdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Wl,--image-base=0x13000000 -o bin/difftest/hostdiff.exe tools/difftest/hostdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/landmediff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe -o bin/difftest/landmediff.exe tools/difftest/landmediff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/maindiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/maindiff.exe tools/difftest/maindiff.c -lm -lgdi32]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/mpchatdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/mpchatdiff.exe tools/difftest/mpchatdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/multipdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -o bin/difftest/multipdiff.exe tools/difftest/multipdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/numericdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/numeric -o bin/difftest/numericdiff.exe tools/difftest/numericdiff.c src/numeric/numeric.c -lm]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/panelsdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=gnu99 -O1 -Wall -Wextra -DGAME_DIR='"..\\..\\game"' -o bin/difftest/panelsdiff.exe tools/difftest/panelsdiff.c -lgdi32 -luser32]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/realitydiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/realitydiff.exe tools/difftest/realitydiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/slewdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/util -o bin/difftest/slewdiff.exe tools/difftest/slewdiff.c src/util/util_math.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/sounddiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/sounddiff.exe tools/difftest/sounddiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/utildiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/util -o bin/difftest/utildiff.exe tools/difftest/utildiff.c src/util/util_math.c src/util/util_exchange.c src/util/util_alert.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/utildiff_portable.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/util -DFS98_PORTABLE_FP -o bin/difftest/utildiff_portable.exe tools/difftest/utildiff.c src/util/util_math.c src/util/util_exchange.c src/util/util_alert.c -lm]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/weathermdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/weathermdiff.exe tools/difftest/weathermdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/windowdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/windowdiff.exe tools/difftest/windowdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/winmsgdiff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -o bin/difftest/winmsgdiff.exe tools/difftest/winmsgdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/flights-diff/flightseventsdiff.exe
  COMMANDS
    [==[mkdir -p bin/flights-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -o bin/flights-diff/flightseventsdiff.exe tools/difftest/flightseventsdiff.c src/flights/flights_events.c src/flights/flights_objects.c src/flights/flights_save.c src/flights/flights_comms.c src/flights/flights_create.c src/flights/flights_aircraft.c src/flights/flights_opening.c src/flights/flights_select.c src/flights/flights_autopilot.c src/flights/flights_navigation.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/fsdialogs.exe
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -O1 -Wall -o bin/fsdialogs.exe tools/drive/fsdialogs.c -lgdi32 -luser32]==]
  )

ofs_step(difftests
  OUTPUTS bin/gauges-all/allgaugesdiff.exe
  DEPENDS_FILES bin/gauges-all/all_gauges.inc
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=gnu99 -O2 -Wall -Wextra -Werror -Ibin/gauges-all -o bin/gauges-all/allgaugesdiff.exe tools/difftest/allgaugesdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/gauges-all/allpaneldiff.exe
  DEPENDS_FILES bin/gauges-all/all_panels.inc
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=gnu99 -O1 -Wall -Wextra -DGAUGES_DIFF -DGAUGES_ALL_DIFF -DGAME_DIR='"..\\..\\game"' -Ibin/gauges-all -o bin/gauges-all/allpaneldiff.exe tools/difftest/panelsdiff.c -lgdi32 -luser32]==]
  )

ofs_step(difftests
  OUTPUTS bin/gauges-diff/gaugesdiff.exe
  COMMANDS
    [==[mkdir -p bin/gauges-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/gauges-diff/gaugesdiff.exe tools/difftest/gaugesdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/gauges-diff/gaugespanelsdiff.exe
  COMMANDS
    [==[mkdir -p bin/gauges-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=gnu99 -O1 -Wall -Wextra -DGAUGES_DIFF -DGAME_DIR='"..\\..\\game"' -o bin/gauges-diff/gaugespanelsdiff.exe tools/difftest/panelsdiff.c -lgdi32 -luser32]==]
  )

ofs_step(difftests
  OUTPUTS bin/gauges-diff/suctiondiff.exe
  COMMANDS
    [==[mkdir -p bin/gauges-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -DSUCTION_DIFF -o bin/gauges-diff/suctiondiff.exe tools/difftest/gaugesdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/help-diff/helpeventsdiff.exe
  COMMANDS
    [==[mkdir -p bin/help-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/help-diff/helpeventsdiff.exe tools/difftest/helpeventsdiff.c src/help/help_events.c src/help/help_objects.c src/help/help_stack.S -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/help-diff/helpuidiff.exe
  COMMANDS
    [==[mkdir -p bin/help-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/help-diff/helpuidiff.exe tools/difftest/helpuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/host-rt/hostrtdiff.exe
  COMMANDS
    [==[mkdir -p bin/host-rt]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Isrc/host/runtime -Iinclude -Wl,--image-base=0x13000000 -Wl,--disable-dynamicbase -o bin/host-rt/hostrtdiff.exe tools/difftest/hostrtdiff.c src/host/runtime/host_rt_child.c src/host/runtime/host_rt_common.c src/host/runtime/host_rt_config.c src/host/runtime/host_rt_cursor.c src/host/runtime/host_rt_ddraw.c src/host/runtime/host_rt_display.c src/host/runtime/host_rt_divide.c src/host/runtime/host_rt_floating.c src/host/runtime/host_rt_frame.c src/host/runtime/host_rt_fullscreen.c src/host/runtime/host_rt_layout.c src/host/runtime/host_rt_main.c src/host/runtime/host_rt_misc.c src/host/runtime/host_rt_module_load.c src/host/runtime/host_rt_modules.c src/host/runtime/host_rt_mouse.c src/host/runtime/host_rt_startup.c src/host/runtime/host_rt_surface.c src/host/runtime/host_rt_window.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/difftest/dividediff.exe
  COMMANDS
    [==[mkdir -p bin/difftest]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -Isrc/host/runtime -Wl,--image-base=0x13000000 -Wl,--disable-dynamicbase -o bin/difftest/dividediff.exe tools/difftest/dividediff.c src/host/runtime/host_rt_divide.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/mdl-crash/mdlcrashdiff.exe
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Ibin/mdl-crash -Ibin/mdl-diff -o bin/mdl-crash/mdlcrashdiff.exe tools/difftest/mdlcrashdiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/mdl-diff/mdldiff.exe
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/gen_mdl_fixtures.py]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/mdl -Ibin/mdl-diff -o bin/mdl-diff/mdldiff.exe tools/difftest/mdldiff.c src/mdl/mdl_normal.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/mdl-diff/mdldlldiff.exe
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Ibin/mdl-diff -o bin/mdl-diff/mdldlldiff.exe tools/difftest/mdldlldiff.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/mpdialog-diff/mpdialoguidiff.exe
  COMMANDS
    [==[mkdir -p bin/mpdialog-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/mpdialog-diff/mpdialoguidiff.exe tools/difftest/mpdialoguidiff.c -luser32 -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/options-diff/optionseventsdiff.exe
  COMMANDS
    [==[mkdir -p bin/options-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -o bin/options-diff/optionseventsdiff.exe tools/difftest/optionseventsdiff.c src/options/options_events.c src/options/options_objects.c src/options/options_video.c src/options/options_videos.c src/options/options_logbook.c src/options/options_book.c src/options/options_graphs.c src/options/options_analysis.c src/options/options_replay.c src/options/options_demo.c src/options/options_display.c src/options/options_prefs.c src/options/options_controls.c src/options/options_keys.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/sim1-diff/sim1airdiff.exe
  COMMANDS
    [==[mkdir -p bin/sim1-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -o bin/sim1-diff/sim1airdiff.exe tools/difftest/sim1airdiff.c tools/difftest/sim1call.S tools/difftest/sim1state_stubs.S tools/difftest/sim1air_stubs.S]==]
  )

ofs_step(difftests
  OUTPUTS bin/sim1-diff/sim1diff.exe
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -o bin/sim1-diff/sim1diff.exe tools/difftest/sim1diff.c tools/difftest/sim1call.S]==]
  )

ofs_step(difftests
  OUTPUTS bin/sim1-diff/sim1rdiff.exe
  COMMANDS
    [==[mkdir -p bin/sim1-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -fno-math-errno -fno-tree-loop-distribute-patterns -fno-tree-vectorize -Wl,--image-base=0x400000 -o bin/sim1-diff/sim1rdiff.exe tools/difftest/sim1rdiff.c src/sim1/sim1_small.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/sim1-diff/sim1statediff.exe
  COMMANDS
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/sim1 -Ibin/sim1-diff -o bin/sim1-diff/sim1statediff.exe tools/difftest/sim1statediff.c tools/difftest/sim1call.S tools/difftest/sim1state_stubs.S]==]
  )

ofs_step(difftests
  OUTPUTS bin/sim-diff/simdiff.exe
  COMMANDS
    [==[mkdir -p bin/sim-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/sim-diff/simdiff.exe tools/difftest/simdiff.c src/sim/sim_events.c src/sim/sim_objects.c src/sim/sim_crash.c src/sim/sim_engines.c src/sim/sim_systems.c src/sim/sim_realism.c src/sim/sim_fuel.c src/sim/sim_settings.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/sim-diff/simuidiff.exe
  COMMANDS
    [==[mkdir -p bin/sim-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/sim-diff/simuidiff.exe tools/difftest/simuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/simsui-diff/simsuidiff.exe
  COMMANDS
    [==[mkdir -p bin/simsui-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -fno-tree-loop-distribute-patterns -fno-tree-vectorize -Iinclude -Wl,--image-base=0x400000 -o bin/simsui-diff/simsuidiff.exe tools/difftest/simsuidiff.c @SIMSUI_SOURCES@]==]
  )

ofs_step(difftests
  OUTPUTS bin/views-diff/viewseventsdiff.exe
  COMMANDS
    [==[mkdir -p bin/views-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/views-diff/viewseventsdiff.exe tools/difftest/viewseventsdiff.c src/views/views_events.c src/views/views_objects.c src/views/views_stack.S tools/difftest/viewsstack.S -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/views-diff/viewsuidiff.exe
  COMMANDS
    [==[mkdir -p bin/views-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/views-diff/viewsuidiff.exe tools/difftest/viewsuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldadvancedclouddiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldadvancedclouddiff.exe tools/difftest/worldadvancedclouddiff.c src/world/world_advanced.c src/world/world_advanced_temperature.c src/world/world_advanced_cloud.c src/world/world_advanced_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldadvancedlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldadvancedlifediff.exe tools/difftest/worldadvancedlifediff.c src/world/world_advanced_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldadvancedparentdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldadvancedparentdiff.exe tools/difftest/worldadvancedparentdiff.c src/world/world_advanced.c src/world/world_advanced_temperature.c src/world/world_advanced_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldadvancedtemperaturediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldadvancedtemperaturediff.exe tools/difftest/worldadvancedtemperaturediff.c src/world/world_advanced.c src/world/world_advanced_temperature.c src/world/world_advanced_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldadvanceduidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldadvanceduidiff.exe tools/difftest/worldadvanceduidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldadvancedwinddiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldadvancedwinddiff.exe tools/difftest/worldadvancedwinddiff.c src/world/world_advanced.c src/world/world_advanced_temperature.c src/world/world_advanced_wind.c src/world/world_advanced_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldairportdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldairportdiff.exe tools/difftest/worldairportdiff.c src/world/world_airport.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldairportlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldairportlifediff.exe tools/difftest/worldairportlifediff.c src/world/world_airport_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldairportuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldairportuidiff.exe tools/difftest/worldairportuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldboosterdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldboosterdiff.exe tools/difftest/worldboosterdiff.c src/world/world_booster.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldboosterinitdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldboosterinitdiff.exe tools/difftest/worldboosterinitdiff.c src/world/world_booster_init.c src/world/world_booster.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldboosterlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldboosterlifediff.exe tools/difftest/worldboosterlifediff.c src/world/world_booster_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldboosteruidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldboosteruidiff.exe tools/difftest/worldboosteruidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcachediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcachediff.exe tools/difftest/worldcachediff.c src/world/world_cache.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcachelifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcachelifediff.exe tools/difftest/worldcachelifediff.c src/world/world_cache_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcacheuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcacheuidiff.exe tools/difftest/worldcacheuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcomplexitydiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcomplexitydiff.exe tools/difftest/worldcomplexitydiff.c src/world/world_complexity.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcomplexityinitdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcomplexityinitdiff.exe tools/difftest/worldcomplexityinitdiff.c src/world/world_complexity.c src/world/world_complexity_init.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcomplexitylifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcomplexitylifediff.exe tools/difftest/worldcomplexitylifediff.c src/world/world_complexity_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcomplexityuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcomplexityuidiff.exe tools/difftest/worldcomplexityuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcopydiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcopydiff.exe tools/difftest/worldcopydiff.c src/world/world_copy.c src/world/world_copy_init.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcopylifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcopylifediff.exe tools/difftest/worldcopylifediff.c src/world/world_copy_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldcopyuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldcopyuidiff.exe tools/difftest/worldcopyuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worlddiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worlddiff.exe tools/difftest/worlddiff.c src/world/world_area.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worlddynamicdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worlddynamicdiff.exe tools/difftest/worlddynamicdiff.c src/world/world_dynamic.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worlddynamicinitdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worlddynamicinitdiff.exe tools/difftest/worlddynamicinitdiff.c src/world/world_dynamic_init.c src/world/world_dynamic.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worlddynamiclifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worlddynamiclifediff.exe tools/difftest/worlddynamiclifediff.c src/world/world_dynamic_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worlddynamicuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worlddynamicuidiff.exe tools/difftest/worlddynamicuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldfacilitiesdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldfacilitiesdiff.exe tools/difftest/worldfacilitiesdiff.c src/world/world_facilities.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldfacilitieslifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldfacilitieslifediff.exe tools/difftest/worldfacilitieslifediff.c src/world/world_facilities_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldfacilitiesuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldfacilitiesuidiff.exe tools/difftest/worldfacilitiesuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldfilesdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldfilesdiff.exe tools/difftest/worldfilesdiff.c src/world/world_files.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldfileslifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldfileslifediff.exe tools/difftest/worldfileslifediff.c src/world/world_files_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldfilesuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldfilesuidiff.exe tools/difftest/worldfilesuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldgameui.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldgameui.exe tools/difftest/worldgameui.c -luser32]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldinitdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldinitdiff.exe tools/difftest/worldinitdiff.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldlifediff.exe tools/difftest/worldlifediff.c src/world/world_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldlistdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldlistdiff.exe tools/difftest/worldlistdiff.c src/world/world_list.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldlistlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldlistlifediff.exe tools/difftest/worldlistlifediff.c src/world/world_list_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldlistuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldlistuidiff.exe tools/difftest/worldlistuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldpositiondiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldpositiondiff.exe tools/difftest/worldpositiondiff.c src/world/world_position.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldpositionlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldpositionlifediff.exe tools/difftest/worldpositionlifediff.c src/world/world_position_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldpositionuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldpositionuidiff.exe tools/difftest/worldpositionuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldprioritydiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldprioritydiff.exe tools/difftest/worldprioritydiff.c src/world/world_priority.c src/world/world_priority_objects.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldprioritylifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldprioritylifediff.exe tools/difftest/worldprioritylifediff.c src/world/world_priority_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldpriorityuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldpriorityuidiff.exe tools/difftest/worldpriorityuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldsearchdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -o bin/world-diff/worldsearchdiff.exe tools/difftest/worldsearchdiff.c src/world/world_search.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldsearchinitdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -Iinclude -o bin/world-diff/worldsearchinitdiff.exe tools/difftest/worldsearchinitdiff.c src/world/world_search_init.c src/world/world_search.c src/world/world_init.c src/world/world_area.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldsearchlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldsearchlifediff.exe tools/difftest/worldsearchlifediff.c src/world/world_search_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldsearchuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldsearchuidiff.exe tools/difftest/worldsearchuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldtimediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldtimediff.exe tools/difftest/worldtimediff.c src/world/world_time.c src/world/world_time_init.c src/world/world_time_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldtimelifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldtimelifediff.exe tools/difftest/worldtimelifediff.c src/world/world_time.c src/world/world_time_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldtimeuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldtimeuidiff.exe tools/difftest/worldtimeuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worlduidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worlduidiff.exe tools/difftest/worlduidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldwareasdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldwareasdiff.exe tools/difftest/worldwareasdiff.c src/world/world_wareas.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldwareasinitdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldwareasinitdiff.exe tools/difftest/worldwareasinitdiff.c src/world/world_wareas.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldwareaslifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldwareaslifediff.exe tools/difftest/worldwareaslifediff.c src/world/world_wareas.c src/world/world_wareas_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldwareasuidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldwareasuidiff.exe tools/difftest/worldwareasuidiff.c -lgdi32 -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldweatherdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldweatherdiff.exe tools/difftest/worldweatherdiff.c src/world/world_weather.c]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldweatherinitdiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldweatherinitdiff.exe tools/difftest/worldweatherinitdiff.c src/world/world_weather.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldweatherlifediff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldweatherlifediff.exe tools/difftest/worldweatherlifediff.c src/world/world_weather.c src/world/world_weather_objects.c -lntdll]==]
  )

ofs_step(difftests
  OUTPUTS bin/world-diff/worldweatheruidiff.exe
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -o bin/world-diff/worldweatheruidiff.exe tools/difftest/worldweatheruidiff.c -lgdi32 -lntdll]==]
  )
