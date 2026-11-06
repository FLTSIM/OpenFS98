# Standalone gauge builds; does not touch the other agent's Options work.
CC := i686-w64-mingw32-gcc
GAUGES_DIR := bin/gauges-diff
N ?= 20000
CFLAGS := -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing
.DEFAULT_GOAL := all-gauges
include tools/gauges-all.mk
.PHONY: gauges gaugesdiff-run gauges-panels-run gauges-breaks gauges-game-run
gauges: $(GAUGES_DIR)/737-400.WARNING_ENUNCIATORS.GAU $(GAUGES_DIR)/CESSNA_182.SUCTION.GAU
$(GAUGES_DIR):
	mkdir -p $@
$(GAUGES_DIR)/warning_data.inc $(GAUGES_DIR)/warning_pointers.inc $(GAUGES_DIR)/warning.res &: tools/gen_warning_data.py tools/dialogmap.py orig/Gauges/737WARNG.GAU | $(GAUGES_DIR)
	python3 -B tools/gen_warning_data.py
$(GAUGES_DIR)/warning-res.o: $(GAUGES_DIR)/warning.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(GAUGES_DIR)/737-400.WARNING_ENUNCIATORS.GAU: src/gauges/warning.c src/gauges/warning.def $(GAUGES_DIR)/warning_data.inc $(GAUGES_DIR)/warning_pointers.inc $(GAUGES_DIR)/warning-res.o
	$(CC) $(CFLAGS) -I$(GAUGES_DIR) -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x32000000 -o $@ src/gauges/warning.c src/gauges/warning.def $(GAUGES_DIR)/warning-res.o -lkernel32
$(GAUGES_DIR)/suction_data.inc $(GAUGES_DIR)/suction_pointers.inc $(GAUGES_DIR)/suction.res &: tools/gen_suction_data.py tools/gen_warning_data.py tools/dialogmap.py orig/Gauges/CESSSUC.GAU | $(GAUGES_DIR)
	python3 -B tools/gen_suction_data.py
$(GAUGES_DIR)/suction-res.o: $(GAUGES_DIR)/suction.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(GAUGES_DIR)/CESSNA_182.SUCTION.GAU: src/gauges/suction.c src/gauges/suction.def $(GAUGES_DIR)/suction_data.inc $(GAUGES_DIR)/suction_pointers.inc $(GAUGES_DIR)/suction-res.o
	$(CC) $(CFLAGS) -I$(GAUGES_DIR) -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x32010000 -o $@ src/gauges/suction.c src/gauges/suction.def $(GAUGES_DIR)/suction-res.o -lkernel32
$(GAUGES_DIR)/gaugesdiff.exe: tools/difftest/gaugesdiff.c | $(GAUGES_DIR)
	$(CC) $(CFLAGS) -o $@ $<
$(GAUGES_DIR)/suctiondiff.exe: tools/difftest/gaugesdiff.c tools/difftest/suction_numeric.inc | $(GAUGES_DIR)
	$(CC) $(CFLAGS) -DSUCTION_DIFF -o $@ $<
.PHONY: suctiondiff-run suction-game-run suction-breaks
suction-breaks: gauges $(GAUGES_DIR)/suctiondiff.exe
	bash tools/run_gaugesdiff.sh --suction-breaks
suctiondiff-run: gauges $(GAUGES_DIR)/suctiondiff.exe
	bash tools/run_gaugesdiff.sh --suction $(N)
	GAUGES_RELOCATE=1 bash tools/run_gaugesdiff.sh --suction $(N)
gaugesdiff-run: gauges $(GAUGES_DIR)/gaugesdiff.exe
	bash tools/run_gaugesdiff.sh $(N)
	GAUGES_RELOCATE=1 bash tools/run_gaugesdiff.sh $(N)
gauges-breaks: gauges $(GAUGES_DIR)/gaugesdiff.exe
	bash tools/run_gaugesdiff.sh --breaks
$(GAUGES_DIR)/gaugespanelsdiff.exe: tools/difftest/panelsdiff.c tools/difftest/gauges_panels.inc tools/difftest/hg2d_model.inc tools/difftest/divfix.h | $(GAUGES_DIR)
	$(CC) -std=gnu99 -O1 -Wall -Wextra -DGAUGES_DIFF -DGAME_DIR='"..\\..\\game"' -o $@ tools/difftest/panelsdiff.c -lgdi32 -luser32
gauges-panels-run: gauges $(GAUGES_DIR)/gaugespanelsdiff.exe
	$(MAKE) bin/PANELS.DLL
	cp bin/PANELS.DLL $(GAUGES_DIR)/PANELS_retail.dll
	cp bin/PANELS.DLL $(GAUGES_DIR)/PANELS_ours.dll
	bash tools/run_gaugesdiff.sh --panels $(N) $(START)
gauges-game-run: gauges
	HG2D_FRAMES_OUT="$(CURDIR)/bin/gauges-frames" HG2D_FRAMES_DISPLAY=:176 HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL HG2D_FRAMES_GAUGE=$(GAUGES_DIR)/737-400.WARNING_ENUNCIATORS.GAU HG2D_FRAMES_GAUGE_RETAIL=orig/Gauges/737WARNG.GAU bash tools/hg2d_frames.sh 'Situatio\HONGKONG.STN' 900
suction-game-run: gauges
	HG2D_FRAMES_OUT="$(CURDIR)/bin/suction-frames" HG2D_FRAMES_DISPLAY=:177 HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL HG2D_FRAMES_GAUGE=$(GAUGES_DIR)/CESSNA_182.SUCTION.GAU HG2D_FRAMES_GAUGE_RETAIL=orig/Gauges/CESSSUC.GAU bash tools/hg2d_frames.sh 'Situatio\STGEORGE.STN' 900
