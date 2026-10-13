/* GLOBAL.DLL -- module id 2.
 *
 * The first module reimplemented, because it is the smallest meaningful one:
 * the retail DLL has 54 bytes of .text, all of it boilerplate (a minimal
 * DllMain plus one no-op reached through both Linkage service slots).  Its
 * actual content is data -- a 4776 byte block that every other module reads
 * and writes as byte offsets from the Linkage pointer it was handed at init.
 *
 * So GLOBAL has no behaviour to reverse: get the ABI and the power-on defaults
 * right and it is finished.  Everything still to be learned about it is the
 * *meaning* of those offsets, which gets filled in from the other modules as
 * they are worked through -- each one that reads [global + N] names a field.
 *
 * Known offsets so far:
 *   +0x365  byte, read by SLEW service slot 4 as an early-out flag
 */

#include <windows.h>

#include "fs98/linkage.h"
#include "global_state.inc"

/* Declared before the no-op and before Linkage so it lands at .text+0, where
 * the retail module's entry point is. */
BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}

static void global_noop(void) { }

/* Laid out to match the retail .data exactly: the Linkage header sits at the
 * base of the block and the shared state follows it. */
struct global_module {
    uint32_t id;
    void   (*init)(void);
    void   (*deinit)(void);
    unsigned char state[FS98_GLOBAL_SIZE - FS98_GLOBAL_HEAD];
};

struct global_module Linkage = {
    FS98_MOD_GLOBAL,
    global_noop,
    global_noop,
    { FS98_GLOBAL_DEFAULTS }
};

/* GLOBAL imports nothing, so unlike every other module it exports no
 * ImportTable -- matching the retail DLL, which has a single export. */

