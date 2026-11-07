# Options.DLL, standalone like the other dialog modules; the shared Makefile installs it.
CC := i686-w64-mingw32-gcc
OPTIONS_DIR := bin/options-diff
N ?= 20000
SEED ?= 0x5f1a6c2d
MODE ?= normal
OPTIONS_SOURCES := src/options/options_events.c src/options/options_objects.c src/options/options_video.c src/options/options_videos.c src/options/options_logbook.c src/options/options_book.c src/options/options_graphs.c src/options/options_analysis.c src/options/options_replay.c src/options/options_demo.c src/options/options_display.c src/options/options_prefs.c src/options/options_controls.c src/options/options_keys.c
OPTIONS_HEADERS := src/options/options_internal.h src/options/options_objects.h src/options/options_unwind.h \
                   src/options/options_tables.inc src/options/options_objects_tables.inc
.PHONY: optionsevents optionseventsdiff-run options-tables options-breaks
optionsevents: $(OPTIONS_DIR)/optionseventsdiff.exe
$(OPTIONS_DIR):
	mkdir -p $@
options-tables: src/options/options_tables.inc
src/options/options_tables.inc src/options/options_objects_tables.inc &: tools/gen_options_tables.py tools/dialogmap.py orig/English/Options.DLL
	python3 -B tools/gen_options_tables.py
$(OPTIONS_DIR)/optionseventsdiff.exe: tools/difftest/optionseventsdiff.c $(OPTIONS_SOURCES) $(OPTIONS_HEADERS) | $(OPTIONS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/optionseventsdiff.c $(OPTIONS_SOURCES) -lntdll
optionseventsdiff-run: optionsevents
	bash tools/run_optionsdiff.sh $(N) $(SEED) $(MODE)
options-breaks: optionsevents
	bash tools/run_optionsdiff.sh --breaks $(N)

# The native DLL: our dialogs over the CD's SIMSUI and MFC42, at retail's base
.PHONY: options options-runtime-tables
options: $(OPTIONS_DIR)/OPTIONS_ours.dll
options-runtime-tables: src/options/options_runtime_tables.inc $(OPTIONS_DIR)/options.res $(OPTIONS_DIR)/simsui-anchor.def
src/options/options_runtime_tables.inc $(OPTIONS_DIR)/options.res $(OPTIONS_DIR)/simsui-anchor.def &: tools/gen_options_runtime.py tools/gen_options_tables.py tools/difftest/optionseventsdiff.c tools/dialogmap.py orig/English/Options.DLL orig/SIMSUI.DLL | $(OPTIONS_DIR)
	python3 -B tools/gen_options_runtime.py
$(OPTIONS_DIR)/liboptions-simsui.a: $(OPTIONS_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@
$(OPTIONS_DIR)/options-res.o: $(OPTIONS_DIR)/options.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(OPTIONS_DIR)/OPTIONS_ours.dll: $(OPTIONS_SOURCES) src/options/options_runtime.c src/options/options.def $(OPTIONS_HEADERS) src/options/options_runtime_tables.inc $(OPTIONS_DIR)/options-res.o $(OPTIONS_DIR)/liboptions-simsui.a
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22390000 -o $@ $(OPTIONS_SOURCES) src/options/options_runtime.c src/options/options.def $(OPTIONS_DIR)/options-res.o $(OPTIONS_DIR)/liboptions-simsui.a -lkernel32 -luser32 -lmsvcrt
