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
#include <math.h>

#define STDCALL __attribute__((stdcall))

#define RETAIL_BASE  0x207e0000u
#define DATA_RVA     0x20000u
#define IMAGE_SIZE   0x2ee00u
#define LINKAGE      0x0c0u
#define LINKAGE_END  (LINKAGE + 51 * 4)
#define ARENA_SIZE   (16u << 20)   /* large, so a corrupt situation's multi-MB cursor jump lands in the 0xff fill and stops, the same on both passes, instead of walking into memory mapped differently between them */
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
static uint8_t acontain[0x40], controls[0x60], sound[0x40], util[0xc0], weatherm[0x40], multip[0x40], fe[0x40];
static uint8_t hg2d[0x40];
static uint8_t g3d[0x40];
static uint8_t chain[0x24];
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t pad_byte;
static uint8_t free_fill = 0xdd;   /* what a freed block and alloc slack hold; 0xff for
                                   * situations, so a parse that walks off a buffer meets
                                   * 0xffff and stops the same on both sides */
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

/* The host repairs integer divide faults in the modules it calls: its __try
 * filter (retail 0x4119a0, docs/modules/host.md and window.md) decodes the
 * faulting F6/F7 and saturates the result past the instruction.  For div it
 * is all ones (quotient and remainder); for idiv the quotient is the largest
 * value -- or the most negative but one (0x8001, 0x80000001; a byte 0x7f81)
 * when the divisor and the dividend's top half differ in sign -- and the
 * remainder the largest.  maindiff stands in for the host and repairs them
 * the same way: +0x74's minutes-to-hours divide and +0xa0's day and latitude
 * divides run into overflow and rely on it, ours modelling the repaired value
 * in C.  This matches windowdiff.c's div_fixup, which the WINDOW slice pinned
 * down.  Only the register-operand forms MAIN uses are decoded. */
static int repair_divide(CONTEXT *c)
{
    const uint8_t *ip = (const uint8_t *)(uintptr_t)c->Eip;
    DWORD *reg[8];
    uint32_t len = 0, v, neg;
    int opsz = 4;
    uint8_t op, modrm, rm, kind;

    reg[0] = &c->Eax; reg[1] = &c->Ecx; reg[2] = &c->Edx; reg[3] = &c->Ebx;
    reg[4] = &c->Esp; reg[5] = &c->Ebp; reg[6] = &c->Esi; reg[7] = &c->Edi;
    if (ip[0] == 0x66) { opsz = 2; len = 1; }
    op = ip[len];
    if (op != 0xf6 && op != 0xf7)
        return 0;
    modrm = ip[len + 1];
    kind = (modrm >> 3) & 7;
    if ((modrm >> 6) != 3 || (kind != 6 && kind != 7))
        return 0;                                  /* div (6) or idiv (7), register operand */
    rm = modrm & 7;
    len += 2;
    if (kind == 6) {                               /* div: all ones */
        if (op == 0xf6)
            c->Eax = (c->Eax & 0xffff0000u) | 0xffffu;
        else if (opsz == 2) {
            c->Eax = (c->Eax & 0xffff0000u) | 0xffffu;
            c->Edx = (c->Edx & 0xffff0000u) | 0xffffu;
        } else {
            c->Eax = 0xffffffffu;
            c->Edx = 0xffffffffu;
        }
    } else {                                       /* idiv: saturate by sign */
        v = *reg[rm];
        if (op == 0xf6) {                          /* 8-bit: AL quot, AH rem; high is AH */
            neg = ((int8_t)v < 0) != ((int8_t)(c->Eax >> 8) < 0);
            c->Eax = (c->Eax & 0xffff0000u) | (neg ? 0x7f81u : 0x7f7fu);
        } else if (opsz == 2) {
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

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;

    if ((code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW) &&
        in_main && cur != NULL && repair_divide(e->ContextRecord))
        return EXCEPTION_CONTINUE_EXECUTION;

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
    memset(p + 16 + size, free_fill == 0xff ? 0xff : pad_byte, n - 16 - size);
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
        memset(b, free_fill, *(uint32_t *)(b - 16));
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
    /* A source off in memory the two sides do not share -- a walk that ran
     * off a situation -- is logged as "wild" rather than by address: the
     * same wild address lands inside one side's module image but not the
     * other's (they load at different bases), which would differ only in
     * the label.  No copy MAIN makes here has its source on the stack. */
    int wild = n > 0x100000 ||
               !known_memory((uint32_t)(uintptr_t)s, n, 0) ||
               !known_memory((uint32_t)(uintptr_t)d, n, 1);

    put(" memmove(");
    if (wild)
        put("wild");
    else
        put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    if (wild)
        put("wild");
    else
        put_ptr((uint32_t)(uintptr_t)s);
    put(",%#x)", n);
    if (wild) {
        if (counting) cover_wild++;
        stub_fault("wild");
    }
    put_bytes(s, n);
    memmove(d, s, n);
    return 0;
}

/* ---- the host's files, in memory ------------------------------------------------- */

#define MAX_FILES 8
#define FILE_MAX  0x4000u

#define FILE_BYTES 0x10000u   /* room for the cloud texture (+0x94); FILE_MAX stays
                               * the cap the situation suites were verified with */

struct vfile {
    char name[64];
    int used;
    uint8_t bytes[FILE_BYTES];
    uint32_t size;
    uint32_t attr;            /* host +0x134/+0x138's flags: 2 read-only */
};

static uint32_t file_cap = FILE_MAX;   /* where a write runs out of disk; per suite */
static uint32_t open_rw_refused;       /* read/write opens refused, for coverage */

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
    if (f == NULL || mode < 0 || mode > 3 || ((f->attr & 2) && mode != 0) || rnd_n(40) == 0) {
        if (mode == 1)
            open_rw_refused++;
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

/* +0x134(&flags, path, ext) and +0x138(flags, path, ext): a file's
 * attributes as the host's flags (1 normal, 2 read-only, 4 archive, ...);
 * -1 with no such file.  A read-only file will not open for writing. */
static int32_t STDCALL h_attr_get(uint32_t *out, const char *path, int32_t ext)
{
    struct vfile *f = vfile_find(path);

    put(" attrs(");
    put_name(path);
    put(",%d)", ext);
    if (f == NULL) {
        put("=-1");
        return -1;
    }
    *out = f->attr;
    put("=%#x", f->attr);
    return 0;
}

static int32_t STDCALL h_attr_set(uint32_t flags, const char *path, int32_t ext)
{
    struct vfile *f = vfile_find(path);

    put(" setattrs(%#x,", flags);
    put_name(path);
    put(",%d)", ext);
    if (f == NULL) {
        put("=-1");
        return -1;
    }
    f->attr = flags;
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
    if (b->pos + k > file_cap || rnd_n(short_write_odds) == 0) {
        k = b->pos < file_cap ? rnd_n(file_cap - b->pos < k ? file_cap - b->pos + 1 : k + 1) : 0;
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

static int32_t STDCALL h_menu(uint32_t id, uint32_t state)
{
    put(" menu(%#x,%#x)", id, state);
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
static int walked_off;
static int situations_suite;      /* the walk-off guard is for situations only */
static unsigned long cover_runaway;
static void listen_save(void);
static void listen_record(uint32_t rec);

static uint32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    uint32_t len_before = cur ? cur->length : 0;
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
        /* A corrupt situation drives the record walk off the end of its
         * buffer, reading record headers out of the 0xff fill and whatever
         * the walk has clobbered.  From there the two passes cannot agree:
         * the modules load at different bases (retail at 0x20800000, ours
         * relocated elsewhere), so the moment a base-relative pointer -- a
         * table NEXT link, a self-pointer copied in as a field -- steers the
         * cursor, the walks diverge though the code is the same.  The
         * signature of a header read from past the data is a length no real
         * record has: cap_lengths holds every generated record's body to
         * 0x400, so a dispatched record whose length exceeds that is reading
         * garbage.  Both passes read the same garbage length at the same
         * first such record (the buffer being shared up to there), so both
         * catch it together: drop this chainsend's text (the length, hence
         * the divergence, is not yet shown) and end the op.  What follows
         * cannot agree, the same as cursor_wild at op end. */
        if (situations_suite && IMG32(0x41ed) > 0x800) {
            cur->length = len_before;
            walked_off = 1;
            in_main = 0;
            __builtin_longjmp(fault_jb, 1);
        }
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

/* UTIL's geometry, as logging models for the nav slice (+0x70).  Each logs
 * every argument MAIN hands it -- so an arg passed wrongly shows in the
 * transcript whatever the model returns -- and returns a value that varies
 * with its inputs so MAIN's own arithmetic around the call is exercised.
 * UTIL's real geometry is verified in UTIL; here only MAIN's glue is. */

/* +0x48(&latbuf, to, &out): the latitude delta (word at +2, dword at +4)
 * scaled toward a planar x, written into the second dword of `out`.
 *
 * Retail's nav_delta keeps `ecx` (the out pointer) live across this call, so
 * the real UTIL leaves ecx (and edx) as it found them; a C stub does not, so
 * the entry below saves and restores them around the body -- the harness
 * lesson that a stub must leave the registers the real callee leaves. */
uint32_t geo_save_ecx, geo_save_edx;
static uint32_t util_geo48_impl(uint32_t latbuf, uint32_t to, uint32_t out) __attribute__((used));
static uint32_t util_geo48_impl(uint32_t latbuf, uint32_t to, uint32_t out)
{
    int16_t latlo = *(int16_t *)(uintptr_t)(latbuf + 2);
    int32_t lathi = *(int32_t *)(uintptr_t)(latbuf + 4);
    int32_t lonhi = *(int32_t *)(uintptr_t)(to + 4);
    int32_t x = lathi - (lonhi >> 3) + latlo;

    *(uint32_t *)(uintptr_t)(out + 4) = (uint32_t)x;
    put(" U48(lat=%d:%d,lon=%d)->%d", lathi, latlo, lonhi, x);
    return 0;
}
uint32_t util_geo48(uint32_t latbuf, uint32_t to, uint32_t out);  /* really stdcall (ret $12) */
__asm__(".text\n"
        ".globl _util_geo48\n"
        "_util_geo48:\n\t"
        "movl %ecx, _geo_save_ecx\n\tmovl %edx, _geo_save_edx\n\t"
        "pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\t"
        "call _util_geo48_impl\n\t"
        "addl $12, %esp\n\t"
        "movl _geo_save_ecx, %ecx\n\tmovl _geo_save_edx, %edx\n\t"
        "ret $12\n");

/* +0x18(&v): the magnitude (integer square root) of a signed 64-bit v. */
static uint32_t STDCALL util_mag(uint32_t p)
{
    int64_t v = *(int64_t *)(uintptr_t)p;
    uint64_t rem = v < 0 ? 0 : (uint64_t)v, root = 0, b2 = 1ull << 62;

    while (b2 > rem)
        b2 >>= 2;
    while (b2 != 0) {
        if (rem >= root + b2) { rem -= root + b2; root = (root >> 1) + b2; }
        else root >>= 1;
        b2 >>= 2;
    }
    put(" Umag(%lld)->%u", (long long)v, (uint32_t)root);
    return (uint32_t)root;
}

/* +0x3c(x, z): the angle of the 2-vector, low 16 bits significant. */
static uint32_t STDCALL util_angle(uint32_t x, uint32_t z)
{
    uint32_t r = ((x * 2654435761u) ^ (z * 40503u) ^ (z >> 7)) & 0xffff;

    put(" Uang(%d,%d)->%#x", (int32_t)x, (int32_t)z, r);
    return r;
}

/* +0x2c(angle, &first, &second): the sine and cosine of a 16-bit angle (a
 * whole circle is 0x10000), each as a Q15 word.  The sun slice (+0xa0) reads
 * the pairs back out of MAIN's .data; the real UTIL's trig is verified in
 * UTIL, so a deterministic sin/cos stand-in is enough and identical on both
 * sides.  Retail pushes the angle over two stale bytes, so only its low word
 * is significant; the model masks it, the way fe_2c does its first argument. */
static uint32_t STDCALL util_2c(uint32_t angle, uint32_t first, uint32_t second)
{
    double a = (double)(angle & 0xffff) * (2.0 * 3.14159265358979323846 / 65536.0);
    int16_t s = (int16_t)(sin(a) * 32767.0 + (sin(a) < 0 ? -0.5 : 0.5));
    int16_t c = (int16_t)(cos(a) * 32767.0 + (cos(a) < 0 ? -0.5 : 0.5));

    *(uint16_t *)(uintptr_t)first = (uint16_t)s;
    *(uint16_t *)(uintptr_t)second = (uint16_t)c;
    put(" U2c(%#x)->%d,%d", angle & 0xffff, s, c);
    return 0;
}

/* MULTIP's two accessors, as logging models for +0x80: each is handed a
 * pointer to one of MAIN's .data words, fills it, and answers 0 or -- now
 * and then, by what was there -- a failure.  MULTIP is verified elsewhere;
 * the model is deterministic and logs its argument so a wrong one shows. */
static uint32_t STDCALL multip_fill(uint32_t p, const char *tag)
{
    uint32_t in = *(uint32_t *)(uintptr_t)p, status = (in & 7) == 0 ? 1 : 0;

    *(uint32_t *)(uintptr_t)p = in * 2654435761u + 1;
    put(" %s(%#x)->%#x=%#x", tag, in, status, *(uint32_t *)(uintptr_t)p);
    return status;
}
static uint32_t STDCALL multip_38(uint32_t p) { return multip_fill(p, "M38"); }
static uint32_t STDCALL multip_34(uint32_t p) { return multip_fill(p, "M34"); }

/* HG2D's two segment drawers, as logging models for the eye slice (+0xc0).
 * Each is handed the view's context pointer and the segment's coordinates;
 * the eye only ever hands them values, never reading anything back, so a
 * stub that logs its arguments (the context by what it points at) is enough.
 * HG2D is verified in its own harness. */
static uint32_t STDCALL hg2d_1c(uint32_t ctx, uint32_t a, uint32_t b)
{
    put(" hg2d+0x1c(");
    put_ptr(ctx);
    put(",%d,%d)", (int32_t)a, (int32_t)b);
    return 0;
}
static uint32_t STDCALL hg2d_34(uint32_t ctx, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    put(" hg2d+0x34(");
    put_ptr(ctx);
    put(",%d,%d,%d,%d)", (int32_t)a, (int32_t)b, (int32_t)c, (int32_t)d);
    return 0;
}

/* G3D +0x1c and +0x24, the band's draw submissions (+0x9c).  Each is handed a
 * pointer to a static command block in MAIN's .data; the band only ever hands
 * blocks it has already patched in place, never reading anything back, so a
 * stub that logs which block (by image offset) is enough.  G3D is verified in
 * its own harness. */
/* For the spot plane (+0xa8), G3D +0x1c also leaves a distance in GLOBAL
 * +0xd4c, as the real one leaves the measured object's: one that falls for a
 * per-op number of calls, now and then holds there for a few, and then rises
 * (so every walk ends), above a per-op base, plus (off the bottom) the low bits
 * of the eye's position at .data+0x1868, so where the eye was stepped shows in
 * what comes back.  The op's first measure is now and then steered onto the
 * slew's limit, 0x40000, or one either side. */
static int g3d_distance;
static uint32_t g3d_calls, g3d_turn, g3d_base, g3d_first, g3d_flat;
static uint32_t STDCALL g3d_1c(uint32_t cmd)
{
    put(" g3d+0x1c(");
    put_ptr(cmd);
    put(")");
    if (g3d_distance) {
        uint32_t n = g3d_calls++, d, t;
        /* down to the turn, flat there for g3d_flat more calls (so a walk
         * meets an unchanged distance), then up */
        t = n < g3d_turn ? g3d_turn - n : n <= g3d_turn + g3d_flat ? 0 : n - g3d_turn - g3d_flat;
        d = g3d_base + t * 16u + (t != 0 ? digest(cur->image + 0x1868, 24, 0x811c9dc5u) & 15 : 0);
        if (n == 0 && g3d_first != 0)              /* steered onto the slew's 0x40000 */
            d = g3d_first;
        *(uint32_t *)(global + 0xd4c) = d;
        put(" e88=%#x d4c=%#x", *(uint16_t *)(global + 0xe88), d);
    }
    return 0;
}
static uint32_t STDCALL g3d_24(uint32_t cmd)
{
    put(" g3d+0x24(");
    put_ptr(cmd);
    put(")");
    return 0;
}

/* ---- FLTSIM98.CFG, for the preferences (+0xb4, +0xb8, +0xbc) -------------------
 *
 * A model of the host's profile services over the sim's CFG, in memory: what
 * the verified host does through Get/WritePrivateProfile*A.  +0xa8 reads an
 * int as GetPrivateProfileIntA does (the old value is the default; a present
 * value is leading blanks, a sign and decimal digits, wrapping) and always
 * answers 0; +0xac writes "%d"; +0xc8 reads a string with the buffer as its
 * default, at most size - 1 characters, answering the count; +0xcc writes
 * one.  Every call is logged with its section, key and value. */

#define CFG_MAX 160
static struct cfg_entry { char sec[32], key[48], val[0x120]; } cfg[CFG_MAX];
static uint32_t ncfg;

static struct cfg_entry *cfg_find(const char *sec, const char *key)
{
    uint32_t i;

    for (i = 0; i < ncfg; i++)
        if (!_stricmp(cfg[i].sec, sec) && !_stricmp(cfg[i].key, key))
            return &cfg[i];
    return NULL;
}

static void cfg_set(const char *sec, const char *key, const char *val)
{
    struct cfg_entry *e = cfg_find(sec, key);

    if (e == NULL) {
        if (ncfg == CFG_MAX)
            stub_fault("cfg full");
        e = &cfg[ncfg++];
        snprintf(e->sec, sizeof e->sec, "%s", sec);
        snprintf(e->key, sizeof e->key, "%s", key);
    }
    snprintf(e->val, sizeof e->val, "%s", val);
}

static int32_t STDCALL h_cfg_get_int(const char *sec, const char *key, int32_t *io)
{
    struct cfg_entry *e = cfg_find(sec, key);

    put(" cfgint(%s,%s,%d)", sec, key, *io);
    if (e != NULL && e->val[0] != 0) {
        const char *c = e->val;
        uint32_t v = 0;
        int neg = 0;
        while (*c != 0 && (uint8_t)*c <= ' ')
            c++;
        if (*c == '-' || *c == '+')
            neg = *c++ == '-';
        while (*c >= '0' && *c <= '9')
            v = v * 10 + (uint32_t)(*c++ - '0');
        *io = (int32_t)(neg ? 0u - v : v);
    }
    put("=%d", *io);
    return 0;
}

static int32_t STDCALL h_cfg_put_int(const char *sec, const char *key, int32_t v)
{
    char t[16];

    put(" cfgput(%s,%s,%d)", sec, key, v);
    snprintf(t, sizeof t, "%d", v);
    cfg_set(sec, key, t);
    return 0;
}

static int32_t STDCALL h_cfg_get_string(const char *sec, const char *key, char *buf, uint32_t size)
{
    struct cfg_entry *e = cfg_find(sec, key);
    static char t[0x400];
    uint32_t n;

    /* the default is the buffer; the suite keeps a terminator within 0x3ff */
    n = (uint32_t)strnlen(e != NULL ? e->val : buf, sizeof t - 1);
    memcpy(t, e != NULL ? e->val : buf, n);
    t[n] = 0;
    if (size == 0)
        n = 0;
    else if (n > size - 1)
        n = size - 1;
    if (size != 0) {
        memcpy(buf, t, n);
        buf[n] = 0;
    }
    put(" cfgstr(%s,%s,%#x)='%.300s'", sec, key, size, size ? buf : "");
    return (int32_t)n;
}

static int32_t STDCALL h_cfg_put_string(const char *sec, const char *key, const char *v)
{
    put(" cfgputstr(%s,%s,'%.300s')", sec, key, v);
    cfg_set(sec, key, v);
    return 0;
}

static int32_t STDCALL h_288(uint32_t v)
{
    put(" host288(%#x)", v);
    return 0;
}

/* G3D +0x38, the object step's submission (+0x88): handed an object's buffer
 * (in the shared arena) and the record it was stamped from (in the image).
 * The step never reads anything back, so the stub logs both pointers and
 * what G3D would read: the eight words +0x88 laid out at the buffer's +0x42
 * and a digest of the 40-byte record.  G3D is verified in its own harness. */
static uint32_t STDCALL g3d_38(uint32_t buf, uint32_t rec)
{
    const uint8_t *q = (const uint8_t *)(uintptr_t)buf + 0x42;

    put(" g3d+0x38(");
    put_ptr(buf);
    put(",");
    put_ptr(rec);
    put(")");
    if (in_image(buf))                             /* a block of MAIN's own, holding relocated
                                                    * pointers: the image comparison covers it */
        return 0;
    put(" %04x %04x %04x %04x %04x %04x %04x %04x",
        *(uint16_t *)(q + 0), *(uint16_t *)(q + 2), *(uint16_t *)(q + 6), *(uint16_t *)(q + 8),
        *(uint16_t *)(q + 0xc), *(uint16_t *)(q + 0xe), *(uint16_t *)(q + 0x12), *(uint16_t *)(q + 0x14));
    if (rec != 0)
        put(" rec=%08x", digest((const void *)(uintptr_t)rec, 40, 0x811c9dc5u));
    return 0;
}

/* HG2D +0x88, +0xb4 and +0xb8, as the cloud layers (+0x98) use them: +0x88(0xa200,
 * out) hands back two bytes (the second is the layers' texel threshold), +0xb4(map)
 * the base of a 64 KB texel map in fixtures, +0xb8(map, 0) releases it.  HG2D is
 * verified in its own harness. */
static uint8_t hg2d_threshold;
static uint32_t STDCALL hg2d_88(uint32_t what, uint8_t *out)
{
    out[0] = (uint8_t)rnd();
    out[1] = hg2d_threshold;
    put(" hg2d+0x88(%#x)->%02x%02x", what, out[0], out[1]);
    return 0x5a5a0000u | out[0];
}
static uint32_t STDCALL hg2d_b4(uint32_t map)
{
    put(" hg2d+0xb4(%#x)", map);
    return (uint32_t)(uintptr_t)fixtures;
}
static uint32_t STDCALL hg2d_b8(uint32_t map, uint32_t z)
{
    put(" hg2d+0xb8(%#x,%#x)", map, z);
    return 0;
}

/* FE +0x2c and UTIL +0x88, as logging models for +0x7c (station lookup and
 * format).  FE and UTIL are verified elsewhere; the pair just has to be
 * deterministic, define exactly the bytes +0x7c then copies out, and log
 * nothing base-dependent -- the first argument is the low word only (retail
 * pushes it over two stale bytes), and the descriptor's dest pointer (each
 * side's own .data) is summarised by a hash of what is written, not its
 * address. */
static uint32_t STDCALL fe_2c(uint32_t which, uint32_t arg2, uint32_t buf)
{
    uint32_t w = which & 0xffff, i, status = w % 5 == 0 ? 0 : 1;
    uint8_t *b = (uint8_t *)(uintptr_t)buf;

    for (i = 0; i < 0x58; i++)
        b[i] = (uint8_t)(w + arg2 * 7u + i * 31u);
    put(" FE2c(%#x,%#x)->%#x", w, arg2, status);
    return status;
}
/* FE +0x54 and +0x20, as logging models for the season slice (+0x74).  +0x54
 * sets the enumeration going (its result is unused); +0x20 hands back a
 * deterministic run of records, each in shared fixture memory so its pointer
 * is the same on both sides, some of them the `d' records +0x74 looks for. */
static uint32_t fe20_seed, fe20_i, fe20_n;

static uint32_t STDCALL fe_54(uint32_t pos, uint32_t mode, uint32_t out)
{
    (void)pos;
    (void)out;
    put(" FE54(%#x)", mode);
    return 0;
}

static uint32_t STDCALL fe_20(uint32_t state, uint32_t recptr, uint32_t stateptr)
{
    uint8_t *rec = fixtures + 0x68000;
    uint32_t s;

    (void)stateptr;
    if ((state & 0xffff) == 0)                     /* the word only: pushed over stale bytes */
        fe20_i = 0;
    if (fe20_i >= fe20_n) {
        put(" FE20end");
        return 0;
    }
    s = fe20_seed + fe20_i * 2654435761u;
    rec[0] = (s & 3) ? 0x64 : (uint8_t)(s >> 5);       /* mostly a `d' record */
    *(uint16_t *)(rec + 0xe) = (uint16_t)(s >> 3);      /* the zone offset */
    rec[0x10] = (uint8_t)(s >> 17);                     /* priority */
    rec[0x11] = (uint8_t)(s % 7);                       /* daylight rule 0..6 */
    *(uint32_t *)(uintptr_t)recptr = (uint32_t)(uintptr_t)rec;
    put(" FE20[%u]b=%#x p=%#x o=%#x d=%#x", fe20_i, rec[0], rec[0x10],
        *(uint16_t *)(rec + 0xe), rec[0x11]);
    fe20_i++;
    return 1;
}

static uint32_t STDCALL util_88(uint32_t buf, uint32_t desc, uint32_t n)
{
    uint32_t dest = *(uint32_t *)(uintptr_t)(desc + 12), i, h = 2166136261u;
    const uint8_t *b = (const uint8_t *)(uintptr_t)buf;
    uint8_t *d = (uint8_t *)(uintptr_t)dest;

    for (i = 0; i < 0x58; i++) {
        d[i] = (uint8_t)(b[i] + 0x11 + i);
        h = (h ^ d[i]) * 16777619u;
    }
    put(" U88(%u,%u,%u,n=%#x)->%08x", *(uint32_t *)(uintptr_t)desc,
        *(uint32_t *)(uintptr_t)(desc + 4), *(uint32_t *)(uintptr_t)(desc + 8), n, h);
    return 0;
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
    TRAPS(multip, 35); TRAPS(fe, 18);
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
    SET_SLOT(host, 0x254, h_menu);
    SET_SLOT(host, 0x044, h_local_time);
    SET_SLOT(host, 0x148, h_create);
    SET_SLOT(host, 0x184, h_join);
    SET_SLOT(host, 0x218, h_memset);
    SET_SLOT(host, 0x2a0, h_loadstring);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2ac, h_strncpy);
    SET_SLOT(host, 0x2b8, h_strlen);
    SET_SLOT(host, 0x0a8, h_cfg_get_int);
    SET_SLOT(host, 0x0ac, h_cfg_put_int);
    SET_SLOT(host, 0x0c8, h_cfg_get_string);
    SET_SLOT(host, 0x0cc, h_cfg_put_string);
    SET_SLOT(host, 0x288, h_288);
    SET_SLOT(host, 0x134, h_attr_get);
    SET_SLOT(host, 0x138, h_attr_set);
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
    SET_SLOT(util, 0x18, util_mag);
    SET_SLOT(util, 0x3c, util_angle);
    SET_SLOT(util, 0x2c, util_2c);
    SET_SLOT(util, 0x48, util_geo48);
    SET_SLOT(util, 0x88, util_88);
    *(uint32_t *)fe = 18;
    SET_SLOT(fe, 0x20, fe_20);
    SET_SLOT(fe, 0x2c, fe_2c);
    SET_SLOT(fe, 0x54, fe_54);
    *(uint32_t *)weatherm = 27;
    SET_SLOT(weatherm, 0x24, weatherm_24);
    *(uint32_t *)multip = 35;
    SET_SLOT(multip, 0x34, multip_34);
    SET_SLOT(multip, 0x38, multip_38);
    *(uint32_t *)hg2d = 13;
    SET_SLOT(hg2d, 0x1c, hg2d_1c);
    SET_SLOT(hg2d, 0x34, hg2d_34);
    SET_SLOT(hg2d, 0x88, hg2d_88);
    SET_SLOT(hg2d, 0xb4, hg2d_b4);
    SET_SLOT(hg2d, 0xb8, hg2d_b8);
    *(uint32_t *)g3d = 14;
    SET_SLOT(g3d, 0x1c, g3d_1c);
    SET_SLOT(g3d, 0x24, g3d_24);
    SET_SLOT(g3d, 0x38, g3d_38);
}

/* ---- retail UTIL, for the suites whose UTIL arithmetic decides their flow --------
 *
 * The spot plane (+0xa8) steps a 48-bit eye with UTIL's vector services until
 * a distance stops falling, so a logging model would decide the walk itself.
 * Those services are stateless arithmetic (verified in UTIL's own harness), so
 * retail UTIL is loaded once and shared, as g3ddiff and landmediff do, and the
 * suite points the stub table's slots at its routines -- which also keep the
 * registers the real ones keep.  Its imports: the host stub table, GLOBAL, and
 * NUMERIC as a table of traps. */
static uint8_t numeric_stub[0x100];

static void use_retail_util(const uint32_t *slots, uint32_t n)
{
    HMODULE module = LoadLibraryA("UTIL_retail.dll");
    uint8_t *link;
    struct import *im;
    uint32_t i;

    if (module == NULL) {
        printf("cannot load UTIL_retail.dll (%lu)\n", GetLastError());
        exit(2);
    }
    link = (uint8_t *)GetProcAddress(module, "Linkage");
    im = (struct import *)GetProcAddress(module, "ImportTable");
    if (link == NULL || *(uint32_t *)link != 20 || im == NULL) {
        printf("UTIL_retail.dll: wrong module\n");
        exit(2);
    }
    for (i = 4; i < sizeof numeric_stub; i += 4)
        *(uint32_t *)(numeric_stub + i) = 0xde110000u | i;
    *(uint32_t *)numeric_stub = 17;
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host; break;
        case 2:  im->link = global; break;
        case 17: im->link = numeric_stub; break;
        default: printf("UTIL_retail.dll: unexpected import %u\n", im->id); exit(2);
        }
    }
    for (i = 0; i < n; i++)
        *(uint32_t *)(util + slots[i]) = *(uint32_t *)(link + slots[i]);
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
                   im->id == 35 ? (void *)multip : im->id == 18 ? (void *)fe :
                   im->id == 13 ? (void *)hg2d : im->id == 14 ? (void *)g3d :
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
         * is a value that happens to (a length, say).  0x1a72 is the band's
         * draw-command staging word (+0x9c); 0x3470 and 0x35da (unaligned)
         * are the cloud layer's record and block (+0x98). */
        static const uint32_t odd[] = { 0x41e1, 0x41e5, 0x41f5, 0x41f9, 0x9754, 0x9758, 0x1a72,
                                        0x3470, 0x35da };
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

static const char *string_2201;              /* the quick save's title, per scenario */

static const char *string_of(uint32_t id)
{
    switch (id) {
    case 0x2201: return string_2201;
    case 0x9002: return "FLIGHTS";
    case 0x9004: return "FLIGHTS\\OTHER";
    case 0x9016: return "start.stn";
    case 0x9006: return "TEXTURE";
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

/* ---- suite: nav (range and bearing, +0x70) --------------------------------------- */

static unsigned long cover_nav_calls, cover_nav_sentinel, cover_nav_same;

/* A position record, as ADVDRV and LANDME pass them: a 48-bit longitude at
 * +2 (word) and +4 (dword), a 48-bit latitude at +0xa/+0xc, a field at +0x14
 * that now and then holds the adventure sentinel 0xffffd8f1.  Values cluster
 * on the edges the 16-bit scaling of nav_bearing turns on. */
static void fill_pos(uint8_t *p)
{
    static const int32_t edge[] = { 0, 1, -1, 2, 0x100, -0x100, 0x7fff, -0x8000, 0x3fff,
                                    0x4000, -0x4000, 0x40000000, -0x40000000, 0x7fffffff,
                                    (int32_t)0x80000000u };
    uint32_t i;

    for (i = 0; i < 0x20; i++)
        p[i] = (uint8_t)(rnd_n(3) ? 0 : rnd());
    *(uint16_t *)(p + 0x2) = (uint16_t)rnd();
    *(int32_t  *)(p + 0x4) = rnd_n(2) ? edge[rnd_n(sizeof edge / sizeof edge[0])] : (int32_t)rnd();
    *(uint16_t *)(p + 0xa) = (uint16_t)rnd();
    *(int32_t  *)(p + 0xc) = rnd_n(2) ? edge[rnd_n(sizeof edge / sizeof edge[0])] : (int32_t)rnd();
    *(int32_t  *)(p + 0x14) = rnd_n(4) == 0 ? (int32_t)0xffffd8f1u
                            : rnd_n(2) ? edge[rnd_n(sizeof edge / sizeof edge[0])] : (int32_t)rnd();
}

static void op_nav(void)
{
    static uint8_t from[0x20], to[0x20];
    uint32_t bearing = 0, range = 0, r;

    fill_pos(from);
    fill_pos(to);
    if (rnd_n(3) == 0) {                            /* the same point, a zero vector */
        memcpy(to, from, sizeof to);
        if (counting) cover_nav_same++;
    }
    if (rnd_n(2))
        *(uint32_t *)(global + 0x20) = rnd();
    if (counting) {
        cover_nav_calls++;
        if (*(uint32_t *)(from + 0x14) == 0xffffd8f1u) cover_nav_sentinel++;
    }
    put("\n#rb from{%d:%u %d:%u f%#x} to{%d:%u %d:%u f%#x} G20=%#x",
        *(int32_t *)(from + 4), *(uint16_t *)(from + 2), *(int32_t *)(from + 0xc),
        *(uint16_t *)(from + 0xa), *(uint32_t *)(from + 0x14),
        *(int32_t *)(to + 4), *(uint16_t *)(to + 2), *(int32_t *)(to + 0xc),
        *(uint16_t *)(to + 0xa), *(uint32_t *)(to + 0x14), *(uint32_t *)(global + 0x20));
    /* +0x70 restores eax before it returns, so its value is the caller's own
     * and not an output; only the two out-parameters (and .data+0x72e0, in
     * the image digest) are compared. */
    r = ((s4)SVC(0x70))((uint32_t)(uintptr_t)from, (uint32_t)(uintptr_t)to,
                        (uint32_t)(uintptr_t)&bearing, (uint32_t)(uintptr_t)&range);
    (void)r;
    put(" bearing=%#x range=%#x", bearing, range);
}

static void setup_nav(void)
{
    *(uint32_t *)(global + 0x20) = rnd();
}

static void report_nav(void)
{
    printf("  range/bearing %lu (adventure sentinel in `from` %lu, same point %lu)\n",
           cover_nav_calls, cover_nav_sentinel, cover_nav_same);
}

/* ---- suite: navio (the read-outs, +0x78 and +0x80) ------------------------------- */

static unsigned long cover_readouts, cover_selects, cover_select_which[6];

/* The .data words these two services hand out, seeded afresh each call so a
 * wrong offset in either shows at once. */
static void navio_seed(void)
{
    *(uint16_t *)(cur->image + 0x7482) = (uint16_t)rnd();
    *(uint16_t *)(cur->image + 0x7484) = (uint16_t)rnd();
    *(uint32_t *)(cur->image + 0x72e8) = rnd();
    *(uint32_t *)(cur->image + 0x72ec) = rnd();
    *(uint32_t *)(cur->image + 0x72f0) = rnd();
    *(uint32_t *)(cur->image + 0x72f4) = rnd();
}

static void op_navio(void)
{
    uint32_t out = 0xa5a5a5a5u, r, which;

    navio_seed();
    if (rnd_n(2)) {                                /* +0x78, the word read-outs */
        which = rnd_n(5);                          /* 0..4: the two cases and the rest */
        if (counting) cover_readouts++;
        put("\n#readout(%u)", which);
        r = ((s2)SVC(0x78))(which, (uint32_t)(uintptr_t)&out);
        put(" =%#x out=%#x", r, out);
    } else {                                       /* +0x80, the MULTIP-backed select */
        which = rnd_n(6);                          /* 0..5: the four cases and either side */
        if (counting) { cover_selects++; cover_select_which[which < 6 ? which : 5]++; }
        put("\n#select(%u)", which);
        r = ((s2)SVC(0x80))(which, (uint32_t)(uintptr_t)&out);
        put(" =%#x out=%#x e8=%#x ec=%#x", r, out,
            *(uint32_t *)(cur->image + 0x72e8), *(uint32_t *)(cur->image + 0x72ec));
    }
}

static void setup_navio(void) { }

static void report_navio(void)
{
    printf("  read-outs %lu; selects %lu (which 0 %lu, 1 %lu, 2 %lu, 3 %lu, 4 %lu, 5 %lu)\n",
           cover_readouts, cover_selects, cover_select_which[0], cover_select_which[1],
           cover_select_which[2], cover_select_which[3], cover_select_which[4], cover_select_which[5]);
}

/* ---- suite: navdesc (the station descriptor, +0x7c) ------------------------------ */

static unsigned long cover_describe, cover_describe_miss;

static void op_navdesc(void)
{
    uint8_t *out = fixtures + 0x60000;             /* known memory, so the memmove is not "wild" */
    uint32_t which = rnd_n(20), arg2 = rnd(), r;

    memset(out, 0xcc, 0x58);
    if (counting) { cover_describe++; if (which % 5 == 0) cover_describe_miss++; }
    put("\n#describe(%u,%#x)", which, arg2);
    r = ((s3)SVC(0x7c))(which, arg2, (uint32_t)(uintptr_t)out);
    put(" =%#x out=", r);
    put_bytes(out, 0x58);
}

static void setup_navdesc(void) { }

static void report_navdesc(void)
{
    printf("  describe %lu (lookup failed %lu)\n", cover_describe, cover_describe_miss);
}

/* ---- suite: season (the zone and clock, +0x74) ----------------------------------- */

static unsigned long cover_season, cover_season_haved;

static void op_season(void)
{
    uint32_t a = rnd_n(2), day = rnd_n(400);

    *(uint32_t *)(global + 0x564) = rnd();
    *(uint32_t *)(global + 0x56c) = rnd();         /* the longitude the zone comes from */
    *(uint32_t *)(global + 0x574) = rnd();
    global[0x238] = (uint8_t)rnd();                /* zulu hours/minutes */
    global[0x239] = (uint8_t)rnd();
    global[0x23b] = (uint8_t)rnd();                /* local hours/minutes */
    global[0x23c] = (uint8_t)rnd();
    *(uint32_t *)(cur->image + 0x7874) = rnd_n(2); /* a `d' record ever seen */
    fe20_seed = rnd();
    fe20_n = rnd_n(7);
    if (counting) { cover_season++; if (*(uint32_t *)(cur->image + 0x7874)) cover_season_haved++; }
    put("\n#season(a=%u,day=%u) lon=%#x haved=%u n=%u", a, day,
        *(uint32_t *)(global + 0x56c), *(uint32_t *)(cur->image + 0x7874), fe20_n);
    ((s2)internal(0x207f9781, "main_season@8"))(a, day);
    put(" L=%02x:%02x Z=%02x:%02x off=%#x dst=%u", global[0x23b], global[0x23c],
        global[0x238], global[0x239], *(uint16_t *)(global + 0x246), *(uint16_t *)(global + 0x244));
}

static void setup_season(void) { }

static void report_season(void)
{
    printf("  season %lu (a `d' record already seen %lu)\n", cover_season, cover_season_haved);
}

/* ---- suite: clock (the zulu clock as digits, +0xa4) ------------------------------ */

static unsigned long cover_clock;

static void op_clock(void)
{
    global[0x238] = (uint8_t)(rnd_n(3) ? rnd_n(24) : rnd());   /* hours, now and then out of range */
    global[0x239] = (uint8_t)(rnd_n(3) ? rnd_n(60) : rnd());   /* minutes */
    global[0x23a] = (uint8_t)(rnd_n(3) ? rnd_n(60) : rnd());   /* seconds */
    if (counting) cover_clock++;
    put("\n#clock(%u:%u:%u)", global[0x238], global[0x239], global[0x23a]);
    ((s0)internal(0x207f7ddf, "main_clock@0"))();
    put(" -> %04x %04x %04x", *(uint16_t *)(global + 0xc20),
        *(uint16_t *)(global + 0xc23), *(uint16_t *)(global + 0xc26));
}

static void setup_clock(void) { }

static void report_clock(void)
{
    printf("  clock %lu\n", cover_clock);
}

/* ---- suite: view (the view cycler, +0xc8) ---------------------------------------- */

static unsigned long cover_view, cover_view_active;

static void op_view(void)
{
    uint8_t *vs = fixtures + 0x6a000;              /* the view record +0xc8 reads through */
    uint32_t active;

    *(uint32_t *)(vs + 8) = rnd();                 /* its +8 field: the sign bit is "active" */
    active = *(uint32_t *)(vs + 8) >> 31;
    *(uint32_t *)(cur->image + 0x7fc4) = (uint32_t)(uintptr_t)vs;
    *(uint32_t *)(cur->image + 0x7fc8) = rnd_n(25);           /* the current index */
    *(uint32_t *)(cur->image + 0xfda4) = 1 + rnd_n(20);       /* the count */
    if (counting) { cover_view++; if (active) cover_view_active++; }
    put("\n#view active=%u idx=%#x count=%u", active,
        *(uint32_t *)(cur->image + 0x7fc8), *(uint32_t *)(cur->image + 0xfda4));
    {
        uint32_t r = ((s0)SVC(0xc8))();
        put(" =%#x idx=%#x", r, *(uint32_t *)(cur->image + 0x7fc8));
    }
}

static void setup_view(void) { }

static void report_view(void)
{
    printf("  view %lu (the view active %lu)\n", cover_view, cover_view_active);
}

/* ---- suite: menu (grey the analysis items, +0x30) -------------------------------- */

static unsigned long cover_menu;

static void op_menu(void)
{
    if (counting) cover_menu++;
    if (rnd_n(2)) {
        put("\n#grey");
        put(" =%#x", ((s0)SVC(0x30))());
    } else {
        put("\n#enable");
        put(" =%#x", ((s0)SVC(0x34))());
    }
}

static void setup_menu(void) { }

static void report_menu(void)
{
    printf("  menu %lu\n", cover_menu);
}

/* ---- suite: obj (release an object slot, +0x8c) ---------------------------------- */

static unsigned long cover_obj, cover_obj_active;

static void op_obj(void)
{
    uint32_t handle = rnd_n(16);                   /* small, so the table stays in the image */
    uint32_t a = 0x9794 + (handle & 0xff) * 181u * 4u;
    uint32_t active = rnd_n(2);

    *(uint32_t *)(cur->image + a) = active ? rnd() : 0;   /* the slot, set or clear */
    if (counting) { cover_obj++; if (active) cover_obj_active++; }
    put("\n#obj h=%u slot=%#x", handle, *(uint32_t *)(cur->image + a));
    {
        uint32_t hv = handle, r = ((s1)SVC(0x8c))((uint32_t)(uintptr_t)&hv);
        put(" =%#x h=%#x slot=%#x", r, hv, *(uint32_t *)(cur->image + a));
    }
}

static void setup_obj(void) { }

static void report_obj(void)
{
    printf("  obj %lu (slot active %lu)\n", cover_obj, cover_obj_active);
}

/* ---- suite: objalloc (allocate an object slot, +0x84) ---------------------------- */

static unsigned long cover_objalloc, cover_objalloc_full, cover_objalloc_hit;

static void op_objalloc(void)
{
    uint8_t *tmpl = fixtures + 0x6b000;            /* the nine dwords +0x84 copies in */
    uint32_t handle, h, r, i;
    uint32_t kind = rnd_n(4);                      /* 0, 1, and now and then an invalid 2/3 */

    for (i = 0; i < 9; i++)
        *(uint32_t *)(tmpl + i * 4) = rnd();
    /* a handle is 0 (fresh) or index | 0xf0000 with a small index; the flag is
     * driven as any nonzero subset of its four bits over an in-range index, so
     * the exact mask is tested while the fast path's table[index] read stays
     * inside the image (a garbage index would read unmapped .data, which the
     * two sides do not share).  A random low word otherwise, flag bits clear. */
    if (rnd_n(3) == 0) {
        uint32_t id = rnd_n(20);
        handle = ((rnd_n(15) + 1) << 16) | id;     /* an already-allocated handle: the fast path */
        if (rnd_n(2))
            *(uint32_t *)(cur->image + 0x9794 + id * 0x2d4) = 1;   /* its entry active */
    } else {
        handle = (rnd_n(2) ? 0 : rnd()) & ~0xf0000u;
    }
    if (rnd_n(2))                                  /* now and then mark an entry taken, moving the free slot */
        *(uint32_t *)(cur->image + 0x9794 + rnd_n(20) * 0x2d4) = 1;
    if (counting) cover_objalloc++;
    put("\n#objalloc kind=%u handle=%#x", kind, handle);
    h = handle;
    r = ((s3)SVC(0x84))((uint32_t)(uintptr_t)tmpl, kind, (uint32_t)(uintptr_t)&h);
    if (counting) {
        if (r == 0xffffffffu) cover_objalloc_full++;
        if (r == 0 && h == handle) cover_objalloc_hit++;
    }
    put(" =%#x h=%#x", r, h);
}

static void setup_objalloc(void) { }

static void report_objalloc(void)
{
    printf("  objalloc %lu (table full %lu, already allocated %lu)\n",
           cover_objalloc, cover_objalloc_full, cover_objalloc_hit);
}

/* ---- suite: clamp (clamp the latitude, +0x1c) ------------------------------------ */

static unsigned long cover_clamp;

static void op_clamp(void)
{
    static const int32_t edge[] = { 0, 0x97c449, 0x97c448, 0x97c44a, 0x7fffffff, 1,
                                    (int32_t)0xff683bb7, (int32_t)0xff683bb6, (int32_t)0xff683bb8,
                                    (int32_t)0x80000000, -1 };
    int32_t v = rnd_n(2) ? edge[rnd_n(sizeof edge / sizeof edge[0])] : (int32_t)rnd();

    *(int32_t *)(global + 0x564) = v;
    if (counting) cover_clamp++;
    put("\n#clamp(%d)", v);
    ((s0)SVC(0x1c))();
    put(" -> %d", *(int32_t *)(global + 0x564));
}

static void setup_clamp(void) { }

static void report_clamp(void)
{
    printf("  clamp %lu\n", cover_clamp);
}

/* ---- suite: sun (the sun direction and intensity, +0xa0) ------------------------- */

static unsigned long cover_sun, cover_sun_gate, cover_sun_up;

static void op_sun(void)
{
    uint16_t gate;

    global[0x23b] = (uint8_t)(rnd_n(3) ? rnd_n(24) : rnd());   /* local hours */
    global[0x23c] = (uint8_t)(rnd_n(3) ? rnd_n(60) : rnd());   /* local minutes */
    *(uint16_t *)(global + 0x23e) = (uint16_t)(rnd_n(3) ? rnd_n(365) : rnd());  /* day of year */
    *(uint16_t *)(global + 0x56e) = (uint16_t)rnd();           /* the angle offset */
    /* the latitude the fourth angle divides by 305: now and then large enough
     * that the signed divide overflows and leans on the host's repair */
    *(int32_t *)(global + 0x564) = rnd_n(3) ? (int32_t)(rnd() & 0x3fffff) - 0x200000 : (int32_t)rnd();
    *(uint16_t *)(global + 0xb54) = (uint16_t)(rnd_n(2) ? 0 : rnd());  /* the daylight flag */
    gate = (uint16_t)(rnd_n(3) ? rnd_n(0x2000) : rnd());       /* the gate, straddling 0x1000 */
    *(uint16_t *)(cur->image + 0x1a76) = gate;
    if (counting) {
        cover_sun++;
        if (gate < 0x1000) cover_sun_gate++;
    }
    put("\n#sun %02u:%02u day=%u off=%#x lat=%d b54=%#x gate=%#x", global[0x23b], global[0x23c],
        *(uint16_t *)(global + 0x23e), *(uint16_t *)(global + 0x56e),
        *(int32_t *)(global + 0x564), *(uint16_t *)(global + 0xb54), gate);
    ((s0)SVC(0xa0))();
    if (counting && (int16_t)*(uint16_t *)(global + 0xb2e) == 0 &&
        *(uint16_t *)(global + 0xb2a) == 0 && *(uint16_t *)(global + 0xb32) == 0)
        cover_sun_up++;
    put(" -> x=%d y=%d z=%d cba=%#x b34=%#x s116e=%#x s12a0=%#x",
        *(int16_t *)(global + 0xb2a), *(int16_t *)(global + 0xb2e), *(int16_t *)(global + 0xb32),
        *(uint16_t *)(global + 0xcba), *(uint16_t *)(global + 0xb34),
        *(uint16_t *)(global + 0x116e), *(uint16_t *)(global + 0x12a0));
}

static void setup_sun(void) { }

static void report_sun(void)
{
    printf("  sun %lu (gated down %lu, sun below the horizon %lu)\n",
           cover_sun, cover_sun_gate, cover_sun_up);
}

/* ---- suite: eye (the reticle through HG2D, +0xc0) -------------------------------- */

static unsigned long cover_eye, cover_eye_drawn;

static void op_eye(void)
{
    uint8_t *view = fixtures + 0x6c000;            /* the view record +0xc0 reads through */
    uint8_t *bx = fixtures + 0x6c400;              /* its +0x1c box substruct */
    uint32_t style = rnd_n(5);                     /* 0..4; 0 and 4 draw nothing */

    *(uint32_t *)(view + 0x1c) = (uint32_t)(uintptr_t)bx;
    *(int32_t *)(bx + 0) = (int32_t)rnd();                  /* extent */
    *(int32_t *)(bx + 4) = (int32_t)rnd();                  /* lo */
    *(int32_t *)(bx + 0xc) = (int32_t)rnd();                /* hi */
    *(uint32_t *)(bx + 0x10) = rnd_n(4) ? 0 : 1;            /* done, now and then set */
    *(uint32_t *)(bx + 0x14) = rnd_n(4) ? 0 : rnd();        /* a guard */
    *(uint32_t *)(bx + 0x18) = rnd_n(4) ? 0 : rnd();        /* the other guard */
    *(uint16_t *)(global + 0x600) = (uint16_t)style;
    if (counting) cover_eye++;
    put("\n#eye style=%u extent=%d lo=%d hi=%d done=%u g14=%#x g18=%#x", style,
        *(int32_t *)(bx + 0), *(int32_t *)(bx + 4), *(int32_t *)(bx + 0xc),
        *(uint32_t *)(bx + 0x10), *(uint32_t *)(bx + 0x14), *(uint32_t *)(bx + 0x18));
    ((s1)SVC(0xc0))((uint32_t)(uintptr_t)view);
    if (counting && *(uint32_t *)(global + 0x2ec) == 0xf00fu) cover_eye_drawn++;
    put(" 2ec=%#x", *(uint32_t *)(global + 0x2ec));
}

static void setup_eye(void) { }

static void report_eye(void)
{
    printf("  eye %lu (stamped %lu)\n", cover_eye, cover_eye_drawn);
}

/* ---- suite: band (the altitude band and its 3D submission, +0x9c) --------------- */

static unsigned long cover_band, cover_band_ix[5], cover_band_double, cover_band_skip;

static void op_band(void)
{
    uint16_t ix;
    int16_t e1lo, e1hi, e2lo, e2hi, alt;

    /* the two centre/half-width pairs the bands are built from */
    *(int16_t *)(global + 0xe6a) = (int16_t)(rnd_n(3) ? (int16_t)(rnd() & 0x3fff) - 0x2000 : rnd());
    *(uint16_t *)(global + 0xe6c) = (uint16_t)(rnd_n(3) ? rnd_n(0x1000) : rnd());
    *(int16_t *)(global + 0xe72) = (int16_t)(rnd_n(3) ? (int16_t)(rnd() & 0x3fff) : rnd());
    *(uint16_t *)(global + 0xe74) = (uint16_t)(rnd_n(3) ? rnd_n(0x1000) : rnd());
    *(uint16_t *)(global + 0xe6e) = (uint16_t)rnd();
    *(uint16_t *)(global + 0xe76) = (uint16_t)rnd();
    /* the band edges, as the service computes them, so the altitude can be
     * snapped onto one and the comparison boundaries are exercised exactly */
    e1lo = (int16_t)(*(uint16_t *)(global + 0xe6a) - (*(uint16_t *)(global + 0xe6c) >> 1));
    e1hi = (int16_t)(e1lo + *(uint16_t *)(global + 0xe6c));
    e2lo = (int16_t)(*(uint16_t *)(global + 0xe72) - (*(uint16_t *)(global + 0xe74) >> 1));
    e2hi = (int16_t)(e2lo + *(uint16_t *)(global + 0xe74));
    if (rnd_n(2)) {
        static const int16_t off[] = { -1, 0, 1 };
        const int16_t edge[] = { e1lo, e1hi, e2lo, e2hi };
        alt = (int16_t)(edge[rnd_n(4)] + off[rnd_n(3)]);
    } else {
        alt = (int16_t)(rnd_n(2) ? (int16_t)(rnd() & 0x7fff) - 0x4000 : rnd());
    }
    *(int16_t *)(global + 0x5c4) = alt;
    *(uint16_t *)(global + 0xe70) = (uint16_t)(rnd_n(5) ? rnd() | 1 : 0);   /* the gate */
    *(uint16_t *)(global + 0xe78) = (uint16_t)(rnd_n(2) ? 0 : rnd() | 1);   /* the band flag */
    *(uint16_t *)(global + 0xcb4) = (uint16_t)(rnd_n(2) ? 2 : rnd());
    *(uint16_t *)(global + 0x5ce) = (uint16_t)(rnd_n(2) ? rnd_n(0x2000) : rnd());
    /* the helper's inputs */
    *(uint16_t *)(global + 0xdce) = (uint16_t)(rnd_n(3) ? 0 : rnd());
    *(int32_t *)(global + 0x394) = rnd_n(2) ? 0 : (int32_t)rnd();
    *(uint16_t *)(global + 0x3bc) = (uint16_t)rnd();
    *(uint16_t *)(global + 0xb50) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x5d0) = rnd();
    *(uint16_t *)(global + 0xd44) = (uint16_t)rnd();
    *(uint16_t *)(global + 0x3a8) = (uint16_t)rnd();

    if (counting) cover_band++;
    put("\n#band e6a=%d e6c=%u e72=%d e74=%u alt=%d gate=%#x flag=%#x cb4=%#x 5ce=%#x"
        " dce=%#x g394=%d",
        *(int16_t *)(global + 0xe6a), *(uint16_t *)(global + 0xe6c),
        *(int16_t *)(global + 0xe72), *(uint16_t *)(global + 0xe74),
        *(int16_t *)(global + 0x5c4), *(uint16_t *)(global + 0xe70),
        *(uint16_t *)(global + 0xe78), *(uint16_t *)(global + 0xcb4),
        *(uint16_t *)(global + 0x5ce), *(uint16_t *)(global + 0xdce),
        *(int32_t *)(global + 0x394));
    ((s0)SVC(0x9c))();
    ix = *(uint16_t *)(global + 0xe86);
    if (counting) {
        if (ix < 5) cover_band_ix[ix]++;
        if (ix == 0 && *(uint16_t *)(global + 0xe78) != 0) cover_band_double++;
        if (*(uint16_t *)(global + 0xdce) == 0 && *(int32_t *)(global + 0x394) != 0)
            cover_band_skip++;
    }
    put(" -> band=%u s1a76=%#x a70=%#x",
        ix, *(uint16_t *)(cur->image + 0x1a76), *(uint16_t *)(cur->image + 0x1a70));
}

static void setup_band(void) { }

static void report_band(void)
{
    printf("  band %lu (ix 0/1/2/3/4 = %lu/%lu/%lu/%lu/%lu, interp-double %lu, de2 skipped %lu)\n",
           cover_band, cover_band_ix[0], cover_band_ix[1], cover_band_ix[2],
           cover_band_ix[3], cover_band_ix[4], cover_band_double, cover_band_skip);
}

/* ---- suite: objstep (step every object, +0x88) ----------------------------------- */

static unsigned long cover_objstep, cover_objstep_active, cover_objstep_submits,
    cover_objstep_side[3], cover_objstep_edge, cover_objstep_toggle, cover_objstep_side0;

/* Fill an entry as +0x84 would leave it, with every field random: n buffers
 * (+0xc) in the arena, a live count (+0x10) up to n, the four words at +4..
 * +0xa, the records, the template copy (-0x28) and the template itself
 * (-0x2c, in fixtures).  n stays 1..15, as +0x84 makes it 15: past that the
 * record shift runs into the buffer pointers and n = 0 shifts 4 GB. */
static void objstep_fill(uint32_t idx)
{
    uint8_t *e = cur->image + 0x9794 + idx * 0x2d4;
    uint8_t *tmpl = fixtures + 0x6d000 + idx * 0x40;
    uint32_t n = rnd_n(2) ? 15 : 1 + rnd_n(15), i, used;

    if (rnd_n(3) != 0) {
        *(uint32_t *)e = rnd_n(4) ? 0 : rnd() & ~1u;   /* inactive (or a stray even word is active) */
        if (*(uint32_t *)e == 0)
            return;
    } else {
        *(uint32_t *)e = 1;
    }
    used = rnd_n(4) == 0 ? n : rnd_n(4) == 0 ? 0 : rnd_n(n + 1);
    *(uint32_t *)(e + 0xc) = n;
    *(uint32_t *)(e + 0x10) = used;
    for (i = 4; i < 0xc; i += 2)
        *(uint16_t *)(e + i) = (uint16_t)(rnd_n(3) ? rnd_n(0x2000) - 0x1000 : rnd());
    for (i = 0; i < 15 * 40; i += 4)
        *(uint32_t *)(e + 0x14 + i) = rnd();
    for (i = 0; i < 9; i++) {
        *(uint32_t *)(tmpl + i * 4) = rnd();
        *(uint32_t *)(e - 0x28 + i * 4) = rnd();
    }
    *(uint32_t *)(e - 0x2c) = (uint32_t)(uintptr_t)tmpl;
    for (i = 0; i < n; i++) {
        uint8_t *b = arena_get(0xaa);
        uint32_t j;
        for (j = 0; j < 0xaa; j++)
            b[j] = (uint8_t)rnd();
        *(uint32_t *)(e + 0x26c + i * 4) = (uint32_t)(uintptr_t)b;
    }
}

static void op_objstep(void)
{
    static const uint32_t edge[] = { 0x15540000u, 0x6aa40000u, 0x954c0000u, 0xea9c0000u };
    uint32_t idx, r, before[20];

    if (op_number == 0 || rnd_n(5) == 0)
        for (idx = 0; idx < 20; idx++)
            objstep_fill(idx);
    *(uint32_t *)(global + 0x5d0) = rnd();
    *(uint32_t *)(global + 0x5c8) = rnd();
    global[0x250] = (uint8_t)rnd();
    for (idx = 0; idx < 20; idx++) {
        uint8_t *e = cur->image + 0x9794 + idx * 0x2d4;
        uint32_t d;

        before[idx] = *(uint32_t *)(e + 0x10);
        if (*(uint32_t *)e == 0)
            continue;
        /* the newest record's +0x24 is the template copy's ninth dword: aim
         * the longitude difference onto a wrap edge, or the side word (its
         * seventh, +0x1c) onto the value that wraps to 0 */
        switch (rnd_n(4)) {
        case 0:
            d = edge[rnd_n(4)] + rnd_n(3) - 1;
            *(uint32_t *)(e - 8) = *(uint32_t *)(global + 0x5d0) - d;
            break;
        case 1:
            *(uint32_t *)(e - 0x10) = rnd_n(2) ? 0xc0040000u : 0x3ffc0000u;
            break;
        default:
            break;
        }
    }
    if (counting) cover_objstep++;
    put("\n#objstep 5d0=%#x 5c8=%#x 250=%#x", *(uint32_t *)(global + 0x5d0),
        *(uint32_t *)(global + 0x5c8), global[0x250]);
    if (counting)
        for (idx = 0; idx < 20; idx++) {
            uint8_t *e = cur->image + 0x9794 + idx * 0x2d4;
            uint32_t d, side, used;
            if (*(uint32_t *)e == 0)
                continue;
            cover_objstep_active++;
            used = before[idx] + (before[idx] != *(uint32_t *)(e + 0xc));
            cover_objstep_submits += used;
            d = *(uint32_t *)(global + 0x5d0) - *(uint32_t *)(e - 8);
            if (d > 0x15540000u && d < 0x6aa40000u) { cover_objstep_side[1]++; side = *(uint32_t *)(e - 0x10) + 0x3ffc0000u; }
            else if (d > 0x954c0000u && d < 0xea9c0000u) { cover_objstep_side[2]++; side = *(uint32_t *)(e - 0x10) - 0x3ffc0000u; }
            else { cover_objstep_side[0]++; side = 1; }
            if (side == 0) cover_objstep_side0++;
            for (r = 0; r < 4; r++)
                if (d - edge[r] + 1 <= 2) cover_objstep_edge++;
            if (used > 1 && (global[0x250] & 1)) cover_objstep_toggle++;
        }
    r = ((s0)SVC(0x88))();
    put(" =%#x", r);
}

static void setup_objstep(void) { }

static void report_objstep(void)
{
    printf("  objstep %lu (active entries %lu, submissions %lu, side 0/+/- %lu/%lu/%lu,"
           " on a wrap edge %lu, side wrapped to 0 %lu, toggled %lu)\n",
           cover_objstep, cover_objstep_active, cover_objstep_submits, cover_objstep_side[0],
           cover_objstep_side[1], cover_objstep_side[2], cover_objstep_edge, cover_objstep_side0,
           cover_objstep_toggle);
}

/* ---- suite: quicksave (save under the default title, +0xb0) -------------------- */

static unsigned long cover_quick, cover_quick_default, cover_quick_ok, cover_quick_found;

/* A situations scenario in which a third of the ops are +0xb0: it saves
 * under string 0x2201 (absent, so the "Temporary Flight" the buffer starts
 * as stands; short; or long enough to fill the buffer) with the description
 * at .data+0x7fd0, and what it saved joins the files the loads pick from. */
static void op_quicksave(void)
{
    uint32_t r;
    struct vfile *f;
    char path[64];

    if (rnd_n(3) != 0) {
        op_situations();
        return;
    }
    random_string((char *)cur->image + 0x7fd0, rnd_n(4) ? 0x80 : 0x180);
    if (rnd_n(3) == 0)
        global[0x500 + rnd_n(0xd00)] = (uint8_t)rnd();
    if (counting) {
        cover_quick++;
        if (string_2201 == NULL) cover_quick_default++;
    }
    put("\n#quicksave('%.40s')", (char *)cur->image + 0x7fd0);
    r = ((s0)SVC(0xb0))();
    put(" =%#x", r);
    if (r == 0 && counting) cover_quick_ok++;
    if (r != 0)
        return;
    /* the save joins FLIGHTS and the title as it copied it, at most 0x1e
     * bytes, to .data+0x441f */
    snprintf(path, sizeof path, "FLIGHTS\\%.30s", (char *)cur->image + 0x441f);
    if ((f = vfile_find(path)) != NULL) {
        if (counting) cover_quick_found++;
        put(" file %#x", f->size);
        put_bytes(f->bytes, f->size);
        if (nsaved < 8)
            snprintf(saved[nsaved++], sizeof saved[0], "%s", path);
    }
}

static void setup_quicksave(void)
{
    static char name[0x140];
    uint32_t k = rnd_n(4);

    setup_situations();
    if (k == 0) {
        string_2201 = NULL;
    } else if (k == 1) {
        string_2201 = "Temporary Flight";
    } else {
        random_string(name, k == 2 ? 0x18 : 0x130);
        string_2201 = name;
    }
}

static void report_quicksave(void)
{
    report_situations();
    printf("  quicksave %lu (title string absent %lu, saved %lu, file found %lu)\n",
           cover_quick, cover_quick_default, cover_quick_ok, cover_quick_found);
}

/* ---- suite: prefs (the preferences: +0xb4 load, +0xb8 save, +0xbc defaults) ------ */

static unsigned long cover_prefs_ops[4], cover_prefs_preset, cover_prefs_wild, cover_prefs_neg,
    cover_prefs_high_mode, cover_prefs_junk;
static int32_t safe_modes[0x300];
static uint32_t nsafe_modes;

/* The performance modes whose preset block, 0x40 bytes at .data+0x5780 +
 * (mode << 7), lies in the image and holds no relocated pointer (one would
 * reach GLOBAL as each side's own address).  The real presets are 1..11;
 * the others read on through .data, as retail would. */
static void prefs_modes(void)
{
    int32_t m;
    uint32_t i;

    for (m = -0xa0; m < 0x200 && nsafe_modes < 0x300; m++) {
        int32_t at = 0x5780 + m * 0x80;
        int ok = at >= 0 && (uint32_t)at + 0x40 <= IMAGE_SIZE;
        for (i = 0; ok && i < nrelocs; i++)
            if (relocs[i] + 4 > (uint32_t)at && relocs[i] < (uint32_t)at + 0x40)
                ok = 0;
        if (ok)
            safe_modes[nsafe_modes++] = m;
    }
}

static int32_t prefs_mode(void)
{
    int32_t m;

    if (rnd_n(3))
        m = (int32_t)rnd_n(12);
    else
        m = safe_modes[rnd_n(nsafe_modes)];
    return m;
}

static void prefs_string(char *d, uint32_t max)
{
    random_string(d, max);
    if (rnd_n(4) == 0)
        d[rnd_n(max)] = ' ';
}

static void op_prefs(void)
{
    uint32_t k = rnd_n(10), r;

    if (k < 3) {                                   /* load */
        static char t[16];
        int32_t m = prefs_mode();
        struct cfg_entry *e = cfg_find("PREFS", "PERFORMANCE_MODE");
        *(int32_t *)(global + 0x123c) = prefs_mode();
        if (e != NULL || rnd_n(2)) {
            /* the mode as the file gives it, now and then with a trailing tail */
            snprintf(t, sizeof t, rnd_n(4) ? "%d" : rnd_n(2) ? " %dx" : "+%d;", m);
            if (counting && t[0] != '-' && (t[0] < '0' || t[0] > '9')) cover_prefs_junk++;
            cfg_set("PREFS", "PERFORMANCE_MODE", t);
        }
        if (counting) cover_prefs_ops[0]++;
        put("\n#load");
        r = ((s0)SVC(0xb4))();
        m = *(int32_t *)(global + 0x123c);         /* the mode the preset was taken for */
        if (counting) {
            if (m != 0) cover_prefs_preset++;
            if (m > 11) cover_prefs_wild++;
            if (m < 0) cover_prefs_neg++;
        }
    } else if (k < 5) {                            /* save */
        if (counting) cover_prefs_ops[1]++;
        put("\n#save");
        r = ((s0)SVC(0xb8))();
    } else if (k < 7) {                            /* defaults */
        int32_t m = rnd_n(4) ? (int32_t)rnd_n(10) : (int32_t)rnd();
        if (counting) { cover_prefs_ops[2]++; if (m > 4) cover_prefs_high_mode++; }
        put("\n#defaults(%d)", m);
        r = ((s1)SVC(0xbc))((uint32_t)m);
    } else {                                       /* the file or GLOBAL changes */
        uint32_t j;
        if (counting) cover_prefs_ops[3]++;
        if (ncfg != 0 && rnd_n(2)) {
            struct cfg_entry *e = &cfg[rnd_n(ncfg)];
            char t[0x120];
            switch (rnd_n(5)) {
            case 0: snprintf(t, sizeof t, "%d", (int32_t)rnd()); break;
            case 1: snprintf(t, sizeof t, "%d", (int32_t)rnd_n(0x20000) - 0x10000); break;
            case 2: t[0] = 0; break;
            case 3: snprintf(t, sizeof t, "%u%c", rnd_n(1000), (char)('a' + rnd_n(26))); break;
            default: prefs_string(t, 0x118); break;
            }
            snprintf(e->val, sizeof e->val, "%s", t);
            put("\n#cfg %s=%.300s", e->key, e->val);
        } else {
            for (j = 0; j < 8; j++)
                global[rnd_n(sizeof global)] = (uint8_t)rnd();
            prefs_string((char *)global + 0x24, rnd_n(4) ? 0x40 : 0x103);
            put("\n#global");
        }
        return;
    }
    put(" =%#x cfg %u %08x", r, ncfg, digest(cfg, sizeof cfg, 0x811c9dc5u));
}

static void setup_prefs(void)
{
    uint32_t i, n;

    ncfg = 0;
    memset(cfg, 0, sizeof cfg);
    if (nsafe_modes == 0)
        prefs_modes();
    for (i = 0; i < sizeof global; i++)
        global[i] = (uint8_t)rnd();
    /* the three string fields, terminated or now and then not within 0x104 */
    prefs_string((char *)global + 0x24, 0x40);
    prefs_string((char *)global + 0x12c, rnd_n(4) ? 0x40 : 0x103);
    prefs_string((char *)global + 0x638, 0x40);
    if (rnd_n(8) == 0) {
        memset(global + 0x12c, 'z', 0x110);       /* longer than the 0x104 read */
        global[0x12c + 0x110] = 0;
    }
    /* a file that already has some of the keys, or none */
    n = rnd_n(3) ? 0 : rnd_n(6);
    for (i = 0; i < n; i++) {
        static const char *const key[] = { "ADF_500_HZ", "PERFORMANCE_MODE", "DEF_PERF_MODE", "WEIRD" };
        char t[16];
        snprintf(t, sizeof t, "%d", (int32_t)rnd_n(12));
        cfg_set("PREFS", key[rnd_n(4)], t);
    }
}

static void report_prefs(void)
{
    printf("  prefs: loads %lu, saves %lu, defaults %lu (mode above 4 %lu), changes %lu; presets"
           " applied %lu (past the table %lu, negative %lu); mode text with a tail %lu\n",
           cover_prefs_ops[0], cover_prefs_ops[1], cover_prefs_ops[2], cover_prefs_high_mode,
           cover_prefs_ops[3], cover_prefs_preset, cover_prefs_wild, cover_prefs_neg, cover_prefs_junk);
}

/* ---- suite: clouds (the cloud layers' draw, +0x98) ------------------------------- */

static unsigned long cover_clouds_which[5], cover_clouds_skip, cover_clouds_inside, cover_clouds_shown,
    cover_clouds_flag8005, cover_clouds_puffs, cover_clouds_texel, cover_clouds_sat;

/* Each op seeds GLOBAL's layer fields (slice 15's centre/width pairs and flags,
 * the altitude now and then on or beside a band edge), the band edges slice 15
 * leaves in .data, the layer records at .data+0x1a80/+0x1a98 (the aircraft
 * usually near enough their origin that +0x9d57's divides fit, now and then
 * far), the texel map and HG2D's threshold, the view record behind GLOBAL
 * +0x1198, and calls +0x98 with a `which` from 0 to 5. */
static void op_clouds(void)
{
    uint8_t *view = fixtures + 0x10000;
    uint32_t which = rnd_n(3) ? 0 : 1 + rnd_n(5), i;
    int16_t lo1, lo2;

    for (i = 0; i < 0x40; i++)
        global[0xe60 + i] = (uint8_t)rnd();
    *(int16_t *)(global + 0xe6a) = (int16_t)(rnd_n(3) ? (int16_t)(rnd() & 0x3fff) - 0x2000 : (int16_t)rnd());
    *(uint16_t *)(global + 0xe6c) = (uint16_t)(rnd_n(3) ? rnd_n(0x1000) : rnd());
    *(int16_t *)(global + 0xe72) = (int16_t)(rnd_n(3) ? (int16_t)(rnd() & 0x3fff) : (int16_t)rnd());
    *(uint16_t *)(global + 0xe74) = (uint16_t)(rnd_n(3) ? rnd_n(0x1000) : rnd());
    *(uint16_t *)(global + 0xe6e) = (uint16_t)(rnd_n(2) ? 0xc000 + rnd_n(0x4000) : rnd());
    if (rnd_n(4) == 0)                             /* on +0x918b's 0xc000 or +0x741f's 0xe001 */
        *(uint16_t *)(global + 0xe6e) = (uint16_t)((rnd_n(2) ? 0xbfff : 0xe000) + rnd_n(3));
    *(uint16_t *)(global + 0xe76) = (uint16_t)(rnd_n(2) ? 0xe000 + rnd_n(3) : rnd());
    *(uint16_t *)(global + 0xe70) = (uint16_t)(rnd_n(5) ? 1 + rnd_n(0xffff) : 0);
    *(uint16_t *)(global + 0xe78) = (uint16_t)(rnd_n(2) ? 0 : 1 + rnd_n(0xffff));
    *(uint16_t *)(global + 0xe82) = (uint16_t)(rnd_n(2) ? 0 : rnd());
    *(uint16_t *)(global + 0xe84) = (uint16_t)(rnd_n(3) ? rnd() | 1 : 0);
    *(uint16_t *)(global + 0xe86) = (uint16_t)rnd_n(5);
    *(uint16_t *)(global + 0xe7a) = (uint16_t)(rnd_n(3) ? rnd() : 0);
    *(uint16_t *)(global + 0xe7e) = (uint16_t)(rnd_n(3) ? rnd() : 0);
    *(uint16_t *)(global + 0xe7c) = (uint16_t)rnd();
    *(uint16_t *)(global + 0xdce) = (uint16_t)(rnd_n(4) ? 0 : rnd());
    *(uint32_t *)(global + 0x398) = rnd_n(4) ? rnd() : 0;
    *(uint16_t *)(global + 0xcb6) = (uint16_t)(rnd_n(4) ? rnd() | 1 : 0);
    *(uint16_t *)(global + 0xcba) = (uint16_t)(rnd_n(2) ? 0x4000 + rnd_n(3) - 1 : rnd());
    global[0x282] = (uint8_t)(rnd_n(2) ? 0 : rnd());
    *(uint16_t *)(global + 0x2a8) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x260) = rnd();
    *(uint32_t *)(global + 0xc8c) = rnd();
    *(uint32_t *)(global + 0x1198) = (uint32_t)(uintptr_t)view;
    *(uint32_t *)(view + 0x14) = rnd_n(6);
    for (i = 0x5b0; i < 0x5c0; i++)
        global[i] = (uint8_t)rnd();
    for (i = 0x560; i < 0x578; i++)
        global[i] = (uint8_t)rnd();

    /* the band edges as slice 15 leaves them, now and then not */
    lo1 = (int16_t)(*(uint16_t *)(global + 0xe6a) - (*(uint16_t *)(global + 0xe6c) >> 1));
    lo2 = (int16_t)(*(uint16_t *)(global + 0xe72) - (*(uint16_t *)(global + 0xe74) >> 1));
    *(int16_t *)(cur->image + 0x1a94) = rnd_n(4) ? lo1 : (int16_t)rnd();
    *(int16_t *)(cur->image + 0x1aac) = rnd_n(4) ? lo2 : (int16_t)rnd();
    *(uint16_t *)(cur->image + 0x1a92) = (uint16_t)rnd();
    *(uint16_t *)(cur->image + 0x1aaa) = (uint16_t)rnd();
    *(uint16_t *)(cur->image + 0x1eb4) = (uint16_t)(rnd_n(4) ? rnd() & 0x7fff : rnd());
    for (i = 0; i < 4; i++)
        cur->image[0x1ec0 + i * 4] = (uint8_t)rnd();
    *(uint32_t *)(cur->image + 0x2088) = *(uint32_t *)(global + 0x260) - rnd_n(20);
    *(uint16_t *)(cur->image + 0x208c) = (uint16_t)rnd();
    *(uint32_t *)(cur->image + 0x2043) = rnd();
    /* the layer records: the aircraft's +0x5b4 near their +4, and its +0x5ba..
     * near their +0xa.., or now and then anywhere */
    for (i = 0; i < 2; i++) {
        uint8_t *r = cur->image + 0x1a80 + i * 0x18;
        uint32_t j;
        for (j = 0; j < 0x14; j++)
            if (j < 0x14 - 4 * (i == 0) || 1)
                r[j] = (uint8_t)rnd();
        if (rnd_n(4)) {
            *(uint32_t *)(r + 4) = *(uint32_t *)(global + 0x5b4) - (rnd() & 0x3ffffff) + 0x2000000;
            *(uint16_t *)(r + 0xc) = (uint16_t)(*(uint16_t *)(global + 0x5bc) - rnd_n(0x40) + 0x20);
            *(uint16_t *)(r + 0xe) = *(uint16_t *)(global + 0x5be);
        }
    }
    /* .data+0x1a94 and +0x1aac sit inside those records (at +0x14); put the edges back */
    *(int16_t *)(cur->image + 0x1a94) = rnd_n(4) ? lo1 : *(int16_t *)(cur->image + 0x1a94);
    *(int16_t *)(cur->image + 0x1aac) = rnd_n(4) ? lo2 : *(int16_t *)(cur->image + 0x1aac);
    /* the altitude: on, or one either side of, a layer's edge or its band's */
    if (rnd_n(2)) {
        /* the edges and one either side, +0x741f's 1500 either way, and its
         * near/close thresholds below a layer (0x34, 0x1a, 4, 2 and their
         * neighbours) and above one (6, 3 and theirs) */
        static const int16_t off[] = { -1, 0, 1, -1500, 1500, -1499, 1499, -1501, 1501,
                                       -0x34, -0x35, -0x33, -0x1a, -0x1b, -0x19, -4, -5, -3, -2,
                                       0x34, 6, 7, 5, 3, 4, 2 };
        int16_t e[4];
        e[0] = *(int16_t *)(cur->image + 0x1a94);
        e[1] = (int16_t)(e[0] + *(uint16_t *)(global + 0xe6c));
        e[2] = *(int16_t *)(cur->image + 0x1aac);
        e[3] = (int16_t)(e[2] + *(uint16_t *)(global + 0xe74));
        *(int16_t *)(global + 0x5c4) = (int16_t)(e[rnd_n(4)] + off[rnd_n(sizeof off / sizeof off[0])]);
    } else {
        *(int16_t *)(global + 0x5c4) = (int16_t)rnd();
    }
    /* +0x918b compares GLOBAL +0x574 with the same edges: on, or one either side */
    if (rnd_n(3) == 0) {
        int16_t e[4];
        e[0] = *(int16_t *)(cur->image + 0x1a94);
        e[1] = (int16_t)(e[0] + *(uint16_t *)(global + 0xe6c));
        e[2] = *(int16_t *)(cur->image + 0x1aac);
        e[3] = (int16_t)(e[2] + *(uint16_t *)(global + 0xe74));
        *(int16_t *)(global + 0x574) = (int16_t)(e[rnd_n(4)] + (int16_t)rnd_n(3) - 1);
    }
    hg2d_threshold = (uint8_t)(rnd_n(3) ? rnd_n(8) : rnd());
    if (op_number == 0)
        for (i = 0; i < 0x10000; i++)
            fixtures[i] = (uint8_t)(rnd_n(3) ? rnd_n(12) : rnd());

    if (counting) {
        cover_clouds_which[which < 4 ? which : 4]++;
        if (which == 0 && *(uint16_t *)(global + 0xdce) != 0) cover_clouds_skip++;
        if (which == 0 && *(uint16_t *)(global + 0xe7a) && *(uint16_t *)(global + 0xe7e) &&
            *(uint16_t *)(global + 0xe7c) >= 0xf000 / 16) cover_clouds_puffs++;
    }
    put("\n#clouds(%u) 5c4=%d e86=%u dce=%#x", which, *(int16_t *)(global + 0x5c4),
        *(uint16_t *)(global + 0xe86), *(uint16_t *)(global + 0xdce));
    ((s1)SVC(0x98))(which);
    if (counting && which == 0) {
        uint16_t f = *(uint16_t *)(cur->image + 0x1afa);
        if (f == 0x8007) cover_clouds_inside++;
        if (*(uint16_t *)(global + 0xf10) == 0x8005) cover_clouds_flag8005++;
        if (*(uint16_t *)(cur->image + 0x22cc) != 0 && f == 4) cover_clouds_shown++;
        if (*(uint32_t *)(cur->image + 0x3498) != 0x258 && *(uint32_t *)(cur->image + 0x3498) != 0x320)
            cover_clouds_texel++;
        if (*(uint16_t *)(cur->image + 0x3438) == 0x7fff || *(uint16_t *)(cur->image + 0x3438) == 0x8000)
            cover_clouds_sat++;
    }
    put(" f10=%#x c90=%#x", *(uint16_t *)(global + 0xf10), *(uint16_t *)(global + 0xc90));
}

static void setup_clouds(void) { }

static void report_clouds(void)
{
    printf("  clouds: which 0/1/2/3/above %lu/%lu/%lu/%lu/%lu (0 skipped by +0xdce %lu, all 16 puffs %lu);"
           " inside a layer %lu, layer block shown %lu, +0xf10 0x8005 %lu, texel looked up %lu,"
           " a divide saturated %lu\n",
           cover_clouds_which[0], cover_clouds_which[1], cover_clouds_which[2], cover_clouds_which[3],
           cover_clouds_which[4], cover_clouds_skip, cover_clouds_puffs, cover_clouds_inside,
           cover_clouds_shown, cover_clouds_flag8005, cover_clouds_texel, cover_clouds_sat);
}

/* ---- suite: cloudtex (the cloud layers' textures, +0x94) ------------------------- */

static unsigned long cover_tex_calls, cover_tex_no_clouds, cover_tex_ro, cover_tex_wild_edi,
    cover_tex_curve[3];

/* TEXTURE\clouds is there (whole, short or empty) or not; TEXTURE\cloud1 and
 * cloud2 are there or not, and read-only now and then (the routine clears the
 * flag first).  Each op seeds the layers' coverages (high bytes on both sides of
 * the curve's turns, 0x7f/0x80 of 0xed's product and 0xe0/0xe1), the flags, the
 * texture ids last made, and calls +0x94.  The texel map is HG2D's, in fixtures;
 * its digest goes in the transcript after every op, since a failed write leaves
 * the routine reading GLOBAL's fields from it and writing +0xcd4 into it. */
static void op_cloudtex(void)
{
    static const uint8_t turn[] = { 0x00, 0x01, 0x89, 0x8a, 0x8b, 0xe0, 0xe1, 0xff, 0x46, 0x10 };
    uint32_t i;

    for (i = 0; i < 2; i++) {
        uint16_t v = (uint16_t)(rnd_n(2) ? ((uint32_t)turn[rnd_n(sizeof turn)] << 8 | (uint8_t)rnd()) : rnd());
        if (rnd_n(6) == 0)
            v = (uint16_t)(0xe000 + rnd_n(3));
        *(uint16_t *)(global + (i ? 0xe76 : 0xe6e)) = v;
        *(uint16_t *)(cur->image + (i ? 0x1edc : 0x1eda)) =
            (uint16_t)(rnd_n(4) == 0 ? (v & 0xff00) : rnd());
    }
    *(uint32_t *)(global + 0x398) = rnd_n(5) ? rnd() : 0;
    *(uint16_t *)(global + 0xe70) = (uint16_t)(rnd_n(5) ? rnd() | 1 : 0);
    *(uint16_t *)(global + 0xe78) = (uint16_t)(rnd_n(3) ? rnd() | 1 : 0);
    /* the flag now and then with only its high byte set, and its neighbour
     * bytes random, so a read of the wrong word shows */
    *(uint16_t *)(global + 0xcb6) = (uint16_t)(rnd_n(6) ? (rnd_n(4) ? rnd() | 1 : 0x100u << rnd_n(8)) : 0);
    global[0xcb5] = (uint8_t)rnd();
    global[0xcb8] = (uint8_t)rnd();
    *(uint16_t *)(global + 0xe6a) = (uint16_t)(rnd_n(2) ? 0x2bc + rnd_n(3) - 1 : rnd());
    *(uint16_t *)(global + 0xe8a) = (uint16_t)(rnd_n(2) ? 0x7d00 + rnd_n(3) - 1 : rnd());
    *(uint32_t *)(global + 0xc8c) = rnd();
    if (rnd_n(4) == 0) {                           /* the files change between ops */
        struct vfile *f;
        if (rnd_n(2) && (f = vfile_find(rnd_n(2) ? "TEXTURE\\cloud1" : "TEXTURE\\cloud2")) != NULL)
            f->attr = rnd_n(2) ? 2 : rnd_n(8);
        else if ((f = vfile_find("TEXTURE\\clouds")) != NULL)
            f->size = rnd_n(3) ? 0x10000 : rnd_n(0x10001);
    }
    /* the map, filled afresh now and then (the routine remaps it in place) */
    if (op_number == 0 || rnd_n(3) == 0)
        for (i = 0; i < 0x10000; i++)
            fixtures[i] = (uint8_t)rnd();
    if (counting) {
        uint8_t b = global[0xe6f];
        cover_tex_calls++;
        cover_tex_curve[b > 0xe0 ? 2 : ((0xedu * b) >> 8) >= 0x80 ? 1 : 0]++;
    }
    put("\n#cloudtex e6e=%#x e76=%#x 398=%#x e70=%#x e78=%#x cb6=%#x", *(uint16_t *)(global + 0xe6e),
        *(uint16_t *)(global + 0xe76), *(uint32_t *)(global + 0x398), *(uint16_t *)(global + 0xe70),
        *(uint16_t *)(global + 0xe78), *(uint16_t *)(global + 0xcb6));
    i = open_rw_refused;
    ((s0)SVC(0x94))();
    if (counting && open_rw_refused != i)
        cover_tex_wild_edi++;                      /* a write's open refused: edi left on the map */
    put(" cd4=%#x map=%08x", *(uint16_t *)(global + 0xcd4), digest(fixtures, 0x10000, 0x811c9dc5u));
}

static void setup_cloudtex(void)
{
    struct vfile *f;
    uint32_t i;

    file_cap = FILE_BYTES;
    short_write_odds = 8;
    if (rnd_n(5)) {
        f = make_file("TEXTURE\\clouds");
        f->size = rnd_n(4) ? 0x10000 : rnd_n(0x10001);
        for (i = 0; i < f->size; i++)
            f->bytes[i] = (uint8_t)rnd();
    } else if (counting) {
        cover_tex_no_clouds++;
    }
    for (i = 1; i <= 2; i++) {
        char name[32];
        if (rnd_n(4) == 0)
            continue;
        snprintf(name, sizeof name, "TEXTURE\\cloud%u", i);
        f = make_file(name);
        f->size = rnd_n(0x100);
        f->attr = rnd_n(3) ? 0 : rnd_n(2) ? 2 : rnd_n(0x20);
        if (counting && (f->attr & 2)) cover_tex_ro++;
    }
}

static void report_cloudtex(void)
{
    printf("  cloudtex %lu (curve shoulder/knee/steepest %lu/%lu/%lu); scenarios with no clouds file %lu,"
           " a cloudN read-only %lu; ops with a write's open refused (edi left on the map) %lu; short writes %lu\n",
           cover_tex_calls, cover_tex_curve[0], cover_tex_curve[1], cover_tex_curve[2], cover_tex_no_clouds,
           cover_tex_ro, cover_tex_wild_edi, cover_short_writes);
}

/* ---- suite: spot (the spot plane's eye, +0xa8) ---------------------------------- */

static unsigned long cover_spot_calls, cover_spot_skip, cover_spot_place, cover_spot_mode[3],
    cover_spot_walk, cover_spot_slew, cover_spot_g3d;

/* Each op seeds the spot view's settings in GLOBAL (+0xe5c..+0xe68: mode 2 or
 * 4 or neither, the second aircraft or the first, the transition 0..2 -- see
 * main_spot.inc for why not beyond --, the reset and slew flags), the view
 * record behind GLOBAL +0x1198 (kind 0 or 1, now and then not a spot view), the
 * aircraft list behind GLOBAL +0x384 and its records (present or not, the
 * fields +0xa8 tests set or clear), and the eye's staged vectors in .data now
 * and then; G3D's distance turns at a random call above a random base. */
static void op_spot(void)
{
    uint8_t *view = fixtures + 0x20000, *list = fixtures + 0x20100;
    uint32_t i, k;

    g3d_calls = 0;
    g3d_turn = rnd_n(3) ? rnd_n(24) : rnd_n(200);
    g3d_base = rnd_n(3) ? rnd_n(2) ? 0x40000 - 0x100 + rnd_n(0x200) : rnd_n(0x10000) : rnd();
    /* now and then the op's first measure -- the slew's, when the eye is not
     * placed afresh first -- exactly on the slew's limit or one either side */
    g3d_first = rnd_n(4) == 0 ? 0x40000 + rnd_n(3) - 1 : 0;
    g3d_flat = rnd_n(3) == 0 ? 1 + rnd_n(3) : 0;
    *(uint32_t *)(global + 0x1198) = rnd_n(12) ? (uint32_t)(uintptr_t)view : 0;
    *(uint32_t *)(view + 0x10) = rnd_n(8) ? 0 : 1;
    *(uint32_t *)(view + 0x14) = rnd_n(8) ? rnd_n(2) : rnd_n(5);
    *(uint32_t *)(view + 0x18) = rnd_n(8) ? 0 : 1;
    *(uint16_t *)(global + 0xe64) = (uint16_t)(rnd_n(8) ? (rnd_n(2) ? 2 : 4) : rnd_n(6));
    *(uint16_t *)(global + 0xe66) = (uint16_t)(rnd_n(2) ? 0 : 1 + rnd_n(0xff));
    *(uint16_t *)(global + 0xe60) = (uint16_t)rnd_n(3);
    *(uint16_t *)(global + 0xe5e) = (uint16_t)(rnd_n(6) ? rnd_n(3) : rnd_n(8));
    *(uint16_t *)(global + 0xe5c) = (uint16_t)rnd();
    *(uint16_t *)(global + 0xe68) = (uint16_t)rnd();
    if (rnd_n(3) == 0)
        *(uint16_t *)(global + 0xe62) = (uint16_t)rnd_n(4);
    *(uint16_t *)(global + 0xc4e) = (uint16_t)rnd();
    *(uint16_t *)(global + 0xc5e) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x20) = rnd();
    *(uint32_t *)(global + 0x384) = (uint32_t)(uintptr_t)list;
    for (i = 0x560; i < 0x580; i++)
        if (rnd_n(4) == 0)
            global[i] = (uint8_t)rnd();
    /* the latitude inside slice 11's clamp, as the game keeps it: UTIL +0x44
     * indexes its latitude table by it and reads off the end otherwise */
    *(int32_t *)(global + 0x564) = (int32_t)(rnd() % (0x97c449u + 0x97c449u + 1)) - 0x97c449;
    for (k = 0; k < 2; k++) {
        uint8_t *rec = fixtures + 0x20200 + k * 0x100;
        *(uint32_t *)(list + k * 4) = rnd_n(10) ? (uint32_t)(uintptr_t)rec : 0;
        if (op_number == 0 || rnd_n(4) == 0)
            for (i = 0; i < 0x60; i++)
                rec[i] = (uint8_t)rnd();
        *(uint16_t *)(rec + 0x18) = (uint16_t)(rnd_n(8) ? 1 + rnd_n(0xfffe) : 0);
        rec[0x1c] = (uint8_t)(rnd_n(2) ? rec[0x1c] | 0x80 : rec[0x1c] & 0x7f);
    }
    if (op_number == 0 || rnd_n(5) == 0) {         /* the staged vectors and scales */
        for (i = 0x1860; i < 0x1960; i++) {
            uint8_t v = (uint8_t)rnd();
            uint32_t r;
            /* not over a relocated pointer (the draw blocks at +0x192e and
             * +0x1946 hold some): a random value there that happened to point
             * into retail's image would compare as a pointer on that side only */
            for (r = 0; r < nrelocs; r++)
                if (relocs[r] <= i && i < relocs[r] + 4)
                    break;
            if (r == nrelocs)
                cur->image[i] = v;
        }
        *(uint32_t *)(cur->image + 0x191c) = rnd_n(2);
        *(uint32_t *)(cur->image + 0x1920) = rnd_n(2);
    }
    if (counting) {
        cover_spot_calls++;
        if (*(uint16_t *)(global + 0xe64) == 2) cover_spot_mode[0]++;
        else if (*(uint16_t *)(global + 0xe64) == 4) cover_spot_mode[1]++;
        else cover_spot_mode[2]++;
        if (*(uint32_t *)(global + 0x1198) == 0 || *(uint32_t *)(view + 0x10) || *(uint32_t *)(view + 0x18) ||
            *(uint32_t *)(view + 0x14) > 1)
            cover_spot_skip++;
        if (*(uint16_t *)(global + 0xe62) & 1) cover_spot_place++;
        if (*(uint32_t *)(cur->image + 0x191c) && (*(uint16_t *)(global + 0xe62) & 2)) cover_spot_slew++;
    }
    put("\n#spot e64=%u e66=%#x e60=%u e5e=%u e62=%#x kind=%u turn=%u base=%#x", *(uint16_t *)(global + 0xe64),
        *(uint16_t *)(global + 0xe66), *(uint16_t *)(global + 0xe60), *(uint16_t *)(global + 0xe5e),
        *(uint16_t *)(global + 0xe62), *(uint32_t *)(view + 0x14), g3d_turn, g3d_base);
    ((s0)SVC(0xa8))();
    if (counting) {
        cover_spot_g3d += g3d_calls;
        if (*(uint32_t *)(cur->image + 0x1920) == 0 && g3d_calls > 2 + 2 * 15) cover_spot_walk++;
    }
    put(" e62=%#x calls=%u", *(uint16_t *)(global + 0xe62), g3d_calls);
}

static void setup_spot(void)
{
    static const uint32_t slots[] = { 0x2c, 0x44, 0x60, 0x74, 0x78, 0x7c, 0x80, 0x84 };
    static int loaded;

    if (!loaded) {
        use_retail_util(slots, sizeof slots / sizeof slots[0]);
        loaded = 1;
    }
    g3d_distance = 1;
}

static void report_spot(void)
{
    printf("  spot %lu (mode 2/4/other %lu/%lu/%lu, not a spot view %lu, placed afresh %lu, slewed %lu,"
           " long walks %lu); G3D measures %lu\n",
           cover_spot_calls, cover_spot_mode[0], cover_spot_mode[1], cover_spot_mode[2], cover_spot_skip,
           cover_spot_place, cover_spot_slew, cover_spot_walk, cover_spot_g3d);
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
    { "nav", op_nav, report_nav, 20, setup_nav, 0x00 },
    { "navio", op_navio, report_navio, 20, setup_navio, 0x00 },
    { "navdesc", op_navdesc, report_navdesc, 20, setup_navdesc, 0x00 },
    { "season", op_season, report_season, 20, setup_season, 0x00 },
    { "clock", op_clock, report_clock, 20, setup_clock, 0x00 },
    { "view", op_view, report_view, 20, setup_view, 0x00 },
    { "menu", op_menu, report_menu, 20, setup_menu, 0x00 },
    { "obj", op_obj, report_obj, 20, setup_obj, 0x00 },
    { "clamp", op_clamp, report_clamp, 20, setup_clamp, 0x00 },
    { "sun", op_sun, report_sun, 20, setup_sun, 0x00 },
    { "objalloc", op_objalloc, report_objalloc, 20, setup_objalloc, 0x00 },
    { "eye", op_eye, report_eye, 20, setup_eye, 0x00 },
    { "band", op_band, report_band, 20, setup_band, 0x00 },
    { "objstep", op_objstep, report_objstep, 20, setup_objstep, 0x00 },
    { "quicksave", op_quicksave, report_quicksave, 30, setup_quicksave, 0xff },
    { "prefs", op_prefs, report_prefs, 30, setup_prefs, 0x00 },
    { "clouds", op_clouds, report_clouds, 20, setup_clouds, 0x00 },
    { "cloudtex", op_cloudtex, report_cloudtex, 12, setup_cloudtex, 0x00 },
    { "spot", op_spot, report_spot, 20, setup_spot, 0x00 },
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
    free_fill = suite->fill;
    memset(arena, suite->fill, ARENA_SIZE);    /* all of it: a runaway walk reads far */
    memset(fixtures, suite->fill, sizeof fixtures);
    memset(files, 0, sizeof files);
    memset(global, 0, sizeof global);
    fix_used = 0;
    nwanted = 0;
    file_cap = FILE_MAX;
    rng = scenario_seed;
    pad_byte = (uint8_t)(rnd_n(12) == 0 ? 0 : rnd());
    suite->setup();
    put("scenario %u %s pad %#x\n", scenario, suite->name, pad_byte);
    faulted = 0;
    nops = 2 + rnd_n(suite->max_ops);
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        dispatches_this_op = 0;
        walked_off = 0;
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
        if (cursor_wild() || walked_off) {
            /* the walk went off into memory the two sides do not share,
             * and may have copied some of it in: what follows cannot agree.
             * cursor_wild() sees a wild cursor still parked at op end;
             * walked_off caught one at a dispatched record mid-parse, before
             * the parse read a terminator from wild memory and moved on. */
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
    situations_suite = !strcmp(suite->name, "situations") || !strcmp(suite->name, "quicksave");
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
