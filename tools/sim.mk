# Sim is isolated from the MAIN/HOST shared build.
CC := i686-w64-mingw32-gcc
SIM_DIR := bin/sim-diff
N ?= 20000
SEED ?= 0x73a915e2
MODE ?= normal
.PHONY: simevents simdiff-run sim-breaks
simevents: $(SIM_DIR)/simdiff.exe
$(SIM_DIR):
	mkdir -p $@
src/sim/sim_tables.inc src/sim/sim_hooks.inc &: tools/gen_sim_tables.py tools/dialogmap.py orig/English/Sim.DLL
	python3 -B tools/gen_sim_tables.py
$(SIM_DIR)/simdiff.exe: tools/difftest/simdiff.c src/sim/sim_events.c src/sim/sim_objects.c src/sim/sim_internal.h src/sim/sim_unwind.h src/sim/sim_tables.inc src/sim/sim_hooks.inc | $(SIM_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/simdiff.c src/sim/sim_events.c src/sim/sim_objects.c -lntdll
simdiff-run: simevents
	bash tools/run_simdiff.sh $(N) $(SEED) $(MODE)
sim-breaks: simevents
	bash tools/run_simdiff.sh --breaks $(N)
