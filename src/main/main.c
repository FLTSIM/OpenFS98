/* MAIN.DLL -- module id 3: the sim's controller.
 *
 * Situations and flights, the default aircraft, clouds and smoke, the
 * frame's order of business, and most of the menus' commands.  It imports
 * every other module.  Retail is MSVC C (about 92 KB) with hand-written
 * assembly (about 28 KB, the routines at odd addresses) interleaved.
 *
 * In progress: services are transliterated a slice at a time, each
 * verified by tools/difftest/maindiff.c before the next; until every slot
 * is done the module is not in MODULES and is never installed.  The slices
 * done are listed in docs/modules/main.md.
 *
 * The whole 0x2ee00-byte .data image is kept at retail's offsets.  Its
 * pointers into itself are installed at load from a generated table; its
 * other code pointers keep retail's values (the slices that call through
 * them map them), and the Linkage's are installed here.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define MAIN_IMAGE_SIZE 0x2ee00u
#define MAIN_DATA_VA    0x20800000u    /* retail's .data */
#define MAIN_LINKAGE    0x0c0u

typedef volatile uint8_t  main_v8;
typedef volatile uint16_t __attribute__((may_alias)) main_v16;
typedef volatile uint32_t __attribute__((may_alias)) main_v32;

struct main_image {
    uint8_t bytes[MAIN_IMAGE_SIZE];
};

extern struct main_image main_image;

#define D       ((uint32_t)(uintptr_t)main_image.bytes)
#define V(a)    (D + ((uint32_t)(a) - MAIN_DATA_VA))
#define M8(a)   (*(main_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(main_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(main_v32 *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

/* The ImportTable's links, as retail reads them (ds:0x20800004 and on). */
#define HOST     M32(V(0x20800004))
#define G        M32(V(0x2080000c))
#define CHAIN    M32(V(0x20800034))

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_FILE_CLOSE  HOST_(0x13c, f1)   /* (file) */
#define H_EXISTS      HOST_(0x158, f2)   /* (path, ext): 0 if it is there */
#define H_FILE_OPEN   HOST_(0x17c, f4)   /* (&file, path, ext, mode) */
#define H_FILE_READ   HOST_(0x19c, f4)   /* (file, buf, n, &got) */
#define H_FILE_SIZE   HOST_(0x1a4, f2)   /* (file, &size) */
#define H_FILE_WRITE  HOST_(0x1b0, f4)   /* (file, buf, n, &wrote) */
#define H_ALLOC       HOST_(0x20c, f4)   /* (flags, size, &out, zero) */
#define H_FREE        HOST_(0x214, f1)   /* (&p) */
#define H_MEMMOVE     HOST_(0x21c, f3)   /* (dst, src, n) */

#define CHAIN_SEND    CALL(CHAIN, 0x20, f3)   /* (list, a, b) */

#include "main_records.inc"

/* ---- the image -------------------------------------------------------------- */

struct main_image main_image = {{
#include "main_data.inc"
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _main_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _main_image + 0xc0");

static const struct { uint32_t at, to; } data_pointers[] = {
#define MAIN_DATA_POINTER(at, to) { at, to },
#define MAIN_CODE_TARGET(a)
#include "main_pointers.inc"
#undef MAIN_DATA_POINTER
#undef MAIN_CODE_TARGET
};

/* The Linkage's slots that are done, by offset; the rest stay null until
 * their slice is. */
static void main_install(void)
{
    static const struct { uint32_t slot; void *fn; } services[] = {
        { 0x54, (void *)record_current },
        { 0x58, (void *)record_read },
        { 0x5c, (void *)record_begin },
        { 0x60, (void *)record_write },
        { 0x64, (void *)record_end },
        { 0x68, (void *)record_find },
        { 0x6c, (void *)record_find_flight },
    };
    uint32_t i;

    for (i = 0; i < sizeof data_pointers / sizeof data_pointers[0]; i++)
        M32(D + data_pointers[i].at) = D + data_pointers[i].to;
    for (i = 0; i < sizeof services / sizeof services[0]; i++)
        M32(D + MAIN_LINKAGE + services[i].slot) = ADDR(services[i].fn);
}

/* The internal entry points the harness drives directly until their
 * callers are done (maindiff.c finds them by name). */
__declspec(dllexport) uint32_t FS98_STDCALL main_dispatch_record(uint32_t rec)
{
    return dispatch_record(rec);
}

__declspec(dllexport) uint32_t FS98_STDCALL main_save_records(uint32_t file)
{
    return save_records(file);
}

/* .text+0x1000 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        main_install();
    }
    return TRUE;
}
