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

/* After a call, the two .data images must be identical; report the first word
 * that is not. */
static void check_state(const char *name)
{
    uint32_t i;
    for (i = 0; i < DATA_SIZE; i += 4) {
        uint32_t rv = norm(*(uint32_t *)(DATA_VA + i));     /* real .data (mapped at 0x417000) */
        uint32_t ov = norm(*(uint32_t *)(our_data + i));    /* our image */
        if (rv != ov) {
            printf("  %-16s STATE DIFFER at .data+%#x: real=%#010x ours=%#010x\n",
                   name, i, rv, ov);
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

int main(int argc, char **argv)
{
    uint32_t n = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 0) : 1, k, i;

    map_real();
    load_ours();
    printf("hostdiff: %u accessors + writers + strings + primitives + stubs, %u iteration(s)\n",
           (unsigned)(sizeof accessors / sizeof accessors[0]), n);
    for (k = 0; k < n; k++) {
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
    }
    printf("hostdiff: %d differ\n", differ);
    return differ ? 1 : 0;
}
