/* Differential test: retail SOUND.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven only through Linkage, the
 * way MAIN, PANELS and the rest drive them, and through the three CHAIN lists
 * SOUND registers on.  What SOUND imports:
 *
 *   host      a stub.  Memory is a bump allocator over an arena, so heap
 *             pointers come out the same on both sides, and what the host
 *             does not zero is filled with 0xa5.  Profiles pass through to
 *             generated SOUND.CFG fixtures and a FLTSIM98.CFG rewritten for
 *             each scenario (and compared after it: +0x3c writes it).  The
 *             growable arrays (+0x2f8..+0x310) are transliterations of the
 *             host's, deferred removals and all.  The voice engine
 *             (+0x2cc..+0x2f4) is a model of the host's: 0x16c-byte voice
 *             blocks on a list, handles checked against it, the play callback
 *             kept until the harness says the sample ended -- or called at once
 *             when the scenario has no sound device -- and the host's error
 *             codes where SOUND reads them.
 *   GLOBAL    one block, set up identically before each side runs and poked
 *             between calls: the flight state SOUND reads, a view record at
 *             +0x11a0, and a fake flight model at +0x11d4 whose +0x38 returns
 *             the scenario's airspeed
 *   ACONTAIN  a stub answering +0x30 with the fixture's sound paths
 *   CHAIN     retail's, behind logging wrappers
 *   UTIL      retail's, behind logging forwarders, with its rand and frand
 *             states set identically before each side
 *
 * A scenario is a random script against one fixture tree, run once against
 * each module, under a random x87 precision control.  Compared: the
 * transcript of every import call and result, GLOBAL, FLTSIM98.CFG, the
 * arena, and SOUND's whole .data image from +0x78.  The image and the arena
 * hold pointers into the module -- into its .data, and to five of its
 * functions -- so they are compared through a translation: a pointer into
 * .data as its offset, a pointer to code through a pairing of retail's
 * functions with ours, learnt as they are first seen and then held to.
 *
 * Fixtures: the CD's seven SOUND.CFGs with most of the samples they name,
 * and nine generated trees, each rewritten for every scenario that uses it.
 * Random scripts alone left whole paths unvisited, so the generator also
 * steers onto them (see method.md, "steer onto boundaries"):
 *   - the view kind set to the engine type= the tree's SOUND.CFG uses;
 *   - engine words set exactly on the CD's curve points, and about the
 *     200-rpm cut-off, through a manifold factor of 0x8000 that halves rpm;
 *   - the wind climbing through its curve, re-armed between retunes, with
 *     the volumes often the wrong way round (their range is taken unsigned);
 *   - the eye due north or south of the aircraft, so that the range is the
 *     latitude difference alone -- under 64-bit precision the square roots
 *     of the stored and the unstored sum then differ;
 *   - the eye 400 m less 2/65536 above an aircraft on the ground, in the
 *     C182, from outside -- under 24-bit precision the two ways SOUND takes
 *     log10 land either side of an integer there;
 *   - CHAIN list 3 run many frames at once, so the idents repeat.
 *
 * Kept clear of: +0x1c before any +0x18 (retail CHAIN faults removing from a
 * list nothing was added to -- MAIN always starts SOUND first).
 *
 * Retail faults reading address 0x1c when +0x24 code 0x1b turns the horn off
 * with no voice loaded (see snd_horn).  A vectored handler catches an access
 * violation inside either SOUND, logs it and unwinds to the scenario loop, so
 * the fault is compared like any other outcome.
 *
 * Build: make sounddiff     Run: make sounddiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_LO    0x78        /* compared from here */
#define IMAGE_HI    0x177c      /* .data's virtual size */
#define GLOBAL_SIZE 0x1300
#define ARENA_SIZE  (1 << 21)
#define NTREES      16

struct import { uint32_t id; void *link; };
typedef int32_t (STDCALL *walk_fn)(void *elem, void *ctx);
struct snd_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t r0c, r10;
    void *dialogs;
    int32_t (*start)(void);
    int32_t (*stop)(void);
    int32_t (*frame)(void);
    int32_t (STDCALL *event)(int32_t code, uint32_t arg);
    void    (STDCALL *wind)(uint32_t a, uint32_t b, uint32_t c);
    void    (STDCALL *set_fader)(int32_t group, int32_t value);
    int32_t (STDCALL *get_fader)(int32_t group);
    int32_t (STDCALL *set_faders)(int32_t *f);
    int32_t (STDCALL *get_faders)(int32_t *f);
    int32_t (STDCALL *save_faders)(const double *v);
    int32_t (STDCALL *load_faders)(double *v);
    uint32_t (*markers)(void);
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
struct list { int32_t count, capacity, grow, elem_size, iterating, pending; uint8_t *data; };

/* ---- the transcript -------------------------------------------------------- */

static char tlog[1 << 23];
static unsigned tlen;

static void put(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (tlen < sizeof tlog - 512)
        tlen += (unsigned)vsnprintf(tlog + tlen, sizeof tlog - tlen, fmt, ap);
    va_end(ap);
}

static void put_s(const char *s)
{
    if (!s) { put("null"); return; }
    put("'%.60s'", s);
}

/* a double, exactly */
static void put_d(double d)
{
    uint64_t u;
    memcpy(&u, &d, 8);
    put("%016llx", (unsigned long long)u);
}

/* ---- the sides ---------------------------------------------------------------- */

struct side {
    const char *name;
    HMODULE dll;
    struct snd_linkage *link;
    uint8_t *image;             /* Linkage - 0x30 */
    uint8_t *lo, *hi;           /* the whole mapped module */
    uint8_t pristine[IMAGE_HI];
    uint32_t fn[64];            /* code addresses in order of first sight */
    unsigned nfn;
};
static struct side retail, ours, *cur;
static int counting;            /* coverage is counted on the retail run */

static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t global_blob[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t view_blob[0x80] __attribute__((aligned(16)));    /* GLOBAL +0x11a0 */
static uint8_t viewf_blob[0x10] __attribute__((aligned(16)));   /* GLOBAL +0x119c */
static uint8_t sim_link[0x40] __attribute__((aligned(4)));      /* GLOBAL +0x11d4 */
static int32_t fbuf[12];        /* +0x34/+0x38's argument */
static double dbuf[5];          /* +0x3c/+0x40's */
static uint32_t crash_ev[5];    /* +0x24 0xe's */
static int32_t ident[3];        /* +0x24 0x17's */
static char ident_text[16];

#define G8(o)  (*(uint8_t  *)(global_blob + (o)))
#define G16(o) (*(uint16_t *)(global_blob + (o)))
#define G32(o) (*(uint32_t *)(global_blob + (o)))

static int is_code(const struct side *sd, uint32_t v)
{
    return (uint8_t *)v >= sd->lo && (uint8_t *)v < sd->hi
        && !((uint8_t *)v >= sd->image && (uint8_t *)v < sd->image + IMAGE_HI);
}

static unsigned fn_id(struct side *sd, uint32_t v)
{
    unsigned k;
    for (k = 0; k < sd->nfn; k++)
        if (sd->fn[k] == v) return k;
    if (sd->nfn < 64) sd->fn[sd->nfn] = v;
    return sd->nfn++;
}

static void put_ptr(const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    if (!p)
        put("null");
    else if (q >= arena && q < arena + sizeof arena)
        put("arena+%#x", (unsigned)(q - arena));
    else if (q >= global_blob && q < global_blob + sizeof global_blob)
        put("G+%#x", (unsigned)(q - global_blob));
    else if (cur && q >= cur->image && q < cur->image + IMAGE_HI)
        put("img+%#x", (unsigned)(q - cur->image));
    else if (cur && is_code(cur, (uint32_t)q))
        put("fn%u", fn_id(cur, (uint32_t)q));
    else if (q >= (uint8_t *)fbuf && q < (uint8_t *)(fbuf + 12))
        put("fbuf+%u", (unsigned)(q - (uint8_t *)fbuf));
    else if (q >= (uint8_t *)dbuf && q < (uint8_t *)(dbuf + 5))
        put("dbuf+%u", (unsigned)(q - (uint8_t *)dbuf));
    else if ((uint32_t)q < 0x10000)
        put("%#x", (unsigned)(uint32_t)q);
    else
        put("stk");
}

/* ---- the stub host: memory ------------------------------------------------------ */

static void *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;
    if (size > ARENA_SIZE || arena_used + n > ARENA_SIZE) {
        printf("arena exhausted\n");
        exit(2);
    }
    p = arena + arena_used;
    arena_used += n;
    *(uint32_t *)p = size;
    return p + 16;
}

/* 0x40cdc0 and 0x40ce60, as the host's own code calls them */
static void *raw_alloc(uint32_t size, int zero)
{
    void *p = arena_get(size);
    memset(p, zero ? 0 : 0xa5, size);
    return p;
}

static void *raw_realloc(void *old, uint32_t size)
{
    uint8_t *p = arena_get(size);
    uint32_t n;
    memset(p, 0xa5, size);
    if (old) {
        n = *(uint32_t *)((uint8_t *)old - 16);
        memcpy(p, old, n < size ? n : size);
    }
    return p;
}

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p = raw_alloc(size, zero);
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

static int32_t STDCALL h_memset(void *d, int32_t c, uint32_t n)
{
    put(" memset(");
    put_ptr(d);
    put(",%d,%#x)", c, n);
    memset(d, c, n);
    return 0;
}

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    put(" memmove(");
    put_ptr(d);
    put(",");
    put_ptr(s);
    put(",%#x)", n);
    memmove(d, s, n);
    return 0;
}

/* ---- the stub host: strings ---------------------------------------------------- */

static HMODULE fll;

static int32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    int32_t r = LoadStringA(fll, id, buf, len);
    put(" ls(%#x)", id);
    return r;
}

static char *STDCALL h_strcpy(char *d, const char *s)
{
    strcpy(d, s);
    put(" strcpy(");
    put_ptr(d);
    put(",");
    put_s(d);
    put(")");
    return d;
}

/* 0x40ef00: the host allocator, not zeroed, then lstrcpyA */
static char *STDCALL h_strdup(const char *s)
{
    char *p;
    if (!s) { put(" strdup(null)"); return NULL; }
    p = raw_alloc((uint32_t)strlen(s) + 1, 0);
    strcpy(p, s);
    put(" strdup(");
    put_s(s);
    put(")=");
    put_ptr(p);
    return p;
}

/* 0x40ef60: two bytes a turn, -1/0/1 */
static int32_t STDCALL h_strcmp(const char *a, const char *b)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    int32_t r;
    for (;;) {
        if (x[0] != y[0]) { r = x[0] < y[0] ? -1 : 1; break; }
        if (!x[0]) { r = 0; break; }
        if (x[1] != y[1]) { r = x[1] < y[1] ? -1 : 1; break; }
        if (!x[1]) { r = 0; break; }
        x += 2;
        y += 2;
    }
    put(" cmp(");
    put_s(a);
    put(",");
    put_s(b);
    put(")=%d", r);
    return r;
}

/* ---- the stub host: paths and profiles -------------------------------------------- */

static char ext_text[0x25][8];

static void load_ext_table(const char *path)
{
    static uint8_t b[0x40000];
    FILE *f = fopen(path, "rb");
    size_t n;
    uint32_t pe, nsec, opt, i, k;
    if (!f) { printf("cannot open %s\n", path); exit(2); }
    n = fread(b, 1, sizeof b, f);
    fclose(f);
    pe = *(uint32_t *)(b + 0x3c);
    nsec = *(uint16_t *)(b + pe + 6);
    opt = *(uint16_t *)(b + pe + 20);
    for (k = 1; k < 0x25; k++) {
        uint32_t va = 0x417f20 + 4 * k, ptr = 0, off = 0;
        for (i = 0; i < nsec; i++) {
            uint8_t *s = b + pe + 24 + opt + 40 * i;
            uint32_t sva = 0x400000 + *(uint32_t *)(s + 12), rs = *(uint32_t *)(s + 16),
                     ro = *(uint32_t *)(s + 20);
            if (va >= sva && va < sva + rs) ptr = *(uint32_t *)(b + ro + va - sva);
        }
        for (i = 0; i < nsec; i++) {
            uint8_t *s = b + pe + 24 + opt + 40 * i;
            uint32_t sva = 0x400000 + *(uint32_t *)(s + 12), rs = *(uint32_t *)(s + 16),
                     ro = *(uint32_t *)(s + 20);
            if (ptr >= sva && ptr < sva + rs) off = ro + ptr - sva;
        }
        if (!off || off + 8 > n) { printf("bad extension table\n"); exit(2); }
        strncpy(ext_text[k], (char *)b + off, 7);
    }
    if (strcmp(ext_text[0x24], ".CFG") || strcmp(ext_text[0x13], ".WAV")) {
        printf("extension table: unexpected contents\n");
        exit(2);
    }
}

/* 0x40b350 */
static int resolve(const char *path, int32_t ext, char *out, size_t len)
{
    const char *t;
    if (ext == -2) t = "";
    else if (ext >= 0 && ext < 0x25) t = ext_text[ext];
    else return -1;
    snprintf(out, len, "%s%s", path, t);
    return 0;
}

/* 0x40a510 */
static int32_t STDCALL h_join(char *out, const char *drive, const char *dir, const char *name)
{
    char b[0x208];
    size_t n;
    memset(b, 0, sizeof b);
    if (!out) return -1;
    if (drive && strlen(drive)) {
        strcat(b, drive);
        if (strlen(b) == 1) strcat(b, ":");
        n = strlen(b);
        if (_strcmpi(b + n - 1, "\\") == 0) b[n - 1] = 0;
    }
    if (dir && strlen(dir)) {
        if (dir[0] != '\\' && strlen(b)) strcat(b, "\\");
        strcat(b, dir);
        n = strlen(b);
        if (_strcmpi(b + n - 1, "\\") == 0) b[n - 1] = 0;
    }
    if (name && strlen(name)) {
        if (name[0] != '\\' && strlen(b)) strcat(b, "\\");
        strcat(b, name);
    }
    put(" join=");
    if (strlen(b) >= 0x104) {
        out[0] = 0;
        put("TOOLONG");
        return -1;
    }
    strcpy(out, b);
    put_s(out);
    return 0;
}

/* 0x40a6c0, for what SOUND asks of it: the name only */
static int32_t STDCALL h_split(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{
    char e[0x100];
    if (!path) return -1;
    if (drive || dir || ext) { printf("split: unexpected arguments\n"); exit(2); }
    if (path[0] == 0) {
        if (name) name[0] = 0;
    } else
        _splitpath(path, NULL, NULL, name, e);
    put(" split=");
    put_s(name);
    return 0;
}

static unsigned long cover_cfg_hit, cover_prof_hit, cover_exists[2];

static int32_t STDCALL h_exists(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;
    if (resolve(path, ext, full, sizeof full)) { put(" exists=-1"); return -1; }
    r = GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES ? -1 : 0;
    if (counting) cover_exists[r == 0]++;
    put(" exists(");
    put_s(full);
    put(")=%d", r);
    return r;
}

static const char *const cfg_path = ".\\FLTSIM98.CFG";

/* 0x408ac0 */
static int32_t STDCALL h_cfg_double(const char *sec, const char *key, double *out)
{
    char b[0x20];
    b[0] = 0;
    GetPrivateProfileStringA(sec, key, b, b, 0x1e, cfg_path);
    put(" cfgd(");
    put_s(key);
    put(")=");
    put_s(b);
    if (b[0]) {
        *out = strtod(b, NULL);
        if (counting) cover_cfg_hit++;
    }
    return 0;
}

/* 0x408b10 */
static int32_t STDCALL h_cfg_put_double(const char *sec, const char *key, double v)
{
    char b[0x20];
    snprintf(b, sizeof b, "%f", v);     /* what sprintf("%f") gives, up to 0x1f */
    put(" cfgw(");
    put_s(key);
    put(",");
    put_d(v);
    put(")");
    WritePrivateProfileStringA(sec, key, b, cfg_path);
    return 0;
}

static int32_t STDCALL h_prof_int(const char *sec, const char *key, int32_t *inout,
                                  const char *path, int32_t ext)
{
    char full[0x200];
    if (resolve(path, ext, full, sizeof full)) { put(" profi=-1"); return -1; }
    *inout = (int32_t)GetPrivateProfileIntA(sec, key, *inout, full);
    put(" profi(");
    put_s(sec);
    put(",");
    put_s(key);
    put(")=%d", *inout);
    return 0;
}

static int32_t STDCALL h_prof_double(const char *sec, const char *key, double *inout,
                                     const char *path, int32_t ext)
{
    char full[0x200], b[0x20];
    if (resolve(path, ext, full, sizeof full)) { put(" profd=-1"); return -1; }
    b[0] = 0;
    GetPrivateProfileStringA(sec, key, b, b, 0x1e, full);
    if (b[0]) {
        *inout = strtod(b, NULL);
        if (counting) cover_prof_hit++;
    }
    put(" profd(");
    put_s(sec);
    put(",");
    put_s(key);
    put(")=");
    put_d(*inout);
    return 0;
}

/* 0x409210 */
static int32_t STDCALL h_prof_doubles(const char *sec, const char *key, double *out, uint32_t n,
                                      const char *path, int32_t ext)
{
    char full[0x200];
    char *b, *p;
    uint32_t k;
    if (resolve(path, ext, full, sizeof full)) { put(" profn=-1"); return -1; }
    b = raw_alloc(n * 30, 0);
    b[0] = 0;
    GetPrivateProfileStringA(sec, key, b, b, n * 30, full);
    put(" profn(");
    put_s(sec);
    put(",");
    put_s(key);
    put(",%u)=", n);
    put_s(b);
    if (b[0] && n) {
        if (counting) cover_prof_hit++;
        p = b;
        for (k = 0; k < n; k++) {
            out[k] = strtod(p, &p);
            p += strspn(p, ", ");
        }
    }
    return 0;
}

static int32_t STDCALL h_prof_string(const char *sec, const char *key, char *buf, uint32_t size,
                                     const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;
    if (resolve(path, ext, full, sizeof full)) { put(" prof=-1"); return -1; }
    r = GetPrivateProfileStringA(sec, key, buf, buf, size, full);
    if (counting && r > 0) cover_prof_hit++;
    put(" prof(");
    put_s(sec);
    put(",");
    put_s(key);
    put(")=%d,", r);
    put_s(buf);
    return r;
}

static void STDCALL h_busy(int32_t on) { put(" busy(%d)", on); }

/* ---- the stub host: growable arrays, 0x4101d0 on ---------------------------------- */

static struct list pending_list;    /* 0x418380: { list, index } pairs */

/* 0x410410: make room for one more; the index */
static int32_t list_grow(struct list *l)
{
    if (l->count == l->capacity) {
        l->capacity += l->grow;
        if (l->count == 0)
            l->data = raw_alloc((uint32_t)(l->elem_size * l->capacity), 0);
        else
            l->data = raw_realloc(l->data, (uint32_t)(l->elem_size * l->capacity));
    }
    return l->count++;
}

/* 0x4101d0 */
static void *list_add(struct list *l)
{
    int32_t i = list_grow(l);
    uint8_t *e = l->data + i * l->elem_size;
    memset(e, 0, (size_t)l->elem_size);
    return e;
}

/* 0x410470 */
static void list_remove_at(struct list *l, int32_t i)
{
    if (l->iterating) {
        uint32_t *q;
        l->pending = 1;
        q = list_add(&pending_list);
        q[0] = (uint32_t)l;
        q[1] = (uint32_t)i;
        return;
    }
    l->count--;
    if (i < l->count)
        memmove(l->data + i * l->elem_size, l->data + (i + 1) * l->elem_size,
                (size_t)((l->count - i) * l->elem_size));
}

static void *STDCALL h_list_add(struct list *l)
{
    void *e = list_add(l);
    put(" ladd(");
    put_ptr(l);
    put(")=");
    put_ptr(e);
    return e;
}

static int32_t STDCALL h_list_free(struct list *l)
{
    put(" lfree(");
    put_ptr(l);
    put(")");
    if (l->data) l->data = NULL;
    return 0;
}

static void STDCALL h_list_remove(struct list *l, void *elem)
{
    uint32_t i = (uint32_t)((uint8_t *)elem - l->data) / (uint32_t)l->elem_size;
    put(" lrem(");
    put_ptr(l);
    put(",%u)", i);
    list_remove_at(l, (int32_t)i);
}

/* 0x4102a0 */
static int32_t STDCALL h_list_walk(struct list *l, void *ctx, walk_fn fn)
{
    int32_t r = 0, i, k;
    put(" walk(");
    put_ptr(l);
    put(",");
    put_ptr(ctx);
    put(",");
    put_ptr((void *)fn);
    put(")[");
    l->iterating++;
    for (i = 0; i < l->count; i++) {
        r = fn(l->data + i * l->elem_size, ctx);
        if (r) break;
    }
    l->iterating--;
    if (l->pending) {
        for (k = pending_list.count; k > 0; k--) {
            uint32_t *q = (uint32_t *)(pending_list.data + (k - 1) * 8);
            if (q[0] == (uint32_t)l) {
                list_remove_at(l, (int32_t)q[1]);
                list_remove_at(&pending_list, k - 1);
            }
        }
        l->pending = 0;
    }
    put("]=");
    put_ptr((void *)r);
    return r;
}

/* 0x410240 */
static void *STDCALL h_list_find(struct list *l, uint32_t off, const void *key, uint32_t len,
                                 int32_t indirect)
{
    int32_t i;
    uint8_t *e = l->data + off, *r = NULL;
    for (i = 0; i < l->count; i++, e += l->elem_size) {
        if (indirect ? memcmp(*(void **)e, *(void **)key, len) == 0 : memcmp(e, key, len) == 0) {
            r = l->data + i * l->elem_size;
            break;
        }
    }
    put(" find(%d)=", len == 4 ? *(const int32_t *)key : -1);
    put_ptr(r);
    return r;
}

/* ---- the stub host: the voice engine ---------------------------------------------- */

/* A voice is a 0x16c-byte block, as the host's: +0x20 its source, +0x24 the
 * load type, +0x28 a reference count, +0x38 looping, +0x48 the rate,
 * +0x50 the volume, +0x58/+0x5c the pending callback and its flag, +0x168
 * the next on the list.  The rest is malloc's garbage, here 0xa5. */
#define V32(v, o) (*(uint32_t *)((uint8_t *)(v) + (o)))
typedef void (STDCALL *done_fn)(uint32_t);

static int sound_state;         /* 0x41a3cc: 1 is off */
static int device;              /* 0x41a3c8 */
static uint32_t voices;         /* 0x41a3c0 */
static uint32_t voice_salt;
static unsigned long cover_load[4], cover_play[3], cover_append[2], cover_cb[2], cover_badh,
                     cover_freq, cover_vol, cover_pan, cover_stop;

static int voice_ok(uint32_t h)
{
    uint32_t v;
    for (v = voices; v; v = V32(v, 0x168))
        if (v == h) return 1;
    if (counting) cover_badh++;
    return 0;
}

static int32_t STDCALL h_sound_off(void)
{
    put(" soundoff=%d", sound_state == 0);
    return sound_state == 0;
}

static int32_t STDCALL h_voice_play(uint32_t h, int32_t loop, double freq, done_fn done);

/* 0x40f120, as far as SOUND can see it: switched back on, every looping voice
 * plays again, which calls a callback left pending */
static int32_t STDCALL h_sound_enable(int32_t on)
{
    uint32_t v;
    put(" enable(%d)[", on);
    if (sound_state != 1 && on)
        for (v = voices; v; v = V32(v, 0x168))
            if (V32(v, 0x38))
                h_voice_play(v, (int32_t)V32(v, 0x38), *(double *)((uint8_t *)v + 0x48),
                             (done_fn)V32(v, 0x58));
    put("]");
    return 0;
}

static unsigned hash(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* 0x40f840, types 2, 4 and 5: from a file */
static int32_t STDCALL h_voice_load(const void *src, int32_t type, uint32_t *out)
{
    char full[0x200];
    uint32_t v;
    int32_t r = 0;
    put(" vload(");
    put_s(src);
    put(",%d)=", type);
    if (sound_state == 1) {
        *out = 0;
        put("-1");
        return -1;
    }
    if (type != 2 && type != 4 && type != 5) { printf("vload: type %d\n", type); exit(2); }
    if (!src) {             /* the host would strcpy from null; say so instead */
        *out = 0;
        put("NULLFILE");
        if (counting) cover_load[3]++;
        return 0x90002;
    }
    snprintf(full, sizeof full, "%s.WAV", (const char *)src);
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
        *out = 0;
        put("%#x", 0x90002);
        if (counting) cover_load[0]++;
        return 0x90002;
    }
    v = (uint32_t)raw_alloc(0x16c, 0);
    V32(v, 0) = 0x16c;
    V32(v, 4) = 0;
    V32(v, 0x60) = V32(v, 0x58) = V32(v, 0x5c) = 0;
    V32(v, 0x20) = (uint32_t)src;
    V32(v, 0x24) = (uint32_t)type;
    V32(v, 0x28) = 1;
    V32(v, 0x50) = 10000;
    V32(v, 0x38) = V32(v, 0x3c) = V32(v, 0x40) = V32(v, 0x44) = V32(v, 0x48) = V32(v, 0x4c) = 0;
    if (type != 4 && (hash(full) ^ voice_salt) % 11 == 0)
        r = 0x90003;        /* no buffer: kept only with no device */
    if (r && device) {
        *out = 0;
        put("%#x", r);
        if (counting) cover_load[2]++;
        return r;
    }
    V32(v, 0x168) = voices;
    voices = v;
    *out = v;
    put_ptr((void *)v);
    if (r) put(",%#x", r);
    if (counting) cover_load[r ? 2 : 1]++;
    return r;
}

/* 0x40fc00 */
static int32_t STDCALL h_voice_append(uint32_t src, int32_t type, uint32_t dst, int32_t final)
{
    put(" vappend(");
    put_ptr((void *)src);
    put(",%d,", type);
    put_ptr((void *)dst);
    put(",%d)", final);
    if (!device) return 0;
    if (!voice_ok(dst)) { put("=-1"); return -1; }
    if (src == 0 && type == 0 && final == 1) {
        if (counting) cover_append[1]++;
        return (hash("final") ^ voice_salt ^ dst) % 13 == 0 ? 0x90003 : 0;
    }
    if (type != 2 && type != 6 && type != 7) return 0;
    if (type != 2 && !voice_ok(src)) { put("=-1"); return -1; }
    if (counting) cover_append[0]++;
    return 0;
}

/* 0x40fb30 */
static int32_t STDCALL h_voice_free(uint32_t *h)
{
    uint32_t *p;
    put(" vfree(");
    put_ptr((void *)*h);
    put(")");
    if (sound_state == 1) return 0;
    if (!voice_ok(*h)) return 0x90019;
    if (--V32(*h, 0x28) == 0)
        for (p = &voices; *p; p = &V32(*p, 0x168))
            if (*p == *h) { *p = V32(*h, 0x168); break; }
    *h = 0;
    return 0;
}

/* 0x40fd50 */
static int32_t STDCALL h_voice_play(uint32_t h, int32_t loop, double freq, done_fn done)
{
    done_fn old;
    put(" vplay(");
    put_ptr((void *)h);
    put(",%d,", loop);
    put_d(freq);
    put(",");
    put_ptr((void *)done);
    put(")");
    if (sound_state == 1) return -1;
    if (!voice_ok(h) || V32(h, 0x24) == 4) { put("=%#x", 0x9001c); return 0x9001c; }
    memcpy((uint8_t *)h + 0x48, &freq, 8);
    V32(h, 0x38) = (uint32_t)loop;
    if (V32(h, 0x58) && V32(h, 0x5c)) {
        old = (done_fn)V32(h, 0x58);
        V32(h, 0x5c) = 0;
        put("[pending");
        if (counting) cover_cb[0]++;
        old(h);
        put("]");
    }
    if (done) {
        if (!device) {
            put("[now");
            if (counting) cover_cb[1]++;
            done(h);
            put("]");
        } else {
            V32(h, 0x5c) = 1;
            V32(h, 0x58) = (uint32_t)done;
        }
    }
    if (!device) {
        if (counting) cover_play[0]++;
        return 0;
    }
    if ((hash("play") ^ voice_salt ^ h) % 17 == 0) {
        put("=%#x", 0x9001a);
        if (counting) cover_play[2]++;
        return 0x9001a;
    }
    V32(h, 0x3c) = 1;
    if (counting) cover_play[1]++;
    return 0;
}

static int32_t STDCALL h_voice_freq(uint32_t h, double f)
{
    put(" vfreq(");
    put_ptr((void *)h);
    put(",");
    put_d(f);
    put(")");
    if (counting) cover_freq++;
    if (sound_state == 1) return 0;
    if (!voice_ok(h) || V32(h, 0x24) == 4) return 0x9002d;
    memcpy((uint8_t *)h + 0x48, &f, 8);
    return 0;
}

static int32_t STDCALL h_voice_volume(uint32_t h, int32_t v)
{
    put(" vvol(");
    put_ptr((void *)h);
    put(",%d)", v);
    if (counting) cover_vol++;
    if (sound_state == 1) return 0;
    if (!voice_ok(h) || V32(h, 0x24) == 4) return 0x9002f;
    V32(h, 0x50) = (uint32_t)v;
    return 0;
}

static int32_t STDCALL h_voice_pan(uint32_t h, int32_t pan)
{
    put(" vpan(");
    put_ptr((void *)h);
    put(",%d)", pan);
    if (counting) cover_pan++;
    if (sound_state == 1) return 0;
    if (!voice_ok(h) || V32(h, 0x24) == 4) return 0x90031;
    V32(h, 0x54) = (uint32_t)pan;
    return 0;
}

static int32_t STDCALL h_voice_stop(uint32_t h)
{
    put(" vstop(");
    put_ptr((void *)h);
    put(")");
    if (counting) cover_stop++;
    if (sound_state == 1) return 0;
    if (!voice_ok(h) || V32(h, 0x24) == 4) return 0x9003e;
    V32(h, 0x3c) = 0;
    V32(h, 0x38) = 0;
    return 0;
}

/* the harness's own: a voice's sample ends, and its callback runs */
static void voice_end(unsigned pick)
{
    uint32_t v, n = 0, k = 0;
    done_fn cb;
    for (v = voices; v; v = V32(v, 0x168))
        if (V32(v, 0x5c)) n++;
    put("\nEND ");
    if (!n) { put("none"); return; }
    for (v = voices; v; v = V32(v, 0x168))
        if (V32(v, 0x5c) && k++ == pick % n) break;
    put_ptr((void *)v);
    cb = (done_fn)V32(v, 0x58);
    V32(v, 0x5c) = 0;
    V32(v, 0x3c) = 0;
    cb(v);
}

/* ---- ACONTAIN, the flight model, the crash callback ---------------------------------- */

static unsigned tree;
static int acontain_fail;
static double airspeed;

static int32_t STDCALL ac_query(void *aircraft, int32_t key, char *buf, uint32_t len)
{
    char p[0x100];
    put(" acq(");
    put_ptr(aircraft);
    put(",%d,%#x)", key, len);
    if (acontain_fail == key) { put("=0"); return 0; }
    if (key == 8) snprintf(p, sizeof p, "sfix\\t%u\\sound\\sound", tree);
    else if (key == 7) snprintf(p, sizeof p, "sfix\\t%u\\sound", tree);
    else { printf("ACONTAIN +0x30 key %d\n", key); exit(2); }
    strncpy(buf, p, len);
    return 1;
}

static double sim_value(void)
{
    put(" sim=");
    put_d(airspeed);
    return airspeed;
}

static unsigned long cover_crash_cb;
static void crash_cb(void)
{
    put(" crashcb");
    if (counting) cover_crash_cb++;
}

/* ---- CHAIN and UTIL, retail's behind wrappers --------------------------------------- */

static struct chain_linkage *chain;
static uint8_t chain_fwd[0x24];

static int32_t STDCALL c_add(uint32_t list, cb_fn fn)
{
    put(" chain.add(%u,", list);
    put_ptr((void *)fn);
    put(")");
    return chain->add(list, fn);
}

static int32_t STDCALL c_remove(uint32_t list, cb_fn fn)
{
    int32_t r;
    put(" chain.remove(%u,", list);
    put_ptr((void *)fn);
    r = chain->remove(list, fn);
    put(")=%d", r);
    return r;
}

static uint32_t STDCALL c_dispatch(uint32_t list, uint32_t a, uint32_t b)
{
    put(" chain.dispatch(%u,%#x,%#x)[", list, a, b);
    chain->dispatch(list, a, b);
    put("]");
    return 0;
}

static uint8_t *util_link;
static uint32_t *util_seed;
static double *util_fseed;
static uint8_t util_fwd[0x100];

#define UTIL(off, type) (*(type *)(util_link + (off)))
typedef void     (STDCALL *convert_fn)(const void *, const void *, void *);
typedef uint32_t (STDCALL *rand_fn)(void);
typedef int32_t  (STDCALL *dtoi_fn)(double);
typedef double   (STDCALL *frand_fn)(void);

static void put48(const void *p)
{
    put("%08x.%04x", *(const uint32_t *)((const uint8_t *)p + 4),
        *(const uint16_t *)((const uint8_t *)p + 2));
}

static void STDCALL u_to_m(const void *in, const void *pos, void *out)
{
    UTIL(0x48, convert_fn)(in, pos, out);
    put(" tom("); put48(in); put(")="); put48(out);
}

static uint32_t STDCALL u_rand32(void)
{
    uint32_t r = UTIL(0x70, rand_fn)();
    put(" rand32=%x", r);
    return r;
}

static int32_t STDCALL u_dtoi(double v)
{
    int32_t r = UTIL(0xb4, dtoi_fn)(v);
    put(" dtoi(");
    put_d(v);
    put(")=%d", r);
    return r;
}

static double STDCALL u_frand(void)
{
    double r = UTIL(0xb8, frand_fn)();
    put(" frand=");
    put_d(r);
    return r;
}

/* ---- fixtures ---------------------------------------------------------------------- */

static uint32_t rs = 0x2545f491u;
static uint32_t rng(void)
{
    uint32_t x;
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    x = rs * 0x9e3779b1u;
    return x ^ (x >> 15);
}

static char sounddir[0x104];    /* string 0x9005 */
static int tree_kind[NTREES];   /* the engine type= the tree's SOUND.CFG uses */

static void deltree(const char *dir)
{
    WIN32_FIND_DATAA d;
    char pat[MAX_PATH], p[MAX_PATH];
    HANDLE h;
    snprintf(pat, sizeof pat, "%s\\*", dir);
    h = FindFirstFileA(pat, &d);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, "..")) continue;
            snprintf(p, sizeof p, "%s\\%s", dir, d.cFileName);
            if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) deltree(p);
            else { SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p); }
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

static void touch(const char *dir, const char *name)
{
    char p[MAX_PATH];
    FILE *f;
    snprintf(p, sizeof p, "%s\\%s.WAV", dir, name);
    f = fopen(p, "wb");
    if (f) fclose(f);
}

/* every file name the templates and the CD's SOUND.CFGs use, and some odd */
static const char *const wavs[] = {
    "prop", "737lp", "gearup", "geardown", "flaps", "outermk", "middmk", "innermk",
    "stallhrn", "screetch", "pcrash3", "psplash", "wind3", "MRSDAHH", "MRSDAHL",
    "MRSDITH", "MRSDITL", "MRSSP", "mrssp", "gearwarn", "caapdis", "laglide",
    "rumblp", "cruiselp", "be1", "xbe3", "bmgeardn", "bmgearup", "bmflaph",
    "bmflaplp", "bmflapt", "bastall", "crash2", "crash4", "splash2", "xprop1",
    "xprop2", "xprop3", "jetw", "jetc",
};
#define NWAVS (sizeof wavs / sizeof wavs[0])

static const char *const sections[] = {
    "SOUND_ENGINE", "JET_ENGINE", "GEAR_UP", "GEAR_DOWN", "FLAPSH", "FLAPSL", "FLAPST",
    "OUTER_MARKER", "MIDDLE_MARKER", "INNER_MARKER", "STALL_WARNING", "LEFT_TOUCHDOWN",
    "CENTER_TOUCHDOWN", "RIGHT_TOUCHDOWN", "CRASH_SOUND", "SPLASH_SOUND", "WIND_SOUND",
    "LOW_TONE_SOUND", "MID_TONE_SOUND", "HIGH_TONE_SOUND", "MRSDAHH_SOUND",
    "MRSDAHL_SOUND", "MRSDITH_SOUND", "MRSDITL_SOUND", "MRSSP_SOUND",
    "OVERSPEED_WARNING_SOUND", "GEAR_UP_WARNING_SOUND", "AP_DISENGAGE_SOUND",
    "GLIDESLOPE_WARNING_SOUND",
};
#define NSECTIONS (sizeof sections / sizeof sections[0])

static const char *pick(const char *const *a, unsigned n) { return a[rng() % n]; }

static void put_number(FILE *f, const char *key, int sign)
{
    switch (rng() % 9) {
    case 0: fprintf(f, "%s=%d\n", key, (int)(rng() % 20001) - (sign ? 10000 : 0)); break;
    case 1: fprintf(f, "%s=%d\n", key, (int)rng()); break;
    case 2: fprintf(f, "%s=\n", key); break;
    case 3: fprintf(f, "%s=x%u\n", key, rng() % 9); break;
    default: fprintf(f, "%s=%u\n", key, rng() % 10001); break;
    }
}

static void put_real(FILE *f, const char *key)
{
    switch (rng() % 8) {
    case 0: fprintf(f, "%s=-%u.%u\n", key, rng() % 50, rng() % 100); break;
    case 1: fprintf(f, "%s=0\n", key); break;
    case 2: fprintf(f, "%s=\n", key); break;
    case 3: fprintf(f, "%s=%ue%u\n", key, rng() % 9, rng() % 4); break;
    default: fprintf(f, "%s=%u.%03u\n", key, rng() % 400, rng() % 1000); break;
    }
}

/* n numbers, rising or not, with odd separators */
static void put_list(FILE *f, const char *key, unsigned n)
{
    static const char *const seps[] = { ",", ", ", " ", ",,", " ,", ";" };
    unsigned k, m = rng() % 5 == 0 ? rng() % (n + 3) : n;
    double x = (double)(rng() % 3000) / 10.0;
    fprintf(f, "%s=", key);
    for (k = 0; k < m; k++) {
        if (rng() % 6 == 0) x -= (double)(rng() % 500) / 10.0;
        else x += (double)(rng() % 900) / 10.0;
        fprintf(f, "%s%.3f", k ? seps[rng() % 8 < 6 ? 0 : rng() % 6] : "", k % 2 && n == 16 ? (double)(rng() % 2000) / 1000.0 : x);
    }
    fprintf(f, "\n");
}

static void write_section(FILE *f, const char *name, unsigned t, unsigned depth)
{
    unsigned k;
    fprintf(f, "\n[%s]\n", name);
    if (rng() % 8 == 0) put_number(f, "flags", 0);
    else if (depth && rng() % 3 == 0) fprintf(f, "flags=8\n");
    else if (rng() % 3 == 0) fprintf(f, "flags=%u\n", (rng() % 8) * 2 + (rng() % 7 == 0));
    if (rng() % 4) fprintf(f, "filename=%s%s\n", rng() % 6 ? "" : "sub\\",
                           rng() % 8 ? pick(wavs, NWAVS) : "missing");
    if (rng() % 2) put_number(f, "initial_volume", 0);
    if (rng() % 2) put_number(f, "maximum_volume", 0);
    if (rng() % 2) put_number(f, "minimum_volume", 0);
    if (rng() % 3 == 0) put_number(f, "panning", 1);
    if (rng() % 3 == 0) fprintf(f, "viewpoint=%u\n", rng() % 4);
    if (!strcmp(name, "SOUND_ENGINE") || !strcmp(name, "JET_ENGINE") || depth) {
        if (rng() % 3) fprintf(f, "type=%u\n", rng() % 5 == 0 ? rng() % 6 : t);
        if (rng() % 2) put_list(f, "rparams", 4);
        if (rng() % 2) put_list(f, "vparams", 16);
        if (rng() % 3) fprintf(f, "link=%s%u\n", "sub", rng() % 6);
    }
    if (!strcmp(name, "CRASH_SOUND") || !strcmp(name, "SPLASH_SOUND")) {
        if (rng() % 3) {
            unsigned n = rng() % 11;
            fprintf(f, "extra=");
            for (k = 0; k < n; k++)
                fprintf(f, "%s%s", k ? (rng() % 3 ? "," : ", ") : "", pick(wavs, NWAVS));
            fprintf(f, "\n");
        }
    }
    if (!strcmp(name, "WIND_SOUND")) {
        /* the wind's file present, and its volumes often the wrong way
         * round: their difference is taken unsigned */
        if (rng() % 2) fprintf(f, "filename=wind3\n");
        if (rng() % 2) {
            unsigned a = rng() % 10001, b = rng() % 10001;
            fprintf(f, "minimum_volume=%u\nmaximum_volume=%u\n", a > b ? a : b, a > b ? b : a);
        }
        if (rng() % 3) put_real(f, "minimum_rate");
        if (rng() % 3) put_real(f, "maximum_rate");
        if (rng() % 3) put_real(f, "minimum_speed");
        if (rng() % 3) put_real(f, "maximum_speed");
    }
}

static void copy_file(const char *from, const char *to)
{
    CopyFileA(from, to, FALSE);
}

static void write_tree(unsigned t)
{
    char dir[MAX_PATH], p[MAX_PATH];
    unsigned k, kind = rng() % 4;
    FILE *f;
    static const char *const cd[] = { "B737_400", "Extra300", "Bell206B", "Schweizr",
                                      "Lear45", "Sopcamel", "C182" };
    snprintf(dir, sizeof dir, "sfix\\t%u", t);
    deltree(dir);
    CreateDirectoryA(dir, NULL);
    snprintf(dir, sizeof dir, "sfix\\t%u\\sound", t);
    CreateDirectoryA(dir, NULL);
    snprintf(p, sizeof p, "%s\\sound.cfg", dir);
    if (t < 7) {
        /* the CD's own, and most of the samples it names */
        char from[MAX_PATH], line[0x200], *v, *tok;
        int kind_seen = 0;
        snprintf(from, sizeof from, "cdsound\\%s.cfg", cd[t]);
        copy_file(from, p);
        f = fopen(p, "r");
        while (f && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "type=", 5) && !kind_seen) {
                tree_kind[t] = atoi(line + 5);
                kind_seen = 1;
            }
            if (!strncmp(line, "filename=", 9)) v = line + 9;
            else if (!strncmp(line, "extra=", 6)) v = line + 6;
            else continue;
            for (tok = strtok(v, ", "); tok; tok = strtok(NULL, ", "))
                if (rng() % 40) touch(dir, tok);
        }
        if (f) fclose(f);
    } else {
        tree_kind[t] = (int)kind;
        f = fopen(p, "w");
        for (k = 0; k < NSECTIONS; k++)
            if (rng() % 5) write_section(f, sections[k], kind, 0);
        for (k = 0; k < 6; k++)
            if (rng() % 3) {
                char sub[8];
                snprintf(sub, sizeof sub, "sub%u", k);
                write_section(f, sub, kind, 1);
            }
        fclose(f);
    }
    for (k = 0; k < NWAVS; k++)
        if (rng() % 3) touch(dir, wavs[k]);
}

/* ---- scenarios --------------------------------------------------------------------- */

enum { OP_START, OP_STOP, OP_FRAME, OP_EVENT, OP_WIND, OP_SET_FADER, OP_GET_FADER,
       OP_SET_FADERS, OP_GET_FADERS, OP_SAVE, OP_LOAD, OP_MARKERS, OP_MSG, OP_LIGHT,
       OP_TICK, OP_END, OP_POKE, OP_COUNT };
static const char *const op_names[OP_COUNT] = {
    "start", "stop", "frame", "event", "wind", "set_fader", "get_fader", "set_faders",
    "get_faders", "save", "load", "markers", "msg", "light", "tick", "end", "poke"
};

struct op { int kind; uint32_t a, b, c, d; };
#define MAXOPS 60
struct scenario {
    unsigned tree, nops;
    int state, device, acfail, precision, viewsel, viewfsel, level;
    uint32_t seed, salt;
    double fseed;
    char cfg[400];
    uint8_t global[GLOBAL_SIZE];
    uint8_t view[0x80], viewf[0x10];
    struct op op[MAXOPS];
};

static void make_cfg(struct scenario *s)
{
    unsigned k, n = 0;
    n += (unsigned)snprintf(s->cfg + n, sizeof s->cfg - n, "[PREFS]\r\n");
    for (k = 1; k <= 5; k++) {
        switch (rng() % 6) {
        case 0: break;
        case 1: n += (unsigned)snprintf(s->cfg + n, sizeof s->cfg - n, "SOUND_FADER%u=-0.%u\r\n", k, rng() % 10); break;
        case 2: n += (unsigned)snprintf(s->cfg + n, sizeof s->cfg - n, "SOUND_FADER%u=%u.%u\r\n", k, rng() % 3, rng() % 1000); break;
        case 3: n += (unsigned)snprintf(s->cfg + n, sizeof s->cfg - n, "SOUND_FADER%u=junk\r\n", k); break;
        default: n += (unsigned)snprintf(s->cfg + n, sizeof s->cfg - n, "SOUND_FADER%u=0.%03u\r\n", k, rng() % 1000); break;
        }
    }
}

static int16_t s16(void) { return (int16_t)(rng() % 4 ? (int)(rng() % 6000) - 1000 : (int)rng()); }

static void make_global(uint8_t *g)
{
    uint32_t k;
    memset(g, 0, GLOBAL_SIZE);
    *(uint32_t *)(g + 0xb24) = rng() % 6 ? 1 : rng() % 3;
    *(uint32_t *)(g + 0x1214) = rng() % 4;
    g[0x609] = (uint8_t)(rng() % 9 ? rng() % 4 : rng());
    *(uint16_t *)(g + 0xaec) = (uint16_t)(rng() % 7);
    for (k = 0; k < 4; k++) {
        /* a prop's rpm, a jet's 16384ths, or anything */
        switch (rng() % 5) {
        case 0: case 1: *(uint16_t *)(g + 0x898 + 0x98 * k) = (uint16_t)(rng() % 4000); break;
        case 2: case 3: *(uint16_t *)(g + 0x898 + 0x98 * k) = (uint16_t)(rng() % 20000); break;
        default: *(uint16_t *)(g + 0x898 + 0x98 * k) = (uint16_t)rng(); break;
        }
        *(int16_t *)(g + 0x88c + 0x98 * k) = s16();
    }
    /* a 0.16 factor: near 1.0 mostly */
    *(uint16_t *)(g + 0x8c8) = (uint16_t)(rng() % 4 ? 0x8000 + rng() % 0x8000 : rng());
    *(uint16_t *)(g + 0x2a8) = (uint16_t)rng();
    *(int16_t *)(g + 0x2aa) = (int16_t)(rng() % 8 ? (int)(1 + rng() % 20) : (int)(rng() % 5) - 2);
    g[0x365] = rng() % 6 == 0;
    g[0x36c] = rng() % 3 == 0;
    g[0xb6a] = rng() % 5 == 0;
    *(uint32_t *)(g + 0xbe0) = rng() % 4;
    *(uint32_t *)(g + 0xbe4) = rng() % 4;
    *(uint32_t *)(g + 0x564) = (uint32_t)((int32_t)(rng() % 0x600000) - 0x300000);
    *(uint32_t *)(g + 0x56c) = rng();
    *(uint16_t *)(g + 0x562) = (uint16_t)rng();
    *(uint16_t *)(g + 0x56a) = (uint16_t)rng();
    *(uint32_t *)(g + 0x574) = rng() % 4 ? rng() % 20000 : rng();
    *(uint16_t *)(g + 0x572) = (uint16_t)rng();
}

/* the view record at +0x11a0: +0x10, +0x14 the kind, +0x58..+0x6e the eye */
static void make_view(struct scenario *s)
{
    uint8_t *v = s->view;
    unsigned k;
    for (k = 0; k < sizeof s->view; k++) v[k] = (uint8_t)rng();
    *(uint32_t *)(v + 0x10) = rng() % 3;
    *(uint32_t *)(v + 0x14) = rng() % 4 ? rng() % 3 : rng() % 6;
    *(uint32_t *)(v + 0x5c) = *(uint32_t *)(s->global + 0x564) + (rng() % 64) - 32;
    *(uint32_t *)(v + 0x64) = *(uint32_t *)(s->global + 0x56c) + (rng() % 64) - 32;
    if (rng() % 4 == 0) *(uint32_t *)(v + 0x64) = rng();
    *(uint32_t *)(v + 0x6c) = rng() % 4 ? *(uint32_t *)(s->global + 0x574) + rng() % 2000 - 1000
                                         : rng() % 30000;
    *(uint32_t *)(s->viewf + 8) = rng() % 2 ? 0x80000000u : rng() % 5;
    if (rng() % 5 == 0) {
        /* the eye straight north or south of the aircraft, level with it:
         * the range is then the latitude difference alone, a 16.16 value
         * the double sum cannot hold exactly -- under 64-bit precision the
         * square root of the stored sum and of the x87's differ by a unit */
        memcpy(v + 0x60, s->global + 0x568, 8);
        memcpy(v + 0x68, s->global + 0x570, 8);
        *(uint32_t *)(v + 0x5c) = *(uint32_t *)(s->global + 0x564) + rng() % 20000 - 10000;
        *(uint16_t *)(v + 0x5a) = (uint16_t)(rng() | 1);
        *(uint32_t *)(v + 0x10) = 0;
        *(uint32_t *)(s->viewf + 8) = 0x80000000u;
        s->level = 1;
        if (rng() % 2) {
            /* or straight above, 400 m or 4000 m: log10 of exactly 10 or 100
             * in the range attenuation, heard from outside */
            memcpy(v + 0x58, s->global + 0x560, 8);
            /* on the ground, the eye a few 1/65536ths short of 400 m up:
             * under 24-bit precision the range then comes out 400 m less
             * 2/65536, where the two ways of taking log10 land either side
             * of an integer.  Higher up, 24 bits cannot hold the fraction. */
            *(uint32_t *)(s->global + 0x574) = 0;
            *(uint16_t *)(s->global + 0x572) = 0x100;
            *(uint32_t *)(v + 0x6c) = rng() % 4 ? 400 : 4000;
            *(uint16_t *)(v + 0x6a) = (uint16_t)(0x100 - (rng() % 3 ? 2 : rng() % 4));
            *(uint32_t *)(v + 0x14) = 2 + rng() % 2;
            s->level = 2;
        }
    }
}

static void make_scenario(struct scenario *s)
{
    unsigned n, started = 0;
    memset(s, 0, sizeof *s);
    s->tree = rng() % NTREES;
    s->state = rng() % 12 == 0 ? (rng() % 2 ? 1 : 2) : 0;
    s->device = rng() % 3 != 0;
    s->acfail = rng() % 15 == 0 ? 7 + (int)(rng() % 2) : 0;
    s->precision = (int)(rng() % 3);
    s->viewsel = rng() % 7 != 0;
    s->viewfsel = rng() % 9 != 0;
    s->seed = rng();
    s->salt = rng();
    s->fseed = (double)(rng() % 1000000) / 1000000.0;
    make_cfg(s);
    make_global(s->global);
    if (rng() % 5 < 3) s->global[0x609] = (uint8_t)tree_kind[s->tree];
    make_view(s);
    if (s->level == 2) {
        /* the range attenuation reaches a prop's volume only from outside,
         * and only the C182 (tree 6) gives its outside sounds curves */
        unsigned k, t0 = rng() % 7;
        s->tree = 6;
        if (rng() % 3 == 0)
            for (k = 0; k < 7; k++)
                if (tree_kind[(t0 + k) % 7] == 0) { s->tree = (t0 + k) % 7; break; }
        s->global[0x609] = 0;
    }
    n = 6 + rng() % (MAXOPS - 6);
    /* start, and load the aircraft's sounds, as MAIN does */
    if (rng() % 10) {
        s->op[s->nops++].kind = OP_START;
        started = 1;
        if (rng() % 8) { s->op[s->nops].kind = OP_MSG; s->op[s->nops].a = 1; s->op[s->nops++].b = 0; }
    }
    while (s->nops < n) {
        struct op *o = &s->op[s->nops++];
        unsigned r = rng() % 100;
        o->a = rng();
        o->b = rng();
        o->c = rng();
        o->d = rng();
        if (r < 2) o->kind = OP_START;
        else if (r < 3) o->kind = started ? OP_STOP : OP_START;
        else if (r < 4) o->kind = OP_FRAME;
        else if (r < 30) o->kind = OP_FRAME;
        else if (r < 55) o->kind = OP_EVENT;
        else if (r < 61) o->kind = OP_WIND;
        else if (r < 63) o->kind = OP_SET_FADER;
        else if (r < 64) o->kind = OP_GET_FADER;
        else if (r < 66) o->kind = OP_SET_FADERS;
        else if (r < 67) o->kind = OP_GET_FADERS;
        else if (r < 69) o->kind = OP_SAVE;
        else if (r < 70) o->kind = OP_LOAD;
        else if (r < 71) o->kind = OP_MARKERS;
        else if (r < 76) o->kind = OP_MSG;
        else if (r < 79) o->kind = OP_LIGHT;
        else if (r < 84) o->kind = OP_TICK;
        else if (r < 90) o->kind = OP_END;
        else o->kind = OP_POKE;
        if (o->kind == OP_START) {
            started = 1;
            if (rng() % 4 && s->nops < MAXOPS) {
                s->op[s->nops].kind = OP_MSG;
                s->op[s->nops].a = 1;
                s->op[s->nops++].b = 0;     /* msgs[0], 0x60 */
            }
        }
    }
}

/* ---- running ---------------------------------------------------------------------- */

static unsigned long cover_wind_vol, cover_wind_rate, cover_engine, cover_engine_far;
static unsigned long cover_ops[OP_COUNT], cover_event[32], cover_fault, cover_sounds,
                     cover_kind[5];
static void *fault_jb[5];      /* __builtin_setjmp's */
static volatile int in_module;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    uint8_t *ip = (uint8_t *)e->ContextRecord->Eip;
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && in_module && cur
        && ip >= cur->lo && ip < cur->hi) {
        put(" FAULT(r%lu,%#lx)", e->ExceptionRecord->ExceptionInformation[0],
            e->ExceptionRecord->ExceptionInformation[1]);
        e->ContextRecord->Eip = (DWORD)fault_land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static unsigned fault_scenario;
static int fault_op = -1;
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    printf("\nFAULT %#lx at %p in %s, scenario %u, op %d\n", e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress, cur ? cur->name : "setup", fault_scenario, fault_op);
    printf("transcript tail: ...%s\n", tlen > 1500 ? tlog + tlen - 1500 : tlog);
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void set_precision(int p)
{
    static const uint16_t cw[3] = { 0x027f, 0x037f, 0x007f };
    uint16_t c = cw[p];
    __asm__ volatile("fldcw %0" : : "m"(c));
}

static void put_ret(int32_t r)
{
    put(" =");
    put_ptr((void *)r);
}

static void poke(const struct op *o)
{
    switch (o->a % 12) {
    case 0: G32(0xb24) = o->b % 12 ? 1 : o->b % 3; break;
    case 1: G8(0x609) = (uint8_t)(o->b % 9 ? o->b % 4 : o->b); break;
    case 2: G32(0xbe0) = o->b % 4; break;
    case 3: G32(0xbe4) = o->b % 4; break;
    case 4: G8(0xb6a) = o->b % 3 == 0; break;
    case 5: G8(0x36c) = o->b % 2; break;
    case 6: *(uint32_t *)(view_blob + 0x14) = o->b % 4 ? o->b % 3 : o->b % 6; break;
    case 7: G32(0x11a0) = o->b % 5 ? (uint32_t)view_blob : 0; break;
    case 8: *(int16_t *)(global_blob + 0x88c + 0x98 * (o->b % 4)) = (int16_t)(o->c % 3 ? (int)(o->c % 4000) - 1000 : (int)o->c); break;
    case 9: G16(0x898 + 0x98 * (o->b % 4)) = (uint16_t)(o->c % 3 ? o->c % 4000 : o->c % 20000);
            G16(0x8c8) = (uint16_t)(o->d % 4 ? 0x8000 + o->d % 0x8000 : o->d); break;
    case 10: G8(0x365) = o->b % 5 == 0; break;
    case 11: G16(0xaec) = (uint16_t)(o->b % 6); break;
    }
    put("\nPOKE %u", o->a % 12);
}

/* one frame of flight: the clock, the engines, the position */
static int level;
static void fly(const struct op *o)
{
    unsigned k;
    G16(0x2a8) = (uint16_t)(G16(0x2a8) + 1 + o->a % 3);
    if (o->b % 4 == 0)
        for (k = 0; k < 4; k++)
            G16(0x898 + 0x98 * k) = (uint16_t)(G16(0x898 + 0x98 * k) + (int)(o->c % 2001) - 1000);
    if (o->b % 8 == 1) {
        /* exactly on a curve point: a factor of 0x8000 halves the rpm, and
         * these are the x values the CD's rparams and vparams use */
        static const uint16_t xs[] = { 200, 450, 550, 750, 1000, 1100, 1300, 1800, 2000,
                                       2350, 2450, 3000, 4000 };
        static const uint16_t raw[] = { 398, 399, 400, 401 };   /* about 200: too slow */
        G16(0x8c8) = 0x8000;
        for (k = 0; k < 4; k++)
            G16(0x898 + 0x98 * k) = o->d % 3 ? (uint16_t)(2 * xs[(o->c >> (4 * k)) % 13])
                                             : raw[(o->c >> (2 * k)) % 4];
    }
    if (o->b % 7 == 0) G32(0xbe0) = o->d % 3;
    if (o->b % 11 == 0) G32(0xbe4) = o->d % 3;
    if (level == 2) return;     /* keep the eye straight above */
    G32(0x564) += (o->d % 64) - 32;
    if (level) return;          /* keep the eye due north or south */
    G32(0x56c) += (o->c % 64) - 32;
    G32(0x574) += (o->a % 50) - 25;
}

static void run(struct side *sd, const struct scenario *s, int count)
{
    volatile unsigned i;
    unsigned op_start;
    int32_t r;
    char p[MAX_PATH];
    FILE *f;

    cur = sd;
    counting = count;
    memcpy(sd->image + IMAGE_LO, sd->pristine + IMAGE_LO, IMAGE_HI - IMAGE_LO);
    memcpy(global_blob, s->global, GLOBAL_SIZE);
    memcpy(view_blob, s->view, sizeof view_blob);
    memcpy(viewf_blob, s->viewf, sizeof viewf_blob);
    G32(0x11a0) = s->viewsel ? (uint32_t)view_blob : 0;
    G32(0x119c) = s->viewfsel ? (uint32_t)viewf_blob : 0;
    G32(0x11d4) = (uint32_t)sim_link;
    memset(arena, 0, arena_used);
    arena_used = 0;
    memset(&pending_list, 0, sizeof pending_list);
    pending_list.grow = 0x20;
    pending_list.elem_size = 8;
    voices = 0;
    sound_state = s->state;
    device = s->device;
    voice_salt = s->salt;
    tree = s->tree;
    level = s->level;
    acontain_fail = s->acfail;
    airspeed = 0.0;
    *util_seed = s->seed;
    *util_fseed = s->fseed;
    f = fopen("FLTSIM98.CFG", "wb");
    fputs(s->cfg, f);
    fclose(f);
    tlen = 0;
    tlog[0] = 0;
    chain->init();
    set_precision(s->precision);

    put("\nINIT");
    sd->link->init();
    for (i = 0; i < s->nops; i++) {
        const struct op *o = &s->op[i];
        fault_op = o->kind;
        if (count) cover_ops[o->kind]++;
        in_module = 1;
        if (__builtin_setjmp(fault_jb)) {
            in_module = 1;
            if (count) cover_fault++;
            set_precision(s->precision);
            continue;
        }
        op_start = tlen;
        switch (o->kind) {
        case OP_START:
            put("\nSTART");
            r = sd->link->start();
            break;
        case OP_STOP:
            put("\nSTOP");
            r = sd->link->stop();
            break;
        case OP_FRAME:
            fly(o);
            put("\nFRAME");
            sd->link->frame();
            break;
        case OP_EVENT: {
            static const int32_t common[] = { 1, 3, 5, 7, 8, 9, 0xe, 0xf, 0x10, 0x11, 0x12,
                                              0x13, 0x14, 0x15, 0x17, 0x18, 0x19, 0x1a, 0x1b,
                                              0x1d, 0x17, 0xe, 0x13, 0x17 };
            int32_t code = o->a % 10 ? common[o->b % (sizeof common / sizeof common[0])]
                                     : (int32_t)(o->b % 34) - 2;
            uint32_t arg = o->c % 3 ? o->c % 2 : o->c;
            if (code == 0xe) {
                crash_ev[0] = o->c & 0x9d;
                if (o->d % 3) crash_ev[0] |= 0x11;
                if (o->d % 9 == 0) crash_ev[0] |= 0x80000000u;
                crash_ev[1] = o->d;
                crash_ev[2] = 0;
                crash_ev[3] = o->d % 2 ? (uint32_t)crash_cb : 0;
                crash_ev[4] = o->a % 3 == 0;
                arg = (uint32_t)crash_ev;
            } else if (code == 0x17) {
                static const char *const ids[] = { "ABC", "LAX", "ox", "1Z?", "", "SEA9",
                                                   "MMM", "abc", "KLAX" };
                ident[0] = o->c % 8 ? (int32_t)(o->c % 5) : (int32_t)(o->c % 9) - 2;
                *(int16_t *)&ident[1] = (int16_t)(o->d % 4 ? 1 + o->d % 100 : 0);
                snprintf(ident_text, sizeof ident_text, "%s", ids[o->a % 9]);
                ident[2] = o->d % 11 ? (int32_t)ident_text : 0;
                arg = (uint32_t)ident;
            } else if (code == 0x13) {
                arg = (uint32_t)((int32_t)(o->c % 3) - 1);
            } else if (code == 0x1d) {
                arg = o->c % 4 ? o->c % 0x14000000u : o->c;
            } else if (code == 0x1b && o->d % 4 == 0) {
                arg = 1;
            }
            if (count && code >= 0 && code < 32) cover_event[code]++;
            put("\nEVENT %d ", code);
            if (arg == (uint32_t)crash_ev) put("ev{%x,%x,%x,%x}", crash_ev[0], crash_ev[1],
                                               crash_ev[3] != 0, crash_ev[4]);
            else if (arg == (uint32_t)ident) put("id{%d,%d,%s}", ident[0], *(int16_t *)&ident[1],
                                                ident[2] ? ident_text : "null");
            else put("%#x", arg);
            r = sd->link->event(code, arg);
            put(" =");
            put_ptr((void *)r);
            break;
        }
        case OP_WIND:
            switch (o->a % 6) {
            case 0: break;      /* the same again */
            case 1: airspeed = 0.0; break;
            case 2: airspeed = -(double)(o->b % 100); break;
            default: airspeed = (double)(o->b % 400000) / 1000.0; break;
            }
            put("\nWIND");
            sd->link->wind(o->b, o->c, o->d);
            if (o->a % 4 == 3) {
                /* climbing through the curve: frame, wind, frame, wind */
                unsigned k;
                for (k = 0; k < 4; k++) {
                    airspeed = (double)((o->c >> (4 * k)) % 160);
                    put("\nFRAME");
                    sd->link->frame();
                    put("\nWIND");
                    sd->link->wind(o->b, o->c, o->d);
                }
            }
            break;
        case OP_SET_FADER:
            put("\nSETFADER %d %d", (int32_t)(o->a % 9) - 1, (int32_t)(o->b % 12000) - 1000);
            sd->link->set_fader((int32_t)(o->a % 9) - 1, (int32_t)(o->b % 12000) - 1000);
            break;
        case OP_GET_FADER:
            r = sd->link->get_fader((int32_t)(o->a % 9) - 1);
            put("\nGETFADER %d =%d", (int32_t)(o->a % 9) - 1, r);
            break;
        case OP_SET_FADERS: {
            unsigned k;
            for (k = 0; k < 6; k++) {
                fbuf[k] = (o->a >> k) & 1;
                uint32_t x = (o->b >> (3 * k)) * 2654435761u ^ o->c * (k + 1);
                fbuf[6 + k] = (int32_t)((o->d >> k) % 3 ? x % 10001 : x);
            }
            put("\nSETFADERS");
            r = sd->link->set_faders(fbuf);
            put_ret(r);
            break;
        }
        case OP_GET_FADERS: {
            unsigned k;
            for (k = 0; k < 6; k++) { fbuf[k] = (o->a >> k) & 1; fbuf[6 + k] = -1; }
            put("\nGETFADERS");
            r = sd->link->get_faders(fbuf);
            put_ret(r);
            for (k = 6; k < 12; k++) put(" %d", fbuf[k]);
            break;
        }
        case OP_SAVE: {
            unsigned k;
            for (k = 0; k < 5; k++)
                dbuf[k] = (o->a >> k) & 1 ? -(double)(o->b % 7) / 10.0 : (double)((o->b >> k) % 1500) / 1000.0;
            put("\nSAVE");
            r = sd->link->save_faders(dbuf);
            put_ret(r);
            break;
        }
        case OP_LOAD: {
            unsigned k;
            for (k = 0; k < 5; k++) dbuf[k] = -7.0;
            put("\nLOAD");
            r = sd->link->load_faders(dbuf);
            put_ret(r);
            for (k = 0; k < 5; k++) { put(" "); put_d(dbuf[k]); }
            break;
        }
        case OP_MARKERS:
            r = (int32_t)sd->link->markers();
            put("\nMARKERS =%#x", r);
            break;
        case OP_MSG: {
            static const uint32_t msgs[] = { 0x60, 0x60, 0x21, 0x40, 0x42, 0x44, 0x41, 0x22 };
            uint32_t m = o->a % 8 ? msgs[o->b % 8] : o->b % 0x70;
            put("\nMSG %#x", m);
            c_dispatch(0x14, m, o->c);
            break;
        }
        case OP_LIGHT:
            put("\nLIGHT");
            c_dispatch(0x12, o->a % 4, (uint32_t)((int32_t)(o->b % 9) - 2));
            break;
        case OP_TICK: {
            /* several frames: the idents repeat every 15 or 30 */
            unsigned k, n = o->a % 3 ? 1 + o->b % 4 : 10 + o->b % 25;
            put("\nTICK %u", n);
            for (k = 0; k < n; k++)
                c_dispatch(3, 0, 0);
            break;
        }
        case OP_END:
            voice_end(o->a);
            break;
        case OP_POKE:
            poke(o);
            break;
        }
        in_module = 0;
        if (count && o->kind == OP_WIND) {
            if (strstr(tlog + op_start, " dtoi(")) cover_wind_vol++;
            if (strstr(tlog + op_start, " vfreq(")) cover_wind_rate++;
        }
        if (count && o->kind == OP_FRAME && strstr(tlog + op_start, ",stk,fn")) {
            cover_engine++;
            if (level == 2 && s->precision == 2) cover_engine_far++;
        }
    }
    in_module = 0;
    if (count) {
        struct list *l = (struct list *)(sd->image + 0x1760);
        cover_sounds += (unsigned long)l->count;
        cover_kind[G8(0x609) < 4 ? G8(0x609) : 4]++;
    }
    put("\nDEINIT");
    sd->link->deinit();
    set_precision(0);
    f = fopen("FLTSIM98.CFG", "rb");
    put("\nCFG ");
    if (f) {
        size_t n = fread(p, 1, sizeof p - 1, f);
        p[n] = 0;
        fclose(f);
        put("%s", p);
    }
    put("\nEND");
}

/* ---- comparing -------------------------------------------------------------------- */

/* the pairing of retail's functions with ours, as they turn up */
static uint32_t pair_r[64], pair_o[64];
static unsigned npairs;

static int same_word(uint32_t r, uint32_t o, const uint8_t *rbase, const uint8_t *obase)
{
    unsigned k;
    int rd, od;
    if (r == o)         /* the two modules never overlap, so equal is equal */
        return 1;
    rd = (uint8_t *)r >= rbase && (uint8_t *)r < rbase + IMAGE_HI;
    od = (uint8_t *)o >= obase && (uint8_t *)o < obase + IMAGE_HI;
    if (rd || od)
        return rd && od && (uint8_t *)r - rbase == (uint8_t *)o - obase;
    if (is_code(&retail, r) || is_code(&ours, o)) {
        if (!is_code(&retail, r) || !is_code(&ours, o)) return 0;
        for (k = 0; k < npairs; k++)
            if (pair_r[k] == r || pair_o[k] == o) return pair_r[k] == r && pair_o[k] == o;
        if (npairs < 64) { pair_r[npairs] = r; pair_o[npairs] = o; npairs++; }
        return 1;
    }
    return r == o;
}

/* the first differing word, or -1 */
static long compare(const uint8_t *r, const uint8_t *o, unsigned lo, unsigned n,
                    const uint8_t *rbase, const uint8_t *obase)
{
    unsigned i;
    for (i = lo; i + 4 <= n; i += 4)
        if (!same_word(*(const uint32_t *)(r + i), *(const uint32_t *)(o + i), rbase, obase))
            return (long)i;
    for (; i < n; i++)
        if (r[i] != o[i]) return (long)i;
    return -1;
}

static void dump_words(const char *what, const uint8_t *r, const uint8_t *o, unsigned lo, unsigned n,
                       const uint8_t *rbase, const uint8_t *obase)
{
    unsigned i, shown = 0;
    for (i = lo; i + 4 <= n && shown < 10; i += 4)
        if (!same_word(*(const uint32_t *)(r + i), *(const uint32_t *)(o + i), rbase, obase)) {
            printf("  %s+%#x: retail %08x ours %08x\n", what, i, *(const uint32_t *)(r + i),
                   *(const uint32_t *)(o + i));
            shown++;
        }
}

/* ---- loading ---------------------------------------------------------------------- */

static uint8_t host_blob[0x320], global_dummy[4], acontain_blob[0x40];

static void bind_imports(struct import *im)
{
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host_blob; break;
        case 2:  im->link = global_blob; break;
        case 4:  im->link = chain_fwd; break;
        case 20: im->link = util_fwd; break;
        case 23: im->link = acontain_blob; break;
        default: printf("unexpected import %u\n", im->id); exit(2);
        }
    }
}

static int load_side(struct side *sd, const char *path, const char *name)
{
    struct import *imports;
    IMAGE_NT_HEADERS *nt;
    sd->name = name;
    sd->dll = LoadLibraryA(path);
    if (!sd->dll) { printf("cannot load %s (%lu)\n", path, GetLastError()); return 0; }
    imports = (struct import *)GetProcAddress(sd->dll, "ImportTable");
    sd->link = (struct snd_linkage *)GetProcAddress(sd->dll, "Linkage");
    if (!imports || !sd->link || sd->link->id != 39) { printf("%s: not SOUND\n", path); return 0; }
    sd->image = (uint8_t *)sd->link - 0x30;
    if ((uint8_t *)imports != sd->image) { printf("%s: ImportTable is not at Linkage-0x30\n", path); return 0; }
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)sd->dll + ((IMAGE_DOS_HEADER *)sd->dll)->e_lfanew);
    sd->lo = (uint8_t *)sd->dll;
    sd->hi = sd->lo + nt->OptionalHeader.SizeOfImage;
    bind_imports(imports);
    memcpy(sd->pristine, sd->image, IMAGE_HI);
    return 1;
}

static void *load_support(const char *path, uint32_t id)
{
    HMODULE m = LoadLibraryA(path);
    void *l;
    if (!m) { printf("cannot load %s\n", path); exit(2); }
    l = (void *)GetProcAddress(m, "Linkage");
    if (!l || *(uint32_t *)l != id) { printf("%s: wrong module\n", path); exit(2); }
    return l;
}

#define SET(blob, off, fn) (*(void **)((blob) + (off)) = (void *)(fn))

int main(int argc, char **argv)
{
    static char rlog[sizeof tlog];
    static uint8_t rimage[IMAGE_HI], rglobal[GLOBAL_SIZE], rarena[ARENA_SIZE];
    static struct scenario s;
    unsigned v, n = argc > 1 ? (unsigned)atoi(argv[1]) : 20000, bad = 0, k;
    struct import *im;
    uint32_t rused;
    long at;

    (void)global_dummy;
    SetUnhandledExceptionFilter(on_fault);
    AddVectoredExceptionHandler(1, on_vectored);
    load_ext_table("FLTSIM98_host.exe");
    fll = LoadLibraryExA("ENGLISH.FLL", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!fll) { printf("cannot load ENGLISH.FLL\n"); return 2; }
    if (LoadStringA(fll, 0x9005, sounddir, sizeof sounddir) <= 0) { printf("no string 0x9005\n"); return 2; }

    *(uint32_t *)host_blob = 1;
    SET(host_blob, 0x050, h_busy);
    SET(host_blob, 0x0b8, h_cfg_double);
    SET(host_blob, 0x0bc, h_cfg_put_double);
    SET(host_blob, 0x0d0, h_prof_int);
    SET(host_blob, 0x0e0, h_prof_double);
    SET(host_blob, 0x0e8, h_prof_doubles);
    SET(host_blob, 0x0f0, h_prof_string);
    SET(host_blob, 0x158, h_exists);
    SET(host_blob, 0x184, h_join);
    SET(host_blob, 0x188, h_split);
    SET(host_blob, 0x20c, h_alloc);
    SET(host_blob, 0x214, h_free);
    SET(host_blob, 0x218, h_memset);
    SET(host_blob, 0x21c, h_memmove);
    SET(host_blob, 0x2a0, h_loadstring);
    SET(host_blob, 0x2a8, h_strcpy);
    SET(host_blob, 0x2b4, h_strdup);
    SET(host_blob, 0x2bc, h_strcmp);
    SET(host_blob, 0x2cc, h_sound_off);
    SET(host_blob, 0x2d0, h_sound_enable);
    SET(host_blob, 0x2d4, h_voice_load);
    SET(host_blob, 0x2d8, h_voice_append);
    SET(host_blob, 0x2dc, h_voice_free);
    SET(host_blob, 0x2e0, h_voice_play);
    SET(host_blob, 0x2e4, h_voice_freq);
    SET(host_blob, 0x2ec, h_voice_volume);
    SET(host_blob, 0x2f0, h_voice_pan);
    SET(host_blob, 0x2f4, h_voice_stop);
    SET(host_blob, 0x2f8, h_list_add);
    SET(host_blob, 0x2fc, h_list_free);
    SET(host_blob, 0x300, h_list_remove);
    SET(host_blob, 0x308, h_list_walk);
    SET(host_blob, 0x310, h_list_find);
    *(uint32_t *)acontain_blob = 23;
    SET(acontain_blob, 0x30, ac_query);
    *(uint32_t *)sim_link = 25;
    SET(sim_link, 0x38, sim_value);

    chain = (struct chain_linkage *)load_support("CHAIN_retail.dll", 4);
    for (im = (struct import *)GetProcAddress(GetModuleHandleA("CHAIN_retail.dll"), "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : (void *)global_blob;
    *(uint32_t *)chain_fwd = 4;
    SET(chain_fwd, 0x18, c_add);
    SET(chain_fwd, 0x1c, c_remove);
    SET(chain_fwd, 0x20, c_dispatch);
    util_link = (uint8_t *)load_support("UTIL_retail.dll", 20);
    im = (struct import *)GetProcAddress(GetModuleHandleA("UTIL_retail.dll"), "ImportTable");
    util_seed = (uint32_t *)((uint8_t *)im + 0xf0);
    /* frand's state: the last eight bytes of UTIL's .data, 0x208e57f8 at
     * its preferred base */
    util_fseed = (double *)((uint8_t *)GetModuleHandleA("UTIL_retail.dll") + 0x57f8);
    for (; im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : (void *)global_blob;
    *(uint32_t *)util_fwd = 20;
    SET(util_fwd, 0x48, u_to_m);
    SET(util_fwd, 0x70, u_rand32);
    SET(util_fwd, 0xb4, u_dtoi);
    SET(util_fwd, 0xb8, u_frand);

    if (!load_side(&retail, "SOUND_retail.dll", "retail")) return 2;
    if (!load_side(&ours, "SOUND_ours.dll", "openfs98")) return 2;
    if ((at = compare(retail.pristine, ours.pristine, IMAGE_LO, IMAGE_HI, retail.image, ours.image)) >= 0) {
        printf("initial images differ at +%#lx\n", at);
        dump_words("img", retail.pristine, ours.pristine, IMAGE_LO, IMAGE_HI, retail.image, ours.image);
        bad++;
    }

    deltree("sfix");
    CreateDirectoryA("sfix", NULL);
    for (k = 0; k < NTREES; k++)
        write_tree(k);
    deltree(sounddir);
    CreateDirectoryA(sounddir, NULL);
    for (k = 0; k < NWAVS; k++)
        if (k % 4) touch(sounddir, wavs[k]);

    for (v = 0; v < n; v++) {
        make_scenario(&s);
        if (s.tree >= 7)
            write_tree(s.tree);     /* a fresh generated tree each time */
        fault_scenario = v;
        run(&retail, &s, 1);
        memcpy(rlog, tlog, tlen + 1);
        memcpy(rimage, retail.image, IMAGE_HI);
        memcpy(rglobal, global_blob, GLOBAL_SIZE);
        rused = arena_used;
        memcpy(rarena, arena, rused);
        run(&ours, &s, 0);
        if (strcmp(rlog, tlog)
            || compare(rimage, ours.image, IMAGE_LO, IMAGE_HI, retail.image, ours.image) >= 0
            || memcmp(rglobal, global_blob, GLOBAL_SIZE)
            || rused != arena_used || compare(rarena, arena, 0, rused, retail.image, ours.image) >= 0) {
            if (bad++ < 4) {
                unsigned a = 0;
                while (rlog[a] && rlog[a] == tlog[a]) a++;
                printf("scenario %u (tree %u) differs", v, s.tree);
                if (rlog[a] || tlog[a])
                    printf(" in the transcript at byte %u:\n  retail ...%.600s\n  ours   ...%.600s\n",
                           a, rlog + (a > 400 ? a - 400 : 0), tlog + (a > 400 ? a - 400 : 0));
                else
                    printf(":\n");
                dump_words("img", rimage, ours.image, IMAGE_LO, IMAGE_HI, retail.image, ours.image);
                for (k = 0; k < GLOBAL_SIZE && rglobal[k] == global_blob[k]; k++) ;
                if (k < GLOBAL_SIZE) printf("  global+%#x: retail %02x ours %02x\n", k, rglobal[k], global_blob[k]);
                if (rused != arena_used) printf("  arena used: retail %#x ours %#x\n", rused, arena_used);
                dump_words("arena", rarena, arena, 0, rused < arena_used ? rused : arena_used,
                           retail.image, ours.image);
            }
            if (getenv("SNDSTOP")) break;       /* for deliberate breaks: the first is enough */
        }
        if (getenv("SNDDUMP") && v == (unsigned)atoi(getenv("SNDDUMP"))) printf("%s\n", rlog);
    }

    printf("ops:");
    for (k = 0; k < OP_COUNT; k++) printf(" %s %lu", op_names[k], cover_ops[k]);
    printf("\nevents:");
    for (k = 0; k < 32; k++) if (cover_event[k]) printf(" %x:%lu", k, cover_event[k]);
    printf("\nsounds built %lu; view kinds 0-3,other %lu %lu %lu %lu %lu\n", cover_sounds,
           cover_kind[0], cover_kind[1], cover_kind[2], cover_kind[3], cover_kind[4]);
    printf("voice loads missing/ok/kept-or-failed/null %lu/%lu/%lu/%lu; plays now/ok/failed %lu/%lu/%lu\n",
           cover_load[0], cover_load[1], cover_load[2], cover_load[3],
           cover_play[0], cover_play[1], cover_play[2]);
    printf("appends %lu, finished %lu; callbacks pending/now %lu/%lu, crash callbacks %lu; bad handles %lu\n",
           cover_append[0], cover_append[1], cover_cb[0], cover_cb[1], cover_crash_cb, cover_badh);
    printf("wind computed volume %lu, retuned %lu; frames with engine walks %lu, %lu of them\n"
           "  at 400 or 4000 m under 24-bit precision\n",
           cover_wind_vol, cover_wind_rate, cover_engine, cover_engine_far);
    printf("freq %lu, volume %lu, pan %lu, stop %lu\n", cover_freq, cover_vol, cover_pan, cover_stop);
    printf("cfg hits %lu, profile hits %lu, exists no/yes %lu/%lu, faults %lu\n", cover_cfg_hit,
           cover_prof_hit, cover_exists[0], cover_exists[1], cover_fault);
    printf("%u scenarios, %u differ\n", n, bad);
    if (!bad) printf("IDENTICAL\n");
    return bad != 0;
}
