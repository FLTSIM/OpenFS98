# The decompiled host runtime (src/host/runtime/) and its differential harness.
HOST_RT_DIR := bin/host-rt
HOST_RT_CC := i686-w64-mingw32-gcc
HOST_RT_CFLAGS := -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -Isrc/host/runtime
HOST_RT_SOURCES := $(wildcard src/host/runtime/*.c)
HOST_RT_HEADERS := src/host/runtime/host_rt.h src/host/runtime/host_rt_module.h src/host/runtime/host_rt_tables.inc

src/host/runtime/host_rt_tables.inc: tools/gen_host_rt_tables.py tools/host_rt_survey.py tools/gen_simsui_tables.py orig/FLTSIM98.EXE
	python3 -B tools/gen_host_rt_tables.py

$(HOST_RT_DIR):
	mkdir -p $@

# linked away from 0x400000, where it maps the retail EXE
$(HOST_RT_DIR)/hostrtdiff.exe: tools/difftest/hostrtdiff.c tools/difftest/hostrt_semantics.inc tools/difftest/hostrt_windows.inc tools/difftest/hostrt_operations.inc $(HOST_RT_SOURCES) $(HOST_RT_HEADERS) | $(HOST_RT_DIR)
	$(HOST_RT_CC) $(HOST_RT_CFLAGS) -Wl,--image-base=0x13000000 -Wl,--disable-dynamicbase -o $@ tools/difftest/hostrtdiff.c $(HOST_RT_SOURCES)

.PHONY: hostrtdiff hostrtdiff-run
hostrtdiff: $(HOST_RT_DIR)/hostrtdiff.exe
hostrtdiff-run: hostrtdiff
	WINEDEBUG=-all timeout 1800 wine $(HOST_RT_DIR)/hostrtdiff.exe $(or $(N),2000)

# The executable from the decompiled runtime and the services (src/host/host_rt_exe.c): no lifted
# code. Its resources are retail's (tools/gen_host_resources.py reads only the .rsrc tree).
HOST_RT_EXE_LIBS := -lmsvcrt -lgdi32 -luser32 -lwinmm -ladvapi32 -lmsacm32 -lddraw -lkernel32
$(HOST_RT_DIR)/host_services.o: src/host/host.c src/host/host_data.inc include/fs98/host.h | $(HOST_RT_DIR)
	$(HOST_RT_CC) -std=c99 -O1 -Wall -Wextra -fno-strict-aliasing -Iinclude -Isrc/host -masm=intel -ffreestanding -fno-builtin -DHOST_IMAGE_SHARED -c $< -o $@

$(HOST_RT_DIR)/host.res: tools/gen_host_resources.py tools/dialogmap.py orig/FLTSIM98.EXE | $(HOST_RT_DIR)
	python3 -B tools/gen_host_resources.py $@

$(HOST_RT_DIR)/host_resources.o: $(HOST_RT_DIR)/host.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(HOST_RT_DIR)/OPENFS98.EXE: src/host/host_rt_exe.c $(HOST_RT_SOURCES) $(HOST_RT_HEADERS) src/host/runtime/host_rt_rdata.inc tools/difftest/hostrt_operations.inc $(HOST_RT_DIR)/host_services.o $(HOST_RT_DIR)/host_resources.o
	$(HOST_RT_CC) $(HOST_RT_CFLAGS) -Iinclude -mwindows -Wl,--disable-dynamicbase -Wl,--image-base,0x43000000 -o $@ src/host/host_rt_exe.c $(HOST_RT_SOURCES) $(HOST_RT_DIR)/host_services.o $(HOST_RT_DIR)/host_resources.o $(HOST_RT_EXE_LIBS)

.PHONY: host-rt-exe
host-rt-exe: $(HOST_RT_DIR)/OPENFS98.EXE
