# Standalone Alerts work; MAIN owns the shared Makefile.
CC := i686-w64-mingw32-gcc
ALERTS_DIR := bin/alerts-diff
N ?= 20000
.PHONY: alertsevents alertseventsdiff-run alerts-events-breaks alertsobjects alertsobjectsdiff-run alerts alertsuidiff-run alerts-smoke
.PHONY: alerts-object-breaks alerts-ui-breaks
alertsevents: $(ALERTS_DIR)/alertseventsdiff.exe
$(ALERTS_DIR):
	mkdir -p $@
src/alerts/alerts_tables.inc: tools/gen_alerts_tables.py tools/dialogmap.py orig/English/Alerts.DLL
	python3 -B tools/gen_alerts_tables.py
$(ALERTS_DIR)/alertseventsdiff.exe: tools/difftest/alertseventsdiff.c src/alerts/alerts_events.c src/alerts/alerts_internal.h src/alerts/alerts_unwind.h src/alerts/alerts_tables.inc | $(ALERTS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -o $@ tools/difftest/alertseventsdiff.c src/alerts/alerts_events.c -lntdll
alertseventsdiff-run: alertsevents
	bash tools/run_alertsdiff.sh $(N)
alerts-events-breaks: alertsevents
	bash tools/run_alertsdiff.sh --breaks $(N)
alertsobjects: $(ALERTS_DIR)/alertsobjectsdiff.exe
$(ALERTS_DIR)/alertsobjectsdiff.exe: tools/difftest/alertsobjectsdiff.c src/alerts/alerts_objects.c src/alerts/alerts_objects.h src/alerts/alerts_internal.h src/alerts/alerts_unwind.h | $(ALERTS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -fno-strict-aliasing -fno-delete-null-pointer-checks -o $@ tools/difftest/alertsobjectsdiff.c src/alerts/alerts_objects.c -lntdll
alertsobjectsdiff-run: alertsobjects
	bash tools/run_alertsdiff.sh --objects $(N)
alerts-object-breaks: alertsevents alertsobjects
	bash tools/run_alertsdiff.sh --object-breaks $(N)
alerts: $(ALERTS_DIR)/ALERTS_ours.dll
src/alerts/alerts_runtime_tables.inc $(ALERTS_DIR)/alerts.res $(ALERTS_DIR)/simsui-anchor.def &: tools/gen_alerts_runtime.py tools/dialogmap.py orig/English/Alerts.DLL orig/SIMSUI.DLL | $(ALERTS_DIR)
	python3 -B tools/gen_alerts_runtime.py
$(ALERTS_DIR)/libalerts-simsui.a: $(ALERTS_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@
$(ALERTS_DIR)/alerts-res.o: $(ALERTS_DIR)/alerts.res
	i686-w64-mingw32-windres -J res -O coff $< $@
$(ALERTS_DIR)/ALERTS_ours.dll: src/alerts/alerts_events.c src/alerts/alerts_objects.c src/alerts/alerts_runtime.c src/alerts/alerts.def src/alerts/alerts_internal.h src/alerts/alerts_objects.h src/alerts/alerts_unwind.h src/alerts/alerts_tables.inc src/alerts/alerts_runtime_tables.inc $(ALERTS_DIR)/alerts-res.o $(ALERTS_DIR)/libalerts-simsui.a
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22300000 -o $@ src/alerts/alerts_events.c src/alerts/alerts_objects.c src/alerts/alerts_runtime.c src/alerts/alerts.def $(ALERTS_DIR)/alerts-res.o $(ALERTS_DIR)/libalerts-simsui.a -lkernel32 -luser32 -lgdi32 -lmsvcrt
$(ALERTS_DIR)/alertsuidiff.exe: tools/difftest/alertsuidiff.c src/alerts/alerts_objects.h src/alerts/alerts_internal.h src/alerts/alerts_unwind.h | $(ALERTS_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32 -lntdll
alertsuidiff-run: alerts $(ALERTS_DIR)/alertsuidiff.exe
	bash tools/run_alertsdiff.sh --ui
alerts-ui-breaks: alerts $(ALERTS_DIR)/alertsuidiff.exe
	bash tools/run_alertsdiff.sh --ui-breaks
alerts-smoke: alerts
	bash tools/run_alertsdiff.sh --smoke
