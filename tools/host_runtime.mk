# The host executable: its build, install, and game-level checks.
FS98_DIR ?= $(CURDIR)/game
HOST_RUNTIME_DIR := bin/host-runtime
HOST_RUNTIME_CC := i686-w64-mingw32-gcc

# The routines the services share with the runtime (src/host/runtime/host_rt.h, HOST_RT_STANDALONE),
# for a build of the services without the runtime: HOST.DLL (hostdiff) and its breaks.
HOST_RT_SHARED := $(addprefix src/host/runtime/host_rt_,ddraw.c config.c modules.c display.c common.c)
HOST_RT_STANDALONE_DIR := bin/host-rt-standalone
HOST_RT_STANDALONE_OBJECTS := $(patsubst src/host/runtime/%.c,$(HOST_RT_STANDALONE_DIR)/%.o,$(HOST_RT_SHARED))
$(HOST_RT_STANDALONE_DIR)/%.o: src/host/runtime/%.c src/host/runtime/host_rt.h src/host/runtime/host_rt_module.h src/host/runtime/host_rt_tables.inc
	@mkdir -p $(@D)
	$(HOST_RUNTIME_CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -fno-asynchronous-unwind-tables -DHOST_RT_STANDALONE -c $< -o $@

# The executable: the decompiled runtime and the services (tools/host_rt.mk, src/host/host_rt_exe.c).
HOST_EXE_SOURCES := src/host/host_rt_exe.c $(wildcard src/host/runtime/*.c src/host/runtime/*.h src/host/runtime/*.inc) \
	src/host/host.c src/host/host_data.inc include/fs98/host.h include/fs98/host_runtime.h tools/difftest/hostrt_operations.inc tools/host_rt.mk
bin/OPENFS98.EXE: $(HOST_EXE_SOURCES) tools/gen_host_resources.py
	$(MAKE) -f tools/host_rt.mk bin/host-rt/OPENFS98.EXE
	cp bin/host-rt/OPENFS98.EXE $@

.PHONY: host-exe
host-exe: bin/OPENFS98.EXE

.PHONY: host-runtime-ui host-runtime-boot
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
