# Partial World native DLL and tests; isolated from the shared build/game.
CC := i686-w64-mingw32-gcc
WORLD_DIR := bin/world-diff
N ?= 2000
SEED ?= 0x4a971b63
MODE ?= normal
.PHONY: world-complexity-core world-complexity-breaks
$(WORLD_DIR)/worldcomplexitydiff.exe: tools/difftest/worldcomplexitydiff.c src/world/world_complexity.c src/world/world_complexity.h src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldcomplexitydiff.c src/world/world_complexity.c
world-complexity-core: $(WORLD_DIR)/worldcomplexitydiff.exe
	bash tools/run_worlddiff.sh --complexity-core $(N) $(SEED) $(MODE)
world-complexity-breaks: $(WORLD_DIR)/worldcomplexitydiff.exe
	bash tools/run_worlddiff.sh --complexity-breaks
.PHONY: world-complexity-metadata
src/world/world_complexity_tables.inc src/world/world_complexity_hooks.inc &: tools/gen_world_complexity.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_complexity.py
world-complexity-metadata: src/world/world_complexity_tables.inc src/world/world_complexity_hooks.inc
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

PRIORITY_OBJECTS := src/world/world_priority_objects.c src/world/world_priority.h src/world/world_priority_tables.inc src/world/world_priority_hooks.inc src/world/world_objects.h src/world/world_init.h src/world/world_unwind.h src/world/world_exceptions.h src/world/world_internal.h
WORLD_SOURCES := src/world/world_dynamic.c src/world/world_dynamic_objects.c src/world/world_dynamic_init.c src/world/world_complexity.c src/world/world_complexity_objects.c src/world/world_complexity_init.c src/world/world_airport.c src/world/world_airport_objects.c src/world/world_facilities.c src/world/world_facilities_objects.c src/world/world_position.c src/world/world_position_objects.c src/world/world_files.c src/world/world_files_objects.c src/world/world_cache.c src/world/world_cache_objects.c src/world/world_list.c src/world/world_list_objects.c src/world/world_search.c src/world/world_search_objects.c src/world/world_search_init.c src/world/world_booster.c src/world/world_booster_objects.c src/world/world_booster_init.c src/world/world_priority.c src/world/world_priority_objects.c src/world/world_area.c src/world/world_objects.c src/world/world_init.c src/world/world_runtime.c
.PHONY: world worlduidiff-run world-ui-breaks world-smoke
src/world/world_runtime_tables.inc $(WORLD_DIR)/world.res $(WORLD_DIR)/simsui-anchor.def &: tools/gen_world_runtime.py tools/gen_views_runtime.py tools/dialogmap.py orig/English/World.DLL orig/SIMSUI.DLL | $(WORLD_DIR)
	python3 -B tools/gen_world_runtime.py
$(WORLD_DIR)/libworld-simsui.a: $(WORLD_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@
$(WORLD_DIR)/world-res.o: $(WORLD_DIR)/world.res
	i686-w64-mingw32-windres -J res -O coff $< $@
world: $(WORLD_DIR)/WORLD_area.dll
$(WORLD_DIR)/WORLD_area.dll: src/world/world_dynamic.h src/world/world_dynamic_tables.inc src/world/world_dynamic_hooks.inc $(PRIORITY_OBJECTS) $(WORLD_SOURCES) src/world/world.def src/world/world_internal.h src/world/world_objects.h src/world/world_init.h src/world/world_exceptions.h src/world/world_unwind.h src/world/world_tables.inc src/world/world_runtime_tables.inc $(WORLD_DIR)/world-res.o $(WORLD_DIR)/libworld-simsui.a
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

src/world/world_priority_tables.inc src/world/world_priority_hooks.inc &: tools/gen_world_priority.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_priority.py
$(WORLD_DIR)/worldprioritylifediff.exe: tools/difftest/worldprioritylifediff.c $(PRIORITY_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldprioritylifediff.c src/world/world_priority_objects.c -lntdll
.PHONY: world-priority-life
world-priority-life: $(WORLD_DIR)/worldprioritylifediff.exe
	bash tools/run_worlddiff.sh --priority-life $(N) $(SEED) $(MODE)
$(WORLD_DIR)/worldprioritydiff.exe: tools/difftest/worldprioritydiff.c src/world/world_priority.c $(PRIORITY_OBJECTS) src/world/world_init.c src/world/world_area.c | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldprioritydiff.c src/world/world_priority.c src/world/world_priority_objects.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: world-priority-core
world-priority-core: $(WORLD_DIR)/worldprioritydiff.exe
	bash tools/run_worlddiff.sh --priority-core $(N) $(MODE)
$(WORLD_DIR)/worldpriorityuidiff.exe: tools/difftest/worldpriorityuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-priority-ui
world-priority-ui: world $(WORLD_DIR)/worldpriorityuidiff.exe
	bash tools/run_worlddiff.sh --priority-ui
.PHONY: world-priority-breaks
world-priority-breaks: world $(WORLD_DIR)/worldprioritydiff.exe $(WORLD_DIR)/worldprioritylifediff.exe $(WORLD_DIR)/worldpriorityuidiff.exe
	bash tools/run_worlddiff.sh --priority-breaks

$(WORLD_DIR)/worldboosterdiff.exe: tools/difftest/worldboosterdiff.c src/world/world_booster.c src/world/world_booster.h src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldboosterdiff.c src/world/world_booster.c
.PHONY: world-booster-core world-booster-breaks
world-booster-core: $(WORLD_DIR)/worldboosterdiff.exe
	bash tools/run_worlddiff.sh --booster-core $(N) $(SEED) $(MODE)
world-booster-breaks: $(WORLD_DIR)/worldboosterdiff.exe
	bash tools/run_worlddiff.sh --booster-breaks

src/world/world_booster_tables.inc src/world/world_booster_hooks.inc &: tools/gen_world_booster.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_booster.py
BOOSTER_OBJECTS := src/world/world_booster_objects.c src/world/world_booster.h src/world/world_booster_tables.inc src/world/world_booster_hooks.inc src/world/world_objects.h src/world/world_init.h src/world/world_unwind.h src/world/world_exceptions.h src/world/world_internal.h
$(WORLD_DIR)/worldboosterlifediff.exe: tools/difftest/worldboosterlifediff.c $(BOOSTER_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldboosterlifediff.c src/world/world_booster_objects.c -lntdll
.PHONY: world-booster-life
world-booster-life: $(WORLD_DIR)/worldboosterlifediff.exe
	bash tools/run_worlddiff.sh --booster-life $(N) $(SEED) $(MODE)
.PHONY: world-booster-life-breaks
world-booster-life-breaks: $(WORLD_DIR)/worldboosterlifediff.exe
	bash tools/run_worlddiff.sh --booster-life-breaks

BOOSTER_INIT := src/world/world_booster_init.c src/world/world_booster.h src/world/world_init.h src/world/world_unwind.h
$(WORLD_DIR)/worldboosterinitdiff.exe: tools/difftest/worldboosterinitdiff.c tools/difftest/worldboosterdiff.c $(BOOSTER_INIT) src/world/world_booster.c src/world/world_init.c src/world/world_area.c | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldboosterinitdiff.c src/world/world_booster_init.c src/world/world_booster.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: world-booster-init
world-booster-init: $(WORLD_DIR)/worldboosterinitdiff.exe
	bash tools/run_worlddiff.sh --booster-init $(N) $(MODE)
$(WORLD_DIR)/WORLD_area.dll: $(BOOSTER_OBJECTS) $(BOOSTER_INIT)
$(WORLD_DIR)/worldboosteruidiff.exe: tools/difftest/worldboosteruidiff.c tools/difftest/worldpriorityuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-booster-ui
world-booster-ui: world $(WORLD_DIR)/worldboosteruidiff.exe
	bash tools/run_worlddiff.sh --booster-ui
.PHONY: world-booster-native-breaks
world-booster-native-breaks: world $(WORLD_DIR)/worldboosterinitdiff.exe $(WORLD_DIR)/worldboosteruidiff.exe
	bash tools/run_worlddiff.sh --booster-native-breaks

$(WORLD_DIR)/worldsearchdiff.exe: tools/difftest/worldsearchdiff.c src/world/world_search.c src/world/world_search.h src/world/world_init.h src/world/world_objects.h src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldsearchdiff.c src/world/world_search.c
.PHONY: world-search-core world-search-breaks
world-search-core: $(WORLD_DIR)/worldsearchdiff.exe
	bash tools/run_worlddiff.sh --search-core $(N) $(SEED) $(MODE)
world-search-breaks: $(WORLD_DIR)/worldsearchdiff.exe
	bash tools/run_worlddiff.sh --search-breaks
src/world/world_search_tables.inc src/world/world_search_hooks.inc &: tools/gen_world_search.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_search.py
.PHONY: world-search-metadata
world-search-metadata: src/world/world_search_tables.inc src/world/world_search_hooks.inc

SEARCH_OBJECTS := src/world/world_search_objects.c src/world/world_search.h src/world/world_init.h src/world/world_search_tables.inc src/world/world_search_hooks.inc src/world/world_objects.h src/world/world_unwind.h src/world/world_exceptions.h src/world/world_internal.h
$(WORLD_DIR)/worldsearchlifediff.exe: tools/difftest/worldsearchlifediff.c $(SEARCH_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldsearchlifediff.c src/world/world_search_objects.c -lntdll
.PHONY: world-search-life world-search-life-breaks
world-search-life: $(WORLD_DIR)/worldsearchlifediff.exe
	bash tools/run_worlddiff.sh --search-life $(N) $(SEED) $(MODE)
world-search-life-breaks: $(WORLD_DIR)/worldsearchlifediff.exe
	bash tools/run_worlddiff.sh --search-life-breaks

SEARCH_INIT := src/world/world_search_init.c src/world/world_search.h src/world/world_init.h src/world/world_internal.h src/world/world_objects.h
$(WORLD_DIR)/worldsearchinitdiff.exe: tools/difftest/worldsearchinitdiff.c tools/difftest/worldsearchdiff.c $(SEARCH_INIT) src/world/world_search.c src/world/world_init.c src/world/world_area.c src/world/world_unwind.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldsearchinitdiff.c src/world/world_search_init.c src/world/world_search.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: world-search-init world-search-init-breaks
world-search-init: $(WORLD_DIR)/worldsearchinitdiff.exe
	bash tools/run_worlddiff.sh --search-init $(N) $(MODE)
world-search-init-breaks: $(WORLD_DIR)/worldsearchinitdiff.exe
	bash tools/run_worlddiff.sh --search-init-breaks

$(WORLD_DIR)/WORLD_area.dll: $(SEARCH_OBJECTS) $(SEARCH_INIT)
$(WORLD_DIR)/worldsearchuidiff.exe: tools/difftest/worldsearchuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-search-ui world-search-native-breaks
world-search-ui: world $(WORLD_DIR)/worldsearchuidiff.exe
	bash tools/run_worlddiff.sh --search-ui
world-search-native-breaks: world $(WORLD_DIR)/worldsearchuidiff.exe
	bash tools/run_worlddiff.sh --search-native-breaks

src/world/world_list_tables.inc src/world/world_list_hooks.inc &: tools/gen_world_list.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_list.py
LIST_CORE := src/world/world_list.c src/world/world_list.h src/world/world_list_tables.inc src/world/world_objects.h src/world/world_internal.h src/world/world_init.h src/world/world_unwind.h
$(WORLD_DIR)/worldlistdiff.exe: tools/difftest/worldlistdiff.c $(LIST_CORE) src/world/world_init.c src/world/world_area.c | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldlistdiff.c src/world/world_list.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: world-list-metadata world-list-core world-list-breaks
world-list-metadata: src/world/world_list_tables.inc src/world/world_list_hooks.inc
world-list-core: $(WORLD_DIR)/worldlistdiff.exe
	bash tools/run_worlddiff.sh --list-core $(N) $(MODE)
world-list-breaks: $(WORLD_DIR)/worldlistdiff.exe
	bash tools/run_worlddiff.sh --list-breaks

LIST_OBJECTS := src/world/world_list_objects.c src/world/world_list.h src/world/world_list_tables.inc src/world/world_list_hooks.inc src/world/world_objects.h src/world/world_init.h src/world/world_unwind.h src/world/world_exceptions.h src/world/world_internal.h
$(WORLD_DIR)/worldlistlifediff.exe: tools/difftest/worldlistlifediff.c $(LIST_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldlistlifediff.c src/world/world_list_objects.c -lntdll
.PHONY: world-list-life world-list-life-breaks
world-list-life: $(WORLD_DIR)/worldlistlifediff.exe
	bash tools/run_worlddiff.sh --list-life $(N) $(SEED) $(MODE)
world-list-life-breaks: $(WORLD_DIR)/worldlistlifediff.exe
	bash tools/run_worlddiff.sh --list-life-breaks

$(WORLD_DIR)/WORLD_area.dll: $(LIST_CORE) $(LIST_OBJECTS)
$(WORLD_DIR)/worldlistuidiff.exe: tools/difftest/worldlistuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-list-ui world-list-native-breaks
world-list-ui: world $(WORLD_DIR)/worldlistuidiff.exe
	bash tools/run_worlddiff.sh --list-ui
world-list-native-breaks: world $(WORLD_DIR)/worldlistuidiff.exe
	bash tools/run_worlddiff.sh --list-native-breaks

# Booster/cache list 0x13080006, isolated from the shared Makefile.
src/world/world_cache_tables.inc src/world/world_cache_hooks.inc &: tools/gen_world_cache.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_cache.py
CACHE_OBJECTS := src/world/world_cache_objects.c src/world/world_cache.h src/world/world_cache_tables.inc src/world/world_cache_hooks.inc src/world/world_objects.h src/world/world_unwind.h src/world/world_exceptions.h
$(WORLD_DIR)/worldcachelifediff.exe: tools/difftest/worldcachelifediff.c $(CACHE_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldcachelifediff.c src/world/world_cache_objects.c -lntdll
.PHONY: world-cache-metadata world-cache-life
world-cache-metadata: src/world/world_cache_tables.inc src/world/world_cache_hooks.inc
world-cache-life: $(WORLD_DIR)/worldcachelifediff.exe
	bash tools/run_worlddiff.sh --cache-life $(N) $(SEED) $(MODE)

CACHE_CORE := src/world/world_cache.c src/world/world_cache.h src/world/world_init.h src/world/world_internal.h src/world/world_unwind.h
$(WORLD_DIR)/worldcachediff.exe: tools/difftest/worldcachediff.c $(CACHE_CORE) src/world/world_init.c src/world/world_area.c | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldcachediff.c src/world/world_cache.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: world-cache-core
world-cache-core: $(WORLD_DIR)/worldcachediff.exe
	bash tools/run_worlddiff.sh --cache-core $(N) $(MODE)

$(WORLD_DIR)/WORLD_area.dll: $(CACHE_CORE) $(CACHE_OBJECTS)

$(WORLD_DIR)/worldcacheuidiff.exe: tools/difftest/worldcacheuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-cache-ui
world-cache-ui: world $(WORLD_DIR)/worldcacheuidiff.exe
	bash tools/run_worlddiff.sh --cache-ui

.PHONY: world-cache-breaks world-cache-life-breaks
world-cache-breaks: $(WORLD_DIR)/worldcachediff.exe
	bash tools/run_worlddiff.sh --cache-breaks
world-cache-life-breaks: $(WORLD_DIR)/worldcachelifediff.exe
	bash tools/run_worlddiff.sh --cache-life-breaks

.PHONY: world-cache-native-breaks
world-cache-native-breaks: world $(WORLD_DIR)/worldcacheuidiff.exe
	bash tools/run_worlddiff.sh --cache-native-breaks

# World-files container 0x1306.
src/world/world_files_tables.inc src/world/world_files_hooks.inc &: tools/gen_world_files.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_files.py
FILES_OBJECTS := src/world/world_files_objects.c src/world/world_files.h src/world/world_files_tables.inc src/world/world_files_hooks.inc src/world/world_objects.h src/world/world_init.h src/world/world_internal.h src/world/world_unwind.h src/world/world_exceptions.h
$(WORLD_DIR)/worldfileslifediff.exe: tools/difftest/worldfileslifediff.c $(FILES_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldfileslifediff.c src/world/world_files_objects.c -lntdll
.PHONY: world-files-metadata world-files-life
world-files-metadata: src/world/world_files_tables.inc src/world/world_files_hooks.inc
world-files-life: $(WORLD_DIR)/worldfileslifediff.exe
	bash tools/run_worlddiff.sh --files-life $(N) $(SEED) $(MODE)

FILES_CORE := src/world/world_files.c src/world/world_files.h src/world/world_internal.h src/world/world_init.h
$(WORLD_DIR)/worldfilesdiff.exe: tools/difftest/worldfilesdiff.c $(FILES_CORE) src/world/world_init.c src/world/world_area.c | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldfilesdiff.c src/world/world_files.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: world-files-core
world-files-core: $(WORLD_DIR)/worldfilesdiff.exe
	bash tools/run_worlddiff.sh --files-core $(N) $(MODE)

$(WORLD_DIR)/WORLD_area.dll: $(FILES_CORE) $(FILES_OBJECTS)

$(WORLD_DIR)/worldfilesuidiff.exe: tools/difftest/worldfilesuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-files-ui
world-files-ui: world $(WORLD_DIR)/worldfilesuidiff.exe
	bash tools/run_worlddiff.sh --files-ui

.PHONY: world-files-breaks world-files-life-breaks
world-files-breaks: $(WORLD_DIR)/worldfilesdiff.exe
	bash tools/run_worlddiff.sh --files-breaks
world-files-life-breaks: $(WORLD_DIR)/worldfileslifediff.exe
	bash tools/run_worlddiff.sh --files-life-breaks

.PHONY: world-files-native-breaks
world-files-native-breaks: world $(WORLD_DIR)/worldfilesuidiff.exe
	bash tools/run_worlddiff.sh --files-native-breaks

# Exact-position dialog 0x1308.
src/world/world_position_tables.inc src/world/world_position_hooks.inc &: tools/gen_world_position.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_position.py
POSITION_OBJECTS := src/world/world_position_objects.c src/world/world_position.h src/world/world_position_tables.inc src/world/world_position_hooks.inc src/world/world_objects.h src/world/world_init.h src/world/world_internal.h src/world/world_unwind.h
$(WORLD_DIR)/worldpositionlifediff.exe: tools/difftest/worldpositionlifediff.c $(POSITION_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldpositionlifediff.c src/world/world_position_objects.c -lntdll
.PHONY: world-position-metadata world-position-life
world-position-metadata: src/world/world_position_tables.inc src/world/world_position_hooks.inc
world-position-life: $(WORLD_DIR)/worldpositionlifediff.exe
	bash tools/run_worlddiff.sh --position-life $(N) $(SEED) $(MODE)

POSITION_CORE := src/world/world_position.c src/world/world_position.h src/world/world_internal.h src/world/world_init.h
$(WORLD_DIR)/worldpositiondiff.exe: tools/difftest/worldpositiondiff.c $(POSITION_CORE) src/world/world_init.c src/world/world_area.c | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldpositiondiff.c src/world/world_position.c src/world/world_init.c src/world/world_area.c -lntdll
.PHONY: world-position-core
world-position-core: $(WORLD_DIR)/worldpositiondiff.exe
	bash tools/run_worlddiff.sh --position-core $(N) $(MODE)

$(WORLD_DIR)/WORLD_area.dll: $(POSITION_CORE) $(POSITION_OBJECTS)

$(WORLD_DIR)/worldpositionuidiff.exe: tools/difftest/worldpositionuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-position-ui
world-position-ui: world $(WORLD_DIR)/worldpositionuidiff.exe
	bash tools/run_worlddiff.sh --position-ui

.PHONY: world-position-breaks world-position-life-breaks
world-position-breaks: $(WORLD_DIR)/worldpositiondiff.exe
	bash tools/run_worlddiff.sh --position-breaks
world-position-life-breaks: $(WORLD_DIR)/worldpositionlifediff.exe
	bash tools/run_worlddiff.sh --position-life-breaks

.PHONY: world-position-native-breaks
world-position-native-breaks: world $(WORLD_DIR)/worldpositionuidiff.exe
	bash tools/run_worlddiff.sh --position-native-breaks

# Facilities directory 0x1307: class/factory, browser core and native UI.
src/world/world_facilities_tables.inc src/world/world_facilities_hooks.inc &: tools/gen_world_facilities.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_facilities.py
FACILITIES_OBJECTS := src/world/world_facilities_objects.c src/world/world_facilities.h src/world/world_facilities_tables.inc src/world/world_facilities_hooks.inc src/world/world_objects.h src/world/world_internal.h src/world/world_unwind.h
$(WORLD_DIR)/worldfacilitieslifediff.exe: tools/difftest/worldfacilitieslifediff.c $(FACILITIES_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldfacilitieslifediff.c src/world/world_facilities_objects.c -lntdll
.PHONY: world-facilities-metadata world-facilities-life world-facilities-life-breaks
world-facilities-metadata: src/world/world_facilities_tables.inc src/world/world_facilities_hooks.inc
world-facilities-life: $(WORLD_DIR)/worldfacilitieslifediff.exe
	bash tools/run_worlddiff.sh --facilities-life $(N) $(SEED) $(MODE)
world-facilities-life-breaks: $(WORLD_DIR)/worldfacilitieslifediff.exe
	bash tools/run_worlddiff.sh --facilities-life-breaks

FACILITIES_CORE := src/world/world_facilities.c src/world/world_facilities.h src/world/world_facilities_tables.inc src/world/world_unwind.h
$(WORLD_DIR)/WORLD_area.dll: $(FACILITIES_CORE) $(FACILITIES_OBJECTS)

$(WORLD_DIR)/worldfacilitiesuidiff.exe: tools/difftest/worldfacilitiesuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-facilities-ui
world-facilities-ui: world $(WORLD_DIR)/worldfacilitiesuidiff.exe
	bash tools/run_worlddiff.sh --facilities-ui

$(WORLD_DIR)/worldfacilitiesdiff.exe: tools/difftest/worldfacilitiesdiff.c $(FACILITIES_CORE) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldfacilitiesdiff.c src/world/world_facilities.c -lntdll
.PHONY: world-facilities-core
world-facilities-core: $(WORLD_DIR)/worldfacilitiesdiff.exe
	bash tools/run_worlddiff.sh --facilities-core $(N) $(MODE)

.PHONY: world-facilities-breaks world-facilities-native-breaks
world-facilities-breaks: $(WORLD_DIR)/worldfacilitiesdiff.exe
	bash tools/run_worlddiff.sh --facilities-breaks
world-facilities-native-breaks: world $(WORLD_DIR)/worldfacilitiesuidiff.exe
	bash tools/run_worlddiff.sh --facilities-native-breaks

# Go-to-airport 0x1309: independent class/factory lifetime milestone.
src/world/world_airport_tables.inc src/world/world_airport_hooks.inc &: tools/gen_world_airport.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_airport.py
AIRPORT_OBJECTS := src/world/world_airport_objects.c src/world/world_airport.h src/world/world_airport_tables.inc src/world/world_airport_hooks.inc src/world/world_objects.h src/world/world_internal.h src/world/world_unwind.h
$(WORLD_DIR)/worldairportlifediff.exe: tools/difftest/worldairportlifediff.c $(AIRPORT_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldairportlifediff.c src/world/world_airport_objects.c -lntdll
.PHONY: world-airport-metadata world-airport-life world-airport-life-breaks
world-airport-metadata: src/world/world_airport_tables.inc src/world/world_airport_hooks.inc
world-airport-life: $(WORLD_DIR)/worldairportlifediff.exe
	bash tools/run_worlddiff.sh --airport-life $(N) $(SEED) $(MODE)
world-airport-life-breaks: $(WORLD_DIR)/worldairportlifediff.exe
	bash tools/run_worlddiff.sh --airport-life-breaks

AIRPORT_CORE := src/world/world_airport.c src/world/world_airport.h src/world/world_internal.h src/world/world_init.h
$(WORLD_DIR)/worldairportdiff.exe: tools/difftest/worldairportdiff.c $(AIRPORT_CORE) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldairportdiff.c src/world/world_airport.c -lntdll
.PHONY: world-airport-core
world-airport-core: $(WORLD_DIR)/worldairportdiff.exe
	bash tools/run_worlddiff.sh --airport-core $(N) $(MODE)

$(WORLD_DIR)/WORLD_area.dll: $(AIRPORT_CORE) $(AIRPORT_OBJECTS)

$(WORLD_DIR)/worldairportuidiff.exe: tools/difftest/worldairportuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-airport-ui
world-airport-ui: world $(WORLD_DIR)/worldairportuidiff.exe
	bash tools/run_worlddiff.sh --airport-ui

.PHONY: world-airport-breaks
world-airport-breaks: $(WORLD_DIR)/worldairportdiff.exe
	bash tools/run_worlddiff.sh --airport-breaks

.PHONY: world-airport-native-breaks
world-airport-native-breaks: world $(WORLD_DIR)/worldairportuidiff.exe
	bash tools/run_worlddiff.sh --airport-native-breaks

COMPLEXITY_OBJECTS := src/world/world_complexity_objects.c src/world/world_complexity.h src/world/world_complexity_tables.inc src/world/world_complexity_hooks.inc src/world/world_objects.h src/world/world_internal.h src/world/world_unwind.h
$(WORLD_DIR)/worldcomplexitylifediff.exe: tools/difftest/worldcomplexitylifediff.c $(COMPLEXITY_OBJECTS) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worldcomplexitylifediff.c src/world/world_complexity_objects.c -lntdll
.PHONY: world-complexity-life world-complexity-life-breaks
world-complexity-life: $(WORLD_DIR)/worldcomplexitylifediff.exe
	bash tools/run_worlddiff.sh --complexity-life $(N) $(SEED) $(MODE)
world-complexity-life-breaks: $(WORLD_DIR)/worldcomplexitylifediff.exe
	bash tools/run_worlddiff.sh --complexity-life-breaks
COMPLEXITY_INIT := src/world/world_complexity_init.c src/world/world_complexity.h src/world/world_unwind.h src/world/world_init.h
$(WORLD_DIR)/worldcomplexityinitdiff.exe: tools/difftest/worldcomplexityinitdiff.c tools/difftest/worldcomplexitydiff.c src/world/world_complexity.c $(COMPLEXITY_INIT) | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worldcomplexityinitdiff.c src/world/world_complexity.c src/world/world_complexity_init.c -lntdll
.PHONY: world-complexity-init
world-complexity-init: $(WORLD_DIR)/worldcomplexityinitdiff.exe
	bash tools/run_worlddiff.sh --complexity-init $(N) $(MODE)

$(WORLD_DIR)/WORLD_area.dll: $(COMPLEXITY_OBJECTS) $(COMPLEXITY_INIT)
$(WORLD_DIR)/worldcomplexityuidiff.exe: tools/difftest/worldcomplexityuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
.PHONY: world-complexity-ui
world-complexity-ui: world $(WORLD_DIR)/worldcomplexityuidiff.exe
	bash tools/run_worlddiff.sh --complexity-ui
.PHONY: world-complexity-init-breaks
world-complexity-init-breaks: $(WORLD_DIR)/worldcomplexityinitdiff.exe
	bash tools/run_worlddiff.sh --complexity-init-breaks
.PHONY: world-complexity-native-breaks
world-complexity-native-breaks: world $(WORLD_DIR)/worldcomplexityuidiff.exe
	bash tools/run_worlddiff.sh --complexity-native-breaks

DYNAMIC_METADATA := src/world/world_dynamic_tables.inc src/world/world_dynamic_hooks.inc
$(DYNAMIC_METADATA) &: tools/gen_world_dynamic.py tools/gen_world_tables.py tools/dialogmap.py orig/English/World.DLL
	python3 -B tools/gen_world_dynamic.py
.PHONY: world-dynamic-metadata world-dynamic-core world-dynamic-life world-dynamic-init world-dynamic-ui world-dynamic-breaks
world-dynamic-metadata: $(DYNAMIC_METADATA)
$(WORLD_DIR)/worlddynamicdiff.exe: tools/difftest/worlddynamicdiff.c src/world/world_dynamic.c src/world/world_dynamic.h src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worlddynamicdiff.c src/world/world_dynamic.c
$(WORLD_DIR)/worlddynamiclifediff.exe: tools/difftest/worlddynamiclifediff.c $(DYNAMIC_METADATA) src/world/world_dynamic_objects.c src/world/world_dynamic.h src/world/world_unwind.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/worlddynamiclifediff.c src/world/world_dynamic_objects.c -lntdll
$(WORLD_DIR)/worlddynamicinitdiff.exe: tools/difftest/worlddynamicinitdiff.c tools/difftest/worlddynamicdiff.c src/world/world_dynamic_init.c src/world/world_dynamic.c src/world/world_dynamic.h src/world/world_unwind.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ tools/difftest/worlddynamicinitdiff.c src/world/world_dynamic_init.c src/world/world_dynamic.c -lntdll
world-dynamic-core: $(WORLD_DIR)/worlddynamicdiff.exe
	bash tools/run_worlddiff.sh --dynamic-core $(N) $(SEED) $(MODE)
world-dynamic-life: $(WORLD_DIR)/worlddynamiclifediff.exe
	bash tools/run_worlddiff.sh --dynamic-life $(N) $(SEED) $(MODE)
world-dynamic-init: $(WORLD_DIR)/worlddynamicinitdiff.exe
	bash tools/run_worlddiff.sh --dynamic-init $(N) $(MODE)
$(WORLD_DIR)/worlddynamicuidiff.exe: tools/difftest/worlddynamicuidiff.c tools/difftest/worlduidiff.c src/world/world_internal.h | $(WORLD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
world-dynamic-ui: world $(WORLD_DIR)/worlddynamicuidiff.exe
	bash tools/run_worlddiff.sh --dynamic-ui
.PHONY: world-dynamic-life-breaks world-dynamic-init-breaks world-dynamic-native-breaks
world-dynamic-breaks: $(WORLD_DIR)/worlddynamicdiff.exe
	bash tools/run_worlddiff.sh --dynamic-breaks
world-dynamic-life-breaks: $(WORLD_DIR)/worlddynamiclifediff.exe
	bash tools/run_worlddiff.sh --dynamic-life-breaks
world-dynamic-init-breaks: $(WORLD_DIR)/worlddynamicinitdiff.exe
	bash tools/run_worlddiff.sh --dynamic-init-breaks
world-dynamic-native-breaks: world $(WORLD_DIR)/worlddynamicuidiff.exe
	bash tools/run_worlddiff.sh --dynamic-native-breaks
