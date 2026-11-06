# Standalone partial SIM1 verification. No install target or shared build edits.
SIM1_DIR := bin/sim1-diff
SIM1_CC := i686-w64-mingw32-gcc
SIM1_CFLAGS := -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -Iinclude -Isrc/sim1 -I$(SIM1_DIR)
SIM1_SOURCES := src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1_state_stack.S src/sim1/sim1_velocity.S
SIM1_DEPS := $(SIM1_SOURCES) src/sim1/sim1_state.inc tools/sim1.mk $(SIM1_DIR)/sim1_data.inc
N ?= 20000

$(SIM1_DIR)/sim1_data.inc: tools/gen_sim1_data.py tools/impscan.py cd/fs98/Sim/SIM1.sim
	python3 -B tools/gen_sim1_data.py

$(SIM1_DIR)/SIM1_PARTIAL.DLL: $(SIM1_DEPS) src/sim1/sim1.def
	$(SIM1_CC) $(SIM1_CFLAGS) -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o $@ $(SIM1_SOURCES) src/sim1/sim1.def

$(SIM1_DIR)/SIM1_STATE_TEST.DLL: $(SIM1_DEPS) tools/sim1_state_test.def
	$(SIM1_CC) $(SIM1_CFLAGS) -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o $@ $(SIM1_SOURCES) tools/sim1_state_test.def

$(SIM1_DIR)/sim1diff.exe: tools/difftest/sim1diff.c tools/difftest/sim1call.S $(SIM1_DIR)/sim1_data.inc
	$(SIM1_CC) $(SIM1_CFLAGS) -o $@ tools/difftest/sim1diff.c tools/difftest/sim1call.S

$(SIM1_DIR)/sim1statediff.exe: tools/difftest/sim1statediff.c tools/difftest/sim1state_stubs.S tools/difftest/sim1diff.c tools/difftest/sim1call.S $(SIM1_DIR)/sim1_data.inc
	$(SIM1_CC) $(SIM1_CFLAGS) -o $@ tools/difftest/sim1statediff.c tools/difftest/sim1call.S tools/difftest/sim1state_stubs.S

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
