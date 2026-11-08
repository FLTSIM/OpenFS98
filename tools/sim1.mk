# SIM1 build, differential verification and installation.
SIM1_DIR := bin/sim1-diff
SIM1_CC := i686-w64-mingw32-gcc
SIM1_CFLAGS := -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -Iinclude -Isrc/sim1 -I$(SIM1_DIR)
SIM1_SOURCES := src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S
SIM1_DEPS := $(SIM1_SOURCES) src/sim1/sim1_state.inc src/sim1/sim1_air.inc tools/sim1.mk $(SIM1_DIR)/sim1_data.inc
N ?= 20000
FS98_DIR ?= $(CURDIR)/game
SIM1_LDFLAGS := -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base,0x22000000
SIM1_LIBS := -lmsvcrt -lkernel32 -lgcc
SIM1_COMPLETE_DEPS := $(SIM1_DEPS) $(SIM1_DIR)/lift/sim1_lift.o $(SIM1_DIR)/lift/sim1_entries.S $(SIM1_DIR)/lift/resource.o src/sim1/sim1_lift_native.S

$(SIM1_DIR)/lift/sim1_lift.c $(SIM1_DIR)/lift/sim1_entries.S $(SIM1_DIR)/lift/sim1.res &: tools/gen_sim1_lift.py tools/gen_mdl_crash.py tools/gen_gauges.py $(SIM1_DIR)/sim1_data.inc
	python3 -B tools/gen_sim1_lift.py > $(SIM1_DIR)/lift-summary.json

$(SIM1_DIR)/lift/sim1_lift.o: $(SIM1_DIR)/lift/sim1_lift.c src/sim1/sim1_lift.h src/mdl/mdl_crash_lift.h
	$(SIM1_CC) $(SIM1_CFLAGS) -masm=intel -Wno-unused-function -Wno-unused-label -c $< -o $@

$(SIM1_DIR)/lift/resource.o: $(SIM1_DIR)/lift/sim1.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(SIM1_DIR)/SIM1_COMPLETE.DLL: $(SIM1_COMPLETE_DEPS) src/sim1/sim1.def
	$(SIM1_CC) $(SIM1_CFLAGS) -DSIM1_COMPLETE $(SIM1_LDFLAGS) -o $@ $(SIM1_SOURCES) $(SIM1_DIR)/lift/sim1_lift.o $(SIM1_DIR)/lift/sim1_entries.S src/sim1/sim1_lift_native.S $(SIM1_DIR)/lift/resource.o src/sim1/sim1.def $(SIM1_LIBS)

.PHONY: sim1-complete
sim1-complete: $(SIM1_DIR)/SIM1_COMPLETE.DLL

$(SIM1_DIR)/SIM1_COMPLETE_TEST.DLL: $(SIM1_COMPLETE_DEPS) tools/sim1_complete_test.def tools/difftest/sim1nativeprobe.c
	$(SIM1_CC) $(SIM1_CFLAGS) -Wno-unused-function -DSIM1_COMPLETE $(SIM1_LDFLAGS) -o $@ $(SIM1_SOURCES) tools/difftest/sim1nativeprobe.c $(SIM1_DIR)/lift/sim1_lift.o $(SIM1_DIR)/lift/sim1_entries.S src/sim1/sim1_lift_native.S $(SIM1_DIR)/lift/resource.o tools/sim1_complete_test.def $(SIM1_LIBS)

$(SIM1_DIR)/sim1decodediff.exe: tools/difftest/sim1decodediff.c tools/difftest/sim1diff.c tools/difftest/sim1call.S tools/difftest/sim1decode_stubs.S
	$(SIM1_CC) $(SIM1_CFLAGS) -Wno-unused-function -o $@ tools/difftest/sim1decodediff.c tools/difftest/sim1call.S tools/difftest/sim1decode_stubs.S

.PHONY: sim1decode-run
sim1decode-run: $(SIM1_DIR)/SIM1_COMPLETE_TEST.DLL $(SIM1_DIR)/sim1decodediff.exe
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1decodediff.exe retail-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1decodediff.exe ours-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1decodediff.exe relocate
	@for order in retail-first ours-first relocate; do \
	 WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1decodediff.exe $$order $(SIM1_DIR)/SIM1_COMPLETE_TEST.DLL setup || exit $$?; \
	done

.PHONY: sim1-install sim1-revert sim1-regression-run
sim1-install: sim1-complete
	python3 -B tools/stage_sim1.py "$(FS98_DIR)"

sim1-revert: $(SIM1_DIR)/sim1_data.inc
	python3 -B tools/stage_sim1.py "$(FS98_DIR)" --retail

sim1-regression-run: sim1-complete $(SIM1_DIR)/SIM1_COMPLETE_TEST.DLL $(SIM1_DIR)/sim1diff.exe $(SIM1_DIR)/sim1statediff.exe
	@for order in retail-first ours-first relocate; do \
	 WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1diff.exe $(N) $$order $(SIM1_DIR)/SIM1_COMPLETE.DLL || exit $$?; \
	 WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1statediff.exe $(N) $$order $(SIM1_DIR)/SIM1_COMPLETE_TEST.DLL || exit $$?; \
	done

.PHONY: sim1-frame-observer sim1frames-run
sim1-frame-observer:
	$(MAKE) -f tools/hg2d.mk bin/hg2d-diff/FRAMELOCK.DLL
	$(MAKE) sehfix

sim1frames-run: sim1-complete sim1-frame-observer
	FS98_DIR="$(CURDIR)/game" HG2D_FRAMES_SIM1="$(CURDIR)/$(SIM1_DIR)/SIM1_COMPLETE.DLL" HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL HG2D_FRAMES_OUT=$(SIM1_DIR)/frames HG2D_FRAMES_DISPLAY=:67 HG2D_FRAMES_NOSOUND=1 tools/hg2d_frames.sh 'Situatio\STGEORGE.STN' 900

.PHONY: sim1flights-run
sim1flights-run: sim1-complete sim1-frame-observer
	python3 -B tools/fly_scripts.py $(SIM1_DIR)/scripts
	FS98_DIR="$(CURDIR)/game" HG2D_FRAMES_ONLY=SIM1 HG2D_FRAMES_SIM1="$(CURDIR)/$(SIM1_DIR)/SIM1_COMPLETE.DLL" HG2D_FRAMES_NOSOUND=1 FRAMES_MANY_OUT=$(SIM1_DIR)/flights FRAMES_MANY_PASSES=1200 FRAMES_MANY_JOBS=8 tools/frames_many.sh $(SIM1_DIR)/scripts/controls.txt
	@test "$$(grep -c ': match' $(SIM1_DIR)/flights/summary.txt)" -eq 74

.PHONY: sim1-lift-breaks
sim1-lift-breaks: sim1-complete sim1-frame-observer $(SIM1_DIR)/SIM1_COMPLETE_TEST.DLL $(SIM1_DIR)/sim1decodediff.exe
	python3 -B tools/fly_scripts.py $(SIM1_DIR)/scripts
	python3 -B tools/sim1_lift_breaks.py

$(SIM1_DIR)/sim1_data.inc: tools/gen_sim1_data.py tools/impscan.py cd/fs98/Sim/SIM1.sim
	python3 -B tools/gen_sim1_data.py

$(SIM1_DIR)/SIM1_PARTIAL.DLL: $(SIM1_DEPS) src/sim1/sim1.def
	$(SIM1_CC) $(SIM1_CFLAGS) -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o $@ $(SIM1_SOURCES) src/sim1/sim1.def

$(SIM1_DIR)/SIM1_STATE_TEST.DLL: $(SIM1_DEPS) tools/sim1_state_test.def
	$(SIM1_CC) $(SIM1_CFLAGS) -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o $@ $(SIM1_SOURCES) tools/sim1_state_test.def

$(SIM1_DIR)/SIM1_AIR_TEST.DLL: $(SIM1_DEPS) tools/sim1_air_test.def
	$(SIM1_CC) $(SIM1_CFLAGS) -DSIM1_AIR_TEST -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o $@ $(SIM1_SOURCES) tools/sim1_air_test.def

$(SIM1_DIR)/sim1diff.exe: tools/difftest/sim1diff.c tools/difftest/sim1call.S $(SIM1_DIR)/sim1_data.inc
	$(SIM1_CC) $(SIM1_CFLAGS) -o $@ tools/difftest/sim1diff.c tools/difftest/sim1call.S

$(SIM1_DIR)/sim1statediff.exe: tools/difftest/sim1statediff.c tools/difftest/sim1state_stubs.S tools/difftest/sim1diff.c tools/difftest/sim1call.S $(SIM1_DIR)/sim1_data.inc
	$(SIM1_CC) $(SIM1_CFLAGS) -o $@ tools/difftest/sim1statediff.c tools/difftest/sim1call.S tools/difftest/sim1state_stubs.S

$(SIM1_DIR)/sim1airdiff.exe: tools/difftest/sim1airdiff.c tools/difftest/sim1air_stubs.S tools/difftest/sim1statediff.c tools/difftest/sim1state_stubs.S tools/difftest/sim1diff.c tools/difftest/sim1call.S $(SIM1_DIR)/sim1_data.inc
	$(SIM1_CC) $(SIM1_CFLAGS) -o $@ tools/difftest/sim1airdiff.c tools/difftest/sim1call.S tools/difftest/sim1state_stubs.S tools/difftest/sim1air_stubs.S

.PHONY: sim1air-run
sim1air-run: $(SIM1_DIR)/SIM1_AIR_TEST.DLL $(SIM1_DIR)/sim1airdiff.exe
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1airdiff.exe $(N) retail-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1airdiff.exe $(N) ours-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1airdiff.exe $(N) relocate

.PHONY: sim1air-breaks
sim1air-breaks: $(SIM1_DIR)/SIM1_AIR_TEST.DLL $(SIM1_DIR)/sim1airdiff.exe
	python3 -B tools/sim1_air_breaks.py

.PHONY: sim1state-run
sim1state-run: $(SIM1_DIR)/SIM1_STATE_TEST.DLL $(SIM1_DIR)/sim1statediff.exe
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1statediff.exe $(N) retail-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1statediff.exe $(N) ours-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1statediff.exe $(N) relocate

.PHONY: sim1state-breaks
sim1state-breaks: $(SIM1_DIR)/SIM1_STATE_TEST.DLL $(SIM1_DIR)/sim1statediff.exe
	python3 -B tools/sim1_state_breaks.py

.PHONY: sim1-partial sim1diff-run sim1-breaks
sim1-partial: $(SIM1_DIR)/SIM1_PARTIAL.DLL $(SIM1_DIR)/sim1diff.exe

sim1diff-run: sim1-partial
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1diff.exe $(N) retail-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1diff.exe $(N) ours-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1diff.exe $(N) relocate

sim1-breaks: sim1-partial
	python3 -B tools/sim1_breaks.py

# The decompilation: the hand-written routines against retail's, one at a time
# (tools/difftest/sim1rdiff.c over in-flight snapshots; docs/modules/sim1.md "Decompilation").
SIM1R_SOURCES := src/sim1/sim1_small.c
SIM1R_HEADERS := src/sim1/sim1_internal.h src/sim1/sim1_routines.h src/sim1/sim1_tables.inc
SIM1R_CFLAGS := -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-math-errno \
                -fno-tree-loop-distribute-patterns -fno-tree-vectorize
SIM1R_N ?= 2000
SIM1R_SEED ?= 0x51e1
.PHONY: sim1rdiff sim1rdiff-run sim1-tables sim1r-breaks sim1-snapshots
sim1-snapshots: sim1-complete sim1-frame-observer
	bash tools/sim1_snapshots.sh
sim1-tables: src/sim1/sim1_tables.inc
src/sim1/sim1_tables.inc: tools/gen_sim1_tables.py tools/dialogmap.py orig/Sim/SIM1.sim
	python3 -B tools/gen_sim1_tables.py
$(SIM1_DIR)/sim1rdiff.exe: tools/difftest/sim1rdiff.c tools/difftest/sim1r_operations.inc $(SIM1R_SOURCES) $(SIM1R_HEADERS)
	@mkdir -p $(SIM1_DIR)
	$(SIM1_CC) $(SIM1R_CFLAGS) -Wl,--image-base=0x400000 -o $@ tools/difftest/sim1rdiff.c $(SIM1R_SOURCES)
sim1rdiff: $(SIM1_DIR)/sim1rdiff.exe
sim1rdiff-run: sim1rdiff
	WINEDEBUG=-all timeout 7200 wine $(SIM1_DIR)/sim1rdiff.exe $(SIM1R_N) $(SIM1R_SEED)
sim1r-breaks: sim1rdiff
	python3 -B tools/sim1r_breaks.py

# The native SIM1.sim: the decompiled routines (src/sim1/sim1_small.c) over retail's data
# (src/sim1/sim1_native.c; tools/gen_sim1_native.py takes the bytes from the CD's SIM1).
SIM1_NATIVE_DIR := bin/sim1-native
SIM1_NATIVE_CFLAGS := $(SIM1R_CFLAGS) -Iinclude -Isrc/sim1 -I$(SIM1_NATIVE_DIR)
.PHONY: sim1-native
sim1-native: $(SIM1_NATIVE_DIR)/SIM1_ours.sim
$(SIM1_NATIVE_DIR)/sim1_data.inc $(SIM1_NATIVE_DIR)/sim1_rdata.inc $(SIM1_NATIVE_DIR)/sim1.res &: tools/gen_sim1_native.py tools/impscan.py orig/Sim/SIM1.sim
	python3 -B tools/gen_sim1_native.py
$(SIM1_NATIVE_DIR)/sim1-res.o: $(SIM1_NATIVE_DIR)/sim1.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(SIM1_NATIVE_DIR)/SIM1_ours.sim: src/sim1/sim1_native.c $(SIM1R_SOURCES) $(SIM1R_HEADERS) src/sim1/sim1.def $(SIM1_NATIVE_DIR)/sim1_data.inc $(SIM1_NATIVE_DIR)/sim1_rdata.inc $(SIM1_NATIVE_DIR)/sim1-res.o
	$(SIM1_CC) $(SIM1_NATIVE_CFLAGS) -shared -nostdlib -Wl,--entry=_sim1_native_entry@12 -Wl,--disable-dynamicbase \
	    -Wl,--disable-nxcompat -Wl,--image-base,0x22000000 -o $@ src/sim1/sim1_native.c $(SIM1R_SOURCES) \
	    $(SIM1_NATIVE_DIR)/sim1-res.o src/sim1/sim1.def -lmsvcrt -lkernel32 -lgcc
# its game acceptance: the 74 scripted flights, retail SIM1 against it, frame for frame
.PHONY: sim1native-flights
sim1native-flights: sim1-native sim1-frame-observer
	python3 -B tools/fly_scripts.py $(SIM1_NATIVE_DIR)/scripts
	FS98_DIR="$(CURDIR)/game" HG2D_FRAMES_ONLY=SIM1 HG2D_FRAMES_SIM1="$(CURDIR)/$(SIM1_NATIVE_DIR)/SIM1_ours.sim" HG2D_FRAMES_NOSOUND=1 FRAMES_MANY_OUT=$(SIM1_NATIVE_DIR)/flights FRAMES_MANY_PASSES=1200 FRAMES_MANY_JOBS=8 tools/frames_many.sh $(SIM1_NATIVE_DIR)/scripts/controls.txt
	@test "$$(grep -c ': match' $(SIM1_NATIVE_DIR)/flights/summary.txt)" -eq 74
