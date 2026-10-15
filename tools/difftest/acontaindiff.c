/* Differential test: retail ACONTAIN.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven only through Linkage, the
 * way MAIN, PANELS and the rest drive them.  What ACONTAIN imports:
 *
 *   host      a stub.  Memory is a bump allocator over an arena, so heap
 *             pointers come out the same on both sides; what the host does
 *             not zero is filled with 0xa5, so code relying on zeroes it never
 *             asked for shows up.  Files, profiles and
 *             directory listings pass through to real generated aircraft
 *             trees, behind transliterations of the host's own path join,
 *             split, find and file services.  Strings come from ENGLISH.FLL.
 *             Modules loaded by path (+0x26c) are fakes: a .SIM or .MDL
 *             fixture file says which fake Linkage it is, and a fake model's
 *             +0x18 hands back one of a set of headers, some of them wrong.
 *   GLOBAL    one block, reset identically before each side runs
 *   CHAIN     retail's, behind a logging wrapper, with a logging callback on
 *             list 20, which +0x28 dispatches
 *   G3D       a stub that logs draws and flushes
 *   PANELS    a stub that, as the real one does, asks +0x30 for the panel.cfg
 *   UTIL      a stub that logs the alert and its text argument
 *
 * A scenario is a random script against one of the trees, run once against
 * each module.  Compared: the transcript of every import call and result,
 * GLOBAL, MAIN's second aircraft record, the arena, and ACONTAIN's .data
 * image from +0x78 -- both builds keep it at Linkage-0x38 with retail's
 * layout.
 *
 * Retail reads stack it never set (see acontain.c).  The harness zeroes the
 * stack below it before each +0x28, which is the case the reimplementation
 * reproduces, and generates trees whose AIRCRAFT\ listing never starts with a
 * file of an unknown extension.  It also keeps clear of what faults retail
 * or overruns its buffers:
 *   - +0x24 with no record (it reads through the pointer unchecked);
 *   - paths and values long enough to overrun the 0x100-byte path buffers;
 *   - .AIR chunks larger than the buffer they are read into, and sim names
 *     with no terminator inside their chunk;
 *   - model headers whose state is under 0x68 bytes (retail writes +0x60 and
 *     +0x64 of it regardless).
 *
 * Build: make acontaindiff     Run: make acontaindiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_LO    0x78        /* compared from here */
#define IMAGE_HI    0x3b0       /* .data's virtual size */
#define GLOBAL_SIZE 0x1300
#define ARENA_SIZE  (1 << 21)
#define NTREES      12
#define NHDR        16
#define NSIM        4

struct import { uint32_t id; void *link; };
struct aircraft { uint8_t *header; uint8_t *state; uint8_t *rec; };
struct ac_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t r0c, r10;
    void *dialogs;
    int32_t (STDCALL *draw)(struct aircraft *a);
    int32_t (STDCALL *list)(int32_t *count, uint8_t **entries);
    int32_t (STDCALL *select)(struct aircraft *a, const char *title);
    int32_t (STDCALL *release)(struct aircraft *a);
    int32_t (STDCALL *load)(const char *title);
    int32_t (*unload)(void);
    int32_t (STDCALL *query)(struct aircraft *a, int32_t key, char *buf, uint32_t len);
    int32_t (STDCALL *model_load)(struct aircraft *a, int32_t kind);
    int32_t (STDCALL *model_unload)(struct aircraft *a);
    int32_t (STDCALL *model_kind)(struct aircraft *a);
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

/* a string, quoted and cut short; null as such */
static void put_s(const char *s)
{
    if (!s) { put("null"); return; }
    put("'%.40s'", s);
}

/* ---- the side being run ------------------------------------------------------ */

struct side {
    const char *name;
    HMODULE dll;
    struct ac_linkage *link;
    uint8_t *image;             /* Linkage - 0x38 */
    uint8_t pristine[IMAGE_HI];
};
static struct side *cur;
static int counting;            /* coverage is counted on the retail run */
static unsigned long cover_colour[3], cover_alias[4], cover_cfg_hit, cover_short, cover_seekfail;
static int after_alias;

static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t global_blob[GLOBAL_SIZE];
static struct aircraft aux;     /* MAIN's own record, at 0x20807f90 */
static char qbuf[0x110];        /* +0x30's output, for OP_QUERY */
static uint8_t hdr[NHDR][0x40] __attribute__((aligned(4)));
static uint8_t mdl_link[NHDR][0x24] __attribute__((aligned(4)));
static uint8_t sim_link[NSIM][0x24] __attribute__((aligned(4)));

/* Pointers ACONTAIN passes out.  Its own stack differs between the builds,
 * so anything not otherwise known prints as "stk". */
static void put_ptr(const void *p)
{
    const uint8_t *q = (const uint8_t *)p;
    if (!p)
        put("null");
    else if (q >= arena && q < arena + sizeof arena)
        put("arena+%#x", (unsigned)(q - arena));
    else if (q >= global_blob && q < global_blob + sizeof global_blob)
        put("G+%#x", (unsigned)(q - global_blob));
    else if (q >= (uint8_t *)&aux && q < (uint8_t *)(&aux + 1))
        put("aux+%u", (unsigned)(q - (uint8_t *)&aux));
    else if (q >= (uint8_t *)qbuf && q < (uint8_t *)qbuf + sizeof qbuf)
        put("qbuf");
    else if (q >= hdr[0] && q < hdr[0] + sizeof hdr)
        put("hdr%u", (unsigned)(q - hdr[0]) / 0x40);
    else if (q >= mdl_link[0] && q < mdl_link[0] + sizeof mdl_link)
        put("mdl%u", (unsigned)(q - mdl_link[0]) / 0x24);
    else if (q >= sim_link[0] && q < sim_link[0] + sizeof sim_link)
        put("sim%u", (unsigned)(q - sim_link[0]) / 0x24);
    else if (cur && q >= cur->image && q < cur->image + IMAGE_HI)
        put("img+%#x", (unsigned)(q - cur->image));
    else
        put("stk");
}

/* ---- the host's extension table, read out of FLTSIM98.EXE ------------------ */

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
    if (strcmp(ext_text[0x24], ".CFG") || strcmp(ext_text[0x0f], ".AIR")) {
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

/* 0x40b410: an extension's id, upper-casing it on the way; nonzero if none */
static int ext_lookup(char *ext, int32_t *out)
{
    int k;
    _strupr(ext);
    for (k = 1; k < 0x25; k++)
        if (strcmp(ext, ext_text[k]) == 0) { *out = k; return 0; }
    return 1;
}

/* ---- the stub host: memory ------------------------------------------------------ */

/* Each block has a 16-byte header holding its size, for realloc. */
static void *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;
    if (size > ARENA_SIZE || arena_used + n > ARENA_SIZE) return NULL;
    p = arena + arena_used;
    arena_used += n;
    *(uint32_t *)p = size;
    return p + 16;
}

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    void *p = arena_get(size);
    put(" alloc(%d,%#x,%d)=", fl, size, zero);
    if (!p) { printf("arena exhausted\n"); exit(2); }
    memset(p, zero ? 0 : 0xa5, size);   /* malloc's garbage, made the same on both sides */
    put_ptr(p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    uint8_t *old = *pp, *p = arena_get(size);
    uint32_t n;
    put(" realloc(%d,%#x,", fl, size);
    put_ptr(old);
    put(")=");
    if (!p) { printf("arena exhausted\n"); exit(2); }
    memset(p, 0xa5, size);              /* realloc's new tail is garbage too */
    if (old) {
        n = *(uint32_t *)(old - 16);
        memcpy(p, old, n < size ? n : size);
    }
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

static int32_t STDCALL h_memset(void *d, int32_t c, uint32_t n)
{
    if (counting) cover_short++;
    put(" memset(");
    put_ptr(d);
    put(",%d,%#x)", c, n);
    memset(d, c, n);
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

static uint32_t STDCALL h_strlen(const char *s)
{
    uint32_t r = s ? (uint32_t)strlen(s) : 0;
    put(" strlen=%u", r);
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

static char *STDCALL h_strcat(char *d, const char *s)
{
    strcat(d, s);
    put(" strcat=");
    put_s(d);
    return d;
}

static char *STDCALL h_strncpy(char *d, const char *s, uint32_t n)
{
    put(" strncpy(");
    put_ptr(d);
    put(",");
    put_s(s);
    put(",%#x)", n);
    return strncpy(d, s, n);
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


static int32_t STDCALL h_stricmp(const char *a, const char *b)
{
    int32_t r = _strcmpi(a, b);
    if (counting && cur && (const uint8_t *)b >= cur->image && (const uint8_t *)b < cur->image + IMAGE_HI)
        cover_colour[r == -1 ? 0 : r == 0 ? 1 : 2]++;    /* early out, hit, go on */
    put(" icmp(");
    put_s(a);
    put(",");
    put_s(b);
    put(")=%d", r);
    return r;
}

/* ---- the stub host: paths ------------------------------------------------------- */

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

/* 0x40a6c0, for what ACONTAIN asks of it: no drive, no extension id */
static int32_t STDCALL h_split(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{
    char e[0x100];
    if (ext) *ext = 0;
    if (!path) return -1;
    if (drive || ext) { printf("split: unexpected arguments\n"); exit(2); }
    if (path[0] == 0) {
        if (dir) dir[0] = 0;
        if (name) name[0] = 0;
    } else
        _splitpath(path, NULL, dir, name, e);
    put(" split=");
    put_s(name);
    return 0;
}

/* ---- the stub host: listing ----------------------------------------------------- */

struct find { HANDLE h; WIN32_FIND_DATAA d; int more; };
struct find_entry { char name[0x104]; int32_t type; uint32_t size; int32_t writable; };

/* 0x40a9a0 */
static int32_t STDCALL h_find_first(struct find **out, const char *dir, const char *pat, int32_t ext)
{
    char path[MAX_PATH * 2];
    struct find *f;
    size_t n;
    path[0] = 0;
    if (dir) {
        strcpy(path, dir);
        n = strlen(path);
        if (n == 0 || path[n - 1] != '\\') strcat(path, "\\");
    }
    strcat(path, pat ? pat : "*");
    if (ext == -1) strcat(path, ".*");
    else if (ext != -2) strcat(path, ext_text[ext]);
    f = calloc(1, sizeof *f);
    f->h = FindFirstFileA(path, &f->d);
    put(" findfirst(");
    put_s(path);
    if (f->h == INVALID_HANDLE_VALUE) {
        free(f);
        put(")=-1");
        return -1;
    }
    f->more = 1;
    *out = f;
    put(")=0");
    return 0;
}

/* 0x40ab60 */
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

/* 0x40ac90 */
static int32_t STDCALL h_find_close(struct find **pf)
{
    FindClose((*pf)->h);
    free(*pf);
    *pf = NULL;
    put(" findclose");
    return 0;
}

/* ---- the stub host: files and profiles ------------------------------------------ */

static int32_t STDCALL h_cfg_string(const char *sec, const char *key, char *buf, uint32_t size)
{
    int32_t r = GetPrivateProfileStringA(sec, key, buf, buf, size, ".\\FLTSIM98.CFG");
    if (counting && r > 0) cover_cfg_hit++;
    put(" cfg(");
    put_s(sec);
    put(",");
    put_s(key);
    put(")=%d,", r);
    put_s(buf);
    return r;
}

static int32_t STDCALL h_profile(const char *sec, const char *key, char *buf, uint32_t size,
                                 const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;
    if (resolve(path, ext, full, sizeof full)) { put(" prof=-1"); return -1; }
    r = GetPrivateProfileStringA(sec, key, buf, buf, size, full);
    if (counting && strcmp(key, "Alias") == 0 && r > 0) { cover_alias[0]++; after_alias = 1; }
    put(" prof(");
    put_s(full);
    put(",");
    put_s(sec);
    put(",");
    put_s(key);
    put(")=%d,", r);
    put_s(buf);
    return r;
}

static int32_t STDCALL h_exists(const char *path, int32_t ext)
{
    char full[0x200];
    int32_t r;
    if (resolve(path, ext, full, sizeof full)) { put(" exists=-1"); return -1; }
    r = GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES ? -1 : 0;
    if (after_alias == 1) { if (r == 0) { cover_alias[1]++; after_alias = 0; } else after_alias = 2; }
    else if (after_alias == 2) { cover_alias[r == 0 ? 2 : 3]++; after_alias = 0; }
    put(" exists(");
    put_s(full);
    put(")=%d", r);
    return r;
}

struct file { HANDLE h; char *path; int32_t mode, ext; };

/* 0x40a000 */
static int32_t STDCALL h_open(struct file **out, const char *path, int32_t ext, int32_t mode)
{
    static const DWORD access[4] = { GENERIC_READ, GENERIC_READ | GENERIC_WRITE,
                                     GENERIC_WRITE, GENERIC_WRITE };
    char full[0x200];
    struct file *f;
    put(" open(");
    if (resolve(path, ext, full, sizeof full)) { *out = NULL; put("-1)"); return -1; }
    put_s(full);
    if (mode < 0 || mode > 3) { *out = NULL; put(",%d)=-1", mode); return -1; }
    f = calloc(1, sizeof *f);
    f->h = CreateFileA(full, access[mode], 0, NULL, OPEN_EXISTING, 0x80, NULL);
    if (f->h == INVALID_HANDLE_VALUE) {
        free(f);
        *out = NULL;
        put(")=-1");
        return -1;
    }
    if (mode == 3) SetFilePointer(f->h, 0, NULL, FILE_END);
    f->mode = mode;
    f->ext = ext;
    *out = f;
    put(")=0");
    return 0;
}

/* 0x40a140 */
static int32_t STDCALL h_close(struct file *f)
{
    int32_t r;
    if (!f) return -1;
    r = CloseHandle(f->h) ? 0 : -1;
    free(f);
    put(" close");
    return r;
}

/* 0x40a360 */
static int32_t STDCALL h_seek(struct file *f, uint32_t off, int32_t whence)
{
    put(" seek(%#x,%d)=", off, whence);
    if (!f) { put("-1"); return -1; }
    if (GetFileSize(f->h, NULL) < off) { if (counting) cover_seekfail++; put("-1"); return -1; }
    if (SetFilePointer(f->h, (LONG)off, NULL, whence) == INVALID_SET_FILE_POINTER) {
        put("-1");
        return -1;
    }
    put("0");
    return 0;
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
    put(" read(%#x)=%lu", n, (unsigned long)k);
    return ok ? 0 : -1;
}

/* ---- the stub host: sorting ------------------------------------------------------ */

typedef int32_t (STDCALL *cmp_fn)(const void *, const void *);
static cmp_fn qsort_cmp;
static int __cdecl qsort_tramp(const void *a, const void *b) { return qsort_cmp(a, b); }

static void STDCALL h_qsort(void *base, uint32_t n, uint32_t w, cmp_fn cmp)
{
    put(" qsort(");
    put_ptr(base);
    put(",%u,%#x)", n, w);
    qsort_cmp = cmp;
    qsort(base, n, w, qsort_tramp);
}

/* ---- modules loaded by path: fake .SIM and .MDL ---------------------------------- */

static unsigned long cover_mdl_load[3], cover_sim_load[3], cover_hdr[NHDR];

/* 0x40da90: the fixture file names the fake */
static int32_t STDCALL h_module_load(void **out, const char *path, int32_t ext)
{
    char full[0x200], text[16];
    unsigned k;
    FILE *f;
    size_t n;
    int kind = ext == 0x14 ? 0 : ext == 0x1b ? 1 : 2;
    put(" modload(");
    if (resolve(path, ext, full, sizeof full)) { put("-1)"); return -1; }
    put_s(full);
    put(")=");
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES || !(f = fopen(full, "rb"))) {
        put("-1");
        if (counting) { if (kind == 0) cover_mdl_load[0]++; else cover_sim_load[0]++; }
        return -1;
    }
    n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = 0;
    if (sscanf(text, "MDL %u", &k) == 1 && k < NHDR && ext == 0x14) {
        *out = mdl_link[k];
        if (counting) cover_mdl_load[2]++;
    } else if (sscanf(text, "SIM %u", &k) == 1 && k < NSIM && ext == 0x1b) {
        *out = sim_link[k];
        if (counting) cover_sim_load[2]++;
    } else {
        put("-1");
        if (counting) { if (kind == 0) cover_mdl_load[1]++; else cover_sim_load[1]++; }
        return -1;
    }
    put_ptr(*out);
    return 0;
}

static int32_t STDCALL h_module_free(void **link)
{
    put(" modfree(");
    put_ptr(*link);
    put(")");
    *link = NULL;
    return 0;
}

/* the fake model's own slots */
static void STDCALL mdl_attach(struct aircraft *a)
{
    uint8_t *link = *(uint8_t **)(a->rec + 8);
    unsigned k = (unsigned)(link - mdl_link[0]) / 0x24;
    put(" mdl%u.attach(", k);
    put_ptr(a);
    put(")");
    a->header = hdr[k];
    if (counting) cover_hdr[k]++;
}

static void STDCALL mdl_state(uint8_t *state)
{
    put(" mdl.state(");
    put_ptr(state);
    put(")");
}

static void mdl_detach(void) { put(" mdl.detach"); }
static void fake_nop(void) { }

static uint8_t host_blob[0x320];

/* ---- G3D, PANELS, UTIL -------------------------------------------------------------- */

static void STDCALL g3d_draw(uint32_t object, void *state)
{
    put(" g3d.draw(%x,", object);
    put_ptr(state);
    put(")");
}

static void g3d_flush(void) { put(" g3d.flush"); }

static void STDCALL panels_attach(struct aircraft *a)
{
    char b[0x104];
    int32_t r;
    put(" panels.attach(");
    put_ptr(a);
    put(")");
    memset(b, 0, sizeof b);
    r = cur->link->query(a, 6, b, sizeof b);
    put(" ->%d,", r);
    put_s(b);
}

static void panels_detach(void) { put(" panels.detach"); }

static unsigned long cover_alert[6];

static int32_t STDCALL util_alert(int32_t fl, int32_t tag, uint32_t text, const char *arg,
                                  uint32_t p3, uint32_t p4, uint32_t p5, int32_t lines)
{
    put(" alert(%d,%d,%#x,", fl, tag, text);
    put_s(arg);
    put(",%x,%x,%x,%d)", p3, p4, p5, lines);
    if (counting && text >= 0xf540 && text <= 0xf544) cover_alert[text - 0xf540]++;
    return 1;
}

static uint8_t g3d_blob[0x60], panels_blob[0x20], util_blob[0xa8];

/* ---- CHAIN, retail's behind a wrapper ------------------------------------------------ */

static struct chain_linkage *chain;
static uint8_t chain_fwd[0x24];

static uint32_t STDCALL c_dispatch(uint32_t list, uint32_t a, uint32_t b)
{
    uint32_t r;
    put(" chain.dispatch(%u,%#x,%#x)", list, a, b);
    r = chain->dispatch(list, a, b);
    return r;
}

static void STDCALL on_list20(uint32_t a, uint32_t b) { put(" list20(%#x,%#x)", a, b); }

/* ---- fixture trees ------------------------------------------------------------------ */

static uint32_t rs = 0x2545f491u;
/* xorshift, with its output mixed: the raw low bits of successive values are
 * correlated enough that `% 16` after `% 12` never came up even */
static uint32_t rng(void)
{
    uint32_t x;
    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
    x = rs * 0x9e3779b9u;
    return x ^ (x >> 16);
}

static const char *const folder_names[] = {
    "C182", "B737_400", "Extra300", "Lear45", "Sopcamel", "Bell206B", "Schweizr",
    "Twin", "Glider", "Cub"
};
static const char *const title_pool[] = {
    "Cessna Skylane 182S", "Cessna Skylane 182R RG", "Boeing 737-400", "Extra 300S",
    "Learjet 45", "Sopwith Camel", "Bell 206B JetRanger", "Schweizer 2-32 Sailplane",
    "Piper Cub", "Twin Otter", "C182", "Glider"
};
static const char *const sim_names[] = { "SIM1", "SIMJET", "HELO", "NOSUCH", "BAD" };
static const char *const odd_colours[] = { "PURPLE", "AAA", "ZZZ", "BRIGHT_", "BLACKX",
                                           "Gray ", "", "MED", "LIGHT_GREENISH" };
static const char *colour_names[64];
static unsigned ncolour_names;

#define MAXTITLES 48
struct tree {
    char titles[MAXTITLES][48];     /* what a scenario may ask for */
    unsigned ntitles;
};
static struct tree trees[NTREES];

static void add_title(struct tree *t, const char *s)
{
    unsigned i;
    if (t->ntitles == MAXTITLES) return;
    for (i = 0; i < t->ntitles; i++) if (strcmp(t->titles[i], s) == 0) return;
    snprintf(t->titles[t->ntitles++], 48, "%s", s);
}

static void case_flip(char *s)
{
    for (; *s; s++)
        if (rng() % 3 == 0) *s = (char)(*s ^ ((*s | 0x20) >= 'a' && (*s | 0x20) <= 'z' ? 0x20 : 0));
}

static void deltree(const char *dir)
{
    char pat[MAX_PATH], sub[MAX_PATH];
    WIN32_FIND_DATAA d;
    HANDLE h;
    snprintf(pat, sizeof pat, "%s\\*", dir);
    h = FindFirstFileA(pat, &d);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(d.cFileName, ".") || !strcmp(d.cFileName, "..")) continue;
            snprintf(sub, sizeof sub, "%s\\%s", dir, d.cFileName);
            if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) deltree(sub);
            else DeleteFileA(sub);
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); exit(2); }
    fputs(text, f);
    fclose(f);
}

static void put_chunk(FILE *f, uint32_t tag, uint32_t size, const void *data, uint32_t have)
{
    fwrite(&tag, 4, 1, f);
    fwrite(&size, 4, 1, f);
    if (have) fwrite(data, 1, have, f);
}

/* A .AIR: chunks 0x100 (the sim's name), 2 and 3 among others, in some order,
 * sometimes truncated, sometimes behind a chunk too big to seek past. */
static void write_air(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint8_t buf[0x400];
    unsigned order[6], n = 0, i, k, len;
    if (!f) { printf("cannot write %s\n", path); exit(2); }
    if (rng() % 30 == 0) { fclose(f); return; }                  /* empty */
    if (rng() % 40 == 0) { fwrite("\x02\x00", 1, 2, f); fclose(f); return; }
    order[n++] = 0x100;
    if (rng() % 6) order[n++] = 2;
    if (rng() % 6) order[n++] = 3;
    for (k = rng() % 3; k--; ) order[n++] = 1 + rng() % 0x400;   /* junk, any id */
    if (rng() % 20 == 0) order[0] = 0x101;                       /* no sim name */
    for (i = 0; i < n; i++) {                                    /* shuffle */
        k = rng() % n;
        uint32_t t = order[i]; order[i] = order[k]; order[k] = t;
    }
    if (rng() % 25 == 0) put_chunk(f, 7, rng() % 2 ? 0xfffffff0u : 0x100000u, NULL, 0);
    for (i = 0; i < n; i++) {
        int last = i == n - 1, trunc = last && rng() % 12 == 0;
        memset(buf, 0, sizeof buf);
        if (order[i] == 0x100) {
            const char *s = sim_names[rng() % 7 ? rng() % 3 : 3 + rng() % 2];
            if (rng() % 15 == 0) s = "";
            len = (unsigned)strlen(s) + 1 + rng() % 16;
            if (len > 0x100) len = 0x100;
            strcpy((char *)buf, s);
        } else if (order[i] == 2 || order[i] == 3) {
            len = rng() % 5 == 0 ? 0x200 + rng() % 0x200 : rng() % 80;
            for (k = 0; k + 1 < len; k++) buf[k] = (uint8_t)(' ' + rng() % 90);
        } else {
            len = rng() % 64;
            for (k = 0; k < len; k++) buf[k] = (uint8_t)rng();
        }
        put_chunk(f, order[i], len, buf, trunc ? len / 2 : len);
    }
    fclose(f);
}

static const char *pick_suffix(void)
{
    static const char *const s[] = { "", "", "", "rg", "float", "x" };
    return s[rng() % 6];
}

/* a model, panel, sound or texture folder, maybe with a .cfg and an alias */
static void write_part(const char *root, const char *folder, const char *part,
                       const char *sfx, const char *const *others, unsigned nothers)
{
    char dir[MAX_PATH], path[MAX_PATH], text[2048];
    unsigned i, k;
    if (sfx[0]) snprintf(dir, sizeof dir, "%s\\%s\\%s.%s", root, folder, part, sfx);
    else snprintf(dir, sizeof dir, "%s\\%s\\%s", root, folder, part);
    if (rng() % 25 == 0) return;                                 /* missing */
    CreateDirectoryA(dir, NULL);
    if (rng() % 8 == 0) return;                                  /* no .cfg */
    text[0] = 0;
    if (strcmp(part, "model") == 0) {
        const char *mdl[2];
        mdl[0] = rng() % 2 ? "PLANE_N" : "PLANE";
        mdl[1] = rng() % 2 ? "PLANE_C" : "WRECK";
        strcat(text, "[models]\n");
        if (rng() % 7) { strcat(text, "normal="); strcat(text, mdl[0]); strcat(text, "\n"); }
        if (rng() % 3) { strcat(text, "crash="); strcat(text, mdl[1]); strcat(text, "\n"); }
        for (k = 0; k < 2; k++) {
            snprintf(path, sizeof path, "%s\\%s.MDL", dir, mdl[k]);
            if (rng() % 10 == 0) continue;
            if (rng() % 12 == 0) write_text(path, "BAD");
            else {
                char t[16];
                snprintf(t, sizeof t, "MDL %u", (unsigned)(rng() % NHDR));
                write_text(path, t);
            }
        }
        strcat(text, "[colors]\n");
        for (i = 0; i < 16; i++) {
            char line[64], name[48];
            if (rng() % 5 == 0) continue;
            switch (rng() % 10) {
            case 0: case 1:
                snprintf(name, sizeof name, "%s", odd_colours[rng() % 9]); break;
            default:
                snprintf(name, sizeof name, "%s", colour_names[rng() % ncolour_names]);
                case_flip(name);
            }
            snprintf(line, sizeof line, "%02u=%s\n", i, name);
            strcat(text, line);
        }
    }
    if (rng() % 5 == 0) {
        /* an alias: another aircraft's part folder, named from AIRCRAFT\
         * (the first place looked), by a path from the root (the second),
         * or nowhere */
        char a[64];
        switch (rng() % 7) {
        case 0: case 1: case 2: case 3:
            snprintf(a, sizeof a, "%s\\%s", others[rng() % nothers], part); break;
        case 4: case 5:
            snprintf(a, sizeof a, "AIRCRAFT\\%s\\%s", others[rng() % nothers], part); break;
        default: snprintf(a, sizeof a, "Nowhere");
        }
        strcat(text, "[fltsim]\nalias=");
        strcat(text, a);
        strcat(text, "\n");
    }
    snprintf(path, sizeof path, "%s\\%s.cfg", dir, part);
    write_text(path, text);
}

static void write_tree(unsigned t)
{
    char root[MAX_PATH], dir[MAX_PATH], path[MAX_PATH], text[8192], line[256];
    const char *folders[6];
    unsigned nf = 1 + rng() % 6, i, k, v, nvar;
    struct tree *tr = &trees[t];
    char cfgtext[2048];

    snprintf(root, sizeof root, "acfix\\t%u", t);
    CreateDirectoryA(root, NULL);
    snprintf(dir, sizeof dir, "%s\\AIRCRAFT", root);
    CreateDirectoryA(dir, NULL);
    snprintf(dir, sizeof dir, "%s\\SIM", root);
    CreateDirectoryA(dir, NULL);
    for (k = 0; k < NSIM; k++) {
        snprintf(path, sizeof path, "%s\\SIM\\%s.SIM", root, sim_names[k]);
        if (k == 3) continue;                                    /* NOSUCH */
        snprintf(line, sizeof line, "SIM %u", k);
        write_text(path, rng() % 10 == 0 ? "BAD" : line);
    }
    snprintf(path, sizeof path, "%s\\SIM\\BAD.SIM", root);
    write_text(path, "BAD");

    /* distinct folders */
    for (i = 0; i < nf; ) {
        const char *f = folder_names[rng() % 10];
        for (k = 0; k < i && strcmp(folders[k], f); k++) ;
        if (k == i) folders[i++] = f;
    }
    snprintf(root, sizeof root, "acfix\\t%u\\AIRCRAFT", t);
    for (i = 0; i < nf; i++) {
        snprintf(dir, sizeof dir, "%s\\%s", root, folders[i]);
        CreateDirectoryA(dir, NULL);
        add_title(tr, folders[i]);
        text[0] = 0;
        nvar = 1 + rng() % 4;
        for (v = 0; v < nvar; v++) {
            const char *title = title_pool[rng() % 12];
            const char *parts[4] = { "model", "panel", "sound", "texture" };
            char air[32];
            unsigned sect = v;
            if (v > 0 && rng() % 10 == 0) sect = v + 1;          /* a gap */
            snprintf(line, sizeof line, "[fltsim.%u]\n", sect);
            strcat(text, line);
            if (rng() % 12) {
                if (rng() % 15 == 0) strcat(text, "title=\n");
                else { snprintf(line, sizeof line, "title=%s\n", title); strcat(text, line); add_title(tr, title); }
            }
            snprintf(air, sizeof air, "%s_%u", folders[i], v);
            if (rng() % 20) { snprintf(line, sizeof line, "sim=%s\n", air); strcat(text, line); }
            snprintf(path, sizeof path, "%s\\%s.AIR", dir, air);
            if (rng() % 15) write_air(path);
            for (k = 0; k < 4; k++) {
                const char *sfx = pick_suffix();
                if (sfx[0] || rng() % 3 == 0) {
                    snprintf(line, sizeof line, "%s=%s\n", parts[k], sfx);
                    strcat(text, line);
                }
                write_part(root, folders[i], parts[k], sfx, folders, nf);
            }
            if (rng() % 2) { snprintf(line, sizeof line, "checklists=%s_check\n", folders[i]); strcat(text, line); }
        }
        snprintf(path, sizeof path, "%s\\aircraft.cfg", dir);
        if (rng() % 20) write_text(path, text);
    }
    /* strays: a registered extension, and sometimes an unknown one */
    snprintf(path, sizeof path, "%s\\notes.CFG", root);
    write_text(path, "[x]\n");
    if (rng() % 2) {
        snprintf(path, sizeof path, "%s\\readme.txt", root);
        write_text(path, "hello");
    }

    /* FLTSIM98.CFG's [AIRtoCONTAINER] */
    cfgtext[0] = 0;
    strcat(cfgtext, "[AIRtoCONTAINER]\n");
    for (k = 0; k < 4; k++) {
        const char *to = rng() % 4 ? tr->titles[rng() % tr->ntitles] : "Nothing Here";
        snprintf(line, sizeof line, "OLD_%u=%s\n", k, to);
        strcat(cfgtext, line);
    }
    snprintf(path, sizeof path, "acfix\\t%u\\FLTSIM98.CFG", t);
    write_text(path, cfgtext);

    /* what a scenario may ask for, beyond the titles and folder names */
    for (k = 0; k < 4; k++) {
        snprintf(line, sizeof line, "OLD_%u", k);
        add_title(tr, line);
    }
    add_title(tr, "C:\\FS5\\OLD_1.AIR");
    add_title(tr, "OLD_2.AIR");
    add_title(tr, "Nothing Here");
    add_title(tr, "");
    for (k = 0, i = tr->ntitles; k < i && tr->ntitles < MAXTITLES; k++)
        if (rng() % 3 == 0) {
            char flipped[48];
            snprintf(flipped, sizeof flipped, "%s", tr->titles[k]);
            case_flip(flipped);
            add_title(tr, flipped);
        }
}

/* Retail's enumeration reads an entry type it never set when the first entry
 * is a file the host has no extension id for; keep such a file off the front. */
static void check_first_entry(unsigned t)
{
    char pat[MAX_PATH], path[MAX_PATH], x[0x100], nm[0x100];
    WIN32_FIND_DATAA d;
    HANDLE h;
    int32_t type;
    snprintf(pat, sizeof pat, "acfix\\t%u\\AIRCRAFT\\*.*", t);
    h = FindFirstFileA(pat, &d);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(d.cFileName, ".") && strcmp(d.cFileName, "..")) break;
    } while (FindNextFileA(h, &d));
    FindClose(h);
    if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return;
    _splitpath(d.cFileName, NULL, NULL, nm, x);
    if (ext_lookup(x, &type) == 0) return;
    snprintf(path, sizeof path, "acfix\\t%u\\AIRCRAFT\\%s", t, d.cFileName);
    DeleteFileA(path);
    printf("tree %u: removed %s from the front of the listing\n", t, d.cFileName);
}

static void make_headers(void)
{
    static const char *const bad_tags[] = { "FSIX", "FSAM", "FTIM", "XSIM", "AAF4", "AAG5",
                                            "ABF5", "FAF5", "ASIM", "fsim" };
    unsigned k;
    for (k = 0; k < NHDR; k++) {
        uint8_t *h = hdr[k];
        memset(h, 0, 0x40);
        *(uint32_t *)h = 0x24;
        *(uint32_t *)(h + 4) = rng();
        *(uint32_t *)(h + 0x18) = k % 5 == 0 ? 0 : 0x68 + rng() % 0x100;
        memcpy(h + 0x1c, k % 2 ? "FSIM" : "AAF5", 4);
        if (k >= 10) {
            if (k == 10) *(uint32_t *)h = 0x23;
            else memcpy(h + 0x1c, bad_tags[rng() % 10], 4);
        }
        /* each fake's Linkage */
        *(uint32_t *)mdl_link[k] = 31;
        *(void **)(mdl_link[k] + 4) = (void *)fake_nop;
        *(void **)(mdl_link[k] + 8) = (void *)fake_nop;
        *(void **)(mdl_link[k] + 0x18) = (void *)mdl_attach;
        *(void **)(mdl_link[k] + 0x1c) = (void *)mdl_state;
        *(void **)(mdl_link[k] + 0x20) = (void *)mdl_detach;
    }
    for (k = 0; k < NSIM; k++) {
        *(uint32_t *)sim_link[k] = 25;
        *(void **)(sim_link[k] + 4) = (void *)fake_nop;
        *(void **)(sim_link[k] + 8) = (void *)fake_nop;
    }
}

/* ---- scenarios ------------------------------------------------------------------- */

enum { OP_LOAD, OP_UNLOAD, OP_LIST, OP_PICK, OP_QUERY, OP_DRAW, OP_SELECT, OP_RELEASE,
       OP_MODEL, OP_MODEL_UNLOAD, OP_KIND, OP_PREVIEW, OP_POKE, OP_COUNT };
static const char *const op_names[OP_COUNT] = {
    "load", "unload", "list", "pick", "query", "draw", "select", "release", "model",
    "model_unload", "kind", "preview", "poke"
};

struct op { int kind, who; uint32_t a, b; char title[48]; };
#define MAXOPS 40
struct scenario {
    unsigned tree, nops;
    uint16_t g11b6;
    struct op op[MAXOPS];
};

static void make_scenario(struct scenario *s)
{
    struct tree *t;
    unsigned n;
    memset(s, 0, sizeof *s);
    s->tree = rng() % NTREES;
    t = &trees[s->tree];
    s->g11b6 = (uint16_t)rng();
    n = 4 + rng() % (MAXOPS - 4);
    while (s->nops < n) {
        struct op *o = &s->op[s->nops++];
        unsigned r = rng() % 100;
        o->who = rng() % 2;                       /* GLOBAL's record, or MAIN's own */
        if (rng() % 20 == 0) o->who = 2;          /* null */
        snprintf(o->title, sizeof o->title, "%s", t->titles[rng() % t->ntitles]);
        o->a = rng();
        o->b = rng();
        if (r < 16) o->kind = OP_LOAD;
        else if (r < 20) o->kind = OP_UNLOAD;
        else if (r < 25) o->kind = OP_LIST;
        else if (r < 31) o->kind = OP_PICK;
        else if (r < 47) o->kind = OP_QUERY;
        else if (r < 51) o->kind = OP_DRAW;
        else if (r < 59) o->kind = OP_SELECT;
        else if (r < 63) o->kind = OP_RELEASE;
        else if (r < 74) o->kind = OP_MODEL;
        else if (r < 80) o->kind = OP_MODEL_UNLOAD;
        else if (r < 84) o->kind = OP_KIND;
        else if (r < 96) o->kind = OP_PREVIEW;
        else o->kind = OP_POKE;
    }
}

/* ---- running ---------------------------------------------------------------------- */

static unsigned long cover_ops[OP_COUNT], cover_list_n, cover_list_empty, cover_query_hit[14],
                     cover_select_ok, cover_load_ok, cover_model_ok;

/* Zero the stack below the caller, so the reads retail makes of stack it never
 * set see what the reimplementation starts them as. */
static void __attribute__((noinline)) scrub(void)
{
    volatile uint32_t pad[0xc00];
    unsigned i;
    for (i = 0; i < 0xc00; i++) pad[i] = 0;
    __asm__ volatile("" : : "r"(pad) : "memory");
}

static struct aircraft *who(int w)
{
    return w == 0 ? (struct aircraft *)(global_blob + 0x11e0) : w == 1 ? &aux : NULL;
}

static void put_query_result(int32_t r, uint32_t len)
{
    unsigned i, end = len < sizeof qbuf ? len : sizeof qbuf;
    put(" =%d [", r);
    for (i = 0; i < end && qbuf[i]; i++) put("%c", qbuf[i]);
    if (i == end && end < sizeof qbuf) put("|%02x", (uint8_t)qbuf[end]);
    put("]");
}

static void run(struct side *sd, const struct scenario *s, int count)
{
    unsigned i;
    int32_t r, n;
    uint8_t *entries;
    char cwd[MAX_PATH], dir[MAX_PATH];

    cur = sd;
    counting = count;
    memcpy(sd->image + IMAGE_LO, sd->pristine + IMAGE_LO, IMAGE_HI - IMAGE_LO);
    memset(global_blob, 0, GLOBAL_SIZE);
    *(uint16_t *)(global_blob + 0x11b6) = s->g11b6;
    memset(&aux, 0, sizeof aux);
    memset(arena, 0, arena_used);
    arena_used = 0;
    tlen = 0;
    tlog[0] = 0;
    GetCurrentDirectoryA(sizeof cwd, cwd);
    snprintf(dir, sizeof dir, "acfix\\t%u", s->tree);
    SetCurrentDirectoryA(dir);
    chain->init();
    chain->add(20, on_list20);

    put("\nINIT");
    sd->link->init();
    for (i = 0; i < s->nops; i++) {
        const struct op *o = &s->op[i];
        struct aircraft *a = who(o->who);
        if (count) cover_ops[o->kind]++;
        switch (o->kind) {
        case OP_LOAD:
            put("\nLOAD ");
            put_s(o->title);
            scrub();
            r = sd->link->load(o->title);
            put(" =%d", r);
            if (count && r) cover_load_ok++;
            break;
        case OP_UNLOAD:
            put("\nUNLOAD");
            r = sd->link->unload();
            put(" =%d", r);
            break;
        case OP_LIST:
        case OP_PICK:
            /* MAIN's aircraft dialog: list, maybe load the chosen entry from
             * the list itself, free the list */
            put(o->kind == OP_LIST ? "\nLIST" : "\nPICK");
            n = -1;
            entries = (uint8_t *)1;
            r = sd->link->list(&n, &entries);
            put(" =%d n=%d ", r, n);
            put_ptr(entries);
            if (count) { if (n > 0) cover_list_n += n; else cover_list_empty++; }
            if (o->kind == OP_PICK && n > 0) {
                char *title = (char *)entries + 0xa08 * (o->a % n);
                put("\n  load ");
                put_s(title);
                scrub();
                r = sd->link->load(title);
                put(" =%d", r);
                if (count && r) cover_load_ok++;
            }
            if (n > 0) h_free((void **)&entries);
            break;
        case OP_QUERY: {
            static const uint32_t lens[] = { 0x104, 0x104, 0x104, 0x10, 1, 0, 0x100 };
            int32_t key = (int32_t)(o->a % 14);
            uint32_t len = lens[o->b % 7];
            memset(qbuf, 0xcc, sizeof qbuf);
            put("\nQUERY %d %d %#x", o->who, key, len);
            r = sd->link->query(a, key, qbuf, len);
            put_query_result(r, len);
            if (count && r) cover_query_hit[key]++;
            break;
        }
        case OP_DRAW:
            put("\nDRAW %d", o->who);
            r = sd->link->draw(a);
            put(" =%d", r);
            break;
        case OP_SELECT:
            put("\nSELECT %d ", o->who);
            put_s(o->a % 13 ? o->title : NULL);
            r = sd->link->select(a, o->a % 13 ? o->title : NULL);
            put(" =%d", r);
            if (count && r) cover_select_ok++;
            break;
        case OP_RELEASE:
            if (!a) break;
            put("\nRELEASE %d", o->who);
            r = sd->link->release(a);
            put(" =%d", r);
            break;
        case OP_MODEL: {
            int32_t kind = o->a % 8 == 0 ? (int32_t)(o->a >> 8) % 4 : 1 + (int32_t)(o->a >> 8) % 2;
            put("\nMODEL %d %d", o->who, kind);
            r = sd->link->model_load(a, kind);
            put(" =%d", r);
            if (count && r) cover_model_ok++;
            break;
        }
        case OP_MODEL_UNLOAD:
            put("\nMODEL_UNLOAD %d", o->who);
            r = sd->link->model_unload(a);
            put(" =%d", r);
            break;
        case OP_KIND:
            put("\nKIND %d", o->who);
            r = sd->link->model_kind(a);
            put(" =%d", r);
            break;
        case OP_PREVIEW:
            /* MAIN's second aircraft, as at 0x207e1378 */
            put("\nPREVIEW ");
            put_s(o->title);
            r = sd->link->model_unload(&aux);
            put(" u=%d", r);
            r = sd->link->release(&aux);
            put(" r=%d", r);
            r = sd->link->select(&aux, o->title);
            put(" s=%d", r);
            if (count && r) cover_select_ok++;
            r = sd->link->model_load(&aux, 1);
            put(" m=%d", r);
            if (count && r) cover_model_ok++;
            r = sd->link->model_kind(&aux);
            put(" k=%d", r);
            break;
        case OP_POKE:
            *(uint16_t *)(global_blob + 0x11b6) = (uint16_t)o->a;
            put("\nPOKE 11b6=%04x", (uint16_t)o->a);
            break;
        }
    }
    put("\nDEINIT");
    sd->link->deinit();
    put("\nEND");
    SetCurrentDirectoryA(cwd);
}

/* ---- loading ---------------------------------------------------------------------- */

static unsigned fault_scenario;
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    printf("\nFAULT %#lx at %p in %s, scenario %u\n", e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress, cur ? cur->name : "setup", fault_scenario);
    printf("transcript tail: ...%s\n", tlen > 1500 ? tlog + tlen - 1500 : tlog);
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void bind_imports(struct import *im)
{
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host_blob; break;
        case 2:  im->link = global_blob; break;
        case 4:  im->link = chain_fwd; break;
        case 14: im->link = g3d_blob; break;
        case 15: im->link = panels_blob; break;
        case 20: im->link = util_blob; break;
        default: printf("unexpected import %u\n", im->id); exit(2);
        }
    }
}

static int load_side(struct side *sd, const char *path, const char *name)
{
    struct import *imports;
    sd->name = name;
    sd->dll = LoadLibraryA(path);
    if (!sd->dll) { printf("cannot load %s (%lu)\n", path, GetLastError()); return 0; }
    imports = (struct import *)GetProcAddress(sd->dll, "ImportTable");
    sd->link = (struct ac_linkage *)GetProcAddress(sd->dll, "Linkage");
    if (!imports || !sd->link || sd->link->id != 23) { printf("%s: not ACONTAIN\n", path); return 0; }
    sd->image = (uint8_t *)sd->link - 0x38;
    if ((uint8_t *)imports != sd->image) { printf("%s: ImportTable is not at Linkage-0x38\n", path); return 0; }
    bind_imports(imports);
    memcpy(sd->pristine, sd->image, IMAGE_HI);
    return 1;
}

static void dump_diff(const char *what, const uint8_t *r, const uint8_t *o, unsigned lo, unsigned n)
{
    unsigned i, shown = 0;
    for (i = lo; i < n && shown < 10; i++)
        if (r[i] != o[i]) {
            printf("  %s+%#x: retail %02x ours %02x\n", what, i, r[i], o[i]);
            shown++;
        }
}

#define SET(blob, off, fn) (*(void **)((blob) + (off)) = (void *)(fn))

int main(int argc, char **argv)
{
    static char rlog[sizeof tlog];
    static uint8_t rimage[IMAGE_HI], rglobal[GLOBAL_SIZE], rarena[ARENA_SIZE];
    static struct side retail, ours;
    static struct scenario s;
    struct aircraft raux;
    unsigned v, n = argc > 1 ? (unsigned)atoi(argv[1]) : 20000, bad = 0, k;
    struct import *im;
    uint32_t rused;
    HMODULE ch;

    SetUnhandledExceptionFilter(on_fault);
    load_ext_table("FLTSIM98_host.exe");
    fll = LoadLibraryExA("ENGLISH.FLL", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!fll) { printf("cannot load ENGLISH.FLL\n"); return 2; }

    *(uint32_t *)host_blob = 1;
    SET(host_blob, 0x0c8, h_cfg_string);
    SET(host_blob, 0x0f0, h_profile);
    SET(host_blob, 0x13c, h_close);
    SET(host_blob, 0x158, h_exists);
    SET(host_blob, 0x15c, h_find_close);
    SET(host_blob, 0x160, h_find_next);
    SET(host_blob, 0x164, h_find_first);
    SET(host_blob, 0x17c, h_open);
    SET(host_blob, 0x184, h_join);
    SET(host_blob, 0x188, h_split);
    SET(host_blob, 0x194, h_seek);
    SET(host_blob, 0x19c, h_read);
    SET(host_blob, 0x20c, h_alloc);
    SET(host_blob, 0x210, h_realloc);
    SET(host_blob, 0x214, h_free);
    SET(host_blob, 0x218, h_memset);
    SET(host_blob, 0x22c, h_qsort);
    SET(host_blob, 0x26c, h_module_load);
    SET(host_blob, 0x270, h_module_free);
    SET(host_blob, 0x2a0, h_loadstring);
    SET(host_blob, 0x2a8, h_strcpy);
    SET(host_blob, 0x2ac, h_strncpy);
    SET(host_blob, 0x2b0, h_strcat);
    SET(host_blob, 0x2b8, h_strlen);
    SET(host_blob, 0x2bc, h_strcmp);
    SET(host_blob, 0x2c0, h_stricmp);
    *(uint32_t *)g3d_blob = 14;
    SET(g3d_blob, 0x38, g3d_draw);
    SET(g3d_blob, 0x58, g3d_flush);
    *(uint32_t *)panels_blob = 15;
    SET(panels_blob, 0x18, panels_attach);
    SET(panels_blob, 0x1c, panels_detach);
    *(uint32_t *)util_blob = 20;
    SET(util_blob, 0xa4, util_alert);

    ch = LoadLibraryA("CHAIN_retail.dll");
    if (!ch) { printf("cannot load CHAIN_retail.dll\n"); return 2; }
    chain = (struct chain_linkage *)GetProcAddress(ch, "Linkage");
    for (im = (struct import *)GetProcAddress(ch, "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : (void *)global_blob;
    *(uint32_t *)chain_fwd = 4;
    SET(chain_fwd, 0x20, c_dispatch);

    if (!load_side(&retail, "ACONTAIN_retail.dll", "retail")) return 2;
    if (!load_side(&ours, "ACONTAIN_ours.dll", "openfs98")) return 2;
    if (memcmp(retail.pristine + IMAGE_LO, ours.pristine + IMAGE_LO, IMAGE_HI - IMAGE_LO)) {
        printf("initial images differ\n");
        dump_diff("img", retail.pristine, ours.pristine, IMAGE_LO, IMAGE_HI);
        bad++;
    }
    /* colour names for the fixtures, from retail's own table */
    {
        uint8_t *rdata = retail.image - 0x1000;
        for (k = 0; k < 64 && *(uint32_t *)(rdata + 8 * k); k++)
            colour_names[ncolour_names++] = *(const char **)(rdata + 8 * k);
    }

    make_headers();
    deltree("acfix");
    CreateDirectoryA("acfix", NULL);
    for (k = 0; k < NTREES; k++) {
        write_tree(k);
        check_first_entry(k);
    }

    for (v = 0; v < n; v++) {
        make_scenario(&s);
        fault_scenario = v;
        run(&retail, &s, 1);
        memcpy(rlog, tlog, tlen + 1);
        memcpy(rimage, retail.image, IMAGE_HI);
        memcpy(rglobal, global_blob, GLOBAL_SIZE);
        raux = aux;
        rused = arena_used;
        memcpy(rarena, arena, rused);
        run(&ours, &s, 0);
        if (strcmp(rlog, tlog) || memcmp(rimage + IMAGE_LO, ours.image + IMAGE_LO, IMAGE_HI - IMAGE_LO)
            || memcmp(rglobal, global_blob, GLOBAL_SIZE) || memcmp(&raux, &aux, sizeof aux)
            || rused != arena_used || memcmp(rarena, arena, rused)) {
            if (bad++ < 4) {
                unsigned at = 0;
                while (rlog[at] && rlog[at] == tlog[at]) at++;
                printf("scenario %u (tree %u) differs", v, s.tree);
                if (rlog[at] || tlog[at])
                    printf(" in the transcript at byte %u:\n  retail ...%.500s\n  ours   ...%.500s\n",
                           at, rlog + (at > 300 ? at - 300 : 0), tlog + (at > 300 ? at - 300 : 0));
                else
                    printf(":\n");
                dump_diff("img", rimage, ours.image, IMAGE_LO, IMAGE_HI);
                dump_diff("global", rglobal, global_blob, 0, GLOBAL_SIZE);
                dump_diff("aux", (uint8_t *)&raux, (uint8_t *)&aux, 0, sizeof aux);
                if (rused != arena_used) printf("  arena used: retail %#x ours %#x\n", rused, arena_used);
                dump_diff("arena", rarena, arena, 0, rused < arena_used ? rused : arena_used);
            }
            if (getenv("ACSTOP")) break;        /* for deliberate breaks: the first is enough */
        }
        if (getenv("ACDUMP") && v == (unsigned)atoi(getenv("ACDUMP"))) printf("%s\n", rlog);
    }

    printf("ops:");
    for (k = 0; k < OP_COUNT; k++) printf(" %s %lu", op_names[k], cover_ops[k]);
    printf("\nloads ok %lu, selects ok %lu, models ok %lu; listed %lu entries, %lu empty lists\n",
           cover_load_ok, cover_select_ok, cover_model_ok, cover_list_n, cover_list_empty);
    printf("alerts f540-f544: %lu %lu %lu %lu %lu\n", cover_alert[0], cover_alert[1],
           cover_alert[2], cover_alert[3], cover_alert[4]);
    printf(".MDL loads missing/bad/ok %lu/%lu/%lu, .SIM %lu/%lu/%lu\nheaders:",
           cover_mdl_load[0], cover_mdl_load[1], cover_mdl_load[2],
           cover_sim_load[0], cover_sim_load[1], cover_sim_load[2]);
    for (k = 0; k < NHDR; k++) printf(" %lu", cover_hdr[k]);
    printf("\naliases found %lu: under AIRCRAFT %lu, as given %lu, neither %lu\n", cover_alias[0],
           cover_alias[1], cover_alias[2], cover_alias[3]);
    printf("colour compares: early out %lu, hit %lu, on %lu; AIRtoCONTAINER hits %lu\n",
           cover_colour[0], cover_colour[1], cover_colour[2], cover_cfg_hit);
    printf("chunks: short reads %lu, seeks refused %lu", cover_short, cover_seekfail);
    printf("\nquery hits by key:");
    for (k = 0; k < 14; k++) printf(" %lu", cover_query_hit[k]);
    printf("\n%u scenarios, %u differ\n", n, bad);
    if (!bad) printf("IDENTICAL\n");
    return bad != 0;
}
