/* The native SIM1.sim: src/sim1/sim1_small.c (every routine, decompiled and verified against
 * retail by tools/difftest/sim1rdiff.c) over retail's own data.
 *
 * The image is retail's .data (0x22016000, 0x43f8 bytes): the ImportTable the host fills (11
 * module links), the Linkage it calls (+0x58) and the state, from retail's bytes
 * (bin/sim1-native/sim1_data.inc, tools/gen_sim1_native.py); retail's relocations put pointers
 * only in the Linkage, which is written here. sim1_base is placed so that sim1_base + rva lands
 * in a copy of retail's .rdata (0x22015000, 0x759 bytes, no pointers), where RF64 reads the
 * constants. Retail's MSVCRT imports (_CIpow, _CIacos, _CIasin; _ftol is sim1_ftol) are
 * found at attach, as retail's import table has them.
 *
 * Linkage entries leave the registers retail leaves for their callers (sim1_tables.inc: eax and
 * the callee-saved). Three leave the caller's eax, or its upper half, which C cannot say: the
 * descriptor (+0x18), the word (+0x28, its ax) and the wind (+0x38, its result in st0). They
 * go through the thunks below, which keep eax around the C. */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include "sim1_internal.h"
#include "sim1_routines.h"

#define SIM1_DATA_SIZE 0x43f8u
#define SIM1_RDATA_RVA 0x15000u
#define SIM1_RDATA_SIZE 0x759u

struct sim1_native_image {
    uint32_t imports[22];                 /* (module id, link) pairs; the host fills the links */
    uintptr_t linkage[15];
    uint8_t data[SIM1_DATA_SIZE - 0x94];
};

uint32_t __cdecl sim1_linkage_init(void);
uint32_t __cdecl sim1_linkage_deinit(void);
void __stdcall sim1_linkage_descriptor(uint8_t *out);
uint32_t __stdcall sim1_linkage_word(uint8_t *out);
void __cdecl sim1_linkage_wind(void);

struct sim1_native_image sim1_native_image __attribute__((aligned(16))) = {
    {1, 0, 2, 0, 23, 0, 4, 0, 16, 0, 3, 0, 32, 0, 39, 0, 20, 0, 10, 0, 0, 0},
    {25, (uintptr_t)sim1_linkage_init, (uintptr_t)sim1_linkage_deinit, 0, 0, 0,
     (uintptr_t)sim1_linkage_descriptor, (uintptr_t)sim1_aircraft_setup,
     (uintptr_t)((uint8_t *)&sim1_native_image + 0x1150),
     (uintptr_t)((uint8_t *)&sim1_native_image + 0x1158),
     (uintptr_t)sim1_linkage_word, (uintptr_t)sim1_service_scaled, (uintptr_t)sim1_service_vector,
     (uintptr_t)sim1_service_reset, (uintptr_t)sim1_linkage_wind},
    {
#include "sim1_data.inc"
    }
};

typedef char sim1_native_layout_check[(sizeof(struct sim1_native_image) == SIM1_DATA_SIZE &&
                                       offsetof(struct sim1_native_image, linkage) == 0x58 &&
                                       offsetof(struct sim1_native_image, data) == 0x94) ? 1 : -1];

__asm__(".globl _ImportTable\n.set _ImportTable, _sim1_native_image\n"
        ".globl _Linkage\n.set _Linkage, _sim1_native_image + 0x58\n");

static const uint8_t sim1_rdata[SIM1_RDATA_SIZE] = {
#include "sim1_rdata.inc"
};

uint8_t *sim1_data = (uint8_t *)&sim1_native_image;
uint8_t *sim1_base = (uint8_t *)sim1_rdata - SIM1_RDATA_RVA;
void *sim1_crt_cipow, *sim1_crt_ciacos, *sim1_crt_ciasin;
uint32_t sim1_callbacks[3] = {(uint32_t)(uintptr_t)sim1_chain_tick, (uint32_t)(uintptr_t)sim1_engines_frame,
                              (uint32_t)(uintptr_t)sim1_situation_message};

/* +0x04 and +0x08: the C, as retail's entries are plain calls returning eax */
uint32_t __cdecl sim1_linkage_init(void) { return sim1_init_core(); }
uint32_t __cdecl sim1_linkage_deinit(void) { return sim1_deinit_core(); }

/* +0x18 keeps the caller's eax; +0x28 sets only ax; +0x38 keeps eax, its result in st0 */
__asm__(".text\n"
        ".globl _sim1_linkage_descriptor@4\n"
        "_sim1_linkage_descriptor@4:\n"
        "    pushl %eax\n"
        "    pushl 8(%esp)\n"
        "    call _sim1_service_descriptor@4\n"
        "    popl %eax\n"
        "    ret $4\n"
        ".globl _sim1_linkage_word@4\n"
        "_sim1_linkage_word@4:\n"
        "    pushl %eax\n"
        "    pushl 8(%esp)\n"
        "    call _sim1_service_word@4\n"
        "    movw %ax, (%esp)\n"
        "    popl %eax\n"
        "    ret $4\n"
        ".globl _sim1_linkage_wind\n"
        "_sim1_linkage_wind:\n"
        "    pushl %eax\n"
        "    call _sim1_service_wind\n"
        "    popl %eax\n"
        "    ret\n");

/* retail's entry (0x22001000): on attach, DisableThreadLibraryCalls; always TRUE */
BOOL WINAPI sim1_native_entry(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HMODULE msvcrt = LoadLibraryA("msvcrt.dll");
        if (!msvcrt) return FALSE;
        sim1_crt_cipow = (void *)GetProcAddress(msvcrt, "_CIpow");
        sim1_crt_ciacos = (void *)GetProcAddress(msvcrt, "_CIacos");
        sim1_crt_ciasin = (void *)GetProcAddress(msvcrt, "_CIasin");
        if (!sim1_crt_cipow || !sim1_crt_ciacos || !sim1_crt_ciasin) return FALSE;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
