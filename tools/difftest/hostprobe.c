/* Can the host executable be loaded and called like a library?
 *
 * FLTSIM98.EXE has no .reloc section, so it can only map at its stated base of
 * 0x400000 and every address in the service map is absolute.  Loading it with
 * DONT_RESOLVE_DLL_REFERENCES maps and relocates nothing and runs no entry
 * point, which is what makes it safe to poke at: services that call imports
 * would fault, but the 84 that call nothing cannot.
 *
 * This harness is linked at a different image base so it does not collide.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hostload.h"

#define HOST_TABLE 0x00417ba0

typedef int32_t (__attribute__((stdcall)) *fn_void_i)(void);

int main(void)
{
    unsigned char *h = fs98_map_host("FLTSIM98.EXE");
    unsigned char *table;
    unsigned i, code = 0;

    if (!h) return 2;
    printf("mapped at %p\n", (void *)h);

    table = (unsigned char *)HOST_TABLE;
    printf("table id = %u (expect 1)\n", *(uint32_t *)table);
    for (i = 4; i <= 0x31c; i += 4) {
        uint32_t v = *(uint32_t *)(table + i);
        if (v >= 0x401000 && v < 0x415060) code++;
    }
    printf("%u service slots point into .text\n", code);

    /* +0x01c is `return 1` -- the simplest possible call across the boundary */
    printf("+0x01c returns %d (expect 1)\n", ((fn_void_i)*(void **)(table + 0x1c))());
    /* +0x020 reads a global */
    printf("+0x020 returns %d\n", ((fn_void_i)*(void **)(table + 0x20))());
    return 0;
}
