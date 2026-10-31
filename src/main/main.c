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
#include <string.h>

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
#define G3D      M32(V(0x2080006c))

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

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
#define CHAIN_ADD     CALL(CHAIN, 0x18, f2)   /* (list, callback) */
#define CHAIN_REMOVE  CALL(CHAIN, 0x1c, f2)

/* ---- the registers, for the assembly slices ---------------------------------
 *
 * Retail's hand-written routines keep values in registers across calls and
 * hand them on; they are transliterated over these, as FE's and G3D's are.
 * A service entered through SERVICE gets the caller's registers in them and
 * hands back eax, ecx and edx as the body leaves them (ebx, esi, edi and
 * ebp are the caller's, as the retail routines save them). */
uint32_t main_eax, main_ebx, main_ecx, main_edx, main_esi, main_edi, main_ebp;
#define eax main_eax
#define ebx main_ebx
#define ecx main_ecx
#define edx main_edx
#define esi main_esi
#define edi main_edi

#define AX        ((uint16_t)eax)
#define AL        ((uint8_t)eax)
#define AH        ((uint8_t)(eax >> 8))
#define BX        ((uint16_t)ebx)
#define BL        ((uint8_t)ebx)
#define BH        ((uint8_t)(ebx >> 8))
#define CX        ((uint16_t)ecx)
#define CL        ((uint8_t)ecx)
#define CH        ((uint8_t)(ecx >> 8))
#define DX        ((uint16_t)edx)
#define DL        ((uint8_t)edx)
#define DH        ((uint8_t)(edx >> 8))
#define SET16(r, v) ((r) = ((r) & 0xffff0000u) | (uint16_t)(v))
#define SETLO(r, v) ((r) = ((r) & 0xffffff00u) | (uint8_t)(v))
#define SETHI(r, v) ((r) = ((r) & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define SET_AX(v) SET16(eax, v)
#define SET_AL(v) SETLO(eax, v)
#define SET_AH(v) SETHI(eax, v)
#define SET_BX(v) SET16(ebx, v)
#define SET_BL(v) SETLO(ebx, v)
#define SET_CX(v) SET16(ecx, v)
#define SET_CL(v) SETLO(ecx, v)
#define SET_CH(v) SETHI(ecx, v)
#define SET_DX(v) SET16(edx, v)
#define SET_DL(v) SETLO(edx, v)
#define SET_DH(v) SETHI(edx, v)

/* A routine that pushes and pops all six: what it does to them is undone. */
#define SAVE_ALL   uint32_t s_eax = eax, s_ebx = ebx, s_ecx = ecx, s_edx = edx, \
                            s_esi = esi, s_edi = edi
#define RESTORE_ALL (eax = s_eax, ebx = s_ebx, ecx = s_ecx, edx = s_edx, \
                     esi = s_esi, edi = s_edi)

#define SERVICE(name, bytes)                                                  \
    static void name##_body(const uint32_t *arg)                             \
        __attribute__((used, noinline));                                     \
    void name(void);                                                         \
    __asm__(".text\n\t.p2align 4\n"                                          \
            "_" #name ":\n\t"                                                \
            "movl %eax, _main_eax\n\tmovl %ecx, _main_ecx\n\t"               \
            "movl %edx, _main_edx\n\tmovl %ebx, _main_ebx\n\t"               \
            "movl %esi, _main_esi\n\tmovl %edi, _main_edi\n\t"               \
            "movl %ebp, _main_ebp\n\t"                                       \
            "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"       \
            "leal 20(%esp), %eax\n\tpushl %eax\n\t"                          \
            "call _" #name "_body\n\t"                                       \
            "addl $4, %esp\n\t"                                              \
            "popl %ebp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\t"           \
            "movl _main_eax, %eax\n\tmovl _main_ecx, %ecx\n\t"               \
            "movl _main_edx, %edx\n\t"                                       \
            "ret $" #bytes "\n");                                            \
    static void name##_body(const uint32_t *arg)

/* External calls from the transliterated assembly: through these thunks,
 * the callee's eax, ecx and edx land in the register variables, as they
 * would in the hardware registers retail goes on to read.  N is the
 * number of stdcall arguments; the callee pops them. */
uint32_t main_xcall0(uint32_t fn);
uint32_t main_xcall1(uint32_t fn, uint32_t a);
uint32_t main_xcall2(uint32_t fn, uint32_t a, uint32_t b);
uint32_t main_xcall3(uint32_t fn, uint32_t a, uint32_t b, uint32_t c);
uint32_t main_xcall4(uint32_t fn, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
#define XCALL_THUNK(n, pushes)                                               \
    __asm__(".text\n\t.p2align 4\n\t.globl _main_xcall" #n "\n"            \
            "_main_xcall" #n ":\n\t"                                         \
            "pushl %ebp\n\tmovl %esp, %ebp\n\t"                             \
            pushes                                                           \
            "call *8(%ebp)\n\t"                                              \
            "movl %eax, _main_eax\n\tmovl %ecx, _main_ecx\n\t"               \
            "movl %edx, _main_edx\n\t"                                       \
            "popl %ebp\n\tret\n")
XCALL_THUNK(0, "");
XCALL_THUNK(1, "pushl 12(%ebp)\n\t");
XCALL_THUNK(2, "pushl 16(%ebp)\n\tpushl 12(%ebp)\n\t");
XCALL_THUNK(3, "pushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\t");
XCALL_THUNK(4, "pushl 24(%ebp)\n\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\t");
#define SLOT_OF(m, off) M32((m) + (off))

/* div and idiv, as retail executes them under the host's divide repair
 * (0x4119a0, docs/modules/host.md): a quotient that does not fit, or a
 * divisor of 0, gives all ones for div, the largest quotient (or the most
 * negative but one) for idiv. */
static void div8(uint8_t d)
{
    if (d == 0 || AX / d > 0xff) {
        SET_AX(0xffff);
        return;
    }
    SET_AX((AX % d) << 8 | AX / d);
}

static void div16(uint16_t d)
{
    uint32_t n = ((uint32_t)DX << 16) | AX;

    if (d == 0 || n / d > 0xffff) {
        SET_AX(0xffff);
        SET_DX(0xffff);
        return;
    }
    SET_AX(n / d);
    SET_DX(n % d);
}

static void div32(uint32_t d)
{
    uint64_t n = ((uint64_t)edx << 32) | eax;

    if (d == 0 || n / d > 0xffffffffu) {
        eax = 0xffffffffu;
        edx = 0xffffffffu;
        return;
    }
    eax = (uint32_t)(n / d);
    edx = (uint32_t)(n % d);
}

static void mul8(uint8_t m)
{
    SET_AX((uint16_t)AL * m);
}

static void mul16(uint16_t m)
{
    uint32_t p = (uint32_t)AX * m;

    SET_AX(p);
    SET_DX(p >> 16);
}

static void imul16(uint16_t m)
{
    uint32_t p = (uint32_t)((int32_t)(int16_t)AX * (int16_t)m);

    SET_AX(p);
    SET_DX(p >> 16);
}

static void mul32(uint32_t m)
{
    uint64_t p = (uint64_t)eax * m;

    eax = (uint32_t)p;
    edx = (uint32_t)(p >> 32);
}

static void imul32(uint32_t m)
{
    uint64_t p = (uint64_t)((int64_t)(int32_t)eax * (int32_t)m);

    eax = (uint32_t)p;
    edx = (uint32_t)(p >> 32);
}

/* ---- the image -------------------------------------------------------------- */

struct main_image main_image = {{
#include "main_data.inc"
}};

static const struct { uint32_t at, to; } data_pointers[] = {
#define MAIN_DATA_POINTER(at, to) { at, to },
#define MAIN_CODE_TARGET(a)
#define MAIN_LINKAGE_RETAIL(at, v)
#include "main_pointers.inc"
#undef MAIN_DATA_POINTER
#undef MAIN_CODE_TARGET
#undef MAIN_LINKAGE_RETAIL
};

/* Retail's Linkage, as an unbounded read of the image sees it. */
static const uint32_t retail_linkage[] = {
#define MAIN_DATA_POINTER(at, to)
#define MAIN_CODE_TARGET(a)
#define MAIN_LINKAGE_RETAIL(at, v) v,
#include "main_pointers.inc"
#undef MAIN_DATA_POINTER
#undef MAIN_CODE_TARGET
#undef MAIN_LINKAGE_RETAIL
};

#include "main_records.inc"
#include "main_situation.inc"
#include "main_nav.inc"
#include "main_season.inc"
#include "main_sun.inc"
#include "main_eye.inc"
#include "main_band.inc"
#include "main_flights.inc"
#include "main_prefs.inc"
#include "main_clouds.inc"
#include "main_spot.inc"
#include "main_frame.inc"
#include "main_tick.inc"
#include "main_step.inc"
#include "main_dialogs.inc"
#include "main_commands.inc"

__asm__(".globl _ImportTable\n\t.set _ImportTable, _main_image + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _main_image + 0xc0");

/* The Linkage's slots that are done, by offset; the rest stay null until
 * their slice is. */
static void main_install(void)
{
    static const struct { uint32_t slot; void *fn; } services[] = {
        { 0x04, (void *)main_init },
        { 0x08, (void *)main_deinit },
        { 0x18, (void *)main_step },
        { 0x20, (void *)main_command },
        { 0x50, (void *)flight_set_record },
        { 0xac, (void *)flight_save_dialog },
        { 0xc4, (void *)flight_open },
        { 0x24, (void *)main_tick },
        { 0x38, (void *)load_default },
        { 0x3c, (void *)load_flight },
        { 0x40, (void *)save_flight },
        { 0x44, (void *)reload_flight },
        { 0x48, (void *)load_memory },
        { 0x4c, (void *)load_file },
        { 0x54, (void *)record_current },
        { 0x58, (void *)record_read },
        { 0x5c, (void *)record_begin },
        { 0x60, (void *)record_write },
        { 0x64, (void *)record_end },
        { 0x68, (void *)record_find },
        { 0x6c, (void *)record_find_flight },
        { 0x70, (void *)nav_range_bearing },
        { 0x74, (void *)nav_season },
        { 0x78, (void *)nav_readout },
        { 0x7c, (void *)nav_describe },
        { 0x80, (void *)nav_select },
        { 0x1c, (void *)lat_clamp },
        { 0x30, (void *)menu_grey },
        { 0x34, (void *)menu_enable },
        { 0x84, (void *)obj_alloc },
        { 0x88, (void *)obj_step },
        { 0x8c, (void *)obj_free },
        { 0x94, (void *)clouds_textures },
        { 0x98, (void *)clouds_draw },
        { 0x9c, (void *)nav_band },
        { 0xa0, (void *)nav_sun },
        { 0xa4, (void *)nav_clock_ascii },
        { 0xa8, (void *)spot_view },
        { 0xb0, (void *)flight_quicksave },
        { 0xb4, (void *)prefs_load },
        { 0xb8, (void *)prefs_save },
        { 0xbc, (void *)prefs_defaults },
        { 0xc0, (void *)nav_eye },
        { 0xc8, (void *)nav_viewcycle },
    };
    uint32_t i;

    for (i = 0; i < sizeof data_pointers / sizeof data_pointers[0]; i++)
        M32(D + data_pointers[i].at) = D + data_pointers[i].to;
    for (i = 0; i < sizeof services / sizeof services[0]; i++)
        M32(D + MAIN_LINKAGE + services[i].slot) = ADDR(services[i].fn);
    /* the dialog descriptors' callbacks, over retail's code addresses */
    M32(V(0x2080068c)) = ADDR(dialog_1305_check);
    M32(V(0x2080123c)) = ADDR(dialog_1304_check);
    M32(V(0x208001a4)) = ADDR(dialog_1203_check);
    M32(V(0x208035ec)) = ADDR(save_dialog_check);
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

/* +0x74 is still reached through the seam in the situations suite; the season
 * suite drives it directly through this export. */
__declspec(dllexport) uint32_t FS98_STDCALL main_season(uint32_t a, uint32_t b)
{
    return nav_season(a, b);
}

/* +0xa4 is still reached through the seam in the situations suite; the clock
 * suite drives it directly through this export. */
__declspec(dllexport) uint32_t FS98_STDCALL main_clock(void)
{
    return nav_clock_ascii();
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
