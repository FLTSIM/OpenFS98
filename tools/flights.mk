# Flights.DLL, standalone like the other dialog modules; the shared Makefile installs it.
CC := i686-w64-mingw32-gcc
FLIGHTS_DIR := bin/flights-diff
N ?= 20000
SEED ?= 0x5f1a6c2d
MODE ?= normal
FLIGHTS_SOURCES := src/flights/flights_events.c src/flights/flights_objects.c src/flights/flights_save.c src/flights/flights_comms.c src/flights/flights_create.c src/flights/flights_aircraft.c src/flights/flights_opening.c src/flights/flights_select.c src/flights/flights_autopilot.c src/flights/flights_navigation.c
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

# The native DLL: our dialogs over the CD's SIMSUI and MFC42, at retail's base
.PHONY: flights flights-runtime-tables
flights: $(FLIGHTS_DIR)/FLIGHTS_ours.dll
flights-runtime-tables: src/flights/flights_runtime_tables.inc $(FLIGHTS_DIR)/flights.res $(FLIGHTS_DIR)/simsui-anchor.def
src/flights/flights_runtime_tables.inc $(FLIGHTS_DIR)/flights.res $(FLIGHTS_DIR)/simsui-anchor.def &: tools/gen_flights_runtime.py tools/dialogmap.py orig/English/Flights.DLL orig/SIMSUI.DLL | $(FLIGHTS_DIR)
	python3 -B tools/gen_flights_runtime.py
$(FLIGHTS_DIR)/libflights-simsui.a: $(FLIGHTS_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@
$(FLIGHTS_DIR)/flights-res.o: $(FLIGHTS_DIR)/flights.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(FLIGHTS_DIR)/FLIGHTS_ours.dll: $(FLIGHTS_SOURCES) src/flights/flights_runtime.c src/flights/flights.def $(FLIGHTS_HEADERS) src/flights/flights_runtime_tables.inc $(FLIGHTS_DIR)/flights-res.o $(FLIGHTS_DIR)/libflights-simsui.a
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22310000 -o $@ $(FLIGHTS_SOURCES) src/flights/flights_runtime.c src/flights/flights.def $(FLIGHTS_DIR)/flights-res.o $(FLIGHTS_DIR)/libflights-simsui.a -lkernel32 -luser32 -lmsvcrt
