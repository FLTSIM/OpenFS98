# Standalone while MAIN owns the shared Makefile. Run from the repository root.
CC := i686-w64-mingw32-gcc
HG2D_DIR := bin/hg2d-diff
N ?= 20000
SUITE ?= all
HG2D_TIMEOUT ?= 7200
.PHONY: hg2d hg2ddiff-run
hg2d: $(HG2D_DIR)/HG2D_ours.dll $(HG2D_DIR)/hg2ddiff.exe
src/hg2d/hg2d_data.inc src/hg2d/hg2d_pointers.inc &: tools/gen_hg2d_data.py orig/Modules/HG2D.DLL
	python3 tools/gen_hg2d_data.py
$(HG2D_DIR):
	mkdir -p $@
$(HG2D_DIR)/HG2D_ours.dll: src/hg2d/hg2d.c src/hg2d/hg2d_palette.inc src/hg2d/hg2d_draw.inc src/hg2d/hg2d_polygon.inc src/hg2d/hg2d_fill.inc src/hg2d/hg2d_textured.inc src/hg2d/hg2d_perspective.inc src/hg2d/hg2d_affine.inc src/hg2d/hg2d_gouraud.inc src/hg2d/hg2d_blit.inc src/hg2d/hg2d_disc.inc src/hg2d/hg2d.def src/hg2d/hg2d_data.inc src/hg2d/hg2d_pointers.inc | $(HG2D_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--image-base=0x30000000 -Wl,--disable-dynamicbase -o $@ src/hg2d/hg2d.c src/hg2d/hg2d.def -lgcc
$(HG2D_DIR)/hg2ddiff.exe: tools/difftest/hg2ddiff.c tools/difftest/hg2d_palette_suite.inc tools/difftest/hg2d_draw_suite.inc tools/difftest/hg2d_polygon_suite.inc tools/difftest/hg2d_fill_suite.inc tools/difftest/hg2d_textured_suite.inc tools/difftest/hg2d_perspective_suite.inc tools/difftest/hg2d_affine_suite.inc tools/difftest/hg2d_gouraud_suite.inc tools/difftest/hg2d_blit_suite.inc tools/difftest/hg2d_disc_suite.inc src/hg2d/hg2d_pointers.inc | $(HG2D_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -o $@ $<
$(HG2D_DIR)/HG2D_retail.dll: orig/Modules/HG2D.DLL | $(HG2D_DIR)
	cp $< $@
hg2ddiff-run: hg2d $(HG2D_DIR)/HG2D_retail.dll $(HG2D_DIR)/UTIL_retail.dll
	cd $(HG2D_DIR) && WINEDEBUG=-all timeout $(HG2D_TIMEOUT) wine hg2ddiff.exe $(N) $(SUITE)

$(HG2D_DIR)/UTIL_retail.dll: orig/Modules/UTIL.DLL | $(HG2D_DIR)
	cp $< $@
