# SIMSUI.DLL: the differential harness now, the native DLL later; the shared Makefile installs it.
CC := i686-w64-mingw32-gcc
SIMSUI_DIR := bin/simsui-diff
N ?= 2000
SEED ?= 0x51c5e1
SIMSUI_SOURCES := src/simsui/simsui_crt.c src/simsui/simsui_util.c src/simsui/simsui_dib.c src/simsui/simsui_fonts.c src/simsui/simsui_dibsection.c src/simsui/simsui_small.c src/simsui/simsui_font.c src/simsui/simsui_button.c src/simsui/simsui_check.c src/simsui/simsui_combo.c src/simsui/simsui_dialog.c src/simsui/simsui_tabs.c src/simsui/simsui_edit.c src/simsui/simsui_groupbox.c src/simsui/simsui_listbox.c src/simsui/simsui_picture.c src/simsui/simsui_radio.c src/simsui/simsui_shadowbutton.c src/simsui/simsui_shadow.c src/simsui/simsui_slider.c src/simsui/simsui_text.c src/simsui/simsui_textart.c src/simsui/simsui_hotkey.c src/simsui/simsui_cdib.c
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
# The native DLL: our SIMSUI over the CD's MFC42, retail's data image bound to our code at load
SIMSUI_NATIVE_DIR := bin/simsui-native
SIMSUI_NATIVE_SOURCES := $(SIMSUI_SOURCES) src/simsui/simsui_native.c
SIMSUI_NATIVE_OBJECTS := $(patsubst src/simsui/%.c,$(SIMSUI_NATIVE_DIR)/%.o,$(SIMSUI_NATIVE_SOURCES))
.PHONY: simsui simsui-native-tables
simsui: $(SIMSUI_NATIVE_DIR)/SIMSUI_ours.dll
$(SIMSUI_NATIVE_DIR):
	mkdir -p $@
$(SIMSUI_NATIVE_DIR)/%.o: src/simsui/%.c $(SIMSUI_HEADERS) src/simsui/simsui_native_tables.inc | $(SIMSUI_NATIVE_DIR)
	$(CC) $(SIMSUI_CFLAGS) -c -o $@ $<
simsui-native-tables: src/simsui/simsui_native_tables.inc
src/simsui/simsui_native_tables.inc: tools/gen_simsui_native.py tools/difftest/simsui_operations.inc src/simsui/simsui_tables.inc orig/SIMSUI.DLL
	python3 -B tools/gen_simsui_native.py
# the image, the exports and the resources, the .def naming the objects' own symbols
$(SIMSUI_NATIVE_DIR)/simsui.def $(SIMSUI_NATIVE_DIR)/simsui_image.S $(SIMSUI_NATIVE_DIR)/simsui.res &: tools/gen_simsui_native.py $(SIMSUI_NATIVE_OBJECTS) orig/SIMSUI.DLL
	python3 -B tools/gen_simsui_native.py $(SIMSUI_NATIVE_OBJECTS)
$(SIMSUI_NATIVE_DIR)/simsui_image.o: $(SIMSUI_NATIVE_DIR)/simsui_image.S
	cd $(SIMSUI_NATIVE_DIR) && $(CC) -c -o simsui_image.o simsui_image.S
$(SIMSUI_NATIVE_DIR)/simsui-res.o: $(SIMSUI_NATIVE_DIR)/simsui.res
	i686-w64-mingw32-windres -J res -O coff $< $@
# MFC42 anchored by one ordinal (AfxGetModuleState), so it loads first
$(SIMSUI_NATIVE_DIR)/libsimsui-mfc42.a: | $(SIMSUI_NATIVE_DIR)
	printf 'LIBRARY MFC42.DLL\nEXPORTS\n    fs98_simsui_mfc42_anchor@0 @1168 NONAME\n' > $(SIMSUI_NATIVE_DIR)/mfc42-anchor.def
	i686-w64-mingw32-dlltool -d $(SIMSUI_NATIVE_DIR)/mfc42-anchor.def -l $@
$(SIMSUI_NATIVE_DIR)/SIMSUI_ours.dll: $(SIMSUI_NATIVE_OBJECTS) $(SIMSUI_NATIVE_DIR)/simsui_image.o $(SIMSUI_NATIVE_DIR)/simsui-res.o $(SIMSUI_NATIVE_DIR)/simsui.def $(SIMSUI_NATIVE_DIR)/libsimsui-mfc42.a
	$(CC) -shared -nostdlib -Wl,--entry=_fs98_simsui_entry@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x20470000 \
	    -o $@ $(SIMSUI_NATIVE_OBJECTS) $(SIMSUI_NATIVE_DIR)/simsui_image.o $(SIMSUI_NATIVE_DIR)/simsui-res.o \
	    $(SIMSUI_NATIVE_DIR)/simsui.def $(SIMSUI_NATIVE_DIR)/libsimsui-mfc42.a -lkernel32 -luser32 -lgdi32 -limm32 -lmsvcrt -lgcc
