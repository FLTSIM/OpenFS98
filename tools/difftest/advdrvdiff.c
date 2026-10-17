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
 *   KERNEL32  the debug console (AllocConsole, FreeConsole, SetConsoleTitleA,
 *             GetStdHandle, WriteFile), hooked in both import tables and
 *             logged
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
    callback_fn cb[64];
    uint8_t removed[64];
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

    if (cur == NULL || cur->length >= TRANSCRIPT_SIZE - 1024)
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
    if (l->count >= 64) { printf("chain list %u full\n", list); exit(2); }
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

/* ---- the debug console, hooked in both import tables -------------------------------- */

#define CONSOLE_HANDLE ((HANDLE)(uintptr_t)0x6c00)

static BOOL WINAPI k_alloc_console(void)
{
    count_call();
    put(" AllocConsole");
    if (counting) cover_console++;
    return TRUE;
}

static BOOL WINAPI k_free_console(void)
{
    count_call();
    put(" FreeConsole");
    return TRUE;
}

static BOOL WINAPI k_set_title(const char *title)
{
    count_call();
    put(" SetConsoleTitle(");
    put_s(title);
    put(")");
    return TRUE;
}

static HANDLE WINAPI k_get_std(DWORD which)
{
    count_call();
    put(" GetStdHandle(%ld)", (long)which);
    return (HANDLE)((uintptr_t)CONSOLE_HANDLE + (which & 0xff));
}

static BOOL WINAPI k_write(HANDLE h, const void *buf, DWORD n, DWORD *wrote, OVERLAPPED *o)
{
    (void)o;
    count_call();
    put(" WriteFile(%#x,%lu,%.*s)", (uint32_t)(uintptr_t)h, (unsigned long)n,
        (int)(n < 200 ? n : 200), (const char *)buf);
    if (wrote) *wrote = n;
    return h != NULL;
}

static int patch_iat(HMODULE m, const char *dll, const char *fn, void *repl)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)m;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((uint8_t *)m + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)((uint8_t *)m + dir->VirtualAddress);

    for (; d->Name; d++) {
        IMAGE_THUNK_DATA *names, *iat;

        if (_stricmp((char *)m + d->Name, dll))
            continue;
        names = (IMAGE_THUNK_DATA *)((uint8_t *)m +
                (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        iat = (IMAGE_THUNK_DATA *)((uint8_t *)m + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            IMAGE_IMPORT_BY_NAME *ibn;
            DWORD old;

            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;
            ibn = (IMAGE_IMPORT_BY_NAME *)((uint8_t *)m + names->u1.AddressOfData);
            if (strcmp((char *)ibn->Name, fn))
                continue;
            VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old);
            iat->u1.Function = (DWORD)(uintptr_t)repl;
            VirtualProtect(&iat->u1.Function, 4, old, &old);
            return 1;
        }
    }
    return 0;
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
    static const char *const console[] = {
        "AllocConsole", "FreeConsole", "SetConsoleTitleA", "GetStdHandle", "WriteFile"
    };
    static void *const hooks[] = {
        (void *)k_alloc_console, (void *)k_free_console, (void *)k_set_title,
        (void *)k_get_std, (void *)k_write
    };
    uint32_t i;

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
    for (i = 0; i < 5; i++)
        if (!patch_iat(s->module, "KERNEL32.dll", console[i], hooks[i])) {
            printf("%s: no import of %s to hook\n", name, console[i]);
            exit(2);
        }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}
