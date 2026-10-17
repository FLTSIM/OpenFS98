/* Differential test: retail DEMO.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven through Linkage and the
 * CHAIN callbacks they register, the way MAIN, CONTROLS, ADVDRV and CHAIN
 * drive them, one side after the other over fixture trees rebuilt the same
 * way for each.  What DEMO imports:
 *
 *   host      a stub.  Memory is a bump arena reset before each side, so heap
 *             pointers come out the same; what the host does not zero is
 *             filled with 0xa5.  Files and listings pass through to real
 *             VIDEOS\ and PILOTS\ trees behind transliterations of the host's
 *             path, find and file services and its extension table (read out
 *             of FLTSIM98.EXE).  Temporary names count up per side.  The
 *             dialog engine (+0x10c) is a stand-in for the four dialogs: it
 *             fills each parameter block from the scenario's script and calls
 *             DEMO's callbacks as the real dialogs do.  The voice engine plays
 *             nothing and calls back at once, as it does with no device.
 *   UTIL      retail's, its catalogue (+0x98) real over the trees; the alerts
 *             logged and answered from the script
 *   CHAIN     a logging stub that keeps what DEMO registers; the harness
 *             calls the callbacks itself
 *   CONTROLS  +0x44 logged.  It leaves ecx a value of its own: retail's 0x40
 *             handler sends its second command with whatever ecx CONTROLS
 *             left, and this makes that visible.
 *   MAIN      +0x48/+0x4c (load a situation) logged
 *   NUMERIC, SOUND, WINMSG   logged; NUMERIC's format writes the number
 *
 * Fixtures: the CD's six videos (Videos\FS98.VID and five lessons), some of
 * them mutated, and synthetic videos written for every scenario with every
 * record class: commands, word commands, the airspeed and vertical-speed
 * records, other lengths, situations, every frame tag, short, delta and long
 * keys, 0x8a..0x8e, messages (and "%%s:" sound cues), unknown tags and
 * truncated files.  Some are read-only.
 *
 * A scenario plays a video (+0x28, or the list dialog +0x2c) frame by frame
 * (+0x18, the tick and the pause callbacks, list 0xc commands), records one
 * (sampling GLOBAL as it changes, +0x24 events with the caller's edi aimed at
 * a block whose +0x11a0 the script sets, messages, stopping and the finished
 * dialog's five answers), runs instant replay (list 3, +0x30, +0x3c), plays
 * recordings from memory (+0x20) and interpolates (+0x1c).
 *
 * Compared after every operation: the transcript of every import call, DEMO's
 * whole .data image (code pointers through a pairing learnt as they are first
 * seen, pointers into the image as offsets), the arena, GLOBAL (its replay
 * pointers as offsets), the registers +0x28 and +0x20 return, and at the end
 * of each scenario the trees.
 *
 * Kept clear of, as the real callers do or as retail faults:
 *   - a replay speed (GLOBAL +0x634) of 0, which divides by zero;
 *   - frame intervals that come out 0 while applying (the same);
 *   - +0x24 outside recording, which writes past the buffer, and view
 *     numbers (its event 10) past 3: the tag it writes is 0x80 + the view,
 *     and a wild one makes reviewing the recording recurse without end;
 *   - the replay dialog (+0x30) while a video plays: it refuses only state 1,
 *     so during a review (state 2) the replay starts, swaps the memory
 *     readers in, and a looping video then spins (retail hangs);
 *   - a video started (+0x28, +0x2c) while instant replay runs, which
 *     swaps the memory readers in on every tick (the module's own replay
 *     dialog refuses the other way round);
 *   - +0x20 while a video or instant replay plays, which swaps the memory
 *     readers in under it (a looping video then rewinds to nothing for
 *     ever, and a replay loses its place);
 *   - memory recordings without an end.
 *
 * Build: make demodiff     Run: make demodiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x1165u
#define GLOBAL_SIZE  0x1300u
#define ARENA_SIZE   (8u << 20)
#define TRANSCRIPT_SIZE (16u << 20)
#define MAX_VIDEOS   12
#define MAX_OPS      80

struct import { uint32_t id; void *link; };

struct linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved[3];
    void (STDCALL *play_frame)(void);                       /* +0x18 */
    uint32_t (STDCALL *interpolate)(uint32_t);              /* +0x1c */
    void (STDCALL *parse)(uint32_t, uint32_t, uint32_t, uint32_t); /* +0x20 */
    void (STDCALL *event)(uint32_t, uint32_t);              /* +0x24 */
    uint32_t (STDCALL *play)(uint32_t, uint32_t);           /* +0x28 */
    uint32_t (STDCALL *video_dialog)(void);                 /* +0x2c */
    uint32_t (STDCALL *replay_dialog)(void);                /* +0x30 */
    void (STDCALL *stop)(void);                             /* +0x34 */
    uint32_t (STDCALL *catalogue)(uint32_t, uint32_t);      /* +0x38 */
    uint32_t (STDCALL *position)(int32_t, uint32_t);        /* +0x3c */
};

typedef void (STDCALL *callback_fn)(uint32_t, uint32_t);

#define MAX_PAIRS 128

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    struct linkage *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    callback_fn chain[0x20];        /* the callback DEMO put on each list */
    callback_fn chain_c;            /* list 0xc: two callbacks share it */
    callback_fn chain_c2;
    char *transcript;
    uint32_t length;
};

struct video {
    char path[MAX_PATH];
    uint8_t data[0x20000];
    uint32_t size;
    int readonly;
};

struct scenario {
    uint32_t number;
    uint32_t nvideos;
    struct video v[MAX_VIDEOS];
    uint32_t free_space;
    uint32_t precision;
};

static struct side retail, ours;
static struct side *cur;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t chain[0x24], controls[0x48], mainmod[0x50], numeric[0x20],
    sound[0x34], winmsg[0x20], util_proxy[0xb0];
static uint8_t *util_link;
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static struct scenario sc;
static uint32_t precision_start;    /* each side starts from the scenario's */
static char cd_root[MAX_PATH];
static uint32_t rng;
static uint32_t op_number;
static const char *volatile fault_side = "none";
static volatile uint32_t fault_op;
static uint32_t temp_counter;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs;
static uint32_t caller_edi_block[0x500];     /* +0x11a0 is word 0x468 */
uint32_t controls_ecx = 0x5a5a1234u;

/* the script, consumed in order */
static uint32_t script[64];
static uint32_t script_at;
static uint32_t op_reads, op_alerts;
static HANDLE main_thread;
static volatile DWORD op_started;
static unsigned long cover_hangs;
static void cut_op(void);

static unsigned long cover_ops[16], cover_frames, cover_controls, cover_main,
    cover_msg, cover_cue, cover_alerts, cover_dialog[8], cover_inserts,
    cover_saves, cover_records, cover_parse_end, cover_play_fail;

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
    if (s == NULL)
        put("null");
    else
        put("'%.120s'", s);
}

static int in_image(uint32_t v)
{
    return cur != NULL && v >= (uint32_t)(uintptr_t)cur->image &&
           v < (uint32_t)(uintptr_t)cur->image + IMAGE_SIZE;
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
    else
        put("stack");
}

/* A code pointer, as its number in the order code pointers were first seen
 * on that side: equal numbers are the paired routines.  Learnt once and held
 * to across scenarios. */
static uint32_t npairs_ours;

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
    if (strcmp(ext_text[0x1d], ".VID") != 0) {
        printf("extension table: 0x1d is %s, not .VID\n", ext_text[0x1d]);
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

static void *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;

    if (size > ARENA_SIZE || arena_used + n > ARENA_SIZE) {
        printf("arena exhausted (scenario %u)\n", sc.number);
        exit(2);
    }
    p = arena + arena_used;
    arena_used += n;
    *(uint32_t *)p = size;
    return p + 16;
}

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p = arena_get(size);

    memset(p, zero ? 0 : 0xa5, size);
    put(" alloc(%d,%#x)=", fl, size);
    put_ptr((uint32_t)(uintptr_t)p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    uint8_t *old = *pp, *p = arena_get(size);
    uint32_t n;

    memset(p, 0xa5, size);
    if (old) {
        n = *(uint32_t *)(old - 16);
        memcpy(p, old, n < size ? n : size);
    }
    put(" realloc(%d,%#x)", fl, size);
    *pp = p;
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    put(" free(");
    put_ptr((uint32_t)(uintptr_t)*pp);
    put(")");
    *pp = NULL;
    return 0;
}

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    memmove(d, s, n);
    put(" memmove(%#x)", n);
    return 0;
}

static int32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    char t[64];

    if (id == 0x9002) strcpy(t, "PILOTS");
    else if (id == 0x9007) strcpy(t, "VIDEOS");
    else snprintf(t, sizeof t, "string %#x", id);
    if (len > 0) {
        strncpy(buf, t, (size_t)len - 1);
        buf[len - 1] = 0;
    }
    put(" ls(%#x)", id);
    return (int32_t)strlen(buf);
}

static char *STDCALL h_strcpy(char *d, const char *s)
{
    strcpy(d, s);
    put(" strcpy=");
    put_s(d);
    return d;
}

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

static int32_t STDCALL h_stricmp(const char *a, const char *b)
{
    int32_t r = _strcmpi(a, b);
    return r < 0 ? -1 : r > 0 ? 1 : 0;
}

static uint32_t STDCALL h_strlen(const char *s)
{
    return s ? (uint32_t)strlen(s) : 0;
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

/* 0x40a6c0: an unknown extension is registered, as the host does */
static int32_t STDCALL h_split(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{
    char d[0x100], di[0x100], nm[0x100], e[0x100];

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
    if (*pf == NULL) {
        printf("scenario %u: find-close of a null handle\n", sc.number);
        return 0;
    }
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
    put(" qsort(%u,%#x)", n, w);
    qsort_cmp = cmp;
    qsort(base, n, w, qsort_tramp);
}

uint32_t stub_edx = 0x3d000000u;
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

static int32_t STDCALL h_open(struct file **out, const char *path, int32_t ext, int32_t mode)
{
    static const DWORD access[4] = { GENERIC_READ, GENERIC_READ | GENERIC_WRITE,
                                     GENERIC_WRITE, GENERIC_WRITE };
    char full[0x200];
    struct file *f;

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

static int32_t STDCALL h_create(struct file **out, const char *path, int32_t ext)
{
    char full[0x200];
    struct file *f;

    put(" create(");
    if (resolve(path, ext, full, sizeof full)) { *out = NULL; put(")=-1"); return -1; }
    put_s(full);
    f = new_file();
    f->h = CreateFileA(full, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0x80, NULL);
    if (f->h == INVALID_HANDLE_VALUE) {
        *out = NULL;
        put(")=-1");
        return -1;
    }
    f->open = 1;
    *out = f;
    put(")=");
    put_file(f);
    return 0;
}

static int32_t STDCALL h_close(struct file *f)
{
    put(" close(");
    put_file(f);
    put(")");
    if (f == NULL || f < files || f >= files + 12 || !f->open) return -1;
    CloseHandle(f->h);
    f->open = 0;
    return 0;
}

/* The read leaves ecx and edx values of its own: retail's refill keeps
 * neither, and its message reader counts in ecx across it. */
int32_t STDCALL h_read_body(struct file *f, void *buf, uint32_t n, uint32_t *got);
int32_t STDCALL h_read(struct file *f, void *buf, uint32_t n, uint32_t *got);
__asm__(".text\n\t.p2align 4\n"
        "_h_read@16:\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "call _h_read_body@16\n\t"
        "addl $0x1111, _stub_edx\n\t"
        "movl _stub_edx, %edx\n\t"
        "movl _stub_edx, %ecx\n\t"
        "andl $0x3f, %ecx\n\t"
        "ret $16\n");

int32_t STDCALL h_read_body(struct file *f, void *buf, uint32_t n, uint32_t *got)
{
    DWORD k = 0;
    BOOL ok;

    if (++op_reads > 4096)
        cut_op();

    if (got) *got = 0;
    if (f == NULL || f < files || f >= files + 12 || !f->open) return -1;
    ok = ReadFile(f->h, buf, n, &k, NULL);
    if (got) *got = k;
    put(" read(%#x)=%lu", n, (unsigned long)k);
    return ok ? 0 : -1;
}

/* The write and the alert leave edx a value of their own, as the real ones
 * leave whatever they leave: retail's flush does not keep edx, and an event
 * written across it records that. */
int32_t STDCALL h_write_body(struct file *f, const void *buf, uint32_t n, uint32_t *wrote);
int32_t STDCALL h_write(struct file *f, const void *buf, uint32_t n, uint32_t *wrote);
__asm__(".text\n\t.p2align 4\n"
        "_h_write@16:\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "call _h_write_body@16\n\t"
        "addl $0x1357, _stub_edx\n\t"
        "movl _stub_edx, %edx\n\t"
        "ret $16\n");

int32_t STDCALL h_write_body(struct file *f, const void *buf, uint32_t n, uint32_t *wrote)
{
    DWORD k = 0;
    BOOL ok;

    if (wrote) *wrote = 0;
    if (f == NULL || f < files || f >= files + 12 || !f->open) return -1;
    ok = WriteFile(f->h, buf, n, &k, NULL);
    if (wrote) *wrote = k;
    put(" write(%#x)=%lu/%08x", n, (unsigned long)k, digest(buf, n, 0x811c9dc5u));
    if (getenv("DEMO_IMAGE")) {
        uint32_t j;
        put("[");
        for (j = 0; j < n; j++)
            put("%02x", ((const uint8_t *)buf)[j]);
        put("]");
    }
    return ok ? 0 : -1;
}

static int32_t STDCALL h_seek(struct file *f, uint32_t off, int32_t whence)
{
    put(" seek(%#x,%d)", off, whence);
    if (f == NULL || f < files || f >= files + 12 || !f->open) return -1;
    if (GetFileSize(f->h, NULL) < off) { put("=-1"); return -1; }
    SetFilePointer(f->h, (LONG)off, NULL, (DWORD)whence);
    return 0;
}

static int32_t STDCALL h_tell(struct file *f, uint32_t *pos)
{
    if (f == NULL || f < files || f >= files + 12 || !f->open) return -1;
    *pos = SetFilePointer(f->h, 0, NULL, FILE_CURRENT);
    put(" tell=%#x", *pos);
    return 0;
}

static int32_t STDCALL h_exists(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;

    if (resolve(path, ext, full, sizeof full)) return -1;
    r = GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES ? -1 : 0;
    put(" exists(");
    put_s(full);
    put(")=%d", r);
    return r;
}

static int32_t STDCALL h_get_attr(int32_t *out, const char *path, int32_t ext)
{
    char full[0x200];
    DWORD a;

    if (resolve(path, ext, full, sizeof full)) return -1;
    a = GetFileAttributesA(full);
    put(" getattr(");
    put_s(full);
    if (a == INVALID_FILE_ATTRIBUTES) { put(")=-1"); return -1; }
    *out = 0;
    if (a & FILE_ATTRIBUTE_NORMAL) *out = 1;
    if (a & FILE_ATTRIBUTE_ARCHIVE) *out |= 4;
    if (a & FILE_ATTRIBUTE_READONLY) *out |= 2;
    if (a & FILE_ATTRIBUTE_HIDDEN) *out |= 8;
    if (a & FILE_ATTRIBUTE_DIRECTORY) *out |= 0x10;
    put(")=%#x", *out);
    return 0;
}

static int32_t STDCALL h_delete(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;

    if (resolve(path, ext, full, sizeof full)) return -1;
    r = DeleteFileA(full) ? 0 : -1;
    put(" delete(");
    put_s(full);
    put(")=%d", r);
    return r;
}

static int32_t STDCALL h_move(const char *to, int32_t toext, const char *from, int32_t fromext)
{
    char a[0x200], b[0x200];
    int32_t r;

    if (resolve(to, toext, a, sizeof a) || resolve(from, fromext, b, sizeof b)) return -1;
    r = MoveFileA(b, a) ? 0 : -1;
    put(" move(");
    put_s(b);
    put(",");
    put_s(a);
    put(")=%d", r);
    return r;
}

static int32_t STDCALL h_temp(char *out, const char *dir)
{
    temp_counter++;
    if (dir != NULL && dir[0] != 0)
        snprintf(out, MAX_PATH, "%s\\%X", dir, 0x1000u + temp_counter);
    else
        snprintf(out, MAX_PATH, "%X", 0x1000u + temp_counter);
    put(" temp=");
    put_s(out);
    return 0;
}

/* +0x170: the length of the full path; the harness's paths are relative,
 * so this is the relative one's, the same on both sides. */
static int32_t STDCALL h_path_len(const char *path, int32_t ext, uint32_t *len)
{
    char full[0x200];

    if (resolve(path, ext, full, sizeof full)) return -1;
    *len = (uint32_t)strlen(full);
    if (sc.number % 7 == 0)
        *len = 0x103 + (sc.number / 7) % 3;     /* onto the 0x104 limit */
    put(" pathlen=%u", *len);
    return 0;
}

/* +0x178: nonzero for a name with a character a file name cannot have */
static int32_t STDCALL h_bad_name(const char *name)
{
    int32_t r = strpbrk(name, "\\/:*?\"<>|") != NULL;
    put(" badname(");
    put_s(name);
    put(")=%d", r);
    return r;
}

static int32_t STDCALL h_free_space(const char *path, uint32_t *out)
{
    put(" freespace(%s)", path ? path : "null");
    out[0] = sc.free_space;
    out[1] = 0;
    return 0;
}

static int32_t STDCALL h_post(uint32_t command, uint32_t parameter)
{
    put(" post(%#x,%#x)", command, parameter);
    return 0;
}

static uint32_t STDCALL voice_load(const char *src, int32_t type, uint32_t *h)
{
    put(" voice.load(");
    put_s(src);
    put(",%d)", type);
    if (counting) cover_cue++;
    if (sc.number % 5 == 0) {
        *h = 0;
        put("=-1");
        return 0x90001;
    }
    *h = 0x7001;
    return 0;
}

static uint32_t STDCALL voice_free(uint32_t *h)
{
    put(" voice.free(%#x)", *h);
    *h = 0;
    return 0;
}

static uint32_t STDCALL voice_volume(uint32_t h, int32_t v)
{
    put(" voice.volume(%#x,%d)", h, v);
    return 0;
}

static uint32_t STDCALL voice_play(uint32_t h, int32_t loop, double freq,
                                   void (STDCALL *done)(uint32_t))
{
    put(" voice.play(%#x,%d,%g)", h, loop, freq);
    if (done != NULL)
        done(h);
    return 0;
}

/* ---- the dialog engine ------------------------------------------------------------ */

typedef uint32_t (STDCALL *check1_fn)(uint32_t);
typedef uint32_t (STDCALL *check2_fn)(uint32_t, uint32_t);

static uint8_t edit_item[0xa08];

static int32_t STDCALL h_dialog(uint32_t id, uint8_t *block)
{
    uint32_t a = next_answer(), r;

    put(" dialog(%#x)", id);
    if (counting) cover_dialog[id == 0x1403 ? 0 : id == 0x1404 ? 1 :
                               id == 0x14040001 ? 2 : id == 0x14040002 ? 3 : 4]++;
    switch (id) {
    case 0x1404: {          /* the video list */
        uint32_t count = *(uint32_t *)(block + 0x2c);
        uint8_t *data = *(uint8_t **)(block + 0x30);
        uint32_t i;

        put(" list(%u", count);
        for (i = 0; i < count && i < 20; i++) {
            put(",");
            put_s((char *)data + 0xa08 * i);
            put("/");
            put_s((char *)data + 0xa08 * i + 0x100);
            put("/");
            put_s((char *)data + 0xa08 * i + 0x900);
            put("/%u", *(uint32_t *)(data + 0xa08 * i + 0xa04));
        }
        put(")");
        *(uint32_t *)(block + 0x28) = count ? (a >> 8) % (count + 1) : 0;
        *(uint32_t *)(block + 0x20) = (a >> 4) % 5;
        r = ((check1_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
        put(" check=%d", (int32_t)r);
        if (r != 0) {
            /* the real dialog does not close on a failed check: cancel */
            *(uint32_t *)(block + 0x20) = 1;
            *(uint32_t *)(block + 0x28) = 0;
            r = ((check1_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
            put(" check=%d", (int32_t)r);
            if (r != 0 || (a & 0x3000000) == 0)
                return 0;
        }
        if ((a & 0xf) == 1 && count) {
            r = ((check1_fn)*(uint32_t *)(block + 8))((uint32_t)(uintptr_t)block);
            put(" delete=%d", (int32_t)r);
            count = *(uint32_t *)(block + 0x2c);
        }
        if ((a & 0xf) == 2 && count) {
            uint32_t pick = *(uint32_t *)(block + 0x28) % count;
            memcpy(edit_item, data + 0xa08 * pick, 0xa08);
            if (a & 0x10000) {
                edit_item[0] = 0;
                snprintf((char *)edit_item + 0x100, 0x400, "New description %u", a & 0xff);
            } else {
                snprintf((char *)edit_item, 0x100, (a & 0x20000) ? "bad/name%u" : "RENAMED%u",
                         a & 0x3);
            }
            r = ((check2_fn)*(uint32_t *)(block + 0xc))((uint32_t)(uintptr_t)block,
                                                       (uint32_t)(uintptr_t)edit_item);
            put(" edit=%d", (int32_t)r);
        }
        *(uint32_t *)(block + 0x10) = (a & 0xf) == 3 || (a & 0xf) >= 8;
        *(uint32_t *)(block + 0x14) = (a & 0xf) == 4;
        *(uint32_t *)(block + 0x24) = (a >> 12) & 1;
        if (*(uint32_t *)(block + 0x2c))
            *(uint32_t *)(block + 0x28) %= *(uint32_t *)(block + 0x2c);
        return 0;
    }
    case 0x1403: {          /* instant replay */
        uint32_t have = *(uint32_t *)(global + 0x630);

        *(uint32_t *)(block + 0x14) = (a >> 4) % 60;
        *(uint32_t *)(block + 0x30) = 1 + (a >> 12) % 0x120;
        *(uint32_t *)(block + 0x4c) = (a >> 20) % 3;
        r = ((check1_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
        put(" check=%d,%d,%d,%d", (int32_t)r, *(uint32_t *)(block + 0x10),
            *(uint32_t *)(block + 0x2c), *(uint32_t *)(block + 0x48));
        if (r != 0) {
            /* the dialog will not close until the values pass */
            *(uint32_t *)(block + 0x14) = have ? 1 + (a >> 4) % have : 1;
            *(uint32_t *)(block + 0x30) = 0x14 + (a >> 12) % 0xec;
            *(uint32_t *)(block + 0x4c) = (a >> 20) & 1;
            r = ((check1_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
            put(" recheck=%d", (int32_t)r);
        }
        *(uint32_t *)block = r == 0 && (a & 3) != 0;
        return (a & 0xf) == 0xf ? 5 : 0;
    }
    case 0x14040001: {      /* the recording finished */
        char *name = *(char **)(block + 0x1c);
        char *desc = *(char **)(block + 0x30);
        uint32_t which = a % 7;

        snprintf(name, 0x100, which == 5 ? "bad?name" : "REC%u", (a >> 8) % 4);
        snprintf(desc, 0x100, "Recorded in scenario %u", sc.number);
        r = ((check1_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
        put(" check=%d,%#x", (int32_t)r, *(uint32_t *)(block + 0x18));
        if (r != 0 && (which == 2 || which == 5)) {
            /* save refused: the user picks another name, or goes on */
            snprintf(name, 0x100, "SAVED%u", sc.number % 1000 + (a >> 20) % 7);
            r = ((check1_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
            put(" recheck=%d,%#x", (int32_t)r, *(uint32_t *)(block + 0x18));
            if (r != 0)
                which = 3;
        }
        *(uint32_t *)(block + 0x10) = which == 0;
        *(uint32_t *)(block + 0x14) = which == 1;
        *(uint32_t *)(block + 0x08) = which == 2 || which == 5;
        *(uint32_t *)(block + 0x0c) = which == 3;
        *(uint32_t *)(block + 0x00) = which != 4;
        if (counting && (which == 2 || which == 5)) cover_saves++;
        return 0;
    }
    case 0x14040002: {      /* a message to put in */
        static const char *const texts[] = {
            "Watch the airspeed", "%%s:beep", "", "A message that is just about as long as it can be ok"
        };
        snprintf((char *)block + 8, 0x38, "%s", texts[a % 4]);
        r = ((check1_fn)*(uint32_t *)(block + 4))((uint32_t)(uintptr_t)block);
        *(uint32_t *)block = (a & 0x10) == 0;
        put(" ask=%d", (int32_t)r);
        return 0;
    }
    default:
        return 0;
    }
}

/* ---- the other modules -------------------------------------------------------------- */

static const char *list_name(uint32_t list)
{
    static char b[16];
    snprintf(b, sizeof b, "%#x", list);
    return b;
}

static int32_t STDCALL chain_add(uint32_t list, callback_fn cb)
{
    put(" chain.add(%s,code%u)", list_name(list), pair_code((uint32_t)(uintptr_t)cb) & 0xffff);
    if (list == 0xc) {
        if (cur->chain_c == NULL) cur->chain_c = cb;
        else cur->chain_c2 = cb;
    } else if (list < 0x20) {
        cur->chain[list] = cb;
    }
    return 0;
}

static int32_t STDCALL chain_remove(uint32_t list, callback_fn cb)
{
    put(" chain.remove(%s)", list_name(list));
    if (list == 0xc) {
        if (cur->chain_c == cb) cur->chain_c = cur->chain_c2, cur->chain_c2 = NULL;
        else if (cur->chain_c2 == cb) cur->chain_c2 = NULL;
    } else if (list < 0x20 && cur->chain[list] == cb) {
        cur->chain[list] = NULL;
    }
    return 0;
}

static int32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    if (++op_alerts > 2048)
        cut_op();
    put(" chain.send(%#x,%#x,%#x)", list, a, b);
    return 0;
}

void STDCALL controls_do_body(uint32_t command, uint32_t value);
void STDCALL controls_do_body(uint32_t command, uint32_t value)
{
    /* The 0x60 handler reads its value past its table into the next, a
     * table of code pointers: retail sends one of its own addresses.  That
     * cannot match, and is logged through the pairing. */
    if (cur != NULL && value >= (uint32_t)(uintptr_t)cur->lo &&
        value < (uint32_t)(uintptr_t)cur->hi)
        put(" controls(%#x,code%u)", command, pair_code(value) & 0xffff);
    else
        put(" controls(%#x,%#x)", command, value);
    if (counting) cover_controls++;
    controls_ecx = controls_ecx * 0x41c64e6du + 12345u + command;
}
void STDCALL controls_do(uint32_t command, uint32_t value);
__asm__(".text\n\t.p2align 4\n"
        "_controls_do@8:\n\t"
        "pushl 8(%esp)\n\t"
        "pushl 8(%esp)\n\t"
        "call _controls_do_body@8\n\t"
        "movl _controls_ecx, %ecx\n\t"
        "ret $8\n");

static uint32_t STDCALL main_load(uint32_t memory)
{
    put(" main.load(");
    put_ptr(memory);
    put(")");
    if (counting) cover_main++;
    return 0;
}

static uint32_t STDCALL main_load_file(struct file *f, uint32_t length)
{
    put(" main.loadfile(");
    put_file(f);
    put(",%#x)", length);
    if (counting) cover_main++;
    return 0;
}

static char *STDCALL num_format(const uint32_t *value, uint32_t format, char *out)
{
    sprintf(out, "%u", *value);
    put(" format(%#x)=%s", format, out);
    return out;
}

static uint32_t STDCALL sound_set(uint32_t what, uint32_t value)
{
    put(" sound.set(%u,%u)", what, value);
    return 0;
}

static uint32_t STDCALL sound_volume(uint32_t which)
{
    put(" sound.volume(%u)", which);
    return 7000 + which;
}

static uint32_t STDCALL winmsg_show(const char *text, uint32_t slot, uint32_t time, uint32_t mode)
{
    put(" winmsg(");
    put_s(text);
    put(",%u,%#x,%u)", slot, time, mode);
    if (counting) cover_msg++;
    return 0;
}

uint32_t STDCALL u_alert_body(uint32_t flags, uint32_t tag, uint32_t p1, uint32_t p2,
                              uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines);
uint32_t STDCALL u_alert(uint32_t flags, uint32_t tag, uint32_t p1, uint32_t p2,
                         uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines);
__asm__(".text\n\t.p2align 4\n"
        "_u_alert@32:\n\t"
        "pushl 32(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "call _u_alert_body@32\n\t"
        "addl $0x2468, _stub_edx\n\t"
        "movl _stub_edx, %edx\n\t"
        "ret $32\n");

uint32_t STDCALL u_alert_body(uint32_t flags, uint32_t tag, uint32_t p1, uint32_t p2,
                              uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines)
{
    uint32_t a = flags == 0xc0 ? ((next_answer() & 1) ? 0x40 : 0x80) : 0;

    if (++op_alerts > 2048)
        cut_op();

    put(" alert(%#x,%#x,", flags, tag);
    if (p1 > 0x10000) put_s((const char *)(uintptr_t)p1); else put("%#x", p1);
    put(",%#x,%#x,%#x,%#x,%u)=%#x", p2, p3, p4, p5, lines, a);
    if (counting) cover_alerts++;
    return a;
}

#define SET_SLOT(table, offset, fn) (*(void **)((table) + (offset)) = (void *)(fn))

static void build_stubs(void)
{
    memset(host, 0, sizeof host);
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x03c, h_post);
    SET_SLOT(host, 0x10c, h_dialog);
    SET_SLOT(host, 0x134, h_get_attr);
    SET_SLOT(host, 0x13c, h_close);
    SET_SLOT(host, 0x148, h_create);
    SET_SLOT(host, 0x14c, h_delete);
    SET_SLOT(host, 0x150, h_free_space);
    SET_SLOT(host, 0x158, h_exists);
    SET_SLOT(host, 0x15c, h_find_close);
    SET_SLOT(host, 0x160, h_find_next);
    SET_SLOT(host, 0x164, h_find_first);
    SET_SLOT(host, 0x16c, h_move);
    SET_SLOT(host, 0x170, h_path_len);
    SET_SLOT(host, 0x174, h_temp);
    SET_SLOT(host, 0x178, h_bad_name);
    SET_SLOT(host, 0x17c, h_open);
    SET_SLOT(host, 0x184, h_join);
    SET_SLOT(host, 0x188, h_split);
    SET_SLOT(host, 0x190, h_tell);
    SET_SLOT(host, 0x194, h_seek);
    SET_SLOT(host, 0x19c, h_read);
    SET_SLOT(host, 0x1b0, h_write);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x210, h_realloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x21c, h_memmove);
    SET_SLOT(host, 0x22c, h_qsort);
    SET_SLOT(host, 0x2a0, h_loadstring);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b8, h_strlen);
    SET_SLOT(host, 0x2bc, h_strcmp);
    SET_SLOT(host, 0x2c0, h_stricmp);
    SET_SLOT(host, 0x2d4, voice_load);
    SET_SLOT(host, 0x2dc, voice_free);
    SET_SLOT(host, 0x2e0, voice_play);
    SET_SLOT(host, 0x2ec, voice_volume);
    SET_SLOT(host, 0x2f8, h_list_add);
    SET_SLOT(host, 0x300, h_list_remove);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    SET_SLOT(chain, 0x20, chain_send);
    *(uint32_t *)controls = 16;
    SET_SLOT(controls, 0x44, controls_do);
    *(uint32_t *)mainmod = 3;
    SET_SLOT(mainmod, 0x48, main_load);
    SET_SLOT(mainmod, 0x4c, main_load_file);
    *(uint32_t *)numeric = 17;
    SET_SLOT(numeric, 0x18, num_format);
    *(uint32_t *)sound = 39;
    SET_SLOT(sound, 0x24, sound_set);
    SET_SLOT(sound, 0x30, sound_volume);
    *(uint32_t *)winmsg = 22;
    SET_SLOT(winmsg, 0x18, winmsg_show);
}

static void load_util(const char *path)
{
    HMODULE m = LoadLibraryA(path);
    struct import *im;

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
    s->link = (struct linkage *)GetProcAddress(s->module, "Linkage");
    if (s->imports == NULL || s->link == NULL || s->link->id != 7) {
        printf("%s: not DEMO\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if ((uint8_t *)s->link != s->image + 0x50) {
        printf("%s: Linkage is not ImportTable+0x50\n", path);
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
        case 16: im->link = controls; break;
        case 17: im->link = numeric; break;
        case 20: im->link = util_proxy; break;
        case 22: im->link = winmsg; break;
        case 39: im->link = sound; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* ---- fixtures ------------------------------------------------------------------------- */

static void put16be(uint8_t *p, uint32_t *n, uint16_t v) { p[(*n)++] = (uint8_t)(v >> 8); p[(*n)++] = (uint8_t)v; }
static void put16le(uint8_t *p, uint32_t *n, uint16_t v) { p[(*n)++] = (uint8_t)v; p[(*n)++] = (uint8_t)(v >> 8); }
static void put32le(uint8_t *p, uint32_t *n, uint32_t v) { put16le(p, n, (uint16_t)v); put16le(p, n, (uint16_t)(v >> 16)); }

/* A synthetic video: the header and title, then frames and records. */
static void make_video(struct video *v, uint32_t k)
{
    uint8_t *p = v->data;
    uint32_t n = 0, frames = 2 + rnd_n(30), f, i;
    int32_t lat = (int32_t)rnd_n(4000000), alt = (int32_t)rnd_n(3000) << 16;
    uint32_t lon = rnd();

    put16be(p, &n, 0x4001);
    put16le(p, &n, 0x21);
    snprintf((char *)p + n, 0x1f, "Synthetic %u", k);
    n += 0x1f;
    put16be(p, &n, 0x4002);
    put16le(p, &n, 0x102);
    snprintf((char *)p + n, 0x100, "A video made for scenario %u", sc.number);
    n += 0x100;
    put16be(p, &n, 0x2014);
    put16le(p, &n, 0x600);
    if (rnd_n(4) == 0) {        /* a situation */
        uint32_t len = 4 + 2 + rnd_n(40);
        put16be(p, &n, 0x6001);
        put32le(p, &n, len);
        for (i = 0; i < len - 6; i++) p[n++] = (uint8_t)rnd();
    }
    for (f = 0; f < frames && n < sizeof v->data - 0x200; f++) {
        uint32_t recs = rnd_n(5), r;
        for (r = 0; r < recs; r++) {
            uint32_t kind = rnd_n(20);
            if (kind < 6) {             /* a command, value in the nibble */
                put16be(p, &n, (uint16_t)(rnd_n(0x19) << 4 | rnd_n(16)));
            } else if (kind < 9) {      /* a word command */
                put16be(p, &n, (uint16_t)(0x2001 + rnd_n(0x18)));
                put16le(p, &n, (uint16_t)rnd());
            } else if (kind == 9) {
                put16be(p, &n, 0x4003); put16le(p, &n, 6); put32le(p, &n, rnd_n(0x5dc00));
            } else if (kind == 10) {
                put16be(p, &n, 0x4004); put16le(p, &n, 6); put32le(p, &n, rnd());
            } else if (kind == 11) {    /* another length record */
                uint32_t len = 2 + rnd_n(8);
                put16be(p, &n, (uint16_t)(0x4005 + rnd_n(0x1000)));
                put16le(p, &n, (uint16_t)(len + 2));
                for (i = 0; i < len; i++) p[n++] = (uint8_t)rnd();
            } else if (kind == 12) {    /* a message, or a sound */
                const char *t = rnd_n(3) ? "Turn left now" : "%%s:SOUNDX";
                p[n++] = 0x84;
                memcpy(p + n, t, strlen(t) + 1);
                n += (uint32_t)strlen(t) + 1;
            } else if (kind == 13) {
                p[n++] = (uint8_t)(0x8b + rnd_n(4));   /* 8b 8c 8d 8e */
                if (p[n - 1] == 0x8d || p[n - 1] == 0x8e) { }
            } else if (kind == 14) {
                p[n++] = 0x82;
                put32le(p, &n, (uint32_t)lat); put32le(p, &n, (uint32_t)alt); put32le(p, &n, lon);
                put16le(p, &n, (uint16_t)rnd()); put16le(p, &n, (uint16_t)rnd()); put16le(p, &n, (uint16_t)rnd());
            } else if (kind == 15) {
                p[n++] = 0x83;
                for (i = 0; i < 6; i++) p[n++] = (uint8_t)rnd();
            } else if (kind == 16) {    /* a dword-length record */
                put16be(p, &n, (uint16_t)(0x6002 + rnd_n(0x1000)));
                put32le(p, &n, 6);
                put16le(p, &n, (uint16_t)rnd());
            } else if (kind == 17 && rnd_n(8) == 0) {
                p[n++] = 0x85;          /* not a tag */
            } else if (kind == 18 && rnd_n(4) == 0) {
                p[n++] = 0x8a; p[n++] = (uint8_t)rnd_n(6);
            }
        }
        lat += (int32_t)rnd_n(2000) - 1000;
        lon += rnd_n(20000);
        alt += (int32_t)rnd_n(0x20000) - 0x10000;
        switch (rnd_n(8)) {
        case 0: case 1: case 2:         /* a long key frame */
            p[n++] = (uint8_t)(0x98 + rnd_n(4));     /* 98..9b */
            p[n++] = 0x86;
            put16le(p, &n, (uint16_t)rnd()); put32le(p, &n, (uint32_t)lat);
            put16le(p, &n, (uint16_t)rnd()); put32le(p, &n, lon);
            put16le(p, &n, (uint16_t)rnd()); put32le(p, &n, (uint32_t)alt);
            put32le(p, &n, rnd()); put32le(p, &n, rnd()); put32le(p, &n, rnd());
            p[n++] = (uint8_t)f;
            break;
        case 3: case 4:                 /* a short key frame */
            p[n++] = rnd_n(2) ? 0x90 : 0x91;
            p[n++] = 0x82;
            put32le(p, &n, (uint32_t)lat); put32le(p, &n, (uint32_t)alt); put32le(p, &n, lon);
            put16le(p, &n, (uint16_t)rnd()); put16le(p, &n, (uint16_t)rnd()); put16le(p, &n, (uint16_t)rnd());
            break;
        case 5:                         /* a delta frame */
            p[n++] = rnd_n(2) ? 0x94 : 0x95;
            p[n++] = 0x83;
            for (i = 0; i < 6; i++) p[n++] = (uint8_t)rnd();
            break;
        case 6:
            p[n++] = (uint8_t)(rnd_n(3) == 0 ? 0x88 : 0x80 + rnd_n(2));
            p[n++] = (uint8_t)rnd();
            break;
        default:
            p[n++] = (uint8_t)(0x92 + rnd_n(2));
            break;
        }
    }
    if (rnd_n(6) != 0)
        p[n++] = 0x89;
    v->size = n;
}

static void load_cd_video(struct video *v, const char *rel)
{
    char path[MAX_PATH];
    FILE *f;

    snprintf(path, sizeof path, "%s\\%s", cd_root, rel);
    f = fopen(path, "rb");
    if (!f) { printf("cannot read %s\n", path); exit(2); }
    v->size = (uint32_t)fread(v->data, 1, sizeof v->data, f);
    fclose(f);
    if (rnd_n(3) == 0) {        /* mutated */
        uint32_t i, m = 1 + rnd_n(6);
        for (i = 0; i < m; i++)
            v->data[0x130 + rnd_n(v->size - 0x130)] ^= (uint8_t)(1u << rnd_n(8));
    }
}

static void make_scenario(uint32_t number)
{
    static const char *const cd[] = {
        "fs98\\Videos\\FS98.VID", "fs98\\Lessons\\videos\\c01_intr.vid",
        "fs98\\Lessons\\videos\\c02_nlnd.vid", "fs98\\Lessons\\videos\\c03_wlnd.vid",
        "fs98\\Lessons\\videos\\c05_mca.vid", "fs98\\Lessons\\videos\\c06_stll.vid"
    };
    uint32_t i;

    rng = 0x9e3779b9u ^ (number * 0x85ebca6bu);
    rnd();
    memset(&sc, 0, sizeof sc);
    sc.number = number;
    sc.nvideos = 2 + rnd_n(MAX_VIDEOS - 2);
    for (i = 0; i < sc.nvideos; i++) {
        struct video *v = &sc.v[i];
        if (i < 3 && rnd_n(2)) {
            load_cd_video(v, cd[rnd_n(6)]);
            snprintf(v->path, sizeof v->path, "VIDEOS\\CD%u.VID", i);
        } else {
            make_video(v, i);
            snprintf(v->path, sizeof v->path, "%s\\VID%u.VID", rnd_n(2) ? "PILOTS" : "VIDEOS", i);
        }
        v->readonly = rnd_n(4) == 0;
    }
    sc.free_space = rnd_n(10) == 0 ? rnd_n(0x2000) : 0x100000 + rnd();
    sc.precision = rnd_n(3);
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
            if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
                continue;
            snprintf(path, sizeof path, "%s\\%s", dir, d.cFileName);
            SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(path);
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
}

/* Handles a side left open: retail's deinit closes the video playing only
 * through the reader table, and after a memory recording that table's close
 * is the memory one, so the file stays open. */
static uint32_t leaked;

static void close_leftovers(void)
{
    int i;

    for (i = 0; i < 12; i++)
        if (files[i].open) {
            CloseHandle(files[i].h);
            files[i].open = 0;
            leaked++;
        }
    for (i = 0; i < 8; i++)
        if (finds[i].open) {
            FindClose(finds[i].h);
            finds[i].open = 0;
        }
}

static void build_trees(void)
{
    uint32_t i;
    FILE *f;

    CreateDirectoryA("VIDEOS", NULL);
    CreateDirectoryA("PILOTS", NULL);
    remove_tree("VIDEOS");
    remove_tree("PILOTS");
    for (i = 0; i < sc.nvideos; i++) {
        f = fopen(sc.v[i].path, "wb");
        if (f == NULL) {
            printf("scenario %u: cannot write %s\n", sc.number, sc.v[i].path);
            exit(2);
        }
        fwrite(sc.v[i].data, 1, sc.v[i].size, f);
        fclose(f);
        if (sc.v[i].readonly)
            SetFileAttributesA(sc.v[i].path, FILE_ATTRIBUTE_READONLY);
    }
}

static int name_cmp(const void *a, const void *b)
{
    return _stricmp((const char *)a, (const char *)b);
}

static void put_tree(const char *dir)
{
    static char names[256][MAX_PATH];
    char pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;
    uint32_t n = 0, i;

    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (strcmp(d.cFileName, ".") && strcmp(d.cFileName, "..") && n < 256)
            snprintf(names[n++], MAX_PATH, "%s", d.cFileName);
    } while (FindNextFileA(h, &d));
    FindClose(h);
    qsort(names, n, MAX_PATH, name_cmp);
    for (i = 0; i < n; i++) {
        static uint8_t buf[0x20000];
        FILE *f;
        size_t got;

        snprintf(path, sizeof path, "%s\\%s", dir, names[i]);
        f = fopen(path, "rb");
        got = f ? fread(buf, 1, sizeof buf, f) : 0;
        if (f) fclose(f);
        put(" %s:%#x/%08x", path, (uint32_t)got, digest(buf, (uint32_t)got, 0x811c9dc5u));
    }
}

/* ---- driving ------------------------------------------------------------------------- */

static void set_precision(uint32_t p)
{
    static const uint16_t cw[3] = { 0x027f, 0x037f, 0x007f };
    uint16_t c = cw[p % 3];
    __asm__ volatile("fldcw %0" : : "m"(c));
}

/* +0x24 with edi aimed at the harness's block, as CONTROLS' happens to be */
void STDCALL call_with_edi(void *fn, uint32_t a, uint32_t b, void *edi);
__asm__(".text\n\t.p2align 4\n"
        "_call_with_edi@16:\n\t"
        "pushl %edi\n\t"
        "movl 20(%esp), %edi\n\t"
        "pushl 16(%esp)\n\t"
        "pushl 16(%esp)\n\t"
        "call *16(%esp)\n\t"
        "popl %edi\n\t"
        "ret $16\n");

static void call_event(uint32_t which, uint32_t value)
{
    call_with_edi((void *)cur->link->event, which, value, caller_edi_block);
}

/* A fault inside DEMO is an outcome: logged, and the op abandoned.  Both
 * sides must fault at the same place for the transcripts to agree. */
static void *fault_jb[5];
static volatile int in_demo;
static int trace_exceptions;
static unsigned long cover_faults;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

/* A misparsed length makes DEMO skip gigabytes 0x200 bytes at a time, and a
 * record that will not parse, or a frame that does not catch the clock up,
 * makes it step a second at a time to the clock (alerting, or telling CHAIN
 * list 0x14 each frame): both sides alike, and only slow.  Past a fixed count
 * of reads, alerts or sends in one op it is cut, at the same count on both
 * sides. */
static void cut_op(void)
{
    if (!in_demo)
        return;
    put(" HANG");
    if (counting)
        cover_hangs++;
    __builtin_longjmp(fault_jb, 1);
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;
    uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;

    if (trace_exceptions) {
        /* no CRT here: the fault may be inside it, holding its lock */
        static char line[4096];
        DWORD w;
        uint32_t *sp = (uint32_t *)(uintptr_t)e->ContextRecord->Esp, k;
        int n = wsprintfA(line, "exception %08lx at %08lx (in_demo %d) esp %08lx\r\n", code,
                          (DWORD)(uintptr_t)ip, in_demo, e->ContextRecord->Esp);
        for (k = 0; k < 128 && n < 3900; k++)
            if (sp[k] >= 0x401000 && sp[k] < 0x480000)
                n += wsprintfA(line + n, "  [esp+%x] harness %08x\r\n", k * 4, sp[k]);
            else if (cur && sp[k] >= (uint32_t)(uintptr_t)cur->lo && sp[k] < (uint32_t)(uintptr_t)cur->hi)
                n += wsprintfA(line + n, "  [esp+%x] demo .text+%x\r\n", k * 4,
                               sp[k] - (uint32_t)(uintptr_t)cur->lo - 0x1000);
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), line, (DWORD)n, &w, NULL);
    }
    if (code == EXCEPTION_STACK_OVERFLOW) {
        static const char text[] = "stack overflow: DEMO recursed without end\r\n";
        DWORD w;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, sizeof text - 1, &w, NULL);
        ExitProcess(6);
    }
    if (!in_demo || cur == NULL || ip < cur->lo || ip >= cur->hi)
        return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW)
        put(" FAULT(div)");
    else if (code == EXCEPTION_ACCESS_VIOLATION)
        put(" FAULT(av)");
    else
        return EXCEPTION_CONTINUE_SEARCH;
    if (counting)
        cover_faults++;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void put_state(void)
{
    uint8_t img[IMAGE_SIZE];
    uint8_t g[GLOBAL_SIZE];
    static const uint16_t code_slots[] = {
        0x54, 0x58, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84, 0x88, 0x8c,
        0xe84, 0xed4
    };
    static const uint16_t image_slots[] = { 0xea4, 0xea8, 0xec0, 0xec4, 0xeec, 0xf00, 0x2bc };
    static const uint16_t g_slots[] = { 0x618, 0x620, 0x624, 0x744, 0x754 };
    uint32_t i, base = (uint32_t)(uintptr_t)cur->image;

    memcpy(img, cur->image, IMAGE_SIZE);
    for (i = 0; i < sizeof code_slots / 2; i++)
        *(uint32_t *)(img + code_slots[i]) = pair_code(*(uint32_t *)(img + code_slots[i]));
    for (i = 0x610; i < 0x638; i += 4)
        *(uint32_t *)(img + i) = pair_code(*(uint32_t *)(img + i));
    for (i = 0x63c; i < 0x760; i += 4)
        *(uint32_t *)(img + i) = pair_code(*(uint32_t *)(img + i));
    for (i = 0; i < sizeof image_slots / 2; i++) {
        uint32_t v = *(uint32_t *)(img + image_slots[i]);
        if (v >= base && v < base + IMAGE_SIZE)
            *(uint32_t *)(img + image_slots[i]) = 0xd0000000u | (v - base);
    }
    memcpy(g, global, GLOBAL_SIZE);
    for (i = 0; i < sizeof g_slots / 2; i++) {
        uint32_t v = *(uint32_t *)(g + g_slots[i]);
        if (v >= base && v < base + IMAGE_SIZE + 0x10)
            *(uint32_t *)(g + g_slots[i]) = 0xd0000000u | (v - base);
    }
    put("\n  state img=%08x arena=%#x/%08x G=%08x",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena, arena_used, 0x811c9dc5u), digest(g, GLOBAL_SIZE, 0x811c9dc5u));
    if (getenv("DEMO_IMAGE")) {
        uint32_t fill = *(uint32_t *)(img + 0x4cc);
        put("\n  pending %u:", fill);
        for (i = 0; i < fill && i < 0x200; i++)
            put(" %02x", img[0x2c4 + i]);
        put("\n  gwhole");
        for (i = 0; i + 4 <= GLOBAL_SIZE; i += 4)
            if (*(uint32_t *)(g + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(g + i));
        put("\n  whole");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(img + i));
    }
    put("\n  image");
    for (i = 0x290; i < 0x2c4; i += 4)
        put(" %08x", *(uint32_t *)(img + i));
    put(" |");
    for (i = 0x4c4; i < 0x4f8; i += 4)
        put(" %08x", *(uint32_t *)(img + i));
    put(" |");
    for (i = 0x530; i < 0x640; i += 4)
        put(" %08x", *(uint32_t *)(img + i));
    put("\n  G 55c:");
    for (i = 0x55c; i < 0x584; i += 4)
        put(" %08x", *(uint32_t *)(g + i));
    put(" replay %x %x %x %x %x %x %x",
        *(uint32_t *)(g + 0x614), *(uint32_t *)(g + 0x618), *(uint32_t *)(g + 0x620),
        *(uint32_t *)(g + 0x624), *(uint32_t *)(g + 0x628), *(uint32_t *)(g + 0x62c),
        *(uint32_t *)(g + 0x630));
    put(" 2b8=%08x 2c8=%08x 37c=%04x 36c=%02x 74c=%x 750=%x 758=%x 760=%x 75c=%x 18=%x",
        *(uint32_t *)(g + 0x2b8), *(uint32_t *)(g + 0x2c8), *(uint16_t *)(g + 0x37c),
        g[0x36c], *(uint32_t *)(g + 0x74c), *(uint32_t *)(g + 0x750),
        *(uint32_t *)(g + 0x758), *(uint32_t *)(g + 0x760), *(uint32_t *)(g + 0x75c),
        *(uint16_t *)(g + 0x18));
}

static void tick(void)
{
    *(uint16_t *)(global + 0x2aa) = (uint16_t)(rnd_n(5) == 0 ? rnd() : 0x7ff + rnd_n(0x800));
    if (cur->chain[1]) cur->chain[1](rnd(), rnd());
    if (cur->chain[2]) cur->chain[2](rnd(), rnd());
}

static void steer_global(void)
{
    uint32_t k = rnd_n(12);

    switch (k) {
    case 0: *(uint16_t *)(global + 0xbb2) = (uint16_t)rnd(); break;
    case 1: *(uint16_t *)(global + 0xbb6) = (uint16_t)rnd(); break;
    case 2: *(uint16_t *)(global + 0xbba) = (uint16_t)rnd(); break;
    case 3: *(uint16_t *)(global + 0xbc0) = (uint16_t)rnd(); break;
    case 4: *(uint16_t *)(global + 0x88c) = (uint16_t)rnd(); break;
    case 5: global[0x36c] = (uint8_t)rnd_n(3); break;
    case 6: *(uint16_t *)(global + 0xc00) = (uint16_t)rnd_n(3); break;
    case 7: global[0x366] = (uint8_t)rnd_n(2); break;
    case 8: *(uint32_t *)(global + 0x278) = rnd_n(2); break;
    case 9: *(uint32_t *)(global + 0x2b8) = rnd_n(0x50000); break;
    case 10: *(uint32_t *)(global + 0x564) += rnd_n(3000); *(uint32_t *)(global + 0x56c) += rnd(); break;
    default: *(uint16_t *)(global + 0x5dc) = (uint16_t)rnd_n(2); global[0x36e] = (uint8_t)rnd(); break;
    }
}

static void run_side(struct side *s)
{
    uint32_t i, nops;

    cur = s;
    fault_side = s->name;
    fault_op = 0;
    s->length = 0;
    memset(s->chain, 0, sizeof s->chain);
    s->chain_c = s->chain_c2 = NULL;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    memset(arena, 0, 0x100000);
    memset(global, 0, sizeof global);
    *(uint32_t *)(global + 0x634) = 100;     /* the replay speed, percent */
    *(uint32_t *)(global + 0x260) = 2;
    *(uint32_t *)(global + 0x564) = 1000000;
    *(uint32_t *)(global + 0x20) = 0x123456;
    ext_count = 0x25;
    temp_counter = 0;
    controls_ecx = 0x5a5a1234u;
    stub_edx = 0x3d000000u;
    memset(caller_edi_block, 0, sizeof caller_edi_block);
    build_trees();
    rng = 0x51ed2701u ^ (sc.number * 0x27d4eb2fu);
    rnd();
    for (i = 0; i < 64; i++)
        script[i] = rnd();
    script_at = 0;
    sc.precision = precision_start;
    set_precision(sc.precision);

    op_number = 0;
    put("init:");
    if (getenv("DEMO_TRACE")) { printf("%s init\n", s->name); fflush(stdout); }
    s->link->init();
    if (getenv("DEMO_TRACE")) { printf("%s init done\n", s->name); fflush(stdout); }
    put_state();
    nops = 6 + rnd_n(MAX_OPS - 6);
    for (op_number = 1; op_number <= nops; op_number++) {
        uint32_t k = rnd_n(100);
        uint32_t r;

        fault_side = s->name;
        fault_op = op_number;
        in_demo = 0;
        op_reads = op_alerts = 0;
        op_started = GetTickCount();
        if (__builtin_setjmp(fault_jb)) {
            in_demo = 0;
            set_precision(sc.precision);
            put_state();
            continue;
        }
        in_demo = 1;
        if (getenv("DEMO_TRACE")) {
            printf("%s scenario %u op %u k %u\n", s->name, sc.number, op_number, k);
            fflush(stdout);
        }

        if (k < 10 && *(uint32_t *)(global + 0x628) == 0) {
            struct video *v = &sc.v[rnd_n(sc.nvideos)];
            char name[MAX_PATH];
            snprintf(name, sizeof name, "%.*s", (int)(strlen(v->path) - 4), v->path);
            put("\nop %u: play(%s)", op_number, name);
            if (counting) cover_ops[0]++;
            r = s->link->play((uint32_t)(uintptr_t)name, rnd_n(2));
            put(" =%#x", r);
        } else if (k < 40) {
            uint32_t frames = 1 + rnd_n(12), f;
            put("\nop %u: frames(%u)", op_number, frames);
            if (counting) cover_ops[1]++;
            for (f = 0; f < frames; f++) {
                tick();
                if (getenv("DEMO_IMAGE")) { put("\n tick"); put_state(); }
                if (*(uint32_t *)(global + 0x758) != 0)
                    s->link->play_frame();
                if (getenv("DEMO_IMAGE")) put("\n played");
                if (getenv("DEMO_IMAGE")) put_state();
                if (s->chain[3]) s->chain[3](0, 0);
                if (getenv("DEMO_IMAGE")) put_state();
                if (counting) cover_frames++;
            }
        } else if (k < 46) {
            static const uint32_t commands[] = {
                0x10001, 0x10033, 0x1002b, 0x10007, 0x10008, 0x10010, 0x10019,
                0x100c4, 0x12345
            };
            uint32_t c = commands[rnd_n(9)], taken = rnd_n(4) == 0;
            put("\nop %u: command(%#x,%u)", op_number, c, taken);
            if (counting) cover_ops[2]++;
            if (s->chain_c) s->chain_c(c, (uint32_t)(uintptr_t)&taken);
            if (s->chain_c2) s->chain_c2(c, (uint32_t)(uintptr_t)&taken);
            put(" taken=%u", taken);
            tick();
        } else if (k < 50 && *(uint32_t *)(global + 0x628) == 0) {
            put("\nop %u: videos", op_number);
            if (counting) cover_ops[3]++;
            r = s->link->video_dialog();
            put(" =%#x", r);
        } else if (k < 58 && *(uint32_t *)(global + 0x628) == 0) {
            put("\nop %u: record", op_number);
            if (counting) cover_ops[4]++;
            script[script_at] = (script[script_at] & ~0xfu) | 4;
            r = s->link->video_dialog();
            put(" =%#x", r);
        } else if (k < 68) {
            uint32_t ticks = 1 + rnd_n(20), t;
            put("\nop %u: sample(%u)", op_number, ticks);
            if (counting) cover_ops[5]++;
            for (t = 0; t < ticks; t++) {
                steer_global();
                if (*(uint32_t *)(global + 0x760) != 0 &&
                    (rnd_n(3) == 0 || *(uint32_t *)(s->image + 0x4cc) >= 0x1f0)) {
                    uint32_t which = rnd_n(0x16), value = rnd();

                    /* the view event (10) writes tag 0x80 + value: CONTROLS
                     * sends a view number.  A wild one is an unknown tag, and
                     * reviewing the recording then recurses until the stack
                     * runs out, in retail as here. */
                    if (which == 10)
                        value = rnd_n(4);
                    if (which == 17)
                        value = rnd_n(6);       /* the five modes, and one past */
                    /* onto a flush: a word event whose tag fills the buffer
                     * (retail then writes the host's edx as the value) */
                    if (*(uint32_t *)(s->image + 0x4cc) >= 0x1f0 && rnd_n(2))
                        which = 1;
                    if (*(uint32_t *)(s->image + 0x4cc) == 0x1fe ||
                        *(uint32_t *)(s->image + 0x4cc) == 0x1ff)
                        which = 1 + rnd_n(3);
                    caller_edi_block[0x468] = rnd_n(2);
                    call_event(which, value);
                    if (counting) cover_records++;
                }
                tick();
                if (s->chain[3]) s->chain[3](0, 0);
            }
        } else if (k < 71) {
            put("\nop %u: stop", op_number);
            if (counting) cover_ops[6]++;
            s->link->stop();
            tick();
            tick();
        } else if (k < 75) {
            put("\nop %u: pause", op_number);
            if (counting) cover_ops[7]++;
            tick();
            if (s->chain[9]) s->chain[9](0, 0);
            if (s->chain[0xd]) s->chain[0xd](0, 0);
        } else if (k < 82) {
            uint32_t frames = 1 + rnd_n(60), f;
            put("\nop %u: replay frames(%u)", op_number, frames);
            if (counting) cover_ops[8]++;
            *(uint32_t *)(global + 0x268) = rnd_n(8) == 0;
            global[0x365] = rnd_n(10) == 0;
            for (f = 0; f < frames; f++) {
                *(uint32_t *)(global + 0x564) += rnd_n(500);
                *(uint32_t *)(global + 0x56c) += rnd_n(100000);
                if (s->chain[3]) s->chain[3](0, 0);
                tick();
            }
            *(uint32_t *)(global + 0x268) = 0;
            global[0x365] = 0;
        } else if (k < 86 && *(uint32_t *)(global + 0x758) == 0) {
            put("\nop %u: replay dialog", op_number);
            if (counting) cover_ops[9]++;
            r = s->link->replay_dialog();
            put(" =%#x", r);
            if (*(uint32_t *)(global + 0x634) == 0 || *(uint32_t *)(global + 0x634) > 0xff)
                *(uint32_t *)(global + 0x634) = 100;
        } else if (k < 89) {
            int32_t index = -(int32_t)rnd_n(55);
            uint8_t out[0x18];
            memset(out, 0xcc, sizeof out);
            r = s->link->position(index, (uint32_t)(uintptr_t)out);
            put("\nop %u: position(%d)=%d %08x%08x %08x%08x %08x%08x", op_number, index, (int32_t)r,
                *(uint32_t *)(out + 0), *(uint32_t *)(out + 4), *(uint32_t *)(out + 8),
                *(uint32_t *)(out + 12), *(uint32_t *)(out + 16), *(uint32_t *)(out + 20));
            if (counting) cover_ops[10]++;
        } else if (k < 93 && *(uint32_t *)(global + 0x758) == 0 &&
                   *(uint32_t *)(global + 0x628) == 0) {
            /* a recording in memory, played through +0x20 -- not while a
             * video plays: that swaps the memory readers in under it, which
             * no caller does */
            struct video *v = &sc.v[rnd_n(sc.nvideos)];
            uint8_t *mem = arena_get(v->size + 0x10);
            uint32_t cursor, done = 0, steps = 0;
            memcpy(mem, v->data, v->size);
            memset(mem + v->size, 0x89, 0x10);
            cursor = (uint32_t)(uintptr_t)mem + 0x12a;
            put("\nop %u: parse", op_number);
            if (counting) cover_ops[11]++;
            *(uint32_t *)(global + 0x74c) = rnd_n(2);
            while (!done && steps++ < 40) {
                s->link->parse((uint32_t)(uintptr_t)&cursor,
                               (uint32_t)(uintptr_t)mem + v->size, steps == 1,
                               (uint32_t)(uintptr_t)&done);
                put(" @%#x/%u", cursor - (uint32_t)(uintptr_t)mem, done);
                if (getenv("DEMO_TRACE")) {
                    printf("  parse step %u cursor %#x done %u size %#x\n", steps,
                           cursor - (uint32_t)(uintptr_t)mem, done, v->size);
                    fflush(stdout);
                }
                tick();
            }
            if (done && counting) cover_parse_end++;
            *(uint32_t *)(global + 0x74c) = 0;
        } else if (k < 96) {
            uint8_t out[0x28];
            memset(out, 0xcc, sizeof out);
            r = s->link->interpolate((uint32_t)(uintptr_t)out);
            put("\nop %u: interpolate", op_number);
            for (i = 0; i < 0x28; i += 4)
                put(" %08x", *(uint32_t *)(out + i));
            if (counting) cover_ops[12]++;
        } else {
            set_precision(sc.precision = rnd_n(3));
            put("\nop %u: precision(%u)", op_number, sc.precision);
            if (counting) cover_ops[13]++;
        }
        in_demo = 0;
        put_state();
    }
    in_demo = 0;
    op_number++;
    put("\ndeinit:");
    if (!__builtin_setjmp(fault_jb)) {
        in_demo = 1;
        s->link->deinit();
    }
    in_demo = 0;
    put_state();
    put("\nopen:");
    for (i = 0; i < 12; i++)
        if (files[i].open)
            put(" file%u", i);
    close_leftovers();
    put("\nfiles:");
    put_tree("VIDEOS");
    put_tree("PILOTS");
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
    printf("scenario %u differs at byte %u\n", sc.number, i);
    from = (uint32_t)(ls - a) > 1500 ? (uint32_t)(ls - a) - 1500 : 0;
    printf("--- before:\n%.*s\n", (int)((uint32_t)(ls - a) - from), a + from);
    from = i - (uint32_t)(ls - a) > 400 ? i - 400 : (uint32_t)(ls - a);
    printf("--- retail:\n...%.900s\n", a + from);
    printf("--- ours:\n...%.900s\n", b + from);
}

/* A spin with no host calls in it: where it is. */

static DWORD WINAPI watchdog(LPVOID unused)
{
    (void)unused;
    for (;;) {
        Sleep(500);
        if (in_demo && op_started && GetTickCount() - op_started > 30000) {
            CONTEXT c;
            struct side *t = strcmp(fault_side, "ours") == 0 ? &ours : &retail;
            uint32_t *sp, k;

            SuspendThread(main_thread);
            c.ContextFlags = CONTEXT_FULL;
            GetThreadContext(main_thread, &c);
            printf("SPIN scenario %u op %u %s eip %08lx (.text+%#lx) esi %08lx edi %08lx eax %08lx ebx %08lx ecx %08lx\n",
                   sc.number, fault_op, fault_side, c.Eip,
                   c.Eip - (DWORD)(uintptr_t)t->lo - 0x1000, c.Esi, c.Edi, c.Eax, c.Ebx, c.Ecx);
            sp = (uint32_t *)(uintptr_t)c.Esp;
            for (k = 0; k < 64; k++)
                if (sp[k] >= (uint32_t)(uintptr_t)t->lo + 0x1000 &&
                    sp[k] < (uint32_t)(uintptr_t)t->lo + 0x6000)
                    printf("  [esp+%#x] .text+%#x\n", k * 4, sp[k] - (uint32_t)(uintptr_t)t->lo - 0x1000);
            {
                FILE *f = fopen("demo_spin.txt", "wb");
                if (f) { fwrite(t->transcript, 1, t->length, f); fclose(f); }
                f = fopen("demo_spin_retail.txt", "wb");
                if (f) { fwrite(retail.transcript, 1, retail.length, f); fclose(f); }
            }
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
            printf("\nFAULT in %s DEMO at .text+%#x\n", s[i]->name,
                   (uint32_t)(at - s[i]->lo - 0x1000));
    {
        CONTEXT *c = e->ContextRecord;
        uint32_t *sp = (uint32_t *)(uintptr_t)c->Esp, k;
        printf("eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx ebp %08lx esp %08lx\n",
               c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        for (k = 0; k < 64; k++) {
            uint32_t v = sp[k];
            for (i = 0; i < 2; i++)
                if (s[i]->module && v >= (uint32_t)(uintptr_t)s[i]->lo + 0x1000 &&
                    v < (uint32_t)(uintptr_t)s[i]->lo + 0x6000)
                    printf("  [esp+%#x] %s .text+%#x\n", k * 4, s[i]->name,
                           v - (uint32_t)(uintptr_t)s[i]->lo - 0x1000);
        }
        printf("true side %s op %u\n", fault_side, fault_op);
        {
            struct side *t = strcmp(fault_side, "ours") == 0 ? &ours : &retail;
            FILE *f = fopen("demo_fault.txt", "wb");
            if (f && t->length < TRANSCRIPT_SIZE) {
                fwrite(t->transcript, 1, t->length, f);
                fclose(f);
            }
            printf("cur=%p retail=%p ours=%p op_number@%p sc@%p script@%p\n", (void *)cur,
                   (void *)&retail, (void *)&ours, (void *)&op_number, (void *)&sc, (void *)script);
        }
    }
    printf("FAULT %#lx at %p, address %p, scenario %u op %u, running %s\n",
           e->ExceptionRecord->ExceptionCode, (void *)at,
           e->ExceptionRecord->NumberParameters > 1
               ? (void *)e->ExceptionRecord->ExceptionInformation[1] : NULL,
           sc.number, op_number, cur ? cur->name : "none");
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
        printf("usage: demodiff N CD-ROOT [FIRST]\n");
        return 2;
    }
    snprintf(cd_root, sizeof cd_root, "%s", argv[2]);
    for (i = 0; cd_root[i]; i++)
        if (cd_root[i] == '/') cd_root[i] = '\\';
    trace_exceptions = getenv("DEMO_EXC") != NULL;
    AddVectoredExceptionHandler(1, on_vectored);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    if (getenv("DEMO_NOFILTER") == NULL)
        SetUnhandledExceptionFilter(on_fault);
    load_ext_table("FLTSIM98_host.exe");
    build_stubs();
    load_util("UTIL_retail.dll");
    load_side(&retail, "DEMO_retail.dll", "retail");
    load_side(&ours, "DEMO_ours.dll", "ours");

    for (i = first; i < first + n; i++) {
        make_scenario(i);
        precision_start = sc.precision;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("DEMO_DUMP") != NULL) {
            FILE *f = fopen("demo_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("demo_ours.txt", "wb");
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
    printf("demodiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops: play %lu, frames %lu, command %lu, videos %lu, record %lu, sample %lu, "
           "stop %lu, pause %lu, replay frames %lu, replay dialog %lu, position %lu, "
           "parse %lu, interpolate %lu, precision %lu\n",
           cover_ops[0], cover_ops[1], cover_ops[2], cover_ops[3], cover_ops[4],
           cover_ops[5], cover_ops[6], cover_ops[7], cover_ops[8], cover_ops[9],
           cover_ops[10], cover_ops[11], cover_ops[12], cover_ops[13]);
    printf("  frames %lu, CONTROLS commands %lu, situations %lu, messages %lu, cues %lu, "
           "alerts %lu, events %lu, saves %lu, memory recordings to the end %lu\n",
           cover_frames, cover_controls, cover_main, cover_msg, cover_cue, cover_alerts,
           cover_records, cover_saves, cover_parse_end);
    printf("  dialogs: replay %lu, list %lu, finished %lu, message %lu, other %lu\n",
           cover_dialog[0], cover_dialog[1], cover_dialog[2], cover_dialog[3], cover_dialog[4]);
    printf("  faults (both sides alike) %lu, ops cut short %lu, handles retail left open %u\n",
           cover_faults, cover_hangs, leaked / 2);
    (void)cover_inserts;
    (void)cover_play_fail;
    return differ ? 1 : 0;
}
