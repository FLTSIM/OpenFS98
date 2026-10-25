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
    [0x108 / 4] = (void *)host_zero2,
    [0x12c / 4] = (void *)host_zero1,
    [0x130 / 4] = (void *)host_130,
    [0x154 / 4] = (void *)host_zero0,
    [0x168 / 4] = (void *)host_168,
    [0x188 / 4] = (void *)host_188,
    [0x1ac / 4] = (void *)host_1ac,
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
