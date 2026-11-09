# The decompiled host runtime (src/host/runtime/) and its differential harness.
HOST_RT_DIR := bin/host-rt
HOST_RT_CC := i686-w64-mingw32-gcc
HOST_RT_CFLAGS := -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -Isrc/host/runtime
HOST_RT_SOURCES := $(wildcard src/host/runtime/*.c)
HOST_RT_HEADERS := src/host/runtime/host_rt.h src/host/runtime/host_rt_tables.inc

src/host/runtime/host_rt_tables.inc: tools/gen_host_rt_tables.py tools/gen_host_runtime.py tools/gen_simsui_tables.py orig/FLTSIM98.EXE
	python3 -B tools/gen_host_rt_tables.py

$(HOST_RT_DIR):
	mkdir -p $@

# linked away from 0x400000, where it maps the retail EXE
$(HOST_RT_DIR)/hostrtdiff.exe: tools/difftest/hostrtdiff.c tools/difftest/hostrt_semantics.inc tools/difftest/hostrt_operations.inc $(HOST_RT_SOURCES) $(HOST_RT_HEADERS) | $(HOST_RT_DIR)
	$(HOST_RT_CC) $(HOST_RT_CFLAGS) -Wl,--image-base=0x13000000 -Wl,--disable-dynamicbase -o $@ tools/difftest/hostrtdiff.c $(HOST_RT_SOURCES)

.PHONY: hostrtdiff hostrtdiff-run
hostrtdiff: $(HOST_RT_DIR)/hostrtdiff.exe
hostrtdiff-run: hostrtdiff
	WINEDEBUG=-all timeout 1800 wine $(HOST_RT_DIR)/hostrtdiff.exe $(or $(N),2000)
