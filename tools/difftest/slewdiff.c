/* Differential test: retail SLEW.DLL against the OpenFS98 build.
 *
 * Loads both modules into one process, hands each its own copy of an
 * identically seeded GLOBAL state block plus a shared stub UTIL, calls the
 * tick at Linkage+0x18 on both, and compares the two blocks byte for byte.
 *
 * This exists because the game cannot test this module: SLEW's tick only runs
 * in slew mode, so booting the sim proves the module loads and links and
 * nothing more.  Everything SLEW does is a pure function of the GLOBAL block
 * and three UTIL calls, which makes it exactly the shape that a differential
 * harness can settle completely.
 *
 * All three UTIL services SLEW uses -- 0x18, 0x2c and 0x44 -- are now the real
 * reimplementations from src/util/util_math.c, each verified against retail by
 * utildiff.  There are no stubs left here: SLEW is driven over the same
 * arithmetic FS98 drives it over.
 *
 * Both DLLs are linked at the same image base, so the loader relocates
 * whichever lands second.  That is fine; neither module depends on its base.
 *
 * Build: make difftest     Run: make difftest-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GLOBAL_SIZE 0x12a8

struct import { uint32_t id; void *link; };

struct slew_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    void *reserved[3];
    void (*update)(void);
};

/* ---- stub UTIL ------------------------------------------------------------ */

static unsigned char util_blob[0x80];
#define UTIL_SLOT(off) (*(void **)(util_blob + (off)))

int trace_tag = 0;   /* 0 = quiet, else names which module is calling */

/* the real UTIL services, verified against retail by `make utildiff-run` */
int32_t __stdcall fs98_util_sqrt64(const void *p);
void    __stdcall fs98_util_sincos(int16_t a, int16_t *s, int16_t *c);
void    __stdcall fs98_util_metres_to_longitude(const void *in, const void *pos,
                                                void *out);

static void __stdcall stub_sincos(int16_t angle, int16_t *a, int16_t *b)
{
    fs98_util_sincos(angle, a, b);
    if (trace_tag)
        printf("    [%s] sincos(angle=%04x) -> sin=%d cos=%d\n",
               trace_tag == 1 ? "retail  " : "openfs98", (uint16_t)angle, *a, *b);
}

/* ---- a module under test --------------------------------------------------- */

struct subject {
    const char          *path;
    HMODULE              h;
    struct import       *imports;
    struct slew_linkage *link;
    unsigned char        global[GLOBAL_SIZE];
};

static int load_subject(struct subject *s, const char *path)
{
    s->path = path;
    s->h = LoadLibraryA(path);
    if (!s->h) { printf("  cannot load %s (%lu)\n", path, GetLastError()); return 0; }
    s->imports = (struct import *)GetProcAddress(s->h, "ImportTable");
    s->link    = (struct slew_linkage *)GetProcAddress(s->h, "Linkage");
    if (!s->imports || !s->link) { printf("  %s: missing exports\n", path); return 0; }
    if (s->link->id != 9) { printf("  %s: id %u, expected 9\n", path, s->link->id); return 0; }
    if (!s->link->update) { printf("  %s: no service at Linkage+0x18\n", path); return 0; }
    return 1;
}

static void bind_imports(struct subject *s, void *host, void *util)
{
    struct import *im;
    for (im = s->imports; im->id; im++) {
        if      (im->id == 2)  im->link = s->global;
        else if (im->id == 20) im->link = util;
        else                   im->link = host;
    }
}

/* ---- test vectors ---------------------------------------------------------- */

static uint32_t rng_state;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static unsigned char aircraft[0x40];

static void seed(unsigned char *g, unsigned iter)
{
    unsigned i;
    for (i = 0; i < GLOBAL_SIZE; i++)
        g[i] = (unsigned char)(rng() >> 11);

    *(uint32_t *)(g + 0) = 2;                       /* module id        */

    /* Fields that would otherwise make the test meaningless or unsafe. */
    g[0x365] = (iter % 16 == 0) ? 1 : 0;            /* exercise the bail-out */
    *(int32_t *)(g + 0x390) = (int32_t)(rng() % 4); /* gates fast slew  */

    /* a real object, or none -- never a random pointer */
    if (rng() & 1) {
        *(uint32_t *)(aircraft + 0x10) = rng() & 1;
        *(uint16_t *)(aircraft + 0x26) = (uint16_t)(rng() % 0x900);
        *(void **)(g + 0x11a0) = aircraft;
    } else {
        *(void **)(g + 0x11a0) = NULL;
    }
}

struct field { int off; int size; const char *name; };
static const struct field fields[] = {
    { 0x0365, 1, "slew_inactive" }, { 0x0390, 4, "mode" },
    { 0x02aa, 2, "time_scale"    }, { 0x0020, 4, "ground_alt" },
    { 0x120c, 4, "alt_offset"    },
    { 0x05e0, 4, "att_zero"      }, { 0x05de, 2, "att_reset" },
    { 0x05ea, 2, "rate_fwd"      }, { 0x05ec, 2, "rate_side" },
    { 0x05e4, 2, "rate_bank"     }, { 0x05e6, 2, "rate_heading" },
    { 0x05e8, 2, "rate_vert"     }, { 0x05ee, 2, "rate_pitch" },
    { 0x0562, 2, "lat_lo"        }, { 0x0564, 4, "lat_hi" },
    { 0x056a, 2, "lon_lo"        }, { 0x056c, 4, "lon_hi" },
    { 0x0572, 2, "alt_lo"        }, { 0x0574, 4, "alt_hi" },
    { 0x0578, 4, "pitch"         }, { 0x057c, 4, "bank" },
    { 0x0580, 4, "heading"       },
    { 0x02b8, 4, "groundspeed_a" }, { 0x02bc, 4, "groundspeed_b" },
    { 0x02c8, 4, "vert_speed"    }, { 0x05f8, 4, "speed_out" },
    { 0, 0, NULL }
};

static uint32_t fetch(const unsigned char *g, const struct field *f)
{
    if (f->size == 1) return g[f->off];
    if (f->size == 2) return *(uint16_t *)(g + f->off);
    return *(uint32_t *)(g + f->off);
}

/* The retail module's scratch area, at a fixed RVA in its loaded image.  Being
 * able to read the intermediates directly is the difference between localising
 * a divergence and guessing at it. */
static void dump_retail_private(HMODULE h)
{
    const unsigned char *p = (const unsigned char *)h + 0x3000;
    printf("    retail scratch: vel_x=%08x vel_y=%08x vel_z=%08x\n",
           *(uint32_t *)(p + 0x08), *(uint32_t *)(p + 0x0c), *(uint32_t *)(p + 0x10));
    printf("                    rate_side=%04x rate_fwd=%04x fast=%04x\n",
           *(uint16_t *)(p + 0x14), *(uint16_t *)(p + 0x16), *(uint16_t *)(p + 0x20));
    printf("                    sq@3018=%08x sq@301c=%08x\n",
           *(uint32_t *)(p + 0x18), *(uint32_t *)(p + 0x1c));
}

static void dump(const unsigned char *in, const unsigned char *ra,
                 const unsigned char *ob)
{
    const struct field *f;
    printf("    %-15s %12s %12s %12s\n", "field", "in", "retail", "openfs98");
    for (f = fields; f->name; f++) {
        uint32_t i = fetch(in, f), r = fetch(ra, f), o = fetch(ob, f);
        printf("    %-15s %12x %12x %12x %s\n", f->name, i, r, o,
               r == o ? "" : "  <-- differs");
    }
}

int main(int argc, char **argv)
{
    static struct subject a, b;
    static unsigned char before[GLOBAL_SIZE];
    unsigned char host[0x40];
    unsigned iters = (argc > 1) ? (unsigned)atoi(argv[1]) : 2000;
    long show = (argc > 2) ? atol(argv[2]) : -1;
    unsigned iter, diffs = 0;

    memset(host, 0, sizeof host);
    memset(util_blob, 0, sizeof util_blob);
    *(uint32_t *)util_blob = 20;
    UTIL_SLOT(0x18) = (void *)fs98_util_sqrt64;
    UTIL_SLOT(0x2c) = (void *)stub_sincos;
    UTIL_SLOT(0x44) = (void *)fs98_util_metres_to_longitude;

    if (!load_subject(&a, "SLEW_retail.dll")) return 2;
    if (!load_subject(&b, "SLEW_ours.dll"))   return 2;
    printf("retail  %s\nopenfs98 %s\n%u vectors\n\n", a.path, b.path, iters);

    bind_imports(&a, host, util_blob);
    bind_imports(&b, host, util_blob);

    for (iter = 0; iter < iters; iter++) {
        unsigned off;
        rng_state = 0x1234567u + iter * 2654435761u;
        seed(a.global, iter);
        memcpy(b.global, a.global, GLOBAL_SIZE);
        memcpy(before, a.global, GLOBAL_SIZE);

        trace_tag = ((long)iter == show) ? 1 : 0;
        a.link->update();
        trace_tag = ((long)iter == show) ? 2 : 0;
        b.link->update();
        trace_tag = 0;

        if (memcmp(a.global, b.global, GLOBAL_SIZE) == 0) {
            if ((long)iter == show) { printf("vector %u (identical):\n", iter);
                                      dump(before, a.global, b.global); }
            continue;
        }

        if ((long)iter == show) {
            printf("vector %u:\n", iter);
            dump(before, a.global, b.global);
            dump_retail_private(a.h);
        }
        if (++diffs <= 10 && show < 0) {
            printf("vector %u differs:\n", iter);
            for (off = 0; off < GLOBAL_SIZE; off++) {
                if (a.global[off] == b.global[off])
                    continue;
                printf("    +0x%04x  retail %02x  openfs98 %02x\n",
                       off, a.global[off], b.global[off]);
                /* skip the rest of a contiguous run to keep output readable */
                while (off + 1 < GLOBAL_SIZE && a.global[off + 1] != b.global[off + 1])
                    off++;
            }
        }
    }

    printf("\n%u/%u vectors differ\n", diffs, iters);
    if (diffs == 0)
        printf("IDENTICAL\n");
    return diffs != 0;
}
