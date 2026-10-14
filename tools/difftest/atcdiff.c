/* Differential test: retail ATC.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven only through Linkage, the
 * way MAIN and CONTROLS drive them.  What ATC imports:
 *
 *   host      a stub: a bump allocator over a fixed arena (so heap pointers
 *             come out the same on both sides), LoadString, path join, strlen
 *   GLOBAL    one block, reseeded identically before each side runs
 *   CHAIN     retail's, behind logging wrappers; a frame is a dispatch of
 *             list 3, which is where ATC's clearance exchange runs
 *   NUMERIC   retail's, initialised against the stub host
 *   UTIL      retail's; ATC only calls its position unpacker
 *   WEATHERM  a stub that logs the position it is asked about and then
 *             re-rolls the GLOBAL weather fields ATC formats
 *   WINMSG    a stub that logs every message -- text, slot, duration, mode,
 *             and which buffer it came from -- and answers "is slot n still
 *             up?" from a scripted random stream
 *
 * ATIS.XLT is real: a set of translation files is written under xlt/ once,
 * and each scenario points LoadString at one of them, so the translator runs
 * against the real GetPrivateProfileSectionA.
 *
 * A scenario is a random script run once against each module.  Compared: the
 * transcript of every import call and return value, the GLOBAL block, and
 * ATC's whole .data image byte for byte -- both builds keep it at Linkage-0x40
 * with retail's layout, so the state machine, the patched templates and the
 * translator's buffers are all compared directly, not only through what they
 * cause.  Each module's image is restored to its as-loaded bytes before every
 * run.
 *
 * Build: make atcdiff     Run: make atcdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE  0x1a00      /* .data's virtual size */
#define GLOBAL_SIZE 0x1300

struct import { uint32_t id; void *link; };
struct atc_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t r0c, r10;
    void *dialogs;
    void (*takeoff)(void);
    void (*landing)(void);
    void (STDCALL *atis)(const uint8_t *record);
    void (*clear)(void);
    uint32_t (*busy)(void);
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

static char tlog[1 << 20];
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
    struct import *imports;
    struct atc_linkage *link;
    uint8_t *image;             /* Linkage - 0x40 */
    uint8_t pristine[IMAGE_SIZE];
};
static struct side *cur;

/* a pointer as "img+off" or "arena+off", so the two sides compare */
static uint8_t arena[1 << 20] __attribute__((aligned(16)));
static uint32_t arena_used;

static void put_ptr(const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    if (q >= cur->image && q < cur->image + 0x2000)
        put("img+%#x", (unsigned)(q - cur->image));
    else if (q >= arena && q < arena + sizeof arena)
        put("arena+%#x", (unsigned)(q - arena));
    else if (!p)
        put("null");
    else
        put("%p", p);
}

static void put_text(const char *s)
{
    unsigned n = 0;
    put("\"");
    for (; *s && n < 4000; s++, n++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 32 && c < 127 && c != '"' && c != '\\') put("%c", c);
        else put("\\x%02x", c);
    }
    put("\"");
}

/* ---- scripted randomness, consumed only inside import calls --------------------- */

static uint32_t srs;            /* reseeded per side-run */
static uint32_t srng(void) { srs ^= srs << 13; srs ^= srs >> 17; srs ^= srs << 5; return srs; }
static unsigned busy_bias;      /* in 8ths: chance a WINMSG slot reads as still up */

static uint8_t global_blob[GLOBAL_SIZE];

/* the weather fields ATC formats */
static void roll_weather(void)
{
    #define GW(o) (*(uint16_t *)(global_blob + (o)))
    GW(0xe8c) = (uint16_t)(srng() & (srng() & 1 ? 0x3fff : 0xffff));   /* temperature */
    GW(0xef0) = (uint16_t)(srng() % 4 == 0 ? srng() % 7 : srng() % 200); /* wind speed */
    GW(0xef2) = (uint16_t)srng();                                        /* wind direction */
    GW(0xec6) = (uint16_t)srng();                                        /* altimeter */
    GW(0xe8a) = (uint16_t)(srng() % 3 == 0 ? srng() % 0x1000 : srng());  /* visibility */
    GW(0xeaa) = (uint16_t)(srng() % 4 == 0);                             /* storms */
    /* two cloud layers: top, base, cover.  A base of 0xffff wraps the next
     * search to 0 and retail loops for ever unless the layer is overcast, so
     * bases stop short of it. */
    GW(0xe9a) = (uint16_t)(srng() % 3 == 0 ? 0 : srng());
    GW(0xe9c) = (uint16_t)(srng() % 4 == 0 ? 0 : srng() % 0xfffe);
    GW(0xe9e) = (uint16_t)(srng() % 3 == 0 ? 0xf000 + srng() % 0x1000 : srng());
    GW(0xea2) = (uint16_t)(srng() % 3 == 0 ? 0 : srng());
    GW(0xea4) = (uint16_t)(srng() % 4 == 0 ? 0 : srng() % 0xfffe);
    GW(0xea6) = (uint16_t)(srng() % 3 == 0 ? 0x9000 + srng() % 0x6000 : srng());
    global_blob[0x23b] = (uint8_t)(srng() % 3 == 0 ? srng() : srng() % 24);
    /* a layer base near the field, for the ceiling's " <10" boundary at 30 */
    if (srng() % 3 == 0) {
        uint16_t field = (uint16_t)(*(uint32_t *)(global_blob + 0x20) >> 8);
        GW(srng() % 2 ? 0xe9c : 0xea4) = (uint16_t)(field + 26 + srng() % 9);
    }
    #undef GW
}

/* ---- WINMSG and WEATHERM stubs ------------------------------------------------- */

static void STDCALL w_show(const char *text, uint32_t slot, uint32_t dur, uint32_t mode)
{
    put("\n  winmsg.show(");
    put_ptr(text);
    put(", slot %u, dur %u, mode %u) ", slot, dur, mode);
    put_text(text);
}

static uint32_t STDCALL w_clear(uint32_t slot)
{
    put(" clr%u", slot);
    return 0;
}

static uint32_t STDCALL w_active(uint32_t slot)
{
    uint32_t r = (srng() & 7) < busy_bias;
    put(" up%u=%u", slot, r);
    return r;
}

static void STDCALL wx_at(void *pos)
{
    unsigned i;
    put("\n  wx.at(");
    put_ptr(pos);
    put(":");
    for (i = 0; i < 24; i++) put("%02x", ((uint8_t *)pos)[i]);
    put(")");
    roll_weather();
}

static void wx_here(void) { put(" wx.here"); }

/* ---- the stub host ------------------------------------------------------------- */

static uint8_t fill_byte;
static char xlt_dir[MAX_PATH];

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t tag)
{
    uint8_t *p;
    size = (size + 15) & ~15u;
    if (arena_used + size > sizeof arena) {
        put(" alloc(%#x)=OUT", size);
        return -1;
    }
    p = arena + arena_used;
    arena_used += size;
    memset(p, fill_byte, size);
    put(" alloc(%d,%#x,tag %d)=", fl, size, tag);
    put_ptr(p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    void *old = *pp, *q = NULL;
    put(" realloc(");
    put_ptr(old);
    put(",%d,%#x)", fl, size);
    if (h_alloc(fl, size, &q, 0) != 0)
        return -1;
    if (old)
        memcpy(q, old, size);       /* arena: reading past an old block is harmless */
    *pp = q;
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

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    memmove(d, s, n);
    return 0;
}

static uint32_t STDCALL h_strlen(const char *s)
{
    uint32_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static int32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    int32_t n = 0;
    put(" loadstring(%#x)", id);
    if (id == 0x9052)
        for (; n + 1 < len && xlt_dir[n]; n++) buf[n] = xlt_dir[n];
    if (len > 0) buf[n] = 0;
    return n;
}

/* +0x184, enough of it: drive is always null here, and out may alias dir. */
static int32_t STDCALL h_path_join(char *out, const char *drive, const char *dir,
                                   const char *name)
{
    char tmp[MAX_PATH];
    size_t n;
    (void)drive;
    snprintf(tmp, sizeof tmp, "%s", dir ? dir : "");
    n = strlen(tmp);
    if (n && tmp[n - 1] != '\\')
        snprintf(tmp + n, sizeof tmp - n, "\\");
    n = strlen(tmp);
    snprintf(tmp + n, sizeof tmp - n, "%s", name ? name : "");
    strcpy(out, tmp);
    put(" join");
    return 0;
}

/* NUMERIC init's [MAIN] reads */
static int32_t STDCALL h_config(const char *section, const char *key, int32_t *value)
{
    (void)section;
    if (strcmp(key, "DISPLAY_FRACTIONAL_MINUTES") == 0) *value = 1;
    else if (strcmp(key, "FRACTIONAL_MINUTE_DIGITS") == 0) *value = 2;
    return *value;
}

static int32_t STDCALL h_numeric_loadstring(uint32_t id, char *buf, int32_t len)
{
    if (id == 0x9052) return h_loadstring(id, buf, len);
    snprintf(buf, (size_t)len, "N S E W");
    return (int32_t)strlen(buf);
}

static uint8_t host_blob[0x320];

/* ---- CHAIN, retail's behind wrappers --------------------------------------------- */

static struct chain_linkage *chain;
static uint8_t chain_blob[0x30];
static cb_fn seen_cb[4];
static unsigned nseen_cb;

static void put_cb(cb_fn cb)
{
    unsigned i;
    for (i = 0; i < nseen_cb && seen_cb[i] != cb; i++) ;
    if (i == nseen_cb && nseen_cb < 4) seen_cb[nseen_cb++] = cb;
    put("cb#%u", i);
}

static int32_t STDCALL c_add(uint32_t list, cb_fn cb)
{
    int32_t r = chain->add(list, cb);
    put(" chain.add(%u,", list); put_cb(cb); put(")=%d", r);
    return r;
}

static int32_t STDCALL c_remove(uint32_t list, cb_fn cb)
{
    int32_t r = chain->remove(list, cb);
    put(" chain.remove(%u,", list); put_cb(cb); put(")=%d", r);
    return r;
}

/* ---- ATIS.XLT fixtures ----------------------------------------------------------- */

#define NXLT 8

/* Keys are compared raw against the message upper-cased, so only an
 * upper-case key can ever match.  Most keys here are upper-case words and
 * prefixes from ATC's own templates, several to a section so that matches
 * come after mismatches; some are mixed case and must never match. */
static const char *xlt_words[] = {
    "WEATHER", "OBSERVATION", "ZULU", "TEMPERATURE", "INFORMATION", "LANDING",
    "LANDING AND", "AND", "DEPARTING", "RUNWAY", "ADVISE", "CONTROLLER",
    "ALTIMETER", "VISIBILITY", "MI", "KM", "WINDS", "WINDS CALM", "AT", "CALM",
    "LIGHT", "VARIABLE", "ON", "INITIAL", "CONTACT", "MICROSOFT",
    "MICROSOFT FLIGHT", "MICROSOFT FLIGHT SIMULATOR", "MICRO", "FLIGHT",
    "SIMULATOR", "REQUESTING", "CLEARANCE", "YOU", "YOU ARE", "ARE", "CLEARED",
    "CLEARED FOR", "THUNDERSTORMS", "OBSERVED", "IN", "VICINITY", "MEASURED",
    "CEILING", "BROKEN", "OVERCAST", "SCATTERED", "SKIES", "SKIES CLEAR",
    "CLEAR", "TAXI", "TO", "TO RUNWAY", "OF", "YOUR", "CHOICE", "HOLD",
    "SHORT", "SQUAWK", "FOR", "TAKEOFF", "TAK", "LAND", "DO", "A", "RIGHT",
    "TURN", "TRAFFIC", "SET", "TRANSPONDER", "CODE", "HAS", "ALREADY",
    "TAKEN-OFF", "LANDED", "ALPHA", "BRAVO", "Microsoft", "Winds", "taxi"
};

#define NXLT 8

static void write_xlt(unsigned k)
{
    static const char *locales[NXLT] = {
        "00000409", "00000409", "0000040c", "00000409",
        NULL,       "0000ffff", "00000409", "409"
    };
    char dir[MAX_PATH], path[MAX_PATH];
    FILE *f;
    unsigned s, i, w, seed = 0x9e3779b9u * (k + 1);
    const unsigned nwords = sizeof xlt_words / sizeof *xlt_words;

    snprintf(dir, sizeof dir, "xlt\\x%u", k);
    CreateDirectoryA("xlt", NULL);
    CreateDirectoryA(dir, NULL);
    snprintf(path, sizeof path, "%s\\ATIS.XLT", dir);
    f = fopen(path, "w");
    if (!f) { printf("cannot write %s\n", path); exit(2); }
    if (locales[k])
        fprintf(f, "[TRANSLATOR]\nLocaleID=%s\n\n", locales[k]);
    if (k == 3) { fclose(f); return; }      /* a locale, and no sections */
    for (s = 0; s < 26; s++) {
        unsigned n = 0, found[64];
        for (w = 0; w < nwords && n < 64; w++)
            if ((xlt_words[w][0] & 0xdf) == 'A' + (int)s)
                found[n++] = w;
        seed = seed * 1103515245u + 12345u;
        if (n == 0 && (seed >> 16) % 2) continue;
        fprintf(f, "[%c]\n", 'A' + s);
        /* filler first, sometimes, so a real key comes after mismatches */
        if ((seed >> 20) % 2)
            fprintf(f, "%cQX=filler\n%cZZ=filler\n", 'A' + s, 'A' + s);
        for (i = 0; i < n; i++) {
            /* shuffle a little, so a prefix may come before or after the
             * longer key it is a prefix of */
            unsigned j = i + (seed >> 8) % (n - i), t = found[i];
            seed = seed * 1103515245u + 12345u;
            found[i] = found[j]; found[j] = t;
            switch ((seed >> 4) % 11) {
            case 0:  fprintf(f, "%s\n", xlt_words[found[i]]); break;      /* no '=' */
            case 1:  fprintf(f, "=empty-key-%u\n", i); break;             /* ends the search */
            default: fprintf(f, "%s=<%c%u>\n", xlt_words[found[i]], 'a' + s, i); break;
            }
        }
        if (k == 6 && (s == 'M' - 'A' || s == 'T' - 'A'))
            for (i = 0; i < 150; i++)       /* past 0x400 and 0x800 */
                fprintf(f, "%cPAD%u=%c-padding-%u\n", 'A' + s, i, 'a' + s, i);
    }
    fclose(f);
}

/* ---- scenarios ------------------------------------------------------------------- */

static uint32_t rs = 0x6d2b79f5u;
static uint32_t rng(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

enum { OP_INIT, OP_DEINIT, OP_ATIS, OP_TAKEOFF, OP_LANDING, OP_TICK, OP_CLEAR,
       OP_BUSY, OP_POKE, OP_SQUAWK };
struct op { int kind; uint32_t a, b; uint8_t record[0x50]; };
#define MAXOPS 48
struct scenario {
    struct op op[MAXOPS];
    unsigned nops;
    uint8_t global[GLOBAL_SIZE];
    uint8_t fill;
    unsigned busy_bias, xlt;
    uint32_t seed;
};

/* GLOBAL fields ATC reads or writes, for OP_POKE */
static const struct { uint16_t off; uint8_t size; } pokes[] = {
    { 0x27c, 4 }, { 0x2b8, 4 }, { 0x366, 1 }, { 0xb04, 2 }, { 0xb68, 1 },
    { 0xb6a, 1 }, { 0x128c, 4 }, { 0x1290, 4 }, { 0x362, 2 }, { 0xc18, 2 },
    { 0x20, 4 }, { 0x2aa, 2 }, { 0x2ac, 4 }, { 0x354, 2 }
};

static uint32_t poke_value(unsigned which)
{
    switch (pokes[which].off) {
    case 0x27c: case 0x2b8: case 0x128c: case 0x1290:
        return rng() % 4 == 0 ? rng() : rng() % 2;
    case 0x366: return rng() % 2;
    case 0xb04: return rng() % 3 ? 8 | (rng() & 0xfff7) : rng();
    case 0xb68: case 0xb6a: return rng() % 4 == 0 ? rng() : rng() % 3;
    case 0x362: return rng() % 3 == 0 ? rng() : rng() % 8;
    case 0xc18: return rng() % 3;
    case 0x20:  return rng() % 3 == 0 ? rng() : rng() % 0x100000;
    default:    return rng();
    }
}

static void make_record(uint8_t *r)
{
    /* +0x03 packed position, +0x0c runway bytes, +0x10 template */
    unsigned i, n, len = 0;
    char *t = (char *)r + 0x10;
    static const char *words[] = {
        "Information ", "Alpha ", "taxi ", "runway ", "Winds ", "winds calm ",
        "Microsoft ", "cleared ", "mi ", "to", "Squawk", "zulu ", " ", "Bravo"
    };
    for (i = 0; i < 0x10; i++) r[i] = (uint8_t)rng();
    n = 1 + rng() % 10;
    for (i = 0; i < n && len < 0x38; i++) {
        if (rng() % 2) {
            t[len++] = (char)(0x80 + rng() % 0x15);         /* a macro */
        } else {
            const char *w = words[rng() % (sizeof words / sizeof *words)];
            while (*w && len < 0x3e) t[len++] = *w++;
        }
    }
    t[len] = 0;
}

static void make_scenario(struct scenario *s, unsigned v)
{
    unsigned i;
    memset(s, 0, sizeof *s);
    for (i = 0; i < GLOBAL_SIZE; i++) s->global[i] = (uint8_t)rng();
    /* a sane baseline that the pokes then disturb */
    *(uint32_t *)(s->global + 0x27c) = 0;
    *(uint32_t *)(s->global + 0x2b8) = 0;
    *(uint16_t *)(s->global + 0xb04) = 8;
    s->global[0xb68] = 0;
    s->global[0xb6a] = 0;
    *(uint32_t *)(s->global + 0x128c) = v % 3 == 0;
    *(uint16_t *)(s->global + 0x362) = (uint16_t)(rng() % 8);
    *(uint16_t *)(s->global + 0xc18) = (uint16_t)(rng() % 3);
    *(uint32_t *)(s->global + 0x20) = rng() % 0x100000;
    s->fill = v % 2 ? 0xab : 0x00;
    s->busy_bias = v % 5 == 0 ? 6 : 1 + rng() % 3;
    s->xlt = rng() % NXLT;
    s->seed = rng() | 1;

    /* Init once at the start and deinit at the end, as the host does.  In
     * between, both fault retail: deinit frees the translation sections and
     * a re-init with +0x128c clear zeroes them, and neither clears the
     * "loaded" flag, so the next translated message reads a null section. */
    s->op[s->nops++].kind = OP_INIT;
    while (s->nops < 12 + rng() % (MAXOPS - 13)) {
        struct op *o = &s->op[s->nops++];
        unsigned r = rng() % 40;
        if (r < 5)       { o->kind = OP_ATIS; make_record(o->record); }
        else if (r < 8)  o->kind = OP_TAKEOFF;
        else if (r < 10) o->kind = OP_LANDING;
        else if (r < 24) { o->kind = OP_TICK; o->a = 1 + rng() % 20; }
        else if (r < 26) o->kind = OP_CLEAR;
        else if (r < 28) o->kind = OP_BUSY;
        else if (r < 30) o->kind = OP_SQUAWK;
        else {
            o->kind = OP_POKE;
            o->a = rng() % (sizeof pokes / sizeof *pokes);
            o->b = poke_value(o->a);
        }
    }
    s->op[s->nops++].kind = OP_DEINIT;
}

/* +0x20 with EDI = GLOBAL, as MAIN calls it: retail reads GLOBAL through EDI */
static void call_atis(void *fn, const void *record, void *global)
{
    uint32_t a, c, d;
    __asm__ volatile("push %%edi\n\t"
                     "mov %%edx, %%edi\n\t"
                     "push %%ecx\n\t"
                     "call *%%eax\n\t"
                     "pop %%edi"
                     : "=a"(a), "=c"(c), "=d"(d)
                     : "a"(fn), "c"(record), "d"(global)
                     : "memory", "cc");
    (void)a; (void)c; (void)d;
}

static unsigned long cover_steps[3][16], cover_xlt, cover_shows[4], cover_grow,
                     cover_replaced, cover_low_ceiling;

static void run(struct side *sd, const struct scenario *s, int count)
{
    unsigned i, k;
    char rel[64];

    cur = sd;
    memcpy(sd->image, sd->pristine, IMAGE_SIZE);
    memcpy(global_blob, s->global, GLOBAL_SIZE);
    arena_used = 0;
    fill_byte = s->fill;
    srs = s->seed;
    busy_bias = s->busy_bias;
    nseen_cb = 0;
    tlen = 0;
    tlog[0] = 0;
    snprintf(rel, sizeof rel, "xlt\\x%u", s->xlt);
    GetFullPathNameA(rel, sizeof xlt_dir, xlt_dir, NULL);
    chain->init();

    for (i = 0; i < s->nops; i++) {
        const struct op *o = &s->op[i];
        switch (o->kind) {
        case OP_INIT:    put("\nINIT"); sd->link->init(); break;
        case OP_DEINIT:  put("\nDEINIT"); sd->link->deinit(); break;
        case OP_ATIS:
            put("\nATIS ");
            put_text((const char *)o->record + 0x10);
            call_atis((void *)sd->link->atis, o->record, global_blob);
            break;
        case OP_TAKEOFF: put("\nTAKEOFF"); sd->link->takeoff(); break;
        case OP_LANDING: put("\nLANDING"); sd->link->landing(); break;
        case OP_CLEAR:   put("\nCLEAR"); sd->link->clear(); break;
        case OP_BUSY:    put("\nBUSY"); put(" =%u", sd->link->busy()); break;
        case OP_SQUAWK: {
            /* dial in the squawk ATC will have picked from +0x2aa/+0x2ac */
            uint16_t ax = (uint16_t)(*(uint16_t *)(global_blob + 0x2ac) |
                                     *(uint16_t *)(global_blob + 0x2aa)) & 0x707;
            *(uint16_t *)(global_blob + 0x354) =
                (uint16_t)(0x0200 | ((ax & 7) << 4) | ((ax >> 8) & 7));
            put("\nSQUAWK");
            break;
        }
        case OP_POKE: {
            uint8_t *p = global_blob + pokes[o->a].off;
            if (pokes[o->a].size == 1) *p = (uint8_t)o->b;
            else if (pokes[o->a].size == 2) *(uint16_t *)p = (uint16_t)o->b;
            else *(uint32_t *)p = o->b;
            put("\nPOKE %#x=%#x", pokes[o->a].off, o->b);
            break;
        }
        case OP_TICK:
            for (k = 0; k < o->a; k++) {
                put("\nTICK");
                chain->dispatch(3, k, 0);
                if (count) {
                    uint32_t mode = *(uint32_t *)(sd->image + 0x496);
                    uint32_t step = *(uint32_t *)(sd->image + 0x492);
                    if (mode <= 2 && step < 16) cover_steps[mode][step]++;
                }
            }
            break;
        }
    }
    put("\nEND");
    if (count) {
        const char *p = tlog;
        while ((p = strstr(p, "winmsg.show(")) != NULL) {
            p += 12;
            if (strncmp(p, "img+0xa00", 9) == 0) cover_xlt++;
            if ((p = strstr(p, "slot ")) != NULL) cover_shows[(p[5] - '0') & 3]++;
            else break;
        }
        p = tlog;
        while ((p = strstr(p, ",0x800,tag 1)")) != NULL) { cover_grow++; p++; }
        for (p = tlog; (p = strchr(p, '<')) != NULL; p++) {
            if (p[1] >= 'a' && p[1] <= 'z' && p[2] >= '0' && p[2] <= '9') cover_replaced++;
            if (strncmp(p, "<10", 3) == 0) cover_low_ceiling++;
        }
    }
}

/* ---- loading ---------------------------------------------------------------------- */

static unsigned fault_scenario;
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    printf("\nFAULT %#lx at %p in %s, scenario %u\n", e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress, cur ? cur->name : "setup", fault_scenario);
    printf("transcript tail: ...%s\n", tlen > 600 ? tlog + tlen - 600 : tlog);
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static uint8_t winmsg_blob[0x30], weatherm_blob[0x30];
static void *numeric_link, *util_link;

static void bind_imports(struct import *im)
{
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host_blob; break;
        case 2:  im->link = global_blob; break;
        case 4:  im->link = chain_blob; break;
        case 17: im->link = numeric_link; break;
        case 20: im->link = util_link; break;
        case 22: im->link = winmsg_blob; break;
        case 27: im->link = weatherm_blob; break;
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
    sd->name = name;
    sd->dll = LoadLibraryA(path);
    if (!sd->dll) { printf("cannot load %s (%lu)\n", path, GetLastError()); return 0; }
    sd->imports = (struct import *)GetProcAddress(sd->dll, "ImportTable");
    sd->link = (struct atc_linkage *)GetProcAddress(sd->dll, "Linkage");
    if (!sd->imports || !sd->link || sd->link->id != 24) { printf("%s: not ATC\n", path); return 0; }
    sd->image = (uint8_t *)sd->link - 0x40;
    if ((uint8_t *)sd->imports != sd->image) { printf("%s: ImportTable is not at Linkage-0x40\n", path); return 0; }
    bind_imports(sd->imports);
    memcpy(sd->pristine, sd->image, IMAGE_SIZE);
    return 1;
}

static void dump_diff(const char *what, const uint8_t *r, const uint8_t *o, unsigned n)
{
    unsigned i, shown = 0;
    for (i = strcmp(what, "img") ? 0 : 0x70; i < n && shown < 8; i++)
        if (r[i] != o[i]) {
            printf("  %s+%#x: retail %02x ours %02x\n", what, i, r[i], o[i]);
            shown++;
        }
}

int main(int argc, char **argv)
{
    static char rlog[sizeof tlog];
    static uint8_t rimage[IMAGE_SIZE], rglobal[GLOBAL_SIZE];
    static struct side retail, ours;
    static struct scenario s;
    unsigned v, n = argc > 1 ? (unsigned)atoi(argv[1]) : 20000, bad = 0, k;
    struct import *im;
    void (*numeric_init)(void);

    SetUnhandledExceptionFilter(on_fault);
    for (k = 0; k < NXLT; k++) write_xlt(k);

    *(uint32_t *)host_blob = 1;
    *(void **)(host_blob + 0x0a8) = (void *)h_config;
    *(void **)(host_blob + 0x184) = (void *)h_path_join;
    *(void **)(host_blob + 0x20c) = (void *)h_alloc;
    *(void **)(host_blob + 0x210) = (void *)h_realloc;
    *(void **)(host_blob + 0x214) = (void *)h_free;
    *(void **)(host_blob + 0x21c) = (void *)h_memmove;
    *(void **)(host_blob + 0x2a0) = (void *)h_numeric_loadstring;
    *(void **)(host_blob + 0x2b8) = (void *)h_strlen;
    *(uint32_t *)winmsg_blob = 22;
    *(void **)(winmsg_blob + 0x18) = (void *)w_show;
    *(void **)(winmsg_blob + 0x1c) = (void *)w_clear;
    *(void **)(winmsg_blob + 0x24) = (void *)w_active;
    *(uint32_t *)weatherm_blob = 27;
    *(void **)(weatherm_blob + 0x18) = (void *)wx_at;
    *(void **)(weatherm_blob + 0x1c) = (void *)wx_here;

    chain = (struct chain_linkage *)load_support("CHAIN_retail.dll", 4);
    numeric_link = load_support("NUMERIC_retail.dll", 17);
    util_link = load_support("UTIL_retail.dll", 20);
    for (im = (struct import *)GetProcAddress(GetModuleHandleA("CHAIN_retail.dll"), "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : (void *)global_blob;
    for (im = (struct import *)GetProcAddress(GetModuleHandleA("NUMERIC_retail.dll"), "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : im->id == 2 ? (void *)global_blob : util_link;
    for (im = (struct import *)GetProcAddress(GetModuleHandleA("UTIL_retail.dll"), "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : im->id == 2 ? (void *)global_blob : numeric_link;
    numeric_init = *(void (**)(void))((uint8_t *)numeric_link + 4);
    numeric_init();
    *(uint32_t *)chain_blob = 4;
    *(void **)(chain_blob + 0x18) = (void *)c_add;
    *(void **)(chain_blob + 0x1c) = (void *)c_remove;
    /* retail ATC before ours, so it gets its preferred base and its tables'
     * absolute addresses are what the image says */
    if (!load_side(&retail, "ATC_retail.dll", "retail")) return 2;
    if (retail.image != (uint8_t *)0x20643000) {
        printf("retail ATC was relocated (.data at %p)\n", (void *)retail.image);
        return 2;
    }
    if (!load_side(&ours, "ATC_ours.dll", "openfs98")) return 2;

    if (memcmp(retail.pristine + 0x70, ours.pristine + 0x70, IMAGE_SIZE - 0x70)) {
        printf("initial images differ\n");
        dump_diff("img", retail.pristine, ours.pristine, IMAGE_SIZE);
        bad++;
    }

    for (v = 0; v < n; v++) {
        make_scenario(&s, v);
        fault_scenario = v;
        run(&retail, &s, 1);
        memcpy(rlog, tlog, tlen + 1);
        memcpy(rimage, retail.image, IMAGE_SIZE);
        memcpy(rglobal, global_blob, GLOBAL_SIZE);
        run(&ours, &s, 0);
        if (strcmp(rlog, tlog) || memcmp(rimage + 0x70, ours.image + 0x70, IMAGE_SIZE - 0x70)
            || memcmp(rglobal, global_blob, GLOBAL_SIZE)) {
            if (bad++ < 4) {
                unsigned at = 0;
                while (rlog[at] && rlog[at] == tlog[at]) at++;
                printf("scenario %u (xlt %u, fill %#x) differs", v, s.xlt, s.fill);
                if (rlog[at] || tlog[at])
                    printf(" in the transcript at byte %u:\n  retail ...%.300s\n  ours   ...%.300s\n",
                           at, rlog + (at > 120 ? at - 120 : 0), tlog + (at > 120 ? at - 120 : 0));
                else
                    printf(":\n");
                dump_diff("img", rimage, ours.image, IMAGE_SIZE);
                dump_diff("global", rglobal, global_blob, GLOBAL_SIZE);
            }
        }
    }

    printf("covered: shows by slot 1/2/3 = %lu/%lu/%lu, %lu translated with %lu replacements, "
           "%lu section regrows, %lu \" <10\" ceilings\n",
           cover_shows[1], cover_shows[2], cover_shows[3], cover_xlt, cover_replaced,
           cover_grow, cover_low_ceiling);
    printf("takeoff steps seen after a tick:");
    for (k = 0; k < 14; k++) printf(" %u:%lu", k, cover_steps[1][k]);
    printf("\nlanding steps seen after a tick:");
    for (k = 0; k < 6; k++) printf(" %u:%lu", k, cover_steps[2][k]);
    printf("\n%u scenarios, %u differ\n", n, bad);
    if (!bad) printf("IDENTICAL\n");
    return bad != 0;
}
