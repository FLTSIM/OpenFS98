# Standalone while MAIN owns the shared Makefile. Run from the repository root.
CC := i686-w64-mingw32-gcc
HG2D_DIR := bin/hg2d-diff
N ?= 20000
SUITE ?= all
.PHONY: hg2d hg2ddiff-run
hg2d: $(HG2D_DIR)/HG2D_ours.dll $(HG2D_DIR)/hg2ddiff.exe
src/hg2d/hg2d_data.inc src/hg2d/hg2d_pointers.inc &: tools/gen_hg2d_data.py orig/Modules/HG2D.DLL
	python3 tools/gen_hg2d_data.py
$(HG2D_DIR):
	mkdir -p $@
$(HG2D_DIR)/HG2D_ours.dll: src/hg2d/hg2d.c src/hg2d/hg2d_palette.inc src/hg2d/hg2d_draw.inc src/hg2d/hg2d.def src/hg2d/hg2d_data.inc src/hg2d/hg2d_pointers.inc | $(HG2D_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--image-base=0x30000000 -Wl,--disable-dynamicbase -o $@ src/hg2d/hg2d.c src/hg2d/hg2d.def -lgcc
$(HG2D_DIR)/hg2ddiff.exe: tools/difftest/hg2ddiff.c tools/difftest/hg2d_palette_suite.inc tools/difftest/hg2d_draw_suite.inc src/hg2d/hg2d_pointers.inc | $(HG2D_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -o $@ $<
$(HG2D_DIR)/HG2D_retail.dll: orig/Modules/HG2D.DLL | $(HG2D_DIR)
	cp $< $@
hg2ddiff-run: hg2d $(HG2D_DIR)/HG2D_retail.dll $(HG2D_DIR)/UTIL_retail.dll
	cd $(HG2D_DIR) && WINEDEBUG=-all timeout 1800 wine hg2ddiff.exe $(N) $(SUITE)

$(HG2D_DIR)/UTIL_retail.dll: orig/Modules/UTIL.DLL | $(HG2D_DIR)
	cp $< $@
