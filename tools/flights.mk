# Flights.DLL, standalone like the other dialog modules; the shared Makefile installs it.
CC := i686-w64-mingw32-gcc
FLIGHTS_DIR := bin/flights-diff
N ?= 20000
SEED ?= 0x5f1a6c2d
MODE ?= normal
FLIGHTS_SOURCES := src/flights/flights_events.c src/flights/flights_objects.c
FLIGHTS_HEADERS := src/flights/flights_internal.h src/flights/flights_objects.h src/flights/flights_unwind.h \
                   src/flights/flights_tables.inc src/flights/flights_objects_tables.inc
.PHONY: flightsevents flightseventsdiff-run flights-tables flights-breaks
flightsevents: $(FLIGHTS_DIR)/flightseventsdiff.exe
$(FLIGHTS_DIR):
	mkdir -p $@
flights-tables: src/flights/flights_tables.inc
src/flights/flights_tables.inc src/flights/flights_objects_tables.inc &: tools/gen_flights_tables.py tools/dialogmap.py orig/English/Flights.DLL
	python3 -B tools/gen_flights_tables.py
$(FLIGHTS_DIR)/flightseventsdiff.exe: tools/difftest/flightseventsdiff.c $(FLIGHTS_SOURCES) $(FLIGHTS_HEADERS) | $(FLIGHTS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/flightseventsdiff.c $(FLIGHTS_SOURCES) -lntdll
flightseventsdiff-run: flightsevents
	bash tools/run_flightsdiff.sh $(N) $(SEED) $(MODE)
flights-breaks: flightsevents
	bash tools/run_flightsdiff.sh --breaks $(N)
