# Isolated partial MP_DIALOG replacement. This does not edit shared build or
# install wiring and never targets the live game tree.
CC := i686-w64-mingw32-gcc
MPD_DIR := bin/mpdialog-diff

.PHONY: mpdialog mpdialog-runtime-tables mpdialoguidiff-run mpdialog-ui-breaks mpdialog-smoke
mpdialog: $(MPD_DIR)/MP_DIALOG_ours.dll

$(MPD_DIR):
	mkdir -p $@

src/mpdialog/mpdialog_runtime_tables.inc $(MPD_DIR)/mpdialog-0x110a-intro.res $(MPD_DIR)/mpdialog-0x110a-choice.res $(MPD_DIR)/mpdialog-0x110a-host.res $(MPD_DIR)/mpdialog-0x110a-connection.res $(MPD_DIR)/mpdialog-0x110a-browser.res $(MPD_DIR)/mpdialog-flow-support.res $(MPD_DIR)/mpdialog-0x110b.res $(MPD_DIR)/mpdialog-0x110c.res $(MPD_DIR)/simsui-anchor.def &: tools/gen_mpdialog_runtime.py tools/dialogmap.py orig/English/mpdialog.dll orig/SIMSUI.DLL
	python3 -B tools/gen_mpdialog_runtime.py

$(MPD_DIR)/libmpdialog-simsui.a: $(MPD_DIR)/simsui-anchor.def
	i686-w64-mingw32-dlltool -d $< -l $@

$(MPD_DIR)/mpdialog-res.o: $(MPD_DIR)/mpdialog-0x110b.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/mpdialog-details-res.o: $(MPD_DIR)/mpdialog-0x110c.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/mpdialog-flow-intro-res.o: $(MPD_DIR)/mpdialog-0x110a-intro.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/mpdialog-flow-choice-res.o: $(MPD_DIR)/mpdialog-0x110a-choice.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/mpdialog-flow-host-res.o: $(MPD_DIR)/mpdialog-0x110a-host.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/mpdialog-flow-connection-res.o: $(MPD_DIR)/mpdialog-0x110a-connection.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/mpdialog-flow-browser-res.o: $(MPD_DIR)/mpdialog-0x110a-browser.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/mpdialog-flow-support-res.o: $(MPD_DIR)/mpdialog-flow-support.res
	i686-w64-mingw32-windres -J res -O coff $< $@

$(MPD_DIR)/MP_DIALOG_ours.dll: src/mpdialog/mpdialog_flow.c src/mpdialog/mpdialog_session.c src/mpdialog/mpdialog_details.c src/mpdialog/mpdialog_runtime.c src/mpdialog/mpdialog_internal.h src/mpdialog/mpdialog_runtime_tables.inc src/mpdialog/mpdialog.def $(MPD_DIR)/mpdialog-res.o $(MPD_DIR)/mpdialog-details-res.o $(MPD_DIR)/mpdialog-flow-intro-res.o $(MPD_DIR)/mpdialog-flow-choice-res.o $(MPD_DIR)/mpdialog-flow-host-res.o $(MPD_DIR)/mpdialog-flow-connection-res.o $(MPD_DIR)/mpdialog-flow-browser-res.o $(MPD_DIR)/mpdialog-flow-support-res.o $(MPD_DIR)/libmpdialog-simsui.a
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -fno-delete-null-pointer-checks -shared -nostdlib -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--image-base=0x22340000 -o $@ src/mpdialog/mpdialog_flow.c src/mpdialog/mpdialog_session.c src/mpdialog/mpdialog_details.c src/mpdialog/mpdialog_runtime.c src/mpdialog/mpdialog.def $(MPD_DIR)/mpdialog-res.o $(MPD_DIR)/mpdialog-details-res.o $(MPD_DIR)/mpdialog-flow-intro-res.o $(MPD_DIR)/mpdialog-flow-choice-res.o $(MPD_DIR)/mpdialog-flow-host-res.o $(MPD_DIR)/mpdialog-flow-connection-res.o $(MPD_DIR)/mpdialog-flow-browser-res.o $(MPD_DIR)/mpdialog-flow-support-res.o $(MPD_DIR)/libmpdialog-simsui.a -lkernel32 -luser32 -lcomctl32 -lmsvcrt

$(MPD_DIR)/mpdialoguidiff.exe: tools/difftest/mpdialoguidiff.c src/mpdialog/mpdialog_internal.h | $(MPD_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -luser32 -lgdi32 -lntdll

mpdialoguidiff-run: mpdialog $(MPD_DIR)/mpdialoguidiff.exe
	bash tools/run_mpdialogdiff.sh --ui

mpdialog-ui-breaks: mpdialog $(MPD_DIR)/mpdialoguidiff.exe
	bash tools/run_mpdialogdiff.sh --ui-breaks

mpdialog-smoke: mpdialog
	bash tools/run_mpdialogdiff.sh --smoke
