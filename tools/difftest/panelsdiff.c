/* Differential test: retail PANELS.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven only through their
 * Linkage and the callbacks they register with CHAIN, and given the same
 * stub world: HOST (38 services: the allocator, lists, surfaces, windows,
 * profile files, module loading), GLOBAL (a blob), CHAIN (the lists'
 * callbacks, kept for the harness to dispatch), ACONTAIN (an aircraft's
 * panel file), MAIN, SOUND, WINDOW, and the HG2D model of hg2d_model.inc.
 * What it loads are the CD's own gauges (the .GAU files of game/Gauges), whose
 * ImportTables are linked to the side under test: their code calls PANELS's
 * Linkage as it does in the game, from the elements' own data.
 *
 * Memory is a bump arena, reset before each side, so the two sides see the
 * same addresses for the same allocations; only module
 * images differ, and a pointer into PANELS's own image is compared as an
 * offset.  A scenario is a random script: an aircraft's panel loaded (by
 * the real panel.cfg and bitmaps), frames, resets, situation messages,
 * commands, window shows and hides, paints, mouse rectangles, named
 * variables, GLOBAL changes, other aircraft, deinit and init.  Compared:
 * the transcript of every call across the boundary, and after each step the
 * hashes of PANELS's .data, of every live block (the surfaces' pixels
 * included) and of each loaded gauge's writable data.
 *
 * Build: make panelsdiff     Run: make panelsdiff-run [N=...]
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <math.h>

#define STDCALL __attribute__((stdcall))

#ifndef GAME_DIR
#define GAME_DIR "..\\..\\game"
#endif

/* ---- the transcript ----------------------------------------------------------- */

static char *tlog;
static size_t tlen, tcap;
static int toverflow;
#define TLOG_MAX ((size_t)192 << 20)

static void put(const char *fmt, ...)
{
    va_list ap;
    char tmp[1024];
    int n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n >= (int)sizeof tmp)
        n = (int)sizeof tmp - 1;
    if (tlen + (size_t)n + 1 > tcap) {
        size_t want = tcap ? tcap * 2 : (1 << 20);

        while (want < tlen + (size_t)n + 1)
            want *= 2;
        if (want > TLOG_MAX) {
            toverflow = 1;
            return;
        }
        tlog = realloc(tlog, want);
        tcap = want;
    }
    memcpy(tlog + tlen, tmp, (size_t)n);
    tlen += (size_t)n;
    tlog[tlen] = 0;
}

/* ---- random numbers ------------------------------------------------------------ */

static uint32_t rstate = 1;

static uint32_t rnd(void)
{
    rstate ^= rstate << 13;
    rstate ^= rstate >> 17;
    rstate ^= rstate << 5;
    return rstate;
}

/* ---- the arena ------------------------------------------------------------------ */

#define ARENA_SIZE (256u << 20)
static uint32_t arena_base;

enum { K_HEAP, K_PIXELS };

struct blk {
    uint32_t off, size;
    uint8_t live, kind;
};

static uint8_t *arena;
static uint32_t arena_used;
static struct blk *blks;
static unsigned nblks;
#define MAXBLKS (1u << 20)

static unsigned nalloc_calls, fail_at;      /* fail_at counts flag-4 allocations only */
static int arena_full;

static void *arena_get(uint32_t size, int kind)
{
    uint8_t *p;

    arena_used = (arena_used + 15) & ~15u;
    if ((uint64_t)arena_used + size + 16 + 64 > ARENA_SIZE || nblks >= MAXBLKS) {
        arena_full = 1;
        return NULL;
    }
    p = arena + arena_used;
    *(uint32_t *)p = size;
    blks[nblks].off = arena_used + 16;
    blks[nblks].size = size;
    blks[nblks].live = 1;
    blks[nblks].kind = (uint8_t)kind;
    nblks++;
    arena_used += 16 + size + 64;      /* slack: retail overruns are not ours to find */
    return p + 16;
}

static void arena_reset(void)
{
    memset(arena, 0, arena_used + 64);
    arena_used = 0;
    nblks = 0;
    nalloc_calls = 0;
    arena_full = 0;
}

static struct blk *blk_of(const void *p)
{
    uint32_t off = (uint32_t)((const uint8_t *)p - arena), lo = 0, hi = nblks;

    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;

        if (blks[mid].off <= off)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo ? &blks[lo - 1] : NULL;
}

/* ---- images, and pointers into them --------------------------------------------- */

#define MAXREG 64
struct region {
    uint32_t lo, hi;
    int live;
};

static struct region regions[MAXREG];       /* 0 PANELS's .data, 1 its .text, 2.. gauges */
static unsigned nregions;

static int region_of(uint32_t v)
{
    unsigned i;

    for (i = 0; i < nregions; i++)
        if (regions[i].live && v >= regions[i].lo && v < regions[i].hi)
            return (int)i;
    return -1;
}

static uint8_t host_blob[0x320 + 16], global_blob[0x2000], chain_blob[0x40],
    acontain_blob[0x40], main_blob[0x100], sound_blob[0x100], window_blob[0x100];

static int dib_of(uint32_t v);

static void put_ptr(uint32_t v)
{
    int r;

    if (v >= arena_base && v < arena_base + ARENA_SIZE)
        put("arena+%x", v - arena_base);
    else if (dib_of(v) >= 0)
        put("dib%d", dib_of(v));
    else if ((r = region_of(v)) >= 0 && (r == 1 || r == 3))
        put("text");
    else if (r >= 0)
        put("mod%d+%x", r, v - regions[r].lo);
    else if (v >= (uint32_t)(uintptr_t)global_blob && v < (uint32_t)(uintptr_t)global_blob + sizeof global_blob)
        put("GLOBAL+%x", v - (uint32_t)(uintptr_t)global_blob);
    else
        put("%#x", v);
}

static int dib_of(uint32_t v);
static int norm_dibs = 1;

/* a word of memory as it compares: an address in a module image is its offset */
static uint32_t norm(uint32_t v)
{
    int r, d;
    uint32_t lo;

    if (v < 0x00100000u)
        return v;
    r = region_of(v);
    if ((v & 3) && r != 1 && r != 3)
        return v;           /* a pointer to a record is aligned; an odd pattern is not one */

    if (r < 0 && norm_dibs && (d = dib_of(v)) >= 0)
        return 0xd0000000u | (uint32_t)d;

    if (r < 0)
        return v;
    if (r == 1 || r == 3)
        return 0xe1000000u;         /* code: the builds' differ, and it is compared by what it does */
    lo = regions[r].lo;
    if (r == 2)
        r = 0;                      /* the other's data, laid out alike: a constant that equals one of its addresses */
    return 0xe0000000u | ((uint32_t)r << 24) | ((v - lo) & 0xffffff);
}

static uint64_t fnv(uint64_t h, uint32_t w)
{
    h ^= w;
    return h * 0x100000001b3ull;
}

static uint64_t hash_words(const uint8_t *p, uint32_t n, int normalise)
{
    uint64_t h = 0xcbf29ce484222325ull;
    uint32_t i;

    for (i = 0; i + 4 <= n; i += 4) {
        uint32_t w;

        memcpy(&w, p + i, 4);
        h = fnv(h, normalise ? norm(w) : w);
    }
    for (; i < n; i++)
        h = fnv(h, p[i]);
    return h;
}

/* ---- the HG2D model ------------------------------------------------------------- */

#include "hg2d_model.inc"

/* ---- faults, and the host's divide repair --------------------------------------- */

#include "divfix.h"

static const char *cur_name = "?";
static uint32_t cur_text_lo;
static void *fault_jb[5];
static int in_module, faulted;
static uint32_t saved_esp;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

/* PANELS_WATCH=<.data offset>[,<length>]: log each write into that range (by
 * protecting its pages and stepping the write) with the writer's place in
 * the module */
static uint32_t watch_addr, watch_len, watch_stepping, watch_at;
static void (*watch_report)(uint32_t eip, uint32_t at, uint32_t after);

/* PANELS_COVER=1: an int3 at the first byte of each of retail's functions,
 * each put back when first hit, so the run says which functions it reached */
static const uint32_t retail_funcs[] = {
#include "panels_funcs.inc"
};
static uint8_t cover_byte[sizeof retail_funcs / sizeof retail_funcs[0]];
static uint8_t cover_hit[sizeof retail_funcs / sizeof retail_funcs[0]];
static uint32_t cover_base;
static int cover_on;

/* PANELS_BREAK=<retail address>[,<args>]: log the arguments each time retail
 * enters that routine (an int3 at its first byte, stepped over) */
static uint32_t brk_addr, brk_args, brk_stepping;
static uint8_t brk_byte;
static void brk_log(const uint32_t *esp);

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;

    if (cover_on && code == EXCEPTION_BREAKPOINT) {
        uint32_t at = (uint32_t)(uintptr_t)e->ExceptionRecord->ExceptionAddress;
        unsigned i;

        for (i = 0; i < sizeof retail_funcs / sizeof retail_funcs[0]; i++)
            if (cover_base + retail_funcs[i] - 0x10000000u == at) {
                cover_hit[i] = 1;
                *(uint8_t *)(uintptr_t)at = cover_byte[i];
                e->ContextRecord->Eip = at;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
    }
    if (brk_addr) {
        CONTEXT *c = e->ContextRecord;

        if (code == EXCEPTION_BREAKPOINT && (uint32_t)(uintptr_t)e->ExceptionRecord->ExceptionAddress == brk_addr) {
            brk_log((const uint32_t *)(uintptr_t)c->Esp);
            *(uint8_t *)(uintptr_t)brk_addr = brk_byte;
            c->EFlags |= 0x100;
            brk_stepping = 1;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (code == EXCEPTION_SINGLE_STEP && brk_stepping) {
            *(uint8_t *)(uintptr_t)brk_addr = 0xcc;
            c->EFlags &= ~0x100u;
            brk_stepping = 0;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    if (watch_addr) {
        CONTEXT *c = e->ContextRecord;
        DWORD old;

        if (code == EXCEPTION_ACCESS_VIOLATION && e->ExceptionRecord->NumberParameters > 1 &&
            e->ExceptionRecord->ExceptionInformation[0] == 1) {
            uint32_t at = (uint32_t)e->ExceptionRecord->ExceptionInformation[1];

            if (at >= (watch_addr & ~0xfffu) && at < ((watch_addr + watch_len + 0xfff) & ~0xfffu)) {
                VirtualProtect((void *)(uintptr_t)(at & ~0xfffu), 0x1000, PAGE_READWRITE, &old);
                c->EFlags |= 0x100;
                watch_stepping = c->Eip;
                watch_at = at;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        if (code == EXCEPTION_SINGLE_STEP && watch_stepping) {
            if (watch_at >= watch_addr && watch_at < watch_addr + watch_len && watch_report)
                watch_report(watch_stepping, watch_at, *(uint32_t *)(uintptr_t)watch_at);
            VirtualProtect((void *)(uintptr_t)(watch_at & ~0xfffu), 0x1000, PAGE_READONLY, &old);
            c->EFlags &= ~0x100u;
            watch_stepping = 0;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    if (!in_module)
        return EXCEPTION_CONTINUE_SEARCH;
    if ((code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW) &&
        div_fixup(e->ContextRecord))
        return EXCEPTION_CONTINUE_EXECUTION;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_INT_OVERFLOW || code == EXCEPTION_STACK_OVERFLOW ||
        code == EXCEPTION_FLT_DIVIDE_BY_ZERO || code == EXCEPTION_FLT_INVALID_OPERATION) {
        /* where it faulted differs between the builds; that it did does not */
        put(" FAULT(%#lx)", (unsigned long)code);
        if (getenv("PANELS_VERBOSE")) {
            CONTEXT *c = e->ContextRecord;
            uint32_t at = (uint32_t)(uintptr_t)e->ExceptionRecord->ExceptionAddress;
            int r = region_of(at);

            printf("fault %#lx (%s) at %#x (region %d%+d, text %#x) target %#lx; eax %08lx ebx %08lx "
                   "ecx %08lx edx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx\n",
                   (unsigned long)code, cur_name, at, r, r >= 0 ? (int)(at - regions[r].lo) : 0, cur_text_lo,
                   e->ExceptionRecord->NumberParameters > 1 ? (unsigned long)e->ExceptionRecord->ExceptionInformation[1] : 0,
                   c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        }
        faulted = 1;
        e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
        e->ContextRecord->Esp = saved_esp - 64;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---- the scenario's world --------------------------------------------------------- */

#define MAXOPS 40

enum op_kind {
    OP_AIRCRAFT, OP_FRAMES, OP_RESET, OP_COMMAND, OP_SITUATION, OP_ENGINES, OP_SHOW,
    OP_HIDE, OP_SHOW_ALL, OP_HIDE_ALL, OP_TOGGLE, OP_OPTIONS, OP_ANY_SHOWN, OP_TOGGLE_ID,
    OP_PAINT, OP_MOUSE, OP_POST, OP_NAMED, OP_TWEAK, OP_REINIT, OP_UNLOAD, OP_NEWELEM, OP_N
};

struct op {
    uint8_t kind;
    uint32_t a, b, c, d;
};

struct scenario {
    uint32_t seed;
    int surf_type;              /* the host's window surfaces: 0..5 */
    int screen_w, screen_h;
    int image_quality;          /* [Panels] ImageQuality in FLTSIM98.CFG */
    uint8_t fill;               /* what an unzeroed allocation holds */
    unsigned fail_alloc;        /* the Nth allocation fails; 0 none */
    unsigned fail_surface;      /* the Nth surface is refused */
    unsigned fail_module;       /* the Nth gauge fails to load */
    unsigned fail_exists;       /* the Nth file test says no */
    uint32_t hg2d_seed;
    uint32_t palette_seed;
    uint8_t global[0x2000];
    uint32_t sim[16];
    int main_fail[8];
    int32_t main_val[8];
    uint32_t sound_state;
    int nops;
    struct op op[MAXOPS];
};

static const struct scenario *scen;

/* per-run counters the stubs consult */
static unsigned nsurfaces, nmodules, nexists, nenters, nmain_calls, nsound_calls;
static int32_t main_val[8];
static int main_fail[8];
static uint32_t sound_state;

#define GL(o) (*(uint32_t *)(global_blob + (o)))

static const uint32_t bytes_per_pixel[6] = { 1, 1, 2, 2, 3, 4 };

/* ---- HOST ------------------------------------------------------------------------- */

static int32_t STDCALL h_alloc(int32_t f, uint32_t n, void **out, int32_t z)
{
    uint8_t *p;

    if (f == 4)
        nalloc_calls++;
    put(" alloc(%d,%#x,%d)", f, n, z);
    /* The host's allocator ends the process on a failure unless the caller's
     * flag is 4 (HG2D's textures), when it returns -1 with *out cleared. */
    if (f == 4 && ((fail_at && nalloc_calls == fail_at) || n > 0x1000000)) {
        put("=FAIL");
        *out = NULL;
        return -1;
    }
    if (n > 0x1000000) {
        arena_full = 1;
        put("=HUGE");
        return -1;
    }
    p = arena_get(n, K_HEAP);
    if (!p) {
        put("=FULL");
        return -1;
    }
    memset(p, z ? 0 : scen->fill, n);
    *out = p;
    put("=");
    put_ptr((uint32_t)(uintptr_t)p);
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    put(" free(");
    put_ptr((uint32_t)(uintptr_t)*pp);
    put(")");
    if (*pp) {
        struct blk *b = blk_of(*pp);

        if (b && b->off == (uint32_t)((uint8_t *)*pp - arena))
            b->live = 0;
    }
    *pp = NULL;
    return 0;
}

static int32_t STDCALL h_memset(void *d, int32_t b, uint32_t n)
{
    put(" memset(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",%#x,%#x)", b & 0xff, n);
    memset(d, b, n);
    return 0;
}

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    put(" memmove(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    if (region_of((uint32_t)(uintptr_t)s) >= 0 ||
        ((uint32_t)(uintptr_t)s >= arena_base && (uint32_t)(uintptr_t)s < arena_base + ARENA_SIZE))
        put_ptr((uint32_t)(uintptr_t)s);
    else
        put("local:%016llx", (unsigned long long)hash_words(s, n < 0x1000 ? n : 0x1000, 1));
    put(",%#x)", n);
    memmove(d, s, n);
    return 0;
}

static int32_t STDCALL h_strcpy(char *d, const char *s)
{
    put(" strcpy(\"%s\")", s);
    strcpy(d, s);
    return 0;
}

static uint32_t STDCALL h_strdup(const char *s)
{
    char *p = arena_get((uint32_t)strlen(s) + 1, K_HEAP);

    put(" strdup(\"%s\")", s);
    if (!p)
        return 0;
    strcpy(p, s);
    return (uint32_t)(uintptr_t)p;
}

static uint32_t STDCALL h_strlen(const char *s)
{
    return (uint32_t)strlen(s);
}

static int32_t STDCALL h_strcmp(const char *a, const char *b)
{
    return strcmp(a, b);
}

static int32_t STDCALL h_post(uint32_t a, uint32_t b)
{
    put(" post(%#x,%#x)", a, b);
    return 0;
}

static int32_t STDCALL h_busy(int32_t on)
{
    put(" busy(%d)", on);
    return 0;
}

static int32_t STDCALL h_window_update(uint32_t w)
{
    put(" wupdate(");
    put_ptr(w);
    put(",flags %#x)", *(uint32_t *)(uintptr_t)(w + 8));
    return 0;
}

static int32_t STDCALL h_window_invalidate(uint32_t w, uint32_t r)
{
    put(" winvalidate(");
    put_ptr(w);
    put(",");
    if (r)
        put("%d,%d,%d,%d)", ((int32_t *)(uintptr_t)r)[0], ((int32_t *)(uintptr_t)r)[1],
            ((int32_t *)(uintptr_t)r)[2], ((int32_t *)(uintptr_t)r)[3]);
    else
        put("null)");
    return 0;
}

static int32_t STDCALL h_cfg_get(const char *s, const char *k, int32_t *p)
{
    put(" cfgget(%s,%s)", s, k);
    *p = scen->image_quality;
    return 0;
}

static int32_t STDCALL h_cfg_put(const char *s, const char *k, int32_t v)
{
    put(" cfgput(%s,%s,%d)", s, k, v);
    return 0;
}

static int32_t STDCALL h_profile_string(const char *sect, const char *key, char *buf, uint32_t n,
                                        const char *path, uint32_t ext)
{
    DWORD r;

    (void)ext;
    if (!sect || !key || n == 0)
        return 0;
    r = GetPrivateProfileStringA(sect, key, "", buf, n, path);
    put(" profile(%s,%s)=\"%s\"", sect, key, r ? buf : "");
    return r ? (int32_t)r : 0;
}

static int32_t STDCALL h_profile_int(const char *sect, const char *key, int32_t *p, const char *path,
                                     uint32_t ext)
{
    char tmp[64];

    (void)ext;
    if (GetPrivateProfileStringA(sect, key, "", tmp, sizeof tmp, path))
        *p = atoi(tmp);
    put(" profileint(%s,%s)=%d", sect, key, *p);
    return 0;
}

static int32_t STDCALL h_profile_num(const char *sect, const char *key, double *p, const char *path,
                                     uint32_t ext)
{
    char tmp[64];

    (void)ext;
    if (GetPrivateProfileStringA(sect, key, "", tmp, sizeof tmp, path))
        *p = atof(tmp);
    put(" profilenum(%s,%s)=%.17g", sect, key, *p);
    return 0;
}

static int32_t STDCALL h_exists(const char *path, uint32_t flags)
{
    char full[600];
    DWORD a;

    nexists++;
    if (scen->fail_exists && nexists == scen->fail_exists) {
        put(" exists(...)=-1");
        return -1;
    }
    snprintf(full, sizeof full, "%s%s", path, flags == 0x10 ? ".GAU" : "");
    a = GetFileAttributesA(full);
    put(" exists(%s)=%d", full, a == INVALID_FILE_ATTRIBUTES ? -1 : 0);
    return a == INVALID_FILE_ATTRIBUTES ? -1 : 0;
}

static int32_t STDCALL h_path_join(char *out, uint32_t drive, const char *dir, const char *name)
{
    (void)drive;
    if (dir && *dir)
        snprintf(out, 0x104, "%s\\%s", dir, name ? name : "");
    else
        snprintf(out, 0x104, "%s", name ? name : "");
    put(" pathjoin=%s", out);
    return 0;
}

static int32_t STDCALL h_load_string(uint32_t id, char *buf, uint32_t n)
{
    if (id == 0x9053)
        snprintf(buf, n, "%s\\Gauges", GAME_DIR);
    else
        snprintf(buf, n, "string%x", id);
    put(" loadstring(%#x)", id);
    return 0;
}

static int32_t STDCALL h_palette_copy(uint8_t *dst)
{
    uint32_t s = scen->palette_seed | 1, i;

    for (i = 0; i < 768; i++) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        dst[i] = (uint8_t)s;
    }
    put(" palette");
    return 0;
}

/* surface records: { type, w, h, row bytes, row table, size, bits } */
static uint32_t surface_make(uint32_t rec, int32_t type, int32_t w, int32_t h)
{
    uint32_t rowbytes, *r = (uint32_t *)(uintptr_t)rec, i;
    uint8_t *bits;
    uint32_t *rows;

    if (type < 0 || type > 5 || w < 0 || h < 0 || w > 4096 || h > 4096)
        return (uint32_t)-1;
    rowbytes = (bytes_per_pixel[type] * (uint32_t)w + 3) & ~3u;
    bits = arena_get(rowbytes * (uint32_t)h, K_PIXELS);
    rows = arena_get(4 * (uint32_t)h, K_HEAP);
    if (!bits || !rows)
        return (uint32_t)-1;
    r[0] = (uint32_t)type;
    r[1] = (uint32_t)w;
    r[2] = (uint32_t)h;
    r[3] = rowbytes;
    r[4] = (uint32_t)(uintptr_t)rows;
    r[5] = rowbytes * (uint32_t)h;
    r[6] = (uint32_t)(uintptr_t)bits;
    for (i = 0; i < (uint32_t)h; i++)
        rows[i] = (uint32_t)(uintptr_t)(bits + i * rowbytes);
    return 0;
}

static void kill_block(uint32_t p)
{
    struct blk *b;

    if (p < arena_base || p >= arena_base + ARENA_SIZE)
        return;
    b = blk_of((void *)(uintptr_t)p);
    if (b && b->off == p - arena_base)
        b->live = 0;
}

static int32_t STDCALL h_surface_new(uint32_t *out, int32_t w, int32_t h, int32_t type)
{
    uint8_t *rec;

    nsurfaces++;
    put(" surfnew(%d,%d,%d)", w, h, type);
    if (type < 0 || type > 5 || w < 0 || h < 0 || w > 4096 || h > 4096) {
        put("=FAIL");           /* the host's check of the type; *out is not touched */
        return -1;
    }
    rec = arena_get(0x20, K_HEAP);
    if (!rec || surface_make((uint32_t)(uintptr_t)rec, type, w, h) != 0) {
        put("=FULL");
        arena_full = 1;
        return -1;
    }
    *out = (uint32_t)(uintptr_t)rec;
    put("=");
    put_ptr(*out);
    return 0;
}

static int32_t STDCALL h_surface_free(uint32_t s)
{
    uint32_t *r = (uint32_t *)(uintptr_t)s;

    put(" surffree(");
    put_ptr(s);
    put(")");
    if (s >= arena_base && s < arena_base + ARENA_SIZE) {
        kill_block(r[6]);
        kill_block(r[4]);
        kill_block(s);
    }
    return 0;
}

static int32_t STDCALL h_surface_resize(uint32_t s, int32_t w, int32_t h)
{
    uint32_t *r = (uint32_t *)(uintptr_t)s;

    put(" surfresize(");
    put_ptr(s);
    put(",%d,%d)", w, h);
    kill_block(r[6]);
    kill_block(r[4]);
    return (int32_t)surface_make(s, (int32_t)r[0], w, h);
}

/* the windows: WINDOW's records of 0xa4 bytes, listed here */
#define MAXWIN 64
static uint32_t wins[MAXWIN];
static unsigned nwins;

static int32_t STDCALL h_window_create(uint32_t w)
{
    uint32_t *r = (uint32_t *)(uintptr_t)w;
    int32_t pw = (int32_t)(((int64_t)(int32_t)r[0x28 / 4] * scen->screen_w) >> 13),
            ph = (int32_t)(((int64_t)(int32_t)r[0x2c / 4] * scen->screen_h) / 0x1800);

    if (pw < 1) pw = 1;
    if (pw > 1600) pw = 1600;
    if (ph < 1) ph = 1;
    if (ph > 1200) ph = 1200;
    put(" wcreate(");
    put_ptr(w);
    put(",%d,%d)", pw, ph);
    r[0x60 / 4] = (uint32_t)pw;
    r[0x64 / 4] = (uint32_t)ph;
    surface_make(w + 0x80, scen->surf_type, pw, ph);
    r[0xa0 / 4] = (uint32_t)(uintptr_t)arena_get(0x90, K_HEAP);
    return 0;
}

static int32_t STDCALL h_window_destroy(uint32_t w)
{
    uint32_t *r = (uint32_t *)(uintptr_t)w;
    unsigned i;

    put(" wdestroy(");
    put_ptr(w);
    put(")");
    kill_block(r[0x98 / 4]);
    kill_block(r[0x90 / 4]);
    kill_block(r[0xa0 / 4]);
    for (i = 0; i < nwins; i++)
        if (wins[i] == w) {
            memmove(wins + i, wins + i + 1, (nwins - i - 1) * sizeof wins[0]);
            nwins--;
            break;
        }
    kill_block(w);
    return 0;
}

static int32_t STDCALL h_child_add(uint32_t w, uint32_t m, int32_t x, int32_t y)
{
    put(" mouseadd(");
    put_ptr(w);
    put(",");
    put_ptr(m);
    put(",%d,%d)", x, y);
    return 0;
}

static int32_t STDCALL h_child_remove(uint32_t w, uint32_t m)
{
    put(" mouseremove(");
    put_ptr(w);
    put(",");
    put_ptr(m);
    put(")");
    return 0;
}

/* the host's growable lists */
struct host_list {
    int32_t count, capacity, grow, elem_size, iterating, pending;
    uint8_t *data;
};

static struct { uint32_t list; int32_t idx; } deferred[256];
static unsigned ndeferred;

static void list_remove_at(struct host_list *l, int32_t i)
{
    int32_t last = l->count - 1;

    if (i < last)
        memmove(l->data + (size_t)i * l->elem_size, l->data + (size_t)(i + 1) * l->elem_size,
                (size_t)(last - i) * l->elem_size);
    l->count = last;
}

static uint32_t STDCALL h_list_add(struct host_list *l)
{
    uint8_t *e;

    if (l->count == l->capacity) {
        int32_t cap = l->capacity + l->grow;
        uint8_t *d;

        if (cap <= 0 || (uint64_t)cap * l->elem_size > 0x1000000) {
            arena_full = 1;
            put(" listadd(HUGE)");
            return 0;
        }
        d = arena_get((uint32_t)(cap * l->elem_size), K_HEAP);
        if (!d)
            return 0;
        memset(d, l->count ? 0 : scen->fill, (size_t)cap * l->elem_size);
        if (l->data)
            memcpy(d, l->data, (size_t)l->count * l->elem_size);
        kill_block((uint32_t)(uintptr_t)l->data);
        l->data = d;
        l->capacity = cap;
    }
    e = l->data + (size_t)l->count * l->elem_size;
    memset(e, 0, (size_t)l->elem_size);
    l->count++;
    put(" listadd(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(")=");
    put_ptr((uint32_t)(uintptr_t)e);
    return (uint32_t)(uintptr_t)e;
}

static int32_t STDCALL h_list_free(struct host_list *l)
{
    put(" listfree(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(")");
    if (l->data) {
        kill_block((uint32_t)(uintptr_t)l->data);
        l->data = NULL;
    }
    return 0;
}

static int32_t STDCALL h_list_remove(struct host_list *l, uint8_t *elem)
{
    uint32_t i = (uint32_t)(elem - l->data) / (uint32_t)l->elem_size;

    put(" listremove(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(",%u)", i);
    if (l->iterating) {
        l->pending = 1;
        if (ndeferred < 256) {
            deferred[ndeferred].list = (uint32_t)(uintptr_t)l;
            deferred[ndeferred].idx = (int32_t)i;
            ndeferred++;
        }
        return 0;
    }
    list_remove_at(l, (int32_t)i);
    return 0;
}

static int32_t STDCALL h_list_reset(struct host_list *l)
{
    put(" listreset(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(")");
    l->count = 0;
    return 0;
}

typedef uint32_t (STDCALL *walk_fn)(uint32_t, uint32_t);

static uint32_t STDCALL h_list_walk(struct host_list *l, uint32_t ctx, walk_fn fn)
{
    uint32_t r = 0;
    int32_t i;

    put(" listwalk(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(",");
    put_ptr(ctx);
    put(")");
    l->iterating++;
    for (i = 0; i < l->count; i++) {
        r = fn((uint32_t)(uintptr_t)(l->data + (size_t)i * l->elem_size), ctx);
        if (r != 0)
            break;
    }
    l->iterating--;
    if (l->pending) {
        unsigned k;

        for (k = ndeferred; k > 0; k--)
            if (deferred[k - 1].list == (uint32_t)(uintptr_t)l) {
                list_remove_at(l, deferred[k - 1].idx);
                memmove(deferred + k - 1, deferred + k, (ndeferred - k) * sizeof deferred[0]);
                ndeferred--;
            }
        l->pending = 0;
    }
    return r;
}

static uint32_t STDCALL h_list_find(struct host_list *l, uint32_t off, const uint8_t *key, uint32_t n,
                                    uint32_t indirect)
{
    int32_t i;

    put(" listfind(");
    put_ptr((uint32_t)(uintptr_t)l);
    put(",%#x)", off);
    for (i = 0; i < l->count; i++) {
        const uint8_t *e = l->data + (size_t)i * l->elem_size + off;
        const uint8_t *a = indirect ? *(const uint8_t *const *)e : e;
        const uint8_t *b = indirect ? *(const uint8_t *const *)key : key;

        if (memcmp(a, b, n) == 0)
            return (uint32_t)(uintptr_t)(l->data + (size_t)i * l->elem_size);
    }
    return 0;
}

/* ---- GDI, virtualised --------------------------------------------------------------- */

/* PANELS draws digits with GDI into DIB sections and keeps the handles in its
 * elements.  The handles' values and the DIBs' addresses differ from run to run,
 * so PANELS's imports are hooked: a handle is a fake one by its order, the
 * calls are made on the real objects and logged, and a DIB's bits are a
 * region compared by offset.  PANELS's IAT is patched in each side. */
#define MAXGDI 4096
static HGDIOBJ gdi_real[MAXGDI];
static unsigned ngdi;
#define FAKE(i) (0xdd000000u | (i))

static HGDIOBJ real_of(uint32_t fake)
{
    if ((fake & 0xff000000u) == 0xdd000000u && (fake & 0xffffff) < ngdi)
        return gdi_real[fake & 0xffffff];
    return (HGDIOBJ)(uintptr_t)fake;
}

static uint32_t fake_of(HGDIOBJ real)
{
    unsigned i;

    if (!real)
        return 0;
    for (i = 0; i < ngdi; i++)
        if (gdi_real[i] == real)
            return FAKE(i);
    if (ngdi >= MAXGDI)
        return (uint32_t)(uintptr_t)real;
    gdi_real[ngdi] = real;
    return FAKE(ngdi++);
}

struct dibreg {
    uint32_t lo, hi;
    uint32_t handle;            /* the fake handle of its bitmap */
    int live;
};
#define MAXDIB 8192
static struct dibreg dibs[MAXDIB];
static unsigned ndibs;
static uint16_t *dib_page;          /* a 4K page of the address space -> its DIB + 1 */

/* a DIB's bits, if v could be a pointer into them (the rows are dword-aligned,
 * and an odd byte pattern such as a colour cache's 0xf5f5f5f5 is not one) */
static int dib_of(uint32_t v)
{
    unsigned k = (v & 3) ? 0 : dib_page[v >> 12];

    return k ? (int)k - 1 : -1;
}

static HBITMAP WINAPI hk_CreateDIBSection(HDC dc, const BITMAPINFO *bmi, UINT use, VOID **bits,
                                          HANDLE sec, DWORD off)
{
    void *b = NULL;
    HBITMAP r = CreateDIBSection(dc, bmi, use, &b, sec, off);
    int32_t w = bmi->bmiHeader.biWidth, h = bmi->bmiHeader.biHeight < 0 ? -bmi->bmiHeader.biHeight : bmi->bmiHeader.biHeight;
    uint32_t stride = (((uint32_t)w * bmi->bmiHeader.biBitCount + 31) / 32) * 4;

    put(" CreateDIBSection(%d,%d,%d)", w, bmi->bmiHeader.biHeight, bmi->bmiHeader.biBitCount);
    if (bits)
        *bits = b;
    if (r && b && ndibs < MAXDIB) {
        uint32_t pg;

        dibs[ndibs].lo = (uint32_t)(uintptr_t)b;
        dibs[ndibs].hi = dibs[ndibs].lo + stride * (uint32_t)h;
        dibs[ndibs].handle = fake_of(r);
        dibs[ndibs].live = 1;
        for (pg = dibs[ndibs].lo >> 12; pg <= (dibs[ndibs].hi - 1) >> 12; pg++)
            dib_page[pg] = (uint16_t)(ndibs + 1);
        ndibs++;
    }
    return (HBITMAP)(uintptr_t)(r ? fake_of(r) : 0);
}

static HDC WINAPI hk_CreateCompatibleDC(HDC dc)
{
    HDC r = CreateCompatibleDC((HDC)real_of((uint32_t)(uintptr_t)dc));

    put(" CreateCompatibleDC");
    return (HDC)(uintptr_t)(r ? fake_of(r) : 0);
}

static HBRUSH WINAPI hk_CreateSolidBrush(COLORREF c)
{
    HBRUSH r = CreateSolidBrush(c);

    put(" CreateSolidBrush(%#lx)", (unsigned long)c);
    return (HBRUSH)(uintptr_t)(r ? fake_of(r) : 0);
}

static HFONT WINAPI hk_CreateFontA(int h, int w, int esc, int ori, int weight, DWORD i, DWORD u, DWORD so,
                                   DWORD cs, DWORD op, DWORD cp, DWORD q, DWORD pf, LPCSTR face)
{
    HFONT r = CreateFontA(h, w, esc, ori, weight, i, u, so, cs, op, cp, q, pf, face);

    put(" CreateFont(%d,%d,%d,%lu,\"%s\")", h, w, weight, (unsigned long)cs, face);
    return (HFONT)(uintptr_t)(r ? fake_of(r) : 0);
}

static HGDIOBJ WINAPI hk_SelectObject(HDC dc, HGDIOBJ o)
{
    HGDIOBJ r = SelectObject((HDC)real_of((uint32_t)(uintptr_t)dc), real_of((uint32_t)(uintptr_t)o));

    put(" SelectObject(%#x,%#x)", (uint32_t)(uintptr_t)dc, (uint32_t)(uintptr_t)o);
    return (HGDIOBJ)(uintptr_t)(r ? fake_of(r) : 0);
}

static BOOL WINAPI hk_DeleteObject(HGDIOBJ o)
{
    BOOL r = DeleteObject(real_of((uint32_t)(uintptr_t)o));
    unsigned i;

    for (i = 0; i < ndibs; i++)
        if (dibs[i].handle == (uint32_t)(uintptr_t)o)
            dibs[i].live = 0;

    put(" DeleteObject(%#x)", (uint32_t)(uintptr_t)o);
    return r;
}

static BOOL WINAPI hk_DeleteDC(HDC dc)
{
    BOOL r = DeleteDC((HDC)real_of((uint32_t)(uintptr_t)dc));

    put(" DeleteDC(%#x)", (uint32_t)(uintptr_t)dc);
    return r;
}

static int WINAPI hk_SetBkMode(HDC dc, int m)
{
    put(" SetBkMode(%d)", m);
    return SetBkMode((HDC)real_of((uint32_t)(uintptr_t)dc), m);
}

static COLORREF WINAPI hk_SetBkColor(HDC dc, COLORREF c)
{
    put(" SetBkColor(%#lx)", (unsigned long)c);
    return SetBkColor((HDC)real_of((uint32_t)(uintptr_t)dc), c);
}

static COLORREF WINAPI hk_SetTextColor(HDC dc, COLORREF c)
{
    put(" SetTextColor(%#lx)", (unsigned long)c);
    return SetTextColor((HDC)real_of((uint32_t)(uintptr_t)dc), c);
}

static BOOL WINAPI hk_PatBlt(HDC dc, int x, int y, int w, int h, DWORD rop)
{
    put(" PatBlt(%d,%d,%d,%d,%#lx)", x, y, w, h, (unsigned long)rop);
    return PatBlt((HDC)real_of((uint32_t)(uintptr_t)dc), x, y, w, h, rop);
}

static int WINAPI hk_FillRect(HDC dc, const RECT *rc, HBRUSH br)
{
    put(" FillRect(%ld,%ld,%ld,%ld)", rc->left, rc->top, rc->right, rc->bottom);
    return FillRect((HDC)real_of((uint32_t)(uintptr_t)dc), rc, (HBRUSH)real_of((uint32_t)(uintptr_t)br));
}

static int WINAPI hk_DrawTextA(HDC dc, LPCSTR text, int n, LPRECT rc, UINT fl)
{
    int r = DrawTextA((HDC)real_of((uint32_t)(uintptr_t)dc), text, n, rc, fl);

    /* measuring leaves the rectangle's far corner as the caller's uninitialised
     * stack had it, and the height it returns with it */
    if (fl & 0x400)
        put(" DrawText(\"%.40s\",%d,%#x) width %ld", text, n, fl, rc->right - rc->left);
    else
        put(" DrawText(\"%.40s\",%d,%#x)=%d rect %ld,%ld,%ld,%ld", n == 1 ? text : "...", n, fl, r,
            rc->left, rc->top, rc->right, rc->bottom);
    return r;
}

static void patch_import(HMODULE m, const char *dll, const char *func, void *hook)
{
    uint8_t *base = (uint8_t *)m;
    uint32_t e = *(uint32_t *)(base + 0x3c), imp = *(uint32_t *)(base + e + 24 + 104);
    uint32_t *d = (uint32_t *)(base + imp);

    for (; d[3]; d += 5) {
        uint32_t *names = (uint32_t *)(base + (d[0] ? d[0] : d[4])), *iat = (uint32_t *)(base + d[4]);
        unsigned i;

        if (_stricmp((const char *)(base + d[3]), dll) != 0)
            continue;
        for (i = 0; names[i]; i++)
            if (!(names[i] & 0x80000000) && strcmp((const char *)(base + names[i] + 2), func) == 0) {
                DWORD old;

                VirtualProtect(&iat[i], 4, PAGE_READWRITE, &old);
                iat[i] = (uint32_t)(uintptr_t)hook;
                VirtualProtect(&iat[i], 4, old, &old);
                return;
            }
    }
    printf("no import %s!%s to hook\n", dll, func);
}

static void hook_gdi(HMODULE m)
{
    patch_import(m, "GDI32.dll", "CreateDIBSection", hk_CreateDIBSection);
    patch_import(m, "GDI32.dll", "CreateCompatibleDC", hk_CreateCompatibleDC);
    patch_import(m, "GDI32.dll", "CreateSolidBrush", hk_CreateSolidBrush);
    patch_import(m, "GDI32.dll", "CreateFontA", hk_CreateFontA);
    patch_import(m, "GDI32.dll", "SelectObject", hk_SelectObject);
    patch_import(m, "GDI32.dll", "DeleteObject", hk_DeleteObject);
    patch_import(m, "GDI32.dll", "DeleteDC", hk_DeleteDC);
    patch_import(m, "GDI32.dll", "SetBkMode", hk_SetBkMode);
    patch_import(m, "GDI32.dll", "SetBkColor", hk_SetBkColor);
    patch_import(m, "GDI32.dll", "SetTextColor", hk_SetTextColor);
    patch_import(m, "GDI32.dll", "PatBlt", hk_PatBlt);
    patch_import(m, "USER32.dll", "FillRect", hk_FillRect);
    patch_import(m, "USER32.dll", "DrawTextA", hk_DrawTextA);
}

static void gdi_reset(void)
{
    unsigned i;

    for (i = 0; i < ngdi; i++)
        if (gdi_real[i])
            DeleteObject(gdi_real[i]);
    ngdi = 0;
    for (i = 0; i < ndibs; i++) {
        uint32_t pg;

        for (pg = dibs[i].lo >> 12; pg <= (dibs[i].hi - 1) >> 12; pg++)
            dib_page[pg] = 0;
    }
    ndibs = 0;
}

/* ---- the sides, and the gauges they load -------------------------------------------- */

#define IMAGE_SIZE 0x16580

struct side {
    const char *name;
    HMODULE h;
    uint32_t *imports;          /* its ImportTable, which its .data starts at */
    uint32_t *linkage;
    uint32_t text_lo, text_hi;
    uint8_t pristine[IMAGE_SIZE];
};

static struct side sides[2];
static struct side *cur;

struct gauge {
    HMODULE h;
    uint32_t *linkage;
    unsigned region;
    int loaded;
    char name[260];
    uint32_t wlo, whi;          /* its writable sections, for the compare */
    uint32_t wlo2, whi2;
};

#define MAXGAUGE 96
static struct gauge gauges[MAXGAUGE];
static unsigned ngauges;

static void find_sections(HMODULE h, uint32_t *tlo, uint32_t *thi, uint32_t *wlo, uint32_t *whi,
                          uint32_t *wlo2, uint32_t *whi2)
{
    uint8_t *base = (uint8_t *)h;
    uint32_t e = *(uint32_t *)(base + 0x3c);
    unsigned n = *(uint16_t *)(base + e + 6), i, opt = *(uint16_t *)(base + e + 20), w = 0;
    uint8_t *sh = base + e + 24 + opt;

    for (i = 0; i < n; i++, sh += 40) {
        uint32_t va = *(uint32_t *)(sh + 12), vs = *(uint32_t *)(sh + 8), ch = *(uint32_t *)(sh + 36);
        uint32_t lo = (uint32_t)(uintptr_t)base + va, hi = lo + vs;

        if (tlo && (ch & 0x20000000)) {
            *tlo = lo;
            *thi = hi;
        }
        if (wlo && (ch & 0x80000000) && !(ch & 0x02000000)) {
            if (w == 0) {
                *wlo = lo;
                *whi = hi;
            } else if (w == 1) {
                *wlo2 = lo;
                *whi2 = hi;
            }
            w++;
        }
    }
}

static void set_links(uint32_t *imports)
{
    for (; imports[0]; imports += 2)
        switch (imports[0]) {
        case 1: imports[1] = (uint32_t)(uintptr_t)host_blob; break;
        case 2: imports[1] = (uint32_t)(uintptr_t)global_blob; break;
        case 3: imports[1] = (uint32_t)(uintptr_t)main_blob; break;
        case 4: imports[1] = (uint32_t)(uintptr_t)chain_blob; break;
        case 10: imports[1] = (uint32_t)(uintptr_t)window_blob; break;
        case 13: imports[1] = (uint32_t)(uintptr_t)hg2d.linkage; break;
        case 19: imports[1] = 0x19190000u; break;       /* never read */
        case 23: imports[1] = (uint32_t)(uintptr_t)acontain_blob; break;
        case 39: imports[1] = (uint32_t)(uintptr_t)sound_blob; break;
        }
}

static int load_side(struct side *s, const char *path)
{
    s->h = LoadLibraryA(path);
    if (!s->h) {
        printf("cannot load %s (%lu)\n", path, GetLastError());
        return 0;
    }
    s->imports = (uint32_t *)GetProcAddress(s->h, "ImportTable");
    s->linkage = (uint32_t *)GetProcAddress(s->h, "Linkage");
    if (!s->imports || !s->linkage || s->linkage[0] != 15) {
        printf("%s: not PANELS\n", path);
        return 0;
    }
    find_sections(s->h, &s->text_lo, &s->text_hi, NULL, NULL, NULL, NULL);
    hook_gdi(s->h);
    return 1;
}

static void select_side(struct side *s)
{
    cur = s;
    cur_name = s->name;
    cur_text_lo = s->text_lo;
    /* this side's image (0, 1) and, so that a constant that happens to equal an
     * address of the other's is the same word in both runs, the other's (2, 3) */
    struct side *o = s == &sides[0] ? &sides[1] : &sides[0];

    nregions = 4;
    regions[0].lo = (uint32_t)(uintptr_t)s->imports;
    regions[0].hi = regions[0].lo + IMAGE_SIZE;
    regions[0].live = 1;
    regions[1].lo = s->text_lo;
    regions[1].hi = s->text_hi;
    regions[1].live = 1;
    regions[2].lo = (uint32_t)(uintptr_t)o->imports;
    regions[2].hi = regions[2].lo + IMAGE_SIZE;
    regions[2].live = 1;
    regions[3].lo = o->text_lo;
    regions[3].hi = o->text_hi;
    regions[3].live = 1;
}

/* host +0x26c: the gauge loaded from its file, its PANELS import this side's Linkage */
static int32_t STDCALL h_module_load(uint32_t *slot, const char *path, uint32_t ext)
{
    char full[600];
    struct gauge *g;
    HMODULE h;
    uint32_t *imp;

    nmodules++;
    snprintf(full, sizeof full, "%s%s", path, ext == 0x10 ? ".GAU" : "");
    put(" modload(%s)", full);
    if (scen->fail_module && nmodules == scen->fail_module) {
        put("=FAIL");
        return -1;
    }
    if (ngauges >= MAXGAUGE || nregions >= MAXREG) {
        put("=FULL");
        return -1;
    }
    h = LoadLibraryA(full);
    if (!h) {
        put("=NOFILE");
        return -1;
    }
    imp = (uint32_t *)GetProcAddress(h, "ImportTable");
    if (!imp || !GetProcAddress(h, "Linkage")) {
        FreeLibrary(h);
        put("=BAD");
        return -1;
    }
    for (; imp[0]; imp += 2)
        if (imp[0] == 15)
            imp[1] = (uint32_t)(uintptr_t)cur->linkage;
    g = &gauges[ngauges++];
    memset(g, 0, sizeof *g);
    g->h = h;
    g->linkage = (uint32_t *)GetProcAddress(h, "Linkage");
    g->loaded = 1;
    snprintf(g->name, sizeof g->name, "%s", full);
    g->region = nregions;
    regions[nregions].lo = (uint32_t)(uintptr_t)h;
    regions[nregions].hi = regions[nregions].lo + *(uint32_t *)((uint8_t *)h + *(uint32_t *)((uint8_t *)h + 0x3c) + 24 + 56);
    regions[nregions].live = 1;
    nregions++;
    find_sections(h, NULL, NULL, &g->wlo, &g->whi, &g->wlo2, &g->whi2);
    *slot = (uint32_t)(uintptr_t)g->linkage;
    put("=ok mod%u", g->region);
    return 0;
}

static int32_t STDCALL h_module_release(uint32_t *slot)
{
    unsigned i;

    put(" modrelease(");
    put_ptr(*slot);
    put(")");
    for (i = 0; i < ngauges; i++)
        if (gauges[i].loaded && (uint32_t)(uintptr_t)gauges[i].linkage == *slot) {
            gauges[i].loaded = 0;
            regions[gauges[i].region].live = 0;
            FreeLibrary(gauges[i].h);
            break;
        }
    return 0;
}

/* the host's stack of modules being run: the module's HMODULE, which the
 * gauge passes back to PANELS to find its bitmaps */
static int32_t STDCALL h_module_enter(uint32_t linkage, uint32_t *out)
{
    unsigned i;

    nenters++;
    put(" enter(");
    put_ptr(linkage);
    put(")");
    *out = 0;
    for (i = 0; i < ngauges; i++)
        if (gauges[i].loaded && (uint32_t)(uintptr_t)gauges[i].linkage == linkage) {
            *out = (uint32_t)(uintptr_t)gauges[i].h;
            return 0;
        }
    return -1;
}

static int32_t STDCALL h_module_leave(uint32_t tag)
{
    (void)tag;
    put(" leave");
    return 0;
}

static void free_gauges(void)
{
    unsigned i;

    for (i = 0; i < ngauges; i++) {
        if (gauges[i].loaded) {
            FreeLibrary(gauges[i].h);
            gauges[i].loaded = 0;
        }
        /* a gauge left mapped carries this run's data into the next */
        while (GetModuleHandleA(gauges[i].name))
            FreeLibrary(GetModuleHandleA(gauges[i].name));
    }
    ngauges = 0;
}

/* ---- CHAIN, ACONTAIN, MAIN, SOUND, WINDOW --------------------------------------------- */

struct cbent {
    uint32_t list, fn;
};
static struct cbent cbs[64];
static unsigned ncbs;

/* a function of the module under test, as the order it was first named in */
static uint32_t fns_seen[64];
static unsigned nfns_seen;

static void put_fn(uint32_t fn)
{
    unsigned i;

    if (fn < cur->text_lo || fn >= cur->text_hi) {
        put_ptr(fn);
        return;
    }
    for (i = 0; i < nfns_seen && fns_seen[i] != fn; i++)
        ;
    if (i == nfns_seen && nfns_seen < 64)
        fns_seen[nfns_seen++] = fn;
    put("fn#%u", i);
}

static int32_t STDCALL ch_add(uint32_t list, uint32_t fn)
{
    put(" chain.add(%#x,", list);
    put_fn(fn);
    put(")");
    if (ncbs < 64) {
        cbs[ncbs].list = list;
        cbs[ncbs].fn = fn;
        ncbs++;
    }
    return 0;
}

static int32_t STDCALL ch_remove(uint32_t list, uint32_t fn)
{
    unsigned i;

    put(" chain.remove(%#x,", list);
    put_fn(fn);
    put(")");
    for (i = 0; i < ncbs; i++)
        if (cbs[i].list == list && cbs[i].fn == fn) {
            memmove(cbs + i, cbs + i + 1, (ncbs - i - 1) * sizeof cbs[0]);
            ncbs--;
            return 0;
        }
    return -1;
}

typedef uint32_t (STDCALL *cb_fn)(uint32_t, uint32_t);

static uint32_t dispatch_depth;

static uint32_t chain_dispatch(uint32_t list, uint32_t a, uint32_t b)
{
    struct cbent snap[64];
    unsigned i, n = 0;

    for (i = 0; i < ncbs; i++)
        if (cbs[i].list == list)
            snap[n++] = cbs[i];
    put(" dispatch(%#x,%#x,%#x){", list, a, b);
    if (++dispatch_depth < 4)
        for (i = 0; i < n; i++) {
            put(" cb:");
            put_fn(snap[i].fn);
            ((cb_fn)(uintptr_t)snap[i].fn)(a, b);
        }
    dispatch_depth--;
    put(" }");
    return 0;
}

static int32_t STDCALL ch_dispatch(uint32_t list, uint32_t a, uint32_t b)
{
    return (int32_t)chain_dispatch(list, a, b);
}

static const char *const aircraft_dirs[7] = {
    "B737_400", "Bell206B", "C182", "Extra300", "Lear45", "Schweizr", "Sopcamel"
};

static int32_t STDCALL ac_query(uint32_t a, uint32_t key, char *buf, uint32_t n)
{
    put(" acontain(%u,%u)", a, key);
    if (a < 1 || a > 7 || (key != 5 && key != 6))
        return 0;
    snprintf(buf, n, "%s\\Aircraft\\%s\\panel%s", GAME_DIR, aircraft_dirs[a - 1],
             key == 6 ? "\\panel.cfg" : "");
    return 1;
}

static int32_t STDCALL mn_begin(uint32_t kind, uint32_t size)
{
    put(" main.begin(%#x,%#x)", kind, size);
    return 0;
}

static int32_t STDCALL mn_write(const uint8_t *p, uint32_t n, uint32_t z)
{
    uint32_t i;

    put(" main.write(%u,%u:", n, z);
    for (i = 0; i + 4 <= n; i += 4) {
        uint32_t w;

        memcpy(&w, p + i, 4);
        put(" %x", norm(w));
    }
    put(")");
    return 0;
}

static int32_t STDCALL mn_end(void)
{
    put(" main.end");
    return 0;
}

static int32_t STDCALL mn_get(uint32_t what, int32_t *t)
{
    nmain_calls++;
    put(" main.get(%u)", what);
    if (what >= 8 || main_fail[what])
        return -1;
    *t = main_val[what];
    return 0;
}

static uint32_t STDCALL sn_state(void)
{
    nsound_calls++;
    return sound_state;
}

static int32_t STDCALL wn_new(uint32_t *out, uint32_t title, uint32_t type)
{
    uint32_t *r;
    unsigned i, same = 0;

    put(" window.new(");
    if (title < 0x10000)
        put("id %#x", title);
    else
        put("\"%s\"", (const char *)(uintptr_t)title);
    put(",%u)", type);
    if (nwins >= MAXWIN)
        return -1;
    r = arena_get(0xa4, K_HEAP);
    if (!r)
        return -1;
    memset(r, 0, 0xa4);
    for (i = 0; i < nwins; i++)
        same += ((uint32_t *)(uintptr_t)wins[i])[1] == type;
    r[0] = title < 0x10000 ? title : (uint32_t)(uintptr_t)arena_get((uint32_t)strlen((const char *)(uintptr_t)title) + 1, K_HEAP);
    if (title >= 0x10000 && r[0])
        strcpy((char *)(uintptr_t)r[0], (const char *)(uintptr_t)title);
    r[1] = type;
    if (type == 2 || type == 3)
        r[0x1c / 4] = (uint32_t)(uintptr_t)arena_get(0xc0, K_HEAP);   /* the view block */
    r[3] = 0x200 + same + 1;
    wins[nwins++] = (uint32_t)(uintptr_t)r;
    *out = (uint32_t)(uintptr_t)r;
    put("=");
    put_ptr(*out);
    return 0;
}

static int32_t STDCALL wn_free(uint32_t *prec)
{
    unsigned i;

    put(" window.free(");
    put_ptr(*prec);
    put(")");
    for (i = 0; i < nwins; i++)
        if (wins[i] == *prec) {
            memmove(wins + i, wins + i + 1, (nwins - i - 1) * sizeof wins[0]);
            nwins--;
            kill_block(*prec);
            return 0;
        }
    return -1;
}

static int32_t STDCALL wn_list(uint32_t *out, uint32_t *count, uint32_t type)
{
    uint32_t *l;
    unsigned i, n = 0;

    put(" window.list(%u)", type);
    if (out)
        *out = 0;
    *count = 0;
    l = arena_get(4 * nwins, K_HEAP);
    if (!l)
        return -1;
    memset(l, 0, 4 * nwins);
    for (i = 0; i < nwins; i++)
        if (type == 0 || ((uint32_t *)(uintptr_t)wins[i])[1] == type)
            l[n++] = wins[i];
    if (out) {
        *out = (uint32_t)(uintptr_t)l;
        *count = n;
        return 0;
    }
    kill_block((uint32_t)(uintptr_t)l);
    *count = n;
    return 0;
}

static uint32_t STDCALL wn_find(uint32_t id, uint32_t type)
{
    unsigned i;

    put(" window.find(%#x,%u)", id, type);
    for (i = 0; i < nwins; i++) {
        uint32_t *r = (uint32_t *)(uintptr_t)wins[i];

        if (r[1] == type && (int32_t)r[3] == (int32_t)id)
            return wins[i];
    }
    return 0;
}

/* the services' tables; every slot not listed traps to a logger */
static int32_t STDCALL host_unexpected(void)
{
    put(" HOST?");
    return 0;
}

#define SLOT(blob, off, fn) (*(void **)((blob) + (off)) = (void *)(fn))

static void build_tables(void)
{
    unsigned i;

    memset(host_blob, 0, sizeof host_blob);
    for (i = 4; i < 0x320; i += 4)
        SLOT(host_blob, i, host_unexpected);
    *(uint32_t *)host_blob = 1;
    SLOT(host_blob, 0x03c, h_post);
    SLOT(host_blob, 0x050, h_busy);
    SLOT(host_blob, 0x064, h_window_create);
    SLOT(host_blob, 0x068, h_window_destroy);
    SLOT(host_blob, 0x07c, h_window_update);
    SLOT(host_blob, 0x088, h_window_invalidate);
    SLOT(host_blob, 0x0a8, h_cfg_get);
    SLOT(host_blob, 0x0ac, h_cfg_put);
    SLOT(host_blob, 0x0d0, h_profile_int);
    SLOT(host_blob, 0x0e0, h_profile_num);
    SLOT(host_blob, 0x0f0, h_profile_string);
    SLOT(host_blob, 0x158, h_exists);
    SLOT(host_blob, 0x184, h_path_join);
    SLOT(host_blob, 0x1b4, h_surface_new);
    SLOT(host_blob, 0x1b8, h_surface_free);
    SLOT(host_blob, 0x1bc, h_surface_resize);
    SLOT(host_blob, 0x20c, h_alloc);
    SLOT(host_blob, 0x214, h_free);
    SLOT(host_blob, 0x218, h_memset);
    SLOT(host_blob, 0x21c, h_memmove);
    SLOT(host_blob, 0x26c, h_module_load);
    SLOT(host_blob, 0x270, h_module_release);
    SLOT(host_blob, 0x27c, h_child_add);
    SLOT(host_blob, 0x284, h_child_remove);
    SLOT(host_blob, 0x28c, h_palette_copy);
    SLOT(host_blob, 0x298, h_module_enter);
    SLOT(host_blob, 0x29c, h_module_leave);
    SLOT(host_blob, 0x2a0, h_load_string);
    SLOT(host_blob, 0x2a8, h_strcpy);
    SLOT(host_blob, 0x2b4, h_strdup);
    SLOT(host_blob, 0x2b8, h_strlen);
    SLOT(host_blob, 0x2bc, h_strcmp);
    SLOT(host_blob, 0x2f8, h_list_add);
    SLOT(host_blob, 0x2fc, h_list_free);
    SLOT(host_blob, 0x300, h_list_remove);
    SLOT(host_blob, 0x304, h_list_reset);
    SLOT(host_blob, 0x308, h_list_walk);
    SLOT(host_blob, 0x310, h_list_find);

    memset(chain_blob, 0, sizeof chain_blob);
    *(uint32_t *)chain_blob = 4;
    SLOT(chain_blob, 0x18, ch_add);
    SLOT(chain_blob, 0x1c, ch_remove);
    SLOT(chain_blob, 0x20, ch_dispatch);
    memset(acontain_blob, 0, sizeof acontain_blob);
    *(uint32_t *)acontain_blob = 23;
    SLOT(acontain_blob, 0x30, ac_query);
    memset(main_blob, 0, sizeof main_blob);
    *(uint32_t *)main_blob = 3;
    SLOT(main_blob, 0x5c, mn_begin);
    SLOT(main_blob, 0x60, mn_write);
    SLOT(main_blob, 0x64, mn_end);
    SLOT(main_blob, 0x80, mn_get);
    memset(sound_blob, 0, sizeof sound_blob);
    *(uint32_t *)sound_blob = 39;
    SLOT(sound_blob, 0x44, sn_state);
    memset(window_blob, 0, sizeof window_blob);
    *(uint32_t *)window_blob = 10;
    SLOT(window_blob, 0x78, wn_new);
    SLOT(window_blob, 0x7c, wn_free);
    SLOT(window_blob, 0x80, wn_list);
    SLOT(window_blob, 0x8c, wn_find);
}

/* ---- scenarios ------------------------------------------------------------------------ */

#define VAR_TABLE 0x608
#define VAR_ENTRIES 380

static uint32_t pick(void)
{
    switch (rnd() % 8) {
    case 0: case 1: return 0;
    case 2: return 1;
    case 3: case 4: return rnd() % 256;
    case 5: return rnd() % 65536;
    default: return rnd();
    }
}

/* a value for a variable of PANELS's table, by the type it declares */
static void fill_field(uint8_t *g, uint32_t off, uint32_t type)
{
    uint32_t v = pick();

    if (off + 16 > 0x1300)
        return;
    switch (type) {
    case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
        g[off] = (uint8_t)v;
        break;
    case 0x21: {
        double d = rnd() % 4 ? ((int32_t)(rnd() % 2000001) - 1000000) / 1000.0 : 0.0;

        memcpy(g + off, &d, 8);
        break;
    }
    case 0x1a: case 0x1b: case 0x1c: case 0x1d: {
        uint16_t w = (uint16_t)(rnd() % 256);
        uint32_t d = pick();

        memcpy(g + off + 2, &w, 2);
        memcpy(g + off + 4, &d, 4);
        break;
    }
    default:
        if (type >= 0x06 && type <= 0x10) {
            uint16_t w = (uint16_t)v;

            memcpy(g + off, &w, 2);
        } else if (type != 0x25) {
            memcpy(g + off, &v, 4);
        }
        break;
    }
}

static void make_scenario(struct scenario *s, unsigned v)
{
    static const int stypes[] = { 1, 1, 1, 1, 0, 2, 2, 3, 3, 4, 4, 4, 4, 5, 5 };
    static const int sizes[][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 800, 600 } };
    const uint32_t *t = (const uint32_t *)(sides[0].pristine + VAR_TABLE);
    unsigned i, n;

    memset(s, 0, sizeof *s);
    rstate = 0x9e3779b9u * (v + 1) | 1;
    for (i = 0; i < 8; i++)
        rnd();
    s->seed = rstate;
    s->surf_type = stypes[rnd() % (sizeof stypes / sizeof stypes[0])];
    i = rnd() % 4;
    s->screen_w = sizes[i][0];
    s->screen_h = sizes[i][1];
    s->image_quality = (int)(rnd() % 4);
    s->fill = (uint8_t)(rnd() % 3 == 0 ? rnd() : 0);
    s->fail_alloc = rnd() % 4 == 0 ? 1 + rnd() % 40 : 0;     /* of the flag-4 allocations */
    s->fail_surface = 0;
    s->fail_module = rnd() % 20 == 0 ? 1 + rnd() % 30 : 0;
    s->fail_exists = rnd() % 20 == 0 ? 1 + rnd() % 30 : 0;
    s->hg2d_seed = rnd();
    s->palette_seed = rnd();
    for (i = 1; i < VAR_ENTRIES; i++) {
        const uint32_t *e = t + 4 * i;

        if (e[0] != 0 && e[1] < 0x1300 && e[2] >= 1 && e[2] <= 0x26)
            fill_field(s->global, e[1], e[2]);
    }
    /* the fields the variables' functions and the harness-owned pointers use */
    n = 1 + rnd() % 256;
    *(uint16_t *)(s->global + 0xaf4) = (uint16_t)n;
    for (i = 0; i < 4; i++) {
        static const uint16_t dbl[4] = { 0x918, 0x9b0, 0xa48, 0xae0 };
        double d = ((int32_t)(rnd() % 2000001) - 1000000) / 100.0;

        memcpy(s->global + dbl[i], &d, 8);
    }
    for (i = 0; i < 64; i++)
        *(uint16_t *)(s->global + (rnd() % 0x100) * 4 + 0x200) = (uint16_t)pick();
    *(uint32_t *)(s->global + 0x1224) = rnd() % 2;
    *(uint32_t *)(s->global + 0x1228) = rnd() % 2;
    *(uint16_t *)(s->global + 0xb02) = 0;
    *(uint16_t *)(s->global + 0xb04) = 0;
    *(uint32_t *)(s->global + 0xb10) = 0;
    *(uint32_t *)(s->global + 0xb14) = 0;
    *(uint8_t *)(s->global + 0xc46) = (uint8_t)(rnd() % 2 ? 4 : 0);
    *(uint8_t *)(s->global + 0x288) = (uint8_t)(rnd() % 34);
    *(uint32_t *)(s->global + 0x2f0) = 0;
    *(uint32_t *)(s->global + 0x2c0) = rnd() % 360;
    for (i = 0; i < 16; i++)
        s->sim[i] = rnd();
    for (i = 0; i < 8; i++) {
        s->main_fail[i] = rnd() % 5 == 0;
        s->main_val[i] = rnd() % 4 == 0 ? -1 : (int32_t)(rnd() % 3000);
    }
    s->sound_state = rnd() % 32;
    s->nops = 3 + (int)(rnd() % (MAXOPS - 3));
    for (i = 0; i < (unsigned)s->nops; i++) {
        struct op *o = &s->op[i];
        unsigned r = rnd() % 100;

        o->a = rnd();
        o->b = rnd();
        o->c = rnd();
        o->d = rnd();
        if (i == 0)
            o->kind = OP_AIRCRAFT;
        else if (r < 12) o->kind = OP_AIRCRAFT;
        else if (r < 40) o->kind = OP_FRAMES;
        else if (r < 46) o->kind = OP_RESET;
        else if (r < 52) o->kind = OP_COMMAND;
        else if (r < 60) o->kind = OP_SITUATION;
        else if (r < 63) o->kind = OP_ENGINES;
        else if (r < 66) o->kind = OP_SHOW;
        else if (r < 69) o->kind = OP_HIDE;
        else if (r < 71) o->kind = OP_SHOW_ALL;
        else if (r < 73) o->kind = OP_HIDE_ALL;
        else if (r < 75) o->kind = OP_TOGGLE;
        else if (r < 77) o->kind = OP_OPTIONS;
        else if (r < 79) o->kind = OP_ANY_SHOWN;
        else if (r < 81) o->kind = OP_TOGGLE_ID;
        else if (r < 87) o->kind = OP_PAINT;
        else if (r < 90) o->kind = OP_MOUSE;
        else if (r < 92) o->kind = OP_POST;
        else if (r < 94) o->kind = OP_NAMED;
        else if (r < 96) o->kind = OP_TWEAK;
        else if (r < 98) o->kind = OP_NEWELEM;
        else o->kind = OP_REINIT;
    }
}

/* ---- the world, and the state compared ------------------------------------------------- */

static uint32_t world_sim, world_view, world_s384;

static uint32_t rnd_hg2d(void)
{
    return rnd();
}

static int detail;

static void world_reset(const struct scenario *s)
{
    uint8_t *p, *q;
    uint32_t i;

    scen = s;
    arena_reset();
    fail_at = s->fail_alloc;
    memcpy(global_blob, s->global, sizeof global_blob);
    nwins = ncbs = ndeferred = nfns_seen = 0;
    nsurfaces = nmodules = nexists = nenters = nmain_calls = nsound_calls = 0;
    dispatch_depth = 0;
    memcpy(main_val, s->main_val, sizeof main_val);
    memcpy(main_fail, s->main_fail, sizeof main_fail);
    sound_state = s->sound_state;
    /* what GLOBAL's pointers point at: the sim module's linkage with two doubles,
     * the active view's block, and the dialog's structure */
    p = arena_get(0x40, K_HEAP);
    q = arena_get(8, K_HEAP);
    for (i = 0; i < 2; i++) {
        double d = ((int32_t)(s->sim[i] % 720001) - 360000) / 1000.0;

        memcpy(q, &d, 8);
        ((uint32_t *)p)[8 + i] = (uint32_t)(uintptr_t)q;
        q = arena_get(8, K_HEAP);
    }
    world_sim = s->sim[2] % 64 ? (uint32_t)(uintptr_t)p : 0;
    p = arena_get(0xc0, K_HEAP);
    for (i = 0; i < 0xc0; i += 4)
        ((uint32_t *)p)[i / 4] = (s->sim[3] + i * 0x9e3779b1u) % 0x400;
    ((uint32_t *)p)[4] = s->sim[4] % 2;
    ((uint32_t *)p)[5] = s->sim[5] % 7;
    world_view = s->sim[6] % 32 ? (uint32_t)(uintptr_t)p : 0;
    p = arena_get(0x10, K_HEAP);
    q = arena_get(0x40, K_HEAP);
    memset(q, (int)s->sim[7], 0x40);
    q[0x1c] = s->sim[8] % 3 ? (uint8_t)(s->sim[8] % 20) : 8;
    ((uint32_t *)p)[1] = (uint32_t)(uintptr_t)q;
    world_s384 = s->sim[9] % 5 ? (uint32_t)(uintptr_t)p : 0;
    *(uint32_t *)(global_blob + 0x11d4) = world_sim;
    *(uint32_t *)(global_blob + 0x11a0) = world_view;
    *(uint32_t *)(global_blob + 0x384) = world_s384;
    rstate = s->hg2d_seed | 1;
    hg2d.global = global_blob;
    hg2d.host_alloc = h_alloc;
    hg2d.host_free = h_free;
    hg2d_model_reset(rnd_hg2d);
}

/* a word of PANELS's .data as it compares: the colour caches (0x1ee0, 0x9ee0,
 * 0x8000 bytes each) are bytes, whatever they look like */
static uint32_t data_word(uint32_t off)
{
    uint32_t w = *(const uint32_t *)((const uint8_t *)cur->imports + off);

    return off >= 0x1ee0 && off < 0x11ee0 ? w : norm(w);
}

static uint64_t hash_data(uint32_t from, uint32_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    uint32_t i;

    for (i = from; i + 4 <= from + n; i += 4)
        h = fnv(h, data_word(i));
    return h;
}

static void put_state(void)
{
    uint64_t h = 0xcbf29ce484222325ull, d, g;
    unsigned i, live = 0;

    d = hash_data(0, IMAGE_SIZE);
    put("\nSTATE data=%016llx", (unsigned long long)d);
    if (detail)
        for (i = 0; i < IMAGE_SIZE; i += 256)
            put(" d%x=%016llx", i, (unsigned long long)hash_data(i, i + 256 <= IMAGE_SIZE ? 256 : IMAGE_SIZE - i));
    if (detail) {
        put("\n  D:");
        for (i = 0; i < IMAGE_SIZE; i += 4)
            put(" %x", data_word(i));
    }
    for (i = 0; i < nblks; i++) {
        uint64_t bh;

        if (!blks[i].live)
            continue;
        live++;
        bh = hash_words(arena + blks[i].off, blks[i].size, blks[i].kind == K_HEAP);
        h = fnv(h, (uint32_t)bh);
        h = fnv(h, (uint32_t)(bh >> 32));
        h = fnv(h, blks[i].off);
        if (detail) {
            put("\n  B%u+%x/%x %016llx", i, blks[i].off, blks[i].size, (unsigned long long)bh);
            if (blks[i].size <= 0x48 && blks[i].kind == K_HEAP) {
                uint32_t k;

                for (k = 0; k + 4 <= blks[i].size; k += 4)
                    put(" %x", norm(*(uint32_t *)(arena + blks[i].off + k)));
            }
        }
    }
    put(" blocks=%016llx/%u", (unsigned long long)h, live);
    for (i = 0; i < ndibs; i++)
        if (dibs[i].live)
            put(" dib%u=%016llx", i, (unsigned long long)hash_words((const uint8_t *)(uintptr_t)dibs[i].lo,
                                                                     dibs[i].hi - dibs[i].lo, 0));
    g = hash_words(global_blob, 0x2000, 0);
    put(" global=%016llx", (unsigned long long)g);
    norm_dibs = 0;          /* a gauge's data holds no pointer into a DIB, and a constant can look like one */
    for (i = 0; i < ngauges; i++)
        if (gauges[i].loaded) {
            uint64_t gh = 0xcbf29ce484222325ull;

            if (gauges[i].whi > gauges[i].wlo)
                gh = hash_words((const uint8_t *)(uintptr_t)gauges[i].wlo, gauges[i].whi - gauges[i].wlo, 1);
            if (gauges[i].whi2 > gauges[i].wlo2)
                gh ^= hash_words((const uint8_t *)(uintptr_t)gauges[i].wlo2, gauges[i].whi2 - gauges[i].wlo2, 1);
            put(" g%u=%016llx", i, (unsigned long long)gh);
            if (detail && gauges[i].whi > gauges[i].wlo) {
                uint32_t k;

                put("\n  G%u:", i);
                for (k = 0; k + 4 <= gauges[i].whi - gauges[i].wlo; k += 4) {
                    uint32_t raw = *(uint32_t *)(uintptr_t)(gauges[i].wlo + k);

                    put(" %x", norm(raw));
                    if (getenv("PANELS_RAW") && norm(raw) != raw)
                        put("[%x]", raw);
                }
                if (gauges[i].whi2 > gauges[i].wlo2) {
                    put("\n  H%u:", i);
                    for (k = 0; k + 4 <= gauges[i].whi2 - gauges[i].wlo2; k += 4)
                        put(" %x", norm(*(uint32_t *)(uintptr_t)(gauges[i].wlo2 + k)));
                }
            }
        }
    norm_dibs = 1;
    hg2d_model_put_state();
}

/* ---- running a script ----------------------------------------------------------------- */

typedef uint32_t (STDCALL *f0_t)(void);
typedef uint32_t (STDCALL *f1_t)(uint32_t);
typedef uint32_t (STDCALL *f2_t)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *f3_t)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (STDCALL *f4_t)(uint32_t, uint32_t, uint32_t, uint32_t);

#define LK(off) (cur->linkage[(off) / 4])

static uint32_t window_pick(uint32_t k)
{
    return nwins ? wins[k % nwins] : 0;
}

static void tweak_global(const struct op *o)
{
    const uint32_t *t = (const uint32_t *)(sides[0].pristine + VAR_TABLE);
    unsigned i, n = 1 + o->a % 12;

    rstate = o->b | 1;
    for (i = 0; i < n; i++) {
        const uint32_t *e = t + 4 * (1 + rnd() % (VAR_ENTRIES - 1));

        if (e[0] != 0 && e[1] < 0x1300 && e[2] >= 1 && e[2] <= 0x26)
            fill_field(global_blob, e[1], e[2]);
    }
    if (o->c % 4 == 0)
        *(uint8_t *)(global_blob + 0x288) = (uint8_t)(o->d % 34);
    if (o->c % 4 == 1)
        *(uint8_t *)(global_blob + 0xc46) = (uint8_t)((o->d & 1) ? 4 : 0);
    if (o->c % 4 == 2)
        main_val[o->d % 8] = (int32_t)(o->b % 3000);
    if (o->c % 4 == 3)
        sound_state = o->d % 32;
    put(" tweak(%u)", n);
}

static void run_named(const struct op *o)
{
    static const char *const names[4] = { "ALPHA", "BETA", "alpha", "GAMMA_LONGER_NAME" };
    uint8_t *v = arena_get(0x20, K_HEAP), *cell = arena_get(16, K_HEAP);
    uint32_t r;

    if (!v || !cell)
        return;
    memset(v, 0, 0x20);
    memcpy(cell, &o->d, 4);
    switch (o->a % 6) {
    case 0:
        r = ((f3_t)LK(0x94))((uint32_t)(uintptr_t)cell, o->b % 0x27, (uint32_t)(uintptr_t)names[o->c % 4]);
        put(" named_add=");
        put_ptr(r);
        break;
    case 1:
        *(uint32_t *)v = 1 + o->b % 380;
        ((f1_t)LK(0x98))((uint32_t)(uintptr_t)v);       /* eax is whatever the variable's function left */
        put(" var_init");
        break;
    case 2:
        ((f2_t)LK(0x9c))((uint32_t)(uintptr_t)v, (uint32_t)(uintptr_t)names[o->c % 4]);
        put(" named_find");           /* eax is the last string compare's */
        break;
    case 3:
        *(uint32_t *)v = 1 + o->b % 380;
        ((f1_t)LK(0x98))((uint32_t)(uintptr_t)v);
        ((f1_t)LK(0xa0))((uint32_t)(uintptr_t)v);
        put(" var_lookup");
        break;
    case 4:
        if (*(uint32_t *)((uint8_t *)cur->imports + 0x1e88) == 0)    /* the list is empty: retail faults on its head */
            break;
        ((f1_t)LK(0xa4))((uint32_t)(uintptr_t)names[o->c % 4]);
        put(" named_remove");         /* eax is the last compare's, or a free's */
        break;
    default:
        ((f0_t)LK(0xa8))();
        put(" named_clear");
        break;
    }
    put(" v=%x,%x,%x,%x", ((uint32_t *)v)[1], ((uint32_t *)v)[2], ((uint32_t *)v)[3], ((uint32_t *)v)[4]);
}

static void run_op(const struct op *o)
{
    uint32_t r, w, i, k;

    switch (o->kind) {
    case OP_AIRCRAFT:
        k = 1 + o->a % 7;
        put("\nAIRCRAFT %u", k);
        r = ((f1_t)LK(0x18))(k);
        put("=%#x", r);
        break;
    case OP_FRAMES:
        k = 1 + o->a % 4;
        put("\nFRAMES %u", k);
        for (i = 0; i < k; i++) {
            chain_dispatch(2, 0, 0);
            if (o->b % 3 == 0)
                chain_dispatch(3, 0, 0);
        }
        break;
    case OP_RESET:
        put("\nRESET");
        chain_dispatch(1, 0, 0);
        break;
    case OP_COMMAND:
        k = 0x10170 + o->a % 13;
        put("\nCOMMAND %#x", k);
        chain_dispatch(0xc, k, 0);
        break;
    case OP_SITUATION: {
        static const uint32_t msgs[] = { 0x20, 0x21, 0x22, 0x22, 0x23, 0x33, 0x34, 0x37, 0x70, 0x71 };
        uint32_t m = msgs[o->a % (sizeof msgs / sizeof msgs[0])], rec = 0;

        put("\nSITUATION %#x", m);
        if (m == 0x22) {
            uint32_t *p = arena_get(0x10, K_HEAP), *d = arena_get(0x28, K_HEAP);

            if (!p || !d)
                break;
            memset(p, 0, 0x10);
            for (i = 0; i < 10; i++)
                d[i] = o->d * (i + 3) % 0x2000;
            d[8] = o->c & 0xc0000001u;
            d[9] = o->b % 3 == 0 ? 0xffffffffu : (o->b % 4) * 25;
            p[0] = o->b % 2 ? 0x320 : 0x321;
            p[3] = (uint32_t)(uintptr_t)d;
            rec = (uint32_t)(uintptr_t)p;
        }
        chain_dispatch(0x14, m, rec);
        break;
    }
    case OP_ENGINES:
        put("\nENGINES");
        chain_dispatch(3, 0, 0);
        break;
    case OP_SHOW:
    case OP_HIDE:
        w = window_pick(o->a);
        put("\n%s ", o->kind == OP_SHOW ? "SHOW" : "HIDE");
        put_ptr(w);
        if (w) {
            r = ((f1_t)LK(o->kind == OP_SHOW ? 0x20 : 0x24))(w);
            put("=%#x", r);
        }
        break;
    case OP_SHOW_ALL:
        put("\nSHOW_ALL");
        ((f0_t)LK(0x28))();
        break;
    case OP_HIDE_ALL:
        put("\nHIDE_ALL");
        ((f0_t)LK(0x2c))();
        break;
    case OP_TOGGLE:
        put("\nTOGGLE");
        ((f0_t)LK(0x30))();
        break;
    case OP_OPTIONS:
        put("\nOPTIONS");
        ((f0_t)LK(0x34))();
        break;
    case OP_ANY_SHOWN:
        r = ((f0_t)LK(0x38))();
        put("\nANY_SHOWN=%u", r);
        break;
    case OP_TOGGLE_ID:
        k = o->a % 3 == 0 ? o->b % 200 : (o->b % 4) * 25;
        r = ((f1_t)LK(0x8c))(k);
        put("\nTOGGLE_ID %u=%u", k, r);
        break;
    case OP_PAINT: {
        uint32_t cb, *rect;
        static const uint32_t modes[] = { 0, 1, 1, 3, 3, 5, 7 };

        w = window_pick(o->a);
        k = modes[o->b % (sizeof modes / sizeof modes[0])];
        put("\nPAINT %u ", k);
        put_ptr(w);
        if (!w)
            break;
        cb = ((uint32_t *)(uintptr_t)w)[0x18 / 4];
        if (!cb)
            break;
        rect = arena_get(16, K_HEAP);
        if (!rect)
            break;
        rect[0] = o->c % 5 ? 0 : o->c % 50;
        rect[1] = o->c % 7 ? 0 : o->d % 50;
        if (o->d % 3 == 0) {
            rect[2] = rect[0] + ((uint32_t *)(uintptr_t)w)[0x60 / 4];
            rect[3] = rect[1] + ((uint32_t *)(uintptr_t)w)[0x64 / 4];
        } else {
            rect[2] = rect[0] + 1 + o->d % 900;
            rect[3] = rect[1] + 1 + (o->d >> 12) % 700;
        }
        r = ((uint32_t (STDCALL *)(uint32_t, uint32_t, uint32_t, uint32_t))cb)(k, w, o->c % 2 ? o->c : 0,
                                                                              (uint32_t)(uintptr_t)rect);
        put("=%#x", r);
        break;
    }
    case OP_MOUSE: {
        uint32_t *m = arena_get(0x2c * 4, K_HEAP), *hdr = arena_get(0x50, K_HEAP), pt[2];

        if (!m || !hdr)
            break;
        memset(m, 0, 0x2c * 4);
        memset(hdr, 0, 0x50);
        m[0] = 1;
        m[1] = o->a % 400;
        m[2] = o->b % 300;
        for (i = 1; i < 3; i++) {
            m[11 * i] = 1;
            m[11 * i + 1] = (o->c >> (4 * i)) % 200;
            m[11 * i + 2] = (o->d >> (4 * i)) % 200;
            m[11 * i + 3] = o->a % 90;
            m[11 * i + 4] = o->b % 90;
        }
        *(float *)&hdr[0x34 / 4] = 0.5f + (o->c % 200) / 100.0f;
        *(float *)&hdr[0x38 / 4] = 0.5f + (o->d % 200) / 100.0f;
        hdr[0x3c / 4] = window_pick(o->a);
        hdr[0x44 / 4] = o->b % 300;
        hdr[0x48 / 4] = o->c % 300;
        pt[0] = o->d % 500;
        pt[1] = o->a % 400;
        put("\nMOUSE %u", o->b % 3);
        if (o->b % 3 == 0) {
            r = ((f3_t)LK(0x80))((uint32_t)(uintptr_t)m, (uint32_t)(uintptr_t)hdr, (uint32_t)(uintptr_t)pt);
            put("=%#x m=%u,%u,%u,%u", r, m[1], m[2], m[3], m[4]);
        } else if (o->b % 3 == 1) {
            r = ((f2_t)LK(0x84))((uint32_t)(uintptr_t)m, (uint32_t)(uintptr_t)hdr);
            put("=%#x", r);
        } else {
            r = ((f2_t)LK(0x88))((uint32_t)(uintptr_t)m, (uint32_t)(uintptr_t)hdr);
            put("=%#x", r);
        }
        break;
    }
    case OP_POST:
        r = ((f2_t)LK(0x90))(o->a % 0x400, o->b % 0x400);
        put("\nPOST=%#x", r);
        break;
    case OP_NAMED:
        put("\nNAMED");
        run_named(o);
        break;
    case OP_TWEAK:
        put("\nTWEAK");
        tweak_global(o);
        break;
    case OP_NEWELEM:
        k = o->a % 10;
        r = ((f1_t)LK(0x5c))(k);
        put("\nNEWELEM %u=", k);
        if (k < 8 && r >= arena_base && r < arena_base + ARENA_SIZE)
            put_ptr(r);
        else
            put("%#x", r);
        ((f1_t)LK(0x54))(0);
        break;
    case OP_REINIT:
        put("\nREINIT");
        ((f0_t)LK(0x08))();
        ((f0_t)LK(0x04))();
        break;
    default:
        break;
    }
}

static volatile DWORD last_progress;

static void call_op_guarded(const struct op *o)
{
    last_progress = GetTickCount();
    __asm__ volatile("movl %%esp, %0" : "=m"(saved_esp));
    if (__builtin_setjmp(fault_jb) == 0) {
        in_module = 1;
        run_op(o);
    }
    in_module = 0;
}

static void call_init_guarded(int deinit)
{
    __asm__ volatile("movl %%esp, %0" : "=m"(saved_esp));
    if (__builtin_setjmp(fault_jb) == 0) {
        in_module = 1;
        ((f0_t)LK(deinit ? 0x08 : 0x04))();
    }
    in_module = 0;
}

static unsigned scenario_no;

static void brk_log(const uint32_t *esp)
{
    unsigned i;

    put(" BREAK(");
    for (i = 1; i <= brk_args; i++) {
        put_ptr(esp[i]);
        put(i < brk_args ? "," : ")");
    }
}

static void arm_cover(void)
{
    unsigned i;
    DWORD old;

    if (!getenv("PANELS_COVER") || cover_on)
        return;
    cover_base = (uint32_t)(uintptr_t)sides[0].h;
    for (i = 0; i < sizeof retail_funcs / sizeof retail_funcs[0]; i++) {
        uint8_t *p = (uint8_t *)(uintptr_t)(cover_base + retail_funcs[i] - 0x10000000u);

        VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old);
        cover_byte[i] = *p;
        *p = 0xcc;
    }
    cover_on = 1;
}

static void arm_break(void)
{
    const char *b = getenv("PANELS_BREAK");
    char *comma;
    DWORD old;

    if (!b || cur != &sides[0] || brk_addr)
        return;
    brk_addr = (uint32_t)(uintptr_t)cur->h + (uint32_t)strtoul(b, &comma, 16) - 0x10000000u;
    brk_args = *comma == ',' ? (uint32_t)strtoul(comma + 1, NULL, 10) : 4;
    VirtualProtect((void *)(uintptr_t)brk_addr, 1, PAGE_EXECUTE_READWRITE, &old);
    brk_byte = *(uint8_t *)(uintptr_t)brk_addr;
    *(uint8_t *)(uintptr_t)brk_addr = 0xcc;
}

static void watch_log(uint32_t eip, uint32_t at, uint32_t after)
{
    int r = region_of(eip);

    put(" WATCH(+%x=%x by %d%+d)", at - (uint32_t)(uintptr_t)cur->imports, after, r,
        r >= 0 ? (int)(eip - regions[r].lo) : 0);
}

static void arm_watch(void)
{
    const char *w = getenv("PANELS_WATCH");
    DWORD old;

    char *comma;

    watch_addr = 0;
    if (!w || !cur)
        return;
    watch_report = watch_log;
    watch_addr = (uint32_t)(uintptr_t)cur->imports + (uint32_t)strtoul(w, &comma, 16);
    watch_len = *comma == ',' ? (uint32_t)strtoul(comma + 1, NULL, 16) : 4;
    VirtualProtect((void *)(uintptr_t)(watch_addr & ~0xfffu), ((watch_addr + watch_len + 0xfff) & ~0xfffu) - (watch_addr & ~0xfffu),
                   PAGE_READONLY, &old);
}

static void disarm_watch(void)
{
    DWORD old;

    if (watch_addr)
        VirtualProtect((void *)(uintptr_t)(watch_addr & ~0xfffu), ((watch_addr + watch_len + 0xfff) & ~0xfffu) - (watch_addr & ~0xfffu),
                       PAGE_READWRITE, &old);
    watch_addr = 0;
}

static void run_side(struct side *s, const struct scenario *sc)
{
    int i;

    select_side(s);
    memcpy(s->imports, s->pristine, IMAGE_SIZE);
    free_gauges();
    gdi_reset();
    nregions = 4;
    tlen = 0;
    if (tlog)
        tlog[0] = 0;
    toverflow = 0;
    faulted = 0;
    world_reset(sc);
    if (getenv("PANELS_REGIONS"))
        printf("%s: data %#x..%#x text %#x..%#x\n", s->name, regions[0].lo, regions[0].hi, regions[1].lo, regions[1].hi);
    put("SCENARIO %u type %d screen %dx%d quality %d fill %02x fail %u/%u/%u/%u", scenario_no,
        sc->surf_type, sc->screen_w, sc->screen_h, sc->image_quality, sc->fill, sc->fail_alloc,
        sc->fail_surface, sc->fail_module, sc->fail_exists);
    put("\nINIT");
    arm_watch();
    arm_break();
    call_init_guarded(0);
    put_state();
    for (i = 0; i < sc->nops && !faulted && !arena_full && !toverflow; i++) {
        call_op_guarded(&sc->op[i]);
        put_state();
    }
    if (arena_full)
        put("\nARENA FULL");
    if (!faulted) {
        put("\nDEINIT");
        call_init_guarded(1);
        put_state();
    }
    disarm_watch();
    free_gauges();
    gdi_reset();
}

/* a fault outside the modules is the harness's own */
static LONG WINAPI on_unhandled(EXCEPTION_POINTERS *e)
{
    char msg[300];
    DWORD wrote;
    int len = snprintf(msg, sizeof msg, "\nharness fault %#lx at %p (scenario %u, %s)\n",
                       (unsigned long)e->ExceptionRecord->ExceptionCode,
                       e->ExceptionRecord->ExceptionAddress, scenario_no, cur_name);

    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)len, &wrote, NULL);
    TerminateProcess(GetCurrentProcess(), 5);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* ---- unit mode: the image routines one at a time ----------------------------------- */

/* The scalers, the colour conversions and the blits are big functions of
 * pure data.  Driven through a whole panel, one wrong rounding shows only as
 * a different byte somewhere in a window; driven alone, over surfaces and
 * bitmaps made at random, it shows where.  `panelsdiff unit [N [START]]`
 * runs each routine over N cases against its retail twin (retail's by
 * address, ours through PANELS_unit.dll's table, built with -DPANELS_UNIT).
 * Compared: the return, the .data, every live block and the DIBs. */
struct ufn {
    const char *name;
    uint32_t retail;
    int nargs;
    int ours_cdecl;
    int has_ret;                /* what it returns means something */
};

enum {
    U_SCALE_24, U_SCALE_8, U_SMOOTH_8, U_SMOOTH_24, U_SHRINK_24, U_SHRINK_8, U_SHRINK_8_TO_8,
    U_SHRINK_8_TO_8_PLAIN, U_SURFACE_FROM_BITMAP, U_TO_8, U_TO_8_HOST, U_TO_555, U_TO_565,
    U_LIGHT_24, U_LIGHT_32, U_MASK_MAKE, U_COPY_8, U_SURFACE_CONVERT, U_CLIP, U_EDGES_24, U_BLIT,
    U_COMPOSE, U_DIB_NEW, U_SURFACE_OVER, U_SURFACE_OVER_FREE, U_BITMAP_SIZE, U_BITMAP_LOAD,
    U_BOUNDS_BUILD, U_VAR_LOOKUP, U_SFB_RESOURCE, U_SFB_PANEL, U_REPLAY, U_N
};

static const struct ufn ufns[U_N] = {
    { "scale_24", 0x10003740, 3, 1, 0 }, { "scale_8", 0x10003840, 3, 1, 0 },
    { "smooth_8", 0x10003960, 3, 1, 0 }, { "smooth_24", 0x10003d40, 3, 1, 0 },
    { "shrink_24", 0x10004120, 3, 1, 0 }, { "shrink_8", 0x10004450, 3, 1, 0 },
    { "shrink_8_to_8", 0x10004770, 6, 1, 0 }, { "shrink_8_to_8_plain", 0x10004f50, 4, 1, 0 },
    { "surface_from_bitmap", 0x10005660, 8, 1, 1 }, { "to_8", 0x100057e0, 2, 0, 0 },
    { "to_8_host", 0x100059e0, 2, 0, 0 }, { "to_555", 0x10005bd0, 4, 0, 0 }, { "to_565", 0x10005d20, 4, 0, 0 },
    { "light_24", 0x10005e50, 4, 0, 0 }, { "light_32", 0x10005f00, 4, 0, 0 }, { "mask_make", 0x10005fb0, 1, 0, 1 },
    { "copy_8", 0x10006080, 2, 0, 0 }, { "surface_convert", 0x100060d0, 4, 0, 0 }, { "clip", 0x100061b0, 10, 0, 1 },
    { "edges_24", 0x100062f0, 3, 0, 0 }, { "blit", 0x10006dc0, 6, 0, 0 }, { "compose", 0x10006f20, 4, 0, 0 },
    { "dib_new", 0x100070c0, 5, 0, 1 }, { "surface_over", 0x10007300, 5, 0, 1 },
    { "surface_over_free", 0x100073e0, 1, 0, 1 }, { "bitmap_size", 0x10001d80, 3, 1, 0 },
    { "bitmap_load", 0x10001e20, 1, 1, 1 }, { "bounds_build", 0x100032f0, 1, 1, 1 },
    { "var_lookup", 0, 0, 0, 0 },
    { "sfb_resource", 0x10005660, 8, 1, 1 },
    { "sfb_panel", 0x10005660, 8, 1, 1 },
    { "replay", 0x10005660, 8, 1, 1 },
};

static void **ours_unit;
static uint32_t unit_ret;

/* the CD's own bitmaps: every RT_BITMAP of every gauge, found once */
struct res {
    uint32_t info;              /* LockResource's: the DIB, in the mapped file */
};
static struct res *ures;
static unsigned nures, cures;

static BOOL CALLBACK enum_bitmap(HMODULE h, LPCSTR type, LPSTR name, LONG_PTR arg)
{
    HRSRC r = FindResourceA(h, name, type);
    HGLOBAL g = r ? LoadResource(h, r) : NULL;
    void *p = g ? LockResource(g) : NULL;

    (void)arg;
    if (p && nures < cures) {
        const uint8_t *q = p;

        if (*(const uint32_t *)q == 40 && (*(const uint16_t *)(q + 14) == 8 || *(const uint16_t *)(q + 14) == 24))
            ures[nures++].info = (uint32_t)(uintptr_t)p;
    }
    return TRUE;
}

static void load_resources(void)
{
    WIN32_FIND_DATAA fd;
    char pat[300], path[400];
    HANDLE f;

    cures = 4096;
    ures = malloc(cures * sizeof *ures);
    snprintf(pat, sizeof pat, "%s\\Gauges\\*.GAU", GAME_DIR);
    f = FindFirstFileA(pat, &fd);
    if (f == INVALID_HANDLE_VALUE)
        return;
    do {
        HMODULE h;

        snprintf(path, sizeof path, "%s\\Gauges\\%s", GAME_DIR, fd.cFileName);
        h = LoadLibraryExA(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (h)
            EnumResourceNamesA(h, RT_BITMAP, enum_bitmap, 0);
    } while (FindNextFileA(f, &fd));
    FindClose(f);
}

static uint32_t call_args(uint32_t fn, int n, const uint32_t *a)
{
    uint32_t ret;

    __asm__ volatile(
        "movl %%esp, %%ebx\n\t"
        "movl %%edx, %%ecx\n\t"
        "1:\n\t"
        "testl %%ecx, %%ecx\n\t"
        "jz 2f\n\t"
        "pushl -4(%%esi,%%ecx,4)\n\t"
        "decl %%ecx\n\t"
        "jmp 1b\n\t"
        "2:\n\t"
        "call *%%edi\n\t"
        "movl %%ebx, %%esp\n\t"
        : "=a"(ret)
        : "D"(fn), "S"(a), "d"(n)
        : "ebx", "ecx", "memory", "cc");
    return ret;
}

static uint32_t u_block_kind(uint32_t n, int kind)
{
    void *p = arena_get(n, kind);

    if (!p)
        return 0;
    memset(p, 0, n);
    return (uint32_t)(uintptr_t)p;
}

static uint32_t u_block(uint32_t n)
{
    return u_block_kind(n, K_HEAP);
}

static void u_fill_pixels(uint32_t rec, int mode)
{
    uint32_t *r = (uint32_t *)(uintptr_t)rec, y, x;
    unsigned bpp = bytes_per_pixel[r[0]];

    for (y = 0; y < r[2]; y++) {
        uint8_t *row = (uint8_t *)(uintptr_t)((uint32_t *)(uintptr_t)r[4])[y];

        for (x = 0; x < r[1]; x++) {
            uint8_t *px = row + x * bpp;
            unsigned k;
            unsigned c = rnd() % 16;

            for (k = 0; k < bpp; k++)
                px[k] = (uint8_t)rnd();
            if (mode == 0)
                continue;
            if (bpp == 1) {
                if (c < 3) px[0] = (uint8_t)c;
            } else if (bpp == 3 || bpp == 4) {
                if (c < 4) px[0] = px[1] = px[2] = 0;
                else if (c < 6) px[0] = px[1] = px[2] = 1;
                else if (c < 8) { px[0] = px[1] = px[2] = (uint8_t)(rnd() % 3); }
                if (bpp == 4) px[3] = 0;
            } else {
                if (c < 3) px[0] = px[1] = 0;
            }
        }
    }
}

static uint32_t u_surface(int type, int w, int h, int mode)
{
    uint32_t rec = u_block(0x20);

    if (!rec || surface_make(rec, type, w, h) != 0)
        return 0;
    u_fill_pixels(rec, mode);
    return rec;
}

/* a DIB in memory: its header, palette and bits */
static uint32_t u_bitmap(int bpp, int w, int h, uint32_t *bits)
{
    uint32_t stride = (((uint32_t)w * (uint32_t)bpp / 8) + 3) & ~3u, ncol = bpp == 8 ? 256 : 0, total;
    uint8_t *p;
    uint32_t i;

    total = 40 + ncol * 4 + stride * (uint32_t)h;
    p = (uint8_t *)(uintptr_t)u_block_kind(total, K_PIXELS);
    if (!p)
        return 0;
    ((uint32_t *)p)[0] = 40;
    ((uint32_t *)p)[1] = (uint32_t)w;
    ((uint32_t *)p)[2] = (uint32_t)h;
    ((uint16_t *)p)[6] = 1;
    ((uint16_t *)p)[7] = (uint16_t)bpp;
    ((uint32_t *)p)[5] = stride * (uint32_t)h;
    for (i = 0; i < ncol; i++) {
        uint8_t *e = p + 40 + 4 * i;
        unsigned c = rnd() % 12;

        e[0] = (uint8_t)rnd();
        e[1] = (uint8_t)rnd();
        e[2] = (uint8_t)rnd();
        if (c < 2) e[0] = e[1] = e[2] = 0;
        else if (c < 4) e[0] = e[1] = e[2] = 1;
        else if (c < 6) e[0] = e[1] = e[2] = (uint8_t)(rnd() % 40);
    }
    p[40] = p[41] = p[42] = 0;
    if (rnd() % 2) {
        p[44] = p[45] = p[46] = 1;
    }
    for (i = 0; i < stride * (uint32_t)h; i++) {
        unsigned c = rnd() % 10;

        p[40 + ncol * 4 + i] = c < 2 ? (uint8_t)(rnd() % 3) : (uint8_t)rnd();
    }
    if (bits)
        *bits = (uint32_t)(uintptr_t)p + 40 + ncol * 4;
    return (uint32_t)(uintptr_t)p;
}

static uint32_t u_point(void)
{
    uint32_t p = u_block(8);

    if (p) {
        ((int32_t *)(uintptr_t)p)[0] = (int32_t)(rnd() % 80) - 12;
        ((int32_t *)(uintptr_t)p)[1] = (int32_t)(rnd() % 80) - 12;
    }
    return p;
}

static int u_range(int lo, int hi)
{
    return lo + (int)(rnd() % (uint32_t)(hi - lo + 1));
}

/* the arguments for one case of routine `fn`, made in the arena */
static int u_setup(int fn, uint32_t *a)
{
    int w, h, dw, dh, t;
    uint32_t bits = 0, info, out, back, k;

    switch (fn) {
    case U_SCALE_24: case U_SCALE_8: case U_SMOOTH_8: case U_SMOOTH_24: case U_SHRINK_24:
    case U_SHRINK_8: {
        int eight = fn == U_SCALE_8 || fn == U_SMOOTH_8 || fn == U_SHRINK_8;
        int big = rnd() % 6 == 0;           /* the smoothers' weights are the output's coordinates */

        w = u_range(1, big ? 500 : 70);
        h = u_range(1, big ? 400 : 70);
        dw = u_range(1, big ? 600 : 70);
        dh = u_range(1, big ? 400 : 70);
        info = u_bitmap(eight ? 8 : 24, w, h, &bits);
        a[0] = info;
        a[1] = bits;
        a[2] = u_surface(4, dw, dh, 0);
        return info && a[2];
    }
    case U_SHRINK_8_TO_8: case U_SHRINK_8_TO_8_PLAIN: {
        int big = rnd() % 6 == 0;

        w = u_range(1, big ? 700 : 90);
        h = u_range(1, big ? 400 : 90);
        dw = u_range(1, big ? 500 : 60);
        dh = u_range(1, big ? 300 : 60);
        info = u_bitmap(8, w, h, &bits);
        a[0] = info;
        a[1] = bits;
        a[2] = u_surface(1, dw, dh, 0);
        if (fn == U_SHRINK_8_TO_8_PLAIN) {
            a[3] = rnd() % 8;
        } else {
            a[3] = rnd() % 3 ? u_surface(1, u_range(10, big ? 700 : 100), u_range(10, big ? 450 : 100), 0) : 0;
            a[4] = u_point();
            a[5] = rnd() % 8;
        }
        return info && a[2];
    }
    case U_SURFACE_FROM_BITMAP: {
        int bpp = rnd() % 2 ? 8 : 24;
        int big = rnd() % 6 == 0;

        w = u_range(1, big ? 500 : 70);
        h = u_range(1, big ? 400 : 70);
        dw = u_range(1, big ? 500 : 60);
        dh = u_range(1, big ? 300 : 60);
        info = u_bitmap(bpp, w, h, &bits);
        out = u_block(8);
        back = rnd() % 3 ? u_surface(bpp == 8 ? 1 : 4, u_range(10, big ? 700 : 100), u_range(10, big ? 450 : 100), 0) : 0;
        a[0] = info;
        a[1] = rnd() % 2 ? bits : 0;
        a[2] = (uint32_t)dw;
        a[3] = (uint32_t)dh;
        a[4] = out;
        a[5] = back;
        a[6] = u_point();
        a[7] = rnd() % 8;
        *(uint32_t *)((uint8_t *)cur->imports + 0x16568) = rnd() % 4;       /* IMAGE_QUALITY */
        return info && out;
    }
    case U_TO_8: case U_TO_8_HOST: case U_COPY_8:
        w = u_range(1, 40);
        h = u_range(1, 40);
        a[0] = u_surface(fn == U_COPY_8 ? 1 : 4, w, h, 1);
        a[1] = u_surface(1, w + (fn == U_COPY_8 ? (int)(rnd() % 3) - 1 : 0), h, 0);
        return a[0] && a[1];
    case U_TO_555: case U_TO_565: case U_LIGHT_24: case U_LIGHT_32: {
        int dt = fn == U_TO_555 ? 2 : fn == U_TO_565 ? 3 : fn == U_LIGHT_24 ? 4 : 5;

        w = u_range(1, 40);
        h = u_range(1, 40);
        a[0] = u_surface(4, w, h, 1);
        a[1] = u_surface(dt, w, h, 0);
        a[2] = u_surface(1, w + 20, h + 20, 0);
        a[3] = u_point();
        ((int32_t *)(uintptr_t)a[3])[0] &= 15;
        ((int32_t *)(uintptr_t)a[3])[1] &= 15;
        return a[0] && a[1] && a[2] && a[3];
    }
    case U_MASK_MAKE: {
        uint32_t pair = u_block(8);

        w = u_range(1, 40);
        h = u_range(1, 40);
        t = rnd() % 2 ? 1 : 4;
        k = u_surface(t, w, h, 1);
        ((uint32_t *)(uintptr_t)pair)[1] = k;
        ((uint32_t *)(uintptr_t)pair)[0] = rnd() % 4 ? u_surface(1, w, h, 0) : k;
        a[0] = pair;
        return pair && k;
    }
    case U_SURFACE_CONVERT: {
        uint32_t pair = u_block(8), e = u_block(0x40);

        w = u_range(1, 40);
        h = u_range(1, 40);
        t = u_range(1, 5);
        ((uint32_t *)(uintptr_t)pair)[1] = u_surface(rnd() % 3 == 0 ? 1 : 4, w, h, 1);
        ((uint32_t *)(uintptr_t)pair)[0] = rnd() % 8 ? u_surface(t, w, h, 0) : 0;
        ((uint32_t *)(uintptr_t)e)[4] = u_surface(1, w + 20, h + 20, 0);
        a[0] = e;
        a[1] = pair;
        a[2] = u_point();
        ((int32_t *)(uintptr_t)a[2])[0] &= 15;
        ((int32_t *)(uintptr_t)a[2])[1] &= 15;
        a[3] = rnd() % 8;
        return pair && e;
    }
    case U_CLIP: {
        uint32_t *v = (uint32_t *)(uintptr_t)u_block(24);

        if (!v)
            return 0;
        for (k = 0; k < 6; k++)
            v[k] = (uint32_t)u_range(-40, 90);
        for (k = 0; k < 6; k++)
            a[k] = (uint32_t)(uintptr_t)&v[k];
        a[6] = (uint32_t)u_range(1, 80);
        a[7] = (uint32_t)u_range(1, 80);
        a[8] = (uint32_t)u_range(1, 80);
        a[9] = (uint32_t)u_range(1, 80);
        return 1;
    }
    case U_EDGES_24:
        w = u_range(1, 40);
        h = u_range(1, 40);
        a[0] = u_surface(4, w, h, 1);
        a[1] = u_surface(4, u_range(10, 80), u_range(10, 80), 0);
        a[2] = u_point();
        return a[0] && a[1] && a[2];
    case U_BLIT: case U_COMPOSE: {
        int ty = u_range(1, 5);

        a[0] = u_surface(ty, u_range(1, 50), u_range(1, 50), 1);
        a[1] = u_surface(rnd() % 8 ? ty : u_range(1, 5), u_range(1, 50), u_range(1, 50), 1);
        if (fn == U_BLIT) {
            uint32_t sz = u_block(8);

            a[2] = u_point();
            a[3] = u_point();
            ((int32_t *)(uintptr_t)sz)[0] = u_range(1, 60);
            ((int32_t *)(uintptr_t)sz)[1] = u_range(1, 60);
            a[4] = sz;
            a[5] = rnd() % 4;
        } else {
            a[2] = u_surface(ty, u_range(1, 50), u_range(1, 50), 1);
            a[3] = u_point();
        }
        return a[0] && a[1];
    }
    case U_DIB_NEW:
        a[0] = rnd() % 6;           /* past 5 retail writes through an uninitialised pointer */
        a[1] = (uint32_t)u_range(1, 40);
        a[2] = (uint32_t)u_range(1, 40);
        a[3] = u_block(4);
        a[4] = rnd() % 8;
        return a[3];
    case U_SURFACE_OVER: {
        uint32_t cell = u_block(4), out2 = u_block(4);

        t = u_range(0, 6);
        w = u_range(1, 40);
        h = u_range(1, 40);
        *(uint32_t *)(uintptr_t)cell = (uint32_t)(uintptr_t)u_block_kind((uint32_t)(w * 4 * h), K_PIXELS);
        a[0] = out2;
        a[1] = (uint32_t)w;
        a[2] = (uint32_t)h;
        a[3] = (uint32_t)t;
        a[4] = cell;
        return cell && out2;
    }
    case U_SURFACE_OVER_FREE: {
        uint32_t cell = u_block(4), out2 = u_block(4);

        *(uint32_t *)(uintptr_t)cell = (uint32_t)(uintptr_t)u_block(4096);
        ((uint32_t (STDCALL *)(uint32_t, int32_t, int32_t, int32_t, uint32_t))
             (uintptr_t)(cur->name[0] == 'r' ? (uint32_t)(uintptr_t)cur->h + (ufns[U_SURFACE_OVER].retail - 0x10000000u)
                                              : (uint32_t)(uintptr_t)ours_unit[U_SURFACE_OVER]))(
            out2, u_range(1, 20), u_range(1, 20), u_range(0, 5), cell);
        a[0] = *(uint32_t *)(uintptr_t)out2;
        return out2 != 0;
    }
    case U_BITMAP_SIZE: case U_BITMAP_LOAD: {
        static const char *const files[] = {
            "Aircraft\\C182\\panel\\cessna182.bmp", "Aircraft\\C182\\panel\\MINICONTROLS.BMP",
            "Aircraft\\C182\\panel\\182S_COMPASS.BMP", "Aircraft\\B737_400\\panel\\Minicontrols.bmp",
            "Aircraft\\B737_400\\panel\\737_compass.bmp", "Aircraft\\Lear45\\panel\\Lear45.bmp",
            "Aircraft\\nothere.bmp"
        };
        char *path = (char *)(uintptr_t)u_block(260);

        if (!path)
            return 0;
        snprintf(path, 260, "%s\\%s", GAME_DIR, files[rnd() % (sizeof files / sizeof files[0])]);
        a[0] = (uint32_t)(uintptr_t)path;
        if (fn == U_BITMAP_SIZE) {
            a[1] = u_block(4);
            a[2] = u_block(4);
            *(uint32_t *)(uintptr_t)a[1] = *(uint32_t *)(uintptr_t)a[2] = 0xa5a5a5a5u;
        }
        return 1;
    }
    case U_BOUNDS_BUILD:
        a[0] = u_surface(rnd() % 2 ? 1 : 4, u_range(1, 40), u_range(1, 40), 1);
        return a[0];
    case U_SFB_RESOURCE: {
        uint32_t inf = ures[rnd() % nures].info;
        int sw = (int)*(uint32_t *)(uintptr_t)(inf + 4), sh = (int)*(uint32_t *)(uintptr_t)(inf + 8);
        int bpp = *(uint16_t *)(uintptr_t)(inf + 14), scale = u_range(20, 130);

        a[0] = inf;
        a[1] = 0;
        a[2] = (uint32_t)(sw * scale / 100 > 0 ? sw * scale / 100 : 1);
        a[3] = (uint32_t)(sh * scale / 100 > 0 ? sh * scale / 100 : 1);
        a[4] = u_block(8);
        a[5] = rnd() % 3 ? u_surface(bpp == 8 ? 1 : 4, u_range(100, 400), u_range(100, 300), 0) : 0;
        a[6] = u_point();
        a[7] = rnd() % 8;
        *(uint32_t *)((uint8_t *)cur->imports + 0x16568) = rnd() % 4;
        return a[4] != 0;
    }
    case U_SFB_PANEL: {
        static char files[64][300];
        static int nfiles;
        FILE *f;
        uint32_t size, off;
        uint8_t *buf;
        int sw, sh, scale, bpp;

        if (!nfiles) {
            static const char *const dirs[] = { "B737_400", "Bell206B", "C182", "Extra300", "Lear45",
                                                "Schweizr", "Sopcamel" };
            unsigned d;

            for (d = 0; d < 7; d++) {
                WIN32_FIND_DATAA fd;
                char pat[300];
                HANDLE h;

                snprintf(pat, sizeof pat, "%s\\Aircraft\\%s\\panel\\*.bmp", GAME_DIR, dirs[d]);
                h = FindFirstFileA(pat, &fd);
                if (h == INVALID_HANDLE_VALUE)
                    continue;
                do
                    if (nfiles < 64)
                        snprintf(files[nfiles++], 300, "%s\\Aircraft\\%s\\panel\\%s", GAME_DIR, dirs[d],
                                 fd.cFileName);
                while (FindNextFileA(h, &fd));
                FindClose(h);
            }
        }
        f = fopen(files[rnd() % (unsigned)nfiles], "rb");
        if (!f)
            return 0;
        fseek(f, 0, SEEK_END);
        size = (uint32_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = (uint8_t *)(uintptr_t)u_block_kind(size, K_PIXELS);
        if (!buf || fread(buf, 1, size, f) != size) {
            fclose(f);
            return 0;
        }
        fclose(f);
        off = *(uint32_t *)(buf + 10);
        a[0] = (uint32_t)(uintptr_t)buf + 14;
        a[1] = (uint32_t)(uintptr_t)buf + off;
        sw = (int)*(uint32_t *)(buf + 18);
        sh = (int)*(uint32_t *)(buf + 22);
        bpp = *(uint16_t *)(buf + 28);
        scale = u_range(15, 110);
        a[2] = (uint32_t)(sw * scale / 100 > 0 ? sw * scale / 100 : 1);
        a[3] = (uint32_t)(sh * scale / 100 > 0 ? sh * scale / 100 : 1);
        a[4] = u_block(8);
        a[5] = rnd() % 3 ? u_surface(bpp == 8 ? 1 : 4, u_range(100, 400), u_range(100, 300), 0) : 0;
        a[6] = u_point();
        a[7] = rnd() % 8;
        *(uint32_t *)((uint8_t *)cur->imports + 0x16568) = rnd() % 4;
        return a[4] != 0;
    }
    case U_REPLAY: {
        /* a surface_from_bitmap call and the state it ran in, written by a
         * debugging build of PANELS: replay.bin */
        FILE *f = fopen("replay.bin", "rb");
        uint32_t hdr[16], total, r, rb;
        uint8_t *info;
        uint8_t blob[768];

        if (!f)
            return 0;
        if (fread(hdr, 4, 16, f) != 16)
            return 0;
        total = hdr[0];
        info = (uint8_t *)(uintptr_t)u_block_kind(total, K_PIXELS);
        fread(info, 1, total, f);
        a[0] = (uint32_t)(uintptr_t)info;
        a[1] = hdr[1] ? a[0] + hdr[1] : 0;
        a[2] = hdr[2];
        a[3] = hdr[3];
        a[4] = u_block(8);
        a[5] = 0;
        if (hdr[5]) {
            a[5] = u_surface((int)hdr[9], (int)hdr[10], (int)hdr[11], 2);
            rb = bytes_per_pixel[hdr[9]] * hdr[10];
            for (r = 0; r < hdr[11]; r++)
                fread((void *)(uintptr_t)((uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)a[5])[4])[r], 1, rb, f);
        }
        a[6] = u_block(8);
        ((uint32_t *)(uintptr_t)a[6])[0] = hdr[6];
        ((uint32_t *)(uintptr_t)a[6])[1] = hdr[7];
        a[7] = hdr[4];
        {
            static uint8_t data[IMAGE_SIZE];

            fread(data, 1, IMAGE_SIZE, f);
            memcpy((uint8_t *)cur->imports + 0x5b8, data + 0x5b8, 0x50);
            memcpy((uint8_t *)cur->imports + 0x1ee0, data + 0x1ee0, 0x10000);
            memcpy((uint8_t *)cur->imports + 0x16568, &hdr[8], 4);
        }
        fread(hg2d.palette, 1, 768, f);
        fread(hg2d.remap, 1, 512, f);
        fread(hg2d.colour, 1, 0x800, f);
        fread(hg2d.realised, 1, 768, f);
        (void)blob;
        fclose(f);
        return a[4] != 0;
    }
    case U_VAR_LOOKUP:
        a[0] = u_block(0x20);
        if (!a[0])
            return 0;
        *(uint32_t *)(uintptr_t)a[0] = 1 + rnd() % 380;
        a[1] = rnd() % 8 ? 1 : 0;
        return 1;
    }
    return 0;
}

static void run_unit(struct side *s, int fn, unsigned seed)
{
    uint32_t args[10];
    static struct scenario sc;
    uint32_t target;
    int ok;

    select_side(s);
    memcpy(s->imports, s->pristine, IMAGE_SIZE);
    free_gauges();
    gdi_reset();
    nregions = 4;
    tlen = 0;
    if (tlog)
        tlog[0] = 0;
    toverflow = 0;
    faulted = 0;
    make_scenario(&sc, seed);
    sc.fail_alloc = sc.fail_surface = 0;
    world_reset(&sc);
    rstate = (seed * 2654435761u) | 1;
    memset(args, 0, sizeof args);
    ok = u_setup(fn, args);
    put("UNIT %s seed %u", ufns[fn].name, seed);
    if (!ok) {
        put(" (no setup)");
        return;
    }
    if (fn == U_VAR_LOOKUP) {
        uint32_t *v = (uint32_t *)(uintptr_t)args[0];

        __asm__ volatile("movl %%esp, %0" : "=m"(saved_esp));
        if (__builtin_setjmp(fault_jb) == 0) {
            in_module = 1;
            ((f1_t)LK(0x98))(args[0]);
            if (args[1] & 1)
                ((f1_t)LK(0xa0))(args[0]);
            put(" v=%x,%x,%x,%x,%x", v[0], v[1], v[2], v[3], v[4]);
        }
        in_module = 0;
        put_state();
        free_gauges();
        gdi_reset();
        return;
    }
    target = s == &sides[0] ? (uint32_t)(uintptr_t)s->h + (ufns[fn].retail - 0x10000000u)
                            : (uint32_t)(uintptr_t)ours_unit[fn == U_SFB_RESOURCE || fn == U_SFB_PANEL || fn == U_REPLAY ? U_SURFACE_FROM_BITMAP : fn];
    __asm__ volatile("movl %%esp, %0" : "=m"(saved_esp));
    if (__builtin_setjmp(fault_jb) == 0) {
        in_module = 1;
        unit_ret = call_args(target, ufns[fn].nargs, args);
        if (ufns[fn].has_ret)
            put(" ret=%x", unit_ret);
    }
    in_module = 0;
    put_state();
    if (getenv("PANELS_ARENA")) {
        char name[64];
        FILE *f;

        snprintf(name, sizeof name, "arena_%s.bin", s->name);
        f = fopen(name, "wb");
        if (f) {
            fwrite(arena, 1, arena_used, f);
            fclose(f);
        }
    }
    free_gauges();
    gdi_reset();
}

/* ---- a watchdog, and main -------------------------------------------------------------- */

static HANDLE main_thread;

/* no progress for a while: say where the main thread is, and stop */
static DWORD WINAPI watchdog(LPVOID arg)
{
    (void)arg;
    for (;;) {
        Sleep(1000);
        if (GetTickCount() - last_progress > 60000) {
            CONTEXT c;

            memset(&c, 0, sizeof c);
            c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            SuspendThread(main_thread);
            GetThreadContext(main_thread, &c);
            char msg[1200];
            DWORD wrote;
            int len = snprintf(msg, sizeof msg,
                "\nHANG in scenario %u (no progress for 60 s) on %s at eip %#lx (region %d%+d, text %#x)"
                " eax %lx ecx %lx edx %lx esi %lx edi %lx ebp %lx esp %lx\ntranscript ends: ...%s\n",
                scenario_no, cur_name, c.Eip, region_of(c.Eip),
                region_of(c.Eip) >= 0 ? (int)(c.Eip - regions[region_of(c.Eip)].lo) : 0, cur_text_lo,
                c.Eax, c.Ecx, c.Edx, c.Esi, c.Edi, c.Ebp, c.Esp,
                tlog && tlen > 400 ? tlog + tlen - 400 : "");

            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)len, &wrote, NULL);
            TerminateProcess(GetCurrentProcess(), 4);
        }
    }
    return 0;
}

static unsigned long cover[16];
static const char *const cover_what[16] = {
    "AIRCRAFT", "modload(", "=ok mod", "FAULT", "wcreate(", "tpolygon(", "surfnew(", "untexture(",
    "SITUATION", "MOUSE", "NAMED", "PAINT", "texture(", "main.write", "listwalk(", "refresh"
};

static void count_cover(const char *log)
{
    unsigned w;

    for (w = 0; w < 16; w++) {
        const char *p = log;

        while ((p = strstr(p, cover_what[w])) != NULL) {
            cover[w]++;
            p++;
        }
    }
}

/* Retail faults on what it does not check (an allocation that fails and is
 * used); OpenFS98 does not reproduce a crash.  Up to retail's fault the two
 * must agree, and what follows is nothing to compare. */
static int logs_differ(const char *rlog, const char *ours)
{
    const char *f = strstr(rlog, " FAULT(");

    if (f && !getenv("PANELS_STRICT"))
        return strncmp(rlog, ours, (size_t)(f - rlog)) != 0;
    return strcmp(rlog, ours) != 0;
}

static void report_diff(const char *a, const char *b, unsigned v)
{
    size_t at = 0;
    const char *p;

    while (a[at] && a[at] == b[at])
        at++;
    printf("scenario %u differs at byte %zu:\n", v, at);
    p = a + (at > 300 ? at - 300 : 0);
    printf("  retail ...%.600s\n", p);
    p = b + (at > 300 ? at - 300 : 0);
    printf("  ours   ...%.600s\n", p);
}

int main(int argc, char **argv)
{
    char *rlog = NULL;
    static struct scenario sc;
    unsigned n = argc > 1 ? (unsigned)atoi(argv[1]) : 200, start = argc > 2 ? (unsigned)atoi(argv[2]) : 0;
    unsigned v, bad = 0, i, failed[64];
    const char *only = getenv("PANELS_SCENARIO");
    int unit_mode;
    const char *only_fn = getenv("PANELS_UNIT");

    _controlfp(_PC_53, _MCW_PC);
    arena = VirtualAlloc(NULL, ARENA_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    blks = malloc(MAXBLKS * sizeof *blks);
    dib_page = calloc(1 << 20, sizeof *dib_page);
    if (!arena || !blks) {
        printf("cannot map the arena\n");
        return 2;
    }
    arena_base = (uint32_t)(uintptr_t)arena;
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    SetUnhandledExceptionFilter(on_unhandled);
    AddVectoredExceptionHandler(1, on_vectored);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    build_tables();

    sides[0].name = "retail";
    sides[1].name = "ours";
    unit_mode = argc > 1 && strcmp(argv[1], "unit") == 0;
    if (unit_mode) {
        argc--;
        argv++;
        n = argc > 1 ? (unsigned)atoi(argv[1]) : 200;
        start = argc > 2 ? (unsigned)atoi(argv[2]) : 0;
    }
    if (!load_side(&sides[0], "PANELS_retail.dll") ||
        !load_side(&sides[1], unit_mode ? "PANELS_unit.dll" : "PANELS_ours.dll"))
        return 2;
    if (unit_mode) {
        load_resources();
        printf("%u bitmaps of the gauges\n", nures);
        ours_unit = (void **)GetProcAddress(sides[1].h, "panels_unit");
        if (!ours_unit) {
            printf("PANELS_unit.dll has no panels_unit table\n");
            return 2;
        }
    }
    arm_cover();
    for (i = 0; i < 2; i++) {
        set_links(sides[i].imports);
        memcpy(sides[i].pristine, sides[i].imports, IMAGE_SIZE);
    }
    if (only) {
        start = (unsigned)atoi(only);
        n = 1;
    }
    if (unit_mode) {
        int fn;
        unsigned seeds = n;

        for (fn = 0; fn < U_N; fn++) {
            unsigned nbad = 0, first = 0;

            if (only_fn && strcmp(only_fn, ufns[fn].name) != 0)
                continue;
            for (v = start; v < start + seeds; v++) {
                last_progress = GetTickCount();
                scenario_no = v;
                detail = 0;
                run_unit(&sides[0], fn, v);
                free(rlog);
                rlog = strdup(tlog);
                run_unit(&sides[1], fn, v);
                if (logs_differ(rlog, tlog)) {
                    if (nbad++ == 0)
                        first = v;
                }
            }
            printf("%-22s %u cases, %u differ%s", ufns[fn].name, seeds, nbad, nbad ? "" : "\n");
            if (nbad) {
                printf(" (first: seed %u)\n", first);
                if (only_fn) {
                    detail = 1;
                    run_unit(&sides[0], fn, first);
                    free(rlog);
                    rlog = strdup(tlog);
                    run_unit(&sides[1], fn, first);
                    report_diff(rlog, tlog, first);
                    if (getenv("PANELS_DUMP")) {
                        FILE *f = fopen("panels_ours_transcript.txt", "w");

                        if (f) {
                            fputs(tlog, f);
                            fclose(f);
                        }
                        f = fopen("panels_retail_transcript.txt", "w");
                        if (f) {
                            fputs(rlog, f);
                            fclose(f);
                        }
                    }
                }
            }
            bad += nbad > 0;
        }
        printf("%u routines differ\n", bad);
        return bad != 0;
    }
    for (v = start; v < start + n; v++) {
        make_scenario(&sc, v);
        scenario_no = v;
        last_progress = GetTickCount();
        if (getenv("PANELS_PROGRESS")) {
            printf("scenario %u\n", v);
            fflush(stdout);
        }
        detail = 0;
        run_side(&sides[0], &sc);
        count_cover(tlog);
        free(rlog);
        rlog = strdup(tlog);
        run_side(&sides[1], &sc);
        if (logs_differ(rlog, tlog)) {
            if (bad < 64)
                failed[bad] = v;
            bad++;
            if (bad <= 3) {
                report_diff(rlog, tlog, v);
                detail = 1;
                run_side(&sides[0], &sc);
                free(rlog);
                rlog = strdup(tlog);
                run_side(&sides[1], &sc);
                if (logs_differ(rlog, tlog)) {
                    printf("-- in detail:\n");
                    report_diff(rlog, tlog, v);
                }
            }
        }
        if (getenv("PANELS_DUMP")) {
            FILE *f = fopen("panels_ours_transcript.txt", "w");

            if (f) {
                fputs(tlog, f);
                fclose(f);
            }
            f = fopen("panels_retail_transcript.txt", "w");
            if (f) {
                fputs(rlog, f);
                fclose(f);
            }
        }
    }
    if (cover_on) {
        printf("functions never reached:");
        for (i = 0; i < sizeof retail_funcs / sizeof retail_funcs[0]; i++)
            if (!cover_hit[i])
                printf(" %x", retail_funcs[i]);
        printf("\n");
    }
    printf("covered:");
    for (i = 0; i < 16; i++)
        printf(" %s %lu,", cover_what[i], cover[i]);
    printf("\n%u scenarios, %u differ", n, bad);
    for (i = 0; i < bad && i < 64; i++)
        printf("%s%u", i ? " " : ": ", failed[i]);
    printf("\n");
    if (!bad)
        printf("IDENTICAL\n");
    return bad != 0;
}
