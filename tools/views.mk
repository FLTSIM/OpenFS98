# Standalone Views work; other modules own the shared Makefile.
CC := i686-w64-mingw32-gcc
VIEWS_DIR := bin/views-diff
N ?= 20000
.PHONY: viewsevents viewseventsdiff-run
viewsevents: $(VIEWS_DIR)/viewseventsdiff.exe
$(VIEWS_DIR):
	mkdir -p $@
src/views/views_tables.inc src/views/views_objects_tables.inc &: tools/gen_views_tables.py tools/dialogmap.py orig/English/Views.DLL
	python3 -B tools/gen_views_tables.py
$(VIEWS_DIR)/viewseventsdiff.exe: tools/difftest/viewseventsdiff.c src/views/views_events.c src/views/views_objects.c src/views/views_stack.S tools/difftest/viewsstack.S src/views/views_objects.h src/views/views_objects_tables.inc src/views/views_internal.h src/views/views_unwind.h src/views/views_tables.inc | $(VIEWS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/viewseventsdiff.c src/views/views_events.c src/views/views_objects.c src/views/views_stack.S tools/difftest/viewsstack.S -lntdll
viewseventsdiff-run: viewsevents
	bash tools/run_viewsdiff.sh $(N)
.PHONY: views
views: $(VIEWS_DIR)/VIEWS_ours.dll
src/views/views_runtime_tables.inc $(VIEWS_DIR)/views.res $(VIEWS_DIR)/simsui-anchor.def &: tools/gen_views_runtime.py tools/dialogmap.py orig/English/Views.DLL orig/SIMSUI.DLL | $(VIEWS_DIR)
	python3 -B tools/gen_views_runtime.py
$(VIEWS_DIR)/libviews-simsui.a: $(VIEWS_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@
$(VIEWS_DIR)/views-res.o: $(VIEWS_DIR)/views.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(VIEWS_DIR)/VIEWS_ours.dll: src/views/views_events.c src/views/views_objects.c src/views/views_stack.S src/views/views_runtime.c src/views/views.def src/views/views_internal.h src/views/views_objects.h src/views/views_unwind.h src/views/views_tables.inc src/views/views_objects_tables.inc src/views/views_runtime_tables.inc $(VIEWS_DIR)/views-res.o $(VIEWS_DIR)/libviews-simsui.a
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x223e0000 -o $@ src/views/views_events.c src/views/views_objects.c src/views/views_stack.S src/views/views_runtime.c src/views/views.def $(VIEWS_DIR)/views-res.o $(VIEWS_DIR)/libviews-simsui.a -lkernel32 -luser32 -lmsvcrt
.PHONY: viewsuidiff-run
$(VIEWS_DIR)/viewsuidiff.exe: tools/difftest/viewsuidiff.c src/views/views_internal.h src/views/views_objects.h src/views/views_unwind.h | $(VIEWS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
viewsuidiff-run: views $(VIEWS_DIR)/viewsuidiff.exe
	bash tools/run_viewsdiff.sh --ui

.PHONY: views-breaks
views-breaks: viewsevents
	bash tools/run_viewsdiff.sh --breaks $(N)

.PHONY: views-ui-breaks views-smoke
views-ui-breaks: views $(VIEWS_DIR)/viewsuidiff.exe
	bash tools/run_viewsdiff.sh --ui-breaks
views-smoke: views
	bash tools/run_viewsdiff.sh --smoke
