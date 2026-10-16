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
BASE_ATC    := 0x20640000
BASE_CHAIN  := 0x20660000
BASE_DYNAMIC := 0x206a0000
BASE_FE     := 0x206c0000
BASE_GLOBAL := 0x20720000
BASE_NUMERIC := 0x20870000
BASE_SLEW   := 0x208c0000
BASE_SOUND  := 0x208d0000
BASE_UTIL   := 0x208e0000
BASE_WEATHERM := 0x208f0000

MODULES := ACONTAIN ATC CHAIN DYNAMIC GLOBAL NUMERIC SLEW SOUND UTIL WEATHERM

# Setup renames two modules on the way from CD to install, and the game loads
# the *installed* names.  A rebuilt FACIL.DLL copied in as FACIL.DLL would just
# sit there unloaded, so installs go through this map.
installed_name = $(strip \
  $(if $(filter FACIL,$(1)),FACILITIES.DLL,\
  $(if $(filter MULTIP,$(1)),MULTIPLAYER.DLL,$(1).DLL)))

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

# FE, the scenery file engine, laid out the same way.  Not in MODULES yet:
# it is being built up a part at a time (docs/preflight.md, round 4), and a
# partial FE must never be installed.
$(BIN)/FE.DLL: src/fe/fe.c src/fe/fe.def src/fe/fe_data.inc $(BIN)/fe.res
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -Isrc/fe $(LDFLAGS) \
	      -Wl,--image-base=$(BASE_FE) \
	      -o $@ src/fe/fe.c $(BIN)/fe.res src/fe/fe.def -lkernel32

$(BIN)/fe.res: src/fe/fe.rc
	@mkdir -p $(BIN)
	$(RC) $< -O coff -o $@

src/fe/fe_data.inc: $(ORIG)/Modules/FE.DLL tools/gen_fe_data.py
	@python3 tools/gen_fe_data.py $< $@

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

# FE's hand-written assembly engine, whole-module: both DLLs driven through
# Linkage over the installed scenery tree, with retail UTIL and a stub host
# passing file I/O through to that fixture.  Prefer a fully extracted orig/
# tree; fall back to the staged game/ tree used by smoke tests.
FE_FIXTURE ?= $(if $(wildcard $(ORIG)/Scenery/world.vis),../../$(ORIG),$(if $(wildcard game/Scenery/world.vis),../../game,))

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
	  python3 tools/pesurvey.py $(ORIG)/Modules/$$m.DLL $(BIN)/$$m.DLL 2>/dev/null; \
	done

run:
	@echo "modules installed:"; for m in $(MODULES); do \
	  n=$$(make -s installed-name-of M=$$m); \
	  if cmp -s $(ORIG)/Modules/$$m.DLL "$(FS98_DIR)/Modules/$$n"; then echo "  $$n  retail"; \
	  elif cmp -s $(BIN)/$$m.DLL "$(FS98_DIR)/Modules/$$n"; then echo "  $$n  OpenFS98"; \
	  else echo "  $$n  unknown"; fi; done
	cd $(FS98_DIR) && WINEDEBUG=-all wine FLTSIM98.EXE

# Capture wine's own complaints while you fly.  Run it, get into a flight, move
# around, quit the sim; the log lands in /tmp/fs98-render.log.  fixme/err only,
# so it stays small enough to read.
# Recover from a wedged sim: exclusive-mode DirectDraw can take the window
# manager with it, leaving nothing clickable.  This kills every wine process in
# the prefix, which is always safe here -- nothing else runs in it.
kill:
	-@pkill -9 -f fltsim98 2>/dev/null; pkill -9 -f wineserver 2>/dev/null; true
	@echo "wine processes killed"

# WineHQ's documented fix for FS98's 3D view: either run in a virtual desktop,
# or stop the window manager owning wine's windows.  Letting the WM manage the
# window is what loses the DirectDraw surfaces mid-flight -- the trace shows
# Restore calls followed immediately by faults in the render path, twice.
#
# wm-unmanaged is the less intrusive of the two: no extra desktop window, the
# sim keeps its own frame, but the WM stops interfering.  Undecorated too,
# since a decorated-but-unmanaged window behaves oddly.
wm-unmanaged:
	wine reg add 'HKCU\Software\Wine\X11 Driver' /v Managed /d N /f
	wine reg add 'HKCU\Software\Wine\X11 Driver' /v Decorated /d N /f
	@echo "window manager no longer controls wine windows"

wm-managed:
	-wine reg delete 'HKCU\Software\Wine\X11 Driver' /v Managed /f
	-wine reg delete 'HKCU\Software\Wine\X11 Driver' /v Decorated /f
	@echo "window manager control restored (wine default)"

# The other documented fix, if wm-unmanaged is not enough.  Everything runs
# inside one wine-drawn desktop window, which is what keeps the host WM away
# from the sim's own window entirely.
vd-on:
	wine reg add 'HKCU\Software\Wine\Explorer\Desktops' /v Default /d 1024x768 /f
	wine reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d Default /f
	@echo "virtual desktop on (1024x768)"

vd-off:
	-wine reg delete 'HKCU\Software\Wine\Explorer' /v Desktop /f
	@echo "virtual desktop off"

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
	  cp $(ORIG)/Modules/$$m.DLL "$(FS98_DIR)/Modules/$$n"; echo "reverted $$n"; \
	done

installed-name-of:
	@echo $(call installed_name,$(M))

# boot the game and assert every module still loads
smoke: all
	@tools/smoketest.sh

clean:
	rm -rf $(BIN)

.PHONY: all stage setup-map graph hostmap compare run trace trace-render trace-ddraw kill window-reset wm-unmanaged wm-managed vd-on vd-off mode8 mode16 ddraw-gdi ddraw-default smoke difftest difftest-run utildiff utildiff-run utildiff-portable numericdiff numericdiff-run chaindiff chaindiff-run atcdiff atcdiff-run weathermdiff weathermdiff-run acontaindiff acontaindiff-run sounddiff sounddiff-run dynamicdiff dynamicdiff-run fegriddiff fegriddiff-run fediff fediff-run ofstest ofstest-run ofstest-remove util-trace install revert installed-name-of clean
