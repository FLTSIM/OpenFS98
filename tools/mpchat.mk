# Native MPCHAT acceptance stays separate from MAIN/HOST and the shared build.
CC := i686-w64-mingw32-gcc
MPCHAT_DIR := bin/mpchat-ui
N ?= 24
MPCHAT_CFLAGS := -std=c99 -Os -Wall -Wextra -Werror -Iinclude -ffreestanding -fno-builtin -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-delete-null-pointer-checks
MPCHAT_LDFLAGS := -shared -nostdlib -Wl,--enable-stdcall-fixup -Wl,--entry=_DllMain@12 -Wl,--disable-dynamicbase -Wl,--disable-nxcompat -Wl,--image-base=0x20840000
.PHONY: mpchat-ui mpchatuidiff-run mpchat-ui-breaks mpchat-modeldiff-run
mpchat-ui: $(MPCHAT_DIR)/MPCHAT_ours.dll $(MPCHAT_DIR)/mpchatuidiff.exe
$(MPCHAT_DIR):
	mkdir -p $@
src/mpchat/mpchat_data.inc: orig/Modules/mpchat.DLL tools/gen_mpchat_data.py
	python3 -B tools/gen_mpchat_data.py $< $@
$(MPCHAT_DIR)/mpchat.res: src/mpchat/mpchat.rc | $(MPCHAT_DIR)
	i686-w64-mingw32-windres $< -O coff -o $@
$(MPCHAT_DIR)/MPCHAT_ours.dll: src/mpchat/mpchat.c src/mpchat/mpchat.def src/mpchat/mpchat_data.inc include/fs98/linkage.h include/fs98/host.h $(MPCHAT_DIR)/mpchat.res
	$(CC) $(MPCHAT_CFLAGS) $(MPCHAT_LDFLAGS) -o $@ src/mpchat/mpchat.c $(MPCHAT_DIR)/mpchat.res src/mpchat/mpchat.def -lkernel32 -luser32 -lgdi32 -lgcc
$(MPCHAT_DIR)/mpchatuidiff.exe: tools/difftest/mpchatuidiff.c | $(MPCHAT_DIR)
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -o $@ $< -lgdi32
mpchatuidiff-run: mpchat-ui
	bash tools/run_mpchatuidiff.sh --ui $(MPCHAT_DIR)/MPCHAT_ours.dll $(N)
mpchat-ui-breaks: mpchat-ui
	bash tools/run_mpchatuidiff.sh --breaks
$(MPCHAT_DIR)/mpchatdiff.exe: tools/difftest/mpchatdiff.c | $(MPCHAT_DIR)
	$(CC) -std=c99 -O1 -Wall -Wextra -o $@ $<
mpchat-modeldiff-run: $(MPCHAT_DIR)/mpchatdiff.exe $(MPCHAT_DIR)/MPCHAT_ours.dll
	bash tools/run_mpchatuidiff.sh --model
