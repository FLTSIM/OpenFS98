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
 *   situations  the situation reader and writer, .text+0x11cd2..+0x13104
 *             (assembly): +0x38 (the default flight), +0x3c (a flight),
 *             +0x40 (save), +0x44 (reload), +0x48 (from memory), +0x4c (from
 *             an open file), over slice 1 and GLOBAL.  Situations are saved
 *             from a random GLOBAL and loaded back, as written or mutated
 *             (bytes flipped, lengths changed, records cut, repeated,
 *             unknown ones put in, the end tag lost), with the mode flags
 *             +0x1248/+0x124c/+0x1250 either way.  The default flight is
 *             there, or only the strings' one, or neither (retail alerts
 *             and spins: a hang).  MAIN +0x74 and +0xa4, not done yet,
 *             are seams: retail's two call sites are patched to harness
 *             stubs, and ours' Linkage slots point at the same stubs.  The
 *             heap past what is allocated reads 0xff, so a walk off the end
 *             of a situation meets an end tag; what the description buffer
 *             at +0x4480 points to after a save is a stack address, logged
 *             as such.
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
static uint8_t acontain[0x40], controls[0x60], sound[0x40], util[0xc0], weatherm[0x40];
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
static void cap_lengths(uint8_t *b, uint32_t n);
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
    else if (b >= global && b < global + sizeof global)
        put("G+%#x", v - (uint32_t)(uintptr_t)global);
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
static uint8_t *exe_lo, *exe_hi;

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
    /* a walk into the stack's guard page, or far enough to exhaust it, is
     * the same fault as any other read off the end of a situation */
    if (!in_main || cur == NULL ||
        (code != EXCEPTION_ACCESS_VIOLATION && code != STATUS_GUARD_PAGE_VIOLATION &&
         code != STATUS_STACK_OVERFLOW))
        return EXCEPTION_CONTINUE_SEARCH;
    put(" FAULT(av)");
    if (getenv("MAIN_EXC")) {
        uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
        if (ip >= cur->lo && ip < cur->hi)
            put("@%s.text+%#x", cur->name, (uint32_t)(ip - cur->lo - 0x1000));
        else
            put("@%p", (void *)ip);
        put(" addr %#lx", (unsigned long)e->ExceptionRecord->ExceptionInformation[1]);
    }
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

static unsigned long cover_wild;
static uint8_t body_buf[0x2000];

/* Memory both sides see alike: the heap, the fixtures, the image, GLOBAL
 * and the stack. */
static int known_memory(uint32_t v, uint32_t n, int stack_too)
{
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    const uint8_t *b = (const uint8_t *)(uintptr_t)v, *e = b + n;

    return (b >= arena && e <= arena + ARENA_SIZE) ||
           (b >= fixtures && e <= fixtures + sizeof fixtures) ||
           (cur != NULL && b >= cur->image && e <= cur->image + IMAGE_SIZE) ||
           (b >= global && e <= global + sizeof global) ||
           (b >= body_buf && e <= body_buf + sizeof body_buf) ||
           (stack_too && (void *)b >= tib->StackLimit && (void *)e <= tib->StackBase);
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
    /* no copy MAIN makes through here has its source on the stack: one
     * there is a walk that has wrapped round to it */
    if (!known_memory((uint32_t)(uintptr_t)s, n, 0) || !known_memory((uint32_t)(uintptr_t)d, n, 1)) {
        if (counting) cover_wild++;
        stub_fault("wild");            /* a walk gone off into the harness's own memory */
    }
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
static uint32_t short_write_odds = 60;

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
    if (b->pos + k > FILE_MAX || rnd_n(short_write_odds) == 0) {
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

static int32_t STDCALL h_create(struct hblock **out, const char *path, int32_t ext)
{
    struct vfile *f = vfile_find(path);
    int i;

    put(" create(");
    put_ptr((uint32_t)(uintptr_t)out);
    put(",");
    put_name(path);
    put(",%d)", ext);
    if (f == NULL)
        for (i = 0; i < MAX_FILES; i++)
            if (!files[i].used) {
                f = &files[i];
                f->used = 1;
                snprintf(f->name, sizeof f->name, "%s", path);
                break;
            }
    if (f == NULL || rnd_n(50) == 0) {
        *out = NULL;
        put("=-1");
        return -1;
    }
    f->size = 0;
    *out = (struct hblock *)arena_get(sizeof **out);
    (*out)->handle = (uint32_t)(f - files);
    (*out)->path = 0;
    (*out)->mode = 1;
    (*out)->ext = (uint32_t)ext;
    (*out)->pos = 0;
    put("=");
    put_ptr((uint32_t)(uintptr_t)*out);
    return 0;
}

/* +0x184, as controlsdiff.c models it (host 0x40a510) */
static int32_t STDCALL h_join(char *out, const char *drive, const char *dir, const char *name)
{
    char b[0x208];
    size_t n;

    put(" join(");
    put_name(drive);
    put(",");
    put_name(dir);
    put(",");
    put_name(name);
    put(")");
    memset(b, 0, sizeof b);
    if (!out) return -1;
    if (drive && strlen(drive)) {
        strcat(b, drive);
        if (strlen(b) == 1) strcat(b, ":");
        n = strlen(b);
        if (b[n - 1] == '\\') b[n - 1] = 0;
    }
    if (dir && strlen(dir)) {
        if (dir[0] != '\\' && strlen(b)) strcat(b, "\\");
        strncat(b, dir, 0x104);
        n = strlen(b);
        if (b[n - 1] == '\\') b[n - 1] = 0;
    }
    if (name && strlen(name)) {
        if (name[0] != '\\' && strlen(b)) strcat(b, "\\");
        strncat(b, name, 0x104);
    }
    if (strlen(b) >= 0x104) { out[0] = 0; put("=-1"); return -1; }
    strcpy(out, b);
    put("='%s'", out);
    return 0;
}

static int32_t STDCALL h_post(uint32_t a, uint32_t b)
{
    put(" post(%#x,%#x)", a, b);
    return 0;
}

static uint16_t clock_words[6];

static int32_t STDCALL h_local_time(uint16_t *out)
{
    put(" localtime");
    memcpy(out, clock_words, sizeof clock_words);
    return 0;
}

static int32_t STDCALL h_memset(void *d, int32_t c, uint32_t n)
{
    put(" memset(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",%#x,%#x)", (uint32_t)c & 0xff, n);
    memset(d, c & 0xff, n);
    return 0;
}

static const char *string_of(uint32_t id);

static int32_t STDCALL h_loadstring(uint32_t id, char *buf, int32_t len)
{
    const char *t = string_of(id);
    int32_t n = 0;

    put(" loadstring(%#x,%d)", id, len);
    if (t != NULL && len > 0) {
        n = (int32_t)strlen(t);
        if (n > len - 1) n = len - 1;
        memcpy(buf, t, (size_t)n);
        buf[n] = 0;
    }
    put("=%d", n);
    return n;
}

static char *STDCALL h_strcpy(char *d, const char *src)
{
    put(" strcpy(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",'%.80s')", src);
    return strcpy(d, src);
}

static char *STDCALL h_strncpy(char *d, const char *src, uint32_t n)
{
    put(" strncpy(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",'%.80s',%#x)", src, n);
    return strncpy(d, src, n);
}

static uint32_t STDCALL h_strlen(const char *src)
{
    uint32_t n = src ? (uint32_t)strlen(src) : 0;
    put(" strlen=%#x", n);
    return n;
}

/* ---- CHAIN ------------------------------------------------------------------------ */

typedef uint32_t (STDCALL *s0)(void);
typedef uint32_t (STDCALL *s1)(uint32_t);
typedef uint32_t (STDCALL *s2)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *s3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (STDCALL *s4)(uint32_t, uint32_t, uint32_t, uint32_t);
#define SVC(off) (*(void **)(cur->link + (off)))

static int listening = 1;
static uint32_t dispatches_this_op;
static unsigned long cover_runaway;
static void listen_save(void);
static void listen_record(uint32_t rec);

static uint32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    put(" chainsend(%#x,%#x,", list, a);
    put_ptr(b);
    put(")");
    if (list == 0x14 && a == 0x23 && listening)
        listen_save();
    else if (list == 0x14 && a == 0x22) {
        /* A record of length 0 never moves the walk on (retail spins on
         * it, sending it again and again): cut the op after a fixed number
         * of dispatches, the same count on both sides, rather than by the
         * watchdog's clock. */
        if (++dispatches_this_op > 4096) {
            if (counting) cover_runaway++;
            stub_fault("runaway");
        }
        if (getenv("MAIN_CUR"))
            put(" [cur=%#x len=%#x body=%#x]", IMG32(0x41e5), IMG32(0x41ed), IMG32(0x41f1));
        if (listening)
            listen_record(b);
    }
    put(" <-");
    return 0x5e17;
}

/* ---- the other modules, and MAIN's own seams --------------------------------------- */

#define LOG_SLOT(table, name, off)                                                  \
    static uint32_t STDCALL table##_##off##_1(uint32_t a)                             \
    { put(" " name "+" #off "(%#x)", a); return 0; }
LOG_SLOT(controls, "controls", 0x28)
LOG_SLOT(controls, "controls", 0x2c)
LOG_SLOT(controls, "controls", 0x30)
LOG_SLOT(controls, "controls", 0x34)
#undef LOG_SLOT

static uint32_t STDCALL controls_24(void) { put(" controls+0x24()"); return 0; }
static uint32_t STDCALL controls_38(uint32_t a, uint32_t b) { put(" controls+0x38(%#x,%#x)", a, b); return 0; }
static uint32_t STDCALL controls_48(void) { put(" controls+0x48()"); return 0; }
static uint32_t STDCALL sound_24(uint32_t a, uint32_t b) { put(" sound+0x24(%#x,%#x)", a, b); return 0; }
static uint32_t STDCALL weatherm_24(void) { put(" weatherm+0x24()"); return 0x3e47; }

static char aircraft_answer[0x110];

/* ACONTAIN +0x30(key, 1, buf, len): the aircraft's path */
static uint32_t STDCALL acontain_30(uint32_t key, uint32_t a, char *buf, uint32_t len)
{
    put(" acontain+0x30(");
    put_ptr(key);
    put(",%#x,", a);
    put_ptr((uint32_t)(uintptr_t)buf);
    put(",%#x)", len);
    if (len) {
        strncpy(buf, aircraft_answer, len - 1);
        buf[len - 1] = 0;
    }
    return 0;
}

static uint32_t STDCALL acontain_28(const char *title)
{
    put(" acontain+0x28('%.80s')", title);
    return 0;
}

static unsigned long cover_alerts;

static uint32_t STDCALL util_alert(uint32_t flags, uint32_t tag, const char *text, uint32_t arg,
                                   uint32_t p3, uint32_t p4, uint32_t p5, uint32_t lines)
{
    put(" alert(%#x,%#x,'%.60s',%#x,%#x,%#x,%#x,%#x)", flags, tag, text, arg, p3, p4, p5, lines);
    if (counting) cover_alerts++;
    return 1;
}

/* MAIN +0x74 and +0xa4 until they are done: +0x74 reads its second
 * argument as a word, pushed over two stale bytes. */
static uint32_t STDCALL seam_74(uint32_t a, uint32_t b)
{
    put(" MAIN+0x74(%#x,%#x)", a, b & 0xffff);
    if (getenv("MAIN_SEASON")) put(" season %#x", *(uint16_t *)(global + 0x248));
    return 0;
}

static uint32_t STDCALL seam_a4(void)
{
    put(" MAIN+0xa4()");
    return 0;
}

static void patch_call(struct side *s, uint32_t retail_va, void *target)
{
    uint8_t *at = (uint8_t *)s->module + (retail_va - RETAIL_BASE);
    DWORD old;

    uint8_t *tail = (uint8_t *)GetProcAddress(ours.module, "main_text_tail");

    if (at[0] != 0xe8) { printf("retail: no call at %#x\n", retail_va); exit(2); }
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int32_t *)(at + 1) = (int32_t)((uint8_t *)target - (at + 5));
    VirtualProtect(at, 5, old, &old);
    /* ours reads retail's code bytes from its own copy (the season table
     * read unbounded): it must see the patch too */
    if (tail == NULL) { printf("ours has no main_text_tail\n"); exit(2); }
    if (retail_va >= 0x207f4093u)
        memcpy(tail + (retail_va - 0x207f4093u), at, 5);
}

static void seams(struct side *s)
{
    if (s == &retail) {
        patch_call(s, 0x207f3b5f, (void *)seam_a4);
        patch_call(s, 0x207f40fe, (void *)seam_74);
    } else {
        *(void **)(s->link + 0x74) = (void *)seam_74;
        *(void **)(s->link + 0xa4) = (void *)seam_a4;
    }
}

static void build_stubs(void)
{
    uint32_t i;

#define TRAPS(table, id) \
    for (i = 4; i < sizeof table; i += 4) *(uint32_t *)(table + i) = 0xde000000u | ((id) << 16) | i
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(stub_module, 0xff);
    TRAPS(acontain, 23); TRAPS(controls, 16); TRAPS(sound, 39); TRAPS(util, 20); TRAPS(weatherm, 27);
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
    SET_SLOT(host, 0x03c, h_post);
    SET_SLOT(host, 0x044, h_local_time);
    SET_SLOT(host, 0x148, h_create);
    SET_SLOT(host, 0x184, h_join);
    SET_SLOT(host, 0x218, h_memset);
    SET_SLOT(host, 0x2a0, h_loadstring);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2ac, h_strncpy);
    SET_SLOT(host, 0x2b8, h_strlen);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x20, chain_send);
    *(uint32_t *)acontain = 23;
    SET_SLOT(acontain, 0x28, acontain_28);
    SET_SLOT(acontain, 0x30, acontain_30);
    *(uint32_t *)controls = 16;
    SET_SLOT(controls, 0x24, controls_24);
    SET_SLOT(controls, 0x28, controls_0x28_1);
    SET_SLOT(controls, 0x2c, controls_0x2c_1);
    SET_SLOT(controls, 0x30, controls_0x30_1);
    SET_SLOT(controls, 0x34, controls_0x34_1);
    SET_SLOT(controls, 0x38, controls_38);
    SET_SLOT(controls, 0x48, controls_48);
    *(uint32_t *)sound = 39;
    SET_SLOT(sound, 0x24, sound_24);
    *(uint32_t *)util = 20;
    SET_SLOT(util, 0xa4, util_alert);
    *(uint32_t *)weatherm = 27;
    SET_SLOT(weatherm, 0x24, weatherm_24);
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
                   im->id == 4 ? (void *)chain : im->id == 23 ? (void *)acontain :
                   im->id == 16 ? (void *)controls : im->id == 39 ? (void *)sound :
                   im->id == 20 ? (void *)util : im->id == 27 ? (void *)weatherm :
                   (void *)stub_module;
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
    {
        /* the words that hold pointers into the image at run time, not
         * being relocations: the situation reader's record and cursor (some
         * unaligned), the record being read and where its body is read
         * from.  Only these: any other word that looks like an image address
         * is a value that happens to (a length, say). */
        static const uint32_t odd[] = { 0x41e1, 0x41e5, 0x41f5, 0x41f9, 0x9754, 0x9758 };
        uint32_t j, v;
        for (j = 0; j < sizeof odd / sizeof odd[0]; j++) {
            memcpy(&v, img + odd[j], 4);
            if (in_image(v)) {
                v = 0xda000000u | (v - (uint32_t)(uintptr_t)cur->image);
                memcpy(img + odd[j], &v, 4);
            }
        }

    }
    memset(img + LINKAGE, 0, LINKAGE_END - LINKAGE);
    {                                              /* a save's description buffer */
        NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
        uint8_t *v = (uint8_t *)(uintptr_t)*(uint32_t *)(img + 0x4480);
        if ((void *)v >= tib->StackLimit && (void *)v < tib->StackBase)
            *(uint32_t *)(img + 0x4480) = 0x57ac0000u;
    }
    for (i = 4; i < 0xb8; i += 8)                  /* the import links */
        *(uint32_t *)(img + i) = 0;
    put("\n  state img=%08x heap=%#x/%08x G=%08x", digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used,
        digest(arena, arena_used, 0x811c9dc5u), digest(global, sizeof global, 0x811c9dc5u));
    if (getenv("MAIN_IMAGE")) {
        put("\n  words");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i) != 0)
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

static void fill_random(uint8_t *p, uint32_t n)
{
    while (n--)
        *p++ = (uint8_t)rnd();
}

/* In the situations suite the modules mostly behave: tags of their own
 * (not MAIN's table's) and lengths that fit the header. */
static int gentle;

static uint32_t make_tag(void)
{
    if (gentle && rnd_n(20))
        return rnd_n(2) ? 0x3df + rnd_n(9) : 0x1000 + rnd_n(0x100);
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
    if (gentle && rnd_n(40))
        return 1 + rnd_n(0x100);
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
    cap_lengths(f->bytes, f->size);
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

/* ---- suite: situations ------------------------------------------------------------- */

static unsigned long cover_saves2, cover_save_fail, cover_loads[6], cover_mutated, cover_unknown,
    cover_no_end, cover_mode_flags[3], cover_long_title, cover_seasons_wild, cover_default_gone,
    cover_strings_default;
static char saved[8][64];
static uint32_t nsaved;

static const char *string_of(uint32_t id)
{
    switch (id) {
    case 0x9002: return "FLIGHTS";
    case 0x9004: return "FLIGHTS\\OTHER";
    case 0x9016: return "start.stn";
    case 0xf461: return "No default flight";
    case 0xf462: return "Flight will not load";
    }
    return NULL;
}

static void random_string(char *d, uint32_t max)
{
    uint32_t n = rnd_n(max), i;

    for (i = 0; i < n; i++)
        d[i] = rnd_n(20) == 0 ? (char)(0x80 + rnd_n(0x7f)) : (char)('a' + rnd_n(26));
    d[n] = 0;
}

static void global_setup(void)
{
    uint32_t i;

    for (i = 0; i < sizeof global; i++)
        global[i] = (uint8_t)(rnd_n(3) ? 0 : rnd());
    strcpy((char *)global + 0x24, rnd_n(4) ? "FLIGHTS\\def.stn" : "FLIGHTS\\none.stn");
    random_string((char *)global + 0x446, rnd_n(16) ? 0x60 : 0x110);
    random_string((char *)global + 0x11e0, 0x20);
    *(uint32_t *)(global + 0x1248) = rnd_n(2);
    *(uint32_t *)(global + 0x124c) = rnd_n(2);
    *(uint32_t *)(global + 0x1250) = rnd_n(2);
    *(uint32_t *)(global + 0x1244) = rnd_n(2);
    *(uint32_t *)(global + 0x1214) = rnd_n(3) ? 2 : rnd_n(4);
    *(uint32_t *)(global + 0x73c) = rnd_n(3) ? 2 : rnd_n(4);
    *(uint16_t *)(global + 0x248) = (uint16_t)(rnd_n(30) ? rnd_n(4) : rnd());
    if (counting && *(uint16_t *)(global + 0x248) > 3) cover_seasons_wild++;
    random_string(aircraft_answer, rnd_n(8) ? 0x40 : 0x108);
    /* the words the load's checks turn on, often at their edges, so that a
     * save takes them out and a load brings them back */
    {
        static const uint16_t words[] = { 0x372, 0xc44, 0xf8c, 0x1074, 0x115c, 0xbc8, 0xe64, 0x354,
                                          0xbd0, 0xbdc, 0xbe8, 0xbe9 };
        static const uint16_t edge[] = { 0, 1, 2, 4, 8, 0x80, 0x100, 0x800, 0x888, 0xff00, 99, 100,
                                         101, 0x7ffe, 0x7fff, 0x8000, 0xffff };
        for (i = 0; i < sizeof words / sizeof words[0]; i++)
            if (rnd_n(2))
                *(uint16_t *)(global + words[i]) = edge[rnd_n(sizeof edge / sizeof edge[0])];
    }
    for (i = 0; i < 6; i++)
        clock_words[i] = (uint16_t)rnd();
}

static unsigned long cover_own_records, cover_own_modes[16];

/* One of MAIN's own records, as files MAIN did not write have them (the
 * CD's flights carry the mode 0xa ones, which a save leaves out): an entry
 * of the table, mostly one with a routine or of mode 0xa, at its own
 * length -- now and then one byte off -- with a random body. */
static uint32_t own_record(uint8_t *b, uint32_t max)
{
    uint32_t e = 0x4456, picks[80], n = 0, size, hdr, i, tag, f;
    const uint8_t *img = cur->image;

    while (*(uint16_t *)(img + e) != 0xffff && n < 80) {
        picks[n++] = e;
        e = *(uint16_t *)(img + e + 4) == 0xffff
            ? *(uint32_t *)(img + e + 0xa) - (uint32_t)(uintptr_t)img : e + 0x12;
    }
    for (i = 0; i < 6; i++) {
        e = picks[rnd_n(n)];
        if (img[e + 2] == 0xa || *(uint32_t *)(img + e + 0xe) != 0)
            break;
    }
    tag = *(uint16_t *)(img + e);
    size = *(uint16_t *)(img + e + 4);
    if (size == 0xffff) {
        size = 0;
        for (f = e + 0x12; f < *(uint32_t *)(img + e + 0xa) - (uint32_t)(uintptr_t)img; f += 5) {
            uint8_t c = img[f + 4];
            size += c == 0x80 ? 1 : c == 0x81 ? 2 : c == 0x83 ? 3 : c == 0x82 ? 4 : c & 0x7f;
        }
    } else if (img[e + 3] >= 2 && img[e + 3] <= 4) {
        size = 1 + rnd_n(0x40);
    }
    if (rnd_n(10) == 0)
        size += rnd_n(3) - 1;
    hdr = tag < 1000 ? 4 : 6;
    if (hdr + size > max)
        return 0;
    b[0] = (uint8_t)tag;
    b[1] = (uint8_t)(tag >> 8);
    b[2] = (uint8_t)(hdr + size);
    b[3] = (uint8_t)((hdr + size) >> 8);
    if (hdr == 6) { b[4] = 0; b[5] = 0; }
    /* the body: random, or the values the load's checks turn on */
    for (i = 0; i < size; i += 2) {
        static const uint16_t edge[] = { 0, 1, 2, 3, 4, 5, 8, 0x7f, 0x80, 0xff, 0x100, 0x800, 0x888,
                                         0xff00, 99, 100, 101, 0x7ffe, 0x7fff, 0x8000, 0xffff };
        uint16_t w = rnd_n(3) == 0 ? edge[rnd_n(sizeof edge / sizeof edge[0])] : (uint16_t)rnd();
        b[hdr + i] = (uint8_t)w;
        if (i + 1 < size)
            b[hdr + i + 1] = (uint8_t)(w >> 8);
    }
    if (counting) {
        cover_own_records++;
        cover_own_modes[img[e + 2] & 15]++;
    }
    return hdr + size;
}

/* Keep a situation's record lengths small enough that the parser's cursor
 * walks forward byte-by-byte rather than leaping off into arbitrary process
 * memory (where a jump of several MB lands at an absolute address mapped on
 * one side's pass but not the other -- harness nondeterminism, not a module
 * difference).  Every length is capped so the body is at most 0x400; the
 * walk then crawls through the buffer into the 0xff tail and stops at a
 * 0xffff there, the same on both sides.  All the parse logic -- every record
 * type, both header widths, unknown records, the terminator, short lengths,
 * past-the-end -- is still exercised; only the leap into wild memory is not.
 */
static void cap_lengths(uint8_t *b, uint32_t n)
{
    uint32_t at = 0;

    while (at + 4 <= n) {
        uint32_t tag = b[at] | (uint32_t)b[at + 1] << 8, len, hdr, body;
        if (tag == 0xffff)
            break;
        if (tag < 1000) {
            hdr = 4;
            len = b[at + 2] | (uint32_t)b[at + 3] << 8;
        } else {
            hdr = 6;
            if (at + 6 > n)
                break;
            len = b[at + 2] | (uint32_t)b[at + 3] << 8 | (uint32_t)b[at + 4] << 16 |
                  (uint32_t)b[at + 5] << 24;
        }
        body = len > hdr ? len - hdr : 0;
        if (body > 0x400) {
            body = 0x400;
            len = hdr + body;
            b[at + 2] = (uint8_t)len;
            b[at + 3] = (uint8_t)(len >> 8);
            if (hdr == 6) { b[at + 4] = 0; b[at + 5] = 0; }
        }
        at += hdr + body;
    }
}

/* A saved situation's bytes, damaged */
static uint32_t mutate(uint8_t *b, uint32_t n, uint32_t max)
{
    uint32_t k = rnd_n(12), i, at;

    if (counting) cover_mutated++;
    if (k >= 8)
        k = 7;                          /* MAIN's own records, a third of the time */
    if (n == 0)
        return n;
    switch (k) {
    case 0:                                        /* bytes flipped */
        for (i = 1 + rnd_n(6); i > 0; i--)
            b[rnd_n(n)] ^= (uint8_t)(1 + rnd_n(255));
        break;
    case 1:                                        /* cut */
        n = rnd_n(n);
        break;
    case 2:                                        /* the end tag lost */
        if (n >= 2 && b[n - 2] == 0xff && b[n - 1] == 0xff) {
            n -= 2;
            if (counting) cover_no_end++;
        }
        break;
    case 7: {                                      /* MAIN's own records put in at the front,
                                                      or last, where nothing after them
                                                      in the file overwrites what they set */
        uint8_t rec[0x200];
        uint32_t j, m, where = 0;
        if (rnd_n(2) && n >= 2 && b[n - 2] == 0xff && b[n - 1] == 0xff)
            where = n - 2;
        for (j = 1 + rnd_n(6); j > 0; j--) {
            m = own_record(rec, sizeof rec);
            if (m == 0 || n + m > max)
                break;
            memmove(b + where + m, b + where, n - where);
            memcpy(b + where, rec, m);
            n += m;
        }
        break;
    }
    case 3: case 4: {                              /* an unknown record put in at the front */
        uint32_t tag = rnd_n(2) ? 0x200 + rnd_n(0x200) : 0x2000 + rnd_n(0x1000);
        uint32_t body = rnd_n(0x40), hdr = tag < 1000 ? 4 : 6;
        if (n + hdr + body > max)
            break;
        memmove(b + hdr + body, b, n);
        b[0] = (uint8_t)tag;
        b[1] = (uint8_t)(tag >> 8);
        b[2] = (uint8_t)(hdr + body);
        b[3] = (uint8_t)((hdr + body) >> 8);
        if (hdr == 6) { b[4] = 0; b[5] = 0; }
        for (i = 0; i < body; i++)
            b[hdr + i] = (uint8_t)rnd();
        n += hdr + body;
        if (counting) cover_unknown++;
        break;
    }
    case 5:                                        /* a length changed */
        at = rnd_n(n);
        if (at + 4 <= n)
            b[at + 2] = (uint8_t)(b[at + 2] + rnd_n(9) - 4);
        break;
    case 6:                                        /* a run repeated */
        at = rnd_n(n);
        i = rnd_n(n - at + 1);
        if (n + i <= max) {
            memmove(b + at + i, b + at, n - at);
            n += i;
        }
        break;
    }
    cap_lengths(b, n);
    return n;
}

static void op_situations(void)
{
    uint32_t k = rnd_n(100), r;
    static char title[0x40], description[0x200];
    struct vfile *f;

    if (k < 25) {                                  /* saved */
        uint32_t t = rnd_n(10);
        if (t < 7)
            snprintf(title, sizeof title, "s%u.stn", rnd_n(4));
        else
            random_string(title, t == 9 ? 0x30 : 0x18);
        if (counting && strlen(title) >= 0x1e) cover_long_title++;
        random_string(description, rnd_n(4) ? 0x80 : 0x180);
        if (rnd_n(3) == 0)
            global[0x500 + rnd_n(0xd00)] = (uint8_t)rnd();
        if (counting) cover_saves2++;
        put("\n#save('%s','%.40s')", title, description);
        r = ((s2)SVC(0x40))((uint32_t)(uintptr_t)title, (uint32_t)(uintptr_t)description);
        put(" =%#x", r);
        if (r != 0 && counting) cover_save_fail++;
        if (r == 0 && nsaved < 8) {
            snprintf(saved[nsaved++], sizeof saved[0], "FLIGHTS\\%s", title);
        }
        f = vfile_find((char *)cur->image + 0x40dc);
        if (f != NULL) {
            put(" file %#x", f->size);
            put_bytes(f->bytes, f->size);
        }
    } else if (k < 45) {                           /* a flight loaded */
        static struct { uint32_t ext; char path[0x104]; } fl;
        fl.ext = rnd_n(8) ? 2 : rnd_n(4);
        if (nsaved && rnd_n(5))
            strcpy(fl.path, saved[rnd_n(nsaved)]);
        else
            snprintf(fl.path, sizeof fl.path, "FLIGHTS\\x%u.stn", rnd_n(3));
        if (rnd_n(4) == 0 && (f = vfile_find(fl.path)) != NULL)
            f->size = mutate(f->bytes, f->size, FILE_MAX);
        if (counting) cover_loads[0]++;
        put("\n#load(%u,'%s')", fl.ext, fl.path);
        r = ((s1)SVC(0x3c))((uint32_t)(uintptr_t)&fl);
        put(" =%#x", r);
    } else if (k < 52) {
        if (counting) cover_loads[1]++;
        put("\n#reload");
        r = ((s0)SVC(0x44))();
        put(" =%#x", r);
    } else if (k < 58) {                           /* the default */
        uint32_t j = rnd_n(10);
        if (j < 2) {
            strcpy((char *)global + 0x24, "FLIGHTS\\gone.stn");
            if (counting) cover_strings_default++;
            if (j == 0 && (f = vfile_find("FLIGHTS\\OTHER\\start.stn")) != NULL) {
                f->used = 0;
                if (counting) cover_default_gone++;
            }
        }
        if (counting) cover_loads[2]++;
        put("\n#default");
        r = ((s0)SVC(0x38))();
        put(" =%#x", r);
    } else if (k < 75) {                           /* from memory */
        uint8_t *m = fixtures + 0x40000;
        uint32_t n = 0;
        if (nsaved && (f = vfile_find(saved[rnd_n(nsaved)])) != NULL) {
            memcpy(m, f->bytes, f->size);
            n = f->size;
        }
        if (rnd_n(2))
            n = mutate(m, n, 0x30000);
        if (counting) cover_loads[3]++;
        put("\n#memory(%#x)", n);
        put_bytes(m, n);
        r = ((s1)SVC(0x48))((uint32_t)(uintptr_t)m);
        put(" =%#x", r);
    } else if (k < 85) {                           /* from an open file */
        struct hblock *b;
        uint32_t len;
        if (!nsaved || (f = vfile_find(saved[rnd_n(nsaved)])) == NULL)
            return;
        if (rnd_n(3) == 0)
            f->size = mutate(f->bytes, f->size, FILE_MAX);
        b = (struct hblock *)arena_get(sizeof *b);
        b->handle = (uint32_t)(f - files);
        b->pos = rnd_n(4) ? 0 : rnd_n(f->size + 1);
        len = rnd_n(4) ? f->size - b->pos : rnd_n(f->size + 0x20);
        if (counting) cover_loads[4]++;
        put("\n#file(%#x,%#x)", b->pos, len);
        r = ((s2)SVC(0x4c))((uint32_t)(uintptr_t)b, len);
        put(" =%#x", r);
    } else if (k < 93) {                           /* GLOBAL's flags change */
        uint32_t j = rnd_n(3);
        *(uint32_t *)(global + 0x1248 + 4 * j) ^= 1;
        if (counting) cover_mode_flags[j]++;
        put("\n#flag %#x=%u", 0x1248 + 4 * j, *(uint32_t *)(global + 0x1248 + 4 * j));
    } else {                                       /* modules that write nothing */
        listening = !listening;
        put("\n#listening %d", listening);
    }
}

static void setup_situations(void)
{
    struct vfile *f;
    uint32_t i;

    nsaved = 0;
    listening = 1;
    short_write_odds = 3000;
    gentle = 1;
    global_setup();
    /* the default flights, as a save of the starting state would leave them */
    if (rnd_n(8)) {
        f = make_file("FLIGHTS\\def.stn");
        f->size = rnd_n(8) ? 0 : rnd_n(0x40);
        for (i = 0; i < f->size; i++) f->bytes[i] = (uint8_t)rnd();
        f->bytes[f->size++] = 0xff;
        f->bytes[f->size++] = 0xff;
    }
    if (rnd_n(16)) {
        f = make_file("FLIGHTS\\OTHER\\start.stn");
        f->bytes[0] = 0xff;
        f->bytes[1] = 0xff;
        f->size = 2;
    }
}

static void report_situations(void)
{
    printf("  saves %lu (failed %lu, titles past 0x1e %lu); loads: flight %lu, reload %lu, default "
           "%lu, memory %lu, open file %lu\n", cover_saves2, cover_save_fail, cover_long_title,
           cover_loads[0], cover_loads[1], cover_loads[2], cover_loads[3], cover_loads[4]);
    printf("  situations damaged %lu (unknown records put in %lu, end tag lost %lu); mode flags "
           "turned %lu/%lu/%lu; seasons past 3 %lu; default gone %lu (strings' one gone too %lu); "
           "alerts %lu\n  MAIN's own records put in %lu (mode 0xa %lu, 0xc %lu, 0xd %lu, 0xe %lu)\n",
           cover_mutated, cover_unknown, cover_no_end, cover_mode_flags[0],
           cover_mode_flags[1], cover_mode_flags[2], cover_seasons_wild, cover_strings_default,
           cover_default_gone, cover_alerts, cover_own_records, cover_own_modes[0xa],
           cover_own_modes[0xc], cover_own_modes[0xd], cover_own_modes[0xe]);
}

/* ---- running ---------------------------------------------------------------------- */

struct suite {
    const char *name;
    void (*op)(void);
    void (*report)(void);
    uint32_t max_ops;
    void (*setup)(void);
    uint8_t fill;                     /* what the heap and fixtures hold past their use */
};

static void setup_records(void) { short_write_odds = 60; gentle = 0; }

static const struct suite suites[] = {
    { "records", op_records, report_records, 40, setup_records, 0x00 },
    { "situations", op_situations, report_situations, 30, setup_situations, 0xff },
};

static const struct suite *suite;

static unsigned long cover_table_overwritten, cover_wild_cut, cover_wild_after_fault;

/* Whether the situation walk's cursor (+0x41e5, unaligned) is outside the
 * memory both sides share: the heap, the fixtures, the image, GLOBAL. */
static int cursor_wild(void)
{
    uint32_t v;

    memcpy(&v, cur->image + 0x41e5, 4);
    return v != 0 && !known_memory(v, 1, 0) && v != *(uint32_t *)(void *)(cur->pristine + 0x41e5);
}

/* Whether a load has written over the situation table (.data+0x4456 to
 * its end at +0x530e) beyond the four fields a save sets: a string record
 * longer than its 0x104-byte buffer at +0x4217 or +0x431b runs on into it. */
static int table_overwritten(void)
{
    uint32_t i;

    for (i = 0x4456; i < 0x5310; i++) {
        if ((i >= 0x447e && i < 0x4484) || (i >= 0x4834 && i < 0x4836) || (i >= 0x48a6 && i < 0x48a8))
            continue;
        if (cur->image[i] != cur->pristine[i])
            return 1;
    }
    return 0;
}

static void run_side(struct side *s)
{
    uint32_t i, nops;

    cur = s;
    s->length = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    memset(arena, suite->fill, ARENA_SIZE);    /* all of it: a runaway walk reads far */
    memset(fixtures, suite->fill, sizeof fixtures);
    memset(files, 0, sizeof files);
    memset(global, 0, sizeof global);
    fix_used = 0;
    nwanted = 0;
    rng = scenario_seed;
    pad_byte = (uint8_t)(rnd_n(12) == 0 ? 0 : rnd());
    suite->setup();
    put("scenario %u %s pad %#x\n", scenario, suite->name, pad_byte);
    faulted = 0;
    nops = 2 + rnd_n(suite->max_ops);
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        dispatches_this_op = 0;
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
        if (cursor_wild()) {
            /* the walk went off into memory the two sides do not share,
             * and may have copied some of it in: what follows cannot agree */
            put(" WALKED OFF");
            if (counting) {
                if (faulted) cover_wild_after_fault++;
                else cover_wild_cut++;
            }
            break;
        }
        if (table_overwritten()) {
            /* the table's self-pointers, overwritten in part, now hold
             * each side's own image address: what follows cannot agree */
            put(" TABLE OVERWRITTEN");
            if (counting) cover_table_overwritten++;
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
    {
        HMODULE exe = GetModuleHandleA(NULL);
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((uint8_t *)exe +
            ((IMAGE_DOS_HEADER *)exe)->e_lfanew);
        exe_lo = (uint8_t *)exe;
        exe_hi = exe_lo + nt->OptionalHeader.SizeOfImage;
    }
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    build_stubs();
    read_relocs("MAIN_retail.dll");
    load_side(&retail, "MAIN_retail.dll", "retail");
    load_side(&ours, "MAIN_ours.dll", "ours");
    seams(&retail);
    seams(&ours);
    memcpy(ours.pristine, ours.image, IMAGE_SIZE);
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
           "%lu, walks off into the harness's memory %lu; hangs %lu\n  runaway dispatch loops cut %lu; scenarios whose state after the last op is "
           "not compared: the walk off in unshared memory %lu (%lu of them after a fault), a load that wrote over "
           "the situation table %lu\n", cover_faults, cover_av, cover_huge, cover_wild, cover_hangs, cover_runaway,
           cover_wild_cut + cover_wild_after_fault, cover_wild_after_fault, cover_table_overwritten);
    return differ ? 1 : 0;
}
