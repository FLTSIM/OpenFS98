# All 120 gauges, explicit C generated from the checked reference listing.
GAUGES_ALL_DIR := bin/gauges-all
.PHONY: all-gauges all-gauges-check all-gauges-hangs all-gauges-panels all-gauges-breaks
$(GAUGES_ALL_DIR)/generated.stamp: tools/gen_gauges.py tools/dialogmap.py docs/setup_map.tsv $(wildcard orig/Gauges/*.GAU)
	python3 -B tools/gen_gauges.py
	touch $@
$(GAUGES_ALL_DIR)/built.stamp: $(GAUGES_ALL_DIR)/generated.stamp tools/build_gauges.py src/gauges/gauge_lift.h
	python3 -B tools/build_gauges.py
	touch $@
all-gauges: $(GAUGES_ALL_DIR)/built.stamp
$(GAUGES_ALL_DIR)/allgaugesdiff.exe: tools/difftest/allgaugesdiff.c $(GAUGES_ALL_DIR)/built.stamp
	$(CC) -std=gnu99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -I$(GAUGES_ALL_DIR) -o $@ $<
all-gauges-check: all-gauges $(GAUGES_ALL_DIR)/allgaugesdiff.exe
	bash tools/run_gaugesdiff.sh --all-batch --count $(N)
all-gauges-hangs: all-gauges $(GAUGES_ALL_DIR)/allgaugesdiff.exe
	bash tools/run_gaugesdiff.sh --all-batch --hangs
all-gauges-breaks: all-gauges $(GAUGES_ALL_DIR)/allgaugesdiff.exe
	bash tools/run_gaugesdiff.sh --all-mutations
$(GAUGES_ALL_DIR)/allpaneldiff.exe: tools/difftest/panelsdiff.c tools/difftest/gauges_panels.inc tools/difftest/hg2d_model.inc tools/difftest/divfix.h $(GAUGES_ALL_DIR)/built.stamp
	$(CC) -std=gnu99 -O1 -Wall -Wextra -DGAUGES_DIFF -DGAUGES_ALL_DIFF -DGAME_DIR='"..\\..\\game"' -I$(GAUGES_ALL_DIR) -o $@ tools/difftest/panelsdiff.c -lgdi32 -luser32
all-gauges-panels: all-gauges $(GAUGES_ALL_DIR)/allpaneldiff.exe
	$(MAKE) bin/PANELS.DLL
	cp bin/PANELS.DLL $(GAUGES_ALL_DIR)/PANELS_retail.dll
	cp bin/PANELS.DLL $(GAUGES_ALL_DIR)/PANELS_ours.dll
	bash tools/run_gaugesdiff.sh --all-panels $(N) $(START)
