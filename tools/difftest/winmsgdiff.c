/* Differential test: retail WINMSG.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven through Linkage and the
 * two CHAIN callbacks they register (list 2, the frame; list 0x14, the
 * situation), the way ATC, LANDME, DEMO, ADVDRV and MAIN drive them, one
 * side after the other over the same script.  What WINMSG imports:
 *
 *   host      a stub.  Memory is a bump arena reset before each side, so heap
 *             pointers come out the same; what the host does not zero is
 *             filled with 0xa5, what it frees with 0xdd, and the slack
 *             after each block with the scenario's padding byte -- WINMSG's
 *             line breaker reads past the end of its copy of a text, and
 *             what it finds there decides what it shows next.  A free or
 *             realloc of something that is not a live block is a heap fault,
 *             an outcome like an access violation.  The window services are
 *             a model of the host's (docs/modules/host.md): +0x64 gives the
 *             record its pixel width (+0x38, the rectangle's 0x2000ths of
 *             the screen's) and a surface (+0x94 bytes at +0x98, which a
 *             blank fills), +0x7c now and then takes a new size as the
 *             user's resize would, +0x84 and +0x8c are logged with the text
 *             they draw.
 *   WINDOW    +0x78/+0x7c: a zeroed 0xa4-byte record titled from its string
 *             id, and its release (title, then the record, *prec nulled),
 *             as WINDOW's are.
 *   CHAIN     a stub that keeps both callbacks, and on +0x20 (message 0x50,
 *             a message done) calls list 0x14's -- WINMSG's own among them
 *             -- and a listener that checks the node against what +0x18
 *             returned and now and then calls back into WINMSG from inside
 *             the dispatch, as ADVDRV's program may.
 *   MAIN      +0x5c..+0x64, the situation writer, logged with the bytes.
 *   GLOBAL    +0x1254 (scroll: 0, 1, or another non-zero) and +0x362 (the
 *             frame rate, the scroll speed; below 500), set per scenario and
 *             changed between ops.
 *
 * A scenario inits the module and runs a script: messages shown in every
 * slot (texts of words, long words, runs of spaces, empty, longer than the
 * 0x400-byte line, high-bit characters; times from none to ones whose
 * shares overflow), plain and appended; frames, in runs, until messages
 * finish; slots cleared, all cleared, queried; the window resized, from
 * under 30 pixels (a scrolling line then starts before WINMSG's buffer) to
 * wider than the line; the situation's record loaded (0x22), saved (0x23)
 * and the window closed (0x37); and deinit.  Ops are aimed with each side's
 * own published state: a text exactly as wide as the window allows, and an
 * append on a scrolling message exactly when its tail comes on screen.
 *
 * Compared after every operation: the transcript of every import call (with
 * the strings and bytes they carry) and each service's return, WINMSG's
 * whole .data image (code pointers through a pairing learnt as they are
 * first seen), the arena and GLOBAL.  A fault ends the scenario on both
 * sides; it is compared as an outcome.
 *
 * Build: make winmsgdiff     Run: make winmsgdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x4a8u
#define GLOBAL_SIZE  0x1300u
#define ARENA_SIZE   (8u << 20)
#define TRANSCRIPT_SIZE (8u << 20)
#define MAX_OPS      80
#define MAX_PAIRS    32

struct import { uint32_t id; void *link; };

typedef uint32_t (STDCALL *callback_fn)(uint32_t, uint32_t);

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    uint8_t *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    callback_fn frame, situation;
    char *transcript;
    uint32_t length;
};

static struct side retail, ours;
static struct side *cur;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t chain[0x24], mainmod[0x68], window[0x80];
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t pad_byte;
static uint32_t rng, scenario_seed;
static uint32_t scenario;
static uint32_t op_number;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs, npairs_ours;
static uint32_t last_handle;
static int in_dispatch;

static unsigned long cover_ops[16], cover_frames, cover_plain, cover_scroll, cover_done,
    cover_done_last, cover_reentered, cover_splits, cover_faults, cover_heap_faults,
    cover_narrow, cover_wide, cover_append[5], cover_aimed_fit, cover_aimed_keep,
    cover_restores, cover_closes, cover_before;

#define G16(o) (*(uint16_t *)(global + (o)))
#define G32(o) (*(uint32_t *)(global + (o)))
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

/* A code pointer, as its number in the order code pointers were first seen
 * on that side: equal numbers are the paired routines. */
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

/* A string the module hands over: it may have run off its block into the
 * arena's slack, so bounded, and with its bytes shown as numbers when they
 * are not plain. */
static void put_s(const char *s)
{
    uint32_t i;

    if (s == NULL) {
        put("null");
        return;
    }
    put("'");
    for (i = 0; s[i] && i < 0x500; i++) {
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
static volatile int in_winmsg;
static volatile int faulted;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

/* A heap operation the real heap would corrupt itself on: the scenario
 * ends there, as the sim would. */
static void heap_fault(const char *what)
{
    put(" FAULT(heap %s)", what);
    if (counting) {
        cover_faults++;
        cover_heap_faults++;
    }
    in_winmsg = 0;
    faulted = 1;
    __builtin_longjmp(fault_jb, 1);
}

static uint32_t fault_at[16];
static unsigned long fault_n[16];

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
    if (!in_winmsg || cur == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW)
        put(" FAULT(div)");
    else if (code == EXCEPTION_ACCESS_VIOLATION)
        put(" FAULT(av)");
    else
        return EXCEPTION_CONTINUE_SEARCH;
    if (getenv("WINMSG_EXC")) {
        uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
        if (ip >= cur->lo && ip < cur->hi)
            put("@%s.text+%#x", cur->name, (uint32_t)(ip - cur->lo - 0x1000));
        else
            put("@%p", (void *)ip);
    }
    if (counting) {
        uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
        uint32_t where = ip >= cur->lo && ip < cur->hi ? (uint32_t)(ip - cur->lo - 0x1000)
                         : 0xffffffffu, k;

        cover_faults++;
        for (k = 0; k < 16 && fault_n[k] && fault_at[k] != where; k++)
            ;
        if (k < 16) {
            fault_at[k] = where;
            fault_n[k]++;
        }
    }
    in_winmsg = 0;
    faulted = 1;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
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
    memset(p + 16 + size, pad_byte, n - 16 - size);
    return p + 16;
}

static uint32_t block_size(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;

    if (!in_arena((uint32_t)(uintptr_t)b) || ((uintptr_t)b & 15) != 0 ||
        b < arena + 16 || *(const uint32_t *)(b - 12) != LIVE)
        return 0xffffffffu;
    return *(const uint32_t *)(b - 16);
}

static void arena_release(void *p)
{
    uint8_t *b = (uint8_t *)p;
    uint32_t size = block_size(b);

    if (size == 0xffffffffu)
        heap_fault("free");
    memset(b, 0xdd, size);
    *(uint32_t *)(b - 12) = FREED;
}

/* ---- the stub host -------------------------------------------------------------- */

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p;

    put(" alloc(%d,%#x,", fl, size);
    put_ptr((uint32_t)(uintptr_t)out);
    put(",%d)=", zero);
    if (size > 0x100000)
        heap_fault("huge");
    p = arena_get(size);
    memset(p, zero ? 0 : 0xa5, size);
    put_ptr((uint32_t)(uintptr_t)p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    uint32_t old;
    void *p;

    put(" realloc(%d,%#x,", fl, size);
    put_ptr((uint32_t)(uintptr_t)pp);
    put(":");
    put_ptr((uint32_t)(uintptr_t)*pp);
    put(")=");
    if (size > 0x100000)
        heap_fault("huge");
    if (*pp == NULL) {
        old = 0;
    } else {
        old = block_size(*pp);
        if (old == 0xffffffffu)
            heap_fault("realloc");
    }
    p = arena_get(size);
    memset(p, 0xa5, size);
    if (*pp != NULL) {
        memcpy(p, *pp, old < size ? old : size);
        arena_release(*pp);
    }
    put_ptr((uint32_t)(uintptr_t)p);
    *pp = p;
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

static int32_t STDCALL h_memset(void *d, int32_t byte, uint32_t n)
{
    /* The host keeps only the low byte; WINMSG pushes dl over whatever edx
     * held. */
    put(" memset(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",%#x,%#x)", (uint32_t)byte & 0xff, n);
    if (n != 0)
        memset(d, byte & 0xff, n);
    return 0;
}

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    put(" memmove(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_ptr((uint32_t)(uintptr_t)s);
    put(",%#x)", n);
    if (n > 0x100000) {
        put(" FAULT(av)");
        if (counting) cover_faults++;
        in_winmsg = 0;
        faulted = 1;
        __builtin_longjmp(fault_jb, 1);
    }
    put_bytes(s, n);
    memmove(d, s, n);
    return 0;
}

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
    put_s(d);
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

static char *STDCALL h_strdup(const char *s)
{
    uint32_t n;
    char *p;

    if (s == NULL) {
        put(" strdup(null)");
        return NULL;
    }
    n = (uint32_t)strlen(s) + 1;
    p = (char *)arena_get(n);
    memcpy(p, s, n);
    put(" strdup(");
    put_s(s);
    put(")=");
    put_ptr((uint32_t)(uintptr_t)p);
    return p;
}

/* ---- the host's window ------------------------------------------------------------ */

static uint32_t screen_w;

/* A surface for the record: +0x94 bytes at +0x98, or none. */
static void host_surface(uint32_t rec)
{
    uint32_t k = rnd_n(8), size;

    if (k == 0) {
        R32(rec, 0x94) = 0;
        R32(rec, 0x98) = 0;
        return;
    }
    size = k == 1 ? 0 : 1 + rnd_n(0x100);
    R32(rec, 0x94) = size;
    R32(rec, 0x98) = (uint32_t)(uintptr_t)arena_get(size);
    memset((void *)(uintptr_t)R32(rec, 0x98), 0x5a, size);
}

/* The width in pixels, +0x38: the host scales the rectangle's 0x2000ths by
 * the screen's (imul by the width << 16 / 0x2000, kept at 0x41bda8, then
 * sar 16); the user's resize, now and then, gives anything down to nothing,
 * and rarely wider than WINMSG's line. */
static void host_width(uint32_t rec, int user)
{
    uint32_t k = user ? rnd_n(16) : 15;
    int32_t w;

    if (k < 3)
        w = (int32_t)rnd_n(40);
    else if (k < 5)
        w = 0x16 + (int32_t)rnd_n(64);
    else if (k == 5)
        w = rnd_n(20) == 0 ? 0x2000 + (int32_t)rnd_n(0x1e) : (int32_t)rnd_n(0x1000);
    else
        w = (int32_t)(R32(rec, 0x28) * (screen_w << 3)) >> 16;
    /* Kept under 0x201e, where WINMSG's limit reaches 0x2000 and a
     * scrolling message is copied with no limit at all: past the line and
     * the image into retail's import table (docs/modules/winmsg.md).  No
     * screen is that wide; a restored rectangle can ask for it. */
    if (w >= 0x201e)
        w = 0x2000 + (int32_t)rnd_n(0x1e);
    R32(rec, 0x38) = (uint32_t)w;
    R32(rec, 0x3c) = 0x16;
    if (counting) {
        if (w < 0x1e) cover_narrow++;
        if (w >= 0x2000) cover_wide++;
    }
}

static void put_rec(uint32_t rec)
{
    put_ptr(rec);
    if (in_arena(rec))
        put("{%#x,%#x %d,%d,%d,%d %d,%d,%d,%d w%d}", R32(rec, 4), R32(rec, 8),
            R32(rec, 0x20), R32(rec, 0x24), R32(rec, 0x28), R32(rec, 0x2c),
            R32(rec, 0x68), R32(rec, 0x6c), R32(rec, 0x70), R32(rec, 0x74),
            R32(rec, 0x38));
}

static int32_t STDCALL h_win_create(uint32_t rec)
{
    put(" wincreate(");
    put_rec(rec);
    put(")");
    if (rec == 0)
        return -1;
    R32(rec, 0xa0) = (uint32_t)(uintptr_t)arena_get(0x90);
    memset((void *)(uintptr_t)R32(rec, 0xa0), 0, 0x90);
    host_width(rec, 0);
    host_surface(rec);
    return 0;
}

static int32_t STDCALL h_win_destroy(uint32_t rec)
{
    put(" windestroy(");
    put_rec(rec);
    put(")");
    if (rec == 0 || !in_arena(rec))
        return 0;
    if (R32(rec, 0xa0)) {
        arena_release((void *)(uintptr_t)R32(rec, 0xa0));
        R32(rec, 0xa0) = 0;
    }
    return 0;
}

static int32_t STDCALL h_win_update(uint32_t rec)
{
    put(" winupdate(");
    put_rec(rec);
    put(")");
    if (rec == 0 || !in_arena(rec))
        return 0;
    if (rnd_n(4) == 0)
        host_width(rec, rnd_n(2));
    return 1;    /* SetWindowTextA's */
}

static int32_t STDCALL h_win_flush(uint32_t rec)
{
    put(" winflush(");
    put_ptr(rec);
    put(")");
    return in_arena(rec) && (R32(rec, 8) & 0x80000000u) ? 1 : 0;
}

static int32_t STDCALL h_text(uint32_t rec, uint32_t where, uint32_t colour, const char *text,
                              uint32_t opaque)
{
    put(" text(");
    put_ptr(rec);
    put(",%u,%#x,", where, colour);
    put_ptr((uint32_t)(uintptr_t)text);
    put(":");
    put_s(text);
    put(",%u)", opaque);
    return rec ? 0 : -1;
}

/* ---- the other modules -------------------------------------------------------------- */

static uint32_t STDCALL window_record(uint32_t *out, uint32_t title, uint32_t type)
{
    uint8_t *rec = (uint8_t *)arena_get(0xa4);
    char t[16];
    uint32_t n;

    memset(rec, 0, 0xa4);
    snprintf(t, sizeof t, "s%04x", title);
    n = (uint32_t)strlen(t) + 1;
    *(char **)rec = (char *)arena_get(n);
    memcpy(*(char **)rec, t, n);
    *(uint32_t *)(rec + 4) = type;
    *out = (uint32_t)(uintptr_t)rec;
    put(" record(");
    put_ptr((uint32_t)(uintptr_t)out);
    put(",%#x,%u)=", title, type);
    put_ptr(*out);
    return 0;
}

static uint32_t STDCALL window_free(uint32_t *prec)
{
    uint32_t rec = *prec;

    put(" recordfree(");
    put_ptr((uint32_t)(uintptr_t)prec);
    put(":");
    put_ptr(rec);
    put(")");
    if (!in_arena(rec))
        return 0xffffffffu;
    h_free((void **)(uintptr_t)rec);
    h_free((void **)prec);
    return 0;
}

static uint32_t STDCALL main_save_open(uint32_t tag, uint32_t size)
{
    put(" saveopen(%#x,%#x)", tag, size);
    return 0;
}

static uint32_t STDCALL main_save_put(const void *p, uint32_t size, uint32_t z)
{
    put(" saveput(");
    put_ptr((uint32_t)(uintptr_t)p);
    put(",%#x,%u)", size, z);
    put_bytes(p, size);
    return 0;
}

static uint32_t STDCALL main_save_close(void)
{
    put(" saveclose");
    return 7;
}

static uint32_t STDCALL chain_add(uint32_t list, callback_fn cb)
{
    put(" chainadd(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    if (list == 2) cur->frame = cb;
    else if (list == 0x14) cur->situation = cb;
    return 0;
}

static uint32_t STDCALL chain_remove(uint32_t list, callback_fn cb)
{
    put(" chainremove(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    if (list == 2 && cur->frame == cb) cur->frame = NULL;
    else if (list == 0x14 && cur->situation == cb) cur->situation = NULL;
    return 0;
}

static void reenter(void);

static uint32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    uint32_t r;

    put(" chainsend(%#x,%#x,", list, a);
    put_ptr(b);
    put(")");
    if (list == 0x14 && a == 0x50) {
        if (counting) {
            cover_done++;
            if (b == last_handle) cover_done_last++;
        }
        if (b == last_handle)
            put(" last");
        if (cur->situation != NULL) {
            r = cur->situation(a, b);
            put(" ->%#x", r);
        }
        /* the listener: ADVDRV moves on when its message is done */
        if (!in_dispatch && rnd_n(6) == 0) {
            in_dispatch = 1;
            reenter();
            in_dispatch = 0;
        }
    }
    return 0;
}

static void build_stubs(void)
{
    uint32_t i;

    /* An unset slot faults at an address that names it: 0xde, the module,
     * the offset. */
#define TRAPS(table, id) \
    for (i = 4; i < sizeof table; i += 4) *(uint32_t *)(table + i) = 0xde000000u | ((id) << 16) | i
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(mainmod, 3); TRAPS(window, 10);
#undef TRAPS
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x064, h_win_create);
    SET_SLOT(host, 0x068, h_win_destroy);
    SET_SLOT(host, 0x07c, h_win_update);
    SET_SLOT(host, 0x084, h_win_flush);
    SET_SLOT(host, 0x08c, h_text);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x210, h_realloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x218, h_memset);
    SET_SLOT(host, 0x21c, h_memmove);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b0, h_strcat);
    SET_SLOT(host, 0x2b4, h_strdup);
    SET_SLOT(host, 0x2b8, h_strlen);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    SET_SLOT(chain, 0x20, chain_send);
    *(uint32_t *)mainmod = 3;
    SET_SLOT(mainmod, 0x5c, main_save_open);
    SET_SLOT(mainmod, 0x60, main_save_put);
    SET_SLOT(mainmod, 0x64, main_save_close);
    *(uint32_t *)window = 10;
    SET_SLOT(window, 0x78, window_record);
    SET_SLOT(window, 0x7c, window_free);
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
    if (s->imports == NULL || s->link == NULL || *(uint32_t *)s->link != 22) {
        printf("%s: not WINMSG\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if (s->link != s->image + 0x30) {
        printf("%s: Linkage is not ImportTable+0x30\n", path);
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
        case 3:  im->link = mainmod; break;
        case 4:  im->link = chain; break;
        case 10: im->link = window; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* ---- the module's own state, to aim the next op ---------------------------------- */

#define IMG32(o) (*(uint32_t *)(cur->image + (o)))
#define REC      IMG32(0x498)
#define QUEUE    IMG32(0x4a4)

/* the first queued message, or 0 */
static uint32_t first_msg(void)
{
    uint32_t q = QUEUE, head, node;

    if (!in_arena(q) || !in_arena(R32(q, 0)) || !in_arena(R32(q, 4)))
        return 0;
    head = R32(q, 0);
    node = R32(head, 4);
    if (node == R32(q, 4) || !in_arena(node) || !in_arena(R32(node, 0)))
        return 0;
    return R32(node, 0);
}

/* What an append in `slot` will do, from the queue as it stands (counted
 * on retail's side only). */
static void classify_append(uint32_t slot)
{
    uint32_t q = QUEUE, head, node, msg;
    int32_t w;

    if (!in_arena(q) || !in_arena(REC))
        return;
    head = R32(q, 0);
    node = R32(R32(q, 4), 8);
    while (node != head && in_arena(node) && R32(R32(node, 0), 8) != slot)
        node = R32(node, 8);
    if (node == head || !in_arena(node)) {
        cover_append[0]++;
        return;
    }
    msg = R32(node, 0);
    if (R32(msg, 0xc) == 0) {
        cover_append[1]++;
        return;
    }
    w = (int32_t)R32(REC, 0x38) - 0x1e;
    if (w > 0x2000) w = 0x2000;
    if (node == R32(head, 4) &&
        (int32_t)((R32(msg, 4) << 3) - R32(msg, 0x1c) + R32(msg, 0x20)) <= w)
        cover_append[(int32_t)((R32(msg, 4) << 3) - R32(msg, 0x1c) + R32(msg, 0x20)) == w
                     ? 4 : 2]++;
    else
        cover_append[3]++;
}

/* ---- driving ------------------------------------------------------------------------- */

typedef uint32_t (STDCALL *s0)(void);
typedef uint32_t (STDCALL *s1)(uint32_t);
typedef uint32_t (STDCALL *s4)(uint32_t, uint32_t, uint32_t, uint32_t);

#define SVC(off) (*(void **)(cur->link + (off)))

static char text_buf[0x1400];

/* A text: words and spaces, now and then a long word, runs of spaces, the
 * high half of the character set; sometimes nothing, or longer than the
 * line. */
static const char *make_text(void)
{
    uint32_t k = rnd_n(20), n, i = 0, w;
    char c;

    if (k == 0) {
        text_buf[0] = 0;
        return text_buf;
    }
    n = k == 1 ? 0x300 + rnd_n(0xc00) : k < 4 ? 1 + rnd_n(8) : 1 + rnd_n(160);
    while (i < n) {
        w = rnd_n(10) == 0 ? 10 + rnd_n(60) : 1 + rnd_n(9);
        while (w-- && i < n) {
            uint32_t r = rnd_n(40);
            c = r == 0 ? (char)(0x80 + rnd_n(0x80)) : r < 4 ? (char)('0' + rnd_n(10))
                : (char)('a' + rnd_n(26));
            text_buf[i++] = c;
        }
        if (i < n) {
            text_buf[i++] = ' ';
            if (rnd_n(8) == 0 && i < n) text_buf[i++] = ' ';
        }
    }
    if (rnd_n(10) == 0) text_buf[0] = ' ';
    text_buf[i] = 0;
    return text_buf;
}

static uint32_t make_time(void)
{
    switch (rnd_n(10)) {
    case 0: return 0;
    case 1: return 1 + rnd_n(3);
    case 2: return 600u / ((uint32_t)G16(0x362) + 1u);   /* ATC's */
    case 3: return 90;                                     /* LANDME's */
    case 4: return 0x90;                                   /* DEMO's */
    case 5: return rnd();                                  /* shares overflow */
    case 6: return 0x10000000u + rnd_n(0x1000);
    default: return 1 + rnd_n(200);
    }
}

static uint32_t make_slot(void)
{
    return rnd_n(12) == 0 ? 0 : 1 + rnd_n(6);
}

static void g_move(void)
{
    uint32_t k = rnd_n(6);

    G32(0x1254) = k < 2 ? 0 : k < 5 ? 1 : 2 + rnd_n(3);
    /* Up to 499: the speed is then at most 251, a scroller's start goes no
     * lower than -250, and its line starts at most 31 bytes early, within
     * the slots' texts.  Further back it overwrites the Linkage and the
     * ImportTable, and both sides call through text. */
    G16(0x362) = (uint16_t)(rnd_n(8) == 0 ? rnd_n(500) : rnd_n(60));
}

static void call_show(const char *text, uint32_t slot, uint32_t time, uint32_t append)
{
    uint32_t r;

    if (counting && append)
        classify_append(slot);
    put("\n#show(");
    put_s(text);
    put(",%u,%#x,%u)", slot, time, append);
    if (counting && text[0] && in_arena(REC) &&
        (int32_t)strlen(text) * 8 > (int32_t)R32(REC, 0x38) - 0x1e)
        cover_splits++;
    r = ((s4)SVC(0x18))((uint32_t)(uintptr_t)text, slot, time, append);
    put(" =");
    put_ptr(r);
    last_handle = r;
}

static void call_frames(uint32_t n)
{
    uint32_t i, r, msg;

    for (i = 0; i < n && !faulted; i++) {
        put("\n#frame");
        if (cur->frame == NULL) {
            put(" none");
            return;
        }
        if (counting) {
            cover_frames++;
            msg = first_msg();
            if (msg != 0) {
                uint32_t s = R32(msg, 8);
                if ((s == 1 || s == 2 || s == 3 || s == 6) && G32(0x1254) != 0) {
                    cover_scroll++;
                    /* the line drawn from before WINMSG's buffer */
                    if ((int32_t)R32(msg, 0x20) <= -8 && R32(msg, 0x1c) >> 3 < R32(msg, 4))
                        cover_before++;
                }
                else
                    cover_plain++;
            }
        }
        r = cur->frame(0, 0);
        put(" =%#x", r);
    }
}

/* From inside a 0x50 dispatch: what a listener might do next. */
static void reenter(void)
{
    uint32_t k = rnd_n(5), r;

    if (counting) cover_reentered++;
    put(" {");
    if (k < 3) {
        call_show(make_text(), make_slot(), make_time(), k == 0);
    } else if (k == 3) {
        r = ((s1)SVC(0x1c))(make_slot());
        put(" clear=");
        put_ptr(r);
    } else {
        r = ((s0)SVC(0x20))();
        put(" clearall=");
        put_ptr(r);
    }
    put(" }");
}

static uint8_t situation_record[0x10], situation_payload[0x24];

static void call_situation(uint32_t message, uint32_t data)
{
    uint32_t r;

    put("\n#situation(%#x)", message);
    if (cur->situation == NULL) {
        put(" none");
        return;
    }
    r = cur->situation(message, data);
    put(" =");
    put_ptr(r);
}

static int closed;

static void op(void)
{
    uint32_t k = rnd_n(100), r, i;

    if (counting) cover_ops[k < 30 ? 0 : k < 42 ? 1 : k < 60 ? 2 : k < 64 ? 3 : k < 67 ? 4 :
                            k < 70 ? 5 : k < 74 ? 6 : k < 78 ? 7 : k < 84 ? 8 : k < 90 ? 9 :
                            k < 93 ? 10 : k < 96 ? 11 : 12]++;
    if (k < 30) {                                  /* a message */
        call_show(make_text(), make_slot(), make_time(), 0);
    } else if (k < 42) {                           /* appended */
        call_show(make_text(), make_slot(), make_time(), 1);
    } else if (k < 60) {                           /* frames */
        call_frames(rnd_n(4) == 0 ? 50 + rnd_n(400) : 1 + rnd_n(12));
    } else if (k < 64) {
        r = ((s1)SVC(0x1c))(make_slot());
        put("\n#clear =");
        put_ptr(r);
    } else if (k < 67) {
        r = ((s0)SVC(0x20))();
        put("\n#clearall =");
        put_ptr(r);
    } else if (k < 70) {
        for (i = 0; i < 7; i++) {
            r = ((s1)SVC(0x24))(i);
            put("\n#queued(%u) =%#x", i, r);
        }
    } else if (k < 74) {                           /* the user resizes */
        put("\n#resize");
        if (in_arena(REC)) {
            host_width(REC, 1);
            if (rnd_n(3) == 0) host_surface(REC);
            put_rec(REC);
        }
    } else if (k < 78) {                           /* GLOBAL moves */
        g_move();
        put("\n#G %u %u", G32(0x1254), G16(0x362));
    } else if (k < 84) {                           /* a text exactly as wide as allowed */
        uint32_t n = 1 + rnd_n(60), j;
        int32_t d = (int32_t)rnd_n(3) - 1;

        if (in_arena(REC))
            R32(REC, 0x38) = n * 8 + 0x1e + (uint32_t)d;
        for (j = 0; j < n; j++)
            text_buf[j] = rnd_n(4) == 0 && j > 0 && j + 1 < n ? ' ' : (char)('a' + rnd_n(26));
        text_buf[n] = 0;
        if (rnd_n(3) == 0 && n + 2 < sizeof text_buf) {
            /* and a word past it, so the break lands on the edge */
            text_buf[n] = ' ';
            text_buf[n + 1] = 'z';
            text_buf[n + 2] = 0;
        }
        if (counting) cover_aimed_fit++;
        put("\n#fit %d", d);
        call_show(text_buf, make_slot(), make_time(), rnd_n(3) == 0);
    } else if (k < 90) {                           /* an append as a scroller's tail arrives */
        uint32_t msg = first_msg();

        if (msg != 0 && R32(msg, 0xc) != 0 && in_arena(REC)) {
            int32_t at = (int32_t)((R32(msg, 4) << 3) - R32(msg, 0x1c) + R32(msg, 0x20));
            int32_t w = at + 0x1e + (int32_t)rnd_n(3) - 1;
            R32(REC, 0x38) = (uint32_t)(w < 0x201e ? w : 0x201d);
            if (counting) cover_aimed_keep++;
            put("\n#aim");
            call_show(make_text(), R32(msg, 8), make_time(), 1);
        } else {
            call_frames(1 + rnd_n(5));
        }
    } else if (k < 93) {                           /* a situation's record in */
        uint32_t *rec = (uint32_t *)situation_record;
        rec[0] = rnd_n(4) == 0 ? 0x3de : 0x3df;
        rec[1] = 0x24;
        rec[3] = (uint32_t)(uintptr_t)situation_payload;
        /* the flags as saved, or anything; rectangles a window could have */
        *(uint32_t *)situation_payload = rnd_n(2) ? rnd() & 0xc0010000u : rnd();
        for (i = 4; i < 0x24; i += 4)
            *(uint32_t *)(situation_payload + i) = rnd_n(0x2400);
        if (counting) cover_restores++;
        call_situation(0x22, (uint32_t)(uintptr_t)rec);
    } else if (k < 96) {
        call_situation(0x23, 0);
    } else if (k < 97) {
        call_situation(rnd_n(2) ? 0x21 : 0x50 + rnd_n(4), 0);
    } else {
        /* the window closed: the sim's end, so only rarely anything after */
        if (rnd_n(4) == 0) {
            if (counting) cover_closes++;
            call_situation(0x37, 0);
            closed = 1;
        } else {
            call_frames(1);
        }
    }
}

/* The comparable state: the image with its code pointers paired, the arena
 * and GLOBAL. */
static uint8_t arena_copy[ARENA_SIZE];

static void put_state(void)
{
    uint8_t img[IMAGE_SIZE];
    uint32_t i;

    memcpy(img, cur->image, IMAGE_SIZE);
    for (i = 0x34; i < 0x58; i += 4)
        *(uint32_t *)(img + i) = pair_code(*(uint32_t *)(img + i));
    memcpy(arena_copy, arena, arena_used);
    put("\n  state img=%08x arena=%#x/%08x G=%08x",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena_copy, arena_used, 0x811c9dc5u),
        digest(global, GLOBAL_SIZE, 0x811c9dc5u));
    put(" texts");
    for (i = 0x78; i < 0x98; i += 4) {
        put(" ");
        put_ptr(*(uint32_t *)(img + i));
    }
    if (getenv("WINMSG_IMAGE")) {
        put("\n  whole");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(img + i));
    }
}

/* Everything the module maps but its .data image: written only by a
 * reach past the image, which must not carry over to the next scenario
 * unseen. */
static uint32_t outside_digest(struct side *s)
{
    uint32_t h = 0x811c9dc5u;
    uint8_t *img = s->image, *p;
    MEMORY_BASIC_INFORMATION mi;

    for (p = s->lo; p < s->hi; p += mi.RegionSize) {
        if (VirtualQuery(p, &mi, sizeof mi) == 0)
            break;
        if (mi.State != MEM_COMMIT || (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            continue;
        if (img >= p && img < p + mi.RegionSize) {
            h = digest(p, (uint32_t)(img - p), h);
            h = digest(img + IMAGE_SIZE, (uint32_t)(p + mi.RegionSize - img - IMAGE_SIZE), h);
        } else {
            h = digest(p, (uint32_t)mi.RegionSize, h);
        }
    }
    return h;
}

static void run_side(struct side *s)
{
    uint32_t i, nops, outside = outside_digest(s);

    cur = s;
    s->length = 0;
    s->frame = s->situation = NULL;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    memset(arena, 0, 0x100000);
    memset(global, 0, GLOBAL_SIZE);
    rng = scenario_seed;
    pad_byte = (uint8_t)(rnd_n(3) == 0 ? 0 : rnd_n(2) ? 0x20 + rnd_n(0x60) : rnd());
    screen_w = (uint32_t[]){ 640, 800, 1024, 1152, 1280, 1600 }[rnd_n(6)];
    g_move();
    last_handle = 0;
    in_dispatch = 0;
    closed = 0;
    put("scenario %u pad %#x screen %u G %u %u\n", scenario, pad_byte, screen_w,
        G32(0x1254), G16(0x362));
    faulted = 0;
    in_winmsg = 1;
    if (__builtin_setjmp(fault_jb) == 0) {
        uint32_t r = ((s0)SVC(4))();
        put(" =%#x", r);
    }
    in_winmsg = 0;
    put_state();
    /* A fault is the sim's end: the scenario stops there. */
    nops = 4 + rnd_n(MAX_OPS);
    for (i = 0; i < nops && !faulted && !(closed && rnd_n(3) != 0); i++) {
        op_number = i;
        in_winmsg = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            op();
        in_winmsg = 0;
        put_state();
    }
    if (!faulted) {
        put("\n#deinit:");
        in_winmsg = 1;
        if (__builtin_setjmp(fault_jb) == 0) {
            uint32_t r = ((s0)SVC(8))();
            put(" =%#x", r);
        }
        in_winmsg = 0;
        put_state();
    }
    put("\n");
    cur = NULL;
    if (outside_digest(s) != outside) {
        printf("scenario %u: %s WINMSG's memory outside its .data image changed\n",
               scenario, s->name);
        exit(4);
    }
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
            printf("\nFAULT in %s WINMSG at .text+%#x\n", s[i]->name,
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
    if (getenv("WINMSG_NOFILTER") == NULL)
        SetUnhandledExceptionFilter(on_fault);
    build_stubs();
    load_side(&retail, "WINMSG_retail.dll", "retail");
    load_side(&ours, "WINMSG_ours.dll", "ours");

    for (i = first; i < first + n; i++) {
        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        scenario_seed = rnd() | 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("WINMSG_DUMP") != NULL) {
            FILE *f = fopen("winmsg_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("winmsg_ours.txt", "wb");
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
    printf("winmsgdiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops:");
    for (i = 0; i < 13; i++)
        printf(" %lu", cover_ops[i]);
    printf("\n  frames %lu (plain %lu, scrolling %lu, of those drawn from before the line %lu), "
           "messages done %lu (the last shown %lu), re-entered from a dispatch %lu\n",
           cover_frames, cover_plain, cover_scroll, cover_before, cover_done, cover_done_last,
           cover_reentered);
    printf("  texts wider than the window %lu, aimed at the edge %lu, appends aimed at a "
           "tail %lu\n", cover_splits, cover_aimed_fit, cover_aimed_keep);
    printf("  appends: no message in the slot %lu, a plain one replaced %lu, a scroller's "
           "tail on screen %lu (and exactly at the edge %lu), a scroller extended %lu\n", cover_append[0],
           cover_append[1], cover_append[2], cover_append[4], cover_append[3]);
    printf("  windows narrower than 30 px %lu, within 30 px of the line's 0x2000 %lu, restores %lu, "
           "closes %lu\n", cover_narrow, cover_wide, cover_restores, cover_closes);
    printf("  faults (both sides alike, the scenario ends) %lu, of them heap %lu; retail's at:",
           cover_faults, cover_heap_faults);
    for (i = 0; i < 16 && fault_n[i]; i++)
        printf(" %s%#x:%lu", fault_at[i] == 0xffffffffu ? "elsewhere" : ".text+",
               fault_at[i] == 0xffffffffu ? 0 : fault_at[i], fault_n[i]);
    printf("\n");
    return differ ? 1 : 0;
}
