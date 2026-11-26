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
#define __USE_MINGW_ANSI_STDIO 0      /* MSVCRT's own sprintf, as retail's */
#include <windows.h>
#include "fs98/image_end.h"
#include <mmreg.h>
#include <msacm.h>
#include <ddraw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs98/host.h"
#include "runtime/host_rt.h"     /* the routines shared with the runtime (host_rt_ddraw.c and others) */

#define HOST_DATA_VA    0x417000u
#define HOST_IMAGE_SIZE 0x55ccu

struct host_image {
    uint8_t bytes[HOST_IMAGE_SIZE];
};

/* host_image.inc defines AT(off) for its designated initialisers; host_rt.h's AT(object, offset) comes back after it */
#pragma push_macro("AT")
#undef AT
struct host_image host_image = {{
#include "host_image.inc"
}};
#pragma pop_macro("AT")

#ifndef HOST_IMAGE_SHARED
/* .rdata, for the shared routines when built without the runtime (HOST_RT_STANDALONE) */
const uint8_t fs98_host_rt_rdata[] = {
#include "runtime/host_rt_rdata.inc"
};
#endif

#ifdef HOST_IMAGE_SHARED
/* the decompiled runtime's executable (src/host/host_rt_exe.c): .data in the image it shares
 * with the runtime, host_image copied there (and its pointers rebased) before anything runs */
extern uint8_t *fs98_host_data;
#define FS98_DATA_IMAGE (*fs98_host_data)
#else
#define FS98_DATA_IMAGE host_image.bytes[0]
#endif
#include "fs98/arena.h"
#define V(a)    (D + ((uint32_t)(a) - HOST_DATA_VA))

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
    M32(out) = fs98_handle(p);
    if (p == 0) {
        if (flags == 4)
            return (uint32_t)-1;
        (uint32_t)fs98_host_rt_hook(0x20012, 0);
        p = malloc(size);
        M32(out) = fs98_handle(p);
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
 * close -- but, unlike +0x20c, returns 0 with *pp null rather than exiting.
 * RETAIL BUG (kept): HOST-7 (the non-fatal second failure, below). */
static uint32_t FS98_STDCALL host_210(uint32_t flags, uint32_t size, uint32_t pp)
{
    void *old = (void *)(uintptr_t)M32(pp);
    void *p = realloc(old, size);
    M32(pp) = fs98_handle(p);
    if (p == 0) {
        if (flags == 4) {
            M32(pp) = fs98_handle(old);
            return (uint32_t)-1;
        }
        (uint32_t)fs98_host_rt_hook(0x20012, 0);
        p = realloc(old, size);
        M32(pp) = fs98_handle(p);
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
    L_COUNT(l) = (uint32_t)count;   /* RETAIL BUG (kept): HOST-4 -- dropped even for an index at or past the end, and when a queued removal is applied */
    if (i < count) {
        uint32_t size = L_SIZE(l), n = (uint32_t)(count - i) * size, k;
        uint8_t *d = (uint8_t *)(uintptr_t)((uint32_t)i * size + L_DATA(l));
        const uint8_t *s = (const uint8_t *)(uintptr_t)((uint32_t)(i + 1) * size + L_DATA(l));
        for (k = 0; k < n; k++)     /* rep movs, upward */
            d[k] = s[k];
        return size;                /* eax: what the move left there */
    }
    /* RETAIL BUG (kept): HOST-6 -- +0x300 returns what eax held (the size after a move, else the depth tested; during a walk the new queue entry's address) */
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
    /* RETAIL BUG (kept): HOST-5 -- the depth L_ITER is not consulted here */
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
 * matching those `key` points at -- or -1.  A zero `len` matches at once
 * (RETAIL BUG (kept): HOST-6, +0x310 with a zero length: the first element). */
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
    /* RETAIL BUG (kept): HOST-6 -- the capacity is left, so a freed list not re-initialised
     * writes through null on its next append; the return is what eax held */
    if (L_DATA(l) == 0)
        return l + 0x18;
    return host_214(l + 0x18);
}

/* ---- the C-runtime wrappers -------------------------------------------------- */

/* qsort and bsearch take a cdecl comparator; the host's callers pass stdcall
 * ones, so the host parks the comparator at 0x41a24c and hands MSVCRT this
 * trampoline (0x40cfe0) instead.  Not reentrant, as retail is not.
 * RETAIL BUG (kept): HOST-3 -- the parked comparator is global (+0x22c and +0x234 both),
 * and the trampoline does not clean up after a stdcall comparator, which must be one. */
static int __cdecl host_cmp_tramp(const void *a, const void *b)
{
    return (int)((uint32_t (FS98_STDCALL *)(uint32_t, uint32_t))(uintptr_t)M32(V(0x41a24c)))(
        fs98_handle(a), fs98_handle(b));
}

/* +0x22c (0x40cfb0): qsort(base, n, width, cmp).  Returns whatever MSVCRT's
 * qsort left in eax; no caller reads it. */
static uint32_t FS98_STDCALL host_22c(uint32_t base, uint32_t n, uint32_t width, uint32_t cmp)
{
    M32(V(0x41a24c)) = cmp;
    qsort((void *)(uintptr_t)base, n, width, host_cmp_tramp);
    return 0;
}

/* +0x234 (0x40d050): bsearch(key, base, n, width, cmp). */
static uint32_t FS98_STDCALL host_234(uint32_t key, uint32_t base, uint32_t n, uint32_t width,
                                      uint32_t cmp)
{
    M32(V(0x41a24c)) = cmp;
    return fs98_handle(bsearch((const void *)(uintptr_t)key, (const void *)(uintptr_t)base, n, width,
                        host_cmp_tramp));
}

/* +0x230 (0x40d000): exchange two blocks of `size` bytes through a temporary
 * from +0x20c (kept, as retail keeps it, in the size argument's own slot).
 * Returns +0x214's 0. */
static uint32_t FS98_STDCALL host_230(uint32_t a, uint32_t b, uint32_t size)
{
    uint32_t tmp;
    host_20c(1, size, fs98_handle(&tmp), 0);
    host_21c(tmp, a, size);
    host_21c(a, b, size);
    host_21c(b, tmp, size);
    return host_214(fs98_handle(&tmp));
}

/* +0x2ac (0x40eea0): strncpy(dst, src, n), returning dst. */
static uint32_t FS98_STDCALL host_2ac(uint32_t dst, uint32_t src, uint32_t n)
{
    return fs98_handle(strncpy((char *)(uintptr_t)dst, (const char *)(uintptr_t)src, n));
}

/* +0x2b4 (0x40ef00): a copy of `s` from +0x20c (fatal on failure), or null for
 * a null `s`. */
static uint32_t FS98_STDCALL host_2b4(uint32_t s)
{
    uint32_t p;
    if (s == 0)
        return 0;
    host_20c(1, host_2b8(s) + 1, fs98_handle(&p), 0);
    lstrcpyA((char *)(uintptr_t)p, (const char *)(uintptr_t)s);
    return p;
}

/* +0x2c0 (0x40efa0): MSVCRT _strcmpi, its result as is. */
static uint32_t FS98_STDCALL host_2c0(uint32_t a, uint32_t b)
{
    return (uint32_t)_strcmpi((const char *)(uintptr_t)a, (const char *)(uintptr_t)b);
}

/* +0x2c8 (0x40efc0): delete every occurrence of the character `c` from `s`, in
 * place, by +0x2a8 over the overlap.  Returns s. */
static uint32_t FS98_STDCALL host_2c8(uint32_t s, uint32_t c)
{
    uint32_t i = 0, p;
    if (M8(s) == 0)
        return s;
    p = s;
    do {
        if (M8(p) == (uint8_t)c) {
            host_2a8(p, i + s + 1);
        } else {
            i++;
            p++;
        }
    } while (M8(p) != 0);
    return s;
}

/* ---- the command line -------------------------------------------------------- */

/* 0x4095e0: find option `name` on command line `p` -- a '/' or '-' just after a
 * space, then the name, then ':' or ' ' -- and return the address just past
 * that separator, or 0.  The first character is tested against the byte
 * before it, which for GetCommandLineA's own buffer is whatever precedes it. */
static uint32_t host_opt_find(uint32_t p, uint32_t name)
{
    if (M8(p) == 0)
        return 0;
    do {
        uint8_t c = M8(p);
        /* RETAIL BUG (kept): HOST-8 -- M8(p - 1) for a first character of '/' or '-' is the byte
         * before the command line's buffer; serves +0x98 (so +0x9c, +0xa0) and +0xa4 */
        if ((c == '/' || c == '-') && M8(p - 1) == ' ' &&
            host_228(p + 1, name, host_2b8(name)) == 0 &&
            (M8(host_2b8(name) + p + 1) == ':' || M8(host_2b8(name) + p + 1) == ' '))
            return host_2b8(name) + p + 2;
        p = fs98_handle(CharNextA((const char *)(uintptr_t)p));
    } while (M8(p) != 0);
    return 0;
}

/* 0x409670: copy an option's value -- up to the next option -- into buf,
 * trimmed of spaces at both ends.  -1 if it does not fit in `len`.  An empty
 * value trims from the byte before buf, and clears it if it is a space. */
static uint32_t host_opt_copy(uint32_t v, uint32_t buf, uint32_t len)
{
    uint32_t e = v, n, q, t;
    uint8_t c = M8(e);
    while (c != 0) {
        if ((c == '/' || c == '-') && M8(e - 1) == ' ')
            break;
        e = fs98_handle(CharNextA((const char *)(uintptr_t)e));
        c = M8(e);
    }
    n = e - v;
    if (n >= len)
        return (uint32_t)-1;
    host_218(buf, 0, len);
    host_21c(buf, v, n);
    q = buf;
    while (M8(q) != 0 && M8(q) == ' ')
        q = fs98_handle(CharNextA((const char *)(uintptr_t)q));
    host_2a8(buf, q);
    t = host_2b8(buf) + buf - 1;
    if (M8(t) == ' ') {
        while (t != buf) {
            M8(t) = 0;
            t = fs98_handle(CharPrevA((const char *)(uintptr_t)buf, (const char *)(uintptr_t)t));
            if (M8(t) != ' ')
                break;
        }
    }
    return 0;
}

/* +0x98 (0x4094f0): the value of "/name:value" on the command line into buf.
 * Only the colon form counts; 0, or -1 if absent, colonless or too long. */
static uint32_t FS98_STDCALL host_098(uint32_t name, uint32_t buf, uint32_t len)
{
    uint32_t cl = fs98_handle(GetCommandLineA()), v;
    if (M8(cl) == 0)
        return (uint32_t)-1;
    v = host_opt_find(cl, name);
    if (v == 0 || M8(v - 1) != ':')
        return (uint32_t)-1;
    return host_opt_copy(v, buf, len);
}

/* +0x9c (0x409530) and +0xa0 (0x409570): the same value through atoi or atof
 * into *out, left alone on failure. */
static uint32_t FS98_STDCALL host_09c(uint32_t name, uint32_t out)
{
    char buf[0x40];
    uint32_t r = host_098(name, fs98_handle(buf), 0x40);
    if (r == 0)
        M32(out) = (uint32_t)atoi(buf);
    return r;
}

static uint32_t FS98_STDCALL host_0a0(uint32_t name, uint32_t out)
{
    char buf[0x40];
    uint32_t r = host_098(name, fs98_handle(buf), 0x40);
    if (r == 0)
        *(double *)(uintptr_t)out = atof(buf);
    return r;
}

/* +0xa4 (0x4095b0): is option `name` given?  0 if not, 2 for "/name:...", 1
 * for "/name ..." (more exactly: 1 when a space precedes what follows). */
static uint32_t FS98_STDCALL host_0a4(uint32_t name)
{
    uint32_t v = host_opt_find(fs98_handle(GetCommandLineA()), name);
    if (v == 0)
        return 0;
    return (M8(v - 1) != ' ') + 1;
}

/* ---- file extensions --------------------------------------------------------- */

/* A pointer the retail image keeps to its own .data, onto our image. */
static uint32_t host_rebase(uint32_t p)
{
    return (p >= HOST_DATA_VA && p < HOST_DATA_VA + HOST_IMAGE_SIZE) ? V(p) : p;
}

/* Two NUL-terminated strings equal (the inlined compare in 0x40b410)? */
static int host_str_eq(uint32_t a, uint32_t b)
{
    while (M8(a) == M8(b)) {
        if (M8(a) == 0)
            return 1;
        a++; b++;
    }
    return 0;
}

/* 0x40b410: upper-case `ext` in place and look it up: the 37 built-in
 * extensions at 0x417f20 are ids 0..0x24, the ones registered at run time
 * (0x4188ac of them, _strdup'd, at *0x4188b0) follow from 0x25.  0 with the id
 * in *out, or -1. */
static uint32_t host_ext_lookup(uint32_t ext, uint32_t out)
{
    int32_t i, n;
    _strupr((char *)(uintptr_t)ext);
    for (i = 0; i < 0x25; i++)
        if (host_str_eq(ext, host_rebase(M32(V(0x417f20) + 4 * i)))) {
            M32(out) = (uint32_t)i;
            return 0;
        }
    n = (int32_t)M32(V(0x4188ac));
    for (i = 0; i < n; i++)
        if (host_str_eq(ext, M32(M32(V(0x4188b0)) + 4 * i))) {
            M32(out) = (uint32_t)i + 0x25;
            return 0;
        }
    return (uint32_t)-1;
}

/* +0x1ac (0x40a480): the id of extension `ext` (upper-cased in place),
 * registering it if new.  Returns 0. */
static uint32_t FS98_STDCALL host_1ac(uint32_t ext, uint32_t out)
{
    uint32_t n;
    _strupr((char *)(uintptr_t)ext);
    if (host_ext_lookup(ext, out) != (uint32_t)-1)
        return 0;
    n = M32(V(0x4188ac));
    if (n == 0)
        host_20c(1, 4, V(0x4188b0), 0);
    else
        host_210(1, n * 4 + 4, V(0x4188b0));
    M32(out) = M32(V(0x4188ac)) + 0x25;
    M32(M32(V(0x4188b0)) + M32(V(0x4188ac)) * 4) = fs98_handle(_strdup((const char *)(uintptr_t)ext));
    M32(V(0x4188ac)) = M32(V(0x4188ac)) + 1;
    return 0;
}

/* 0x40a7f0: _splitpath, except that the host's empty string (0x4187f8) splits
 * into empty parts by copying it into each part asked for. */
static uint32_t host_split(uint32_t path, uint32_t drive, uint32_t dir, uint32_t fname,
                           uint32_t ext)
{
    if (path == 0)
        return (uint32_t)-1;
    if (host_2bc(path, V(0x4187f8)) != 0) {
        _splitpath((const char *)(uintptr_t)path, (char *)(uintptr_t)drive,
                   (char *)(uintptr_t)dir, (char *)(uintptr_t)fname, (char *)(uintptr_t)ext);
        return 0;
    }
    if (drive != 0) host_2a8(drive, V(0x4187f8));
    if (dir != 0) host_2a8(dir, V(0x4187f8));
    if (fname != 0) host_2a8(fname, V(0x4187f8));
    if (ext == 0)
        return 0;
    host_2a8(ext, V(0x4187f8));
    return 0;
}

/* +0x188 (0x40a6c0): split `path` into drive, directory and name, and give
 * the extension's id (registering a new one) through `ext_id` -- left 0 when
 * there is no extension.  A UNC path's "\\server" counts as its drive.
 * Returns 0, or -1 for a null path. */
static uint32_t FS98_STDCALL host_188(uint32_t path, uint32_t drive, uint32_t dir, uint32_t fname,
                                      uint32_t ext_id)
{
    uint8_t unc[0x100], ext[0x100];
    uint32_t p, i;
    unc[0] = M8(V(0x4187f8));
    for (i = 1; i < sizeof unc; i++)
        unc[i] = 0;
    if (ext_id != 0)
        M32(ext_id) = 0;
    if (path == 0)
        return (uint32_t)-1;
    if (drive != 0 && host_168(path) != 0) {
        host_2a8(fs98_handle(unc), V(0x4180c8));
        host_2a8(fs98_handle(unc) + 2, path + 2);
        p = fs98_handle(unc) + 2;
        do
            p = fs98_handle(CharNextA((const char *)(uintptr_t)p));
        while (M8(p) != '\\' && M8(p) != 0);
        host_split(p, 0, dir, fname, fs98_handle(ext));
        M8(p) = 0;
        host_2a8(drive, fs98_handle(unc));
    } else {
        host_split(path, drive, dir, fname, fs98_handle(ext));
    }
    if (ext[0] != 0 && ext_id != 0 && host_ext_lookup(fs98_handle(ext), ext_id) != 0)
        host_1ac(fs98_handle(ext), ext_id);
    return 0;
}

/* ---- string resources -------------------------------------------------------- */

/* The modules strings are looked up in: a list at 0x4182e0 of HMODULEs (the
 * language DLL and the like), searched newest first, then the host itself
 * (its instance at 0x41882c).  The module records the host keeps for its
 * plug-in modules are another list, at 0x417b30, of 0x14-byte records: the
 * HMODULE at +8, the module id at +0xc, non-zero at +0x10 once loaded. */
#define HOST_STRMODS    V(0x4182e0)
#define HOST_MODULES    V(0x417b30)

/* 0x40ee50: the walk's callback -- LoadStringA from one module; the first
 * non-zero count stops the walk. */
static uint32_t FS98_STDCALL host_str_cb(uint32_t elem, uint32_t req)
{
    return (uint32_t)LoadStringA((HINSTANCE)(uintptr_t)M32(elem), M32(req),
                                 (char *)(uintptr_t)M32(req + 4), (int)M32(req + 8));
}

/* +0x2a0 (0x40ed70): LoadStringA(id) into buf from the first module on the
 * list that has it, else from the host.  Returns the count. */
static uint32_t FS98_STDCALL host_2a0(uint32_t id, uint32_t buf, uint32_t len)
{
    uint32_t req[3], n;
    req[0] = id;
    req[1] = buf;
    req[2] = len;
    n = host_30c(HOST_STRMODS, fs98_handle(req), fs98_handle(host_str_cb));
    if (n == 0)
        n = (uint32_t)LoadStringA((HINSTANCE)(uintptr_t)M32(V(0x41882c)), id,
                                  (char *)(uintptr_t)buf, (int)len);
    return n;
}

/* +0x2a4 (0x40edc0): string `id` into a new block from +0x20c (fatal flag),
 * *out null if there is none.  Returns the count. */
static uint32_t FS98_STDCALL host_2a4(uint32_t id, uint32_t out)
{
    char buf[0x400];
    uint32_t n;
    M32(out) = 0;
    n = host_2a0(id, fs98_handle(buf), 0x400);
    if (n != 0) {
        host_20c(1, n + 1, out, 0);
        host_2a8(M32(out), fs98_handle(buf));
    }
    return n;
}

/* +0x298 (0x40ece0): put the HMODULE of the module whose Linkage is `id`
 * (records are found by Linkage, +0xc) on the string list, and in *out.  -1
 * with *out 0 if there is no such record or it is not loaded. */
static uint32_t FS98_STDCALL host_298(uint32_t id, uint32_t out)
{
    uint32_t rec = host_310(HOST_MODULES, 0xc, fs98_handle(&id), 4, 0), e;
    if (rec == 0) {
        M32(out) = 0;
        return (uint32_t)-1;
    }
    if (M32(rec + 0x10) == 0) {
        M32(out) = 0;
        return (uint32_t)-1;
    }
    e = host_2f8(HOST_STRMODS);
    M32(e) = M32(rec + 8);
    M32(out) = M32(rec + 8);
    return 0;
}

/* +0x29c (0x40ed40): take `hmod` off the string list.  0, or -1 if absent. */
static uint32_t FS98_STDCALL host_29c(uint32_t hmod)
{
    uint32_t e = host_310(HOST_STRMODS, 0, fs98_handle(&hmod), 4, 0);
    if (e == 0)
        return (uint32_t)-1;
    host_300(HOST_STRMODS, e);
    return 0;
}

/* The input names' prefixes, strings 0x8012 ("Button ") and 0x8013 ("Axis ")
 * into 0x418108 and 0x418140, with their lengths at 0x41a244 and 0x41a248;
 * loaded on first use (flag 0x41a240).  Returns what eax holds after: the
 * second length when it loads, else the flag. */
static uint32_t host_input_names(void)
{
    uint32_t eax = M32(V(0x41a240));
    if (eax == 0) {
        M32(V(0x41a240)) = 1;
        M32(V(0x41a244)) = host_2a0(0x8012, V(0x418108), 0x32);
        eax = host_2a0(0x8013, V(0x418140), 0x32);
        M32(V(0x41a248)) = eax;
    }
    return eax;
}

/* +0x1d0 (0x40bfc0): the name of input `code` -- 0x11..0x50 are buttons 1..64
 * and 1..0x10 axes 1..16 -- as prefix plus two digits ("%s%2.2d") into buf,
 * else "".  Returns wsprintfA's count, or what the loading left in eax. */
static uint32_t FS98_STDCALL host_1d0(uint32_t code, uint32_t buf, uint32_t unused)
{
    int32_t c = (int32_t)code;
    uint32_t eax = host_input_names();
    (void)unused;
    M8(buf) = 0;
    /* RETAIL BUG (kept): HOST-9 -- the return is whatever eax holds (wsprintfA's count, or the loading's) */
    if (c >= 0x11 && c <= 0x50)
        eax = (uint32_t)wsprintfA((char *)(uintptr_t)buf, (const char *)(uintptr_t)V(0x41826c),
                                  (const char *)(uintptr_t)V(0x418108), c - 0x10);
    if (c >= 1 && c <= 0x10)
        eax = (uint32_t)wsprintfA((char *)(uintptr_t)buf, (const char *)(uintptr_t)V(0x41826c),
                                  (const char *)(uintptr_t)V(0x418140), c);
    return eax;
}

/* +0x1d4 (0x40c060): the reverse -- the input code a name denotes, into *out
 * (0 for none).  A button number past 64 or an axis past 16 gives 0.  The
 * axis test starts past the button prefix when that matched, so "Button Axis
 * 3" is axis 3.  Returns the last compare's result, or the code it made. */
static uint32_t FS98_STDCALL host_1d4(uint32_t s, uint32_t out)
{
    uint32_t eax;
    host_input_names();
    M32(out) = 0;
    /* RETAIL BUG (kept): HOST-9 -- the return is the last compare or the code made; a button past 64
     * or an axis past 16 stores 0 in *out (the return is the code made, not 0); the axis prefix is compared from just past a matched button prefix */
    eax = host_228(s, V(0x418108), M32(V(0x41a244)));
    if (eax == 0) {
        M32(out) = 0x11;
        s += M32(V(0x41a244));
        eax = M32(out) + (uint32_t)atoi((const char *)(uintptr_t)s) - 1;
        M32(out) = eax;
        if ((int32_t)eax > 0x50)
            M32(out) = 0;
    }
    eax = host_228(s, V(0x418140), M32(V(0x41a248)));
    if (eax == 0) {
        M32(out) = 1;
        s += M32(V(0x41a248));
        eax = M32(out) + (uint32_t)atoi((const char *)(uintptr_t)s) - 1;
        M32(out) = eax;
        if ((int32_t)eax > 0x10)
            M32(out) = 0;
    }
    return eax;
}

/* ---- names and paths --------------------------------------------------------- */

/* The characters 0x22..0x7c that the two name checks refuse (their jump
 * tables at 0x40aeac and 0x40af94, read out); everything outside that range
 * passes, high bytes included. */
static int host_name_bad(uint8_t c, int path)
{
    switch (c) {
    case '"': case '&': case '*': case '/': case '<': case '>': case '?': case '|':
        return 1;
    case ':': case '\\':
        return !path;
    default:
        return 0;
    }
}

/* 0x40ae20 / 0x40af10: walk `s` by CharNextA; -1 for null or a refused
 * character, 0x1003 for an empty name, else 0. */
static uint32_t host_name_check(uint32_t s, int path)
{
    uint32_t n = 0;
    if (s == 0)
        return (uint32_t)-1;
    while (M8(s) != 0) {
        if (host_name_bad(M8(s), path))
            return (uint32_t)-1;
        s = fs98_handle(CharNextA((const char *)(uintptr_t)s));
        n++;
    }
    return n != 0 ? 0 : 0x1003;
}

/* +0x178 (0x40ae20): a valid file name (no '\\' or ':' either). */
static uint32_t FS98_STDCALL host_178(uint32_t s)
{
    return host_name_check(s, 0);
}

/* +0x18c (0x40af10): a valid path. */
static uint32_t FS98_STDCALL host_18c(uint32_t s)
{
    return host_name_check(s, 1);
}

/* 0x40a510's trailing-separator cut: if buf ends in "\\" (by _strcmpi, at
 * strlen(buf) - strlen(sep)), cut it off. */
static void host_cut_sep(uint32_t buf, uint32_t seplen)
{
    if (host_2c0(host_2b8(buf) + buf - seplen, V(0x4180c4)) == 0)
        M8(host_2b8(buf) + buf - seplen) = 0;
}

/* +0x184 (0x40a510): join drive, directory and name into `out` (0x104
 * bytes).  A one-character drive gets ':'; a part not starting with '\\' gets
 * one before it unless it comes first; a trailing '\\' is cut after the drive
 * and the directory.  -1 with out[0] cleared if the result would not fit; -1
 * for a null `out`; else 0.  The work buffer is 0x208 bytes, unchecked. */
static uint32_t FS98_STDCALL host_184(uint32_t out, uint32_t drive, uint32_t dir, uint32_t name)
{
    uint8_t buf[0x208];     /* RETAIL BUG (kept): HOST-2 -- only the result is checked against 0x104, not the parts */
    uint32_t seplen = host_2b8(V(0x4180c4)), i, b = fs98_handle(buf);
    buf[0] = M8(V(0x4187f8));
    for (i = 1; i < sizeof buf; i++)
        buf[i] = 0;
    if (out == 0)
        return (uint32_t)-1;
    if (drive != 0 && host_2b8(drive) != 0) {
        host_2b0(b, drive);
        if (host_2b8(b) == 1)
            host_2b0(b, V(0x4180c0));
        host_cut_sep(b, seplen);
    }
    if (dir != 0 && host_2b8(dir) != 0) {
        if (M8(dir) != '\\' && host_2b8(b) != 0)
            host_2b0(b, V(0x4180c4));
        host_2b0(b, dir);
        host_cut_sep(b, seplen);
    }
    if (name != 0 && host_2b8(name) != 0) {
        if (M8(name) != '\\' && host_2b8(b) != 0)
            host_2b0(b, V(0x4180c4));
        host_2b0(b, name);
    }
    if (host_2b8(b) >= 0x104) {
        M8(out) = 0;
        return (uint32_t)-1;
    }
    host_2a8(out, b);
    return 0;
}

/* ---- image buffers ----------------------------------------------------------- */

/* A 0x20-byte header from +0x20c: format (0..5, bytes per pixel from the
 * table at 0x4180e8: 1 1 2 2 3 4), width, height, stride (width times that,
 * rounded up to 4), the row table, size (height times stride), the pixels.
 * Pixels and row table are zeroed. */
#define IMG_BPP(f)  M32(V(0x4180e8) + 4 * (f))

/* 0x40b6d0's and 0x40b590's shared tail: fill the row table. */
static void host_img_rows(uint32_t p)
{
    uint32_t d = M32(p + 0x18);
    int32_t i;
    for (i = 0; i < (int32_t)M32(p + 8); i++) {
        M32(M32(p + 0x10) + 4 * (uint32_t)i) = d;
        d += M32(p + 0xc);
    }
}

/* +0x1b4 (0x40b590): a new image of `fmt`, w by h, into *out.  0, or -1 for
 * a format outside 0..5 (or, never in practice, a failed allocation, which
 * leaks what was made). */
static uint32_t FS98_STDCALL host_1b4(uint32_t out, uint32_t w, uint32_t h, uint32_t fmt)
{
    uint32_t p;
    if ((int32_t)fmt < 0 || (int32_t)fmt >= 6)
        return (uint32_t)-1;
    host_20c(1, 0x20, fs98_handle(&p), 1);
    M32(p) = fmt;
    M32(p + 4) = w;
    M32(p + 8) = h;
    M32(p + 0xc) = IMG_BPP(fmt) * w;
    M32(p + 0xc) = (M32(p + 0xc) + 3) & ~3u;
    M32(p + 0x14) = M32(p + 8) * M32(p + 0xc);
    host_20c(1, M32(p + 0x14), p + 0x18, 1);
    if (M32(p + 0x18) == 0)
        return (uint32_t)-1;
    host_20c(1, M32(p + 8) * 4, p + 0x10, 1);
    if (M32(p + 0x10) == 0)
        return (uint32_t)-1;
    host_img_rows(p);
    M32(out) = p;
    return 0;
}

/* +0x1b8 (0x40b680): free an image.  0, or -1 for null. */
static uint32_t FS98_STDCALL host_1b8(uint32_t p)
{
    if (p == 0)
        return (uint32_t)-1;
    if (M32(p + 0x18) != 0)
        host_214(p + 0x18);
    if (M32(p + 0x10) != 0)
        host_214(p + 0x10);
    host_214(fs98_handle(&p));
    return 0;
}

/* +0x1bc (0x40b6d0): resize an image to w by h, same format, contents
 * discarded (zeroed).  0, or -1 on a failed allocation. */
static uint32_t FS98_STDCALL host_1bc(uint32_t p, uint32_t w, uint32_t h)
{
    uint32_t stride;
    if (M32(p + 0x18) != 0)
        host_214(p + 0x18);
    if (M32(p + 0x10) != 0)
        host_214(p + 0x10);
    M32(p + 4) = w;
    M32(p + 8) = h;
    stride = (IMG_BPP(M32(p)) * w + 3) & ~3u;
    M32(p + 0xc) = stride;
    M32(p + 0x14) = h * stride;
    host_20c(1, h * stride, p + 0x18, 1);
    if (M32(p + 0x18) == 0)
        return (uint32_t)-1;
    host_20c(1, M32(p + 8) * 4, p + 0x10, 1);
    if (M32(p + 0x10) == 0)
        return (uint32_t)-1;
    host_img_rows(p);
    return 0;
}

/* ---- accelerator tables ------------------------------------------------------ */

/* The host's keyboard accelerators: a list at 0x418278 of 0x10-byte tables
 * { enabled, id, entries, count }, ids handed out from the counter at
 * 0x418294 (never 0), each entry 12 bytes { command, flags, key + 0x10000 }
 * where flags are 1 shift, 2 ctrl, 4 alt, 8 virtual-key -- Windows' ACCEL
 * rearranged by 0x40cd50. */
#define HOST_ACCELS     V(0x418278)

/* 0x40cd10: a new empty table with the next id. */
static uint32_t host_accel_new(void)
{
    uint32_t t = host_2f8(HOST_ACCELS);
    M32(t) = 0;
    M32(t + 4) = M32(V(0x418294));
    M32(t + 8) = 0;
    M32(t + 0xc) = 0;
    M32(V(0x418294)) = M32(V(0x418294)) + 1;
    if (M32(V(0x418294)) == 0)
        M32(V(0x418294)) = 1;
    return t;
}

/* The table with id *key, or 0. */
static uint32_t host_accel_find(uint32_t key)
{
    return host_310(HOST_ACCELS, 4, key, 4, 0);
}

/* 0x40cd50: an ACCEL { fVirt byte, pad, key word, cmd word } to an entry. */
static void host_accel_entry(uint32_t a, uint32_t e)
{
    M32(e) = M16(a + 2);
    M32(e + 4) = 0;
    M32(e + 8) = M16(a + 4) + 0x10000u;
    if (M8(a) & 0x4)
        M32(e + 4) = 1;
    if (M8(a) & 0x8)
        M32(e + 4) |= 2;
    if (M8(a) & 0x10)
        M32(e + 4) |= 4;
    if (M8(a) & 0x1)
        M32(e + 4) |= 8;
}

/* +0x1d8 (0x40c140): a new empty table; its id into *out.  Returns 0. */
static uint32_t FS98_STDCALL host_1d8(uint32_t out)
{
    M32(out) = M32(host_accel_new() + 4);
    return 0;
}

/* +0x1dc (0x40c160): destroy the table *id, and zero *id.  0, or -1. */
static uint32_t FS98_STDCALL host_1dc(uint32_t id)
{
    uint32_t t = host_accel_find(id);
    if (t == 0)
        return (uint32_t)-1;
    M32(t + 4) = 0;
    M32(t) = 0;
    if (M32(t + 8) != 0)
        host_214(t + 8);
    M32(t + 0xc) = 0;
    M32(id) = 0;
    host_300(HOST_ACCELS, t);
    return 0;
}

/* +0x1e0 (0x40c1d0): the id of the i-th table into *out (0 and -1 past the
 * end). */
static uint32_t FS98_STDCALL host_1e0(uint32_t out, uint32_t i)
{
    uint32_t t = i < L_COUNT(HOST_ACCELS) ? L_SIZE(HOST_ACCELS) * i + L_DATA(HOST_ACCELS) : 0;
    if (t == 0) {
        M32(out) = 0;
        return (uint32_t)-1;
    }
    M32(out) = M32(t + 4);
    return 0;
}

/* 0x40ccf0: the walk's callback -- LoadAcceleratorsA from one module. */
static uint32_t FS98_STDCALL host_accel_cb(uint32_t elem, uint32_t res)
{
    return fs98_handle(LoadAcceleratorsA((HINSTANCE)(uintptr_t)M32(elem),
                                  (const char *)(uintptr_t)(uint32_t)M16(res)));
}

/* +0x1e4 (0x40c200): a new table from accelerator resource `res`, found as
 * strings are (the string modules newest first, then the host); its id into
 * *out.  -1 with *out 0 if no module has it. */
static uint32_t FS98_STDCALL host_1e4(uint32_t out, uint32_t res)
{
    uint32_t h, n, tmp, t, i;
    h = host_30c(HOST_STRMODS, fs98_handle(&res), fs98_handle(host_accel_cb));
    if (h == 0)
        h = fs98_handle(LoadAcceleratorsA((HINSTANCE)(uintptr_t)M32(V(0x41882c)),
                                   (const char *)(uintptr_t)(res & 0xffff)));
    if (h == 0) {
        M32(out) = 0;
        return (uint32_t)-1;
    }
    n = (uint32_t)CopyAcceleratorTableA((HACCEL)(uintptr_t)h, 0, 0);
    if (host_20c(1, n * 6, fs98_handle(&tmp), 0) != 0)
        return (uint32_t)-1;
    CopyAcceleratorTableA((HACCEL)(uintptr_t)h, (ACCEL *)(uintptr_t)tmp, (int)n);
    DestroyAcceleratorTable((HACCEL)(uintptr_t)h);
    t = host_accel_new();
    M32(out) = M32(t + 4);
    M32(t + 0xc) = n;
    host_20c(1, n * 12, t + 8, 1);
    for (i = 0; i < M32(t + 0xc); i++)
        host_accel_entry(tmp + 6 * i, M32(t + 8) + 12 * i);
    host_214(fs98_handle(&tmp));
    return 0;
}

/* +0x1e8 / +0x1ec (0x40c310 / 0x40c350): enable / disable table `id`.  0,
 * or -1 for id 0 or an unknown one. */
static uint32_t FS98_STDCALL host_1e8(uint32_t id)
{
    uint32_t t;
    if (id == 0)
        return (uint32_t)-1;
    t = host_accel_find(fs98_handle(&id));
    if (t == 0)
        return (uint32_t)-1;
    M32(t) = 1;
    return 0;
}

static uint32_t FS98_STDCALL host_1ec(uint32_t id)
{
    uint32_t t;
    if (id == 0)
        return (uint32_t)-1;
    t = host_accel_find(fs98_handle(&id));
    if (t == 0)
        return (uint32_t)-1;
    M32(t) = 0;
    return 0;
}

/* +0x1f0 (0x40c390): empty table `id`.  0, or -1. */
static uint32_t FS98_STDCALL host_1f0(uint32_t id)
{
    uint32_t t = host_accel_find(fs98_handle(&id));
    if (t == 0)
        return (uint32_t)-1;
    host_214(t + 8);
    M32(t + 0xc) = 0;
    return 0;
}

/* +0x1f4 (0x40c3d0): append n entries to table `id`.  0, or -1. */
static uint32_t FS98_STDCALL host_1f4(uint32_t id, uint32_t entries, uint32_t n)
{
    uint32_t t = host_accel_find(fs98_handle(&id));
    if (t == 0)
        return (uint32_t)-1;
    if (M32(t + 0xc) != 0)
        host_210(1, (M32(t + 0xc) + n) * 12, t + 8);
    else
        host_20c(1, n * 12, t + 8, 1);
    if (M32(t + 8) == 0)
        return (uint32_t)-1;
    host_21c(M32(t + 0xc) * 12 + M32(t + 8), entries, n * 12);
    M32(t + 0xc) = M32(t + 0xc) + n;
    return 0;
}

/* +0x1f8 (0x40c470): remove from table `id` every entry equal to one of the
 * n given.  Each match closes the gap and zeroes the last slot; the count
 * only drops at the end, the array shrinking to it (freed if empty).  0, or
 * -1 for an unknown table. */
static uint32_t FS98_STDCALL host_1f8(uint32_t id, uint32_t entries, uint32_t n)
{
    uint32_t t = host_accel_find(fs98_handle(&id)), removed = 0, k, j, at, next, cnt;
    if (t == 0)
        return (uint32_t)-1;
    for (k = n; k != 0; k--, entries += 12) {
        cnt = M32(t + 0xc);
        if (cnt - removed == 0)
            continue;
        at = 0;
        next = 0xc;
        j = 0;
        do {
            uint32_t e = M32(t + 8) + at;
            if (M32(e) == M32(entries) && M32(e + 4) == M32(entries + 4) &&
                M32(e + 8) == M32(entries + 8)) {
                host_21c(e, M32(t + 8) + next, cnt * 12 - next);
                host_218(M32(t + 0xc) * 12 + M32(t + 8) - 0xc, 0, 0xc);
                at -= 0xc;
                removed++;
                j--;
                next -= 0xc;
            }
            j++;
            cnt = M32(t + 0xc);
            at += 0xc;
            next += 0xc;
        } while (j < cnt - removed);
    }
    cnt = M32(t + 0xc) - removed;
    M32(t + 0xc) = cnt;
    if (removed == 0)
        return 0;
    if (cnt == 0) {
        host_214(t + 8);
        return 0;
    }
    host_210(1, cnt * 12, t + 8);
    return 0;
}

/* +0x1fc (0x40c5b0): with mode 2, the count of table `id`'s entries from
 * `start` on into *n, and those entries into buf if it is not null (*n 0
 * when start is past the end).  Any other mode does nothing.  0, or -1 for
 * an unknown table. */
static uint32_t FS98_STDCALL host_1fc(uint32_t id, uint32_t unused, uint32_t start, uint32_t mode,
                                      uint32_t buf, uint32_t n)
{
    uint32_t t = host_accel_find(fs98_handle(&id)), c;
    (void)unused;
    if (t == 0)
        return (uint32_t)-1;
    if (mode != 2)
        return 0;
    c = M32(t + 0xc);
    if (start > c) {
        M32(n) = 0;
        return 0;
    }
    M32(n) = c - start;
    if (buf != 0)
        host_21c(buf, start * 12 + M32(t + 8), (c - start) * 12);
    return 0;
}

/* ---- key names --------------------------------------------------------------- */

/* The words a key's name is made of, strings 0x9520..0x9529 that the host's
 * init loads (+0x2a4) into these pointers: "CTRL + ", "SHIFT + ", "ALT + ",
 * "NumLock", "Num Enter", "Num /", "Break", "Menu", "Right Windows",
 * "Left Windows". */
#define KN_CTRL     M32(V(0x41bec0))
#define KN_SHIFT    M32(V(0x41bc80))
#define KN_ALT      M32(V(0x41bb64))
#define KN_NUMLOCK  M32(V(0x41bfe8))
#define KN_NUMENTER M32(V(0x41bb68))
#define KN_NUMDIV   M32(V(0x41bdac))
#define KN_BREAK    M32(V(0x41bda4))
#define KN_MENU     M32(V(0x41c360))
#define KN_RWIN     M32(V(0x41bc84))
#define KN_LWIN     M32(V(0x41c104))

/* The keyboard's own name for virtual key vk, as both services ask for it:
 * the scan code with the "don't care" bit (0x200 << 16). */
static uint32_t host_key_text(uint32_t vk, uint32_t buf, uint32_t len)
{
    uint32_t sc = MapVirtualKeyA(vk, 0) | 0x200;
    return (uint32_t)GetKeyNameTextA((LONG)(sc << 16), (char *)(uintptr_t)buf, (int)len);
}

/* +0x200 (0x40c630): the name of key code `key` -- a virtual key in the low
 * byte, 2 ctrl, 1 shift, 4 alt in the next -- into buf (len bytes): the
 * modifiers' words, then the key's.  Returns leftover eax: 0 for virtual key
 * 0 (which names nothing past the modifiers), strncpy's destination for the
 * host's own words, else GetKeyNameTextA's count. */
static uint32_t FS98_STDCALL host_200(uint32_t key, uint32_t buf, uint32_t len)
{
    uint32_t vk = key & 0xff, mods = key >> 8, n, w;
    M8(buf) = 0;
    if (mods & 2)
        host_2b0(buf, KN_CTRL);
    if (mods & 1)
        host_2b0(buf, KN_SHIFT);
    if (mods & 4)
        host_2b0(buf, KN_ALT);
    n = host_2b8(buf);
    switch (vk) {
    case 0x00: return 0;
    case 0x03: case 0x13: w = KN_BREAK; break;
    case 0x5b: w = KN_LWIN; break;
    case 0x5c: w = KN_RWIN; break;
    case 0x5d: w = KN_MENU; break;
    case 0x6f: w = KN_NUMDIV; break;
    case 0x87: w = KN_NUMENTER; break;
    case 0x90: w = KN_NUMLOCK; break;
    default:
        return host_key_text(vk, buf + n, len - n - 1);     /* RETAIL BUG (kept): HOST-10 -- len - n - 1 unchecked */
    }
    return host_2ac(buf + n, w, len - n - 1);               /* RETAIL BUG (kept): HOST-10 -- as above; strncpy's zero fill then runs on */
}

/* Does `s` begin with word `w`?  (host +0x228 over the word's length.) */
static int host_key_word(uint32_t s, uint32_t w)
{
    return host_228(s, w, host_2b8(w)) == 0;
}

/* +0x204 (0x40c860): the key code a name denotes, into *out: the modifier
 * words (in the order ctrl, shift, alt, each optional), then one of the
 * host's own key words by prefix, else the first virtual key 0..0xff whose
 * keyboard name matches the rest case-insensitively (0x100 if none).
 * Returns leftover eax: 0 on a match, else the last _strcmpi. */
static uint32_t FS98_STDCALL host_204(uint32_t s, uint32_t out)
{
    char text[0x80];
    uint32_t mods = 0, vk, r;
    if (host_key_word(s, KN_CTRL)) {
        mods = 2;
        s += host_2b8(KN_CTRL);
    }
    if (host_key_word(s, KN_SHIFT)) {
        mods |= 1;
        s += host_2b8(KN_SHIFT);
    }
    if (host_key_word(s, KN_ALT)) {
        mods |= 4;
        s += host_2b8(KN_ALT);
    }
    r = 0;
    if (host_key_word(s, KN_BREAK))
        vk = 0x13;
    else if (host_key_word(s, KN_LWIN))
        vk = 0x5b;
    else if (host_key_word(s, KN_RWIN))
        vk = 0x5c;
    else if (host_key_word(s, KN_MENU))
        vk = 0x5d;
    else if (host_key_word(s, KN_NUMDIV))
        vk = 0x6f;
    else if (host_key_word(s, KN_NUMENTER))
        vk = 0x87;
    else if (host_key_word(s, KN_NUMLOCK))
        vk = 0x90;
    else {
        for (vk = 0; vk < 0x100; vk++) {
            host_key_text(vk, fs98_handle(text), 0x7f);
            r = (uint32_t)_strcmpi((const char *)(uintptr_t)s, text);
            if (r == 0)
                break;
        }
    }
    M32(out) = (mods << 8) + vk;
    return r;
}

/* ---- profiles ---------------------------------------------------------------- */

/* The sim's configuration file is FLTSIM98.CFG, its full path at 0x41c120;
 * the services from +0x0d0 on take a path and an extension id instead.
 * Lists are written "1,2,3" and read back by strtoul/strtod, skipping ", "
 * between; a list buffer is 15 bytes an int or 30 a double, unchecked. */
#define HOST_CFG        V(0x41c120)

/* MSVCRT's strtod, as retail calls it (mingw's header would substitute its
 * own). */
extern double __cdecl host_crt_strtod(const char *s, char **end) __asm__("_strtod");

/* 0x40b4f0: extension `id`'s text into buf -- "" for -2, a built-in name,
 * or a registered one.  0, or -1 for an id past the registered ones. */
static uint32_t host_ext_name(uint32_t id, uint32_t buf)
{
    if (id == 0xfffffffeu) {
        M8(buf) = 0;
        return 0;
    }
    if ((int32_t)id < 0x25) {
        host_2a8(buf, host_rebase(M32(V(0x417f20) + 4 * id)));
        return 0;
    }
    if ((int32_t)id < (int32_t)(M32(V(0x4188ac)) + 0x25)) {
        host_2a8(buf, M32(M32(V(0x4188b0)) + 4 * id - 0x94));
        return 0;
    }
    return (uint32_t)-1;
}

/* 0x40b350: *out = a new block holding path + the extension's text.  0, or
 * -1 for an unknown extension (and nothing allocated). */
static uint32_t host_path_ext(uint32_t out, uint32_t path, uint32_t id)
{
    char ext[0x100];
    if (host_ext_name(id, fs98_handle(ext)) == (uint32_t)-1)
        return (uint32_t)-1;
    host_20c(1, host_2b8(path) + host_2b8(fs98_handle(ext)) + 1, out, 0);
    host_2a8(M32(out), path);
    host_2b0(M32(out), fs98_handle(ext));
    return 0;
}

/* The bodies, over an explicit file. */
static void host_prof_get_int(uint32_t sec, uint32_t key, uint32_t io, uint32_t file)
{
    M32(io) = GetPrivateProfileIntA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key, (INT)M32(io),
                                    (LPCSTR)(uintptr_t)file);
}

static void host_prof_put_int(uint32_t sec, uint32_t key, uint32_t v, uint32_t file)
{
    char t[0x10];
    sprintf(t, (const char *)(uintptr_t)V(0x417a24), v);
    WritePrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key, t,
                               (LPCSTR)(uintptr_t)file);
}

static void host_prof_get_double(uint32_t sec, uint32_t key, uint32_t out, uint32_t file)
{
    char t[0x20];
    t[0] = 0;
    GetPrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key, t, t, 0x1e,
                             (LPCSTR)(uintptr_t)file);
    if (t[0] != 0)
        *(double *)(uintptr_t)out = host_crt_strtod(t, 0);
}

static void host_prof_put_double(uint32_t sec, uint32_t key, double v, uint32_t file)
{
    char t[0x20];
    sprintf(t, (const char *)(uintptr_t)V(0x417ed8), v);
    WritePrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key, t,
                               (LPCSTR)(uintptr_t)file);
}

/* An int or double list: the string read into a block of `per` bytes an
 * entry, then n values parsed off it (0s once it runs out), only if the
 * string is not empty.  -1 if the block cannot be had. */
static uint32_t host_prof_get_list(uint32_t sec, uint32_t key, uint32_t out, uint32_t n,
                                   uint32_t file, int dbl)
{
    uint32_t size = n * (dbl ? 30 : 15), buf;   /* RETAIL BUG (kept): HOST-11 -- 15 bytes an int, 30 a double */
    char *p;
    if (host_20c(1, size, fs98_handle(&buf), 0) != 0)
        return (uint32_t)-1;
    M8(buf) = 0;
    GetPrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                             (LPCSTR)(uintptr_t)buf, (LPSTR)(uintptr_t)buf, size,
                             (LPCSTR)(uintptr_t)file);
    if (M8(buf) != 0) {
        p = (char *)(uintptr_t)buf;
        for (; n != 0; n--) {
            if (dbl) {
                *(double *)(uintptr_t)out = host_crt_strtod(p, &p);
                out += 8;
            } else {
                M32(out) = (uint32_t)strtoul(p, &p, 0);
                out += 4;
            }
            p += strspn(p, (const char *)(uintptr_t)V(0x417ed0));
        }
    }
    host_214(fs98_handle(&buf));
    return 0;
}

/* A list written "v,v,v". */
static uint32_t host_prof_put_list(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n,
                                   uint32_t file, int dbl)
{
    char t[0x24];
    uint32_t buf, i;
    /* RETAIL BUG (kept): HOST-11 -- the same block size, written into unchecked; and a double's "%f" overruns t[0x24] (one value suffices: 318 bytes at most) */
    if (host_20c(1, n * (dbl ? 30 : 15), fs98_handle(&buf), 0) != 0)
        return (uint32_t)-1;
    M8(buf) = 0;
    for (i = 0; i < n; i++) {
        if (i != 0)
            host_2b0(buf, V(0x417ed4));
        if (dbl)
            sprintf(t, (const char *)(uintptr_t)V(0x417ed8), *(double *)(uintptr_t)(vals + 8 * i));
        else
            sprintf(t, (const char *)(uintptr_t)V(0x417a24), M32(vals + 4 * i));
        host_2b0(buf, fs98_handle(t));
    }
    WritePrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                               (LPCSTR)(uintptr_t)buf, (LPCSTR)(uintptr_t)file);
    host_214(fs98_handle(&buf));
    return 0;
}

/* Does the section have anything in it (its key list, read into 4 bytes,
 * non-empty)? */
static void host_prof_section(uint32_t sec, uint32_t out, uint32_t file)
{
    char t[4] = { 0, 0, 0, 0 };
    M32(out) = 0;
    if (GetPrivateProfileStringA((LPCSTR)(uintptr_t)sec, 0, t, t, 4, (LPCSTR)(uintptr_t)file) > 0)
        M32(out) = 1;
}

/* Empty a section, writing it the host's empty double-NUL (0x4188a0). */
static void host_prof_clear(uint32_t sec, uint32_t file)
{
    uint16_t t = M16(V(0x4188a0));
    WritePrivateProfileSectionA((LPCSTR)(uintptr_t)sec, (LPCSTR)&t, (LPCSTR)(uintptr_t)file);
}

/* +0x0a8 (0x408890): *io = the int at section/key of the CFG, *io the default. */
static uint32_t FS98_STDCALL host_0a8(uint32_t sec, uint32_t key, uint32_t io)
{
    host_prof_get_int(sec, key, io, HOST_CFG);
    return 0;
}

/* +0x0ac (0x4088c0): write an int ("%d"). */
static uint32_t FS98_STDCALL host_0ac(uint32_t sec, uint32_t key, uint32_t v)
{
    host_prof_put_int(sec, key, v, HOST_CFG);
    return 0;
}

/* +0x0b0 / +0x0b4 (0x408900 / 0x4089c0): read / write a list of n ints. */
static uint32_t FS98_STDCALL host_0b0(uint32_t sec, uint32_t key, uint32_t out, uint32_t n)
{
    return host_prof_get_list(sec, key, out, n, HOST_CFG, 0);
}

static uint32_t FS98_STDCALL host_0b4(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n)
{
    return host_prof_put_list(sec, key, vals, n, HOST_CFG, 0);
}

/* +0x0b8 / +0x0bc (0x408ac0 / 0x408b10): read (left alone if absent) /
 * write ("%f") a double. */
static uint32_t FS98_STDCALL host_0b8(uint32_t sec, uint32_t key, uint32_t out)
{
    host_prof_get_double(sec, key, out, HOST_CFG);
    return 0;
}

static uint32_t FS98_STDCALL host_0bc(uint32_t sec, uint32_t key, double v)
{
    host_prof_put_double(sec, key, v, HOST_CFG);
    return 0;
}

/* +0x0c0 / +0x0c4 (0x408b60 / 0x408c20): read / write a list of n doubles. */
static uint32_t FS98_STDCALL host_0c0(uint32_t sec, uint32_t key, uint32_t out, uint32_t n)
{
    return host_prof_get_list(sec, key, out, n, HOST_CFG, 1);
}

static uint32_t FS98_STDCALL host_0c4(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n)
{
    return host_prof_put_list(sec, key, vals, n, HOST_CFG, 1);
}

/* +0x0c8 (0x408d20): a string into buf, whose contents are the default.
 * Returns the count. */
static uint32_t FS98_STDCALL host_0c8(uint32_t sec, uint32_t key, uint32_t buf, uint32_t size)
{
    return GetPrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                                    (LPCSTR)(uintptr_t)buf, (LPSTR)(uintptr_t)buf, size,
                                    (LPCSTR)(uintptr_t)HOST_CFG);
}

/* +0x0cc (0x408d50): write a string. */
static uint32_t FS98_STDCALL host_0cc(uint32_t sec, uint32_t key, uint32_t s)
{
    WritePrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                               (LPCSTR)(uintptr_t)s, (LPCSTR)(uintptr_t)HOST_CFG);
    return 0;
}

/* +0x0f8 (0x408860): empty a section of the CFG. */
static uint32_t FS98_STDCALL host_0f8(uint32_t sec)
{
    host_prof_clear(sec, HOST_CFG);
    return 0;
}

/* +0x100 (0x408810): *out = 1 if the CFG's section has keys, else 0. */
static uint32_t FS98_STDCALL host_100(uint32_t sec, uint32_t out)
{
    host_prof_section(sec, out, HOST_CFG);
    return 0;
}

/* The same over path + extension `ext`: each resolves the file first (-1 for
 * an unknown extension) and frees it after. */
static uint32_t FS98_STDCALL host_0d0(uint32_t sec, uint32_t key, uint32_t io, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_get_int(sec, key, io, f);
    host_214(fs98_handle(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0d4(uint32_t sec, uint32_t key, uint32_t v, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_put_int(sec, key, v, f);
    host_214(fs98_handle(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0d8(uint32_t sec, uint32_t key, uint32_t out, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_get_list(sec, key, out, n, f, 0);
    host_214(fs98_handle(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0dc(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_put_list(sec, key, vals, n, f, 0);
    host_214(fs98_handle(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0e0(uint32_t sec, uint32_t key, uint32_t out, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_get_double(sec, key, out, f);
    host_214(fs98_handle(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0e4(uint32_t sec, uint32_t key, double v, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_put_double(sec, key, v, f);
    host_214(fs98_handle(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0e8(uint32_t sec, uint32_t key, uint32_t out, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_get_list(sec, key, out, n, f, 1);
    host_214(fs98_handle(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0ec(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_put_list(sec, key, vals, n, f, 1);
    host_214(fs98_handle(&f));
    return r;
}

/* +0x0f0 (0x409440): a string, buf the default; returns the count. */
static uint32_t FS98_STDCALL host_0f0(uint32_t sec, uint32_t key, uint32_t buf, uint32_t size,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = GetPrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                                 (LPCSTR)(uintptr_t)buf, (LPSTR)(uintptr_t)buf, size,
                                 (LPCSTR)(uintptr_t)f);
    host_214(fs98_handle(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0f4(uint32_t sec, uint32_t key, uint32_t s, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    WritePrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                               (LPCSTR)(uintptr_t)s, (LPCSTR)(uintptr_t)f);
    host_214(fs98_handle(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0fc(uint32_t sec, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_clear(sec, f);
    host_214(fs98_handle(&f));
    return 0;
}

/* +0x104 (0x408d70): the output comes last here, after path and extension. */
static uint32_t FS98_STDCALL host_104(uint32_t sec, uint32_t path, uint32_t ext, uint32_t out)
{
    uint32_t f;
    M32(out) = 0;
    if (host_path_ext(fs98_handle(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_section(sec, out, f);
    host_214(fs98_handle(&f));
    return 0;
}

/* ---- files ------------------------------------------------------------------- */

/* A host file is a 0x10-byte block { HANDLE, path (a heap copy of path +
 * extension), mode, extension id }.  The last Windows error a file service
 * met is kept at 0x41ba50 -- by most of them, on some of their failures. */
#define HOST_FILE_ERR   M32(V(0x41ba50))

/* +0x17c (0x40a000): open path + extension, *out the new file.  Mode 0
 * reads, 1 reads and writes, 2 writes, 3 writes at the end; the file must
 * exist, and is not shared.  0, or -1 (any other mode fails too). */
static uint32_t FS98_STDCALL host_17c(uint32_t out, uint32_t path, uint32_t ext, uint32_t mode)
{
    uint32_t f, h;
    host_20c(1, 0x10, out, 0);
    f = M32(out);
    if (f == 0) {
        HOST_FILE_ERR = 0;
        return (uint32_t)-1;
    }
    if (host_path_ext(f + 4, path, ext) != 0) {
        host_214(out);
        return (uint32_t)-1;
    }
    M32(M32(out) + 0xc) = ext;
    M32(M32(out) + 8) = mode;
    switch (mode) {
    case 0:
    case 1:
    case 2:
        h = fs98_handle(CreateFileA((LPCSTR)(uintptr_t)M32(M32(out) + 4),
                             mode == 0 ? GENERIC_READ : mode == 1 ? GENERIC_READ | GENERIC_WRITE
                                                                  : GENERIC_WRITE,
                             0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
        M32(M32(out)) = h;
        break;
    case 3:
        h = fs98_handle(CreateFileA((LPCSTR)(uintptr_t)M32(M32(out) + 4), GENERIC_WRITE, 0, 0,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
        M32(M32(out)) = h;
        SetFilePointer((HANDLE)(uintptr_t)M32(M32(out)), 0, 0, FILE_END);
        break;
    default:
        M32(M32(out)) = 0xffffffffu;
        break;
    }
    if (M32(M32(out)) == 0xffffffffu) {
        HOST_FILE_ERR = GetLastError();
        host_214(M32(out) + 4);
        host_214(out);
        return (uint32_t)-1;
    }
    return 0;
}

/* +0x148 (0x409f20): create (or truncate) path + extension for reading and
 * writing, *out the new file (mode 1).  0, or -1 -- without the error. */
static uint32_t FS98_STDCALL host_148(uint32_t out, uint32_t path, uint32_t ext)
{
    host_20c(1, 0x10, out, 0);
    if (host_path_ext(M32(out) + 4, path, ext) != 0) {
        host_214(out);
        return (uint32_t)-1;
    }
    M32(M32(out) + 0xc) = ext;
    M32(M32(out) + 8) = 1;
    M32(M32(out)) = fs98_handle(CreateFileA((LPCSTR)(uintptr_t)M32(M32(out) + 4),
                                     GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL, 0));
    if (M32(M32(out)) == 0xffffffffu) {
        host_214(M32(out) + 4);
        host_214(out);
        return (uint32_t)-1;
    }
    return 0;
}

/* +0x13c (0x40a140): close and free a file.  0, or -1 (null, or CloseHandle
 * failed -- the file is freed regardless). */
static uint32_t FS98_STDCALL host_13c(uint32_t f)
{
    uint32_t r = 0;
    if (f == 0) {
        HOST_FILE_ERR = 0;
        return (uint32_t)-1;
    }
    if (!CloseHandle((HANDLE)(uintptr_t)M32(f))) {
        HOST_FILE_ERR = GetLastError();
        r = (uint32_t)-1;
    }
    host_214(f + 4);
    host_214(fs98_handle(&f));
    return r;
}

/* +0x19c (0x40a190): ReadFile n bytes into buf, the count into *got if that
 * is not null.  0, or -1. */
static uint32_t FS98_STDCALL host_19c(uint32_t f, uint32_t buf, uint32_t n, uint32_t got)
{
    DWORD count;
    BOOL ok;
    if (got != 0)
        M32(got) = 0;
    if (f == 0)
        return (uint32_t)-1;
    ok = ReadFile((HANDLE)(uintptr_t)M32(f), (void *)(uintptr_t)buf, n, &count, 0);
    if (got != 0)
        M32(got) = count;
    if (ok)
        return 0;
    HOST_FILE_ERR = GetLastError();
    return (uint32_t)-1;
}

/* +0x1b0 (0x40a1f0): WriteFile, likewise -- without the error. */
static uint32_t FS98_STDCALL host_1b0(uint32_t f, uint32_t buf, uint32_t n, uint32_t put)
{
    DWORD count;
    BOOL ok;
    if (put != 0)
        M32(put) = 0;
    if (f == 0)
        return (uint32_t)-1;
    ok = WriteFile((HANDLE)(uintptr_t)M32(f), (const void *)(uintptr_t)buf, n, &count, 0);
    if (put != 0)
        M32(put) = count;
    return ok ? 0 : (uint32_t)-1;
}

/* +0x194 (0x40a360): seek to pos by `method` -- refused (error 0) if pos is
 * past the file's size, whatever the method.  0, or -1. */
static uint32_t FS98_STDCALL host_194(uint32_t f, uint32_t pos, uint32_t method)
{
    if (f == 0) {
        HOST_FILE_ERR = 0;
        return (uint32_t)-1;
    }
    if (GetFileSize((HANDLE)(uintptr_t)M32(f), 0) < pos) {
        HOST_FILE_ERR = 0;
        return (uint32_t)-1;
    }
    if (SetFilePointer((HANDLE)(uintptr_t)M32(f), (LONG)pos, 0, method) == 0xffffffffu) {
        HOST_FILE_ERR = GetLastError();
        return (uint32_t)-1;
    }
    return 0;
}

/* +0x190 (0x40a3d0): the position into *out. */
static uint32_t FS98_STDCALL host_190(uint32_t f, uint32_t out)
{
    if (f == 0)
        return (uint32_t)-1;
    M32(out) = SetFilePointer((HANDLE)(uintptr_t)M32(f), 0, 0, FILE_CURRENT);
    return 0;
}

/* +0x1a4 (0x40a400): the size into *out. */
static uint32_t FS98_STDCALL host_1a4(uint32_t f, uint32_t out)
{
    if (f == 0)
        return (uint32_t)-1;
    M32(out) = GetFileSize((HANDLE)(uintptr_t)M32(f), 0);
    return 0;
}

/* +0x1a8 (0x40a430): cut the file at pos, keeping the position. */
static uint32_t FS98_STDCALL host_1a8(uint32_t f, uint32_t pos)
{
    uint32_t cur;
    if (f == 0)
        return (uint32_t)-1;
    cur = SetFilePointer((HANDLE)(uintptr_t)M32(f), 0, 0, FILE_CURRENT);
    SetFilePointer((HANDLE)(uintptr_t)M32(f), (LONG)pos, 0, FILE_BEGIN);
    SetEndOfFile((HANDLE)(uintptr_t)M32(f));
    SetFilePointer((HANDLE)(uintptr_t)M32(f), (LONG)cur, 0, FILE_BEGIN);
    return 0;
}

/* +0x134 (0x40a240): the attributes of path + extension as the host's flags
 * into *out: 1 normal, 4 archive, 2 read-only, 8 hidden, 0x10 directory.
 * 0, or -1 if there is no such file. */
static uint32_t FS98_STDCALL host_134(uint32_t out, uint32_t path, uint32_t ext)
{
    uint32_t file, a;
    if (host_path_ext(fs98_handle(&file), path, ext) != 0)
        return (uint32_t)-1;
    a = GetFileAttributesA((LPCSTR)(uintptr_t)file);
    if (a == 0xffffffffu) {
        host_214(fs98_handle(&file));
        return (uint32_t)-1;
    }
    M32(out) = 0;
    if (a & FILE_ATTRIBUTE_NORMAL)
        M32(out) = 1;
    if (a & FILE_ATTRIBUTE_ARCHIVE)
        M32(out) |= 4;
    if (a & FILE_ATTRIBUTE_READONLY)
        M32(out) |= 2;
    if (a & FILE_ATTRIBUTE_HIDDEN)
        M32(out) |= 8;
    if (a & FILE_ATTRIBUTE_DIRECTORY)
        M32(out) |= 0x10;
    host_214(fs98_handle(&file));
    return 0;
}

/* +0x138 (0x40a2c0): set them, from the same flags.  0, or -1 if there is no
 * such file or Windows refuses. */
static uint32_t FS98_STDCALL host_138(uint32_t flags, uint32_t path, uint32_t ext)
{
    uint32_t file, a = 0;
    BOOL ok;
    if (host_path_ext(fs98_handle(&file), path, ext) != 0)
        return (uint32_t)-1;
    if (GetFileAttributesA((LPCSTR)(uintptr_t)file) == 0xffffffffu) {
        host_214(fs98_handle(&file));
        return (uint32_t)-1;
    }
    if (flags & 1)
        a = FILE_ATTRIBUTE_NORMAL;
    if (flags & 4)
        a |= FILE_ATTRIBUTE_ARCHIVE;
    if (flags & 2)
        a |= FILE_ATTRIBUTE_READONLY;
    if (flags & 8)
        a |= FILE_ATTRIBUTE_HIDDEN;
    if (flags & 0x10)
        a |= FILE_ATTRIBUTE_DIRECTORY;
    ok = SetFileAttributesA((LPCSTR)(uintptr_t)file, a);
    host_214(fs98_handle(&file));
    return ok ? 0 : (uint32_t)-1;
}

/* +0x158 (0x409ed0): does path + extension exist?  0, or -1. */
static uint32_t FS98_STDCALL host_158(uint32_t path, uint32_t ext)
{
    uint32_t file, r = 0;
    if (host_path_ext(fs98_handle(&file), path, ext) != 0)
        return (uint32_t)-1;
    if (GetFileAttributesA((LPCSTR)(uintptr_t)file) == 0xffffffffu)
        r = (uint32_t)-1;
    host_214(fs98_handle(&file));
    return r;
}

/* +0x11c (0x409fb0): make a directory.  0, or -1 -- leaking its copy of the
 * path when CreateDirectoryA fails. */
static uint32_t FS98_STDCALL host_11c(uint32_t path)
{
    uint32_t file = 0;
    if (host_path_ext(fs98_handle(&file), path, 0) != 0)
        return (uint32_t)-1;
    if (!CreateDirectoryA((LPCSTR)(uintptr_t)file, 0))
        return (uint32_t)-1;    /* RETAIL BUG (kept): HOST-12 -- the copy is not freed */
    return host_214(fs98_handle(&file)) != 0 ? (uint32_t)-1 : 0;
}

/* +0x14c (0x40acb0): delete path + extension.  0, or -1. */
static uint32_t FS98_STDCALL host_14c(uint32_t path, uint32_t ext)
{
    uint32_t file;
    BOOL ok;
    if (host_path_ext(fs98_handle(&file), path, ext) != 0)
        return (uint32_t)-1;
    ok = DeleteFileA((LPCSTR)(uintptr_t)file);
    host_214(fs98_handle(&file));
    return ok ? 0 : (uint32_t)-1;
}

/* +0x144 / +0x16c (0x40ad00 / 0x40ad90): copy / move the file (from_path,
 * from_ext) to (to_path, to_ext) -- the destination first in the arguments.
 * A copy will not overwrite.  0, or -1. */
static uint32_t host_copy_move(uint32_t to, uint32_t to_ext, uint32_t from, uint32_t from_ext,
                               int move)
{
    uint32_t src, dst;
    BOOL ok;
    if (host_path_ext(fs98_handle(&src), from, from_ext) != 0)
        return (uint32_t)-1;
    if (host_path_ext(fs98_handle(&dst), to, to_ext) != 0) {
        host_214(fs98_handle(&src));
        return (uint32_t)-1;
    }
    ok = move ? MoveFileA((LPCSTR)(uintptr_t)src, (LPCSTR)(uintptr_t)dst)
              : CopyFileA((LPCSTR)(uintptr_t)src, (LPCSTR)(uintptr_t)dst, TRUE);
    host_214(fs98_handle(&src));
    host_214(fs98_handle(&dst));
    return ok ? 0 : (uint32_t)-1;
}

static uint32_t FS98_STDCALL host_144(uint32_t to, uint32_t to_ext, uint32_t from, uint32_t from_ext)
{
    return host_copy_move(to, to_ext, from, from_ext, 0);
}

static uint32_t FS98_STDCALL host_16c(uint32_t to, uint32_t to_ext, uint32_t from, uint32_t from_ext)
{
    return host_copy_move(to, to_ext, from, from_ext, 1);
}

/* ---- directory enumeration --------------------------------------------------- */

/* A search is a 0x14c-byte block: the FindFirstFileA handle, its
 * WIN32_FIND_DATAA (+4: attributes at +4, size at +0x24, name at +0x30), the
 * extension id at +0x144, and at +0x148 whether an entry is waiting. */

/* +0x164 (0x40a9a0): open a search for dir "\" pattern extension (pattern
 * "*" if null; extension -1 appends ".*", -2 nothing).  *out the search; 0,
 * or -1 if the extension is unknown or nothing matches.  An empty dir tests
 * the byte before the path buffer for '\' -- in retail the top byte of the
 * search block's address, which sits there. */
static uint32_t FS98_STDCALL host_164(uint32_t out, uint32_t dir, uint32_t pattern, uint32_t ext)
{
    char buf[0x104];
    uint32_t f, n;
    host_20c(1, 0x14c, fs98_handle(&f), 0);
    M32(f + 0x144) = ext;
    M32(f + 0x148) = 0;
    buf[0] = 0;
    if (dir != 0) {
        host_2b0(fs98_handle(buf), dir);
        n = host_2b8(fs98_handle(buf));
        /* RETAIL BUG (kept): HOST-1 -- an empty dir reads the byte before buf (the heap pointer's top byte; 0x5c appends nothing) */
        if ((n != 0 ? (uint8_t)buf[n - 1] : (uint8_t)(f >> 24)) != '\\')
            host_2b0(fs98_handle(buf), V(0x4180c4));
    }
    host_2b0(fs98_handle(buf), pattern != 0 ? pattern : V(0x4180d0));
    if (ext == 0xffffffffu) {
        host_2b0(fs98_handle(buf), V(0x4180cc));
    } else if (host_ext_name(ext, fs98_handle(buf) + host_2b8(fs98_handle(buf))) == (uint32_t)-1) {
        host_214(fs98_handle(&f));
        return (uint32_t)-1;
    }
    M32(f) = fs98_handle(FindFirstFileA(buf, (WIN32_FIND_DATAA *)(uintptr_t)(f + 4)));
    if (M32(f) == 0xffffffffu) {
        host_214(fs98_handle(&f));
        return (uint32_t)-1;
    }
    M32(f + 0x148) = 1;
    M32(out) = f;
    return 0;
}

/* +0x160 (0x40ab60): the next entry, skipping "." and "..": a directory's
 * name with type -2 and size 0; a file's name without its extension, the
 * extension's id if the host knows it (left alone if not -- it is not
 * registered), its size, and 0 if read-only else 1.  0, or -1 when there are
 * no more. */
static uint32_t FS98_STDCALL host_160(uint32_t f, uint32_t entry)
{
    char ext[0x100];
    uint32_t found = 0;
    do {
        if (M32(f + 0x148) == 0)
            return (uint32_t)-1;
        if (M8(f + 4) & FILE_ATTRIBUTE_DIRECTORY) {
            if (host_2bc(f + 0x30, V(0x4180d8)) != 0 && host_2bc(f + 0x30, V(0x4180d4)) != 0) {
                host_2a8(entry, f + 0x30);
                M32(entry + 0x104) = 0xfffffffeu;
                M32(entry + 0x108) = 0;
                found = 1;
            }
        } else {
            _splitpath((const char *)(uintptr_t)(f + 0x30), 0, 0, (char *)(uintptr_t)entry, ext);
            host_ext_lookup(fs98_handle(ext), entry + 0x104);
            M32(entry + 0x108) = M32(f + 0x24);
            M32(entry + 0x10c) = (M8(f + 4) & FILE_ATTRIBUTE_READONLY) ? 0 : 1;
            found = 1;
        }
        M32(f + 0x148) = 0;
        if (FindNextFileA((HANDLE)(uintptr_t)M32(f), (WIN32_FIND_DATAA *)(uintptr_t)(f + 4)))
            M32(f + 0x148) = 1;
    } while (!found);
    return 0;
}

/* +0x15c (0x40ac90): close the search *pf and null it.  Returns 0. */
static uint32_t FS98_STDCALL host_15c(uint32_t pf)
{
    FindClose((HANDLE)(uintptr_t)M32(M32(pf)));
    host_214(pf);
    return 0;
}

/* ---- messages to the main window, the clock ---------------------------------- */

/* The main window is at 0x41880c; 0x418868 is set when the sim is asked to
 * quit. */
#define HOST_HWND       ((HWND)(uintptr_t)M32(V(0x41880c)))

/* +0x024 (0x408550): quit -- set the flag and send WM_CLOSE.  Returns 1. */
static uint32_t FS98_STDCALL host_024(void)
{
    host_quit();
    return 1;
}

/* +0x028 (0x408580): the same by posting command 0x110a.  Returns 1. */
static uint32_t FS98_STDCALL host_028(void)
{
    M32(V(0x418868)) = 1;
    PostMessageA(HOST_HWND, WM_COMMAND, 0x110a, 0);
    return 1;
}

/* +0x038 (0x4085c0): timeGetTime, a plain thunk. */
static uint32_t FS98_STDCALL host_038(void)
{
    return timeGetTime();
}

/* +0x03c / +0x040 (0x408620 / 0x408650): post / send WM_COMMAND (command,
 * parameter) to the main window.  Return 0. */
static uint32_t FS98_STDCALL host_03c(uint32_t command, uint32_t param)
{
    PostMessageA(HOST_HWND, WM_COMMAND, command, (LPARAM)param);
    return 0;
}

static uint32_t FS98_STDCALL host_040(uint32_t command, uint32_t param)
{
    SendMessageA(HOST_HWND, WM_COMMAND, command, (LPARAM)param);
    return 0;
}

/* +0x044 (0x4085d0): the local date and time as six words: year, month,
 * day, hour, minute, second (SYSTEMTIME without the weekday or the
 * milliseconds).  Returns 0. */
static uint32_t FS98_STDCALL host_044(uint32_t out)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    M16(out + 0) = t.wYear;
    M16(out + 2) = t.wMonth;
    M16(out + 4) = t.wDay;
    M16(out + 6) = t.wHour;
    M16(out + 8) = t.wMinute;
    M16(out + 10) = t.wSecond;
    return 0;
}

/* A view window's record (window.md): its title at +0, flags at +8, the
 * host's block at +0xa0, which holds the frame and client HWNDs at +0x24 and
 * +0x28. */
#define WIN_FRAME(w)    ((HWND)(uintptr_t)M32(M32((w) + 0xa0) + 0x24))
#define WIN_CLIENT(w)   ((HWND)(uintptr_t)M32(M32((w) + 0xa0) + 0x28))

/* +0x07c (0x410c00): tell the frame 0x467, then give it the record's title.
 * Returns SetWindowTextA's result (for a null record, whatever eax held --
 * no caller reads it). */
static uint32_t FS98_STDCALL host_07c(uint32_t w)
{
    if (w == 0)
        return 0;
    SendMessageA(WIN_FRAME(w), 0x467, 0, 0);
    return (uint32_t)SetWindowTextA(WIN_FRAME(w), (LPCSTR)(uintptr_t)M32(w));
}

/* +0x080 (0x410c40): give frame and client the record's title. */
static uint32_t FS98_STDCALL host_080(uint32_t w)
{
    SetWindowTextA(WIN_FRAME(w), (LPCSTR)(uintptr_t)M32(w));
    return (uint32_t)SetWindowTextA(WIN_CLIENT(w), (LPCSTR)(uintptr_t)M32(w));
}

/* +0x084 (0x410c70): if the record's flags have the top bit, tell the frame
 * 0x466.  Returns leftover eax: 0 for a null record, the record when the bit
 * is clear, else SendMessageA's result. */
static uint32_t FS98_STDCALL host_084(uint32_t w)
{
    if (w == 0)
        return 0;
    if (!(M32(w + 8) & 0x80000000u))
        return w;
    return (uint32_t)SendMessageA(WIN_FRAME(w), 0x466, 0, 0);
}

/* 0x410ce0: merge rectangle *r ({left, top, right, bottom}) into the list's
 * rectangle e if they overlap or come within 16 of each other (the test as
 * retail writes it, signed); returns e if merged, else 0. */
static uint32_t FS98_STDCALL host_rect_merge(uint32_t e, uint32_t r)
{
    int32_t el = (int32_t)M32(e), et = (int32_t)M32(e + 4), er = (int32_t)M32(e + 8);
    int32_t eb = (int32_t)M32(e + 0xc);
    int32_t rl = (int32_t)M32(r), rt = (int32_t)M32(r + 4), rr = (int32_t)M32(r + 8);
    int32_t rb = (int32_t)M32(r + 0xc);
    if (rl > er + 0x10 || rl < el - rr + rl - 0x10)
        return 0;
    if (rt > eb + 0x10 || rt < et + rt - rb - 0x10)
        return 0;
    M32(e) = (uint32_t)(el < rl ? el : rl);
    M32(e + 4) = (uint32_t)(et < rt ? et : rt);
    M32(e + 8) = (uint32_t)(er > rr ? er : rr);
    M32(e + 0xc) = (uint32_t)(eb > rb ? eb : rb);
    return e;
}

/* +0x088 (0x410ca0): add rectangle r to a view window's dirty list (the host
 * list at +0x54 of its host block): merged into the first rectangle near
 * enough, else appended.  Returns the rectangle merged into, or 0. */
static uint32_t FS98_STDCALL host_088(uint32_t w, uint32_t r)
{
    uint32_t l, e;
    if (w == 0)
        return 0;
    l = M32(w + 0xa0) + 0x54;
    e = host_308(l, r, fs98_handle(host_rect_merge));
    if (e != 0)
        return e;
    return host_21c(host_2f8(l), r, 0x10);
}

/* ---- joysticks --------------------------------------------------------------- */

/* The joysticks the host has found: up to 16 entries of 0x198 bytes at
 * 0x4188b8 -- the WINMM id + 1, then its JOYCAPSA (0x194 bytes) -- 0x41a23c
 * of them; 0x41a238 once enumerated.  0x418100 / 0x418104 are a slow-probe
 * mode: the device at the limit is polled sixteen times 10 ms apart first. */
#define JOY_ENTRY(i)    (V(0x4188b8) + 0x198 * (i))
#define JOY_COUNT       M32(V(0x41a23c))

typedef MMRESULT (WINAPI *host_joyposex_fn)(UINT, LPJOYINFOEX);

/* +0x1c0 (0x40b780): enumerate the joysticks, adding new ones to the table.
 * Returns how many answered. */
static uint32_t FS98_STDCALL host_1c0(void)
{
    JOYINFOEX info;
    uint32_t found = 0, num = joyGetNumDevs(), i, k, limit;
    if (num == 0)
        return 0;
    if (JOY_COUNT == 0)
        host_218(V(0x4188b8), 0, 0x1980);
    for (i = 0; i < num; i++) {
        limit = M32(V(0x418104));
        if (i >= limit)
            goto done;
        if (M32(V(0x418100)) != 0 && i == limit - 1) {
            for (k = 0x10; k != 0; k--) {
                info.dwSize = 0x34;
                info.dwFlags = 0xff;
                joyGetPosEx(i, &info);
                Sleep(10);
            }
            Sleep(100);
        }
        info.dwSize = 0x34;
        info.dwFlags = 0xff;
        if (joyGetPosEx(i, &info) == 0) {
            found++;
            for (k = 0; k < JOY_COUNT; k++)
                if (M32(JOY_ENTRY(k)) == i + 1)
                    break;
            if (k >= JOY_COUNT) {
                joyGetDevCapsA(i, (JOYCAPSA *)(uintptr_t)(JOY_ENTRY(JOY_COUNT) + 4), 0x194);
                M32(JOY_ENTRY(JOY_COUNT)) = i + 1;
                JOY_COUNT = JOY_COUNT + 1;
            }
        }
        if (i + 1 >= 0x10)
            break;
    }
    limit = M32(V(0x418104));
done:
    if (M32(V(0x418100)) != 0 && limit < num)
        M32(V(0x418104)) = limit + 1;
    else
        M32(V(0x418100)) = 0;
    M32(V(0x41a238)) = 1;
    return found;
}

/* +0x1c4 (0x40b8f0): read joystick `idx` of the table into a state block:
 * axes at +0xc.. (each - 0x8000) for the bits of the mask at +8 -- which is
 * first cut to the axes the caps report (the bit for each of R, U, V, Z the
 * device has, then one more for each axis left over) -- 32 buttons as 0/1
 * from +0x4c, the POV as one of eight directions from +0xcc, the raw POV at
 * +0x24 for mask bit 0x40.  0, or -1. */
static uint32_t FS98_STDCALL host_1c4(uint32_t idx, uint32_t st)
{
    JOYINFOEX info;
    uint32_t e, dev, caps, mask = 0, bit, *p;
    uint16_t axes;
    uint32_t pov;
    if (M32(V(0x41a238)) == 0)
        return (uint32_t)-1;
    if (M32(V(0x418858)) == 0)
        return (uint32_t)-1;
    e = JOY_ENTRY(idx);
    dev = M32(e) - 1;
    if (dev > 0x10)
        return (uint32_t)-1;
    caps = M32(e + 0x64);
    axes = M16(e + 0x6c);
    /* RETAIL BUG (kept): HOST-23 -- the axis count is 16 bits and wraps when the caps claim fewer axes than flags */
    if (caps & 0x8) { mask = 8; axes--; }
    if (caps & 0x4) { mask |= 4; axes--; }
    if (caps & 0x2) { mask |= 2; axes--; }
    if (caps & 0x1) { mask |= 1; axes--; }
    for (; axes != 0; axes--)
        mask = mask * 2 | 1;
    mask &= 0xffff;
    info.dwSize = 0x34;
    info.dwFlags = 0x4ff;
    M32(st + 8) = M32(st + 8) & mask;
    if (joyGetPosEx(dev, &info) != 0)
        return (uint32_t)-1;
    p = (uint32_t *)(uintptr_t)(st + 0x4c);
    for (bit = 1; bit != 0; bit <<= 1)
        *p++ = (info.dwButtons & bit) ? 1 : 0;
    host_218(st + 0xcc, 0, 0x80);
    pov = info.dwPOV;
    if (M8(e + 0x64) & 0x10) {
        if (pov < 0x8ca || (pov >= 0x83d6 && pov < 0x8c3c)) M32(st + 0xcc) = 1;
        else if (pov < 0x1a5e) M32(st + 0xd0) = 1;
        else if (pov < 0x2bf2) M32(st + 0xd4) = 1;
        else if (pov < 0x3d86) M32(st + 0xd8) = 1;
        else if (pov < 0x4f1a) M32(st + 0xdc) = 1;
        else if (pov < 0x60ae) M32(st + 0xe0) = 1;
        else if (pov < 0x7242) M32(st + 0xe4) = 1;
        else if (pov < 0x83d6) M32(st + 0xe8) = 1;
    }
    if (M32(st + 8) & 0x01) M32(st + 0x0c) = info.dwXpos - 0x8000;
    if (M32(st + 8) & 0x02) M32(st + 0x10) = info.dwYpos - 0x8000;
    if (M32(st + 8) & 0x04) M32(st + 0x14) = info.dwZpos - 0x8000;
    if (M32(st + 8) & 0x08) M32(st + 0x18) = info.dwRpos - 0x8000;
    if (M32(st + 8) & 0x10) M32(st + 0x1c) = info.dwUpos - 0x8000;
    if (M32(st + 8) & 0x20) M32(st + 0x20) = info.dwVpos - 0x8000;
    if (M32(st + 8) & 0x40) M32(st + 0x24) = pov;
    return 0;
}

/* 0x40bb30: refresh joystick idx's caps into the table and copy them to
 * `caps`; then its OEM name from the registry into `name`: the value
 * Joystick<n>OEMName under MediaResources\Joystick\<its regkey>\
 * CurrentJoystickSettings, then OEMName under MediaProperties\...\OEM\<that>.
 * 0 when both are found; RegOpenKeyExA's error if a key is missing; -1 with
 * name "" if the first value is; the second query's error otherwise. */
static uint32_t host_joy_oem(uint32_t idx, uint32_t caps, uint32_t name)
{
    char path[0x100], val[0x30], key[0x30], oem[0x30];
    HKEY h;
    DWORD size;
    uint32_t e = JOY_ENTRY(idx), r;
    joyGetDevCapsA(M32(e) - 1, (JOYCAPSA *)(uintptr_t)(e + 4), 0x194);
    host_21c(caps, e + 4, 0x194);
    sprintf(path, (const char *)(uintptr_t)V(0x4181e8), (const char *)(uintptr_t)V(0x4181f4),
            (const char *)(uintptr_t)(caps + 0x70), (const char *)(uintptr_t)V(0x418230));
    r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_ALL_ACCESS, &h);
    if (r != 0)
        return r;
    sprintf(val, (const char *)(uintptr_t)V(0x4181d4), M32(e));
    size = 0x30;
    r = RegQueryValueExA(h, val, 0, 0, (BYTE *)key, &size);
    RegCloseKey(h);
    if (r != 0) {
        M8(name) = 0;
        return (uint32_t)-1;
    }
    sprintf(path, (const char *)(uintptr_t)V(0x41817c), (const char *)(uintptr_t)V(0x418184), key);
    r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_ALL_ACCESS, &h);
    if (r != 0)
        return r;
    size = 0x30;
    r = RegQueryValueExA(h, (const char *)(uintptr_t)V(0x418174), 0, 0, (BYTE *)oem, &size);
    RegCloseKey(h);
    host_2a8(name, fs98_handle(oem));
    return r;
}

/* The first string id 0xb000..0xb0ff equal to `s` into *out; 0, or -1. */
static uint32_t host_joy_match(uint32_t s, uint32_t out)
{
    char t[0x30];
    uint32_t id;
    for (id = 0xb000; id < 0xb100; id++) {
        host_2a0(id, fs98_handle(t), 0x30);
        if (host_2bc(s, fs98_handle(t)) == 0) {
            M32(out) = id;
            return 0;
        }
    }
    return (uint32_t)-1;
}

/* +0x1c8 (0x40bcc0): describe joystick idx into `out`: its OEM name at +0x10
 * (0x40 bytes), axes at +4, buttons at +8, 1 at +0xc if it has a rudder
 * (JOYCAPS_HASR), and at +0 the id of the language DLL's string naming it --
 * by its OEM name (with "_r" if it has a rudder), else by "Generic_<axes>"
 * ("Generic_2_r" for three axes and a rudder, "Generic_<axes>_r" otherwise).
 * An empty OEM name becomes the string 0x73 formatted with the axes.  0, or
 * -1 if nothing matches or the registry could not be opened. */
static uint32_t FS98_STDCALL host_1c8(uint32_t idx, uint32_t out)
{
    char name[0x40], caps[0x194], rname[0x40], gen[0x40];
    uint32_t r = host_joy_oem(idx, fs98_handle(caps), fs98_handle(name)), rudder, axes, t;
    if (r != 0 && r != (uint32_t)-1)
        return (uint32_t)-1;
    host_2ac(out + 0x10, fs98_handle(name), 0x40);
    sprintf(rname, (const char *)(uintptr_t)V(0x418264), name);
    rudder = *(uint32_t *)(caps + 0x60) & 2;
    M32(out + 0xc) = rudder ? 1 : 0;
    axes = *(uint32_t *)(caps + 0x68);
    M32(out + 4) = axes;
    M32(out + 8) = *(uint32_t *)(caps + 0x6c);
    if (r == 0) {
        if (host_joy_match(rudder ? fs98_handle(rname) : fs98_handle(name), out) == 0)
            return 0;
    }
    if (*(uint32_t *)(caps + 0x60) & 2) {
        if (axes == 3)
            sprintf(gen, (const char *)(uintptr_t)V(0x418254), 2);
        else
            sprintf(gen, (const char *)(uintptr_t)V(0x418254), axes);
    } else {
        sprintf(gen, (const char *)(uintptr_t)V(0x418248), axes);
    }
    if (M8(out + 0x10) == 0) {
        host_2a4(0x73, fs98_handle(&t));
        sprintf((char *)(uintptr_t)(out + 0x10), (const char *)(uintptr_t)t, axes);
        host_214(fs98_handle(&t));
    }
    return host_joy_match(fs98_handle(gen), out);
}

/* +0x1cc (0x40be90): wait (up to 5 s by timeGetTime) for joystick idx to
 * move: with mode 0 or 2 a button or POV change gives 0x11 + its index, with
 * 0 or 1 an axis moved more than 5000 gives 1 + its index, into *out (0 on
 * timeout).  Returns what eax held: that code, the axis's movement, or out. */
static uint32_t FS98_STDCALL host_1cc(uint32_t idx, uint32_t mode, uint32_t out)
{
    uint32_t a[0x53], b[0x53], start, i;
    int32_t d;
    host_218(fs98_handle(a), 0, 0x14c);
    a[2] = 0xffff;
    host_1c4(idx, fs98_handle(a));
    start = timeGetTime();
    M32(out) = 0;
    for (;;) {
        host_218(fs98_handle(b), 0, 0x14c);
        b[2] = 0xffff;
        host_1c4(idx, fs98_handle(b));
        if (mode == 0 || mode == 2)
            for (i = 0; i < 0x40; i++)
                if (a[0x13 + i] != b[0x13 + i]) {
                    M32(out) = i + 0x11;
                    return i + 0x11;
                }
        if (mode == 0 || mode == 1)
            for (i = 0; i < 0x10; i++) {
                d = (int32_t)(a[3 + i] - b[3 + i]);
                if (d < 0)
                    d = -d;
                if (d > 0x1388) {
                    M32(out) = i + 1;
                    return (uint32_t)d;
                }
            }
        if (timeGetTime() - start > 0x1388) {
            M32(out) = 0;
            return out;
        }
    }
}

/* +0x208 (0x40ca90): fold the numeric keypad onto the keys it doubles as:
 * VK_NUMPAD0..9 become Insert, End, Down, PageDown, Left, Clear, Right,
 * Home, Up, PageUp, and VK_DECIMAL Delete, each setting bit 1 of *flags; an
 * Enter with *extended set becomes 0x87 (the host's "Num Enter").  The
 * second argument is unused.  Returns leftover eax: flags for a folded key,
 * else key. */
static uint32_t FS98_STDCALL host_208(uint32_t key, uint32_t unused, uint32_t flags, uint32_t extended)
{
    static const uint8_t pad[10] = { 0x2d, 0x23, 0x28, 0x22, 0x25, 0x0c, 0x27, 0x24, 0x26, 0x21 };
    uint32_t vk = M32(key);
    (void)unused;
    if (vk >= 0x60 && vk <= 0x69) {
        M32(key) = pad[vk - 0x60];
    } else if (vk == 0x6e) {
        M32(key) = 0x2e;
    } else {
        if (vk == 0x0d && M32(extended) != 0)
            M32(key) = 0x87;    /* RETAIL BUG (kept): HOST-16 -- *flags is not set */
        return key;
    }
    M32(flags) |= 1;
    return flags;
}

/* +0x170 (0x40aff0): the length *len of path + extension made absolute: a
 * path with a drive is taken as it is; otherwise its directory is joined
 * with a dummy name ("a.x", 0x4180dc), made absolute by _fullpath, split
 * again and joined with the original name.  0, or -1 with *len -1. */
static uint32_t FS98_STDCALL host_170(uint32_t path, uint32_t ext, uint32_t len)
{
    char drive[0x100], dir[0x100], name[0x100], joined[0x400], full[0x208];
    uint32_t file = 0;
    char dummy[4];
    *(uint32_t *)dummy = M32(V(0x4180dc));
    if (host_path_ext(fs98_handle(&file), path, ext) != 0)
        goto fail;
    if (host_188(path, fs98_handle(drive), fs98_handle(dir), fs98_handle(name), 0) != 0)
        goto fail;
    if (drive[0] != 0) {
        M32(len) = host_2b8(file);
        host_214(fs98_handle(&file));
        return 0;
    }
    if (host_184(fs98_handle(joined), fs98_handle(drive), fs98_handle(dir), fs98_handle(dummy)) != 0)
        goto fail;
    if (_fullpath(full, joined, 0x208) == 0)
        goto fail;
    if (host_188(fs98_handle(full), fs98_handle(drive), fs98_handle(dir), 0, 0) != 0)
        goto fail;
    if (host_184(fs98_handle(joined), fs98_handle(drive), fs98_handle(dir), fs98_handle(name)) != 0)
        goto fail;
    if (file != 0)
        host_214(fs98_handle(&file));
    if (host_path_ext(fs98_handle(&file), fs98_handle(joined), ext) != 0)
        goto fail;
    M32(len) = host_2b8(file);
    host_214(fs98_handle(&file));
    return 0;
fail:
    M32(len) = 0xffffffffu;
    if (file != 0)
        host_214(fs98_handle(&file));
    return (uint32_t)-1;
}

/* +0x030 (0x401a10): the counterpart of +0x2c -- the purge hook told
 * (0x20013, 0), the flag at 0x4187ec cleared.  Returns the hook's result (0
 * without one). */
static uint32_t FS98_STDCALL host_030(void)
{
    uint32_t r = (uint32_t)fs98_host_rt_hook(0x20013, 0);
    M32(V(0x4187ec)) = 0;
    return r;
}

/* +0x050 (0x408760): the busy cursor on (+0x2c, the hourglass shown) or off
 * (hidden, the arrow, +0x30), remembered at 0x41885c; nothing if unchanged.
 * Returns leftover eax: the argument when unchanged, else ShowCursor's
 * count turning on, +0x30's result turning off. */
static uint32_t FS98_STDCALL host_050(uint32_t on)
{
    if (M32(V(0x41885c)) == on)
        return on;
    M32(V(0x41885c)) = on;
    if (on != 0) {
        host_02c();
        SetCursor(LoadCursorA(0, IDC_WAIT));
        return (uint32_t)ShowCursor(TRUE);
    }
    ShowCursor(FALSE);
    SetCursor(LoadCursorA(0, IDC_ARROW));
    return host_030();
}

/* ---- the mouse, the heaps, disk space, temporary names ----------------------- */

/* +0x274 (0x40db40): capture the mouse for the main window: remember the
 * cursor position (0x41ba30) and clip (0x41ba40), hide the cursor, park it at
 * 0x41c358, capture, clip to 0x41c590; 0x41881c set, `arg` kept at 0x418820.
 * -1 if already captured, else 0. */
static uint32_t FS98_STDCALL host_274(uint32_t arg)
{
    if (M32(V(0x41881c)) != 0)
        return (uint32_t)-1;
    GetCursorPos((POINT *)(uintptr_t)V(0x41ba30));
    GetClipCursor((RECT *)(uintptr_t)V(0x41ba40));
    ShowCursor(FALSE);
    SetCursorPos((int)M32(V(0x41c358)), (int)M32(V(0x41c35c)));
    SetCapture(HOST_HWND);
    GetCapture();
    ClipCursor((const RECT *)(uintptr_t)V(0x41c590));
    M32(V(0x41881c)) = 1;
    M32(V(0x418820)) = arg;
    return 0;
}

/* +0x278 (0x40dbc0): undo it.  -1 if not captured, else 0. */
static uint32_t FS98_STDCALL host_278(void)
{
    if (M32(V(0x41881c)) == 0)
        return (uint32_t)-1;
    M32(V(0x41881c)) = 0;
    M32(V(0x418820)) = 0;
    ClipCursor((const RECT *)(uintptr_t)V(0x41ba40));
    SetCursorPos((int)M32(V(0x41ba30)), (int)M32(V(0x41ba34)));
    ReleaseCapture();
    ShowCursor(TRUE);
    return 0;
}

/* +0x238 (0x40d080): 1 if every process heap validates, else 0. */
static uint32_t FS98_STDCALL host_238(void)
{
    HANDLE heaps[0x14];
    uint32_t n = GetProcessHeaps(0x14, heaps), r = 1, i;
    if ((int32_t)n <= 0)
        return 1;
    for (i = 0; i < n; i++)
        if (!HeapValidate(heaps[i], 0, 0))
            r = 0;
    return r;
}

/* +0x150 (0x40a8b0): free bytes on the drive of `path` (of the host's
 * current directory, 0x41bca0, if null) into the 64-bit *out --
 * sectors-per-cluster times bytes-per-sector in 32 bits, times the free
 * clusters.  0, or -1 (with *out 0). */
static uint32_t FS98_STDCALL host_150(uint32_t path, uint32_t out)
{
    char cwd[0x104], drive[0x104];
    DWORD spc, bps, free_, total;
    if (path == 0) {
        host_130(fs98_handle(cwd));
        host_188(fs98_handle(cwd), fs98_handle(drive), 0, 0, 0);
    } else {
        host_188(path, fs98_handle(drive), 0, 0, 0);
    }
    host_2b0(fs98_handle(drive), V(0x4180c4));
    M32(out + 4) = 0;
    M32(out) = 0;
    if (!GetDiskFreeSpaceA(drive, &spc, &bps, &free_, &total))
        return (uint32_t)-1;
    /* RETAIL BUG (kept): HOST-15 -- bps * spc in 32 bits before widening */
    *(uint64_t *)(uintptr_t)out = (uint64_t)(uint32_t)(bps * spc) * free_;
    return 0;
}

/* +0x174 (0x409de0): a temporary file name in directory `dir` (the host's
 * current directory if null) into out, without its ".tmp": GetTempFileNameA
 * makes the file, which is deleted again.  0, or -1. */
static uint32_t FS98_STDCALL host_174(uint32_t out, uint32_t dir)
{
    char cwd[0x104], drive[0x104], sub[0x104], name[0x104], path[0x104], prefix[2];
    prefix[0] = (char)M8(V(0x4187f8));
    prefix[1] = 0;
    if (dir == 0) {
        host_130(fs98_handle(cwd));
        host_188(fs98_handle(cwd), fs98_handle(drive), fs98_handle(sub), 0, 0);
        host_184(fs98_handle(path), fs98_handle(drive), fs98_handle(sub), 0);
    } else {
        host_188(dir, fs98_handle(drive), fs98_handle(sub), fs98_handle(name), 0);
        host_184(fs98_handle(path), fs98_handle(drive), fs98_handle(sub), fs98_handle(name));
    }
    if (GetTempFileNameA(path, prefix, 0, (char *)(uintptr_t)out) == 0)
        return (uint32_t)-1;
    DeleteFileA((const char *)(uintptr_t)out);
    M8(host_2b8(out) + out - 4) = 0;
    return 0;
}

/* ---- menus ------------------------------------------------------------------- */

/* The main menu is a tree of item arrays, each item 0x14 bytes: its parent's
 * id (word +0), its id (word +2; 0xffff ends an array, 1 is a separator), its
 * text (+4: a string id below 0x10000, else a pointer), flags (+8: 1
 * checked, 2 grayed), its submenu's items (+0xc) and HMENU (+0x10).  The
 * root is a one-item array at 0x418298 -- the menu bar, id 0 -- whose items
 * are at 0x4182a4 and HMENU at 0x4182a8; the menu the window has is at
 * 0x418810.  Every change rebuilds all the HMENUs from the tree. */
#define MENU_ROOT       V(0x418298)
#define MI_PARENT(i)    M16((i) + 0)
#define MI_ID(i)        M16((i) + 2)
#define MI_TEXT(i)      M32((i) + 4)
#define MI_FLAGS(i)     M32((i) + 8)
#define MI_SUB(i)       M32((i) + 0xc)
#define MI_HMENU(i)     M32((i) + 0x10)

/* 0x40d860: the item with `id` in the tree under `items` (depth first, an
 * item before its submenu), or 0. */
static uint32_t host_menu_find(uint32_t items, uint32_t id)
{
    uint32_t i = items, r;
    for (; MI_ID(i) != 0xffff; i += 0x14) {
        if (MI_ID(i) == (uint16_t)id)
            return i;
        if (MI_SUB(i) != 0 && (r = host_menu_find(MI_SUB(i), id)) != 0)
            return r;
    }
    return 0;
}

/* 0x40d8c0: the index of `id` in one array, or -1. */
static int32_t host_menu_index(uint32_t items, uint32_t id)
{
    int32_t n = 0;
    for (; MI_ID(items) != 0xffff; items += 0x14, n++)
        if (MI_ID(items) == (uint16_t)id)
            return n;
    return -1;
}

/* 0x40d900: the items in an array, its terminator included. */
static uint32_t host_menu_count(uint32_t items)
{
    uint32_t n = 0;
    for (; MI_ID(items) != 0xffff; items += 0x14)
        n++;
    return n + 1;
}

/* 0x40d930: a heap copy of an array (one level). */
static uint32_t host_menu_dup(uint32_t items)
{
    uint32_t size = host_menu_count(items) * 0x14, p;
    host_20c(1, size, fs98_handle(&p), 0);
    host_21c(p, items, size);
    return p;
}

/* The item's text into the host's buffer at 0x41c480. */
static void host_menu_text(uint32_t item)
{
    if (MI_TEXT(item) < 0x10000)
        host_2a0(MI_TEXT(item), V(0x41c480), 0x104);
    else
        host_2a8(V(0x41c480), MI_TEXT(item));
}

/* 0x40d690: a new HMENU into *hmenu for the array `items` (copied first, all
 * the way down, when `dup` is set, the copy going to *pitems) and its
 * submenus: a submenu as a popup (grayed with flag 2), id 1 as a separator,
 * the rest as items (grayed with 2, checked with 1). */
static void host_menu_build(uint32_t pitems, uint32_t hmenu, uint32_t items, uint32_t dup)
{
    uint32_t i;
    M32(hmenu) = fs98_handle(CreateMenu());
    if (dup != 0)
        items = host_menu_dup(items);
    M32(pitems) = items;
    for (i = items; MI_ID(i) != 0xffff; i += 0x14) {
        if (MI_SUB(i) != 0) {
            host_menu_build(i + 0xc, i + 0x10, MI_SUB(i), dup);
            host_menu_text(i);
            AppendMenuA((HMENU)(uintptr_t)M32(hmenu), (MI_FLAGS(i) & 2) ? 0x11 : 0x10,
                        MI_HMENU(i), (LPCSTR)(uintptr_t)V(0x41c480));
        } else if (MI_ID(i) == 1) {
            AppendMenuA((HMENU)(uintptr_t)M32(hmenu), MF_SEPARATOR, 0, 0);
        } else {
            uint32_t f = 0;
            host_menu_text(i);
            if (MI_FLAGS(i) & 2)
                f = 1;
            if (MI_FLAGS(i) & 1)
                f |= 8;
            AppendMenuA((HMENU)(uintptr_t)M32(hmenu), f, MI_ID(i), (LPCSTR)(uintptr_t)V(0x41c480));
        }
    }
}

/* 0x40d7b0: free a tree of arrays (the copies the host made), *pitems and
 * *phmenu nulled. */
static void host_menu_free(uint32_t pitems, uint32_t phmenu)
{
    uint32_t i;
    for (i = M32(pitems); MI_ID(i) != 0xffff; i += 0x14)
        if (MI_SUB(i) != 0)
            host_menu_free(i + 0xc, i + 0x10);
    host_214(pitems);
    M32(phmenu) = 0;
}

/* Give the main window the menu -- unless 0x418854 is set and 0x417198 is
 * too, or 0x418854 is clear and 0x418824 is set. */
static void host_menu_attach(uint32_t hmenu)
{
    uint32_t a = M32(V(0x418854));
    if (a != 0) {
        if (M32(V(0x417198)) != 0)
            return;
    } else if (M32(V(0x418824)) != 0) {
        return;
    }
    SetMenu(HOST_HWND, (HMENU)(uintptr_t)hmenu);
}

/* 0x40d800: rebuild every HMENU from the tree and attach the new bar. */
static void host_menu_rebuild(void)
{
    DestroyMenu((HMENU)(uintptr_t)M32(V(0x418810)));
    host_menu_build(V(0x4182a4), V(0x4182a8), M32(V(0x4182a4)), 0);
    M32(V(0x418810)) = M32(V(0x4182a8));
    host_menu_attach(M32(V(0x418810)));
}

/* 0x40d970: a new array -- items before `before` (or all, for 0xffff), the
 * n new items (their submenus built, and copied), the rest -- the old array
 * freed. */
static uint32_t host_menu_insert(uint32_t items, uint32_t n, uint32_t add, uint32_t before)
{
    uint32_t count = host_menu_count(items), p, k = 0, i;
    host_20c(1, (count + n) * 0x14, fs98_handle(&p), 0);
    while (MI_ID(items + k * 0x14) != before) {
        host_21c(p + k * 0x14, items + k * 0x14, 0x14);
        k++;
    }
    host_21c(p + k * 0x14, add, n * 0x14);
    for (i = k; (int32_t)i < (int32_t)(k + n); i++)
        if (MI_SUB(p + i * 0x14) != 0)
            host_menu_build(p + i * 0x14 + 0xc, p + i * 0x14 + 0x10, MI_SUB(p + i * 0x14), 1);
    host_21c(p + (k + n) * 0x14, items + k * 0x14, (count - k) * 0x14);
    host_214(fs98_handle(&items));
    return p;
}

/* +0x23c (0x40d0d0): make the menu bar from the definition `items` (copied).
 * -1 if there is one already. */
static uint32_t FS98_STDCALL host_23c(uint32_t items)
{
    if (M32(V(0x418810)) != 0)
        return (uint32_t)-1;
    host_menu_build(V(0x4182a4), V(0x4182a8), items, 1);
    M32(V(0x418810)) = M32(V(0x4182a8));
    host_menu_attach(M32(V(0x418810)));
    return 0;
}

/* +0x240 (0x40d140): take the menu off the window and destroy it.  -1 if
 * there is none. */
static uint32_t FS98_STDCALL host_240(void)
{
    if (M32(V(0x4182a4)) == 0)
        return (uint32_t)-1;
    if (IsWindow(HOST_HWND))
        SetMenu(HOST_HWND, 0);
    DestroyMenu((HMENU)(uintptr_t)M32(V(0x418810)));
    host_menu_free(V(0x4182a4), V(0x418810));
    return 0;
}

/* +0x244 (0x40d190): append an item (id, text, flags) to item `parent`'s
 * submenu.  -1 if there is no such item. */
static uint32_t FS98_STDCALL host_244(uint32_t parent, uint32_t id, uint32_t text, uint32_t flags)
{
    uint32_t item[5], p = host_menu_find(MENU_ROOT, parent), r;
    item[0] = (parent & 0xffff) | (id << 16);
    item[1] = text;
    item[2] = flags;
    item[3] = 0;
    item[4] = 0;
    if (p == 0)
        return (uint32_t)-1;
    /* RETAIL BUG (kept): HOST-17 -- an item with no submenu (MI_SUB 0): host_menu_count reads through null here */
    r = host_menu_insert(MI_SUB(p), 1, fs98_handle(item), 0xffff);
    MI_SUB(p) = r;
    if (r == 0)
        return (uint32_t)-1;
    host_menu_rebuild();
    return 0;
}

/* +0x248 (0x40d220): append the array `items` to item `id`'s submenu. */
static uint32_t FS98_STDCALL host_248(uint32_t id, uint32_t items)
{
    uint32_t p = host_menu_find(MENU_ROOT, id), r;
    if (p == 0)
        return (uint32_t)-1;
    /* RETAIL BUG (kept): HOST-17 -- an item with no submenu: counted through null here (+0x244's case) */
    r = host_menu_insert(MI_SUB(p), host_menu_count(items) - 1, items, 0xffff);
    MI_SUB(p) = r;
    if (r == 0)
        return (uint32_t)-1;
    host_menu_rebuild();
    return 0;
}

/* +0x24c (0x40d280): insert an item (new_id, text, flags) before item
 * `id`, in its parent's array. */
static uint32_t FS98_STDCALL host_24c(uint32_t id, uint32_t new_id, uint32_t text, uint32_t flags)
{
    uint32_t item[5], n = host_menu_find(MENU_ROOT, id), p, r;
    if (n == 0)
        return (uint32_t)-1;
    p = host_menu_find(MENU_ROOT, MI_PARENT(n));   /* RETAIL BUG (kept): HOST-17 -- the array is found through the item's own parent id */
    if (p == 0)
        return (uint32_t)-1;
    item[0] = MI_ID(p) | (new_id << 16);
    item[1] = text;
    item[2] = flags;
    item[3] = 0;
    item[4] = 0;
    r = host_menu_insert(MI_SUB(p), 1, fs98_handle(item), id & 0xffff);
    MI_SUB(p) = r;
    if (r == 0)
        return (uint32_t)-1;
    host_menu_rebuild();
    return 0;
}

/* +0x250 (0x40d340): insert the array `items` before item `id`. */
static uint32_t FS98_STDCALL host_250(uint32_t id, uint32_t items)
{
    uint32_t n = host_menu_find(MENU_ROOT, id), p, r;
    if (n == 0)
        return (uint32_t)-1;
    p = host_menu_find(MENU_ROOT, MI_PARENT(n));   /* RETAIL BUG (kept): HOST-17 -- as +0x24c */
    if (p == 0)
        return (uint32_t)-1;
    r = host_menu_insert(MI_SUB(p), host_menu_count(items) - 1, items, id & 0xffff);
    MI_SUB(p) = r;
    if (r == 0)
        return (uint32_t)-1;
    host_menu_rebuild();
    return 0;
}

/* +0x254 (0x40d3c0): item `id` grayed (bit 2) and checked (bit 1) as
 * `flags` says, in the tree and in its parent's HMENU by position; while
 * 0x4182c0 is set, the bar is redrawn -- and rebuilt first if the parent's
 * own parent id is 0. */
static uint32_t FS98_STDCALL host_254(uint32_t id, uint32_t flags)
{
    uint32_t n = host_menu_find(MENU_ROOT, id), p, e, how;
    int32_t idx;
    if (n == 0)
        return (uint32_t)-1;
    p = host_menu_find(MENU_ROOT, MI_PARENT(n));
    if (p == 0)
        return (uint32_t)-1;
    /* RETAIL BUG (kept): HOST-17 -- found through the item's own parent id; a wrong but resolvable one leaves idx -1, used unchecked */
    idx = host_menu_index(MI_SUB(p), id);
    e = MI_SUB(p) + (uint32_t)idx * 0x14;
    if (flags & 2) {
        MI_FLAGS(e) |= 2;
        how = MF_BYPOSITION | MF_GRAYED;
    } else {
        MI_FLAGS(e) &= ~2u;
        how = MF_BYPOSITION;
    }
    EnableMenuItem((HMENU)(uintptr_t)MI_HMENU(p), (UINT)idx, how);
    e = MI_SUB(p) + (uint32_t)idx * 0x14;
    if (flags & 1) {
        MI_FLAGS(e) |= 1;
        how = MF_BYPOSITION | MF_CHECKED;
    } else {
        MI_FLAGS(e) &= ~1u;
        how = MF_BYPOSITION;
    }
    CheckMenuItem((HMENU)(uintptr_t)MI_HMENU(p), (UINT)idx, how);
    if (M32(V(0x4182c0)) != 0) {
        if (MI_PARENT(p) == 0)      /* RETAIL BUG (kept): HOST-17 -- rebuilds the bar before redrawing */
            host_menu_rebuild();
        DrawMenuBar(HOST_HWND);
    }
    return 0;
}

/* +0x258 (0x40d4c0): item `id`'s flags into *out (0 and -1 if none). */
static uint32_t FS98_STDCALL host_258(uint32_t id, uint32_t out)
{
    uint32_t n;
    M32(out) = 0;
    n = host_menu_find(MENU_ROOT, id);
    if (n == 0)
        return (uint32_t)-1;
    M32(out) = MI_FLAGS(n);
    return 0;
}

/* +0x25c (0x40d4f0): remove item `id` from its parent's array (its submenu,
 * if any, is not freed). */
static uint32_t FS98_STDCALL host_25c(uint32_t id)
{
    uint32_t n = host_menu_find(MENU_ROOT, id), p, sub, fresh;
    int32_t idx;
    if (n == 0)
        return (uint32_t)-1;
    p = host_menu_find(MENU_ROOT, MI_PARENT(n));
    if (p == 0)
        return (uint32_t)-1;
    sub = p + 0xc;
    /* RETAIL BUG (kept): HOST-17 -- found through the item's own parent id, idx unchecked; and
     * the removed item's own submenu is never freed */
    idx = host_menu_index(M32(sub), id);
    host_20c(1, host_menu_count(M32(sub)) * 0x14 - 0x14, fs98_handle(&fresh), 0);
    if (fresh == 0)
        return (uint32_t)-1;
    host_21c(fresh, M32(sub), (uint32_t)idx * 0x14);
    host_21c(fresh + (uint32_t)idx * 0x14, M32(sub) + ((uint32_t)idx + 1) * 0x14,
             (host_menu_count(M32(sub)) - (uint32_t)idx - 1) * 0x14);
    host_214(sub);
    M32(sub) = fresh;
    host_menu_rebuild();
    return 0;
}

/* +0x260 (0x40d5e0): change item `id`'s id. */
static uint32_t FS98_STDCALL host_260(uint32_t id, uint32_t new_id)
{
    uint32_t n = host_menu_find(MENU_ROOT, id);
    if (n == 0)
        return (uint32_t)-1;
    MI_ID(n) = (uint16_t)new_id;
    host_menu_rebuild();
    return 0;
}

/* +0x264 (0x40d610): change item `id`'s text. */
static uint32_t FS98_STDCALL host_264(uint32_t id, uint32_t text)
{
    uint32_t n = host_menu_find(MENU_ROOT, id);
    if (n == 0)
        return (uint32_t)-1;
    MI_TEXT(n) = text;
    host_menu_rebuild();
    return 0;
}

/* +0x268 (0x40d640): is there an item `id`? */
static uint32_t FS98_STDCALL host_268(uint32_t id)
{
    return host_menu_find(MENU_ROOT, id) != 0 ? 1 : 0;
}

/* ---- modules ----------------------------------------------------------------- */

/* The modules the host knows are the list at 0x417b30 of 0x14-byte records:
 * name (_strdup'd), id (Linkage[0]), HMODULE, Linkage, reference count.  A
 * module is loaded while its count is above 0; loading it resolves its
 * ImportTable (pairs { id, slot } ending at id 0, each slot given the Linkage
 * of the module with that id, itself acquired) and calls its init (Linkage
 * +4, then cleared); the last release calls its deinit (+8), releases its
 * imports and frees the library. */
typedef void (*host_mod_fn)(void);


/* 0x408050's mirror for the imports (0x4080d0): release each. */


/* +0x26c (0x40da90): load the module path + extension (which must exist),
 * *out its Linkage.  0, or -1. */
static uint32_t FS98_STDCALL host_26c(uint32_t out, uint32_t path, uint32_t ext)
{
    uint32_t file;
    if (host_158(path, ext) != 0)
        return (uint32_t)-1;
    if (host_path_ext(fs98_handle(&file), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    if (fs98_host_rt_add_module((const char *)(uintptr_t)file, 0) == 0) {
        host_214(fs98_handle(&file));
        return (uint32_t)-1;
    }
    M32(out) = fs98_handle(fs98_host_rt_acquire(fs98_host_rt_module_named((const char *)(uintptr_t)file)));
    host_214(fs98_handle(&file));
    return 0;
}

/* +0x270 (0x40db10): release the module whose Linkage is *plink, and null
 * *plink.  Returns 0. */
static uint32_t FS98_STDCALL host_270(uint32_t plink)
{
    fs98_host_rt_module_with_linkage((uint32_t *)(uintptr_t)M32(plink));
    fs98_host_rt_release(fs98_host_rt_module_with_linkage((uint32_t *)(uintptr_t)M32(plink)));
    M32(plink) = 0;
    return 0;
}

/* +0x294 (0x40ec00): load a language module -- the host's directory
 * (0x41bca0), string 0x9052, `name`, ".fll" -- and put it first in line for
 * strings.  *out its HMODULE; 0, or -1 (and the line entry taken back). */
static uint32_t FS98_STDCALL host_294(uint32_t name, uint32_t out)
{
    uint32_t buf = V(0x41c480), e, h;
    host_2a8(buf, V(0x41bca0));
    host_2a0(0x9052, host_2b8(buf) + buf, 0x104 - host_2b8(buf));
    host_184(buf, 0, buf, name);
    host_2b0(buf, V(0x4182fc));
    e = host_2f8(HOST_STRMODS);
    h = fs98_handle(LoadLibraryA((LPCSTR)(uintptr_t)buf));
    M32(e) = h;
    if (h == 0) {
        host_300(HOST_STRMODS, e);
        M32(out) = 0;
        return (uint32_t)-1;
    }
    M32(out) = h;
    return 0;
}

/* ---- sound ------------------------------------------------------------------- */

/* A voice is a 0x16c-byte block: size (+0), the sound (+4, a heap block of
 * +8 bytes), its WAVEFORMATEX (+0xc), the voice it duplicates (+0x20), its
 * type (+0x24: 1/3 from memory, 2/4/5 from a .wav, 6/7 a duplicate; 4 has
 * no buffer), references (+0x28), the frequency set (+0x2c), pan (+0x30),
 * volume as given to DirectSound (+0x34), play state (+0x38/+0x3c), +0x44
 * set for a hardware buffer, the frequency multiplier (+0x48, a double), the
 * volume (+0x50, 0..10000), pan (+0x54), a callback and whether it is due
 * (+0x58/+0x5c), the IDirectSoundBuffer (+0x60), the file name (+0x64), and
 * the next voice (+0x168).  The list's head is a dummy at 0x41a258, whose
 * +0x168 is the pointer at 0x41a3c0; DirectSound is at 0x41a3c8, and
 * 0x41a3cc is set when sound is off.  Errors are the host's 0x900xx codes. */
#define SND_OFF         M32(V(0x41a3cc))
#define SND_DS          M32(V(0x41a3c8))
#define SND_HEAD        V(0x41a258)
#define COM(o, off)     (*(uint32_t *)(uintptr_t)(M32(o) + (off)))

typedef uint32_t (FS98_STDCALL *host_com1)(uint32_t);
typedef uint32_t (FS98_STDCALL *host_com2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *host_com3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *host_com4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *host_com5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *host_com6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *host_com8)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                           uint32_t, uint32_t, uint32_t);
typedef void (FS98_STDCALL *host_voice_cb)(uint32_t);

/* 0x40f5c0: 0 if v is on the voice list (the dummy head counts), else -1. */
static uint32_t host_voice_check(uint32_t v)
{
    uint32_t e;
    if (v == 0)
        return (uint32_t)-1;
    for (e = SND_HEAD; e != 0; e = M32(e + 0x168))
        if (e == v)
            return 0;
    return (uint32_t)-1;
}

/* 0x40f570: a DirectSound HRESULT as the host's error. */
static uint32_t host_snd_error(uint32_t hr)
{
    if ((int32_t)hr > (int32_t)0x88780032u) {
        if (hr == 0x88780046u)
            return 0x9000c;
        if (hr == 0x88780096u)
            return 0x90009;
        return 0x90004;
    }
    if (hr == 0x88780032u)
        return 0x9000a;
    if (hr == 0x80070057u)
        return 0x9000b;
    return 0x90004;
}

/* 0x40fb00: release a voice's buffer (a type 4 has none of its own). */
static void host_voice_unbuffer(uint32_t v)
{
    if (M32(v + 0x24) != 4 && M32(v + 0x60) != 0) {
        ((host_com1)(uintptr_t)COM(M32(v + 0x60), 8))(M32(v + 0x60));
        M32(v + 0x60) = 0;
    }
}

static uint32_t host_wav_load(uint32_t name, uint32_t unused, uint32_t pdata, uint32_t plen,
                              uint32_t fmt);

/* 0x408450: find RIFF chunk c[0] from the file's position, skipping others
 * (each skip a +0x194 seek, which refuses a size past the file's length);
 * c[1] its size.  1, or 0 at the end (only the count read is checked). */
static uint32_t host_wav_chunk(uint32_t f, uint32_t c)
{
    uint32_t h[2], got;
    host_19c(f, fs98_handle(h), 8, fs98_handle(&got));
    if (got != 8)
        return 0;
    while (h[0] != M32(c)) {
        if (host_194(f, h[1], FILE_CURRENT) != 0)
            return 0;
        host_19c(f, fs98_handle(h), 8, fs98_handle(&got));
        if (got != 8)
            return 0;
    }
    M32(c + 4) = h[1];
    return 1;
}

/* 0x408150: read a .wav (name, no extension added) into a new block *pdata
 * of *plen bytes and *fmt (a WAVEFORMATEX, made PCM): PCM is copied as it
 * is; anything else is converted to 16-bit PCM through MSACM32.  Returns
 * whether *pdata was made.  The second argument is unused. */
static uint32_t host_wav_load(uint32_t name, uint32_t unused, uint32_t pdata, uint32_t plen,
                              uint32_t fmt)
{
    uint32_t f = 0, riff[3], c[2], got, fmtbuf = 0, databuf = 0, prepared = 0;
    HACMSTREAM has = 0;
    ACMSTREAMHEADER hdr;
    (void)unused;
    M32(pdata) = 0;
    M32(plen) = 0;
    if (host_17c(fs98_handle(&f), name, 0, 0) != 0)
        goto out;
    if (host_19c(f, fs98_handle(riff), 0xc, fs98_handle(&got)) != 0 || got != 0xc)
        goto out;
    if (riff[0] != 0x46464952u || riff[2] != 0x45564157u)
        goto out;
    c[0] = 0x20746d66u;
    if (!host_wav_chunk(f, fs98_handle(c)))
        goto out;
    host_20c(1, c[1], fs98_handle(&fmtbuf), 0);
    if (host_19c(f, fmtbuf, c[1], fs98_handle(&got)) != 0 || got != c[1])
        goto out;
    c[0] = 0x61746164u;
    if (!host_wav_chunk(f, fs98_handle(c)))
        goto out;
    host_20c(1, c[1], fs98_handle(&databuf), 0);
    if (host_19c(f, databuf, c[1], fs98_handle(&got)) != 0 || got != c[1])
        goto out;
    M16(fmt) = 1;
    M16(fmt + 2) = M16(fmtbuf + 2);
    M32(fmt + 4) = M32(fmtbuf + 4);
    M16(fmt + 0x10) = 0;
    if (M16(fmtbuf) == 1) {
        M32(fmt + 8) = M32(fmtbuf + 8);
        M16(fmt + 0xc) = M16(fmtbuf + 0xc);
        M16(fmt + 0xe) = M16(fmtbuf + 0xe);
        M32(plen) = c[1];
        host_20c(1, c[1], pdata, 0);
        host_21c(M32(pdata), databuf, M32(plen));
        goto out;
    }
    M16(fmt + 0xc) = (uint16_t)(((int32_t)M16(fmt + 2) << 4) / 8);
    M32(fmt + 8) = M16(fmt + 0xc) * M32(fmt + 4);
    M16(fmt + 0xe) = 0x10;
    if (acmStreamOpen(&has, 0, (LPWAVEFORMATEX)(uintptr_t)fmtbuf, (LPWAVEFORMATEX)(uintptr_t)fmt,
                      0, 0, 0, 0) != 0)
        goto out;
    if (acmStreamSize(has, c[1], (LPDWORD)(uintptr_t)plen, ACM_STREAMSIZEF_SOURCE) == 0) {
        host_20c(1, M32(plen), pdata, 0);
        host_218(fs98_handle(&hdr), 0, sizeof hdr);
        hdr.cbStruct = 0x54;
        hdr.pbSrc = (LPBYTE)(uintptr_t)databuf;
        hdr.cbSrcLength = c[1];
        hdr.pbDst = (LPBYTE)(uintptr_t)M32(pdata);
        hdr.cbDstLength = M32(plen);
        if (acmStreamPrepareHeader(has, &hdr, 0) != 0) {
            host_214(pdata);
        } else {
            prepared = 1;
            if (acmStreamConvert(has, &hdr, 0) != 0)
                host_214(pdata);
        }
    }
    if (prepared)
        acmStreamUnprepareHeader(has, &hdr, 0);
    if (has != 0)
        acmStreamClose(has, 0);
out:
    if (fmtbuf != 0)
        host_214(fs98_handle(&fmtbuf));
    if (databuf != 0)
        host_214(fs98_handle(&databuf));
    if (f != 0)
        host_13c(f);
    return M32(pdata) != 0;
}

/* 0x40f600: give a voice its DirectSound buffer -- a duplicate of its
 * source's for type 6, else a new one of its format (DSBCAPS_STATIC |
 * CTRLFREQUENCY | CTRLPAN | CTRLVOLUME, 0xe2) filled with its sound (a type
 * 5 reads its .wav now, and frees the sound once copied) -- volume and pan
 * set to 0, and +0x44 from the caps.  0, or an error (the buffer released). */
static uint32_t host_voice_buffer(uint32_t v)
{
    uint32_t ds = SND_DS, err = 0, p1 = 0, n1 = 0, p2 = 0, n2 = 0, hr, desc[5], caps[5], scratch;
    if (ds == 0 || v == 0 || M32(v + 0x24) == 4)
        return 0x90003;
    if (M32(v + 0x24) == 6) {
        hr = ((host_com3)(uintptr_t)COM(ds, 0x14))(ds, M32(M32(v + 0x20) + 0x60), v + 0x60);
    } else {
        desc[0] = 0x14;
        desc[1] = 0xe2;
        desc[2] = M32(v + 8);
        desc[3] = 0;
        desc[4] = v + 0xc;
        hr = ((host_com4)(uintptr_t)COM(ds, 0xc))(ds, fs98_handle(desc), v + 0x60, 0);
    }
    if (hr != 0) {
        err = 0x90003;
        M32(v + 0x60) = 0;
        goto cleanup;
    }
    if (M32(v + 0x24) != 6) {
        hr = ((host_com8)(uintptr_t)COM(M32(v + 0x60), 0x2c))(M32(v + 0x60), 0, M32(v + 8),
                                                             fs98_handle(&p1), fs98_handle(&n1), fs98_handle(&p2),
                                                             fs98_handle(&n2), 0);
        if (hr != 0) {
            err = host_snd_error(hr);
            goto cleanup;
        }
        if (M32(v + 0x24) == 5 && M32(v + 4) == 0) {
            if (host_wav_load(v + 0x64, fs98_handle(&scratch), v + 4, v + 8, v + 0xc) == 0)
                goto cleanup;
        }
        /* RETAIL BUG (kept): HOST-18 -- copies from the voice's sound, null for a type 7 duplicate of a buffered type 5 voice */
        host_21c(p1, M32(v + 4), M32(v + 8));
        if (M32(v + 0x24) == 5)
            host_214(v + 4);
        hr = ((host_com5)(uintptr_t)COM(M32(v + 0x60), 0x4c))(M32(v + 0x60), p1, M32(v + 8), 0, 0);
        if (hr != 0) {
            err = 0x90005;
            goto cleanup;
        }
        p1 = 0;
    }
    if (((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x3c))(M32(v + 0x60), 0) != 0) {
        err = 0x90006;
        goto cleanup;
    }
    if (((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x40))(M32(v + 0x60), 0) != 0) {
        err = 0x90007;
        goto cleanup;
    }
    caps[0] = 0x14;
    if (((host_com2)(uintptr_t)COM(M32(v + 0x60), 0xc))(M32(v + 0x60), fs98_handle(caps)) != 0) {
        err = 0x90008;
        goto cleanup;
    }
    M32(v + 0x44) = (caps[1] & 4) ? 1 : 0;
    return err;
cleanup:
    if (p1 != 0) {
        ((host_com5)(uintptr_t)COM(M32(v + 0x60), 0x4c))(M32(v + 0x60), p1, M32(v + 8), 0, 0);
        p1 = 0;
    }
    if (M32(v + 0x60) != 0) {
        ((host_com1)(uintptr_t)COM(M32(v + 0x60), 8))(M32(v + 0x60));
        M32(v + 0x60) = 0;
    }
    return err;
}

/* The frequency to play at: the voice's rate (+0x10, its nSamplesPerSec),
 * times the multiplier unless that is 0 (or NaN, which the x87 compare also
 * reads as equal), truncated (MSVCRT's _ftol). */
static uint32_t host_voice_freq(uint32_t v)
{
    /* In the x87 register, as retail: fild, fmul, then _ftol -- the product
     * is never rounded to a double before it is truncated. */
    double m = *(double *)(uintptr_t)(v + 0x48);
    long double f;
    if (m == 0.0 || m != m)
        f = (long double)(int64_t)(uint64_t)M32(v + 0x10);
    else
        f = (long double)(int64_t)(uint64_t)M32(v + 0x10) * (long double)m;
    return (uint32_t)(int64_t)f;
}

/* +0x2d4 (0x40f840): a new voice of `type` from src -- a { data, fmt } pair
 * for 1/3 (data's +0 the sound, +8 its length), a name (".wav" added) for
 * 2/4/5, a voice for 6/7 (not itself a duplicate, which gives *out 0 and 0)
 * -- buffered (types 1, 2, 5, 6, 7) and put first on the list; *out the
 * voice.  0, -1 with sound off, or an error (*out 0). */
static uint32_t FS98_STDCALL host_2d4(uint32_t src, uint32_t type, uint32_t out)
{
    uint32_t v = 0, err = 0, unused;
    if (SND_OFF == 1) {
        M32(out) = 0;
        return (uint32_t)-1;
    }
    if (type == 6 || type == 7) {
        if (host_voice_check(src) != 0 || M32(src + 0x24) == 6 || M32(src + 0x24) == 7)
            goto fail;
    }
    host_20c(1, 0x16c, fs98_handle(&v), 0);
    M32(out) = v;
    if (v == 0) {
        err = 0x90001;
        goto fail;
    }
    M32(v) = 0x16c;
    M32(v + 4) = 0;
    M32(v + 0x60) = 0;
    M32(v + 0x58) = 0;
    M32(v + 0x5c) = 0;
    M32(v + 0x20) = src;
    M32(v + 0x24) = type;
    M32(v + 0x28) = 1;
    M32(v + 0x50) = 10000;
    M32(v + 0x38) = 0;
    M32(v + 0x3c) = 0;
    M32(v + 0x40) = 0;
    M32(v + 0x44) = 0;
    M32(v + 0x48) = 0;
    M32(v + 0x4c) = 0;
    if (type == 2 || type == 4 || type == 5) {
        host_2a8(v + 0x64, src);
        host_2b0(v + 0x64, V(0x418374));
        if (!host_wav_load(v + 0x64, fs98_handle(&unused), v + 4, v + 8, v + 0xc)) {
            err = 0x90002;
            goto fail;
        }
    } else if (type == 1 || type == 3) {
        M8(v + 0x64) = 0;
        M32(v + 4) = M32(M32(src));
        M32(v + 8) = M32(M32(src) + 8);
        host_21c(v + 0xc, M32(src + 4), 0x12);
    } else if (type == 6 || type == 7) {
        /* RETAIL BUG (kept): HOST-18 -- copies the source's (possibly null) sound pointer: a type 7 duplicate of a type 5 voice, whose sound was freed once buffered */
        M32(v + 8) = M32(src + 8);
        host_21c(v + 0xc, src + 0xc, 0x12);
        M32(v + 4) = M32(src + 4);
    }
    if (type == 2 || type == 6 || type == 7 || type == 1 || type == 5) {
        err = host_voice_buffer(v);
        if (SND_DS != 0 && err != 0)
            goto fail;
    }
    M32(v + 0x168) = M32(V(0x41a3c0));
    M32(V(0x41a3c0)) = v;
    if (type == 6 || type == 7)
        M32(M32(v + 0x20) + 0x28) = M32(M32(v + 0x20) + 0x28) + 1;
    return err;
fail:
    if (v != 0) {
        /* RETAIL BUG (kept): HOST-18 -- a duplicate that fails to get a buffer frees the sound it shares with its source */
        if (M32(v + 4) != 0)
            host_214(v + 4);
        host_214(fs98_handle(&v));
    }
    M32(out) = 0;
    return err;
}

/* +0x2dc (0x40fb30): drop a voice (*pv nulled): a duplicate drops its
 * source too; the last reference takes it off the list and frees it with its
 * buffer and (unless a duplicate) its sound.  0 (with sound off too), or
 * 0x90019 for no such voice. */
static uint32_t FS98_STDCALL host_2dc(uint32_t pv)
{
    uint32_t v, e, prev, src;
    if (SND_OFF == 1)
        return 0;
    v = M32(pv);
    if (host_voice_check(v) != 0)
        return 0x90019;
    if (M32(v + 0x24) == 6) {
        src = M32(v + 0x20);
        host_2dc(fs98_handle(&src));
    }
    M32(v + 0x28) = M32(v + 0x28) - 1;
    if (M32(v + 0x28) == 0) {
        prev = v;
        for (e = SND_HEAD; e != 0 && e != v; e = M32(e + 0x168))
            prev = e;
        if (e == v)
            M32(prev + 0x168) = M32(v + 0x168);
        host_voice_unbuffer(v);
        if (M32(v + 0x24) != 6 && M32(v + 4) != 0)
            host_214(v + 4);
        host_214(fs98_handle(&v));
    }
    M32(pv) = 0;
    return 0;
}

/* +0x2d8 (0x40fc00): append to voice `v` the sound of `src` -- a .wav name
 * (kind 2, loaded as a type 4 voice and dropped after) or a voice (kind 6 or
 * 7) -- dropping v's buffer, and rebuilding it if `rebuild` is 1.  With src
 * and kind 0 and rebuild 1, just rebuild.  0 with no DirectSound or another
 * kind, -1 for a bad voice, else the load's or the rebuild's result. */
static uint32_t FS98_STDCALL host_2d8(uint32_t src, uint32_t kind, uint32_t v, uint32_t rebuild)
{
    uint32_t tmp = 0, add, block, r;
    if (SND_DS == 0)
        return 0;
    if (host_voice_check(v) != 0)
        return (uint32_t)-1;
    if (src == 0 && kind == 0 && rebuild == 1)
        return host_voice_buffer(v);
    if (kind == 2) {
        r = host_2d4(src, 4, fs98_handle(&tmp));
        if (r != 0)
            return r;
        add = tmp;
    } else if (kind == 7 || kind == 6) {
        if (host_voice_check(src) != 0)
            return (uint32_t)-1;
        add = src;
    } else {
        return 0;
    }
    host_20c(1, M32(v + 8) + M32(add + 8), fs98_handle(&block), 0);
    /* RETAIL BUG (kept): HOST-18 -- either sound may be null (a type 5 voice's, once buffered), copied from regardless */
    host_21c(block, M32(v + 4), M32(v + 8));
    host_21c(block + M32(v + 8), M32(add + 4), M32(add + 8));
    M32(v + 8) = M32(v + 8) + M32(add + 8);
    /* RETAIL BUG (kept): HOST-18 -- for a duplicate v this frees the sound it shares with its source */
    host_214(v + 4);
    M32(v + 4) = block;
    if (kind == 2)
        host_2dc(fs98_handle(&tmp));
    host_voice_unbuffer(v);
    if (rebuild == 1)
        return host_voice_buffer(v);
    return 0;
}

/* +0x2e0 (0x40fd50): play a voice (looping if `loop`) at its rate times the
 * multiplier (a double), with callback `cb` -- called now without
 * DirectSound, else when it finishes; a pending callback is called first.
 * A voice already playing is rewound instead.  0, -1 with sound off, or an
 * error. */
static uint32_t FS98_STDCALL host_2e0(uint32_t v, uint32_t loop, uint32_t m_lo, uint32_t m_hi, uint32_t cb)
{
    uint32_t status, b;
    if (SND_OFF == 1)
        return (uint32_t)-1;
    if (host_voice_check(v) != 0 || M32(v + 0x24) == 4)
        return 0x9001c;
    M32(v + 0x48) = m_lo;
    M32(v + 0x4c) = m_hi;
    M32(v + 0x38) = loop;
    if (M32(v + 0x58) != 0 && M32(v + 0x5c) != 0) {
        M32(v + 0x5c) = 0;
        ((host_voice_cb)(uintptr_t)M32(v + 0x58))(v);
    }
    if (cb != 0) {
        if (SND_DS == 0) {
            ((host_voice_cb)(uintptr_t)cb)(v);
        } else {
            M32(v + 0x5c) = 1;
            M32(v + 0x58) = cb;
        }
    }
    if (SND_DS == 0 || M32(v + 0x60) == 0)
        return 0;
    b = M32(v + 0x60);
    M32(v + 0x2c) = host_voice_freq(v);
    if (((host_com2)(uintptr_t)COM(b, 0x24))(b, fs98_handle(&status)) == 0 && (status & 1) == 1)
        return ((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x34))(M32(v + 0x60), 0) != 0 ? 0x9001b : 0;
    ((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x44))(M32(v + 0x60), M32(v + 0x2c));
    if (((host_com4)(uintptr_t)COM(M32(v + 0x60), 0x30))(M32(v + 0x60), 0, 0, loop != 0 ? 1 : 0) != 0)
        return 0x9001a;
    M32(v + 0x3c) = 1;
    return 0;
}

/* +0x2e4 (0x40fed0): a new frequency multiplier.  (With DirectSound, the
 * buffer is used unchecked -- a type 3 voice, which has none, faults.) */
static uint32_t FS98_STDCALL host_2e4(uint32_t v, uint32_t m_lo, uint32_t m_hi)
{
    if (SND_OFF == 1)
        return 0;
    if (host_voice_check(v) != 0 || M32(v + 0x24) == 4)
        return 0x9002d;
    M32(v + 0x48) = m_lo;
    M32(v + 0x4c) = m_hi;
    if (SND_DS == 0)
        return 0;
    M32(v + 0x2c) = host_voice_freq(v);
    /* RETAIL BUG (kept): HOST-18 -- the buffer is used unchecked */
    return ((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x44))(M32(v + 0x60), M32(v + 0x2c)) != 0 ? 0x9002c : 0;
}

/* +0x2ec (0x40ff80): the volume, 0..10000 (DirectSound gets it - 10000). */
static uint32_t FS98_STDCALL host_2ec(uint32_t v, uint32_t vol)
{
    uint32_t hr;
    if (SND_OFF == 1)
        return 0;
    if (host_voice_check(v) != 0 || M32(v + 0x24) == 4)
        return (uint32_t)-1;
    M32(v + 0x50) = vol;
    if (SND_DS == 0 || M32(v + 0x60) == 0)
        return 0;
    M32(v + 0x34) = vol - 10000;
    hr = ((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x3c))(M32(v + 0x60), vol - 10000);
    if (hr == 0)
        return 0;
    if (hr == 0x80070057u)
        return 0x90042;
    if (hr == 0x8878001eu)
        return 0x90041;
    return hr == 0x88780046u ? 0x90043 : 0x90040;
}

/* +0x2f0 (0x410020): the pan, -10000..10000.  (The buffer unchecked, as
 * +0x2e4.) */
static uint32_t FS98_STDCALL host_2f0(uint32_t v, uint32_t pan)
{
    uint32_t hr;
    if (SND_OFF == 1)
        return 0;
    if (host_voice_check(v) != 0 || M32(v + 0x24) == 4)
        return (uint32_t)-1;
    M32(v + 0x54) = pan;
    if (SND_DS == 0)
        return 0;
    M32(v + 0x30) = pan;
    /* RETAIL BUG (kept): HOST-18 -- the buffer is used unchecked */
    hr = ((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x40))(M32(v + 0x60), pan);
    if (hr == 0)
        return 0;
    if (hr == 0x80070057u)
        return 0x90052;
    if (hr == 0x8878001eu)
        return 0x90051;
    return hr == 0x88780046u ? 0x90053 : 0x90050;
}

/* +0x2f4 (0x410170): stop.  (The buffer unchecked, as +0x2e4.) */
static uint32_t FS98_STDCALL host_2f4(uint32_t v)
{
    if (SND_OFF == 1)
        return 0;
    if (host_voice_check(v) != 0 || M32(v + 0x24) == 4)
        return 0x9003e;
    M32(v + 0x3c) = 0;
    M32(v + 0x38) = 0;
    if (SND_DS == 0)
        return 0;
    /* RETAIL BUG (kept): HOST-18 -- the buffer is used unchecked */
    return ((host_com1)(uintptr_t)COM(M32(v + 0x60), 0x48))(M32(v + 0x60)) != 0 ? 0x9003d : 0;
}

/* +0x2e8 (0x4100b0): a voice's state (the primary buffer's for null) into
 * a 0x24-byte block: size, flags (the DirectSound state + 1 in the low
 * nibble; 0x100 with none, 0x20 playing, 0x40 looping, 0x80 stopped or no
 * status), rate, frequency, looping, playing, volume.  Returns 0. */
static uint32_t FS98_STDCALL host_2e8(uint32_t v, uint32_t st)
{
    uint32_t status, f;
    M32(st) = 0x24;
    f = (M32(V(0x418308)) + 1) & 0xf;
    M32(st + 4) = f;
    if (SND_DS == 0) {
        M32(st + 4) = f | 0x100;
        return 0;
    }
    if (v == 0)
        v = SND_HEAD;
    /* RETAIL BUG (kept): HOST-18 -- the voice (the list's head if null) and its buffer are used unchecked */
    if (((host_com2)(uintptr_t)COM(M32(v + 0x60), 0x24))(M32(v + 0x60), fs98_handle(&status)) != 0) {
        M32(st + 4) |= 0x80;
    } else {
        M32(v + 0x3c) = 0;
        M32(st + 0x20) = 0;
        M32(v + 0x38) = 0;
        M32(st + 0x1c) = 0;
        if (status == 2) {
            M32(st + 4) |= 0x80;
        } else {
            if (status == 1) {
                M32(v + 0x3c) = 1;
                M32(st + 0x20) = 1;
                M32(st + 4) |= 0x20;
            }
            if (status == 4) {
                M32(v + 0x38) = 1;
                M32(st + 0x1c) = 1;
                M32(st + 4) |= 0x40;
            }
        }
    }
    M32(st + 0xc) = M32(v + 0x10);
    M32(st + 0x10) = M32(v + 0x2c);
    M32(st + 0x18) = M32(v + 0x34) + 10000;
    return 0;
}

/* +0x2d0 (0x40f120): sound on (DirectSound started if it is not, every voice
 * rebuffered and the looping ones restarted) or off (due callbacks called,
 * buffers released, DirectSound released, 0x418308 -1), under the busy
 * cursor.  Returns 0. */
static uint32_t FS98_STDCALL host_2d0(uint32_t on)
{
    uint32_t v, next;
    if (SND_OFF == 1)
        return 0;
    host_050(1);
    if (on != 0) {
        if (M32(V(0x418308)) == 0)
            goto done;
        fs98_host_rt_start_sound();
        if (M32(V(0x418308)) != 0)
            goto failed;
        for (v = M32(V(0x41a3c0)); v != 0; v = M32(v + 0x168)) {
            host_voice_buffer(v);
            if (M32(v + 0x38) != 0) {
                host_2ec(v, M32(v + 0x50));
                host_2e0(v, M32(v + 0x38), M32(v + 0x48), M32(v + 0x4c), M32(v + 0x58));
            }
        }
        host_050(0);
        return 0;
    }
    for (v = M32(V(0x41a3c0)); v != 0; v = next) {
        next = M32(v + 0x168);
        if (M32(v + 0x5c) != 0 && M32(v + 0x58) != 0) {
            M32(v + 0x5c) = 0;
            ((host_voice_cb)(uintptr_t)M32(v + 0x58))(v);
        }
        if (host_voice_check(v) == 0)
            host_voice_unbuffer(v);
    }
    if (SND_DS != 0) {
        ((host_com1)(uintptr_t)COM(SND_DS, 8))(SND_DS);
        SND_DS = 0;
    }
failed:
    M32(V(0x418308)) = 0xffffffffu;
done:
    host_050(0);
    return 0;
}

/* ---- abbreviation ------------------------------------------------------------ */

/* +0x2c4 (0x40f000): src into dst, shortened to fit `len` bytes (with its
 * terminator) by keeping characters alternately from its end and its start
 * with "..." (0x418304) between: a string that fits is copied whole (0);
 * -1 if len leaves no room past the "...", and -1 too, leaking the working
 * copy (from +0x20c, flags 5, zeroed), whenever it does shorten. */
static uint32_t FS98_STDCALL host_2c4(uint32_t dst, uint32_t src, uint32_t len)
{
    uint32_t srclen = host_2b8(src), suf = host_2b8(V(0x418304)), tmp, left, last, i = 0, end;
    if (len > srclen) {
        host_2a8(dst, src);
        return 0;
    }
    if (len <= suf)
        return (uint32_t)-1;
    if (host_20c(5, len, fs98_handle(&tmp), 1) != 0)
        return (uint32_t)-1;
    left = len;
    last = len - 1;
    if (left > suf) {
        end = src + srclen;
        for (;;) {
            M8(tmp + last - i) = M8(end);
            left--;
            if (left <= suf)
                break;
            left--;
            M8(tmp + i) = M8(src + i);
            i++;
            end--;
            if (left <= suf)
                break;
        }
    }
    if (host_21c(tmp + i, V(0x418304), suf) != 0)
        return (uint32_t)-1;
    /* Retail tests strcpy's result the wrong way round: the copy is never
     * null, so this always returns -1 -- and never frees its working copy. */
    if (host_2a8(dst, tmp) != 0)    /* RETAIL BUG (kept): HOST-14 */
        return (uint32_t)-1;
    return host_214(fs98_handle(&tmp)) != 0 ? (uint32_t)-1 : 0;
}

/* ---- view-window surfaces and order ------------------------------------------ */

/* A view window's record (window.md) has, for the host: its group key
 * (+0x78), whether it draws (+0x7c), the drawing surface's bits, pitch, row
 * table (+0x98, +0x8c, +0x90), the DirectDraw surface while locked (+0x9c),
 * flags (+8: 0x2000 locked, 0x4000 keep the surface locked, 0x10000 and
 * 0x40000000 no DirectDraw, 0x80000000 a frame of the main window), and its
 * host block (+0xa0, 0x90 bytes: frame and client HWNDs +0x24/+0x28, the DIB
 * bits and pitch +0x1c/+0x20, its bitmap +0x30 and DC +0x38, the dirty-rect
 * list +0x54 and another list +0x70).  Windows are grouped for z-order in a
 * list at 0x41a3dc of { key, nodes, next } groups of { record, next } nodes. */
#define HOST_SURFACE    M32(V(0x41a4e0))

/* The conditions under which a record draws through DirectDraw. */
static int host_view_dd(uint32_t rec)
{
    uint32_t f = M32(rec + 8);
    if (HOST_SURFACE == 0 || M32(V(0x418858)) != 1 || M32(V(0x41a250)) != 0)
        return 0;
    if (f & 0x10000)
        return 0;
    if ((int32_t)M32(V(0x4183b4)) > 1 && (f & 0x40000000))
        return 0;
    return 1;
}

/* +0x06c (0x410740): lock a view window for drawing: its DIB's bits and
 * pitch, or DirectDraw's when it draws through it (the surface noted at
 * +0x9c), the row table refilled if the bits moved.  0 (nothing for a window
 * that does not draw), or -1 for null or one already locked. */
static uint32_t FS98_STDCALL host_06c(uint32_t rec)
{
    uint32_t block, bits, pitch, bm[6];
    int32_t i;
    if (rec == 0)
        return (uint32_t)-1;
    block = M32(rec + 0xa0);
    if (M32(rec + 0x7c) == 0)
        return 0;
    bits = M32(block + 0x1c);   /* RETAIL BUG (kept): HOST-19 -- the host block is read unchecked (as +0x068's and the restack's) */
    pitch = M32(block + 0x20);
    if (M32(rec + 8) & 0x2000)
        return (uint32_t)-1;
    if (host_view_dd(rec)) {
        if (M32(rec + 8) & 0x4000)
            M32(rec + 0x9c) = HOST_SURFACE;
        else if (fs98_host_rt_lock((void *)(uintptr_t)HOST_SURFACE, (void **)(uintptr_t)fs98_handle(&bits), (int32_t *)(uintptr_t)fs98_handle(&pitch)))
            M32(rec + 0x9c) = HOST_SURFACE;
    }
    M32(rec + 0x98) = bits;
    M32(rec + 8) |= 0x2000;
    M32(rec + 0x8c) = pitch;
    if (M32(M32(rec + 0x90)) == bits)   /* RETAIL BUG (kept): HOST-19 -- the row table is refilled only when its first row moved */
        return 0;
    GetObjectA((HGDIOBJ)(uintptr_t)M32(block + 0x30), 0x18, bm);
    for (i = 0; i < (int32_t)bm[2]; i++) {
        M32(M32(rec + 0x90) + 4 * (uint32_t)i) = bits;
        bits += pitch;
    }
    return 0;
}

/* +0x070 (0x410860): unlock it.  0, or -1 for null or one not locked. */
static uint32_t FS98_STDCALL host_070(uint32_t rec)
{
    if (rec == 0)
        return (uint32_t)-1;
    if (M32(rec + 0x7c) == 0)
        return 0;
    if (!(M32(rec + 8) & 0x2000))
        return (uint32_t)-1;
    if (host_view_dd(rec) && !(M32(rec + 8) & 0x4000))
        ((host_com2)(uintptr_t)COM(HOST_SURFACE, 0x80))(HOST_SURFACE, 0);
    M32(rec + 0x9c) = 0;
    M32(rec + 8) &= ~0x2000u;
    return 0;
}

/* +0x074 (0x410aa0): give record `rec` a new host block (zeroed; the old one,
 * if any, is not freed) with frame window `hwnd` and its two lists set up,
 * and tell the frame 0x467.  0, or -1 for a null record. */
static uint32_t FS98_STDCALL host_074(uint32_t hwnd, uint32_t rec)
{
    uint32_t b;
    if (rec == 0)
        return (uint32_t)-1;
    host_20c(1, 0x90, fs98_handle(&b), 1);
    M32(rec + 0xa0) = b;    /* RETAIL BUG (kept): HOST-19 -- the old block is not freed */
    M32(b + 0x54) = 0;
    M32(b + 0x58) = 0;
    M32(b + 0x5c) = 0x20;
    M32(b + 0x60) = 0x10;
    M32(b + 0x64) = 0;
    M32(b + 0x68) = 0;
    M32(b + 0x6c) = 0;
    M32(b + 0x70) = 0;
    M32(b + 0x74) = 0;
    M32(b + 0x78) = 0x20;
    M32(b + 0x7c) = 0x14;
    M32(b + 0x80) = 0;
    M32(b + 0x84) = 0;
    M32(b + 0x88) = 0;
    M32(b + 0x24) = hwnd;
    SendMessageA((HWND)(uintptr_t)hwnd, 0x467, 0, (LPARAM)rec);
    return 0;
}

/* +0x078 (0x410b80): free a record's drawing resources and host block. */
static uint32_t FS98_STDCALL host_078(uint32_t rec)
{
    uint32_t block;
    if (rec == 0)
        return 0;
    block = M32(rec + 0xa0);
    if (M32(rec + 0x98) != 0) {
        DeleteObject((HGDIOBJ)(uintptr_t)M32(block + 0x30));
        DeleteDC((HDC)(uintptr_t)M32(block + 0x38));
        M32(rec + 0x98) = 0;
    }
    if (M32(rec + 0x90) != 0) {
        host_214(rec + 0x90);
        M32(rec + 0x90) = 0;
    }
    if (M32(rec + 0xa0) != 0) {
        host_2fc(block + 0x54);
        host_2fc(block + 0x70);
        host_214(rec + 0xa0);
    }
    return 0;
}

/* +0x090 (0x411560): bring a record to the front of its group (and restack
 * if it moved).  0, or -1 if it is in no group. */
static uint32_t FS98_STDCALL host_090(uint32_t rec)
{
    uint32_t g, n, prev;
    if (rec == 0 || M32(V(0x41a3dc)) == 0)
        return (uint32_t)-1;
    for (g = M32(V(0x41a3dc)); g != 0; g = M32(g + 8)) {
        if (M32(g) != M32(rec + 0x78))
            continue;
        for (prev = 0, n = M32(g + 4); n != 0; prev = n, n = M32(n + 4)) {
            if (M32(n) != rec)
                continue;
            if (prev != 0) {
                M32(prev + 4) = M32(n + 4);
                M32(n + 4) = M32(g + 4);
                M32(g + 4) = n;
                fs98_host_rt_restack();
            }
            return 0;
        }
    }
    return (uint32_t)-1;
}

/* 0x411900: take a record out of its group, dropping the group when it
 * empties. */
static void host_view_unlink(uint32_t rec)
{
    uint32_t g, pg = 0, n, prev, next;
    for (g = M32(V(0x41a3dc)); g != 0; pg = g, g = M32(g + 8)) {
        if (M32(g) != M32(rec + 0x78))
            continue;
        for (prev = 0, n = M32(g + 4); n != 0; prev = n, n = M32(n + 4)) {
            if (M32(n) != rec)
                continue;
            next = M32(n + 4);
            if (prev != 0) {
                M32(prev + 4) = next;
            } else if (next != 0) {
                M32(g + 4) = next;
            } else {
                if (pg != 0)
                    M32(pg + 8) = M32(g + 8);
                else
                    M32(V(0x41a3dc)) = M32(g + 8);
                host_214(fs98_handle(&g));
            }
            host_214(fs98_handle(&n));
            return;
        }
    }
}

/* 0x4108f0: a DC to draw a view record's window with, into *out: the
 * DirectDraw surface's (unlocking the record first if it is locked -- its
 * lock flag kept so the release relocks -- and clipped to and offset by the
 * block's rectangle when 0x41a4f4 is set and the record's +8 lacks
 * 0x20000000), else the block's memory DC with its bitmap selected (the old
 * object kept at 0x41a3d0).  0, or -1 (*out untouched) for null. */
static uint32_t host_view_getdc(uint32_t rec, uint32_t out)
{
    uint32_t block;
    if (rec == 0)
        return (uint32_t)-1;
    block = M32(rec + 0xa0);
    if (M32(rec + 0x7c) != 0 && host_view_dd(rec)) {
        if (M32(rec + 8) & 0x2000) {
            host_070(rec);
            M32(rec + 8) |= 0x2000;
        }
        M32(out) = 0;
        ((host_com2)(uintptr_t)COM(HOST_SURFACE, 0x44))(HOST_SURFACE, out);
        if (M32(V(0x41a4f4)) != 0 && !(M32(rec + 8) & 0x20000000)) {
            IntersectClipRect((HDC)(uintptr_t)M32(out), (int)M32(block + 0xc), (int)M32(block + 0x10),
                              (int)M32(block + 0x14), (int)M32(block + 0x18));
            SetViewportOrgEx((HDC)(uintptr_t)M32(out), (int)M32(block + 0xc), (int)M32(block + 0x10), 0);
        }
        return 0;
    }
    M32(V(0x41a3d0)) = fs98_handle(SelectObject((HDC)(uintptr_t)M32(block + 0x38), (HGDIOBJ)(uintptr_t)M32(block + 0x30)));
    M32(out) = M32(block + 0x38);
    return 0;
}

/* 0x4109f0: give back a DC from 0x4108f0 -- to the surface (relocking the
 * record if it was locked), or the old object back into the block's DC.
 * 0, or -1 for a null record or DC. */
static uint32_t host_view_releasedc(uint32_t rec, uint32_t dc)
{
    if (rec == 0 || dc == 0)
        return (uint32_t)-1;
    if (M32(rec + 0x7c) != 0 && host_view_dd(rec)) {
        ((host_com2)(uintptr_t)COM(HOST_SURFACE, 0x68))(HOST_SURFACE, dc);
        if (M32(rec + 8) & 0x2000) {
            M32(rec + 8) &= ~0x2000u;
            host_06c(rec);
        }
        return 0;
    }
    if (dc == M32(M32(rec + 0xa0) + 0x38))
        SelectObject((HDC)(uintptr_t)dc, (HGDIOBJ)(uintptr_t)M32(V(0x41a3d0)));
    return 0;
}

/* A colour for a view's DC: through the master palette (a COLORREF) for a
 * record whose +8 has 0x8000 in the 8-bit mode (0x418858 == 1), else a
 * palette index (0x10ff0000 | the low 16 bits). */
static uint32_t host_view_colour(uint32_t rec, uint32_t c)
{
    if ((M32(rec + 8) & 0x8000) && M32(V(0x418858)) == 1) {
        c = (c & 0xff) * 4;
        return M8(V(0x4171a4) + c) | (uint32_t)M8(V(0x4171a5) + c) << 8 | (uint32_t)M8(V(0x4171a6) + c) << 16;
    }
    return (c & 0xffff) | 0x10ff0000;
}

/* A font from the "face,height" string resource `id`. */
static uint32_t host_res_font(uint32_t id, int weight)
{
    char face[0x40], size[0x40], *p, *q;
    host_2a0(id, fs98_handle(face), 0x3f);
    for (p = face; *p != 0 && *p != ','; p = CharNextA(p))
        ;
    q = CharNextA(p);
    host_2a8(fs98_handle(size), fs98_handle(q));
    *CharPrevA(face, q) = 0;
    for (p = size; *p != 0 && *p != ','; p = CharNextA(p))
        ;
    *p = 0;
    return fs98_handle(CreateFontA(atoi(size), 0, 0, 0, weight, 0, 0, 0, 0, 4, 2, 2, 0, face));
}

/* +0x318 (0x411200): draw text in a view record's window, in a font by kind
 * (0 a 7-pixel default made afresh each call, 1 the stock fixed font, 2 and
 * 3 the plain and bold ones from string resources 0x10/0x11, made once), in
 * colour `fg` (-1: just measure, DT_CALCRECT) on `bg` (-1: transparent).
 * Another kind returns -1 without giving the DC back.  The seventh argument
 * is unused. */
static uint32_t FS98_STDCALL host_318(uint32_t rec, uint32_t rect, uint32_t fg, uint32_t bg, uint32_t text,
                                      uint32_t kind, uint32_t unused)
{
    uint32_t dc, font, old;
    (void)unused;
    host_view_getdc(rec, fs98_handle(&dc));
    if (kind == 0)      /* RETAIL BUG (kept): HOST-21 -- a new font every call, never deleted */
        font = fs98_handle(CreateFontA(-7, 0, 0, 0, 0, 0, 0, 0, 1, 6, 0, 0, 0, (LPCSTR)(uintptr_t)V(0x41839c)));
    else if (kind == 1)
        font = fs98_handle(GetStockObject(SYSTEM_FIXED_FONT));
    else if (kind == 2) {
        if (M32(V(0x41a3e0)) == 0)
            M32(V(0x41a3e0)) = host_res_font(0x10, 0x190);
        font = M32(V(0x41a3e0));
    } else if (kind == 3) {
        if (M32(V(0x41a3e4)) == 0)
            M32(V(0x41a3e4)) = host_res_font(0x11, 0x2bc);
        font = M32(V(0x41a3e4));
    } else
        return (uint32_t)-1;
    old = fs98_handle(SelectObject((HDC)(uintptr_t)dc, (HGDIOBJ)(uintptr_t)font));
    if (fg == (uint32_t)-1) {
        DrawTextA((HDC)(uintptr_t)dc, (LPCSTR)(uintptr_t)text, -1, (LPRECT)(uintptr_t)rect, 0x2410);
    } else {
        SetTextColor((HDC)(uintptr_t)dc, host_view_colour(rec, fg));
        if (bg != (uint32_t)-1) {
            SetBkMode((HDC)(uintptr_t)dc, OPAQUE);
            SetBkColor((HDC)(uintptr_t)dc, host_view_colour(rec, bg));
        } else
            SetBkMode((HDC)(uintptr_t)dc, TRANSPARENT);
        DrawTextA((HDC)(uintptr_t)dc, (LPCSTR)(uintptr_t)text, -1, (LPRECT)(uintptr_t)rect, 0x2010);
        GdiFlush();
    }
    SelectObject((HDC)(uintptr_t)dc, (HGDIOBJ)(uintptr_t)old);
    host_view_releasedc(rec, dc);
    return 0;
}

/* +0x08c (0x410d70): a label in a view record's window, at one of fifteen
 * places (0-2 right and 3-5 left on lines 1-3 from the top; 6-8 right and
 * 9-11 left on lines 1-3 up from the bottom -- of the window if its +8 has
 * 0x60000000, else of its top half; 12-14 centred, a line above, on and
 * below the middle), transparent, in `colour` over a shadow one pixel down
 * and right in palette colour 0 -- no shadow for a record whose +8 has
 * 0x10000, which also gets the text in capitals in the small default font
 * (0x41a3d8) outside code page 932; else the stock fixed one (0x41a3d4).
 * Both fonts are made once.  0, or -1 for a null record or another place
 * (then without giving the DC back).  The fifth argument is unused. */
static uint32_t FS98_STDCALL host_08c(uint32_t rec, uint32_t place, uint32_t colour, uint32_t text, uint32_t unused)
{
    char buf[0x80];
    uint32_t font, dc, old, len, prev;
    int32_t x, y, w, h, cx, cy;
    SIZE size;
    (void)unused;
    if (rec == 0)
        return (uint32_t)-1;
    if ((M32(rec + 8) & 0x10000) && GetACP() != 0x3a4) {
        lstrcpynA(buf, (LPCSTR)(uintptr_t)text, 0x80);
        CharUpperA(buf);
        text = fs98_handle(buf);
        if (M32(V(0x41a3d8)) == 0)
            M32(V(0x41a3d8)) = fs98_handle(CreateFontA(-7, 0, 0, 0, 0, 0, 0, 0, 1, 6, 0, 0, 0, (LPCSTR)(uintptr_t)V(0x41839c)));
        font = M32(V(0x41a3d8));
    } else {
        if (M32(V(0x41a3d4)) == 0)
            M32(V(0x41a3d4)) = fs98_handle(GetStockObject(SYSTEM_FIXED_FONT));
        font = M32(V(0x41a3d4));
    }
    host_view_getdc(rec, fs98_handle(&dc));
    old = fs98_handle(SelectObject((HDC)(uintptr_t)dc, (HGDIOBJ)(uintptr_t)font));
    SetBkMode((HDC)(uintptr_t)dc, TRANSPARENT);
    len = (uint32_t)lstrlenA((LPCSTR)(uintptr_t)text);
    GetTextExtentPoint32A((HDC)(uintptr_t)dc, (LPCSTR)(uintptr_t)text, (int)len, &size);
    w = (int32_t)M32(rec + 0x60);
    h = (int32_t)M32(rec + 0x64);
    cx = size.cx;
    cy = size.cy;
    switch (place) {
    case 0: x = w - cx - 4; y = 4; break;
    case 1: x = w - cx - 4; y = cy + 4; break;
    case 2: x = w - cx - 4; y = 2 * cy + 4; break;
    case 3: x = 4; y = 4; break;
    case 4: x = 4; y = cy + 4; break;
    case 5: x = 4; y = 2 * cy + 4; break;
    case 6: case 9:
        x = place == 6 ? w - cx - 4 : 4;
        y = (M32(rec + 8) & 0x60000000) ? h - cy - 4 : h / 2 - cy - 4;
        break;
    case 7: case 10:
        x = place == 7 ? w - cx - 4 : 4;
        y = (M32(rec + 8) & 0x60000000) ? h - 2 * cy - 4 : h / 2 - 2 * cy - 4;
        break;
    case 8: case 11:
        x = place == 8 ? w - cx - 4 : 4;
        y = (M32(rec + 8) & 0x60000000) ? h - 3 * cy - 4 : h / 2 - 3 * cy - 4;
        break;
    case 12: x = w / 2 - cx / 2; y = h / 2 - cy / 2 - cy; break;
    case 13: x = w / 2 - cx / 2; y = h / 2 - cy / 2; break;
    case 14: x = w / 2 - cx / 2; y = h / 2 + cy / 2; break;
    default: return (uint32_t)-1;
    }
    if (!(M32(rec + 8) & 0x10000)) {
        prev = SetTextColor((HDC)(uintptr_t)dc, host_view_colour(rec, 0));
        TextOutA((HDC)(uintptr_t)dc, x + 1, y + 1, (LPCSTR)(uintptr_t)text, (int)len);
        SetTextColor((HDC)(uintptr_t)dc, prev);
    }
    prev = SetTextColor((HDC)(uintptr_t)dc, host_view_colour(rec, colour));
    TextOutA((HDC)(uintptr_t)dc, x, y, (LPCSTR)(uintptr_t)text, (int)len);
    SetTextColor((HDC)(uintptr_t)dc, prev);
    GdiFlush();
    SelectObject((HDC)(uintptr_t)dc, (HGDIOBJ)(uintptr_t)old);
    host_view_releasedc(rec, dc);
    return 0;
}

/* +0x290 (0x40e290): set the palette.  The ten system colours at each end go
 * from the master palette (0x4171a4) to its copy (0x4175ac) and, as BGR, to
 * 0x41adc0; given RGB triples, entries 10..245 are set from them (pal the
 * master's, copy the copy's, flagged PC_RESERVED), the tinted table made,
 * DirectDraw given the master, and the GDI palette animated (and realised, by
 * day) or set.  Returns ReleaseDC's result, or 0 with no triples. */
static uint32_t FS98_STDCALL host_290(uint32_t pal, uint32_t copy)
{
    uint32_t i, k;
    HDC dc;
    M32(V(0x41ba2c))++;
    for (i = 0; i < 0x28; i += 4) {
        k = 0x3fc - i;
        M8(V(0x4175ac) + i) = M8(V(0x41adc2) + i) = M8(V(0x4171a4) + i);
        M8(V(0x4175ad) + i) = M8(V(0x41adc1) + i) = M8(V(0x4171a5) + i);
        M8(V(0x4175ae) + i) = M8(V(0x41adc0) + i) = M8(V(0x4171a6) + i);
        M8(V(0x4175ac) + k) = M8(V(0x41adc2) + k) = M8(V(0x4171a4) + k);
        M8(V(0x4175ad) + k) = M8(V(0x41adc1) + k) = M8(V(0x4171a5) + k);
        M8(V(0x4175ae) + k) = M8(V(0x41adc0) + k) = M8(V(0x4171a6) + k);
    }
    if (pal == 0)
        return 0;
    for (i = 0, k = 0x1e; i < 0x3b0; i += 4, k += 3) {
        M8(V(0x4171cc) + i) = M8(pal + k);
        M8(V(0x4171cd) + i) = M8(pal + k + 1);
        M8(V(0x4171ce) + i) = M8(pal + k + 2);
        M8(V(0x4171cf) + i) = 1;
        M8(V(0x4175d4) + i) = M8(copy + k);
        M8(V(0x4175d5) + i) = M8(copy + k + 1);
        M8(V(0x4175d6) + i) = M8(copy + k + 2);
        M8(V(0x4175d7) + i) = 1;
        M8(V(0x41adea) + i) = M8(pal + k);
        M8(V(0x41ade9) + i) = M8(pal + k + 1);
        M8(V(0x41ade8) + i) = M8(pal + k + 2);
    }
    fs98_host_rt_tint_palette();
    fs98_host_rt_set_palette((void *)(uintptr_t)V(0x4171a4));
    dc = GetDC(HOST_HWND);
    if (GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) {
        AnimatePalette((HPALETTE)(uintptr_t)M32(V(0x41bb60)), 10, 0xec, (const PALETTEENTRY *)(uintptr_t)V(0x4171cc));
        if (M32(V(0x41a250)) == 0 && M32(V(0x41a254)) == 0) {
            SelectPalette(dc, (HPALETTE)(uintptr_t)M32(V(0x41bb60)), FALSE);
            RealizePalette(dc);
            RedrawWindow(HOST_HWND, 0, 0, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    } else {
        SetPaletteEntries((HPALETTE)(uintptr_t)M32(V(0x41bb60)), 10, 0xec, (const PALETTEENTRY *)(uintptr_t)V(0x4171cc));
    }
    return (uint32_t)ReleaseDC(HOST_HWND, dc);
}

/* +0x094 (0x4115d0): the idx'th record (counting those whose +8 has its top
 * bit) in the group(s) of a key, into *out: 0, or -1 with *out 0.  Every
 * group of the key is walked, the count starting again in each. */
static uint32_t FS98_STDCALL host_094(uint32_t out, uint32_t key, uint32_t idx)
{
    uint32_t g, n, i;
    for (g = M32(V(0x41a3dc)); g != 0; g = M32(g + 8)) {
        if (M32(g) != key)
            continue;
        for (i = 0, n = M32(g + 4); n != 0; n = M32(n + 4)) {
            if (!(M32(M32(n) + 8) & 0x80000000u))
                continue;
            if (i == idx) {
                M32(out) = M32(n);
                return 0;
            }
            i++;
        }
    }
    M32(out) = 0;
    return (uint32_t)-1;
}

/* +0x128 (0x413620): clear the three words a block points at (if given),
 * then free the library at 0x41a51c if 0x41a518 says one is loaded: -1 if
 * that fails (both left), else 0 with both cleared. */
static uint32_t FS98_STDCALL host_128(uint32_t p)
{
    if (p != 0) {
        M32(M32(p)) = 0;
        M32(M32(p + 4)) = 0;
        M32(M32(p + 8)) = 0;
    }
    if (M32(V(0x41a518)) != 0 && !FreeLibrary((HMODULE)(uintptr_t)M32(V(0x41a51c))))
        return (uint32_t)-1;
    M32(V(0x41a518)) = 0;
    M32(V(0x41a51c)) = 0;
    return 0;
}

/* Constants from retail's .rdata (0x416120..0x41616f), which is not part of
 * the .data image: the display settings' profile section, and the IIDs and
 * device GUIDs DirectPlay, DirectDraw and Direct3D are asked about. */
static const char host_hardware[] = "HARDWARE";                                  /* 0x416120 */
static const uint8_t host_iid_d3d[16] = { 0xc1, 0x1e, 0xae, 0x6a, 0x2a, 0x66, 0xd0, 0x11,  /* 0x416130 */
                                          0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a };
static const uint8_t host_iid_dplay2a[16] = { 0x41, 0xfe, 0x3e, 0x13, 0xdc, 0x32, 0xd0, 0x11,  /* 0x416140 */
                                              0x9c, 0xfb, 0x00, 0xa0, 0xc9, 0x0a, 0x43, 0xcb };
static const uint8_t host_guid_mmx[16] = { 0xa1, 0x49, 0x19, 0x88, 0xf3, 0xd6, 0xd0, 0x11,  /* 0x416150 */
                                           0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29 };
static const uint8_t host_guid_hal[16] = { 0xe0, 0x3d, 0xe6, 0x84, 0xaa, 0x46, 0xcf, 0x11,  /* 0x416160 */
                                           0x81, 0x6f, 0x00, 0x00, 0xc0, 0x20, 0x15, 0x6e };

/* 0x413680: whether DirectPlay works through a loaded dplayx: a
 * DirectPlayCreate that succeeds and an object that answers QueryInterface
 * for IDirectPlay2A (0x416140).  1 or 0. */
static uint32_t host_dplay_works(uint32_t lib)
{
    uint32_t dp = 0, dp2 = 0, ok = 0;
    FARPROC create = GetProcAddress((HMODULE)(uintptr_t)lib, (LPCSTR)(uintptr_t)V(0x418440));
    if (create != 0 && ((host_com3)(uintptr_t)create)(V(0x41a508), fs98_handle(&dp), 0) == 0 &&
        ((host_com3)(uintptr_t)COM(dp, 0))(dp, fs98_handle(host_iid_dplay2a), fs98_handle(&dp2)) == 0)
        ok = 1;
    if (dp2 != 0)
        ((host_com1)(uintptr_t)COM(dp2, 8))(dp2);
    if (dp != 0)
        ((host_com1)(uintptr_t)COM(dp, 8))(dp);
    return ok;
}

/* +0x124 (0x413570): load dplayx (once: 0x41a518 the flag, 0x41a51c the
 * library) and, if DirectPlay works through it -- else it is freed again
 * through +0x128, -1 if that fails -- put DirectPlayCreate,
 * DirectPlayEnumerateA and DirectPlayLobbyCreateA through the block's three
 * pointers.  -1 if it cannot be loaded, else 0. */
static uint32_t FS98_STDCALL host_124(uint32_t p)
{
    if (M32(V(0x41a518)) == 0) {
        M32(V(0x41a51c)) = fs98_handle(LoadLibraryA((LPCSTR)(uintptr_t)V(0x418420)));
        if (M32(V(0x41a51c)) == 0)
            return (uint32_t)-1;
        M32(V(0x41a518)) = 1;
        if (!host_dplay_works(M32(V(0x41a51c))) && host_128(0) != 0)
            return (uint32_t)-1;
        if (M32(V(0x41a518)) == 0)
            return 0;
    }
    if (p != 0) {
        M32(M32(p)) = fs98_handle(GetProcAddress((HMODULE)(uintptr_t)M32(V(0x41a51c)), (LPCSTR)(uintptr_t)V(0x418440)));
        M32(M32(p + 4)) = fs98_handle(GetProcAddress((HMODULE)(uintptr_t)M32(V(0x41a51c)), (LPCSTR)(uintptr_t)V(0x418428)));
        M32(M32(p + 8)) = fs98_handle(GetProcAddress((HMODULE)(uintptr_t)M32(V(0x41a51c)), (LPCSTR)(uintptr_t)V(0x418408)));
    }
    return 0;
}

/* +0x120 (0x413260): the DirectPlay lobby's connection settings, if the game
 * was lobby-launched: *block and *size (both 0 first) get a new block -- the
 * DPLCONNECTION as the lobby gives it, or, with `flat`, flattened: the
 * connection, its session description, player name and address one after
 * the other followed by the session name and the player's short and long
 * names, the pointers among them replaced by offsets from the block's start
 * (the session's password pointer left as it was).  dplayx is loaded if it
 * is not (and freed again, 0x41a51c left set).  0, or -1 if a block cannot be
 * had or a copy fails; a lobby with nothing returns 0 without releasing it. */
static uint32_t FS98_STDCALL host_120(uint32_t block, uint32_t size, uint32_t flat)
{
    uint32_t loaded = 0, conn = 0, lobby = 0, n = 0, lib, l1, l2, l3, b, at, sess, player;
    FARPROC create;
    M32(size) = 0;
    M32(block) = 0;
    lib = M32(V(0x41a51c));
    if (lib == 0) {
        lib = fs98_handle(LoadLibraryA((LPCSTR)(uintptr_t)V(0x418420)));
        M32(V(0x41a51c)) = lib;
        loaded = 1;
        if (lib == 0)
            return 0;
    }
    create = GetProcAddress((HMODULE)(uintptr_t)lib, (LPCSTR)(uintptr_t)V(0x418408));
    if (create != 0 && ((host_com5)(uintptr_t)create)(0, fs98_handle(&lobby), 0, 0, 0) == 0 &&
        host_dplay_works(M32(V(0x41a51c)))) {
        ((host_com4)(uintptr_t)COM(lobby, 0x20))(lobby, 0, 0, fs98_handle(&n));
        if (n == 0)
            return 0;
        if (host_20c(1, n, fs98_handle(&conn), 1) != 0)
            return (uint32_t)-1;
        ((host_com4)(uintptr_t)COM(lobby, 0x20))(lobby, 0, conn, fs98_handle(&n));
    }
    if (lobby != 0)
        ((host_com1)(uintptr_t)COM(lobby, 8))(lobby);
    if (loaded)
        FreeLibrary((HMODULE)(uintptr_t)M32(V(0x41a51c)));
    if (n == 0)
        return 0;
    if (flat == 0) {
        M32(block) = conn;
        M32(size) = n;
        return 0;
    }
    M32(size) += 0x88;
    M32(size) += M32(conn + 0x24);
    l1 = host_2b8(M32(M32(conn + 8) + 0x30)) + 1;
    l2 = host_2b8(M32(M32(conn + 0xc) + 8)) + 1;
    l3 = host_2b8(M32(M32(conn + 0xc) + 0xc)) + 1;
    M32(size) += l3 + l2 + l1;
    at = M32(size);
    if (host_20c(1, at, block, 1) != 0)
        return (uint32_t)-1;
    b = at = M32(block);
    if (host_21c(at, conn, 0x28) != 0)
        return (uint32_t)-1;
    at += 0x28;
    sess = at - M32(block);
    M32(M32(block) + 8) = sess;
    if (host_21c(at, M32(conn + 8), 0x50) != 0)
        return (uint32_t)-1;
    at += 0x50;
    player = at - M32(block);
    M32(M32(block) + 0xc) = player;
    if (host_21c(at, M32(conn + 0xc), 0x10) != 0)
        return (uint32_t)-1;
    at += 0x10;
    M32(M32(block) + 0x20) = at - M32(block);
    if (host_21c(at, M32(conn + 0x20), M32(conn + 0x24)) != 0)
        return (uint32_t)-1;
    at += M32(conn + 0x24);
    M32(sess + M32(block) + 0x30) = at - M32(block);
    if (host_21c(at, M32(M32(conn + 8) + 0x30), l1) != 0)
        return (uint32_t)-1;
    at += l1;
    M32(player + M32(block) + 8) = at - M32(block);
    if (host_21c(at, M32(M32(conn + 0xc) + 8), l2) != 0)
        return (uint32_t)-1;
    at += l2;
    M32(player + M32(block) + 0xc) = at - M32(block);
    if (host_21c(at, M32(M32(conn + 0xc) + 0xc), l3) != 0)
        return (uint32_t)-1;
    (void)b;
    if (host_214(fs98_handle(&conn)) != 0)
        return (uint32_t)-1;
    return 0;
}

/* ---- display settings (+0x31c) -------------------------------------------
 *
 * The device list, the settings read (0x4121a0..0x412420, 0x414400..0x414510)
 * and DirectDraw itself are the runtime's (src/host/runtime/host_rt_ddraw.c and
 * host_rt_config.c), called from here: one copy, shared with the services.
 *
 * A settings block (0xe8 bytes): the device, as "<description>" or
 * "<description> (<name>)" for one named "\\.<...>", with an optional
 * " <Direct3D suffix>" (+0x00, 0x80 bytes); that suffix alone (+0x80); the
 * mode as "WxHxB" (+0xa8, 0x28 bytes); option flags (+0xd0: the table at
 * 0x418458 names them in the HARDWARE section; 0x10000000/0x20000000 mark a
 * Direct3D HAL / MMX device seen); the mode parsed (+0xd4 width, +0xd8
 * height, +0xdc depth, +0xe0 a fourth number -- or, when modes are listed,
 * the video memory); and where the next string of a list goes (+0xe4). */

/* 0x414440: an option flag back to the HARDWARE section: its key removed when
 * it has its default, else "1" or "0". */
static void host_cfg_flag_write(uint32_t cfg, uint32_t mask, uint32_t key, uint32_t def)
{
    uint32_t v = M32(cfg + 0xd0) & mask;
    if ((v != 0) == def)
        WritePrivateProfileStringA(host_hardware, (LPCSTR)(uintptr_t)key, 0, (LPCSTR)(uintptr_t)V(0x41c120));
    else
        WritePrivateProfileStringA(host_hardware, (LPCSTR)(uintptr_t)key,
                                   (LPCSTR)(uintptr_t)(v != 0 ? V(0x417064) : V(0x417a20)),
                                   (LPCSTR)(uintptr_t)V(0x41c120));
}

/* 0x414720 (ops 5 and 6): the settings back to the HARDWARE section, or (6)
 * the section removed; then the main window is posted 0x46f. */
static uint32_t host_display_write(uint32_t cfg, uint32_t clear)
{
    uint32_t e;
    if (clear) {
        WritePrivateProfileStringA(host_hardware, 0, 0, (LPCSTR)(uintptr_t)V(0x41c120));
    } else {
        WritePrivateProfileStringA(host_hardware, (LPCSTR)(uintptr_t)V(0x418594), (LPCSTR)(uintptr_t)cfg,
                                   (LPCSTR)(uintptr_t)V(0x41c120));
        WritePrivateProfileStringA(host_hardware, (LPCSTR)(uintptr_t)V(0x41858c), (LPCSTR)(uintptr_t)(cfg + 0xa8),
                                   (LPCSTR)(uintptr_t)V(0x41c120));
        if (M32(V(0x418458)) != 0)
            for (e = V(0x41845c); ; e += 0xc) {
                host_cfg_flag_write(cfg, M32(e - 4), M32(e), M32(e + 4));
                if (M32(e + 8) == 0)
                    break;
            }
    }
    PostMessageA(HOST_HWND, 0x46f, 0, 0);
    return 0;
}

/* Append a copy of a string to the settings' list. */
static void host_display_add(uint32_t cfg, uint32_t s)
{
    M32(M32(cfg + 0xe4)) = host_2b4(s);
    M32(cfg + 0xe4) += 4;
}

/* 0x4147c0, a Direct3D EnumDevices callback: a hardware (HAL) device with
 * hardware rasterisation marks 0x10000000 and, with Direct3D on (4), lists
 * "<device> (Direct3D)"; an MMX one, with MMX allowed (8), marks 0x20000000
 * and lists "<device> (Direct3D MMX)" if no HAL was seen.  1. */
static HRESULT WINAPI host_d3d_dev_cb(GUID *guid, LPSTR desc, LPSTR name, void *hal, void *hel, LPVOID ctx)
{
    char buf[0x80];
    uint32_t cfg = fs98_handle(ctx), f;
    (void)desc; (void)name; (void)hel;
    if (guid != 0 && memcmp(guid, host_guid_hal, 16) == 0 && hal != 0 && M32(fs98_handle(hal) + 4) != 0 &&
        (M8(fs98_handle(hal) + 0x84) & 1)) {
        M32(cfg + 0xd0) |= 0x10000000;
        if (M8(cfg + 0xd0) & 4) {
            wsprintfA(buf, (const char *)(uintptr_t)V(0x41859c), (const char *)(uintptr_t)cfg,
                      (const char *)(uintptr_t)V(0x4183fc));
            host_display_add(cfg, fs98_handle(buf));
        }
    }
    f = M32(cfg + 0xd0);
    if ((f & 8) && guid != 0 && memcmp(guid, host_guid_mmx, 16) == 0) {
        f |= 0x20000000;
        M32(cfg + 0xd0) = f;
        if ((f & 4) && !(f & 0x10000000)) {
            wsprintfA(buf, (const char *)(uintptr_t)V(0x41859c), (const char *)(uintptr_t)cfg,
                      (const char *)(uintptr_t)V(0x4183ec));
            host_display_add(cfg, fs98_handle(buf));
        }
    }
    return 1;
}

/* 0x4148f0, a DirectDrawEnumerateA callback: each driver that can be made,
 * labelled into the settings' device string; primary ones listed; then its
 * Direct3D devices.  1. */
static BOOL WINAPI host_dd_driver_cb(GUID *guid, LPSTR desc, LPSTR name, LPVOID ctx)
{
    uint32_t cfg = fs98_handle(ctx), d3d = 0, dd = 0;
    if (DirectDrawCreate(guid, (LPDIRECTDRAW *)&dd, 0) != 0)
        return TRUE;
    fs98_host_rt_device_name((char *)(uintptr_t)cfg, desc, name, 0);
    if (fs98_host_rt_device_is(guid, desc, name))
        host_display_add(cfg, cfg);
    ((host_com3)(uintptr_t)COM(dd, 0))(dd, fs98_handle(host_iid_d3d), fs98_handle(&d3d));
    if (d3d != 0) {
        ((host_com3)(uintptr_t)COM(d3d, 0xc))(d3d, fs98_handle(host_d3d_dev_cb), cfg);
        ((host_com1)(uintptr_t)COM(d3d, 8))(d3d);
    }
    ((host_com1)(uintptr_t)COM(dd, 8))(dd);
    return TRUE;
}

/* 0x4149a0 (op 3): the devices into the list (null-terminated), or
 * "display" alone when DirectDraw is disabled, under the busy cursor. */
static uint32_t host_display_devices(uint32_t cfg)
{
    host_050(1);
    M32(cfg + 0xd0) &= 0xcfffffff;
    if (!(M8(cfg + 0xd0) & 1)) {
        DirectDrawEnumerateA(host_dd_driver_cb, (LPVOID)(uintptr_t)cfg);
        M32(M32(cfg + 0xe4)) = 0;
        host_050(0);
        return 0;
    }
    M32(M32(cfg + 0xe4)) = host_2b4(V(0x4183c4));
    M32(M32(cfg + 0xe4) + 4) = 0;
    host_050(0);
    return 0;
}

/* 0x414a20, an EnumDisplayModes callback: "WxHxB" listed for 8-bit modes
 * (no Direct3D suffix or the RAMP one), else 16-bit modes the video memory
 * (+0xe0, if known) holds three times over plus a megabyte.  1. */
static HRESULT WINAPI host_dd_mode_cb(void *descp, LPVOID ctx)
{
    char buf[0x28];
    uint32_t desc = fs98_handle(descp), cfg = fs98_handle(ctx);
    int32_t w = (int32_t)M32(desc + 0xc), h = (int32_t)M32(desc + 8), wh2;
    uint32_t bpp = M32(desc + 0x54);
    if (lstrcmpA((const char *)(uintptr_t)(cfg + 0x80), (const char *)(uintptr_t)V(0x4187f8)) != 0 &&
        lstrcmpA((const char *)(uintptr_t)(cfg + 0x80), (const char *)(uintptr_t)V(0x4183cc)) != 0) {
        if (bpp != 0x10)
            return 1;
        if (M32(cfg + 0xe0) != 0) {
            wh2 = w * h * 2;
            if (M32(cfg + 0xe0) < (uint32_t)(wh2 / 2 + wh2 + 0x100000))
                return 1;
        }
    } else if (bpp != 8)
        return 1;
    wsprintfA(buf, (const char *)(uintptr_t)V(0x4185a4), w, h, bpp);
    host_display_add(cfg, fs98_handle(buf));
    return 1;
}

/* 0x414af0: a DirectDraw object's video memory: the total, plus the
 * current mode's frame if there is any. */
static uint32_t host_dd_memory(uint32_t dd)
{
    uint32_t desc[0x1b], caps[0x5b], m;
    memset(desc, 0, sizeof desc);
    desc[0] = 0x6c;
    ((host_com2)(uintptr_t)COM(dd, 0x30))(dd, fs98_handle(desc));
    caps[0] = 0x16c;
    ((host_com3)(uintptr_t)COM(dd, 0x2c))(dd, fs98_handle(caps), 0);
    m = caps[0x3c / 4];
    if (m != 0)
        m += (desc[0x54 / 4] >> 3) * desc[0xc / 4] * desc[8 / 4];
    return m;
}

/* 0x414b60 (op 4): the device's display modes into the list
 * (null-terminated), or "640x480" alone when DirectDraw is disabled, under
 * the busy cursor; its suffix split out (0x10000000 if it has one) and its
 * video memory at +0xe0. */
static uint32_t host_display_modes(uint32_t cfg)
{
    uint32_t dd;
    host_050(1);
    M32(cfg + 0xd0) &= 0xcfffffff;
    if (M8(cfg + 0xd0) & 1) {
        M32(M32(cfg + 0xe4)) = host_2b4(V(0x4185b0));
        M32(M32(cfg + 0xe4) + 4) = 0;
        host_050(0);
        return 0;
    }
    if (fs98_host_rt_device_create((const char *)(uintptr_t)cfg, (void **)(uintptr_t)fs98_handle(&dd)) != 0) {
        fs98_host_rt_device_suffix((char *)(uintptr_t)cfg, (char *)(uintptr_t)(cfg + 0x80), 0);
        if (M8(cfg + 0x80) != 0)
            M32(cfg + 0xd0) |= 0x10000000;
        M32(cfg + 0xe0) = host_dd_memory(dd);
        ((host_com5)(uintptr_t)COM(dd, 0x20))(dd, 0, 0, cfg, fs98_handle(host_dd_mode_cb));
        ((host_com1)(uintptr_t)COM(dd, 8))(dd);
    }
    M32(M32(cfg + 0xe4)) = 0;
    host_050(0);
    return 0;
}

/* +0x31c (0x414380): the display settings: 1 read them, 2 their defaults,
 * 3 list the devices, 4 list the device's modes, 5 write them, 6 remove
 * them.  -1 for another op; the third argument is unused. */
static uint32_t FS98_STDCALL host_31c(uint32_t op, uint32_t cfg, uint32_t unused)
{
    (void)unused;
    switch (op) {
    case 1: case 2: return (uint32_t)fs98_host_rt_read_display((uint8_t *)(uintptr_t)cfg, op == 2);
    case 3: return host_display_devices(cfg);
    case 4: return host_display_modes(cfg);
    case 5: case 6: return host_display_write(cfg, op == 6);
    default: return (uint32_t)-1;
    }
}

/* ---- the display mode (+0x048) -------------------------------------------
 *
 * DirectDraw's state: the object (0x41a4e4), its Direct3D (0x41a4dc), the
 * primary surface (0x41a4f8, flipping with a back buffer when 0x41a4f4),
 * the palette (0x41a4e8), 0x41a4e0 and 0x41a4fc (surfaces the views draw
 * on), the driver's kind (0x41a4ec, from 0x412420), the settings read into
 * 0x41a3f0, the mode in use (0x4183a8 width, 0x4183ac height, 0x4183b0
 * depth) and a re-entry count (0x41a500).  0x418854 says full screen. */

/* +0x048 (0x408680): full screen on or off; nothing if it already is.  0. */
static uint32_t FS98_STDCALL host_048(uint32_t on)
{
    if (on == M32(V(0x418854)))
        return 0;
    return on ? (uint32_t)fs98_host_rt_full_screen_on() : (uint32_t)fs98_host_rt_full_screen_off();
}

/* ---- dialogs (+0x10c..+0x118) --------------------------------------------
 *
 * The registry (0x417ee0) holds { id, module id, index, flags } records;
 * flags 2: the factory returns a null-terminated array of windows; 4:
 * modeless; 8: no pause around it.  Modeless dialogs are { record, window }
 * on the list at 0x417f00; modal ones { record, window or array } on a stack
 * at 0x41ba60, 0x4188a8 deep. */

/* 0x409d80, +0x114's: the same without a message. */
static uint32_t FS98_STDCALL host_modeless_tick(uint32_t e, uint32_t unused)
{
    (void)unused;
    if (!IsWindow((HWND)(uintptr_t)M32(e + 4))) {
        fs98_host_rt_release(fs98_host_rt_module_with_id(M32(M32(e) + 4)));
        if (!(M8(M32(e) + 0xc) & 8))
            fs98_host_rt_pause_look(0);
        host_300(V(0x417f00), e);
        return 0;
    }
    SendMessageA((HWND)(uintptr_t)M32(e + 4), 0x464, 0, 0);
    return 0;
}

/* 0x409d10: whether one of an array's windows took a message (its
 * accelerators, then as a dialog). */
static uint32_t host_array_take(uint32_t msg, uint32_t arr, uint32_t accel)
{
    for (; M32(arr) != 0; arr += 4) {
        if (TranslateAcceleratorA((HWND)(uintptr_t)M32(arr), (HACCEL)(uintptr_t)accel, (LPMSG)(uintptr_t)msg))
            return 1;
        if (IsDialogMessageA((HWND)(uintptr_t)M32(arr), (LPMSG)(uintptr_t)msg))
            return 1;
    }
    return 0;
}

#define DLG_ALIVE(w) (IsWindow((HWND)(uintptr_t)(w)))
#define DLG_DONE(w)  (GetWindowLongA((HWND)(uintptr_t)(w), GWLP_USERDATA) != 0)

/* +0x10c (0x409720): run dialog `id` with its parameter block (see
 * host.md, "The dialog engine").  -1 if it is not registered or its factory
 * makes nothing (the module then kept, the pause undone); else 0. */
static uint32_t FS98_STDCALL host_10c(uint32_t id, uint32_t param)
{
    MSG msg;
    uint32_t rec, link, w, arr = 0, paused = 0, n, prev, accel, p;
    rec = host_310(V(0x417ee0), 0, fs98_handle(&id), 4, 0);
    if (rec == 0)
        return (uint32_t)-1;
    if (IsWindowEnabled(HOST_HWND)) {
        paused = 1;
        if (!(M8(rec + 0xc) & 8))
            fs98_host_rt_pause_look(1);
    }
    link = fs98_handle(fs98_host_rt_acquire(fs98_host_rt_module_with_id(M32(rec + 4))));
    w = ((uint32_t (FS98_STDCALL *)(uint32_t, uint32_t))(uintptr_t)M32(M32(link + 0x14) + M32(rec + 8) * 12 + 4))(
        M32(V(0x41880c)), param);
    if (M8(rec + 0xc) & 2) {
        arr = w;
        w = M32(w);
    }
    if (arr == 0 && w == 0) {   /* RETAIL BUG (kept): HOST-22 -- single-window factory only (an array factory faults at w = M32(w)): the module acquired above is not released */
        if (paused && !(M8(rec + 0xc) & 8))
            fs98_host_rt_pause_look(0);
        return (uint32_t)-1;
    }
    ShowWindow((HWND)(uintptr_t)w, SW_SHOW);
    UpdateWindow((HWND)(uintptr_t)w);
    if (M8(rec + 0xc) & 4) {
        uint32_t e = host_2f8(V(0x417f00));
        M32(e) = rec;
        M32(e + 4) = w;
        while (PeekMessageA(&msg, (HWND)(uintptr_t)w, 0, 0, PM_REMOVE))
            if (!fs98_host_rt_modeless_took(&msg)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        return 0;
    }
    /* modal: on the stack, the window below told 0x469 (an array's first
     * visible window, or nothing if none is) */
    n = M32(V(0x4188a8));
    M32(V(0x41ba60) + n * 8) = rec;
    M32(V(0x41ba64) + n * 8) = (M8(rec + 0xc) & 2) ? arr : w;
    if ((int32_t)n > 0) {
        prev = M32(V(0x41ba58) + n * 8);
        if (M8(prev + 0xc) & 2) {
            /* RETAIL BUG (kept): HOST-22 -- retail reuses its register: when the current record and the previous one are both arrays, the pump below runs on the one below */
            arr = M32(V(0x41ba5c) + n * 8);
            for (p = arr; M32(p) != 0; p += 4)
                if (IsWindowVisible((HWND)(uintptr_t)M32(p))) {
                    SendMessageA((HWND)(uintptr_t)w, 0x469, 0, (LPARAM)M32(p));
                    break;
                }
        } else
            SendMessageA((HWND)(uintptr_t)w, 0x469, 0, (LPARAM)M32(V(0x41ba5c) + n * 8));
    } else
        SendMessageA((HWND)(uintptr_t)w, 0x469, 0, (LPARAM)M32(V(0x41880c)));
    M32(V(0x418850)) = 0;
    M32(V(0x4188a8)) = n + 1;
    if (paused && M32(V(0x418848)) == 0) {
        M32(V(0x418848)) = 3;
        (uint32_t)fs98_host_rt_hook(0x20004, 0);
    }
    accel = fs98_handle(LoadAcceleratorsA((HINSTANCE)(uintptr_t)M32(V(0x41882c)), (LPCSTR)0x69));
    /* the pumps, in retail's order of checks: alive, done, a message */
    if (M8(rec + 0xc) & 2) {
        if (!DLG_ALIVE(M32(arr)))
            goto array_end;
    array_outer:
        if (DLG_DONE(M32(arr)))
            goto array_end;
        if (!DLG_ALIVE(M32(arr)))
            goto array_idle;
    array_pump:
        if (DLG_DONE(M32(arr)) || !PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
            goto array_idle;
        if (!fs98_host_rt_modeless_took(&msg) && !host_array_take(fs98_handle(&msg), arr, accel)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (DLG_ALIVE(M32(arr)))
            goto array_pump;
    array_idle:
        if (DLG_ALIVE(M32(arr)) && !DLG_DONE(M32(arr))) {
            for (p = arr; M32(p) != 0; p += 4)
                SendMessageA((HWND)(uintptr_t)M32(p), 0x464, 0, 0);
            (uint32_t)fs98_host_rt_hook(0x20006, 0);
        }
        if (DLG_ALIVE(M32(arr)))
            goto array_outer;
    array_end:
        for (p = arr; M32(p) != 0; p += 4)
            if (DLG_ALIVE(M32(p)))
                DestroyWindow((HWND)(uintptr_t)M32(p));
    } else {
        if (!DLG_ALIVE(w))
            goto single_end;
    single_outer:
        if (DLG_DONE(w))
            goto single_end;
        if (!DLG_ALIVE(w))
            goto single_idle;
    single_pump:
        if (DLG_DONE(w) || !PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
            goto single_idle;
        if (!TranslateAcceleratorA((HWND)(uintptr_t)w, (HACCEL)(uintptr_t)accel, &msg) &&
            !fs98_host_rt_modeless_took(&msg) && !IsDialogMessageA((HWND)(uintptr_t)w, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (DLG_ALIVE(w))
            goto single_pump;
    single_idle:
        if (DLG_ALIVE(w) && !DLG_DONE(w)) {
            SendMessageA((HWND)(uintptr_t)w, 0x464, 0, 0);
            (uint32_t)fs98_host_rt_hook(0x20006, 0);
        }
        if (DLG_ALIVE(w))
            goto single_outer;
    single_end:
        if (DLG_ALIVE(w))
            DestroyWindow((HWND)(uintptr_t)w);
    }
    M32(V(0x4188a8))--;
    fs98_host_rt_release(fs98_host_rt_module_with_id(M32(rec + 4)));
    if (paused) {
        if (M32(V(0x418848)) == 3) {
            (uint32_t)fs98_host_rt_hook(0x20005, 0);
            M32(V(0x418848)) = 0;
            M32(V(0x41884c)) = 1;
        }
        if (!(M8(rec + 0xc) & 8))
            fs98_host_rt_pause_look(0);
    }
    return 0;
}

/* +0x110 / +0x118 (0x409bd0): +0x10c. */
static uint32_t FS98_STDCALL host_110(uint32_t id, uint32_t param)
{
    return host_10c(id, param);
}

/* +0x114 (0x409bf0): the modeless dialogs ticked (0x464), the closed ones
 * released and dropped. */
static uint32_t FS98_STDCALL host_114(void)
{
    return host_308(V(0x417f00), 0, fs98_handle(host_modeless_tick));
}

/* +0x314 (0x401140): does nothing (two arguments). */
static void FS98_STDCALL host_314(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
}

/* 0x411710: put a record in its z-order group -- groups kept in descending
 * key order (a new one before the first with a smaller key), the record at
 * the front of its group's nodes (left where it is if already there). */
static void host_view_link(uint32_t rec)
{
    uint32_t key = M32(rec + 0x78), g, prev = 0, n, fresh;
    if (M32(V(0x41a3dc)) == 0) {
        host_20c(1, 0xc, V(0x41a3dc), 0);
        g = M32(V(0x41a3dc));
        M32(g) = key;
        M32(g + 8) = 0;
        host_20c(1, 8, g + 4, 0);
        M32(M32(g + 4)) = rec;
        M32(M32(g + 4) + 4) = 0;
        return;
    }
    for (g = M32(V(0x41a3dc)); g != 0; prev = g, g = M32(g + 8)) {
        if (M32(g) == key) {
            for (n = M32(g + 4); n != 0; n = M32(n + 4))
                if (M32(n) == rec)
                    return;
            host_20c(1, 8, fs98_handle(&fresh), 0);
            M32(fresh) = rec;
            M32(fresh + 4) = M32(g + 4);
            M32(g + 4) = fresh;
            return;
        }
        if (M32(g) < key)
            break;
    }
    if (prev != 0) {
        host_20c(1, 0xc, prev + 8, 0);
        fresh = M32(prev + 8);
    } else {
        host_20c(1, 0xc, fs98_handle(&fresh), 0);
        M32(V(0x41a3dc)) = fresh;
    }
    M32(fresh) = key;
    M32(fresh + 8) = g;
    host_20c(1, 8, fresh + 4, 0);
    M32(M32(fresh + 4)) = rec;
    M32(M32(fresh + 4) + 4) = 0;
}

/* +0x064 (0x410540): make a view window for a record: a host block (as
 * +0x074), its frame ("FS98CHILD", child of the main window, at the record's
 * +0x30..+0x3c) and its client ("FS98FLOAT"), both shown hidden and updated,
 * the record in its z-order group, and the frame told 0x467.  0, or -1 (the
 * block freed) if a window cannot be made. */
static uint32_t FS98_STDCALL host_064(uint32_t rec)
{
    uint32_t b;
    if (rec == 0)
        return (uint32_t)-1;
    host_20c(1, 0x90, fs98_handle(&b), 1);
    M32(rec + 0xa0) = b;
    M32(b + 0x54) = 0;
    M32(b + 0x58) = 0;
    M32(b + 0x5c) = 0x20;
    M32(b + 0x60) = 0x10;
    M32(b + 0x64) = 0;
    M32(b + 0x68) = 0;
    M32(b + 0x6c) = 0;
    M32(b + 0x70) = 0;
    M32(b + 0x74) = 0;
    M32(b + 0x78) = 0x20;
    M32(b + 0x7c) = 0x14;
    M32(b + 0x80) = 0;
    M32(b + 0x84) = 0;
    M32(b + 0x88) = 0;
    M32(b + 0x24) = fs98_handle(CreateWindowExA(0, (LPCSTR)(uintptr_t)V(0x417168), (LPCSTR)(uintptr_t)M32(rec),
                                         0x44000000, (int)M32(rec + 0x30), (int)M32(rec + 0x34),
                                         (int)M32(rec + 0x38), (int)M32(rec + 0x3c), HOST_HWND, 0,
                                         (HINSTANCE)(uintptr_t)M32(V(0x418828)), (LPVOID)(uintptr_t)rec));
    if (M32(b + 0x24) != 0) {
        ShowWindow((HWND)(uintptr_t)M32(b + 0x24), SW_HIDE);
        UpdateWindow((HWND)(uintptr_t)M32(b + 0x24));
        M32(b + 0x28) = fs98_handle(CreateWindowExA(0x80, (LPCSTR)(uintptr_t)V(0x417178), (LPCSTR)(uintptr_t)M32(rec),
                                             0x84cc0000, 0, 0, 0, 0, HOST_HWND, 0,
                                             (HINSTANCE)(uintptr_t)M32(V(0x418828)), (LPVOID)(uintptr_t)rec));
        if (M32(b + 0x28) != 0) {
            ShowWindow((HWND)(uintptr_t)M32(b + 0x28), SW_HIDE);
            UpdateWindow((HWND)(uintptr_t)M32(b + 0x28));
            host_view_link(rec);
            SendMessageA((HWND)(uintptr_t)M32(b + 0x24), 0x467, 0, 0);
            return 0;
        }
    }
    host_214(rec + 0xa0);
    return (uint32_t)-1;
}

/* +0x04c (0x4086b0): size the main window for a 640x480 client area
 * (unless 0x418854 is set), then post it WM_SIZE.  Returns 0. */
static uint32_t FS98_STDCALL host_04c(void)
{
    RECT r = { 0, 0, 0x280, 0x1e0 };
    if (M32(V(0x418854)) == 0) {
        LONG ex = GetWindowLongA(HOST_HWND, GWL_EXSTYLE);
        BOOL menu = GetMenu(HOST_HWND) != 0;
        LONG style = GetWindowLongA(HOST_HWND, GWL_STYLE);
        AdjustWindowRectEx(&r, (DWORD)style, menu, (DWORD)ex);
        SetWindowPos(HOST_HWND, 0, 0, 0, r.right - r.left, r.bottom - r.top, 0x16);
    }
    PostMessageA(HOST_HWND, WM_SIZE, 0, 0);
    return 0;
}

/* +0x068 (0x4106e0): destroy a view window: out of its group, frame and
 * client destroyed, its lists and host block freed. */
static uint32_t FS98_STDCALL host_068(uint32_t rec)
{
    uint32_t block;
    if (rec == 0)
        return 0;
    block = M32(rec + 0xa0);
    host_view_unlink(rec);
    /* RETAIL BUG (kept): HOST-19 -- the host block is read unchecked */
    DestroyWindow((HWND)(uintptr_t)M32(block + 0x24));
    DestroyWindow((HWND)(uintptr_t)M32(block + 0x28));
    if (M32(rec + 0xa0) != 0) {
        host_2fc(block + 0x54);
        host_2fc(block + 0x70);
        host_214(rec + 0xa0);
    }
    return 0;
}

/* ---- tooltip regions --------------------------------------------------------- */

/* A window's tooltip regions are a tree of 0x2c-byte nodes in arrays: type
 * (+0: 0 ends a list, 1 opens a group whose nodes follow, 3 closes it), x, y,
 * width, height (+4..+0x10), a tip string id (word +0x18), and the heap
 * rectangle the host keeps for it (+0x28: left, top, right, bottom, the
 * group id, and the offsets it was placed at).  A node with a tip is a tool
 * of the window's tooltip control (host block +0x2c), its rectangle scaled by
 * the block's 16.16 factors (+0x44, +0x48).  Registered trees are on the
 * block's +0x70 list ({ ?, ?, 0, 0, tree }). */
#define TN(n, i)        ((n) + 0x2c * (uint32_t)(i))

/* The TOOLINFO the host sends, for a node's rectangle. */
static void host_tip_send(uint32_t block, uint32_t msg, uint32_t node, uint32_t r)
{
    uint32_t ti[10];
    ti[0] = 0x28;
    ti[1] = 0;
    ti[2] = M32(block + 0x24);
    ti[3] = M32(node + 0x28);
    if (msg == 0x404) {
        ti[4] = (uint32_t)((int32_t)(M32(r) * M32(block + 0x44)) >> 16);
        ti[6] = (uint32_t)((int32_t)(M32(r + 8) * M32(block + 0x44)) >> 16);
        ti[5] = (uint32_t)((int32_t)(M32(r + 4) * M32(block + 0x48)) >> 16);
        ti[7] = (uint32_t)((int32_t)(M32(r + 0xc) * M32(block + 0x48)) >> 16);
        ti[8] = M32(V(0x41882c));
        ti[9] = M16(node + 0x18);
    }
    SendMessageA((HWND)(uintptr_t)M32(block + 0x2c), msg, 0, (LPARAM)fs98_handle(ti));
}

/* 0x40dd90: give each node of a list (and its groups' nodes) a rectangle at
 * (dx, dy), growing `bounds` -- the outermost one, for nested nodes too --
 * and register those with tips.  Returns the nodes walked + 1. */
static uint32_t host_tip_add(uint32_t rec, uint32_t nodes, int32_t dx, int32_t dy, uint32_t id,
                             uint32_t bounds)
{
    uint32_t block = M32(rec + 0xa0), i = 0, n, r;
    if (M32(nodes) == 0)
        return 1;
    do {
        n = TN(nodes, i);
        if (M32(n) == 3)
            break;
        host_20c(1, 0x1c, fs98_handle(&r), 0);
        M32(n + 0x28) = r;
        M32(r) = M32(n + 4) + (uint32_t)dx;
        M32(r + 8) = M32(n + 0xc) + M32(r) + 1;
        /* RETAIL BUG (kept): HOST-20 -- nested nodes grow the outermost bounds, not their group's */
        if ((int32_t)M32(bounds) > (int32_t)M32(r))
            M32(bounds) = M32(r);
        if ((int32_t)M32(bounds + 8) < (int32_t)M32(r + 8))
            M32(bounds + 8) = M32(r + 8);
        M32(r + 4) = M32(n + 8) + (uint32_t)dy;
        M32(r + 0xc) = M32(r + 4) + M32(n + 0x10) + 1;
        if ((int32_t)M32(bounds + 4) > (int32_t)M32(r + 4))
            M32(bounds + 4) = M32(r + 4);
        if ((int32_t)M32(bounds + 0xc) < (int32_t)M32(r + 0xc))
            M32(bounds + 0xc) = M32(r + 0xc);
        M32(r + 0x14) = (uint32_t)dx;
        M32(r + 0x18) = (uint32_t)dy;
        if (M32(n + 0x18) != 0)
            host_tip_send(block, 0x404, n, r);
        if (M32(n) == 1) {
            M32(r + 0x10) = id + 1;
            i += host_tip_add(rec, TN(nodes, i + 1), (int32_t)M32(r), (int32_t)M32(r + 4), M32(r + 0x10), bounds);
        } else {
            M32(r + 0x10) = id;
        }
        i++;
    } while (M32(TN(nodes, i)) != 0);
    return i + 1;
}

/* 0x40df70: the same over the existing rectangles, each tool removed and
 * added again. */
static uint32_t host_tip_move(uint32_t rec, uint32_t nodes, int32_t dx, int32_t dy, uint32_t id)
{
    uint32_t block = M32(rec + 0xa0), i = 0, n, r;
    if (M32(nodes) == 0)
        return 1;
    do {
        n = TN(nodes, i);
        if (M32(n) == 3)
            break;
        r = M32(n + 0x28);
        M32(r) = M32(n + 4) + (uint32_t)dx;
        M32(r + 8) = M32(n + 0xc) + M32(r) + 1;
        M32(r + 4) = M32(n + 8) + (uint32_t)dy;
        M32(r + 0x18) = (uint32_t)dy;
        M32(r + 0xc) = M32(n + 0x10) + M32(r + 4) + 1;
        M32(r + 0x14) = (uint32_t)dx;
        if (M32(n + 0x18) != 0) {
            host_tip_send(block, 0x405, n, r);
            host_tip_send(block, 0x404, n, r);
        }
        if (M32(n) == 1) {
            M32(r + 0x10) = id + 1;
            i += host_tip_move(rec, TN(nodes, i + 1), (int32_t)M32(r), (int32_t)M32(r + 4), M32(r + 0x10));
        } else {
            M32(r + 0x10) = id;
        }
        i++;
    } while (M32(TN(nodes, i)) != 0);
    return i + 1;
}

/* 0x40e0e0: remove the tools and free the rectangles. */
static uint32_t host_tip_remove(uint32_t rec, uint32_t nodes)
{
    uint32_t block = M32(rec + 0xa0), i = 0, n;
    if (M32(nodes) == 0)
        return 1;
    do {
        n = TN(nodes, i);
        if (M32(n) == 3)
            break;
        if (M32(n + 0x18) != 0 && M32(block + 0x2c) != 0)
            host_tip_send(block, 0x405, n, 0);
        host_214(n + 0x28);
        if (M32(n) == 1)
            i += host_tip_remove(rec, TN(nodes, i + 1));
        i++;
    } while (M32(TN(nodes, i)) != 0);
    return i + 1;
}

/* 0x40e1f0: do a group's nodes fit inside it (x + width, y + height within
 * its own)?  The nodes walked, or 0 if one does not. */
static uint32_t host_tip_fits(uint32_t g)
{
    uint32_t i = 1, r;
    if (M32(TN(g, 1)) == 3)
        return 1;
    do {
        uint32_t n = TN(g, i);
        if ((int32_t)(M32(n + 0xc) + M32(n + 4)) > (int32_t)M32(g + 0xc))
            return 0;
        if ((int32_t)(M32(n + 0x10) + M32(n + 8)) > (int32_t)M32(g + 0x10))
            return 0;
        if (M32(n) == 1) {
            r = host_tip_fits(n);
            if (r == 0)
                return 0;
            i += r;
        }
        i++;
    } while (M32(TN(g, i)) != 3);
    return i;
}

/* 0x40e1a0: check a tree.  0, or -1 if something does not fit. */
static uint32_t host_tip_check(uint32_t tree)
{
    uint32_t i = 0, r;
    if (M32(tree) == 0)
        return 0;
    do {
        /* RETAIL BUG (kept): HOST-20 -- every top-level node is read as a group: one that is not scans past the end for a closing 3 */
        r = host_tip_fits(TN(tree, i));
        if (r == 0)
            return (uint32_t)-1;
        i += r + 1;
    } while (M32(TN(tree, i)) != 0);
    return 0;
}

/* +0x27c (0x40dc10): register a tree of tooltip regions for a window at
 * (x, y), making the window's tooltip control first if it has none.  The
 * tree is checked only after its tools are in.  0, or -1 (already
 * registered, or a node does not fit). */
static uint32_t FS98_STDCALL host_27c(uint32_t rec, uint32_t tree, uint32_t x, uint32_t y)
{
    uint32_t block = M32(rec + 0xa0), e;
    if (M32(block + 0x2c) == 0)
        M32(block + 0x2c) = fs98_handle(CreateWindowExA(8, (LPCSTR)(uintptr_t)V(0x4182c4), 0, 1, (int)0x80000000,
                                                 (int)0x80000000, (int)0x80000000, (int)0x80000000, 0, 0,
                                                 (HINSTANCE)(uintptr_t)M32(V(0x418828)), 0));
    if (host_310(block + 0x70, 0x10, fs98_handle(&tree), 4, 0) != 0)
        return (uint32_t)-1;
    /* RETAIL BUG (kept): HOST-20 -- the tools are registered (host_tip_add) before host_tip_check */
    e = host_2f8(block + 0x70);
    M32(e) = M32(rec + 0x38);
    M32(e + 4) = M32(rec + 0x3c);
    M32(e + 8) = 0;
    M32(e + 0xc) = 0;
    M32(e + 0x10) = tree;
    if (host_tip_add(rec, tree, (int32_t)x, (int32_t)y, 1, e) == 0)
        return (uint32_t)-1;
    return host_tip_check(tree);
}

/* +0x280 (0x40dcd0): re-place a registered tree after its nodes moved, at
 * the offsets its first rectangle was placed at. */
static uint32_t FS98_STDCALL host_280(uint32_t rec, uint32_t tree)
{
    uint32_t r = M32(tree + 0x28);
    if (host_tip_move(rec, tree, (int32_t)M32(r + 0x14), (int32_t)M32(r + 0x18), M32(r + 0x10)) == 0)
        return (uint32_t)-1;
    return host_tip_check(tree);
}

/* +0x284 (0x40dd10): unregister a tree; the tooltip control goes with the
 * last.  0, or -1 if it was not registered. */
static uint32_t FS98_STDCALL host_284(uint32_t rec, uint32_t tree)
{
    uint32_t block = M32(rec + 0xa0), e = host_310(block + 0x70, 0x10, fs98_handle(&tree), 4, 0);
    if (e == 0)
        return (uint32_t)-1;
    host_300(block + 0x70, e);
    if (host_tip_remove(rec, tree) == 0)
        return (uint32_t)-1;
    if (M32(block + 0x70) == 0) {
        DestroyWindow((HWND)(uintptr_t)M32(block + 0x2c));
        M32(block + 0x2c) = 0;
    }
    return 0;
}

/* ---- the fatal file wrappers ------------------------------------------------- */

/* 0x40b1a0: string `id` (a newline and `text` after it, if given) in a message box
 * captioned with the string at 0x41c380, then exit. */
static void host_fatal(uint32_t id, uint32_t text)
{
    char buf[0x400];
    host_2a0(id, fs98_handle(buf), 0x400);
    if (text != 0) {
        host_2b0(fs98_handle(buf), V(0x4180e0));
        host_2b0(fs98_handle(buf), text);
    }
    MessageBoxA(0, buf, (LPCSTR)(uintptr_t)V(0x41c380), 0);
    ExitProcess(1);
}

/* +0x180 (0x40b210): +0x17c, fatal (0xf521, the path) on failure. */
static uint32_t FS98_STDCALL host_180(uint32_t out, uint32_t path, uint32_t ext, uint32_t mode)
{
    uint32_t r = host_17c(out, path, ext, mode);
    if (r != 0)
        host_fatal(0xf521, path);
    return r;
}

/* +0x1a0 (0x40b240): +0x19c, fatal (0xf523, the file's path) on failure. */
static uint32_t FS98_STDCALL host_1a0(uint32_t f, uint32_t buf, uint32_t n, uint32_t got)
{
    uint32_t r = host_19c(f, buf, n, got);
    if (r != 0)
        host_fatal(0xf523, f != 0 ? M32(f + 4) : 0);
    return r;
}

/* +0x198 (0x40b290): +0x194, fatal (0xf522, the file's path) on failure. */
static uint32_t FS98_STDCALL host_198(uint32_t f, uint32_t pos, uint32_t method)
{
    uint32_t r = host_194(f, pos, method);
    if (r != 0)
        host_fatal(0xf522, f != 0 ? M32(f + 4) : 0);
    return r;
}

/* +0x140 (0x40b2d0): close and free a file; fatal (0xf520, its path) if
 * CloseHandle fails or the file is null.  Returns 0. */
static uint32_t FS98_STDCALL host_140(uint32_t f)
{
    char text[0x100];
    BOOL ok = FALSE;
    text[0] = 0;
    if (f != 0) {
        ok = CloseHandle((HANDLE)(uintptr_t)M32(f));
        if (!ok)
            host_2a8(fs98_handle(text), M32(f + 4));
        host_214(f + 4);
        host_214(fs98_handle(&f));
    }
    if (!ok)
        host_fatal(0xf520, fs98_handle(text));
    return 0;
}

/* ---- the table --------------------------------------------------------------- */

/* The service table, the shape of the host's: slot 0 is the module id, the rest
 * are service pointers; unfinished slots stay null. */
__declspec(dllexport) void *HostTable[0xc8] = {
    [0x00 / 4] = (void *)1,
    [0x18 / 4] = (void *)host_018,
    [0x1c / 4] = (void *)host_01c,
    [0x20 / 4] = (void *)host_020,
    [0x24 / 4] = (void *)host_024,
    [0x28 / 4] = (void *)host_028,
    [0x2c / 4] = (void *)host_02c,
    [0x30 / 4] = (void *)host_030,
    [0x34 / 4] = (void *)host_034,
    [0x38 / 4] = (void *)host_038,
    [0x3c / 4] = (void *)host_03c,
    [0x40 / 4] = (void *)host_040,
    [0x44 / 4] = (void *)host_044,
    [0x48 / 4] = (void *)host_048,
    [0x4c / 4] = (void *)host_04c,
    [0x50 / 4] = (void *)host_050,
    [0x54 / 4] = (void *)host_054,
    [0x58 / 4] = (void *)host_058,
    [0x5c / 4] = (void *)host_05c,
    [0x60 / 4] = (void *)host_060,
    [0x64 / 4] = (void *)host_064,
    [0x68 / 4] = (void *)host_068,
    [0x6c / 4] = (void *)host_06c,
    [0x70 / 4] = (void *)host_070,
    [0x74 / 4] = (void *)host_074,
    [0x78 / 4] = (void *)host_078,
    [0x7c / 4] = (void *)host_07c,
    [0x80 / 4] = (void *)host_080,
    [0x84 / 4] = (void *)host_084,
    [0x88 / 4] = (void *)host_088,
    [0x8c / 4] = (void *)host_08c,
    [0x90 / 4] = (void *)host_090,
    [0x94 / 4] = (void *)host_094,
    [0x98 / 4] = (void *)host_098,
    [0x9c / 4] = (void *)host_09c,
    [0xa0 / 4] = (void *)host_0a0,
    [0xa4 / 4] = (void *)host_0a4,
    [0xa8 / 4] = (void *)host_0a8,
    [0xac / 4] = (void *)host_0ac,
    [0xb0 / 4] = (void *)host_0b0,
    [0xb4 / 4] = (void *)host_0b4,
    [0xb8 / 4] = (void *)host_0b8,
    [0xbc / 4] = (void *)host_0bc,
    [0xc0 / 4] = (void *)host_0c0,
    [0xc4 / 4] = (void *)host_0c4,
    [0xc8 / 4] = (void *)host_0c8,
    [0xcc / 4] = (void *)host_0cc,
    [0xd0 / 4] = (void *)host_0d0,
    [0xd4 / 4] = (void *)host_0d4,
    [0xd8 / 4] = (void *)host_0d8,
    [0xdc / 4] = (void *)host_0dc,
    [0xe0 / 4] = (void *)host_0e0,
    [0xe4 / 4] = (void *)host_0e4,
    [0xe8 / 4] = (void *)host_0e8,
    [0xec / 4] = (void *)host_0ec,
    [0xf0 / 4] = (void *)host_0f0,
    [0xf4 / 4] = (void *)host_0f4,
    [0xf8 / 4] = (void *)host_0f8,
    [0xfc / 4] = (void *)host_0fc,
    [0x100 / 4] = (void *)host_100,
    [0x104 / 4] = (void *)host_104,
    [0x108 / 4] = (void *)host_zero2,
    [0x10c / 4] = (void *)host_10c,
    [0x110 / 4] = (void *)host_110,
    [0x114 / 4] = (void *)host_114,
    [0x118 / 4] = (void *)host_110,
    [0x11c / 4] = (void *)host_11c,
    [0x120 / 4] = (void *)host_120,
    [0x124 / 4] = (void *)host_124,
    [0x128 / 4] = (void *)host_128,
    [0x12c / 4] = (void *)host_zero1,
    [0x130 / 4] = (void *)host_130,
    [0x134 / 4] = (void *)host_134,
    [0x138 / 4] = (void *)host_138,
    [0x13c / 4] = (void *)host_13c,
    [0x140 / 4] = (void *)host_140,
    [0x144 / 4] = (void *)host_144,
    [0x148 / 4] = (void *)host_148,
    [0x14c / 4] = (void *)host_14c,
    [0x150 / 4] = (void *)host_150,
    [0x154 / 4] = (void *)host_zero0,
    [0x158 / 4] = (void *)host_158,
    [0x15c / 4] = (void *)host_15c,
    [0x160 / 4] = (void *)host_160,
    [0x164 / 4] = (void *)host_164,
    [0x168 / 4] = (void *)host_168,
    [0x16c / 4] = (void *)host_16c,
    [0x170 / 4] = (void *)host_170,
    [0x174 / 4] = (void *)host_174,
    [0x178 / 4] = (void *)host_178,
    [0x17c / 4] = (void *)host_17c,
    [0x180 / 4] = (void *)host_180,
    [0x184 / 4] = (void *)host_184,
    [0x188 / 4] = (void *)host_188,
    [0x18c / 4] = (void *)host_18c,
    [0x190 / 4] = (void *)host_190,
    [0x194 / 4] = (void *)host_194,
    [0x198 / 4] = (void *)host_198,
    [0x19c / 4] = (void *)host_19c,
    [0x1a0 / 4] = (void *)host_1a0,
    [0x1a4 / 4] = (void *)host_1a4,
    [0x1a8 / 4] = (void *)host_1a8,
    [0x1ac / 4] = (void *)host_1ac,
    [0x1b0 / 4] = (void *)host_1b0,
    [0x1b4 / 4] = (void *)host_1b4,
    [0x1b8 / 4] = (void *)host_1b8,
    [0x1bc / 4] = (void *)host_1bc,
    [0x1c0 / 4] = (void *)host_1c0,
    [0x1c4 / 4] = (void *)host_1c4,
    [0x1c8 / 4] = (void *)host_1c8,
    [0x1cc / 4] = (void *)host_1cc,
    [0x1d0 / 4] = (void *)host_1d0,
    [0x1d4 / 4] = (void *)host_1d4,
    [0x1d8 / 4] = (void *)host_1d8,
    [0x1dc / 4] = (void *)host_1dc,
    [0x1e0 / 4] = (void *)host_1e0,
    [0x1e4 / 4] = (void *)host_1e4,
    [0x1e8 / 4] = (void *)host_1e8,
    [0x1ec / 4] = (void *)host_1ec,
    [0x1f0 / 4] = (void *)host_1f0,
    [0x1f4 / 4] = (void *)host_1f4,
    [0x1f8 / 4] = (void *)host_1f8,
    [0x1fc / 4] = (void *)host_1fc,
    [0x200 / 4] = (void *)host_200,
    [0x204 / 4] = (void *)host_204,
    [0x208 / 4] = (void *)host_208,
    [0x20c / 4] = (void *)host_20c,
    [0x210 / 4] = (void *)host_210,
    [0x214 / 4] = (void *)host_214,
    [0x218 / 4] = (void *)host_218,
    [0x21c / 4] = (void *)host_21c,
    [0x220 / 4] = (void *)host_zero2,
    [0x224 / 4] = (void *)host_zero2,
    [0x228 / 4] = (void *)host_228,
    [0x22c / 4] = (void *)host_22c,
    [0x230 / 4] = (void *)host_230,
    [0x234 / 4] = (void *)host_234,
    [0x238 / 4] = (void *)host_238,
    [0x23c / 4] = (void *)host_23c,
    [0x240 / 4] = (void *)host_240,
    [0x244 / 4] = (void *)host_244,
    [0x248 / 4] = (void *)host_248,
    [0x24c / 4] = (void *)host_24c,
    [0x250 / 4] = (void *)host_250,
    [0x254 / 4] = (void *)host_254,
    [0x258 / 4] = (void *)host_258,
    [0x25c / 4] = (void *)host_25c,
    [0x260 / 4] = (void *)host_260,
    [0x264 / 4] = (void *)host_264,
    [0x268 / 4] = (void *)host_268,
    [0x26c / 4] = (void *)host_26c,
    [0x270 / 4] = (void *)host_270,
    [0x274 / 4] = (void *)host_274,
    [0x278 / 4] = (void *)host_278,
    [0x27c / 4] = (void *)host_27c,
    [0x280 / 4] = (void *)host_280,
    [0x284 / 4] = (void *)host_284,
    [0x288 / 4] = (void *)host_288,
    [0x28c / 4] = (void *)host_28c,
    [0x290 / 4] = (void *)host_290,
    [0x294 / 4] = (void *)host_294,
    [0x298 / 4] = (void *)host_298,
    [0x29c / 4] = (void *)host_29c,
    [0x2a0 / 4] = (void *)host_2a0,
    [0x2a4 / 4] = (void *)host_2a4,
    [0x2a8 / 4] = (void *)host_2a8,
    [0x2ac / 4] = (void *)host_2ac,
    [0x2b0 / 4] = (void *)host_2b0,
    [0x2b4 / 4] = (void *)host_2b4,
    [0x2b8 / 4] = (void *)host_2b8,
    [0x2bc / 4] = (void *)host_2bc,
    [0x2c0 / 4] = (void *)host_2c0,
    [0x2c4 / 4] = (void *)host_2c4,
    [0x2c8 / 4] = (void *)host_2c8,
    [0x2cc / 4] = (void *)host_2cc,
    [0x2d0 / 4] = (void *)host_2d0,
    [0x2d4 / 4] = (void *)host_2d4,
    [0x2d8 / 4] = (void *)host_2d8,
    [0x2dc / 4] = (void *)host_2dc,
    [0x2e0 / 4] = (void *)host_2e0,
    [0x2e4 / 4] = (void *)host_2e4,
    [0x2e8 / 4] = (void *)host_2e8,
    [0x2ec / 4] = (void *)host_2ec,
    [0x2f0 / 4] = (void *)host_2f0,
    [0x2f4 / 4] = (void *)host_2f4,
    [0x2f8 / 4] = (void *)host_2f8,
    [0x2fc / 4] = (void *)host_2fc,
    [0x300 / 4] = (void *)host_300,
    [0x304 / 4] = (void *)host_304,
    [0x308 / 4] = (void *)host_308,
    [0x30c / 4] = (void *)host_30c,
    [0x310 / 4] = (void *)host_310,
    [0x314 / 4] = (void *)host_314,
    [0x318 / 4] = (void *)host_318,
    [0x31c / 4] = (void *)host_31c,
};

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    FS98_IMAGE_END_IN(host_image);  /* the one place the image's end symbol is emitted: a helper function would change .eh_frame */
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
