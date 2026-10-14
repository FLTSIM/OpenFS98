/* Differential test: retail WEATHERM.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven only through Linkage, the
 * way MAIN, ATC, G3D and multip drive them.  What WEATHERM imports:
 *
 *   host      a stub: alloc and free over a zeroed bump arena, so the C half's
 *             heap pointers come out the same on both sides
 *   GLOBAL    one block, reseeded identically before each side runs
 *   CHAIN     retail's, behind logging wrappers; a frame is a dispatch of
 *             list 3, which is where the areas drift and the pressure wanders
 *   G3D       a stub that logs the object +0x20 is asked to release
 *   MAIN      a stub that logs the +0x94 call
 *   UTIL      retail's, behind logging forwarders; its rand seed is reset to
 *             the scenario's before each side runs
 *
 * A scenario is a random script run once against each module.  Compared: the
 * transcript of every import call and return value, the GLOBAL block, the
 * arena, and WEATHERM's .data image from +0x98 -- both builds keep it at
 * Linkage-0x38 with retail's layout.
 *
 * Retail faults on some states, and the generator keeps clear of them:
 *   - temperature layers the position is above must be an odd number of
 *     metres apart (an even band overflows an idiv -- see wx_interpolate).
 *     +0x24's own defaults are 320 m apart, so after a reset the harness logs
 *     them and puts odd-gap layers back;
 *   - an area's fade width (+0x28) must not be 1 (a divide by zero);
 *   - latitudes stay inside UTIL's table, and areas drift slowly enough
 *     that UTIL's metres-to-longitude does not overflow;
 *   - the C half gets zeroed memory, valid record types, and no list cycles.
 *
 * Build: make weathermdiff     Run: make weathermdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_LO    0x98        /* compared from here */
#define IMAGE_HI    0x1c5       /* .data's virtual size */
#define GLOBAL_SIZE 0x1300
#define ARENA_SIZE  (1 << 18)

struct import { uint32_t id; void *link; };
struct wx_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t r0c, r10;
    void *dialogs;
    void     (STDCALL *weather_at)(void *position);
    void     (*weather_here)(void);
    void     (STDCALL *temperature)(uint32_t base, uint16_t *temp, uint16_t *swing);
    void     (*reset)(void);
    void     (STDCALL *effect)(const uint8_t *record);
    int32_t  (STDCALL *list_create)(void **cell);
    int32_t  (STDCALL *list_destroy)(void **cell);
    void    *(STDCALL *area_new)(void *cell);
    int32_t  (STDCALL *area_remove)(void *cell, void *area);
    uint32_t (STDCALL *area_copy)(void *src, void *dst);
    void    *(STDCALL *area_next)(void *cell, void *area);
    uint32_t (STDCALL *area_append)(void *cell, void *area);
    uint32_t (STDCALL *area_count)(void *cell);
    void    *(STDCALL *record_new)(void *area, uint32_t type);
    uint32_t (STDCALL *record_free)(void *area, void *record, uint32_t type);
    uint32_t (STDCALL *records_copy)(void *src, void *dst, void *record,
                                     uint32_t type, uint32_t keep);
    void    *(STDCALL *record_next)(void *area, void *record, uint32_t type);
    uint32_t (STDCALL *record_append)(void *area, void *record, uint32_t type);
};
typedef void (STDCALL *cb_fn)(uint32_t, uint32_t);
struct chain_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t r0c, r10;
    void *dialogs;
    int32_t  (STDCALL *add)(uint32_t, cb_fn);
    int32_t  (STDCALL *remove)(uint32_t, cb_fn);
    uint32_t (STDCALL *dispatch)(uint32_t, uint32_t, uint32_t);
};

/* ---- the transcript -------------------------------------------------------- */

static char tlog[1 << 21];
static unsigned tlen;

static void put(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (tlen < sizeof tlog - 256)
        tlen += (unsigned)vsnprintf(tlog + tlen, sizeof tlog - tlen, fmt, ap);
    va_end(ap);
}

/* ---- the side being run ------------------------------------------------------ */

struct side {
    const char *name;
    HMODULE dll;
    struct wx_linkage *link;
    uint8_t *image;             /* Linkage - 0x38 */
    uint8_t pristine[IMAGE_HI];
};
static struct side *cur;

static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t global_blob[GLOBAL_SIZE];
static uint8_t position[24];    /* for +0x18 */

static void put_ptr(const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    if (!p)
        put("null");
    else if (q >= arena && q < arena + sizeof arena)
        put("arena+%#x", (unsigned)(q - arena));
    else if (q >= global_blob && q < global_blob + sizeof global_blob)
        put("G+%#x", (unsigned)(q - global_blob));
    else if (q >= position && q < position + sizeof position)
        put("pos+%#x", (unsigned)(q - position));
    else if (q >= cur->image && q < cur->image + 0x200)
        put("img+%#x", (unsigned)(q - cur->image));
    else
        put("%p", p);
}

/* ---- the stub host ------------------------------------------------------------- */

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    uint8_t *p;
    uint32_t n = (size + 15) & ~15u;
    if (arena_used + n > sizeof arena) {
        put(" alloc(%#x)=OUT", size);
        *out = NULL;
        return -1;
    }
    p = arena + arena_used;
    arena_used += n;
    put(" alloc(%d,%#x,%d)=", fl, size, zero);
    put_ptr(p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    put(" free(");
    put_ptr(*pp);
    put(")");
    *pp = NULL;
    return 0;
}

static uint8_t host_blob[0x320];

/* ---- G3D and MAIN ---------------------------------------------------------------- */

static void STDCALL g3d_release(void *object)
{
    put(" g3d.release(%p)", object);
}

static void main_weather(void) { put(" main.weather"); }

static uint8_t g3d_blob[0x30], main_blob[0x100];

/* ---- UTIL, retail's behind forwarders ----------------------------------------- */

static uint8_t *util_link;
static uint32_t *util_seed;
static uint8_t util_fwd[0x100];

#define UTIL(off, type) (*(type *)(util_link + (off)))
typedef void     (STDCALL *mul48_fn)(const void *, int16_t, void *);
typedef void     (STDCALL *sincos_fn)(int16_t, int16_t *, int16_t *);
typedef void     (STDCALL *rotate_fn)(int32_t, int32_t, int16_t, int16_t, int32_t *, int32_t *);
typedef int32_t  (STDCALL *atan2_fn)(int32_t, int32_t);
typedef void     (STDCALL *convert_fn)(const void *, const void *, void *);
typedef uint32_t (STDCALL *rand_fn)(void);

static void put48(const void *p)
{
    put("%08x.%04x", *(const uint32_t *)((const uint8_t *)p + 4),
        *(const uint16_t *)((const uint8_t *)p + 2));
}

static void STDCALL u_mul48(const void *src, int16_t m, void *dst)
{
    UTIL(0x24, mul48_fn)(src, m, dst);
    put(" mul48("); put48(src); put(",%d)=%08x%08x", m, *(uint32_t *)((uint8_t *)dst + 4),
                                     *(uint32_t *)dst);
}

static void STDCALL u_sincos(int16_t a, int16_t *s, int16_t *c)
{
    UTIL(0x2c, sincos_fn)(a, s, c);
    put(" sincos(%04x)", (uint16_t)a);
}

static void STDCALL u_rotate(int32_t x, int32_t y, int16_t s, int16_t c, int32_t *ox, int32_t *oy)
{
    UTIL(0x34, rotate_fn)(x, y, s, c, ox, oy);
    put(" rot(%x,%x)=%x,%x", x, y, *ox, *oy);
}

static int32_t STDCALL u_atan2(int32_t x, int32_t y)
{
    int32_t r = UTIL(0x3c, atan2_fn)(x, y);
    put(" atan2(%x,%x)=%04x", x, y, (uint16_t)r);
    return r;
}

static void STDCALL u_to_lon(const void *in, const void *pos, void *out)
{
    UTIL(0x44, convert_fn)(in, pos, out);
    put(" tolon("); put48(in); put(")="); put48(out);
}

static void STDCALL u_to_m(const void *in, const void *pos, void *out)
{
    UTIL(0x48, convert_fn)(in, pos, out);
    put(" tom("); put48(in); put(")="); put48(out);
}

static uint32_t STDCALL u_rand(void)
{
    uint32_t r = UTIL(0x6c, rand_fn)();
    put(" rand=%x", r);
    return r;
}

/* ---- CHAIN, retail's behind wrappers --------------------------------------------- */

static struct chain_linkage *chain;
static uint8_t chain_blob[0x30];

static int32_t STDCALL c_add(uint32_t list, cb_fn cb)
{
    int32_t r = chain->add(list, cb);
    put(" chain.add(%u)=%d", list, r);
    return r;
}

static int32_t STDCALL c_remove(uint32_t list, cb_fn cb)
{
    int32_t r = chain->remove(list, cb);
    put(" chain.remove(%u)=%d", list, r);
    return r;
}

/* ---- scenarios ------------------------------------------------------------------- */

static uint32_t rs = 0x2545f491u;
static uint32_t rng(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

#define G8(g, o)  (*(uint8_t  *)((g) + (o)))
#define G16(g, o) (*(uint16_t *)((g) + (o)))
#define G32(g, o) (*(uint32_t *)((g) + (o)))

enum { OP_INIT, OP_DEINIT, OP_HERE, OP_AT, OP_TICK, OP_TEMP, OP_RESET, OP_EFFECT,
       OP_POKE, OP_OBJECT, OP_LIST, OP_AREA, OP_TUNE, OP_SNAP };

/* boundaries in the working weather an altitude can sit exactly on */
static const uint16_t snaps[] = {
    0xee2, 0xee4, 0xed6, 0xed8, 0xeca, 0xecc, 0xeee,    /* wind layers, surface */
    0xe9a, 0xe9c, 0xea2, 0xea4, 0xeaa, 0xeac,           /* cloud tops and bases */
    0xeb2, 0xeb6, 0xeba, 0xebe, 0                       /* temperature layers, 11000 m */
};
struct op { int kind; uint32_t a, b, c; uint8_t data[24]; };
#define MAXOPS 64
struct scenario {
    struct op op[MAXOPS];
    unsigned nops;
    uint8_t global[GLOBAL_SIZE];
    uint32_t seed;
};

/* a latitude (the high dword) inside UTIL's table, and one near it */
static uint32_t latitude(void) { return (uint32_t)((int32_t)(rng() % 0x600000) - 0x300000); }
static uint32_t close_to(uint32_t v, uint32_t r) { return v + (uint32_t)((int32_t)(rng() % (2 * r + 1)) - (int32_t)r); }

static void make_position(uint8_t *p, const uint8_t *g)
{
    unsigned i;
    for (i = 0; i < 24; i++) p[i] = (uint8_t)rng();
    *(uint32_t *)(p + 4) = g ? close_to(G32(g, 0x564), 0x8000) : latitude();
    if (g && rng() % 2) *(uint32_t *)(p + 0xc) = close_to(G32(g, 0x56c), 0x8000);
    switch (rng() % 6) {
    case 0:  *(uint32_t *)(p + 0x14) = rng(); break;
    case 1:  *(uint32_t *)(p + 0x14) = rng() % 0x8000; break;
    case 2:  *(uint32_t *)(p + 0x14) = 0x2af8 + rng() % 8 - 4; break;
    default: *(uint32_t *)(p + 0x14) = rng() % 15000; break;
    }
}

/* Four temperature layers, (altitude, temperature) from the top: the lowest
 * near the ground, odd gaps upward, and sometimes the unused marker from some
 * layer up. */
static void make_temperatures(uint8_t *t)
{
    int16_t alt = (int16_t)(rng() % 700) - 100;
    int k, none = rng() % 3 ? 4 : 1 + (int)(rng() % 3);
    if (rng() % 8 == 0) alt = (int16_t)0xd8f1;
    for (k = 0; k < 4; k++) {
        uint8_t *l = t + 0xc - 4 * k;           /* +0xc is the lowest */
        *(uint16_t *)(l + 2) = (uint16_t)(rng() % 3 ? (int16_t)(rng() % 0x3000) - 0x1800 : (int16_t)rng());
        if (k > 0 && rng() % 4 == 0)                    /* isothermal: no lapse */
            *(uint16_t *)(l + 2) = *(uint16_t *)(l + 6);
        if (k > 0 && k >= none) {
            *(uint16_t *)l = 0xd8f1;
            continue;
        }
        *(uint16_t *)l = (uint16_t)alt;
        if (k == 0 && alt == (int16_t)0xd8f1) alt = 0;
        alt = (int16_t)(alt + 1 + 2 * (int16_t)(rng() % (k == 2 ? 3000 : 1500)));
    }
    if (rng() % 6 == 0 && none == 4) {
        /* the top layer at exactly 11000 m, odd gaps below it */
        int16_t a = 0x2af8;
        for (k = 3; k >= 1; k--) {
            *(uint16_t *)(t + 0xc - 4 * k) = (uint16_t)a;
            a = (int16_t)(a - 1 - 2 * (int16_t)(rng() % 1500));
        }
        *(uint16_t *)(t + 0xc) = (uint16_t)a;
    }
    *(uint16_t *)(t + 0x10) = (uint16_t)(rng() % 3 ? rng() % 2 : rng());    /* daily swing on */
    *(uint16_t *)(t + 0x12) = (uint16_t)rng();                               /* its amplitude */
    *(uint16_t *)(t + 0x14) = (uint16_t)(rng() % 3 ? 0x3000 + rng() % 0x2000 : rng()); /* pressure */
    *(uint16_t *)(t + 0x16) = (uint16_t)(rng() % 2);                         /* wander on */
}

/* Three cloud layers of (top, base, cover, spare), some missing (top 0). */
static void make_clouds(uint8_t *c)
{
    unsigned k;
    for (k = 0; k < 3; k++) {
        uint16_t base = (uint16_t)(rng() % 12000), top = (uint16_t)(base + rng() % 3000);
        uint8_t *l = c + 8 * k;
        if (rng() % 4 == 0) top = 0;
        if (rng() % 8 == 0) base = 0;
        *(uint16_t *)(l + 0) = top;
        *(uint16_t *)(l + 2) = base;
        *(uint16_t *)(l + 4) = (uint16_t)(rng() % 3 ? rng() % 9 : rng());
    }
}

/* Three wind layers of (top, base, speed, direction, extra, spare) and the
 * surface wind's (height, speed, direction, extra). */
static void make_winds(uint8_t *w)
{
    unsigned k;
    for (k = 0; k < 3; k++) {
        uint8_t *l = w + 12 * k;
        uint16_t base = (uint16_t)(rng() % 12000);
        *(uint16_t *)(l + 0) = (uint16_t)(base + rng() % 4000);
        *(uint16_t *)(l + 2) = base;
        *(uint16_t *)(l + 4) = (uint16_t)rng();
        *(uint16_t *)(l + 6) = (uint16_t)rng();
        *(uint16_t *)(l + 8) = (uint16_t)rng();
    }
    *(uint16_t *)(w + 0x24) = (uint16_t)(rng() % 3 ? rng() % 1500 : rng());
}

static void make_area(uint8_t *g, uint32_t off)
{
    uint8_t *a = g + off;
    uint32_t lat = G32(g, 0x564), lon = G32(g, 0x56c);
    *(uint16_t *)(a + 0) = (uint16_t)(rng() % 3 ? rng() % 2 : rng());
    *(uint32_t *)(a + 0x08) = close_to(lat, 0x4000);
    *(uint32_t *)(a + 0x10) = close_to(lon, 0x4000);
    *(uint32_t *)(a + 0x18) = close_to(lat, 0x4000);
    *(uint32_t *)(a + 0x20) = close_to(lon, 0x4000);
    if (*(uint32_t *)(a + 0x08) == *(uint32_t *)(a + 0x18))
        (*(uint32_t *)(a + 0x18))++;
    /* the centre: computed from the corners while the area is on, but the
     * position is measured against it either way, and UTIL walks off its
     * latitude table on a wild one */
    *(uint32_t *)(a + 0x3c) = close_to(lat, 0x4000);
    *(uint32_t *)(a + 0x44) = close_to(lon, 0x4000);
    *(uint32_t *)(a + 0x24) = rng() % 4 ? rng() % 0x1000000 : rng();
    do *(uint32_t *)(a + 0x28) = rng() % 4 ? rng() % 0x400000 : rng(); while (*(uint32_t *)(a + 0x28) == 1);
    *(uint16_t *)(a + 0x2e) = (uint16_t)(rng() % 3 ? rng() % 0x400 : 0);
    *(uint16_t *)(a + 0x30) = (uint16_t)(rng() % 2);
    *(uint16_t *)(a + 0x32) = (uint16_t)(rng() % 2);
    make_clouds(a + 0x74);
    make_temperatures(a + 0x8c);
    make_winds(a + 0xa4);
    if (rng() % 4 == 0) *(uint16_t *)(a + 0xe4) = 0;
}

static void make_global(uint8_t *g)
{
    unsigned i;
    for (i = 0; i < GLOBAL_SIZE; i++) g[i] = (uint8_t)rng();
    make_position(g + 0x560, NULL);
    G32(g, 0x20) = (rng() % 4 ? rng() % 1500 : rng() % 0x8000) << 8 | (rng() & 0xff);
    G8(g, 0x238) = (uint8_t)(rng() % 5 ? rng() % 24 : rng());
    G8(g, 0x239) = (uint8_t)(rng() % 5 ? rng() % 60 : rng());
    G16(g, 0xc1a) = (uint16_t)(rng() % 4 ? rng() % 0x400 : 0x100);
    G32(g, 0x1164) = rng() % 2 ? 0 : rng();
    G8(g, 0xf12) = (uint8_t)(rng() % 3 == 0);
    G8(g, 0xf13) = (uint8_t)(rng() % 3 == 0);
    make_clouds(g + 0xf1c);
    make_temperatures(g + 0xf34);
    make_winds(g + 0xf4c);
    make_area(g, 0xf90);
    make_area(g, 0x1078);
}

/* GLOBAL fields WEATHERM reads, for OP_POKE */
static const struct { uint16_t off; uint8_t size; } pokes[] = {
    { 0x20, 4 }, { 0x238, 1 }, { 0x239, 1 }, { 0x2a0, 2 }, { 0xc1a, 2 }, { 0x1162, 2 },
    { 0xf12, 1 }, { 0xf13, 1 }, { 0xf90, 2 }, { 0x1078, 2 }, { 0xfc0, 2 }, { 0xfc2, 2 },
    { 0x10a8, 2 }, { 0x10aa, 2 }, { 0x574, 4 }, { 0xeee, 2 }, { 0xef0, 2 }, { 0xef2, 2 },
    { 0xf1c, 2 }, { 0xf1e, 2 }, { 0xf20, 2 }, { 0xf7a, 2 }, { 0xf8c, 2 }, { 0x116a, 1 },
    { 0xf4a, 2 }, { 0x1032, 2 }, { 0x111a, 2 }, { 0xf48, 2 }
};

static uint32_t poke_value(unsigned which)
{
    switch (pokes[which].off) {
    case 0x20:   return (rng() % 1500) << 8;
    case 0xc1a:  return rng() % 0x400;
    case 0x574:  return rng() % 3 ? rng() % 15000 : rng() % 0x8000;
    case 0xf90: case 0x1078: case 0xf12: case 0xf13:
    case 0xf4a: case 0x1032: case 0x111a: return rng() % 2;
    default:     return rng();
    }
}

/* C-half operations */
enum { L_CREATE, L_DESTROY, L_AREA_NEW, L_AREA_REMOVE, L_AREA_COPY, L_AREA_NEXT,
       L_AREA_APPEND, L_AREA_COUNT, L_REC_NEW, L_REC_FREE, L_RECS_COPY, L_REC_NEXT,
       L_REC_APPEND, L_POKE, L_COUNT };

static void make_scenario(struct scenario *s, unsigned v)
{
    memset(s, 0, sizeof *s);
    make_global(s->global);
    s->seed = rng() % 0x1b0963;

    s->op[s->nops++].kind = OP_INIT;
    if (v % 4 == 3) {                               /* some scenarios are mostly lists */
        s->op[s->nops].kind = OP_LIST;
        s->op[s->nops++].a = L_CREATE;
        while (s->nops < 6) {
            s->op[s->nops].kind = OP_LIST;
            s->op[s->nops].a = L_AREA_NEW;
            s->op[s->nops++].b = 1 + rng() % 12;
        }
    }
    if (v % 10 == 5) {                              /* a long run, for the pressure clocks */
        s->op[s->nops].kind = OP_POKE;
        s->op[s->nops].a = 4;                       /* 0xc1a */
        s->op[s->nops++].b = 0x300 + rng() % 0x100;
        s->op[s->nops].kind = OP_TICK;
        s->op[s->nops++].a = 300;
    }
    while (s->nops < 12 + rng() % (MAXOPS - 13)) {
        struct op *o = &s->op[s->nops++];
        unsigned r = rng() % 40;
        if (v % 4 == 3 && r < 22) r = 39;
        if (v % 4 != 3 && r >= 31 && rng() % 2) r = 30;
        if (r < 8)       o->kind = OP_HERE;
        else if (r < 11) { o->kind = OP_AT; make_position(o->data, s->global); }
        else if (r < 18) { o->kind = OP_TICK; o->a = 1 + rng() % 8; }
        else if (r < 20) { o->kind = OP_TEMP; o->a = rng(); }
        else if (r < 22) { o->kind = OP_RESET; make_temperatures(o->data); }
        else if (r < 25) {
            o->kind = OP_EFFECT;
            o->data[1] = (uint8_t)(rng() % 3 ? 1 + rng() % 4 : rng() % 7);
            *(uint16_t *)(o->data + 2) = (uint16_t)rng();
            *(uint16_t *)(o->data + 4) = (uint16_t)(rng() % 2 ? rng() % 0x40 : rng());
            if (rng() % 4 == 0) *(uint16_t *)(o->data + 4) = (uint16_t)((rng() & 0xff00) | 3);
            o->a = rng() % 4 == 0 ? 1 + rng() % 2 : 0;      /* 1, 2: square to the wind */
        }
        else if (r < 30) {
            o->kind = OP_POKE;
            o->a = rng() % (sizeof pokes / sizeof *pokes);
            o->b = poke_value(o->a);
        }
        else if (r < 31) {
            unsigned q = rng() % 8;
            if (q < 2) { o->kind = OP_OBJECT; o->a = rng() | 1; }
            else if (q < 5) {
                if (rng() % 2) { o->kind = OP_TUNE; o->a = 1 + rng() % 3; o->b = 2 + rng() % 2; }
                else { o->kind = OP_SNAP; o->a = rng() % (sizeof snaps / sizeof *snaps); o->b = rng() % 3; }
            }
            else { o->kind = OP_AREA; o->a = rng() % 2; o->b = rng() % 3; }
        }
        else { o->kind = OP_LIST; o->a = rng() % L_COUNT; o->b = rng(); o->c = rng(); }
    }
    s->op[s->nops++].kind = OP_DEINIT;
}

/* ---- the C half's world, per run ------------------------------------------------- */

#define MAXAREAS 24
static void *cell;                  /* the list head's cell, from +0x2c */
static int have_list;
static void *areas[MAXAREAS];
static unsigned nareas;

static void *pick_area(uint32_t r)
{
    if (nareas == 0 || r % 9 == 0)
        return NULL;
    return areas[r % nareas];
}

static void forget_area(void *a)
{
    unsigned i;
    for (i = 0; i < nareas; i++)
        if (areas[i] == a) {
            areas[i] = areas[--nareas];
            return;
        }
}

/* a record in one of `a`'s lists, by walking it the way the module would */
static void *pick_record(void *a, uint32_t r, uint32_t *type)
{
    static const uint32_t heads[] = { 0xb0, 0xb4, 0xb8, 0xbc };
    unsigned h = r % 4, n = 0, k;
    void *e;
    *type = h < 3 ? h + 1 : 4 + (r >> 4) % 3;
    if (a == NULL)
        return NULL;
    for (e = *(void **)((uint8_t *)a + heads[h]); e; e = *(void **)e) n++;
    if (n == 0)
        return NULL;
    k = (r >> 8) % n;
    for (e = *(void **)((uint8_t *)a + heads[h]); k--; e = *(void **)e) ;
    return e;
}

static void *fresh(uint32_t size)
{
    void *p = arena + arena_used;
    arena_used += (size + 15) & ~15u;
    return p;
}

static void run_list_op(const struct op *o, const struct wx_linkage *l)
{
    void *a, *b, *rec;
    uint32_t t, r;

    switch (o->a) {
    case L_CREATE:
        if (have_list) return;
        put("\nL.create"); r = (uint32_t)l->list_create(&cell); put(" =%d cell=", r); put_ptr(cell);
        have_list = 1;
        break;
    case L_DESTROY:
        if (!have_list || o->b % 6) return;
        put("\nL.destroy"); r = (uint32_t)l->list_destroy(&cell); put(" =%d", r);
        have_list = 0; nareas = 0;
        break;
    case L_AREA_NEW:
        if (!have_list || nareas == MAXAREAS) return;
        put("\nL.area_new"); a = l->area_new(o->b % 13 ? cell : NULL); put(" ="); put_ptr(a);
        if (a && o->b % 13) areas[nareas++] = a;
        break;
    case L_AREA_REMOVE:
        if (!have_list) return;
        a = pick_area(o->b);
        if (a == NULL && o->c % 3) return;
        if (o->c % 7 == 0) a = fresh(0xc0);         /* not in the list: freed anyway */
        put("\nL.area_remove("); put_ptr(a); put(")");
        r = (uint32_t)l->area_remove(cell, a); put(" =%d", r);
        if (a) forget_area(a); else nareas = 0;
        break;
    case L_AREA_COPY:
        a = pick_area(o->b); b = pick_area(o->c);
        if (!a || !b || a == b) return;
        put("\nL.area_copy("); put_ptr(a); put(","); put_ptr(b); put(")");
        r = l->area_copy(a, b); put(" =%u", r);
        break;
    case L_AREA_NEXT:
        a = pick_area(o->b);
        put("\nL.area_next("); put_ptr(a); put(")");
        put(" ="); put_ptr(l->area_next(have_list && o->c % 7 ? cell : NULL, a));
        break;
    case L_AREA_APPEND:
        /* only something not already in the list, so no cycle */
        a = o->b % 3 ? NULL : fresh(0xc0);
        put("\nL.area_append("); put_ptr(a); put(")");
        r = l->area_append(have_list && o->c % 5 ? cell : NULL, a); put(" ="); put_ptr((void *)r);
        if (a && have_list && o->c % 5 && nareas < MAXAREAS) areas[nareas++] = a;
        break;
    case L_AREA_COUNT:
        put("\nL.area_count = %u", l->area_count(have_list && o->b % 7 ? cell : NULL));
        break;
    case L_REC_NEW:
        a = pick_area(o->b);
        t = o->c % 9;                       /* 0, 7 and 8 are not types */
        put("\nL.rec_new("); put_ptr(a); put(",%u)", t);
        put(" ="); put_ptr(l->record_new(a, t));
        break;
    case L_REC_FREE:
        a = pick_area(o->b);
        if (!a) return;
        rec = o->c % 3 ? NULL : pick_record(a, o->c >> 2, &t);
        if (!rec) t = o->c % 8;             /* by type, 0 for every type, 7 for none */
        if (o->c % 11 == 0) {               /* not in its list: freed anyway */
            rec = fresh(0x14);
            t = 1 + (o->c >> 4) % 6;
        }
        put("\nL.rec_free("); put_ptr(a); put(","); put_ptr(rec); put(",%u)", t);
        r = l->record_free(a, rec, t); put(" =%u", r);
        break;
    case L_RECS_COPY:
        a = pick_area(o->b); b = pick_area(o->c);
        if (!b || a == b) return;
        rec = (o->b >> 8) % 3 ? NULL : pick_record(a, o->c >> 8, &t);
        if (!rec) t = (o->c >> 4) % 8;
        put("\nL.recs_copy("); put_ptr(a); put(","); put_ptr(b); put(","); put_ptr(rec);
        put(",%u,%u)", t, (o->b >> 12) % 2);
        r = l->records_copy(a, b, rec, t, (o->b >> 12) % 2); put(" =%u", r);
        break;
    case L_REC_NEXT:
        a = pick_area(o->b);
        rec = o->c % 2 ? pick_record(a, o->c >> 2, &t) : NULL;
        t = (o->c >> 12) % 8;
        put("\nL.rec_next("); put_ptr(a); put(","); put_ptr(rec); put(",%u)", t);
        put(" ="); put_ptr(l->record_next(a, rec, t));
        break;
    case L_REC_APPEND:
        /* a fresh record of the list's own type, so the lists stay uniform */
        a = pick_area(o->b);
        t = 1 + o->c % 6;
        rec = NULL;
        if (o->c % 4) {
            rec = fresh(0x14);
            memset(rec, 0, 0x14);
            *(uint16_t *)((uint8_t *)rec + 4) = (uint16_t)t;
            if (t >= 4) *(uint16_t *)((uint8_t *)rec + 6) = (uint16_t)(1 + (o->c >> 8) % 0xfffe);
            *(uint32_t *)((uint8_t *)rec + 8) = o->b;
        }
        put("\nL.rec_append("); put_ptr(a); put(","); put_ptr(rec); put(",%u)", t);
        r = l->record_append(a, rec, t); put(" ="); put_ptr((void *)r);
        break;
    case L_POKE:
        /* a payload byte of an area or record; never a link or a type */
        a = pick_area(o->b);
        if (!a) return;
        if (o->c % 2) {
            rec = pick_record(a, o->c >> 1, &t);
            if (!rec) return;
            ((uint8_t *)rec)[8 + (o->c >> 16) % 4] = (uint8_t)(o->b >> 8);
            put("\nL.poke rec");
        } else {
            ((uint8_t *)a)[6 + (o->c >> 16) % 0xa8] = (uint8_t)(o->b >> 8);
            put("\nL.poke area");
        }
        break;
    }
}

/* ---- running ---------------------------------------------------------------------- */

static unsigned long cover_between, cover_inside, cover_outside, cover_wind[3],
                     cover_effect[5], cover_list[L_COUNT], cover_temp[5], cover_cloud;

/* after an update, classify what the position saw (counted on the retail run) */
static void count_update(const uint8_t *pos)
{
    uint16_t w1 = G16(global_blob, 0xfe0), w2 = G16(global_blob, 0x10c8), w;
    int16_t alt = (int16_t)*(uint16_t *)(pos + 0x14);
    static const uint16_t L[4] = { 0xebe, 0xeba, 0xeb6, 0xeb2 };
    int k;

    w = (int16_t)w1 < (int16_t)w2 ? w1 : w2;
    if (G16(global_blob, 0xf90) | G16(global_blob, 0x1078)) {
        if (w == 0x4000) cover_outside++;
        else if (w == 0) cover_inside++;
        else cover_between++;
    }
    if (G16(global_blob, 0xf0a) | G16(global_blob, 0xf0c) | G16(global_blob, 0xf0e))
        cover_cloud++;
    if (alt <= (int16_t)G16(global_blob, 0xebe)) { cover_temp[0]++; return; }
    for (k = 1; k < 4; k++) {
        uint16_t top = G16(global_blob, L[k]);
        if (top == 0xd8f1) break;
        if (alt <= (int16_t)top) { cover_temp[k]++; return; }
    }
    cover_temp[4]++;
}

static void run(struct side *sd, const struct scenario *s, int count)
{
    unsigned i, k;
    uint16_t t1, t2;

    cur = sd;
    memcpy(sd->image + IMAGE_LO, sd->pristine + IMAGE_LO, IMAGE_HI - IMAGE_LO);
    memcpy(global_blob, s->global, GLOBAL_SIZE);
    memset(arena, 0, sizeof arena);
    arena_used = 0;
    *util_seed = s->seed;
    tlen = 0;
    tlog[0] = 0;
    have_list = 0;
    nareas = 0;
    cell = NULL;
    chain->init();

    for (i = 0; i < s->nops; i++) {
        const struct op *o = &s->op[i];
        switch (o->kind) {
        case OP_INIT:   put("\nINIT"); sd->link->init(); break;
        case OP_DEINIT: put("\nDEINIT"); sd->link->deinit(); break;
        case OP_HERE:
            put("\nHERE");
            sd->link->weather_here();
            if (count) count_update(global_blob + 0x560);
            break;
        case OP_AT:
            memcpy(position, o->data, sizeof position);
            put("\nAT");
            sd->link->weather_at(position);
            if (count) count_update(position);
            break;
        case OP_TICK:
            for (k = 0; k < o->a; k++) {
                put("\nTICK");
                chain->dispatch(3, k, 0);
            }
            break;
        case OP_TEMP:
            t1 = t2 = 0x5555;
            sd->link->temperature(o->a, &t1, &t2);
            put("\nTEMP(%x) = %04x %04x", o->a, t1, t2);
            break;
        case OP_RESET:
            /* +0x24's default temperature layers are 320 m apart, which faults
             * retail above 500 m: log them, then put odd-gap ones back */
            put("\nRESET");
            sd->link->reset();
            if (G8(global_blob, 0xf12) | G8(global_blob, 0xf13)) {
                put(" temps");
                for (k = 0; k < 0x18; k += 2) put(" %04x", G16(global_blob, 0xf34 + k));
                memcpy(global_blob + 0xf34, o->data, 0x18);
            }
            break;
        case OP_EFFECT:
        {
            uint8_t rec[8];
            memcpy(rec, o->data, 8);
            if (o->a)
                *(uint16_t *)(rec + 2) = (uint16_t)(G16(global_blob, 0xef2) - (o->a == 1 ? 0x4000 : 0xc000));
            put("\nEFFECT %u %04x %04x", rec[1], *(uint16_t *)(rec + 2), *(uint16_t *)(rec + 4));
            sd->link->effect(rec);
        }
            if (count && o->data[1] >= 1 && o->data[1] <= 4) cover_effect[o->data[1]]++;
            break;
        case OP_POKE: {
            uint8_t *p = global_blob + pokes[o->a].off;
            if (pokes[o->a].size == 1) *p = (uint8_t)o->b;
            else if (pokes[o->a].size == 2) *(uint16_t *)p = (uint16_t)o->b;
            else *(uint32_t *)p = o->b;
            put("\nPOKE %#x=%#x", pokes[o->a].off, o->b);
            break;
        }
        case OP_AREA: {
            /* switch an area on or off, as the weather dialog would */
            uint32_t off = o->a ? 0x1078 : 0xf90;
            G16(global_blob, off) = (uint16_t)(o->b != 0);
            put("\nAREA %#x=%u", off, o->b != 0);
            break;
        }
        case OP_TUNE: {
            /* Put area 2's edge exactly `a` metres inside the position along
             * its axis, with a fade `b` metres wide, so the weight lands on
             * the boundaries (0x2000, and the fade's own end).  WX_X after an
             * update is the position's offset in area 2's frame. */
            int32_t x;
            G16(global_blob, 0x1078) = 1;
            sd->link->weather_here();
            x = *(int32_t *)(sd->image + 0xa8);
            if (x < 0) x = -x;
            if (x >= (int32_t)o->a) {
                G32(global_blob, 0x1078 + 0x24) = 2 * (uint32_t)(x - (int32_t)o->a);
                G32(global_blob, 0x1078 + 0x28) = o->b;
            }
            put("\nTUNE %x", x);
            sd->link->weather_here();
            if (count) count_update(global_blob + 0x560);
            break;
        }
        case OP_SNAP: {
            /* the aircraft exactly on (or one metre either side of) a boundary
             * the last update published */
            uint16_t at = snaps[o->a] ? G16(global_blob, snaps[o->a]) : 0x2af8;
            if (snaps[o->a] == 0xeee) at = (uint16_t)(at + G16(global_blob, 0x21));
            at = (uint16_t)(at + o->b - 1);
            G32(global_blob, 0x574) = at;
            put("\nSNAP %#x %04x", snaps[o->a], at);
            sd->link->weather_here();
            if (count) count_update(global_blob + 0x560);
            break;
        }
        case OP_OBJECT:
            G32(global_blob, 0x1164) = o->a;
            put("\nOBJECT %x", o->a);
            break;
        case OP_LIST: {
            unsigned before = tlen;
            run_list_op(o, sd->link);
            if (count && tlen != before) cover_list[o->a]++;
            break;
        }
        }
    }
    put("\nEND");
}

/* ---- loading ---------------------------------------------------------------------- */

static unsigned fault_scenario;
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    printf("\nFAULT %#lx at %p in %s, scenario %u\n", e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress, cur ? cur->name : "setup", fault_scenario);
    printf("transcript tail: ...%s\n", tlen > 800 ? tlog + tlen - 800 : tlog);
    if (getenv("WXDEBUG")) {
        unsigned i;
        for (i = 0; i < 0x60; i += 4)
            printf("  f90+%02x %08x   1078+%02x %08x\n", i, G32(global_blob, 0xf90 + i), i, G32(global_blob, 0x1078 + i));
        printf("  pos %08x %08x %08x\n", G32(global_blob, 0x564), G32(global_blob, 0x56c), G32(global_blob, 0x574));
    }
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void bind_imports(struct import *im)
{
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host_blob; break;
        case 2:  im->link = global_blob; break;
        case 3:  im->link = main_blob; break;
        case 4:  im->link = chain_blob; break;
        case 14: im->link = g3d_blob; break;
        case 20: im->link = util_fwd; break;
        default: printf("unexpected import %u\n", im->id); exit(2);
        }
    }
}

static void *load_support(const char *path, uint32_t id)
{
    HMODULE h = LoadLibraryA(path);
    uint8_t *l;
    if (!h) { printf("cannot load %s (%lu)\n", path, GetLastError()); exit(2); }
    l = (uint8_t *)GetProcAddress(h, "Linkage");
    if (!l || *(uint32_t *)l != id) { printf("%s: not module %u\n", path, id); exit(2); }
    return l;
}

static int load_side(struct side *sd, const char *path, const char *name)
{
    struct import *imports;
    sd->name = name;
    sd->dll = LoadLibraryA(path);
    if (!sd->dll) { printf("cannot load %s (%lu)\n", path, GetLastError()); return 0; }
    imports = (struct import *)GetProcAddress(sd->dll, "ImportTable");
    sd->link = (struct wx_linkage *)GetProcAddress(sd->dll, "Linkage");
    if (!imports || !sd->link || sd->link->id != 27) { printf("%s: not WEATHERM\n", path); return 0; }
    sd->image = (uint8_t *)sd->link - 0x38;
    if ((uint8_t *)imports != sd->image) { printf("%s: ImportTable is not at Linkage-0x38\n", path); return 0; }
    bind_imports(imports);
    memcpy(sd->pristine, sd->image, IMAGE_HI);
    return 1;
}

static void dump_diff(const char *what, const uint8_t *r, const uint8_t *o, unsigned lo, unsigned n)
{
    unsigned i, shown = 0;
    for (i = lo; i < n && shown < 10; i++)
        if (r[i] != o[i]) {
            printf("  %s+%#x: retail %02x ours %02x\n", what, i, r[i], o[i]);
            shown++;
        }
}

int main(int argc, char **argv)
{
    static char rlog[sizeof tlog];
    static uint8_t rimage[IMAGE_HI], rglobal[GLOBAL_SIZE], rarena[ARENA_SIZE];
    static struct side retail, ours;
    static struct scenario s;
    unsigned v, n = argc > 1 ? (unsigned)atoi(argv[1]) : 20000, bad = 0, k;
    struct import *im;
    uint32_t rused;

    SetUnhandledExceptionFilter(on_fault);

    *(uint32_t *)host_blob = 1;
    *(void **)(host_blob + 0x20c) = (void *)h_alloc;
    *(void **)(host_blob + 0x214) = (void *)h_free;
    *(uint32_t *)g3d_blob = 14;
    *(void **)(g3d_blob + 0x20) = (void *)g3d_release;
    *(uint32_t *)main_blob = 3;
    *(void **)(main_blob + 0x94) = (void *)main_weather;

    chain = (struct chain_linkage *)load_support("CHAIN_retail.dll", 4);
    util_link = (uint8_t *)load_support("UTIL_retail.dll", 20);
    for (im = (struct import *)GetProcAddress(GetModuleHandleA("CHAIN_retail.dll"), "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : (void *)global_blob;
    im = (struct import *)GetProcAddress(GetModuleHandleA("UTIL_retail.dll"), "ImportTable");
    util_seed = (uint32_t *)((uint8_t *)im + 0xf0);
    for (; im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : im->id == 2 ? (void *)global_blob : (void *)main_blob;
    *(uint32_t *)chain_blob = 4;
    *(void **)(chain_blob + 0x18) = (void *)c_add;
    *(void **)(chain_blob + 0x1c) = (void *)c_remove;
    *(uint32_t *)util_fwd = 20;
    *(void **)(util_fwd + 0x24) = (void *)u_mul48;
    *(void **)(util_fwd + 0x2c) = (void *)u_sincos;
    *(void **)(util_fwd + 0x34) = (void *)u_rotate;
    *(void **)(util_fwd + 0x3c) = (void *)u_atan2;
    *(void **)(util_fwd + 0x44) = (void *)u_to_lon;
    *(void **)(util_fwd + 0x48) = (void *)u_to_m;
    *(void **)(util_fwd + 0x6c) = (void *)u_rand;

    if (!load_side(&retail, "WEATHERM_retail.dll", "retail")) return 2;
    if (!load_side(&ours, "WEATHERM_ours.dll", "openfs98")) return 2;
    if (memcmp(retail.pristine + IMAGE_LO, ours.pristine + IMAGE_LO, IMAGE_HI - IMAGE_LO)) {
        printf("initial images differ\n");
        dump_diff("img", retail.pristine, ours.pristine, IMAGE_LO, IMAGE_HI);
        bad++;
    }

    for (v = 0; v < n; v++) {
        make_scenario(&s, v);
        fault_scenario = v;
        run(&retail, &s, 1);
        memcpy(rlog, tlog, tlen + 1);
        memcpy(rimage, retail.image, IMAGE_HI);
        memcpy(rglobal, global_blob, GLOBAL_SIZE);
        rused = arena_used;
        memcpy(rarena, arena, rused);
        run(&ours, &s, 0);
        if (strcmp(rlog, tlog) || memcmp(rimage + IMAGE_LO, ours.image + IMAGE_LO, IMAGE_HI - IMAGE_LO)
            || memcmp(rglobal, global_blob, GLOBAL_SIZE) || rused != arena_used
            || memcmp(rarena, arena, rused)) {
            if (bad++ < 4) {
                unsigned at = 0;
                while (rlog[at] && rlog[at] == tlog[at]) at++;
                printf("scenario %u differs", v);
                if (rlog[at] || tlog[at])
                    printf(" in the transcript at byte %u:\n  retail ...%.400s\n  ours   ...%.400s\n",
                           at, rlog + (at > 200 ? at - 200 : 0), tlog + (at > 200 ? at - 200 : 0));
                else
                    printf(":\n");
                dump_diff("img", rimage, ours.image, IMAGE_LO, IMAGE_HI);
                dump_diff("global", rglobal, global_blob, 0, GLOBAL_SIZE);
                dump_diff("arena", rarena, arena, 0, rused);
            }
        }
    }

    printf("covered: areas outside/between/inside %lu/%lu/%lu, in cloud %lu\n",
           cover_outside, cover_between, cover_inside, cover_cloud);
    printf("temperature band: below %lu, layers 1-3 %lu/%lu/%lu, above %lu\n",
           cover_temp[0], cover_temp[1], cover_temp[2], cover_temp[3], cover_temp[4]);
    printf("effects 1-4: %lu/%lu/%lu/%lu\nlist ops:", cover_effect[1], cover_effect[2],
           cover_effect[3], cover_effect[4]);
    for (k = 0; k < L_COUNT; k++) printf(" %lu", cover_list[k]);
    printf("\n%u scenarios, %u differ\n", n, bad);
    if (!bad) printf("IDENTICAL\n");
    (void)cover_wind;
    return bad != 0;
}
