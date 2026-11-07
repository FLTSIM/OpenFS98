# SIMSUI.DLL: the differential harness now, the native DLL later; the shared Makefile installs it.
CC := i686-w64-mingw32-gcc
SIMSUI_DIR := bin/simsui-diff
N ?= 2000
SEED ?= 0x51c5e1
SIMSUI_SOURCES := src/simsui/simsui_crt.c src/simsui/simsui_util.c src/simsui/simsui_dib.c src/simsui/simsui_fonts.c src/simsui/simsui_dibsection.c src/simsui/simsui_small.c src/simsui/simsui_font.c
SIMSUI_HEADERS := src/simsui/simsui_internal.h src/simsui/simsui_tables.inc
SIMSUI_HARNESS := tools/difftest/simsuidiff.c tools/difftest/simsui_semantics.inc tools/difftest/simsui_operations.inc
# loops stay loops, in retail's order: no library calls or vectors made of them
SIMSUI_CFLAGS := -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks \
                 -fno-tree-loop-distribute-patterns -fno-tree-vectorize
.PHONY: simsuidiff simsuidiff-run simsui-tables simsui-breaks
simsuidiff: $(SIMSUI_DIR)/simsuidiff.exe
$(SIMSUI_DIR):
	mkdir -p $@
simsui-tables: src/simsui/simsui_tables.inc
src/simsui/simsui_tables.inc: tools/gen_simsui_tables.py tools/dialogmap.py orig/SIMSUI.DLL cd/mfc42.dll
	python3 -B tools/gen_simsui_tables.py
$(SIMSUI_DIR)/simsuidiff.exe: $(SIMSUI_HARNESS) $(SIMSUI_SOURCES) $(SIMSUI_HEADERS) | $(SIMSUI_DIR)
	$(CC) $(SIMSUI_CFLAGS) -Wl,--image-base=0x400000 -o $@ tools/difftest/simsuidiff.c $(SIMSUI_SOURCES)
simsuidiff-run: simsuidiff
	bash tools/run_simsuidiff.sh $(N) $(SEED)
simsui-breaks: simsuidiff
	bash tools/run_simsuidiff.sh --breaks $(N)
