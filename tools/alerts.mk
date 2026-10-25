# Standalone Alerts work; MAIN owns the shared Makefile.
CC := i686-w64-mingw32-gcc
ALERTS_DIR := bin/alerts-diff
N ?= 20000
.PHONY: alertsevents alertseventsdiff-run alerts-events-breaks
alertsevents: $(ALERTS_DIR)/alertseventsdiff.exe
$(ALERTS_DIR):
	mkdir -p $@
src/alerts/alerts_tables.inc: tools/gen_alerts_tables.py tools/dialogmap.py orig/English/Alerts.DLL
	python3 -B tools/gen_alerts_tables.py
$(ALERTS_DIR)/alertseventsdiff.exe: tools/difftest/alertseventsdiff.c src/alerts/alerts_events.c src/alerts/alerts_internal.h src/alerts/types.h src/alerts/alerts_tables.inc | $(ALERTS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -o $@ tools/difftest/alertseventsdiff.c src/alerts/alerts_events.c
alertseventsdiff-run: alertsevents
	bash tools/run_alertsdiff.sh $(N)
alerts-events-breaks: alertsevents
	bash tools/run_alertsdiff.sh --breaks $(N)
