/* Differential test: retail ANALYSIS.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven through Linkage and the
 * three CHAIN callbacks they register (list 5 every frame, 0xc for commands,
 * 0xe to draw the trail), the way MAIN, CHAIN and G3D drive them, one side
 * after the other over the same script.  What ANALYSIS imports:
 *
 *   host      a stub over a bump arena reset before each side, so heap
 *             pointers come out the same; unzeroed blocks filled with 0xa5,
 *             freed ones with 0xdd.  The dialog engine (+0x10c) is a
 *             stand-in for the three dialogs: the two graphs' controls are
 *             attached, painted and let go, and what they show is read back;
 *             the options' records are shown, edited as a user might,
 *             checked, and answered from the script.  +0x74 gives a graph's
 *             record a size and a surface, as the host's 0x467 handler
 *             would.  memset and memmove leave ecx as the real ones do --
 *             0, and the CD MSVCRT's 7 -- because retail reads it.
 *   CHAIN     add and remove, logged; add leaves the list's new count in
 *             ecx, as retail CHAIN does.  The callbacks are called with a
 *             chosen ecx: mostly 0, which is what retail CHAIN passes for
 *             MAIN's list 5 (its first argument), sometimes not.
 *   UTIL      retail's, its state restored per side: +0x64, +0x48 and the
 *             48-bit conversions, and the exchange (+0x8c, +0x90).
 *   NUMERIC   retail's, its state restored per side, for the texts.
 *   HG2D      the three line services, logged with every argument, the
 *             colour in GLOBAL +0x2ec and the view they draw through.
 *   G3D       +0x28, the trail's display list, logged by its bytes.
 *
 * GLOBAL is random per scenario: the mode and options at +0x814..+0x828,
 * the position at +0x560 moving or standing still frame by frame, the
 * height steered across the landing's two thresholds (+0x572/+0x574 over
 * +0x20, whose bit 7 is the borrow), the ground at +0x366/+0x2b8, crashes
 * at +0x838.  The buffer is filled to the edge of its compaction through
 * +0x30.  Each scenario runs under a random x87 precision control, and the
 * host's divide filter is modelled (a zero range in the landing graph).
 *
 * Compared after every operation: the transcript of every import call (with
 * the strings and bytes they carry), ANALYSIS's whole .data image (code
 * pointers through a pairing learnt as they are first seen, pointers into
 * itself as offsets), the arena, GLOBAL, and UTIL's and NUMERIC's state.
 * Each side's module memory outside the image is digested before and after,
 * and must not change.  A fault ends the scenario on both sides.  A graph
 * dialog opened with no samples ends it too: retail reads an uninitialised
 * frame there (docs/modules/analysis.md).
 *
 * Build: make analysisdiff     Run: make analysisdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x32cu
#define GLOBAL_SIZE  0x1300u
#define ARENA_SIZE   (8u << 20)
#define TRANSCRIPT_SIZE (16u << 20)
#define MAX_OPS      50
#define MAX_PAIRS    48
#define MEMMOVE_ECX  7u             /* the CD's MSVCRT.DLL */

struct import { uint32_t id; void *link; };

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    uint8_t *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    uint32_t list5, list12, list14;
    char *transcript;
    uint32_t length;
    uint32_t outside;
};

/* a retail helper module, and a copy of its writable memory to restore */
struct helper {
    HMODULE module;
    uint8_t *link, *lo, *hi;
    uint8_t *saved[8], *at[8];
    uint32_t size[8], n;
};

static struct side retail, ours;
static struct side *cur;
static struct helper util, numeric;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t chain[0x24], hg2d[0x40], g3d[0x40];
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint32_t rng, scenario_seed;
static uint32_t scenario;
static uint32_t op_number;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs, npairs_ours;
static uint32_t chain_count[0x20];
uint32_t chain_ecx;                 /* what the add stub leaves in ecx */
static uint32_t still;              /* frames the aircraft stands still */

static unsigned long cover_ops[12], cover_frames, cover_samples, cover_compactions,
    cover_landing, cover_manoeuvre, cover_options, cover_trails, cover_lines, cover_divfix,
    cover_faults, cover_poison, cover_wide, cover_wide_sample, cover_mode[6], cover_armed,
    cover_recording, cover_commands, cover_ended, cover_checks, cover_refused, cover_lists[4],
    cover_speed, cover_landed_crash, cover_unsigned, cover_long_list, cover_one;

#define G8(o)  (*(uint8_t *)(global + (o)))
#define G16(o) (*(uint16_t *)(global + (o)))
#define G32(o) (*(uint32_t *)(global + (o)))
#define I32(o) (*(uint32_t *)(cur->image + (o)))
#define I8(o)  (*(uint8_t *)(cur->image + (o)))
#define R32(rec, off) (*(uint32_t *)(uintptr_t)((rec) + (off)))
#define SET_SLOT(table, off, fn) (*(void **)((table) + (off)) = (void *)(fn))

/* ---- utilities ---------------------------------------------------------------- */

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static uint32_t rnd_n(uint32_t n)
{
    return n ? rnd() % n : 0;
}

static uint32_t digest(const void *p, uint32_t n, uint32_t h)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 0x01000193u;
    return h;
}

static void put(const char *format, ...)
{
    va_list args;
    int n;

    if (cur == NULL || cur->length >= TRANSCRIPT_SIZE - 4096)
        return;
    va_start(args, format);
    n = vsnprintf(cur->transcript + cur->length, TRANSCRIPT_SIZE - cur->length,
                  format, args);
    va_end(args);
    if (n > 0)
        cur->length += (uint32_t)n;
}

static int in_arena(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;
    return b >= arena && b < arena + ARENA_SIZE;
}

static int in_image(uint32_t v)
{
    return cur != NULL && v >= (uint32_t)(uintptr_t)cur->image &&
           v < (uint32_t)(uintptr_t)cur->image + IMAGE_SIZE;
}

static int in_module(uint32_t v)
{
    return cur != NULL && v >= (uint32_t)(uintptr_t)cur->lo &&
           v < (uint32_t)(uintptr_t)cur->hi;
}

static uint32_t pair_code(uint32_t v)
{
    uint32_t *list = cur == &retail ? pair_retail : pair_ours;
    uint32_t *n = cur == &retail ? &npairs : &npairs_ours;
    uint32_t i;

    if (v == 0)
        return 0;
    for (i = 0; i < *n; i++)
        if (list[i] == v)
            return 0xc0de0000u | (i + 1);
    if (*n < MAX_PAIRS) {
        list[*n] = v;
        return 0xc0de0000u | ++*n;
    }
    return 0xc0deffffu;
}

static void put_ptr(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;

    if (v == 0)
        put("null");
    else if (in_arena(v))
        put("arena+%#x", (uint32_t)(b - arena));
    else if (in_image(v))
        put("image+%#x", v - (uint32_t)(uintptr_t)cur->image);
    else if (b >= global && b < global + GLOBAL_SIZE)
        put("G+%#x", (uint32_t)(b - global));
    else if (in_module(v))
        put("code%u", pair_code(v) & 0xffff);
    else if (v < 0x10000)
        put("%#x", v);
    else
        put("other");
}

static void put_s(const char *s)
{
    uint32_t i;

    if (s == NULL) {
        put("null");
        return;
    }
    put("'");
    for (i = 0; s[i] && i < 0x100; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c >= 0x20 && c < 0x7f && c != '\\')
            put("%c", c);
        else
            put("\\%02x", c);
    }
    put("'");
}

static void put_bytes(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;

    put("[");
    for (i = 0; i < n && i < 0x40; i++)
        put("%02x", b[i]);
    if (n > 0x40)
        put("..%08x", digest(b, n, 0x811c9dc5u));
    put("]");
}

/* ---- faults ---------------------------------------------------------------------- */

static void *fault_jb[5];
static volatile int in_analysis;
static volatile int faulted;
static int stopped;
static uint32_t fault_at[16];
static unsigned long fault_n[16];

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

/* The host's divide filter (0x4119a0): a faulting div gives all ones in eax
 * and edx and the sim carries on after it.  ANALYSIS divides only by a
 * register, and only unsigned (.text+0x2159, `div esi`). */
static int div_fixup(CONTEXT *c)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;

    if (p[0] != 0xf7 || (p[1] >> 6) != 3 || ((p[1] >> 3) & 7) != 6)
        return 0;
    c->Eax = 0xffffffffu;
    c->Edx = 0xffffffffu;
    c->Eip += 2;
    return 1;
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;

    if (code == EXCEPTION_ACCESS_VIOLATION &&
        (e->ContextRecord->Eip & 0xff000000u) == 0xde000000u) {
        printf("scenario %u op %u %s: called module %lu's unstubbed +%#lx\n", scenario,
               op_number, cur ? cur->name : "none", (e->ContextRecord->Eip >> 16) & 0xff,
               e->ContextRecord->Eip & 0xffff);
        fflush(stdout);
        ExitProcess(5);
    }
    if (!in_analysis || cur == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO && in_module(e->ContextRecord->Eip) &&
        div_fixup(e->ContextRecord)) {
        if (counting) cover_divfix++;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW)
        put(" FAULT(div)");
    else if (code == EXCEPTION_ACCESS_VIOLATION)
        put(" FAULT(av)");
    else
        return EXCEPTION_CONTINUE_SEARCH;
    if (counting) {
        uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
        uint32_t where = ip >= cur->lo && ip < cur->hi ? (uint32_t)(ip - cur->lo - 0x1000)
                         : 0xffffffffu, k;

        cover_faults++;
        if (getenv("ANALYSIS_FAULTS"))
            printf("scenario %u op %u: fault %#lx at %p (.text+%#x) address %p\n", scenario,
                   op_number, code, (void *)ip, where,
                   (void *)e->ExceptionRecord->ExceptionInformation[1]);
        for (k = 0; k < 16 && fault_n[k] && fault_at[k] != where; k++)
            ;
        if (k < 16) {
            fault_at[k] = where;
            fault_n[k]++;
        }
    }
    in_analysis = 0;
    faulted = 1;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* A stop that is not a fault: the scenario ends here on both sides. */
static void stop(const char *why)
{
    put(" STOP(%s)", why);
    stopped = 1;
    in_analysis = 0;
    faulted = 1;
    __builtin_longjmp(fault_jb, 1);
}

/* ---- the arena ------------------------------------------------------------------ */

#define LIVE  0x0a110cu
#define FREED 0xf4eeu

static void *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;

    if (size > ARENA_SIZE || arena_used + n > ARENA_SIZE) {
        printf("arena exhausted (scenario %u)\n", scenario);
        exit(2);
    }
    p = arena + arena_used;
    arena_used += n;
    memset(p, 0, 16);
    *(uint32_t *)p = size;
    *(uint32_t *)(p + 4) = LIVE;
    return p + 16;
}

static void arena_release(void *p)
{
    uint8_t *b = (uint8_t *)p;

    if (in_arena((uint32_t)(uintptr_t)b) && ((uintptr_t)b & 15) == 0 &&
        *(uint32_t *)(b - 12) == LIVE) {
        memset(b, 0xdd, *(uint32_t *)(b - 16));
        *(uint32_t *)(b - 12) = FREED;
    }
}

/* ---- the stub host -------------------------------------------------------------- */

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p = arena_get(size);

    memset(p, zero ? 0 : 0xa5, size);
    put(" alloc(%d,%#x,", fl, size);
    put_ptr((uint32_t)(uintptr_t)out);
    put(",%d)=", zero);
    put_ptr((uint32_t)(uintptr_t)p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    put(" free(");
    put_ptr((uint32_t)(uintptr_t)pp);
    put(":");
    put_ptr((uint32_t)(uintptr_t)*pp);
    put(")");
    if (*pp != NULL)
        arena_release(*pp);
    *pp = NULL;
    return 0;
}

int32_t STDCALL h_memset_c(void *d, int32_t byte, uint32_t n)
{
    put(" memset(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",%#x,%#x)", (uint32_t)byte & 0xff, n);
    if (n != 0)
        memset(d, byte & 0xff, n);
    return 0;
}

int32_t STDCALL h_memmove_c(void *d, const void *s, uint32_t n)
{
    put(" memmove(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_ptr((uint32_t)(uintptr_t)s);
    put(",%#x)", n);
    if (n != 0)
        memmove(d, s, n);
    return 0;
}

/* The host's memset is rep stosd / rep stosb: ecx 0 after.  Its memmove
 * calls MSVCRT's, which for these copies leaves the dword count. */
void h_memset(void);
void h_memmove(void);
__asm__(".text\n"
        "_h_memset:\n\t"
        "pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\t"
        "call _h_memset_c@12\n\t"
        "xorl %ecx, %ecx\n\t"
        "ret $12\n"
        "_h_memmove:\n\t"
        "pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\t"
        "call _h_memmove_c@12\n\t"
        "movl $7, %ecx\n\t"
        "ret $12");

static char *STDCALL h_strcpy(char *d, const char *s)
{
    put(" strcpy(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_s(s);
    put(")");
    return strcpy(d, s);
}

static char *STDCALL h_strcat(char *d, const char *s)
{
    put(" strcat(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_s(s);
    put(")");
    return strcat(d, s);
}

static uint32_t STDCALL h_strlen(const char *s)
{
    uint32_t n = s ? (uint32_t)strlen(s) : 0;
    put(" strlen=%u", n);
    return n;
}

/* +0x74: a dialog control's record given its host block and the size and
 * surface the control's 0x467 handler gives it */
static int32_t STDCALL h_attach(uint32_t hwnd, uint32_t rec)
{
    uint32_t k = rnd_n(16), w, h, size;

    put(" attach(%#x,", hwnd);
    put_ptr(rec);
    put(")");
    if (rec == 0)
        return -1;
    if (k == 0) {
        put("=-1");
        return -1;
    }
    R32(rec, 0xa0) = (uint32_t)(uintptr_t)arena_get(0x90);
    memset((void *)(uintptr_t)R32(rec, 0xa0), 0, 0x90);
    w = k == 1 ? rnd_n(8) : k == 2 ? rnd() : 100 + rnd_n(400);
    h = k == 1 ? rnd_n(8) : k == 3 ? rnd() : 60 + rnd_n(300);
    R32(rec, 0x84) = w;
    R32(rec, 0x88) = h;
    R32(rec, 0x80) = 0x5f5f0000u | rnd_n(0x100);
    size = rnd_n(6) == 0 ? 0 : 1 + rnd_n(0x200);
    R32(rec, 0x94) = size;
    R32(rec, 0x98) = size ? (uint32_t)(uintptr_t)arena_get(size) : 0;
    if (size)
        memset((void *)(uintptr_t)R32(rec, 0x98), 0x5a, size);
    put("{%d,%d,%#x,%#x}", (int32_t)w, (int32_t)h, R32(rec, 0x80), size);
    return 0;
}

static int32_t STDCALL h_release(uint32_t rec)
{
    put(" release(");
    put_ptr(rec);
    put(")");
    if (rec && in_arena(R32(rec, 0x98))) {
        arena_release((void *)(uintptr_t)R32(rec, 0x98));
        R32(rec, 0x98) = 0;
    }
    if (rec && in_arena(R32(rec, 0xa0))) {
        arena_release((void *)(uintptr_t)R32(rec, 0xa0));
        R32(rec, 0xa0) = 0;
    }
    return 0;
}

static int32_t STDCALL h_flush(uint32_t rec)
{
    put(" flush(");
    put_ptr(rec);
    put(")");
    return 0;
}

typedef uint32_t (STDCALL *check_fn)(uint32_t);
typedef uint32_t (STDCALL *control_fn)(uint32_t, uint32_t, uint32_t);

static uint32_t dialog_result(uint8_t *block)
{
    uint32_t a = rnd(), r;

    *(uint32_t *)block = (a >> 20) % 3 != 0;
    switch ((a >> 24) % 6) {
    case 0: case 1: case 2: case 3: r = 0; break;
    case 4: r = 1; break;
    default: r = rnd(); break;
    }
    put(" result=%#x,%u", r, *(uint32_t *)block);
    return r;
}

/* a graph: the control attached, painted, perhaps asked something else, its
 * texts read, let go */
static void graph(uint8_t *block)
{
    uint32_t a = rnd(), r, i, paints;
    uint32_t hwnd = 0x7e570000u | (a & 0xff);

    r = ((check_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
    put(" check=%#x", r);
    r = ((control_fn)*(uint32_t *)(block + 8))(0, 0, hwnd);
    put(" attached=%#x", r);
    paints = (a >> 8) % 3;
    for (i = 0; i < paints; i++) {
        r = ((control_fn)*(uint32_t *)(block + 8))(1, 0, hwnd);
        put(" painted=%#x", r);
    }
    if ((a >> 12) % 8 == 0) {
        r = ((control_fn)*(uint32_t *)(block + 8))(3 + (a >> 16) % 4, 0, hwnd);
        put(" other=%#x", r);
    }
}

static void graph_release(uint8_t *block)
{
    uint32_t r = ((control_fn)*(uint32_t *)(block + 8))(2, 0, 0x7e570000u);
    put(" released=%#x", r);
}

/* +0x10c: the three dialogs */
static int32_t STDCALL h_dialog(uint32_t id, uint8_t *block)
{
    uint32_t i, r;

    put(" dialog(%#x)", id);
    if (id == 0x14050002 || id == 0x14050003) {
        put(" count=%u", I32(0x24));
        if (I32(0x24) == 0) {
            if (counting) cover_poison++;
            stop("no samples");
        }
    }
    if (id == 0x14050002) {
        if (counting) cover_landing++;
        graph(block);
        put(" landed=%u", *(uint32_t *)(block + 0xc));
        if (counting && *(uint32_t *)(block + 0xc)) cover_speed++;
        for (i = 0; i < 4; i++) {
            put(" t%u=", i);
            put_s(*(const char **)(block + 0x10 + 4 * i));
        }
        graph_release(block);
    } else if (id == 0x14050003) {
        if (counting) cover_manoeuvre++;
        put(" units=%u", *(uint32_t *)(block + 0xc));
        graph(block);
        put(" scaled=%u text=", *(uint32_t *)(block + 0x10));
        put_s(*(const char **)(block + 0x14));
        graph_release(block);
    } else if (id == 0x1405) {
        /* the records, then edited as a user might, then checked */
        static const uint32_t values[] = { 0xc, 0x14, 0x1c, 0x24, 0x2c, 0x34, 0x3c };
        if (counting) cover_options++;
        put(" records");
        put_bytes(block + 8, 0x38);
        for (i = 0; i < 7; i++) {
            if (rnd_n(3) != 0)
                continue;
            /* the lists' values index tables in the image: past 0x20 they
             * read wherever the address lands, which differs by side */
            *(uint32_t *)(block + values[i]) =
                i >= 5 ? (rnd_n(16) == 0 ? 0x80000000u | rnd() : rnd_n(8) == 0 ? rnd_n(0x20)
                          : rnd_n(5))
                : rnd_n(8) == 0 ? rnd() : rnd_n(i == 1 ? 4 : 2);
        }
        if (rnd_n(4) != 0) {
            r = ((check_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
            put(" check=%#x", r);
            if (counting) { cover_checks++; if (r) cover_refused++; }
            put_bytes(block + 8, 0x38);
            /* a refused entry keeps the dialog open: OK only now and then,
             * which commits a list value past 2 (retail then reads its
             * tables out of bounds) */
            if (r != 0 && rnd_n(8) != 0) {
                r = dialog_result(block);
                *(uint32_t *)block = 0;
                return (int32_t)r;
            }
        }
        /* a value outside 0..0x1f would index the tables anywhere: cancel */
        if (*(uint32_t *)(block + 0x34) >= 0x20 || *(uint32_t *)(block + 0x3c) >= 0x20) {
            r = dialog_result(block);
            *(uint32_t *)block = 0;
            return (int32_t)r;
        }
    }
    return (int32_t)dialog_result(block);
}

/* ---- the other modules -------------------------------------------------------------- */

static uint32_t *list_slot(uint32_t list)
{
    return list == 5 ? &cur->list5 : list == 0xc ? &cur->list12 : list == 0xe ? &cur->list14
                                                                            : NULL;
}

uint32_t STDCALL chain_add_c(uint32_t list, uint32_t cb)
{
    uint32_t *slot = list_slot(list);

    put(" chainadd(%#x,", list);
    put_ptr(cb);
    put(")");
    if (slot) *slot = cb;
    chain_ecx = ++chain_count[list & 0x1f];
    return 0;
}

/* retail CHAIN's add leaves the list's new count in ecx */
void chain_add(void);
__asm__(".text\n"
        "_chain_add:\n\t"
        "pushl 8(%esp)\n\tpushl 8(%esp)\n\t"
        "call _chain_add_c@8\n\t"
        "movl _chain_ecx, %ecx\n\t"
        "ret $8");

static uint32_t STDCALL chain_remove(uint32_t list, uint32_t cb)
{
    uint32_t *slot = list_slot(list);

    put(" chainremove(%#x,", list);
    put_ptr(cb);
    put(")");
    if (slot && *slot == cb) *slot = 0;
    if (chain_count[list & 0x1f]) chain_count[list & 0x1f]--;
    return 0;
}

/* HG2D draws through the record the surface belongs to (s is its +0x80),
 * clipped to that record's view block (+0x1c): its half-extents. */
static void put_view(uint32_t s)
{
    uint32_t rec = s - 0x80, view;

    if (!in_arena(rec) && !in_image(rec))
        return;
    view = R32(rec, 0x1c);
    if (in_arena(view))
        put(" v%d,%d,%d,%d", (int32_t)R32(view, 0), (int32_t)R32(view, 4),
            (int32_t)R32(view, 8), (int32_t)R32(view, 0xc));
}

static uint32_t STDCALL hg2d_h(uint32_t s, int32_t x1, int32_t x2, int32_t y)
{
    if (counting) cover_lines++;
    put(" lineh(");
    put_ptr(s);
    put(",%d,%d,%d c%#x)", x1, x2, y, G32(0x2ec));
    put_view(s);
    return 0;
}

static uint32_t STDCALL hg2d_v(uint32_t s, int32_t x, int32_t y1, int32_t y2)
{
    if (counting) cover_lines++;
    put(" linev(");
    put_ptr(s);
    put(",%d,%d,%d c%#x)", x, y1, y2, G32(0x2ec));
    put_view(s);
    return 0;
}

static uint32_t STDCALL hg2d_line(uint32_t s, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    if (counting) cover_lines++;
    put(" line(");
    put_ptr(s);
    put(",%d,%d,%d,%d c%#x)", x1, y1, x2, y2, G32(0x2ec));
    put_view(s);
    return 0;
}

/* G3D +0x28: the display list, by its bytes (the block is 0x59e4) */
static uint32_t STDCALL g3d_list(uint8_t *list)
{
    uint32_t n = 0;

    if (counting) cover_trails++;
    put(" g3dlist(");
    put_ptr((uint32_t)(uintptr_t)list);
    put(")");
    if (in_arena((uint32_t)(uintptr_t)list)) {
        while (n + 2 <= 0x59e4 && *(uint16_t *)(list + n) != 0)
            n += *(uint16_t *)(list + n) == 0x2f ? 0x20 : *(uint16_t *)(list + n) == 0xe ? 0xa
               : *(uint16_t *)(list + n) == 0x76 ? 2 : 4;
        if (counting && n > 0x59e4 - 0x40) cover_long_list++;
        put(" %#x", n);
        put_bytes(list, 0x59e4);
    }
    return 0;
}

/* ---- loading -------------------------------------------------------------------- */

static void helper_load(struct helper *h, const char *path, uint32_t id)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    struct import *im;
    uint32_t i;

    h->module = LoadLibraryA(path);
    if (h->module == NULL) { printf("cannot load %s\n", path); exit(2); }
    h->link = (uint8_t *)GetProcAddress(h->module, "Linkage");
    im = (struct import *)GetProcAddress(h->module, "ImportTable");
    if (h->link == NULL || im == NULL || *(uint32_t *)h->link != id) {
        printf("%s: not module %u\n", path, id);
        exit(2);
    }
    for (; im->id; im++) {
        if (im->id == 1) im->link = host;
        else if (im->id == 2) im->link = global;
        else if (im->id == 17) im->link = numeric.link;
        else if (im->id == 20) im->link = util.link;
        else { printf("%s: unexpected import %u\n", path, im->id); exit(2); }
    }
    dos = (IMAGE_DOS_HEADER *)h->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)h->module + dos->e_lfanew);
    h->lo = (uint8_t *)h->module;
    h->hi = h->lo + nt->OptionalHeader.SizeOfImage;
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections && h->n < 8; i++)
        if (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            h->at[h->n] = h->lo + sec[i].VirtualAddress;
            h->size[h->n] = sec[i].Misc.VirtualSize;
            h->saved[h->n] = malloc(h->size[h->n]);
            h->n++;
        }
}

static void helper_save(struct helper *h)
{
    uint32_t i;
    for (i = 0; i < h->n; i++)
        memcpy(h->saved[i], h->at[i], h->size[i]);
}

static void helper_restore(struct helper *h)
{
    uint32_t i;
    for (i = 0; i < h->n; i++)
        memcpy(h->at[i], h->saved[i], h->size[i]);
}

static uint32_t helper_digest(struct helper *h)
{
    uint32_t i, d = 0x811c9dc5u;
    for (i = 0; i < h->n; i++)
        d = digest(h->at[i], h->size[i], d);
    return d;
}

static void build_stubs(void)
{
    uint32_t i;

#define TRAPS(table, id) \
    for (i = 4; i < sizeof table; i += 4) *(uint32_t *)(table + i) = 0xde000000u | ((id) << 16) | i
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(hg2d, 13); TRAPS(g3d, 14);
#undef TRAPS
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x074, h_attach);
    SET_SLOT(host, 0x078, h_release);
    SET_SLOT(host, 0x084, h_flush);
    SET_SLOT(host, 0x10c, h_dialog);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x218, h_memset);
    SET_SLOT(host, 0x21c, h_memmove);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b0, h_strcat);
    SET_SLOT(host, 0x2b8, h_strlen);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    *(uint32_t *)hg2d = 13;
    SET_SLOT(hg2d, 0x2c, hg2d_h);
    SET_SLOT(hg2d, 0x30, hg2d_v);
    SET_SLOT(hg2d, 0x34, hg2d_line);
    *(uint32_t *)g3d = 14;
    SET_SLOT(g3d, 0x28, g3d_list);
}

/* everything the module maps, outside its .data image */
static uint32_t outside_digest(struct side *s)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)s->module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((uint8_t *)s->module + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    uint32_t i, d = 0x811c9dc5u;

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        uint8_t *a = s->lo + sec[i].VirtualAddress, *b = a + sec[i].Misc.VirtualSize;
        if (s->image >= a && s->image < b) {
            d = digest(a, (uint32_t)(s->image - a), d);
            d = digest(s->image + IMAGE_SIZE, (uint32_t)(b - s->image - IMAGE_SIZE), d);
        } else {
            d = digest(a, (uint32_t)(b - a), d);
        }
    }
    return d;
}

static void load_side(struct side *s, const char *path, const char *name)
{
    struct import *im;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;

    memset(s, 0, sizeof *s);
    s->name = name;
    s->transcript = malloc(TRANSCRIPT_SIZE);
    s->module = LoadLibraryA(path);
    if (s->module == NULL) { printf("cannot load %s (%lu)\n", path, GetLastError()); exit(2); }
    s->imports = (struct import *)GetProcAddress(s->module, "ImportTable");
    s->link = (uint8_t *)GetProcAddress(s->module, "Linkage");
    if (s->imports == NULL || s->link == NULL || *(uint32_t *)s->link != 29) {
        printf("%s: not ANALYSIS\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports - 0x88;
    if (s->link != s->image + 0xc8) {
        printf("%s: Linkage is not ImportTable+0x40\n", path);
        exit(2);
    }
    dos = (IMAGE_DOS_HEADER *)s->module;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)s->module + dos->e_lfanew);
    s->lo = (uint8_t *)s->module;
    s->hi = s->lo + nt->OptionalHeader.SizeOfImage;
    for (im = s->imports; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host; break;
        case 2:  im->link = global; break;
        case 4:  im->link = chain; break;
        case 13: im->link = hg2d; break;
        case 14: im->link = g3d; break;
        case 17: im->link = numeric.link; break;
        case 20: im->link = util.link; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
    s->outside = outside_digest(s);
}

/* ---- GLOBAL --------------------------------------------------------------------- */

static uint32_t pick(const uint32_t *v, uint32_t n)
{
    return v[rnd_n(n)];
}

/* the options, as the options dialog or a saved flight might leave them */
static void g_mode(void)
{
    G32(0x814) = rnd_n(10) == 0 ? (rnd_n(2) ? 4 + rnd_n(3) : 0) : 1 + rnd_n(3);
    if (G32(0x814)) G16(0x1a) |= 0x200;
    G32(0x818) = rnd_n(4) != 0;
    G32(0x81c) = rnd_n(3) != 0;
    G32(0x820) = rnd_n(12) == 0;
    /* table indices kept to the image's own ids and stub links: further
     * on they read code pointers, which differ between the sides */
    G32(0x824) = rnd_n(6) == 0 ? rnd_n(0x20) : rnd_n(4);
    G32(0x828) = rnd_n(6) == 0 ? rnd_n(10) : rnd_n(3);
}

/* the ground: 24.8, the low byte's bit 7 the borrow the landing test takes */
static void g_ground(void)
{
    G32(0x20) = rnd_n(4) == 0 ? rnd() : (rnd_n(0x4000) << 8) | rnd_n(0x100);
    if (rnd_n(5) == 0) G32(0x20) = (uint32_t)-(int32_t)rnd_n(0x10000);
}

/* a height above the ground near one of the landing's thresholds: 0x22 to
 * arm, 0x1e:0x7ae5 to start */
static void g_height(void)
{
    static const uint32_t marks[] = { 0x21, 0x22, 0x23, 0x1e, 0x1f, 0x1d, 0x00, 0xff, 0x100,
                                      0x30 };
    uint32_t whole = rnd_n(3) == 0 ? rnd_n(0x60) : pick(marks, 10), frac;
    int32_t ground;

    /* now and then the ground placed so the altitude's whole part is about
     * 0x100, the landing graph's clamp */
    if (rnd_n(4) == 0)
        G32(0x20) = ((0xff + rnd_n(3) - whole) << 8) | rnd_n(0x100);
    ground = (int32_t)G32(0x20);
    frac = rnd_n(2) ? pick((const uint32_t[]){ 0x7ae5, 0x7ae6, 0x7ae4, 0, 0xffff, 0x7a00,
                                               0x7b00 }, 7) : rnd_n(0x10000);
    /* height = alt - ground: put the wanted whole part and fraction back */
    G32(0x574) = whole + (uint32_t)(ground >> 8) + ((G32(0x20) >> 7) & 1);
    G16(0x572) = (uint16_t)((frac & 0xff) | ((((frac >> 8) + (G32(0x20) & 0xff)) & 0xff) << 8));
    if (rnd_n(8) == 0) G32(0x574) = rnd();
}

static void g_position(void)
{
    uint32_t i;

    for (i = 0x560; i < 0x578; i += 4)
        G32(i) = rnd();
    /* latitude: the whole part is metres north, inside +-10,001,750 or UTIL
     * +0x48's table is read past its end (retail UTIL faults, rarely kept) */
    G32(0x564) = rnd_n(64) == 0 ? rnd() : rnd_n(4) == 0 ? 10001750u - rnd_n(3)
                 : (uint32_t)((int32_t)rnd_n(18000000) - 9000000);
    G32(0x56c) = rnd_n(3) == 0 ? 0x4000 + rnd_n(3) - 1 : rnd_n(3) == 0 ? 0xc000 + rnd_n(3) - 1
                 : rnd_n(2) ? 0x8000 + rnd_n(0x100) : rnd();          /* longitude */
    g_height();
}

/* one frame's movement: mostly a little, sometimes none */
static void g_move(void)
{
    uint32_t k = rnd_n(10);

    if (still) {
        still--;
        return;
    }
    if (k < 2)
        return;
    if (k < 8) {
        *(uint16_t *)(global + 0x562) += (uint16_t)rnd_n(0x4000);
        G32(0x564) += rnd_n(3) - 1;
        *(uint16_t *)(global + 0x56a) += (uint16_t)rnd_n(0x4000);
        G32(0x56c) += rnd_n(3) - 1;
        G16(0x572) -= (uint16_t)rnd_n(0x3000);
        if (rnd_n(16) == 0) G32(0x574) -= 1;
    } else if (k < 9) {
        /* a bit anywhere but the latitude's top word (see g_position) */
        uint32_t w = rnd_n(11);
        G16(0x560 + 2 * (w < 3 ? w : w + 1)) ^= (uint16_t)(1u << rnd_n(16));
    } else if (rnd_n(3) == 0) {
        still = rnd_n(12);
    }
}

static void g_setup(void)
{
    uint32_t i;

    for (i = 0; i < GLOBAL_SIZE; i += 4)
        G32(i) = rnd_n(3) == 0 ? rnd() : 0;
    G16(0xc18) = (uint16_t)(rnd_n(16) == 0 ? rnd() : rnd_n(8) == 0 ? rnd_n(6) : rnd_n(3));
    G32(0x268) = rnd_n(10) == 0;
    G8(0x365) = rnd_n(8) == 0;
    G8(0x366) = rnd_n(4) == 0;
    G32(0x2b8) = rnd_n(2) ? 0xc7f + rnd_n(3) - 1 : rnd();
    G8(0x838) = (uint8_t)(rnd_n(6) == 0 ? rnd_n(7) : 0);
    g_mode();
    g_ground();
    g_position();
    still = 0;
}

/* ---- driving ------------------------------------------------------------------------- */

typedef uint32_t (STDCALL *s0)(void);
typedef uint32_t (STDCALL *s1)(uint32_t);
typedef uint32_t (STDCALL *s2)(uint32_t, uint32_t);

#define SVC(off) (*(void **)(cur->link + (off)))

/* a CHAIN callback called the way CHAIN calls it, ecx chosen */
static uint32_t callback(uint32_t fn, uint32_t ecx, uint32_t a, uint32_t b)
{
    uint32_t r;

    __asm__ volatile("pushl %4\n\tpushl %3\n\tcall *%2"
                     : "=a"(r), "+c"(ecx) : "r"(fn), "r"(a), "r"(b) : "edx", "memory", "cc");
    return r;
}

static uint32_t frame_ecx(void)
{
    uint32_t k = rnd_n(10);

    return k < 7 ? 0 : k == 7 ? rnd() : k == 8 ? rnd() & 0xffff : rnd() | 0x10000;
}

static void frames(uint32_t n)
{
    uint32_t i, before, ecx;

    for (i = 0; i < n && !faulted; i++) {
        g_move();
        before = I32(0x24);
        ecx = frame_ecx();
        if (counting) {
            cover_frames++;
            cover_mode[G32(0x814) < 5 ? G32(0x814) : 5]++;
            if (I8(0x72) == 1) cover_armed++;
            if (I8(0x72) == 2) cover_recording++;
            if (ecx >> 16) cover_wide++;
        }
        put("\n#frame ecx=%#x", ecx);
        if (cur->list5)
            callback(cur->list5, ecx, 0, 0);
        if (counting) {
            if (I32(0x24) == before + 1) {
                cover_samples++;
                if ((ecx >> 16) && memcmp(global + 0x560, cur->image + 0x2c, 0x18) == 0)
                    cover_wide_sample++;
            }
            if (before >= 990 && I32(0x24) == MEMMOVE_ECX + 1) cover_compactions++;
        }
        if (cur->list14 && rnd_n(3) != 0) {
            put(" [0xe]");
            callback(cur->list14, rnd_n(4) == 0 ? rnd() : 0, 0, 0);
        }
        if (cur->list12 && rnd_n(6) == 0) {
            uint32_t arg = rnd_n(3) == 0, command = rnd_n(3) ? 0x1002b : 0x10000 + rnd_n(0x40);
            put(" [0xc %#x,%u]", command, arg);
            callback(cur->list12, 0, command, (uint32_t)(uintptr_t)&arg);
        }
        put(" G814=%u 820=%u count=%u cd=%u st=%u iv=%#x ended=%u", G32(0x814), G32(0x820),
            I32(0x24), I8(0x70), I8(0x72), I8(0x71), I32(0));
    }
}

static void accessor(void)
{
    uint8_t buf[0x20];
    uint32_t k = rnd_n(6), index = rnd_n(4) == 0 ? 1000 + rnd_n(3) - 1 : rnd_n(4) == 0 ? rnd()
                                   : rnd_n(I32(0x24) + 2);

    memset(buf, 0xee, sizeof buf);
    put("\n#accessor %u %#x", k, index);
    switch (k) {
    case 0: case 1: case 2:
        ((s2)SVC(0x1c + 4 * k))(index, (uint32_t)(uintptr_t)buf);
        put_bytes(buf, sizeof buf);
        break;
    case 3: {
        uint32_t i;
        for (i = 0; i < 0x1c; i++)
            buf[i] = (uint8_t)rnd();
        if (rnd_n(32) != 0)     /* a latitude UTIL can take, as in g_position */
            *(int32_t *)(buf + 8) = (int32_t)rnd_n(18000000) - 9000000;
        buf[0] = (uint8_t)pick((const uint32_t[]){ 0x81, 0x83, 0x80, 0 }, 4);
        ((s2)SVC(0x28))(index, (uint32_t)(uintptr_t)buf);
        break;
    }
    case 4:
        put(" =%u", ((s0)SVC(0x2c))());
        break;
    default:
        ((s1)SVC(0x30))(index);
        break;
    }
}

static void op(void)
{
    uint32_t k = rnd_n(100);

    if (counting) cover_ops[k < 32 ? 0 : k < 40 ? 1 : k < 52 ? 2 : k < 60 ? 3 : k < 65 ? 4 :
                            k < 70 ? 5 : k < 80 ? 6 : k < 87 ? 7 : k < 93 ? 8 : 9]++;
    if (k < 32) {                                   /* frames */
        frames(1 + rnd_n(rnd_n(4) == 0 ? 80 : 10));
    } else if (k < 40) {                            /* the options change */
        g_mode();
        put("\n#mode %u", G32(0x814));
        frames(1 + rnd_n(8));
    } else if (k < 52) {                            /* an approach */
        G32(0x814) = 1;
        G8(0x838) = 0;
        G32(0x268) = 0;
        G8(0x365) = rnd_n(10) == 0;
        if (rnd_n(2)) I8(0x72) = (uint8_t)(rnd_n(5) == 0 ? rnd() : rnd_n(3));
        g_ground();
        g_height();
        G8(0x366) = rnd_n(3) == 0;
        G32(0x2b8) = rnd_n(2) ? 0xc7f + rnd_n(3) - 1 : rnd();
        /* the frame recording starts on: armed, the countdown due, the
         * position the last sample's -- the reset's ecx decides it */
        if (rnd_n(3) == 0) {
            I8(0x72) = 1;
            I8(0x70) = 0;
            memcpy(cur->image + 0x2c, global + 0x560, 0x18);
            still = 4;
        }
        put("\n#approach st=%u h=%#x:%#x g=%#x", I8(0x72), G32(0x574), G16(0x572), G32(0x20));
        frames(1 + rnd_n(12));
    } else if (k < 60) {                            /* a manoeuvre and its end */
        G32(0x814) = 3;
        G8(0x838) = 0;
        G32(0x268) = 0;
        put("\n#manoeuvre");
        frames(1 + rnd_n(20));
        if (cur->list12 && !faulted) {
            uint32_t arg = rnd_n(4) == 0;
            if (counting) cover_commands++;
            put(" [0xc 0x1002b,%u]", arg);
            callback(cur->list12, 0, 0x1002b, (uint32_t)(uintptr_t)&arg);
            if (counting && I32(0)) cover_ended++;
            frames(1 + rnd_n(4));
        }
    } else if (k < 65) {                            /* the options dialog */
        put("\n#options");
        ((s0)SVC(0x34))();
        put(" G814=%u 818=%u 81c=%u 820=%u 824=%u 828=%u 1a=%#x", G32(0x814), G32(0x818),
            G32(0x81c), G32(0x820), G32(0x824), G32(0x828), G16(0x1a));
    } else if (k < 70) {                            /* a dialog straight from MAIN's menu */
        put("\n#dialog");
        if (I32(0x24) != 0 || rnd_n(8) == 0)       /* none: a stop, see h_dialog */
            ((s0)SVC(rnd_n(2) ? 0x38 : 0x3c))();
    } else if (k < 80) {
        accessor();
    } else if (k < 87) {                            /* near the compaction */
        if (counting) cover_one++;
        ((s1)SVC(0x30))(rnd_n(4) == 0 ? rnd_n(4) : 995 + rnd_n(5));
        G32(0x814) = 1 + rnd_n(3);
        G8(0x838) = 0;
        G32(0x268) = 0;
        G8(0x366) = 0;
        if (G32(0x814) == 1) I8(0x72) = 2;
        if (rnd_n(3) == 0) I8(0x71) = (uint8_t)(rnd_n(2) ? 0x80 : 1u << rnd_n(8));
        still = 0;
        put("\n#fill %u mode %u", I32(0x24), G32(0x814));
        frames(4 + rnd_n(60));
    } else if (k < 93) {                            /* a crash */
        G8(0x838) = (uint8_t)(rnd_n(6) == 0 ? rnd() : 1 + rnd_n(4));
        if (counting && G32(0x814) == 1 && I32(0x24)) cover_landed_crash++;
        put("\n#crash %u", G8(0x838));
        frames(1 + rnd_n(3));
        G8(0x838) = 0;
    } else {                                        /* a reset asked for */
        if (rnd_n(2)) I32(4) = 1;
        else G32(0x820) = 1;
        put("\n#reset");
        frames(1 + rnd_n(3));
    }
    if (counting && I32(0x220)) cover_unsigned++;
    if (counting) cover_lists[G32(0x828) < 3 ? G32(0x828) : 3]++;
}

/* The comparable state: the image with its code pointers paired and its
 * pointers into itself as offsets, the arena, GLOBAL, UTIL and NUMERIC. */
static void put_state(void)
{
    static const uint32_t code[] = { 0xcc, 0xd0, 0xe0, 0xe4, 0xe8, 0xec, 0xf0, 0xf4, 0xf8,
                                     0xfc, 0x100, 0x104, 0x14c, 0x150, 0x18c, 0x190, 0x1a4,
                                     0x1ec };
    uint8_t img[IMAGE_SIZE];
    uint32_t i, base = (uint32_t)(uintptr_t)cur->image;

    memcpy(img, cur->image, IMAGE_SIZE);
    for (i = 0; i < sizeof code / 4; i++)
        *(uint32_t *)(img + code[i]) = pair_code(*(uint32_t *)(img + code[i]));
    for (i = 0; i + 4 <= IMAGE_SIZE; i += 4) {
        uint32_t v = *(uint32_t *)(img + i);
        if (v >= base && v < base + IMAGE_SIZE)
            *(uint32_t *)(img + i) = 0xd0000000u | (v - base);
    }
    put("\n  state img=%08x arena=%#x/%08x G=%08x util=%08x numeric=%08x",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena, arena_used, 0x811c9dc5u), digest(global, GLOBAL_SIZE, 0x811c9dc5u),
        helper_digest(&util), helper_digest(&numeric));
    if (getenv("ANALYSIS_IMAGE")) {
        put("\n  whole");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(img + i));
    }
}

static void set_precision(uint32_t pc)
{
    uint16_t cw;

    __asm__ volatile("fnstcw %0" : "=m"(cw));
    cw = (uint16_t)((cw & ~0x300u) | (pc << 8));
    __asm__ volatile("fldcw %0" : : "m"(cw));
}

static void run_side(struct side *s)
{
    uint32_t i, nops, pc;

    cur = s;
    s->length = 0;
    s->list5 = s->list12 = s->list14 = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    memset(chain_count, 0, sizeof chain_count);
    helper_restore(&util);
    helper_restore(&numeric);
    arena_used = 0;
    memset(arena, 0, 0x100000);
    rng = scenario_seed;
    pc = (uint32_t[]){ 0, 2, 3, 3 }[rnd_n(4)];
    g_setup();
    put("scenario %u pc %u mode %u units %u\n", scenario, pc, G32(0x814), G16(0xc18));
    faulted = 0;
    stopped = 0;
    in_analysis = 1;
    set_precision(pc);
    if (__builtin_setjmp(fault_jb) == 0) {
        uint32_t r = ((s0)SVC(4))();
        put(" =%#x", r);
    }
    in_analysis = 0;
    put_state();
    nops = 4 + rnd_n(MAX_OPS);
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        in_analysis = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            op();
        in_analysis = 0;
        if (!stopped)
            put_state();
    }
    if (!faulted) {
        put("\n#deinit:");
        in_analysis = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            ((s0)SVC(8))();
        in_analysis = 0;
        put_state();
    }
    set_precision(3);
    put("\n");
    if (outside_digest(s) != s->outside) {
        printf("scenario %u: %s ANALYSIS's memory outside its image changed\n", scenario,
               s->name);
        fflush(stdout);
        exit(4);
    }
    cur = NULL;
}

static void show_difference(void)
{
    const char *a = retail.transcript, *b = ours.transcript;
    uint32_t n = retail.length < ours.length ? retail.length : ours.length, i, from;
    const char *ls;

    for (i = 0; i < n && a[i] == b[i]; i++)
        ;
    ls = a + i;
    while (ls > a && ls[-1] != '\n')
        ls--;
    printf("scenario %u differs at byte %u\n", scenario, i);
    from = (uint32_t)(ls - a) > 1500 ? (uint32_t)(ls - a) - 1500 : 0;
    printf("--- before:\n%.*s\n", (int)((uint32_t)(ls - a) - from), a + from);
    from = i - (uint32_t)(ls - a) > 400 ? i - 400 : (uint32_t)(ls - a);
    printf("--- retail:\n...%.900s\n", a + from);
    printf("--- ours:\n...%.900s\n", b + from);
}

static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    uint8_t *at = (uint8_t *)e->ExceptionRecord->ExceptionAddress;
    struct side *s[2] = { &retail, &ours };
    int i;

    for (i = 0; i < 2; i++)
        if (s[i]->module && at >= s[i]->lo && at < s[i]->hi)
            printf("\nFAULT in %s ANALYSIS at .text+%#x\n", s[i]->name,
                   (uint32_t)(at - s[i]->lo - 0x1000));
    printf("FAULT %#lx at %p, address %p, scenario %u op %u, running %s\n",
           e->ExceptionRecord->ExceptionCode, (void *)at,
           e->ExceptionRecord->NumberParameters > 1
               ? (void *)e->ExceptionRecord->ExceptionInformation[1] : NULL,
           scenario, op_number, cur ? cur->name : "none");
    if (cur != NULL && cur->length > 0) {
        uint32_t from = cur->length > 2500 ? cur->length - 2500 : 0;
        printf("--- transcript tail:\n%.*s\n", (int)(cur->length - from), cur->transcript + from);
    }
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 20000;
    uint32_t first = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0;
    uint32_t i, differ = 0;

    AddVectoredExceptionHandler(1, on_vectored);
    if (getenv("ANALYSIS_NOFILTER") == NULL)
        SetUnhandledExceptionFilter(on_fault);
    build_stubs();
    /* UTIL first (NUMERIC imports it), its NUMERIC link set after */
    helper_load(&util, "UTIL_retail.dll", 20);
    helper_load(&numeric, "NUMERIC_retail.dll", 17);
    {
        struct import *im = (struct import *)GetProcAddress(util.module, "ImportTable");
        for (; im->id; im++)
            if (im->id == 17) im->link = numeric.link;
    }
    helper_save(&util);
    helper_save(&numeric);
    load_side(&retail, "ANALYSIS_retail.dll", "retail");
    load_side(&ours, "ANALYSIS_ours.dll", "ours");

    for (i = first; i < first + n; i++) {
        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        scenario_seed = rnd() | 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("ANALYSIS_DUMP") != NULL) {
            FILE *f = fopen("analysis_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("analysis_ours.txt", "wb");
            fwrite(ours.transcript, 1, ours.length, f);
            fclose(f);
        }
        if (retail.length != ours.length ||
            memcmp(retail.transcript, ours.transcript, retail.length) != 0) {
            if (differ++ < 3)
                show_difference();
        }
        if ((i + 1 - first) % 1000 == 0) {
            printf("%u scenarios, %u differ\n", i + 1 - first, differ);
            fflush(stdout);
        }
    }
    printf("analysisdiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops:");
    for (i = 0; i < 10; i++)
        printf(" %lu", cover_ops[i]);
    printf("\n  frames %lu (modes 0..4, more:", cover_frames);
    for (i = 0; i < 6; i++)
        printf(" %lu", cover_mode[i]);
    printf("), samples %lu, compactions %lu\n", cover_samples, cover_compactions);
    printf("  landing: armed frames %lu, recording frames %lu, dialogs %lu (touchdown text %lu),"
           " by a crash %lu\n", cover_armed, cover_recording, cover_landing, cover_speed,
           cover_landed_crash);
    printf("  manoeuvre: commands %lu (ended %lu), dialogs %lu, unsigned extents at an op %lu\n",
           cover_commands, cover_ended, cover_manoeuvre, cover_unsigned);
    printf("  options dialogs %lu (checked %lu, refused %lu); trails drawn %lu (long %lu);"
           " trail length index 0, 1, 2, more at an op: %lu %lu %lu %lu\n", cover_options,
           cover_checks, cover_refused, cover_trails, cover_long_list, cover_lists[0],
           cover_lists[1], cover_lists[2], cover_lists[3]);
    printf("  ecx with a top half %lu, a sample of an unmoved position from it %lu;"
           " fills %lu\n", cover_wide, cover_wide_sample, cover_one);
    printf("  lines %lu, divide fixes %lu, stops with no samples %lu\n", cover_lines,
           cover_divfix, cover_poison);
    printf("  faults (both sides alike, the scenario ends) %lu; retail's at:", cover_faults);
    for (i = 0; i < 16 && fault_n[i]; i++)
        printf(" %s%#x:%lu", fault_at[i] == 0xffffffffu ? "elsewhere" : ".text+",
               fault_at[i] == 0xffffffffu ? 0 : fault_at[i], fault_n[i]);
    printf("\n");
    return differ ? 1 : 0;
}
