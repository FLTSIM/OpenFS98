/* Differential test for individual UTIL.DLL services.
 *
 * UTIL cannot be swapped into the game until all 47 of its slots exist, so a
 * part-finished module cannot be tested by running the sim.  Instead this
 * loads the retail UTIL.DLL, reads the service pointers straight out of its
 * Linkage, and compares each against the reimplementation over a sweep of
 * inputs -- edge cases first, then randomised.
 *
 * The services covered here touch neither GLOBAL nor NUMERIC, so retail's
 * ImportTable is left unbound; they never follow those pointers.  A service
 * that does reach for an import will need the binding that slewdiff.c does.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define FS98_STDCALL __attribute__((stdcall))
int32_t FS98_STDCALL fs98_util_sqrt32(uint32_t v);
int32_t FS98_STDCALL fs98_util_sqrt64(const void *p);
int32_t FS98_STDCALL fs98_util_hypot(int16_t a, int16_t b);
void    FS98_STDCALL fs98_util_sincos(int16_t a, int16_t *s, int16_t *c);
void    FS98_STDCALL fs98_util_metres_to_longitude(const void *in, const void *pos, void *out);
int32_t FS98_STDCALL fs98_util_stub_50(int32_t, int32_t);
int32_t FS98_STDCALL fs98_util_stub_54(int32_t, int32_t);
int32_t FS98_STDCALL fs98_util_stub_58(int32_t, int32_t);
void    FS98_STDCALL fs98_util_stub_5c(void);
void    FS98_STDCALL fs98_util_deinit(void);
int32_t  FS98_STDCALL fs98_util_sqrt16(uint16_t v);
uint32_t FS98_STDCALL fs98_util_rand(void);
uint32_t FS98_STDCALL fs98_util_rand32(void);
void     FS98_STDCALL fs98_util_copy24(void *dst, const void *src);
int32_t  FS98_STDCALL fs98_util_double_to_int(double v);
void                  fs98_util_rand_seed(uint32_t seed);
void     FS98_STDCALL fs98_util_v48_add(void *, const void *, const void *);
void     FS98_STDCALL fs98_util_v48_sub(void *, const void *, const void *);
int32_t  FS98_STDCALL fs98_util_v48_equal(const void *, const void *);
void     FS98_STDCALL fs98_util_shr48(void *, const void *, uint32_t);
int32_t  FS98_STDCALL fs98_util_double_to_int64_low(double);
void     FS98_STDCALL fs98_util_rotate2d(int32_t, int32_t, int16_t, int16_t,
                                         int32_t *, int32_t *);
void     FS98_STDCALL fs98_util_div48_u16(const void *, uint16_t, void *);
void     FS98_STDCALL fs98_util_metres32_to_longitude(const int32_t *,
                                                      const void *, void *);
void     FS98_STDCALL fs98_util_mul48_s16(const void *, int16_t, void *);
void     FS98_STDCALL fs98_util_unpack_position(const void *, void *);
int32_t  FS98_STDCALL fs98_util_atan2(int32_t, int32_t);
int32_t  FS98_STDCALL fs98_util_angle_from_sin(int16_t, int16_t);
void     FS98_STDCALL fs98_util_fixed48_to_double(double *, const void *);
void     FS98_STDCALL fs98_util_double_to_fixed48(const double *, void *);
void     FS98_STDCALL fs98_util_double_to_fixed48_signed(const double *, void *);
void     FS98_STDCALL fs98_util_fixed48_to_double_signed(double *, const void *);
void     FS98_STDCALL fs98_util_range_and_bearing(const int32_t *, const int32_t *,
                                                  int32_t, int32_t *, int16_t *);
void     FS98_STDCALL fs98_util_longitude_to_metres(const void *, const void *, void *);
void     FS98_STDCALL fs98_util_rotate3d(const int16_t *, const int16_t *, int32_t *);
void     FS98_STDCALL fs98_util_range_and_bearing48(const void *, const void *,
                                                    int32_t, int32_t *, int16_t *,
                                                    uint8_t *);
void     FS98_STDCALL fs98_util_marshal(const void *, const void *, int32_t);
double   FS98_STDCALL fs98_util_frand(void);
void                  fs98_util_frand_seed(double);
const char * FS98_STDCALL fs98_util_longer_string(const char *, const char *);
void                  fs98_util_set_host(void *);
uint32_t              fs98_util_rand_get(void);
double                fs98_util_frand_get(void);
void     FS98_STDCALL fs98_util_init(void);
uint32_t FS98_STDCALL fs98_util_rand(void);
int32_t  FS98_STDCALL fs98_util_exchange_put(void *, uint32_t, void *, int32_t);
void     FS98_STDCALL fs98_util_exchange_get(void *, uint32_t, void *);
void                  fs98_util_set_global(void *);
void                  fs98_util_set_numeric(void *);
int32_t  FS98_STDCALL fs98_util_alert_modal(uint32_t, uint32_t, uint32_t, uint32_t,
                                            uint32_t, uint32_t, uint32_t, int32_t);
void     FS98_STDCALL fs98_util_alert_open(void **, uint32_t, uint32_t, uint32_t,
                                           uint32_t, uint32_t, uint32_t, int32_t);
void     FS98_STDCALL fs98_util_alert_close(void **);

typedef int32_t (FS98_STDCALL *fn_sqrt32)(uint32_t);
typedef int32_t (FS98_STDCALL *fn_sqrt64)(const void *);
typedef int32_t (FS98_STDCALL *fn_hypot)(int16_t, int16_t);
typedef void    (FS98_STDCALL *fn_sincos)(int16_t, int16_t *, int16_t *);
typedef void    (FS98_STDCALL *fn_m2lon)(const void *, const void *, void *);
typedef int32_t (FS98_STDCALL *fn_pick2)(int32_t, int32_t);
typedef void    (FS98_STDCALL *fn_void)(void);
typedef int32_t  (FS98_STDCALL *fn_sqrt16)(uint16_t);
typedef uint32_t (FS98_STDCALL *fn_rand)(void);
typedef void     (FS98_STDCALL *fn_copy24)(void *, const void *);
typedef int32_t  (FS98_STDCALL *fn_d2i)(double);
typedef void     (FS98_STDCALL *fn_v48op)(void *, const void *, const void *);
typedef int32_t  (FS98_STDCALL *fn_v48eq)(const void *, const void *);
typedef void     (FS98_STDCALL *fn_shr48)(void *, const void *, uint32_t);
typedef void     (FS98_STDCALL *fn_rot2d)(int32_t, int32_t, int16_t, int16_t,
                                          int32_t *, int32_t *);
typedef void     (FS98_STDCALL *fn_div48)(const void *, uint16_t, void *);
typedef void     (FS98_STDCALL *fn_m32lon)(const int32_t *, const void *, void *);
typedef void     (FS98_STDCALL *fn_mul48)(const void *, int16_t, void *);
typedef void     (FS98_STDCALL *fn_unpack)(const void *, void *);
typedef int32_t  (FS98_STDCALL *fn_atan2)(int32_t, int32_t);
typedef int32_t  (FS98_STDCALL *fn_asin)(int16_t, int16_t);
typedef void     (FS98_STDCALL *fn_f48d)(double *, const void *);
typedef void     (FS98_STDCALL *fn_df48)(const double *, void *);
typedef double   (FS98_STDCALL *fn_frand)(void);
typedef const char * (FS98_STDCALL *fn_longer)(const char *, const char *);
typedef uint32_t (FS98_STDCALL *fn_strlen)(const char *);

/* A stand-in host.  Only one of its 194 services is needed here: +0x2b8, the
 * null-safe strlen that UTIL's string comparison measures through. */
static unsigned char host_blob[0x320];
#define HOST_SLOT(off) (*(void **)(host_blob + (off)))

/* A clock the test controls: a fixed sequence including the zero that the
 * retry loop in init exists to skip. */
static const uint32_t tick_seq[] = { 12345, 0, 0, 777, 1, 0xffffffffu, 2 };
static unsigned tick_pos;

static uint32_t FS98_STDCALL stub_ticks(void)
{
    uint32_t v = tick_seq[tick_pos % (sizeof tick_seq / sizeof tick_seq[0])];
    tick_pos++;
    return v;
}

static uint32_t FS98_STDCALL stub_strlen(const char *s)
{
    uint32_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

/* ---- slots 0x8c / 0x90: an arena, and stubs that log what they are asked --
 *
 * Both sides run against the SAME arena, restored between them, so every
 * pointer the engine stores or passes is identical and the bytes can be
 * compared directly.  The fake GLOBAL block is the arena's start, so a
 * relocated descriptor points into it. */
#define X_GLOBAL   0
#define X_DESC     256
#define X_RECS     640
#define X_POOL     1024             /* 32 strings of 32 bytes */
#define X_ABS      2048             /* variables addressed absolutely */
#define X_CBOUT    3072             /* where the callback leaves its marks */
#define X_ARENA    4096

#define X_BITS8_T  0x41100000u
#define X_BITS16_T 0x42200000u
#define X_BITS32_T 0x44400000u

static unsigned char x_arena[X_ARENA], x_init[X_ARENA];
static unsigned char numeric_blob[0x40];

struct x_call { uint32_t what, a, b, c; };
static struct x_call x_log[64];
static unsigned x_nlog;

/* a pointer inside the arena as its offset; anything else -- retail's and
 * our own scratch buffers -- as one marker */
static uint32_t x_norm(const void *p)
{
    const unsigned char *q = p;
    if (q >= x_arena && q < x_arena + X_ARENA)
        return (uint32_t)(q - x_arena);
    return 0xffffffffu;
}

static void x_note(uint32_t what, uint32_t a, uint32_t b, uint32_t c)
{
    if (x_nlog < 64) {
        x_log[x_nlog].what = what; x_log[x_nlog].a = a;
        x_log[x_nlog].b = b; x_log[x_nlog].c = c;
    }
    x_nlog++;
}

static int32_t FS98_STDCALL stub_xcb(uint32_t kind, void *base, int32_t mode)
{
    x_note(1, kind, x_norm(base), (uint32_t)mode);
    x_arena[X_CBOUT + (kind & 63)] += (unsigned char)(mode + 1);
    return ((kind + (uint32_t)mode) % 3 == 0) ? 0 : (int32_t)(kind ^ (uint32_t)mode);
}

static int32_t FS98_STDCALL stub_parse(const char *text, uint32_t fmt, void *out)
{
    uint32_t h = fmt * 2654435761u;
    const char *t = text;
    while (*t) h = h * 31 + (unsigned char)*t++;
    x_note(2, x_norm(text), fmt, x_norm(out));
    *(uint32_t *)out = h;
    return (fmt & 3) ? (int32_t)(fmt & 7) : 0;
}

static char * FS98_STDCALL stub_format(const void *value, uint32_t fmt, char *out)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char v = *(const unsigned char *)value;
    x_note(3, x_norm(value), fmt, x_norm(out));
    out[0] = 'F'; out[1] = hex[fmt & 15]; out[2] = hex[v >> 4]; out[3] = hex[v & 15];
    out[4] = 0;
    return out;
}

static char * FS98_STDCALL stub_strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++) != 0) ;
    return r;
}

/* ---- slots 0xa4 / 0xa8 / 0xac: a host that records what the alerts do ----
 *
 * Heap blocks are numbered in allocation order, so "freed block 3" can be
 * compared across the two sides even though the addresses differ.  The dialog
 * stub snapshots the descriptor it is handed -- with the text pointer replaced
 * by the text, and the handler checked separately -- and answers modal alerts
 * from a script. */
static uint32_t rng(void);
#define A_MAXBLK 64
static void *a_blk[A_MAXBLK];
static unsigned a_nblk;
static char a_log[8192];
static unsigned a_len;
static int32_t a_answer;
static void *a_handler_seen;

static void a_put(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (a_len < sizeof a_log)
        a_len += (unsigned)vsnprintf(a_log + a_len, sizeof a_log - a_len, fmt, ap);
    va_end(ap);
}

static int a_index(const void *p)
{
    unsigned i;
    for (i = 0; i < a_nblk; i++) if (a_blk[i] == p) return (int)i;
    return -1;
}

static int32_t FS98_STDCALL a_alloc(int32_t fl, uint32_t size, void **out, int32_t tag)
{
    unsigned char *p = malloc(size);
    memset(p, 0xab, size);                  /* "uninitialised", alike on both sides */
    a_put("alloc#%u(%d,%u,tag %d) ", a_nblk, fl, size, tag);
    if (a_nblk < A_MAXBLK) a_blk[a_nblk++] = p;
    *out = p;
    return 0;
}

static int32_t FS98_STDCALL a_realloc(int32_t fl, uint32_t size, void **pp)
{
    int i = a_index(*pp);
    void *q = realloc(*pp, size);
    a_put("realloc#%d(%d,%u) ", i, fl, size);
    if (i >= 0) a_blk[i] = q;
    *pp = q;
    return 0;
}

static int32_t FS98_STDCALL a_free(void **pp)
{
    int i = *pp ? a_index(*pp) : -2;
    a_put("free#%d ", i);
    if (i >= 0) a_blk[i] = NULL;            /* malloc will hand the address out again */
    free(*pp);
    *pp = NULL;
    return 0;
}

static int32_t FS98_STDCALL a_loadstring(uint32_t id, char *buf, int32_t len)
{
    /* a length that depends on the id, long enough to be truncated at 254 */
    int32_t n = (int32_t)((id * 7) % 300), k;
    if (n > len - 1) n = len - 1;
    for (k = 0; k < n; k++) buf[k] = (char)('A' + (id + (uint32_t)k) % 26);
    buf[n] = 0;
    a_put("load(%u,%d) ", id, len);
    return n;
}

static char * FS98_STDCALL a_strcat(char *d, const char *s)
{
    strcat(d, s);
    return d;
}

static int32_t FS98_STDCALL a_dialog(uint32_t id, void *param)
{
    uint32_t *w = param;
    const char *text = (const char *)(uintptr_t)(id == 2 ? w[5] : w[9]);
    a_handler_seen = (void *)(uintptr_t)w[1];
    if (id == 2)
        a_put("dialog(2: %#x,%#x,%#x | %s) ", w[0], w[2], w[4], text);
    else
        a_put("dialog(%u: %#x,%#x,%#x,%#x,%08x%08x%08x%08x | %s) ", id, w[0], w[2], w[3],
              w[4], w[5], w[6], w[7], w[8], text);
    if (id == 2) w[3] = (uint32_t)a_answer;
    return 0;
}

static void FS98_STDCALL a_tick(void)
{
    a_put("tick ");
}

static int32_t FS98_STDCALL a_quit(void)
{
    a_put("quit ");
    return 1;
}

static uint32_t a_piece(char *pool, unsigned slot)
{
    unsigned r = rng() % 10, n, k;
    char *s = pool + slot * 400;
    if (r < 2) return 0;
    if (r < 6) return 1 + rng() % 0xffff;   /* a string-resource id */
    n = rng() % 390;                        /* a char *, sometimes long */
    for (k = 0; k < n; k++) s[k] = (char)('a' + rng() % 26);
    s[n] = 0;
    return (uint32_t)(uintptr_t)s;
}

/* 48-bit value: 16-bit low half at +2, 32-bit high half at +4 */
static void put48(unsigned char *p, uint16_t lo, int32_t hi)
{
    *(uint16_t *)(p + 2) = lo;
    *(int32_t  *)(p + 4) = hi;
}

static void *slot(HMODULE h, unsigned off)
{
    unsigned char *link = (unsigned char *)GetProcAddress(h, "Linkage");
    return link ? *(void **)(link + off) : NULL;
}

/* A fault in either implementation must say where it happened and end the
 * run, not leave wine's crash dialog waiting for someone to close it. */
static const char *fault_where = "startup";
static const uint32_t *fault_descs;
static unsigned fault_vector;

static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    printf("\nFAULT %#lx at %p, reading/writing %p, in %s vector %u\n",
           e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress,
           e->ExceptionRecord->NumberParameters > 1
               ? (void *)e->ExceptionRecord->ExceptionInformation[1] : NULL,
           fault_where, fault_vector);
    if (fault_descs) {
        unsigned i;
        for (i = 0; i < 8; i++)
            printf("   desc %u: kind %#x off %#x sub %#x var %#x reloc %u\n", i,
                   fault_descs[5*i], fault_descs[5*i+1], fault_descs[5*i+2],
                   fault_descs[5*i+3], fault_descs[5*i+4]);
    }
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static uint32_t rng_state = 0x9e3779b9u;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

int main(void)
{
    HMODULE h = LoadLibraryA("UTIL_retail.dll");
    fn_sqrt32 r_sqrt32; fn_sqrt64 r_sqrt64; fn_hypot r_hypot;
    fn_sincos r_sincos; fn_m2lon r_m2lon;
    unsigned i, bad = 0, checked = 0;

    SetUnhandledExceptionFilter(on_fault);
    if (!h) { printf("cannot load UTIL_retail.dll (%lu)\n", GetLastError()); return 2; }
    r_sqrt32 = (fn_sqrt32)slot(h, 0x1c);
    r_sqrt64 = (fn_sqrt64)slot(h, 0x18);
    r_hypot  = (fn_hypot) slot(h, 0x28);
    r_sincos = (fn_sincos)slot(h, 0x2c);
    r_m2lon  = (fn_m2lon) slot(h, 0x44);
    if (!r_sqrt32 || !r_sqrt64 || !r_hypot || !r_sincos || !r_m2lon) {
        printf("missing service slots\n"); return 2;
    }

    /* slot 0x1c -- sqrt32 */
    for (i = 0; i < 300000; i++) {
        uint32_t v = (i < 300) ? i : rng();
        int32_t a = r_sqrt32(v), b = fs98_util_sqrt32(v);
        checked++;
        if (a != b && bad++ < 8)
            printf("sqrt32(%u): retail %d, openfs98 %d\n", v, a, b);
    }

    /* slot 0x18 -- sqrt64 */
    for (i = 0; i < 300000; i++) {
        uint32_t q[2];
        int32_t a, b;
        if (i < 4) { q[0] = (uint32_t)i; q[1] = 0; }
        else       { q[0] = rng(); q[1] = rng(); }
        a = r_sqrt64(q); b = fs98_util_sqrt64(q);
        checked++;
        if (a != b && bad++ < 8)
            printf("sqrt64(%08x%08x): retail %d, openfs98 %d\n", q[1], q[0], a, b);
    }

    /* slot 0x28 -- hypot.  16 bit inputs, so sweep a slice exhaustively. */
    for (i = 0; i < 300000; i++) {
        int16_t x, y;
        int32_t a, b;
        if (i < 65536) { x = (int16_t)(i - 32768); y = (int16_t)(i & 0x7f); }
        else           { x = (int16_t)rng(); y = (int16_t)rng(); }
        a = r_hypot(x, y); b = fs98_util_hypot(x, y);
        checked++;
        if (a != b && bad++ < 8)
            printf("hypot(%d,%d): retail %d, openfs98 %d\n", x, y, a, b);
    }

    /* slot 0x2c -- sincos.  The whole 16 bit input domain, exhaustively. */
    for (i = 0; i < 65536; i++) {
        int16_t ang = (int16_t)i;
        int16_t rs = 0x5a5a, rc = 0x5a5a, os = 0x5a5a, oc = 0x5a5a;
        r_sincos(ang, &rs, &rc);
        fs98_util_sincos(ang, &os, &oc);
        checked++;
        if ((rs != os || rc != oc) && bad++ < 8)
            printf("sincos(%d): retail sin=%d cos=%d, openfs98 sin=%d cos=%d\n",
                   ang, rs, rc, os, oc);
    }
    /* slot 0x44 -- metres to longitude.
     *
     * Two constraints on the inputs, both real properties of the retail
     * routine rather than conveniences:
     *
     *  - Latitudes stay inside the table's range.  The index is the top bits
     *    of the latitude, and past roughly +/-0x4e retail walks off the end of
     *    the table into neighbouring data.  Matching it there would mean
     *    locking in whatever happens to sit in .data.
     *
     *  - The numerator stays small enough that the quotient fits in 32 bits.
     *    The routine divides a 64-bit numerator by a divisor of about 2^25
     *    using a single `div`, which raises #DE when the quotient overflows --
     *    retail crashes outright on a large step, so there is nothing to
     *    match.  Keeping the high half under 2^24 stays well clear. */
    for (i = 0; i < 400000; i++) {
        unsigned char in[8], pos[8], ra[8], ob[8];
        int32_t lat_hi = (int32_t)(rng() % 0x9c) - 0x4e;   /* table range */
        uint16_t lat_lo = (uint16_t)rng();
        int32_t n_hi = (int32_t)(rng() & 0x01ffffffu) - 0x01000000;
        uint16_t n_lo = (uint16_t)rng();

        if (i < 64) { lat_hi = (int32_t)i - 16; lat_lo = 0; n_hi = (int32_t)i - 32; n_lo = 0; }
        if (i % 7 == 0) n_hi >>= 12;    /* exercise small steps too */

        memset(in, 0, 8); memset(pos, 0, 8);
        memset(ra, 0xcd, 8); memset(ob, 0xcd, 8);
        put48(in, n_lo, n_hi);
        put48(pos, lat_lo, lat_hi);

        r_m2lon(in, pos, ra);
        fs98_util_metres_to_longitude(in, pos, ob);
        checked++;
        if (memcmp(ra, ob, 8) && bad++ < 8)
            printf("m2lon(in=%04x:%08x lat=%04x:%08x): retail %04x:%08x, "
                   "openfs98 %04x:%08x\n",
                   n_lo, n_hi, lat_lo, lat_hi,
                   *(uint16_t *)(ra + 2), *(uint32_t *)(ra + 4),
                   *(uint16_t *)(ob + 2), *(uint32_t *)(ob + 4));
    }

    /* slots 0x50, 0x54, 0x58 -- unfinished in retail: ignore the first
     * argument, return the second.  0x5c leaves EAX undefined, so there is no
     * return value to compare; it is called only to confirm it returns and
     * does not disturb the stack. */
    {
        fn_pick2 r50 = (fn_pick2)slot(h, 0x50);
        fn_pick2 r54 = (fn_pick2)slot(h, 0x54);
        fn_pick2 r58 = (fn_pick2)slot(h, 0x58);
        fn_void  r5c = (fn_void) slot(h, 0x5c);
        for (i = 0; i < 20000; i++) {
            int32_t a = (int32_t)rng(), bb = (int32_t)rng();
            struct { fn_pick2 r; int32_t (FS98_STDCALL *o)(int32_t, int32_t);
                     const char *n; } t[3] = {
                { r50, fs98_util_stub_50, "0x50" },
                { r54, fs98_util_stub_54, "0x54" },
                { r58, fs98_util_stub_58, "0x58" },
            };
            unsigned k;
            for (k = 0; k < 3; k++) {
                int32_t x = t[k].r(a, bb), y = t[k].o(a, bb);
                checked++;
                if (x != y && bad++ < 8)
                    printf("slot %s(%d,%d): retail %d, openfs98 %d\n",
                           t[k].n, a, bb, x, y);
            }
        }
        r5c();
        fs98_util_stub_5c();
        checked++;

        /* slot 0x08 -- deinit, a bare ret.  Called only to confirm it returns
         * and leaves the stack where it found it. */
        {
            fn_void r08 = (fn_void)slot(h, 0x08);
            r08();
            fs98_util_deinit();
            checked++;
        }
    }

    /* slot 0x20 -- sqrt of a 16 bit value.  Exhaustive: the domain is 65536. */
    {
        fn_sqrt16 r20 = (fn_sqrt16)slot(h, 0x20);
        for (i = 0; i < 65536; i++) {
            int32_t x = r20((uint16_t)i), y = fs98_util_sqrt16((uint16_t)i);
            checked++;
            if (x != y && bad++ < 8)
                printf("sqrt16(%u): retail %d, openfs98 %d\n", i, x, y);
        }
    }

    /* slots 0x6c and 0x70 -- the generator.  Stateful, so both are put on the
     * same seed first; retail keeps its seed in its own .data at RVA 0x50f0. */
    {
        fn_rand r6c = (fn_rand)slot(h, 0x6c);
        fn_rand r70 = (fn_rand)slot(h, 0x70);
        uint32_t *retail_seed = (uint32_t *)((unsigned char *)h + 0x50f0);

        *retail_seed = 12345; fs98_util_rand_seed(12345);
        for (i = 0; i < 200000; i++) {
            uint32_t x = r6c(), y = fs98_util_rand();
            checked++;
            if (x != y && bad++ < 8)
                printf("rand #%u: retail %u, openfs98 %u\n", i, x, y);
        }
        *retail_seed = 1; fs98_util_rand_seed(1);
        for (i = 0; i < 100000; i++) {
            uint32_t x = r70(), y = fs98_util_rand32();
            checked++;
            if (x != y && bad++ < 8)
                printf("rand32 #%u: retail %08x, openfs98 %08x\n", i, x, y);
        }
    }

    /* slot 0x7c -- copy twenty-four bytes */
    {
        fn_copy24 r7c = (fn_copy24)slot(h, 0x7c);
        for (i = 0; i < 20000; i++) {
            uint32_t src[6], da[6], db[6];
            unsigned k;
            for (k = 0; k < 6; k++) src[k] = rng();
            memset(da, 0xa5, sizeof da); memset(db, 0xa5, sizeof db);
            r7c(da, src); fs98_util_copy24(db, src);
            checked++;
            if (memcmp(da, db, sizeof da) && bad++ < 8)
                printf("copy24 #%u differs\n", i);
        }
    }

    /* slot 0xb0 -- double to int, in the x87's rounding mode */
    {
        fn_d2i rb0 = (fn_d2i)slot(h, 0xb0);
        static const double fixed[] = { 0.0, 0.5, 1.5, 2.5, -0.5, -1.5, -2.5,
                                        3.49, 3.51, -3.49, -3.51, 1e9, -1e9 };
        unsigned k;
        for (k = 0; k < sizeof fixed / sizeof fixed[0]; k++) {
            int32_t x = rb0(fixed[k]), y = fs98_util_double_to_int(fixed[k]);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i(%f): retail %d, openfs98 %d\n", fixed[k], x, y);
        }
        for (i = 0; i < 50000; i++) {
            double d = (double)(int32_t)rng() / (double)(1 + (rng() % 1000));
            int32_t x = rb0(d), y = fs98_util_double_to_int(d);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i(%f): retail %d, openfs98 %d\n", d, x, y);
        }
    }

    /* slots 0x74, 0x78, 0x80 -- the 24-byte three-component 48-bit vector */
    {
        fn_v48op r74 = (fn_v48op)slot(h, 0x74);
        fn_v48op r78 = (fn_v48op)slot(h, 0x78);
        fn_v48eq r80 = (fn_v48eq)slot(h, 0x80);
        for (i = 0; i < 120000; i++) {
            unsigned char a[24], b[24], da[24], db[24];
            unsigned k;
            int32_t ea, eb;
            for (k = 0; k < 24; k++) { a[k] = (unsigned char)rng(); b[k] = (unsigned char)rng(); }
            /* every so often make them equal, so 0x80's true branch is reached */
            if (i % 11 == 0) memcpy(b, a, 24);

            memset(da, 0x5a, 24); memset(db, 0x5a, 24);
            r74(da, a, b); fs98_util_v48_add(db, a, b);
            checked++;
            if (memcmp(da, db, 24) && bad++ < 8) printf("v48_add #%u differs\n", i);

            memset(da, 0x5a, 24); memset(db, 0x5a, 24);
            r78(da, a, b); fs98_util_v48_sub(db, a, b);
            checked++;
            if (memcmp(da, db, 24) && bad++ < 8) printf("v48_sub #%u differs\n", i);

            ea = r80(a, b); eb = fs98_util_v48_equal(a, b);
            checked++;
            if (ea != eb && bad++ < 8)
                printf("v48_equal #%u: retail %d, openfs98 %d\n", i, ea, eb);
        }
    }

    /* slot 0x84 -- 48-bit arithmetic shift.  Counts past 31 are included on
     * purpose: x86 masks them to five bits and the result is not what the
     * arithmetic would suggest. */
    {
        fn_shr48 r84 = (fn_shr48)slot(h, 0x84);
        for (i = 0; i < 120000; i++) {
            unsigned char src[8], da[8], db[8];
            uint32_t n = (i < 70) ? i : (rng() % 70);
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            memset(da, 0x5a, 8); memset(db, 0x5a, 8);
            r84(da, src, n); fs98_util_shr48(db, src, n);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("shr48 by %u differs\n", n);
        }
    }

    /* slot 0xb4 -- double to the low half of a 64-bit conversion */
    {
        fn_d2i rb4 = (fn_d2i)slot(h, 0xb4);
        static const double fixed[] = { 0.0, 0.5, 1.5, 2.5, -0.5, -1.5, -2.5,
                                        2147483647.0, 2147483648.0, -2147483649.0,
                                        1e12, -1e12 };
        unsigned k;
        for (k = 0; k < sizeof fixed / sizeof fixed[0]; k++) {
            int32_t x = rb4(fixed[k]), y = fs98_util_double_to_int64_low(fixed[k]);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i64(%f): retail %d, openfs98 %d\n", fixed[k], x, y);
        }
        for (i = 0; i < 50000; i++) {
            double d = (double)(int32_t)rng() * (double)(1 + (rng() % 1000));
            int32_t x = rb4(d), y = fs98_util_double_to_int64_low(d);
            checked++;
            if (x != y && bad++ < 8)
                printf("d2i64(%f): retail %d, openfs98 %d\n", d, x, y);
        }
    }

    /* slot 0x34 -- 2D rotation in Q15 */
    {
        fn_rot2d r34 = (fn_rot2d)slot(h, 0x34);
        for (i = 0; i < 200000; i++) {
            int32_t x = (int32_t)rng(), y = (int32_t)rng();
            int16_t sn = (int16_t)rng(), cs = (int16_t)rng();
            int32_t ax = 0, ay = 0, bx = 0, by = 0;
            if (i < 16) { x = (int32_t)i - 8; y = 8 - (int32_t)i; sn = 0; cs = 32767; }
            r34(x, y, sn, cs, &ax, &ay);
            fs98_util_rotate2d(x, y, sn, cs, &bx, &by);
            checked++;
            if ((ax != bx || ay != by) && bad++ < 8)
                printf("rot2d(%d,%d,s=%d,c=%d): retail %d,%d openfs98 %d,%d\n",
                       x, y, sn, cs, ax, ay, bx, by);
        }
    }

    /* slot 0x30 -- 48-bit divided by an unsigned 16-bit value.  Divisor is
     * never zero: retail faults on that exactly as a `div` must. */
    {
        fn_div48 r30 = (fn_div48)slot(h, 0x30);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[8], db[8];
            uint16_t d = (uint16_t)(1 + (rng() % 65535));
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20; d = 1 + (uint16_t)(i % 7); }
            memset(da, 0x3c, 8); memset(db, 0x3c, 8);
            r30(src, d, da); fs98_util_div48_u16(src, d, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("div48(%04x:%08x / %u): retail %04x:%08x openfs98 %04x:%08x\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), d,
                       *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
    }

    /* slot 0x40 -- a 32-bit step through the longitude scaling.  Same domain
     * limits as slot 0x44, which it calls. */
    {
        fn_m32lon r40 = (fn_m32lon)slot(h, 0x40);
        for (i = 0; i < 150000; i++) {
            unsigned char pos[8], ra[8], ob[8];
            int32_t v = (int32_t)(rng() & 0x00ffffffu) - 0x00800000;
            int32_t lat_hi = (int32_t)(rng() % 0x9c) - 0x4e;
            memset(pos, 0, 8);
            put48(pos, (uint16_t)rng(), lat_hi);
            memset(ra, 0xcd, 8); memset(ob, 0xcd, 8);
            r40(&v, pos, ra);
            fs98_util_metres32_to_longitude(&v, pos, ob);
            checked++;
            if (memcmp(ra, ob, 8) && bad++ < 8)
                printf("m32lon(%d, lat_hi=%d): retail %04x:%08x openfs98 %04x:%08x\n",
                       v, lat_hi,
                       *(uint16_t *)(ra + 2), *(uint32_t *)(ra + 4),
                       *(uint16_t *)(ob + 2), *(uint32_t *)(ob + 4));
        }
    }

    /* slot 0x24 -- 48-bit times 16-bit, into a full 64-bit result */
    {
        fn_mul48 r24 = (fn_mul48)slot(h, 0x24);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[8], db[8];
            int16_t m = (int16_t)rng();
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20;
                          m = (int16_t)((int)i - 20); }
            memset(da, 0x77, 8); memset(db, 0x77, 8);
            r24(src, m, da); fs98_util_mul48_s16(src, m, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("mul48(%04x:%08x * %d): retail %08x%08x openfs98 %08x%08x\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), m,
                       *(uint32_t *)(da + 4), *(uint32_t *)da,
                       *(uint32_t *)(db + 4), *(uint32_t *)db);
        }
    }

    /* slot 0x60 -- unpack an eight-byte record into the 24-byte vector */
    {
        fn_unpack r60 = (fn_unpack)slot(h, 0x60);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[24], db[24];
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 32) { memset(src, 0, 8); src[i % 8] = (unsigned char)(1u << (i / 8)); }
            memset(da, 0x11, 24); memset(db, 0x11, 24);
            r60(src, da); fs98_util_unpack_position(src, db);
            checked++;
            if (memcmp(da, db, 24) && bad++ < 8)
                printf("unpack #%u differs\n", i);
        }
    }

    /* slot 0x3c -- atan2.  Includes the axes, the diagonals and both zeros,
     * since the equal-magnitude short-circuit is what keeps a zero vector away
     * from the divide. */
    {
        fn_atan2 r3c = (fn_atan2)slot(h, 0x3c);
        static const int32_t edge[] = { 0, 1, -1, 2, -2, 1000, -1000,
                                        0x7fffffff, -0x7fffffff, 0x40000000 };
        unsigned a, b;
        for (a = 0; a < sizeof edge / sizeof edge[0]; a++)
            for (b = 0; b < sizeof edge / sizeof edge[0]; b++) {
                int32_t x = r3c(edge[a], edge[b]);
                int32_t y = fs98_util_atan2(edge[a], edge[b]);
                checked++;
                if (x != y && bad++ < 8)
                    printf("atan2(%d,%d): retail %d, openfs98 %d\n",
                           edge[a], edge[b], x, y);
            }
        for (i = 0; i < 300000; i++) {
            int32_t px = (int32_t)rng() >> (rng() % 20);
            int32_t py = (int32_t)rng() >> (rng() % 20);
            int32_t x, y;
            if (i % 5 == 0) py = px;            /* the equal-magnitude path */
            if (i % 11 == 0) py = -px;
            x = r3c(px, py); y = fs98_util_atan2(px, py);
            checked++;
            if (x != y && bad++ < 8)
                printf("atan2(%d,%d): retail %d, openfs98 %d\n", px, py, x, y);
        }
    }

    /* slot 0x38 -- angle from a sine.  The sine argument is swept exhaustively
     * over all 65536 values against both signs of cosine, so every one of the
     * three piecewise ranges and both range boundaries are covered. */
    {
        fn_asin r38 = (fn_asin)slot(h, 0x38);
        for (i = 0; i < 65536; i++) {
            int16_t sn = (int16_t)i;
            int16_t cs;
            for (cs = -1; cs <= 1; cs += 2) {
                int32_t x = r38(sn, cs), y = fs98_util_angle_from_sin(sn, cs);
                checked++;
                if (x != y && bad++ < 8)
                    printf("asin(%d, cos %d): retail %d, openfs98 %d\n",
                           sn, cs, x, y);
            }
        }
    }

    /* slots 0xc0 and 0xbc -- 32.16 fixed point to double and back */
    {
        fn_f48d rc0 = (fn_f48d)slot(h, 0xc0);
        fn_df48 rbc = (fn_df48)slot(h, 0xbc);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8], da[8], db[8];
            double x = 0, y = 0, v;
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20; }
            rc0(&x, src); fs98_util_fixed48_to_double(&y, src);
            checked++;
            if (memcmp(&x, &y, sizeof x) && bad++ < 8)
                printf("fixed48->double(%04x:%08x): retail %.17g openfs98 %.17g\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), x, y);

            /* and back, over a spread that includes negatives, fractions and
             * values past the 32-bit boundary where _ftol gives up */
            v = (double)(int32_t)rng() / (double)(1 + (rng() % 100000));
            if (i % 9 == 0) v = -v;
            if (i % 23 == 0) v = (double)(int32_t)rng() * 1000.0;
            memset(da, 0x2b, 8); memset(db, 0x2b, 8);
            rbc(&v, da); fs98_util_double_to_fixed48(&v, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("double->fixed48(%.17g): retail %04x:%08x openfs98 %04x:%08x\n",
                       v, *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
    }

    /* slot 0xb8 -- the floating-point generator.  Stateful, and its state is
     * the last eight bytes of retail's .data, so both are seeded alike first.
     * Zero is included deliberately: it is the initial state and a fixed
     * point, so an unseeded generator must return zero forever. */
    {
        fn_frand rb8 = (fn_frand)slot(h, 0xb8);
        double *retail_state = (double *)((unsigned char *)h + 0x57f8);
        static const double seeds[] = { 0.0, 0.5, 0.125, 0.9999, 1.0 / 3.0 };
        unsigned k;
        for (k = 0; k < sizeof seeds / sizeof seeds[0]; k++) {
            *retail_state = seeds[k];
            fs98_util_frand_seed(seeds[k]);
            for (i = 0; i < 40000; i++) {
                double x = rb8(), y = fs98_util_frand();
                checked++;
                if (memcmp(&x, &y, sizeof x) && bad++ < 8)
                    printf("frand seed %.17g step %u: retail %.17g openfs98 %.17g\n",
                           seeds[k], i, x, y);
            }
        }
    }

    /* slot 0x9c -- the longer of two strings, measured through the host.
     * Retail's ImportTable entry for module 1 is pointed at the stub host, so
     * both sides measure with the same strlen and any difference is theirs. */
    {
        fn_longer r9c = (fn_longer)slot(h, 0x9c);
        struct import { uint32_t id; void *link; } *im;
        static const char *s[] = { "", "a", "ab", "abc", "abcd", "hello world",
                                   "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" };
        unsigned x, y;

        memset(host_blob, 0, sizeof host_blob);
        *(uint32_t *)host_blob = 1;
        HOST_SLOT(0x2b8) = (void *)stub_strlen;
        HOST_SLOT(0x038) = (void *)stub_ticks;
        fs98_util_set_host(host_blob);
        for (im = (struct import *)GetProcAddress(h, "ImportTable"); im && im->id; im++)
            if (im->id == 1) im->link = host_blob;

        for (x = 0; x < sizeof s / sizeof s[0]; x++)
            for (y = 0; y < sizeof s / sizeof s[0]; y++) {
                const char *p = r9c(s[x], s[y]);
                const char *q = fs98_util_longer_string(s[x], s[y]);
                checked++;
                if (p != q && bad++ < 8)
                    printf("longer(%s,%s): retail %s, openfs98 %s\n",
                           s[x], s[y], p ? p : "(null)", q ? q : "(null)");
            }
        /* and the null-pointer case the host service goes out of its way to
         * tolerate */
        {
            const char *p = r9c(NULL, "abc");
            const char *q = fs98_util_longer_string(NULL, "abc");
            checked++;
            if (p != q && bad++ < 8) printf("longer(NULL,abc) differs\n");
        }
    }

    /* slot 0x04 -- init.  Not a function of its arguments, but the state it
     * leaves behind is, and that state is readable on both sides: retail keeps
     * its integer seed at .data+0x0f0 and its floating-point seed at
     * .data+0x7f8.  Driving both through the same controlled clock -- which
     * includes the zeros the retry loop exists to skip -- makes init testable
     * after all. */
    {
        fn_void r04 = (fn_void)slot(h, 0x04);
        uint32_t *retail_seed  = (uint32_t *)((unsigned char *)h + 0x50f0);
        double   *retail_fseed = (double   *)((unsigned char *)h + 0x57f8);
        unsigned start;

        for (start = 0; start < sizeof tick_seq / sizeof tick_seq[0]; start++) {
            uint32_t rs, os;
            double rf, of;

            tick_pos = start;
            *retail_seed = 0xdeadbeef; *retail_fseed = -1.0;
            r04();
            rs = *retail_seed; rf = *retail_fseed;

            tick_pos = start;
            fs98_util_rand_seed(0xdeadbeef); fs98_util_frand_seed(-1.0);
            fs98_util_init();
            os = fs98_util_rand_get(); of = fs98_util_frand_get();

            checked++;
            if (os != rs && bad++ < 8)
                printf("init from tick %u: integer seed retail %u, openfs98 %u\n",
                       start, rs, os);
            checked++;
            if (memcmp(&of, &rf, sizeof of) && bad++ < 8)
                printf("init from tick %u: float seed retail %.17g, openfs98 %.17g\n",
                       start, rf, of);
        }
    }

    /* slot 0xc4 -- the signed double to 32.16 conversion.  Negatives matter
     * most here, since the wrap into the positive range is the whole point. */
    {
        fn_df48 rc4 = (fn_df48)slot(h, 0xc4);
        static const double fixed[] = { 0.0, 1.5, -1.5, 0.25, -0.25, -0.0,
                                        1e6, -1e6, 32767.75, -32768.25 };
        unsigned k;
        for (k = 0; k < sizeof fixed / sizeof fixed[0]; k++) {
            unsigned char da[8], db[8];
            memset(da, 0x6e, 8); memset(db, 0x6e, 8);
            rc4(&fixed[k], da); fs98_util_double_to_fixed48_signed(&fixed[k], db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("to_fixed48s(%.17g): retail %04x:%08x openfs98 %04x:%08x\n",
                       fixed[k], *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
        for (i = 0; i < 200000; i++) {
            unsigned char da[8], db[8];
            double v = (double)(int32_t)rng() / (double)(1 + (rng() % 100000));
            if (i % 3 == 0) v = -v;
            memset(da, 0x6e, 8); memset(db, 0x6e, 8);
            rc4(&v, da); fs98_util_double_to_fixed48_signed(&v, db);
            checked++;
            if (memcmp(da, db, 8) && bad++ < 8)
                printf("to_fixed48s(%.17g): retail %04x:%08x openfs98 %04x:%08x\n",
                       v, *(uint16_t *)(da + 2), *(uint32_t *)(da + 4),
                       *(uint16_t *)(db + 2), *(uint32_t *)(db + 4));
        }
    }

    /* slot 0xc8 -- signed 32.16 to double.  Checked both on its own and as a
     * round trip with +0xc4, which is the pair that should be inverse. */
    {
        fn_f48d rc8 = (fn_f48d)slot(h, 0xc8);
        fn_df48 rc4 = (fn_df48)slot(h, 0xc4);
        for (i = 0; i < 200000; i++) {
            unsigned char src[8];
            double x = 0, y = 0;
            unsigned k;
            for (k = 0; k < 8; k++) src[k] = (unsigned char)rng();
            if (i < 40) { memset(src, 0, 8); *(int32_t *)(src + 4) = (int32_t)i - 20; }
            rc8(&x, src); fs98_util_fixed48_to_double_signed(&y, src);
            checked++;
            if (memcmp(&x, &y, sizeof x) && bad++ < 8)
                printf("fixed48s->double(%04x:%08x): retail %.17g openfs98 %.17g\n",
                       *(uint16_t *)(src + 2), *(uint32_t *)(src + 4), x, y);

            /* round trip: a double through +0xc4 and back through +0xc8 */
            if (i % 4 == 0) {
                unsigned char mid[8];
                double v = (double)(int32_t)rng() / 4096.0, back = 0;
                rc4(&v, mid); rc8(&back, mid);
                checked++;
                if (back != v && bad++ < 8)
                    printf("round trip %.17g -> %.17g\n", v, back);
            }
        }
    }

    /* slot 0x68 -- distance and bearing.  All sixteen flag combinations are
     * swept, since three separate bits change what the function computes, and
     * the precision-doubling branch is reached by including small deltas. */
    {
        typedef void (FS98_STDCALL *fn_rb)(const int32_t *, const int32_t *,
                                           int32_t, int32_t *, int16_t *);
        fn_rb r68 = (fn_rb)slot(h, 0x68);
        for (i = 0; i < 200000; i++) {
            int32_t A[3], B[3], da = 0, db = 0;
            int16_t ba = 0, bb = 0;
            int32_t fl = (int32_t)(i & 0xf);
            unsigned k;
            for (k = 0; k < 3; k++) {
                A[k] = (int32_t)rng() >> (rng() % 24);
                B[k] = (int32_t)rng() >> (rng() % 24);
            }
            if (i % 5 == 0) { B[0] = A[0]; B[2] = A[2]; }   /* the zero-delta path */
            if (i % 7 == 0) { B[0] = A[0] + 3; B[2] = A[2] - 5; } /* small: doubling */
            r68(A, B, fl, &da, &ba);
            fs98_util_range_and_bearing(A, B, fl, &db, &bb);
            checked++;
            if ((da != db || ba != bb) && bad++ < 8)
                printf("range_bearing flags %x: retail %d/%d openfs98 %d/%d\n",
                       fl, da, ba, db, bb);
        }
    }

    /* slot 0x48 -- longitude back to metres, the inverse of +0x44.  Same
     * latitude range limit, since it shares the table. */
    {
        fn_m2lon r48 = (fn_m2lon)slot(h, 0x48);
        for (i = 0; i < 300000; i++) {
            unsigned char in[8], pos[8], ra[8], ob[8];
            int32_t lat_hi = (int32_t)(rng() % 0x9c) - 0x4e;
            int32_t v_hi = (int32_t)(rng() & 0x01ffffffu) - 0x01000000;
            if (i % 6 == 0) v_hi = -v_hi;
            if (i < 40) v_hi = (int32_t)i - 20;
            memset(in, 0, 8); memset(pos, 0, 8);
            memset(ra, 0xa7, 8); memset(ob, 0xa7, 8);
            put48(in, (uint16_t)rng(), v_hi);
            put48(pos, (uint16_t)rng(), lat_hi);
            r48(in, pos, ra);
            fs98_util_longitude_to_metres(in, pos, ob);
            checked++;
            if (memcmp(ra, ob, 8) && bad++ < 8)
                printf("lon2m(%04x:%08x lat %d): retail %04x:%08x openfs98 %04x:%08x\n",
                       *(uint16_t *)(in + 2), v_hi, lat_hi,
                       *(uint16_t *)(ra + 2), *(uint32_t *)(ra + 4),
                       *(uint16_t *)(ob + 2), *(uint32_t *)(ob + 4));
        }
    }

    /* slot 0x4c -- three-angle rotation.  Includes the axis-aligned angles,
     * where the sine or cosine is exactly zero or full scale. */
    {
        typedef void (FS98_STDCALL *fn_rot3)(const int16_t *, const int16_t *, int32_t *);
        fn_rot3 r4c = (fn_rot3)slot(h, 0x4c);
        static const int16_t axis[] = { 0, 0x4000, (int16_t)0x8000, (int16_t)0xc000 };
        unsigned k;
        for (k = 0; k < 64; k++) {
            int16_t ang[3], v[3];
            int32_t a[3] = {0,0,0}, bb[3] = {0,0,0};
            ang[0] = axis[k & 3]; ang[1] = axis[(k >> 2) & 3]; ang[2] = axis[(k >> 4) & 3];
            v[0] = 1000; v[1] = -2000; v[2] = 3000;
            r4c(ang, v, a); fs98_util_rotate3d(ang, v, bb);
            checked++;
            if (memcmp(a, bb, sizeof a) && bad++ < 8)
                printf("rot3d axis %u: retail %d,%d,%d openfs98 %d,%d,%d\n",
                       k, a[0], a[1], a[2], bb[0], bb[1], bb[2]);
        }
        for (i = 0; i < 200000; i++) {
            int16_t ang[3], v[3];
            int32_t a[3] = {0,0,0}, bb[3] = {0,0,0};
            unsigned j;
            for (j = 0; j < 3; j++) { ang[j] = (int16_t)rng(); v[j] = (int16_t)rng(); }
            r4c(ang, v, a); fs98_util_rotate3d(ang, v, bb);
            checked++;
            if (memcmp(a, bb, sizeof a) && bad++ < 8)
                printf("rot3d: retail %d,%d,%d openfs98 %d,%d,%d\n",
                       a[0], a[1], a[2], bb[0], bb[1], bb[2]);
        }
    }

    /* slot 0x64 -- range and bearing on 48-bit positions.  All sixteen flag
     * combinations, since four bits change what it computes, and latitudes are
     * kept inside the table range because it calls +0x48. */
    {
        typedef void (FS98_STDCALL *fn_rb48)(const void *, const void *, int32_t,
                                             int32_t *, int16_t *, uint8_t *);
        fn_rb48 r64 = (fn_rb48)slot(h, 0x64);
        for (i = 0; i < 200000; i++) {
            unsigned char A[24], B[24];
            int32_t da = 0, db = 0; int16_t ba = 0, bb = 0;
            uint8_t sa = 0xee, sb = 0xee;
            int32_t fl = (int32_t)(i & 0xf);
            unsigned k;
            memset(A, 0, 24); memset(B, 0, 24);
            for (k = 0; k < 3; k++) {
                int32_t hi = (int32_t)(rng() % 0x9c) - 0x4e;
                put48(A + k * 8, (uint16_t)rng(), hi);
                put48(B + k * 8, (uint16_t)rng(),
                      hi + (int32_t)(rng() % 7) - 3);
            }
            if (i % 9 == 0) memcpy(B, A, 24);        /* the zero-difference path */
            r64(A, B, fl, &da, &ba, &sa);
            fs98_util_range_and_bearing48(A, B, fl, &db, &bb, &sb);
            checked++;
            if ((da != db || ba != bb || sa != sb) && bad++ < 8)
                printf("rb48 flags %x: retail %d/%d/%u openfs98 %d/%d/%u\n",
                       fl, da, ba, sa, db, bb, sb);
        }
    }

    /* slot 0x88 -- the marshalling engine.  Every one of the 32 type pairs is
     * exercised, plus repeat counts, a zero count, and the string copy, which
     * is the only handler whose length depends on the data. */
    {
        typedef void (FS98_STDCALL *fn_mar)(const void *, const void *, int32_t);
        fn_mar r88 = (fn_mar)slot(h, 0x88);
        unsigned st, dt;
        for (st = 0; st < 4; st++)
            for (dt = 0; dt < 8; dt++) {
                unsigned char srcbuf[64], da[128], db[128];
                uint32_t desc[8];
                unsigned k;
                for (k = 0; k < 64; k++) srcbuf[k] = (unsigned char)(rng() | 1);
                /* the string handler copies until a NUL, so the source needs
                 * one per repeat or it runs off the end */
                if (dt == 7) for (k = 5; k < 64; k += 6) srcbuf[k] = 0;
                memset(da, 0xcc, sizeof da); memset(db, 0xcc, sizeof db);

                desc[0] = 2; desc[1] = st; desc[2] = dt; desc[3] = 0;
                desc[4] = 1; desc[5] = st; desc[6] = dt; desc[7] = 0;

                *(unsigned char **)&desc[3] = da;
                *(unsigned char **)&desc[7] = da + 48;
                r88(srcbuf, desc, 2);
                *(unsigned char **)&desc[3] = db;
                *(unsigned char **)&desc[7] = db + 48;
                fs98_util_marshal(srcbuf, desc, 2);
                checked++;
                if (memcmp(da, db, sizeof da) && bad++ < 8)
                    printf("marshal src %u dst %u differs\n", st, dt);
            }
        {   /* a zero repeat count must touch nothing at all */
            unsigned char srcbuf[16], da[32], db[32];
            uint32_t desc[4];
            memset(srcbuf, 0x5a, sizeof srcbuf);
            memset(da, 0x11, sizeof da); memset(db, 0x11, sizeof db);
            desc[0] = 0; desc[1] = 0; desc[2] = 0;
            *(unsigned char **)&desc[3] = da;
            r88(srcbuf, desc, 1);
            *(unsigned char **)&desc[3] = db;
            fs98_util_marshal(srcbuf, desc, 1);
            checked++;
            if (memcmp(da, db, sizeof da) && bad++ < 8)
                printf("marshal with a zero count differs\n");
        }
    }

    /* slots 0x8c and 0x90 -- the data-exchange engine.  Random descriptor
     * tables over every kind and subtype retail survives, including the ones
     * that only work because the dispatch is one flat table: kinds 9..20,
     * and subtypes 0x1ffd..0x2000 that land on a kind handler.  The relocation
     * flag is 0, 1 or 2, since only exactly 1 relocates.  Compared: the whole
     * arena (descriptors, records, variables, strings), the return value, and
     * the sequence of callback and NUMERIC calls. */
    {
        typedef int32_t (FS98_STDCALL *fn_put)(void *, uint32_t, void *, int32_t);
        typedef void    (FS98_STDCALL *fn_get)(void *, uint32_t, void *);
        fn_put r8c = (fn_put)slot(h, 0x8c);
        fn_get r90 = (fn_get)slot(h, 0x90);
        struct import { uint32_t id; void *link; } *im;
        static unsigned char after[X_ARENA];
        static struct x_call rlog[64];
        unsigned v, k;

        memset(numeric_blob, 0, sizeof numeric_blob);
        *(uint32_t *)numeric_blob = 17;
        *(void **)(numeric_blob + 0x18) = (void *)stub_format;
        *(void **)(numeric_blob + 0x1c) = (void *)stub_parse;
        HOST_SLOT(0x2a8) = (void *)stub_strcpy;
        for (im = (struct import *)GetProcAddress(h, "ImportTable"); im && im->id; im++) {
            if (im->id == 1)  im->link = host_blob;
            if (im->id == 2)  im->link = x_arena;
            if (im->id == 17) im->link = numeric_blob;
        }
        fs98_util_set_host(host_blob);
        fs98_util_set_global(x_arena);
        fs98_util_set_numeric(numeric_blob);

        for (v = 0; v < 400000; v++) {
            int get = v & 1;
            uint32_t n = 1 + rng() % 8, i;
            uint32_t *d = (uint32_t *)(x_init + X_DESC);
            int32_t mode = (int32_t)(rng() % 3), rr = 0, oo = 0;
            unsigned rn;

            for (k = 0; k < X_ARENA; k++) x_init[k] = (unsigned char)rng();
            for (k = 0; k < 32; k++) {              /* the string pool */
                unsigned len = rng() % 13, c;
                for (c = 0; c < len; c++) x_init[X_POOL + k * 32 + c] = (unsigned char)('a' + rng() % 26);
                x_init[X_POOL + k * 32 + len] = 0;
            }
            /* strings the engine may copy out of a variable */
            for (k = 15; k < 256; k += 16) x_init[X_GLOBAL + k] = 0;
            for (k = 15; k < 1024; k += 16) x_init[X_ABS + k] = 0;

            for (i = 0; i < n; i++, d += 5) {
                uint32_t kind, sub, r = rng() % 100;
                uint32_t *rec = (uint32_t *)(x_init + X_RECS + i * 8);

                kind = (r < 85) ? rng() % 9 : 9 + rng() % 12;
                r = rng() % 100;
                if (kind <= 3) {                    /* only what retail survives */
                    if (kind == 0 && r < 25)
                        sub = (r & 1 ? X_BITS8_T : (r & 2 ? X_BITS16_T : X_BITS32_T))
                              + (rng() % 5 ? 1u << (rng() % 34 % 32) : rng() % 4);
                    else if (r < 35) sub = 0x2001;
                    else if (r < 50) sub = 0x1ffd + rng() % 4;
                    else             sub = 0x2002 + rng() % 12;
                } else if (kind == 5 && r < 25) {
                    sub = (r & 1 ? X_BITS8_T : X_BITS32_T) + (1u << (rng() % 32));
                } else if (kind >= 9) {             /* the subtype is not read */
                    sub = 0x2002 + rng() % 12;
                } else {
                    if (r < 15)      sub = 0x2001;
                    else if (r < 30) sub = 0x2000;
                    else if (r < 50) sub = 0x200e;
                    else if (r < 60) sub = 0x2002 + rng() % 12;
                    else             sub = 0x1000 + rng() % 0x60;
                }

                d[0] = kind;
                d[1] = i * 8;
                d[2] = sub;
                if (sub == 0x2001) {                /* a callback: never relocated */
                    d[3] = (uint32_t)(uintptr_t)stub_xcb;
                    d[4] = rng() % 2 ? 0 : 2;
                } else if (rng() % 2) {
                    d[3] = rng() % 224;             /* an offset into GLOBAL */
                    d[4] = 1;
                } else {
                    d[3] = (uint32_t)(uintptr_t)(x_arena + X_ABS + rng() % 992);
                    d[4] = rng() % 2 ? 0 : 2;
                }

                /* status: always a pool buffer, for kind 8's reads; value: a
                 * pool buffer where text may flow through it, else a number
                 * the flag handlers will test against 1 */
                rec[0] = (uint32_t)(uintptr_t)(x_arena + X_POOL + (i * 2) * 32);
                if (kind == 4 || sub == 0x1ffd)
                    rec[1] = (uint32_t)(uintptr_t)(x_arena + X_POOL + (i * 2 + 1) * 32);
                else
                    rec[1] = rng() % 4 ? rng() % 3 : rng();
            }

            fault_vector = v;
            fault_descs = (const uint32_t *)(x_init + X_DESC);
            memcpy(x_arena, x_init, X_ARENA); x_nlog = 0;
            fault_where = get ? "retail 0x90" : "retail 0x8c";
            if (get) r90(x_arena + X_DESC, n, x_arena + X_RECS);
            else     rr = r8c(x_arena + X_DESC, n, x_arena + X_RECS, mode);
            memcpy(after, x_arena, X_ARENA); memcpy(rlog, x_log, sizeof rlog); rn = x_nlog;

            memcpy(x_arena, x_init, X_ARENA); x_nlog = 0;
            fault_where = get ? "our 0x90" : "our 0x8c";
            if (get) fs98_util_exchange_get(x_arena + X_DESC, n, x_arena + X_RECS);
            else     oo = fs98_util_exchange_put(x_arena + X_DESC, n, x_arena + X_RECS, mode);

            checked++;
            if ((rr != oo || rn != x_nlog || memcmp(rlog, x_log, sizeof rlog)
                 || memcmp(after, x_arena, X_ARENA)) && bad++ < 8) {
                unsigned at = 0;
                while (at < X_ARENA && after[at] == x_arena[at]) at++;
                printf("exchange %s vector %u (n %u, mode %d): ret %d/%d, calls %u/%u, "
                       "first byte differing %u\n", get ? "0x90" : "0x8c", v, n, mode,
                       rr, oo, rn, x_nlog, at);
                for (i = 0; i < n; i++) {
                    uint32_t *dd = (uint32_t *)(x_init + X_DESC) + 5 * i;
                    printf("   desc %u: kind %u sub %#x var %#x reloc %u\n",
                           i, dd[0], dd[2], dd[3], dd[4]);
                }
            }
        }
    }

    /* slots 0xa4, 0xa8, 0xac -- the alerts.  Every argument shape the text
     * builder takes (ids, pointers, zeros after the first, long strings that
     * the 254-character load limit truncates, both separators), every answer
     * including the quit case, and an open/close pair each time.  Compared:
     * the whole transcript of host calls, block by block, and the text. */
    {
        typedef int32_t (FS98_STDCALL *fn_amodal)(uint32_t, uint32_t, uint32_t, uint32_t,
                                                  uint32_t, uint32_t, uint32_t, int32_t);
        typedef void (FS98_STDCALL *fn_aopen)(void **, uint32_t, uint32_t, uint32_t,
                                              uint32_t, uint32_t, uint32_t, int32_t);
        typedef void (FS98_STDCALL *fn_aclose)(void **);
        fn_amodal ra4 = (fn_amodal)slot(h, 0xa4);
        fn_aopen  ra8 = (fn_aopen) slot(h, 0xa8);
        fn_aclose rac = (fn_aclose)slot(h, 0xac);
        void *retail_default = (unsigned char *)h + 0x3470;
        static char pool[5 * 400], rlog[sizeof a_log];
        unsigned v;

        HOST_SLOT(0x20c) = (void *)a_alloc;
        HOST_SLOT(0x210) = (void *)a_realloc;
        HOST_SLOT(0x214) = (void *)a_free;
        HOST_SLOT(0x2a0) = (void *)a_loadstring;
        HOST_SLOT(0x2b0) = (void *)a_strcat;
        HOST_SLOT(0x10c) = (void *)a_dialog;
        HOST_SLOT(0x110) = (void *)a_dialog;
        HOST_SLOT(0x114) = (void *)a_tick;
        HOST_SLOT(0x024) = (void *)a_quit;

        for (v = 0; v < 100000; v++) {
            uint32_t p[5], flags = rng() & 0x1f, tag = rng();
            int32_t lines = (int32_t)(rng() % 3), rr, oo;
            void *robj = NULL, *oobj = NULL, *rh, *oh;
            unsigned k;
            int answers[] = { 0, 1, 2, 8, 8, 7, -1 };

            for (k = 0; k < 5; k++) p[k] = a_piece(pool, k);
            if (!p[0]) p[0] = 1 + rng() % 500;      /* never zero: see util_alert.c */
            a_answer = answers[rng() % 7];
            fault_vector = v;

            a_len = 0; a_nblk = 0; a_log[0] = 0;
            fault_where = "retail 0xa4";
            rr = ra4(flags, tag, p[0], p[1], p[2], p[3], p[4], lines);
            rh = a_handler_seen;
            fault_where = "retail 0xa8/0xac";
            ra8(&robj, tag, p[0], p[1], p[2], p[3], p[4], lines);
            if (!(rh == retail_default && a_handler_seen == retail_default)) rh = NULL;
            rac(&robj);
            memcpy(rlog, a_log, sizeof rlog);

            a_len = 0; a_nblk = 0; a_log[0] = 0;
            fault_where = "our 0xa4";
            oo = fs98_util_alert_modal(flags, tag, p[0], p[1], p[2], p[3], p[4], lines);
            oh = a_handler_seen;
            fault_where = "our 0xa8/0xac";
            fs98_util_alert_open(&oobj, tag, p[0], p[1], p[2], p[3], p[4], lines);
            if (!(oh != retail_default && a_handler_seen == oh)) oh = NULL;
            fs98_util_alert_close(&oobj);

            checked++;
            if ((rr != oo || strcmp(rlog, a_log) || !rh || !oh || robj || oobj)
                && bad++ < 8) {
                unsigned at = 0;
                while (rlog[at] && rlog[at] == a_log[at]) at++;
                printf("alert vector %u: ret %d/%d handlers %s/%s objs %p/%p\n"
                       "  retail ...%.200s\n  ours   ...%.200s\n", v, rr, oo,
                       rh ? "ok" : "BAD", oh ? "ok" : "BAD", robj, oobj,
                       rlog + (at > 40 ? at - 40 : 0), a_log + (at > 40 ? at - 40 : 0));
            }
        }
    }

    /* and that null outputs are tolerated the way retail tolerates them */
    r_sincos(1234, NULL, NULL);
    fs98_util_sincos(1234, NULL, NULL);

    printf("\n%u comparisons, %u mismatches\n", checked, bad);
    if (!bad) printf("IDENTICAL\n");
    return bad != 0;
}
