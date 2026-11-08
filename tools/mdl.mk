# Aircraft MDL builds, installation and verification.
MDL_DIR := bin/mdl-diff
MDL_CC := i686-w64-mingw32-gcc
MDL_ORIGINALS := $(shell find orig/Aircraft -iname '*_N.MDL' 2>/dev/null)

$(MDL_DIR)/fixtures.inc: tools/gen_mdl_fixtures.py tools/mdl_survey.py tools/impscan.py $(MDL_ORIGINALS)
	PYTHONDONTWRITEBYTECODE=1 python3 tools/gen_mdl_fixtures.py

$(MDL_DIR)/mdldiff.exe: tools/difftest/mdldiff.c src/mdl/mdl_normal.c src/mdl/mdl_normal.h $(MDL_DIR)/fixtures.inc
	$(MDL_CC) -std=c99 -O2 -Wall -Wextra -Iinclude -Isrc/mdl -I$(MDL_DIR) -o $@ tools/difftest/mdldiff.c src/mdl/mdl_normal.c

.PHONY: mdl-normal-diff mdl-normal-diff-run mdl-normal-breaks
mdl-normal-diff: $(MDL_DIR)/mdldiff.exe
mdl-normal-diff-run: mdl-normal-diff
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine $(MDL_DIR)/mdldiff.exe $(or $(N),70000)
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine $(MDL_DIR)/mdldiff.exe $(or $(N),70000) relocate

mdl-normal-breaks: mdl-normal-diff
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_breaks.py

MDL_MODULE_FILES := $(addprefix $(MDL_DIR)/models/,$(notdir $(MDL_ORIGINALS)))
$(MDL_DIR)/modules.inc $(MDL_DIR)/modules.json $(MDL_MODULE_FILES) &: tools/build_mdls.py tools/gen_gauges.py tools/dialogmap.py tools/mdl_survey.py src/mdl/mdl_module.c src/mdl/mdl_normal.c src/mdl/mdl_normal.h $(MDL_ORIGINALS)
	PYTHONDONTWRITEBYTECODE=1 python3 tools/build_mdls.py

$(MDL_DIR)/mdldlldiff.exe: tools/difftest/mdldlldiff.c $(MDL_DIR)/modules.inc
	$(MDL_CC) -std=c99 -O2 -Wall -Wextra -I$(MDL_DIR) -o $@ tools/difftest/mdldlldiff.c

.PHONY: mdl-normal-modules mdl-normal-modules-run mdl-normal-module-breaks
mdl-normal-modules: $(MDL_DIR)/modules.inc $(MDL_MODULE_FILES) $(MDL_DIR)/mdldlldiff.exe
mdl-normal-modules-run: mdl-normal-modules
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine $(MDL_DIR)/mdldlldiff.exe $(or $(N),70000) retail-first
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine $(MDL_DIR)/mdldlldiff.exe $(or $(N),70000) ours-first
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 180s wine $(MDL_DIR)/mdldlldiff.exe $(or $(N),70000) relocate

mdl-normal-module-breaks: mdl-normal-modules
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_module_breaks.py

.PHONY: mdl-normal-game mdl-normal-render-break
mdl-normal-game: mdl-normal-modules
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_frames.py --passes $(or $(N),600) --jobs $(or $(JOBS),4)

mdl-normal-render-break: mdl-normal-modules
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_render_break.py

MDL_CRASH_DIR := bin/mdl-crash
MDL_CRASH_ORIGINALS := $(shell find orig/Aircraft -iname '*_C.MDL' 2>/dev/null)
MDL_CRASH_FILES := $(addprefix $(MDL_CRASH_DIR)/models/,$(notdir $(MDL_CRASH_ORIGINALS)))
$(MDL_CRASH_DIR)/crashes.inc $(MDL_CRASH_DIR)/modules.json $(MDL_CRASH_FILES) &: tools/gen_mdl_crash.py tools/build_mdls.py tools/mdl_survey.py tools/gen_gauges.py tools/dialogmap.py src/mdl/mdl_crash_lift.h src/mdl/mdl_crash_native.S src/mdl/mdl_crash.c src/mdl/mdl_crash_entry.S include/fs98/linkage.h $(MDL_CRASH_ORIGINALS)
	PYTHONDONTWRITEBYTECODE=1 python3 tools/gen_mdl_crash.py

$(MDL_CRASH_DIR)/mdlcrashdiff.exe: tools/difftest/mdlcrashdiff.c tools/difftest/mdldlldiff.c $(MDL_CRASH_DIR)/crashes.inc $(MDL_DIR)/modules.inc
	$(MDL_CC) -std=c99 -O2 -Wall -Wextra -I$(MDL_CRASH_DIR) -I$(MDL_DIR) -o $@ tools/difftest/mdlcrashdiff.c

.PHONY: mdl-crash-modules mdl-crash-diff-run mdl-crash-lifted-run mdl-crash-breaks
mdl-crash-modules: $(MDL_CRASH_FILES) $(MDL_CRASH_DIR)/mdlcrashdiff.exe
mdl-crash-diff-run: mdl-crash-modules
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 300s wine $(MDL_CRASH_DIR)/mdlcrashdiff.exe $(or $(N),2000) retail-first
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 300s wine $(MDL_CRASH_DIR)/mdlcrashdiff.exe $(or $(N),2000) relocate

# the lifted oracle (bin/mdl-crash/lifted/) against retail, all registers, with routine coverage
mdl-crash-lifted-run: mdl-crash-modules
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 300s wine $(MDL_CRASH_DIR)/mdlcrashdiff.exe $(or $(N),2000) lifted-retail-first
	WINEPREFIX="$(CURDIR)/$(MDL_DIR)/wineprefix" WINEARCH=win32 WINEDEBUG=-all timeout 300s wine $(MDL_CRASH_DIR)/mdlcrashdiff.exe $(or $(N),2000) lifted-relocate

mdl-crash-breaks: mdl-crash-modules
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_crash_breaks.py

.PHONY: mdl-crash-lift-breaks
mdl-crash-lift-breaks: mdl-crash-modules
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_crash_lift_breaks.py

.PHONY: mdl-crash-game mdl-crash-render-break
mdl-crash-game: mdl-crash-modules
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_crash_frames.py --passes $(or $(N),1200) --jobs $(or $(JOBS),3)

mdl-crash-render-break: mdl-crash-modules
	PYTHONDONTWRITEBYTECODE=1 python3 tools/mdl_crash_render_break.py

.PHONY: mdls mdls-install mdls-revert
mdls: $(MDL_MODULE_FILES) $(MDL_CRASH_FILES) $(MDL_DIR)/modules.json $(MDL_CRASH_DIR)/modules.json

mdls-install: mdls
	python3 -B tools/stage_mdls.py "$(FS98_DIR)" --all

mdls-revert: $(MDL_DIR)/modules.json $(MDL_CRASH_DIR)/modules.json
	python3 -B tools/stage_mdls.py "$(FS98_DIR)" --all --retail
