/* Differential test: retail WINDOW.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven through Linkage and the
 * CHAIN list 0x14 callback they register, the way MAIN, CONTROLS, PANELS,
 * WINMSG, G3D and REALITY drive them, one side after the other over the same
 * script.  What WINDOW imports:
 *
 *   host      a stub.  Memory is a bump arena reset before each side, so heap
 *             pointers come out the same; what the host does not zero is
 *             filled with 0xa5, and what it frees with 0xdd.  The window
 *             services are a model of the host's: +0x64 gives a record its
 *             0x90-byte block (+0xa0) and puts it at the front of its group
 *             (+0x78) in the z-order, +0x68 takes it out, +0x90 raises it,
 *             +0x94 finds the nth shown record of a group, and the size a
 *             window gets (+0x84/+0x88, +0x60/+0x64) follows the host's
 *             WM_SIZE handler: halved for +0x10000, clamped by +0x68..+0x74,
 *             odd for a record with a view block that is not a map.  The
 *             growable arrays are the host's (+0x2f8..+0x310).  Strings
 *             (+0x2a4) are made up from their ids; the stars file is the
 *             CD's or a synthetic one, or not there.
 *   UTIL      retail's, its alert (+0xa4) logged
 *   CHAIN     a logging stub that keeps list 0x14's callback; list 0x13
 *             sometimes claims the eye
 *   MAIN, G3D, HG2D, PANELS, CONTROLS, REALITY, ACONTAIN, NUMERIC
 *             logging stubs; MAIN +0x9c, G3D +0x34 and HG2D +0xbc answer
 *             from the script, G3D +0x3c normalises as G3D does (the top
 *             bit to bit 28), NUMERIC writes the format and the value, and
 *             ACONTAIN +0x30 a path.  GLOBAL +0x11e0 is an aircraft object
 *             whose callback logs.
 *
 * A scenario inits the module and runs a script of operations: 3D views
 * created (+0x3c), past the limit of 32 too, and records of other kinds
 * (+0x78) with their own paint callbacks; destroyed one by one, by record
 * (+0x7c) and all at once (+0x44, +0x18); shown, hidden, maximised over the
 * panel, made plain, activated; their kind and mode set and stepped; listed,
 * walked forward and back, found by number; resized as the host would;
 * painted (+0x1c) with GLOBAL steered through the cockpit, virtual cockpit,
 * tower, track, spot and map eyes and every transition between them, frame
 * after frame; the situation's records loaded (message 0x22: every old
 * record type and the 0x3de layout), restored (0x21), saved (0x23), and a
 * new aircraft's geometry (0x60); the matrix services (+0x20..+0x30) on
 * random matrices and angles; and deinit.
 *
 * Compared after every operation: the transcript of every import call (with
 * the strings and bytes they carry), the registers a service returns,
 * WINDOW's whole .data image (code pointers through a pairing learnt as they
 * are first seen, pointers into the image as offsets), the arena (records'
 * paint callbacks through the pairing) and GLOBAL.  Faults inside WINDOW or
 * UTIL are outcomes: logged, and the op abandoned on both sides.
 *
 * Kept clear of, as the real callers do or as retail faults or reads
 * garbage:
 *   - a view block's kind (+0x10) other than 0 or 1, or mode (+0x14) past
 *     4: the title is then built on an uninitialised stack buffer;
 *   - message 0x21 with nothing loaded since the last: it reads the first
 *     entry of a null array (both sides fault; it is let through now and
 *     then and compared as an outcome);
 *   - a stars file shorter than its own length word, which allocates ~4GB
 *     with the fatal flag.
 *
 * Build: make windowdiff     Run: make windowdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x8acu
#define GLOBAL_SIZE  0x1300u
#define ARENA_SIZE   (16u << 20)
#define TRANSCRIPT_SIZE (24u << 20)
#define MAX_OPS      60
#define MAX_PAIRS    128
#define MAX_RECS     64

struct import { uint32_t id; void *link; };

typedef void (STDCALL *callback_fn)(uint32_t, uint32_t);
typedef void (STDCALL *paint_fn)(uint32_t, uint32_t, uint32_t, uint32_t);

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    uint8_t *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    callback_fn chain14;
    char *transcript;
    uint32_t length;
};

static struct side retail, ours;
static struct side *cur;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t chain[0x24], controls[0x48], mainmod[0xc8], numeric[0x20],
    hg2d[0xc0], g3d[0x60], panels[0x30], reality[0x20], acontain[0x34],
    util_proxy[0xc4];
static uint8_t *util_link;
static uint8_t *util_lo, *util_hi;
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint32_t rng, scenario_seed;
static uint32_t scenario;
static uint32_t op_number;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs, npairs_ours;
static const char *volatile fault_side = "none";
static char cd_root[MAX_PATH];
static uint8_t cd_stars[0x2000];
static uint32_t cd_stars_size;

/* the scenario's answers, drawn in order on each side alike */
static uint32_t answers[4096];
static uint32_t answer_at;

static unsigned long cover_ops[40], cover_frames, cover_paint3d, cover_map,
    cover_modes[5], cover_states[0x60], cover_faults, cover_texts, cover_layers,
    cover_stars, cover_model, cover_alerts, cover_restored, cover_records[0x20],
    cover_geometry_moved, cover_claimed, cover_pairs[5][5], cover_high[11];

#define G8(o)  (*(uint8_t *)(global + (o)))
#define G16(o) (*(uint16_t *)(global + (o)))
#define G32(o) (*(uint32_t *)(global + (o)))

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

static uint32_t answer(void)
{
    return answers[answer_at++ & 4095];
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

static void put_s(const char *s)
{
    if (s == NULL)
        put("null");
    else
        put("'%.200s'", s);
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

static int in_arena(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;
    return b >= arena && b < arena + ARENA_SIZE;
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
        put("module");
    else if (v < 0x10000)
        put("%#x", v);
    else
        put("other");
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

/* ---- the arena ------------------------------------------------------------------ */

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
    *(uint32_t *)p = size;
    *(uint32_t *)(p + 4) = 0x0a110c;
    return p + 16;
}

static void arena_release(void *p)
{
    uint8_t *b = (uint8_t *)p;

    if (in_arena((uint32_t)(uintptr_t)b) && *(uint32_t *)(b - 12) == 0x0a110c) {
        memset(b, 0xdd, *(uint32_t *)(b - 16));
        *(uint32_t *)(b - 12) = 0xf4ee;
    }
}

/* ---- the stub host -------------------------------------------------------------- */

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p = arena_get(size);

    memset(p, zero ? 0 : 0xa5, size);
    put(" alloc(%d,%#x,", fl, size);
    put_ptr((uint32_t)(uintptr_t)out);
    put(")=");
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

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    put(" memmove(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_ptr((uint32_t)(uintptr_t)s);
    put(",%#x)", n);
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

static int32_t STDCALL h_strcmp(const char *a, const char *b)
{
    int r = strcmp(a, b);
    r = r < 0 ? -1 : r > 0;
    put(" strcmp(");
    put_s(a);
    put(",");
    put_s(b);
    put(")=%d", r);
    return r;
}

static char *STDCALL h_strdup(const char *s)
{
    uint32_t n = (uint32_t)strlen(s) + 1;
    char *p = (char *)arena_get(n);

    memcpy(p, s, n);
    put(" strdup(");
    put_s(s);
    put(")=");
    put_ptr((uint32_t)(uintptr_t)p);
    return p;
}

/* +0x2a4: string `id`, copied */
static int32_t STDCALL h_string(uint32_t id, char **out)
{
    char t[64];
    uint32_t n;

    switch (id) {
    /* ENGLISH.FLL's, for the ones the titles are made of */
    case 0x9240: strcpy(t, "View "); break;
    case 0x9241: strcpy(t, "COCKPIT"); break;
    case 0x9242: strcpy(t, "VIRTUAL COCKPIT"); break;
    case 0x9243: strcpy(t, "TOWER"); break;
    case 0x9244: strcpy(t, "SPOT"); break;
    case 0x9245: strcpy(t, "TRACK"); break;
    case 0x9246: strcpy(t, "MAP"); break;
    case 0x9268: strcpy(t, "  "); break;
    case 0x9269: strcpy(t, " - "); break;
    default: snprintf(t, sizeof t, "s%04x", id); break;
    }
    n = (uint32_t)strlen(t);
    *out = (char *)arena_get(n + 1);
    memcpy(*out, t, n + 1);
    put(" string(%#x)=", id);
    put_ptr((uint32_t)(uintptr_t)*out);
    return (int32_t)n;
}

/* ---- the host's growable arrays ---------------------------------------------- */

struct host_list {
    uint32_t count, capacity, grow, elem_size, iterating, pending;
    uint8_t *data;
};

static void put_list(struct host_list *l)
{
    put_ptr((uint32_t)(uintptr_t)l);
}

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
            arena_release(list->data);     /* realloc moved it */
        }
        list->data = data;
        list->capacity = capacity;
    }
    element = list->data + list->count * list->elem_size;
    memset(element, 0, list->elem_size);
    list->count++;
    put(" listadd(");
    put_list(list);
    put(")=");
    put_ptr((uint32_t)(uintptr_t)element);
    return element;
}

static void STDCALL h_list_remove(struct host_list *list, void *element)
{
    uint32_t index = (uint32_t)((uint8_t *)element - list->data) / list->elem_size;

    put(" listremove(");
    put_list(list);
    put(",%u)", index);
    if (index >= list->count)
        return;
    if (index + 1 < list->count)
        memmove(list->data + index * list->elem_size,
                list->data + (index + 1) * list->elem_size,
                (list->count - index - 1) * list->elem_size);
    list->count--;
}

static int32_t STDCALL h_list_free(struct host_list *list)
{
    put(" listfree(");
    put_list(list);
    put(")");
    if (list->data != NULL)
        arena_release(list->data);
    list->data = NULL;
    return 0;
}

typedef int32_t (STDCALL *walk_fn)(void *, void *);

static int32_t STDCALL h_list_walk(struct host_list *list, void *ctx, walk_fn fn)
{
    uint32_t i;
    int32_t r = 0;

    put(" listwalk(");
    put_list(list);
    put(",code%u)", pair_code((uint32_t)(uintptr_t)fn) & 0xffff);
    list->iterating++;
    for (i = 0; i < list->count; i++)
        if ((r = fn(list->data + i * list->elem_size, ctx)) != 0)
            break;
    list->iterating--;
    return r;
}

static void *STDCALL h_list_find(struct host_list *list, uint32_t offset, const void *key,
                                 uint32_t len, int32_t indirect)
{
    uint32_t i;
    void *r = NULL;

    for (i = 0; i < list->count; i++) {
        uint8_t *e = list->data + i * list->elem_size + offset;
        if (indirect ? memcmp(*(void **)e, key, len) == 0 : memcmp(e, key, len) == 0) {
            r = list->data + i * list->elem_size;
            break;
        }
    }
    put(" listfind(");
    put_list(list);
    put(",%#x,", offset);
    put_bytes(key, len);
    put(",%d)=", indirect);
    put_ptr((uint32_t)(uintptr_t)r);
    return r;
}

/* ---- the host's windows ------------------------------------------------------- */

/* The z-order: groups by record +0x78, each a list front first. */
struct zgroup {
    uint32_t key;
    uint32_t n;
    uint32_t rec[MAX_RECS];
};
static struct zgroup zorder[8];
static uint32_t ngroups;

static struct zgroup *zgroup_of(uint32_t key, int make)
{
    uint32_t i;

    for (i = 0; i < ngroups; i++)
        if (zorder[i].key == key)
            return &zorder[i];
    if (!make || ngroups == 8)
        return NULL;
    zorder[ngroups].key = key;
    zorder[ngroups].n = 0;
    return &zorder[ngroups++];
}

static int zfind(struct zgroup *g, uint32_t rec)
{
    uint32_t i;

    for (i = 0; g && i < g->n; i++)
        if (g->rec[i] == rec)
            return (int)i;
    return -1;
}

static void zfront(struct zgroup *g, uint32_t rec)
{
    int at = zfind(g, rec);
    uint32_t i;

    if (at < 0) {
        if (g->n == MAX_RECS) return;
        at = (int)g->n++;
    }
    for (i = (uint32_t)at; i > 0; i--)
        g->rec[i] = g->rec[i - 1];
    g->rec[0] = rec;
}

static void zremove(uint32_t rec)
{
    uint32_t i, j;

    for (i = 0; i < ngroups; i++) {
        int at = zfind(&zorder[i], rec);
        if (at >= 0) {
            for (j = (uint32_t)at; j + 1 < zorder[i].n; j++)
                zorder[i].rec[j] = zorder[i].rec[j + 1];
            zorder[i].n--;
        }
    }
}

#define R32(rec, off) (*(uint32_t *)(uintptr_t)((rec) + (off)))

/* The host's WM_SIZE: a client size into the record. */
static void host_size(uint32_t rec, int32_t w, int32_t h)
{
    if (R32(rec, 8) & 0x10000) {
        w /= 2;
        h /= 2;
    }
    R32(rec, 0x84) = (uint32_t)w;
    R32(rec, 0x88) = (uint32_t)h;
    R32(rec, 0x60) = (uint32_t)w;
    R32(rec, 0x64) = (uint32_t)h;
    if (R32(rec, 0x68) && w < (int32_t)R32(rec, 0x68))
        R32(rec, 0x84) = R32(rec, 0x60) = R32(rec, 0x68);
    if (R32(rec, 0x70) && (int32_t)R32(rec, 0x84) > (int32_t)R32(rec, 0x70))
        R32(rec, 0x84) = R32(rec, 0x60) = R32(rec, 0x70);
    if (R32(rec, 0x6c) && h < (int32_t)R32(rec, 0x6c))
        R32(rec, 0x88) = R32(rec, 0x64) = R32(rec, 0x6c);
    if (R32(rec, 0x74) && (int32_t)R32(rec, 0x88) > (int32_t)R32(rec, 0x74))
        R32(rec, 0x88) = R32(rec, 0x64) = R32(rec, 0x74);
    if (R32(rec, 0x1c) != 0 && R32(rec, 4) != 2) {
        if (!(R32(rec, 0x84) & 1)) R32(rec, 0x84)++;
        if (!(R32(rec, 0x88) & 1)) R32(rec, 0x88)++;
    }
}

/* A client size from the record's rectangle (fractions of 0x1000 of a
 * 1024x768 screen), or now and then an odd one. */
static void host_size_from_rect(uint32_t rec)
{
    int32_t w = (int32_t)(R32(rec, 0x28) * 1024u >> 12), h = (int32_t)(R32(rec, 0x2c) * 768u >> 12);
    uint32_t k = answer() % 16;

    if (k == 0) { w = (int32_t)(answer() % 5); h = (int32_t)(answer() % 5); }
    else if (k == 1) { w = (int32_t)(answer() % 4000); h = (int32_t)(answer() % 3000); }
    else if (k == 2) { w = (int32_t)answer(); h = (int32_t)answer(); }
    host_size(rec, w, h);
}

static void put_rec(uint32_t rec)
{
    put_ptr(rec);
    if (in_arena(rec))
        put("{%#x,%#x,%#x %d,%d,%d,%d}", R32(rec, 4), R32(rec, 8), R32(rec, 0xc),
            R32(rec, 0x20), R32(rec, 0x24), R32(rec, 0x28), R32(rec, 0x2c));
}

static int32_t STDCALL h_win_create(uint32_t rec)
{
    void *block;

    put(" wincreate(");
    put_rec(rec);
    if (in_arena(rec) && R32(rec, 0))
        put_s((const char *)(uintptr_t)R32(rec, 0));
    put(")");
    if (rec == 0)
        return -1;
    block = arena_get(0x90);
    memset(block, 0, 0x90);
    R32(rec, 0xa0) = (uint32_t)(uintptr_t)block;
    zfront(zgroup_of(R32(rec, 0x78), 1), rec);
    host_size_from_rect(rec);
    return 0;
}

static int32_t STDCALL h_win_destroy(uint32_t rec)
{
    put(" windestroy(");
    put_rec(rec);
    put(")");
    if (rec == 0)
        return 0;
    zremove(rec);
    if (R32(rec, 0xa0)) {
        arena_release((void *)(uintptr_t)R32(rec, 0xa0));
        R32(rec, 0xa0) = 0;
    }
    return 0;
}

static int32_t STDCALL h_paint_begin(uint32_t rec)
{
    put(" paintbegin(");
    put_ptr(rec);
    put(")");
    return 0;
}

static int32_t STDCALL h_paint_end(uint32_t rec)
{
    put(" paintend(");
    put_ptr(rec);
    put(")");
    return 0;
}

static int32_t STDCALL h_win_update(uint32_t rec)
{
    put(" winupdate(");
    put_rec(rec);
    put(")");
    if (rec != 0 && (answer() % 4) == 0)
        host_size_from_rect(rec);
    return 0;
}

static int32_t STDCALL h_win_title(uint32_t rec)
{
    put(" wintitle(");
    put_ptr(rec);
    if (rec)
        put_s((const char *)(uintptr_t)R32(rec, 0));
    put(")");
    return 0;
}

static int32_t STDCALL h_win_flush(uint32_t rec)
{
    put(" winflush(");
    put_ptr(rec);
    put(")");
    return 0;
}

static int32_t STDCALL h_text(uint32_t rec, uint32_t where, uint32_t colour, const char *text,
                              uint32_t opaque)
{
    put(" text(");
    put_ptr(rec);
    put(",%u,%#x,", where, colour);
    put_ptr((uint32_t)(uintptr_t)text);
    put_s(text);
    put(",%u)", opaque);
    if (counting) cover_texts++;
    return 0;
}

static int32_t STDCALL h_raise(uint32_t rec)
{
    uint32_t i;

    put(" raise(");
    put_ptr(rec);
    put(")");
    for (i = 0; i < ngroups; i++)
        if (zfind(&zorder[i], rec) >= 0) {
            zfront(&zorder[i], rec);
            return 0;
        }
    return -1;
}

static int32_t STDCALL h_nth(uint32_t *out, uint32_t key, uint32_t n)
{
    struct zgroup *g = zgroup_of(key, 0);
    uint32_t i, k = 0;

    put(" nth(");
    put_ptr((uint32_t)(uintptr_t)out);
    put(",%#x,%u)=", key, n);
    for (i = 0; g && i < g->n; i++)
        if (R32(g->rec[i], 8) & 0x80000000u) {
            if (k == n) {
                *out = g->rec[i];
                put_ptr(*out);
                return 0;
            }
            k++;
        }
    *out = 0;
    put("none");
    return -1;
}

static int32_t sizes[4];

static int32_t STDCALL h_profile_int(const char *section, const char *key, int32_t *inout,
                                     const char *path, int32_t ext)
{
    put(" profile(");
    put_s(section);
    put(",");
    put_s(key);
    put(",%d,", *inout);
    put_s(path);
    put(",%#x)", ext);
    if (strcmp(key, "X") == 0 && sizes[0] >= 0) *inout = sizes[0];
    else if (strcmp(key, "Y") == 0 && sizes[1] >= 0) *inout = sizes[1];
    else if (strcmp(key, "SIZE_X") == 0 && sizes[2] >= 0) *inout = sizes[2];
    else if (strcmp(key, "SIZE_Y") == 0 && sizes[3] >= 0) *inout = sizes[3];
    put("=%d", *inout);
    return 0;
}

/* The stars file, the only file WINDOW opens. */
static uint8_t stars[0x2000];
static uint32_t stars_size, stars_at;
static int stars_there;
static uint32_t stars_handle = 0x5f5f0001;

static int32_t STDCALL h_open(uint32_t *out, const char *path, int32_t ext, int32_t mode)
{
    put(" open(");
    put_s(path);
    put(",%d,%d)", ext, mode);
    if (!stars_there) {
        put("=fail");
        return -1;
    }
    stars_at = 0;
    *out = stars_handle;
    return 0;
}

static int32_t STDCALL h_read(uint32_t f, void *buf, uint32_t n, uint32_t *got)
{
    uint32_t k = n;

    if (f != stars_handle) {
        printf("read of a file that is not open\n");
        exit(2);
    }
    if (stars_at + k > stars_size)
        k = stars_at < stars_size ? stars_size - stars_at : 0;
    memcpy(buf, stars + stars_at, k);
    stars_at += k;
    *got = k;
    put(" read(%#x)=%#x", n, k);
    return 0;
}

static int32_t STDCALL h_close(uint32_t f)
{
    put(" close(%s)", f == stars_handle ? "stars" : "?");
    return 0;
}

static uint32_t active_answer;

static int32_t STDCALL h_active(void)
{
    put(" active=%u", active_answer);
    return (int32_t)active_answer;
}

static int32_t STDCALL h_menu_add(uint32_t command, uint32_t id, const char *text, uint32_t z)
{
    put(" menuadd(%#x,%#x,", command, id);
    put_s(text);
    put(",%u)", z);
    return 0;
}

static int32_t STDCALL h_menu_remove(uint32_t id)
{
    put(" menuremove(%#x)", id);
    return 0;
}

static int32_t STDCALL h_menu_text(uint32_t id, const char *text)
{
    put(" menutext(%#x,", id);
    put_s(text);
    put(")");
    return 0;
}

/* ---- the other modules -------------------------------------------------------------- */

static int32_t STDCALL chain_add(uint32_t list, callback_fn cb)
{
    put(" chain.add(%#x,code%u)", list, pair_code((uint32_t)(uintptr_t)cb) & 0xffff);
    if (list == 0x14)
        cur->chain14 = cb;
    return 0;
}

static int32_t STDCALL chain_remove(uint32_t list, callback_fn cb)
{
    put(" chain.remove(%#x,code%u)", list, pair_code((uint32_t)(uintptr_t)cb) & 0xffff);
    if (list == 0x14 && cur->chain14 == cb)
        cur->chain14 = NULL;
    return 0;
}

static uint32_t claim_eye;

static int32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    put(" chain.send(%#x,%#x,", list, a);
    put_ptr(b);
    put(")");
    if (list == 0x13 && claim_eye && (answer() % 3) == 0) {
        *(uint32_t *)(uintptr_t)b = 1;
        put(" claimed");
        if (counting) cover_claimed++;
    }
    return (int32_t)(answer() & 0xff);
}

static uint32_t STDCALL controls_redo(void)
{
    put(" controls.redo");
    return 0;
}

static uint32_t STDCALL controls_set(uint32_t what, uint32_t v)
{
    put(" controls.set(%#x,%#x)", what, v);
    return 0;
}

static uint32_t STDCALL hg2d_line_h(uint32_t s, int32_t a, int32_t b, int32_t c)
{
    put(" hg2d.lineh(");
    put_ptr(s);
    put(",%d,%d,%d)", a, b, c);
    return 0;
}

static uint32_t STDCALL hg2d_line_v(uint32_t s, int32_t a, int32_t b, int32_t c)
{
    put(" hg2d.linev(");
    put_ptr(s);
    put(",%d,%d,%d)", a, b, c);
    return 0;
}

static uint32_t STDCALL hg2d_present(uint32_t s, int32_t a, int32_t b, int32_t c, int32_t d)
{
    put(" hg2d.present(");
    put_ptr(s);
    put(",%d,%d,%d,%d)", a, b, c, d);
    return 0;
}

static uint32_t STDCALL hg2d_begin(uint32_t rec)
{
    put(" hg2d.begin(");
    put_ptr(rec);
    put(")");
    return 0;
}

static uint32_t STDCALL hg2d_end(uint32_t rec)
{
    put(" hg2d.end(");
    put_ptr(rec);
    put(")");
    return 0;
}

static uint32_t STDCALL hg2d_hit(uint32_t rec, uint32_t arg)
{
    uint32_t r = answer();

    put(" hg2d.hit(");
    put_ptr(rec);
    put(",%#x)=%#x", arg, r);
    return r;
}

static uint32_t STDCALL g3d_stars(uint32_t p)
{
    put(" g3d.sky(");
    put_ptr(p);
    put(")");
    put_bytes((void *)(uintptr_t)p, 0x34);
    return 0;
}

static uint32_t STDCALL g3d_draw(uint32_t p)
{
    put(" g3d.draw(");
    put_ptr(p);
    put(")");
    put_bytes(global + 0xcdc, 0xc);
    put_bytes((void *)(uintptr_t)p, 0x14);
    if (counting) cover_stars++;
    return 0;
}

static uint32_t STDCALL g3d_frame_a(void)
{
    put(" g3d.framea");
    return 0;
}

static uint32_t STDCALL g3d_frame_b(void)
{
    put(" g3d.frameb");
    return 0;
}

static uint32_t STDCALL g3d_layer(int32_t a, int32_t b, uint32_t c)
{
    uint32_t r = (answer() % 3) == 0 ? 0 : answer() % 5;

    put(" g3d.layer(%d,%d,%u)=%u", a, b, c, r);
    if (counting) cover_layers++;
    return r;
}

static uint32_t STDCALL g3d_normal(int32_t *v)
{
    uint32_t m = 0;
    int i, top = -1;

    for (i = 0; i < 3; i++)
        m |= (uint32_t)(v[i] ^ (v[i] >> 31));
    for (i = 31; i >= 0; i--)
        if (m & (1u << i)) { top = i; break; }
    put(" g3d.normal(%d,%d,%d)", v[0], v[1], v[2]);
    /* G3D's table: top bits 0..3 shift by 25, 4..27 up to bit 28, 28
     * stays, 29 one down, 30 and 31 two. */
    if (top >= 0 && top < 4) {
        for (i = 0; i < 3; i++) v[i] = (int32_t)((uint32_t)v[i] << 25);
    } else if (top >= 4 && top < 28) {
        for (i = 0; i < 3; i++) v[i] = (int32_t)((uint32_t)v[i] << (28 - top));
    } else if (top == 29) {
        for (i = 0; i < 3; i++) v[i] >>= 1;
    } else if (top >= 30) {
        for (i = 0; i < 3; i++) v[i] >>= 2;
    }
    put("=(%d,%d,%d)", v[0], v[1], v[2]);
    return 0;
}

static uint32_t STDCALL g3d_begin(void)
{
    put(" g3d.begin");
    return 0;
}

static uint32_t STDCALL main_save_open(uint32_t tag, uint32_t size)
{
    put(" main.saveopen(%#x,%#x)", tag, size);
    return 0;
}

static uint32_t STDCALL main_save_put(uint32_t p, uint32_t size, uint32_t z)
{
    put(" main.saveput(");
    put_ptr(p);
    put(",%#x,%u)", size, z);
    put_bytes((void *)(uintptr_t)p, size);
    return 0;
}

static uint32_t STDCALL main_save_close(void)
{
    put(" main.saveclose");
    return 0;
}

static uint32_t STDCALL main_scene(void)
{
    put(" main.scene");
    return 0;
}

static uint32_t STDCALL main_layers(uint32_t n)
{
    put(" main.layer(%u)", n);
    return 0;
}

/* MAIN +0x9c comes straight after the eye: what the eye made of GLOBAL. */
static uint32_t STDCALL main_prepare(void)
{
    uint32_t r = (answer() % 2) ? 0 : answer() & 0xffff;

    put(" main.prepare");
    put_bytes(global + 0x5b0, 0x24);
    put_bytes(global + 0x2b0, 0x40);
    put_bytes(global + 0xce8, 0x30);
    put_bytes(global + 0xd44, 0xc);
    if (answer() % 3 == 0)
        G16(0x832) = (uint16_t)(answer() % 0x18);
    if (counting && G16(0xdce) && (int32_t)G32(0x390) >= 2) {
        /* the height test that follows: as frame_3d makes it */
        uint32_t b = (uint8_t)(G16(0x5c2) >> 8) < G8(0x20);
        uint32_t b2 = (uint32_t)G16(0x5c4) < (uint32_t)G16(0x21) + b;
        int16_t cx = (int16_t)(G16(0x5c6) - (int16_t)(int8_t)G8(0x23) - b2);
        cover_high[cx < -1 ? 0 : cx > 8 ? 10 : cx + 1]++;
    }
    put("=%#x", r);
    return r;
}

static uint32_t STDCALL main_after(void)
{
    put(" main.after");
    return 0;
}

static uint32_t STDCALL main_paint(uint32_t rec)
{
    put(" main.paint(");
    put_ptr(rec);
    put(")");
    return 0;
}

static char *STDCALL num_format(const uint32_t *value, uint32_t format, char *out)
{
    sprintf(out, "<%x:%x>", format, *value);
    put(" format(");
    put_ptr((uint32_t)(uintptr_t)value);
    put(",%#x)=%s", format, out);
    return out;
}

static uint32_t STDCALL panels_show(void)
{
    put(" panels.show");
    return 0;
}

static uint32_t STDCALL panels_hide(void)
{
    put(" panels.hide");
    return 0;
}

static uint32_t STDCALL reality_frame(void)
{
    put(" reality.frame");
    return 0;
}

static uint32_t STDCALL acontain_done(uint32_t p)
{
    put(" acontain.done(");
    put_ptr(p);
    put(")");
    return 0;
}

static uint32_t STDCALL acontain_path(uint32_t a, uint32_t what, char *out, uint32_t size)
{
    put(" acontain.path(");
    put_ptr(a);
    put(",%u,%#x)", what, size);
    snprintf(out, size, "aircraft\\c182\\aircraft.cfg");
    return 0;
}

uint32_t STDCALL u_alert(uint32_t flags, uint32_t tag, uint32_t p1, uint32_t p2,
                         uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines);
uint32_t STDCALL u_alert(uint32_t flags, uint32_t tag, uint32_t p1, uint32_t p2,
                         uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines)
{
    put(" alert(%#x,%#x,%#x,%#x,%#x,%#x,%#x,%u)", flags, tag, p1, p2, p3, p4, p5, lines);
    if (counting) cover_alerts++;
    return 0;
}

/* The aircraft object at GLOBAL +0x11e0, and the callback at its +0x10. */
static uint32_t STDCALL model_draw(uint32_t arg)
{
    put(" model(%#x)", arg);
    if (counting) cover_model++;
    return 0;
}
static uint32_t model_object[8];

/* Other modules' windows paint through their own callbacks. */
static void STDCALL foreign_paint(uint32_t mode, uint32_t rec, uint32_t arg, uint32_t out)
{
    put(" foreignpaint(%u,", mode);
    put_ptr(rec);
    put(",%#x,", arg);
    put_ptr(out);
    put(")");
}

#define SET_SLOT(table, offset, fn) (*(void **)((table) + (offset)) = (void *)(fn))

static void build_stubs(void)
{
    uint32_t i;

    /* An unset slot faults at an address that names it: 0xde, the module,
     * the offset. */
#define TRAPS(table, id) \
    for (i = 4; i < sizeof table; i += 4) *(uint32_t *)(table + i) = 0xde000000u | ((id) << 16) | i
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(controls, 16); TRAPS(mainmod, 3);
    TRAPS(numeric, 17); TRAPS(hg2d, 13); TRAPS(g3d, 14); TRAPS(panels, 15);
    TRAPS(reality, 32); TRAPS(acontain, 23);
#undef TRAPS
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x020, h_active);
    SET_SLOT(host, 0x064, h_win_create);
    SET_SLOT(host, 0x068, h_win_destroy);
    SET_SLOT(host, 0x06c, h_paint_begin);
    SET_SLOT(host, 0x070, h_paint_end);
    SET_SLOT(host, 0x07c, h_win_update);
    SET_SLOT(host, 0x080, h_win_title);
    SET_SLOT(host, 0x084, h_win_flush);
    SET_SLOT(host, 0x08c, h_text);
    SET_SLOT(host, 0x090, h_raise);
    SET_SLOT(host, 0x094, h_nth);
    SET_SLOT(host, 0x0d0, h_profile_int);
    SET_SLOT(host, 0x13c, h_close);
    SET_SLOT(host, 0x17c, h_open);
    SET_SLOT(host, 0x19c, h_read);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x21c, h_memmove);
    SET_SLOT(host, 0x244, h_menu_add);
    SET_SLOT(host, 0x25c, h_menu_remove);
    SET_SLOT(host, 0x264, h_menu_text);
    SET_SLOT(host, 0x2a4, h_string);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b0, h_strcat);
    SET_SLOT(host, 0x2b4, h_strdup);
    SET_SLOT(host, 0x2b8, h_strlen);
    SET_SLOT(host, 0x2bc, h_strcmp);
    SET_SLOT(host, 0x2f8, h_list_add);
    SET_SLOT(host, 0x2fc, h_list_free);
    SET_SLOT(host, 0x300, h_list_remove);
    SET_SLOT(host, 0x308, h_list_walk);
    SET_SLOT(host, 0x310, h_list_find);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    SET_SLOT(chain, 0x20, chain_send);
    *(uint32_t *)controls = 16;
    SET_SLOT(controls, 0x24, controls_redo);
    SET_SLOT(controls, 0x40, controls_set);
    *(uint32_t *)hg2d = 13;
    SET_SLOT(hg2d, 0x2c, hg2d_line_h);
    SET_SLOT(hg2d, 0x30, hg2d_line_v);
    SET_SLOT(hg2d, 0x3c, hg2d_present);
    SET_SLOT(hg2d, 0x9c, hg2d_begin);
    SET_SLOT(hg2d, 0xa0, hg2d_end);
    SET_SLOT(hg2d, 0xbc, hg2d_hit);
    *(uint32_t *)g3d = 14;
    SET_SLOT(g3d, 0x1c, g3d_stars);
    SET_SLOT(g3d, 0x20, g3d_draw);
    SET_SLOT(g3d, 0x2c, g3d_frame_a);
    SET_SLOT(g3d, 0x30, g3d_frame_b);
    SET_SLOT(g3d, 0x34, g3d_layer);
    SET_SLOT(g3d, 0x3c, g3d_normal);
    SET_SLOT(g3d, 0x5c, g3d_begin);
    *(uint32_t *)mainmod = 3;
    SET_SLOT(mainmod, 0x5c, main_save_open);
    SET_SLOT(mainmod, 0x60, main_save_put);
    SET_SLOT(mainmod, 0x64, main_save_close);
    SET_SLOT(mainmod, 0x88, main_scene);
    SET_SLOT(mainmod, 0x98, main_layers);
    SET_SLOT(mainmod, 0x9c, main_prepare);
    SET_SLOT(mainmod, 0xa8, main_after);
    SET_SLOT(mainmod, 0xc0, main_paint);
    *(uint32_t *)numeric = 17;
    SET_SLOT(numeric, 0x18, num_format);
    *(uint32_t *)panels = 15;
    SET_SLOT(panels, 0x28, panels_show);
    SET_SLOT(panels, 0x2c, panels_hide);
    *(uint32_t *)reality = 32;
    SET_SLOT(reality, 0x1c, reality_frame);
    *(uint32_t *)acontain = 23;
    SET_SLOT(acontain, 0x18, acontain_done);
    SET_SLOT(acontain, 0x30, acontain_path);
    model_object[4] = (uint32_t)(uintptr_t)model_draw;
}

static void load_util(const char *path)
{
    HMODULE m = LoadLibraryA(path);
    struct import *im;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;

    if (m == NULL) { printf("cannot load %s\n", path); exit(2); }
    util_link = (uint8_t *)GetProcAddress(m, "Linkage");
    im = (struct import *)GetProcAddress(m, "ImportTable");
    if (util_link == NULL || im == NULL || *(uint32_t *)util_link != 20) {
        printf("%s: not UTIL\n", path);
        exit(2);
    }
    for (; im->id; im++) {
        if (im->id == 1) im->link = host;
        else if (im->id == 2) im->link = global;
        else if (im->id == 17) im->link = numeric;
        else { printf("UTIL: unexpected import %u\n", im->id); exit(2); }
    }
    dos = (IMAGE_DOS_HEADER *)m;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)m + dos->e_lfanew);
    util_lo = (uint8_t *)m;
    util_hi = util_lo + nt->OptionalHeader.SizeOfImage;
    memcpy(util_proxy, util_link, sizeof util_proxy);
    SET_SLOT(util_proxy, 0xa4, u_alert);
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
    if (s->imports == NULL || s->link == NULL || *(uint32_t *)s->link != 10) {
        printf("%s: not WINDOW\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if (s->link != s->image + 0x68) {
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
        case 13: im->link = hg2d; break;
        case 14: im->link = g3d; break;
        case 15: im->link = panels; break;
        case 16: im->link = controls; break;
        case 17: im->link = numeric; break;
        case 20: im->link = util_proxy; break;
        case 23: im->link = acontain; break;
        case 32: im->link = reality; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

static uint32_t n_windows(void);
static uint32_t window_at(uint32_t i);
static void map_high(void);

/* ---- GLOBAL --------------------------------------------------------------------- */


/* A 48-bit position: a fraction word, then the 32.16 value as a word and a
 * dword. */
static void put_position(uint32_t at, int32_t hi)
{
    G16(at) = (uint16_t)rnd();
    G16(at + 2) = (uint16_t)rnd();
    G32(at + 4) = (uint32_t)hi;
}

static void near_position(uint32_t at, uint32_t from, int32_t reach)
{
    memcpy(global + at, global + from, 8);
    if (rnd_n(8) == 0)
        return;                 /* level with it, or on its meridian */
    G32(at + 4) += (uint32_t)((int32_t)rnd_n((uint32_t)reach * 2 + 1) - reach);
    G16(at + 2) = (uint16_t)rnd();
}

static void g_status(void)
{
    G16(0x552) = (uint16_t)rnd_n(2);
    G16(0x5dc) = (uint16_t)(rnd_n(4) == 0);
    G32(0x5f4) = rnd_n(6) - 1;
    G32(0x5fc) = rnd_n(6) - 1;
    G32(0x268) = rnd_n(3) == 0;
    G8(0x258) = (uint8_t)rnd_n(4);
    G16(0xbc8) = (uint16_t)(rnd_n(5) == 0);
    G16(0xbca) = (uint16_t)(rnd_n(5) == 0);
    G32(0x760) = rnd_n(3) == 0;
    G32(0x79c) = rnd_n(2);
    G32(0x758) = rnd_n(6) == 0;
    G32(0x628) = rnd_n(6) == 0;
    G32(0x75c) = rnd_n(7);
    G8(0x36c) = (uint8_t)(rnd_n(6) == 0);
    G8(0x36d) = (uint8_t)(rnd_n(6) == 0);
    G16(0x612) = (uint16_t)rnd_n(2);
    G32(0x2b8) = rnd();
    G32(0x2bc) = rnd();
    G16(0xc18) = (uint16_t)rnd_n(4);
    G16(0x11ba) = (uint16_t)(rnd_n(4) == 0 ? rnd() : rnd_n(0x3000) - 0x1800);
    if (rnd_n(4) == 0)          /* g loads about the clamp, 158 */
        G16(0x11ba) = (uint16_t)((rnd_n(2) ? 1 : -1) * (int32_t)(157 * 39 - 32 + rnd_n(4 * 39)));
    G16(0x2aa) = (uint16_t)(rnd_n(8) == 0 ? rnd() : 0x100 + rnd_n(0x2000));
}

static void g_attitude(void)
{
    G32(0x578) = rnd_n(3) ? rnd() : 0;
    G32(0x57c) = rnd_n(3) ? rnd() : 0;
    G32(0x580) = rnd();
}

static void g_eye(void)
{
    uint32_t i;

    for (i = 0; i < 3; i++)
        G32(0xdb0 + 4 * i) = (uint32_t)((int32_t)rnd_n(0x40000) - 0x20000);
    for (i = 0; i < 3; i++)
        G32(0xd98 + 4 * i) = rnd_n(3) ? (uint32_t)((int32_t)rnd_n(0x20000000) - 0x10000000) : 0;
    for (i = 0; i < 9; i++)
        G16(0xd18 + 2 * i) = (uint16_t)rnd();
    G32(0xd2c) = rnd_n(4) == 0 ? 1 : rnd_n(3) == 0 ? rnd_n(4) : 0;
    G32(0x274) = rnd_n(4) == 0 ? rnd() : 0x7000 + rnd_n(0x1000);
    G32(0xdb8) = rnd_n(10) == 0 ? rnd() : 0x1400 + rnd_n(0x1000);
    G16(0xdc0) = (uint16_t)(rnd_n(5) != 0);
    G16(0xdc2) = (uint16_t)(rnd_n(3) == 0);
    G16(0xdc4) = (uint16_t)rnd_n(2);
    G16(0xdc6) = (uint16_t)rnd_n(3);
    G16(0x2b2) = (uint16_t)(rnd_n(3) == 0 ? rnd() : rnd_n(0x800));
    G32(0x390) = rnd_n(3) == 0 ? 2 : rnd_n(4);
    if (rnd_n(4) == 0) {        /* the map high enough for G3D's own frame */
        G16(0x2b2) = (uint16_t)(0x980 + rnd_n(0x180));
        G32(0x390) = 2;
        G32(0x20) = rnd_n(4) ? rnd_n(0x100000) : 0;
    }
    G16(0x11b0) = (uint16_t)rnd_n(2);
}

static void g_scene(void)
{
    G16(0x1172) = (uint16_t)rnd_n(8);
    G16(0x116e) = (uint16_t)rnd_n(8);
    G16(0xcba) = (uint16_t)(rnd_n(8) == 0 ? 0x7fff + rnd_n(3) : rnd());
    G16(0xb54) = (uint16_t)rnd_n(2);
    G16(0xe86) = (uint16_t)rnd_n(6);
    G16(0xe6e) = (uint16_t)rnd();
    G16(0xe76) = (uint16_t)rnd();
    if (rnd_n(6) == 0) {        /* the dark test's edge: ~(~a * ~b >> 16) 0x7fff or 0x8000 */
        G16(0xe6e) = (uint16_t)(rnd_n(2) ? 0x0000 : 0x7fff);
        G16(0xe76) = (uint16_t)(G16(0xe6e) ? 0x0000 : 0x7ffe + rnd_n(3));
    }
    G16(0x556) = (uint16_t)(rnd_n(3) ? 0 : rnd());
    G32(0x11e0) = rnd_n(4) ? (uint32_t)(uintptr_t)model_object : 0;
    model_object[4] = rnd_n(5) ? (uint32_t)(uintptr_t)model_draw : 0;
    G32(0x11dc) = rnd_n(4) != 0;
    G32(0x11e4) = rnd_n(4) ? rnd() : 0;
    G16(0xe5a) = (uint16_t)rnd_n(2);
    G8(0x840) = (uint8_t)rnd_n(2);
    G32(0x11ac) = rnd();
    G32(0x11a4) = rnd();
    G32(0x11a8) = rnd();
    G16(0xe6a) = (uint16_t)rnd_n(0x3000);
    G16(0xe6c) = (uint16_t)rnd_n(0x800);
    G16(0xe72) = (uint16_t)(G16(0xe6a) + rnd_n(0x3000));
    G16(0xe74) = (uint16_t)rnd_n(0x800);
    G16(0xe70) = (uint16_t)rnd_n(2);
    G16(0xe78) = (uint16_t)rnd_n(2);
    G16(0xe8a) = (uint16_t)(rnd_n(4) == 0 ? 0x5da + rnd_n(2) : rnd_n(0xc00));
    G32(0xb60) = rnd();
    G32(0x3d4) = rnd();
    G32(0x3d8) = rnd();
    G16(0x3ae) = (uint16_t)rnd();
    G16(0x39c) = (uint16_t)rnd();
    G16(0x832) = (uint16_t)rnd_n(0x18);
    G8(0x280) = (uint8_t)rnd_n(2);
    G16(0xb34) = (uint16_t)rnd();
    G32(0x394) = rnd();
    G16(0xe88) = (uint16_t)rnd();
    G8(0x11ec) = (uint8_t)rnd();
    G8(0x11ed) = (uint8_t)rnd();
    G32(0xcdc) = rnd();
    G32(0xce0) = rnd();
    G32(0xce4) = rnd();
    G32(0x1238) = rnd_n(3) == 0;
    G32(0x54c) = rnd_n(3) == 0;
    G32(0x1250) = rnd_n(5) != 0;
    G16(0x550) = (uint16_t)rnd_n(2);
}

static void g_setup(void)
{
    int32_t lat = (int32_t)rnd_n(18000000) - 9000000;

    memset(global, 0, sizeof global);
    *(uint32_t *)global = 2;
    G32(0x20) = rnd_n(0x200000);
    put_position(0x560, lat);
    put_position(0x568, (int32_t)rnd());
    put_position(0x570, (int32_t)(G32(0x20) >> 8) + (int32_t)rnd_n(4000) - 50);
    near_position(0xd50, 0x560, 3000);
    near_position(0xd58, 0x568, 3000);
    near_position(0xd60, 0x570, 300);
    near_position(0xd74, 0x560, 30000);
    near_position(0xd7c, 0x568, 30000);
    near_position(0xd84, 0x570, 100);
    g_attitude();
    g_eye();
    g_scene();
    g_status();
}

/* Between frames: the aircraft moves and turns, and now and then something
 * else changes. */
static void g_move(void)
{
    uint32_t k = rnd_n(13);

    G32(0x564) += (uint32_t)((int32_t)rnd_n(41) - 20);
    G32(0x56c) += (uint32_t)((int32_t)rnd_n(41) - 20);
    G32(0x574) += (uint32_t)((int32_t)rnd_n(11) - 5);
    G32(0x578) += (uint32_t)((int32_t)rnd_n(0x2000001) - 0x1000000);
    G32(0x57c) += (uint32_t)((int32_t)rnd_n(0x2000001) - 0x1000000);
    G32(0x580) += (uint32_t)((int32_t)rnd_n(0x4000001) - 0x2000000);
    G16(0x2aa) = (uint16_t)(rnd_n(10) == 0 ? rnd() : 0x100 + rnd_n(0x2000));
    switch (k) {
    case 0: g_status(); break;
    case 1: g_eye(); break;
    case 2: g_scene(); break;
    case 3: g_attitude(); break;
    case 4: near_position(0xd50, 0x560, 3000); break;
    case 5: near_position(0xd74, 0x560, 30000); break;
    case 6: G32(0x20) = rnd_n(0x200000); break;
    case 7: /* the ground just under the eye: the clamp's edge */
        G32(0x20) = (G32(0x5c4) << 8) - 0x200 + rnd_n(3) - 1;
        break;
    case 8: /* the attitude at the lag's edge */
        G32(0x578) = *(uint32_t *)(cur->image + 0x2b0) + (rnd_n(2) ? 0x40000 : -0x40000) + rnd_n(3) - 1;
        G32(0x57c) = *(uint32_t *)(cur->image + 0x2b4) + (rnd_n(2) ? 0x40000 : -0x40000) + rnd_n(3) - 1;
        G32(0x580) = *(uint32_t *)(cur->image + 0x2b8) + (rnd_n(2) ? 0x40000 : -0x40000) + rnd_n(3) - 1;
        break;
    case 10:
        map_high();
        break;
    case 9: /* the ground at a cloud band's edge */
        G32(0x20) = (uint32_t)(uint16_t)(rnd_n(2) ? G16(0xe72) - (G16(0xe74) >> 1)
                                                  : G16(0xe6a) - (G16(0xe6c) >> 1)) << 8 | rnd_n(256);
        break;
    default: break;
    }
}

/* ---- the module's own lists, to aim the next op ------------------------------------- */

static uint32_t n_windows(void)
{
    return *(uint32_t *)(cur->image + 0x110);
}

static uint32_t window_at(uint32_t i)
{
    uint8_t *data = *(uint8_t **)(cur->image + 0x128);
    uint32_t n = n_windows();

    if (n == 0 || data == NULL)
        return 0;
    return *(uint32_t *)(data + 4 * (i % n));
}

/* a window of type `type`, or any (0); null if none, or now and then */
static uint32_t pick_window(uint32_t type)
{
    uint32_t n = n_windows(), i, start, rec;

    if (n == 0 || answer() % 16 == 0)
        return 0;
    start = answer();
    for (i = 0; i < n; i++) {
        rec = window_at(start + i);
        if (type == 0 || R32(rec, 4) == type)
            return rec;
    }
    return 0;
}

/* The maps about high enough for G3D's own frame: with GLOBAL +0x390 2 the
 * eye is twice the zoom's height over the ground, and the test is its high
 * word against 4, which zooms 0x580..0x63f straddle. */
static void map_high(void)
{
    uint32_t i, rec, v;

    for (i = 0; i < n_windows(); i++)
        if ((rec = window_at(i)) && (v = R32(rec, 0x1c)) && R32(v, 0x10) == 1)
            *(uint16_t *)(uintptr_t)(v + 0x26) = (uint16_t)(0x580 + rnd_n(0xc0));
    G32(0x390) = 2;
    G32(0x20) = rnd_n(0x10000) << rnd_n(9);
}

/* ---- driving ------------------------------------------------------------------------- */

typedef uint32_t (STDCALL *s0)(void);
typedef uint32_t (STDCALL *s1)(uint32_t);
typedef uint32_t (STDCALL *s2)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *s3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (STDCALL *s4)(uint32_t, uint32_t, uint32_t, uint32_t);

#define SVC(off) (*(void **)(cur->link + (off)))

static void *fault_jb[5];
static volatile int in_window;
static volatile int faulted;
static int loaded;            /* records since the last restore */

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

/* The host runs the sim under a filter (0x4119a0, through the __try at
 * 0x412140) that repairs an integer divide's fault: it decodes the F6/F7
 * (with or without 0x66), saturates the result -- all ones for div; for
 * idiv 0x7f..ff, or 0x80..01 when the divisor and the dividend's top half
 * differ in sign, with the remainder 0x7f..ff (a byte idiv 0x7f7f/0x7f81
 * in ax) -- and carries on after the instruction.  WINDOW and UTIL divide
 * by registers only, the only form modelled here. */
static int div_fixup(CONTEXT *c)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
    uint32_t len = 0, word = 0, byte, reg, rm, v, neg;
    DWORD *regs[8];

    regs[0] = &c->Eax; regs[1] = &c->Ecx; regs[2] = &c->Edx; regs[3] = &c->Ebx;
    regs[4] = &c->Esp; regs[5] = &c->Ebp; regs[6] = &c->Esi; regs[7] = &c->Edi;
    if (*p == 0x66) {
        word = 1;
        p++;
        len = 1;
    }
    if ((p[0] & 0xfe) != 0xf6)
        return 0;
    byte = !(p[0] & 1);
    reg = (p[1] >> 3) & 7;
    rm = p[1] & 7;
    if (reg != 6 && reg != 7)
        return 0;
    if ((p[1] >> 6) != 3) {
        printf("scenario %u: a divide with a memory operand at %p, not modelled\n",
               scenario, (const void *)p);
        exit(2);
    }
    len += 2;
    if (reg == 6) {
        if (byte) {
            c->Eax |= 0xffff;
        } else if (word) {
            c->Eax |= 0xffff;
            c->Edx |= 0xffff;
        } else {
            c->Eax = 0xffffffffu;
            c->Edx = 0xffffffffu;
        }
    } else {
        v = *regs[rm];
        if (byte) {
            neg = ((int8_t)v < 0) != ((int8_t)(c->Eax >> 8) < 0);
            c->Eax = (c->Eax & 0xffff0000u) | (neg ? 0x7f81u : 0x7f7fu);
        } else if (word) {
            neg = ((int16_t)v < 0) != ((int16_t)c->Edx < 0);
            c->Eax = (c->Eax & 0xffff0000u) | (neg ? 0x8001u : 0x7fffu);
            c->Edx = (c->Edx & 0xffff0000u) | 0x7fffu;
        } else {
            neg = ((int32_t)v < 0) != ((int32_t)c->Edx < 0);
            c->Eax = neg ? 0x80000001u : 0x7fffffffu;
            c->Edx = 0x7fffffffu;
        }
    }
    c->Eip += len;
    return 1;
}

static unsigned long cover_divfix;
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
    if (!in_window || cur == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
    if ((code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW) &&
        div_fixup(e->ContextRecord)) {
        /* not in the transcript: ours saturates without the fault */
        if (counting) cover_divfix++;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW)
        put(" FAULT(div)");
    else if (code == EXCEPTION_ACCESS_VIOLATION)
        put(" FAULT(av)");
    else
        return EXCEPTION_CONTINUE_SEARCH;
    if (getenv("WINDOW_EXC")) {
        uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
        if (ip >= cur->lo && ip < cur->hi)
            put("@%s.text+%#x", cur->name, (uint32_t)(ip - cur->lo - 0x1000));
        else if (ip >= util_lo && ip < util_hi)
            put("@util.text+%#x", (uint32_t)(ip - util_lo - 0x1000));
        else
            put("@%p", (void *)ip);
    }
    if (counting) {
        uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
        uint32_t where = ip >= cur->lo && ip < cur->hi ? (uint32_t)(ip - cur->lo - 0x1000)
                         : ip >= util_lo && ip < util_hi ? 0x80000000u | (uint32_t)(ip - util_lo - 0x1000)
                         : 0xffffffffu, k;

        cover_faults++;
        for (k = 0; k < 16 && fault_at[k] != where && fault_n[k]; k++)
            ;
        if (k < 16) {
            fault_at[k] = where;
            fault_n[k]++;
        }
    }
    in_window = 0;
    faulted = 1;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* The comparable state: the image with its code pointers paired and its
 * pointers into itself as offsets, the arena with the records' paint
 * callbacks paired, and GLOBAL. */
static uint8_t arena_copy[ARENA_SIZE];

static void put_state(void)
{
    uint8_t img[IMAGE_SIZE];
    uint32_t i, base = (uint32_t)(uintptr_t)cur->image, n, data;

    memcpy(img, cur->image, IMAGE_SIZE);
    for (i = 0x6c; i < 0xf8; i += 4)
        if (i != 0x74 && i != 0x78 && i != 0x7c)
            *(uint32_t *)(img + i) = pair_code(*(uint32_t *)(img + i));
    {
        uint32_t v = *(uint32_t *)(img + 0x35e);
        if (v >= base && v < base + IMAGE_SIZE)
            *(uint32_t *)(img + 0x35e) = 0xd0000000u | (v - base);
    }
    memcpy(arena_copy, arena, arena_used);
    n = *(uint32_t *)(img + 0x110);
    data = *(uint32_t *)(img + 0x128);
    for (i = 0; i < n && in_arena(data); i++) {
        uint32_t rec = *(uint32_t *)(uintptr_t)(data + 4 * i);
        if (in_arena(rec)) {
            uint32_t at = rec + 0x18 - (uint32_t)(uintptr_t)arena;
            uint32_t v = *(uint32_t *)(arena_copy + at);
            if (in_module(v))
                *(uint32_t *)(arena_copy + at) = pair_code(v);
        }
    }
    put("\n  state img=%08x arena=%#x/%08x G=%08x",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena_copy, arena_used, 0x811c9dc5u),
        digest(global, GLOBAL_SIZE, 0x811c9dc5u));
    if (getenv("WINDOW_IMAGE")) {
        put("\n  whole");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(img + i));
        put("\n  gwhole");
        for (i = 0; i + 4 <= GLOBAL_SIZE; i += 4)
            if (*(uint32_t *)(global + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(global + i));
    }
    put("\n  G 2e0:");
    for (i = 0x2d4; i < 0x2ec; i += 2)
        put(" %04x", G16(i));
    put(" 5b0:");
    for (i = 0x5b0; i < 0x5d4; i += 4)
        put(" %08x", G32(i));
    put(" ce8:");
    for (i = 0xce8; i < 0xd18; i += 2)
        put(" %04x", G16(i));
    put(" d44=%04x %04x %08x dbc=%04x %04x 119c=", G16(0xd44), G16(0xd46), G32(0xd48),
        G16(0xdbc), G16(0xdbe));
    put_ptr(G32(0x119c));
    put(" 832=%x 2b0=%x 2b2=%x", G16(0x832), G16(0x2b0), G16(0x2b2));
}

/* the stars: the CD's, a synthetic one, or none */
static void make_stars(void)
{
    uint32_t k = rnd_n(6), i;

    stars_there = k != 0;
    if (k <= 2 && cd_stars_size) {
        memcpy(stars, cd_stars, cd_stars_size);
        stars_size = cd_stars_size;
        return;
    }
    stars_size = 2 + rnd_n(0x200);
    for (i = 0; i < stars_size; i++)
        stars[i] = (uint8_t)rnd();
    i = k == 3 ? stars_size : 2 + rnd_n(stars_size - 1);   /* or short of it */
    if (k == 4)
        i = stars_size + rnd_n(0x100);                      /* or past the end */
    stars[0] = (uint8_t)i;
    stars[1] = (uint8_t)(i >> 8);
}

static const char *const titles[] = { "Panel", "Map View", "ATC", "", "Instruments 2" };

/* A situation's window record for message 0x22. */
static uint8_t *make_record(uint32_t type)
{
    uint8_t *r = (uint8_t *)arena_get(0x10), *d = (uint8_t *)arena_get(0x100);
    uint32_t i;

    memset(r, 0, 0x10);
    for (i = 0; i < 0x100; i++)
        d[i] = (uint8_t)rnd();
    *(uint32_t *)r = type;
    *(uint32_t *)(r + 0xc) = (uint32_t)(uintptr_t)d;
    switch (type) {
    case 5: case 8:
        d[0] = (uint8_t)rnd_n(5);           /* the mode, as old as it was */
        break;
    case 11:
        d[0] = (uint8_t)rnd_n(4);
        break;
    case 12: case 13: case 14:
        d[0] = (uint8_t)(rnd_n(3) ? rnd_n(2) : rnd());
        break;
    case 0x1e:
        for (i = 0; i < 3; i++)
            d[i] = (uint8_t)(rnd_n(4) ? rnd_n(5) : rnd());
        break;
    case 0x3de:
        /* z, active; flags, rect, rect; then the view: kind and mode kept
         * where the title can be built */
        *(uint32_t *)(d + 0) = rnd_n(5) ? rnd_n(4) : rnd_n(2) ? 0x100 : 0xff;
        *(uint32_t *)(d + 0x34) = rnd_n(2) ? rnd_n(16) : rnd();     /* view +0x18, the turn */
        *(uint32_t *)(d + 4) = rnd_n(3) == 0;
        *(uint32_t *)(d + 8) = (rnd_n(4) ? 0x80000000u : 0) | (rnd_n(4) == 0 ? 0x20000000u : 0) |
                               (rnd_n(4) == 0 ? 0x10000u : 0) | (rnd_n(8) == 0 ? 0x40000000u : 0) |
                               (rnd_n(3) ? 0 : rnd() & 0x0fffeffu);
        *(uint32_t *)(d + 0x2c) = rnd_n(6) == 0;  /* view +0x10, the kind */
        *(uint32_t *)(d + 0x30) = rnd_n(5);       /* view +0x14, the mode */
        if (rnd_n(4) == 0)                        /* view +0x28, the spot's far one */
            *(uint16_t *)(d + 0x44) = (uint16_t)(0x8000 + rnd_n(3));
        if (rnd_n(3) == 0)                        /* view +0x26, the map's zoom */
            *(uint16_t *)(d + 0x42) = (uint16_t)(rnd_n(2) ? 0x900 + rnd_n(0x100) : rnd());
        d[0x4a] = (uint8_t)rnd_n(3);              /* +0x30, whose heading */
        d[0x4b] = (uint8_t)(rnd_n(4) == 0);       /* +0x31, pinned */
        break;
    }
    return r;
}

static void call_chain14(uint32_t message, uint32_t data)
{
    put(" [0x14:%#x]", message);
    if (cur->chain14 == NULL) {
        put(" none");
        return;
    }
    cur->chain14(message, data);
}

static void frame(void)
{
    uint32_t r;

    if (counting) {
        uint32_t i;

        for (i = 0; i < n_windows(); i++) {
            uint32_t rec = window_at(i), v = R32(rec, 0x1c);
            if (R32(rec, 4) == 3 && v && (R32(rec, 8) & 0x80000000u) && R32(v, 0x10) == 0 &&
                R32(v, 0x14) < 5 && *(uint16_t *)(uintptr_t)(v + 0x56) < 5)
                cover_pairs[R32(v, 0x14)][*(uint16_t *)(uintptr_t)(v + 0x56)]++;
        }
    }
    active_answer = answer() % 8 != 0;
    claim_eye = answer() % 5 == 0;
    if (getenv("WINDOW_MODES")) {
        uint32_t i;

        for (i = 0; i < n_windows(); i++) {
            uint32_t rec = window_at(i), v = R32(rec, 0x1c);
            if (R32(rec, 4) == 3 && v)
                put(" {%x %x %x}", R32(rec, 8) >> 28, R32(v, 0x14), *(uint16_t *)(uintptr_t)(v + 0x56));
        }
        put(" dc0=%x", G16(0xdc0));
    }
    put(" [paint]");
    r = ((s0)SVC(0x1c))();
    (void)r;
    if (counting) {
        uint32_t i;

        cover_frames++;
        for (i = 0; i < n_windows(); i++) {
            uint32_t rec = window_at(i), v = R32(rec, 0x1c);
            if (R32(rec, 4) == 3 && v && (R32(rec, 8) & 0x80000000u)) {
                cover_paint3d++;
                if (R32(v, 0x10) == 1) cover_map++;
                else if (R32(v, 0x14) < 5) cover_modes[R32(v, 0x14)]++;
                cover_states[*(uint16_t *)(uintptr_t)(v + 0x56) % 0x60]++;
            }
        }
    }
}

static uint32_t op_kind(void)
{
    static const uint8_t weights[] = {
        6, 3, 2, 2, 1, 3, 3, 3, 2, 4, 1, 3, 2, 2, 2, 2, 14, 3, 2, 2, 2, 4, 2, 2, 2
    };
    uint32_t total = 0, i, k;

    for (i = 0; i < sizeof weights; i++)
        total += weights[i];
    k = answer() % total;
    for (i = 0; i < sizeof weights; i++) {
        if (k < weights[i])
            return i;
        k -= weights[i];
    }
    return 0;
}

static void op(void)
{
    uint32_t kind = op_kind(), rec, r, a, b, out = 0, count = 0, i;

    put("\n#%u op %u:", op_number, kind);
    if (counting) cover_ops[kind]++;
    switch (kind) {
    case 0:                                             /* a 3D view, or many */
        count = answer() % 40 == 0 ? 40 : 1;
        for (i = 0; i < count; i++) {
            a = answer() % 6 == 0;
            b = answer() % 5;
            put(" create3d(%u,%u)", a, b);
            r = ((s2)SVC(0x3c))(a, b);
            put("=");
            put_rec(r);
        }
        break;
    case 1: {                                           /* another module's window */
        static const uint32_t types[] = { 1, 4, 5, 2, 3, 6 };
        uint32_t type = types[answer() % 6], title;

        title = answer() % 3 ? 0x9200 + answer() % 0x80
                             : (uint32_t)(uintptr_t)titles[answer() % 5];
        put(" record(%#x,%u)", title < 0x10000 ? title : 0, type);
        r = ((s3)SVC(0x78))((uint32_t)(uintptr_t)&out, title, type);
        put("=%d,", (int32_t)r);
        put_ptr(out);
        if (out != 0) {
            R32(out, 0x18) = answer() % 4 ? (uint32_t)(uintptr_t)foreign_paint : 0;
            R32(out, 0x78) = 0x1000 * (1 + answer() % 3);
            R32(out, 0x20) = answer() % 0x1000;
            R32(out, 0x24) = answer() % 0x1000;
            R32(out, 0x28) = 0x100 + answer() % 0x1000;
            R32(out, 0x2c) = 0x100 + answer() % 0x1000;
            R32(out, 0x68) = answer() % 0x40;
            R32(out, 0x6c) = answer() % 0x40;
            R32(out, 8) = (answer() % 2 ? 0x80000000u : 0) | (answer() % 3 ? 0 : answer() & 0xffefu);
            h_win_create(out);
        }
        break;
    }
    case 2:                                             /* close one */
        rec = pick_window(answer() % 3 ? 3 : 0);
        put(" destroy(");
        put_ptr(rec);
        put(")");
        ((s1)SVC(0x40))(rec);
        break;
    case 3:                                             /* a record, by its owner */
        rec = pick_window(answer() % 5 + 1);
        if (rec == G32(0x119c) || (rec && R32(rec, 4) == 3 && R32(rec, 0x18) != 0))
            rec = 0;
        put(" destroyrecord(");
        put_ptr(rec);
        put(")");
        if (rec != 0)
            h_win_destroy(rec);
        r = ((s1)SVC(0x7c))((uint32_t)(uintptr_t)&rec);
        put("=%d", (int32_t)r);
        break;
    case 4:                                             /* all of them */
        put(" destroyall");
        ((s0)SVC(answer() % 2 ? 0x44 : 0x18))();
        break;
    case 5:
        rec = pick_window(3);
        put(" activate(");
        put_ptr(rec);
        put(")");
        ((s1)SVC(0x48))(rec);
        break;
    case 6:
        a = 0x4c + 4 * (answer() % 3);
        rec = pick_window(0);
        put(" max%#x(", a);
        put_ptr(rec);
        put(")");
        ((s1)SVC(a))(rec);
        break;
    case 7:
        a = 0x64 + 4 * (answer() % 3);
        rec = pick_window(0);
        put(" show%#x(", a);
        put_ptr(rec);
        put(")");
        ((s1)SVC(a))(rec);
        break;
    case 8:
        a = answer() % 2 ? 0x70 : 0x74;
        rec = pick_window(0);
        put(" plain%#x(", a);
        put_ptr(rec);
        put(")");
        ((s1)SVC(a))(rec);
        break;
    case 9:                                             /* a view's mode */
        rec = pick_window(answer() % 5 ? 3 : 0);
        b = answer() % 5;
        put(" mode(");
        put_ptr(rec);
        put(",%u)", b);
        ((s2)SVC(0x58))(rec, b);
        break;
    case 10:
        rec = pick_window(3);
        b = answer() % 5 == 0;
        put(" kind(");
        put_ptr(rec);
        put(",%u)", b);
        ((s2)SVC(0x5c))(rec, b);
        break;
    case 11:
        a = answer() % 3;
        if (a == 0) {
            b = answer() % 5;
            put(" activemode(%u)", b);
            ((s1)SVC(0x60))(b);
        } else {
            put(" stepmode(%u)", a);
            ((s0)SVC(a == 1 ? 0x38 : 0x34))();
        }
        break;
    case 12:                                            /* list */
        a = answer() % 7;
        b = answer() % 4 != 0;
        put(" list(%u,%u)", a, b);
        r = ((s3)SVC(0x80))(b ? (uint32_t)(uintptr_t)&out : 0, (uint32_t)(uintptr_t)&count, a);
        put("=%d,%u:", (int32_t)r, count);
        if (b && out) {
            for (i = 0; i < count; i++) {
                put(" ");
                put_ptr(*(uint32_t *)(uintptr_t)(out + 4 * i));
            }
            h_free((void **)&out);
        }
        break;
    case 13:                                            /* walk */
        a = answer() % 4;
        rec = answer() % 6 ? pick_window(a) : (uint32_t)(uintptr_t)global;
        b = answer() % 2 ? 0x84 : 0x88;
        put(" walk%#x(", b);
        put_ptr(rec);
        put(",%u)", a);
        r = ((s3)SVC(b))((uint32_t)(uintptr_t)&out, rec, a);
        put("=%d,", (int32_t)r);
        put_ptr(out);
        break;
    case 14:                                            /* find */
        a = answer() % 3;
        b = answer() % 4 ? 0x100 + answer() % 6 : 0x200 + answer() % 4;
        put(" find(%#x,%u)", b, a);
        r = ((s2)SVC(0x8c))(b, a);
        put("=");
        put_ptr(r);
        break;
    case 15:                                            /* the user resizes */
        rec = pick_window(0);
        if (rec) {
            int32_t w = (int32_t)(answer() % 1200), h = (int32_t)(answer() % 900);
            put(" resize(");
            put_ptr(rec);
            put(",%d,%d)", w, h);
            host_size(rec, w, h);
        }
        break;
    case 16:                                            /* frames */
        a = 1 + answer() % 8;
        if (answer() % 4 == 0)
            map_high();
        for (i = 0; i < a && !faulted; i++) {
            if (i)
                g_move();
            frame();
            put_state();
        }
        break;
    case 17:                                            /* a situation's records */
        a = 1 + answer() % 8;
        for (i = 0; i < a; i++) {
            static const uint32_t types[] = {
                4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0x1e, 0x4d, 0x66, 0x67, 0x73,
                0x3de, 0x3de, 0x3de, 0x3de, 0x5c, 3, 15, 0x1f, 0x74
            };
            uint32_t type = types[answer() % (sizeof types / sizeof types[0])];
            uint8_t *record = make_record(type);
            if (counting) cover_records[type % 0x20]++;
            call_chain14(0x22, (uint32_t)(uintptr_t)record);
        }
        loaded = 1;
        break;
    case 18:                                            /* restored */
        if (!loaded && answer() % 40 != 0)
            break;
        call_chain14(0x21, 0);
        if (counting) cover_restored++;
        loaded = 0;
        break;
    case 19:                                            /* saved */
        call_chain14(0x23, 0);
        break;
    case 20:                                            /* a new aircraft */
        for (i = 0; i < 4; i++)
            sizes[i] = answer() % 5 == 0 ? -1 : (int32_t)(answer() % 0x1000);
        if (answer() % 3 == 0 && n_windows()) {       /* or one that matches */
            uint32_t w = pick_window(3);
            if (w) {
                *(uint32_t *)(cur->image + 0x418) = R32(w, 0x20) + answer() % 51 - 25;
                *(uint32_t *)(cur->image + 0x41c) = R32(w, 0x24) + answer() % 51 - 25;
                *(uint32_t *)(cur->image + 0x420) = R32(w, 0x28) + answer() % 53 - 26;
                *(uint32_t *)(cur->image + 0x424) = R32(w, 0x2c) + answer() % 53 - 26;
                put(" geometry-near");
                if (counting) cover_geometry_moved++;
            }
        }
        call_chain14(0x60, 0);
        break;
    case 21: {                                          /* the matrix services */
        static uint32_t buf[0x40];
        uint32_t v = (uint32_t)(uintptr_t)arena_get(0xc0);

        for (i = 0; i < 0x30; i++)
            ((uint32_t *)(uintptr_t)v)[i] = rnd_n(3) ? rnd() & 0x7fff7fffu : rnd();
        a = answer() % 5;
        for (i = 0; i < 0x40; i++)
            buf[i] = answer();
        if (a == 0) {
            for (i = 0; i < 9; i++)
                G16(0xce8 + 2 * i) = (uint16_t)answer();
            put(" cofactors");
            ((s0)SVC(0x20))();
        } else if (a == 1) {
            rec = answer() % 4 ? pick_window(3) : 0;
            b = answer() & (answer() % 4 ? 0xf : 0xffffffffu);
            put(" orient(%#x)", b);
            ((s2)SVC(0x24))(rec && R32(rec, 0x1c) ? R32(rec, 0x1c) : v, b);
        } else if (a == 2) {
            uint16_t *m = (uint16_t *)buf;
            if (answer() % 2) {
                ((s4)SVC(0x2c))(answer() & 0xffff, answer() & 0xffff, answer() & 0xffff,
                                (uint32_t)(uintptr_t)m);
                put(" fromangles");
            }
            if (answer() % 3 == 0) {
                /* both quotients on the 45-degree switch's edge: the cosine
                 * as WINDOW takes it, through UTIL's square root */
                static const int32_t edge[6] = { 0x5a81, 0x5a82, 0x5a83, -0x5a81, -0x5a82, -0x5a83 };
                int16_t m5 = (int16_t)(answer() % 0x4000);
                uint32_t sq = 0x40000000u - (uint32_t)((int32_t)m5 * m5), k, j;
                int32_t c = (int16_t)((uint32_t (STDCALL *)(uint32_t))*(void **)(util_link + 0x1c))(sq);

                if (c < 0) c = 0x7fff;
                m[5] = (uint16_t)m5;
                for (k = 0; k < 2; k++) {
                    int32_t q = edge[answer() % 6], t = (int32_t)(((int64_t)q * c) >> 15);
                    for (j = 0; j < 5; j++) {
                        int32_t m2 = t - 2 + (int32_t)j;
                        if ((int32_t)(((int64_t)m2 * 32768) / c) == q) { t = m2; break; }
                    }
                    m[k ? 3 : 2] = (uint16_t)t;
                }
                put(" edge");
            }
            put(" angles");
            put_bytes(m, 0x12);
            ((s2)SVC(0x28))((uint32_t)(uintptr_t)m, (uint32_t)(uintptr_t)(buf + 0x20));
            put_bytes(buf + 0x20, 6);
        } else if (a == 3) {
            a = answer();
            put(" matrix(%#x)", a);
            ((s4)SVC(0x2c))(a & 0xffff, answer() & 0xffff, answer() & 0xffff,
                            (uint32_t)(uintptr_t)buf);
            put_bytes(buf, 0x12);
        } else {
            rec = pick_window(0);
            if (rec) G32(0x1194) = rec;
            put(" aspect(");
            put_ptr(G32(0x1194));
            put(")");
            if (G32(0x1194)) ((s0)SVC(0x30))();
        }
        break;
    }
    case 22:                                            /* the sim moves on */
        g_move();
        put(" steer");
        break;
    case 24: {                                          /* the spot's spring at its reach */
        uint32_t v;

        rec = pick_window(3);
        if (rec == 0 || (v = R32(rec, 0x1c)) == 0 || R32(v, 0x14) != 4 || R32(v, 0x10) != 0)
            break;
        a = (answer() % 3) * 8;
        *(uint16_t *)(uintptr_t)(v + a + 0x96) = 0;
        *(uint32_t *)(uintptr_t)(v + a + 0x98) = 0;
        *(uint16_t *)(uintptr_t)(v + a + 0x7e) = *(uint16_t *)(cur->image + 0x310 + a);
        *(uint32_t *)(uintptr_t)(v + a + 0x80) = *(uint32_t *)(cur->image + 0x312 + a);
        *(uint16_t *)(uintptr_t)(v + a + 0x5a) = *(uint16_t *)(cur->image + 0x310 + a);
        *(uint32_t *)(uintptr_t)(v + a + 0x5c) = *(uint32_t *)(cur->image + 0x312 + a) +
            (answer() % 2 ? 0x200000u : 0xffe00000u) + answer() % 3 - 1;
        put(" spring(");
        put_ptr(rec);
        put(",%u)", a);
        frame();
        break;
    }
    case 23: {                                          /* the user clicks one */
        rec = pick_window(0);
        put(" click(");
        put_ptr(rec);
        put(")");
        if (rec) h_raise(rec);
        break;
    }
    }
}

static void run_side(struct side *s)
{
    uint32_t i, nops;

    cur = s;
    fault_side = s->name;
    s->length = 0;
    s->chain14 = NULL;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    memset(arena, 0, 0x100000);
    ngroups = 0;
    loaded = 0;
    answer_at = 0;
    rng = scenario_seed;
    g_setup();
    make_stars();
    put("scenario %u stars %u/%#x\n", scenario, stars_there, stars_size);
    faulted = 0;
    in_window = 1;
    if (__builtin_setjmp(fault_jb) == 0)
        ((void (*)(void))*(void **)(s->link + 4))();
    in_window = 0;
    put_state();
    /* A fault is the sim's end: the scenario stops there. */
    nops = 4 + answer() % MAX_OPS;
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        in_window = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            op();
        in_window = 0;
        put_state();
    }
    if (!faulted) {
        put("\n#deinit:");
        in_window = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            ((void (*)(void))*(void **)(s->link + 8))();
        in_window = 0;
        put_state();
    }
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
            printf("\nFAULT in %s WINDOW at .text+%#x\n", s[i]->name,
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

static void load_cd_stars(void)
{
    char path[MAX_PATH];
    FILE *f;

    snprintf(path, sizeof path, "%s\\fs98\\Scenery\\STARS.FS", cd_root);
    f = fopen(path, "rb");
    if (f == NULL) {
        printf("no %s; synthetic stars only\n", path);
        return;
    }
    cd_stars_size = (uint32_t)fread(cd_stars, 1, sizeof cd_stars, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 20000;
    uint32_t first = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 0) : 0;
    uint32_t i, k, differ = 0;

    snprintf(cd_root, sizeof cd_root, "%s", argc > 2 ? argv[2] : ".");
    for (i = 0; cd_root[i]; i++)
        if (cd_root[i] == '/') cd_root[i] = '\\';
    AddVectoredExceptionHandler(1, on_vectored);
    if (getenv("WINDOW_NOFILTER") == NULL)
        SetUnhandledExceptionFilter(on_fault);
    load_cd_stars();
    build_stubs();
    load_util("UTIL_retail.dll");
    load_side(&retail, "WINDOW_retail.dll", "retail");
    load_side(&ours, "WINDOW_ours.dll", "ours");

    for (i = first; i < first + n; i++) {
        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        for (k = 0; k < 4096; k++)
            answers[k] = rnd();
        scenario_seed = rnd() | 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("WINDOW_DUMP") != NULL) {
            FILE *f = fopen("window_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("window_ours.txt", "wb");
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
    printf("windowdiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops:");
    for (i = 0; i < 25; i++)
        printf(" %lu", cover_ops[i]);
    printf("\n  frames %lu, 3D views painted %lu (map %lu; cockpit %lu, virtual %lu, tower %lu, "
           "track %lu, spot %lu), texts %lu, layers %lu, stars %lu, model %lu, eye claimed %lu\n",
           cover_frames, cover_paint3d, cover_map, cover_modes[0], cover_modes[1],
           cover_modes[2], cover_modes[3], cover_modes[4], cover_texts, cover_layers,
           cover_stars, cover_model, cover_claimed);
    printf("  transition states seen:");
    for (i = 0; i < 0x60; i++)
        if (cover_states[i])
            printf(" %#x:%lu", i, cover_states[i]);
    printf("\n  mode/last-mode pairs at a frame:");
    for (i = 0; i < 25; i++)
        printf(" %u%u:%lu", i / 5, i % 5, cover_pairs[i / 5][i % 5]);
    printf("\n  map height test (<-1,-1..8,>8):");
    for (i = 0; i < 11; i++)
        printf(" %lu", cover_high[i]);
    printf("\n  records:");
    for (i = 0; i < 0x20; i++)
        if (cover_records[i])
            printf(" %#x:%lu", i, cover_records[i]);
    printf("\n  restores %lu, geometry near %lu, alerts %lu, retail divides repaired %lu, "
           "faults (both sides alike, the scenario ends) %lu\n",
           cover_restored, cover_geometry_moved, cover_alerts, cover_divfix, cover_faults);
    printf("  retail faults at:");
    for (i = 0; i < 16 && fault_n[i]; i++)
        printf(" %s+%#x:%lu", fault_at[i] == 0xffffffffu ? "elsewhere" :
               fault_at[i] & 0x80000000u ? "util" : "window", fault_at[i] & 0x7fffffffu,
               fault_n[i]);
    printf("\n");
    return differ ? 1 : 0;
}
