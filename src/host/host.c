/* HOST -- module id 1, the host itself (FLTSIM98.EXE).
 *
 * The host is the executable, not a DLL: every module imports it, and it hands
 * each one a pointer to a 0x320-byte service table (a dword 1 then 194 slots)
 * that the retail build keeps at 0x417ba0.  A native OpenFS98 reimplements that
 * table on a portable runtime; this is that reimplementation, done a slice at a
 * time and verified by tools/difftest/hostdiff.c, which maps the real host at
 * 0x400000 and runs it in process beside this one, service by service.  The
 * mapping (see host.md) and the slices done are tracked there.
 *
 * The whole 0x55cc-byte .data image is kept at retail's offsets so the harness
 * can compare it; our service table is exported as HostTable, the same shape as
 * the host's, with unfinished slots left null.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/host.h"

#define HOST_DATA_VA    0x417000u
#define HOST_IMAGE_SIZE 0x55ccu

typedef volatile uint8_t  host_v8;
typedef volatile uint16_t __attribute__((may_alias)) host_v16;
typedef volatile uint32_t __attribute__((may_alias)) host_v32;

struct host_image {
    uint8_t bytes[HOST_IMAGE_SIZE];
};

struct host_image host_image = {{
#include "host_data.inc"
}};

#define D       ((uint32_t)(uintptr_t)host_image.bytes)
#define V(a)    (D + ((uint32_t)(a) - HOST_DATA_VA))
#define M8(a)   (*(host_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(host_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(host_v32 *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

/* ---- services ---------------------------------------------------------------- */

/* +0x18 (0x4084e0): copies six fields of the argument struct into the host
 * globals at 0x418830.  Returns 1. */
static uint32_t FS98_STDCALL host_018(uint32_t arg)
{
    M32(V(0x418830)) = M32(arg + 0x18);
    M32(V(0x418834)) = M32(arg + 0x20);
    M32(V(0x418838)) = M32(arg + 0x24);
    M32(V(0x41883c)) = M32(arg + 0x28);
    M32(V(0x418840)) = M32(arg + 0x2c);
    M32(V(0x418844)) = M32(arg + 0xbc);
    return 1;
}

/* +0x1c (0x408530): returns the constant 1. */
static uint32_t FS98_STDCALL host_01c(void)
{
    return 1;
}

/* +0x2c (0x401a00): sets the host flag at 0x4187ec to 1.  (eax is leftover; no
 * caller reads it.) */
static uint32_t FS98_STDCALL host_02c(void)
{
    M32(V(0x4187ec)) = 1;
    return 0;
}

/* +0x54 (0x4087c0): writes the host globals 0x41880c and 0x418828 back through
 * the two out-pointers. */
static uint32_t FS98_STDCALL host_054(uint32_t out1, uint32_t out2)
{
    M32(out1) = M32(V(0x41880c));
    M32(out2) = M32(V(0x418828));
    return 0;
}

/* +0x20 (0x408540): returns the global word at 0x41719c. */
static uint32_t FS98_STDCALL host_020(void)
{
    return M32(V(0x41719c));
}

/* +0x34 (0x4085b0): returns the global word at 0x418850. */
static uint32_t FS98_STDCALL host_034(void)
{
    return M32(V(0x418850));
}

/* +0x58 (0x4087e0): returns the address immediately past the service table,
 * where the host keeps the next structure. */
static uint32_t FS98_STDCALL host_058(void)
{
    return V(0x417ec0);
}

/* +0x5c (0x4087f0) and +0x60 (0x408800): return the addresses of two further
 * host structures. */
static uint32_t FS98_STDCALL host_05c(void)
{
    return V(0x41c240);
}

static uint32_t FS98_STDCALL host_060(void)
{
    return V(0x41bb80);
}

/* +0x2a8 (0x40ee70): strcpy(dst, src), returns dst. */
static uint32_t FS98_STDCALL host_2a8(uint32_t dst, uint32_t src)
{
    char *d = (char *)(uintptr_t)dst;
    const char *s = (const char *)(uintptr_t)src;
    while ((*d++ = *s++) != '\0')
        ;
    return dst;
}

/* +0x2b0 (0x40eec0): strcat(dst, src), returns dst. */
static uint32_t FS98_STDCALL host_2b0(uint32_t dst, uint32_t src)
{
    char *d = (char *)(uintptr_t)dst;
    const char *s = (const char *)(uintptr_t)src;
    while (*d != '\0')
        d++;
    while ((*d++ = *s++) != '\0')
        ;
    return dst;
}

/* +0x130 (0x40a970): copies the host's string constant at 0x41bca0 into the
 * caller's buffer (a plain strcpy from a fixed source).  Returns 0. */
static uint32_t FS98_STDCALL host_130(uint32_t dst)
{
    char *d = (char *)(uintptr_t)dst;
    const char *s = (const char *)(uintptr_t)V(0x41bca0);
    while ((*d++ = *s++) != '\0')
        ;
    return 0;
}

/* +0x168 (0x40a890): true when the string begins "\\" (a UNC path). */
static uint32_t FS98_STDCALL host_168(uint32_t s)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)s;
    return (p != 0 && p[0] == 0x5c && p[1] == 0x5c) ? 1 : 0;
}

/* +0x218 (0x40cf20): memset(dst, val, count).  Returns 0. */
static uint32_t FS98_STDCALL host_218(uint32_t dst, uint32_t val, uint32_t count)
{
    uint8_t *d = (uint8_t *)(uintptr_t)dst;
    uint32_t i;
    for (i = 0; i < count; i++)
        d[i] = (uint8_t)val;
    return 0;
}

/* +0x228 (0x40cf90): memcmp(a, b, n) -- -1, 0 or 1. */
static uint32_t FS98_STDCALL host_228(uint32_t a, uint32_t b, uint32_t n)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)a;
    const uint8_t *q = (const uint8_t *)(uintptr_t)b;
    uint32_t i;
    for (i = 0; i < n; i++)
        if (p[i] != q[i])
            return p[i] < q[i] ? (uint32_t)-1 : 1;
    return 0;
}

/* +0x288 (0x40dd80): stores the argument in the host global 0x417194.
 * Returns 0. */
static uint32_t FS98_STDCALL host_288(uint32_t v)
{
    M32(V(0x417194)) = v;
    return 0;
}

/* +0x28c (0x40e260): compacts the 256-entry, 4-byte-per-entry table at
 * 0x4171a4 into the caller's buffer, keeping the first three bytes of each. */
static uint32_t FS98_STDCALL host_28c(uint32_t dst)
{
    uint32_t i;
    for (i = 0; i < 256; i++) {
        M8(dst + 3 * i + 0) = M8(V(0x4171a4) + 4 * i + 0);
        M8(dst + 3 * i + 1) = M8(V(0x4171a4) + 4 * i + 1);
        M8(dst + 3 * i + 2) = M8(V(0x4171a4) + 4 * i + 2);
    }
    return dst;
}

/* +0x2bc (0x40ef60): strcmp(a, b) -- -1, 0 or 1. */
static uint32_t FS98_STDCALL host_2bc(uint32_t a, uint32_t b)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)a;
    const uint8_t *q = (const uint8_t *)(uintptr_t)b;
    for (;;) {
        if (*p != *q)
            return *p < *q ? (uint32_t)-1 : 1;
        if (*p == 0)
            return 0;
        p++; q++;
    }
}

/* +0x2cc (0x40f110): true when the host global 0x41a3cc is zero. */
static uint32_t FS98_STDCALL host_2cc(void)
{
    return M32(V(0x41a3cc)) == 0 ? 1 : 0;
}

/* +0x304 (0x410230): clears the dword the argument points at.  Returns it. */
static uint32_t FS98_STDCALL host_304(uint32_t p)
{
    M32(p) = 0;
    return p;
}

/* Stubs the host leaves returning zero, by argument count. */
static uint32_t FS98_STDCALL host_zero0(void) { return 0; }
static uint32_t FS98_STDCALL host_zero1(uint32_t a) { (void)a; return 0; }
static uint32_t FS98_STDCALL host_zero2(uint32_t a, uint32_t b) { (void)a; (void)b; return 0; }

/* +0x2b8 (0x40ef40): strlen(s), 0 for a null pointer. */
static uint32_t FS98_STDCALL host_2b8(uint32_t s)
{
    const char *p = (const char *)(uintptr_t)s;
    uint32_t n = 0;
    if (p == 0)
        return 0;
    while (p[n] != '\0')
        n++;
    return n;
}

/* ---- the table --------------------------------------------------------------- */

/* The service table, the shape of the host's: slot 0 is the module id, the rest
 * are service pointers; unfinished slots stay null. */
__declspec(dllexport) void *HostTable[0xc8] = {
    [0x00 / 4] = (void *)1,
    [0x18 / 4] = (void *)host_018,
    [0x1c / 4] = (void *)host_01c,
    [0x20 / 4] = (void *)host_020,
    [0x2c / 4] = (void *)host_02c,
    [0x34 / 4] = (void *)host_034,
    [0x54 / 4] = (void *)host_054,
    [0x58 / 4] = (void *)host_058,
    [0x5c / 4] = (void *)host_05c,
    [0x60 / 4] = (void *)host_060,
    [0x108 / 4] = (void *)host_zero2,
    [0x12c / 4] = (void *)host_zero1,
    [0x130 / 4] = (void *)host_130,
    [0x154 / 4] = (void *)host_zero0,
    [0x168 / 4] = (void *)host_168,
    [0x218 / 4] = (void *)host_218,
    [0x220 / 4] = (void *)host_zero2,
    [0x224 / 4] = (void *)host_zero2,
    [0x228 / 4] = (void *)host_228,
    [0x288 / 4] = (void *)host_288,
    [0x28c / 4] = (void *)host_28c,
    [0x2a8 / 4] = (void *)host_2a8,
    [0x2b0 / 4] = (void *)host_2b0,
    [0x2b8 / 4] = (void *)host_2b8,
    [0x2bc / 4] = (void *)host_2bc,
    [0x2cc / 4] = (void *)host_2cc,
    [0x304 / 4] = (void *)host_304,
};

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
