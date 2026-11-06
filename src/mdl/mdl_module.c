/* Normal MDL module boundary. Model data and pointer fixups are generated
 * from the CD; the executable update is the verified C in mdl_normal.c. */
#include <windows.h>
#include <stdint.h>
#include "fs98/linkage.h"
#include "mdl_normal.h"
#include "model_config.h"

extern uint8_t mdl_image[];
extern struct fs98_import ImportTable[];

void FS98_STDCALL mdl_update(uint8_t *state)
{
    fs98_mdl_normal_update(MDL_KIND, state,
                          (const uint8_t *)ImportTable[1].link, mdl_image,
                          FS98_HOST_MEMMOVE(ImportTable[0].link));
}

/* Match the no-op boundaries exactly, including their return registers.
 * Attach leaves eax pointing at the caller's aircraft, as retail does. */
__asm__(".text\n"
        ".globl _mdl_nop\n_mdl_nop:\n\tret\n"
        ".globl _mdl_state@4\n_mdl_state@4:\n\tret $4\n"
        ".globl _mdl_detach\n_mdl_detach:\n\tret\n"
        ".globl _mdl_attach@4\n_mdl_attach@4:\n\t"
        "movl 4(%esp), %eax\n\tmovl $_mdl_header, (%eax)\n\tret $4\n");

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
