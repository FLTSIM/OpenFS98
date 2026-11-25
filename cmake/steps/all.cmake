# Build steps for the "all" group, in the Makefile's own order. Each step is the command list
# `make -n -B` printed for that output (win32-retail, run from the source root). Outputs land in
# bin/, as the Makefile's do. Edit by hand when a rule changes.

ofs_step(all
  OUTPUTS bin/ACONTAIN.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/acontain/acontain.rc -O coff -o bin/acontain.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/acontain -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20600000 -o bin/ACONTAIN.DLL src/acontain/acontain.c bin/acontain.res src/acontain/acontain.def -lmsvcrt -luser32 -lkernel32]==]
    [==[python3 -B tools/check_data_image.py --hash ACONTAIN bin/ACONTAIN.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/ADVDRV.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/advdrv/advdrv.rc -O coff -o bin/advdrv.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/advdrv -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20610000 -o bin/ADVDRV.DLL src/advdrv/advdrv.c bin/advdrv.res src/advdrv/advdrv.def -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash ADVDRV bin/ADVDRV.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/ANALYSIS.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/analysis/analysis.rc -O coff -o bin/analysis.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/analysis -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20630000 -o bin/ANALYSIS.DLL src/analysis/analysis.c bin/analysis.res src/analysis/analysis.def -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash ANALYSIS bin/ANALYSIS.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/ATC.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/atc/atc.rc -O coff -o bin/atc.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/atc -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20640000 -o bin/ATC.DLL src/atc/atc.c bin/atc.res src/atc/atc.def -lmsvcrt -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash ATC bin/ATC.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/BOOSTER.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/booster/booster.rc -O coff -o bin/booster.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/booster -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20650000 -o bin/BOOSTER.DLL src/booster/booster.c bin/booster.res src/booster/booster.def -luser32 -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash BOOSTER bin/BOOSTER.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/CHAIN.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/chain/chain.rc -O coff -o bin/chain.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20660000 -o bin/CHAIN.DLL src/chain/chain.c bin/chain.res src/chain/chain.def -lkernel32]==]
  )

ofs_step(all
  OUTPUTS bin/CONTROLS.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/controls/controls.rc -O coff -o bin/controls.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/controls -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20670000 -o bin/CONTROLS.DLL src/controls/controls.c src/controls/controls_input.c bin/controls.res src/controls/controls.def -lkernel32 -luser32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash CONTROLS bin/CONTROLS.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/DEMO.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/demo/demo.rc -O coff -o bin/demo.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/demo -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20690000 -o bin/DEMO.DLL src/demo/demo.c bin/demo.res src/demo/demo.def -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash DEMO bin/DEMO.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/DYNAMIC.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/dynamic/dynamic.rc -O coff -o bin/dynamic.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/dynamic -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x206a0000 -o bin/DYNAMIC.DLL src/dynamic/dynamic.c bin/dynamic.res src/dynamic/dynamic.def -lkernel32]==]
    [==[python3 -B tools/check_data_image.py --hash DYNAMIC bin/DYNAMIC.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/FACIL.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/facil/facil.rc -O coff -o bin/facil.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/facil -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x206b0000 -o bin/FACIL.DLL src/facil/facil.c bin/facil.res src/facil/facil.def -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash FACIL bin/FACIL.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/FE.DLL
  DEPENDS_FILES src/fe/fe_library.c
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/fe/fe.rc -O coff -o bin/fe.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/fe -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x206c0000 -o bin/FE.DLL src/fe/fe.c bin/fe.res src/fe/fe.def -lkernel32]==]
    [==[python3 -B tools/check_data_image.py --hash FE bin/FE.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/G3D.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/g3d/g3d.rc -O coff -o bin/g3d.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/g3d -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x206e0000 -o bin/G3D.DLL src/g3d/g3d.c src/g3d/g3d_mesh.c bin/g3d.res src/g3d/g3d.def -lkernel32 -luser32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash G3D bin/G3D.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/GLOBAL.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/global/global.rc -O coff -o bin/global.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20720000 -o bin/GLOBAL.DLL src/global/global.c bin/global.res src/global/global.def -lkernel32]==]
    [==[python3 -B tools/check_data_image.py --hash GLOBAL bin/GLOBAL.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/HG2D.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/hg2d/hg2d.rc -O coff -o bin/hg2d.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20730000 -o bin/HG2D.DLL src/hg2d/hg2d.c bin/hg2d.res src/hg2d/hg2d.def -lgcc -lkernel32 -lgdi32]==]
    [==[python3 -B tools/check_data_image.py --hash HG2D bin/HG2D.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/LANDME.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/landme/landme.rc -O coff -o bin/landme.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/landme -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x207d0000 -o bin/LANDME.DLL src/landme/landme.c bin/landme.res src/landme/landme.def -lkernel32]==]
    [==[python3 -B tools/check_data_image.py --hash LANDME bin/LANDME.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/MAIN.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[python3 -B tools/gen_main_bitmap.py orig/Modules/MAIN.DLL bin/main_kneeboard.bmp]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres -Ibin src/main/main.rc -O coff -o bin/main.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/main -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x207e0000 -o bin/MAIN.DLL src/main/main.c bin/main.res src/main/main.def -lmsvcrt -lkernel32 -luser32 -lgdi32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash MAIN bin/MAIN.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/MPCHAT.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/mpchat/mpchat.rc -O coff -o bin/mpchat.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/mpchat -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20840000 -o bin/MPCHAT.DLL src/mpchat/mpchat.c bin/mpchat.res src/mpchat/mpchat.def -lkernel32 -luser32 -lgdi32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash MPCHAT bin/MPCHAT.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/MULTIP.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/multip/multip.rc -O coff -o bin/multip.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20850000 -o bin/MULTIP.DLL src/multip/multip.c bin/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32]==]
    [==[python3 -B tools/check_data_image.py --hash MULTIP bin/MULTIP.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/NUMERIC.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/numeric/numeric.rc -O coff -o bin/numeric.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DFS98_NUMERIC_MODULE -Isrc/numeric -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20870000 -o bin/NUMERIC.DLL src/numeric/numeric.c bin/numeric.res src/numeric/numeric.def -lmsvcrt -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash NUMERIC bin/NUMERIC.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/PANELS.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/panels/panels.rc -O coff -o bin/panels.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/panels -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x10000000 -o bin/PANELS.DLL src/panels/panels.c bin/panels.res src/panels/panels.def -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash PANELS bin/PANELS.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/REALITY.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/reality/reality.rc -O coff -o bin/reality.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/reality -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x208b0000 -o bin/REALITY.DLL src/reality/reality.c bin/reality.res src/reality/reality.def -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash REALITY bin/REALITY.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/SLEW.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/slew/slew.rc -O coff -o bin/slew.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x208c0000 -o bin/SLEW.DLL src/slew/slew.c bin/slew.res src/slew/slew.def -lkernel32]==]
  )

ofs_step(all
  OUTPUTS bin/SOUND.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/sound/sound.rc -O coff -o bin/sound.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/sound -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x208d0000 -o bin/SOUND.DLL src/sound/sound.c bin/sound.res src/sound/sound.def -lmsvcrt -luser32 -lkernel32]==]
    [==[python3 -B tools/check_data_image.py --hash SOUND bin/SOUND.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/UTIL.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/util/util.rc -O coff -o bin/util.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -DFS98_UTIL_MODULE -Isrc/util -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x208e0000 -o bin/UTIL.DLL src/util/util.c src/util/util_math.c src/util/util_files.c src/util/util_exchange.c src/util/util_alert.c bin/util.res src/util/util.def -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash UTIL bin/UTIL.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/WEATHERM.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/weatherm/weatherm.rc -O coff -o bin/weatherm.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/weatherm -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x208f0000 -o bin/WEATHERM.DLL src/weatherm/weatherm.c bin/weatherm.res src/weatherm/weatherm.def -lmsvcrt -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash WEATHERM bin/WEATHERM.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/WINDOW.DLL bin/window/window_text_tail.inc
  COMMANDS
    [==[mkdir -p bin/window]==]
    [==[python3 -B tools/gen_window_data.py orig/Modules/WINDOW.DLL bin/window/window_text_tail.inc]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/window/window.rc -O coff -o bin/window.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-ipa-icf -Isrc/window -Ibin/window -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20900000 -o bin/WINDOW.DLL src/window/window.c bin/window.res src/window/window.def -lkernel32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash WINDOW bin/WINDOW.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/WINMSG.DLL
  COMMANDS
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-windres src/winmsg/winmsg.rc -O coff -o bin/winmsg.res]==]
    [==[mkdir -p bin]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -Isrc/winmsg -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20910000 -o bin/WINMSG.DLL src/winmsg/winmsg.c bin/winmsg.res src/winmsg/winmsg.def -lkernel32 -luser32 -lgcc]==]
    [==[python3 -B tools/check_data_image.py --hash WINMSG bin/WINMSG.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/WORLD.DLL
  COMMANDS
    [==[mkdir -p bin/world-diff]==]
    [==[python3 -B tools/gen_world_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/world-diff/world.res bin/world-diff/world-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/world-diff/simsui-anchor.def -l bin/world-diff/libworld-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x223f0000 -o bin/world-diff/WORLD_area.dll @WORLD_SOURCES@ src/world/world.def bin/world-diff/world-res.o bin/world-diff/libworld-simsui.a -lkernel32 -luser32 -lmsvcrt -lntdll]==]
    [==[cmp -s bin/world-diff/WORLD_area.dll bin/WORLD.DLL || cp bin/world-diff/WORLD_area.dll bin/WORLD.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/ALERTS.DLL
  COMMANDS
    [==[mkdir -p bin/alerts-diff]==]
    [==[python3 -B tools/gen_alerts_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/alerts-diff/alerts.res bin/alerts-diff/alerts-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/alerts-diff/simsui-anchor.def -l bin/alerts-diff/libalerts-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22300000 -o bin/alerts-diff/ALERTS_ours.dll src/alerts/alerts_events.c src/alerts/alerts_objects.c src/alerts/alerts_runtime.c src/alerts/alerts.def bin/alerts-diff/alerts-res.o bin/alerts-diff/libalerts-simsui.a -lkernel32 -luser32 -lgdi32 -lmsvcrt]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/alerts-diff/ALERTS_ours.dll bin/ALERTS.DLL || cp bin/alerts-diff/ALERTS_ours.dll bin/ALERTS.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/HELP.DLL
  COMMANDS
    [==[mkdir -p bin/help-diff]==]
    [==[python3 -B tools/gen_help_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/help-diff/help.res bin/help-diff/help-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/help-diff/simsui-anchor.def -l bin/help-diff/libhelp-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22330000 -o bin/help-diff/HELP_ours.dll src/help/help_events.c src/help/help_objects.c src/help/help_stack.S src/help/help_runtime.c src/help/help.def bin/help-diff/help-res.o bin/help-diff/libhelp-simsui.a -lkernel32 -luser32 -ladvapi32 -lcomctl32 -lmsvcrt]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/help-diff/HELP_ours.dll bin/HELP.DLL || cp bin/help-diff/HELP_ours.dll bin/HELP.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/SIM.DLL
  COMMANDS
    [==[mkdir -p bin/sim-diff]==]
    [==[python3 -B tools/gen_sim_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/sim-diff/sim.res bin/sim-diff/sim-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/sim-diff/simsui-anchor.def -l bin/sim-diff/libsim-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x223c0000 -o bin/sim-diff/SIM_ours.dll src/sim/sim_events.c src/sim/sim_objects.c src/sim/sim_crash.c src/sim/sim_engines.c src/sim/sim_systems.c src/sim/sim_realism.c src/sim/sim_fuel.c src/sim/sim_settings.c src/sim/sim_runtime.c src/sim/sim.def bin/sim-diff/sim-res.o bin/sim-diff/libsim-simsui.a -lkernel32 -luser32 -lmsvcrt]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/sim-diff/SIM_ours.dll bin/SIM.DLL || cp bin/sim-diff/SIM_ours.dll bin/SIM.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/VIEWS.DLL
  COMMANDS
    [==[mkdir -p bin/views-diff]==]
    [==[python3 -B tools/gen_views_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/views-diff/views.res bin/views-diff/views-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/views-diff/simsui-anchor.def -l bin/views-diff/libviews-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x223e0000 -o bin/views-diff/VIEWS_ours.dll src/views/views_events.c src/views/views_objects.c src/views/views_stack.S src/views/views_runtime.c src/views/views.def bin/views-diff/views-res.o bin/views-diff/libviews-simsui.a -lkernel32 -luser32 -lmsvcrt]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/views-diff/VIEWS_ours.dll bin/VIEWS.DLL || cp bin/views-diff/VIEWS_ours.dll bin/VIEWS.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/FLIGHTS.DLL
  COMMANDS
    [==[mkdir -p bin/flights-diff]==]
    [==[python3 -B tools/gen_flights_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/flights-diff/flights.res bin/flights-diff/flights-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/flights-diff/simsui-anchor.def -l bin/flights-diff/libflights-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22310000 -o bin/flights-diff/FLIGHTS_ours.dll src/flights/flights_events.c src/flights/flights_objects.c src/flights/flights_save.c src/flights/flights_comms.c src/flights/flights_create.c src/flights/flights_aircraft.c src/flights/flights_opening.c src/flights/flights_select.c src/flights/flights_autopilot.c src/flights/flights_navigation.c src/flights/flights_runtime.c src/flights/flights.def bin/flights-diff/flights-res.o bin/flights-diff/libflights-simsui.a -lkernel32 -luser32 -lmsvcrt]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/flights-diff/FLIGHTS_ours.dll bin/FLIGHTS.DLL || cp bin/flights-diff/FLIGHTS_ours.dll bin/FLIGHTS.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/MP_DIALOG.DLL
  COMMANDS
    [==[python3 -B tools/gen_mpdialog_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-0x110b.res bin/mpdialog-diff/mpdialog-res.o]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-0x110c.res bin/mpdialog-diff/mpdialog-details-res.o]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-0x110a-intro.res bin/mpdialog-diff/mpdialog-flow-intro-res.o]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-0x110a-choice.res bin/mpdialog-diff/mpdialog-flow-choice-res.o]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-0x110a-host.res bin/mpdialog-diff/mpdialog-flow-host-res.o]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-0x110a-connection.res bin/mpdialog-diff/mpdialog-flow-connection-res.o]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-0x110a-browser.res bin/mpdialog-diff/mpdialog-flow-browser-res.o]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/mpdialog-diff/mpdialog-flow-support.res bin/mpdialog-diff/mpdialog-flow-support-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/mpdialog-diff/simsui-anchor.def -l bin/mpdialog-diff/libmpdialog-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22340000 -o bin/mpdialog-diff/MP_DIALOG_ours.dll src/mpdialog/mpdialog_flow.c src/mpdialog/mpdialog_session.c src/mpdialog/mpdialog_details.c src/mpdialog/mpdialog_runtime.c src/mpdialog/mpdialog.def bin/mpdialog-diff/mpdialog-res.o bin/mpdialog-diff/mpdialog-details-res.o bin/mpdialog-diff/mpdialog-flow-intro-res.o bin/mpdialog-diff/mpdialog-flow-choice-res.o bin/mpdialog-diff/mpdialog-flow-host-res.o bin/mpdialog-diff/mpdialog-flow-connection-res.o bin/mpdialog-diff/mpdialog-flow-browser-res.o bin/mpdialog-diff/mpdialog-flow-support-res.o bin/mpdialog-diff/libmpdialog-simsui.a -lkernel32 -luser32 -lcomctl32 -lmsvcrt]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/mpdialog-diff/MP_DIALOG_ours.dll bin/MP_DIALOG.DLL || cp bin/mpdialog-diff/MP_DIALOG_ours.dll bin/MP_DIALOG.DLL]==]
  )

ofs_step(all
  OUTPUTS bin/OPTIONS.DLL
  COMMANDS
    [==[mkdir -p bin/options-diff]==]
    [==[python3 -B tools/gen_options_runtime.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/options-diff/options.res bin/options-diff/options-res.o]==]
    [==[i686-w64-mingw32-dlltool -d bin/options-diff/simsui-anchor.def -l bin/options-diff/liboptions-simsui.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22390000 -o bin/options-diff/OPTIONS_ours.dll src/options/options_events.c src/options/options_objects.c src/options/options_video.c src/options/options_videos.c src/options/options_logbook.c src/options/options_book.c src/options/options_graphs.c src/options/options_analysis.c src/options/options_replay.c src/options/options_demo.c src/options/options_display.c src/options/options_prefs.c src/options/options_controls.c src/options/options_keys.c src/options/options_runtime.c src/options/options.def bin/options-diff/options-res.o bin/options-diff/liboptions-simsui.a -lkernel32 -luser32 -lmsvcrt]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/options-diff/OPTIONS_ours.dll bin/OPTIONS.DLL || cp bin/options-diff/OPTIONS_ours.dll bin/OPTIONS.DLL]==]
  )

# one compile per SIMSUI unit of src/simsui/sources.txt (OFS_SIMSUI_SOURCES, cmake/OpenFS98.cmake)
set(simsui_units "")
foreach(s IN LISTS OFS_SIMSUI_SOURCES)
  get_filename_component(n "${s}" NAME_WE)
  list(APPEND simsui_units "i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -fno-tree-loop-distribute-patterns -fno-tree-vectorize -c -o bin/simsui-native/${n}.o ${s}")
endforeach()
ofs_step(all
  OUTPUTS bin/SIMSUI.DLL
  COMMANDS
    [==[python3 -B tools/gen_simsui_native.py]==]
    [==[mkdir -p bin/simsui-native]==]
    ${simsui_units}
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -fno-tree-loop-distribute-patterns -fno-tree-vectorize -c -o bin/simsui-native/simsui_native.o src/simsui/simsui_native.c]==]
    [==[python3 -B tools/gen_simsui_native.py @SIMSUI_OBJECTS@ bin/simsui-native/simsui_native.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -fno-ident -fno-tree-loop-distribute-patterns -fno-tree-vectorize -Ibin/simsui-native -c -o bin/simsui-native/simsui_image.o src/simsui/simsui_image.c]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/simsui-native/simsui.res bin/simsui-native/simsui-res.o]==]
    [==[printf 'LIBRARY MFC42.DLL\nEXPORTS\n    fs98_simsui_mfc42_anchor@0 @1168 NONAME\n' > bin/simsui-native/mfc42-anchor.def]==]
    [==[i686-w64-mingw32-dlltool -d bin/simsui-native/mfc42-anchor.def -l bin/simsui-native/libsimsui-mfc42.a]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -shared -nostdlib -Wl,--entry=_fs98_simsui_entry@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x20470000 -o bin/simsui-native/SIMSUI_ours.dll @SIMSUI_OBJECTS@ bin/simsui-native/simsui_native.o bin/simsui-native/simsui_image.o bin/simsui-native/simsui-res.o bin/simsui-native/simsui.def bin/simsui-native/libsimsui-mfc42.a -lkernel32 -luser32 -lgdi32 -limm32 -lmsvcrt -lgcc]==]
    [==[mkdir -p bin]==]
    [==[cmp -s bin/simsui-native/SIMSUI_ours.dll bin/SIMSUI.DLL || cp bin/simsui-native/SIMSUI_ours.dll bin/SIMSUI.DLL]==]
    [==[python3 -B tools/check_data_image.py --hash SIMSUI bin/SIMSUI.DLL]==]
  )

# the 120 gauges' rows of tools/data_images.txt, as MODULE DLL pairs for one --hash run (the script's own parser; it fails
# unless the rows are exactly the gauges of src/gauges/gauges_manifest.json)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/data_images.txt" "${CMAKE_SOURCE_DIR}/src/gauges/gauges_manifest.json")
execute_process(COMMAND python3 -B tools/check_data_image.py --gauge-pairs src/gauges/gauges_manifest.json
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE OFS_GAUGE_PAIRS ERROR_VARIABLE OFS_GAUGE_ERR RESULT_VARIABLE OFS_GAUGE_RC)
if(NOT OFS_GAUGE_RC EQUAL 0)
  message(FATAL_ERROR "tools/data_images.txt gauge rows: ${OFS_GAUGE_ERR}")
endif()
string(STRIP "${OFS_GAUGE_PAIRS}" OFS_GAUGE_PAIRS)

ofs_step(all
  OUTPUTS bin/gauges-all/built.stamp bin/gauges-all/all_gauges.inc bin/gauges-all/all_panels.inc
  DEPENDS_GLOB src/gauges/*.c src/gauges/*.h include/fs98/arena.h include/fs98/abi.h src/gauges/images/* src/gauges/gauges_manifest.json tools/data_images.txt orig/Gauges/*.GAU docs/setup_map.tsv
  COMMANDS
    [==[python3 -B tools/gen_gauges.py]==]
    [==[touch bin/gauges-all/generated.stamp]==]
    [==[python3 -B tools/build_gauges.py --jobs 2]==]
    # exit 3: UPDATE=1 only warned about a mismatch (the DLL's mtime is reset): age the stamp, so the next build reruns the step
    "python3 -B tools/check_data_image.py --hash ${OFS_GAUGE_PAIRS}@SEMI@ rc=$? @SEMI@ if [ $rc -eq 0 ]@SEMI@ then touch bin/gauges-all/built.stamp@SEMI@ elif [ $rc -eq 3 ]@SEMI@ then touch -d @0 bin/gauges-all/built.stamp@SEMI@ else exit $rc@SEMI@ fi"
  )

# the eleven MDLs' rows of tools/data_images.txt, as MODULE DLL pairs for one --hash run
set(OFS_MDL_NAMES BOEING737-400_N BELL_206B_JETRANGER_N CESSNA182RRG_N CESSNA182S_N EXTRA300S_N LEAR45_N SCHWEIZER2_32_N SOPWITHCAMEL_N)
set(OFS_MDL_PAIRS "")
set(OFS_MDL_DLLS "")
foreach(n IN LISTS OFS_MDL_NAMES)
  string(APPEND OFS_MDL_PAIRS "${n} bin/mdl-diff/models/${n}.MDL ")
  list(APPEND OFS_MDL_DLLS bin/mdl-diff/models/${n}.MDL)
endforeach()
foreach(n IN ITEMS CESSNA182RRG_C CESSNA182S_C EXTRA300S_C)
  string(APPEND OFS_MDL_PAIRS "${n} bin/mdl-crash/models/${n}.MDL ")
  list(APPEND OFS_MDL_DLLS bin/mdl-crash/models/${n}.MDL)
endforeach()
string(STRIP "${OFS_MDL_PAIRS}" OFS_MDL_PAIRS)

# the models' data is committed C (src/mdl/images/<NAME>/ built by src/mdl/mdl_image.c, src/mdl/mdl_manifest.json); retail's resources only come from orig/
ofs_step(all
  OUTPUTS bin/mdl-diff/modules.json bin/mdl-crash/modules.json bin/mdl-diff/modules.inc bin/mdl-crash/crashes.inc ${OFS_MDL_DLLS}
  DEPENDS_GLOB src/mdl/* tools/data_images.txt orig/Aircraft/*.MDL
  COMMANDS
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/build_mdls.py]==]
    [==[PYTHONDONTWRITEBYTECODE=1 python3 tools/gen_mdl_crash.py]==]
    # a mismatch (exit 1) or one UPDATE=1 only warned about (exit 3, the DLL's mtime is reset) ages both outputs, so the next build reruns the step
    "python3 -B tools/check_data_image.py --hash ${OFS_MDL_PAIRS}@SEMI@ rc=$? @SEMI@ if [ $rc -ne 0 ]@SEMI@ then touch -d @0 bin/mdl-diff/modules.json bin/mdl-crash/modules.json bin/mdl-diff/modules.inc bin/mdl-crash/crashes.inc@SEMI@ fi@SEMI@ if [ $rc -ne 0 ] && [ $rc -ne 3 ]@SEMI@ then exit $rc@SEMI@ fi"
  )

ofs_step(all
  OUTPUTS bin/SIM1.sim
  COMMANDS
    [==[python3 -B tools/gen_sim1_native.py]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/sim1-native/sim1.res bin/sim1-native/sim1-res.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Werror -fno-math-errno -fno-tree-loop-distribute-patterns -fno-tree-vectorize -Iinclude -Isrc/sim1 -Ibin/sim1-native -shared -nostdlib -Wl,--entry=_sim1_native_entry@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base,0x22000000 -o bin/sim1-native/SIM1_ours.sim src/sim1/sim1_native.c src/sim1/sim1_small.c bin/sim1-native/sim1-res.o src/sim1/sim1.def -lmsvcrt -lkernel32 -lgcc]==]
    [==[cmp -s bin/sim1-native/SIM1_ours.sim bin/SIM1.sim || cp bin/sim1-native/SIM1_ours.sim bin/SIM1.sim]==]
    [==[python3 -B tools/check_data_image.py --hash SIM1 bin/SIM1.sim]==]
  )

ofs_step(all
  OUTPUTS bin/OPENFS98.EXE
  COMMANDS
    [==[mkdir -p bin/host-rt]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/host -masm=intel -ffreestanding -fno-builtin -DHOST_IMAGE_SHARED -c src/host/host.c -o bin/host-rt/host_services.o]==]
    [==[python3 -B tools/gen_host_resources.py bin/host-rt/host.res]==]
    [==[i686-w64-mingw32-windres -J res -O coff bin/host-rt/host.res bin/host-rt/host_resources.o]==]
    [==[i686-w64-mingw32-gcc @MODULE_FLAGS@ -std=c99 -O2 -Wall -Wextra -Isrc/host/runtime -Iinclude -mwindows -Wl,--disable-dynamicbase -Wl,--image-base,0x43000000 -o bin/host-rt/OPENFS98.EXE src/host/host_rt_exe.c src/host/runtime/host_rt_child.c src/host/runtime/host_rt_common.c src/host/runtime/host_rt_config.c src/host/runtime/host_rt_cursor.c src/host/runtime/host_rt_ddraw.c src/host/runtime/host_rt_display.c src/host/runtime/host_rt_divide.c src/host/runtime/host_rt_floating.c src/host/runtime/host_rt_frame.c src/host/runtime/host_rt_fullscreen.c src/host/runtime/host_rt_layout.c src/host/runtime/host_rt_main.c src/host/runtime/host_rt_misc.c src/host/runtime/host_rt_module_load.c src/host/runtime/host_rt_modules.c src/host/runtime/host_rt_mouse.c src/host/runtime/host_rt_startup.c src/host/runtime/host_rt_surface.c src/host/runtime/host_rt_window.c bin/host-rt/host_services.o bin/host-rt/host_resources.o -lmsvcrt -lgdi32 -luser32 -lwinmm -ladvapi32 -lmsacm32 -lddraw -lkernel32]==]
    [==[cp bin/host-rt/OPENFS98.EXE bin/OPENFS98.EXE]==]
    [==[python3 -B tools/check_data_image.py --hash HOST bin/OPENFS98.EXE]==]
  )
