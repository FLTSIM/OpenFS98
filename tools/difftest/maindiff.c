/* Differential test: retail MAIN.DLL against the OpenFS98 build, a suite
 * per slice.
 *
 * MAIN is transliterated a slice at a time (docs/modules/main.md).  Both
 * DLLs are loaded into one process; each suite drives the services of its
 * slice on both sides, one after the other, over the same script, and
 * compares the transcript of every call either makes and its return, and
 * MAIN's whole .data image: words the loader relocated are compared as
 * what they point at (an offset into the image, retail's code address),
 * and the Linkage is left out, its slots being filled a slice at a time.
 * Internal routines of a slice, whose callers are not done yet, are called
 * directly: retail's at their addresses, ours through exports named
 * main_<routine>.
 *
 * Suites:
 *
 *   records   the situation-record layer, .text+0x15c80..+0x15fff: the
 *             writer (+0x5c, +0x60, +0x64, and the save that sends list
 *             0x14 message 0x23 around them), the reader (the dispatch of
 *             a record as message 0x22, +0x54, +0x58) and the finder
 *             (+0x68, +0x6c).  The host's file services are a model over
 *             files kept in memory; list 0x14's listeners are scripted
 *             modules that write records -- whole, short, too long, begun
 *             and never ended, or none -- and read them in chunks.  The
 *             finder runs over generated situation files: records of both
 *             header sizes, the terminator, or none, lengths below the
 *             header's own, past the end of the file, and now and then
 *             huge.
 *
 * A fault is compared as an outcome and ends the scenario.  So is a hang:
 * a watchdog moves a call that has spun for HANG_MS out of the module
 * (the finder's walk never moves on from a record of length 0, which a
 * file cut short inside a header can show it, reading on into what follows
 * the block), the transcript says HANG and the scenario ends there with
 * its state not compared, since where the spin was cut is timing
 * (docs/method.md, "a hang is an outcome").
 *
 * Build: make maindiff     Run: make maindiff-run [SUITE=records] [N=...]
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define RETAIL_BASE  0x207e0000u
#define DATA_RVA     0x20000u
#define IMAGE_SIZE   0x2ee00u
#define LINKAGE      0x0c0u
#define LINKAGE_END  (LINKAGE + 51 * 4)
#define ARENA_SIZE   (4u << 20)
#define TRANSCRIPT_SIZE (8u << 20)
#define MAX_RELOCS   1024
#define MAX_PAIRS    64

struct import { uint32_t id; void *link; };

struct side {
    const char *name;
    HMODULE module;
    uint8_t *lo, *hi;
    struct import *imports;
    uint8_t *link;
    uint8_t *image;
    uint8_t *pristine;
    char *transcript;
    uint32_t length;
};

static struct side retail, ours;
static struct side *cur;
static uint32_t relocs[MAX_RELOCS], nrelocs;
static uint8_t host[0x340], global[0x1400], stub_module[0x400];
static uint8_t chain[0x24];
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t pad_byte;
static uint32_t rng, scenario_seed, scenario, op_number;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs_retail, npairs_ours;

#define SET_SLOT(table, off, fn) (*(void **)((table) + (off)) = (void *)(fn))
#define IMG32(o) (*(uint32_t *)(cur->image + (o)))

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

    if (cur == NULL || cur->length >= TRANSCRIPT_SIZE - 8192)
        return;
    va_start(args, format);
    n = vsnprintf(cur->transcript + cur->length, TRANSCRIPT_SIZE - cur->length, format, args);
    va_end(args);
    if (n > 0)
        cur->length += (uint32_t)n;
}

static uint32_t wanted_tags[8], nwanted;
static uint8_t fixtures[0x80000] __attribute__((aligned(16)));
static uint32_t fix_used;

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

static int in_fixtures(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;
    return b >= fixtures && b < fixtures + sizeof fixtures;
}

static uint32_t pair_code(uint32_t v)
{
    uint32_t *list = cur == &retail ? pair_retail : pair_ours;
    uint32_t *n = cur == &retail ? &npairs_retail : &npairs_ours;
    uint32_t i;

    for (i = 0; i < *n; i++)
        if (list[i] == v)
            return i + 1;
    if (*n < MAX_PAIRS)
        list[(*n)++] = v;
    return *n;
}

static void put_ptr(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();

    if (v == 0)
        put("null");
    else if (in_arena(v))
        put("arena+%#x", v - (uint32_t)(uintptr_t)arena);
    else if (in_image(v))
        put("image+%#x", v - (uint32_t)(uintptr_t)cur->image);
    else if (in_fixtures(v))
        put("fix+%#x", v - (uint32_t)(uintptr_t)fixtures);
    else if ((void *)b >= tib->StackLimit && (void *)b < tib->StackBase)
        put("stack");
    else if (cur != NULL && b >= cur->lo && b < cur->hi)
        put("code%u", pair_code(v));
    else
        put("%#x", v);
}

static void put_bytes(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;

    put("[");
    for (i = 0; i < n && i < 0x20; i++)
        put("%02x", b[i]);
    if (n > 0x20)
        put("..%08x", digest(b, n, 0x811c9dc5u));
    put("]");
}

/* ---- faults ---------------------------------------------------------------------- */

static void *fault_jb[5];
static volatile int in_main;
static volatile int faulted;
static unsigned long cover_faults, cover_av, cover_huge, cover_hangs;
static volatile int hung;
static volatile DWORD call_start;
static HANDLE main_thread;

#define HANG_MS 300

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

static void stub_fault(const char *what)
{
    put(" FAULT(%s)", what);
    if (counting) cover_faults++;
    in_main = 0;
    faulted = 1;
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
    if (!in_main || cur == NULL || code != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    put(" FAULT(av)");
    if (counting) {
        cover_faults++;
        cover_av++;
    }
    in_main = 0;
    faulted = 1;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* A call spinning past HANG_MS inside the module is moved out of it. */
static DWORD WINAPI watchdog(void *arg)
{
    CONTEXT ctx;

    (void)arg;
    for (;;) {
        Sleep(20);
        if (!in_main || GetTickCount() - call_start < HANG_MS)
            continue;
        SuspendThread(main_thread);
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (in_main && GetTickCount() - call_start >= HANG_MS && GetThreadContext(main_thread, &ctx)
            && cur && (uint8_t *)(uintptr_t)ctx.Eip >= cur->lo && (uint8_t *)(uintptr_t)ctx.Eip < cur->hi) {
            hung = 1;
            in_main = 0;
            faulted = 1;
            ctx.Eip = (DWORD)(uintptr_t)fault_land;
            SetThreadContext(main_thread, &ctx);
        }
        ResumeThread(main_thread);
    }
    return 0;
}

/* ---- the heap ------------------------------------------------------------------- */

#define LIVE  0x0a110cu
#define FREED 0xf4eeu

static void *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;

    if (arena_used + n + 0x100 > ARENA_SIZE)
        stub_fault("arena");
    p = arena + arena_used;
    arena_used += n;
    memset(p, 0, 16);
    *(uint32_t *)p = size;
    *(uint32_t *)(p + 4) = LIVE;
    memset(p + 16 + size, pad_byte, n - 16 - size);
    return p + 16;
}

static int32_t STDCALL h_alloc(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    void *p;

    put(" alloc(%d,%#x,", flags, size);
    put_ptr((uint32_t)(uintptr_t)out);
    put(",%d)=", zero);
    if (size > 0x100000) {
        if (counting) cover_huge++;
        stub_fault("alloc huge");     /* flags 1: the host's out-of-memory exit */
    }
    p = arena_get(size);
    memset(p, zero ? 0 : 0xa5, size);
    put_ptr((uint32_t)(uintptr_t)p);
    *out = p;
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    uint8_t *b = (uint8_t *)*pp;

    put(" free(");
    put_ptr((uint32_t)(uintptr_t)pp);
    put(":");
    put_ptr((uint32_t)(uintptr_t)b);
    put(")");
    if (b != NULL) {
        if (!in_arena((uint32_t)(uintptr_t)b) || *(uint32_t *)(b - 12) != LIVE)
            stub_fault("heap free");
        memset(b, 0xdd, *(uint32_t *)(b - 16));
        *(uint32_t *)(b - 12) = FREED;
    }
    *pp = NULL;
    return 0;
}

/* MSVCRT memmove leaves 7 in ecx on the CD's 1997 DLL (docs/modules/analysis.md);
 * nothing in this slice reads it. */
static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    put(" memmove(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_ptr((uint32_t)(uintptr_t)s);
    put(",%#x)", n);
    if (n > 0x100000)
        stub_fault("memmove huge");
    put_bytes(s, n);
    memmove(d, s, n);
    return 0;
}

/* ---- the host's files, in memory ------------------------------------------------- */

#define MAX_FILES 8
#define FILE_MAX  0x4000u

struct vfile {
    char name[64];
    int used;
    uint8_t bytes[FILE_MAX];
    uint32_t size;
};

/* The host's block: { HANDLE, char *path, mode, ext } (host.h); the handle
 * here is the file's index, and the position is kept beside it. */
struct hblock {
    uint32_t handle, path, mode, ext;
    uint32_t pos;
};

static struct vfile files[MAX_FILES];

static struct vfile *vfile_find(const char *name)
{
    int i;

    for (i = 0; i < MAX_FILES; i++)
        if (files[i].used && !_stricmp(files[i].name, name))
            return &files[i];
    return NULL;
}

static void put_name(const char *s)
{
    if (s == NULL)
        put("null");
    else
        put("'%.60s'", s);
}

static int32_t STDCALL h_exists(const char *path, int32_t ext)
{
    int32_t r = vfile_find(path) ? 0 : -1;

    put(" exists(");
    put_name(path);
    put(",%d)=%d", ext, r);
    return r;
}

static int32_t STDCALL h_open(struct hblock **out, const char *path, int32_t ext, int32_t mode)
{
    struct vfile *f = vfile_find(path);
    struct hblock *b;

    put(" open(");
    put_ptr((uint32_t)(uintptr_t)out);
    put(",");
    put_name(path);
    put(",%d,%d)", ext, mode);
    if (f == NULL || mode < 0 || mode > 3 || rnd_n(40) == 0) {
        *out = NULL;
        put("=-1");
        return -1;
    }
    b = (struct hblock *)arena_get(sizeof *b);
    b->handle = (uint32_t)(f - files);
    b->path = 0;                    /* the caller's pointer: each side's own */
    b->mode = (uint32_t)mode;
    b->ext = (uint32_t)ext;
    b->pos = mode == 3 ? f->size : 0;
    if (mode == 2)
        f->size = 0;
    *out = b;
    put("=");
    put_ptr((uint32_t)(uintptr_t)b);
    return 0;
}

static int32_t STDCALL h_close(struct hblock *b)
{
    put(" close(");
    put_ptr((uint32_t)(uintptr_t)b);
    put(")");
    return 0;
}

static int32_t STDCALL h_size(struct hblock *b, uint32_t *size)
{
    put(" size(");
    put_ptr((uint32_t)(uintptr_t)b);
    put(")");
    if (b == NULL)
        return -1;
    *size = files[b->handle].size;
    put("=%#x", *size);
    return 0;
}

static int32_t STDCALL h_read(struct hblock *b, void *buf, uint32_t n, uint32_t *got)
{
    struct vfile *f;
    uint32_t k;

    put(" read(");
    put_ptr((uint32_t)(uintptr_t)b);
    put(",");
    put_ptr((uint32_t)(uintptr_t)buf);
    put(",%#x,", n);
    put_ptr((uint32_t)(uintptr_t)got);
    put(")");
    if (got != NULL)
        *got = 0;
    if (b == NULL)
        return -1;
    f = &files[b->handle];
    k = b->pos < f->size ? f->size - b->pos : 0;
    if (k > n)
        k = n;
    memcpy(buf, f->bytes + b->pos, k);
    b->pos += k;
    if (got != NULL)
        *got = k;
    put("=%#x", k);
    return 0;
}

static unsigned long cover_short_writes;

static int32_t STDCALL h_write(struct hblock *b, const void *buf, uint32_t n, uint32_t *wrote)
{
    struct vfile *f;
    uint32_t k;

    put(" write(");
    put_ptr((uint32_t)(uintptr_t)b);
    put(",");
    put_ptr((uint32_t)(uintptr_t)buf);
    put(",%#x,", n);
    put_ptr((uint32_t)(uintptr_t)wrote);
    put(")");
    if (wrote != NULL)
        *wrote = 0;
    if (b == NULL)
        return -1;
    put_bytes(buf, n);
    f = &files[b->handle];
    k = n;
    /* now and then the disk fills */
    if (b->pos + k > FILE_MAX || rnd_n(60) == 0) {
        k = b->pos < FILE_MAX ? rnd_n(FILE_MAX - b->pos < k ? FILE_MAX - b->pos + 1 : k + 1) : 0;
        if (counting) cover_short_writes++;
    }
    memcpy(f->bytes + b->pos, buf, k);
    b->pos += k;
    if (b->pos > f->size)
        f->size = b->pos;
    if (wrote != NULL)
        *wrote = k;
    put("=%#x", k);
    return 0;
}

/* ---- CHAIN ------------------------------------------------------------------------ */

typedef uint32_t (STDCALL *s0)(void);
typedef uint32_t (STDCALL *s1)(uint32_t);
typedef uint32_t (STDCALL *s2)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *s3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (STDCALL *s4)(uint32_t, uint32_t, uint32_t, uint32_t);
#define SVC(off) (*(void **)(cur->link + (off)))

static void listen_save(void);
static void listen_record(uint32_t rec);

static uint32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    put(" chainsend(%#x,%#x,", list, a);
    put_ptr(b);
    put(")");
    if (list == 0x14 && a == 0x23)
        listen_save();
    else if (list == 0x14 && a == 0x22)
        listen_record(b);
    put(" <-");
    return 0x5e17;
}

static void build_stubs(void)
{
    uint32_t i;

#define TRAPS(table, id) \
    for (i = 4; i < sizeof table; i += 4) *(uint32_t *)(table + i) = 0xde000000u | ((id) << 16) | i
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(stub_module, 0xff);
#undef TRAPS
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x13c, h_close);
    SET_SLOT(host, 0x158, h_exists);
    SET_SLOT(host, 0x17c, h_open);
    SET_SLOT(host, 0x19c, h_read);
    SET_SLOT(host, 0x1a4, h_size);
    SET_SLOT(host, 0x1b0, h_write);
    SET_SLOT(host, 0x20c, h_alloc);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x21c, h_memmove);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x20, chain_send);
}

/* ---- loading ----------------------------------------------------------------------- */

/* Every relocation into .data, from retail's own .reloc. */
static void read_relocs(const char *path)
{
    FILE *f = fopen(path, "rb");
    static uint8_t blob[0x40000];
    uint32_t pe, n, o, i, rel_raw = 0, rel_size = 0, c = 0;

    if (f == NULL) { printf("cannot read %s\n", path); exit(2); }
    n = (uint32_t)fread(blob, 1, sizeof blob, f);
    fclose(f);
    (void)n;
    pe = *(uint32_t *)(blob + 0x3c);
    n = *(uint16_t *)(blob + pe + 6);
    o = pe + 24 + *(uint16_t *)(blob + pe + 20);
    for (i = 0; i < n; i++, o += 40)
        if (!memcmp(blob + o, ".reloc", 6)) {
            rel_size = *(uint32_t *)(blob + o + 8);
            rel_raw = *(uint32_t *)(blob + o + 20);
        }
    while (c + 8 <= rel_size) {
        uint32_t page = *(uint32_t *)(blob + rel_raw + c), size = *(uint32_t *)(blob + rel_raw + c + 4), e;
        if (size == 0)
            break;
        for (e = 8; e < size; e += 2) {
            uint16_t x = *(uint16_t *)(blob + rel_raw + c + e);
            uint32_t at = page + (x & 0xfff);
            if (x >> 12 == 3 && at >= DATA_RVA && at < DATA_RVA + IMAGE_SIZE && nrelocs < MAX_RELOCS)
                relocs[nrelocs++] = at - DATA_RVA;
        }
        c += size;
    }
    if (nrelocs != 635) { printf("%u relocations into .data, expected 635\n", nrelocs); exit(2); }
}

static void load_side(struct side *s, const char *path, const char *name)
{
    struct import *im;
    IMAGE_NT_HEADERS *nt;

    memset(s, 0, sizeof *s);
    s->name = name;
    s->transcript = malloc(TRANSCRIPT_SIZE);
    s->pristine = malloc(IMAGE_SIZE);
    s->module = LoadLibraryA(path);
    if (s->module == NULL) { printf("cannot load %s (%lu)\n", path, GetLastError()); exit(2); }
    s->imports = (struct import *)GetProcAddress(s->module, "ImportTable");
    s->link = (uint8_t *)GetProcAddress(s->module, "Linkage");
    if (s->imports == NULL || s->link == NULL || *(uint32_t *)s->link != 3) {
        printf("%s: not MAIN\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if (s->link != s->image + LINKAGE) { printf("%s: Linkage is not ImportTable+0xc0\n", path); exit(2); }
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)s->module + ((IMAGE_DOS_HEADER *)s->module)->e_lfanew);
    s->lo = (uint8_t *)s->module;
    s->hi = s->lo + nt->OptionalHeader.SizeOfImage;
    for (im = s->imports; im->id; im++)
        im->link = im->id == 1 ? (void *)host : im->id == 2 ? (void *)global :
                   im->id == 4 ? (void *)chain : (void *)stub_module;
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* A retail routine by its retail address, or ours by name. */
static void *internal(uint32_t retail_va, const char *name)
{
    void *p;

    if (cur == &retail)
        return (uint8_t *)retail.module + (retail_va - RETAIL_BASE);
    p = (void *)GetProcAddress(ours.module, name);
    if (p == NULL) { printf("ours has no %s\n", name); exit(2); }
    return p;
}

/* ---- the comparable image ------------------------------------------------------------ */

static void put_state(void)
{
    static uint8_t img[IMAGE_SIZE];
    uint32_t i;

    memcpy(img, cur->image, IMAGE_SIZE);
    for (i = 0; i < nrelocs; i++) {
        uint32_t at = relocs[i], v;
        if (at + 4 > IMAGE_SIZE)
            continue;
        v = *(uint32_t *)(img + at);
        if (in_image(v))
            v = 0xda000000u | (v - (uint32_t)(uintptr_t)cur->image);
        else if (cur == &retail && (uint8_t *)(uintptr_t)v >= retail.lo && (uint8_t *)(uintptr_t)v < retail.hi)
            v = v - (uint32_t)(uintptr_t)retail.lo + RETAIL_BASE;
        *(uint32_t *)(img + at) = v;
    }
    memset(img + LINKAGE, 0, LINKAGE_END - LINKAGE);
    for (i = 4; i < 0xb8; i += 8)                  /* the import links */
        *(uint32_t *)(img + i) = 0;
    put("\n  state img=%08x heap=%#x/%08x", digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena, arena_used, 0x811c9dc5u));
    if (getenv("MAIN_IMAGE")) {
        put("\n  words");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i) != *(uint32_t *)(cur->pristine + i))
                put(" %x:%08x", i, *(uint32_t *)(img + i));
    }
}

/* ---- suite: records ----------------------------------------------------------------- */

#define S_FIND_PATH  0x40dc
#define S_RECORD     0x9754
#define S_READ_AT    0x9758
#define S_FILE       0x975c
#define S_PENDING    0x9760
#define S_READ_LEFT  0xd144

static unsigned long cover_saves, cover_records_written, cover_unended, cover_overlong,
    cover_reads, cover_read_chunks, cover_stale_reads, cover_finds, cover_found, cover_missing,
    cover_terminated, cover_past_end, cover_short_length, cover_big_tags, cover_no_file,
    cover_flight;

static uint8_t body_buf[0x2000];

static void fill_random(uint8_t *p, uint32_t n)
{
    while (n--)
        *p++ = (uint8_t)rnd();
}

static uint32_t make_tag(void)
{
    switch (rnd_n(6)) {
    case 0: return rnd_n(1000);
    case 1: return 1000 + rnd_n(0xfc00);
    case 2: return 999 + rnd_n(2);
    case 3: return rnd_n(2) ? rnd() : 0x80000000u | rnd();
    default: return 1 + rnd_n(40) + (rnd_n(2) ? 0x3df : 0);
    }
}

static uint32_t make_size(void)
{
    switch (rnd_n(8)) {
    case 0: return 0;
    case 1: return 1 + rnd_n(4);
    case 2: return 0xfffc + rnd_n(8);              /* where tag < 1000's 16 bits wrap */
    default: return 1 + rnd_n(0x200);
    }
}

/* A module writing its records, as WINMSG, WINDOW and PANELS do. */
static void listen_save(void)
{
    uint32_t n = rnd_n(5), i, r, wrote;

    for (i = 0; i < n && !faulted; i++) {
        uint32_t tag = make_tag(), size = make_size(), k = rnd_n(8), left, part;

        if (counting) cover_records_written++;
        put("\n   {begin(%#x,%#x)", tag, size);
        r = ((s2)SVC(0x5c))(tag, size);
        put(" =%#x", r);
        left = size > sizeof body_buf ? sizeof body_buf : size;
        if (k == 0) {                              /* too much */
            left = left + 1 + rnd_n(0x40);
            if (counting) cover_overlong++;
        } else if (k == 1) {
            left = rnd_n(left + 1);                /* too little: the rest is padded */
        }
        if (left > sizeof body_buf)
            left = sizeof body_buf;
        fill_random(body_buf, left);
        while (left > 0 && !faulted) {
            part = rnd_n(3) == 0 ? 1 + rnd_n(left) : left;
            wrote = 0xcccccccc;
            r = ((s3)SVC(0x60))((uint32_t)(uintptr_t)body_buf, part,
                                rnd_n(6) ? (uint32_t)(uintptr_t)&wrote : 0);
            put(" write(%#x)=%#x/%#x", part, r, wrote);
            left -= part;
        }
        if (rnd_n(4)) {
            r = ((s0)SVC(0x64))();
            put(" end=%#x", r);
        } else if (counting) {
            cover_unended++;
        }
        put("}");
    }
}

/* A module reading its record, as WINDOW and PANELS do. */
static void listen_record(uint32_t rec)
{
    uint32_t r, out = 0xcccccccc, got, n, guard = 0;

    r = ((s1)SVC(0x54))((uint32_t)(uintptr_t)&out);
    put("\n   {current=%#x:", r);
    put_ptr(out);
    if (counting) cover_reads++;
    (void)rec;
    while (rnd_n(5) && guard++ < 40 && !faulted) {
        n = rnd_n(4) == 0 ? 0 : rnd_n(3) == 0 ? 1 + rnd_n(4) : 1 + rnd_n(0x300);
        got = 0xcccccccc;
        memset(body_buf, 0x5c, n > sizeof body_buf ? sizeof body_buf : n);
        r = ((s3)SVC(0x58))((uint32_t)(uintptr_t)body_buf, n,
                            rnd_n(6) ? (uint32_t)(uintptr_t)&got : 0);
        put(" read(%#x)=%#x/%#x", n, r, got);
        if (r == 0 && got <= sizeof body_buf)
            put_bytes(body_buf, got);
        if (counting) cover_read_chunks++;
    }
    put("}");
}

static struct vfile *make_file(const char *name)
{
    struct vfile *f = NULL;
    int i;

    for (i = 0; i < MAX_FILES; i++)
        if (!files[i].used || !_stricmp(files[i].name, name)) {
            f = &files[i];
            break;
        }
    if (f == NULL)
        f = &files[rnd_n(MAX_FILES)];
    memset(f, 0, sizeof *f);
    f->used = 1;
    snprintf(f->name, sizeof f->name, "%s", name);
    return f;
}


/* A situation file: records of both header sizes; now and then the
 * terminator, a length below the header's, one past the end, or a huge
 * one (never 0: retail spins on it, and so does ours). */
static void fill_situation(struct vfile *f)
{
    uint32_t n = rnd_n(10), i, at = 0;

    nwanted = 0;
    for (i = 0; i < n; i++) {
        uint32_t tag = rnd_n(3) ? rnd_n(1000) : 1000 + rnd_n(0xfeff), len, body, k = rnd_n(20);

        if (rnd_n(20) == 0) tag = 0xffff;
        body = rnd_n(4) ? rnd_n(0x80) : rnd_n(0x400);
        len = body + (tag < 1000 ? 4 : 6);
        if (k == 0) {
            len = 1 + rnd_n(tag < 1000 ? 3 : 5);
            if (counting) cover_short_length++;
        } else if (k == 1) {
            len += 1 + rnd_n(0x100);               /* past the end */
            if (counting) cover_past_end++;
        } else if (k == 2 && tag >= 1000) {
            len = 0x80000000u + rnd_n(0x100);
            if (counting) cover_big_tags++;
        }
        if (at + 6 + body > FILE_MAX)
            break;
        f->bytes[at++] = (uint8_t)tag;
        f->bytes[at++] = (uint8_t)(tag >> 8);
        if (tag == 0xffff)
            break;
        f->bytes[at++] = (uint8_t)len;
        f->bytes[at++] = (uint8_t)(len >> 8);
        if (tag >= 1000) {
            f->bytes[at++] = (uint8_t)(len >> 16);
            f->bytes[at++] = (uint8_t)(len >> 24);
        }
        fill_random(f->bytes + at, body);
        at += body;
        if (nwanted < 8) wanted_tags[nwanted++] = tag;
    }
    if (rnd_n(3) == 0 && at + 2 <= FILE_MAX) {
        f->bytes[at++] = 0xff;
        f->bytes[at++] = 0xff;
        if (counting) cover_terminated++;
    }
    if (rnd_n(8) == 0 && at > 0)
        at = rnd_n(at);                            /* cut short */
    f->size = at;
}

static char name_buf[4][32];

static void op_records(void)
{
    uint32_t k = rnd_n(100), r;

    if (k < 25) {                                  /* a situation saved */
        struct vfile *f = make_file("save.stn");
        struct hblock *b = (struct hblock *)arena_get(sizeof *b);
        uint32_t file = (uint32_t)(uintptr_t)b;

        b->handle = (uint32_t)(f - files);
        b->pos = 0;
        if (rnd_n(20) == 0) file = 0;              /* nothing to write to */
        if (counting) cover_saves++;
        put("\n#save(");
        put_ptr(file);
        put(")");
        r = ((s1)internal(0x207f6d50, "main_save_records@4"))(file);
        put(" =%#x file %#x", r, f->size);
        put_bytes(f->bytes, f->size);
    } else if (k < 45) {                           /* a record read back out */
        static uint32_t rec[4];
        uint32_t n = make_size();

        rec[0] = make_tag();
        rec[1] = rnd();
        rec[2] = n > 0x1000 ? 0x1000 : n;
        rec[3] = (uint32_t)(uintptr_t)fixtures;
        fill_random(fixtures, rec[2] + 0x40);
        put("\n#record(%#x,%#x)", rec[0], rec[2]);
        r = ((s1)internal(0x207f6c80, "main_dispatch_record@4"))((uint32_t)(uintptr_t)rec);
        put(" =%#x", r);
    } else if (k < 55) {                           /* the reader with no record going */
        uint32_t out = 0xcccccccc, got = 0xcccccccc, n = rnd_n(3) ? 1 + rnd_n(0x40) : 0;
        if (counting && IMG32(S_READ_LEFT) != 0) cover_stale_reads++;
        r = ((s1)SVC(0x54))((uint32_t)(uintptr_t)&out);
        put("\n#current=%#x:", r);
        put_ptr(out);
        r = ((s3)SVC(0x58))((uint32_t)(uintptr_t)body_buf, n, (uint32_t)(uintptr_t)&got);
        put(" read(%#x)=%#x/%#x", n, r, got);
        if (r == 0) put_bytes(body_buf, got);
    } else if (k < 62) {                           /* the writer with no file */
        uint32_t wrote = 0xcccccccc;
        if (counting) cover_no_file++;
        r = ((s2)SVC(0x5c))(make_tag(), make_size());
        put("\n#begin=%#x", r);
        r = ((s3)SVC(0x60))((uint32_t)(uintptr_t)body_buf, 1 + rnd_n(9), (uint32_t)(uintptr_t)&wrote);
        put(" write=%#x/%#x", r, wrote);
        r = ((s0)SVC(0x64))();
        put(" end=%#x", r);
    } else {                                       /* a record looked for */
        uint32_t which = rnd_n(4), out = 0xcccccccc, size = 0xcccccccc, tag, flight = rnd_n(4) == 0;
        char *name = name_buf[which];
        int use_size = rnd_n(6) != 0;

        snprintf(name, sizeof name_buf[0], "sit%u.stn", which);
        if (rnd_n(8) != 0)
            fill_situation(make_file(name));
        else if (counting)
            cover_missing++;
        tag = nwanted && rnd_n(4) ? wanted_tags[rnd_n(nwanted)] : rnd_n(2) ? rnd_n(1000) : rnd();
        if (counting) {
            cover_finds++;
            if (flight) cover_flight++;
        }
        if (flight) {
            strcpy((char *)cur->image + S_FIND_PATH, name);
            put("\n#findflight(%#x)", tag);
            r = ((s3)SVC(0x6c))(tag, (uint32_t)(uintptr_t)&out,
                                use_size ? (uint32_t)(uintptr_t)&size : 0);
        } else {
            put("\n#find(%s,%#x)", name, tag);
            r = ((s4)SVC(0x68))((uint32_t)(uintptr_t)name, tag, (uint32_t)(uintptr_t)&out,
                                use_size ? (uint32_t)(uintptr_t)&size : 0);
        }
        put(" =%#x ", r);
        put_ptr(out);
        put(" %#x", size);
        if (r == 0 && counting) cover_found++;
        if (r == 0 && in_arena(out) && size <= 0x100000)
            put_bytes((void *)(uintptr_t)out, size);
    }
}

static void report_records(void)
{
    printf("  saves %lu (records %lu, never ended %lu, written past their size %lu, short "
           "writes %lu), records read %lu in %lu chunks, read with none going %lu\n",
           cover_saves, cover_records_written, cover_unended, cover_overlong, cover_short_writes,
           cover_reads, cover_read_chunks, cover_stale_reads);
    printf("  finds %lu (flight file %lu, found %lu, file missing %lu; files with the terminator "
           "%lu, short lengths %lu, past the end %lu, huge %lu); writes with no file %lu\n",
           cover_finds, cover_flight, cover_found, cover_missing, cover_terminated,
           cover_short_length, cover_past_end, cover_big_tags, cover_no_file);
}

/* ---- running ---------------------------------------------------------------------- */

struct suite {
    const char *name;
    void (*op)(void);
    void (*report)(void);
    uint32_t max_ops;
};

static const struct suite suites[] = {
    { "records", op_records, report_records, 40 },
};

static const struct suite *suite;

static void run_side(struct side *s)
{
    uint32_t i, nops;

    cur = s;
    s->length = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    memset(arena, 0, 0x40000);
    memset(files, 0, sizeof files);
    memset(global, 0, sizeof global);
    fix_used = 0;
    nwanted = 0;
    rng = scenario_seed;
    pad_byte = (uint8_t)(rnd_n(12) == 0 ? 0 : rnd());
    put("scenario %u %s pad %#x\n", scenario, suite->name, pad_byte);
    faulted = 0;
    nops = 2 + rnd_n(suite->max_ops);
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        hung = 0;
        call_start = GetTickCount();
        in_main = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            suite->op();
        in_main = 0;
        if (hung) {
            put(" HANG");
            if (counting) cover_hangs++;
            break;
        }
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

    printf("FAULT %#lx at %p, scenario %u op %u, running %s\n", e->ExceptionRecord->ExceptionCode,
           (void *)at, scenario, op_number, cur ? cur->name : "none");
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
    const char *which = argc > 2 ? argv[2] : "records";
    uint32_t first = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 0) : 0;
    uint32_t i, differ = 0;

    for (i = 0; i < sizeof suites / sizeof suites[0]; i++)
        if (!strcmp(suites[i].name, which))
            suite = &suites[i];
    if (suite == NULL) { printf("no suite %s\n", which); return 2; }
    AddVectoredExceptionHandler(1, on_vectored);
    if (getenv("MAIN_NOFILTER") == NULL)
        SetUnhandledExceptionFilter(on_fault);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    build_stubs();
    read_relocs("MAIN_retail.dll");
    load_side(&retail, "MAIN_retail.dll", "retail");
    load_side(&ours, "MAIN_ours.dll", "ours");
    for (i = first; i < first + n; i++) {
        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        scenario_seed = rnd() | 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("MAIN_DUMP") != NULL) {
            FILE *f = fopen("main_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("main_ours.txt", "wb");
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
    printf("maindiff %s: %u scenarios, %u differ\n", suite->name, n, differ);
    suite->report();
    printf("  faults (alike, the scenario ends) %lu: access violations %lu, allocations too big "
           "%lu; hangs %lu\n", cover_faults, cover_av, cover_huge, cover_hangs);
    return differ ? 1 : 0;
}
