/* Differential test: retail REALITY.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven through Linkage and the
 * two CHAIN callbacks they register (lists 1 and 3, each frame), the way MAIN
 * and WINDOW drive them, one side after the other over the same script.
 * What REALITY imports:
 *
 *   host      a stub over a bump arena reset before each side, so heap
 *             pointers come out the same; unzeroed blocks filled with 0xa5,
 *             freed ones with 0xdd.  The dialog engine (+0x10c) is a
 *             stand-in for the crash analysis 0x14050001: it attaches the
 *             graph control, paints it, reads back what the dialog would
 *             show, lets it go and answers from the script.  +0x74 gives the
 *             control's record a size and a surface as the host's 0x467
 *             handler would.  The host arrays are the host's (+0x2f8, and
 *             +0x304 which empties one).
 *   UTIL      retail's, its state restored per side: +0x6c/+0x70 roll the
 *             stress damage, +0x90 fills the dialog's records, +0xc8 turns
 *             heights into doubles.
 *   NUMERIC   retail's, its state restored per side, for the labels.
 *   DEMO      +0x3c, the replay ring read back: heights from the script, -1
 *             for an index DEMO refuses.
 *   HG2D      the three line services, logged with every argument and the
 *             colour in GLOBAL +0x2ec: that is all the drawing there is.
 *   CHAIN, CONTROLS, MAIN, SOUND, WINDOW, WINMSG   logged.  MAIN +0x44 (the
 *             reload) moves GLOBAL the way a reload might; SOUND +0x24
 *             answers from the script and keeps the crash's callback, which
 *             the script calls later, as the sound ending would.
 *
 * GLOBAL is random per scenario and steered onto the crash checks: codes
 * 0..0x18 at +0x832 with each gate (+0x760, +0x18, +0x365, +0x270, +0x838,
 * +0x83c, +0x830, +0x84a) open or shut, the countdown at +0x838 through 7..0
 * with +0x830 2 so the reload leads to the analysis, the descent rate at
 * +0x2c8 about its clamps, and the stress gates and thresholds.  Each
 * scenario runs under a random x87 precision control.
 *
 * Compared after every operation: the transcript of every import call (with
 * the strings and bytes they carry), REALITY's whole .data image (code
 * pointers through a pairing learnt as they are first seen, pointers into
 * itself as offsets), the arena, GLOBAL, and UTIL's and NUMERIC's state.  A
 * fault ends the scenario on both sides.
 *
 * Build: make realitydiff     Run: make realitydiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x510u
#define GLOBAL_SIZE  0x1300u
#define ARENA_SIZE   (8u << 20)
#define TRANSCRIPT_SIZE (8u << 20)
#define MAX_OPS      60
#define MAX_PAIRS    48

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
    callback_fn list1, list3;
    char *transcript;
    uint32_t length;
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
static uint8_t chain[0x24], controls[0x60], demo[0x40], hg2d[0x40], mainmod[0x48],
    sound[0x28], window[0x5c], winmsg[0x1c];
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint32_t rng, scenario_seed;
static uint32_t scenario;
static uint32_t op_number;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs, npairs_ours;
static uint32_t sound_callback;
static uint8_t view_rec[0xa4], view_block[0xc0];

static unsigned long cover_ops[16], cover_crashes, cover_codes[0x20], cover_count[9],
    cover_dialogs, cover_dialog_one, cover_paints, cover_reloads, cover_resumes,
    cover_stress, cover_rand32, cover_cracks, cover_lines, cover_units[4], cover_faults,
    cover_spot, cover_scale_i[32];

#define G8(o)  (*(uint8_t *)(global + (o)))
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
    else if (b >= view_rec && b < view_rec + sizeof view_rec)
        put("view_rec+%#x", (uint32_t)(b - view_rec));
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
static volatile int in_reality;
static volatile int faulted;
static uint32_t fault_at[16];
static unsigned long fault_n[16];

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
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
    if (!in_reality || cur == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
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
        for (k = 0; k < 16 && fault_n[k] && fault_at[k] != where; k++)
            ;
        if (k < 16) {
            fault_at[k] = where;
            fault_n[k]++;
        }
    }
    in_reality = 0;
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

static int32_t STDCALL h_memset(void *d, int32_t byte, uint32_t n)
{
    put(" memset(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",%#x,%#x)", (uint32_t)byte & 0xff, n);
    if (d != NULL && n != 0)
        memset(d, byte & 0xff, n);
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

static uint32_t STDCALL h_strlen(const char *s)
{
    uint32_t n = s ? (uint32_t)strlen(s) : 0;
    put(" strlen=%u", n);
    return n;
}

static int32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    char t[32];

    snprintf(t, sizeof t, "s%04x", id);
    put(" loadstring(%#x,", id);
    put_ptr((uint32_t)(uintptr_t)buf);
    put(",%d)", len);
    if (len > 0) {
        strncpy(buf, t, (size_t)len - 1);
        buf[len - 1] = 0;
    }
    return (int32_t)strlen(t);
}

static int32_t STDCALL h_post(uint32_t command, uint32_t parameter)
{
    put(" post(%#x,%#x)", command, parameter);
    return 0;
}

/* the host's growable arrays */
struct host_list {
    uint32_t count, capacity, grow, elem_size, iterating, pending;
    uint8_t *data;
};

static void *STDCALL h_list_add(struct host_list *list)
{
    uint8_t *element;

    if (list->count == list->capacity) {
        uint32_t capacity = list->capacity + list->grow;
        uint32_t size = capacity * list->elem_size;
        uint8_t *data = (uint8_t *)arena_get(size);

        memset(data, 0xa5, size);
        if (list->data != NULL) {
            memcpy(data, list->data, list->count * list->elem_size);
            arena_release(list->data);
        }
        list->data = data;
        list->capacity = capacity;
    }
    element = list->data + list->count * list->elem_size;
    memset(element, 0, list->elem_size);
    list->count++;
    put(" listadd(");
    put_ptr((uint32_t)(uintptr_t)list);
    put(")=");
    put_ptr((uint32_t)(uintptr_t)element);
    return element;
}

/* +0x304: the count to 0, nothing else */
static int32_t STDCALL h_list_empty(struct host_list *list)
{
    put(" listempty(");
    put_ptr((uint32_t)(uintptr_t)list);
    put(")");
    list->count = 0;
    return 0;
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

/* +0x10c: the crash analysis.  Its block: +0 the result, +4 a check, +8 the
 * graph control, +0xc the units, +0x14 and +0x18 two texts, +0x1c/+0x20 and
 * +0x24/+0x28 the two axes' label counts and arrays, +0x2c the labels' flag,
 * +0x30 the graph's ranges. */
static int32_t STDCALL h_dialog(uint32_t id, uint8_t *block)
{
    uint32_t a = rnd(), r, i, n, paints;
    uint32_t hwnd = 0x7e570000u | (a & 0xff);

    put(" dialog(%#x)", id);
    if (counting) cover_dialogs++;
    put(" units=%#x", *(uint32_t *)(block + 0xc));
    put(" text=");
    put_s(*(const char **)(block + 0x14));
    put(" text2=");
    put_s(*(const char **)(block + 0x18));
    r = ((check_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
    put(" check=%#x", r);
    r = ((control_fn)*(uint32_t *)(block + 8))(0, 0, hwnd);
    put(" attached=%#x", r);
    paints = (a >> 8) % 3;
    for (i = 0; i < paints; i++) {
        if (counting) cover_paints++;
        r = ((control_fn)*(uint32_t *)(block + 8))(1, 0, hwnd);
        put(" painted=%#x", r);
    }
    if ((a >> 12) % 8 == 0) {
        r = ((control_fn)*(uint32_t *)(block + 8))(3 + (a >> 16) % 4, 0, hwnd);
        put(" other=%#x", r);
    }
    put(" labelled=%u", *(uint32_t *)(block + 0x2c));
    if (*(uint32_t *)(block + 0x2c)) {
        n = *(uint32_t *)(block + 0x1c);
        for (i = 0; i < n && i < 16; i++) {
            put(i ? "," : " x:");
            put_s(((const char **)(uintptr_t)*(uint32_t *)(block + 0x20))[i]);
        }
        n = *(uint32_t *)(block + 0x24);
        for (i = 0; i < n && i < 16; i++) {
            put(i ? "," : " y:");
            put_s(((const char **)(uintptr_t)*(uint32_t *)(block + 0x28))[i]);
        }
    }
    put(" graph");
    put_bytes(block + 0x30, 0x28);
    r = ((control_fn)*(uint32_t *)(block + 8))(2, 0, hwnd);
    put(" released=%#x", r);
    *(uint32_t *)block = (a >> 20) % 3 == 0;
    switch ((a >> 24) % 8) {
    case 0: case 1: case 2: r = 0; break;
    case 3: r = 1; break;
    case 4: r = 0x100; break;           /* its second byte is what ah reads */
    case 5: r = 0x1ff; break;
    case 6: r = 0x200; break;
    default: r = rnd(); break;
    }
    if (counting && ((r >> 8) & 0xff) == 1) cover_dialog_one++;
    put(" result=%#x,%u", r, *(uint32_t *)block);
    return (int32_t)r;
}

/* ---- the other modules -------------------------------------------------------------- */

static uint32_t STDCALL chain_add(uint32_t list, callback_fn cb)
{
    put(" chainadd(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    if (list == 1) cur->list1 = cb;
    else if (list == 3) cur->list3 = cb;
    return 0;
}

static uint32_t STDCALL chain_remove(uint32_t list, callback_fn cb)
{
    put(" chainremove(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    if (list == 1 && cur->list1 == cb) cur->list1 = NULL;
    else if (list == 3 && cur->list3 == cb) cur->list3 = NULL;
    return 0;
}

static uint32_t STDCALL controls_pause(uint32_t what, uint32_t on)
{
    put(" pause(%u,%u)", what, on);
    return 0;
}

static uint32_t height_mode, height_limit;

/* DEMO +0x3c: the replay ring `index` back, a height above the ground at
 * +0x12 (fraction) and +0x14 */
static uint32_t STDCALL demo_back(int32_t index, uint8_t *out)
{
    uint32_t k;

    put(" back(%d,", index);
    put_ptr((uint32_t)(uintptr_t)out);
    put(")");
    if (index >= 0 || rnd_n(20) == 0) {
        put("=-1");
        return 0xffffffffu;
    }
    k = rnd_n(8);
    memset(out, 0, 0x18);
    if (height_mode != 0) {
        /* whole units, so the range lands exactly: flat, one, or a scale
         * table's limit (a refused entry is a height of 0) */
        *(int32_t *)(out + 0x14) = height_mode == 1 ? 0 : height_mode == 2 ? (int32_t)rnd_n(2)
                                   : rnd_n(2) ? (int32_t)height_limit : 0;
        put("{0,%d}", *(int32_t *)(out + 0x14));
        return 0;
    }
    *(uint32_t *)(out + 4) = rnd();
    *(uint32_t *)(out + 0xc) = rnd();
    *(uint16_t *)(out + 0x12) = (uint16_t)rnd();
    /* Heights within 10^9 of each other: past a range of 2,545,942,528 the
     * scale search leaves .rdata (docs/modules/reality.md). */
    *(int32_t *)(out + 0x14) = k == 0 ? (int32_t)rnd_n(2000000000u) - 1000000000
                               : k == 1 ? -(int32_t)rnd_n(100)
                               : k == 2 ? 0x10000 + (int32_t)rnd_n(3)
                               : (int32_t)(G32(0x1240) + rnd_n(G32(0x1244) + 1));
    put("{%#x,%d}", *(uint16_t *)(out + 0x12), *(int32_t *)(out + 0x14));
    return 0;
}

/* HG2D draws through the record the surface belongs to (s is its +0x80),
 * clipped to that record's view block (+0x1c): its half-extents. */
static void put_view(uint32_t s)
{
    uint32_t rec = s - 0x80, view;

    if (!in_arena(rec) && !in_image(rec) && rec != (uint32_t)(uintptr_t)view_rec)
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

static uint32_t STDCALL main_grey(void)
{
    put(" grey");
    return 0;
}

static uint32_t STDCALL main_ungrey(void)
{
    put(" ungrey");
    return 0;
}

/* the reload: GLOBAL moves as a reloaded flight might */
static uint32_t STDCALL main_reload(void)
{
    put(" reload");
    if (counting) cover_reloads++;
    if (rnd_n(2)) G16(0x846) = (uint16_t)rnd_n(3);
    if (rnd_n(3) == 0) G16(0x830) = (uint16_t)rnd_n(4);
    if (rnd_n(3) == 0) G16(0x832) = (uint16_t)rnd_n(0x18);
    if (rnd_n(4) == 0) G16(0xc18) = (uint16_t)rnd_n(4);
    return 0;
}

static uint32_t STDCALL sound_event(uint32_t code, uint32_t arg)
{
    uint32_t k = rnd_n(4);

    put(" sound(%#x,", code);
    put_ptr(arg);
    if (code == 0xe && arg) {
        uint32_t *b = (uint32_t *)(uintptr_t)arg;
        put("{%#x,%#x,%#x,", b[0], b[1], b[2]);
        put_ptr(b[3]);
        put(",%#x}", b[4]);
        if (b[0] & 4)
            sound_callback = b[3];
    }
    put(")");
    return k == 0 ? 0 : arg;        /* SOUND returns its copy's address */
}

static uint32_t STDCALL window_mode(uint32_t rec, uint32_t mode)
{
    if (counting) cover_spot++;
    put(" setmode(");
    put_ptr(rec);
    put(",%u)", mode);
    return 0;
}

static uint32_t STDCALL winmsg_show(const char *text, uint32_t slot, uint32_t time,
                                    uint32_t append)
{
    put(" winmsg(");
    put_s(text);
    put(",%u,%#x,%u)", slot, time, append);
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
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(controls, 16); TRAPS(demo, 7); TRAPS(hg2d, 13);
    TRAPS(mainmod, 3); TRAPS(sound, 39); TRAPS(window, 10); TRAPS(winmsg, 22);
#undef TRAPS
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x03c, h_post);
    SET_SLOT(host, 0x074, h_attach);
    SET_SLOT(host, 0x078, h_release);
    SET_SLOT(host, 0x084, h_flush);
    SET_SLOT(host, 0x10c, h_dialog);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x218, h_memset);
    SET_SLOT(host, 0x2a0, h_loadstring);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b8, h_strlen);
    SET_SLOT(host, 0x2f8, h_list_add);
    SET_SLOT(host, 0x304, h_list_empty);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    *(uint32_t *)controls = 16;
    SET_SLOT(controls, 0x5c, controls_pause);
    *(uint32_t *)demo = 7;
    SET_SLOT(demo, 0x3c, demo_back);
    *(uint32_t *)hg2d = 13;
    SET_SLOT(hg2d, 0x2c, hg2d_h);
    SET_SLOT(hg2d, 0x30, hg2d_v);
    SET_SLOT(hg2d, 0x34, hg2d_line);
    *(uint32_t *)mainmod = 3;
    SET_SLOT(mainmod, 0x30, main_grey);
    SET_SLOT(mainmod, 0x34, main_ungrey);
    SET_SLOT(mainmod, 0x44, main_reload);
    *(uint32_t *)sound = 39;
    SET_SLOT(sound, 0x24, sound_event);
    *(uint32_t *)window = 10;
    SET_SLOT(window, 0x58, window_mode);
    *(uint32_t *)winmsg = 22;
    SET_SLOT(winmsg, 0x18, winmsg_show);
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
    if (s->imports == NULL || s->link == NULL || *(uint32_t *)s->link != 32) {
        printf("%s: not REALITY\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports - 0x48;
    if (s->link != s->image + 0xb0) {
        printf("%s: Linkage is not ImportTable+0x68\n", path);
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
        case 7:  im->link = demo; break;
        case 10: im->link = window; break;
        case 13: im->link = hg2d; break;
        case 16: im->link = controls; break;
        case 17: im->link = numeric.link; break;
        case 20: im->link = util.link; break;
        case 22: im->link = winmsg; break;
        case 39: im->link = sound; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* ---- GLOBAL --------------------------------------------------------------------- */

static uint32_t pick(const uint32_t *v, uint32_t n)
{
    return v[rnd_n(n)];
}

/* the view WINDOW's frame would have current: a 3D view, its block */
static void g_view(void)
{
    uint32_t w = rnd_n(8) == 0 ? rnd_n(4) : 50 + rnd_n(600), h = rnd_n(8) == 0 ? rnd_n(4) : 30 + rnd_n(400);

    memset(view_rec, 0, sizeof view_rec);
    memset(view_block, 0, sizeof view_block);
    *(uint32_t *)(view_rec + 4) = rnd_n(6) == 0 ? 2 : 3;
    *(uint32_t *)(view_rec + 0x80) = 0x5e5e0000u | rnd_n(0x100);
    *(int32_t *)(view_block + 0) = (int32_t)(w / 2);
    *(int32_t *)(view_block + 4) = (int32_t)(w / 2);
    *(int32_t *)(view_block + 8) = 1 - (int32_t)(h / 2);
    *(int32_t *)(view_block + 0xc) = (int32_t)(h / 2);
    if (rnd_n(10) == 0)
        *(uint32_t *)(view_block + rnd_n(4) * 4) = rnd();
    *(uint32_t *)(view_block + 0x10) = rnd_n(5) == 0 ? 1 : rnd_n(8) == 0 ? rnd_n(3) : 0;
    *(uint32_t *)(view_block + 0x14) = rnd_n(3) == 0 ? rnd_n(5) : 0;
    G32(0x1194) = rnd_n(12) == 0 ? 0 : (uint32_t)(uintptr_t)view_rec;
    G32(0x1198) = (uint32_t)(uintptr_t)view_block;
}

static void g_setup(void)
{
    uint32_t i;

    for (i = 0; i < GLOBAL_SIZE; i += 4)
        G32(i) = rnd_n(3) == 0 ? rnd() : 0;
    g_view();
    G16(0xc18) = (uint16_t)(rnd_n(8) == 0 ? rnd_n(6) : rnd_n(3));
    /* a height spread for the replay, kept inside the scale tables */
    G32(0x1240) = rnd_n(3) == 0 ? (uint32_t)-(int32_t)rnd_n(200) : rnd_n(5000);
    {
        static const uint32_t limits[] = { 3, 7, 15, 30, 75, 150, 300, 750, 1500, 3000,
                                           7500, 65535, 10, 25, 1000, 65335 };
        height_mode = rnd_n(3) == 0 ? 1 + rnd_n(3) : 0;
        height_limit = limits[rnd_n(16)] + rnd_n(3) - 1;
    }
    G32(0x1244) = pick((const uint32_t[]){ 0, 1, 3, 10, 11, 25, 100, 1000, 1001,
                                           3000, 9999, 20000, 65535 }, 13);
}

/* the crash gates, each now and then open */
static void g_crash(void)
{
    static const uint32_t codes[] = { 0, 2, 4, 6, 8, 0xa, 0xc, 0xe, 0x10, 0x12, 0x14,
                                      0x16, 1, 3, 0x15, 0x18, 0x20, 0xff };

    G16(0x832) = (uint16_t)(rnd_n(4) == 0 ? rnd() : pick(codes, sizeof codes / 4));
    G32(0x760) = rnd_n(3) == 0;
    G16(0x18) = (uint16_t)(rnd_n(4) == 0 ? 5 : rnd_n(8));
    G8(0x365) = rnd_n(6) == 0;
    G32(0x270) = rnd_n(6) == 0;
    G8(0x838) = (uint8_t)(rnd_n(3) == 0 ? rnd_n(9) : rnd_n(12) == 0 ? rnd() : 0);
    G32(0x83c) = rnd_n(4) == 0 ? rnd_n(3) : rnd_n(20) == 0 ? rnd() : 0;
    G16(0x830) = (uint16_t)(rnd_n(5) == 0 ? 0 : rnd_n(2) ? 2 : rnd_n(4));
    G16(0x84a) = rnd_n(2);
    G32(0xb24) = rnd_n(2);
    G16(0x846) = (uint16_t)rnd_n(3);
    G32(0x82c) = rnd_n(3) != 0;
    G16(0x5dc) = rnd_n(5) == 0;
    G16(0x344) = rnd_n(2);
    G32(0x2b4) = rnd_n(2) ? 0x7fff + rnd_n(3) - 1 : rnd();
    {
        static const int32_t edges[] = { 0xa674, 0xa675, 0xa673, -0xa674, -0xa675,
                                         -0xa673, 0x1a674, 0x10000, -0x10000, 0 };
        G32(0x2c8) = rnd_n(2) ? (uint32_t)edges[rnd_n(10)] : rnd();
    }
    G32(0x2b8) = rnd();
    G16(0xc46) = (uint16_t)rnd();
    G8(0x280) = rnd_n(2);
    G16(0x372) = (uint16_t)(rnd_n(2) ? rnd_n(0x70) : rnd());
    G32(0x258) = rnd();
    for (int k = 0xb64; k < 0xb74; k++)
        G8(k) = (uint8_t)(rnd_n(3) == 0 ? rnd_n(3) : 0);
}

/* ---- driving ------------------------------------------------------------------------- */

typedef uint32_t (STDCALL *s0)(void);
typedef void (STDCALL *v0)(void);

#define SVC(off) (*(void **)(cur->link + (off)))

static void frames(uint32_t n)
{
    uint32_t i;

    for (i = 0; i < n && !faulted; i++) {
        if (counting) {
            uint32_t c = G8(0x838);
            cover_count[c < 8 ? c : 8]++;
            if (G8(0x832)) cover_codes[G8(0x832) & 0x1f]++;
            cover_units[G16(0xc18) < 3 ? G16(0xc18) : 3]++;
        }
        put("\n#frame");
        if (rnd_n(2) && cur->list1) {
            put(" [1]");
            cur->list1(0, 0);
        }
        if (cur->list3) {
            put(" [3]");
            cur->list3(0, 0);
        }
        put(" G838=%u 832=%#x 830=%#x 846=%#x 842=%#x 844=%#x 348=%u", G8(0x838), G16(0x832),
            G16(0x830), G16(0x846), G16(0x842), G16(0x844), G32(0x348));
        if (rnd_n(12) == 0)
            g_crash();
    }
}

static void op(void)
{
    uint32_t k = rnd_n(100);

    if (counting) cover_ops[k < 40 ? 0 : k < 60 ? 1 : k < 70 ? 2 : k < 78 ? 3 : k < 86 ? 4 :
                            k < 92 ? 5 : 6]++;
    if (k < 40) {                                   /* frames, the gates as they are */
        frames(1 + rnd_n(rnd_n(4) == 0 ? 60 : 8));
    } else if (k < 60) {                            /* a crash, or not */
        g_crash();
        put("\n#crash 832=%#x 838=%u 83c=%#x", G16(0x832), G8(0x838), G32(0x83c));
        frames(1 + rnd_n(12));
    } else if (k < 70) {                            /* the countdown on its way to the analysis */
        G8(0x838) = (uint8_t)(1 + rnd_n(4));
        G16(0x830) = 2;
        G32(0x83c) = 0;
        if (rnd_n(2)) *(uint32_t *)(cur->image + 0x2c) = 0;   /* the sound has ended */
        put("\n#countdown %u", G8(0x838));
        frames(1 + rnd_n(6));
    } else if (k < 78) {                            /* the windscreen */
        g_view();
        if (counting) cover_cracks++;
        put("\n#crack");
        ((v0)SVC(0x1c))();
    } else if (k < 86) {                            /* the damage cleared */
        for (int f = 0xb64; f < 0xb74; f++)
            G8(f) = (uint8_t)rnd_n(3);
        G8(0x374) = (uint8_t)rnd();
        G8(0x364) = (uint8_t)rnd();
        put("\n#clear");
        ((v0)SVC(0x18))();
    } else if (k < 92) {                            /* the crash's sound ends */
        put("\n#sounddone");
        if (sound_callback && in_module(sound_callback)) {
            put(" ");
            put_ptr(sound_callback);
            ((v0)(uintptr_t)sound_callback)();
            sound_callback = 0;
        }
    } else {                                        /* GLOBAL moves; stress at its odds */
        g_crash();
        G16(0x372) = (uint16_t)rnd_n(0x66);
        G16(0xc46) |= 8;
        G8(0x280) = 1;
        G16(0x5dc) = 0;
        *(uint32_t *)(cur->image + 0x28) = rnd_n(3);
        if (counting) cover_stress++;
        put("\n#stress");
        frames(1 + rnd_n(4));
    }
}

/* The comparable state: the image with its code pointers paired and its
 * pointers into itself as offsets, the arena, GLOBAL, UTIL and NUMERIC. */
static void put_state(void)
{
    static const uint32_t code[] = { 0, 4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0xb4, 0xb8, 0xc8,
                                     0xcc, 0x22c, 0x230, 0x2a0 };
    static const uint32_t self[] = { 0x23c, 0x240, 0x248, 0x250 };
    uint8_t img[IMAGE_SIZE];
    uint32_t i, base = (uint32_t)(uintptr_t)cur->image;

    memcpy(img, cur->image, IMAGE_SIZE);
    for (i = 0; i < sizeof code / 4; i++)
        *(uint32_t *)(img + code[i]) = pair_code(*(uint32_t *)(img + code[i]));
    /* the four relocated, and the label arrays the labels fill in */
    (void)self;
    for (i = 0; i + 4 <= IMAGE_SIZE; i += 4) {
        uint32_t v = *(uint32_t *)(img + i);
        if (v >= base && v < base + IMAGE_SIZE)
            *(uint32_t *)(img + i) = 0xd0000000u | (v - base);
    }
    /* the sound block's callback, when it is there */
    *(uint32_t *)(img + 0x3c) = in_module(*(uint32_t *)(img + 0x3c))
                                ? pair_code(*(uint32_t *)(img + 0x3c)) : *(uint32_t *)(img + 0x3c);
    put("\n  state img=%08x arena=%#x/%08x G=%08x util=%08x numeric=%08x",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena, arena_used, 0x811c9dc5u), digest(global, GLOBAL_SIZE, 0x811c9dc5u),
        helper_digest(&util), helper_digest(&numeric));
    if (getenv("REALITY_IMAGE")) {
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
    s->list1 = s->list3 = NULL;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    helper_restore(&util);
    helper_restore(&numeric);
    arena_used = 0;
    memset(arena, 0, 0x100000);
    rng = scenario_seed;
    sound_callback = 0;
    pc = (uint32_t[]){ 0, 2, 3, 3 }[rnd_n(4)];
    g_setup();
    g_crash();
    put("scenario %u pc %u units %u\n", scenario, pc, G16(0xc18));
    faulted = 0;
    in_reality = 1;
    set_precision(pc);
    if (__builtin_setjmp(fault_jb) == 0) {
        uint32_t r = ((s0)SVC(4))();
        put(" =%#x", r);
    }
    in_reality = 0;
    put_state();
    nops = 4 + rnd_n(MAX_OPS);
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        in_reality = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            op();
        in_reality = 0;
        put_state();
    }
    if (!faulted) {
        put("\n#deinit:");
        in_reality = 1;
        if (__builtin_setjmp(fault_jb) == 0) {
            uint32_t r = ((s0)SVC(8))();
            put(" =%#x", r);
        }
        in_reality = 0;
        put_state();
    }
    set_precision(3);
    put("\n");
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
            printf("\nFAULT in %s REALITY at .text+%#x\n", s[i]->name,
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
    if (getenv("REALITY_NOFILTER") == NULL)
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
    load_side(&retail, "REALITY_retail.dll", "retail");
    load_side(&ours, "REALITY_ours.dll", "ours");

    for (i = first; i < first + n; i++) {
        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        scenario_seed = rnd() | 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("REALITY_DUMP") != NULL) {
            FILE *f = fopen("reality_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("reality_ours.txt", "wb");
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
    printf("realitydiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops:");
    for (i = 0; i < 7; i++)
        printf(" %lu", cover_ops[i]);
    printf("\n  countdown at a frame (0..7, more):");
    for (i = 0; i < 9; i++)
        printf(" %lu", cover_count[i]);
    printf("\n  crash codes at a frame:");
    for (i = 0; i < 0x20; i++)
        if (cover_codes[i])
            printf(" %#x:%lu", i, cover_codes[i]);
    printf("\n  units at a frame (0, 1, 2, other): %lu %lu %lu %lu\n", cover_units[0],
           cover_units[1], cover_units[2], cover_units[3]);
    printf("  reloads %lu, analyses %lu (paints %lu, a second byte of 1 %lu), spot views %lu, "
           "cracks %lu, stress ops %lu, lines %lu\n", cover_reloads, cover_dialogs, cover_paints,
           cover_dialog_one, cover_spot, cover_cracks, cover_stress, cover_lines);
    printf("  faults (both sides alike, the scenario ends) %lu; retail's at:", cover_faults);
    for (i = 0; i < 16 && fault_n[i]; i++)
        printf(" %s%#x:%lu", fault_at[i] == 0xffffffffu ? "elsewhere" : ".text+",
               fault_at[i] == 0xffffffffu ? 0 : fault_at[i], fault_n[i]);
    printf("\n");
    (void)cover_crashes; (void)cover_resumes; (void)cover_rand32; (void)cover_scale_i;
    return differ ? 1 : 0;
}
