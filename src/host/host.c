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
#include <stdint.h>
#include <stdio.h>
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

/* ---- the C-runtime wrappers -------------------------------------------------- */

/* qsort and bsearch take a cdecl comparator; the host's callers pass stdcall
 * ones, so the host parks the comparator at 0x41a24c and hands MSVCRT this
 * trampoline (0x40cfe0) instead.  Not reentrant, as retail is not. */
static int __cdecl host_cmp_tramp(const void *a, const void *b)
{
    return (int)((uint32_t (FS98_STDCALL *)(uint32_t, uint32_t))(uintptr_t)M32(V(0x41a24c)))(
        ADDR(a), ADDR(b));
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
    return ADDR(bsearch((const void *)(uintptr_t)key, (const void *)(uintptr_t)base, n, width,
                        host_cmp_tramp));
}

/* +0x230 (0x40d000): exchange two blocks of `size` bytes through a temporary
 * from +0x20c (kept, as retail keeps it, in the size argument's own slot).
 * Returns +0x214's 0. */
static uint32_t FS98_STDCALL host_230(uint32_t a, uint32_t b, uint32_t size)
{
    uint32_t tmp;
    host_20c(1, size, ADDR(&tmp), 0);
    host_21c(tmp, a, size);
    host_21c(a, b, size);
    host_21c(b, tmp, size);
    return host_214(ADDR(&tmp));
}

/* +0x2ac (0x40eea0): strncpy(dst, src, n), returning dst. */
static uint32_t FS98_STDCALL host_2ac(uint32_t dst, uint32_t src, uint32_t n)
{
    return ADDR(strncpy((char *)(uintptr_t)dst, (const char *)(uintptr_t)src, n));
}

/* +0x2b4 (0x40ef00): a copy of `s` from +0x20c (fatal on failure), or null for
 * a null `s`. */
static uint32_t FS98_STDCALL host_2b4(uint32_t s)
{
    uint32_t p;
    if (s == 0)
        return 0;
    host_20c(1, host_2b8(s) + 1, ADDR(&p), 0);
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
        if ((c == '/' || c == '-') && M8(p - 1) == ' ' &&
            host_228(p + 1, name, host_2b8(name)) == 0 &&
            (M8(host_2b8(name) + p + 1) == ':' || M8(host_2b8(name) + p + 1) == ' '))
            return host_2b8(name) + p + 2;
        p = ADDR(CharNextA((const char *)(uintptr_t)p));
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
        e = ADDR(CharNextA((const char *)(uintptr_t)e));
        c = M8(e);
    }
    n = e - v;
    if (n >= len)
        return (uint32_t)-1;
    host_218(buf, 0, len);
    host_21c(buf, v, n);
    q = buf;
    while (M8(q) != 0 && M8(q) == ' ')
        q = ADDR(CharNextA((const char *)(uintptr_t)q));
    host_2a8(buf, q);
    t = host_2b8(buf) + buf - 1;
    if (M8(t) == ' ') {
        while (t != buf) {
            M8(t) = 0;
            t = ADDR(CharPrevA((const char *)(uintptr_t)buf, (const char *)(uintptr_t)t));
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
    uint32_t cl = ADDR(GetCommandLineA()), v;
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
    uint32_t r = host_098(name, ADDR(buf), 0x40);
    if (r == 0)
        M32(out) = (uint32_t)atoi(buf);
    return r;
}

static uint32_t FS98_STDCALL host_0a0(uint32_t name, uint32_t out)
{
    char buf[0x40];
    uint32_t r = host_098(name, ADDR(buf), 0x40);
    if (r == 0)
        *(double *)(uintptr_t)out = atof(buf);
    return r;
}

/* +0xa4 (0x4095b0): is option `name` given?  0 if not, 2 for "/name:...", 1
 * for "/name ..." (more exactly: 1 when a space precedes what follows). */
static uint32_t FS98_STDCALL host_0a4(uint32_t name)
{
    uint32_t v = host_opt_find(ADDR(GetCommandLineA()), name);
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
    M32(M32(V(0x4188b0)) + M32(V(0x4188ac)) * 4) = ADDR(_strdup((const char *)(uintptr_t)ext));
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
        host_2a8(ADDR(unc), V(0x4180c8));
        host_2a8(ADDR(unc) + 2, path + 2);
        p = ADDR(unc) + 2;
        do
            p = ADDR(CharNextA((const char *)(uintptr_t)p));
        while (M8(p) != '\\' && M8(p) != 0);
        host_split(p, 0, dir, fname, ADDR(ext));
        M8(p) = 0;
        host_2a8(drive, ADDR(unc));
    } else {
        host_split(path, drive, dir, fname, ADDR(ext));
    }
    if (ext[0] != 0 && ext_id != 0 && host_ext_lookup(ADDR(ext), ext_id) != 0)
        host_1ac(ADDR(ext), ext_id);
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
    n = host_30c(HOST_STRMODS, ADDR(req), ADDR(host_str_cb));
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
    n = host_2a0(id, ADDR(buf), 0x400);
    if (n != 0) {
        host_20c(1, n + 1, out, 0);
        host_2a8(M32(out), ADDR(buf));
    }
    return n;
}

/* +0x298 (0x40ece0): put module `id`'s HMODULE on the string list (and in
 * *out).  -1 with *out 0 if the module is unknown or not loaded. */
static uint32_t FS98_STDCALL host_298(uint32_t id, uint32_t out)
{
    uint32_t rec = host_310(HOST_MODULES, 0xc, ADDR(&id), 4, 0), e;
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
    uint32_t e = host_310(HOST_STRMODS, 0, ADDR(&hmod), 4, 0);
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
        s = ADDR(CharNextA((const char *)(uintptr_t)s));
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
    uint8_t buf[0x208];
    uint32_t seplen = host_2b8(V(0x4180c4)), i, b = ADDR(buf);
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
    host_20c(1, 0x20, ADDR(&p), 1);
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
    host_214(ADDR(&p));
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
    return ADDR(LoadAcceleratorsA((HINSTANCE)(uintptr_t)M32(elem),
                                  (const char *)(uintptr_t)(uint32_t)M16(res)));
}

/* +0x1e4 (0x40c200): a new table from accelerator resource `res`, found as
 * strings are (the string modules newest first, then the host); its id into
 * *out.  -1 with *out 0 if no module has it. */
static uint32_t FS98_STDCALL host_1e4(uint32_t out, uint32_t res)
{
    uint32_t h, n, tmp, t, i;
    h = host_30c(HOST_STRMODS, ADDR(&res), ADDR(host_accel_cb));
    if (h == 0)
        h = ADDR(LoadAcceleratorsA((HINSTANCE)(uintptr_t)M32(V(0x41882c)),
                                   (const char *)(uintptr_t)(res & 0xffff)));
    if (h == 0) {
        M32(out) = 0;
        return (uint32_t)-1;
    }
    n = (uint32_t)CopyAcceleratorTableA((HACCEL)(uintptr_t)h, 0, 0);
    if (host_20c(1, n * 6, ADDR(&tmp), 0) != 0)
        return (uint32_t)-1;
    CopyAcceleratorTableA((HACCEL)(uintptr_t)h, (ACCEL *)(uintptr_t)tmp, (int)n);
    DestroyAcceleratorTable((HACCEL)(uintptr_t)h);
    t = host_accel_new();
    M32(out) = M32(t + 4);
    M32(t + 0xc) = n;
    host_20c(1, n * 12, t + 8, 1);
    for (i = 0; i < M32(t + 0xc); i++)
        host_accel_entry(tmp + 6 * i, M32(t + 8) + 12 * i);
    host_214(ADDR(&tmp));
    return 0;
}

/* +0x1e8 / +0x1ec (0x40c310 / 0x40c350): enable / disable table `id`.  0,
 * or -1 for id 0 or an unknown one. */
static uint32_t FS98_STDCALL host_1e8(uint32_t id)
{
    uint32_t t;
    if (id == 0)
        return (uint32_t)-1;
    t = host_accel_find(ADDR(&id));
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
    t = host_accel_find(ADDR(&id));
    if (t == 0)
        return (uint32_t)-1;
    M32(t) = 0;
    return 0;
}

/* +0x1f0 (0x40c390): empty table `id`.  0, or -1. */
static uint32_t FS98_STDCALL host_1f0(uint32_t id)
{
    uint32_t t = host_accel_find(ADDR(&id));
    if (t == 0)
        return (uint32_t)-1;
    host_214(t + 8);
    M32(t + 0xc) = 0;
    return 0;
}

/* +0x1f4 (0x40c3d0): append n entries to table `id`.  0, or -1. */
static uint32_t FS98_STDCALL host_1f4(uint32_t id, uint32_t entries, uint32_t n)
{
    uint32_t t = host_accel_find(ADDR(&id));
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
    uint32_t t = host_accel_find(ADDR(&id)), removed = 0, k, j, at, next, cnt;
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
    uint32_t t = host_accel_find(ADDR(&id)), c;
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
        return host_key_text(vk, buf + n, len - n - 1);
    }
    return host_2ac(buf + n, w, len - n - 1);
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
            host_key_text(vk, ADDR(text), 0x7f);
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
    if (host_ext_name(id, ADDR(ext)) == (uint32_t)-1)
        return (uint32_t)-1;
    host_20c(1, host_2b8(path) + host_2b8(ADDR(ext)) + 1, out, 0);
    host_2a8(M32(out), path);
    host_2b0(M32(out), ADDR(ext));
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
    uint32_t size = n * (dbl ? 30 : 15), buf;
    char *p;
    if (host_20c(1, size, ADDR(&buf), 0) != 0)
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
    host_214(ADDR(&buf));
    return 0;
}

/* A list written "v,v,v". */
static uint32_t host_prof_put_list(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n,
                                   uint32_t file, int dbl)
{
    char t[0x24];
    uint32_t buf, i;
    if (host_20c(1, n * (dbl ? 30 : 15), ADDR(&buf), 0) != 0)
        return (uint32_t)-1;
    M8(buf) = 0;
    for (i = 0; i < n; i++) {
        if (i != 0)
            host_2b0(buf, V(0x417ed4));
        if (dbl)
            sprintf(t, (const char *)(uintptr_t)V(0x417ed8), *(double *)(uintptr_t)(vals + 8 * i));
        else
            sprintf(t, (const char *)(uintptr_t)V(0x417a24), M32(vals + 4 * i));
        host_2b0(buf, ADDR(t));
    }
    WritePrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                               (LPCSTR)(uintptr_t)buf, (LPCSTR)(uintptr_t)file);
    host_214(ADDR(&buf));
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
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_get_int(sec, key, io, f);
    host_214(ADDR(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0d4(uint32_t sec, uint32_t key, uint32_t v, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_put_int(sec, key, v, f);
    host_214(ADDR(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0d8(uint32_t sec, uint32_t key, uint32_t out, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_get_list(sec, key, out, n, f, 0);
    host_214(ADDR(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0dc(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_put_list(sec, key, vals, n, f, 0);
    host_214(ADDR(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0e0(uint32_t sec, uint32_t key, uint32_t out, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_get_double(sec, key, out, f);
    host_214(ADDR(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0e4(uint32_t sec, uint32_t key, double v, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_put_double(sec, key, v, f);
    host_214(ADDR(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0e8(uint32_t sec, uint32_t key, uint32_t out, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_get_list(sec, key, out, n, f, 1);
    host_214(ADDR(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0ec(uint32_t sec, uint32_t key, uint32_t vals, uint32_t n,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = host_prof_put_list(sec, key, vals, n, f, 1);
    host_214(ADDR(&f));
    return r;
}

/* +0x0f0 (0x409440): a string, buf the default; returns the count. */
static uint32_t FS98_STDCALL host_0f0(uint32_t sec, uint32_t key, uint32_t buf, uint32_t size,
                                      uint32_t path, uint32_t ext)
{
    uint32_t f, r;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    r = GetPrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                                 (LPCSTR)(uintptr_t)buf, (LPSTR)(uintptr_t)buf, size,
                                 (LPCSTR)(uintptr_t)f);
    host_214(ADDR(&f));
    return r;
}

static uint32_t FS98_STDCALL host_0f4(uint32_t sec, uint32_t key, uint32_t s, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    WritePrivateProfileStringA((LPCSTR)(uintptr_t)sec, (LPCSTR)(uintptr_t)key,
                               (LPCSTR)(uintptr_t)s, (LPCSTR)(uintptr_t)f);
    host_214(ADDR(&f));
    return 0;
}

static uint32_t FS98_STDCALL host_0fc(uint32_t sec, uint32_t path, uint32_t ext)
{
    uint32_t f;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_clear(sec, f);
    host_214(ADDR(&f));
    return 0;
}

/* +0x104 (0x408d70): the output comes last here, after path and extension. */
static uint32_t FS98_STDCALL host_104(uint32_t sec, uint32_t path, uint32_t ext, uint32_t out)
{
    uint32_t f;
    M32(out) = 0;
    if (host_path_ext(ADDR(&f), path, ext) == (uint32_t)-1)
        return (uint32_t)-1;
    host_prof_section(sec, out, f);
    host_214(ADDR(&f));
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
        h = ADDR(CreateFileA((LPCSTR)(uintptr_t)M32(M32(out) + 4),
                             mode == 0 ? GENERIC_READ : mode == 1 ? GENERIC_READ | GENERIC_WRITE
                                                                  : GENERIC_WRITE,
                             0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
        M32(M32(out)) = h;
        break;
    case 3:
        h = ADDR(CreateFileA((LPCSTR)(uintptr_t)M32(M32(out) + 4), GENERIC_WRITE, 0, 0,
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
    M32(M32(out)) = ADDR(CreateFileA((LPCSTR)(uintptr_t)M32(M32(out) + 4),
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
    host_214(ADDR(&f));
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
    if (host_path_ext(ADDR(&file), path, ext) != 0)
        return (uint32_t)-1;
    a = GetFileAttributesA((LPCSTR)(uintptr_t)file);
    if (a == 0xffffffffu) {
        host_214(ADDR(&file));
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
    host_214(ADDR(&file));
    return 0;
}

/* +0x138 (0x40a2c0): set them, from the same flags.  0, or -1 if there is no
 * such file or Windows refuses. */
static uint32_t FS98_STDCALL host_138(uint32_t flags, uint32_t path, uint32_t ext)
{
    uint32_t file, a = 0;
    BOOL ok;
    if (host_path_ext(ADDR(&file), path, ext) != 0)
        return (uint32_t)-1;
    if (GetFileAttributesA((LPCSTR)(uintptr_t)file) == 0xffffffffu) {
        host_214(ADDR(&file));
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
    host_214(ADDR(&file));
    return ok ? 0 : (uint32_t)-1;
}

/* +0x158 (0x409ed0): does path + extension exist?  0, or -1. */
static uint32_t FS98_STDCALL host_158(uint32_t path, uint32_t ext)
{
    uint32_t file, r = 0;
    if (host_path_ext(ADDR(&file), path, ext) != 0)
        return (uint32_t)-1;
    if (GetFileAttributesA((LPCSTR)(uintptr_t)file) == 0xffffffffu)
        r = (uint32_t)-1;
    host_214(ADDR(&file));
    return r;
}

/* +0x11c (0x409fb0): make a directory.  0, or -1 -- leaking its copy of the
 * path when CreateDirectoryA fails. */
static uint32_t FS98_STDCALL host_11c(uint32_t path)
{
    uint32_t file = 0;
    if (host_path_ext(ADDR(&file), path, 0) != 0)
        return (uint32_t)-1;
    if (!CreateDirectoryA((LPCSTR)(uintptr_t)file, 0))
        return (uint32_t)-1;
    return host_214(ADDR(&file)) != 0 ? (uint32_t)-1 : 0;
}

/* +0x14c (0x40acb0): delete path + extension.  0, or -1. */
static uint32_t FS98_STDCALL host_14c(uint32_t path, uint32_t ext)
{
    uint32_t file;
    BOOL ok;
    if (host_path_ext(ADDR(&file), path, ext) != 0)
        return (uint32_t)-1;
    ok = DeleteFileA((LPCSTR)(uintptr_t)file);
    host_214(ADDR(&file));
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
    if (host_path_ext(ADDR(&src), from, from_ext) != 0)
        return (uint32_t)-1;
    if (host_path_ext(ADDR(&dst), to, to_ext) != 0) {
        host_214(ADDR(&src));
        return (uint32_t)-1;
    }
    ok = move ? MoveFileA((LPCSTR)(uintptr_t)src, (LPCSTR)(uintptr_t)dst)
              : CopyFileA((LPCSTR)(uintptr_t)src, (LPCSTR)(uintptr_t)dst, TRUE);
    host_214(ADDR(&src));
    host_214(ADDR(&dst));
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
    [0x11c / 4] = (void *)host_11c,
    [0x12c / 4] = (void *)host_zero1,
    [0x130 / 4] = (void *)host_130,
    [0x134 / 4] = (void *)host_134,
    [0x138 / 4] = (void *)host_138,
    [0x13c / 4] = (void *)host_13c,
    [0x144 / 4] = (void *)host_144,
    [0x148 / 4] = (void *)host_148,
    [0x14c / 4] = (void *)host_14c,
    [0x154 / 4] = (void *)host_zero0,
    [0x158 / 4] = (void *)host_158,
    [0x168 / 4] = (void *)host_168,
    [0x16c / 4] = (void *)host_16c,
    [0x178 / 4] = (void *)host_178,
    [0x17c / 4] = (void *)host_17c,
    [0x184 / 4] = (void *)host_184,
    [0x188 / 4] = (void *)host_188,
    [0x18c / 4] = (void *)host_18c,
    [0x190 / 4] = (void *)host_190,
    [0x194 / 4] = (void *)host_194,
    [0x19c / 4] = (void *)host_19c,
    [0x1a4 / 4] = (void *)host_1a4,
    [0x1a8 / 4] = (void *)host_1a8,
    [0x1ac / 4] = (void *)host_1ac,
    [0x1b0 / 4] = (void *)host_1b0,
    [0x1b4 / 4] = (void *)host_1b4,
    [0x1b8 / 4] = (void *)host_1b8,
    [0x1bc / 4] = (void *)host_1bc,
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
    [0x288 / 4] = (void *)host_288,
    [0x28c / 4] = (void *)host_28c,
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
    [0x2c8 / 4] = (void *)host_2c8,
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
