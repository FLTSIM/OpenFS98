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
#include <stdlib.h>
#include <string.h>

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

/* ---- the allocator ----------------------------------------------------------- */

/* 0x401a30: the purge hook the host offers before retrying a failed allocation
 * -- the function pointer at 0x418834 (one of the fields +0x18 copies in),
 * called (code, arg) if set. */
static uint32_t host_purge(uint32_t code, uint32_t arg)
{
    uint32_t fn = M32(V(0x418834));
    if (fn == 0)
        return 0;
    return ((uint32_t (FS98_STDCALL *)(uint32_t, uint32_t))(uintptr_t)fn)(code, arg);
}

/* Out of memory, after the retry: string 9 from the host instance (0x418828)
 * into the buffer at 0x41c480, in an MB_ICONHAND box. */
static void host_oom_box(void)
{
    LoadStringA((HINSTANCE)(uintptr_t)M32(V(0x418828)), 9,
                (char *)(uintptr_t)V(0x41c480), 0x104);
    MessageBoxA(0, (const char *)(uintptr_t)V(0x41c480), 0, MB_ICONHAND);
}

/* +0x20c (0x40cdc0): *out = malloc(size), zero-filled when `zero` is set.
 * Failure with flags 4 returns -1; otherwise it calls the purge hook and
 * retries once, and a second failure is fatal.  Returns 0. */
static uint32_t FS98_STDCALL host_20c(uint32_t flags, uint32_t size, uint32_t out, uint32_t zero)
{
    uint8_t *p = malloc(size);
    uint32_t i;
    M32(out) = ADDR(p);
    if (p == 0) {
        if (flags == 4)
            return (uint32_t)-1;
        host_purge(0x20012, 0);
        p = malloc(size);
        M32(out) = ADDR(p);
        if (p == 0) {
            host_oom_box();
            ExitProcess(0);
        }
    }
    if (zero != 0) {
        p = (uint8_t *)(uintptr_t)M32(out);
        for (i = 0; i < size; i++)
            p[i] = 0;
    }
    return 0;
}

/* +0x024's body (0x408550): set the quit flag and send WM_CLOSE to the main
 * window. */
static void host_quit(void)
{
    M32(V(0x418868)) = 1;
    SendMessageA((HWND)(uintptr_t)M32(V(0x41880c)), WM_CLOSE, 0, 0);
}

/* +0x210 (0x40ce60): *pp = realloc(*pp, size).  Failure with flags 4 puts the
 * old pointer back and returns -1; otherwise the purge hook and one retry, and
 * a second failure shows the out-of-memory box and asks the main window to
 * close -- but, unlike +0x20c, returns 0 with *pp null rather than exiting. */
static uint32_t FS98_STDCALL host_210(uint32_t flags, uint32_t size, uint32_t pp)
{
    void *old = (void *)(uintptr_t)M32(pp);
    void *p = realloc(old, size);
    M32(pp) = ADDR(p);
    if (p == 0) {
        if (flags == 4) {
            M32(pp) = ADDR(old);
            return (uint32_t)-1;
        }
        host_purge(0x20012, 0);
        p = realloc(old, size);
        M32(pp) = ADDR(p);
        if (p == 0) {
            host_oom_box();
            host_quit();
        }
    }
    return 0;
}

/* +0x214 (0x40cef0): free(*pp) and null it, if it is set.  Returns 0. */
static uint32_t FS98_STDCALL host_214(uint32_t pp)
{
    if (M32(pp) != 0) {
        free((void *)(uintptr_t)M32(pp));
        M32(pp) = 0;
    }
    return 0;
}

/* +0x21c (0x40cf60): MSVCRT memmove.  Returns 0. */
static uint32_t FS98_STDCALL host_21c(uint32_t dst, uint32_t src, uint32_t size)
{
    memmove((void *)(uintptr_t)dst, (const void *)(uintptr_t)src, size);
    return 0;
}

/* ---- the growable array ------------------------------------------------------ */

/* struct fs98_host_list (host.h), by offset: count, capacity, grow, element
 * size, for-each depth, removal-queued flag, data.  The host keeps one of its
 * own at 0x418380 (8-byte elements, grow 0x20): the queue of removals made
 * while a list was being walked, each { list, index }. */
#define L_COUNT(l)  M32((l) + 0x00)
#define L_CAP(l)    M32((l) + 0x04)
#define L_GROW(l)   M32((l) + 0x08)
#define L_SIZE(l)   M32((l) + 0x0c)
#define L_ITER(l)   M32((l) + 0x10)
#define L_PEND(l)   M32((l) + 0x14)
#define L_DATA(l)   M32((l) + 0x18)
#define HOST_QUEUE  V(0x418380)

typedef uint32_t (FS98_STDCALL *host_walk_fn)(uint32_t elem, uint32_t ctx);

/* 0x410410: make room for one more element and return its index.  A full list
 * grows by `grow`, allocated afresh when it is empty and reallocated when not
 * -- so a list freed by +0x2fc with its capacity left standing never
 * reallocates, and its next element lands on a null data pointer. */
static uint32_t host_list_grow(uint32_t l)
{
    uint32_t count = L_COUNT(l);
    if (count == L_CAP(l)) {
        uint32_t cap = L_CAP(l) + L_GROW(l);
        L_CAP(l) = cap;
        if (count == 0)
            host_20c(1, L_SIZE(l) * cap, l + 0x18, 0);
        else
            host_210(1, L_SIZE(l) * cap, l + 0x18);
    }
    count = L_COUNT(l) + 1;
    L_COUNT(l) = count;
    return count - 1;
}

/* +0x2f8 (0x4101d0): append a zeroed element and return its address. */
static uint32_t FS98_STDCALL host_2f8(uint32_t l)
{
    uint32_t i = host_list_grow(l), size = L_SIZE(l), j;
    uint8_t *p = (uint8_t *)(uintptr_t)(i * size + L_DATA(l));
    for (j = 0; j < size; j++)
        p[j] = 0;
    return i * L_SIZE(l) + L_DATA(l);
}

/* 0x410470: remove element `i`.  During a walk the removal is only queued on
 * 0x418380 (and the queue entry returned); otherwise the count drops -- even
 * for an index at or past the end, which then moves nothing -- and the tail
 * closes up.  Returns whatever eax held: the element size after a move, else
 * 0. */
static uint32_t host_list_remove_at(uint32_t l, int32_t i)
{
    int32_t count;
    if (L_ITER(l) != 0) {
        uint32_t e;
        L_PEND(l) = 1;
        e = host_2f8(HOST_QUEUE);
        M32(e) = l;
        M32(e + 4) = (uint32_t)i;
        return e;
    }
    count = (int32_t)L_COUNT(l) - 1;
    L_COUNT(l) = (uint32_t)count;
    if (i < count) {
        uint32_t size = L_SIZE(l), n = (uint32_t)(count - i) * size, k;
        uint8_t *d = (uint8_t *)(uintptr_t)((uint32_t)i * size + L_DATA(l));
        const uint8_t *s = (const uint8_t *)(uintptr_t)((uint32_t)(i + 1) * size + L_DATA(l));
        for (k = 0; k < n; k++)     /* rep movs, upward */
            d[k] = s[k];
        return size;                /* eax: what the move left there */
    }
    return 0;                       /* eax: the depth it tested */
}

/* +0x300 (0x410210): remove the element `elem` points into. */
static uint32_t FS98_STDCALL host_300(uint32_t l, uint32_t elem)
{
    return host_list_remove_at(l, (int32_t)((elem - L_DATA(l)) / L_SIZE(l)));
}

/* The tail the two walks share: when the walk is over, apply the removals
 * queued against this list, newest first.  The depth is not consulted, so a
 * nested walk's end applies them while the outer walk is still running --
 * where each removal is queued again, and the list's flag cleared over it. */
static void host_list_flush(uint32_t l)
{
    uint32_t n, q;
    if (L_PEND(l) == 0)
        return;
    n = L_COUNT(HOST_QUEUE);
    if (n != 0) {
        q = L_DATA(HOST_QUEUE);
        do {
            if (M32(q + n * 8 - 8) == l) {
                host_list_remove_at(l, (int32_t)M32(q + n * 8 - 4));
                host_list_remove_at(HOST_QUEUE, (int32_t)(n - 1));
                q = L_DATA(HOST_QUEUE);
            }
        } while (--n != 0);
    }
    L_PEND(l) = 0;
}

/* +0x308 (0x4102a0): fn(element, ctx) for each element in order until one
 * returns non-zero; returns that, or 0.  The count is re-read every step, so
 * elements appended by fn are walked too. */
static uint32_t FS98_STDCALL host_308(uint32_t l, uint32_t ctx, uint32_t fn)
{
    uint32_t r = 0, i;
    L_ITER(l) = L_ITER(l) + 1;
    for (i = 0; i < L_COUNT(l); i++) {
        r = ((host_walk_fn)(uintptr_t)fn)(i * L_SIZE(l) + L_DATA(l), ctx);
        if (r != 0)
            break;
    }
    L_ITER(l) = L_ITER(l) - 1;
    host_list_flush(l);
    return r;
}

/* +0x30c (0x410340): the same walk from the last element back. */
static uint32_t FS98_STDCALL host_30c(uint32_t l, uint32_t ctx, uint32_t fn)
{
    uint32_t r = 0, i;
    L_ITER(l) = L_ITER(l) + 1;
    for (i = 0; i < L_COUNT(l); i++) {
        r = ((host_walk_fn)(uintptr_t)fn)((L_COUNT(l) - i - 1) * L_SIZE(l) + L_DATA(l), ctx);
        if (r != 0)
            break;
    }
    L_ITER(l) = L_ITER(l) - 1;
    host_list_flush(l);
    return r;
}

/* 0x4104e0: the index of the first element whose `len` bytes at `off` match
 * `key` -- or, with `indirect`, whose pointer at `off` points at bytes
 * matching those `key` points at -- or -1.  A zero `len` matches at once. */
static int32_t host_list_index(uint32_t l, uint32_t off, uint32_t key, uint32_t len,
                               uint32_t indirect)
{
    uint32_t p = L_DATA(l) + off;
    int32_t i;
    for (i = 0; i < (int32_t)L_COUNT(l); i++) {
        const uint8_t *a = (const uint8_t *)(uintptr_t)(indirect ? M32(p) : p);
        const uint8_t *b = (const uint8_t *)(uintptr_t)(indirect ? M32(key) : key);
        uint32_t k = 0;
        while (k < len && a[k] == b[k])
            k++;
        if (k == len)
            return i;
        p += L_SIZE(l);
    }
    return -1;
}

/* +0x310 (0x410240): that element's address, or null. */
static uint32_t FS98_STDCALL host_310(uint32_t l, uint32_t off, uint32_t key, uint32_t len,
                                      uint32_t indirect)
{
    int32_t i = host_list_index(l, off, key, len, indirect);
    if (i == -1)
        return 0;
    return L_SIZE(l) * (uint32_t)i + L_DATA(l);
}

/* +0x2fc (0x4103e0): free the list's data through +0x214.  The rest of the
 * header -- capacity included -- is left as it was.  Returns +0x214's 0, or,
 * when there was nothing to free, the address of the data field. */
static uint32_t FS98_STDCALL host_2fc(uint32_t l)
{
    if (L_DATA(l) == 0)
        return l + 0x18;
    return host_214(l + 0x18);
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
    [0x20c / 4] = (void *)host_20c,
    [0x210 / 4] = (void *)host_210,
    [0x214 / 4] = (void *)host_214,
    [0x218 / 4] = (void *)host_218,
    [0x21c / 4] = (void *)host_21c,
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
    [0x2f8 / 4] = (void *)host_2f8,
    [0x2fc / 4] = (void *)host_2fc,
    [0x300 / 4] = (void *)host_300,
    [0x304 / 4] = (void *)host_304,
    [0x308 / 4] = (void *)host_308,
    [0x30c / 4] = (void *)host_30c,
    [0x310 / 4] = (void *)host_310,
};

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
