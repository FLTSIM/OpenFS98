/* Differential test: retail BOOSTER.DLL against the OpenFS98 build,
 * whole-module, worker thread included.
 *
 * Both DLLs are loaded into one process and driven only through Linkage and
 * their two CHAIN callbacks, the way FE and CHAIN drive them.  What BOOSTER
 * imports:
 *
 *   host      a stub.  Memory is a bump allocator over an arena reset before
 *             each side, so heap pointers come out the same on both; what the
 *             host does not zero is filled with 0xa5.  Directory listings and
 *             files pass through to real cache trees, behind
 *             transliterations of the host's path join, split, find and file
 *             services, its extension table read out of FLTSIM98.EXE.  The
 *             free space +0x150 reports is the scenario's to choose.
 *   GLOBAL    one block, reset identically before each side runs
 *   CHAIN     a logging stub; the harness calls the two callbacks itself
 *   UTIL      retail's +0x44/+0x48 called directly; the three alerts logged
 *   FE        +0x70/+0x74 logged
 *   MSVCRT    operator new and delete, redirected (retail's through its import
 *             table, ours through GetProcAddress) to a second arena, reset
 *             the same way, so the queue and its jobs compare too
 *   Win32     the file calls, MessageBoxA, PostThreadMessageA and SetEvent go
 *             through import-table hooks that log them; MessageBoxA answers
 *             from the scenario's script, sometimes repairing what made the
 *             copy fail first.  The file calls themselves are real.
 *
 * The thread is made deterministic by draining it after every operation:
 * every 0x401 posted is counted, every SetEvent it answers with is counted,
 * and the harness waits until they agree.  So the thread is idle in
 * GetMessage whenever the main thread calls in, the frame callback's
 * zero-timeout mutex wait never fails by accident, and it fails on purpose
 * when a helper thread holds the mutex ("busy" frames).
 *
 * Fixtures: four of the CD's boosters (Antarcti, samerica, oceania, canada,
 * whose bstinfo files only have type 1 records), and synthetic boosters
 * written for every scenario with all five record types, missing and
 * read-only source files, pre-cached files (some read-only), files with an
 * unregistered extension or none, BGLs not named as BOOSTER names them, and
 * a subdirectory.  Each side gets the cache trees rebuilt from the same
 * description.  The aircraft is moved inside boxes, onto their edges and onto
 * the reach boundaries (lat and lon), and onto altitude band edges.
 *
 * Kept clear of, as the real callers do or as retail faults:
 *   - +0x1c with no booster added (null ring);
 *   - caches without SCENERY\ and TEXTURE\ (the host's find-close faults on
 *     the null handle a failed find leaves);
 *   - grids with no columns or rows (both divide by zero);
 *   - records shorter than 0x13 bytes (the file loops run 2^31 times).
 *
 * Compared after every operation: the transcript of every import and Win32
 * call, BOOSTER's whole .data image (handles, the critical section and the
 * Linkage code pointers masked), both arenas, GLOBAL and FE's booster
 * records.  Every change BOOSTER makes to the cache trees goes through a
 * logged call; the trees themselves (names, sizes, read-only bits, contents)
 * are compared at the end of each scenario.
 *
 * Build: make boosterdiff     Run: make boosterdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x904u
#define GLOBAL_SIZE  0x1300u
#define ARENA_SIZE   (4u << 20)
#define NEW_SIZE     (4u << 20)
#define MAX_BOOSTERS 3
#define REC_SIZE     0x220
#define INFO_MAX     0x10000
#define TRANSCRIPT_SIZE (24u << 20)
#define MAX_RECORDS  512
#define MAX_FILES    1024
#define MAX_ANSWERS  16
#define MAX_OPS      40

struct import { uint32_t id; void *link; };

struct linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved[3];
    void (STDCALL *add)(uint8_t *rec, uint32_t area);
    void (STDCALL *drop)(uint8_t *rec);
};

typedef void (STDCALL *callback_fn)(uint32_t, uint32_t);
typedef void (STDCALL *convert_fn)(const void *, const void *, void *);

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    struct linkage *link;
    uint8_t *image;
    uint8_t pristine[IMAGE_SIZE];
    callback_fn frame, message;
    char *transcript;
    uint32_t length;
};

/* One file a booster's bstinfo names. */
struct bfile {
    char source[MAX_PATH];      /* relative to the booster's source */
    char cached[64];            /* relative to the cache */
    uint32_t size;
    uint8_t missing, readonly, precached, precached_ro;
};

/* One box of a record or grid cell, for steering. */
struct box {
    int32_t south, north;
    uint32_t west, east, reach;
};

struct booster {
    int real;                   /* index into real_names, or -1 */
    char cache[32];
    char source[MAX_PATH];
    uint16_t quota_kb;
    uint8_t keep;               /* rec +0x21a */
    uint8_t added;              /* in the current side's run */
    uint8_t info[INFO_MAX];
    uint32_t info_len;
    struct bfile files[MAX_FILES];
    uint32_t nfiles;
    struct box boxes[MAX_RECORDS];
    uint32_t nboxes;
    int32_t bands[16][2];
    uint32_t nbands;
    uint32_t junk;              /* which odd files go in the cache */
    uint32_t uniform;           /* every file this size, if nonzero */
};

struct scenario {
    uint32_t number;
    uint32_t nboosters;
    struct booster b[MAX_BOOSTERS];
    uint32_t free_mode;         /* 0 fixed, 1 fails, 2 base less the cache */
    uint64_t free_base;
    uint16_t flags444;
    uint32_t precision;
    int answers[MAX_ANSWERS];   /* MessageBoxA's script; 0 ends it */
};

static const char *const real_names[] = { "Antarcti", "samerica", "oceania", "canada" };

static struct side retail, ours;
static struct side *cur;
static uint8_t host[0x320];
static uint8_t global[GLOBAL_SIZE] __attribute__((aligned(16)));
static uint8_t chain[0x24];
static uint8_t numeric[0x20];
static uint8_t util_proxy[0xb0];
static uint8_t fe[0x80];
static uint8_t *util_link;
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t new_arena[NEW_SIZE] __attribute__((aligned(16)));
static uint32_t new_used;
static uint8_t recs[MAX_BOOSTERS][REC_SIZE];
static struct scenario sc;
static struct scenario sc_start;    /* what each side starts from */
static char cd_root[MAX_PATH];
static CRITICAL_SECTION put_lock;
static volatile LONG posted, handled;
static HANDLE handled_sem;
static uint32_t answer_next;
static uint32_t answers_this_copy;
static int counting;
static uint32_t rng;
static uint32_t op_number;
static int sources_dirty;    /* a side changed the CD trees */

static BOOL (WINAPI *real_SetEvent)(HANDLE);
static BOOL (WINAPI *real_PostThreadMessageA)(DWORD, UINT, WPARAM, LPARAM);

/* coverage */
static unsigned long cover_ops[12], cover_copy, cover_copy_fail, cover_register,
    cover_unregister, cover_delete_bst, cover_host_delete, cover_msgbox,
    cover_cancel, cover_retry_fix, cover_alert_space, cover_busy_skip,
    cover_full, cover_level, cover_level_max, cover_queue_max, cover_jobs, cover_flush,
    cover_types[6], cover_free_fail;

/* ---- utilities ------------------------------------------------------------- */

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

    if (cur == NULL)
        return;
    EnterCriticalSection(&put_lock);
    if (cur->length < TRANSCRIPT_SIZE - 1024) {
        va_start(args, format);
        n = vsnprintf(cur->transcript + cur->length,
                      TRANSCRIPT_SIZE - cur->length, format, args);
        va_end(args);
        if (n > 0)
            cur->length += (uint32_t)n;
    }
    LeaveCriticalSection(&put_lock);
}

static void put_s(const char *s)
{
    if (s == NULL)
        put("null");
    else
        put("'%.200s'", s);
}

static int in_image(const void *p)
{
    return cur != NULL && (const uint8_t *)p >= cur->image &&
           (const uint8_t *)p < cur->image + IMAGE_SIZE;
}

static void put_ptr(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;

    if (p == NULL)
        put("null");
    else if (b >= arena && b < arena + ARENA_SIZE)
        put("arena+%#x", (uint32_t)(b - arena));
    else if (b >= new_arena && b < new_arena + NEW_SIZE)
        put("new+%#x", (uint32_t)(b - new_arena));
    else if (in_image(p))
        put("image+%#x", (uint32_t)(b - cur->image));
    else if (b >= global && b < global + GLOBAL_SIZE)
        put("G+%#x", (uint32_t)(b - global));
    else if (b >= &recs[0][0] && b < &recs[0][0] + sizeof(recs))
        put("rec+%#x", (uint32_t)(b - &recs[0][0]));
    else
        put("stack");
}

/* ---- the host's extension table, read out of FLTSIM98.EXE ----------------- */

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
    if (strcmp(ext_text[1], ".BGL") || strcmp(ext_text[0x24], ".CFG")) {
        printf("extension table: unexpected contents\n");
        exit(2);
    }
}

/* 0x40b350: path + the extension's text into `out`; -1 for an unknown id */
static int resolve(const char *path, int32_t ext, char *out, size_t len)
{
    const char *t;

    if (ext == -2) t = "";
    else if (ext >= 0 && ext < 0x25) t = ext_text[ext];
    else return -1;
    snprintf(out, len, "%s%s", path, t);
    return 0;
}

/* 0x40b410: an extension's id, upper-casing it; nonzero if not registered */
static int ext_lookup(char *ext, int32_t *out)
{
    int k;

    _strupr(ext);
    for (k = 1; k < 0x25; k++)
        if (strcmp(ext, ext_text[k]) == 0) { *out = k; return 0; }
    return 1;
}

/* ---- the stub host -------------------------------------------------------------- */

static void *arena_get(uint8_t *base, uint32_t *used, uint32_t limit, uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;

    if (size > limit || *used + n > limit) {
        printf("arena exhausted (scenario %u)\n", sc.number);
        exit(2);
    }
    p = base + *used;
    *used += n;
    *(uint32_t *)p = size;
    return p + 16;
}

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p = arena_get(arena, &arena_used, ARENA_SIZE, size);

    memset(p, zero ? 0 : 0xa5, size);
    put(" alloc(%d,%#x,%d)=", fl, size, zero);
    put_ptr(p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    uint8_t *old = *pp, *p = arena_get(arena, &arena_used, ARENA_SIZE, size);
    uint32_t n;

    memset(p, 0xa5, size);
    if (old) {
        n = *(uint32_t *)(old - 16);
        memcpy(p, old, n < size ? n : size);
    }
    put(" realloc(%d,%#x,", fl, size);
    put_ptr(old);
    put(")=");
    put_ptr(p);
    *pp = p;
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

static void STDCALL h_busy_on(void)
{
    put(" busy(1)");
}

static void STDCALL h_busy_off(void)
{
    put(" busy(0)");
}

static int32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    const char *t = "";

    if (id == 0x9003) t = "SCENERY";
    else if (id == 0x9006) t = "TEXTURE";
    else if (id == 0xf429) t = "Error copying file from this booster source directory";
    else if (id == 3) t = "Flight Simulator";
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

static char *STDCALL h_strcat(char *d, const char *s)
{
    strcat(d, s);
    put(" strcat=");
    put_s(d);
    return d;
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

/* 0x40a6c0, for what BOOSTER asks of it: the name only */
static int32_t STDCALL h_split(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{
    char e[0x100];

    if (!path) return -1;
    if (drive || dir || ext) { printf("split: unexpected arguments\n"); exit(2); }
    if (path[0] == 0)
        name[0] = 0;
    else
        _splitpath(path, NULL, NULL, name, e);
    put(" split=");
    put_s(name);
    return 0;
}

struct find { HANDLE h; WIN32_FIND_DATAA d; int more, open; };
struct find_entry { char name[0x104]; int32_t type; uint32_t size; int32_t writable; };
static struct find finds[4];

/* 0x40a9a0 */
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
    for (i = 0; i < 4; i++)
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
    put(")=find%d", (int)(f - finds));
    return 0;
}

/* 0x40ab60 -- in FindFirstFileA's order, the filesystem's: each side's trees
 * are built and changed by the same steps, so both see the same order. */
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

/* 0x40ac90 -- dereferences the handle unconditionally, as the host does: a
 * failed find leaves it null, and the host faults.  The fixtures keep clear
 * of that; it is reported if it ever happens. */
static int32_t STDCALL h_find_close(struct find **pf)
{
    if (*pf == NULL) {
        put(" findclose(null)=FAULT");
        printf("scenario %u: find-close of a null handle (the host faults)\n", sc.number);
        return 0;
    }
    FindClose((*pf)->h);
    (*pf)->open = 0;
    *pf = NULL;
    put(" findclose");
    return 0;
}

struct file { HANDLE h; int open; };
static struct file files[8];

/* 0x40a000 */
static int32_t STDCALL h_open(struct file **out, const char *path, int32_t ext, int32_t mode)
{
    static const DWORD access[4] = { GENERIC_READ, GENERIC_READ | GENERIC_WRITE,
                                     GENERIC_WRITE, GENERIC_WRITE };
    char full[0x200];
    struct file *f = NULL;
    int i;

    put(" open(");
    if (resolve(path, ext, full, sizeof full)) { *out = NULL; put("ext %d)=-1", ext); return -1; }
    put_s(full);
    if (mode < 0 || mode > 3) { *out = NULL; put(",%d)=-1", mode); return -1; }
    for (i = 0; i < 8; i++)
        if (!files[i].open) { f = &files[i]; break; }
    if (f == NULL) { printf("out of files\n"); exit(2); }
    f->h = CreateFileA(full, access[mode], 0, NULL, OPEN_EXISTING, 0x80, NULL);
    if (f->h == INVALID_HANDLE_VALUE) {
        *out = NULL;
        put(")=-1");
        return -1;
    }
    f->open = 1;
    *out = f;
    put(")=file%d", i);
    return 0;
}

/* 0x40a140 */
static int32_t STDCALL h_close(struct file *f)
{
    int32_t r;

    if (!f) return -1;
    r = CloseHandle(f->h) ? 0 : -1;
    f->open = 0;
    put(" close");
    return r;
}

/* 0x40a190 */
static int32_t STDCALL h_read(struct file *f, void *buf, uint32_t n, uint32_t *got)
{
    DWORD k = 0;
    BOOL ok;

    if (got) *got = 0;
    if (!f) return -1;
    ok = ReadFile(f->h, buf, n, &k, NULL);
    if (got) *got = k;
    put(" read(%#x)=%lu/%08x", n, (unsigned long)k, digest(buf, k, 0x811c9dc5u));
    return ok ? 0 : -1;
}

/* 0x40a400 */
static int32_t STDCALL h_size(struct file *f, uint32_t *size)
{
    if (!f) return -1;
    *size = GetFileSize(f->h, NULL);
    put(" size=%#x", *size);
    return 0;
}

/* 0x40acb0 */
static int32_t STDCALL h_delete(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;

    if (resolve(path, ext, full, sizeof full)) { put(" delete(ext %d)=-1", ext); return -1; }
    r = DeleteFileA(full) ? 0 : -1;
    put(" delete(");
    put_s(full);
    put(")=%d", r);
    if (counting) cover_host_delete++;
    return r;
}

static uint64_t tree_bytes(const char *dir)
{
    char pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;
    uint64_t total = 0;

    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    h = FindFirstFileA(pattern, &d);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
            continue;
        snprintf(path, sizeof path, "%s\\%s", dir, d.cFileName);
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            total += tree_bytes(path);
        else
            total += d.nFileSizeLow;
    } while (FindNextFileA(h, &d));
    FindClose(h);
    return total;
}

/* +0x150 -- the drive's free space, as the scenario has it. */
static int32_t STDCALL h_free_space(const char *path, uint32_t *out)
{
    uint64_t v = sc.free_base;
    uint32_t i;

    put(" freespace(");
    put_s(path);
    if (sc.free_mode == 1) {
        put(")=-1");
        if (counting) cover_free_fail++;
        return -1;
    }
    if (sc.free_mode == 2) {
        for (i = 0; i < sc.nboosters; i++) {
            uint64_t used = tree_bytes(sc.b[i].cache);
            v = v > used ? v - used : 0;
        }
    }
    out[0] = (uint32_t)v;
    out[1] = (uint32_t)(v >> 32);
    put(")=%#llx", (unsigned long long)v);
    return 0;
}

/* ---- the other imports ---------------------------------------------------------- */

static const char *callback_name(void *cb)
{
    if (cur == NULL) return "?";
    if ((callback_fn)cb == cur->frame) return "frame";
    if ((callback_fn)cb == cur->message) return "message";
    return "other";
}

static int32_t STDCALL chain_add(uint32_t list, callback_fn cb)
{
    if (list == 3 && cur->frame == NULL) cur->frame = cb;
    if (list == 0x14 && cur->message == NULL) cur->message = cb;
    put(" chain.add(%#x,%s)", list, callback_name((void *)cb));
    return 0;
}

static int32_t STDCALL chain_remove(uint32_t list, callback_fn cb)
{
    put(" chain.remove(%#x,%s)", list, callback_name((void *)cb));
    return 0;
}

static uint32_t STDCALL u_alert(uint32_t flags, uint32_t tag, uint32_t p1, uint32_t p2,
                                uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines)
{
    put(" alert(%#x,%#x,%#x,%#x,%#x,%#x,%#x,%u)", flags, tag, p1, p2, p3, p4, p5, lines);
    if (counting && p1 == 0xf421) cover_alert_space++;
    return 0;
}

static void STDCALL u_alert_open(void **out, uint32_t tag, uint32_t p1, uint32_t p2,
                                 uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines)
{
    put(" alert.open(%#x,%#x,%#x,%#x,%#x,%#x,%u)", tag, p1, p2, p3, p4, p5, lines);
    *out = (void *)0x7a1e7700;
}

static void STDCALL u_alert_close(void **pp)
{
    put(" alert.close(%p)", *pp);
    *pp = NULL;
}

static uint32_t STDCALL fe_register(uint32_t area, const char *name)
{
    put(" fe.register(%u,", area);
    put_ptr(name);
    put(",");
    put_s(name);
    put(")");
    if (counting) cover_register++;
    return 0;
}

static uint32_t STDCALL fe_unregister(uint32_t area, const char *name)
{
    put(" fe.unregister(%u,", area);
    put_ptr(name);
    put(",");
    put_s(name);
    put(")");
    if (counting) {
        if (in_image(name)) cover_unregister++;
        else cover_delete_bst++;
    }
    return 0;
}

/* MSVCRT's operator new and delete, for both sides. */
static CRITICAL_SECTION new_lock;

static void *__cdecl x_new(uint32_t size)
{
    void *p;

    EnterCriticalSection(&new_lock);
    p = arena_get(new_arena, &new_used, NEW_SIZE, size);
    memset(p, 0xcd, size);
    LeaveCriticalSection(&new_lock);
    return p;
}

static void __cdecl x_delete(void *p)
{
    (void)p;
}

/* ---- Win32 hooks -------------------------------------------------------------- */

static void make_file(const char *path, uint32_t size, int readonly);

static BOOL WINAPI x_CopyFileA(LPCSTR from, LPCSTR to, BOOL fail)
{
    BOOL r = CopyFileA(from, to, fail);

    put(" copy(");
    put_s(from);
    put(",");
    put_s(to);
    put(",%d)=%d", fail, r);
    if (counting) { if (r) cover_copy++; else cover_copy_fail++; }
    return r;
}

static BOOL WINAPI x_DeleteFileA(LPCSTR path)
{
    BOOL r = DeleteFileA(path);

    put(" DeleteFile(");
    put_s(path);
    put(")=%d", r);
    return r;
}

static BOOL WINAPI x_SetFileAttributesA(LPCSTR path, DWORD a)
{
    BOOL r = SetFileAttributesA(path, a);

    put(" SetAttr(");
    put_s(path);
    put(",%#lx)=%d", (unsigned long)a, r);
    return r;
}

static DWORD WINAPI x_GetFileAttributesA(LPCSTR path)
{
    DWORD r = GetFileAttributesA(path);

    put(" GetAttr(");
    put_s(path);
    put(")=%s", r == INVALID_FILE_ATTRIBUTES ? "none" :
                (r & FILE_ATTRIBUTE_READONLY) ? "ro" : "rw");
    return r;
}

static HANDLE WINAPI x_CreateFileA(LPCSTR path, DWORD access, DWORD share,
                                   LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                   DWORD flags, HANDLE tmpl)
{
    HANDLE h = CreateFileA(path, access, share, sa, disp, flags, tmpl);

    put(" CreateFile(");
    put_s(path);
    put(",%#lx,%lu,%lu,%#lx)=%s", (unsigned long)access, (unsigned long)share,
        (unsigned long)disp, (unsigned long)flags,
        h == INVALID_HANDLE_VALUE ? "-1" : "ok");
    return h;
}

static DWORD WINAPI x_GetFileSize(HANDLE h, LPDWORD hi)
{
    DWORD r = GetFileSize(h, hi);

    put(" GetFileSize=%#lx", (unsigned long)r);
    return r;
}

/* The scenario's answers: 4 retry, 2 cancel, 0x14 retry once the source is
 * back, 0x24 retry once the cache file is there.  The script running out,
 * or six in one copy, cancels. */
static int WINAPI x_MessageBoxA(HWND w, LPCSTR text, LPCSTR caption, UINT type)
{
    int a = answer_next < MAX_ANSWERS ? sc.answers[answer_next] : 0;
    const char *nl;
    char source[MAX_PATH];
    char dest[MAX_PATH];

    (void)w;
    if (a != 0)
        answer_next++;
    if (a == 0 || ++answers_this_copy > 6)
        a = IDCANCEL;
    put(" msgbox(");
    put_s(text);
    put(",");
    put_s(caption);
    put(",%#x)=", type);
    nl = strchr(text, '\n');
    if (a == 0x14 && nl != NULL) {
        snprintf(source, sizeof source, "%s", nl + 1);
        make_file(source, 0x321, 0);
        sources_dirty = 1;
        put("fixed:");
        if (counting) cover_retry_fix++;
        a = IDRETRY;
    } else if (a == 0x24 && cur != NULL) {
        snprintf(dest, sizeof dest, "%s", (char *)cur->image + 0x368);
        make_file(dest, 0x123, 0);
        put("appeared:");
        if (counting) cover_retry_fix++;
        a = IDRETRY;
    }
    put("%d", a);
    if (counting) {
        cover_msgbox++;
        if (a == IDCANCEL) cover_cancel++;
    }
    return a;
}

static BOOL WINAPI x_PostThreadMessageA(DWORD id, UINT msg, WPARAM w, LPARAM l)
{
    BOOL r = real_PostThreadMessageA(id, msg, w, l);

    if (msg == 0x401 && !r) {
        printf("scenario %u op %u (%s): a 0x401 was lost\n", sc.number, op_number,
               cur ? cur->name : "?");
        exit(3);
    }
    if (msg == 0x401) {
        InterlockedIncrement(&posted);
        if (counting && cur != NULL) {
            uint8_t *queue = *(uint8_t **)(cur->image + 0x8e4);
            if (queue != NULL && *(uint32_t *)(queue + 8) > cover_queue_max)
                cover_queue_max = *(uint32_t *)(queue + 8);
            if (queue != NULL)
                cover_jobs += *(uint32_t *)(queue + 8);
        }
    }
    return r;
}

static BOOL WINAPI x_SetEvent(HANDLE h)
{
    BOOL r = real_SetEvent(h);

    InterlockedIncrement(&handled);
    ReleaseSemaphore(handled_sem, 1, NULL);
    return r;
}

static HMODULE crt;
static FARPROC WINAPI x_GetProcAddress(HMODULE m, LPCSTR name)
{
    if (m == crt && !strcmp(name, "??2@YAPAXI@Z")) return (FARPROC)(void *)x_new;
    if (m == crt && !strcmp(name, "??3@YAXPAX@Z")) return (FARPROC)(void *)x_delete;
    printf("%s: unexpected GetProcAddress(%s)\n", cur ? cur->name : "?", name);
    exit(2);
}

/* ---- loading -------------------------------------------------------------------- */

static int patch_iat(HMODULE m, const char *dll, const char *fn, void *repl, void **orig)
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
            if (orig)
                *orig = (void *)iat->u1.Function;
            iat->u1.Function = (DWORD)(uintptr_t)repl;
            VirtualProtect(&iat->u1.Function, 4, old, &old);
            return 1;
        }
    }
    return 0;
}

static void must_patch(struct side *s, const char *dll, const char *fn, void *repl, void **orig)
{
    if (!patch_iat(s->module, dll, fn, repl, orig)) {
        printf("%s: no import %s!%s to hook\n", s->name, dll, fn);
        exit(2);
    }
}

#define SET_SLOT(table, offset, fn) (*(void **)((table) + (offset)) = (void *)(fn))

static void build_stubs(void)
{
    memset(host, 0, sizeof host);
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x02c, h_busy_on);
    SET_SLOT(host, 0x030, h_busy_off);
    SET_SLOT(host, 0x13c, h_close);
    SET_SLOT(host, 0x14c, h_delete);
    SET_SLOT(host, 0x150, h_free_space);
    SET_SLOT(host, 0x15c, h_find_close);
    SET_SLOT(host, 0x160, h_find_next);
    SET_SLOT(host, 0x164, h_find_first);
    SET_SLOT(host, 0x17c, h_open);
    SET_SLOT(host, 0x184, h_join);
    SET_SLOT(host, 0x188, h_split);
    SET_SLOT(host, 0x19c, h_read);
    SET_SLOT(host, 0x1a4, h_size);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x210, h_realloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x2a0, h_loadstring);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b0, h_strcat);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    *(uint32_t *)numeric = 17;
    *(uint32_t *)fe = 18;
    SET_SLOT(fe, 0x70, fe_register);
    SET_SLOT(fe, 0x74, fe_unregister);
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
    SET_SLOT(util_proxy, 0xa8, u_alert_open);
    SET_SLOT(util_proxy, 0xac, u_alert_close);
}

static void load_side(struct side *s, const char *path, const char *name, int is_retail)
{
    struct import *im;

    memset(s, 0, sizeof *s);
    s->name = name;
    s->transcript = malloc(TRANSCRIPT_SIZE);
    s->module = LoadLibraryA(path);
    if (s->module == NULL) { printf("cannot load %s (%lu)\n", path, GetLastError()); exit(2); }
    s->imports = (struct import *)GetProcAddress(s->module, "ImportTable");
    s->link = (struct linkage *)GetProcAddress(s->module, "Linkage");
    if (s->imports == NULL || s->link == NULL || s->link->id != 34) {
        printf("%s: not BOOSTER\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if ((uint8_t *)s->link != s->image + 0x38) {
        printf("%s: Linkage is not ImportTable+0x38\n", path);
        exit(2);
    }
    for (im = s->imports; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host; break;
        case 2:  im->link = global; break;
        case 4:  im->link = chain; break;
        case 17: im->link = numeric; break;
        case 18: im->link = fe; break;
        case 20: im->link = util_proxy; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    must_patch(s, "KERNEL32.dll", "CopyFileA", (void *)x_CopyFileA, NULL);
    must_patch(s, "KERNEL32.dll", "DeleteFileA", (void *)x_DeleteFileA, NULL);
    must_patch(s, "KERNEL32.dll", "SetFileAttributesA", (void *)x_SetFileAttributesA, NULL);
    must_patch(s, "KERNEL32.dll", "GetFileAttributesA", (void *)x_GetFileAttributesA, NULL);
    must_patch(s, "KERNEL32.dll", "CreateFileA", (void *)x_CreateFileA, NULL);
    must_patch(s, "KERNEL32.dll", "GetFileSize", (void *)x_GetFileSize, NULL);
    must_patch(s, "KERNEL32.dll", "SetEvent", (void *)x_SetEvent, (void **)&real_SetEvent);
    must_patch(s, "USER32.dll", "MessageBoxA", (void *)x_MessageBoxA, NULL);
    must_patch(s, "USER32.dll", "PostThreadMessageA", (void *)x_PostThreadMessageA,
               (void **)&real_PostThreadMessageA);
    if (is_retail) {
        must_patch(s, "MSVCRT.dll", "??2@YAPAXI@Z", (void *)x_new, NULL);
        must_patch(s, "MSVCRT.dll", "??3@YAXPAX@Z", (void *)x_delete, NULL);
    } else {
        must_patch(s, "KERNEL32.dll", "GetProcAddress", (void *)x_GetProcAddress, NULL);
    }
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* ---- fixtures -------------------------------------------------------------------- */

static void make_dirs_for(const char *path)
{
    char p[MAX_PATH];
    char *s;

    snprintf(p, sizeof p, "%s", path);
    for (s = p; *s; s++) {
        if (*s == '\\' && s != p && s[-1] != ':') {
            *s = 0;
            CreateDirectoryA(p, NULL);
            *s = '\\';
        }
    }
}

static void make_file(const char *path, uint32_t size, int readonly)
{
    static uint8_t buf[0x10000];
    HANDLE h;
    DWORD w;
    uint32_t i, seed = digest(path, (uint32_t)strlen(path), 0x811c9dc5u);

    make_dirs_for(path);
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("cannot write %s\n", path); exit(2); }
    while (size > 0) {
        uint32_t n = size < sizeof buf ? size : sizeof buf;
        for (i = 0; i < n; i++) {
            seed = seed * 1103515245u + 12345u;
            buf[i] = (uint8_t)(seed >> 16);
        }
        WriteFile(h, buf, n, &w, NULL);
        size -= n;
    }
    CloseHandle(h);
    if (readonly)
        SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY);
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
            if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                remove_tree(path);
            else {
                SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(path);
            }
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

static int name_cmp(const void *a, const void *b)
{
    return _stricmp((const char *)a, (const char *)b);
}

/* The cache trees, as one digest: names, sizes and read-only bits, and with
 * `list` the contents too, each file listed. */
static uint32_t tree_digest(const char *dir, uint32_t h, int list)
{
    static char names[2048][MAX_PATH];
    static uint32_t found_sizes[2048];
    static DWORD found_attrs[2048];
    DWORD *attrs;
    uint32_t *sizes;
    char pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE f;
    uint32_t n = 0, i, base;

    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    f = FindFirstFileA(pattern, &d);
    if (f == INVALID_HANDLE_VALUE)
        return digest("none", 4, h);
    base = 0;
    do {
        if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, ".."))
            continue;
        if (n < 2048) {
            found_sizes[n] = d.nFileSizeLow;
            found_attrs[n] = d.dwFileAttributes;
            snprintf(names[n++], MAX_PATH, "%s", d.cFileName);
        }
    } while (FindNextFileA(f, &d));
    FindClose(f);
    (void)base;
    {
        char (*mine)[MAX_PATH] = malloc((size_t)n * (MAX_PATH + 8) + 8);
        uint32_t j;

        sizes = (uint32_t *)(mine + n);
        attrs = (DWORD *)(sizes + n);
        /* sort the names, carrying each size along */
        for (j = 0; j < n; j++)
            snprintf(mine[j], MAX_PATH, "%s", names[j]);
        qsort(mine, n, MAX_PATH, name_cmp);
        for (j = 0; j < n; j++) {
            uint32_t k;
            for (k = 0; k < n; k++)
                if (!strcmp(mine[j], names[k])) {
                    sizes[j] = found_sizes[k];
                    attrs[j] = found_attrs[k];
                    break;
                }
        }
        for (i = 0; i < n; i++) {
            DWORD a;

            snprintf(path, sizeof path, "%s\\%s", dir, mine[i]);
            a = attrs[i];
            h = digest(mine[i], (uint32_t)strlen(mine[i]), h);
            if (a & FILE_ATTRIBUTE_DIRECTORY) {
                h = tree_digest(path, h, list);
            } else {
                uint32_t size = 0, cd = 0x811c9dc5u;
                static uint8_t buf[0x10000];
                DWORD got;
                HANDLE fh;

                if (list) {
                    fh = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                                     OPEN_EXISTING, 0, NULL);
                    if (fh != INVALID_HANDLE_VALUE) {
                        size = GetFileSize(fh, NULL);
                        while (ReadFile(fh, buf, sizeof buf, &got, NULL) && got)
                            cd = digest(buf, got, cd);
                        CloseHandle(fh);
                    }
                    put(" %s:%#x/%08x%s", path, size, cd,
                        (a & FILE_ATTRIBUTE_READONLY) ? "ro" : "");
                } else {
                    size = sizes[i];
                }
                h = digest(&size, 4, h);
                h = digest(&cd, 4, h);
                h ^= (a & FILE_ATTRIBUTE_READONLY) ? 0x5a5a : 0;
            }
        }
        free(mine);
    }
    return h;
}

static void put24_lat(uint8_t *p, int32_t v)
{
    uint32_t u = (uint32_t)(v >> 1);

    p[0] = (uint8_t)u;
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
}

static void put24_lon(uint8_t *p, uint32_t v)
{
    uint32_t u = v >> 8;

    p[0] = (uint8_t)u;
    p[1] = (uint8_t)(u >> 8);
    p[2] = (uint8_t)(u >> 16);
}

static int32_t get_lat(const uint8_t *r, int o)
{
    return (int32_t)((((uint32_t)(int32_t)(int8_t)r[o + 2] << 16) |
                      *(const uint16_t *)(r + o)) << 1);
}

static uint32_t get_lon(const uint8_t *r, int o)
{
    return (((uint32_t)r[o + 2] << 16) | *(const uint16_t *)(r + o)) << 8;
}

static void add_file(struct booster *b, int grid, uint16_t kind, uint32_t index)
{
    struct bfile *f;
    uint32_t type = kind >> 12, number = kind & 0xfff;
    const char *ext = type == 0 ? ".TXR" : type == 1 ? ".BGL" : type == 2 ? ".PAL" : ".HAZ";
    const char *dir = type == 1 ? "SCENERY" : "TEXTURE";
    uint32_t i;

    if (b->nfiles >= MAX_FILES)
        return;
    f = &b->files[b->nfiles];
    memset(f, 0, sizeof *f);
    snprintf(f->source, sizeof f->source, "%s\\%s%.3u\\%c%.3u%.3u%s", dir,
             grid ? "GRID" : "FILES", number, grid ? 'G' : 'F', number, index & 0xffff, ext);
    snprintf(f->cached, sizeof f->cached, "%s\\%c%.3u%.3u%s", dir, grid ? 'G' : 'F',
             number, index & 0xffff, ext);
    for (i = 0; i < b->nfiles; i++)
        if (!strcmp(b->files[i].source, f->source))
            return;
    f->size = b->uniform ? b->uniform : 1 + rnd_n(rnd_n(4) == 0 ? 0x6000 : 0x900);
    f->missing = rnd_n(25) == 0;
    f->readonly = rnd_n(6) == 0;
    f->precached = rnd_n(8) == 0;
    f->precached_ro = f->precached && rnd_n(4) == 0;
    b->nfiles++;
}

/* A synthetic bstinfo: a header and records of every type around a centre. */
static void make_synthetic(struct booster *b, uint32_t k)
{
    uint8_t *p = b->info;
    uint32_t o, nrec, r, j;
    int32_t lat0 = (int32_t)(rnd_n(18000000) - 9000000) & ~1;
    uint32_t lon0 = rnd();

    memset(p, 0, 0x18);
    *(uint16_t *)(p + 0x14) = 0x18;
    *(uint16_t *)(p + 0x16) = (uint16_t)(rnd_n(5) == 0 ? 0 : rnd_n(4) == 0 ? rnd() : 20 + rnd_n(400));
    o = 0x18;
    nrec = 1 + rnd_n(9);
    b->nfiles = 0;
    b->uniform = rnd_n(4) == 0 ? 1 + rnd_n(0x900) : 0;
    for (r = 0; r < nrec && o < INFO_MAX - 0x200; r++) {
        uint32_t t = rnd_n(12);
        uint8_t *q = p + o;
        int32_t south, north;
        uint32_t west, east, half_lat = 200 + rnd_n(40000), half_lon = 20000 + rnd_n(6000000);
        uint16_t kind;

        if (t == 0) {
            int32_t lo = (int32_t)(rnd_n(4000) - 1000) << 8, hi = lo + (int32_t)(rnd_n(8000) << 8);

            if (rnd_n(4) == 0) { lo = (int32_t)0x80000000; }
            q[0] = 3;
            *(uint16_t *)(q + 1) = 0xb;
            *(int32_t *)(q + 3) = lo;
            *(int32_t *)(q + 7) = hi;
            o += 0xb;
            continue;
        }
        south = lat0 + (int32_t)(rnd_n(80000)) - 40000 - (int32_t)half_lat;
        north = south + 2 * (int32_t)half_lat;
        west = lon0 + rnd_n(8000000) - 4000000 - half_lon;
        east = west + 2 * half_lon;
        put24_lat(q + 3, north);
        put24_lat(q + 6, south);
        put24_lon(q + 9, east);
        put24_lon(q + 0xc, west);
        /* Retail UTIL +0x44 faults (#DE) once the reach, scaled by up to
         * 1.925, reaches the latitude circle: 0x1800 << 8 clears it to
         * +-9,400,000. */
        *(uint16_t *)(q + 0xf) = (uint16_t)(rnd_n(8) == 0 ? rnd_n(0x1800) : 4 + rnd_n(300));
        kind = (uint16_t)((rnd_n(4) << 12) | rnd_n(1000));
        if (t <= 6) {                       /* a list, type 1 or 5 */
            uint32_t n = rnd_n(7);
            q[0] = t <= 4 ? 1 : 5;
            if (q[0] == 5) kind = (uint16_t)((1u << 12) | (kind & 0xfff));
            *(uint16_t *)(q + 0x11) = kind;
            for (j = 0; j < n; j++) {
                uint16_t index = (uint16_t)rnd_n(1000);

                if (rnd_n(10) == 0) index |= 0x2000;     /* marked cached */
                *(uint16_t *)(q + 0x13 + 2 * j) = index;
                if (q[0] == 1) {
                    add_file(b, 0, kind, index & 0xfff);
                } else {
                    add_file(b, 0, (uint16_t)(kind | 0x1000), index & 0xfff);
                    add_file(b, 0, (uint16_t)(kind & 0xfff), index & 0xfff);
                }
            }
            *(uint16_t *)(q + 1) = (uint16_t)(0x13 + 2 * n + (rnd_n(5) == 0));
            o += *(uint16_t *)(q + 1);
        } else {                            /* a grid, type 2 or 4 */
            uint32_t cols = 1 + rnd_n(5), rows = 1 + rnd_n(5), cells = cols * rows, bytes;
            q[0] = t <= 9 ? 2 : 4;
            if (q[0] == 4) kind = (uint16_t)((1u << 12) | (kind & 0xfff));
            if (rnd_n(6) == 0) {
                /* north below south: the row step divides negative */
                put24_lat(q + 3, south);
                put24_lat(q + 6, north);
            }
            *(uint16_t *)(q + 0x11) = kind;
            *(uint16_t *)(q + 0x13) = (uint16_t)cols;
            *(uint16_t *)(q + 0x15) = (uint16_t)rows;
            bytes = (cells + 1) / 2;
            for (j = 0; j < bytes; j++) {
                uint8_t v = 0;
                uint32_t c;

                for (c = 0; c < 2; c++) {
                    uint32_t cell = 2 * j + c;
                    uint8_t nib = 0;

                    if (cell < cells && rnd_n(4) != 0) {
                        nib = 1;
                        if (rnd_n(8) == 0) nib |= 2;
                        if (q[0] == 2) {
                            add_file(b, 1, kind, cell);
                        } else {
                            add_file(b, 1, (uint16_t)(kind | 0x1000), cell);
                            add_file(b, 1, (uint16_t)(kind & 0xfff), cell);
                        }
                    }
                    v |= (uint8_t)(nib << (4 * c));
                }
                q[0x17 + j] = v;
            }
            *(uint16_t *)(q + 1) = (uint16_t)(0x17 + bytes);
            o += 0x17 + bytes;
        }
    }
    if (rnd_n(10) == 0)
        p[o++] = (uint8_t)(6 + rnd_n(250));   /* an unknown type ends the scan */
    p[o++] = 0;
    b->info_len = o;
    snprintf(b->source, sizeof b->source, "bw\\s%u", k);
    b->junk = rnd();
}

static void index_boxes(struct booster *b)
{
    const uint8_t *p = b->info;
    uint32_t o = *(const uint16_t *)(p + 0x14);

    b->nboxes = 0;
    b->nbands = 0;
    while (o + 0x17 < b->info_len && p[o] != 0) {
        const uint8_t *r = p + o;
        uint16_t len = *(const uint16_t *)(r + 1);

        if (r[0] == 3 && b->nbands < 16) {
            b->bands[b->nbands][0] = *(const int32_t *)(r + 3);
            b->bands[b->nbands][1] = *(const int32_t *)(r + 7);
            b->nbands++;
        } else if ((r[0] == 1 || r[0] == 5) && b->nboxes < MAX_RECORDS) {
            struct box *x = &b->boxes[b->nboxes++];
            x->north = get_lat(r, 3);
            x->south = get_lat(r, 6);
            x->east = get_lon(r, 9);
            x->west = get_lon(r, 0xc);
            x->reach = (uint32_t)*(const uint16_t *)(r + 0xf) << 8;
        } else if (r[0] == 2 || r[0] == 4) {
            uint16_t cols = *(const uint16_t *)(r + 0x13);
            int16_t rows = *(const int16_t *)(r + 0x15);
            int32_t north = get_lat(r, 3), south = get_lat(r, 6);
            uint32_t west = get_lon(r, 0xc), east = get_lon(r, 9);
            int32_t lat_step = (north - south) / rows;
            uint32_t lon_step = (east - west) / cols;
            int i, j;

            for (i = 0; i < rows; i++)
                for (j = 0; j < cols && b->nboxes < MAX_RECORDS; j++) {
                    struct box *x = &b->boxes[b->nboxes++];
                    x->north = north - i * lat_step;
                    x->south = x->north - lat_step;
                    x->west = west + (uint32_t)j * lon_step;
                    x->east = x->west + lon_step;
                    x->reach = (uint32_t)*(const uint16_t *)(r + 0xf) << 8;
                }
        }
        if (len == 0)
            break;
        o += len;
    }
}

static void load_real(struct booster *b, uint32_t which)
{
    char path[MAX_PATH];
    FILE *f;

    snprintf(path, sizeof path, "%s\\Boosters\\%s\\bstinfo", cd_root, real_names[which]);
    f = fopen(path, "rb");
    if (!f) { printf("cannot read %s\n", path); exit(2); }
    b->info_len = (uint32_t)fread(b->info, 1, INFO_MAX, f);
    fclose(f);
    if (rnd_n(3) == 0)
        *(uint16_t *)(b->info + 0x16) = (uint16_t)rnd_n(0x3000);
    snprintf(b->source, sizeof b->source, "%s\\Boosters\\%s", cd_root, real_names[which]);
    b->nfiles = 0;
    b->uniform = 0;
    b->junk = rnd();
}

static void make_scenario(uint32_t number)
{
    static const uint64_t frees[] = {
        0, 0x3ffff, 0x40000, 0x7ffff, 0x80000, 0x80001, 0x200000, 0x100000000ull
    };
    uint32_t i;

    rng = 0x9e3779b9u ^ (number * 0x85ebca6bu);
    rnd();
    memset(&sc, 0, sizeof sc);
    sc.number = number;
    sc.nboosters = 1 + rnd_n(MAX_BOOSTERS);
    for (i = 0; i < sc.nboosters; i++) {
        struct booster *b = &sc.b[i];

        snprintf(b->cache, sizeof b->cache, "bw\\c%u", i);
        b->real = rnd_n(4) == 0 ? (int)rnd_n(4) : -1;
        if (b->real >= 0)
            load_real(b, (uint32_t)b->real);
        else
            make_synthetic(b, i);
        index_boxes(b);
        b->quota_kb = (uint16_t)(rnd_n(6) == 0 ? rnd_n(4) : rnd_n(5) == 0 ? rnd() : 8 + rnd_n(400));
        b->keep = (uint8_t)(rnd_n(5) == 0 ? 2 + rnd_n(3) : rnd_n(2));
    }
    if (number % 16 == 15) {
        /* The long scenarios: one booster, emptied when taken back, whose
         * small cache fills and stays full, so its reach shrinks a level
         * every 60 scans. */
        sc.nboosters = 1;
        sc.b[0].keep = 0;
        sc.b[0].quota_kb = (uint16_t)(4 + rnd_n(40));
    }
    sc.free_mode = rnd_n(6) == 0 ? 1 : rnd_n(2) ? 2 : 0;
    sc.free_base = rnd_n(3) == 0 ? frees[rnd_n(8)] : 0x40000 + rnd_n(0x800000);
    sc.flags444 = (uint16_t)(rnd_n(4) == 0 ? rnd() : rnd_n(4) * 0x10);
    if (number % 64 == 63)
        sc.free_mode = 1;           /* 0xffff free: full from the first copy */
    sc.precision = rnd_n(3);
    for (i = 0; i < MAX_ANSWERS - 1; i++) {
        uint32_t a = rnd_n(10);
        sc.answers[i] = a < 3 ? IDRETRY : a < 6 ? 0x14 : a < 7 ? 0x24 : IDCANCEL;
    }
    sc.answers[MAX_ANSWERS - 1] = 0;
}

/* The sources and the caches, rebuilt before each side. */
static void build_sources(void)
{
    uint32_t i, j;
    char path[MAX_PATH];

    for (i = 0; i < MAX_BOOSTERS; i++) {
        snprintf(path, sizeof path, "bw\\s%u", i);
        remove_tree(path);
    }
    for (i = 0; i < sc.nboosters; i++) {
        struct booster *b = &sc.b[i];
        if (b->real >= 0)
            continue;
        for (j = 0; j < b->nfiles; j++) {
            if (b->files[j].missing)
                continue;
            snprintf(path, sizeof path, "%s\\%s", b->source, b->files[j].source);
            make_file(path, b->files[j].size, b->files[j].readonly);
        }
    }
}

static void build_caches(void)
{
    uint32_t i, j;
    char path[MAX_PATH];
    FILE *f;

    for (i = 0; i < MAX_BOOSTERS; i++) {
        snprintf(path, sizeof path, "bw\\c%u", i);
        remove_tree(path);
    }
    for (i = 0; i < sc.nboosters; i++) {
        struct booster *b = &sc.b[i];

        snprintf(path, sizeof path, "%s\\SCENERY\\", b->cache);
        make_dirs_for(path);
        snprintf(path, sizeof path, "%s\\TEXTURE\\", b->cache);
        make_dirs_for(path);
        snprintf(path, sizeof path, "%s\\bstinfo", b->cache);
        f = fopen(path, "wb");
        fwrite(b->info, 1, b->info_len, f);
        fclose(f);
        for (j = 0; j < b->nfiles; j++) {
            if (!b->files[j].precached)
                continue;
            snprintf(path, sizeof path, "%s\\%s", b->cache, b->files[j].cached);
            make_file(path, b->files[j].size, b->files[j].precached_ro);
        }
        if (b->junk & 1) { snprintf(path, sizeof path, "%s\\SCENERY\\README.XYZ", b->cache); make_file(path, 77, 0); }
        if (b->junk & 2) { snprintf(path, sizeof path, "%s\\SCENERY\\NOTMINE.BGL", b->cache); make_file(path, 99, 0); }
        if (b->junk & 4) { snprintf(path, sizeof path, "%s\\TEXTURE\\f123456.txr", b->cache); make_file(path, 55, 0); }
        if (b->junk & 8) { snprintf(path, sizeof path, "%s\\SCENERY\\g000001.bgl", b->cache); make_file(path, 66, b->junk & 0x100); }
        if (b->junk & 0x10) { snprintf(path, sizeof path, "%s\\TEXTURE\\NOEXT", b->cache); make_file(path, 33, 0); }
        if (b->junk & 0x20) { snprintf(path, sizeof path, "%s\\SCENERY\\SUBDIR\\", b->cache); make_dirs_for(path); }
        if (b->junk & 0x40) { snprintf(path, sizeof path, "%s\\SCENERY\\F12345.BGL", b->cache); make_file(path, 44, 0); }
        if (b->junk & 0x80) { snprintf(path, sizeof path, "%s\\TEXTURE\\X.PAL", b->cache); make_file(path, 22, 0); }
        if (b->junk & 0x200) { snprintf(path, sizeof path, "%s\\SCENERY\\F1234567.BGL", b->cache); make_file(path, 88, 0); }
        if (b->junk & 0x400) { snprintf(path, sizeof path, "%s\\SCENERY\\G654321A.BGL", b->cache); make_file(path, 11, 0); }
    }
}

static void build_recs(void)
{
    uint32_t i;

    memset(recs, 0, sizeof recs);
    for (i = 0; i < sc.nboosters; i++) {
        struct booster *b = &sc.b[i];

        memset(recs[i], 0x11 * (i + 1), 0x12);
        snprintf((char *)recs[i] + 0x12, 0x104, "%s", b->cache);
        snprintf((char *)recs[i] + 0x116, 0x104, "%s", b->source);
        recs[i][0x21a] = b->keep;
        *(uint16_t *)(recs[i] + 0x21c) = b->quota_kb;
        b->added = 0;
    }
}

/* ---- driving ---------------------------------------------------------------------- */

static void set_precision(uint32_t p)
{
    static const uint16_t cw[3] = { 0x027f, 0x037f, 0x007f };
    uint16_t c = cw[p % 3];
    __asm__ volatile("fldcw %0" : : "m"(c));
}

static void drain(void)
{
    while (handled < posted) {
        if (WaitForSingleObject(handled_sem, 20000) == WAIT_TIMEOUT) {
            printf("scenario %u op %u (%s): the thread did not drain\n", sc.number,
                   op_number, cur->name);
            fflush(stdout);
            exit(3);
        }
    }
    while (WaitForSingleObject(handled_sem, 0) == WAIT_OBJECT_0)
        ;
}

static HANDLE busy_ready, busy_release;
static HANDLE busy_mutex;

static DWORD WINAPI busy_thread(LPVOID unused)
{
    (void)unused;
    WaitForSingleObject(busy_mutex, INFINITE);
    SetEvent(busy_ready);
    WaitForSingleObject(busy_release, INFINITE);
    ReleaseMutex(busy_mutex);
    return 0;
}

static void util_lon(int32_t lat, uint32_t metres, int32_t *out_hi)
{
    uint8_t in[8] = { 0 }, pos[0x18] = { 0 }, out[8] = { 0 };

    /* the same clamp as move(), so the harness's own conversions cannot
     * fault where BOOSTER's would not */
    if (lat > 9400000) lat = 9400000;
    if (lat < -9400000) lat = -9400000;
    *(uint32_t *)(in + 4) = metres;
    *(int32_t *)(pos + 4) = lat;
    ((convert_fn)*(void **)(util_link + 0x44))(in, pos, out);
    *out_hi = *(int32_t *)(out + 4);
}

static const double factors[5] = { 0.5, 1.0, 1.18, 1.36, 1.54 };

/* Move the aircraft: inside a box, onto its edges, onto a reach boundary. */
static void move(void)
{
    struct booster *b = &sc.b[rnd_n(sc.nboosters)];
    int32_t lat;
    uint32_t lon;
    uint32_t mode = rnd_n(9);

    if (b->nboxes == 0 || mode == 0) {
        lat = (int32_t)(rnd_n(18000000) - 9000000);
        lon = rnd();
    } else {
        struct box *x = &b->boxes[rnd_n(b->nboxes)];
        /* BOOSTER's two reaches: the record's scaled by its level, then a
         * quarter of the unscaled one more */
        uint32_t r = (uint32_t)((double)x->reach * factors[rnd_n(5)]) +
                     (rnd_n(2) ? 0 : x->reach >> 2);
        int32_t d = (int32_t)rnd_n(3) - 1;
        int32_t rl;

        lat = x->south + (x->north - x->south) / 2;
        lon = x->west + (x->east - x->west) / 2;
        switch (mode) {
        case 1: break;
        case 2:
            if (rnd_n(2)) {
                /* a corner exactly R south: dlat^2 + 0^2 == R^2 */
                lon = rnd_n(2) ? x->west : x->east;
                lat = x->south - (int32_t)r + d;
            } else {
                lat = (rnd_n(2) ? x->south : x->north) + d;
            }
            break;
        case 3: lon = (rnd_n(2) ? x->west : x->east) + (uint32_t)d; break;
        case 4: lat = x->south - (int32_t)r + d; break;
        case 5: lat = x->north + (int32_t)r + d; break;
        case 6:
            util_lon(lat, r, &rl);
            lon = x->west - (uint32_t)rl + (uint32_t)d;
            break;
        case 7:
            util_lon(lat, r, &rl);
            lon = x->east + (uint32_t)rl + (uint32_t)d;
            break;
        default: {
            double a = (double)(rnd_n(1000)) / 1000.0 * 1.5707963;
            int32_t dl = (int32_t)((double)r * sin(a));
            util_lon(x->south - dl, (uint32_t)((double)r * cos(a)), &rl);
            lat = x->south - dl;
            lon = (rnd_n(2) ? x->west - (uint32_t)rl : x->east + (uint32_t)rl) + (uint32_t)d;
            break;
        }
        }
    }
    /* Retail UTIL +0x44 divides by the latitude circle's circumference in
     * one 64/32 div, which faults once the reach is the whole circle. */
    if (lat > 9400000) lat = 9400000;
    if (lat < -9400000) lat = -9400000;
    *(uint16_t *)(global + 0x562) = (uint16_t)rnd();
    *(int32_t *)(global + 0x564) = lat;
    *(uint16_t *)(global + 0x56a) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x56c) = lon;
    if (b->nbands && rnd_n(3) == 0) {
        int32_t *band = b->bands[rnd_n(b->nbands)];
        *(int32_t *)(global + 0x574) = band[rnd_n(2)] + (int32_t)rnd_n(3) - 1;
    } else if (rnd_n(3) == 0) {
        *(int32_t *)(global + 0x574) = (int32_t)(rnd_n(3000) - 500) << 8;
    }
    put(" lat=%d lon=%#x alt=%d", lat, lon, *(int32_t *)(global + 0x574));
}

static void put_state(void)
{
    uint8_t img[IMAGE_SIZE];
    uint8_t *ring, *node, *queue;
    uint32_t i;

    memcpy(img, cur->image, IMAGE_SIZE);
    memset(img + 0x3c, 0, 8);
    memset(img + 0x50, 0, 8);
    memset(img + 0x358, 0, 0xc);
    memset(img + 0x470, 0, 0x18);
    memset(img + 0x594, 0, 4);
    put("\n  state img=%08x arena=%#x/%08x new=%#x/%08x G=%08x recs=%08x",
        digest(img, IMAGE_SIZE, 0x811c9dc5u),
        arena_used, digest(arena, arena_used, 0x811c9dc5u),
        new_used, digest(new_arena, new_used, 0x811c9dc5u),
        digest(global, GLOBAL_SIZE, 0x811c9dc5u),
        digest(recs, sizeof recs, 0x811c9dc5u));
    ring = *(uint8_t **)(img + 0x8a4);
    node = ring;
    if (node != NULL) {
        int guard = 0;
        do {
            put("\n  node ");
            put_ptr(node);
            put(" rec=");
            put_ptr(*(void **)node);
            put(" levels=%d used=%#x free=%#x:%x level=%u count=%u",
                *(int32_t *)(node + 0xc), *(uint32_t *)(node + 0x10),
                *(uint32_t *)(node + 0x18), *(uint32_t *)(node + 0x14),
                node[0x1c], node[0x1d]);
            if (counting) {
                if (node[0x1d]) cover_full++;
                if (node[0x1c]) cover_level++;
                if (node[0x1c] == 5) cover_level_max++;
            }
            node = *(uint8_t **)(node + 0x20);
        } while (node != ring && node != NULL && ++guard < 8);
    }
    queue = *(uint8_t **)(img + 0x8e4);
    if (queue != NULL) {
        uint32_t count = *(uint32_t *)(queue + 8);
        put("\n  queue cap=%#x count=%u flags=%#x", *(uint32_t *)queue, count,
            *(uint32_t *)(queue + 0x10));
    }
    put("\n  image");
    for (i = 0x8a0; i < 0x8fc; i += 4)
        put(" %08x", *(uint32_t *)(img + i));
}

static void run_side(struct side *s)
{
    uint32_t i, nops;
    HANDLE helper;

    cur = s;
    sc.free_mode = sc_start.free_mode;
    sc.free_base = sc_start.free_base;
    sc.precision = sc_start.precision;
    s->length = 0;
    s->frame = NULL;
    s->message = NULL;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    new_used = 0;
    memset(arena, 0, sizeof arena);
    memset(new_arena, 0, sizeof new_arena);
    memset(global, 0, sizeof global);
    *(uint16_t *)(global + 0x444) = sc.flags444;
    *(uint32_t *)(global + 0x2ac) = 1000;
    if (sources_dirty)
        build_sources();
    sources_dirty = 0;
    build_caches();
    build_recs();
    answer_next = 0;
    posted = handled = 0;
    while (WaitForSingleObject(handled_sem, 0) == WAIT_OBJECT_0)
        ;
    rng = 0x51ed2701u ^ (sc.number * 0x27d4eb2fu);
    rnd();
    set_precision(sc.precision);

    op_number = 0;
    put("init:");
    s->link->init();
    /* A thread has no message queue until it first asks for a message, and
     * a 0x401 posted before then is lost: +0x18 would wait for ever.  Init
     * does not wait for it; the sim starts long before a booster is handed
     * over, so the harness waits here instead.  WM_NULL is dispatched to
     * nothing. */
    while (!real_PostThreadMessageA(*(DWORD *)(s->image + 0x35c), WM_NULL, 0, 0))
        Sleep(1);
    put_state();
    if (s->frame == NULL || s->message == NULL) {
        printf("%s: the CHAIN callbacks were not registered\n", s->name);
        exit(2);
    }
    move();
    /* One scenario in sixteen runs long enough for a full cache's reach to
     * shrink, 60 scans a level, and one in 64 for it to reach the last. */
    nops = sc.number % 64 == 63 ? 500 + rnd_n(100) :
           sc.number % 16 == 15 ? 150 + rnd_n(150) : 4 + rnd_n(MAX_OPS - 4);
    for (op_number = 1; op_number <= nops; op_number++) {
        uint32_t k = rnd_n(100), which = rnd_n(sc.nboosters);
        struct booster *b = &sc.b[which];

        answers_this_copy = 0;
        /* Long scenarios keep their booster and its cache as they are. */
        if (nops > MAX_OPS && op_number > 1 &&
            (rnd_n(4) != 0 || k < 17 || (k >= 48 && k < 57)))
            k = 99;
        if (op_number == 1 || (k < 10 && !b->added)) {
            if (op_number == 1)
                which = 0, b = &sc.b[0];
            if (b->added) goto frame;
            put("\nop %u: add(%u)", op_number, which);
            if (counting) cover_ops[0]++;
            s->link->add(recs[which], which + 1);
            b->added = 1;
        } else if (k < 16 && b->added) {
            put("\nop %u: drop(%u)", op_number, which);
            if (counting) cover_ops[1]++;
            s->link->drop(recs[which]);
            b->added = 0;
            drain();
        } else if (k < 17) {
            uint32_t any = 0, j;
            for (j = 0; j < sc.nboosters; j++) any |= sc.b[j].added;
            if (!any || b->added) goto frame;
            put("\nop %u: drop(%u, not added)", op_number, which);
            if (counting) cover_ops[2]++;
            s->link->drop(recs[which]);
            drain();
        } else if (k < 23) {
            static const uint32_t messages[] = { 0x21, 0x32, 0x33, 0x34, 0x35, 0x36, 0x20, 0x22 };
            uint32_t m = messages[rnd_n(8)];
            put("\nop %u: message(%#x)", op_number, m);
            if (counting) cover_ops[3]++;
            s->message(m, rnd());
            drain();
        } else if (k < 26) {
            put("\nop %u: frame(busy)", op_number);
            if (counting) cover_ops[4]++;
            busy_mutex = *(HANDLE *)(s->image + 0x594);
            ResetEvent(busy_ready);
            ResetEvent(busy_release);
            helper = CreateThread(NULL, 0, busy_thread, NULL, 0, NULL);
            WaitForSingleObject(busy_ready, INFINITE);
            *(uint32_t *)(global + 0x2ac) += rnd_n(200);
            s->frame(0, 0);
            SetEvent(busy_release);
            WaitForSingleObject(helper, INFINITE);
            CloseHandle(helper);
            drain();
        } else if (k < 45) {
            put("\nop %u: move", op_number);
            if (counting) cover_ops[5]++;
            move();
        } else if (k < 48) {
            *(uint16_t *)(global + 0x444) = (uint16_t)(rnd_n(3) ? rnd_n(4) * 0x10 : rnd());
            put("\nop %u: flags(%#x)", op_number, *(uint16_t *)(global + 0x444));
            if (counting) cover_ops[6]++;
        } else if (k < 51) {
            static const uint32_t edges[] = { 0x3ffff, 0x40000, 0x7ffff, 0x80000, 0x80001 };
            uint32_t how = rnd_n(4);
            sc.free_mode = rnd_n(5) == 0 ? 1 : rnd_n(2) ? 2 : 0;
            sc.free_base = rnd_n(2) ? 0x40000 + rnd_n(0x400000) : rnd_n(0x100000);
            if (how == 0) {
                /* exactly on the thresholds BOOSTER tests */
                sc.free_mode = 0;
                sc.free_base = edges[rnd_n(5)];
            } else if (how == 1 && b->nfiles) {
                /* the low dword one file short of wrapping: deleting a file
                 * exactly fills it, without a carry.  With every file the
                 * same size, whichever goes first does. */
                sc.free_mode = 0;
                sc.free_base = 0xffffffffu -
                               (b->uniform ? b->uniform : b->files[rnd_n(b->nfiles)].size) +
                               (rnd_n(3) ? 0 : rnd_n(3) - 1);
            }
            put("\nop %u: free(%u,%#llx)", op_number, sc.free_mode,
                (unsigned long long)sc.free_base);
            if (counting) cover_ops[7]++;
        } else if (k < 54) {
            uint8_t *r = recs[which];
            if (rnd_n(2)) r[0x21a] = (uint8_t)rnd_n(3);
            else r[0x21f] = (uint8_t)(rnd_n(3) == 0);
            put("\nop %u: rec(%u,%u,%u)", op_number, which, r[0x21a], r[0x21f]);
            if (counting) cover_ops[8]++;
        } else if (k < 57 && b->nfiles) {
            struct bfile *f = &b->files[rnd_n(b->nfiles)];
            char path[MAX_PATH];
            uint32_t how = rnd_n(3);
            if (how == 0) {
                snprintf(path, sizeof path, "%s\\%s", b->cache, f->cached);
                SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(path);
            } else if (how == 1) {
                snprintf(path, sizeof path, "%s\\%s", b->cache, f->cached);
                make_file(path, f->size + 1, rnd_n(3) == 0);
            } else if (b->real < 0) {
                sources_dirty = 1;
                snprintf(path, sizeof path, "%s\\%s", b->source, f->source);
                SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(path);
            }
            put("\nop %u: fs(%u,%u)", op_number, which, how);
            if (counting) cover_ops[9]++;
        } else if (k < 58) {
            sc.precision = rnd_n(3);
            set_precision(sc.precision);
            put("\nop %u: precision(%u)", op_number, sc.precision);
            if (counting) cover_ops[10]++;
        } else {
        frame:
            /* onto the tick BOOSTER itself set, +0x8f8, now and then */
            if (rnd_n(4) == 0)
                *(uint32_t *)(global + 0x2ac) = *(uint32_t *)(s->image + 0x8f8) + rnd_n(2);
            else
                *(uint32_t *)(global + 0x2ac) += rnd_n(5) == 0 ? 0x97 : rnd_n(60);
            if (rnd_n(4) == 0) move();
            put("\nop %u: frame", op_number);
            if (counting) cover_ops[11]++;
            s->frame(rnd(), rnd());
            drain();
        }
        put_state();
    }
    op_number++;
    put("\ndeinit:");
    s->link->deinit();
    put_state();
    put("\nfiles:");
    for (i = 0; i < sc.nboosters; i++)
        tree_digest(sc.b[i].cache, 0, 1);
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
    from = (uint32_t)(ls - a) > 1200 ? (uint32_t)(ls - a) - 1200 : 0;
    printf("--- before:\n%.*s\n", (int)((uint32_t)(ls - a) - from), a + from);
    from = i - (uint32_t)(ls - a) > 400 ? i - 400 : (uint32_t)(ls - a);
    printf("--- retail:\n...%.900s\n", a + from);
    printf("--- ours:\n...%.900s\n", b + from);
}

/* A fault ends the run and says where, rather than waiting on a debugger. */
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    uint8_t *at = (uint8_t *)e->ExceptionRecord->ExceptionAddress;
    const char *where = "elsewhere";
    struct side *s[2] = { &retail, &ours };
    int i;

    for (i = 0; i < 2; i++) {
        IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)s[i]->module;
        IMAGE_NT_HEADERS *nt;

        if (dos == NULL)
            continue;
        nt = (IMAGE_NT_HEADERS *)((uint8_t *)dos + dos->e_lfanew);
        if (at >= (uint8_t *)dos && at < (uint8_t *)dos + nt->OptionalHeader.SizeOfImage) {
            printf("\nFAULT in %s BOOSTER at .text+%#x\n", s[i]->name,
                   (uint32_t)(at - (uint8_t *)dos - 0x1000));
            where = s[i]->name;
        }
    }
    printf("\nFAULT %#lx at %p (%s), address %p, thread %lu, scenario %u op %u, running %s\n",
           e->ExceptionRecord->ExceptionCode, (void *)at, where,
           e->ExceptionRecord->NumberParameters > 1
               ? (void *)e->ExceptionRecord->ExceptionInformation[1] : NULL,
           GetCurrentThreadId(), sc.number, op_number, cur ? cur->name : "none");
    if (cur != NULL)
        printf("radius %#x lat %d:%04x lon %#x rec %#x\n", *(uint32_t *)(cur->image + 0x8c4),
               *(int32_t *)(global + 0x564), *(uint16_t *)(global + 0x562),
               *(uint32_t *)(global + 0x56c),
               (uint32_t)(*(uint8_t **)(cur->image + 0x8e8) - arena));
    if (cur != NULL && cur->length > 0) {
        uint32_t from = cur->length > 2000 ? cur->length - 2000 : 0;
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
    unsigned long real_used = 0;

    if (argc < 3) {
        printf("usage: boosterdiff N CD-ROOT [FIRST]\n");
        return 2;
    }
    snprintf(cd_root, sizeof cd_root, "%s", argv[2]);
    for (i = 0; i < sizeof cd_root && cd_root[i]; i++)
        if (cd_root[i] == '/') cd_root[i] = '\\';
    SetUnhandledExceptionFilter(on_fault);
    InitializeCriticalSection(&put_lock);
    InitializeCriticalSection(&new_lock);
    handled_sem = CreateSemaphoreA(NULL, 0, 0x7fffffff, NULL);
    busy_ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    busy_release = CreateEventA(NULL, TRUE, FALSE, NULL);
    crt = LoadLibraryA("MSVCRT.dll");
    CreateDirectoryA("bw", NULL);
    load_ext_table("FLTSIM98_host.exe");
    build_stubs();
    load_util("UTIL_retail.dll");
    load_side(&retail, "BOOSTER_retail.dll", "retail", 1);
    load_side(&ours, "BOOSTER_ours.dll", "ours", 0);

    for (i = first; i < first + n; i++) {
        uint32_t j;

        make_scenario(i);
        for (j = 0; j < sc.nboosters; j++) {
            if (sc.b[j].real >= 0) real_used++;
            if (sc.b[j].real < 0) {
                const uint8_t *p = sc.b[j].info;
                uint32_t o = *(const uint16_t *)(p + 0x14);
                while (o + 3 <= sc.b[j].info_len && p[o] != 0 && p[o] < 6) {
                    cover_types[p[o]]++;
                    if (*(const uint16_t *)(p + o + 1) == 0)
                        break;
                    o += *(const uint16_t *)(p + o + 1);
                }
            }
        }
        sc_start = sc;
        sources_dirty = 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("BOOSTER_DUMP") != NULL) {
            FILE *f = fopen("booster_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("booster_ours.txt", "wb");
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
    printf("boosterdiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops: add %lu, drop %lu, drop-unadded %lu, message %lu, busy frame %lu, "
           "move %lu, flags %lu, free %lu, rec %lu, fs %lu, precision %lu, frame %lu\n",
           cover_ops[0], cover_ops[1], cover_ops[2], cover_ops[3], cover_ops[4],
           cover_ops[5], cover_ops[6], cover_ops[7], cover_ops[8], cover_ops[9],
           cover_ops[10], cover_ops[11]);
    printf("  records: type1 %lu, type2 %lu, type3 %lu, type4 %lu, type5 %lu; "
           "real boosters %lu\n", cover_types[1], cover_types[2], cover_types[3],
           cover_types[4], cover_types[5], real_used);
    printf("  copies %lu (failed %lu), registered %lu, unregistered %lu, "
           "flushed BGLs %lu, host deletes %lu\n", cover_copy, cover_copy_fail,
           cover_register, cover_unregister, cover_delete_bst, cover_host_delete);
    printf("  message boxes %lu (repaired %lu, cancelled %lu), space alerts %lu, "
           "free-space failures %lu\n", cover_msgbox, cover_retry_fix, cover_cancel,
           cover_alert_space, cover_free_fail);
    printf("  states with the cache full %lu, reach shrunk %lu (to the last level %lu), "
           "jobs queued %lu, longest queue %lu\n", cover_full, cover_level, cover_level_max,
           cover_jobs, cover_queue_max);
    (void)cover_busy_skip;
    (void)cover_flush;
    return differ ? 1 : 0;
}
