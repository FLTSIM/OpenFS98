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
static int trace;                       /* HOSTDIFF_TRACE: print each profile and file operation */

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
#define EXT_END     0x1100u            /* ... their strings, the colon, separator and UNC prefix, the bytes per pixel */
#define CFG_OFF     0x00a0u            /* 0x4170a0: ".CFG", the last of them */
#define EMPTY_OFF   0x17f8u            /* 0x4187f8: the host's empty string (bss) */
#define REG_N_OFF   0x18acu            /* 0x4188ac: extensions registered at run time */
#define REG_P_OFF   0x18b0u            /* 0x4188b0: their _strdup'd names (heap) */
#define MODS_OFF    0x0b30u            /* 0x417b30: the module records, 0x14 bytes each */
#define STRMODS_OFF 0x12e0u            /* 0x4182e0: the modules strings are looked up in */
#define FMT_OFF     0x126cu            /* 0x41826c: "%s%2.2d" */
#define HINST_OFF   0x182cu            /* 0x41882c: the host's instance */
#define INAMES_OFF  0x3240u            /* 0x41a240: input names loaded; their lengths */
#define ACCEL_OFF   0x1278u            /* 0x418278: accelerator tables, a host list; id counter after */
#define BUTTON_OFF  0x1108u            /* 0x418108: "Button " (0x32 bytes) */
#define AXIS_OFF    0x1140u            /* 0x418140: "Axis " */

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

/* .data the seeding leaves alone: what the real host dispatches through or
 * treats as constant, and the state the harness sets up itself. */
static const struct { uint32_t lo, hi; } keep[] = {
    { TABLE_OFF, TABLE_LAST + 4 },      /* the service table */
    { QUEUE_OFF, QUEUE_END },           /* the removal queue (a host list) */
    { EXT_OFF, EXT_END },               /* the extension names and "\\\\" */
    { CFG_OFF, CFG_OFF + 8 },
    { EMPTY_OFF, EMPTY_OFF + 4 },
    { REG_N_OFF, REG_P_OFF + 4 },       /* the extension registry */
    { MODS_OFF, MODS_OFF + 0x1c },      /* the module records (a host list) */
    { STRMODS_OFF, STRMODS_OFF + 0x1c },/* the string modules (a host list) */
    { ACCEL_OFF, ACCEL_OFF + 0x1c },    /* the accelerator tables (a host list) */
    { FMT_OFF, FMT_OFF + 8 },           /* "%s%2.2d" */
    { HINST_OFF, HINST_OFF + 4 },       /* the host instance strings fall back on */
    { INAMES_OFF, INAMES_OFF + 0xc },   /* the input-name flag and lengths */
    { 0x5120, 0x5224 },                 /* FLTSIM98.CFG's path (seed_profile) */
    { 0x18a0, 0x18a4 },                 /* 0x4188a0: the empty double-NUL a section is cleared with */
    { 0x0a24, 0x0a28 },                 /* 0x417a24: "%d" */
    { 0x0ed0, 0x0ee0 },                 /* 0x417ed0: ", ", ",", "%f" */
    { 0x4ec0, 0x4ec4 }, { 0x4c80, 0x4c88 }, { 0x4b64, 0x4b6c }, { 0x4fe8, 0x4fec },
    { 0x4da4, 0x4da8 }, { 0x4dac, 0x4db0 }, { 0x5360, 0x5364 }, { 0x5104, 0x5108 },
                                        /* the key words' pointers (seed_keynames) */
};

static int kept(uint32_t i)
{
    uint32_t k;
    for (k = 0; k < sizeof keep / sizeof keep[0]; k++)
        if (i + 4 > keep[k].lo && i < keep[k].hi)
            return 1;
    return 0;
}

static void seed_strings(void);
static void seed_accels(void);
static void seed_keynames(void);

/* Modules with string tables, as data files: the language DLL (ENGLISH.FLL,
 * which has the input names 0x8012/0x8013), two more, and the real host's
 * own image (string 9). */
static uint32_t strmod[4];

static void load_strmods(void)
{
    static const char *const f[] = { "ENGLISH.FLL", "SIMSUI.DLL", "DIALOG.DLL" };
    uint32_t i;
    for (i = 0; i < 3; i++) {
        strmod[i] = (uint32_t)(uintptr_t)LoadLibraryExA(f[i], 0, LOAD_LIBRARY_AS_DATAFILE);
        if (!strmod[i]) { printf("cannot load %s as data (%lu)\n", f[i], GetLastError()); exit(2); }
    }
    strmod[3] = REAL_BASE;
}

/* A host list header {count, cap, grow, size, 0, 0, data} on both sides, the
 * data a heap copy each of `bytes`. */
static void seed_list(uint32_t off, uint32_t n, uint32_t size, const void *bytes)
{
    uint32_t h[7] = { n, n, 0x20, size, 0, 0, 0 };
    uint8_t *r = 0, *o = 0;
    if (n) {
        r = malloc(n * size); o = malloc(n * size);
        memcpy(r, bytes, n * size); memcpy(o, bytes, n * size);
    }
    h[6] = (uint32_t)(uintptr_t)r; memcpy((void *)(DATA_VA + off), h, sizeof h);
    h[6] = (uint32_t)(uintptr_t)o; memcpy(our_data + off, h, sizeof h);
}

static const char *const prefixes[] = { "Button ", "Axis ", "Btn", "", "A" };

/* The string modules, the module records, the host instance, and the input
 * names -- either still to load (flag 0) or loaded as some short prefixes. */
static void seed_strings(void)
{
    uint32_t mods[4], recs[5 * 5], n, m, i, v;
    n = rnd() % 5;
    for (i = 0; i < n; i++) mods[i] = strmod[rnd() % 4];
    if (n == 4 && rnd() & 1) n = 3;
    seed_list(STRMODS_OFF, n, 4, mods);
    m = rnd() % 6;
    for (i = 0; i < m; i++) {
        recs[i * 5 + 0] = rnd();
        recs[i * 5 + 1] = rnd();
        recs[i * 5 + 2] = strmod[rnd() % 4];
        recs[i * 5 + 3] = 1 + rnd() % 8;
        recs[i * 5 + 4] = rnd() % 3 ? 1 + rnd() % 3 : 0;
    }
    seed_list(MODS_OFF, m, 0x14, recs);
    /* the host instance: the real host's image, or (as if the host's own
     * resources were the language DLL's) ENGLISH.FLL, which has accelerators */
    *(uint32_t *)(DATA_VA + HINST_OFF) = *(uint32_t *)(our_data + HINST_OFF) = rnd() % 4 ? REAL_BASE : strmod[0];
    v = rnd() & 1;
    *(uint32_t *)(DATA_VA + INAMES_OFF) = *(uint32_t *)(our_data + INAMES_OFF) = v;
    if (v) {
        const char *b = prefixes[rnd() % 5], *a = prefixes[rnd() % 5];
        strcpy((char *)(DATA_VA + BUTTON_OFF), b); strcpy((char *)(our_data + BUTTON_OFF), b);
        strcpy((char *)(DATA_VA + AXIS_OFF), a); strcpy((char *)(our_data + AXIS_OFF), a);
        v = rnd() % 8;
        *(uint32_t *)(DATA_VA + INAMES_OFF + 4) = *(uint32_t *)(our_data + INAMES_OFF + 4) = v;
        v = rnd() % 8;
        *(uint32_t *)(DATA_VA + INAMES_OFF + 8) = *(uint32_t *)(our_data + INAMES_OFF + 8) = v;
    }
}

static void seed_data(void)
{
    uint32_t i, len;
    for (i = 0; i + 4 <= DATA_SIZE; i += 4) {
        uint32_t v;
        if (kept(i))
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
    seed_strings();
    seed_accels();
    seed_keynames();
}

/* After a call, the two .data images must be identical; report the first word
 * that is not. */
static void check_state(const char *name)
{
    uint32_t i;
    for (i = 0; i < DATA_SIZE; i += 4) {
        uint32_t rv;
        uint32_t ov;
        if (i == QUEUE_OFF + 0x18 || i == REG_P_OFF || i == MODS_OFF + 0x18 || i == STRMODS_OFF + 0x18 ||
            i == ACCEL_OFF + 0x18 || (i >= 0x5120 && i < 0x5224))   /* the CFG paths differ in a letter */
            /* each side's own heap block */
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
    {   /* the two host lists the harness seeds: contents through each side's data */
        static const uint32_t lists[] = { MODS_OFF, STRMODS_OFF };
        for (i = 0; i < 2; i++) {
            uint32_t n = *(uint32_t *)(DATA_VA + lists[i]), sz = *(uint32_t *)(DATA_VA + lists[i] + 0xc);
            uint8_t *rd = *(uint8_t **)(DATA_VA + lists[i] + 0x18), *od = *(uint8_t **)(our_data + lists[i] + 0x18);
            if (n && n < 64 && sz <= 0x14 && (!rd || !od || memcmp(rd, od, n * sz))) {
                printf("  %-16s LIST .data+%#x CONTENTS DIFFER\n", name, lists[i]);
                differ++;
                return;
            }
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
        uint32_t dense = rnd() % 4 == 0;    /* mostly removals and nesting: several queued at once */
        for (i = 0; i < VISITS; i++) {
            script[i].r = rnd();
            script[i].act = rnd() % 7;                  /* 1 remove, 2 append, 3 nest, 4 bystander */
            if (script[i].act == 3 && rnd() % 3 && !dense) script[i].act = 0;
            if (dense && rnd() % 2) script[i].act = rnd() % 3 ? 1 : 3;
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

/* ---- string resources -------------------------------------------------------- */

/* Ids worth asking for: the input names, strings only ENGLISH.FLL, SIMSUI or
 * DIALOG has, the host's own 9, and ones nobody has. */
static const uint32_t str_ids[] = { 9, 0x8012, 0x8013, 0x2201, 0x73, 0xb000, 0xb001, 0xf000, 0xf001,
                                    0xf100, 0xf800, 0xf8d0, 1, 0x5c, 0x7fff };

/* +0x2a0 into a guarded buffer of random length, +0x2a4. */
static void check_strings(void)
{
    static char rb[0x180], ob[0x180];
    fn3 r = (fn3)real_fn(0x2a0), o = (fn3)our_fn(0x2a0);
    fn2 r4 = (fn2)real_fn(0x2a4), o4 = (fn2)our_fn(0x2a4);
    uint32_t id = str_ids[rnd() % 15], len = rnd() % 4 ? 0x100 : rnd() % 12, rv, ov;
    char *rp = (char *)1, *op = (char *)1;
    if (!o || !o4) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rv = r(id, P(rb), len); ov = o(id, P(ob), len);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x2a0 string(%#x, %u) DIFFER real=%u ours=%u\n", id, len, rv, ov); differ++;
    }
    check_state("+0x2a0 string");
    rv = r4(id, P(&rp)); ov = o4(id, P(&op));
    if (rv != ov || !rp != !op || (rp && strcmp(rp, op))) {
        printf("  +0x2a4 string-dup(%#x) DIFFER real=%u ours=%u\n", id, rv, ov); differ++;
    }
    free(rp); free(op);
    check_state("+0x2a4 string-dup");
}

/* +0x298 (a module's HMODULE onto the string list) and +0x29c (one off it). */
static void check_strmods(void)
{
    fn2 r8 = (fn2)real_fn(0x298), o8 = (fn2)our_fn(0x298);
    fn1 rc = (fn1)real_fn(0x29c), oc = (fn1)our_fn(0x29c);
    uint32_t id = rnd() % 10, ro = rnd(), oo = ro, rv, ov, h;
    if (!o8 || !oc) return;
    if (*(uint32_t *)(DATA_VA + STRMODS_OFF) < 12) {
        rv = r8(id, P(&ro)); ov = o8(id, P(&oo));
        if (rv != ov || ro != oo) { printf("  +0x298 add-strings(%u) DIFFER real=%d/%#x ours=%d/%#x\n", id, (int)rv, ro, (int)ov, oo); differ++; }
        check_state("+0x298 add-strings");
    }
    h = rnd() % 5 ? strmod[rnd() % 4] : rnd();
    rv = rc(h); ov = oc(h);
    if (rv != ov) { printf("  +0x29c drop-strings DIFFER real=%d ours=%d\n", (int)rv, (int)ov); differ++; }
    check_state("+0x29c drop-strings");
}

/* +0x1d0 (an input's name) and +0x1d4 (a name's input). */
static void check_inputs(void)
{
    static char rb[96], ob[96], name[48];
    fn3 r0 = (fn3)real_fn(0x1d0), o0 = (fn3)our_fn(0x1d0);
    fn2 r4 = (fn2)real_fn(0x1d4), o4 = (fn2)our_fn(0x1d4);
    int32_t code = (int32_t)(rnd() % 0x58) - 4;
    uint32_t rv, ov, ro, oo;
    if (!o0 || !o4) return;
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rv = r0((uint32_t)code, P(rb), rnd()); ov = o0((uint32_t)code, P(ob), rnd());
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x1d0 input-name(%d) DIFFER real=%#x ours=%#x [%s] [%s]\n", code, rv, ov, rb, ob); differ++;
    }
    check_state("+0x1d0 input-name");
    if (rnd() & 1)
        strcpy(name, rb);                       /* a name it made */
    else
        sprintf(name, "%s%s%d", prefixes[rnd() % 5], rnd() & 1 ? prefixes[rnd() % 5] : "",
                (int)(rnd() % 0x50) - 3);
    ro = oo = rnd();
    rv = r4(P(name), P(&ro)); ov = o4(P(name), P(&oo));
    if (rv != ov || ro != oo) {
        printf("  +0x1d4 input-code(\"%s\") DIFFER real=%#x/%d ours=%#x/%d\n", name, rv, (int)ro, ov, (int)oo); differ++;
    }
    check_state("+0x1d4 input-code");
}

/* ---- names and paths --------------------------------------------------------- */

/* +0x178 and +0x18c over strings drawn from a set that has every refused
 * character, a space, high bytes, and sometimes nothing. */
static void check_names(void)
{
    static const char cs[] = "aZ09._-\"&*/:<>?\\|! }~#{\x80\xe9\xff";
    static char s[24];
    fn1 r8 = (fn1)real_fn(0x178), o8 = (fn1)our_fn(0x178);
    fn1 rc = (fn1)real_fn(0x18c), oc = (fn1)our_fn(0x18c);
    uint32_t n = rnd() % 8, i, p;
    if (!o8 || !oc) return;
    for (i = 0; i < n; i++)
        s[i] = rnd() % 3 ? cs[rnd() % 7] : cs[rnd() % (sizeof cs - 1)];
    s[n] = 0;
    p = rnd() % 32 ? P(s) : 0;
    if (r8(p) != o8(p)) { printf("  +0x178 file-name(\"%s\") DIFFER\n", s); differ++; }
    if (rc(p) != oc(p)) { printf("  +0x18c path-name(\"%s\") DIFFER\n", s); differ++; }
}

/* +0x184 over parts with and without separators, sometimes long enough not
 * to fit. */
static void check_184(void)
{
    static const char *const drives[] = { "", "c", "C:", "c:\\", "\\\\srv", "ab", "\\" };
    static const char *const parts[] = { "", "fs98", "\\fs98", "fs98\\", "a\\b\\", "\\", "x.bgl", "\\\\" };
    static char dir[200], name[300], rb[0x120], ob[0x120];
    fn4 r = (fn4)real_fn(0x184), o = (fn4)our_fn(0x184);
    uint32_t rv, ov, k;
    const char *d;
    if (!o) return;
    d = drives[rnd() % 7];
    strcpy(dir, parts[rnd() % 8]);
    strcpy(name, parts[rnd() % 8]);
    if (rnd() % 4 == 0) { k = strlen(dir); memset(dir + k, 'd', 60 + rnd() % 130); dir[k + 60 + rnd() % 130] = 0; dir[199] = 0; }
    if (rnd() % 4 == 0) { k = strlen(name); memset(name + k, 'n', 40 + rnd() % 150); name[k + 40 + rnd() % 150] = 0; name[199] = 0; }
    if (rnd() % 8 == 0) {           /* a result right at the limit: dir plus name */
        k = 0x102 + rnd() % 5;
        strcpy(dir, "d"); memset(name, 'n', k - 2); name[k - 2] = 0;
        d = "";
    }
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    k = rnd();
    rv = r(k % 32 ? P(rb) : 0, k & 32 ? P(d) : 0, k & 64 ? P(dir) : 0, k & 128 ? P(name) : 0);
    ov = o(k % 32 ? P(ob) : 0, k & 32 ? P(d) : 0, k & 64 ? P(dir) : 0, k & 128 ? P(name) : 0);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x184 join(\"%s\",\"%.20s\",\"%.20s\") DIFFER real=%d ours=%d\n", d, dir, name, (int)rv, (int)ov);
        differ++;
    }
}

/* ---- image buffers ----------------------------------------------------------- */

/* Compare two images: the header but its pointers, the pixels, and the row
 * table as offsets into the pixels. */
static int img_differ(const uint32_t *r, const uint32_t *o)
{
    int32_t i;
    if (!r != !o) return 1;
    if (!r) return 0;
    if (r[0] != o[0] || r[1] != o[1] || r[2] != o[2] || r[3] != o[3] || r[5] != o[5]) return 1;
    if (!r[6] != !o[6] || !r[4] != !o[4]) return 1;
    if (r[6] && memcmp((void *)(uintptr_t)r[6], (void *)(uintptr_t)o[6], r[5])) return 1;
    for (i = 0; r[4] && i < (int32_t)r[2]; i++)
        if (((uint32_t *)(uintptr_t)r[4])[i] - r[6] != ((uint32_t *)(uintptr_t)o[4])[i] - o[6]) return 1;
    return 0;
}

/* +0x1b4 create (every format, and some that are not), scribble, +0x1bc
 * resize, +0x1b8 free (and of null). */
static void check_images(void)
{
    fn4 rmk = (fn4)real_fn(0x1b4), omk = (fn4)our_fn(0x1b4);
    fn3 rrs = (fn3)real_fn(0x1bc), ors = (fn3)our_fn(0x1bc);
    fn1 rfr = (fn1)real_fn(0x1b8), ofr = (fn1)our_fn(0x1b8);
    uint32_t *ri = (uint32_t *)1, *oi = (uint32_t *)1, rv, ov, w, h, f, i;
    if (!omk || !ors || !ofr) return;
    f = (uint32_t)((int32_t)(rnd() % 9) - 1);
    w = rnd() % 41; h = rnd() % 21;
    rv = rmk(P(&ri), w, h, f); ov = omk(P(&oi), w, h, f);
    if (rv != ov || (rv == 0 && img_differ(ri, oi)) || (rv && ri != oi)) {
        printf("  +0x1b4 image(%u,%u,%d) DIFFER real=%d ours=%d\n", w, h, (int)f, (int)rv, (int)ov); differ++; return;
    }
    if (rv) return;
    for (i = 0; i < ri[5]; i++)
        ((uint8_t *)(uintptr_t)ri[6])[i] = ((uint8_t *)(uintptr_t)oi[6])[i] = (uint8_t)rnd();
    w = rnd() % 41; h = rnd() % 21;
    rv = rrs(P(ri), w, h); ov = ors(P(oi), w, h);
    if (rv != ov || img_differ(ri, oi)) { printf("  +0x1bc resize(%u,%u) DIFFER\n", w, h); differ++; }
    rv = rfr(P(ri)); ov = ofr(P(oi));
    if (rv != ov) { printf("  +0x1b8 free DIFFER\n"); differ++; }
    if (rfr(0) != ofr(0)) { printf("  +0x1b8 free(null) DIFFER\n"); differ++; }
}

/* ---- accelerator tables ------------------------------------------------------ */

/* A few entries for tables to hold, so that removals find matches. */
static const uint32_t accel_pool[6][3] = {
    { 0x9c40, 8, 0x10041 }, { 0x9c41, 9, 0x10042 }, { 0x9c40, 8, 0x10042 },
    { 7, 0, 0x10070 }, { 0x9c40, 2, 0x10041 }, { 1, 1, 1 },
};

/* The table list: up to four tables of up to six pool entries each. */
static void seed_accels(void)
{
    uint32_t recs[4 * 4], m = rnd() % 5, i, j, n;
    uint8_t *ra[4], *oa[4];
    for (i = 0; i < m; i++) {
        n = rnd() % 7;
        recs[i * 4 + 0] = rnd() & 1;
        recs[i * 4 + 1] = 1 + rnd() % 12;
        recs[i * 4 + 3] = n;
        ra[i] = oa[i] = 0;
        if (n || rnd() & 1) {
            ra[i] = malloc(n * 12 + 1); oa[i] = malloc(n * 12 + 1);
            for (j = 0; j < n; j++) {
                const uint32_t *e = accel_pool[rnd() % 6];
                memcpy(ra[i] + 12 * j, e, 12); memcpy(oa[i] + 12 * j, e, 12);
            }
        }
        recs[i * 4 + 2] = (uint32_t)(uintptr_t)ra[i];
    }
    seed_list(ACCEL_OFF, m, 0x10, recs);
    for (i = 0; i < m; i++)
        (*(uint32_t **)(our_data + ACCEL_OFF + 0x18))[i * 4 + 2] = (uint32_t)(uintptr_t)oa[i];
    if (rnd() % 8 == 0)     /* the id counter about to wrap, or at 0 (the next table's id) */
        *(uint32_t *)(DATA_VA + ACCEL_OFF + 0x1c) = *(uint32_t *)(our_data + ACCEL_OFF + 0x1c) =
            rnd() & 1 ? 0xffffffffu : 0;
}

/* The tables, field by field, entries through each side's own pointer. */
static int accels_differ(void)
{
    uint32_t n = *(uint32_t *)(DATA_VA + ACCEL_OFF), i;
    uint32_t *r = *(uint32_t **)(DATA_VA + ACCEL_OFF + 0x18), *o = *(uint32_t **)(our_data + ACCEL_OFF + 0x18);
    if (n != *(uint32_t *)(our_data + ACCEL_OFF)) return 1;
    for (i = 0; i < n; i++) {
        uint32_t *rt = r + 4 * i, *ot = o + 4 * i;
        if (rt[0] != ot[0] || rt[1] != ot[1] || rt[3] != ot[3] || !rt[2] != !ot[2]) return 1;
        if (rt[2] && rt[3] < 4096 && memcmp((void *)(uintptr_t)rt[2], (void *)(uintptr_t)ot[2], rt[3] * 12)) return 1;
    }
    return 0;
}

/* An id worth asking about: one a table has, 0, or one nobody has. */
static uint32_t accel_id(void)
{
    uint32_t n = *(uint32_t *)(DATA_VA + ACCEL_OFF);
    uint32_t *r = *(uint32_t **)(DATA_VA + ACCEL_OFF + 0x18);
    if (n && rnd() % 4) return r[4 * (rnd() % n) + 1];
    return rnd() % 3 ? rnd() % 20 : 0;
}

static void check_accels(void)
{
    static uint32_t rbuf[512 * 3], obuf[512 * 3], ents[8 * 3];
    uint32_t ops = 1 + rnd() % 6, k;
    for (k = 0; k < ops && *(uint32_t *)(DATA_VA + ACCEL_OFF) < 12; k++) {
        uint32_t op = rnd() % 10, s = 0x1d8 + 4 * op, rv = 0, ov = 0, ro, oo, id = accel_id(), i, n;
        void *of = our_fn(s);
        void *rf = real_fn(s);
        if (!of) return;
        ro = oo = rnd();
        switch (op) {
        case 0:                         /* +0x1d8 new */
            rv = ((fn1)rf)(P(&ro)); ov = ((fn1)of)(P(&oo));
            break;
        case 1:                         /* +0x1dc destroy (through a pointer it zeroes) */
            ro = oo = id;
            rv = ((fn1)rf)(P(&ro)); ov = ((fn1)of)(P(&oo));
            break;
        case 2:                         /* +0x1e0 by index */
            i = rnd() % 6;
            rv = ((fn2)rf)(P(&ro), i); ov = ((fn2)of)(P(&oo), i);
            break;
        case 3:                         /* +0x1e4 from a resource */
            i = rnd() % 4 ? 103 + rnd() % 3 : rnd() % 300;
            rv = ((fn2)rf)(P(&ro), i); ov = ((fn2)of)(P(&oo), i);
            break;
        case 4: case 5: case 6:         /* +0x1e8 enable, +0x1ec disable, +0x1f0 clear */
            rv = ((fn1)rf)(id); ov = ((fn1)of)(id);
            break;
        case 7: case 8:                 /* +0x1f4 add, +0x1f8 remove */
            n = rnd() % 5;
            for (i = 0; i < n; i++) memcpy(ents + 3 * i, accel_pool[rnd() % 6], 12);
            rv = ((fn3)rf)(id, P(ents), n); ov = ((fn3)of)(id, P(ents), n);
            break;
        case 9: {                       /* +0x1fc read */
            uint32_t (__stdcall *r6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = (void *)rf;
            uint32_t (__stdcall *o6)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = (void *)of;
            uint32_t start = rnd() % 8, mode = rnd() % 4 ? 2 : rnd() % 4, b = rnd() % 4;
            memset(rbuf, 0xaa, sizeof rbuf); memset(obuf, 0xaa, sizeof obuf);
            rv = r6(id, rnd(), start, mode, b ? P(rbuf) : 0, P(&ro));
            ov = o6(id, rnd(), start, mode, b ? P(obuf) : 0, P(&oo));
            if (memcmp(rbuf, obuf, sizeof rbuf)) {
                for (i = 0; i < 512 * 3 && rbuf[i] == obuf[i]; i++) ;
                printf("  +0x1fc read(id %u, start %u, mode %u) DIFFER at dword %u: real=%#x ours=%#x, n %u/%u\n",
                       id, start, mode, i, rbuf[i], obuf[i], ro, oo);
                differ++;
            }
            break;
        }
        }
        if (rv != ov || ro != oo) {
            printf("  +%#05x accel op DIFFER real=%d/%#x ours=%d/%#x\n", s, (int)rv, ro, (int)ov, oo); differ++;
        }
        if (accels_differ()) { printf("  +%#05x accel TABLES DIFFER\n", s); differ++; }
        check_state("accelerators");
        if (differ) return;
    }
}

/* ---- key names --------------------------------------------------------------- */

/* The host's key words, as its init loads them from ENGLISH.FLL (0x9520 on),
 * and the slots it keeps their copies in; the harness points both sides'
 * slots at one set of strings (pointers outside .data compare as they are). */
static const uint32_t kn_slot[10] = { 0x41bec0, 0x41bc80, 0x41bb64, 0x41bfe8, 0x41bb68,
                                      0x41bdac, 0x41bda4, 0x41c360, 0x41bc84, 0x41c104 };
static const char *const kn_text[10] = { "CTRL + ", "SHIFT + ", "ALT + ", "NumLock", "Num Enter",
                                         "Num /", "Break", "Menu", "Right Windows", "Left Windows" };

static void seed_keynames(void)
{
    uint32_t i;
    for (i = 0; i < 10; i++) {
        const char *t = kn_text[i];
        if (i >= 3 && rnd() % 16 == 0) t = kn_text[rnd() % 10];  /* now and then a key word out of place */
        *(uint32_t *)(DATA_VA + kn_slot[i] - DATA_VA) = (uint32_t)(uintptr_t)t;
        *(uint32_t *)(our_data + kn_slot[i] - DATA_VA) = (uint32_t)(uintptr_t)t;
    }
}

/* +0x200 over key codes (every virtual key, any modifiers) into a guarded
 * buffer of random length; +0x204 over the names it made and some others. */
static void check_keynames(void)
{
    static char rb[96], ob[96], name[96];
    static const char *const odd[] = { "", "ctrl + a", "CTRL + ", "Num Enterprise", "F1", "Space", "q", "CTRL + SHIFT + ALT + Break" };
    fn3 r0 = (fn3)real_fn(0x200), o0 = (fn3)our_fn(0x200);
    fn2 r4 = (fn2)real_fn(0x204), o4 = (fn2)our_fn(0x204);
    uint32_t key = (rnd() % 8) << 8 | rnd() % 0x100, len = rnd() % 4 ? 64 : 23 + rnd() % 20;
    /* (len must leave room past the modifier words, up to 21 bytes: retail
     * passes len - used - 1 on unchecked, and a negative count runs strncpy's
     * zero fill off the end of memory) */
    uint32_t rv, ov, ro, oo;
    if (!o0 || !o4) return;
    if (rnd() % 4 == 0) key = (key & 0x700) | (uint32_t)"\x00\x03\x13\x5b\x5c\x5d\x6f\x87\x90"[rnd() % 9];
    memset(rb, 0xaa, sizeof rb); memset(ob, 0xaa, sizeof ob);
    rv = r0(key, P(rb), len); ov = o0(key, P(ob), len);
    if (rv > 0x10000) rv -= P(rb);
    if (ov > 0x10000) ov -= P(ob);
    if (rv != ov || memcmp(rb, ob, sizeof rb)) {
        printf("  +0x200 key-name(%#x) DIFFER real=%#x ours=%#x [%s] [%s]\n", key, rv, ov, rb, ob); differ++;
    }
    if (rnd() % 4) { memcpy(name, rb, sizeof name); name[sizeof name - 1] = 0; }
    else strcpy(name, odd[rnd() % 8]);
    ro = oo = rnd();
    rv = r4(P(name), P(&ro)); ov = o4(P(name), P(&oo));
    if (rv != ov || ro != oo) {
        printf("  +0x204 key-code(\"%s\") DIFFER real=%d/%#x ours=%d/%#x\n", name, (int)rv, ro, (int)ov, oo); differ++;
    }
}

/* ---- profiles ---------------------------------------------------------------- */

/* Each side has its own copy of a made-up profile, in hostfs\r and hostfs\o
 * (paths of one length, differing in that letter), under a new name each
 * iteration so wine's profile cache never serves a stale copy.  The CFG path
 * at 0x41c120 points each side at its own; the path+extension services are
 * given each side's own path. */
#define CFGPATH_OFF 0x5120u            /* 0x41c120: FLTSIM98.CFG's full path */

static char fs_base[MAX_PATH];         /* ...\hostfs\ */
static char prof_r[MAX_PATH], prof_o[MAX_PATH];   /* without the extension */
static uint32_t prof_seq;

static void fs_init(void)
{
    GetCurrentDirectoryA(sizeof fs_base - 16, fs_base);
    strcat(fs_base, "\\hostfs\\");
    CreateDirectoryA(fs_base, 0);
    sprintf(prof_r, "%sr", fs_base); CreateDirectoryA(prof_r, 0);
    sprintf(prof_o, "%so", fs_base); CreateDirectoryA(prof_o, 0);
}

static const char *const prof_sections[] = { "Main", "Lists", "Empty", "Nope", "main" };
static const char *const prof_keys[] = { "a", "b", "list", "dbl", "s", "missing", "A" };
static const char *const prof_values[] = {
    "12", "-5", "0x1f", "077", "1, 2,3", "4,,5", "1.5", "2.25, -3e2,7", " 9 ", "", "abc", "3.14159",
    "100000000000", "1e5, 2e-3", "-0", "7,8,9,10,11,12,13"
};

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static int files_differ(const char *a, const char *b)
{
    static char ba[8192], bb[8192];
    FILE *f;
    size_t na = 0, nb = 0;
    if ((f = fopen(a, "rb"))) { na = fread(ba, 1, sizeof ba, f); fclose(f); }
    if ((f = fopen(b, "rb"))) { nb = fread(bb, 1, sizeof bb, f); fclose(f); }
    return na != nb || memcmp(ba, bb, na);
}

/* A fresh pair of profiles; the CFG path on each side. */
static void seed_profile(void)
{
    static char text[2048], rcfg[MAX_PATH], ocfg[MAX_PATH];
    uint32_t s, k, n;
    char *p = text;
    if (prof_seq) {                     /* the last pair */
        sprintf(rcfg, "%sr\\P%07u.CFG", fs_base, prof_seq); DeleteFileA(rcfg);
        sprintf(ocfg, "%so\\P%07u.CFG", fs_base, prof_seq); DeleteFileA(ocfg);
    }
    prof_seq++;
    for (s = 0; s < 3; s++) {
        if (rnd() % 4 == 0) continue;
        p += sprintf(p, "[%s]\r\n", prof_sections[s]);
        n = s == 2 ? 0 : 1 + rnd() % 5;
        for (k = 0; k < n; k++)
            p += sprintf(p, "%s=%s\r\n", prof_keys[rnd() % 6], prof_values[rnd() % 16]);
    }
    sprintf(prof_r, "%sr\\P%07u", fs_base, prof_seq);
    sprintf(prof_o, "%so\\P%07u", fs_base, prof_seq);
    sprintf(rcfg, "%s.CFG", prof_r); sprintf(ocfg, "%s.CFG", prof_o);
    write_file(rcfg, text); write_file(ocfg, text);
    strcpy((char *)(DATA_VA + CFGPATH_OFF), rcfg);
    strcpy((char *)(our_data + CFGPATH_OFF), ocfg);
}

/* One random profile service on each side; outputs and both files compared. */
static void check_profiles(void)
{
    static char rbuf[256], obuf[256], rcfg[MAX_PATH], ocfg[MAX_PATH];
    static uint8_t rout[64], oout[64], vals[64];
    uint32_t ops = 1 + rnd() % 3, k;
    sprintf(rcfg, "%s.CFG", prof_r); sprintf(ocfg, "%s.CFG", prof_o);
    for (k = 0; k < ops; k++) {
        uint32_t s = 0xa8 + 4 * (rnd() % 0x18), i, n = rnd() % 7, rv = 0, ov = 0, ext;
        uint32_t sec = P(prof_sections[rnd() % 5]), key = P(prof_keys[rnd() % 7]);
        void *rf = real_fn(s), *of = our_fn(s);
        double d = (double)(int32_t)(rnd() % 2000000 - 1000000) / (1 + rnd() % 64);
        const char *rp = prof_r, *op = prof_o;
        if (!of) return;
        if (trace) printf("prof +%#x n=%u\n", s, n);
        ext = rnd() % 8 ? 0x24 : rnd() & 1 ? 999 : 0xfffffffeu;
        if (ext == 0xfffffffeu) { rp = rcfg; op = ocfg; }
        for (i = 0; i < sizeof vals; i++) vals[i] = (uint8_t)rnd();
        for (i = 0; i < 8; i++) ((double *)vals)[i] = (double)(int32_t)(rnd() % 20000 - 10000) / 8;
        memcpy(rout, vals, sizeof rout); memcpy(oout, vals, sizeof oout);
        memset(rbuf, 0, sizeof rbuf); memset(obuf, 0, sizeof obuf);
        if (rnd() & 1) { strcpy(rbuf, "dflt"); strcpy(obuf, "dflt"); }
        switch (s) {
        case 0xa8: case 0xb8: rv = ((fn3)rf)(sec, key, P(rout)); ov = ((fn3)of)(sec, key, P(oout)); break;
        case 0xac: rv = ((fn3)rf)(sec, key, vals[0] * 1000u - 7); ov = ((fn3)of)(sec, key, vals[0] * 1000u - 7); break;
        case 0xb0: case 0xc0: rv = ((fn4)rf)(sec, key, P(rout), n); ov = ((fn4)of)(sec, key, P(oout), n); break;
        case 0xb4: case 0xc4: rv = ((fn4)rf)(sec, key, P(vals), n); ov = ((fn4)of)(sec, key, P(vals), n); break;
        case 0xbc: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, double) = rf, (__stdcall *o)(uint32_t, uint32_t, double) = of;
            rv = r(sec, key, d); ov = o(sec, key, d);
            break;
        }
        case 0xc8: i = 1 + rnd() % 64; rv = ((fn4)rf)(sec, key, P(rbuf), i); ov = ((fn4)of)(sec, key, P(obuf), i); break;
        case 0xcc: i = rnd() % 16; rv = ((fn3)rf)(sec, key, P(prof_values[i])); ov = ((fn3)of)(sec, key, P(prof_values[i])); break;
        case 0xd0: case 0xe0: rv = ((fn5)rf)(sec, key, P(rout), P(rp), ext); ov = ((fn5)of)(sec, key, P(oout), P(op), ext); break;
        case 0xd4: rv = ((fn5)rf)(sec, key, vals[1] * 77u, P(rp), ext); ov = ((fn5)of)(sec, key, vals[1] * 77u, P(op), ext); break;
        case 0xd8: case 0xe8: case 0xdc: case 0xec: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = rf, (__stdcall *o)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = of;
            int put = s == 0xdc || s == 0xec;
            rv = r(sec, key, put ? P(vals) : P(rout), n, P(rp), ext);
            ov = o(sec, key, put ? P(vals) : P(oout), n, P(op), ext);
            break;
        }
        case 0xe4: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, double, uint32_t, uint32_t) = rf, (__stdcall *o)(uint32_t, uint32_t, double, uint32_t, uint32_t) = of;
            rv = r(sec, key, d, P(rp), ext); ov = o(sec, key, d, P(op), ext);
            break;
        }
        case 0xf0: {
            uint32_t (__stdcall *r)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = rf, (__stdcall *o)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = of;
            i = 1 + rnd() % 64;
            rv = r(sec, key, P(rbuf), i, P(rp), ext); ov = o(sec, key, P(obuf), i, P(op), ext);
            break;
        }
        case 0xf4: i = rnd() % 16; rv = ((fn5)rf)(sec, key, P(prof_values[i]), P(rp), ext); ov = ((fn5)of)(sec, key, P(prof_values[i]), P(op), ext); break;
        case 0xf8: rv = ((fn1)rf)(sec); ov = ((fn1)of)(sec); break;
        case 0xfc: rv = ((fn3)rf)(sec, P(rp), ext); ov = ((fn3)of)(sec, P(op), ext); break;
        case 0x100: rv = ((fn2)rf)(sec, P(rout)); ov = ((fn2)of)(sec, P(oout)); break;
        case 0x104: rv = ((fn4)rf)(sec, P(rp), ext, P(rout)); ov = ((fn4)of)(sec, P(op), ext, P(oout)); break;
        }
        if (rv != ov || memcmp(rout, oout, sizeof rout) || memcmp(rbuf, obuf, sizeof rbuf)) {
            printf("  +%#05x profile DIFFER real=%d ours=%d [%s] [%s]\n", s, (int)rv, (int)ov, rbuf, obuf); differ++;
        }
        if (files_differ(rcfg, ocfg)) { printf("  +%#05x profile FILES DIFFER\n", s); differ++; }
        check_state("profiles");
        if (differ) return;
    }
}

/* ---- files ------------------------------------------------------------------- */

/* Each side works in its own directory (hostfs\r\, hostfs\o\) on the same
 * small set of files, names relative to the harness's directory; up to three
 * files open at a time each.  After every operation the results, the file
 * objects, and the two directories' contents are compared. */
static const char *const fs_names[] = { "Fa", "Fb", "Fc", "Dd" };
static const uint32_t fs_exts[] = { 18, 30, 0xfffffffeu, 18, 30, 999 };   /* .BIN .TMP none, bad */
static uint32_t rfh[3], ofh[3];

/* The file a name and extension make, on a side ('r' or 'o'). */
static void fs_path(char *out, char side, const char *name, uint32_t ext)
{
    static const char *const e[] = { [18] = ".BIN", [30] = ".TMP" };
    sprintf(out, "hostfs\\%c\\%s%s", side, name, ext < 31 && e[ext] ? e[ext] : "");
}

/* Remove every file and directory the operations can make, on both sides. */
static void fs_clean(void)
{
    static char p[MAX_PATH];
    uint32_t s, n, x;
    for (s = 0; s < 2; s++)
        for (n = 0; n < 4; n++)
            for (x = 0; x < 3; x++) {
                fs_path(p, "ro"[s], fs_names[n], fs_exts[x]);
                SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(p);
                RemoveDirectoryA(p);
            }
}

/* A fresh pair of directories: some of the files, with the same contents. */
static void seed_files(void)
{
    static char rp[MAX_PATH], op[MAX_PATH], data[200];
    uint32_t n, x, len, i;
    fs_clean();
    for (n = 0; n < 3; n++)
        for (x = 0; x < 3; x++) {
            if (rnd() % 3) continue;
            len = rnd() % sizeof data;
            for (i = 0; i < len; i++) data[i] = (char)rnd();
            fs_path(rp, 'r', fs_names[n], fs_exts[x]); fs_path(op, 'o', fs_names[n], fs_exts[x]);
            {
                FILE *f = fopen(rp, "wb"); if (f) { fwrite(data, 1, len, f); fclose(f); }
                f = fopen(op, "wb"); if (f) { fwrite(data, 1, len, f); fclose(f); }
            }
        }
    memset(rfh, 0, sizeof rfh); memset(ofh, 0, sizeof ofh);
}

/* The two directories alike: each name present on both or neither, with the
 * same contents and the same attributes. */
static int dirs_differ(void)
{
    static char rp[MAX_PATH], op[MAX_PATH];
    uint32_t n, x;
    for (n = 0; n < 4; n++)
        for (x = 0; x < 3; x++) {
            uint32_t ra, oa;
            fs_path(rp, 'r', fs_names[n], fs_exts[x]); fs_path(op, 'o', fs_names[n], fs_exts[x]);
            ra = GetFileAttributesA(rp); oa = GetFileAttributesA(op);
            if (ra != oa) return 1;
            if (ra != 0xffffffffu && !(ra & FILE_ATTRIBUTE_DIRECTORY) && files_differ(rp, op)) return 1;
        }
    return 0;
}

/* A file object on each side alike: mode, extension, and path but for the
 * side's letter. */
static int fobj_differ(uint32_t r, uint32_t o)
{
    const uint32_t *rf = (const uint32_t *)(uintptr_t)r, *of = (const uint32_t *)(uintptr_t)o;
    const char *a, *b;
    if (!r != !o) return 1;
    if (!r) return 0;
    if (rf[2] != of[2] || rf[3] != of[3]) return 1;
    a = (const char *)(uintptr_t)rf[1]; b = (const char *)(uintptr_t)of[1];
    if (strlen(a) != strlen(b) || strncmp(a, b, 7) || strcmp(a + 8, b + 8)) return 1;
    return 0;
}

static void check_files(void)
{
    static uint8_t rbuf[128], obuf[128], data[128];
    static char rp[MAX_PATH], op[MAX_PATH], rq[MAX_PATH], oq[MAX_PATH];
    uint32_t ops = 1 + rnd() % 8, k, i;
    for (k = 0; k < ops; k++) {
        uint32_t op_ = rnd() % 16, slot = rnd() % 3, rv = 0, ov = 0, ro, oo, n, x, x2;
        uint32_t rf = rfh[slot], of_ = ofh[slot];
        const char *nm = fs_names[rnd() % 4], *nm2 = fs_names[rnd() % 3];
        static const uint32_t slots[16] = { 0x17c, 0x17c, 0x148, 0x13c, 0x19c, 0x19c, 0x1b0, 0x194,
                                            0x190, 0x1a4, 0x1a8, 0x134, 0x138, 0x158, 0x14c, 0x144 };
        uint32_t s = slots[op_];
        void *R = real_fn(s), *O = our_fn(s);
        if (!O) return;
        if (trace) printf("file +%#x slot %u\n", s, slot);
        x = fs_exts[rnd() % 6]; x2 = fs_exts[rnd() % 5];
        sprintf(rp, "hostfs\\r\\%s", nm); sprintf(op, "hostfs\\o\\%s", nm);
        sprintf(rq, "hostfs\\r\\%s", nm2); sprintf(oq, "hostfs\\o\\%s", nm2);
        if (x == 0xfffffffeu && rnd() & 1) { strcat(rp, ".BIN"); strcat(op, ".BIN"); }
        ro = oo = rnd();
        if (rnd() % 16 == 0) rf = of_ = 0;              /* a null file now and then */
        memset(rbuf, 0x5a, sizeof rbuf); memset(obuf, 0x5a, sizeof obuf);
        switch (s) {
        case 0x17c: case 0x148:
            if (rfh[slot] || ofh[slot]) continue;
            n = rnd() % 5;                              /* modes 0..3, and 4 */
            if (s == 0x17c) { rv = ((fn4)R)(P(&rfh[slot]), P(rp), x, n); ov = ((fn4)O)(P(&ofh[slot]), P(op), x, n); }
            else { rv = ((fn3)R)(P(&rfh[slot]), P(rp), x); ov = ((fn3)O)(P(&ofh[slot]), P(op), x); }
            if (rv) rfh[slot] = 0;
            if (ov) ofh[slot] = 0;
            if (fobj_differ(rfh[slot], ofh[slot])) { printf("  +%#05x open OBJECT DIFFER\n", s); differ++; }
            break;
        case 0x13c:
            rv = ((fn1)R)(rf); ov = ((fn1)O)(of_);
            if (rf) rfh[slot] = 0;
            if (of_) ofh[slot] = 0;
            break;
        case 0x19c: case 0x1b0:
            n = rnd() % 100;
            for (i = 0; i < n; i++) data[i] = (uint8_t)rnd();
            if (s == 0x1b0) { memcpy(rbuf, data, n); memcpy(obuf, data, n); }
            i = rnd() & 1;
            rv = ((fn4)R)(rf, P(rbuf), n, i ? P(&ro) : 0); ov = ((fn4)O)(of_, P(obuf), n, i ? P(&oo) : 0);
            break;
        case 0x194: {
            uint32_t size = 0;
            if (rf) size = GetFileSize((HANDLE)(uintptr_t)*(uint32_t *)(uintptr_t)rf, 0);
            n = rnd() % (size + 4); i = rnd() % 3;
            rv = ((fn3)R)(rf, n, i); ov = ((fn3)O)(of_, n, i);
            break;
        }
        case 0x190: case 0x1a4:
            rv = ((fn2)R)(rf, P(&ro)); ov = ((fn2)O)(of_, P(&oo));
            break;
        case 0x1a8:
            n = rnd() % 120;
            rv = ((fn2)R)(rf, n); ov = ((fn2)O)(of_, n);
            break;
        case 0x134:
            rv = ((fn3)R)(P(&ro), P(rp), x); ov = ((fn3)O)(P(&oo), P(op), x);
            break;
        case 0x138:
            n = rnd() % 0x20;
            rv = ((fn3)R)(n, P(rp), x); ov = ((fn3)O)(n, P(op), x);
            break;
        case 0x158: case 0x14c:
            if (s == 0x14c && rnd() % 4 == 0) {         /* +0x11c, a directory, instead */
                s = 0x11c; R = real_fn(s); O = our_fn(s);
                if (!O) return;
                sprintf(rp, "hostfs\\r\\Dd"); sprintf(op, "hostfs\\o\\Dd");
                rv = ((fn1)R)(P(rp)); ov = ((fn1)O)(P(op));
                break;
            }
            rv = ((fn2)R)(P(rp), x); ov = ((fn2)O)(P(op), x);
            break;
        case 0x144:
            if (rnd() & 1) { s = 0x16c; R = real_fn(s); O = our_fn(s); if (!O) return; }
            rv = ((fn4)R)(P(rq), x2, P(rp), x); ov = ((fn4)O)(P(oq), x2, P(op), x);
            break;
        }
        if (rv != ov || ro != oo || memcmp(rbuf, obuf, sizeof rbuf)) {
            printf("  +%#05x file op DIFFER real=%d/%#x ours=%d/%#x (%s, %#x)\n", s, (int)rv, ro, (int)ov, oo, nm, x); differ++;
        }
        if (dirs_differ()) { printf("  +%#05x file op DIRS DIFFER (%s, %#x)\n", s, nm, x); differ++; }
        check_state("files");
        if (differ) break;
    }
    for (k = 0; k < 3; k++) {                           /* close what is open, each side its own */
        if (rfh[k]) ((fn1)real_fn(0x13c))(rfh[k]);
        if (ofh[k] && our_fn(0x13c)) ((fn1)our_fn(0x13c))(ofh[k]);
        rfh[k] = ofh[k] = 0;
    }
}

int main(int argc, char **argv)
{
    uint32_t n = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 0) : 1, k, i;

    setvbuf(stdout, 0, _IONBF, 0);     /* so a crash still shows what came before it */
    trace = getenv("HOSTDIFF_TRACE") != 0;
    map_real();
    load_ours();
    load_strmods();
    fs_init();
    printf("hostdiff: %u accessors + writers + strings + primitives + stubs + allocator + lists + command line + extensions + CRT + string resources + names + images + accelerators + key names + profiles + files, %u iteration(s)\n",
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
        check_strings();
        check_strmods();
        check_inputs();
        check_names();
        check_184();
        check_images();
        check_accels();
        check_keynames();
        seed_profile();
        check_profiles();
        seed_files();
        check_files();
    }
    printf("hostdiff: %d differ\n", differ);
    return differ ? 1 : 0;
}
