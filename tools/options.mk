# Options.DLL, standalone like the other dialog modules; the shared Makefile installs it.
CC := i686-w64-mingw32-gcc
OPTIONS_DIR := bin/options-diff
N ?= 20000
SEED ?= 0x5f1a6c2d
MODE ?= normal
OPTIONS_SOURCES := src/options/options_events.c src/options/options_objects.c src/options/options_video.c src/options/options_videos.c src/options/options_logbook.c src/options/options_book.c
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
