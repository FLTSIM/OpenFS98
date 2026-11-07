# Host executable runtime; standalone to leave the verified service DLL intact.
FS98_DIR ?= $(CURDIR)/game
HOST_RUNTIME_DIR := bin/host-runtime
HOST_RUNTIME_CC := i686-w64-mingw32-gcc
HOST_RUNTIME_CFLAGS := -std=c99 -O1 -Wall -Wextra -fno-strict-aliasing -Iinclude -Isrc/host -masm=intel
HOST_RUNTIME_LIBS := -lmsvcrt -lgdi32 -luser32 -lwinmm -ladvapi32 -lmsacm32 -lddraw -lkernel32

$(HOST_RUNTIME_DIR)/host_runtime.c $(HOST_RUNTIME_DIR)/host_entries.S $(HOST_RUNTIME_DIR)/host.res &: tools/gen_host_runtime.py tools/gen_sim1_lift.py tools/gen_mdl_crash.py orig/FLTSIM98.EXE | $(HOST_RUNTIME_DIR)
	python3 -B tools/gen_host_runtime.py > $(HOST_RUNTIME_DIR)/generation.json

$(HOST_RUNTIME_DIR):
	mkdir -p $@

$(HOST_RUNTIME_DIR)/host_runtime.o: $(HOST_RUNTIME_DIR)/host_runtime.c src/host/host_runtime.h include/fs98/host_runtime.h src/mdl/mdl_crash_lift.h
	$(HOST_RUNTIME_CC) $(HOST_RUNTIME_CFLAGS) -Wno-unused-function -Wno-unused-label -c $< -o $@

$(HOST_RUNTIME_DIR)/host_resources.o: $(HOST_RUNTIME_DIR)/host.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(HOST_RUNTIME_DIR)/host_services.o: src/host/host.c src/host/host_data.inc include/fs98/host.h
	$(HOST_RUNTIME_CC) $(HOST_RUNTIME_CFLAGS) -ffreestanding -fno-builtin -c $< -o $@

bin/OPENFS98.EXE: $(HOST_RUNTIME_DIR)/host_runtime.o $(HOST_RUNTIME_DIR)/host_entries.S $(HOST_RUNTIME_DIR)/host_resources.o $(HOST_RUNTIME_DIR)/host_services.o src/host/host_main.c src/host/host_runtime_native.S src/host/host_runtime.h tools/host_runtime.mk
	$(HOST_RUNTIME_CC) $(HOST_RUNTIME_CFLAGS) -mwindows -Wno-unused-function -Wl,--disable-dynamicbase -Wl,--image-base,0x43000000 -o $@ src/host/host_main.c src/host/host_runtime_native.S $(HOST_RUNTIME_DIR)/host_runtime.o $(HOST_RUNTIME_DIR)/host_entries.S $(HOST_RUNTIME_DIR)/host_services.o $(HOST_RUNTIME_DIR)/host_resources.o $(HOST_RUNTIME_LIBS)

.PHONY: host-exe
host-exe: bin/OPENFS98.EXE

$(HOST_RUNTIME_DIR)/HOST_ours.dll: $(HOST_RUNTIME_DIR)/host_runtime.o $(HOST_RUNTIME_DIR)/host_entries.S $(HOST_RUNTIME_DIR)/host_services.o src/host/host_main.c src/host/host_runtime_native.S tools/host_runtime_test.def
	$(HOST_RUNTIME_CC) $(HOST_RUNTIME_CFLAGS) -Wno-unused-function -shared -Wl,--disable-dynamicbase -Wl,--image-base,0x43000000 -o $@ src/host/host_main.c src/host/host_runtime_native.S $(HOST_RUNTIME_DIR)/host_runtime.o $(HOST_RUNTIME_DIR)/host_entries.S $(HOST_RUNTIME_DIR)/host_services.o tools/host_runtime_test.def $(HOST_RUNTIME_LIBS)

$(HOST_RUNTIME_DIR)/hostruntime.exe: tools/difftest/hostruntime.c tools/difftest/hostdiff.c include/fs98/host_runtime.h | $(HOST_RUNTIME_DIR)
	$(HOST_RUNTIME_CC) -std=c99 -O1 -Wall -Wextra -Wno-unused-function -Iinclude -Wl,--image-base,0x13000000 -o $@ tools/difftest/hostruntime.c

.PHONY: host-runtime-diff
host-runtime-diff: $(HOST_RUNTIME_DIR)/HOST_ours.dll $(HOST_RUNTIME_DIR)/hostruntime.exe
	cp orig/FLTSIM98.EXE $(HOST_RUNTIME_DIR)/FLTSIM98.EXE
	cp "$(FS98_DIR)/mfc42.dll" $(HOST_RUNTIME_DIR)/mfc42.dll
	@test -d $(HOST_RUNTIME_DIR)/test-prefix || cp -a --reflink=auto "$${WINEPREFIX:-$$HOME/.wine}" $(HOST_RUNTIME_DIR)/test-prefix
	cd $(HOST_RUNTIME_DIR) && WINEPREFIX="$(CURDIR)/$(HOST_RUNTIME_DIR)/test-prefix" WINEDEBUG=-all timeout 180 wine hostruntime.exe $(if $(N),$(N),20000)

.PHONY: host-runtime-breaks host-runtime-ui host-runtime-boot
host-runtime-breaks: host-runtime-diff
	python3 -B tools/host_runtime_breaks.py

host-runtime-ui: host-exe
	$(MAKE) bin/fsdialogs.exe sehfix
	FS98_DIR="$(FS98_DIR)" tools/host_runtime_ui.sh

host-runtime-boot: host-exe
	FS98_DIR="$(FS98_DIR)" tools/host_runtime_boot.sh

# Keep the retail executable available; install ours under its own name.
.PHONY: host-install host-run
host-install: host-exe
	cp bin/OPENFS98.EXE "$(FS98_DIR)/OPENFS98.EXE"

host-run: host-install
	cd "$(FS98_DIR)" && WINEDEBUG=-all wine OPENFS98.EXE

all: host-exe

.PHONY: host-runtime-frame-driver host-runtime-flights host-runtime-d3d
host-runtime-frame-driver:
	$(MAKE) -f tools/hg2d.mk bin/hg2d-diff/FRAMELOCK.DLL
	$(MAKE) sehfix

host-runtime-flights: host-exe host-runtime-frame-driver
	python3 -B tools/fly_scripts.py $(HOST_RUNTIME_DIR)/scripts
	FS98_DIR="$(FS98_DIR)" HG2D_FRAMES_HOST="$(CURDIR)/bin/OPENFS98.EXE" \
	 HG2D_FRAMES_ONLY=HOST HG2D_FRAMES_SIM1="$(CURDIR)/orig/Sim/SIM1.sim" \
	 HG2D_FRAMES_NOSOUND=1 FRAMES_MANY_OUT=$(HOST_RUNTIME_DIR)/flights \
	 FRAMES_MANY_PASSES=900 FRAMES_MANY_JOBS=$${HOST_RUNTIME_JOBS:-4} \
	 tools/frames_many.sh $(HOST_RUNTIME_DIR)/scripts/controls.txt
	! grep -E ': (DIFFERS|NONDETERMINISTIC|NO-RUN)|FEW FRAMES' $(HOST_RUNTIME_DIR)/flights/summary.txt

host-runtime-d3d: host-exe host-runtime-frame-driver
	python3 -B tools/fly_scripts.py $(HOST_RUNTIME_DIR)/scripts
	python3 -c 'from pathlib import Path; p=Path("$(HOST_RUNTIME_DIR)/scripts"); (p/"direct3d.txt").write_text("20 111 1501 0\n"+(p/"controls.txt").read_text())'
	FS98_DIR="$(FS98_DIR)" HG2D_FRAMES_HOST="$(CURDIR)/bin/OPENFS98.EXE" \
	 HG2D_FRAMES_ONLY=HOST HG2D_FRAMES_SIM1="$(CURDIR)/orig/Sim/SIM1.sim" \
	 HG2D_FRAMES_OURS=orig/Modules/HG2D.DLL HG2D_FRAMES_NOSOUND=1 \
	 HG2D_FRAMES_OUT=$(HOST_RUNTIME_DIR)/direct3d \
	 HG2D_FRAMES_DISPLAY=:70 HG2D_FRAMES_TIMEOUT=300 \
	 HG2D_FRAMES_DEVICE='DirectDraw HAL (Direct3D)' HG2D_FRAMES_MODE=640x480x16 \
	 HG2D_FRAMES_SCREEN=640x480 HG2D_FRAMES_HOLD=150 \
	 tools/hg2d_frames.sh 'Situatio\PARIS.STN' 900 $(HOST_RUNTIME_DIR)/scripts/direct3d.txt
