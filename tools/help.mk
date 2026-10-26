# Help stays independent while MAIN and HOST own the shared Makefile.
CC := i686-w64-mingw32-gcc
HELP_DIR := bin/help-diff
N ?= 20000
SEED ?= 0x19a847e3
MODE ?= normal
.PHONY: helpevents helpeventsdiff-run help-breaks
helpevents: $(HELP_DIR)/helpeventsdiff.exe
$(HELP_DIR):
	mkdir -p $@
src/help/help_tables.inc src/help/help_objects_tables.inc &: tools/gen_help_tables.py tools/dialogmap.py orig/English/Help.DLL
	python3 -B tools/gen_help_tables.py
$(HELP_DIR)/helpeventsdiff.exe: tools/difftest/helpeventsdiff.c src/help/help_events.c src/help/help_internal.h src/help/help_tables.inc src/help/help_objects.c src/help/help_objects.h src/help/help_objects_tables.inc src/help/help_unwind.h src/help/help_stack.S | $(HELP_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/helpeventsdiff.c src/help/help_events.c src/help/help_objects.c src/help/help_stack.S -lntdll
helpeventsdiff-run: helpevents
	bash tools/run_helpdiff.sh $(N) $(SEED) $(MODE)
help-breaks: helpevents
	bash tools/run_helpdiff.sh --breaks $(N)

HELP_SOURCES := src/help/help_events.c src/help/help_objects.c src/help/help_stack.S
.PHONY: help help-runtime-tables helpuidiff-run help-ui-breaks help-smoke
help: $(HELP_DIR)/HELP_ours.dll
help-runtime-tables: src/help/help_runtime_tables.inc $(HELP_DIR)/help.res $(HELP_DIR)/simsui-anchor.def
src/help/help_runtime_tables.inc $(HELP_DIR)/help.res $(HELP_DIR)/simsui-anchor.def &: tools/gen_help_runtime.py tools/dialogmap.py orig/English/Help.DLL orig/SIMSUI.DLL | $(HELP_DIR)
	python3 -B tools/gen_help_runtime.py
$(HELP_DIR)/libhelp-simsui.a: $(HELP_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@
$(HELP_DIR)/help-res.o: $(HELP_DIR)/help.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(HELP_DIR)/HELP_ours.dll: $(HELP_SOURCES) src/help/help_runtime.c src/help/help.def src/help/help_internal.h src/help/help_objects.h src/help/help_unwind.h src/help/help_tables.inc src/help/help_objects_tables.inc src/help/help_runtime_tables.inc $(HELP_DIR)/help-res.o $(HELP_DIR)/libhelp-simsui.a
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22330000 -o $@ $(HELP_SOURCES) src/help/help_runtime.c src/help/help.def $(HELP_DIR)/help-res.o $(HELP_DIR)/libhelp-simsui.a -lkernel32 -luser32 -ladvapi32 -lcomctl32 -lmsvcrt
$(HELP_DIR)/helpuidiff.exe: tools/difftest/helpuidiff.c src/help/help_internal.h src/help/help_unwind.h | $(HELP_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
helpuidiff-run: help $(HELP_DIR)/helpuidiff.exe
	bash tools/run_helpdiff.sh --ui
help-ui-breaks: help $(HELP_DIR)/helpuidiff.exe
	bash tools/run_helpdiff.sh --ui-breaks
help-smoke: help
	bash tools/run_helpdiff.sh --smoke
