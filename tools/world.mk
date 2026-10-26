# Partial World area DLL and core tests; isolated from the shared build/game.
CC := i686-w64-mingw32-gcc
WORLD_DIR := bin/world-diff
N ?= 2000
SEED ?= 0x4a971b63
MODE ?= normal
.PHONY: worldcore worlddiff-run world-breaks
worldcore: $(WORLD_DIR)/worlddiff.exe
$(WORLD_DIR):
	mkdir -p $@
$(WORLD_DIR)/worlddiff.exe: tools/difftest/worlddiff.c src/world/world_area.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worlddiff.c src/world/world_area.c
worlddiff-run: worldcore
	bash tools/run_worlddiff.sh $(N) $(SEED) $(MODE)
world-breaks: worldcore
	bash tools/run_worlddiff.sh --breaks

src/world/world_tables.inc src/world/world_hooks.inc &: tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_tables.py
WORLD_OBJECTS := src/world/world_objects.c src/world/world_objects.h src/world/world_unwind.h src/world/world_exceptions.h src/world/world_tables.inc src/world/world_hooks.inc
$(WORLD_DIR)/worldlifediff.exe: tools/difftest/worldlifediff.c $(WORLD_OBJECTS) src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldlifediff.c src/world/world_objects.c -lntdll
.PHONY: worldlife worldlifediff-run world-life-breaks
worldlife: $(WORLD_DIR)/worldlifediff.exe
worldlifediff-run: worldlife
	bash tools/run_worlddiff.sh --life $(N) $(SEED) $(MODE)
world-life-breaks: worldlife
	bash tools/run_worlddiff.sh --life-breaks

WORLD_SOURCES := src/world/world_area.c src/world/world_objects.c src/world/world_init.c src/world/world_runtime.c
.PHONY: world worlduidiff-run world-ui-breaks world-smoke
src/world/world_runtime_tables.inc $(WORLD_DIR)/world.res $(WORLD_DIR)/simsui-anchor.def &: tools/gen_world_runtime.py tools/gen_views_runtime.py tools/dialogmap.py orig/English/World.DLL orig/SIMSUI.DLL | $(WORLD_DIR)
	python3 -B tools/gen_world_runtime.py
$(WORLD_DIR)/libworld-simsui.a: $(WORLD_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@
$(WORLD_DIR)/world-res.o: $(WORLD_DIR)/world.res
	i686-w64-mingw32-windres -J res -O coff $< $@
world: $(WORLD_DIR)/WORLD_area.dll
$(WORLD_DIR)/WORLD_area.dll: $(WORLD_SOURCES) src/world/world.def src/world/world_internal.h src/world/world_objects.h src/world/world_init.h src/world/world_exceptions.h src/world/world_unwind.h src/world/world_tables.inc src/world/world_runtime_tables.inc $(WORLD_DIR)/world-res.o $(WORLD_DIR)/libworld-simsui.a
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x223f0000 -o $@ $(WORLD_SOURCES) src/world/world.def $(WORLD_DIR)/world-res.o $(WORLD_DIR)/libworld-simsui.a -lkernel32 -luser32 -lmsvcrt -lntdll
$(WORLD_DIR)/worlduidiff.exe: tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
worlduidiff-run: world $(WORLD_DIR)/worlduidiff.exe
	bash tools/run_worlddiff.sh --ui
world-ui-breaks: world $(WORLD_DIR)/worlduidiff.exe
	bash tools/run_worlddiff.sh --ui-breaks
world-smoke: world
	bash tools/run_worlddiff.sh --smoke

$(WORLD_DIR)/worldinitdiff.exe: tools/difftest/worldinitdiff.c tools/difftest/worlddiff.c src/world/world_init.c src/world/world_init.h src/world/world_area.c src/world/world_unwind.h src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldinitdiff.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: worldinitdiff-run
worldinitdiff-run: $(WORLD_DIR)/worldinitdiff.exe
	bash tools/run_worlddiff.sh --init $(N) $(MODE)
.PHONY: world-init-breaks
world-init-breaks: $(WORLD_DIR)/worldinitdiff.exe
	bash tools/run_worlddiff.sh --init-breaks
