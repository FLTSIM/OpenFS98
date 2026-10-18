/* Differential test: retail ADVDRV.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven through Linkage and the
 * CHAIN callbacks they register, the way MAIN and the menus drive them, one
 * side after the other over fixture trees rebuilt the same way for each.
 * What ADVDRV imports:
 *
 *   host      a stub.  Memory is a bump arena reset before each side, so heap
 *             pointers come out the same; what the host does not zero is
 *             filled with 0xa5.  Files and listings pass through to real
 *             ADV\, LESSONS\, CHALLENG\, SITUATIO\, PILOTS\ and VIDEOS\
 *             trees, and a CD tree for the sound cache, behind
 *             transliterations of the host's path, find and file services and
 *             its extension table (read out of FLTSIM98.EXE).  String
 *             resources are ENGLISH.FLL's.  The clock is the scenario's.  The
 *             dialog engine is a stand-in for the two pickers.  The voice
 *             engine plays nothing, and calls back when the harness says a
 *             sound has finished.  Key maps are logged.
 *   KERNEL32  the debug console is left closed: synthetic programs never
 *             open it, so error messages go to a null handle and fail on both
 *             sides alike, uncompared
 *   UTIL      retail's: the catalogue (+0x98) real over the trees, the
 *             modeless alerts real over the stub host; the modal alert logged
 *             and answered; the conversions and the random number generator
 *             logged around retail's.  Its .data is put back before each side.
 *   CHAIN     an emulation of the real one -- duplicates allowed, removal
 *             during a dispatch deferred, an add during one visited by it --
 *             whose lists the harness dispatches itself
 *   MAIN      +0x3c (load a situation), +0x70 (range and bearing, a function
 *             of the bytes retail defines), +0x74, +0x80 (the DME), +0xa4
 *             logged
 *   DEMO, SOUND, WINMSG   logged; WINMSG hands out message numbers
 *   GLOBAL    random, per scenario, with +0x384 aimed at three navaid
 *             records and +0x11d4 at a stub module, and steered as the
 *             programs run
 *
 * Programs: the CD's twenty adventures and twenty-four lessons, and
 * synthetic ones written for every scenario by an assembler here: every
 * instruction, operator, text piece and system variable (read and written),
 * loops, calls, switches, events of all three kinds, key handlers, weather
 * and the error paths -- the operand stack's overflow and underflow, the call
 * stack's, bad operators and instructions.  Some are in the old encrypted
 * format, some carry a string table, some have bad headers or CRCs.
 *
 * Compared after every operation: the transcript of every import call, the
 * whole .data image (code pointers through a pairing learnt in order of first
 * sight, pointers into the image as offsets), the arena and GLOBAL, and at
 * each scenario's end the sound cache's files.
 *
 * Faults in either module are outcomes: the op is abandoned (the x87 reset)
 * and the next one runs.  An op past a fixed count of host calls is cut at
 * that count on both sides.  A watchdog reports a spin with no host calls.
 *
 * Kept clear of, as no real program does:
 *   - a variable reference that is neither tag 7 nor 8: retail then uses a
 *     reference on the stack it never filled;
 *   - text runs past the 0x400-byte buffer to the end of the image, which
 *     writes beyond .data differently in the two builds.
 *
 * Build: make advdrvdiff     Run: make advdrvdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x32a0u
#define GLOBAL_SIZE  0x1300u
#define ARENA_SIZE   (16u << 20)
#define TRANSCRIPT_SIZE (24u << 20)
#define MAX_OPS      60
#define MAX_PROGRAMS 6

/* the image's relocated slots, from the generator's list */
static const uint16_t code_slots[] = {
#define ADVDRV_CODE(offset, retail) offset,
#define ADVDRV_DATA(offset, target)
#define ADVDRV_RELOCS
#include "../../src/advdrv/advdrv_data.inc"
#undef ADVDRV_RELOCS
#undef ADVDRV_CODE
#undef ADVDRV_DATA
};
static const uint16_t data_slots[] = {
#define ADVDRV_CODE(offset, retail)
#define ADVDRV_DATA(offset, target) offset,
#define ADVDRV_RELOCS
#include "../../src/advdrv/advdrv_data.inc"
#undef ADVDRV_RELOCS
#undef ADVDRV_CODE
#undef ADVDRV_DATA
};

struct import { uint32_t id; void *link; };

struct linkage {
    uint32_t id;
    uint32_t (*init)(void);
    uint32_t (*deinit)(void);
    uint32_t reserved[3];
    uint32_t (STDCALL *running)(uint32_t *);                 /* +0x18 */
    uint32_t (STDCALL *load)(const char *, uint32_t);        /* +0x1c */
    uint32_t (*restart)(void);                               /* +0x20 */
    uint32_t (*stop)(void);                                  /* +0x24 */
    uint32_t (*adventures)(void);                            /* +0x28 */
    uint32_t (*lessons)(void);                               /* +0x2c */
    uint32_t (STDCALL *set_skip)(uint32_t);                  /* +0x30 */
};

typedef uint32_t (STDCALL *callback_fn)(uint32_t, uint32_t);

/* CHAIN, as the real one keeps a list */
struct chain_list {
    callback_fn cb[2048];
    uint8_t removed[2048];
    uint32_t count, depth, pending;
};

#define MAX_PAIRS 512

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    struct linkage *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    struct chain_list chain[0x24];
    char *transcript;
    uint32_t length;
    uint32_t pairs[MAX_PAIRS], npairs;
};

static struct side retail, ours;
static struct side *cur;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t chain[0x24], mainmod[0xb0], demo[0x30], sound[0x34], winmsg[0x24],
    util_proxy[0xc0], module_11d4[0x30];
static uint8_t nav_records[3][0x60];
static uint32_t nav_table[3];
static uint8_t *util_link, *util_data, util_data_pristine[0x4000];
static uint32_t util_data_size;
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static char cd_root[MAX_PATH];
static uint32_t rng;
static uint32_t op_number, scenario_number;
static const char *volatile fault_side = "none";
static volatile uint32_t fault_op;
static int counting;
static uint32_t op_calls;
static HANDLE main_thread;
static volatile DWORD op_started;
static volatile int in_module;
static int op_faulted;
static int poisoned;  /* op_error fired: the code is freed, continuation undefined */
static void cut_op(void);

/* the scenario: the clock, the answers, and what the stubs hand out */
static uint32_t clock_ms, msg_counter, voice_counter, dme_answer, sound_on,
    max_events, cache_wav, keymap_counter, value_11d4;
static uint32_t script[64], script_at;
static uint32_t precision_start, precision;

/* voices whose "finished" callback the harness owes */
struct voice_done { uint32_t h; void (STDCALL *done)(uint32_t); };
static struct voice_done pending[16];
static uint32_t npending;

static unsigned long cover_ops[16], cover_frames, cover_loads, cover_loaded,
    cover_main, cover_msg, cover_sounds, cover_alerts, cover_dialog[4],
    cover_console, cover_events, cover_keys, cover_faults, cover_hangs,
    cover_copies, cover_deletes, cover_videos, cover_commands, cover_errors;

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

static uint32_t next_answer(void)
{
    return script_at < 64 ? script[script_at++] : 0;
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

    if (cur == NULL || poisoned || cur->length >= TRANSCRIPT_SIZE - 1024)
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
    const uint8_t *b = (const uint8_t *)s;

    if (s == NULL)
        put("null");
    else if (!((b >= arena && b < arena + ARENA_SIZE) ||
               (cur && b >= cur->lo && b < cur->hi) || (uintptr_t)b > 0x10000))
        put("bad");
    else
        put("'%.120s'", s);
}

static int in_image(uint32_t v)
{
    return cur != NULL && v >= (uint32_t)(uintptr_t)cur->image &&
           v < (uint32_t)(uintptr_t)cur->image + IMAGE_SIZE;
}

/* A code pointer, as its number in the order code pointers were first seen
 * on that side: equal numbers are the paired routines.  Learnt once and held
 * to across scenarios. */
static uint32_t pair_code(uint32_t v)
{
    uint32_t i;

    if (v == 0)
        return 0;
    for (i = 0; i < cur->npairs; i++)
        if (cur->pairs[i] == v)
            return 0xc0de0000u | (i + 1);
    if (cur->npairs < MAX_PAIRS) {
        cur->pairs[cur->npairs] = v;
        return 0xc0de0000u | ++cur->npairs;
    }
    return 0xc0deffffu;
}

static void put_ptr(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;

    if (v == 0)
        put("null");
    else if (b >= arena && b < arena + ARENA_SIZE)
        put("arena+%#x", (uint32_t)(b - arena));
    else if (in_image(v))
        put("image+%#x", v - (uint32_t)(uintptr_t)cur->image);
    else if (b >= global && b < global + GLOBAL_SIZE)
        put("G+%#x", (uint32_t)(b - global));
    else if (cur && b >= cur->lo && b < cur->hi)
        put("code%u", pair_code(v) & 0xffff);
    else if (v < 0x10000)
        put("%#x", v);
    else
        put("other");
}

/* ---- the host's extension table ------------------------------------------------ */

static char ext_text[0x40][8];
static uint32_t ext_count;

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
    if (strcmp(ext_text[0x1a], ".ADV") != 0 || strcmp(ext_text[0x20], ".LES") != 0 ||
        strcmp(ext_text[0x13], ".WAV") != 0) {
        printf("extension table: unexpected entries\n");
        exit(2);
    }
    ext_count = 0x25;
}

static int resolve(const char *path, int32_t ext, char *out, size_t len)
{
    const char *t;

    if (ext == -2) t = "";
    else if (ext >= 0 && (uint32_t)ext < ext_count) t = ext_text[ext];
    else return -1;
    snprintf(out, len, "%s%s", path, t);
    return 0;
}

static int ext_lookup(char *ext, int32_t *out)
{
    uint32_t k;

    _strupr(ext);
    for (k = 1; k < ext_count; k++)
        if (strcmp(ext, ext_text[k]) == 0) { *out = (int32_t)k; return 0; }
    return 1;
}

/* ---- the stub host -------------------------------------------------------------- */

static void count_call(void)
{
    if (++op_calls > 200000)
        cut_op();
}

static void *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;

    if (size > ARENA_SIZE || arena_used + n > ARENA_SIZE) {
        printf("arena exhausted (scenario %u)\n", scenario_number);
        exit(2);
    }
    p = arena + arena_used;
    arena_used += n;
    *(uint32_t *)p = size;
    return p + 16;
}

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p;

    count_call();
    if (size > 0x400000) {
        put(" alloc(%d,%#x)=-1", fl, size);
        return -1;
    }
    p = arena_get(size);
    memset(p, zero ? 0 : 0xa5, size);
    put(" alloc(%d,%#x,%d)=", fl, size, zero);
    put_ptr((uint32_t)(uintptr_t)p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    uint8_t *old = *pp, *p;
    uint32_t n;

    count_call();
    if (size > 0x400000) {
        put(" realloc(%d,%#x)=-1", fl, size);
        return -1;
    }
    p = arena_get(size);
    memset(p, 0xa5, size);
    if (old) {
        n = *(uint32_t *)(old - 16);
        memcpy(p, old, n < size ? n : size);
    }
    put(" realloc(%d,%#x)=", fl, size);
    put_ptr((uint32_t)(uintptr_t)p);
    *pp = p;
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    count_call();
    put(" free(");
    put_ptr((uint32_t)(uintptr_t)*pp);
    put(")");
    *pp = NULL;
    return 0;
}

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    count_call();
    memmove(d, s, n);
    put(" memmove(%#x)", n);
    return 0;
}

static int32_t STDCALL h_memset(void *d, int32_t c, uint32_t n)
{
    count_call();
    memset(d, c, n);
    put(" memset(%#x,%#x)", c & 0xff, n);
    return 0;
}

/* ENGLISH.FLL's strings for the ids ADVDRV and UTIL's alerts load */
static const struct { uint32_t id; const char *text; } strings[] = {
    {0x2330, "APL debug window"},
    {0x2331, "Subroutine stack overflow"},
    {0x2332, "Subroutine stack underflow"},
    {0x2333, "Illegal value for PRECISION"},
    {0x2334, "Illegal value for PRINT_TIME"},
    {0x2335, "Illegal time value for WAIT"},
    {0x2336, "No slot available for ONKEY"},
    {0x2337, "Illegal value assigned to variable"},
    {0x2338, "Infinite loop suspected. Ending Adventure."},
    {0x9002, "PILOTS"}, {0x9004, "SITUATIO"}, {0x9007, "VIDEOS"},
    {0x9009, "ADV"}, {0x900a, "CHALLENG"}, {0x900b, "LESSONS"},
    {0x900c, "WAV"}, {0x900d, "ADV\\WAV"}, {0x900e, "ADV\\WAV\\CACHE"},
    {0x900f, "VIDEOS"},
    {0xf430, "There is not enough memory available to run an adventure."},
    {0xf431, "The adventure title is too long."},
    {0xf432, "Flight Simulator cannot open the adventure:"},
    {0xf433, "Flight Simulator cannot read the adventure:"},
    {0xf434, "The selected adventure is not a Flight Simulator 95 file."},
    {0xf435, "The selected adventure appears to be corrupted."},
    {0xf436, "Loading"}, {0xf437, "Ending"},
    {0xf438, "Could not find the required flight for the adventure."},
    {0xf439, "The running adventure is corrupted."},
    {0xf43a, "An adventure registered too many events."},
};

static int32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    char t[96];
    uint32_t i;

    count_call();
    snprintf(t, sizeof t, "string %#x", id);
    for (i = 0; i < sizeof strings / sizeof strings[0]; i++)
        if (strings[i].id == id)
            snprintf(t, sizeof t, "%s", strings[i].text);
    if (len > 0) {
        strncpy(buf, t, (size_t)len - 1);
        buf[len - 1] = 0;
    }
    put(" ls(%#x)", id);
    return (int32_t)strlen(buf);
}

static char *STDCALL h_strcpy(char *d, const char *s)
{
    count_call();
    strcpy(d, s);
    put(" strcpy=");
    put_s(d);
    return d;
}

static char *STDCALL h_strcat(char *d, const char *s)
{
    count_call();
    strcat(d, s);
    put(" strcat=");
    put_s(d);
    return d;
}

static int32_t STDCALL h_strcmp(const char *a, const char *b)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    int32_t r;

    count_call();
    for (;;) {
        if (x[0] != y[0]) { r = x[0] < y[0] ? -1 : 1; break; }
        if (!x[0]) { r = 0; break; }
        if (x[1] != y[1]) { r = x[1] < y[1] ? -1 : 1; break; }
        if (!x[1]) { r = 0; break; }
        x += 2;
        y += 2;
    }
    put(" cmp=%d", r);
    return r;
}

static int32_t STDCALL h_stricmp(const char *a, const char *b)
{
    int32_t r = _strcmpi(a, b);
    count_call();
    return r < 0 ? -1 : r > 0 ? 1 : 0;
}

static uint32_t STDCALL h_strlen(const char *s)
{
    count_call();
    return s ? (uint32_t)strlen(s) : 0;
}

/* 0x40a510 */
static int32_t STDCALL h_join(char *out, const char *drive, const char *dir, const char *name)
{
    char b[0x208];
    size_t n;

    count_call();
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
        strncat(b, dir, 0x104);
        n = strlen(b);
        if (_strcmpi(b + n - 1, "\\") == 0) b[n - 1] = 0;
    }
    if (name && strlen(name)) {
        if (name[0] != '\\' && strlen(b)) strcat(b, "\\");
        strncat(b, name, 0x104);
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

/* 0x40a6c0: an unknown extension is registered, as the host does */
static int32_t STDCALL h_split(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{
    char d[0x100], di[0x100], nm[0x100], e[0x100];

    count_call();
    if (!path) return -1;
    if (path[0] == 0) {
        if (drive) drive[0] = 0;
        if (dir) dir[0] = 0;
        if (name) name[0] = 0;
        if (ext) *ext = 0;
        return 0;
    }
    _splitpath(path, d, di, nm, e);
    if (drive) strcpy(drive, d);
    if (dir) strcpy(dir, di);
    if (name) strcpy(name, nm);
    if (ext) {
        if (e[0] == 0)
            *ext = 0;
        else if (ext_lookup(e, ext) != 0 && ext_count < 0x40) {
            strncpy(ext_text[ext_count], e, 7);
            *ext = (int32_t)ext_count++;
        }
    }
    put(" split=");
    put_s(name ? name : "");
    return 0;
}

struct find { HANDLE h; WIN32_FIND_DATAA d; int more, open; };
struct find_entry { char name[0x104]; int32_t type; uint32_t size; int32_t writable; };
static struct find finds[8];

static int32_t STDCALL h_find_first(struct find **out, const char *dir, const char *pat, int32_t ext)
{
    char path[MAX_PATH * 2];
    struct find *f = NULL;
    size_t n;
    int i;

    count_call();
    path[0] = 0;
    if (dir) {
        strcpy(path, dir);
        n = strlen(path);
        if (n == 0 || path[n - 1] != '\\') strcat(path, "\\");
    }
    strcat(path, pat ? pat : "*");
    if (ext == -1) strcat(path, ".*");
    else if (ext != -2) strcat(path, ext_text[ext]);
    for (i = 0; i < 8; i++)
        if (!finds[i].open) { f = &finds[i]; break; }
    if (f == NULL) { printf("out of finds\n"); exit(2); }
    f->h = FindFirstFileA(path, &f->d);
    put(" findfirst(");
    put_s(path);
    if (f->h == INVALID_HANDLE_VALUE) {
        put(")=-1");
        return -1;
    }
    f->open = 1;
    f->more = 1;
    *out = f;
    put(")");
    return 0;
}

static int32_t STDCALL h_find_next(struct find *f, struct find_entry *e)
{
    int got;
    char x[0x100];

    count_call();
    for (;;) {
        if (!f->more) { put(" findnext=-1"); return -1; }
        got = 0;
        if (f->d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(f->d.cFileName, ".") && strcmp(f->d.cFileName, "..")) {
                strcpy(e->name, f->d.cFileName);
                e->type = -2;
                e->size = 0;
                got = 1;
            }
        } else {
            _splitpath(f->d.cFileName, NULL, NULL, e->name, x);
            ext_lookup(x, &e->type);
            e->size = f->d.nFileSizeLow;
            e->writable = f->d.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? 0 : 1;
            got = 1;
        }
        f->more = FindNextFileA(f->h, &f->d) ? 1 : 0;
        if (got) {
            put(" findnext=");
            put_s(e->name);
            put("/%d", e->type);
            return 0;
        }
    }
}

static int32_t STDCALL h_find_close(struct find **pf)
{
    count_call();
    if (*pf == NULL)
        return 0;
    FindClose((*pf)->h);
    (*pf)->open = 0;
    *pf = NULL;
    put(" findclose");
    return 0;
}

struct host_list {
    uint32_t count, capacity, grow, elem_size, iterating, pending;
    uint8_t *data;
};

static void *STDCALL h_list_add(struct host_list *list)
{
    uint8_t *element;

    count_call();
    if (list->count == list->capacity) {
        uint32_t capacity = list->capacity + list->grow;
        uint32_t size = capacity * list->elem_size;
        uint8_t *data = (uint8_t *)arena_get(size);

        memset(data, 0xa5, size);
        if (list->data != NULL)
            memcpy(data, list->data, list->count * list->elem_size);
        list->data = data;
        list->capacity = capacity;
    }
    element = list->data + list->count * list->elem_size;
    memset(element, 0, list->elem_size);
    list->count++;
    put(" listadd");
    return element;
}

static void STDCALL h_list_remove(struct host_list *list, void *element)
{
    uint32_t index = (uint32_t)((uint8_t *)element - list->data) / list->elem_size;

    count_call();
    put(" listremove(%u)", index);
    if (index >= list->count)
        return;
    if (index + 1 < list->count)
        memmove(list->data + index * list->elem_size,
                list->data + (index + 1) * list->elem_size,
                (list->count - index - 1) * list->elem_size);
    list->count--;
}

typedef int32_t (STDCALL *cmp_fn)(const void *, const void *);
static cmp_fn qsort_cmp;
static int __cdecl qsort_tramp(const void *a, const void *b) { return qsort_cmp(a, b); }

static void STDCALL h_qsort(void *base, uint32_t n, uint32_t w, cmp_fn cmp)
{
    count_call();
    put(" qsort(%u,%#x)", n, w);
    qsort_cmp = cmp;
    qsort(base, n, w, qsort_tramp);
}

struct file { HANDLE h; int open; };
static struct file files[12];

static struct file *new_file(void)
{
    int i;

    for (i = 0; i < 12; i++)
        if (!files[i].open)
            return &files[i];
    printf("out of files\n");
    exit(2);
}

static void put_file(struct file *f)
{
    if (f == NULL)
        put("null");
    else if (f >= files && f < files + 12)
        put("file%d", (int)(f - files));
    else
        put("bad");
}

static int valid_file(struct file *f)
{
    return f != NULL && f >= files && f < files + 12 && f->open;
}

static int32_t STDCALL h_open(struct file **out, const char *path, int32_t ext, int32_t mode)
{
    static const DWORD access[4] = { GENERIC_READ, GENERIC_READ | GENERIC_WRITE,
                                     GENERIC_WRITE, GENERIC_WRITE };
    char full[0x200];
    struct file *f;

    count_call();
    put(" open(");
    if (resolve(path, ext, full, sizeof full)) { *out = NULL; put("ext %d)=-1", ext); return -1; }
    put_s(full);
    if (mode < 0 || mode > 3) { *out = NULL; put(",%d)=-1", mode); return -1; }
    f = new_file();
    f->h = CreateFileA(full, access[mode], 0, NULL, OPEN_EXISTING, 0x80, NULL);
    if (f->h == INVALID_HANDLE_VALUE) {
        *out = NULL;
        put(",%d)=-1", mode);
        return -1;
    }
    if (mode == 3) SetFilePointer(f->h, 0, NULL, FILE_END);
    f->open = 1;
    *out = f;
    put(",%d)=", mode);
    put_file(f);
    return 0;
}

static int32_t STDCALL h_close(struct file *f)
{
    count_call();
    put(" close(");
    put_file(f);
    put(")");
    if (!valid_file(f)) return -1;
    CloseHandle(f->h);
    f->open = 0;
    return 0;
}

static int32_t STDCALL h_read(struct file *f, void *buf, uint32_t n, uint32_t *got)
{
    DWORD k = 0;
    BOOL ok;

    count_call();
    if (got) *got = 0;
    if (!valid_file(f)) { put(" read(bad)"); return -1; }
    ok = ReadFile(f->h, buf, n, &k, NULL);
    if (got) *got = k;
    put(" read(%#x)=%lu/%08x", n, (unsigned long)k, digest(buf, k, 0x811c9dc5u));
    return ok ? 0 : -1;
}

/* +0x1a4 */
static int32_t STDCALL h_size(struct file *f, uint32_t *size)
{
    count_call();
    if (f == NULL) { put(" size(null)=-1"); return -1; }
    if (!valid_file(f)) { put(" size(bad)"); *size = 0; return 0; }
    *size = GetFileSize(f->h, NULL);
    put(" size=%#x", *size);
    return 0;
}

static int32_t STDCALL h_exists(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;

    count_call();
    if (resolve(path, ext, full, sizeof full)) return -1;
    r = GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES ? -1 : 0;
    put(" exists(");
    put_s(full);
    put(")=%d", r);
    return r;
}

/* +0x138: the attribute flags as the host translates them */
static int32_t STDCALL h_set_attr(int32_t flags, const char *path, int32_t ext)
{
    char full[0x200];
    DWORD a = 0;
    int32_t r;

    count_call();
    if (resolve(path, ext, full, sizeof full)) return -1;
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
        put(" setattr(");
        put_s(full);
        put(")=-1");
        return -1;
    }
    if (flags & 1) a = 0x80;
    if (flags & 4) a |= 0x20;
    if (flags & 2) a |= 1;
    if (flags & 8) a |= 2;
    if (flags & 0x10) a |= 0x10;
    r = SetFileAttributesA(full, a) ? 0 : -1;
    put(" setattr(%#x,", flags);
    put_s(full);
    put(")=%d", r);
    return r;
}

/* +0x144: CopyFileA(from, to, fail if it exists) */
static int32_t STDCALL h_copy(const char *to, int32_t toext, const char *from, int32_t fromext)
{
    char a[0x200], b[0x200];
    int32_t r;

    count_call();
    if (resolve(to, toext, a, sizeof a) || resolve(from, fromext, b, sizeof b)) return -1;
    r = CopyFileA(b, a, TRUE) ? 0 : -1;
    put(" copy(");
    put_s(b);
    put(",");
    put_s(a);
    put(")=%d", r);
    if (counting && r == 0) cover_copies++;
    return r;
}

static int32_t STDCALL h_delete(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;

    count_call();
    if (resolve(path, ext, full, sizeof full)) return -1;
    r = DeleteFileA(full) ? 0 : -1;
    put(" delete(");
    put_s(full);
    put(")=%d", r);
    if (counting && r == 0) cover_deletes++;
    return r;
}

static uint32_t STDCALL h_ticks(void)
{
    count_call();
    put(" ticks=%u", clock_ms);
    return clock_ms;
}

static int32_t STDCALL h_command(uint32_t command, uint32_t parameter)
{
    count_call();
    put(" command(%#x,%#x)", command, parameter);
    if (counting) cover_commands++;
    return 0;
}

static const char *STDCALL h_root(void)
{
    count_call();
    put(" root");
    return "CDROOT";
}

/* +0xa8: [APL] MAX_EVENTS and CACHE_WAV_FILES, as the scenario sets them */
static int32_t STDCALL h_cfg_int(const char *section, const char *key, int32_t *value)
{
    count_call();
    put(" cfgint(%s,%s,%d)", section, key, *value);
    if (strcmp(key, "MAX_EVENTS") == 0 && max_events != 0)
        *value = (int32_t)max_events;
    if (strcmp(key, "CACHE_WAV_FILES") == 0 && cache_wav != 2)
        *value = (int32_t)cache_wav;
    put("=%d", *value);
    return 0;
}

static int32_t STDCALL h_cfg_string(const char *section, const char *key, char *buf, uint32_t size)
{
    count_call();
    put(" cfgstring(%s,%s)", section, key);
    if (strcmp(key, "WAVE") == 0 && (scenario_number & 4))
        snprintf(buf, size, "%s", "EXTRAWAV");
    put("=");
    put_s(buf);
    return (int32_t)strlen(buf);
}

/* the key maps */
static int32_t STDCALL h_keys_new(uint32_t *map)
{
    count_call();
    *map = 0x4b000 + ++keymap_counter;
    put(" keys.new=%#x", *map);
    return 0;
}

static int32_t STDCALL h_keys_free(uint32_t *map)
{
    count_call();
    put(" keys.free(%#x)", *map);
    *map = 0;
    return 0;
}

static int32_t STDCALL h_keys_on(uint32_t map)
{
    count_call();
    put(" keys.on(%#x)", map);
    return 0;
}

static int32_t STDCALL h_keys_add(uint32_t map, const uint32_t *binding, uint32_t n)
{
    count_call();
    put(" keys.add(%#x,%#x,%#x,%#x,%u)", map, binding[0], binding[1], binding[2], n);
    if (counting) cover_keys++;
    return 0;
}

static int32_t STDCALL h_keys_drop(uint32_t map, const uint32_t *binding, uint32_t n)
{
    count_call();
    put(" keys.drop(%#x,%#x,%#x,%#x,%u)", map, binding[0], binding[1], binding[2], n);
    return 0;
}

/* the voice engine */
static uint32_t STDCALL h_sound_on(void)
{
    count_call();
    put(" sound.on=%u", sound_on);
    return sound_on;
}

static uint32_t STDCALL voice_load(const char *src, int32_t type, uint32_t *h)
{
    count_call();
    put(" voice.load(");
    put_s(src);
    put(",%d)", type);
    if (counting) cover_sounds++;
    if ((voice_counter + scenario_number) % 9 == 0) {
        voice_counter++;
        *h = 0;
        put("=-1");
        return 0x90001;
    }
    *h = 0x7000 + ++voice_counter;
    put("=%#x", *h);
    return 0;
}

static uint32_t STDCALL voice_add(const char *src, int32_t type, uint32_t dst, int32_t final)
{
    count_call();
    put(" voice.add(");
    put_s(src);
    put(",%d,%#x,%d)", type, dst, final);
    return 0;
}

static uint32_t STDCALL voice_free(uint32_t *h)
{
    count_call();
    put(" voice.free(%#x)", *h);
    *h = 0;
    return 0;
}

static uint32_t STDCALL voice_volume(uint32_t h, int32_t v)
{
    count_call();
    put(" voice.volume(%#x,%d)", h, v);
    return 0;
}

static uint32_t STDCALL voice_play(uint32_t h, int32_t loop, double freq,
                                   void (STDCALL *done)(uint32_t))
{
    count_call();
    put(" voice.play(%#x,%d,%g,", h, loop, freq);
    put_ptr((uint32_t)(uintptr_t)done);
    put(")");
    if (done != NULL && npending < 16) {
        pending[npending].h = h;
        pending[npending].done = done;
        npending++;
    }
    return 0;
}

static uint32_t STDCALL voice_stop(uint32_t h)
{
    count_call();
    put(" voice.stop(%#x)", h);
    return 0;
}

static uint32_t STDCALL h_dialog_tick(void)
{
    count_call();
    put(" dialogtick");
    return 0;
}

static uint32_t STDCALL h_quit(void)
{
    count_call();
    put(" quit");
    return 1;
}

/* ---- the dialog engine: the two pickers, and UTIL's alert dialogs ------------------- */

static int32_t STDCALL h_dialog(uint32_t id, uint8_t *block)
{
    uint32_t a = next_answer();

    count_call();
    put(" dialog(%#x)", id);
    if (id == 0x1108 || id == 0x1106) {
        uint32_t count = *(uint32_t *)(block + 8);
        uint8_t *data = *(uint8_t **)(block + 0x10);
        uint32_t i;

        if (counting) cover_dialog[id == 0x1108 ? 0 : 1]++;
        put(" list(%u", count);
        for (i = 0; i < count && i < 40; i++) {
            put(",");
            put_s((char *)data + 0xa08 * i);
            put("/");
            put_s((char *)data + 0xa08 * i + 0x100);
            put("/");
            put_s((char *)data + 0xa08 * i + 0x900);
            put("/%u", *(uint32_t *)(data + 0xa08 * i + 0xa04));
        }
        put(") lesson=%u", id == 0x1106 ? *(uint32_t *)(block + 0x14) : 0);
        /* the dialog always answers: OK or cancel, and a choice */
        *(uint32_t *)block = (a & 7) != 0;
        *(int32_t *)(block + 0xc) = count && (a & 0x30) ? (int32_t)((a >> 8) % count) : -1;
        if (id == 0x1106)
            *(uint32_t *)(block + 0x14) = (a >> 20) % 30;
        return 0;
    }
    if (counting) cover_dialog[2]++;
    return 0;
}

/* ---- CHAIN, emulated ---------------------------------------------------------------- */

static int32_t STDCALL chain_add(uint32_t list, callback_fn cb)
{
    struct chain_list *l;

    count_call();
    put(" chain.add(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    if (list >= 0x24) return -1;
    l = &cur->chain[list];
    /* a program that sets its rate to the list it is running on re-adds
     * itself to the dispatch in progress, which visits it again: the sim
     * hangs.  Cut at the same count on both sides. */
    if (l->count >= 2048) cut_op();
    l->cb[l->count] = cb;
    l->removed[l->count] = 0;
    l->count++;
    return 0;
}

static void chain_compact(struct chain_list *l)
{
    uint32_t i, n = 0;

    for (i = 0; i < l->count; i++)
        if (!l->removed[i]) {
            l->cb[n] = l->cb[i];
            l->removed[n] = 0;
            n++;
        }
    l->count = n;
    l->pending = 0;
}

static int32_t STDCALL chain_remove(uint32_t list, callback_fn cb)
{
    struct chain_list *l;
    uint32_t i;

    count_call();
    put(" chain.remove(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    if (list >= 0x24) return -1;
    l = &cur->chain[list];
    for (i = 0; i < l->count; i++)
        if (!l->removed[i] && l->cb[i] == cb) {
            l->removed[i] = 1;
            if (l->depth == 0)
                chain_compact(l);
            else
                l->pending = 1;
            return 0;
        }
    put("=-1");
    return -1;
}

/* the dispatch: the count is reread, so an add during it is visited */
static void dispatch(uint32_t list, uint32_t a, uint32_t b)
{
    struct chain_list *l = &cur->chain[list];
    uint32_t i;

    l->depth++;
    for (i = 0; i < l->count; i++)
        if (!l->removed[i])
            l->cb[i](a, b);
    l->depth--;
    if (l->depth == 0 && l->pending)
        chain_compact(l);
}

/* ---- the other modules -------------------------------------------------------------- */

static uint32_t STDCALL main_situate(const uint8_t *block)
{
    uint32_t h = digest(block + 4, (uint32_t)strlen((const char *)block + 4), 0x811c9dc5u), i;

    count_call();
    put(" main.situate(%d,", *(int32_t *)block);
    put_s((const char *)block + 4);
    put(")");
    if (counting) cover_main++;
    /* a situation moves the aircraft */
    for (i = 0; i < 6; i++)
        global[0x560 + 2 + 4 * i] ^= (uint8_t)(h >> (4 * i));
    return 0;
}

/* +0x70 (from, to, &bearing, &range): a function of the bytes retail
 * defines in both positions, so stack garbage around them does not leak */
static uint32_t STDCALL main_range(const uint8_t *from, const uint8_t *to,
                                   uint16_t *bearing, int32_t *range)
{
    uint32_t h = 0x811c9dc5u, i;

    count_call();
    for (i = 0; i < 0x18; i++)
        if ((i & 7) >= 2)
            h = (h ^ from[i]) * 0x01000193u;
    for (i = 0; i < 0x18; i++)
        if ((i & 7) >= 2)
            h = (h ^ to[i]) * 0x01000193u;
    put(" main.range(%08x", h);
    *bearing = (uint16_t)h;
    *range = (int32_t)((h >> 8) % 400000u);
    if ((h & 0xf000) == 0x3000)
        *range = -(int32_t)(h >> 20);
    put(")=%#x,%d", *bearing, *range);
    return 0;
}

static uint32_t STDCALL main_clock(uint32_t a, uint32_t b)
{
    count_call();
    put(" main.clock(%u,%#x)", a, b);
    return 0;
}

static uint32_t STDCALL main_dme(uint32_t which, uint32_t *tenths)
{
    count_call();
    *tenths = dme_answer * which + 7;
    put(" main.dme(%u)=%u,%u", which, (dme_answer & 3) == 0, *tenths);
    return (dme_answer & 3) == 0;
}

static uint32_t STDCALL main_timeofday(void)
{
    count_call();
    put(" main.timeofday");
    return 0;
}

static uint32_t STDCALL demo_play(const char *name, uint32_t dialog)
{
    count_call();
    put(" demo.play(");
    put_s(name);
    put(",%u)", dialog);
    if (counting) cover_videos++;
    return 0;
}

static uint32_t STDCALL sound_event(uint32_t code, uint32_t arg)
{
    count_call();
    put(" sound.event(%#x,%#x)", code, arg);
    return 0;
}

static uint32_t STDCALL sound_volume(uint32_t which)
{
    count_call();
    put(" sound.volume(%u)", which);
    return 6000 + which;
}

static uint32_t STDCALL winmsg_show(const char *text, uint32_t slot, uint32_t time, uint32_t mode)
{
    uint32_t h = (msg_counter++ % 5 == 4) ? 0 : 0x500 + msg_counter;

    count_call();
    put(" winmsg(");
    put_s(text);
    put(",%u,%#x,%u)=%#x", slot, time, mode, h);
    if (counting) cover_msg++;
    return h;
}

static uint32_t STDCALL winmsg_clear(void)
{
    count_call();
    put(" winmsg.clear");
    return 0;
}

/* the module at GLOBAL +0x11d4: its +0x28 writes a 16-bit angle */
static uint32_t STDCALL angle_11d4(uint32_t *out)
{
    count_call();
    *(uint16_t *)out = (uint16_t)value_11d4;
    put(" m11d4(%#x)", value_11d4 & 0xffff);
    return 0;
}

/* UTIL: the modal alert answered, the rest retail's, some logged */
static uint32_t STDCALL u_alert(uint32_t flags, uint32_t tag, uint32_t p1, uint32_t p2,
                                uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines)
{
    uint32_t a = next_answer() & 1;

    count_call();
    put(" alert(%#x,%#x,", flags, tag);
    if (p1 >= 0x10000) put_s((const char *)(uintptr_t)p1); else put("%#x", p1);
    put(",%#x,%#x,%#x,%#x,%u)=%u", p2, p3, p4, p5, lines, a);
    if (counting) cover_alerts++;
    /* op_error's alert: it has already freed the running program's code and
     * will return to a dangling interpreter.  Everything the program touches
     * from here is undefined (and differs between builds); the transcript is
     * still compared, but the derived state is not. */
    if (p1 == 0xf439 && !poisoned) {
        put(" POISON");
        poisoned = 1;
        if (counting) cover_errors++;
    }
    return a;
}

typedef uint32_t (STDCALL *util_fd)(double);
typedef long double (STDCALL *util_frand_fn)(void);
typedef uint32_t (STDCALL *util_f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (STDCALL *util_f8)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t);
typedef uint32_t (STDCALL *util_f1)(uint32_t);
typedef uint32_t (STDCALL *util_f9)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t);
#define UTIL_REAL(off, type) (*(type *)(util_link + (off)))

static uint32_t STDCALL u_round(double v)
{
    uint32_t r = UTIL_REAL(0xb0, util_fd)(v);
    count_call();
    put(" round(%a)=%#x", v, r);
    return r;
}

static uint32_t STDCALL u_low(double v)
{
    uint32_t r = UTIL_REAL(0xb4, util_fd)(v);
    count_call();
    put(" low(%a)=%#x", v, r);
    return r;
}

static long double STDCALL u_frand(void)
{
    long double r = UTIL_REAL(0xb8, util_frand_fn)();
    count_call();
    put(" frand=%La", r);
    return r;
}

static uint32_t STDCALL u_metres(uint32_t in, uint32_t at, uint32_t out)
{
    uint32_t r = UTIL_REAL(0x48, util_f3)(in, at, out);
    count_call();
    put(" metres(%04x%08x)=%04x%08x", *(uint16_t *)(uintptr_t)(in + 2),
        *(uint32_t *)(uintptr_t)(in + 4), *(uint16_t *)(uintptr_t)(out + 2),
        *(uint32_t *)(uintptr_t)(out + 4));
    return r;
}

static uint32_t STDCALL u_open(uint32_t out, uint32_t tag, uint32_t p1, uint32_t p2,
                               uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines)
{
    count_call();
    put(" alertopen(%#x,", tag);
    put_ptr(p1);
    put(")");
    return UTIL_REAL(0xa8, util_f8)(out, tag, p1, p2, p3, p4, p5, lines);
}

static uint32_t STDCALL u_close(uint32_t pp)
{
    count_call();
    put(" alertclose(");
    put_ptr(*(uint32_t *)(uintptr_t)pp);
    put(")");
    return UTIL_REAL(0xac, util_f1)(pp);
}

static uint32_t STDCALL u_list(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e,
                               uint32_t f, uint32_t g, uint32_t h, uint32_t i)
{
    uint32_t r;

    count_call();
    put(" catalogue(%#x,%#x,%#x,", a, b, c);
    put_ptr(f);
    put(",");
    put_ptr(g);
    put(",%#x,%u)", h, i);
    r = UTIL_REAL(0x98, util_f9)(a, b, c, d, e, f, g, h, i);
    put("=%#x,%u", r, *(uint32_t *)(uintptr_t)d);
    return r;
}

#define SET_SLOT(table, offset, fn) (*(void **)((table) + (offset)) = (void *)(fn))

static void build_stubs(void)
{
    memset(host, 0, sizeof host);
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x024, h_quit);
    SET_SLOT(host, 0x038, h_ticks);
    SET_SLOT(host, 0x040, h_command);
    SET_SLOT(host, 0x060, h_root);
    SET_SLOT(host, 0x0a8, h_cfg_int);
    SET_SLOT(host, 0x0c8, h_cfg_string);
    SET_SLOT(host, 0x10c, h_dialog);
    SET_SLOT(host, 0x110, h_dialog);
    SET_SLOT(host, 0x114, h_dialog_tick);
    SET_SLOT(host, 0x138, h_set_attr);
    SET_SLOT(host, 0x13c, h_close);
    SET_SLOT(host, 0x144, h_copy);
    SET_SLOT(host, 0x14c, h_delete);
    SET_SLOT(host, 0x158, h_exists);
    SET_SLOT(host, 0x15c, h_find_close);
    SET_SLOT(host, 0x160, h_find_next);
    SET_SLOT(host, 0x164, h_find_first);
    SET_SLOT(host, 0x17c, h_open);
    SET_SLOT(host, 0x184, h_join);
    SET_SLOT(host, 0x188, h_split);
    SET_SLOT(host, 0x19c, h_read);
    SET_SLOT(host, 0x1a4, h_size);
    SET_SLOT(host, 0x1d8, h_keys_new);
    SET_SLOT(host, 0x1dc, h_keys_free);
    SET_SLOT(host, 0x1e8, h_keys_on);
    SET_SLOT(host, 0x1f4, h_keys_add);
    SET_SLOT(host, 0x1f8, h_keys_drop);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x210, h_realloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x218, h_memset);
    SET_SLOT(host, 0x21c, h_memmove);
    SET_SLOT(host, 0x22c, h_qsort);
    SET_SLOT(host, 0x2a0, h_loadstring);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b0, h_strcat);
    SET_SLOT(host, 0x2b8, h_strlen);
    SET_SLOT(host, 0x2bc, h_strcmp);
    SET_SLOT(host, 0x2c0, h_stricmp);
    SET_SLOT(host, 0x2cc, h_sound_on);
    SET_SLOT(host, 0x2d4, voice_load);
    SET_SLOT(host, 0x2d8, voice_add);
    SET_SLOT(host, 0x2dc, voice_free);
    SET_SLOT(host, 0x2e0, voice_play);
    SET_SLOT(host, 0x2ec, voice_volume);
    SET_SLOT(host, 0x2f4, voice_stop);
    SET_SLOT(host, 0x2f8, h_list_add);
    SET_SLOT(host, 0x300, h_list_remove);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    *(uint32_t *)mainmod = 3;
    SET_SLOT(mainmod, 0x3c, main_situate);
    SET_SLOT(mainmod, 0x70, main_range);
    SET_SLOT(mainmod, 0x74, main_clock);
    SET_SLOT(mainmod, 0x80, main_dme);
    SET_SLOT(mainmod, 0xa4, main_timeofday);
    *(uint32_t *)demo = 7;
    SET_SLOT(demo, 0x28, demo_play);
    *(uint32_t *)sound = 39;
    SET_SLOT(sound, 0x24, sound_event);
    SET_SLOT(sound, 0x30, sound_volume);
    *(uint32_t *)winmsg = 22;
    SET_SLOT(winmsg, 0x18, winmsg_show);
    SET_SLOT(winmsg, 0x20, winmsg_clear);
    SET_SLOT(module_11d4, 0x28, angle_11d4);
}

static void load_util(const char *path)
{
    HMODULE m = LoadLibraryA(path);
    struct import *im;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *s;
    uint32_t i;

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
        else if (im->id == 17) im->link = NULL;
        else { printf("UTIL: unexpected import %u\n", im->id); exit(2); }
    }
    memcpy(util_proxy, util_link, sizeof util_proxy);
    SET_SLOT(util_proxy, 0x48, u_metres);
    SET_SLOT(util_proxy, 0x98, u_list);
    SET_SLOT(util_proxy, 0xa4, u_alert);
    SET_SLOT(util_proxy, 0xa8, u_open);
    SET_SLOT(util_proxy, 0xac, u_close);
    SET_SLOT(util_proxy, 0xb0, u_round);
    SET_SLOT(util_proxy, 0xb4, u_low);
    SET_SLOT(util_proxy, 0xb8, u_frand);
    /* its .data, put back before each side: the random generator's seed */
    dos = (IMAGE_DOS_HEADER *)m;
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)m + dos->e_lfanew);
    s = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, s++)
        if (memcmp(s->Name, ".data", 6) == 0) {
            util_data = (uint8_t *)m + s->VirtualAddress;
            util_data_size = s->Misc.VirtualSize;
        }
    if (util_data == NULL || util_data_size > sizeof util_data_pristine) {
        printf("UTIL: no .data\n");
        exit(2);
    }
    memcpy(util_data_pristine, util_data, util_data_size);
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
    s->link = (struct linkage *)GetProcAddress(s->module, "Linkage");
    if (s->imports == NULL || s->link == NULL || s->link->id != 21) {
        printf("%s: not ADVDRV\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if ((uint8_t *)s->link != s->image + 0x48) {
        printf("%s: Linkage is not ImportTable+0x48\n", path);
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
        case 20: im->link = util_proxy; break;
        case 22: im->link = winmsg; break;
        case 39: im->link = sound; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* ---- fixtures ------------------------------------------------------------------------- */

static char wav_names[1200][16];
static uint32_t nwav;
static char cd_adv[24][16], cd_les[32][16];
static uint32_t ncd_adv, ncd_les;

/* which system variables have a getter and a setter, from the image's
 * relocation list */
static uint8_t has_get[0xb0], has_set[0xb0];
static uint8_t get_list[0xb0], set_list[0xb0];
static uint32_t nget, nset;

static void touch(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) { printf("cannot write %s\n", path); exit(2); }
    fputs("RIFF", f);
    fclose(f);
}

static void copy_file(const char *from, const char *to)
{
    if (!CopyFileA(from, to, FALSE)) { printf("cannot copy %s\n", from); exit(2); }
    SetFileAttributesA(to, FILE_ATTRIBUTE_NORMAL);
}

static void remove_tree(const char *dir)
{
    char pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;

    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    h = FindFirstFileA(pattern, &d);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            snprintf(path, sizeof path, "%s\\%s", dir, d.cFileName);
            SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(path);
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
}

/* copy every file matching `pattern` in `from`, keeping the names */
static uint32_t copy_matching(const char *from, const char *pattern, const char *to,
                              char (*names)[16], uint32_t max)
{
    char p[MAX_PATH], a[MAX_PATH], b[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;
    uint32_t n = 0;

    snprintf(p, sizeof p, "%s\\%s", from, pattern);
    h = FindFirstFileA(p, &d);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        snprintf(a, sizeof a, "%s\\%s", from, d.cFileName);
        snprintf(b, sizeof b, "%s\\%s", to, d.cFileName);
        copy_file(a, b);
        if (names && n < max) {
            char *dot;
            snprintf(names[n], 16, "%s", d.cFileName);
            dot = strchr(names[n], '.');
            if (dot) *dot = 0;
            _strupr(names[n]);
        }
        n++;
    } while (FindNextFileA(h, &d));
    FindClose(h);
    return n;
}

static void build_static_trees(void)
{
    static const char *const dirs[] = {
        "ADV", "ADV\\WAV", "ADV\\WAV\\CACHE", "ADV\\VIDEOS", "LESSONS", "LESSONS\\WAV",
        "LESSONS\\VIDEOS", "CHALLENG", "SITUATIO", "PILOTS", "VIDEOS", "EXTRAWAV",
        "CDROOT", "CDROOT\\fs98", "CDROOT\\fs98\\adv", "CDROOT\\fs98\\adv\\wav"
    };
    char p[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;
    uint32_t i;

    for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        CreateDirectoryA(dirs[i], NULL);
        remove_tree(dirs[i]);
    }
    snprintf(p, sizeof p, "%s\\fs98\\Adv", cd_root);
    ncd_adv = copy_matching(p, "*.adv", "ADV", cd_adv, 24);
    copy_matching(p, "*.stn", "ADV", NULL, 0);
    snprintf(p, sizeof p, "%s\\fs98\\Lessons", cd_root);
    ncd_les = copy_matching(p, "*.les", "LESSONS", cd_les, 32);
    copy_matching(p, "*.stn", "LESSONS", NULL, 0);
    snprintf(p, sizeof p, "%s\\fs98\\Adv\\WAV\\*.wav", cd_root);
    h = FindFirstFileA(p, &d);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (nwav < 1200) {
                char *dot;
                snprintf(wav_names[nwav], 16, "%s", d.cFileName);
                dot = strchr(wav_names[nwav], '.');
                if (dot) *dot = 0;
                _strupr(wav_names[nwav]);
                nwav++;
            }
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    if (ncd_adv != 20 || ncd_les != 24 || nwav < 1000) {
        printf("CD tree: %u adventures, %u lessons, %u sounds\n", ncd_adv, ncd_les, nwav);
        exit(2);
    }
    /* sounds: all on the CD, half in ADV\WAV, a few elsewhere */
    for (i = 0; i < nwav; i++) {
        snprintf(p, sizeof p, "CDROOT\\fs98\\adv\\wav\\%s.WAV", wav_names[i]);
        touch(p);
        if (i % 2 == 0) {
            snprintf(p, sizeof p, "ADV\\WAV\\%s.WAV", wav_names[i]);
            touch(p);
        }
        if (i % 37 == 0) {
            snprintf(p, sizeof p, "LESSONS\\WAV\\%s.WAV", wav_names[i]);
            touch(p);
        }
        if (i % 53 == 0) {
            snprintf(p, sizeof p, "EXTRAWAV\\%s.WAV", wav_names[i]);
            touch(p);
        }
    }
    touch("ADV\\SIT1.STN");
    touch("LESSONS\\SIT1.STN");
    touch("SITUATIO\\SIT2.STN");
    touch("PILOTS\\SIT3.STN");
    touch("ADV\\VIDEOS\\INTRO.VID");
    touch("LESSONS\\VIDEOS\\INTRO.VID");
    touch("VIDEOS\\OTHER.VID");
    for (i = 0; i < sizeof code_slots / 2; i++) {
        uint32_t o = code_slots[i];
        if (o >= 0x318 && o < 0x10d8) {
            if ((o - 0x318) % 0x14 == 0) has_get[(o - 0x318) / 0x14] = 1;
            if ((o - 0x318) % 0x14 == 4) has_set[(o - 0x318) / 0x14] = 1;
        }
    }
    for (i = 0; i < 0xb0; i++) {
        if (has_get[i]) get_list[nget++] = (uint8_t)i;
        if (has_set[i]) set_list[nset++] = (uint8_t)i;
    }
}

/* the cache, as the scenario starts it */
static uint32_t cache_seed;

static void build_cache(void)
{
    char p[MAX_PATH];
    uint32_t i, saved = rng;

    remove_tree("ADV\\WAV\\CACHE");
    rng = cache_seed | 1;
    for (i = 0; i < 6; i++) {
        snprintf(p, sizeof p, "ADV\\WAV\\CACHE\\%s.WAV", wav_names[rnd_n(nwav)]);
        touch(p);
    }
    rng = saved;
}

/* ---- the assembler: synthetic programs ----------------------------------------------- */

struct program {
    uint8_t b[0x10000];
    uint32_t n;
    uint32_t var0, nvars;
    uint32_t sub[8], nsubs;
    uint32_t fix[256], fixsub[256], nfix;
    uint32_t nstrings;
    uint32_t varfix[8192], varidx[8192], nvarfix;
    uint32_t first_bad;
    uint32_t bound[4096], nbound;    /* instruction boundaries, for scan_size */
    int wild;    /* may emit malformed code that trips op_error */
};

static struct program prog;

static void b1(uint32_t v) { prog.b[prog.n++] = (uint8_t)v; }
static void b2(uint32_t v) { b1(v); b1(v >> 8); }
static void b4(uint32_t v) { b2(v); b2(v >> 16); }
static void bd(double v) { memcpy(prog.b + prog.n, &v, 8); prog.n += 8; }
static void bstr(const char *s) { size_t k = strlen(s) + 1; memcpy(prog.b + prog.n, s, k); prog.n += (uint32_t)k; }

/* a variable's offset, patched once the code's end is known */
static void bvar(uint32_t index)
{
    if (prog.nvarfix < 8192) {
        prog.varfix[prog.nvarfix] = prog.n;
        prog.varidx[prog.nvarfix++] = index;
    }
    b4(8 * index);
}

/* a handler's address, or -1 */
static void target_sub(int allow_none)
{
    if (prog.nsubs == 0 || prog.nfix >= 256 || (allow_none && rnd_n(5) == 0)) {
        b4(0xffffffffu);
        return;
    }
    prog.fix[prog.nfix] = prog.n;
    prog.fixsub[prog.nfix++] = rnd_n(prog.nsubs);
    b4(0);
}

static double interesting(void)
{
    static const double v[] = { 0.0, 1.0, -1.0, 0.5, 2.0, 10.0, 360.0, -360.0, 180.0,
                                90.0, 1e10, -1e10, 0.25, 1000.0, 3.5, 118.0, 136.95,
                                108.15, 200.0, 1699.9, 7777.0, 29.92, 99.0, 1.0/3 };
    uint32_t k = rnd_n(30);
    union { uint64_t u; double d; } nan;

    if (k < 24) return v[k];
    if (k == 24) { nan.u = 0x7ff8000000000000ull; return nan.d; }
    if (k == 25) { nan.u = 0x7ff0000000000000ull; return nan.d; }
    return (double)(int32_t)rnd() / (double)(1 + rnd_n(1000));
}

static uint32_t ops[400], nops;
static int start_dump;

static void leaf(void)
{
    uint32_t k = rnd_n(14);

    if (k < 3) { ops[nops++] = 2; ops[nops++] = 0x1000000 | rnd_n(256); }
    else if (k < 4) { ops[nops++] = 3; ops[nops++] = 0x2000000 | (rnd() & 0xffff); }
    else if (k < 5) { ops[nops++] = 4; ops[nops++] = 0x3000000; }
    else if (k < 7) { ops[nops++] = 5; ops[nops++] = 0x4000000; }
    else if (k < 10 && prog.nvars) { ops[nops++] = 7; ops[nops++] = 0x5000000 | rnd_n(prog.nvars); }
    else {
        uint32_t index = get_list[rnd_n(nget)];
        if (prog.wild && rnd_n(60) == 0) index = rnd_n(0xc0);
        ops[nops++] = 8;
        ops[nops++] = 0x6000000 | index;
    }
}

static void tree(int depth)
{
    static const uint8_t unary[] = { 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x20, 0x21, 0x22, 0x23,
                                     0x24, 0x25, 0x26, 0x27, 0x28, 0x2d, 0x2e, 0x2f };
    static const uint8_t binary[] = { 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12,
                                      0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x29,
                                      0x2a, 0x2b, 0x2c };
    uint32_t k = rnd_n(10);

    if (nops > 300 || depth <= 0 || k < 4) {
        leaf();
        return;
    }
    if (k < 6) {
        tree(depth - 1);
        ops[nops++] = unary[rnd_n(sizeof unary)];
    } else if (k < 9) {
        tree(depth - 1);
        tree(depth - 1);
        ops[nops++] = binary[rnd_n(sizeof binary)];
    } else if (rnd_n(3) == 0) {
        tree(depth - 1); tree(depth - 1); tree(depth - 1); tree(depth - 1);
        ops[nops++] = 0x30;
    } else {
        tree(depth - 1); tree(depth - 1); tree(depth - 1);
        ops[nops++] = rnd_n(2) ? 0x1e : 0x31;
    }
}

static void emit_ops(void)
{
    uint32_t i;

    if (nops == 2 && (ops[0] < 9)) {
        /* one operator: the short form */
    } else {
        b1(1);
    }
    for (i = 0; i < nops; i++) {
        uint32_t o = ops[i];
        if (o < 0x100) { b1(o); continue; }
        switch (o >> 24) {
        case 1: b1(o & 0xff); break;
        case 2: b2(o & 0xffff); break;
        case 3: b4(rnd_n(3) ? rnd_n(5000) - 100 : rnd()); break;
        case 4: bd(interesting()); break;
        case 5: bvar(o & 0xffffff); break;
        case 6: b2(o & 0xffffff); break;
        }
    }
    if (!(nops == 2 && ops[0] < 9))
        b1(0);
    if (getenv("ADV_EXPR") && start_dump && prog.n >= 0x19a && prog.n <= 0x1b0) {
        uint32_t j;
        printf("expr at end %#x nops=%u ops:", prog.n, nops);
        for (j = 0; j < nops; j++) printf(" %#x", ops[j]);
        printf("\n");
    }
}

/* an expression; now and then a malformed one */
static void expr(void)
{
    uint32_t k = rnd_n(400);

    nops = 0;
    if (!prog.wild) k = 100 + rnd_n(300);   /* well-formed only */
    if (k == 0) {                   /* past the operand stack */
        uint32_t i, n = 99 + rnd_n(4);
        for (i = 0; i < n; i++) { ops[nops++] = 2; ops[nops++] = 0x101; }
        for (i = 1; i < n; i++) ops[nops++] = 9;
    } else if (k == 1) {            /* two values left */
        leaf(); leaf();
    } else if (k == 2) {            /* none: underflow */
        ops[nops++] = 9;
    } else if (k == 3) {            /* an operator with no handler */
        leaf();
        ops[nops++] = 0x32 + rnd_n(0x10);
    } else if (k == 4) {            /* operator 6 stops the program */
        ops[nops++] = 6;
        leaf();
    } else {
        tree(rnd_n(4));
    }
    emit_ops();
}

/* a small integer, as an expression */
static void expr_int(uint32_t below)
{
    b1(2);
    b1(rnd_n(below));
}

static void expr_value(double v)
{
    b1(5);
    bd(v);
}

static void ref(int allow_system)
{
    if (allow_system && nset && rnd_n(4) == 0) {
        b1(8);
        b2(prog.wild && rnd_n(40) == 0 ? rnd_n(0xb0) : set_list[rnd_n(nset)]);
    } else {
        b1(7);
        bvar(rnd_n(prog.nvars));
    }
}

static void text(void)
{
    uint32_t pieces = 1 + rnd_n(4), i;
    static const char *const words[] = { "Fly heading", "Altitude", "Good", "",
                                         "Turn left now", "Contact tower" };

    for (i = 0; i < pieces; i++) {
        uint32_t k = rnd_n(10);
        if (k < 4) { b1(2); bstr(words[rnd_n(6)]); }
        else if (k < 7) { b1(3); expr(); }
        else if (k < 8) { b1(1); }
        else if (k < 9 && prog.nstrings) { b1(4); b1(0x32); b4(rnd_n(prog.nstrings)); }
        else if (rnd_n(30) == 0) { b1(5 + rnd_n(3)); return; }
    }
    b1(0);
}

static const char *sound_name(void)
{
    uint32_t k = rnd_n(10);
    if (k == 0) return "";
    if (k == 1) return "NOPE";
    return wav_names[rnd_n(nwav)];
}

static void stmt(int level);

static void stmts(int level, uint32_t n)
{
    while (n--)
        stmt(level);
}

static void stmt(int level)
{
    uint32_t k = rnd_n(1000), at;

    if (prog.nbound < 4096 && prog.first_bad == 0)
        prog.bound[prog.nbound++] = prog.n;
    if (prog.n > 0xc000) { b1(0x12); return; }
    if (k < 160) {                          /* assign */
        b1(9); ref(1); expr();
    } else if (k < 220 && level > 0) {      /* if */
        b1(2); at = prog.n; b4(0); expr();
        stmts(level - 1, 1 + rnd_n(3));
        memcpy(prog.b + at, &prog.n, 4);
    } else if (k < 240 && level > 0) {      /* a counted loop */
        uint32_t next, out;
        uint32_t var = rnd_n(prog.nvars);
        b1(3); b1(7); bvar(var);
        expr_int(4); expr_int(8); expr_value(rnd_n(5) ? 1.0 : rnd_n(2) ? -1.0 : 0.5);
        next = prog.n;
        b1(4); bd(0); bd(0); out = prog.n; b4(0); b1(7); bvar(var);
        stmts(level - 1, 1 + rnd_n(2));
        b1(1); b4(next);
        memcpy(prog.b + out, &prog.n, 4);
    } else if (k < 290) {                   /* call */
        b1(5); target_sub(0);
    } else if (k < 300) {                   /* switch */
        uint32_t n = 1 + rnd_n(4), i, table, call = rnd_n(2) && prog.nsubs;
        b1(call ? 8 : 7); b1(n); expr_int(6);
        table = prog.n;
        for (i = 0; i < n; i++)
            b4(0);
        /* jump tables go on to the next instruction; call tables to handlers */
        for (i = 0; i < n; i++) {
            uint32_t to = prog.n;
            memcpy(prog.b + table + 4 * i, &to, 4);
            if (call && prog.nfix < 256) {
                prog.fix[prog.nfix] = table + 4 * i;
                prog.fixsub[prog.nfix++] = rnd_n(prog.nsubs);
            }
        }
    } else if (k < 380) {                   /* wait */
        static const double waits[] = { 0.0, -1.0, 0.05, 0.1, 0.3, 1.0, 2.5, -2.0 };
        b1(0x11); expr_value(waits[rnd_n(8)]); b1(rnd_n(3));
    } else if (k < 430) {                   /* messages */
        b1(rnd_n(2) ? 0xd : 0x10); text();
    } else if (k < 445) {
        b1(0x22); text();
    } else if (k < 455) {
        b1(0x23); expr_int(10); expr_int(10); expr_int(10);
    } else if (k < 462) {
        b1(0x24); expr_int(rnd_n(5) ? 20 : 255);
    } else if (k < 500) {                   /* a sound */
        b1(0xe); bstr(sound_name());
    } else if (k < 508) {
        static const char *const v[] = { "INTRO", "OTHER", "NOVIDEO" };
        b1(0x26); bstr(v[rnd_n(3)]);
    } else if (k < 514) {
        static const char *const s[] = { "SIT1", "SIT2", "SIT3", "NOSIT", "X\\SIT2.STN" };
        b1(0x18); bstr(s[rnd_n(5)]);
    } else if (k < 530) {
        b1(0x16); expr(); expr(); expr();
    } else if (k < 600) {                   /* weather */
        uint32_t w = rnd_n(7);
        b1(0x1a + w);
        switch (w) {
        case 0: bstr("AREA"); expr(); expr(); expr(); expr(); break;
        case 1: expr(); expr(); expr(); expr(); break;
        case 2:
            expr_int(5); b1(rnd_n(4) != 0);
            if (prog.b[prog.n - 1]) {
                expr_value(rnd_n(20000)); expr_value(rnd_n(30000)); b1(rnd_n(12));
                expr_int(10); expr_int(8); expr_int(8); expr_int(3);
            }
            break;
        case 3:
            expr_int(5); b1(rnd_n(4) != 0);
            if (prog.b[prog.n - 1]) {
                expr_value(rnd_n(3) ? rnd_n(30000) : 0); expr_value(rnd_n(30000));
                b1(rnd_n(2)); expr(); expr_int(250); expr_int(255);
            }
            break;
        case 4:
            expr_int(6); b1(rnd_n(4) != 0);
            if (prog.b[prog.n - 1]) {
                expr_value(rnd_n(40000) - 2000.0); expr_value(rnd_n(160) - 80.0);
                expr_value(rnd_n(30));
            }
            break;
        case 5: expr(); expr(); break;
        default: expr(); break;
        }
    } else if (k < 620) {                   /* keys */
        uint32_t n = rnd_n(4), i;
        b1(0x25); b1(rnd_n(3));
        if (prog.b[prog.n - 1]) {
            b1(n);
            for (i = 0; i < n; i++) {
                b1(3); b2(rnd_n(8) ? 0x41 + rnd_n(26) : rnd_n(0x900));
            }
        }
    } else if (k < 650) {
        b1(0xc); b1(3); b2(rnd_n(4) ? 0x41 + rnd_n(26) : rnd_n(0x10000)); target_sub(1);
    } else if (k < 660) {
        b1(rnd_n(2) ? 0xa : 0xb); target_sub(1);
    } else if (k < 720) {                   /* events */
        uint32_t e = rnd_n(4);
        b1(0x28 + e);
        if (e == 3) { target_sub(1); }
        else {
            expr(); target_sub(1);
            if (e == 0) target_sub(1);
            if (e == 1) { target_sub(1); bd(0); }
            if (e == 2) bd(0);
        }
        if (counting) cover_events++;
    } else if (k < 740) {
        static const uint8_t which[] = { 0, 1, 2, 3, 4, 5, 100 };
        b1(0x27); b1(which[rnd_n(7)]);
        if (prog.b[prog.n - 1] != 100) expr_int(rnd_n(10) ? 7 : 1);
    } else if (k < 760) {
        b1(0x12 + rnd_n(4));
    } else if (k < 765) {
        b1(0x17);
    } else if (k < 768) {
        b1(0x19);
    } else if (k < 770) {
        if (prog.wild) {
            if (prog.first_bad == 0) prog.first_bad = prog.n;
            b1(rnd_n(3) ? 0x0f : 0x2c + rnd_n(0x10));
        } else {
            b1(0x12);
        }
    } else if (k < 772) {
        b1(0);
    } else if (k < 790 && level > 0) {      /* a jump over what follows */
        b1(1); at = prog.n; b4(0);
        stmts(level - 1, 1);
        memcpy(prog.b + at, &prog.n, 4);
    } else {
        b1(9); ref(0); expr();
    }
}

static uint32_t crc_table[256];

static uint32_t crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xffffffffu;
    while (n--)
        c = (c >> 8) ^ crc_table[(c ^ *p++) & 0xff];
    return ~c;
}

/* assemble a program and write it at `path` */
static void make_program(const char *path, uint32_t k)
{
    static uint8_t file[0x12000];
    uint32_t i, size, scan, version, code_size, kind = rnd_n(20);
    FILE *f;

    memset(&prog, 0, sizeof prog);
    prog.nvars = 4 + rnd_n(12);
    prog.nsubs = rnd_n(6);
    prog.wild = rnd_n(8) == 0;
    prog.nstrings = kind < 4 ? 1 + rnd_n(5) : 0;
    /* the main program, a loop */
    if (rnd_n(2)) { b1(0x27); b1(rnd_n(2) ? 2 : 1); expr_int(6); }
    stmts(2, 2 + rnd_n(6));
    {
        uint32_t top = prog.n;
        stmts(2, 4 + rnd_n(20));
        b1(0x11); expr_value(rnd_n(2) ? 0.2 : 0.0); b1(rnd_n(2));
        b1(1); b4(rnd_n(8) ? top : 0);
    }
    for (i = 0; i < prog.nsubs; i++) {
        prog.sub[i] = prog.n;
        stmts(1, 1 + rnd_n(5));
        b1(6);
    }
    for (i = 0; i < prog.nfix; i++)
        memcpy(prog.b + prog.fix[i], &prog.sub[prog.fixsub[i]], 4);
    /* the scan region, always ending on an instruction boundary and never
     * reaching a deliberately-bad opcode -- as the APL compiler's scan_size
     * does.  Usually the whole code, sometimes a shorter prefix. */
    {
        uint32_t limit = prog.first_bad ? prog.first_bad : prog.n;
        scan = limit;
        if (rnd_n(3) == 0 && prog.nbound) {
            uint32_t tries;
            for (tries = 0; tries < 8; tries++) {
                uint32_t b = prog.bound[rnd_n(prog.nbound)];
                if (b <= limit) { scan = b; break; }
            }
        }
    }
    prog.var0 = prog.n;
    for (i = 0; i < prog.nvars; i++)
        bd(rnd_n(3) ? (double)rnd_n(20) : interesting());
    for (i = 0; i < prog.nvarfix; i++) {
        uint32_t v = prog.var0 + 8 * prog.varidx[i];
        memcpy(prog.b + prog.varfix[i], &v, 4);
    }
    code_size = prog.n;
    for (i = 0; i < prog.nstrings; i++) {
        char s[40];
        snprintf(s, sizeof s, "String %u of program %u", i, k);
        bstr(s);
    }
    memset(file, 0, 0x200);
    memcpy(file, rnd_n(3) ? "FS6X Adventure\x1a" : "FS6 Adventure\x1a", 16);
    snprintf((char *)file + 0x10, 0x40, "Synthetic %u", k);
    snprintf((char *)file + 0x50, 0x100, "A program assembled for scenario %u", scenario_number);
    version = kind == 4 || kind == 5 ? 0x10 : 0x61;     /* 1.0 is the old format */
    if (kind == 6) version = 0x60;                      /* refused */
    *(uint32_t *)(file + 0x150) = version >> 4;
    *(uint32_t *)(file + 0x154) = version & 0xf;
    *(uint32_t *)(file + 0x15c) = prog.nstrings;
    *(uint32_t *)(file + 0x160) = prog.nstrings && rnd_n(4) == 0 ? code_size + 0x100 : code_size;
    *(uint32_t *)(file + 0x164) = scan;
    size = prog.n;
    memcpy(file + 0x200, prog.b, size);
    if (version == 0x10)
        for (i = 0; i < size; i++)
            file[0x200 + i] = (uint8_t)(i - file[0x200 + i]);
    *(uint32_t *)(file + 0x158) = crc32(file + 0x200, size) + (kind == 7);
    if (kind == 8) file[3] = 'Y';
    f = fopen(path, "wb");
    if (f == NULL) { printf("cannot write %s\n", path); exit(2); }
    fwrite(file, 1, 0x200 + size, f);
    fclose(f);
    if (getenv("ADV_PROG") && k == (uint32_t)atoi(getenv("ADV_PROG"))) {
        uint32_t j;
        printf("prog %u: code=%#x scan=%#x first_bad=%#x nbound=%u\n",
               k, code_size, scan, prog.first_bad, prog.nbound);
        printf("bounds:");
        for (j = 0; j < prog.nbound; j++) printf(" %#x", prog.bound[j]);
        printf("\n");
    }
}

/* ---- scenarios ------------------------------------------------------------------------ */

static void set_precision(uint32_t p);
static uint8_t global_start[GLOBAL_SIZE];
static uint8_t nav_start[3][0x60];
static char programs[MAX_PROGRAMS][MAX_PATH];
static uint32_t program_types[MAX_PROGRAMS], nprograms;

static void make_scenario(uint32_t number)
{
    static const char *const dirs[3] = { "ADV", "LESSONS", "CHALLENG" };
    static const uint32_t types[3] = { 0x1a, 0x20, 0x21 };
    static const char *const exts[3] = { "ADV", "LES", "CHL" };
    char path[MAX_PATH];
    uint32_t i;

    scenario_number = number;
    set_precision(0);       /* the generator's own arithmetic, the same each time */
    rng = 0x9e3779b9u ^ (number * 0x85ebca6bu);
    rnd();
    precision = rnd_n(3);
    sound_on = rnd_n(8) != 0;
    max_events = rnd_n(4) == 0 ? 3 + rnd_n(20) : 0;
    cache_wav = rnd_n(3);
    dme_answer = rnd();
    value_11d4 = rnd();
    cache_seed = rnd();
    for (i = 0; i < GLOBAL_SIZE; i += 4)
        *(uint32_t *)(global_start + i) = rnd_n(3) ? rnd() & 0x0101ffff : rnd();
    /* the -9999 "none" sentinel in a spread of 16-bit slots, so the getters
     * that special-case it are exercised */
    for (i = 0; i < 40; i++)
        if (rnd_n(2))
            *(uint16_t *)(global_start + 2 * rnd_n(GLOBAL_SIZE / 2)) = 0xd8f1;
    /* the weather-area readback words the getters special-case */
    for (i = 0xe70; i < 0x1078; i += 2)
        if (rnd_n(4) == 0)
            *(uint16_t *)(global_start + i) = 0xd8f1;
    *(uint32_t *)(global_start + 0x268) = 0;
    *(uint32_t *)(global_start + 0x758) = 0;
    *(uint32_t *)(global_start + 0x264) = 0;
    *(uint32_t *)(global_start + 0x384) = (uint32_t)(uintptr_t)nav_table;
    *(uint32_t *)(global_start + 0x11d4) = (uint32_t)(uintptr_t)module_11d4;
    *(uint32_t *)(global_start + 0xb24) = rnd_n(6) != 0;
    *(uint16_t *)(global_start + 0xf90) = rnd_n(3) == 0;
    *(uint16_t *)(global_start + 0xb00) = (uint16_t)(1 + rnd_n(0x4000));
    *(uint16_t *)(global_start + 0x832) = 0;
    global_start[0x36c] = 0;
    for (i = 0; i < 3; i++) {
        uint32_t j;
        for (j = 0; j < 0x60; j += 4)
            *(uint32_t *)(nav_start[i] + j) = rnd();
        nav_start[i][0x1c] = (uint8_t)(rnd_n(2) ? 0xc1 : rnd());
        *(uint16_t *)(nav_start[i] + 0x18) = (uint16_t)rnd_n(2);
    }
    /* the synthetic programs, written once for both sides */
    for (i = 0; i < 3; i++) {
        WIN32_FIND_DATAA d;
        HANDLE h;
        char pat[MAX_PATH];

        snprintf(pat, sizeof pat, "%s\\SYN*.*", dirs[i]);
        h = FindFirstFileA(pat, &d);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                snprintf(path, sizeof path, "%s\\%s", dirs[i], d.cFileName);
                DeleteFileA(path);
            } while (FindNextFileA(h, &d));
            FindClose(h);
        }
    }
    nprograms = 2 + rnd_n(MAX_PROGRAMS - 1);
    for (i = 0; i < nprograms; i++) {
        start_dump = getenv("ADV_EXPR") && i == 0;
        uint32_t d = rnd_n(5) == 0 ? 2 : rnd_n(2);
        snprintf(programs[i], MAX_PATH, "%s\\SYN%u", dirs[d], i);
        snprintf(path, sizeof path, "%s.%s", programs[i], exts[d]);
        program_types[i] = types[d];
        make_program(path, i);
    }
}

/* ---- driving ------------------------------------------------------------------------- */

static void set_precision(uint32_t p)
{
    static const uint16_t cw[3] = { 0x027f, 0x037f, 0x007f };
    uint16_t c = cw[p % 3];
    __asm__ volatile("fninit\n\tfldcw %0" : : "m"(c));
}

/* A fault in either module is an outcome: logged, and the op abandoned.
 * Both sides must fault at the same place for the transcripts to agree. */
static void *fault_jb[5];
static int trace_exceptions;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

static void cut_op(void)
{
    if (!in_module)
        return;
    put(" CUT");
    if (counting)
        cover_hangs++;
    in_module = 0;
    op_faulted = 1;
    __builtin_longjmp(fault_jb, 1);
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;

    if (!in_module || cur == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
    if (trace_exceptions) {
        static char line[256];
        DWORD w;
        int n = wsprintfA(line, "exception %08lx at %08lx\r\n", code, e->ContextRecord->Eip);
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, (DWORD)n, &w, NULL);
    }
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW)
        put(" FAULT(div)");
    else if (code == EXCEPTION_ACCESS_VIOLATION)
        put(" FAULT(av)");
    else if (code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_PRIV_INSTRUCTION)
        put(" FAULT(ill)");
    else
        return EXCEPTION_CONTINUE_SEARCH;
    if (counting)
        cover_faults++;
    in_module = 0;
    op_faulted = 1;
    poisoned = 1;   /* a fault is a crash: the program is dead until reloaded */
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

#define IMG32(img, o) (*(uint32_t *)((img) + (o)))

static void put_state(void)
{
    static uint8_t img[IMAGE_SIZE];
    uint32_t i, base = (uint32_t)(uintptr_t)cur->image, code;

    if (poisoned)
        return;    /* op_error fired: output suppressed, both sides alike */
    memcpy(img, cur->image, IMAGE_SIZE);
    /* op_error frees the code buffer (A_CODE=0) and the interpreter keeps
     * running on a dangling cursor until it faults at an implementation-
     * defined point: the whole image is then post-free garbage, so compare
     * only the persistent heap and GLOBAL.  A clean fault (a divide, say)
     * leaves A_CODE intact and the image meaningful but for the cursor. */
    if (op_faulted && IMG32(img, 0x18fc) == 0) {
        put("\n  state img=freed arena=%#x/%08x G=%08x util=%08x (fault, code freed)",
            arena_used, digest(arena, arena_used, 0x811c9dc5u),
            digest(global, GLOBAL_SIZE, 0x811c9dc5u),
            digest(util_data, util_data_size, 0x811c9dc5u));
        return;
    }
    /* while stopped the interpreter cursor is dead state: a fault or a
     * normal stop can leave A_PC and the operand stack at different values
     * in the two builds, and nothing reads them until a program loads
     * (which resets them).  Mask them unless a program is running. */
    if (IMG32(img, 0x18f8) == 0 || op_faulted) {
        IMG32(img, 0x1900) = 0;         /* A_PC */
        IMG32(img, 0x278c) = 0;         /* A_SP */
        IMG32(img, 0x12f8) = 0;         /* A_STEPPED */
        memset(img + 0x27a0, 0, 0x2ac0 - 0x27a0);   /* the operand stack */
    }
    for (i = 0; i < sizeof code_slots / 2; i++)
        IMG32(img, code_slots[i]) = pair_code(IMG32(img, code_slots[i]));
    for (i = 0x26c0; i < 0x278c; i += 4)
        IMG32(img, i) = pair_code(IMG32(img, i));
    for (i = 0; i < sizeof data_slots / 2; i++) {
        uint32_t v = IMG32(img, data_slots[i]);
        if (v >= base && v < base + IMAGE_SIZE)
            IMG32(img, data_slots[i]) = 0xd0000000u | (v - base);
    }
    code = IMG32(img, 0x18fc);
    put("\n  state img=%08x arena=%#x/%08x G=%08x util=%08x run=%u pc=%#x wait=%u "
        "depth=%u sp=%u steps=%u list=%u rate=%u events=%u",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena, arena_used, 0x811c9dc5u), digest(global, GLOBAL_SIZE, 0x811c9dc5u),
        digest(util_data, util_data_size, 0x811c9dc5u), IMG32(img, 0x18f8),
        (op_faulted || IMG32(img, 0x18f8) == 0) ? 0 : IMG32(img, 0x1900) - code,
        IMG32(img, 0x1708), IMG32(img, 0x18bc),
        IMG32(img, 0x278c), IMG32(img, 0x12f8), IMG32(img, 0x7c), IMG32(img, 0xc8),
        IMG32(img, 0x1968));
    if (getenv("ADV_ARENA")) {
        char name[64];
        FILE *f;
        snprintf(name, sizeof name, "arena_%s_%u.bin", cur->name, op_number);
        f = fopen(name, "wb");
        if (f) { fwrite(arena, 1, arena_used, f); fclose(f); }
    }
    if (getenv("ADV_IMAGE")) {
        put("\n  whole");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (IMG32(img, i) != 0)
                put(" %x:%08x", i, IMG32(img, i));
        put("\n  gwhole");
        for (i = 0; i + 4 <= GLOBAL_SIZE; i += 4)
            put(" %x:%08x", i, *(uint32_t *)(global + i));
    }
}

static void steer_global(void)
{
    uint32_t k = rnd_n(16), i;

    switch (k) {
    case 0: *(uint32_t *)(global + 0x564) += rnd_n(4000) - 2000; break;
    case 1: *(uint32_t *)(global + 0x56c) += rnd() >> 12; break;
    case 2: *(uint32_t *)(global + 0x574) = rnd_n(3000); break;
    case 3: *(uint16_t *)(global + 0x2a0) += (uint16_t)rnd_n(4000); break;
    case 4: *(uint16_t *)(global + 0x832) = (uint16_t)rnd_n(2); break;
    case 5: global[0x36c] = (uint8_t)rnd_n(2); break;
    case 6: *(uint32_t *)(global + 0xb24) = rnd_n(5) != 0; break;
    case 7: *(uint32_t *)(global + 0x268) = rnd_n(10) == 0; break;
    case 8:
        for (i = 0; i < 4; i++)
            *(uint16_t *)(global + 2 * rnd_n(GLOBAL_SIZE / 2)) = 0xd8f1;
        for (i = 0; i < 8; i++) {
            uint32_t o = rnd_n(GLOBAL_SIZE / 2) * 2;
            if (o != 0x384 && o != 0x386 && o != 0x11d4 && o != 0x11d6 &&
                o != 0x758 && o != 0x75a && o != 0xb00)
                *(uint16_t *)(global + o) = (uint16_t)rnd();
        }
        break;
    default: break;
    }
}

static uint32_t image32(uint32_t o)
{
    return *(uint32_t *)(cur->image + o);
}

static void frame(void)
{
    if (poisoned)
        return;
    clock_ms += rnd_n(20) == 0 ? 1000 + rnd_n(4000) : 40 + rnd_n(40);
    if (rnd_n(3) == 0)
        steer_global();
    dispatch(1, 0, 0);
    dispatch(2, 0, 0);
    dispatch(5, 0, 0);
    dispatch(3, 0, 0);
    *(uint32_t *)(global + 0x268) = 0;
    if (counting) cover_frames++;
}

static void run_side(struct side *s)
{
    uint32_t i, nops;
    char path[0x200];

    cur = s;
    fault_side = s->name;
    fault_op = 0;
    s->length = 0;
    memset(s->chain, 0, sizeof s->chain);
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    memcpy(util_data, util_data_pristine, util_data_size);
    arena_used = 0;
    memset(arena, 0, 0x200000);
    memcpy(global, global_start, GLOBAL_SIZE);
    memcpy(nav_records, nav_start, sizeof nav_records);
    for (i = 0; i < 3; i++)
        nav_table[i] = (uint32_t)(uintptr_t)nav_records[i];
    ext_count = 0x25;
    clock_ms = 100000 + scenario_number;
    msg_counter = voice_counter = keymap_counter = npending = 0;
    build_cache();
    rng = 0x51ed2701u ^ (scenario_number * 0x27d4eb2fu);
    rnd();
    for (i = 0; i < 64; i++)
        script[i] = rnd();
    script_at = 0;
    set_precision(precision_start);

    op_number = 0;
    op_calls = 0;
    op_faulted = 0;
    poisoned = 0;
    put("init:");
    in_module = 1;
    if (!__builtin_setjmp(fault_jb))
        put(" =%#x", s->link->init());
    in_module = 0;
    put_state();
    nops = 8 + rnd_n(MAX_OPS - 8);
    for (op_number = 1; op_number <= nops; op_number++) {
        uint32_t k = rnd_n(100), r;

        fault_op = op_number;
        op_calls = 0;
        op_faulted = 0;
        op_started = GetTickCount();
        if (__builtin_setjmp(fault_jb)) {
            in_module = 0;
            set_precision(precision);
            put_state();
            if (poisoned)
                break;    /* a fault is a crash: end the scenario here */
            continue;
        }
        in_module = 1;
        /* a load, picker, stop or restart brings the interpreter back to a
         * clean state, so a prior fault/op_error no longer poisons from here */
        if (k < 16 || (k >= 80 && k < 83))
            poisoned = 0;
        if (k < 12) {
            uint32_t which = rnd_n(3) ? rnd_n(nprograms) :
                             rnd_n(nprograms + ncd_adv + ncd_les + 2), type;
            if (which < nprograms) {
                snprintf(path, sizeof path, "%s", programs[which]);
                type = program_types[which];
            } else if ((which -= nprograms) < ncd_adv) {
                snprintf(path, sizeof path, "ADV\\%s", cd_adv[which]);
                type = 0x1a;
            } else if ((which -= ncd_adv) < ncd_les) {
                snprintf(path, sizeof path, "LESSONS\\%s", cd_les[which]);
                type = 0x20;
            } else if (which == ncd_les) {
                snprintf(path, sizeof path, "ADV\\MISSING");
                type = 0x1a;
            } else {
                memset(path, 'L', 0x110);
                path[0x110] = 0;
                type = 0x1a;
            }
            put("\nop %u: load(%.80s,%#x)", op_number, path, type);
            if (counting) cover_ops[0]++;
            r = s->link->load(path, type);
            put(" =%#x", r);
            if (counting && image32(0x18f8)) cover_loaded++;
            if (counting) cover_loads++;
        } else if (k < 14) {
            put("\nop %u: adventures", op_number);
            if (counting) cover_ops[1]++;
            put(" =%#x", s->link->adventures());
        } else if (k < 16) {
            put("\nop %u: lessons", op_number);
            if (counting) cover_ops[2]++;
            put(" =%#x", s->link->lessons());
        } else if (k < 58) {
            uint32_t frames = 1 + rnd_n(40), f;
            put("\nop %u: frames(%u)", op_number, frames);
            if (counting) cover_ops[3]++;
            for (f = 0; f < frames; f++)
                frame();
        } else if (k < 68) {
            uint32_t command, handled = 0x55, j, keys[16], nkeys = 0;
            for (j = 0; j < 0x800 && nkeys < 16; j++)
                if (cur->image[0x1ec0 + j])
                    keys[nkeys++] = j;
            switch (rnd_n(6)) {
            case 0: command = 0x10001; break;
            case 1: command = 0x1e000 + rnd_n(0x1000); break;
            case 2: command = 0x10000 + rnd_n(0x200); break;
            default:
                command = 0x1e000 + (nkeys ? keys[rnd_n(nkeys)] : 0x41 + rnd_n(26));
                break;
            }
            put("\nop %u: command(%#x)", op_number, command);
            if (counting) cover_ops[4]++;
            if (!poisoned)
                dispatch(0xb, command, (uint32_t)(uintptr_t)&handled);
            put(" handled=%#x", handled);
        } else if (k < 74) {
            uint32_t code = rnd_n(4) ? 0x50 : 0x21, arg;
            arg = rnd_n(3) ? image32(0x1720) : rnd_n(2) ? image32(0x1700) : rnd_n(4);
            put("\nop %u: message(%#x,%#x)", op_number, code, arg);
            if (counting) cover_ops[5]++;
            if (!poisoned)
                dispatch(0x14, code, arg);
        } else if (k < 80) {
            put("\nop %u: sound done", op_number);
            if (counting) cover_ops[6]++;
            if (npending && !poisoned) {
                struct voice_done v = pending[0];
                memmove(pending, pending + 1, --npending * sizeof pending[0]);
                put(" (%#x)", v.h);
                v.done(v.h);
            }
        } else if (k < 82) {
            put("\nop %u: stop", op_number);
            if (counting) cover_ops[7]++;
            put(" =%#x", s->link->stop());
        } else if (k < 83) {
            put("\nop %u: restart", op_number);
            if (counting) cover_ops[8]++;
            s->link->restart();
        } else if (k < 86) {
            uint32_t type = 0xabcd;
            r = s->link->running(rnd_n(5) ? &type : NULL);
            put("\nop %u: running=%u,%#x", op_number, r, type);
            if (counting) cover_ops[9]++;
        } else if (k < 88) {
            uint32_t v = rnd_n(2);
            put("\nop %u: skip(%u)=%u", op_number, v, s->link->set_skip(v));
            if (counting) cover_ops[10]++;
        } else if (k < 97) {
            put("\nop %u: steer", op_number);
            if (counting) cover_ops[11]++;
            for (i = 0; i < 6; i++)
                steer_global();
            value_11d4 = rnd();
            dme_answer = rnd();
        } else {
            precision = rnd_n(3);
            set_precision(precision);
            put("\nop %u: precision(%u)", op_number, precision);
            if (counting) cover_ops[12]++;
        }
        in_module = 0;
        put_state();
        if (poisoned)
            break;    /* op_error or a fault: a crash -- nothing after is real */
    }
    if (poisoned) {
        /* both sides stopped at the same crash, transcripts identical up to
         * it; close OS handles silently and end */
        for (i = 0; i < 12; i++)
            if (files[i].open) { CloseHandle(files[i].h); files[i].open = 0; }
        for (i = 0; i < 8; i++)
            if (finds[i].open) { FindClose(finds[i].h); finds[i].open = 0; }
        cur = NULL;
        return;
    }
    op_number++;
    op_calls = 0;
    op_faulted = 0;
    put("\ndeinit:");
    if (!__builtin_setjmp(fault_jb)) {
        in_module = 1;
        put(" =%#x", s->link->deinit());
    }
    in_module = 0;
    put_state();
    for (i = 0; i < 12; i++)
        if (files[i].open) {
            put(" open:file%u", i);
            CloseHandle(files[i].h);
            files[i].open = 0;
        }
    for (i = 0; i < 8; i++)
        if (finds[i].open) {
            FindClose(finds[i].h);
            finds[i].open = 0;
        }
    {
        WIN32_FIND_DATAA d;
        HANDLE h = FindFirstFileA("ADV\\WAV\\CACHE\\*", &d);
        static char names[64][64];
        uint32_t n = 0, j;
        put("\ncache:");
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < 64)
                    snprintf(names[n++], 64, "%s/%lx", d.cFileName,
                             (unsigned long)(d.dwFileAttributes & FILE_ATTRIBUTE_READONLY));
            } while (FindNextFileA(h, &d));
            FindClose(h);
        }
        qsort(names, n, 64, (int (*)(const void *, const void *))_stricmp);
        for (j = 0; j < n; j++)
            put(" %s", names[j]);
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
    printf("scenario %u differs at byte %u\n", scenario_number, i);
    from = (uint32_t)(ls - a) > 1500 ? (uint32_t)(ls - a) - 1500 : 0;
    printf("--- before:\n%.*s\n", (int)((uint32_t)(ls - a) - from), a + from);
    from = i - (uint32_t)(ls - a) > 400 ? i - 400 : (uint32_t)(ls - a);
    printf("--- retail:\n...%.900s\n", a + from);
    printf("--- ours:\n...%.900s\n", b + from);
}

static DWORD WINAPI watchdog(LPVOID unused)
{
    (void)unused;
    for (;;) {
        Sleep(500);
        if (in_module && op_started && GetTickCount() - op_started > 30000) {
            CONTEXT c;
            struct side *t = strcmp(fault_side, "ours") == 0 ? &ours : &retail;

            SuspendThread(main_thread);
            c.ContextFlags = CONTEXT_FULL;
            GetThreadContext(main_thread, &c);
            printf("SPIN scenario %u op %u %s eip %08lx (.text+%#lx)\n", scenario_number,
                   fault_op, fault_side, c.Eip, c.Eip - (DWORD)(uintptr_t)t->lo - 0x1000);
            fflush(stdout);
            ExitProcess(4);
        }
    }
    return 0;
}

static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    uint8_t *at = (uint8_t *)e->ExceptionRecord->ExceptionAddress;
    struct side *s[2] = { &retail, &ours };
    int i;

    for (i = 0; i < 2; i++)
        if (s[i]->module && at >= s[i]->lo && at < s[i]->hi)
            printf("\nFAULT in %s ADVDRV at .text+%#x\n", s[i]->name,
                   (uint32_t)(at - s[i]->lo - 0x1000));
    printf("FAULT %#lx at %p, scenario %u op %u, running %s\n",
           e->ExceptionRecord->ExceptionCode, (void *)at, scenario_number, op_number,
           cur ? cur->name : "none");
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
    uint32_t first = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 0) : 0;
    uint32_t i, differ = 0;

    if (argc < 3) {
        printf("usage: advdrvdiff N CD-ROOT [FIRST]\n");
        return 2;
    }
    snprintf(cd_root, sizeof cd_root, "%s", argv[2]);
    for (i = 0; cd_root[i]; i++)
        if (cd_root[i] == '/') cd_root[i] = '\\';
    for (i = 0; i < 256; i++) {
        uint32_t v = i, b;
        for (b = 0; b < 8; b++)
            v = v & 1 ? (v >> 1) ^ 0xedb88320u : v >> 1;
        crc_table[i] = v;
    }
    trace_exceptions = getenv("ADV_EXC") != NULL;
    AddVectoredExceptionHandler(1, on_vectored);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    SetUnhandledExceptionFilter(on_fault);
    load_ext_table("FLTSIM98_host.exe");
    build_stubs();
    load_util("UTIL_retail.dll");
    load_side(&retail, "ADVDRV_retail.dll", "retail");
    load_side(&ours, "ADVDRV_ours.dll", "ours");
    build_static_trees();

    if (getenv("ADV_GENCHECK")) {
        for (i = first; i < first + n; i++) {
            static uint8_t a[0x20000], b[0x20000];
            FILE *f;
            size_t na, nb;
            make_scenario(i);
            f = fopen("LESSONS\\SYN1.LES", "rb"); na = f ? fread(a, 1, sizeof a, f) : 0; if (f) fclose(f);
            if (!f) { f = fopen("ADV\\SYN1.ADV", "rb"); na = f ? fread(a, 1, sizeof a, f) : 0; if (f) fclose(f); }
            precision_start = precision;
            {
                static uint32_t saved[256];
                uint32_t q;
                memcpy(saved, crc_table, sizeof saved);
                if (getenv("ADV_GENRUN")) run_side(&retail);
                for (q = 0; q < 256; q++)
                    if (saved[q] != crc_table[q])
                        printf("crc_table[%u] %08x -> %08x at %p\n", q, saved[q], crc_table[q], (void *)&crc_table[q]);
                printf("prog %p ops %p crc %p global %p nav %p\n", (void *)&prog, (void *)ops, (void *)crc_table, (void *)global, (void *)nav_records);
            }
            printf("nget %u nset %u nwav %u\n", nget, nset, nwav);
            make_scenario(i);
            f = fopen("LESSONS\\SYN1.LES", "rb"); nb = f ? fread(b, 1, sizeof b, f) : 0; if (f) fclose(f);
            if (!f) { f = fopen("ADV\\SYN1.ADV", "rb"); nb = f ? fread(b, 1, sizeof b, f) : 0; if (f) fclose(f); }
            if (na != nb || memcmp(a, b, na)) {
                size_t j;
                for (j = 0; j < na && a[j] == b[j]; j++) ;
                printf("scenario %u: generation differs at %#x of %#x/%#x\n", i, (unsigned)j, (unsigned)na, (unsigned)nb);
                for (j = 0; j < na; j++) if (a[j] != b[j]) printf("  %#x: %02x %02x\n", (unsigned)j, a[j], b[j]);
            }
        }
        return 0;
    }
    for (i = first; i < first + n; i++) {
        make_scenario(i);
        precision_start = precision;
        counting = 1;
        run_side(&retail);
        counting = 0;
        precision = precision_start;
        make_scenario(i);
        run_side(&ours);
        if (getenv("ADV_DUMP") != NULL) {
            FILE *f = fopen("adv_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("adv_ours.txt", "wb");
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
    printf("advdrvdiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops: load %lu, adventures %lu, lessons %lu, frames %lu, command %lu, "
           "message %lu, sound done %lu, stop %lu, restart %lu, running %lu, skip %lu, "
           "steer %lu, precision %lu\n",
           cover_ops[0], cover_ops[1], cover_ops[2], cover_ops[3], cover_ops[4],
           cover_ops[5], cover_ops[6], cover_ops[7], cover_ops[8], cover_ops[9],
           cover_ops[10], cover_ops[11], cover_ops[12]);
    printf("  loads %lu (%lu started), frames %lu, situations %lu, messages %lu, sounds %lu, "
           "videos %lu, alerts %lu, commands %lu, keys bound %lu, cache copies %lu, "
           "deletes %lu\n",
           cover_loads, cover_loaded, cover_frames, cover_main, cover_msg, cover_sounds,
           cover_videos, cover_alerts, cover_commands, cover_keys, cover_copies,
           cover_deletes);
    printf("  dialogs: adventures %lu, lessons %lu, other %lu\n",
           cover_dialog[0], cover_dialog[1], cover_dialog[2]);
    printf("  faults (both sides alike) %lu, ops cut short %lu\n", cover_faults, cover_hangs);
    {
        /* the coverage build's counts: what never ran */
        uint32_t *c = (uint32_t *)(void *)GetProcAddress(ours.module, "advdrv_cover"), j;
        if (c != NULL) {
            printf("  coverage (ours):\n   never run: instructions");
            for (j = 0; j < 0x2c; j++) if (!c[j]) printf(" %#x", j);
            printf("\n   operators");
            for (j = 2; j < 0x32; j++) if (!c[0x40 + j]) printf(" %#x", j);
            printf("\n   read");
            for (j = 0; j < 0xb0; j++) if (has_get[j] && !c[0x100 + j]) printf(" %u", j);
            printf("\n   written");
            for (j = 0; j < 0xb0; j++) if (has_set[j] && !c[0x200 + j]) printf(" %u", j);
            printf("\n   instructions run:");
            for (j = 0; j < 0x2c; j++) printf(" %x:%u", j, c[j]);
            printf("\n   errors:");
            for (j = 0; j < 10; j++) printf(" %u", c[0x300 + j]);
            printf("\n");
        }
    }
    (void)cover_console;
    (void)cover_events;
    (void)cover_errors;
    return differ ? 1 : 0;
}
