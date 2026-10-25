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
