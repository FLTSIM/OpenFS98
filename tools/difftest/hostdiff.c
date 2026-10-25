/* hostdiff -- differential test of the HOST reimplementation (module id 1,
 * FLTSIM98.EXE) against the real host, both in one process.
 *
 * The host is a reloc-stripped executable that must sit at 0x400000, which wine
 * keeps committed and will not hand out or free.  So we do not load it: we take
 * the region it already owns, make it executable, and copy the file's sections
 * into place (see host.md).  Its service table then stands at 0x417ba0 with its
 * code pointers (absolute 0x4xxxxx) valid, and we can call any pure service.
 *
 * Our side is HOST_ours.dll, a normal relocatable DLL that exports its own
 * service table (HostTable) and its .data image (host_image).  Our image is a
 * byte-for-byte copy of the host's .data, so both sides start identical: after
 * a call the whole .data compares equal unless our logic wrote something the
 * host did not.  A returned (or written-back) pointer into either side's .data
 * is normalised to an offset before comparing.
 *
 * The harness itself is linked at 0x13000000 so it does not occupy 0x400000.
 *
 * Usage: hostdiff [N]        (from bin/difftest, after `make hostdiff`)
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define REAL_BASE   0x400000u
#define DATA_VA     0x417000u
#define DATA_SIZE   0x55ccu
#define DATA_END    (DATA_VA + DATA_SIZE)
#define TABLE_VA    0x417ba0u

typedef uint32_t (__stdcall *fn0)(void);
typedef uint32_t (__stdcall *fn1)(uint32_t);
typedef uint32_t (__stdcall *fn2)(uint32_t, uint32_t);

static uint32_t *real_tbl;
static void **our_tbl;
static uint8_t *our_data;               /* base of HOST_ours.dll's host_image */
static uint32_t rng = 0x1234567u;
static int differ;

/* xorshift32: unlike an LCG, its low bits are as well-mixed as its high ones,
 * so decisions like rnd() & 1 and rnd() % n get full-period coverage. */
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* An import the harness could not bind.  Nothing tested should reach one. */
static void __stdcall unbound(void)
{
    printf("hostdiff: the real host called an unbound import\n");
    exit(3);
}

/* Both sides' realloc: always a new block, the old one poisoned before it is
 * freed, so a pointer kept across a move reads garbage rather than the
 * still-intact old contents. */
static void *__cdecl moving_realloc(void *old, size_t size)
{
    size_t n;
    void *p;
    if (!old) return malloc(size);
    p = malloc(size);
    if (!p) return 0;
    n = _msize(old);
    memcpy(p, old, n < size ? n : size);
    memset(old, 0xdd, n);
    free(old);
    return p;
}

/* GetCommandLineA, both sides: a command line the harness makes up, with a
 * byte before it (the host's option finder reads one). */
static char fake_cl[512];
static char *__stdcall fake_GetCommandLineA(void) { return fake_cl + 1; }

/* Point one import of a loaded image at `fn`. */
static void hook_import(uint8_t *b, const char *dll, const char *name, void *fn)
{
    uint32_t pe = *(uint32_t *)(b + 0x3c);
    uint32_t *d = (uint32_t *)(b + *(uint32_t *)(b + pe + 0x80));
    for (; d[3] != 0; d += 5) {
        uint32_t *nm = (uint32_t *)(b + (d[0] ? d[0] : d[4]));
        uint32_t *iat = (uint32_t *)(b + d[4]);
        DWORD old;
        if (_stricmp((const char *)(b + d[3]), dll)) continue;
        for (; *nm != 0; nm++, iat++)
            if (!(*nm & 0x80000000u) && !strcmp((const char *)(b + *nm + 2), name)) {
                VirtualProtect(iat, 4, PAGE_READWRITE, &old);
                *iat = (uint32_t)(uintptr_t)fn;
                VirtualProtect(iat, 4, old, &old);
            }
    }
}

/* Bind the real host's import table as the loader would have, so the services
 * that reach MSVCRT (malloc, realloc, free, memmove) or the window system run.
 * Both sides then share one CRT heap.  A DLL or entry wine lacks (MFC42's
 * ordinals) is bound to a trap instead. */
static void bind_real(void)
{
    uint8_t *b = (uint8_t *)REAL_BASE;
    uint32_t pe = *(uint32_t *)(b + 0x3c);
    uint32_t *d = (uint32_t *)(b + *(uint32_t *)(b + pe + 0x80));

    for (; d[3] != 0; d += 5) {
        HMODULE m = LoadLibraryA((const char *)(b + d[3]));
        uint32_t *name = (uint32_t *)(b + (d[0] ? d[0] : d[4]));
        uint32_t *iat = (uint32_t *)(b + d[4]);
        for (; *name != 0; name++, iat++) {
            FARPROC f = 0;
            if (m)
                f = (*name & 0x80000000u) ? GetProcAddress(m, (LPCSTR)(uintptr_t)(*name & 0xffff))
                                          : GetProcAddress(m, (const char *)(b + *name + 2));
            *iat = f ? (uint32_t)(uintptr_t)f : (uint32_t)(uintptr_t)unbound;
        }
    }
    hook_import(b, "MSVCRT.dll", "realloc", (void *)moving_realloc);
    hook_import(b, "KERNEL32.dll", "GetCommandLineA", (void *)fake_GetCommandLineA);
}

/* Map the real host in place: make its region executable and copy the file. */
static void map_real(void)
{
    static uint8_t f[0x30000];
    FILE *fp = fopen("FLTSIM98.EXE", "rb");
    uint32_t pe, nsec, i, soi, soh;
    DWORD old;

    if (!fp) { printf("cannot open FLTSIM98.EXE\n"); exit(2); }
    fread(f, 1, sizeof f, fp);
    fclose(fp);
    pe = *(uint32_t *)(f + 0x3c);
    soi = *(uint32_t *)(f + pe + 0x50);
    soh = *(uint32_t *)(f + pe + 0x54);
    nsec = *(uint16_t *)(f + pe + 6);
    if (!VirtualProtect((void *)REAL_BASE, soi, PAGE_EXECUTE_READWRITE, &old)) {
        printf("cannot make 0x400000 executable (%lu)\n", GetLastError());
        exit(2);
    }
    memcpy((void *)REAL_BASE, f, soh);
    for (i = 0; i < nsec; i++) {
        uint8_t *s = f + pe + 24 + *(uint16_t *)(f + pe + 20) + i * 40;
        uint32_t va = *(uint32_t *)(s + 12), raw = *(uint32_t *)(s + 20), rsz = *(uint32_t *)(s + 16);
        memcpy((uint8_t *)REAL_BASE + va, f + raw, rsz);
    }
    bind_real();
    real_tbl = (uint32_t *)TABLE_VA;
    if (real_tbl[0] != 1) { printf("real host table id = %#x, not 1\n", real_tbl[0]); exit(2); }
}

static void load_ours(void)
{
    HMODULE m = LoadLibraryA("HOST_ours.dll");
    if (!m) { printf("cannot load HOST_ours.dll (%lu)\n", GetLastError()); exit(2); }
    our_tbl = (void **)GetProcAddress(m, "HostTable");
    our_data = (uint8_t *)GetProcAddress(m, "host_image");
    if (!our_tbl || !our_data) { printf("HOST_ours.dll: missing exports\n"); exit(2); }
    hook_import((uint8_t *)m, "msvcrt.dll", "realloc", (void *)moving_realloc);
    hook_import((uint8_t *)m, "KERNEL32.dll", "GetCommandLineA", (void *)fake_GetCommandLineA);
    if ((uintptr_t)our_tbl[0] != 1) { printf("our table id = %p, not 1\n", our_tbl[0]); exit(2); }
}

/* Normalise a .data pointer (either side) to a tagged offset. */
static uint32_t norm(uint32_t v)
{
    if (v >= DATA_VA && v < DATA_END)
        return 0xda000000u | (v - DATA_VA);
    if (v >= (uint32_t)(uintptr_t)our_data && v < (uint32_t)(uintptr_t)our_data + DATA_SIZE)
        return 0xda000000u | (v - (uint32_t)(uintptr_t)our_data);
    return v;
}

static void *real_fn(uint32_t off) { return (void *)real_tbl[off / 4]; }
static void *our_fn(uint32_t off)
{
    void *p = our_tbl[off / 4];
    if (!p) { printf("  +%#05x OURS MISSING\n", off); differ++; }
    return p;
}

/* Seed .data with distinct random values, identically on both sides, so a
 * service that reads the wrong offset of an otherwise-zero (or coincidentally
 * equal) global is caught.  The service table at 0xba0..0xec0 is left alone --
 * the real host dispatches through it -- and the string constant at 0x41bca0
 * (offset 0x4ca0) is given a short NUL-terminated value so the strcpy from it
 * stays bounded. */
#define TABLE_OFF   0xba0u
#define TABLE_LAST  0xec0u
#define STR_OFF     0x4ca0u            /* 0x41bca0 */
#define QUEUE_OFF   0x1380u            /* 0x418380, the removal queue */
#define QUEUE_END   0x139cu
#define EXT_OFF     0x0f20u            /* 0x417f20: the 37 built-in extension names */
#define EXT_END     0x10d0u            /* ... their strings, and the "\\\\" at 0x4180c8 */
#define CFG_OFF     0x00a0u            /* 0x4170a0: ".CFG", the last of them */
#define EMPTY_OFF   0x17f8u            /* 0x4187f8: the host's empty string (bss) */
#define REG_N_OFF   0x18acu            /* 0x4188ac: extensions registered at run time */
#define REG_P_OFF   0x18b0u            /* 0x4188b0: their _strdup'd names (heap) */

/* The run-time extension registry: a few names, the same on both sides, each
 * side's array and strings its own heap blocks (the host grows them with
 * realloc and _strdup). */
static const char *const reg_names[] = { ".XYZ", ".Q", ".ABCD", ".BGL2", ".", ".ZZ" };

static void seed_registry(void)
{
    uint32_t n = rnd() % 4, i;
    char **ra = 0, **oa = 0;
    if (n) {
        ra = malloc(n * 4); oa = malloc(n * 4);
        for (i = 0; i < n; i++) {
            const char *nm = reg_names[rnd() % 6];
            ra[i] = _strdup(nm); oa[i] = _strdup(nm);
        }
    }
    *(uint32_t *)(DATA_VA + REG_N_OFF) = n;
    *(uint32_t *)(our_data + REG_N_OFF) = n;
    *(uint32_t *)(DATA_VA + REG_P_OFF) = (uint32_t)(uintptr_t)ra;
    *(uint32_t *)(our_data + REG_P_OFF) = (uint32_t)(uintptr_t)oa;
    *(uint32_t *)(DATA_VA + EMPTY_OFF) = 0;
    *(uint32_t *)(our_data + EMPTY_OFF) = 0;
}

static void seed_data(void)
{
    uint32_t i, len;
    for (i = 0; i + 4 <= DATA_SIZE; i += 4) {
        uint32_t v;
        if (i + 4 > TABLE_OFF && i < TABLE_LAST)
            continue;
        if (i >= QUEUE_OFF && i < QUEUE_END)
            continue;
        if ((i >= EXT_OFF && i < EXT_END) || (i >= CFG_OFF && i < CFG_OFF + 8))
            continue;                   /* constant strings and pointers to them */
        if (i == EMPTY_OFF || i == REG_N_OFF || i == REG_P_OFF)
            continue;
        v = rnd();
        *(uint32_t *)(DATA_VA + i) = v;
        *(uint32_t *)(our_data + i) = v;
    }
    len = rnd() % 60;
    for (i = 0; i < len; i++) {
        uint8_t c = (uint8_t)(0x21 + rnd() % 0x5e);
        *(uint8_t *)(DATA_VA + STR_OFF + i) = c;
        *(uint8_t *)(our_data + STR_OFF + i) = c;
    }
    *(uint8_t *)(DATA_VA + STR_OFF + len) = 0;
    *(uint8_t *)(our_data + STR_OFF + len) = 0;
    /* The queue is the host's own list and stays one: emptied each iteration,
     * its capacity and data (a heap block on each side) kept. */
    *(uint32_t *)(DATA_VA + QUEUE_OFF) = 0;
    *(uint32_t *)(our_data + QUEUE_OFF) = 0;
    seed_registry();
}

/* After a call, the two .data images must be identical; report the first word
 * that is not. */
static void check_state(const char *name)
{
    uint32_t i;
    for (i = 0; i < DATA_SIZE; i += 4) {
        uint32_t rv;
        uint32_t ov;
        if (i == QUEUE_OFF + 0x18 || i == REG_P_OFF)   /* each side's own heap block */
            continue;
        rv = norm(*(uint32_t *)(DATA_VA + i));     /* real .data (mapped at 0x417000) */
        ov = norm(*(uint32_t *)(our_data + i));    /* our image */
        if (rv != ov) {
            printf("  %-16s STATE DIFFER at .data+%#x: real=%#010x ours=%#010x\n",
                   name, i, rv, ov);
            differ++;
            return;
        }
    }
    {   /* the registry's names, through each side's own array */
        uint32_t n = *(uint32_t *)(DATA_VA + REG_N_OFF);
        char **ra = *(char ***)(DATA_VA + REG_P_OFF), **oa = *(char ***)(our_data + REG_P_OFF);
        for (i = 0; i < n && i < 64; i++)
            if (strcmp(ra[i], oa[i])) {
                printf("  %-16s REGISTRY DIFFER at %u: real=\"%s\" ours=\"%s\"\n", name, i, ra[i], oa[i]);
                differ++;
                return;
            }
    }
}

/* ---- the accessors (no arguments, no writes) --------------------------------- */

struct slot { uint32_t off; const char *name; int ret_ptr; };

static const struct slot accessors[] = {
    { 0x1c, "const-one",     0 },
    { 0x20, "global-41719c", 0 },
    { 0x34, "global-418850", 0 },
    { 0x58, "past-table",    1 },
    { 0x5c, "struct-41c240", 1 },
    { 0x60, "struct-41bb80", 1 },
    { 0x154, "zero0",        0 },
    { 0x2cc, "flag-41a3cc",  0 },
};

static void check_accessor(const struct slot *s)
{
    fn0 r, o;
    uint32_t rv, ov;
    r = (fn0)real_fn(s->off);
    o = (fn0)our_fn(s->off);
    if (!o) return;
    rv = r(); ov = o();
    if (s->ret_ptr) { rv = norm(rv); ov = norm(ov); }
    if (rv != ov) {
        printf("  +%#05x %-16s DIFFER  real=%#010x ours=%#010x\n", s->off, s->name, rv, ov);
        differ++;
    }
}

/* ---- the state writers ------------------------------------------------------- */

/* +0x18: copies six fields of its argument struct into host globals. */
static void check_018(void)
{
    static uint8_t buf[0x100];
    fn1 r = (fn1)real_fn(0x18), o = (fn1)our_fn(0x18);
    uint32_t i, rv, ov;
    if (!o) return;
    for (i = 0; i < sizeof buf; i += 4) *(uint32_t *)(buf + i) = rnd();
    rv = r((uint32_t)(uintptr_t)buf);
    ov = o((uint32_t)(uintptr_t)buf);
    if (rv != ov) { printf("  +0x018 arg-copy RET DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    check_state("+0x018 arg-copy");
}

/* +0x2c: sets a host flag to 1. */
static void check_02c(void)
{
    fn0 r = (fn0)real_fn(0x2c), o = (fn0)our_fn(0x2c);
    if (!o) return;
    r(); o();
    check_state("+0x02c set-flag");
}

/* +0x54: writes two host globals back through two out-pointers. */
static void check_054(void)
{
    fn2 r = (fn2)real_fn(0x54), o = (fn2)our_fn(0x54);
    uint32_t r1 = 0, r2 = 0, o1 = 0, o2 = 0;
    if (!o) return;
    r((uint32_t)(uintptr_t)&r1, (uint32_t)(uintptr_t)&r2);
    o((uint32_t)(uintptr_t)&o1, (uint32_t)(uintptr_t)&o2);
    if (norm(r1) != norm(o1) || norm(r2) != norm(o2)) {
        printf("  +0x054 read-pair DIFFER real=(%#x,%#x) ours=(%#x,%#x)\n", r1, r2, o1, o2);
        differ++;
    }
}

/* ---- the C-runtime strings --------------------------------------------------- */

/* A random NUL-terminated string of up to max-1 printable bytes. */
static void rnd_str(char *b, uint32_t max)
{
    uint32_t len = rnd() % max, i;
    for (i = 0; i < len; i++) b[i] = (char)(0x21 + rnd() % 0x5e);
    b[len] = '\0';
}

/* +0x2b8 strlen: a null pointer, the empty string, and random strings. */
static void check_2b8(void)
{
    static char s[128];
    fn1 r = (fn1)real_fn(0x2b8), o = (fn1)our_fn(0x2b8);
    uint32_t rv, ov;
    if (!o) return;
    if (r(0) != o(0)) { printf("  +0x2b8 strlen(NULL) DIFFER\n"); differ++; }
    rnd_str(s, sizeof s);
    rv = r((uint32_t)(uintptr_t)s); ov = o((uint32_t)(uintptr_t)s);
    if (rv != ov) { printf("  +0x2b8 strlen DIFFER real=%u ours=%u\n", rv, ov); differ++; }
}

/* +0x2a8 strcpy and +0x2b0 strcat: each side copies into its own buffer, filled
 * past the string with 0xaa so an over-copy shows; compare the buffers. */
static void check_2a8(void)
{
    static char src[96], rb[192], ob[192];
    fn2 r = (fn2)real_fn(0x2a8), o = (fn2)our_fn(0x2a8);
    if (!o) return;
    rnd_str(src, sizeof src);
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    r((uint32_t)(uintptr_t)rb, (uint32_t)(uintptr_t)src);
    o((uint32_t)(uintptr_t)ob, (uint32_t)(uintptr_t)src);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x2a8 strcpy DIFFER\n"); differ++; }
}

static void check_2b0(void)
{
    static char src[96], rb[192], ob[192];
    fn2 r = (fn2)real_fn(0x2b0), o = (fn2)our_fn(0x2b0);
    uint32_t pre;
    if (!o) return;
    rnd_str(src, sizeof src);
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rnd_str(rb, 64);                         /* a prefix to append onto */
    pre = (uint32_t)strlen(rb);
    memcpy(ob, rb, pre + 1);
    memset(rb + pre + 1, 0xaa, sizeof rb - pre - 1);
    memset(ob + pre + 1, 0xaa, sizeof ob - pre - 1);
    r((uint32_t)(uintptr_t)rb, (uint32_t)(uintptr_t)src);
    o((uint32_t)(uintptr_t)ob, (uint32_t)(uintptr_t)src);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x2b0 strcat DIFFER\n"); differ++; }
}

/* ---- the block and string primitives ---------------------------------------- */

/* +0x288: stores its argument in a host global; +0x304: clears the dword its
 * argument points at (the pointer is harness memory, so only the effect is
 * compared, not the returned pointer). */
static void check_288(void)
{
    fn1 r = (fn1)real_fn(0x288), o = (fn1)our_fn(0x288);
    uint32_t v;
    if (!o) return;
    v = rnd();
    r(v); o(v);
    check_state("+0x288 set-417194");
}

static void check_304(void)
{
    fn1 r = (fn1)real_fn(0x304), o = (fn1)our_fn(0x304);
    uint32_t rc = rnd(), oc = rc;
    if (!o) return;
    r((uint32_t)(uintptr_t)&rc); o((uint32_t)(uintptr_t)&oc);
    if (rc != oc) { printf("  +0x304 clear DIFFER real=%#x ours=%#x\n", rc, oc); differ++; }
}

/* +0x130: copies a fixed host string into the caller's buffer. */
static void check_130(void)
{
    static char rb[128], ob[128];
    fn1 r = (fn1)real_fn(0x130), o = (fn1)our_fn(0x130);
    if (!o) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    r((uint32_t)(uintptr_t)rb); o((uint32_t)(uintptr_t)ob);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x130 strcpy-const DIFFER\n"); differ++; }
}

/* +0x168: the UNC-prefix predicate over crafted inputs. */
static void check_168(void)
{
    static const char *t[] = { "\\\\server", "\\x", "x\\\\", "\\", "", "ab" };
    fn1 r = (fn1)real_fn(0x168), o = (fn1)our_fn(0x168);
    uint32_t i;
    if (!o) return;
    if (r(0) != o(0)) { printf("  +0x168 UNC(NULL) DIFFER\n"); differ++; }
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (r((uint32_t)(uintptr_t)t[i]) != o((uint32_t)(uintptr_t)t[i]))
            { printf("  +0x168 UNC(\"%s\") DIFFER\n", t[i]); differ++; }
}

/* +0x218 memset, +0x228 memcmp. */
static void check_218(void)
{
    static uint8_t rb[256], ob[256];
    {
        uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t) = (void *)real_fn(0x218);
        uint32_t (__stdcall *o)(uint32_t, uint32_t, uint32_t) = (void *)our_fn(0x218);
        uint32_t val = rnd() & 0xff, cnt = rnd() % sizeof rb;
        if (!o) return;
        memset(rb, 0x11, sizeof rb); memset(ob, 0x11, sizeof ob);
        r((uint32_t)(uintptr_t)rb, val, cnt); o((uint32_t)(uintptr_t)ob, val, cnt);
        if (memcmp(rb, ob, sizeof rb)) { printf("  +0x218 memset DIFFER\n"); differ++; }
    }
}

static void check_228(void)
{
    static uint8_t a[64], b[64];
    uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t) = (void *)real_fn(0x228);
    uint32_t (__stdcall *o)(uint32_t, uint32_t, uint32_t) = (void *)our_fn(0x228);
    uint32_t i, n = 1 + rnd() % sizeof a, rv, ov;
    if (!o) return;
    for (i = 0; i < sizeof a; i++) { a[i] = (uint8_t)rnd(); b[i] = a[i]; }
    if (rnd() & 1) b[rnd() % n] ^= (uint8_t)(1 + rnd());    /* sometimes differ */
    rv = r((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b, n);
    ov = o((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b, n);
    if (rv != ov) { printf("  +0x228 memcmp DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }
}

/* +0x2bc strcmp. */
static void check_2bc(void)
{
    static char a[64], b[64];
    fn2 r = (fn2)real_fn(0x2bc), o = (fn2)our_fn(0x2bc);
    uint32_t rv, ov;
    if (!o) return;
    rnd_str(a, sizeof a);
    if (rnd() & 1) memcpy(b, a, sizeof b); else rnd_str(b, sizeof b);  /* often equal */
    rv = r((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b);
    ov = o((uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b);
    if (rv != ov) { printf("  +0x2bc strcmp DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }
}

/* +0x28c: compacts a 256-entry host table into a 0x300 buffer. */
static void check_28c(void)
{
    static uint8_t rb[0x300], ob[0x300];
    fn1 r = (fn1)real_fn(0x28c), o = (fn1)our_fn(0x28c);
    if (!o) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    r((uint32_t)(uintptr_t)rb); o((uint32_t)(uintptr_t)ob);
    if (memcmp(rb, ob, sizeof rb)) { printf("  +0x28c compact DIFFER\n"); differ++; }
}

/* The zero stubs, by argument count. */
static void check_stubs(void)
{
    fn1 s12c = (fn1)our_fn(0x12c);
    fn2 s108 = (fn2)our_fn(0x108), s220 = (fn2)our_fn(0x220), s224 = (fn2)our_fn(0x224);
    if (s12c && ((fn1)real_fn(0x12c))(rnd()) != s12c(rnd())) { printf("  +0x12c DIFFER\n"); differ++; }
    if (s108 && ((fn2)real_fn(0x108))(1, 2) != s108(1, 2)) { printf("  +0x108 DIFFER\n"); differ++; }
    if (s220 && ((fn2)real_fn(0x220))(1, 2) != s220(1, 2)) { printf("  +0x220 DIFFER\n"); differ++; }
    if (s224 && ((fn2)real_fn(0x224))(1, 2) != s224(1, 2)) { printf("  +0x224 DIFFER\n"); differ++; }
}

/* ---- the allocator ----------------------------------------------------------- */

typedef uint32_t (__stdcall *fn3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (__stdcall *fn4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (__stdcall *fn5)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#define P(x) ((uint32_t)(uintptr_t)(x))

/* +0x20c alloc, +0x210 realloc, +0x214 free.  The pointers differ by side, so
 * what is compared is the return, null-or-not, and the bytes the service
 * defines: zeroed when asked, the old contents kept across a realloc.  Only
 * flags 4 is asked to fail (a size no heap grants); any other failure is
 * fatal by design. */
static void check_alloc(void)
{
    fn4 ra = (fn4)real_fn(0x20c), oa = (fn4)our_fn(0x20c);
    fn3 rr = (fn3)real_fn(0x210), or_ = (fn3)our_fn(0x210);
    fn1 rf = (fn1)real_fn(0x214), of = (fn1)our_fn(0x214);
    uint8_t *rp = (uint8_t *)1, *op = (uint8_t *)1;
    uint32_t size = 1 + rnd() % 300, zero = rnd() & 1, nsize, i, rv, ov;
    if (!oa || !or_ || !of) return;

    rv = ra(rnd() & 1 ? 4 : 1, size, P(&rp), zero);
    ov = oa(rnd() & 1 ? 4 : 1, size, P(&op), zero);
    if (rv != ov || !rp != !op) { printf("  +0x20c alloc DIFFER real=%#x ours=%#x\n", rv, ov); differ++; return; }
    if (zero)
        for (i = 0; i < size; i++)
            if (rp[i] != op[i]) { printf("  +0x20c zero-fill DIFFER at %u\n", i); differ++; break; }
    for (i = 0; i < size; i++) rp[i] = op[i] = (uint8_t)rnd();

    nsize = 1 + rnd() % 600;
    rv = rr(rnd() & 1 ? 4 : 1, nsize, P(&rp));
    ov = or_(rnd() & 1 ? 4 : 1, nsize, P(&op));
    if (rv != ov || !rp != !op) { printf("  +0x210 realloc DIFFER real=%#x ours=%#x\n", rv, ov); differ++; return; }
    if (memcmp(rp, op, size < nsize ? size : nsize)) { printf("  +0x210 realloc contents DIFFER\n"); differ++; }

    {   /* flags 4 failing: -1, the old pointer put back */
        uint8_t *rk = rp, *ok = op;
        rv = rr(4, 0xfffffff0u, P(&rp));
        ov = or_(4, 0xfffffff0u, P(&op));
        if (rv != ov || (rp == rk) != (op == ok)) { printf("  +0x210 failing DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    }
    rv = rf(P(&rp)); ov = of(P(&op));
    if (rv != ov || rp != 0 || op != 0) { printf("  +0x214 free DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    rv = rf(P(&rp)); ov = of(P(&op));                   /* and of null */
    if (rv != ov) { printf("  +0x214 free(null) DIFFER\n"); differ++; }

    {   /* flags 4 failing on a fresh allocation */
        void *rq = (void *)1, *oq = (void *)1;
        rv = ra(4, 0xfffffff0u, P(&rq), 1);
        ov = oa(4, 0xfffffff0u, P(&oq), 1);
        if (rv != ov || rq != oq) { printf("  +0x20c failing DIFFER real=%#x ours=%#x\n", rv, ov); differ++; }
    }
}

/* +0x21c memmove, overlapping either way. */
static void check_21c(void)
{
    static uint8_t rb[256], ob[256];
    fn3 r = (fn3)real_fn(0x21c), o = (fn3)our_fn(0x21c);
    uint32_t i, n = rnd() % 128, s = rnd() % 128, d = rnd() % 128, rv, ov;
    if (!o) return;
    for (i = 0; i < sizeof rb; i++) rb[i] = ob[i] = (uint8_t)rnd();
    rv = r(P(rb + d), P(rb + s), n);
    ov = o(P(ob + d), P(ob + s), n);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) { printf("  +0x21c memmove DIFFER\n"); differ++; }
}

/* ---- the growable array ------------------------------------------------------ */

/* struct fs98_host_list.  One per side, built identically; the services see
 * only their own.  Elements are compared by contents and returned element
 * pointers as offsets into their own side's data. */
struct hlist { int32_t count, cap, grow, size, iter, pend; uint8_t *data; };
static struct hlist rl, ol;
/* A bystander pair, permanently mid-walk, so removals from it are only ever
 * queued: the queue then holds entries for two lists, and a walk's end must
 * apply only its own. */
static struct hlist rby, oby;

#define REAL_QUEUE  ((struct hlist *)(DATA_VA + QUEUE_OFF))
#define OUR_QUEUE   ((struct hlist *)(our_data + QUEUE_OFF))

static char pool[64];                   /* what an indirect list's elements point into */

/* A fresh pair.  `indirect` makes 8-byte elements whose first or second dword
 * points into the pool, so +0x310 can follow it. */
static void list_init(int indirect, uint32_t ptr_off)
{
    int32_t i;
    free(rl.data); free(ol.data);
    free(rby.data); free(oby.data);
    memset(&rby, 0, sizeof rby);
    rby.size = 4; rby.grow = 4; rby.count = rby.cap = 30; rby.iter = 1;
    oby = rby;
    rby.data = calloc(30, 4); oby.data = calloc(30, 4);
    /* Sometimes the queue grows one entry at a time, so that a flush which
     * queues again (a nested walk's end) moves it under its own feet. */
    REAL_QUEUE->grow = OUR_QUEUE->grow = 0x20;
    if (rnd() % 4 == 0) {           /* the queue is empty here (seed_data) */
        free(REAL_QUEUE->data); free(OUR_QUEUE->data);
        REAL_QUEUE->data = OUR_QUEUE->data = 0;
        REAL_QUEUE->cap = OUR_QUEUE->cap = 0;
        REAL_QUEUE->grow = OUR_QUEUE->grow = 1;
    }
    memset(&rl, 0, sizeof rl);
    rl.size = indirect ? 8 : 1 + rnd() % 24;
    rl.grow = 1 + rnd() % 6;
    rl.count = rnd() % 12;
    rl.cap = rl.count + (rnd() % 3 ? rnd() % 4 : 0);
    rl.iter = (rnd() % 8 == 0);
    ol = rl;
    if (rl.cap) {
        rl.data = malloc(rl.cap * rl.size);
        ol.data = malloc(rl.cap * rl.size);
        for (i = 0; i < rl.cap * rl.size; i++) rl.data[i] = ol.data[i] = (uint8_t)rnd();
        if (indirect)
            for (i = 0; i < rl.cap; i++) {
                uint32_t p = P(pool + rnd() % 8);
                memcpy(rl.data + i * 8 + ptr_off, &p, 4);
                memcpy(ol.data + i * 8 + ptr_off, &p, 4);
            }
    }
}

/* Removals queued against list `l` on its side's queue. */
static int32_t queued(const struct hlist *q, const struct hlist *l)
{
    int32_t i, n = 0;
    for (i = 0; i < q->count; i++)
        if (*(uint32_t *)(q->data + i * 8) == P(l)) n++;
    return n;
}

/* A removal is allowed only while the list keeps more elements than removals
 * queued against it: retail drops the count once per queued removal whatever
 * its index, and a negative count makes the next append write below the data
 * -- a caller's error, not something to test. */
static int may_remove(const struct hlist *q, const struct hlist *l, int now)
{
    return l->count > queued(q, l) + (now ? 1 : 0);
}

static void list_compare(const char *name)
{
    int32_t i;
    if (rl.count != ol.count || rl.cap != ol.cap || rl.grow != ol.grow || rl.size != ol.size ||
        rl.iter != ol.iter || rl.pend != ol.pend || !rl.data != !ol.data) {
        printf("  %-16s LIST DIFFER real={%d,%d,%d,%d,%d,%d,%s} ours={%d,%d,%d,%d,%d,%d,%s}\n", name,
               rl.count, rl.cap, rl.grow, rl.size, rl.iter, rl.pend, rl.data ? "p" : "0",
               ol.count, ol.cap, ol.grow, ol.size, ol.iter, ol.pend, ol.data ? "p" : "0");
        differ++;
        return;
    }
    if (rby.count != oby.count || rby.pend != oby.pend) {
        printf("  %-16s BYSTANDER DIFFER real=%d,%d ours=%d,%d\n", name, rby.count, rby.pend, oby.count, oby.pend);
        differ++;
        return;
    }
    if (rl.count > 0 && rl.data && memcmp(rl.data, ol.data, rl.count * rl.size)) {
        printf("  %-16s LIST CONTENTS DIFFER\n", name); differ++; return;
    }
    {   /* the queue: entries name each side's own list */
        struct hlist *rq = REAL_QUEUE, *oq = OUR_QUEUE;
        if (rq->count != oq->count || rq->cap != oq->cap) {
            printf("  %-16s QUEUE DIFFER real=%d/%d ours=%d/%d\n", name, rq->count, rq->cap, oq->count, oq->cap);
            differ++; return;
        }
        for (i = 0; i < rq->count; i++) {
            uint32_t *re = (uint32_t *)(rq->data + i * 8), *oe = (uint32_t *)(oq->data + i * 8);
            uint32_t rt = re[0] == P(&rl) ? 1 : re[0] == P(&rby) ? 2 : 0;
            uint32_t ot = oe[0] == P(&ol) ? 1 : oe[0] == P(&oby) ? 2 : 0;
            if (rt != ot || re[1] != oe[1]) {
                printf("  %-16s QUEUE ENTRY %d DIFFER\n", name, i); differ++; return;
            }
        }
    }
    check_state(name);
}

/* Offset of a returned element pointer from its list's data, -1 for null. */
static int32_t elem_off(uint32_t p, const struct hlist *l)
{
    return p ? (int32_t)(p - P(l->data)) : -1;
}

/* A walk's callback.  Each side gets its own context, and both follow one
 * script drawn before either walks, so they make the same decisions as long
 * as the lists agree; what each visit saw is logged and compared. */
#define VISITS 64
struct visit { uint32_t r, act, stop; };
static struct visit script[VISITS];

struct walkctx {
    struct hlist *l;
    struct hlist *by;
    struct hlist *q;
    int real;
    int depth;
    int n;
    int32_t log[VISITS * 2];
};

static uint32_t __stdcall walk_cb(uint32_t elem, uint32_t c)
{
    struct walkctx *w = (struct walkctx *)(uintptr_t)c;
    const struct visit *v;
    if (w->n >= VISITS) return 0x7777;
    v = &script[w->n];
    w->log[w->n * 2] = (int32_t)(elem - P(w->l->data));
    w->log[w->n * 2 + 1] = w->l->count;
    w->n++;
    switch (v->act) {
    case 1:     /* remove an element, queued since we are walking */
        if (may_remove(w->q, w->l, 0)) {
            fn2 f = (fn2)(w->real ? real_fn(0x300) : our_fn(0x300));
            f(P(w->l), P(w->l->data) + (v->r % w->l->count) * w->l->size + (v->r >> 16) % w->l->size);
        }
        break;
    case 2:     /* append */
        if (w->l->count < 40) {
            fn1 f = (fn1)(w->real ? real_fn(0x2f8) : our_fn(0x2f8));
            f(P(w->l));
        }
        break;
    case 4:     /* remove from the bystander: queued */
        if (may_remove(w->q, w->by, 0)) {
            fn2 f = (fn2)(w->real ? real_fn(0x300) : our_fn(0x300));
            f(P(w->by), P(w->by->data) + (v->r % w->by->count) * 4);
        }
        break;
    case 3:     /* a nested walk, once */
        if (w->depth == 0) {
            fn3 f = (fn3)(w->real ? real_fn(v->r & 1 ? 0x308 : 0x30c) : our_fn(v->r & 1 ? 0x308 : 0x30c));
            w->depth++;
            f(P(w->l), c, P(walk_cb));
            w->depth--;
        }
        break;
    }
    return v->stop;
}

static void list_op(void)
{
    uint32_t op = rnd() % 8, rv, ov;
    switch (op) {
    case 0: case 1: {                                   /* +0x2f8 append */
        fn1 r = (fn1)real_fn(0x2f8), o = (fn1)our_fn(0x2f8);
        int32_t i;
        if (!o || rl.count >= 40) return;
        rv = r(P(&rl)); ov = o(P(&ol));
        if (elem_off(rv, &rl) != elem_off(ov, &ol)) {
            printf("  +0x2f8 append RET DIFFER real=%d ours=%d\n", elem_off(rv, &rl), elem_off(ov, &ol));
            differ++;
        }
        list_compare("+0x2f8 append");
        if (rl.iter == 0 && rv && ov)                   /* give it contents, both alike */
            for (i = 0; i < rl.size; i++)
                ((uint8_t *)(uintptr_t)rv)[i] = ((uint8_t *)(uintptr_t)ov)[i] = (uint8_t)rnd();
        break;
    }
    case 2: {                                           /* +0x300 remove */
        fn2 r = (fn2)real_fn(0x300), o = (fn2)our_fn(0x300);
        uint32_t i, sub;
        if (!o || !may_remove(REAL_QUEUE, &rl, !rl.iter) || !may_remove(OUR_QUEUE, &ol, !ol.iter)) return;
        i = rnd() % (rl.count + 1);                     /* the end itself: a count drop, no move */
        sub = rnd() % rl.size;
        rv = r(P(&rl), P(rl.data) + i * rl.size + sub);
        ov = o(P(&ol), P(ol.data) + i * ol.size + sub);
        if ((rv > 0xffff ? 1 : rv) != (ov > 0xffff ? 1 : ov)) {   /* a queue entry, or the size, or 0 */
            printf("  +0x300 remove RET DIFFER real=%#x ours=%#x\n", rv, ov); differ++;
        }
        list_compare("+0x300 remove");
        break;
    }
    case 3: case 4: {                                   /* +0x308 / +0x30c walk */
        uint32_t s = op == 3 ? 0x308 : 0x30c, i;
        fn3 r = (fn3)real_fn(s), o = (fn3)our_fn(s);
        static struct walkctx rw, ow;
        if (!o) return;
        for (i = 0; i < VISITS; i++) {
            script[i].r = rnd();
            script[i].act = rnd() % 7;                  /* 1 remove, 2 append, 3 nest, 4 bystander */
            if (script[i].act == 3 && rnd() % 3) script[i].act = 0;
            script[i].stop = rnd() % 12 == 0 ? rnd() | 1 : 0;
        }
        memset(&rw, 0, sizeof rw); memset(&ow, 0, sizeof ow);
        rw.l = &rl; rw.by = &rby; rw.q = REAL_QUEUE; rw.real = 1;
        ow.l = &ol; ow.by = &oby; ow.q = OUR_QUEUE;
        rv = r(P(&rl), P(&rw), P(walk_cb));
        ov = o(P(&ol), P(&ow), P(walk_cb));
        if (rv != ov || rw.n != ow.n || memcmp(rw.log, ow.log, sizeof rw.log)) {
            printf("  +%#05x walk DIFFER ret real=%#x ours=%#x, visits %d/%d\n", s, rv, ov, rw.n, ow.n);
            differ++;
        }
        list_compare(op == 3 ? "+0x308 walk" : "+0x30c walk back");
        break;
    }
    case 5: case 6: {                                   /* +0x310 find */
        fn5 r = (fn5)real_fn(0x310), o = (fn5)our_fn(0x310);
        static uint8_t key[32];
        uint32_t off = rnd() % rl.size, len = rnd() % (rl.size - off + 1), i;
        if (!o) return;
        if (rl.count > 0 && rnd() % 4)                  /* usually a key some element holds */
            memcpy(key, rl.data + (rnd() % rl.count) * rl.size + off, len);
        else
            for (i = 0; i < len; i++) key[i] = (uint8_t)rnd();
        if (rnd() % 8 == 0) len = 0;                    /* matches at once */
        rv = r(P(&rl), off, P(key), len, 0);
        ov = o(P(&ol), off, P(key), len, 0);
        if (elem_off(rv, &rl) != elem_off(ov, &ol)) {
            printf("  +0x310 find DIFFER real=%d ours=%d\n", elem_off(rv, &rl), elem_off(ov, &ol));
            differ++;
        }
        break;
    }
    case 7:                                             /* +0x304 clears the count */
        if (rl.iter == 0 && rnd() % 4 == 0) {
            ((fn1)real_fn(0x304))(P(&rl)); ((fn1)our_fn(0x304))(P(&ol));
            list_compare("+0x304 clear");
        }
        break;
    }
}

/* +0x310 with `indirect`: elements hold pointers into the pool, the key is a
 * pointer to a pointer into it. */
static void check_find_indirect(void)
{
    fn5 r = (fn5)real_fn(0x310), o = (fn5)our_fn(0x310);
    uint32_t off = (rnd() & 1) * 4, len = rnd() % 9, rv, ov, i;
    char *k;
    if (!o) return;
    for (i = 0; i < sizeof pool - 1; i++) pool[i] = (char)('a' + rnd() % 3);
    list_init(1, off);
    k = pool + rnd() % 8;
    rv = r(P(&rl), off, P(&k), len, 1);
    ov = o(P(&ol), off, P(&k), len, 1);
    if (elem_off(rv, &rl) != elem_off(ov, &ol)) {
        printf("  +0x310 find-indirect DIFFER real=%d ours=%d\n", elem_off(rv, &rl), elem_off(ov, &ol));
        differ++;
    }
}

/* A run of operations on one pair, ended by +0x2fc on each side. */
static void check_lists(void)
{
    fn1 r = (fn1)real_fn(0x2fc), o = (fn1)our_fn(0x2fc);
    uint32_t n = 4 + rnd() % 16, i, rv, ov;
    list_init(0, 0);
    for (i = 0; i < n && !differ; i++)
        list_op();
    if (!o) return;
    rv = r(P(&rl)); ov = o(P(&ol));
    if ((rv == P(&rl.data) ? 1 : rv) != (ov == P(&ol.data) ? 1 : ov)) {
        printf("  +0x2fc free DIFFER real=%#x ours=%#x\n", rv, ov); differ++;
    }
    list_compare("+0x2fc free");
    rv = r(P(&rl)); ov = o(P(&ol));                     /* and with nothing to free */
    if ((rv == P(&rl.data) ? 1 : rv) != (ov == P(&ol.data) ? 1 : ov)) {
        printf("  +0x2fc free(null) DIFFER real=%#x ours=%#x\n", rv, ov); differ++;
    }
}

/* ---- the command line -------------------------------------------------------- */

static const char *const opt_names[] = { "a", "ab", "fly", "mode", "x", "" };
static const char *const opt_words[] = { "fs98", "12", "-3.5", "1e3", "abc", "c:\\fs98", "  ", "", "0x10", "7" };

/* A random command line of options in both forms, values and stray words,
 * with irregular spacing; and the byte before it, a space or not. */
static void make_cmdline(void)
{
    char *p = fake_cl + 1;
    uint32_t n = rnd() % 7, i;
    fake_cl[0] = rnd() & 1 ? ' ' : 'q';
    p += sprintf(p, "%s", rnd() % 4 ? "fltsim98.exe" : "");
    for (i = 0; i < n; i++) {
        uint32_t k = rnd() % 5;
        p += sprintf(p, "%s", rnd() % 4 ? " " : rnd() & 1 ? "   " : "");
        if (k < 3)
            p += sprintf(p, "%c%s%s%s", rnd() & 1 ? '/' : '-', opt_names[rnd() % 6],
                         rnd() % 3 ? ":" : rnd() & 1 ? " " : "", opt_words[rnd() % 10]);
        else
            p += sprintf(p, "%s", opt_words[rnd() % 10]);
    }
    if (rnd() % 4 == 0) p += sprintf(p, "  ");
}

/* +0x98 into a guarded buffer (the byte before it included -- an empty value
 * trims from there), +0x9c, +0xa0 and +0xa4, for one name. */
static void check_cmdline(void)
{
    static uint8_t rb[80], ob[80];
    const char *nm = opt_names[rnd() % 6];
    uint32_t len = 1 + rnd() % 40, rv, ov, ri, oi;
    double rd, od;
    fn3 r98 = (fn3)real_fn(0x98), o98 = (fn3)our_fn(0x98);
    fn2 r9c = (fn2)real_fn(0x9c), o9c = (fn2)our_fn(0x9c);
    fn2 ra0 = (fn2)real_fn(0xa0), oa0 = (fn2)our_fn(0xa0);
    fn1 ra4 = (fn1)real_fn(0xa4), oa4 = (fn1)our_fn(0xa4);
    if (!o98 || !o9c || !oa0 || !oa4) return;
    make_cmdline();
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rb[7] = ob[7] = rnd() & 1 ? ' ' : 'z';
    rv = r98(P(nm), P(rb + 8), len);
    ov = o98(P(nm), P(ob + 8), len);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x098 option(\"%s\", %u) DIFFER real=%d ours=%d  cl=[%s]\n", nm, len, (int)rv, (int)ov, fake_cl + 1);
        differ++;
    }
    ri = oi = rnd();
    rv = r9c(P(nm), P(&ri)); ov = o9c(P(nm), P(&oi));
    if (rv != ov || ri != oi) { printf("  +0x09c option-int DIFFER  cl=[%s]\n", fake_cl + 1); differ++; }
    rd = od = (double)rnd();
    rv = ra0(P(nm), P(&rd)); ov = oa0(P(nm), P(&od));
    if (rv != ov || memcmp(&rd, &od, 8)) { printf("  +0x0a0 option-double DIFFER  cl=[%s]\n", fake_cl + 1); differ++; }
    rv = ra4(P(nm)); ov = oa4(P(nm));
    if (rv != ov) { printf("  +0x0a4 option-given DIFFER real=%u ours=%u  cl=[%s]\n", rv, ov, fake_cl + 1); differ++; }
}

/* ---- file extensions --------------------------------------------------------- */

static const char *const ext_samples[] = {
    ".bgl", ".BGL", ".Wav", ".xyz", ".q", ".abcd", ".bgl2", ".", ".zz", ".cfg", ".SC1", ".r81", ".new", ""
};

/* +0x1ac: each side upper-cases its own copy. */
static void check_1ac(void)
{
    static char rs[16], os[16];
    fn2 r = (fn2)real_fn(0x1ac), o = (fn2)our_fn(0x1ac);
    uint32_t ri = rnd(), oi = ri, rv, ov;
    if (!o) return;
    strcpy(rs, ext_samples[rnd() % 14]); strcpy(os, rs);
    rv = r(P(rs), P(&ri)); ov = o(P(os), P(&oi));
    if (rv != ov || ri != oi || strcmp(rs, os)) {
        printf("  +0x1ac ext(\"%s\") DIFFER real=%d/%#x ours=%d/%#x\n", rs, (int)rv, ri, (int)ov, oi);
        differ++;
    }
    check_state("+0x1ac ext");
}

static const char *const path_dirs[] = { "", "c:", "C:\\", "\\\\srv", "\\\\srv\\share", "dir\\", "a\\b\\", "\\", "\\\\" };
static const char *const path_names[] = { "", "name", "x", "a.b", "scenery", "..", "n m" };

/* +0x188 over composed paths, with each part's buffer and the id sometimes
 * null. */
static void check_188(void)
{
    static char path[160], rb[3][160], ob[3][160];
    fn5 r = (fn5)real_fn(0x188), o = (fn5)our_fn(0x188);
    uint32_t ri = rnd(), oi = ri, rv, ov, k, mask = rnd();
    if (!o) return;
    sprintf(path, "%s%s%s%s", path_dirs[rnd() % 9], rnd() & 1 ? "sub\\" : "",
            path_names[rnd() % 7], ext_samples[rnd() % 14]);
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
#define PART(b, k) ((mask >> (k)) & 1 ? P((b)[k]) : 0)
    k = rnd() % 32 ? P(path) : 0;
    rv = r(k, PART(rb, 0), PART(rb, 1), PART(rb, 2), mask & 8 ? P(&ri) : 0);
    ov = o(k, PART(ob, 0), PART(ob, 1), PART(ob, 2), mask & 8 ? P(&oi) : 0);
    if (rv != ov || ri != oi || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x188 split(\"%s\") DIFFER real=%d/%#x ours=%d/%#x\n", path, (int)rv, ri, (int)ov, oi);
        differ++;
    }
    check_state("+0x188 split");
}

/* ---- the C-runtime wrappers -------------------------------------------------- */

/* A stdcall comparator on the first dword. */
static uint32_t __stdcall cmp_first(uint32_t a, uint32_t b)
{
    int32_t x = *(int32_t *)(uintptr_t)a, y = *(int32_t *)(uintptr_t)b;
    return (uint32_t)(x < y ? -1 : x > y);
}

/* +0x22c qsort (ties included: both sides run one MSVCRT qsort) and +0x234
 * bsearch over the sorted result. */
static void check_sort(void)
{
    static int32_t ra[3 * 40], oa[3 * 40];
    fn4 rq = (fn4)real_fn(0x22c), oq = (fn4)our_fn(0x22c);
    fn5 rb = (fn5)real_fn(0x234), ob = (fn5)our_fn(0x234);
    uint32_t n = rnd() % 40, w = rnd() & 1 ? 4 : 12, i, rv, ov;
    int32_t key;
    if (!oq || !ob) return;
    for (i = 0; i < 3 * 40; i++) ra[i] = oa[i] = (int32_t)(rnd() % 16) - 4;
    rq(P(ra), n, w, P(cmp_first)); oq(P(oa), n, w, P(cmp_first));
    if (memcmp(ra, oa, sizeof ra)) { printf("  +0x22c qsort DIFFER\n"); differ++; return; }
    check_state("+0x22c qsort");
    key = (int32_t)(rnd() % 18) - 5;
    rv = rb(P(&key), P(ra), n, w, P(cmp_first));
    ov = ob(P(&key), P(oa), n, w, P(cmp_first));
    if ((rv ? (int32_t)(rv - P(ra)) : -1) != (ov ? (int32_t)(ov - P(oa)) : -1)) {
        printf("  +0x234 bsearch DIFFER\n"); differ++;
    }
}

/* +0x230 swap, +0x2ac strncpy, +0x2b4 strdup, +0x2c0 _strcmpi, +0x2c8 strip. */
static void check_crt(void)
{
    static uint8_t ra[128], oa[128];
    static char rs[96], os[96], src[64];
    uint32_t rv, ov, i, n;
    fn3 r230 = (fn3)real_fn(0x230), o230 = (fn3)our_fn(0x230);
    fn3 r2ac = (fn3)real_fn(0x2ac), o2ac = (fn3)our_fn(0x2ac);
    fn1 r2b4 = (fn1)real_fn(0x2b4), o2b4 = (fn1)our_fn(0x2b4);
    fn2 r2c0 = (fn2)real_fn(0x2c0), o2c0 = (fn2)our_fn(0x2c0);
    fn2 r2c8 = (fn2)real_fn(0x2c8), o2c8 = (fn2)our_fn(0x2c8);
    if (!o230 || !o2ac || !o2b4 || !o2c0 || !o2c8) return;

    for (i = 0; i < sizeof ra; i++) ra[i] = oa[i] = (uint8_t)rnd();
    n = rnd() % 64;
    rv = r230(P(ra), P(ra + 64), n); ov = o230(P(oa), P(oa + 64), n);
    if (rv != ov || memcmp(ra, oa, sizeof ra)) { printf("  +0x230 swap DIFFER\n"); differ++; }

    rnd_str(src, rnd() & 1 ? 16 : sizeof src);
    memset(rs, 0xaa, sizeof rs); memset(os, 0xaa, sizeof os);
    n = rnd() % 80;
    rv = r2ac(P(rs), P(src), n); ov = o2ac(P(os), P(src), n);
    if (rv - P(rs) != ov - P(os) || memcmp(rs, os, sizeof rs)) { printf("  +0x2ac strncpy DIFFER\n"); differ++; }

    rv = r2b4(rnd() % 8 ? P(src) : 0);
    ov = o2b4(rv ? P(src) : 0);
    if (!rv != !ov || (rv && strcmp((char *)(uintptr_t)rv, (char *)(uintptr_t)ov))) {
        printf("  +0x2b4 strdup DIFFER\n"); differ++;
    }
    free((void *)(uintptr_t)rv); free((void *)(uintptr_t)ov);

    rnd_str(rs, 24);
    if (rnd() & 1) { strcpy(os, rs); for (i = 0; os[i]; i++) if (rnd() & 1) os[i] ^= 0x20 * ((os[i] | 0x20) >= 'a' && (os[i] | 0x20) <= 'z'); }
    else rnd_str(os, 24);
    rv = r2c0(P(rs), P(os)); ov = o2c0(P(rs), P(os));
    if (rv != ov) { printf("  +0x2c0 strcmpi DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }

    for (i = 0, n = rnd() % 60; i < n; i++) rs[i] = "ab\\ c"[rnd() % 5];
    rs[n] = 0; strcpy(os, rs);
    memset(rs + n + 1, 0x55, sizeof rs - n - 1); memset(os + n + 1, 0x55, sizeof os - n - 1);
    i = "ab\\ cz"[rnd() % 6];
    rv = r2c8(P(rs), i); ov = o2c8(P(os), i);
    if (rv - P(rs) != ov - P(os) || memcmp(rs, os, sizeof rs)) { printf("  +0x2c8 strip DIFFER\n"); differ++; }
}

int main(int argc, char **argv)
{
    uint32_t n = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 0) : 1, k, i;

    map_real();
    load_ours();
    printf("hostdiff: %u accessors + writers + strings + primitives + stubs + allocator + lists + command line + extensions + CRT, %u iteration(s)\n",
           (unsigned)(sizeof accessors / sizeof accessors[0]), n);
    for (k = 0; k < n; k++) {
        seed_data();
        for (i = 0; i < sizeof accessors / sizeof accessors[0]; i++)
            check_accessor(&accessors[i]);
        check_018();
        check_02c();
        check_054();
        check_2b8();
        check_2a8();
        check_2b0();
        check_288();
        check_304();
        check_130();
        check_168();
        check_218();
        check_228();
        check_2bc();
        check_28c();
        check_stubs();
        check_alloc();
        check_21c();
        check_lists();
        check_find_indirect();
        check_cmdline();
        check_1ac();
        check_188();
        check_sort();
        check_crt();
    }
    printf("hostdiff: %d differ\n", differ);
    return differ ? 1 : 0;
}
