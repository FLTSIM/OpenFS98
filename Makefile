# OpenFS98 -- hybrid decompilation of Microsoft Flight Simulator 98.
#
# Each target is a drop-in replacement for one module in the retail Modules/
# directory.  The retail FLTSIM98.EXE loads whatever is there, so a rebuilt
# module can be swapped in and tested against the real game immediately.
#
#   make                 build every reimplemented module into bin/
#   make install         copy bin/*.DLL over a game install (see FS98_DIR)
#   make graph           print the module id/dependency table from orig/
#   make compare         size-compare each built module against the retail one
#   make run             launch the game under wine (reports which modules are in)
#   make stage           build game/ from the CD (applies setup's renames)
#   make smoke           boot the game and assert it reaches the panel
#   make difftest-run    diff a rebuilt module against retail on synthetic state
#
# Needs a 32-bit mingw-w64 toolchain:
#   sudo apt install gcc-mingw-w64-i686-win32

CC      := i686-w64-mingw32-gcc
RC      := i686-w64-mingw32-windres
OBJDUMP := i686-w64-mingw32-objdump
CFLAGS  := -std=c99 -Os -Wall -Wextra -Iinclude -ffreestanding \
           -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer
# -nostdlib: no CRT, so DllMain is the entry point and has to be named as such
#            explicitly -- otherwise ld leaves AddressOfEntryPoint at 0 and the
#            module's DllMain is never called.
# dynamicbase/nxcompat off: the retail modules have DllCharacteristics 0, are
#            based at fixed non-default addresses, and pass raw pointers between
#            each other through Linkage.  Match that.
LDFLAGS := -shared -nostdlib -Wl,--enable-stdcall-fixup \
           -Wl,--entry=_DllMain@12 \
           -Wl,--disable-dynamicbase -Wl,--disable-nxcompat

ORIG    := orig
BIN     := bin
FS98_DIR?= $(HOME)/.wine/drive_c/FS98

# module name -> retail image base, so our build maps where the original did
BASE_ACONTAIN := 0x20600000
BASE_ADVDRV := 0x20610000
BASE_ATC    := 0x20640000
BASE_BOOSTER := 0x20650000
BASE_CHAIN  := 0x20660000
BASE_DEMO   := 0x20690000
BASE_DYNAMIC := 0x206a0000
BASE_FACIL  := 0x206b0000
BASE_FE     := 0x206c0000
BASE_LANDME := 0x207d0000
BASE_GLOBAL := 0x20720000
BASE_NUMERIC := 0x20870000
BASE_SLEW   := 0x208c0000
BASE_SOUND  := 0x208d0000
BASE_UTIL   := 0x208e0000
BASE_WEATHERM := 0x208f0000
BASE_WINDOW := 0x20900000
BASE_WINMSG := 0x20910000
BASE_REALITY := 0x208b0000
BASE_ANALYSIS := 0x20630000
BASE_CONTROLS := 0x20670000
BASE_G3D    := 0x206e0000
BASE_MULTIP := 0x20850000
BASE_MPCHAT := 0x20840000
BASE_MAIN   := 0x207e0000

MODULES := ACONTAIN ADVDRV ANALYSIS ATC BOOSTER CHAIN CONTROLS DEMO DYNAMIC FACIL FE G3D GLOBAL LANDME MPCHAT MULTIP NUMERIC PANELS REALITY SLEW SOUND UTIL WEATHERM WINDOW WINMSG

# MULTIP installs as MULTIPLAYER.DLL; its acceptance harness is below.
$(BIN)/MULTIP.DLL: src/multip/multip.c src/multip/multip_callbacks.inc src/multip/multip_motion.inc src/multip/multip_commands.inc src/multip/multip_weather.inc src/multip/multip_tick.inc src/multip/multip_system.inc src/multip/multip_app_players.inc src/multip/multip_updates.inc src/multip/multip_textures.inc src/multip/multip_texture_receive.inc src/multip/multip_transfer_messages.inc src/multip/multip_texture_send.inc src/multip/multip_model_send.inc src/multip/multip_model_cache.inc src/multip/multip_model_receive.inc src/multip/multip_title_receive.inc src/multip/multip_state_messages.inc src/multip/multip_control_updates.inc src/multip/multip_lifecycle.inc src/multip/multip_dispatch.inc src/multip/multip_dispatch_cases.inc src/multip/multip_weather_fields.inc src/multip/multip_frame_actions.inc src/multip/multip_players.inc src/multip/multip_nametag.inc src/multip/multip_messages.inc src/multip/multip_tracking.inc src/multip/multip_records.inc src/multip/multip_aircraft.inc src/multip/multip_session.inc src/multip/multip_profile.inc src/multip/multip_profile_fields.inc src/multip/multip.def src/multip/multip_data.inc src/multip/multip_rdata.inc src/multip/multip_pointers.inc $(BIN)/multip.res src/multip/multip_providers.inc src/multip/multip_browser.inc src/multip/multip_configuration.inc src/multip/multip_details.inc src/multip/multip_options.inc src/multip/multip_lobby.inc src/multip/multip_toggle.inc src/multip/multip_init.inc
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--image-base=$(BASE_MULTIP) \
	      -o $@ src/multip/multip.c $(BIN)/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32

src/multip/multip_dispatch_cases.inc: $(ORIG)/Modules/multip.dll tools/gen_multip_dispatch.py src/multip/multip.c tools/difftest/multip_send.inc
	python3 -B tools/gen_multip_dispatch.py

src/multip/multip_weather_fields.inc: $(ORIG)/Modules/multip.dll tools/gen_multip_weather.py
	@python3 tools/gen_multip_weather.py

src/multip/multip_data.inc src/multip/multip_rdata.inc src/multip/multip_pointers.inc src/multip/multip_nametag.inc src/multip/multip_frame_actions.inc &: $(ORIG)/Modules/multip.dll tools/gen_multip_data.py
	@python3 tools/gen_multip_data.py

src/multip/multip_profile_fields.inc: $(ORIG)/Modules/multip.dll tools/gen_multip_profile.py
	@python3 tools/gen_multip_profile.py

$(BIN)/multip.res: src/multip/multip.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

# Setup renames two modules on the way from CD to install, and the game loads
# the *installed* names.  A rebuilt FACIL.DLL copied in as FACIL.DLL would just
# sit there unloaded, so installs go through this map.
installed_name = $(strip \
  $(if $(filter FACIL,$(1)),FACILITIES.DLL,\
  $(if $(filter MULTIP,$(1)),MULTIPLAYER.DLL,\
  $(if $(filter MPCHAT,$(1)),mpchat.DLL,$(1).DLL))))

# The CD's own file names are not all upper case either: facil.dll, multip.dll.
orig_module = $(ORIG)/Modules/$(strip \
  $(if $(filter FACIL,$(1)),facil.dll,\
  $(if $(filter MULTIP,$(1)),multip.dll,\
  $(if $(filter MPCHAT,$(1)),mpchat.DLL,$(1).DLL))))

all: $(addprefix $(BIN)/,$(addsuffix .DLL,$(MODULES)))

$(BIN)/GLOBAL.DLL: src/global/global.c src/global/global.def src/global/global_state.inc $(BIN)/global.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--image-base=$(BASE_GLOBAL) \
	      -o $@ src/global/global.c $(BIN)/global.res src/global/global.def -lkernel32

$(BIN)/global.res: src/global/global.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

$(BIN)/CHAIN.DLL: src/chain/chain.c src/chain/chain.def $(BIN)/chain.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--image-base=$(BASE_CHAIN) \
	      -o $@ src/chain/chain.c $(BIN)/chain.res src/chain/chain.def -lkernel32

$(BIN)/chain.res: src/chain/chain.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

# ATC keeps ImportTable, Linkage and its state in one image laid out as
# retail's .data; the exports are aliases into it.  sscanf comes from MSVCRT,
# as retail's does.
$(BIN)/ATC.DLL: src/atc/atc.c src/atc/atc.def src/atc/atc_data.inc $(BIN)/atc.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -Isrc/atc $(LDFLAGS) -Wl,--image-base=$(BASE_ATC) \
	      -o $@ src/atc/atc.c $(BIN)/atc.res src/atc/atc.def -lmsvcrt -lkernel32 -lgcc

$(BIN)/atc.res: src/atc/atc.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/atc/atc_data.inc: $(ORIG)/Modules/ATC.DLL tools/gen_atc_data.py
	@python3 tools/gen_atc_data.py $< $@

# WEATHERM, laid out the same way.  MSVCRT for whatever block copies gcc emits
# as memcpy, libgcc for the 64-bit divisions.
$(BIN)/WEATHERM.DLL: src/weatherm/weatherm.c src/weatherm/weatherm.def \
                     src/weatherm/weatherm_data.inc $(BIN)/weatherm.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -Isrc/weatherm $(LDFLAGS) -Wl,--image-base=$(BASE_WEATHERM) \
	      -o $@ src/weatherm/weatherm.c $(BIN)/weatherm.res src/weatherm/weatherm.def \
	      -lmsvcrt -lkernel32 -lgcc

$(BIN)/weatherm.res: src/weatherm/weatherm.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/weatherm/weatherm_data.inc: $(ORIG)/Modules/WEATHERM.DLL tools/gen_weatherm_data.py
	@python3 tools/gen_weatherm_data.py $< $@

# ACONTAIN, laid out the same way.  USER32 for wsprintfA, which retail calls
# directly; MSVCRT for whatever block copies gcc emits as memcpy.
$(BIN)/ACONTAIN.DLL: src/acontain/acontain.c src/acontain/acontain.def \
                     src/acontain/acontain_data.inc src/acontain/acontain_colours.inc \
                     $(BIN)/acontain.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -Isrc/acontain $(LDFLAGS) -Wl,--image-base=$(BASE_ACONTAIN) \
	      -o $@ src/acontain/acontain.c $(BIN)/acontain.res src/acontain/acontain.def \
	      -lmsvcrt -luser32 -lkernel32

$(BIN)/acontain.res: src/acontain/acontain.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/acontain/acontain_data.inc src/acontain/acontain_colours.inc &: $(ORIG)/Modules/ACONTAIN.DLL tools/gen_acontain_data.py
	@python3 tools/gen_acontain_data.py $< src/acontain

# SOUND, laid out the same way.  USER32 for wsprintfA and MSVCRT for strtok,
# both of which retail imports directly.
$(BIN)/SOUND.DLL: src/sound/sound.c src/sound/sound.def \
                  src/sound/sound_data.inc src/sound/sound_consts.inc $(BIN)/sound.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -Isrc/sound $(LDFLAGS) -Wl,--image-base=$(BASE_SOUND) \
	      -o $@ src/sound/sound.c $(BIN)/sound.res src/sound/sound.def \
	      -lmsvcrt -luser32 -lkernel32

$(BIN)/sound.res: src/sound/sound.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/sound/sound_data.inc src/sound/sound_consts.inc &: $(ORIG)/Modules/SOUND.DLL tools/gen_sound_data.py
	@python3 tools/gen_sound_data.py $< src/sound

# DYNAMIC, laid out the same way.  An interpreter for programs from scenery
# files, so null-pointer checks stay in: a bad program has to fault where
# retail's does, not wherever the optimiser moved the access.
$(BIN)/DYNAMIC.DLL: src/dynamic/dynamic.c src/dynamic/dynamic.def \
                    src/dynamic/dynamic_data.inc $(BIN)/dynamic.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/dynamic $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_DYNAMIC) \
	      -o $@ src/dynamic/dynamic.c $(BIN)/dynamic.res src/dynamic/dynamic.def -lkernel32

$(BIN)/dynamic.res: src/dynamic/dynamic.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/dynamic/dynamic_data.inc: $(ORIG)/Modules/DYNAMIC.DLL tools/gen_dynamic_data.py
	@python3 tools/gen_dynamic_data.py $< $@

# FE, the scenery file engine, laid out the same way: the assembly engine and
# file services (fediff), and the Scenery Library in fe_library.c (fesldiff).
$(BIN)/FE.DLL: src/fe/fe.c src/fe/fe_library.c src/fe/fe.def src/fe/fe_data.inc $(BIN)/fe.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/fe $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_FE) \
	      -o $@ src/fe/fe.c $(BIN)/fe.res src/fe/fe.def -lkernel32

$(BIN)/fe.res: src/fe/fe.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/fe/fe_data.inc: $(ORIG)/Modules/FE.DLL tools/gen_fe_data.py
	@python3 tools/gen_fe_data.py $< $@

# FACIL, the compiled facility index and its three dialogs' callbacks.  Every
# Linkage slot is verified by facildiff.
$(BIN)/FACIL.DLL: src/facil/facil.c src/facil/facil.def \
                   src/facil/facil_data.inc $(BIN)/facil.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -Isrc/facil $(LDFLAGS) -Wl,--image-base=$(BASE_FACIL) \
	      -o $@ src/facil/facil.c $(BIN)/facil.res src/facil/facil.def \
	      -lkernel32 -lgcc

$(BIN)/facil.res: src/facil/facil.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/facil/facil_data.inc: $(ORIG)/Modules/facil.dll tools/gen_facil_data.py
	@python3 tools/gen_facil_data.py $< $@

# BOOSTER, the CD scenery cache and its copying thread.  USER32 for the
# thread's message loop, MessageBoxA and wsprintfA, all of which retail calls
# directly; MSVCRT's operator new/delete are bound by name at init; libgcc for
# _ftol's 64-bit conversion.
$(BIN)/BOOSTER.DLL: src/booster/booster.c src/booster/booster.def \
                    src/booster/booster_data.inc $(BIN)/booster.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/booster $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_BOOSTER) \
	      -o $@ src/booster/booster.c $(BIN)/booster.res src/booster/booster.def \
	      -luser32 -lkernel32 -lgcc

$(BIN)/booster.res: src/booster/booster.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/booster/booster_data.inc: $(ORIG)/Modules/BOOSTER.DLL tools/gen_booster_data.py
	@python3 tools/gen_booster_data.py $< $@

# ADVDRV, the adventure and lesson interpreter: MSVC C with the x87
# throughout.  libgcc for _ftol's 64-bit conversion.  Its tables hold distinct
# routines with identical bodies, so identical-code folding is off.
$(BIN)/ADVDRV.DLL: src/advdrv/advdrv.c src/advdrv/advdrv.def src/advdrv/advdrv_data.inc \
                   $(BIN)/advdrv.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/advdrv $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_ADVDRV) \
	      -o $@ src/advdrv/advdrv.c $(BIN)/advdrv.res src/advdrv/advdrv.def \
	      -lkernel32 -lgcc

$(BIN)/advdrv.res: src/advdrv/advdrv.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/advdrv/advdrv_data.inc: $(ORIG)/Modules/ADVDRV.DLL tools/gen_advdrv_data.py
	@python3 tools/gen_advdrv_data.py $< $@

# DEMO, flight videos and instant replay: an assembly recorder and player
# with a C half of dialog callbacks.  libgcc for the 64-bit divisions.  The
# image's tables hold distinct routines that do nothing; identical-code folding
# would make them one, so it is off.
$(BIN)/DEMO.DLL: src/demo/demo.c src/demo/demo.def src/demo/demo_data.inc $(BIN)/demo.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/demo $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_DEMO) \
	      -o $@ src/demo/demo.c $(BIN)/demo.res src/demo/demo.def \
	      -lkernel32 -lgcc

$(BIN)/demo.res: src/demo/demo.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/demo/demo_data.inc: $(ORIG)/Modules/DEMO.DLL tools/gen_demo_data.py
	@python3 tools/gen_demo_data.py $< $@

# WINDOW, the view windows and the eyepoint: MSVC C with an assembly half.
# libgcc for the 64-bit arithmetic; identical-code folding is off so the
# Linkage's distinct services stay distinct.
$(BIN)/WINDOW.DLL: src/window/window.c src/window/window.def src/window/window_data.inc \
                   src/window/window_text.inc $(BIN)/window.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/window $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_WINDOW) \
	      -o $@ src/window/window.c $(BIN)/window.res src/window/window.def \
	      -lkernel32 -lgcc

# G3D: the asm transliterated in g3d.c, the mesh compiler (0x38) carried over
# as instructions in g3d_mesh.S; g3d_mesh.c is its portable C (see
# docs/modules/g3d.md and g3ddiff-meshc-run).
$(BIN)/G3D.DLL: src/g3d/g3d.c src/g3d/g3d_mesh.S src/g3d/g3d.def src/g3d/g3d_data.inc \
                src/g3d/g3d_pointers.inc $(BIN)/g3d.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/g3d $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_G3D) \
	      -o $@ src/g3d/g3d.c src/g3d/g3d_mesh.S $(BIN)/g3d.res src/g3d/g3d.def \
	      -lkernel32 -luser32 -lgcc

src/g3d/g3d_data.inc src/g3d/g3d_pointers.inc: $(ORIG)/Modules/G3D.DLL tools/gen_g3d_data.py
	@python3 tools/gen_g3d_data.py $< src/g3d/g3d_data.inc src/g3d/g3d_pointers.inc

# The mesh compiler (0x38), x87 compiled C carried over instruction for
# instruction (tools/gen_g3d_mesh.py says why).
src/g3d/g3d_mesh.S: $(ORIG)/Modules/G3D.DLL tools/gen_g3d_mesh.py
	@python3 tools/gen_g3d_mesh.py $< $@

$(BIN)/g3d.res: src/g3d/g3d.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

# PANELS: its own image base (0x10000000, as retail's), and the MSVCRT and GDI
# calls retail makes.
BASE_PANELS := 0x10000000

$(BIN)/PANELS.DLL: src/panels/panels.c src/panels/panels.h src/panels/panels.def \
                   src/panels/panels_data.inc src/panels/panels_rdata.inc \
                   src/panels/panels_pointers.inc $(BIN)/panels.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/panels $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_PANELS) \
	      -o $@ src/panels/panels.c $(BIN)/panels.res src/panels/panels.def \
	      -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc

src/panels/panels_data.inc src/panels/panels_rdata.inc src/panels/panels_pointers.inc: \
    $(ORIG)/Modules/PANELS.DLL tools/gen_panels_data.py
	@python3 tools/gen_panels_data.py $< src/panels/panels_data.inc \
	      src/panels/panels_rdata.inc src/panels/panels_pointers.inc

$(BIN)/panels.res: src/panels/panels.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

$(BIN)/window.res: src/window/window.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/window/window_data.inc src/window/window_text.inc: $(ORIG)/Modules/WINDOW.DLL tools/gen_window_data.py
	@python3 tools/gen_window_data.py $< src/window/window_data.inc src/window/window_text.inc

# REALITY, crashes, damage and the crash analysis: assembly and MSVC C.
$(BIN)/REALITY.DLL: src/reality/reality.c src/reality/reality.def src/reality/reality_data.inc \
                    src/reality/reality_rdata.inc $(BIN)/reality.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/reality $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_REALITY) \
	      -o $@ src/reality/reality.c $(BIN)/reality.res src/reality/reality.def \
	      -lkernel32 -lgcc

$(BIN)/reality.res: src/reality/reality.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/reality/reality_data.inc src/reality/reality_rdata.inc: $(ORIG)/Modules/REALITY.DLL tools/gen_reality_data.py
	@python3 tools/gen_reality_data.py $< src/reality/reality_data.inc src/reality/reality_rdata.inc

# CONTROLS, the keyboard, joysticks and mouse: a hand-written assembly half
# (controls.c) and a compiled-C half (controls_input.c), both transliterated
# and verified whole by controlsdiff.
CONTROLS_INC := src/controls/controls_data.inc src/controls/controls_rdata.inc \
                src/controls/controls_table.inc src/controls/controls_pointers.inc
$(BIN)/CONTROLS.DLL: src/controls/controls.c src/controls/controls_input.c \
                     src/controls/controls.h src/controls/controls.def \
                     $(CONTROLS_INC) $(BIN)/controls.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/controls $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_CONTROLS) \
	      -o $@ src/controls/controls.c src/controls/controls_input.c \
	      $(BIN)/controls.res src/controls/controls.def -lkernel32 -luser32 -lgcc

$(BIN)/controls.res: src/controls/controls.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

$(CONTROLS_INC): $(ORIG)/Modules/CONTROLS.DLL tools/gen_controls_data.py
	@python3 tools/gen_controls_data.py $< $(CONTROLS_INC)

# ANALYSIS, landing and manoeuvre analysis and course tracking: assembly and
# MSVC C.
$(BIN)/ANALYSIS.DLL: src/analysis/analysis.c src/analysis/analysis.def src/analysis/analysis_data.inc \
                     src/analysis/analysis_rdata.inc $(BIN)/analysis.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/analysis $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_ANALYSIS) \
	      -o $@ src/analysis/analysis.c $(BIN)/analysis.res src/analysis/analysis.def \
	      -lkernel32 -lgcc

$(BIN)/analysis.res: src/analysis/analysis.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/analysis/analysis_data.inc src/analysis/analysis_rdata.inc: $(ORIG)/Modules/ANALYSIS.DLL tools/gen_analysis_data.py
	@python3 tools/gen_analysis_data.py $< src/analysis/analysis_data.inc src/analysis/analysis_rdata.inc

# MAIN, the sim's controller: MSVC C and assembly.  In progress, a slice at a
# time (docs/modules/main.md); not in MODULES until every slot is done.
MAIN_SOURCES := src/main/main.c src/main/main_records.inc src/main/main_situation.inc src/main/main_nav.inc src/main/main_season.inc src/main/main_sun.inc src/main/main_eye.inc src/main/main_text_tail.inc
$(BIN)/MAIN.DLL: $(MAIN_SOURCES) src/main/main.def src/main/main_data.inc \
                 src/main/main_pointers.inc $(BIN)/main.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/main $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_MAIN) \
	      -o $@ src/main/main.c $(BIN)/main.res src/main/main.def \
	      -lkernel32 -luser32 -lgdi32 -lgcc

$(BIN)/main.res: src/main/main.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/main/main_data.inc src/main/main_pointers.inc src/main/main_text_tail.inc &: $(ORIG)/Modules/MAIN.DLL tools/gen_main_data.py
	@python3 tools/gen_main_data.py

# MPCHAT, the multiplayer chat window: MSVC C.  The CD names it mpchat.DLL,
# and setup installs it under that name.
$(BIN)/MPCHAT.DLL: src/mpchat/mpchat.c src/mpchat/mpchat.def src/mpchat/mpchat_data.inc \
                   $(BIN)/mpchat.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/mpchat $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_MPCHAT) \
	      -o $@ src/mpchat/mpchat.c $(BIN)/mpchat.res src/mpchat/mpchat.def \
	      -lkernel32 -luser32 -lgdi32 -lgcc

$(BIN)/mpchat.res: src/mpchat/mpchat.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/mpchat/mpchat_data.inc: $(ORIG)/Modules/mpchat.DLL tools/gen_mpchat_data.py
	@python3 tools/gen_mpchat_data.py $< $@

# WINMSG, the on-screen message window: MSVC C.
$(BIN)/WINMSG.DLL: src/winmsg/winmsg.c src/winmsg/winmsg.def src/winmsg/winmsg_data.inc \
                   $(BIN)/winmsg.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/winmsg $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_WINMSG) \
	      -o $@ src/winmsg/winmsg.c $(BIN)/winmsg.res src/winmsg/winmsg.def \
	      -lkernel32 -luser32 -lgcc

$(BIN)/winmsg.res: src/winmsg/winmsg.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/winmsg/winmsg_data.inc: $(ORIG)/Modules/WINMSG.DLL tools/gen_winmsg_data.py
	@python3 tools/gen_winmsg_data.py $< $@

# LANDME, the landing-assistance state machine.  It uses retail FE in-game and
# in its differential harness, so it can be completed independently of FE's
# remaining Scenery Library dialog.
$(BIN)/LANDME.DLL: src/landme/landme.c src/landme/landme.def \
                   src/landme/landme_data.inc $(BIN)/landme.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -Isrc/landme $(LDFLAGS) -Wl,--image-base=$(BASE_LANDME) \
	      -o $@ src/landme/landme.c $(BIN)/landme.res src/landme/landme.def -lkernel32

$(BIN)/landme.res: src/landme/landme.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/landme/landme_data.inc: $(ORIG)/Modules/LANDME.DLL tools/gen_landme_data.py
	@python3 tools/gen_landme_data.py $< $@

$(BIN)/SLEW.DLL: src/slew/slew.c src/slew/slew.def $(BIN)/slew.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--image-base=$(BASE_SLEW) \
	      -o $@ src/slew/slew.c $(BIN)/slew.res src/slew/slew.def -lkernel32

$(BIN)/slew.res: src/slew/slew.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

$(BIN)/NUMERIC.DLL: src/numeric/numeric.c src/numeric/numeric.def \
                    src/numeric/numeric_imports.h \
                    src/numeric/numeric_data.inc include/fs98/numeric.h \
                    $(BIN)/numeric.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -DFS98_NUMERIC_MODULE -Isrc/numeric $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_NUMERIC) \
	      -o $@ src/numeric/numeric.c $(BIN)/numeric.res \
	      src/numeric/numeric.def -lmsvcrt -lkernel32 -lgcc

$(BIN)/numeric.res: src/numeric/numeric.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

# UTIL is assembled from four service files plus the module shell.
# FS98_UTIL_MODULE makes the services read their imports from ImportTable, as
# retail does; libgcc supplies the 64-bit division helpers.
UTIL_SRC := src/util/util.c src/util/util_math.c src/util/util_files.c \
            src/util/util_exchange.c src/util/util_alert.c

$(BIN)/UTIL.DLL: $(UTIL_SRC) src/util/util.def src/util/util_imports.h \
                 src/util/cos_table.inc src/util/util_data.inc $(BIN)/util.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -DFS98_UTIL_MODULE -Isrc/util $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_UTIL) \
	      -o $@ $(UTIL_SRC) $(BIN)/util.res src/util/util.def -lkernel32 -lgcc

# The same module with every slot counted -- see FS98_UTIL_TRACE in util.c.
# `make util-trace` installs it, boots the sim for UTIL_TRACE_SECS (or until
# you quit it), prints which services were called and how often, and puts the
# normal build back.
UTIL_TRACE_SECS ?= 90
UTIL_TRACE_LOG  := $(HOME)/.wine/drive_c/openfs98-util-trace.log

$(BIN)/UTIL_TRACE.DLL: $(UTIL_SRC) src/util/util.def src/util/util_imports.h \
                       src/util/cos_table.inc src/util/util_data.inc $(BIN)/util.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -DFS98_UTIL_MODULE -DFS98_UTIL_TRACE -Isrc/util $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_UTIL) \
	      -o $@ $(UTIL_SRC) $(BIN)/util.res src/util/util.def -lkernel32 -lgcc

util-trace: $(BIN)/UTIL_TRACE.DLL $(BIN)/UTIL.DLL
	@cp $(BIN)/UTIL_TRACE.DLL "$(FS98_DIR)/Modules/UTIL.DLL"
	@rm -f "$(UTIL_TRACE_LOG)"
	@echo "tracing UTIL for up to $(UTIL_TRACE_SECS)s -- use the sim, or just wait"
	@cd $(FS98_DIR) && WINEDEBUG=-all timeout $(UTIL_TRACE_SECS) wine FLTSIM98.EXE >/dev/null 2>&1 || true
	@cp $(BIN)/UTIL.DLL "$(FS98_DIR)/Modules/UTIL.DLL"
	@echo "--- services called (slot, count, hex) ---"
	@grep -v ' 00000000' "$(UTIL_TRACE_LOG)" 2>/dev/null || echo "(no log)"
	@echo "--- never called ---"
	@grep ' 00000000' "$(UTIL_TRACE_LOG)" 2>/dev/null | cut -d' ' -f1 | tr '\n' ' '; echo

$(BIN)/util.res: src/util/util.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/global/global_state.inc: $(ORIG)/Modules/GLOBAL.DLL tools/gen_global_state.py
	python3 tools/gen_global_state.py $< $@

src/numeric/numeric_data.inc: $(ORIG)/Modules/NUMERIC.DLL tools/gen_numeric_tables.py
	@python3 tools/gen_numeric_tables.py $< $@

# The in-process test module.  The host cannot be loaded beside an external
# test program -- no .reloc, so it only works at 0x400000, which is already
# committed in any wine process -- so anything that calls the host has to be
# tested from inside the sim instead.  This builds a module with its own id
# that the real loader binds and calls, and which writes its results to a file.
#
# It needs libgcc for the 64-bit division helpers util_math.c uses.
OFSTEST_LOG := $(HOME)/.wine/drive_c/openfs98-test.log

$(BIN)/OFSTEST.DLL: src/test/ofstest.c src/test/ofstest.def src/util/util_math.c src/util/util_files.c src/util/util_exchange.c src/util/util_alert.c \
                    src/util/cos_table.inc src/util/util_data.inc
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -Isrc/util $(LDFLAGS) -Wl,--image-base=0x20950000 \
	      -o $@ src/test/ofstest.c src/util/util_math.c src/util/util_files.c src/util/util_exchange.c src/util/util_alert.c src/test/ofstest.def \
	      -lkernel32 -lgcc

ofstest: $(BIN)/OFSTEST.DLL

# Drop it in, start the sim long enough for the loader to call init, stop, and
# show what it found.  The module only needs startup, not a flight.
#
# The tests compare the reimplementation against whatever UTIL.DLL the loader
# binds, so retail's is put in for the run -- with OpenFS98's UTIL installed
# they would be comparing it with itself -- and whatever was there is put back.
ofstest-run: ofstest
	@cp $(BIN)/OFSTEST.DLL "$(FS98_DIR)/Modules/OFSTEST.DLL"
	@cp "$(FS98_DIR)/Modules/UTIL.DLL" $(BIN)/.util-installed.bak
	@cp $(ORIG)/Modules/UTIL.DLL "$(FS98_DIR)/Modules/UTIL.DLL"
	@rm -f "$(OFSTEST_LOG)"
	@cd $(FS98_DIR) && WINEDEBUG=-all timeout 45 wine FLTSIM98.EXE >/dev/null 2>&1 || true
	@cp $(BIN)/.util-installed.bak "$(FS98_DIR)/Modules/UTIL.DLL"; rm -f $(BIN)/.util-installed.bak
	@echo "--- $(OFSTEST_LOG) ---"
	@cat "$(OFSTEST_LOG)" 2>/dev/null || echo "(no log -- init was not reached)"

# Take it back out; it is a test fixture, not part of the sim.
ofstest-remove:
	@rm -f "$(FS98_DIR)/Modules/OFSTEST.DLL"
	@echo "OFSTEST.DLL removed from Modules"

# Wine compatibility, not part of the sim: wine swallows exceptions raised in
# window procedures, so the host's integer-divide repair (filter 0x4119a0)
# never sees the ones retail takes in WM_SIZE and in HG2D's rasteriser.
# SEHFIX.DLL hands them to that filter from a vectored handler, and logs each
# one with the SEH chain it walked.  See src/winecompat/sehfix.c and
# docs/wine.md.
SEHFIX_LOG := $(HOME)/.wine/drive_c/openfs98-sehfix.log

$(BIN)/SEHFIX.DLL: src/winecompat/sehfix.c src/winecompat/sehfix.def
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -D_WIN32_WINNT=0x0501 $(LDFLAGS) -Wl,--image-base=0x20960000 \
	      -o $@ src/winecompat/sehfix.c src/winecompat/sehfix.def -lkernel32

sehfix: $(BIN)/SEHFIX.DLL

sehfix-install: sehfix
	@cp $(BIN)/SEHFIX.DLL "$(FS98_DIR)/Modules/SEHFIX.DLL"
	@echo "SEHFIX.DLL installed; its log is $(SEHFIX_LOG)"

sehfix-remove:
	@rm -f "$(FS98_DIR)/Modules/SEHFIX.DLL"
	@echo "SEHFIX.DLL removed from Modules"

sehfix-log:
	@cat "$(SEHFIX_LOG)" 2>/dev/null || echo "(no log -- SEHFIX was not loaded)"

# Differential test.  The game cannot exercise SLEW (its tick only runs in slew
# mode), so the module is driven directly instead: both builds get an
# identically seeded GLOBAL block and a stub UTIL, and the blocks are compared
# afterwards.  Needs the full CRT, so it does not use the module LDFLAGS.
DIFFDIR := $(BIN)/difftest

$(DIFFDIR)/slewdiff.exe: tools/difftest/slewdiff.c src/util/util_math.c src/util/cos_table.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/util -o $@ \
	      tools/difftest/slewdiff.c src/util/util_math.c

$(DIFFDIR)/utildiff.exe: tools/difftest/utildiff.c src/util/util_math.c src/util/util_exchange.c src/util/util_alert.c src/util/cos_table.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/util -o $@ \
	      tools/difftest/utildiff.c src/util/util_math.c src/util/util_exchange.c src/util/util_alert.c

$(DIFFDIR)/numericdiff.exe: tools/difftest/numericdiff.c \
                            src/numeric/numeric.c \
                            src/numeric/numeric_imports.h \
                            src/numeric/numeric_data.inc \
                            include/fs98/numeric.h
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/numeric -o $@ \
	      tools/difftest/numericdiff.c src/numeric/numeric.c -lm

src/util/cos_table.inc: $(ORIG)/Modules/UTIL.DLL tools/gen_util_tables.py
	@python3 tools/gen_util_tables.py $< $@

utildiff: $(DIFFDIR)/utildiff.exe
	@cp $(ORIG)/Modules/UTIL.DLL $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/Modules/WINDOW.DLL $(DIFFDIR)/WINDOW_retail.dll

# The same check with the x87 assembly compiled out, which is what an ARM build
# (Apple silicon, Android) will use.  It has to match retail just as exactly,
# or those builds would quietly compute different numbers.
$(DIFFDIR)/utildiff_portable.exe: tools/difftest/utildiff.c src/util/util_math.c src/util/util_exchange.c src/util/util_alert.c src/util/cos_table.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/util -DFS98_PORTABLE_FP -o $@ \
	      tools/difftest/utildiff.c src/util/util_math.c src/util/util_exchange.c src/util/util_alert.c -lm

utildiff-portable: $(DIFFDIR)/utildiff_portable.exe utildiff
	@cd $(DIFFDIR) && WINEDEBUG=-all wine utildiff_portable.exe

utildiff-run: utildiff
	@cd $(DIFFDIR) && WINEDEBUG=-all wine utildiff.exe

# FE's grid conversions (+0x60..+0x6c), by vectors: retail and OpenFS98
# loaded side by side and called directly.  N=... sets the random count.
$(DIFFDIR)/fegriddiff.exe: tools/difftest/fegriddiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/fegriddiff.c

fegriddiff: $(DIFFDIR)/fegriddiff.exe $(BIN)/FE.DLL
	@cp $(ORIG)/Modules/FE.DLL $(DIFFDIR)/FE_retail.dll
	@cp $(BIN)/FE.DLL          $(DIFFDIR)/FE_ours.dll

fegriddiff-run: fegriddiff
	@cd $(DIFFDIR) && WINEDEBUG=-all wine fegriddiff.exe $(N)

# FE's scenery engine and compiled-C file services, whole-module: both DLLs
# driven through Linkage over the installed scenery/texture tree, with retail
# UTIL and a stub host passing file I/O through to that fixture.  Prefer a fully
# extracted orig/ tree; fall back to the staged game/ tree used by smoke tests.
FE_FIXTURE ?= $(if $(and $(wildcard $(ORIG)/Scenery/world.vis),$(wildcard $(ORIG)/Texture/fs5.pal)),../../$(ORIG),$(if $(and $(wildcard game/Scenery/world.vis),$(wildcard game/Texture/fs5.pal)),../../game,))

$(DIFFDIR)/fediff.exe: tools/difftest/fediff.c src/fe/fe_data.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe -o $@ tools/difftest/fediff.c

fediff: $(DIFFDIR)/fediff.exe $(BIN)/FE.DLL
	@cp $(ORIG)/Modules/FE.DLL   $(DIFFDIR)/FE_retail.dll
	@cp $(BIN)/FE.DLL            $(DIFFDIR)/FE_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL $(DIFFDIR)/UTIL_retail.dll

fediff-run: fediff
	@test -n "$(FE_FIXTURE)" || { \
	    echo "FE fixture missing: run tools/extract.sh again or make stage" >&2; exit 2; }
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine fediff.exe $(if $(N),$(N),20000) "$(FE_FIXTURE)"

# LANDME's whole state machine, with a separate retail FE instance behind each
# side so both see the same scenery without sharing FE's mutable search state.
$(DIFFDIR)/landmediff.exe: tools/difftest/landmediff.c tools/difftest/fediff.c \
                           src/fe/fe_data.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe \
	      -o $@ tools/difftest/landmediff.c

landmediff: $(DIFFDIR)/landmediff.exe $(BIN)/LANDME.DLL
	@cp $(ORIG)/Modules/LANDME.DLL $(DIFFDIR)/LANDME_retail.dll
	@cp $(BIN)/LANDME.DLL          $(DIFFDIR)/LANDME_ours.dll
	@cp $(ORIG)/Modules/FE.DLL     $(DIFFDIR)/FE_landme_retail.dll
	@cp $(ORIG)/Modules/FE.DLL     $(DIFFDIR)/FE_landme_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL   $(DIFFDIR)/UTIL_retail.dll

landmediff-run: landmediff
	@test -n "$(FE_FIXTURE)" || { \
		echo "landmediff-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"; exit 2; }
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine landmediff.exe $(if $(N),$(N),20000) "$(FE_FIXTURE)"

# FACIL's externally testable file half: each side gets a separate retail FE,
# the harness builds a synthetic five-level facility directory, rebuilds and
# validates INDEX_FILE, then copies the serialized airport record through
# Linkage +0x20.  A second phase writes 20,000 seeded synthetic caches and
# drives +0x18/+0x1c/+0x20 and deinit over each.  A third rebuilds the index
# from the real CD scenery and boosters and runs 10,000 dialog scenarios
# against a stand-in for World.DLL, with retail NUMERIC.
$(DIFFDIR)/facildiff.exe: tools/difftest/facildiff.c tools/difftest/fediff.c \
                          src/fe/fe_data.inc src/facil/facil_data.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe \
	      -o $@ tools/difftest/facildiff.c

facildiff: $(DIFFDIR)/facildiff.exe $(BIN)/FACIL.DLL
	@cp $(ORIG)/Modules/facil.dll $(DIFFDIR)/FACIL_retail.dll
	@cp $(BIN)/FACIL.DLL           $(DIFFDIR)/FACIL_ours.dll
	@cp $(ORIG)/Modules/FE.DLL     $(DIFFDIR)/FE_facil_retail.dll
	@cp $(ORIG)/Modules/FE.DLL     $(DIFFDIR)/FE_facil_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL   $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/Modules/NUMERIC.DLL $(DIFFDIR)/NUMERIC_retail.dll

facildiff-run: facildiff
	@test -n "$(FE_FIXTURE)" || { \
		echo "facildiff-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"; exit 2; }
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine facildiff.exe \
	      FACIL_retail.dll FACIL_ours.dll "$(FE_FIXTURE)"

# The same harness with OpenFS98's FE behind OpenFS98's FACIL, against retail
# FE behind retail FACIL: both reimplementations together, over the real CD
# index and the dialogs.  Shorter by default; Q and D set the phase sizes.
facildiff-fe-run: facildiff $(BIN)/FE.DLL
	@test -n "$(FE_FIXTURE)" || { \
		echo "facildiff-fe-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"; exit 2; }
	@cp $(BIN)/FE.DLL $(DIFFDIR)/FE_facil_ours.dll
	@(cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine facildiff.exe \
	      FACIL_retail.dll FACIL_ours.dll "$(FE_FIXTURE)" \
	      $(if $(Q),$(Q),2000) $(if $(D),$(D),2000)); \
	  status=$$?; cp $(ORIG)/Modules/FE.DLL $(DIFFDIR)/FE_facil_ours.dll; exit $$status

# FE Linkage +0x78, the Scenery Library: retail FE against OpenFS98's, each
# run over a fresh copy of a writable fixture (the real world.vis and
# synthetic ones) with a stand-in for World.DLL's nine dialogs, retail UTIL
# and NUMERIC, and sandboxed drive letters.  The CD's Boosters directory, when
# extracted under cd/, is drive K:.  N=... sets the scenario count.
FE_CD ?= $(if $(wildcard cd/Boosters),../../cd,)

$(DIFFDIR)/fesldiff.exe: tools/difftest/fesldiff.c tools/difftest/facildiff.c \
                         tools/difftest/fediff.c src/fe/fe_data.inc \
                         src/facil/facil_data.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -Isrc/fe \
	      -o $@ tools/difftest/fesldiff.c

# OpenFS98's FE linked again at another 0x20xxxxxx base: one retail
# leftover is the top byte of a .data address, so the comparison copy keeps
# retail's (FE_retail.dll stays at its own base).
$(DIFFDIR)/FE_sl_ours.dll: src/fe/fe.c src/fe/fe_library.c src/fe/fe.def \
                           src/fe/fe_data.inc $(BIN)/fe.res
	@mkdir -p $(DIFFDIR)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/fe $(LDFLAGS) \
	      -Wl,--image-base=0x20c00000 \
	      -o $@ src/fe/fe.c $(BIN)/fe.res src/fe/fe.def -lkernel32

fesldiff: $(DIFFDIR)/fesldiff.exe $(DIFFDIR)/FE_sl_ours.dll
	@cp $(ORIG)/Modules/FE.DLL      $(DIFFDIR)/FE_retail.dll
	@cp $(ORIG)/Modules/UTIL.DLL    $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/Modules/NUMERIC.DLL $(DIFFDIR)/NUMERIC_retail.dll

fesldiff-run: fesldiff
	@test -n "$(FE_FIXTURE)" || { \
		echo "fesldiff-run needs Scenery/world.vis and Texture/fs5.pal under orig/ or game/"; exit 2; }
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine fesldiff.exe \
	      $(if $(N),$(N),5000) "$(FE_FIXTURE)" $(FE_CD)

# BOOSTER, whole-module with its worker thread: retail and OpenFS98 run in turn
# over identical cache trees, four of the CD's boosters and synthetic ones with
# every record type, the thread drained after every operation.  Needs the CD
# tree (cd/Boosters).  N=... sets the scenario count.
$(DIFFDIR)/boosterdiff.exe: tools/difftest/boosterdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/boosterdiff.c

boosterdiff: $(DIFFDIR)/boosterdiff.exe $(BIN)/BOOSTER.DLL
	@cp $(ORIG)/Modules/BOOSTER.DLL $(DIFFDIR)/BOOSTER_retail.dll
	@cp $(BIN)/BOOSTER.DLL          $(DIFFDIR)/BOOSTER_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL    $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/FLTSIM98.EXE        $(DIFFDIR)/FLTSIM98_host.exe

boosterdiff-run: boosterdiff
	@test -n "$(FE_CD)" || { echo "boosterdiff-run needs the CD tree in cd/ (cd/Boosters)"; exit 2; }
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 7200 wine boosterdiff.exe $(if $(N),$(N),20000) $(FE_CD)

# DEMO, whole-module: the flight-video recorder and player, instant replay and
# the four dialogs' callbacks, retail and OpenFS98 in turn over the CD's six
# videos and synthetic ones, with retail UTIL's catalogue and a stand-in for
# the host's dialog engine.  Needs cd/.  N=... sets the scenario count.
# build ADVDRV with one deliberate break and confirm the harness goes red
advdrv-break-%: advdrvdiff
	$(CC) $(CFLAGS) -DBREAK=$* -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/advdrv \
	      $(LDFLAGS) -Wl,--image-base=$(BASE_ADVDRV) -o $(DIFFDIR)/ADVDRV_ours.dll \
	      src/advdrv/advdrv.c $(BIN)/advdrv.res src/advdrv/advdrv.def -lkernel32 -lgcc
	@cd $(DIFFDIR) && WINEDEBUG=-all wine advdrvdiff.exe $(if $(N),$(N),300) $(FE_CD); \
	 test $$? -ne 0 && echo "BREAK $* caught" || echo "BREAK $* MISSED"
	@cp $(BIN)/ADVDRV.DLL $(DIFFDIR)/ADVDRV_ours.dll

$(DIFFDIR)/advdrvdiff.exe: tools/difftest/advdrvdiff.c src/advdrv/advdrv_data.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/advdrvdiff.c

advdrvdiff: $(DIFFDIR)/advdrvdiff.exe $(BIN)/ADVDRV.DLL
	@cp $(ORIG)/Modules/ADVDRV.DLL $(DIFFDIR)/ADVDRV_retail.dll
	@cp $(BIN)/ADVDRV.DLL          $(DIFFDIR)/ADVDRV_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL   $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/FLTSIM98.EXE       $(DIFFDIR)/FLTSIM98_host.exe

advdrvdiff-run: advdrvdiff
	@test -n "$(FE_CD)" || { echo "advdrvdiff-run needs the CD tree in cd/"; exit 2; }
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 7200 wine advdrvdiff.exe $(if $(N),$(N),20000) $(FE_CD)

# a coverage build of ADVDRV for the harness: what the programs reach
advdrv-cover: advdrvdiff
	$(CC) $(CFLAGS) -DADVDRV_COVER -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/advdrv \
	      $(LDFLAGS) -Wl,--image-base=$(BASE_ADVDRV) -o $(DIFFDIR)/ADVDRV_ours.dll \
	      src/advdrv/advdrv.c $(BIN)/advdrv.res src/advdrv/advdrv_cover.def -lkernel32 -lgcc
	@cd $(DIFFDIR) && WINEDEBUG=-all wine advdrvdiff.exe $(if $(N),$(N),1000) $(FE_CD)
	@cp $(BIN)/ADVDRV.DLL $(DIFFDIR)/ADVDRV_ours.dll

$(DIFFDIR)/demodiff.exe: tools/difftest/demodiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/demodiff.c

demodiff: $(DIFFDIR)/demodiff.exe $(BIN)/DEMO.DLL
	@cp $(ORIG)/Modules/DEMO.DLL $(DIFFDIR)/DEMO_retail.dll
	@cp $(BIN)/DEMO.DLL          $(DIFFDIR)/DEMO_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/FLTSIM98.EXE     $(DIFFDIR)/FLTSIM98_host.exe

demodiff-run: demodiff
	@test -n "$(FE_CD)" || { echo "demodiff-run needs the CD tree in cd/"; exit 2; }
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine demodiff.exe $(if $(N),$(N),20000) $(FE_CD)

# WINDOW, whole-module: the view windows and the eyepoint, retail and OpenFS98
# in turn over the same scripts, with retail UTIL, a model of the host's
# window services and logging stubs for MAIN, G3D, HG2D and the rest.  The
# CD's stars file is used when cd/ is there.  N=... sets the scenario count.
$(DIFFDIR)/windowdiff.exe: tools/difftest/windowdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/windowdiff.c

windowdiff: $(DIFFDIR)/windowdiff.exe $(BIN)/WINDOW.DLL
	@cp $(ORIG)/Modules/WINDOW.DLL $(DIFFDIR)/WINDOW_retail.dll
	@cp $(BIN)/WINDOW.DLL          $(DIFFDIR)/WINDOW_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL   $(DIFFDIR)/UTIL_retail.dll

# A deliberately broken copy of window.c as the harness's "ours" (for
# tools/window_breaks.py, which makes the copies and runs the harness).
window-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/window $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_WINDOW) -o $(DIFFDIR)/WINDOW_ours.dll \
	      $(WINDOW_SRC) $(BIN)/window.res src/window/window.def -lkernel32 -lgcc

window-breaks: windowdiff
	@python3 tools/window_breaks.py $(if $(N),$(N),2000)

# G3D, in progress: retail and OpenFS98 in turn over the same scripts, every
# register set before each service call and compared after, the host's divide
# repair modelled; the image (pointers into it as offsets) and GLOBAL compared.
$(DIFFDIR)/g3ddiff.exe: tools/difftest/g3ddiff.c src/g3d/g3d_pointers.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/g3ddiff.c

g3ddiff: $(DIFFDIR)/g3ddiff.exe $(BIN)/G3D.DLL
	@cp $(ORIG)/Modules/G3D.DLL $(DIFFDIR)/G3D_retail.dll
	@cp $(ORIG)/Modules/UTIL.DLL $(DIFFDIR)/UTIL_retail.dll
	@cp $(BIN)/G3D.DLL          $(DIFFDIR)/G3D_ours.dll

g3ddiff-run: g3ddiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 7200 wine g3ddiff.exe $(if $(N),$(N),20000)

# MULTIP whole-module acceptance: protocols, workers, dialogs and mixed lifecycle.
$(DIFFDIR)/multipdiff.exe: tools/difftest/multipdiff.c tools/difftest/multip_send.inc tools/difftest/multip_receive.inc tools/difftest/multip_workers.inc tools/difftest/multip_directplay.inc tools/difftest/multip_profile.inc tools/difftest/multip_session.inc tools/difftest/multip_aircraft.inc tools/difftest/multip_tracking.inc tools/difftest/multip_messages.inc tools/difftest/multip_players.inc tools/difftest/multip_local.inc tools/difftest/multip_frame.inc tools/difftest/multip_draw.inc tools/difftest/multip_motion.inc tools/difftest/multip_commands.inc tools/difftest/multip_weather.inc tools/difftest/multip_tick.inc tools/difftest/multip_system.inc tools/difftest/multip_app_players.inc tools/difftest/multip_updates.inc tools/difftest/multip_textures.inc tools/difftest/multip_texture_receive.inc tools/difftest/multip_transfer_messages.inc tools/difftest/multip_texture_send.inc tools/difftest/multip_model_send.inc tools/difftest/multip_model_cache.inc tools/difftest/multip_model_receive.inc tools/difftest/multip_state_messages.inc tools/difftest/multip_control_updates.inc tools/difftest/multip_dispatch.inc tools/difftest/multip_lifecycle.inc tools/difftest/multip_providers.inc tools/difftest/multip_dialogs.inc tools/difftest/multip_lobby.inc tools/difftest/multip_init.inc tools/difftest/multip_bounds.inc tools/difftest/multip_whole.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -Iinclude -o $@ tools/difftest/multipdiff.c

# Internal transport/worker routines have no public Linkage slots. Expose their addresses
# only in the differential build, alongside the unchanged retail exports.
$(DIFFDIR)/MULTIP_test.dll: src/multip/multip.c src/multip/multip_callbacks.inc src/multip/multip_motion.inc src/multip/multip_commands.inc src/multip/multip_weather.inc src/multip/multip_tick.inc src/multip/multip_system.inc src/multip/multip_app_players.inc src/multip/multip_updates.inc src/multip/multip_textures.inc src/multip/multip_texture_receive.inc src/multip/multip_transfer_messages.inc src/multip/multip_texture_send.inc src/multip/multip_model_send.inc src/multip/multip_model_cache.inc src/multip/multip_model_receive.inc src/multip/multip_title_receive.inc src/multip/multip_state_messages.inc src/multip/multip_control_updates.inc src/multip/multip_lifecycle.inc src/multip/multip_dispatch.inc src/multip/multip_dispatch_cases.inc src/multip/multip_weather_fields.inc src/multip/multip_frame_actions.inc src/multip/multip_players.inc src/multip/multip_nametag.inc src/multip/multip_messages.inc src/multip/multip_tracking.inc src/multip/multip_records.inc src/multip/multip_aircraft.inc src/multip/multip_session.inc src/multip/multip_profile.inc src/multip/multip_profile_fields.inc src/multip/multip.def src/multip/multip_data.inc src/multip/multip_rdata.inc src/multip/multip_pointers.inc $(BIN)/multip.res src/multip/multip_providers.inc src/multip/multip_browser.inc src/multip/multip_configuration.inc src/multip/multip_details.inc src/multip/multip_options.inc src/multip/multip_lobby.inc src/multip/multip_toggle.inc src/multip/multip_init.inc
	@mkdir -p $(DIFFDIR)
	$(CC) $(CFLAGS) -DMULTIP_DIFFTEST $(LDFLAGS) -Wl,--image-base=$(BASE_MULTIP) \
	      -o $@ src/multip/multip.c $(BIN)/multip.res src/multip/multip.def -lkernel32 -lshell32 -luser32

# Replace staged files atomically: Wine may still have the prior DLL mapped.
$(DIFFDIR)/MULTIP_retail.dll: $(ORIG)/Modules/multip.dll
	@mkdir -p $(DIFFDIR)
	@cp $< $@.tmp
	@mv -f $@.tmp $@

$(DIFFDIR)/MULTIP_ours.dll: $(DIFFDIR)/MULTIP_test.dll
	@cp $< $@.tmp
	@mv -f $@.tmp $@

$(DIFFDIR)/G3D_retail.dll: $(ORIG)/Modules/G3D.DLL
	@mkdir -p $(DIFFDIR)
	@cp $< $@.tmp
	@mv -f $@.tmp $@

multipdiff: $(DIFFDIR)/G3D_retail.dll $(DIFFDIR)/UTIL_retail.dll $(DIFFDIR)/multipdiff.exe $(BIN)/MULTIP.DLL $(DIFFDIR)/MULTIP_retail.dll $(DIFFDIR)/MULTIP_ours.dll

multipdiff-run: multipdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all wine multipdiff.exe $(if $(N),$(N),20000)

multip-smoke: all $(BIN)/MULTIP.DLL
	@tools/multip_smoke.sh

multip-acceptance: all multipdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all wine multipdiff.exe 20000
	@python3 -B tools/multip_breaks.py
	@git diff --check
	@tools/multip_smoke.sh
	@echo "MULTIP acceptance: PASS"

multip-breaks: multipdiff
	@python3 -B tools/multip_breaks.py

multip-directplay-breaks: multipdiff
	@python3 tools/multip_breaks.py --slice directplay

multip-worker-breaks: multipdiff
	@python3 tools/multip_breaks.py --slice workers

multip-receive-breaks: multipdiff
	@python3 tools/multip_breaks.py --slice receive

multip-send-breaks: multipdiff
	@python3 tools/multip_breaks.py --slice send

# G3D broken on purpose, a place at a time (tools/g3d_breaks.py).
G3D_SRC ?= src/g3d/g3d.c
g3d-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/g3d $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_G3D) -o $(DIFFDIR)/G3D_ours.dll \
	      $(G3D_SRC) src/g3d/g3d_mesh.S $(BIN)/g3d.res src/g3d/g3d.def -lkernel32 -luser32 -lgcc

g3d-breaks: g3ddiff
	@python3 tools/g3d_breaks.py $(if $(N),$(N),2000)

# The mesh compiler (0x38) three ways -- retail's instructions, the C on the
# x87, the C on its software float -- over generated polygons under each
# precision control; and the whole module with the portable C one in it,
# against retail.
$(DIFFDIR)/g3dmeshdiff.exe: tools/difftest/g3dmeshdiff.c src/g3d/g3d_mesh.c src/g3d/g3d_mesh.S
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -c -DG3D_MESH_NAME=g3d_mesh_hw \
	      -o $(DIFFDIR)/g3d_mesh_hw.o src/g3d/g3d_mesh.c
	$(CC) -std=c99 -O1 -Wall -Wextra -c -DFS98_PORTABLE_FP -DG3D_MESH_NAME=g3d_mesh_soft \
	      -o $(DIFFDIR)/g3d_mesh_soft.o src/g3d/g3d_mesh.c
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/g3dmeshdiff.c src/g3d/g3d_mesh.S \
	      $(DIFFDIR)/g3d_mesh_hw.o $(DIFFDIR)/g3d_mesh_soft.o

g3dmeshdiff: $(DIFFDIR)/g3dmeshdiff.exe

g3dmeshdiff-run: g3dmeshdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all wine g3dmeshdiff.exe $(if $(N),$(N),100000)

# The HG2D model (tools/difftest/hg2d_model.inc) that harnesses of modules
# drawing through HG2D use, against retail HG2D where it is exact: the
# texture pool, +0xac and +0xb0.
$(DIFFDIR)/hg2dmodeldiff.exe: tools/difftest/hg2dmodeldiff.c tools/difftest/hg2d_model.inc
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/hg2dmodeldiff.c

hg2dmodeldiff-run: $(DIFFDIR)/hg2dmodeldiff.exe
	@cp $(ORIG)/Modules/HG2D.DLL $(DIFFDIR)/HG2D_retail.dll
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 1800 wine hg2dmodeldiff.exe $(if $(N),$(N),20000)

g3ddiff-meshc-run: $(DIFFDIR)/g3ddiff.exe $(BIN)/g3d.res
	$(CC) $(CFLAGS) -DFS98_PORTABLE_FP -c -o $(DIFFDIR)/g3d_mesh_portable.o src/g3d/g3d_mesh.c
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/g3d -DG3D_MESH_C $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_G3D) -o $(DIFFDIR)/G3D_ours.dll \
	      src/g3d/g3d.c $(DIFFDIR)/g3d_mesh_portable.o $(BIN)/g3d.res src/g3d/g3d.def \
	      -lkernel32 -luser32 -lgcc
	@cp $(ORIG)/Modules/G3D.DLL $(DIFFDIR)/G3D_retail.dll
	@cp $(ORIG)/Modules/UTIL.DLL $(DIFFDIR)/UTIL_retail.dll
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 7200 wine g3ddiff.exe $(if $(N),$(N),20000)

windowdiff-run: windowdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 7200 wine windowdiff.exe $(if $(N),$(N),20000) $(if $(FE_CD),$(FE_CD),.)

# REALITY, whole-module: crashes, damage and the crash analysis, retail and
# OpenFS98 in turn over the same scripts, with retail UTIL and NUMERIC, a
# stand-in for the analysis dialog and logging stubs for the rest (HG2D's
# lines with their colour).  N=... sets the scenario count.
$(DIFFDIR)/realitydiff.exe: tools/difftest/realitydiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/realitydiff.c

realitydiff: $(DIFFDIR)/realitydiff.exe $(BIN)/REALITY.DLL
	@cp $(ORIG)/Modules/REALITY.DLL $(DIFFDIR)/REALITY_retail.dll
	@cp $(BIN)/REALITY.DLL          $(DIFFDIR)/REALITY_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL    $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/Modules/NUMERIC.DLL $(DIFFDIR)/NUMERIC_retail.dll

# A deliberately broken copy of reality.c as the harness's "ours" (for
# tools/reality_breaks.py, which makes the copies and runs the harness).
reality-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/reality $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_REALITY) -o $(DIFFDIR)/REALITY_ours.dll \
	      $(REALITY_SRC) $(BIN)/reality.res src/reality/reality.def -lkernel32 -lgcc

reality-breaks: realitydiff
	@python3 tools/reality_breaks.py $(if $(N),$(N),2000)

realitydiff-run: realitydiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine realitydiff.exe $(if $(N),$(N),20000)

# ANALYSIS, whole-module: landing and manoeuvre analysis and course
# tracking, retail and OpenFS98 in turn over the same scripts, with retail
# UTIL and NUMERIC, a stand-in for the three dialogs and logging stubs for
# HG2D's lines and G3D's display list.  N=... sets the scenario count.
$(DIFFDIR)/analysisdiff.exe: tools/difftest/analysisdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/analysisdiff.c

analysisdiff: $(DIFFDIR)/analysisdiff.exe $(BIN)/ANALYSIS.DLL
	@cp $(ORIG)/Modules/ANALYSIS.DLL $(DIFFDIR)/ANALYSIS_retail.dll
	@cp $(BIN)/ANALYSIS.DLL          $(DIFFDIR)/ANALYSIS_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL     $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/Modules/NUMERIC.DLL  $(DIFFDIR)/NUMERIC_retail.dll

# CONTROLS, whole-module: retail and OpenFS98 loaded side by side over
# commands, the CHAIN frames, the joysticks, the key bindings, force feedback
# and the assignments dialog, with fixture CFG files, models of the host's
# profile, accelerator and joystick services, a DirectInput stand-in and a
# stand-in for Options.DLL's dialog; compares GLOBAL, the whole .data image,
# the import-call transcript and the profile files.  See
# docs/modules/controls.md.
$(DIFFDIR)/controlsdiff.exe: tools/difftest/controlsdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/controlsdiff.c

controlsdiff: $(DIFFDIR)/controlsdiff.exe $(BIN)/CONTROLS.DLL
	@cp $(call orig_module,CONTROLS) $(DIFFDIR)/CONTROLS_retail.dll
	@cp $(BIN)/CONTROLS.DLL          $(DIFFDIR)/CONTROLS_ours.dll
	@cp $(ORIG)/Modules/UTIL.DLL     $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/Modules/NUMERIC.DLL  $(DIFFDIR)/NUMERIC_retail.dll
	@cp $(ORIG)/ENGLISH.FLL          $(DIFFDIR)/ENGLISH.FLL
	@cp $(ORIG)/FLTSIM98.EXE         $(DIFFDIR)/FLTSIM98_host.exe

controlsdiff-run: controlsdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine controlsdiff.exe $(if $(N),$(N),20000)

# A deliberately broken copy of the module as the harness's "ours" (for
# tools/controls_breaks.py).  It rebuilds CONTROLS from the break copies of
# controls.c and controls_input.c, one of them broken, the other unchanged.
CONTROLS_SRC := src/controls/controls.c src/controls/controls_input.c
controls-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/controls $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_CONTROLS) -o $(DIFFDIR)/CONTROLS_ours.dll \
	      $(DIFFDIR)/controls_break.c $(DIFFDIR)/controls_input_break.c \
	      $(BIN)/controls.res src/controls/controls.def -lkernel32 -luser32 -lgcc

controls-breaks: controlsdiff
	@python3 tools/controls_breaks.py $(if $(N),$(N),2000)

# A deliberately broken copy of analysis.c as the harness's "ours" (for
# tools/analysis_breaks.py, which makes the copies and runs the harness).
analysis-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/analysis $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_ANALYSIS) -o $(DIFFDIR)/ANALYSIS_ours.dll \
	      $(ANALYSIS_SRC) $(BIN)/analysis.res src/analysis/analysis.def -lkernel32 -lgcc

analysis-breaks: analysisdiff
	@python3 tools/analysis_breaks.py $(if $(N),$(N),2000)

analysisdiff-run: analysisdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine analysisdiff.exe $(if $(N),$(N),20000)

# MAIN, a suite per slice (docs/modules/main.md).  SUITE=... picks one
# (records), N=... the scenario count.
$(DIFFDIR)/maindiff.exe: tools/difftest/maindiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/maindiff.c -lm

maindiff: $(DIFFDIR)/maindiff.exe $(BIN)/MAIN.DLL
	@cp $(ORIG)/Modules/MAIN.DLL $(DIFFDIR)/MAIN_retail.dll
	@cp $(BIN)/MAIN.DLL          $(DIFFDIR)/MAIN_ours.dll

maindiff-run: maindiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine maindiff.exe $(if $(N),$(N),20000) $(if $(SUITE),$(SUITE),records)

# A deliberately broken copy of a MAIN slice as the harness's "ours" (for
# tools/main_breaks.py).
# MAIN_BREAK_DIR is a copy of src/main with one file broken.
main-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -I$(MAIN_BREAK_DIR) $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_MAIN) -o $(DIFFDIR)/MAIN_ours.dll \
	      $(MAIN_BREAK_DIR)/main.c $(BIN)/main.res src/main/main.def -lkernel32 -luser32 -lgdi32 -lgcc

main-breaks: maindiff
	@python3 tools/main_breaks.py $(if $(N),$(N),2000) $(if $(SUITE),$(SUITE),records)

# MPCHAT, whole-module: the chat window, retail and OpenFS98 in turn over the
# same scripts, against a model of the window system (hooked into both DLLs'
# import tables), the host's dialog runner and strings, MULTIP's player lists
# and WINMSG.  N=... sets the scenario count.
$(DIFFDIR)/mpchatdiff.exe: tools/difftest/mpchatdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/mpchatdiff.c

mpchatdiff: $(DIFFDIR)/mpchatdiff.exe $(BIN)/MPCHAT.DLL
	@cp $(ORIG)/Modules/mpchat.DLL $(DIFFDIR)/MPCHAT_retail.dll
	@cp $(BIN)/MPCHAT.DLL          $(DIFFDIR)/MPCHAT_ours.dll

# A deliberately broken copy of mpchat.c as the harness's "ours" (for
# tools/mpchat_breaks.py, which makes the copies and runs the harness).
mpchat-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/mpchat $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_MPCHAT) -o $(DIFFDIR)/MPCHAT_ours.dll \
	      $(MPCHAT_SRC) $(BIN)/mpchat.res src/mpchat/mpchat.def \
	      -lkernel32 -luser32 -lgdi32 -lgcc

mpchat-breaks: mpchatdiff
	@python3 tools/mpchat_breaks.py $(if $(N),$(N),2000)

mpchatdiff-run: mpchatdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine mpchatdiff.exe $(if $(N),$(N),20000)

# WINMSG, whole-module: the message window, retail and OpenFS98 in turn over
# the same scripts, with a model of the host's window and heap, stand-ins for
# WINDOW's record and MAIN's situation writer, and a CHAIN that answers the
# done message.  N=... sets the scenario count.
$(DIFFDIR)/winmsgdiff.exe: tools/difftest/winmsgdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/winmsgdiff.c

winmsgdiff: $(DIFFDIR)/winmsgdiff.exe $(BIN)/WINMSG.DLL
	@cp $(ORIG)/Modules/WINMSG.DLL $(DIFFDIR)/WINMSG_retail.dll
	@cp $(BIN)/WINMSG.DLL          $(DIFFDIR)/WINMSG_ours.dll

# A deliberately broken copy of winmsg.c as the harness's "ours" (for
# tools/winmsg_breaks.py, which makes the copies and runs the harness).
winmsg-break-build:
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/winmsg $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_WINMSG) -o $(DIFFDIR)/WINMSG_ours.dll \
	      $(WINMSG_SRC) $(BIN)/winmsg.res src/winmsg/winmsg.def -lkernel32 -luser32 -lgcc

winmsg-breaks: winmsgdiff
	@python3 tools/winmsg_breaks.py $(if $(N),$(N),2000)

winmsgdiff-run: winmsgdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine winmsgdiff.exe $(if $(N),$(N),20000)

# PANELS, whole-module: retail and OpenFS98 driven through Linkage and CHAIN
# over random scripts, against stubs of the host, GLOBAL, CHAIN, ACONTAIN,
# MAIN, SOUND and WINDOW, the HG2D model, and the CD's own gauges.
# N=... sets the scenario count, START=... the first.
$(DIFFDIR)/panelsdiff.exe: tools/difftest/panelsdiff.c tools/difftest/hg2d_model.inc \
                           tools/difftest/divfix.h
	@mkdir -p $(DIFFDIR)
	$(CC) -std=gnu99 -O1 -Wall -Wextra -DGAME_DIR='"..\\..\\game"' \
	      -o $@ tools/difftest/panelsdiff.c -lgdi32 -luser32

panelsdiff: $(DIFFDIR)/panelsdiff.exe $(BIN)/PANELS.DLL
	@cp $(ORIG)/Modules/PANELS.DLL $(DIFFDIR)/PANELS_retail.dll
	@cp $(BIN)/PANELS.DLL          $(DIFFDIR)/PANELS_ours.dll

# PANELS's image routines one at a time (panelsdiff's unit mode): PANELS.DLL
# built again with a table of them exported.
$(DIFFDIR)/PANELS_unit.dll: src/panels/panels.c src/panels/panels.h src/panels/panels_unit.def \
                            src/panels/panels_data.inc src/panels/panels_rdata.inc \
                            src/panels/panels_pointers.inc $(BIN)/panels.res
	@mkdir -p $(DIFFDIR)
	$(CC) $(CFLAGS) -DPANELS_UNIT -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/panels $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_PANELS) \
	      -o $@ src/panels/panels.c $(BIN)/panels.res src/panels/panels_unit.def \
	      -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc

panelsunit: $(DIFFDIR)/panelsdiff.exe $(DIFFDIR)/PANELS_unit.dll
	@cp $(ORIG)/Modules/PANELS.DLL $(DIFFDIR)/PANELS_retail.dll

panelsunit-run: panelsunit
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine panelsdiff.exe unit $(if $(N),$(N),300) $(START)

# A deliberately broken copy of panels.c as the harness's "ours" (for
# tools/panels_breaks.py, which makes the copies and runs the harness).
panels-break-build:
	$(CC) $(CFLAGS) $(PANELS_BREAK_FLAGS) -fno-delete-null-pointer-checks -fno-ipa-icf -Isrc/panels $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_PANELS) -o $(PANELS_OUT) $(PANELS_SRC) $(BIN)/panels.res \
	      $(PANELS_DEF) -lmsvcrt -lgdi32 -luser32 -lkernel32 -lgcc

panels-breaks: panelsdiff panelsunit
	@python3 tools/panels_breaks.py $(if $(N),$(N),150) $(if $(COUNT),$(COUNT),200) $(if $(SEED),$(SEED),1)

panelsdiff-run: panelsdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 7200 wine panelsdiff.exe $(if $(N),$(N),200) $(START)

# CHAIN, whole-module: both DLLs driven through Linkage over random scripts
# whose callbacks re-enter the module.  N=... sets the scenario count.
$(DIFFDIR)/chaindiff.exe: tools/difftest/chaindiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/chaindiff.c

chaindiff: $(DIFFDIR)/chaindiff.exe $(BIN)/CHAIN.DLL
	@cp $(ORIG)/Modules/CHAIN.DLL $(DIFFDIR)/CHAIN_retail.dll
	@cp $(BIN)/CHAIN.DLL          $(DIFFDIR)/CHAIN_ours.dll

chaindiff-run: chaindiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 600 wine chaindiff.exe $(N)

# ATC, whole-module: retail and OpenFS98 driven through Linkage over random
# scripts, against retail CHAIN, NUMERIC and UTIL and stub WINMSG/WEATHERM.
# N=... sets the scenario count.
$(DIFFDIR)/atcdiff.exe: tools/difftest/atcdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/atcdiff.c

atcdiff: $(DIFFDIR)/atcdiff.exe $(BIN)/ATC.DLL
	@cp $(ORIG)/Modules/ATC.DLL     $(DIFFDIR)/ATC_retail.dll
	@cp $(BIN)/ATC.DLL              $(DIFFDIR)/ATC_ours.dll
	@cp $(ORIG)/Modules/CHAIN.DLL   $(DIFFDIR)/CHAIN_retail.dll
	@cp $(ORIG)/Modules/NUMERIC.DLL $(DIFFDIR)/NUMERIC_retail.dll
	@cp $(ORIG)/Modules/UTIL.DLL    $(DIFFDIR)/UTIL_retail.dll

atcdiff-run: atcdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 900 wine atcdiff.exe $(N)

# WEATHERM, whole-module: retail and OpenFS98 driven through Linkage over
# random scripts, against retail CHAIN and UTIL and stub G3D/MAIN.
# N=... sets the scenario count.
$(DIFFDIR)/weathermdiff.exe: tools/difftest/weathermdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/weathermdiff.c

weathermdiff: $(DIFFDIR)/weathermdiff.exe $(BIN)/WEATHERM.DLL
	@cp $(ORIG)/Modules/WEATHERM.DLL $(DIFFDIR)/WEATHERM_retail.dll
	@cp $(BIN)/WEATHERM.DLL          $(DIFFDIR)/WEATHERM_ours.dll
	@cp $(ORIG)/Modules/CHAIN.DLL    $(DIFFDIR)/CHAIN_retail.dll
	@cp $(ORIG)/Modules/UTIL.DLL     $(DIFFDIR)/UTIL_retail.dll

weathermdiff-run: weathermdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 900 wine weathermdiff.exe $(N)

# SOUND, whole-module: retail and OpenFS98 driven through Linkage and their
# CHAIN lists over random scripts against generated SOUND.CFG trees (the CD's
# seven among them), with retail CHAIN and UTIL and a stub host that models
# the voice engine.  N=... sets the scenario count.
$(DIFFDIR)/sounddiff.exe: tools/difftest/sounddiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/sounddiff.c

sounddiff: $(DIFFDIR)/sounddiff.exe $(BIN)/SOUND.DLL
	@cp $(ORIG)/Modules/SOUND.DLL  $(DIFFDIR)/SOUND_retail.dll
	@cp $(BIN)/SOUND.DLL           $(DIFFDIR)/SOUND_ours.dll
	@cp $(ORIG)/Modules/CHAIN.DLL  $(DIFFDIR)/CHAIN_retail.dll
	@cp $(ORIG)/Modules/UTIL.DLL   $(DIFFDIR)/UTIL_retail.dll
	@cp $(ORIG)/ENGLISH.FLL        $(DIFFDIR)/ENGLISH.FLL
	@cp $(ORIG)/FLTSIM98.EXE       $(DIFFDIR)/FLTSIM98_host.exe
	@mkdir -p $(DIFFDIR)/cdsound
	@for a in B737_400 Extra300 Bell206B Schweizr Lear45 Sopcamel C182; do \
	    cp $(ORIG)/Aircraft/$$a/sound/sound.cfg $(DIFFDIR)/cdsound/$$a.cfg; done

sounddiff-run: sounddiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine sounddiff.exe $(N)

# DYNAMIC, whole-module: retail and OpenFS98 driven through Linkage and their
# CHAIN lists over random scripts against the dynamic-scenery blocks of the
# CD's twelve airport BGLs, often mutated.  Compares transcript, GLOBAL, heap
# and .data.  See the header of tools/difftest/dynamicdiff.c.
$(DIFFDIR)/dynamicdiff.exe: tools/difftest/dynamicdiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/dynamicdiff.c

dynamicdiff: $(DIFFDIR)/dynamicdiff.exe $(BIN)/DYNAMIC.DLL
	@cp $(ORIG)/Modules/DYNAMIC.DLL $(DIFFDIR)/DYNAMIC_retail.dll
	@cp $(BIN)/DYNAMIC.DLL          $(DIFFDIR)/DYNAMIC_ours.dll
	@cp $(ORIG)/Modules/CHAIN.DLL   $(DIFFDIR)/CHAIN_retail.dll
	@cp $(ORIG)/Modules/UTIL.DLL    $(DIFFDIR)/UTIL_retail.dll
	@mkdir -p $(DIFFDIR)/cdscenery
	@cp $(ORIG)/Scenery/*5.bgl $(DIFFDIR)/cdscenery/

dynamicdiff-run: dynamicdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 3600 wine dynamicdiff.exe $(N)

# ACONTAIN, whole-module: retail and OpenFS98 driven through Linkage over
# random scripts against generated aircraft trees, with retail CHAIN, a stub
# host that passes file I/O through, and fake .SIM and .MDL modules.
# N=... sets the scenario count.
$(DIFFDIR)/acontaindiff.exe: tools/difftest/acontaindiff.c
	@mkdir -p $(DIFFDIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ tools/difftest/acontaindiff.c

acontaindiff: $(DIFFDIR)/acontaindiff.exe $(BIN)/ACONTAIN.DLL
	@cp $(ORIG)/Modules/ACONTAIN.DLL $(DIFFDIR)/ACONTAIN_retail.dll
	@cp $(BIN)/ACONTAIN.DLL          $(DIFFDIR)/ACONTAIN_ours.dll
	@cp $(ORIG)/Modules/CHAIN.DLL    $(DIFFDIR)/CHAIN_retail.dll
	@cp $(ORIG)/ENGLISH.FLL          $(DIFFDIR)/ENGLISH.FLL
	@cp $(ORIG)/FLTSIM98.EXE         $(DIFFDIR)/FLTSIM98_host.exe

acontaindiff-run: acontaindiff
	@cd $(DIFFDIR) && WINEDEBUG=-all timeout 1800 wine acontaindiff.exe $(N)

numericdiff: $(DIFFDIR)/numericdiff.exe
	@cp $(ORIG)/Modules/NUMERIC.DLL $(DIFFDIR)/NUMERIC_retail.dll

numericdiff-run: numericdiff
	@cd $(DIFFDIR) && WINEDEBUG=-all wine numericdiff.exe

difftest: $(DIFFDIR)/slewdiff.exe all
	@cp $(ORIG)/Modules/SLEW.DLL $(DIFFDIR)/SLEW_retail.dll
	@cp $(BIN)/SLEW.DLL          $(DIFFDIR)/SLEW_ours.dll

difftest-run: difftest
	@cd $(DIFFDIR) && WINEDEBUG=-all wine slewdiff.exe $(N)

stage:
	@python3 tools/stage_game.py

setup-map:
	@python3 tools/extract_setup_map.py cd/Setup.exe > docs/setup_map.tsv
	@wc -l < docs/setup_map.tsv | xargs echo "entries:"

# Map the host's service table: where it is, how the services classify by code
# shape, and which libraries each reaches for.
hostmap:
	@python3 tools/hostmap.py $(ORIG)/FLTSIM98.EXE

graph:
	@python3 tools/modgraph.py $(ORIG)/Modules

# side-by-side disassembly of a module's .text, ours vs retail
disasm-%: all
	@echo "=== retail $* ==="; $(OBJDUMP) -d -M intel $(ORIG)/Modules/$*.DLL | sed -n '/^[0-9a-f]* <.text>/,$$p'
	@echo "=== openfs98 $* ==="; $(OBJDUMP) -d -M intel $(BIN)/$*.DLL | sed -n '/^[0-9a-f]* <.text>/,$$p'

compare: all
	@for m in $(MODULES); do \
	  echo "== $$m"; \
	  python3 tools/pesurvey.py $$(make -s orig-module-of M=$$m) $(BIN)/$$m.DLL 2>/dev/null; \
	done

run:
	@echo "modules installed:"; for m in $(MODULES); do \
	  n=$$(make -s installed-name-of M=$$m); \
	  o=$$(make -s orig-module-of M=$$m); \
	  if cmp -s "$$o" "$(FS98_DIR)/Modules/$$n"; then echo "  $$n  retail"; \
	  elif cmp -s $(BIN)/$$m.DLL "$(FS98_DIR)/Modules/$$n"; then echo "  $$n  OpenFS98"; \
	  else echo "  $$n  unknown"; fi; done
	cd $(FS98_DIR) && WINEDEBUG=-all wine $(if $(VD),explorer /desktop=FS98$(comma)$(VD)) FLTSIM98.EXE

# Capture wine's own complaints while you fly.  Run it, get into a flight, move
# around, quit the sim; the log lands in /tmp/fs98-render.log.  fixme/err only,
# so it stays small enough to read.
# Recover from a wedged sim: exclusive-mode DirectDraw can take the window
# manager with it, leaving nothing clickable.  This kills every wine process in
# the prefix, which is always safe here -- nothing else runs in it.
kill:
	-@wineserver -k 2>/dev/null; true
	@echo "wine processes killed"

# Window-manager control of wine's windows.  wm-unmanaged was tried as a fix
# for the 3D view freezing; the real cause was wine swallowing the host's
# divide faults (see sehfix above and docs/wine.md), and an unmanaged window
# never gets keyboard focus, so keys go to whatever window had it.  Leave the
# WM in charge (wm-managed, wine's default) and use vd-on for focus.
wm-unmanaged:
	wine reg add 'HKCU\Software\Wine\X11 Driver' /v Managed /d N /f
	wine reg add 'HKCU\Software\Wine\X11 Driver' /v Decorated /d N /f
	@echo "window manager no longer controls wine windows"

wm-managed:
	-wine reg delete 'HKCU\Software\Wine\X11 Driver' /v Managed /f
	-wine reg delete 'HKCU\Software\Wine\X11 Driver' /v Decorated /f
	@echo "window manager control restored (wine default)"

# The virtual desktop: everything runs inside one wine-drawn desktop window,
# which keeps the host WM away from the sim's window and settles the focus
# fight on the opening screen.  `make run` asks for one on the command line
# (wine explorer /desktop=FS98,$(VD)), so it applies to the sim alone and
# needs no registry setting; `make run VD=` runs without it.  vd-on and
# vd-off only clear the registry settings earlier versions of them made.
VD    ?= 1024x768
comma := ,

vd-on vd-off:
	-@wine reg delete 'HKCU\Software\Wine\AppDefaults\FLTSIM98.EXE\Explorer' /v Desktop /f >/dev/null 2>&1
	-@wine reg delete 'HKCU\Software\Wine\Explorer' /v Desktop /f >/dev/null 2>&1
	@echo "make run uses a $(VD) virtual desktop by itself now; make run VD= for none"

# The sim renders its 3D view at exactly 640x480 and does not survive being
# asked for anything else: resize the window and the view goes black, and the
# new geometry is saved to the config, so it stays black on the next launch.
# 648x533 is 640x480 of client area plus the border, title bar and menu.
#
# This also parks it at the top-left, because the position the game saves can
# leave most of the window outside a 1024x768 virtual desktop -- which is what
# tempts you into resizing it in the first place.
#
# Use the keyboard rather than reaching for the panel with a mouse:
#   F4 full throttle   F3 throttle up   F2 throttle down   F1 cut
#   .  brakes          ctrl+.  parking brakes              Y  slew mode
window-reset:
	@sed -i 's/^Location=.*/Location=0,0,648,533/' "$(FS98_DIR)/FLTSIM98.CFG"
	@sed -i 's/^Maximized=.*/Maximized=0/' "$(FS98_DIR)/FLTSIM98.CFG"
	@grep -E '^Location=|^Maximized=' "$(FS98_DIR)/FLTSIM98.CFG"
	@echo "window set to its native 640x480 client size -- do not resize it"

# Colour depth.  The desktop is 24-bit and the sim renders 16-bit, so every
# windowed frame needs a depth conversion on the way to the screen -- which is
# the kind of blit wine's ddraw is worst at.  8-bit takes a different path
# (palette expansion) that is much better travelled.
mode8:
	@sed -i 's/^Mode=.*/Mode=640x480x8/' "$(FS98_DIR)/FLTSIM98.CFG"
	@grep '^Mode=' "$(FS98_DIR)/FLTSIM98.CFG"

mode16:
	@sed -i 's/^Mode=.*/Mode=640x480x16/' "$(FS98_DIR)/FLTSIM98.CFG"
	@grep '^Mode=' "$(FS98_DIR)/FLTSIM98.CFG"

# A full DirectDraw call trace.  Large and slow -- the sim will crawl -- so fly
# only until the view freezes and then quit.  Worth it because it names the
# call that stops working rather than leaving it to inference.
trace-ddraw:
	@echo "logging to /tmp/fs98-ddraw.log -- fly until the view freezes, then quit"
	cd $(FS98_DIR) && WINEDEBUG=+ddraw,fixme+all,err+all wine FLTSIM98.EXE \
	    > /tmp/fs98-ddraw.log 2>&1 || true
	@echo "log size: $$(du -h /tmp/fs98-ddraw.log | cut -f1)"
	@echo "--- last 40 distinct ddraw calls ---"
	@grep -oE 'ddraw:[a-zA-Z0-9_]+' /tmp/fs98-ddraw.log | tail -2000 \
	    | uniq | tail -40

trace-render:
	@echo "logging to /tmp/fs98-render.log -- fly, then quit the sim"
	cd $(FS98_DIR) && WINEDEBUG=fixme+all,err+all wine FLTSIM98.EXE \
	    > /tmp/fs98-render.log 2>&1 || true
	@echo "--- most common complaints ---"
	@grep -oE '(fixme|err):[a-z0-9_]+:[A-Za-z0-9_]+' /tmp/fs98-render.log \
	    | sort | uniq -c | sort -rn | head -20

# wine's DirectDraw can render through OpenGL (default) or plain GDI.  The GDI
# path is the long-standing fix for mid-90s DirectDraw games whose screen never
# updates, which is exactly the symptom here.
ddraw-gdi:
	wine reg add 'HKCU\Software\Wine\Direct3D' /v renderer /d gdi /f
	@echo "wine ddraw now uses GDI.  'make ddraw-default' puts it back."

ddraw-default:
	-wine reg delete 'HKCU\Software\Wine\Direct3D' /v renderer /f
	@echo "wine ddraw back to its default renderer."

trace:
	cd $(FS98_DIR) && WINEDEBUG=+loaddll wine FLTSIM98.EXE 2>&1 | grep -i Modules

# orig/ is a pristine extraction of the CD, so it is the authoritative source
# for putting a retail module back -- no backup copies to keep in sync, and
# nothing spare left lying in Modules\ where the host enumerates it.
install: all
	@test -d "$(FS98_DIR)/Modules" || { echo "no $(FS98_DIR)/Modules -- set FS98_DIR"; exit 1; }
	@for m in $(MODULES); do \
	  n=$$(make -s installed-name-of M=$$m); \
	  cp $(BIN)/$$m.DLL "$(FS98_DIR)/Modules/$$n"; echo "installed $$m -> $$n"; \
	done

revert:
	@for m in $(MODULES); do \
	  n=$$(make -s installed-name-of M=$$m); \
	  cp "$$(make -s orig-module-of M=$$m)" "$(FS98_DIR)/Modules/$$n"; echo "reverted $$n"; \
	done

installed-name-of:
	@echo $(call installed_name,$(M))

orig-module-of:
	@echo $(call orig_module,$(M))

# boot the game and assert every module still loads
smoke: all
	@tools/smoketest.sh

clean:
	rm -rf $(BIN)

.PHONY: multip-acceptance multip-smoke multip-directplay-breaks multip-worker-breaks multip-breaks multip-receive-breaks multip-send-breaks all stage setup-map graph hostmap compare run trace trace-render trace-ddraw kill window-reset wm-unmanaged wm-managed vd-on vd-off mode8 mode16 ddraw-gdi ddraw-default smoke difftest difftest-run utildiff utildiff-run utildiff-portable numericdiff numericdiff-run chaindiff chaindiff-run atcdiff atcdiff-run weathermdiff weathermdiff-run acontaindiff acontaindiff-run sounddiff sounddiff-run dynamicdiff dynamicdiff-run fegriddiff fegriddiff-run fediff fediff-run fesldiff fesldiff-run boosterdiff boosterdiff-run demodiff demodiff-run windowdiff windowdiff-run g3ddiff g3ddiff-run multipdiff multipdiff-run g3d-break-build g3d-breaks g3dmeshdiff g3dmeshdiff-run g3ddiff-meshc-run hg2dmodeldiff-run panelsdiff panelsdiff-run panelsunit panelsunit-run panels-breaks panels-break-build window-breaks window-break-build maindiff maindiff-run main-break-build main-breaks mpchatdiff mpchatdiff-run mpchat-breaks mpchat-break-build winmsgdiff winmsgdiff-run winmsg-breaks winmsg-break-build realitydiff realitydiff-run reality-breaks reality-break-build analysisdiff analysisdiff-run analysis-breaks analysis-break-build controlsdiff controlsdiff-run controls-breaks controls-break-build advdrvdiff advdrvdiff-run advdrv-cover landmediff landmediff-run facildiff facildiff-run facildiff-fe-run ofstest ofstest-run ofstest-remove sehfix sehfix-install sehfix-remove sehfix-log util-trace install revert installed-name-of orig-module-of clean
