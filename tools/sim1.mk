# Standalone partial SIM1 verification. No install target or shared build edits.
SIM1_DIR := bin/sim1-diff
SIM1_CC := i686-w64-mingw32-gcc
SIM1_CFLAGS := -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -Iinclude -I$(SIM1_DIR)
N ?= 20000

$(SIM1_DIR)/sim1_data.inc: tools/gen_sim1_data.py tools/impscan.py cd/fs98/Sim/SIM1.sim
	python3 -B tools/gen_sim1_data.py

$(SIM1_DIR)/SIM1_PARTIAL.DLL: src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1.def tools/sim1.mk $(SIM1_DIR)/sim1_data.inc
	$(SIM1_CC) $(SIM1_CFLAGS) -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x22000000 -o $@ src/sim1/sim1.c src/sim1/sim1_stack.S src/sim1/sim1.def

$(SIM1_DIR)/sim1diff.exe: tools/difftest/sim1diff.c tools/difftest/sim1call.S $(SIM1_DIR)/sim1_data.inc
	$(SIM1_CC) $(SIM1_CFLAGS) -o $@ tools/difftest/sim1diff.c tools/difftest/sim1call.S

.PHONY: sim1-partial sim1diff-run sim1-breaks
sim1-partial: $(SIM1_DIR)/SIM1_PARTIAL.DLL $(SIM1_DIR)/sim1diff.exe

sim1diff-run: sim1-partial
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1diff.exe $(N) retail-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1diff.exe $(N) ours-first
	WINEPREFIX="$(CURDIR)/$(SIM1_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all flock $(SIM1_DIR)/wine.lock timeout 180s wine $(SIM1_DIR)/sim1diff.exe $(N) relocate

sim1-breaks: sim1-partial
	python3 -B tools/sim1_breaks.py
