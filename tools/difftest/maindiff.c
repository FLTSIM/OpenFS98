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
    uint8_t *tail;                                 /* what follows the image, while writable */
    uint32_t tail_size;
    char *transcript;
    uint32_t length;
};

static struct side retail, ours;
static struct side *cur;
static uint32_t relocs[MAX_RELOCS], nrelocs;
static uint8_t host[0x340], global[0x1400], stub_module[0x400];
static uint8_t acontain[0x40], controls[0x60], sound[0x60], util[0xc0], weatherm[0x40], multip[0x40], fe[0x100];
static uint8_t hg2d[0x100];   /* the clouds reach +0xb8 */
static uint8_t g3d[0x100];   /* the frame reaches +0x60 */
static uint8_t chain[0x24];
static uint8_t module_stub[48][0x100];   /* the other modules, each its own table */
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint8_t pad_byte;
static uint8_t free_fill = 0xdd;   /* what a freed block and alloc slack hold; 0xff for
                                   * situations, so a parse that walks off a buffer meets
                                   * 0xffff and stops the same on both sides */
static uint32_t rng, scenario_seed, scenario, op_number;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs_retail, npairs_ours;

/* A slot past the end of its stub table would land in the next one (the clouds'
 * HG2D +0x88..+0xb8 once overwrote MULTIP's +0x34/+0x38), so refuse it. */
#define SET_SLOT(table, off, fn)                                                   \
    do {                                                                           \
        if ((off) + 4 > sizeof (table)) {                                          \
            printf("stub slot %#x past the end of %s\n", (unsigned)(off), #table); \
            exit(2);                                                               \
        }                                                                          \
        *(void **)((table) + (off)) = (void *)(fn);                                \
    } while (0)
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

static int stack_sources;                           /* the logbook suite */

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
static unsigned long cover_div_memory;           /* repairs of a memory-operand divide */

static int repair_divide(CONTEXT *c)
{
    const uint8_t *ip = (const uint8_t *)(uintptr_t)c->Eip;
    DWORD *reg[8];
    uint32_t len = 0, v = 0, neg;
    int opsz = 4;
    uint8_t op, modrm, rm, kind, mod;

    reg[0] = &c->Eax; reg[1] = &c->Ecx; reg[2] = &c->Edx; reg[3] = &c->Ebx;
    reg[4] = &c->Esp; reg[5] = &c->Ebp; reg[6] = &c->Esi; reg[7] = &c->Edi;
    if (ip[0] == 0x66) { opsz = 2; len = 1; }
    op = ip[len];
    if (op != 0xf6 && op != 0xf7)
        return 0;
    modrm = ip[len + 1];
    kind = (modrm >> 3) & 7;
    if (kind != 6 && kind != 7)
        return 0;                                  /* div (6) or idiv (7) */
    rm = modrm & 7;
    mod = modrm >> 6;
    if (mod == 3) {
        len += 2;
        v = *reg[rm];
    } else {
        /* a memory operand, decoded as the host's filter decodes it
         * (0x4119a0, docs/modules/host.md): its length, and for idiv the
         * divisor's dword for the sign.  The filter's own quirks are kept:
         * a disp8 is taken zero-extended, and a dword is read whatever the
         * operand's size. */
        const uint8_t *q = ip + len;               /* the opcode */
        uint8_t sib = q[2];
        uint32_t ea;

        if (counting) cover_div_memory++;

        if (rm == 4)
            len++;
        if (mod == 0)
            len += (rm == 5 || (rm == 4 && (sib & 7) == 5)) ? 6 : 2;
        else
            len += mod == 1 ? 3 : 6;
        if (kind == 7) {
            if (rm == 4) {
                uint32_t idx = (sib >> 3) & 7, b = sib & 7;
                ea = idx == 4 ? 0 : *reg[idx] << (sib >> 6);
                if (b != 5 || mod != 0)
                    ea += *reg[b];
                if (mod == 1)
                    ea += q[3];
                else if (mod == 2 || b == 5)
                    ea += *(const uint32_t *)(q + 3);
            } else if (mod == 0 && rm == 5) {
                ea = *(const uint32_t *)(q + 2);
            } else {
                ea = *reg[rm] + (mod == 1 ? q[2] : mod == 2 ? *(const uint32_t *)(q + 2) : 0);
            }
            if (IsBadReadPtr((const void *)(uintptr_t)ea, 4))
                return 0;
            v = *(const uint32_t *)(uintptr_t)ea;
        }
    }
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
    } else if (op == 0xf6) {                       /* idiv, 8-bit: AL quot, AH rem; high is AH */
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
     * the label.  Only the logbook's sort, swapping rows through a local, copies from the stack. */
    int wild = n > 0x100000 ||
               !known_memory((uint32_t)(uintptr_t)s, n, stack_sources) ||
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

static uint32_t fixture_arg;           /* what a dialog's plain callback is handed */
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

static uint32_t pf_exists_answer;
static int32_t STDCALL h_exists(const char *path, int32_t ext)
{
    int32_t r = ext == 0x1d ? (pf_exists_answer ? 0 : -1) : vfile_find(path) ? 0 : -1;     /* 0x1d: the aircraft */

    put(" exists(");
    put_name(path);
    put(",%d)=%d", ext, r);
    return r;
}

static int open_makes;
static struct vfile *make_file(const char *name);
static int32_t STDCALL h_open(struct hblock **out, const char *path, int32_t ext, int32_t mode)
{
    struct vfile *f = vfile_find(path);
    struct hblock *b;

    put(" open(");
    put_ptr((uint32_t)(uintptr_t)out);
    put(",");
    put_name(path);
    put(",%d,%d)", ext, mode);
    if (f == NULL && open_makes) {                 /* the logbook's reader would go on with counts its stack held */
        f = make_file(path);
        f->size = (uint32_t)sprintf((char *)f->bytes, "T\r\n\r\nHead\r\n\r\n\r\nTotals\r\n");
    }
    if (f == NULL || mode < 0 || mode > 3 || ((f->attr & 2) && mode != 0) || (!open_makes && rnd_n(40) == 0)) {
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
    if (stack_sources && b->pos == 0 && k > 0x20 && k < 0x80) {
        /* the logbook suite: a book that is only its heads, as "new" writes it, has no CR
         * after the column heads, and .text+0x11480 then leaves .text+0xfd70's row count
         * as the stack held it (retail; ours has 0): nothing to compare from here on */
        const uint8_t *cr = memchr(f->bytes, 0xd, k);
        if (cr != NULL && cr + 3 < f->bytes + k && memchr(cr + 3, 0xd, (size_t)(f->bytes + k - (cr + 3))) == NULL)
            stub_fault("heads only");
    }
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
    if (n == 2 && *(const uint16_t *)buf == 0xffff)
        put("endtag");                             /* retail's word is in its .text, ours in .rdata */
    else
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
    put(" menu(%#x,%#x)", (uint16_t)id, state);    /* the host looks it up as 16 bits */
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

static int session_empty_strings;

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
    } else if (session_empty_strings && len > 0) {
        buf[0] = 0;                                /* a string the tables lack, as LoadStringA leaves it */
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

static uint32_t load_mark;                          /* where the op's latest load began in the transcript */

static uint32_t STDCALL chain_send(uint32_t list, uint32_t a, uint32_t b)
{
    uint32_t len_before = cur ? cur->length : 0;
    if (list == 0x14 && a == 0x20)
        load_mark = len_before;
    put(" chainsend(%#x,%#x,", list, a);
    put_ptr(b);
    put(")");
    if (list == 0xb && b != 0) {                   /* the other modules' menus: { command, handled, argument } */
        uint32_t *block = (uint32_t *)(uintptr_t)b;
        put(" {%#x,%u,", block[0], block[1]);
        put_ptr(block[2]);
        put("}");
        block[1] = rnd_n(2);
    }
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
    if ((uint32_t)(uintptr_t)text < 0x10000)       /* a string's id, which UTIL loads */
        put(" alert(%#x,%#x,#%#x,", flags, tag, (uint32_t)(uintptr_t)text);
    else
        put(" alert(%#x,%#x,'%.60s',", flags, tag, text);
    put_ptr(arg);
    put(",%#x,%#x,%#x,%#x)", p3, p4, p5, lines);
    if (counting) cover_alerts++;
    if (flags == 0xc0)                             /* a question: Yes is 0x40 */
        return rnd_n(2) ? 0x40 : 0x80;
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

/* ---- for init/deinit and the frame callbacks (slice 22) ------------------------ */

/* CHAIN +0x18 and +0x1c: log the list and the callback (paired by first
 * appearance, so retail's code address and ours compare), and keep lists 1
 * and 3's callbacks so the frame suite can dispatch them. */
static uint32_t frame_cb[2][2];                    /* [side][list 1, list 3] */
static uint32_t record_cb[2];                      /* [side] list 0x14's */

/* Call a callback (a, b) with eax, ecx and edx set to known values, and come
 * back with what it left in them and how far it moved esp: retail's list
 * 0x14 listener is `ret 8` alone, so all four are part of its behaviour. */
uint32_t cb_eax, cb_ecx, cb_edx, cb_esp;
uint32_t call_with_regs(uint32_t cb, uint32_t a, uint32_t b);
__asm__(".text\n\t.globl _call_with_regs\n_call_with_regs:\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"
        "movl 20(%esp), %ebx\n\t"                /* cb */
        "movl %esp, %esi\n\t"
        "pushl 28(%esp)\n\tpushl 28(%esp)\n\t"  /* b, a */
        "movl $0x11111111, %eax\n\tmovl $0x22222222, %ecx\n\tmovl $0x33333333, %edx\n\t"
        "call *%ebx\n\t"
        "movl %eax, _cb_eax\n\tmovl %ecx, _cb_ecx\n\tmovl %edx, _cb_edx\n\t"
        "movl %esi, %eax\n\tsubl %esp, %eax\n\tmovl %eax, _cb_esp\n\t"
        "movl %esi, %esp\n\t"
        "popl %ebp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret\n");
static uint32_t STDCALL chain_add(uint32_t list, uint32_t cb)
{
    put(" chain+0x18(%#x,cb%u)", list, pair_code(cb));
    if (list == 1 || list == 3)
        frame_cb[cur == &ours][list == 3] = cb;
    if (list == 0x14)
        record_cb[cur == &ours] = cb;
    return 0;
}
static uint32_t STDCALL chain_remove(uint32_t list, uint32_t cb)
{
    put(" chain+0x1c(%#x,cb%u)", list, pair_code(cb));
    if ((list == 1 || list == 3) && frame_cb[cur == &ours][list == 3] == cb)
        frame_cb[cur == &ours][list == 3] = 0;
    if (list == 0x14 && record_cb[cur == &ours] == cb)
        record_cb[cur == &ours] = 0;
    return 0;
}

/* host +0x18 (the Linkage's frame services to the host), +0x1c (a constant 1),
 * +0x298 (onto the module stack, an HMODULE back) and +0x29c (off it).  The
 * Linkage is logged as where it is, not what its slots hold: MAIN's +0x18,
 * +0x20 and +0x24..+0x2c are not all done yet. */
static uint32_t STDCALL h_frame_services(uint32_t linkage)
{
    put(" host+0x18(");
    put_ptr(linkage);
    put(")");
    return 1;
}
static uint32_t STDCALL h_one(void)
{
    put(" host+0x1c");
    return 1;
}
static uint32_t STDCALL h_push_module(uint32_t linkage, uint32_t *out)
{
    put(" host+0x298(");
    put_ptr(linkage);
    put(")");
    *out = 0x5a5a0000u;
    return 0;
}
static uint32_t STDCALL h_pop_module(uint32_t v)
{
    put(" host+0x29c(%#x)", v);
    return 0;
}

/* FE's grid conversions, +0x60..+0x6c, as models (FE's are verified by
 * fegriddiff): a latitude or longitude (the dword at +4 of a position) to its
 * band at a level, a band back to its edge.  The band is the dword / 0x2000. */
static uint32_t STDCALL fe_60(uint32_t pos, uint32_t level)
{
    uint32_t b = (uint32_t)((int32_t)*(uint32_t *)(uintptr_t)pos / 0x2000) & 0xffff;
    put(" fe+0x60(%d,%u)=%#x", *(int32_t *)(uintptr_t)pos, level, b);
    return b;
}
static uint32_t STDCALL fe_64(uint32_t band, uint32_t level, uint32_t out)
{
    put(" fe+0x64(%#x,%u)", band & 0xffff, level);
    *(uint32_t *)(uintptr_t)out = 0;
    *(int32_t *)(uintptr_t)(out + 4) = (int32_t)(int16_t)band * 0x2000;
    return 0;
}
static uint32_t STDCALL fe_68(uint32_t pos, uint32_t level)
{
    uint32_t b = (uint32_t)((int32_t)*(uint32_t *)(uintptr_t)pos / 0x2000) & 0xffff;
    put(" fe+0x68(%d,%u)=%#x", *(int32_t *)(uintptr_t)pos, level, b);
    return b;
}
static uint32_t STDCALL fe_6c(uint32_t band, uint32_t level, uint32_t out)
{
    put(" fe+0x6c(%#x,%u)", band & 0xffff, level);
    *(uint32_t *)(uintptr_t)out = 0;
    *(int32_t *)(uintptr_t)(out + 4) = (int32_t)(int16_t)band * 0x2000;
    return 0;
}

/* HG2D +0x7c, select a colour table (its result is not read), and UTIL +0x6c,
 * the random generator (UTIL's own keeps its seed in UTIL's .data, which a
 * shared retail UTIL would carry from one side to the other): the harness's
 * generator, which each side starts afresh. */
static uint32_t STDCALL hg2d_7c(uint32_t t)
{
    put(" hg2d+0x7c(%#x)", t & 0xffff);
    return 0;
}
static uint32_t STDCALL util_rand(void)
{
    uint32_t r = rnd() & 0x7fff;
    put(" rand=%#x", r);
    return r;
}

/* ---- for the frame (+0x18) ------------------------------------------------------ */

/* The modules' per-frame steps the frame calls: fire and forget, so each
 * logs that it was called, and a digest of GLOBAL as the module would find
 * it (a field the frame sets and clears again within the frame -- the lamp
 * at +0x282 -- is seen only by the steps called in between).  The host's
 * clock (+0x34) answers the frame time the op chose. */
#define STEP_STUB(name, text) \
    static uint32_t STDCALL name(void) { put(" " text " G=%08x", digest(global, sizeof global, 0x811c9dc5u)); return 0; }
STEP_STUB(step_m6_18, "m6+0x18")
STEP_STUB(step_m7_18, "m7+0x18")
STEP_STUB(step_m9_18, "m9+0x18")
STEP_STUB(step_m10_1c, "m10+0x1c")
STEP_STUB(step_m26_18, "m26+0x18")
STEP_STUB(step_sound_20, "sound+0x20")
STEP_STUB(step_weatherm_1c, "weatherm+0x1c")
STEP_STUB(step_g3d_60, "g3d+0x60")
STEP_STUB(step_hg2d_18, "hg2d+0x18")
STEP_STUB(step_util_70, "util+0x70")
#undef STEP_STUB
static uint32_t frame_ms;
static uint32_t STDCALL h_clock(void)
{
    put(" host+0x34=%u", frame_ms);
    return frame_ms;
}

/* ---- for the command dispatcher (+0x20) ------------------------------------------ */

/* The services the menus' commands call: logged, with their arguments. */
#define CMD_STUB0(name, text) \
    static uint32_t STDCALL name(void) { put(" " text); return 0; }
#define CMD_STUB1(name, text) \
    static uint32_t STDCALL name(uint32_t a) { put(" " text "("); put_ptr(a); put(")"); return 0; }
#define CMD_STUB2(name, text) \
    static uint32_t STDCALL name(uint32_t a, uint32_t b) { put(" " text "("); put_ptr(a); put(",%#x)", b); return 0; }
CMD_STUB0(cmd_advdrv_28, "advdrv+0x28")
CMD_STUB0(cmd_advdrv_2c, "advdrv+0x2c")
CMD_STUB0(cmd_multip_18, "multip+0x18")
CMD_STUB0(cmd_multip_1c, "multip+0x1c")
CMD_STUB1(cmd_facil_24, "facil+0x24")
CMD_STUB0(cmd_facil_28, "facil+0x28")
CMD_STUB0(cmd_facil_30, "facil+0x30")
CMD_STUB0(cmd_fe_78, "fe+0x78")
CMD_STUB0(cmd_demo_2c, "demo+0x2c")
CMD_STUB0(cmd_demo_30, "demo+0x30")
CMD_STUB0(cmd_demo_34, "demo+0x34")
CMD_STUB0(cmd_analysis_34, "analysis+0x34")
CMD_STUB0(cmd_controls_58, "controls+0x58")
CMD_STUB1(cmd_panels_20, "panels+0x20")
CMD_STUB1(cmd_panels_24, "panels+0x24")
CMD_STUB1(cmd_window_40, "window+0x40")
CMD_STUB1(cmd_window_48, "window+0x48")
CMD_STUB1(cmd_window_50, "window+0x50")
CMD_STUB1(cmd_window_54, "window+0x54")
CMD_STUB2(cmd_window_58, "window+0x58")
CMD_STUB2(cmd_window_5c, "window+0x5c")
CMD_STUB1(cmd_window_68, "window+0x68")
CMD_STUB1(cmd_window_6c, "window+0x6c")
CMD_STUB1(cmd_host_48, "host+0x48")
CMD_STUB1(cmd_host_7c, "host+0x7c")
#undef CMD_STUB0
#undef CMD_STUB1
#undef CMD_STUB2
/* WINDOW +0x8c: find a window or panel by its command; none now and then */
static uint32_t cmd_record_now;
static uint32_t STDCALL cmd_window_8c(uint32_t command, uint32_t kind)
{
    put(" window+0x8c(%#x,%u)=", command, kind);
    put_ptr(cmd_record_now);
    return cmd_record_now;
}

/* ---- the host's and UTIL's file services the Flights dialogs add ------------------ */

static int32_t STDCALL h_split(const char *path, char *drive, char *dir, char *name, uint32_t *ext)
{
    const char *p = path, *slash;

    put(" split(");
    put_name(path);
    put(",%d%d%d%d)", drive != NULL, dir != NULL, name != NULL, ext != NULL);
    if (rnd_n(60) == 0) {
        put("=-1");
        return -1;
    }
    if (ext != NULL) *ext = 0;
    if (drive != NULL) drive[0] = 0;
    if (p[0] && p[1] == ':') {
        if (drive != NULL) { drive[0] = p[0]; drive[1] = ':'; drive[2] = 0; }
        p += 2;
    }
    slash = strrchr(p, '\\');
    if (dir != NULL) {
        size_t n = slash ? (size_t)(slash - p) + 1 : 0;
        memcpy(dir, p, n);
        dir[n] = 0;
    }
    if (name != NULL)
        strcpy(name, slash ? slash + 1 : p);
    return 0;
}

static int32_t STDCALL h_tempname(char *out, const char *dir)
{
    put(" tempname(");
    put_name(dir);
    put(")");
    if (rnd_n(60) == 0) {
        put("=-1");
        return -1;
    }
    /* the host's GetTempFileNameA, less its ".tmp": a full path in `dir` */
    snprintf(out, 40, "%s~t%u", dir, rnd_n(1000));
    put("='%s'", out);
    return 0;
}

static int32_t STDCALL h_delete(const char *path, int32_t ext)
{
    struct vfile *f = vfile_find(path);

    put(" delete(");
    put_name(path);
    put(",%d)", ext);
    if (f == NULL || (f->attr & 2) || rnd_n(60) == 0) {
        put("=-1");
        return -1;
    }
    f->used = 0;
    return 0;
}

static int32_t STDCALL h_rename(const char *to, int32_t to_ext, const char *from, int32_t from_ext)
{
    struct vfile *f = vfile_find(from);

    put(" rename(");
    put_name(to);
    put(",%d,", to_ext);
    put_name(from);
    put(",%d)", from_ext);
    if (f == NULL || vfile_find(to) != NULL || rnd_n(60) == 0) {
        put("=-1");
        return -1;
    }
    snprintf(f->name, sizeof f->name, "%s", to);
    return 0;
}

static int32_t STDCALL h_pathlen(const char *path, int32_t ext, uint32_t *length)
{
    put(" pathlen(");
    put_name(path);
    put(",%d)", ext);
    if (rnd_n(40) == 0) {
        /* the Save dialog's check goes on to test it, an uninitialised local in
         * retail and in ours, whatever each stack holds: give them the same */
        *length = 0;
        put("=-1");
        return -1;
    }
    /* now and then too long, or right on the limit */
    *length = rnd_n(6) == 0 ? 0x103 + rnd_n(2) : (uint32_t)strlen(path) + 8;
    put("=%#x", *length);
    return 0;
}

static int32_t STDCALL h_nameok(const char *name)
{
    int32_t r = strpbrk(name, "?*\\/") != NULL || !name[0] ? -1 : 0;

    put(" nameok(");
    put_name(name);
    put(")=%d", r);
    return r;
}

static int32_t STDCALL h_cwd(char *out)
{
    put(" cwd");
    if (rnd_n(60) == 0) {
        put("=-1");
        return -1;
    }
    strcpy(out, "C:\\FS98");
    return 0;
}

static int32_t STDCALL h_free_space(const char *drive, uint32_t *space)
{
    put(" freespace(");
    put_name(drive);
    put(")");
    if (rnd_n(60) == 0) {
        put("=-1");
        return -1;
    }
    space[1] = rnd_n(4) == 0 ? 1 : 0;
    space[0] = rnd_n(4) == 0 ? rnd_n(0x2000) : 0x100000;
    put("=%#x,%#x", space[1], space[0]);
    return 0;
}

static int32_t STDCALL h_strcmp(const char *a, const char *b)
{
    int r = strcmp(a, b);

    put(" strcmp(");
    put_name(a);
    put(",");
    put_name(b);
    put(")");
    return r < 0 ? -1 : r > 0;
}

static int32_t STDCALL h_stricmp(const char *a, const char *b)
{
    int r = _stricmp(a, b);

    put(" stricmp(");
    put_name(a);
    put(",");
    put_name(b);
    put(")");
    return r < 0 ? -1 : r > 0;
}

/* UTIL +0x98: list the situation files of two directories into records of
 * 0xa08: +0 the name, +0x100 and +0x500 from the callbacks, +0x900 the path,
 * +0xa04 set for the second directory's, which may be changed.  UTIL is verified
 * in its own harness; here only MAIN's glue around it. */
static unsigned long cover_list_empty;
static uint32_t list_last_count;

static uint32_t STDCALL util_list(uint32_t id1, uint32_t id2, uint32_t ext, uint32_t *count, uint32_t *list,
                                  uint32_t title, uint32_t description, uint32_t a8, uint32_t a9)
{
    uint32_t ids[2], d, i, n = 0;
    uint8_t *recs;

    ids[0] = id1;
    ids[1] = id2;
    put(" list(%#x,%#x,%u,", id1, id2, ext);
    put_ptr(title);
    put(",");
    put_ptr(description);
    put(",%#x,%#x)", a8, a9);
    recs = (uint8_t *)arena_get(MAX_FILES * 0xa08u);
    for (d = 0; d < 2; d++) {
        const char *dir = ids[d] ? string_of(ids[d]) : NULL;
        size_t k;

        if (dir == NULL)
            continue;
        k = strlen(dir);
        for (i = 0; i < MAX_FILES; i++) {
            uint8_t *r = recs + n * 0xa08u;

            if (!files[i].used || _strnicmp(files[i].name, dir, k) || files[i].name[k] != '\\' ||
                strchr(files[i].name + k + 1, '\\') != NULL)
                continue;
            strcpy((char *)r, files[i].name + k + 1);
            ((s2)(uintptr_t)title)((uint32_t)(uintptr_t)files[i].name, (uint32_t)(uintptr_t)(r + 0x100));
            ((s2)(uintptr_t)description)((uint32_t)(uintptr_t)files[i].name, (uint32_t)(uintptr_t)(r + 0x500));
            strcpy((char *)r + 0x900, files[i].name);
            *(uint32_t *)(r + 0xa04) = d;
            n++;
        }
    }
    *count = n;
    *list = (uint32_t)(uintptr_t)recs;
    list_last_count = n;
    put("=%u", n);
    if (counting && n == 0) cover_list_empty++;
    return 0;
}

static void dump_files(void)
{
    uint32_t i, j, h;

    for (i = 0; i < MAX_FILES; i++)
        if (files[i].used) {
            for (h = 0x811c9dc5u, j = 0; j < files[i].size; j++)
                h = (h ^ files[i].bytes[j]) * 0x01000193u;
            put(" [%s %#x %08x]", files[i].name, files[i].size, h);
        }
}

/* ---- the dialogs' stand-in (host +0x10c) ----------------------------------------- */

/* A dialog as the English DLLs drive it: it edits the records of its exchange
 * table, may ask the descriptor's callback at +4 to validate, and answers OK or
 * not in the descriptor's +0.  Each dialog is told by its id: its table(s) in
 * MAIN's .data.  Now and then the host fails to open it. */
static const struct dialog_kind { uint32_t id, table, n; } dialog_kinds[] = {
    { 0x1301, 0x430, 0xf },
    { 0x1206, 0xd48, 0x3f },
    { 0x1204, 0x850, 7 },
    { 0x1303, 0x1590, 1 },
    { 0x1409, 0x5ee8, 0x19 },
    { 0x1508, 0x6e98, 0 },
    { 0x1407, 0x3b08, 0 },
    { 0x1302, 0x79b0, 0 },
    { 0x1202, 0x5588, 0xe },
    { 0x1101, 0x3b08, 0 },
    { 0x13040001, 0x7b40, 0 },
    { 0x14070001, 0x3b08, 0 },
    { 0x14070002, 0x3b08, 0 },
    { 0x14090001, 0x60e0, 0x10 },
    { 0x1305, 0x6b8, 5 }, { 0x1304, 0x1260, 4 }, { 0x1203, 0x1f8, 5 }, { 0x1203, 0x1e0, 1 },
};
static unsigned long cover_cd_field, cover_cd_field_ok;
static unsigned long cover_dlg, cover_dlg_ok, cover_dlg_refused, cover_dlg_child, cover_dlg_noopen;

static void dialog_edit(uint32_t desc, const uint8_t *table, uint32_t n)
{
    uint32_t i;

    for (i = 0; i < n; i++, table += 20) {
        uint32_t kind = *(const uint32_t *)table, off = *(const uint32_t *)(table + 4);
        uint8_t *rec = (uint8_t *)(uintptr_t)(desc + off);

        if (kind == 4) {
            char *t = *(char **)(rec + 4);
            uint32_t k = rnd_n(6);
            if (t != NULL && !IsBadWritePtr(t, 16)) {
                if (k >= 3) {                      /* a COM frequency: 0x1203's limit is 118.00 */
                    static const char *const edge[] = { "118.00", "117.95", "118.02", "118.05" };
                    if (rnd_n(2))
                        snprintf(t, 16, "%s", edge[rnd_n(4)]);
                    else
                        snprintf(t, 16, "%u.%02u", 108 + rnd_n(30), rnd_n(4) * 25);
                }
                else
                    snprintf(t, 16, k == 0 ? "x%u" : k == 1 ? "%u" : "%u.%u", k == 1 ? rnd_n(0x4000) : rnd_n(70000),
                             rnd_n(100));
            }
        } else if (kind < 4 && rnd_n(2)) {
            *(uint32_t *)(rec + 4) = rnd_n(4) ? (rnd_n(4) ? 1 : rnd_n(3)) : rnd();
        }
    }
}

/* The Open Flight dialog over its block { answer, nothing, delete, edit, default,
 * selection, count, list }: pressing its buttons in some order, then closing it. */
static unsigned long cover_open_delete, cover_open_edit, cover_open_ok, cover_save_ok, cover_save_status;

static uint32_t flight_pick(uint32_t count)
{
    return count ? rnd_n(count) : 0;
}

static void open_dialog(uint32_t *b)
{
    uint32_t i, n = rnd_n(4), r;

    put(" {sel=%u of %u}", b[5], b[6]);
    r = ((s1)(uintptr_t)b[1])((uint32_t)(uintptr_t)b);
    put(" nothing=%#x", r);
    for (i = 0; i < n; i++) {
        b[5] = flight_pick(b[6]);
        if (b[6] == 0)
            break;
        if (rnd_n(2)) {
            if (counting) cover_open_delete++;
            r = ((s1)(uintptr_t)b[2])((uint32_t)(uintptr_t)b);
            put(" delete=%#x sel=%u of %u", r, b[5], b[6]);
        } else {
            char *edit = (char *)arena_get(0xa08);
            uint32_t k = rnd_n(8);

            memset(edit, 0, 0xa08);
            b[5] = rnd_n(16) == 0 ? b[6] : b[5];
            if (b[5] < b[6])
                strcpy(edit + 0x900, (char *)(uintptr_t)b[7] + b[5] * 0xa08u + 0x900);
            if (k == 0)                            /* past what record 0 keeps */
                snprintf(edit, 48, "FL%u_%034u", rnd_n(8), rnd_n(100));
            else if (k < 3)
                snprintf(edit, 32, "FL%u", rnd_n(8));
            else if (k == 3)
                strcpy(edit, "bad?name");
            if (k >= 3 && rnd_n(2)) {
                if (rnd_n(3) == 0) {               /* past what record 2 keeps */
                    uint32_t n = 0xf0 + rnd_n(0x40);
                    memset(edit + 0x100, 'w', n);
                    edit[0x100 + n] = 0;
                } else {
                    snprintf(edit + 0x100, 64, "words %u", rnd());
                }
            }
            if (counting) cover_open_edit++;
            r = ((s2)(uintptr_t)b[3])((uint32_t)(uintptr_t)b, (uint32_t)(uintptr_t)edit);
            put(" edit=%#x", r);
        }
    }
    b[5] = flight_pick(b[6]);
    b[4] = rnd_n(2);
    /* any other button's number: retail tests != 0 here, != 1 in +0xc4 */
    b[0] = b[6] == 0 ? 0 : rnd_n(4) ? 1 : rnd_n(3);
    if (counting && b[0]) cover_open_ok++;
}

/* The Save Flight dialog over its descriptor: a file name and description in the
 * buffers its records name, its callback asked now and then. */
static void save_dialog(uint32_t desc)
{
    char *name = *(char **)(uintptr_t)(desc + 0xc), *words = *(char **)(uintptr_t)(desc + 0x20);
    uint32_t k = rnd_n(8), r;

    /* the callback: left retail's, ours would call retail's mapped copy and agree */
    put(" {%#x,", *(uint32_t *)(uintptr_t)(desc + 0x10));
    put_ptr(*(uint32_t *)(uintptr_t)(desc + 4));
    put("}");
    if (k == 0)
        name[0] = 0;
    else if (k == 1)
        strcpy(name, "no?good");
    else
        snprintf(name, 32, "FL%u", rnd_n(8));
    snprintf(words, 32, "about %u", rnd_n(100));
    if (rnd_n(2)) {
        r = ((s1)(uintptr_t)*(uint32_t *)(uintptr_t)(desc + 4))(desc);
        put(" check=%#x status=%#x", r, *(uint32_t *)(uintptr_t)(desc + 8));
        if (counting && r != 0) cover_save_status++;
    }
    *(uint32_t *)(uintptr_t)(desc + 0x30) = rnd_n(2);
    *(uint32_t *)(uintptr_t)desc = rnd_n(3) != 0;
    if (counting && *(uint32_t *)(uintptr_t)desc) cover_save_ok++;
}

static uint32_t pf_entries;

/* The advanced weather dialog's text fields, mostly numbers on the limits its check holds them to. */
static void wx_text(char *t)
{
    static const uint16_t v[] = { 0, 1, 9, 10, 100, 199, 200, 201, 250, 0x7710, 0x7711, 0x76ab, 0x76ac, 36, 37, 20, 21 };

    if (t != NULL && !IsBadWritePtr(t, 8))
        snprintf(t, 7, rnd_n(16) ? "%u" : "x%u", v[rnd_n(17)]);
}

/* Every field in range, then one at an edge, in metres so that the text is the value:
 * refused text returns from the check at the first layer, and the rest is never looked at */
static void wx_good(uint32_t desc)
{
    static const char *const edges[][2] = {
        { "1", "1100" }, { "0", "1000" }, { "30379", "30380" }, { "30380", "30500" }, { "1000", "1100" }, { "1000", "30480" },
        { "1000", "30481" }, { "1000", "2000" },
    };
    uint8_t *b = (uint8_t *)(uintptr_t)desc;
    uint8_t *cloud = (uint8_t *)(uintptr_t)*(uint32_t *)(cur->image + 0x7f4c), *misc = (uint8_t *)(uintptr_t)*(uint32_t *)(cur->image + 0x7f50);
    uint32_t i, at = rnd_n(3), which = rnd_n(8);

    *(uint16_t *)(global + 0xc18) = 2;
    for (i = 0; i < 3; i++) {
        uint8_t *r = b + 0x24 + i * 0x58, *c = b + 0x168 + i * 0x64;

        *(uint32_t *)(r + 0x40) = rnd_n(2) * 10;
        snprintf(*(char **)(r + 4), 7, "5000");
        snprintf(*(char **)(r + 0x18), 7, "8000");
        snprintf(*(char **)(r + 0x2c), 7, "10");
        snprintf(*(char **)(c + 4), 7, "%s", i == at ? edges[which][0] : "2000");
        snprintf(*(char **)(c + 0x18), 7, "%s", i == at ? edges[which][1] : "3000");
        snprintf(*(char **)(c + 0x2c), 7, "%s", i == at && rnd_n(4) == 0 ? (rnd_n(2) ? "199" : "200") : "4");
        snprintf(*(char **)(c + 0x40), 7, "0");
    }
    for (i = 0; i < 4; i++) {
        uint8_t *t = b + 0x2f8 + i * 0x2c;

        snprintf(*(char **)(t + 4), 7, "%u", 500 + i * 1000);
        snprintf(*(char **)(t + 0x18), 7, "10");
    }
    /* one refused: its limits come from the layer before. Only where the check looks at it:
     * passed over, .text+0x1ce70 reads it and compares what its stack held */
    if (rnd_n(3) == 0 && *(uint32_t *)(cur->image + 0x7b54) == 0 && *(uint32_t *)(cur->image + 0x7b58) == 0) {
        for (i = 0; i < 4; i++)
            *(uint32_t *)(b + 0x2f8 + i * 0x2c + 0x28) = 1;
        snprintf(*(char **)(b + 0x2f8 + (1 + rnd_n(2)) * 0x2c + 4), 7, "x");
    }
    snprintf(*(char **)(misc + 0xc4), 7, "5");
    snprintf(*(char **)(misc + 0xe0), 7, "29.92");
    snprintf(*(char **)(misc + 0xf4), 7, "1013.2");
    *(uint32_t *)(cloud + 0x140) = rnd_n(2);
    snprintf(*(char **)(cloud + 0x148), 7, "%u", rnd_n(2) ? 2000 : 9 + rnd_n(3));
    snprintf(*(char **)(cloud + 0x15c), 7, "%s", rnd_n(4) ? "4" : rnd_n(2) ? "199" : "200");
    snprintf(*(char **)(cloud + 0x170), 7, "0");
}

static void wx_edit(uint32_t desc)
{
    uint8_t *b = (uint8_t *)(uintptr_t)desc;
    uint32_t i;

    for (i = 0; i < 3; i++) {
        uint8_t *r = b + 0x24 + i * 0x58, *c = b + 0x168 + i * 0x64;

        if (rnd_n(2)) {
            *(uint32_t *)(r + 0x54) = rnd_n(3) ? 1 : 0;
            *(uint32_t *)(r + 0x40) = rnd_n(11);
            *(uint32_t *)(r + 0x48) = rnd_n(0x100);
            wx_text(*(char **)(r + 4));
            wx_text(*(char **)(r + 0x18));
            wx_text(*(char **)(r + 0x2c));
        }
        if (rnd_n(2)) {
            *(uint32_t *)(c + 0x60) = rnd_n(3) ? 1 : 0;
            /* the cloud sort reads these on applying: refused, it compares what its stack held */
            snprintf(*(char **)(c + 4), 7, "%u", rnd_n(0x7700));
            snprintf(*(char **)(c + 0x18), 7, "%u", rnd_n(0x7700));
            wx_text(*(char **)(c + 0x2c));
            wx_text(*(char **)(c + 0x40));
        }
    }
    for (i = 0; i < 4; i++) {
        uint8_t *t = b + 0x2f8 + i * 0x2c;

        if (rnd_n(2)) {
            *(uint32_t *)(t + 0x28) = rnd_n(3) ? 1 : 0;
            /* refused, .text+0x1ce70 would compare with what its stack held */
            snprintf(*(char **)(t + 4), 7, "%u", rnd_n(0x600));
            wx_text(*(char **)(t + 0x18));
        }
    }
    if (rnd_n(2)) {
        uint8_t *x = b + 0x2f0;                    /* .data+0x7e30, the misc fields */
        /* refused, with every layer off, the check would read what its stack held */
        snprintf(*(char **)(x + 0xc4), 7, "%u", rnd_n(0x30));
        wx_text(*(char **)(x + 0xe0));
        wx_text(*(char **)(x + 0xf4));
    }
    if (rnd_n(2)) {
        uint8_t *x = b + 0x160;                    /* .data+0x7ca0, the fourth cloud */
        *(uint32_t *)(x + 0x140) = rnd_n(2);
        snprintf(*(char **)(x + 0x148), 7, "%u", rnd_n(0x7700));
        wx_text(*(char **)(x + 0x15c));
        wx_text(*(char **)(x + 0x170));
    }
}

/* The text a table entry's field shows. */
static void clock_text(uint32_t desc, uint32_t entry, uint32_t v)
{
    char *t = *(char **)(uintptr_t)(desc + *(uint32_t *)(cur->image + entry + 4) + 4);

    if (t != NULL && !IsBadWritePtr(t, 8))
        snprintf(t, 8, "%u", v);
}

/* A date on an edge: past a month or the year's end, or by a season's first day. */
static void clock_date_edge(uint32_t desc)
{
    static const uint8_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    uint32_t year = rnd_n(3) ? 1999 : rnd_n(2) ? 2000 : 1900, month, day;

    if (rnd_n(2)) {
        static const uint8_t edge[][2] = { { 1, 0 }, { 12, 31 }, { 12, 32 }, { 12, 33 }, { 12, 34 }, { 2, 29 }, { 2, 30 }, { 13, 1 }, { 0, 1 } };
        const uint8_t *e = edge[rnd_n(9)];
        month = e[0];
        day = e[1];
    } else {
        uint32_t t = *(uint16_t *)(cur->image + 0x2f8 + 2 * rnd_n(5)) + rnd_n(3) - 1;

        month = 1;
        while (month < 12 && t > days[month - 1]) {
            t -= days[month - 1];
            month++;
        }
        day = t;
    }
    clock_text(desc, 0x444, year);
    clock_text(desc, 0x520, month);
    clock_text(desc, 0x534, day);
}

/* And a time on one. */
static void clock_time_edge(uint32_t desc)
{
    clock_text(desc, 0x494, rnd_n(26));
    clock_text(desc, 0x4a8, rnd_n(62));
    clock_text(desc, 0x4bc, rnd_n(26));
    clock_text(desc, 0x4d0, rnd_n(62));
}

/* The view dialog's records as they stand: all but their pointers, and their texts */
static void vw_recs(uint32_t recs, uint32_t n)
{
    uint32_t i, off, h = 0x811c9dc5u;

    for (i = 0; i < n; i++) {
        const uint8_t *rec = (const uint8_t *)(uintptr_t)recs + i * 0xb4;

        for (off = 0; off < 0xb0; off += 4)
            switch (off) {
            case 0x10: case 0x24: case 0x38: case 0x4c: case 0x60: case 0x80: case 0x94:
                h = digest(*(const uint8_t *const *)(rec + off), 8, h);
                break;
            case 0x18: case 0x1c: case 0x2c: case 0x30: case 0x40: case 0x44: case 0x54: case 0x58:
            case 0x68: case 0x6c: case 0x88: case 0x8c: case 0x9c: case 0xa0:
                break;
            default:
                h = digest(rec + off, 4, h);
            }
    }
    put(" recs=%08x", h);
}

static uint32_t STDCALL h_dialog(uint32_t id, uint32_t desc)
{
    uint32_t i, known = 0;

    if (id == 0x1508) {                            /* the limits each record names, each side's own address */
        uint32_t *d = (uint32_t *)(uintptr_t)desc, r, j;
        static const uint8_t limits[14] = { 0x18, 0x1c, 0x2c, 0x30, 0x40, 0x44, 0x54, 0x58, 0x68, 0x6c, 0x88, 0x8c, 0x9c, 0xa0 };

        for (r = 0; r < d[6]; r++)
            for (j = 0; j < 14; j++)
                *(uint32_t *)(uintptr_t)(d[7] + r * 0xb4 + limits[j]) = 0;
    }

    put(" dialog(%#x,", id);
    put_ptr(desc);
    put(")");
    if (counting) cover_dlg++;
    if (id == 0x16070001) {
        if (counting) cover_dlg_child++;
        return 0;
    }
    if (id == 0x1607) {
        uint32_t *b = (uint32_t *)(uintptr_t)desc, r;
        put(" {%u}", b[0]);
        r = ((s1)(uintptr_t)b[1])(fixture_arg);
        put(" a=%#x", r);
        r = ((s1)(uintptr_t)b[4])(fixture_arg);
        put(" b=%#x", r);
        return 0;
    }
    if (rnd_n(8) == 0) {
        if (counting) cover_dlg_noopen++;
        put(" no");
        return 0xffffffffu;
    }
    if (id == 0x14090002) {                        /* the aircraft */
        uint32_t *b = (uint32_t *)(uintptr_t)desc;

        put(" %u a=%#x b=%#x c=%#x", b[5], ((s1)(uintptr_t)b[1])(desc), ((s1)(uintptr_t)b[2])(desc), ((s1)(uintptr_t)b[3])(desc));
        b[0] = rnd_n(2);
        b[4] = b[5] ? rnd_n(b[5]) : 0;
        pf_exists_answer = rnd_n(3) == 0;
        return 0;
    }
    if (id == 0x13050001) {                        /* which to copy from, to, and what */
        uint32_t *b = (uint32_t *)(uintptr_t)desc, i;

        put(" {%u of %u}", b[2], b[3]);
        b[0] = rnd_n(4) ? 1 : rnd_n(3);
        b[7] = rnd_n(3);
        b[8] = rnd_n(3);
        b[10] = rnd_n(2);
        for (i = 12; i <= 20; i += 2)
            b[i] = rnd_n(2);
        return 0;
    }
    if (id == 0x1102) {
        open_dialog((uint32_t *)(uintptr_t)desc);
        return 0;
    }
    if (id == 0x1107) {
        uint32_t *b = (uint32_t *)(uintptr_t)desc;
        put(" nothing=%#x", ((s1)(uintptr_t)b[1])(desc));
        b[3] = flight_pick(b[2]);
        b[0] = b[2] != 0 && rnd_n(4) != 0;
        return 0;
    }
    if (id == 0x1104) {
        save_dialog(desc);
        return 0;
    }
    for (i = 0; i < sizeof dialog_kinds / sizeof dialog_kinds[0]; i++)
        if (dialog_kinds[i].id == id) {
            dialog_edit(desc, cur->image + dialog_kinds[i].table, dialog_kinds[i].n);
            known = 1;
        }
    if (!known) {
        printf("no stand-in for dialog %#x\n", id);
        exit(2);
    }
    if (id == 0x1301) {                            /* a field at a time, as they are edited */
        uint32_t k = rnd_n(6);

        put(" 7ff0=%x 7ff8=%x", *(uint16_t *)(cur->image + 0x7ff0), *(uint16_t *)(cur->image + 0x7ff8));
        dialog_edit(desc, cur->image + 0x560, 0xb);
        while (k-- != 0) {
            static const uint8_t pick[8] = { 0, 1, 1, 2, 2, 2, 3, 4 };
            uint32_t f = pick[rnd_n(8)], r;

            if (rnd_n(4) == 0)
                dialog_edit(desc, cur->image + 0x430 + 20 * rnd_n(0xc), 1);
            if (f == 1)
                clock_date_edge(desc);
            if (rnd_n(2))
                clock_time_edge(desc);
            if (f == 2)
                *(uint32_t *)(uintptr_t)(desc + 0xa4) = rnd_n(5);
            r = ((s1)(uintptr_t)*(uint32_t *)(uintptr_t)(desc + 0x110 + f * 4))(desc);
            put(" field%u=%#x", f, r);
            if (counting) { cover_cd_field++; if (r == 0) cover_cd_field_ok++; }
            put(" %u:%u %u:%u", cur->image[0x8004], cur->image[0x7fd8], cur->image[0x7fe8], cur->image[0x7fe4]);
        }
        *(uint32_t *)(uintptr_t)(desc + 0xc) = rnd_n(9);
        if (rnd_n(2)) {                            /* all well but for one thing, on its edge */
            static const uint8_t ok[3] = { 1, 2, 4 };
            uint32_t edge = rnd_n(7);

            *(uint32_t *)(uintptr_t)(desc + 0xc) = 0;
            clock_text(desc, 0x444, 1999);
            clock_text(desc, 0x520, 1 + rnd_n(12));
            clock_text(desc, 0x534, 1 + rnd_n(28));
            clock_text(desc, 0x494, rnd_n(24));
            clock_text(desc, 0x4a8, rnd_n(60));
            clock_text(desc, 0x4bc, rnd_n(24));
            clock_text(desc, 0x4d0, rnd_n(60));
            *(uint32_t *)(uintptr_t)(desc + 0x70) = rnd_n(4);
            *(uint32_t *)(uintptr_t)(desc + 0xac) = ok[rnd_n(3)];
            switch (edge) {
            case 0: clock_text(desc, 0x494, 0x17 + rnd_n(3)); break;
            case 1: clock_text(desc, 0x4a8, 0x3b + rnd_n(3)); break;
            case 2: clock_text(desc, 0x4bc, 0x17 + rnd_n(3)); break;
            case 3: clock_text(desc, 0x4d0, 0x3b + rnd_n(3)); break;
            case 4: *(uint32_t *)(uintptr_t)(desc + 0x70) = rnd_n(6); break;
            case 5: *(uint32_t *)(uintptr_t)(desc + 0xac) = rnd_n(6); break;
            default: clock_text(desc, rnd_n(2) ? 0x520 : 0x534, 0); break;
            }
        }
    }
    if (id == 0x14090001) {                        /* the display: drivers, modes, flags */
        uint8_t *e = (uint8_t *)(uintptr_t)(desc + 0xa0);
        uint32_t k;

        for (k = rnd_n(3); k-- != 0; ) {
            *(uint32_t *)(e + 0x10) = rnd_n(2);
            *(uint32_t *)(e + 0x34) = rnd_n(2);
            *(uint32_t *)(e + 0x38) = rnd_n(2);
            *(uint32_t *)(e + 0x3c) = rnd_n(2);
            put(" drivers=%#x", ((s1)(uintptr_t)*(uint32_t *)e)((uint32_t)(uintptr_t)e));
            put(" d=%u/%u", *(uint32_t *)(e + 0x18), *(uint32_t *)(e + 0x20));
            if (rnd_n(2))
                *(uint32_t *)(e + 0x20) = pf_entries ? rnd_n(pf_entries) : 0;
            put(" modes=%#x", ((s1)(uintptr_t)*(uint32_t *)(e + 4))((uint32_t)(uintptr_t)e));
            put(" m=%d f=%u", *(int32_t *)(e + 0x2c), *(uint32_t *)(e + 0x30));
            if (*(int32_t *)(e + 0x2c) < 0 && rnd_n(2))
                *(uint32_t *)(e + 0x2c) = 0;
            if (rnd_n(4) == 0)
                put(" refresh=%#x", ((s1)(uintptr_t)*(uint32_t *)(e + 8))((uint32_t)(uintptr_t)e));
        }
        *(uint32_t *)(e + 0x14) = *(int32_t *)(e + 0x2c) >= 0 && *(uint32_t *)(e + 0x18) != 0 && *(uint32_t *)(e + 0x24) != 0 ? rnd_n(2) : 0;
    }
    if (id == 0x1409) {                            /* the buttons, some of them */
        uint32_t *b = (uint32_t *)(uintptr_t)desc, k;

        put(" sel=%u %u", b[0xd0 / 4], b[0xdc / 4]);
        for (k = rnd_n(4); k-- != 0; )
            switch (rnd_n(5)) {
            case 0: put(" defaults=%#x", ((s1)(uintptr_t)b[3])(desc)); break;
            case 1: put(" aircraft=%#x g638='%.20s'", ((s1)(uintptr_t)b[5])(desc), (char *)global + 0x638); break;
            case 2: put(" flight=%#x", ((s1)(uintptr_t)b[6])(desc)); break;
            case 3: put(" display=%#x", ((s1)(uintptr_t)b[0xc4 / 4])(desc)); break;
            default:
                b[0xd0 / 4] = rnd_n(6);
                put(" preset=%#x %x %x", ((s1)(uintptr_t)b[0xd4 / 4])(desc), b[0xdc / 4], b[0xf4 / 4]);
                break;
            }
        put(" sel=%u", b[0xd0 / 4]);
        b[0xe4 / 4] = rnd_n(2);
    }
    if (id == 0x14070001) {                        /* a new logbook's name */
        uint8_t *b = (uint8_t *)(uintptr_t)desc;
        char *name = *(char **)(b + 0x34);
        static const char *const names[] = { "NEWLOG", "LOG1", "LOG1.LOG", "", "bad:name", "LOG2" };
        uint32_t k = rnd_n(3);

        strcpy(name, names[rnd_n(6)]);
        *(uint32_t *)(b + 0x28) = rnd_n(4) ? 1 : 0;
        while (k-- != 0)
            put(" check=%#x s=%#x x=%u", ((s1)(uintptr_t)*(uint32_t *)(b + 0x2c))((uint32_t)(uintptr_t)(b + 0x28)), *(uint32_t *)(b + 0x30), *(uint32_t *)(b + 0x44));
        put(" name=%s", name);
        return 0;
    }
    if (id == 0x14070002) {                        /* a row's fields */
        uint8_t *b = (uint8_t *)(uintptr_t)desc;
        static const uint16_t cols[3] = { 0x9c, 0xb0, 0xc4 };
        uint32_t k;
        static const char *const nums[] = { "", "0", "1.5", " 12.3", "99.9", "999.9", "1000", ".5", "1.", "1..2", "12x", "000.0", " .", "99999", "   ", "2.75", "12:5", "1:", "1 2", "1234", "1234.5", "123.4", "12345", "999.", "0.0", "00.5", "001", "1000.0", "9999", "x", "7", "123.45", "1234567", "100.0", "100", "12.345"};
        static const char *const dates[] = { "01-01-98", "12-31-99", "13-01-98", "02-29-96", "02-29-97", "04-31-98", "1-1-98", "", "02-30-96", "11-31-00", "09-31-98", "09-30-98", "01-32-98", "01-31-98", "20-01-98", "1x-01-98", "01-40-98", "01/01/98", "01-01-9x", "04-30-98", "01-01/98"};

        put(" [%.8s|%.16s|%.24s|%.6s|%.6s|%.6s|%.6s]", *(char **)(b + 0x60), *(char **)(b + 0x74), *(char **)(b + 0x88), *(char **)(b + 0x9c), *(char **)(b + 0xb0), *(char **)(b + 0xc4), *(char **)(b + 0xd8));
        put(" sel=%u", *(uint32_t *)(b + 0xf4));
        *(uint32_t *)(b + 0x54) = rnd_n(4) ? 1 : 0;
        if (rnd_n(2))
            strcpy(*(char **)(b + 0x60), dates[rnd_n(sizeof dates / sizeof dates[0])]);
        {
            static const char *const models[] = { "CESSNA", "737", "Plane1 Long Mode", "Plane1 Long Model Name" };

            strcpy(*(char **)(b + 0x74), models[rnd_n(4)]);
        }
        for (k = 0; k < 3; k++)
            if (rnd_n(2))
                strcpy(*(char **)(b + cols[k]), nums[rnd_n(sizeof nums / sizeof nums[0])]);
        put(" check=%#x", ((s1)(uintptr_t)*(uint32_t *)(b + 0x58))((uint32_t)(uintptr_t)(b + 0x54)));
        put(" %x %x %x %x %.6s", *(uint32_t *)(b + 0x5c), *(uint32_t *)(b + 0x98), *(uint32_t *)(b + 0xac), *(uint32_t *)(b + 0xc0), *(char **)(b + 0xd8));
        return 0;
    }
    if (id == 0x1407) {                            /* the buttons */
        uint8_t *b = (uint8_t *)(uintptr_t)desc;
        uint32_t k;

        for (k = rnd_n(5); k-- != 0; ) {
            static const uint8_t often[4] = { 2, 3, 6, 7 };
            uint32_t op = rnd_n(2) ? rnd_n(10) : often[rnd_n(4)], books = *(uint32_t *)(b + 0x1c), rows = *(uint32_t *)(b + 0x48);
            uint32_t i, j;

            if (cur->image[0x3c86] != 0)
                break;
            i = rnd_n(4) ? rnd_n((op >= 5 && op <= 7 ? rows : books) + 1) : rnd_n(4);

            if (op == 8) {
                static const char *const titles[] = { "New title", "", "A title of thirty characters..", "A title of thirty-one characters"};

                strcpy(*(char **)(b + 0xc), titles[rnd_n(4)]);
            }
            /* the selection past the rows, as deleting a longer logbook leaves it */
            if (op == 6 && rows != 0 && rnd_n(4) == 0)
                *(uint32_t *)(b + 0x4c) = rows + rnd_n(2);
            put(" button(%u,%u)=%#x", op, i, ((s3)(uintptr_t)*(uint32_t *)(b + 0x150))(op, i, desc));
            put(" %u/%u %u/%u", *(uint32_t *)(b + 0x20), *(uint32_t *)(b + 0x1c), *(uint32_t *)(b + 0x4c), *(uint32_t *)(b + 0x48));
            rows = *(uint32_t *)(b + 0x48);
            if (*(uint32_t *)(b + 0x4c) > rows)
                *(uint32_t *)(b + 0x4c) = 0;
            for (j = 0; j < rows && j < 16; j++) {         /* the rows as held: the file keeps only their columns */
                const uint8_t *row = (const uint8_t *)(uintptr_t)(*(uint32_t *)(b + 0x50) + j * 0x4f);
                uint32_t h = 0x811c9dc5u, q;

                if (IsBadReadPtr(row, 0x4f))
                    break;
                for (q = 0; q < 0x4f; q++)
                    h = (h ^ row[q]) * 0x01000193u;
                put(" r%x", h);
            }
        }
        return 0;
    }
    if (id == 0x1302) {                            /* the simple dialog: its three answers */
        uint32_t *b = (uint32_t *)(uintptr_t)desc;

        put(" %u %u %u %u %u", b[2], b[3], b[4], b[5], b[7]);
        {
            uint32_t j;
            for (j = 0; j < 4; j++)
                put(" T[%.8s|%.8s %u]", *(char **)(cur->image + 0x7e38 + j * 0x2c + 4), *(char **)(cur->image + 0x7e38 + j * 0x2c + 0x18),
                    *(uint32_t *)(cur->image + 0x7e38 + j * 0x2c + 0x28));
            for (j = 0; j < 8; j++)
                put(" %x", *(uint16_t *)(global + 0xf34 + j * 2));
        }
        b[6] = rnd_n(6);
        b[5] = rnd_n(6);
        b[7] = rnd_n(5);
        b[0] = rnd_n(4) ? 1 : 0;
        return 0;
    }
    if (id == 0x13040001) {                        /* the advanced one: its pages and their fields */
        uint8_t *b = (uint8_t *)(uintptr_t)desc;
        uint32_t k;

        for (k = 0; k < 3; k++)
            put(" top%x", *(uint16_t *)(b + 0x24 + k * 0x58 + 0x3c));
        for (k = rnd_n(4); k-- != 0; )
            wx_edit(desc);
        for (k = rnd_n(3); k-- != 0; ) {
            *(uint32_t *)(b + 0x404) = rnd_n(4);
            *(uint16_t *)(b + 0x20) = (uint16_t)rnd_n(3);
            *(uint16_t *)(b + 0x164) = (uint16_t)rnd_n(4);
            *(uint16_t *)(b + 0x2f4) = (uint16_t)rnd_n(4);
            switch (rnd_n(5)) {
            case 0: put(" add=%#x", ((s1)(uintptr_t)*(uint32_t *)(b + 0x3fc))(desc)); break;
            case 1: put(" del=%#x", ((s1)(uintptr_t)*(uint32_t *)(b + 0x400))(desc)); break;
            case 2: put(" up=%#x", ((s1)(uintptr_t)*(uint32_t *)(b + 0x3f4))(desc)); break;
            case 3: put(" down=%#x", ((s1)(uintptr_t)*(uint32_t *)(b + 0x3f8))(desc)); break;
            default:
                if (rnd_n(2)) {                    /* the height a wind type's defaults are held to */
                    static const char *const heights[] = { "13996", "13997", "99", "100", "4799", "4800", "5998", "5999" };
                    *(uint16_t *)(global + 0xc18) = 2;
                    snprintf(*(char **)(b + 0x24 + *(uint16_t *)(b + 0x20) * 0x58 + 4), 7, "%s", heights[rnd_n(8)]);
                }
                put(" def=%#x", ((s1)(uintptr_t)*(uint32_t *)(b + 0x158))(desc));
                put(" %.6s", *(char **)(b + 0x24 + *(uint16_t *)(b + 0x20) * 0x58 + 4));
                break;
            }
        }
        wx_edit(desc);
        if (rnd_n(2))
            wx_good(desc);
        *(uint32_t *)b = rnd_n(4) ? 1 : rnd_n(3);
        /* the selected temperature, absent or not: refused, .text+0x1ce70 starts from its stack */
        snprintf(*(char **)(b + 0x2fc + *(uint32_t *)(b + 0x2f4) * 0x2c), 7, "%u", rnd_n(0x600));
        put(" check=%#x", ((s1)(uintptr_t)*(uint32_t *)(b + 4))(desc));
        for (k = 0; k < 3; k++) {
            uint8_t *r = b + 0x24 + k * 0x58, *c = b + 0x168 + k * 0x64;
            put(" w%x,%x,%x c%x,%x,%x,%x", *(uint32_t *)r, *(uint32_t *)(r + 0x14), *(uint32_t *)(r + 0x28), *(uint32_t *)c,
                *(uint32_t *)(c + 0x14), *(uint32_t *)(c + 0x28), *(uint32_t *)(c + 0x3c));
        }
        for (k = 0; k < 4; k++)
            put(" t%x,%x", *(uint32_t *)(b + 0x2f8 + k * 0x2c), *(uint32_t *)(b + 0x2f8 + k * 0x2c + 0x14));
        return 0;
    }
    if (id == 0x1101) {                            /* Create a Flight: its buttons */
        uint32_t *b = (uint32_t *)(uintptr_t)desc, k;

        put(" %.20s|%.20s|%.30s|%.20s", (char *)(uintptr_t)b[7], (char *)(uintptr_t)b[8], (char *)(uintptr_t)b[9], (char *)(uintptr_t)b[10]);
        for (k = rnd_n(4); k-- != 0; ) {
            uint32_t w = rnd_n(5);
            put(" button%u=%#x", w, ((s1)(uintptr_t)b[2 + w])(desc));
            put(" %u %u %u %.20s|%.20s|%.60s|%.20s", b[11], b[12], b[13], (char *)(uintptr_t)b[7], (char *)(uintptr_t)b[8],
                (char *)(uintptr_t)b[9], (char *)(uintptr_t)b[10]);
        }
        b[0] = rnd_n(2);
        return 0;
    }
    if (id == 0x1202) {                            /* the radios: frequencies at their edges, a check */
        put(" 37e=%x", *(uint16_t *)(global + 0x37e));
        static const char *const adf[] = { "199", "200", "1699", "1699.9", "1700", "999", "1000", "1000.5", "99", "20000", "0999.5", "3a4", "12.5", "1.2", "", " 500 " };
        static const char *const nav[] = { "108.00", "117.95", "118.00", "111.10", "x", "", "122.50" };
        uint32_t b = desc, k;

        static const char *const adf2[] = { "\t300", " 450", "", "12", "1234567", "123.45", "1234.5", "2000", "2000.5", "699", "1699", "1700",
                                            "200", "199", "999", "1000", "300.5", "1699.9", "1234.56" };
        snprintf(*(char **)(uintptr_t)(b + 0x178), 8, "%s", rnd_n(2) ? adf[rnd_n(16)] : adf2[rnd_n(19)]);
        /* the rest of the check's fields, or it never gets past them */
        snprintf(*(char **)(uintptr_t)(b + 0x164), 8, "%s", rnd_n(8) ? "1200" : "9999");
        snprintf(*(char **)(uintptr_t)(b + 0x20c), 8, "%s", rnd_n(8) ? "100" : "x");
        snprintf(*(char **)(uintptr_t)(b + 0x44), 8, "%u", rnd_n(362));
        snprintf(*(char **)(uintptr_t)(b + 0xcc), 8, "%u", rnd_n(362));
        snprintf(*(char **)(uintptr_t)(b + 0x2c), 8, "%s", nav[rnd_n(4)]);
        snprintf(*(char **)(uintptr_t)(b + 0xb4), 8, "%s", nav[rnd_n(4)]);
        *(uint32_t *)(uintptr_t)(b + 0xc) = rnd_n(3);
        *(uint32_t *)(uintptr_t)(b + 0x12c) = rnd_n(2);
        *(uint32_t *)(uintptr_t)(b + 0x1b8) = rnd_n(2);
        *(uint32_t *)(uintptr_t)(b + 0xac) = rnd_n(4);
        *(uint32_t *)(uintptr_t)(b + 0x1cc) = rnd_n(4);
        *(uint32_t *)(uintptr_t)(b + 0x1d4) = rnd_n(4);
        *(uint32_t *)(uintptr_t)(b + 0x1dc) = rnd_n(4);
        *(uint32_t *)(uintptr_t)(b + 0x1e4) = rnd_n(4);
        *(uint32_t *)(uintptr_t)(b + 0x1ec) = 1 + rnd_n(4);
        *(uint32_t *)(uintptr_t)(b + 0x1fc) = rnd_n(3);
        *(uint32_t *)(uintptr_t)(b + 0x1bc) = rnd_n(2);
        *(uint32_t *)(uintptr_t)b = rnd_n(4) ? 1 : rnd_n(2);
        *(uint32_t *)(uintptr_t)(b + 0x174) = rnd_n(3);    /* a mark from an earlier look */
        if (rnd_n(2)) {
            static const uint16_t words[] = { 0x174, 0xa0, 0xa4, 0xac, 0x128, 0x12c, 0x1b4, 0x1b8, 0x1c8, 0x1d0, 0x1d8, 0x1e0, 0x1e8, 0x1f0, 0x1f4, 0x200, 0x204, 0x3c, 0xc4 };

            put(" check=%#x", ((s1)(uintptr_t)*(uint32_t *)(uintptr_t)(b + 4))(desc));
            for (k = 0; k < sizeof words / sizeof words[0]; k++)
                put(" %x", *(uint32_t *)(uintptr_t)(b + words[k]));
            put(" [%.6s|%.6s|%x %x %x %x %x]", *(char **)(uintptr_t)(b + 0x178), (char *)(uintptr_t)(b + 0x142), *(uint32_t *)(uintptr_t)(b + 0x174),
                *(uint32_t *)(uintptr_t)(b + 0x128), *(uint32_t *)(uintptr_t)(b + 0x1f0), *(uint32_t *)(uintptr_t)(b + 0x200), *(uint32_t *)(uintptr_t)(b + 0x204));
        }
        for (k = 0; k < 2; k++)
            put(" [%.8s|%.8s]", (char *)(uintptr_t)(b + (k ? 0xe2 : 0x5a)), (char *)(uintptr_t)(b + (k ? 0xdc : 0x54)));
        return 0;
    }
    if (id == 0x1508) {                            /* the views: picked, edited, previewed */
        uint32_t *b = (uint32_t *)(uintptr_t)desc, window = 0x7100 + rnd_n(0x100), r, k, recs = b[7], n = b[6];
        static const uint8_t fields[7] = { 0x10, 0x24, 0x38, 0x60, 0x4c, 0x80, 0x94 };

        put(" count=%u", n);
        vw_recs(recs, n);
        b[5] = n != 0 ? rnd_n(n) : (rnd_n(2) ? 0xffffffffu : 0);
        if (n != 0 && rnd_n(8) == 0) {             /* no window picked, among some */
            b[5] = 0xffffffffu;
            put(" open=%#x", ((s3)(uintptr_t)b[2])(1, desc, window));
            put(" draw=%#x", ((s3)(uintptr_t)b[2])(3, desc, window));
            put(" other=%#x", ((s3)(uintptr_t)b[2])(5, desc, window));
            put(" close=%#x", ((s3)(uintptr_t)b[2])(2, desc, window));
            b[5] = 0;
            b[0] = rnd_n(3);
            return 0;
        }
        r = ((s3)(uintptr_t)b[2])(1, desc, window);
        put(" open=%#x", r == (uint32_t)(uintptr_t)global ? 0xc0 : r);
        *(uint32_t *)(global + 0x1194) = 0;
        if (r == 0 || r == (uint32_t)(uintptr_t)global) {
            for (k = rnd_n(4); k-- != 0; ) {
                if (n != 0) {
                    uint8_t *rec;

                    if (rnd_n(2))
                        b[5] = rnd_n(n);
                    rec = (uint8_t *)(uintptr_t)recs + b[5] * 0xb4;
                    if (rnd_n(2)) {
                        char *t = *(char **)(rec + fields[rnd_n(7)]);
                        static const uint16_t v[] = { 0, 1, 45, 90, 180, 360, 361, 9999 };
                        static const char *const zooms[] = { "0.5", "1", "2", "32", "64", "0.01", "100" };
                        if (rnd_n(2))
                            snprintf(t, 8, rnd_n(4) ? "%u" : "x%u", v[rnd_n(8)]);
                        else
                            snprintf(t, 8, "%s", zooms[rnd_n(7)]);
                    }
                    if (rnd_n(3) == 0) {
                        *(uint32_t *)(rec + 4) = rnd_n(2);
                        *(uint32_t *)(rec + 8) = rnd_n(6);
                    }
                    if (rnd_n(3) == 0)                 /* a field only the apply takes back */
                        *(uint32_t *)(rec + 0x70) = rnd();
                }
                r = ((s3)(uintptr_t)b[2])(3, desc, window);
                put(" draw=%#x", r);
                if (n != 0 && rnd_n(2)) {                  /* the card: a click, or a key */
                    static uint32_t ev[2];
                    static const uint8_t keys[] = { 0x25, 0x26, 0x27, 0x28, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x24, 0x6a };
                    uint32_t key = rnd_n(2);
                    uint16_t *h = (uint16_t *)(uintptr_t)(recs + b[5] * 0xb4 + 0x78);

                    ev[0] = key ? keys[rnd_n(15)] : rnd_n(0x100);
                    ev[1] = rnd_n(0x100);
                    *h = (uint16_t)(rnd_n(2) ? rnd() : (rnd_n(5) * 0x4000 - 1 + rnd_n(3)));
                    r = ((s2)(uintptr_t)*(uint32_t *)(cur->image + 0x6f3c))((uint32_t)(uintptr_t)ev, key ? 0x100000 : rnd_n(0x10));
                    put(" card=%#x %x", r, *h);
                }
                if (rnd_n(2)) {
                    if (n == 0)
                        continue;
                    r = ((s1)(uintptr_t)b[1])(desc);
                    put(" check=%#x", r);
                    vw_recs(recs, n);
                }
            }
            r = ((s3)(uintptr_t)b[2])(rnd_n(8) ? 2 : 4, desc, window);
            put(" close=%#x", r == (uint32_t)(uintptr_t)global ? 0xc0 : r);
        }
        b[0] = rnd_n(3);
        b[8] = (uint16_t)rnd();                    /* +0x600 and +0x552, as the dialog leaves them */
        b[9] = (uint16_t)rnd();
        return 0;
    }
    if (id == 0x1303) {                            /* the areas: edited, added, taken away and copied */
        uint32_t *b = (uint32_t *)(uintptr_t)desc, recs = b[5], r, k;
        static const uint16_t t4[3] = { 0x15a8, 0x1688, 0x1768 }, t7[3] = { 0x15f8, 0x16d8, 0x17b8 };

        put(" which=%u count=%u", b[3], b[4]);
        for (r = 0; r < 3; r++) {
            static const uint8_t status[11] = { 0, 4, 0x18, 0x2c, 0x40, 0x54, 0x68, 0x7c, 0x90, 0xa4, 0xb8 };
            uint32_t j;

            if (rnd_n(2))
                dialog_edit(recs + r * 0xc8, cur->image + t4[r], 4);
            if (rnd_n(2))
                dialog_edit(recs + r * 0xc8, cur->image + t7[r], 7);
            if (rnd_n(2))                          /* marks from an earlier look, which a copy carries */
                for (j = 0; j < 11; j++)
                    *(uint32_t *)(uintptr_t)(recs + r * 0xc8 + status[j]) = rnd_n(4) + 2;
            if (rnd_n(2))
                *(uint32_t *)(uintptr_t)(recs + r * 0xc8 + 0xc0) = rnd_n(4) + 2;
        }
        for (k = rnd_n(5); k-- != 0; ) {
            uint32_t which = rnd_n(3);

            if (rnd_n(2))
                b[3] = rnd_n(4);
            put(" %s=%#x", which == 0 ? "add" : which == 1 ? "del" : "copy", ((s1)(uintptr_t)b[8 + which])(desc));
            put(" which=%u count=%u %u,%u", b[3], b[4], b[6], b[7]);
        }
        put(" area=%08x", digest(cur->image + 0x83e8, 0x170, 0x811c9dc5u));
    }
    if (id == 0x1204) {                            /* the fields, near their limits */
        static const uint16_t texts[3] = { 0x4c, 0x7c, 0x90 };
        uint32_t j;

        for (j = 0; j < 4; j++) {
            char *t = *(char **)(uintptr_t)(desc + (j < 3 ? texts[j] : 0xc4));
            static const int32_t edge[] = { -1000, -1001, -999, 1000, 1001, 999, 0, 49, 50, 25, 24, 75, 100, 150, 2500, 30000,
                                            2200, 2130, -1700, -1625, 6950, 7000, -5950, -6000 };
            if (t != NULL && !IsBadWritePtr(t, 8) && rnd_n(2))
                snprintf(t, 8, rnd_n(2) ? "%d" : "%d.5", edge[rnd_n(24)]);
        }
        *(uint32_t *)(uintptr_t)(desc + 0x64) = rnd_n(2);
        *(uint32_t *)(uintptr_t)(desc + 0x6c) = rnd_n(2);
        *(uint32_t *)(uintptr_t)(desc + 0xa0) = rnd_n(3);
        *(uint32_t *)(uintptr_t)(desc + 0xa4) = rnd_n(3);
        *(uint32_t *)(uintptr_t)(desc + 0xbc) = rnd_n(3);
        *(uint16_t *)(global + 0xc18) = (uint16_t)rnd_n(3);
        *(uint32_t *)(uintptr_t)(desc + 0x44) = rnd_n(2);
        *(uint32_t *)(uintptr_t)(desc + 0x18) = rnd_n(6);
        *(uint32_t *)(uintptr_t)(desc + 0xe0) = rnd_n(7);
        for (j = 0; j < 4; j++)
            put(" [%.8s]", *(char **)(uintptr_t)(desc + (j < 3 ? texts[j] : 0xc4)));
    }
    if (id == 0x1206) {                            /* the engines, a field edited at a time */
        uint32_t k = rnd_n(5);
        static const uint16_t base[3] = { 0x17c, 0x1e0, 0x244 };

        put(" a00=%u", *(uint32_t *)(uintptr_t)(desc + 8));
        {
            uint32_t j;
            for (j = 0; j < 5; j++)
                put(" [%.8s %.8s %.8s %.8s]", *(char **)(uintptr_t)(desc + 0x180 + j * 20), *(char **)(uintptr_t)(desc + 0x1e4 + j * 20),
                    *(char **)(uintptr_t)(desc + 0x248 + j * 20), *(char **)(uintptr_t)(desc + 0x2a8 + j * 4));
        }
        while (k-- != 0) {
            uint32_t i = rnd_n(6), which = rnd_n(3), r, j;
            char *cap = *(char **)(uintptr_t)(desc + 0x2a8 + (i % 5) * 4);

            for (j = 0; j < 3; j++) {                  /* numbers, mostly; the edges of the limits */
                static const uint16_t edges[] = { 0, 1, 99, 100, 101, 200, 255, 256 };
                char *t = *(char **)(uintptr_t)(desc + base[j] + (i % 5) * 20 + 4);
                if (t != NULL && !IsBadWritePtr(t, 8))
                    snprintf(t, 8, "%u", rnd_n(3) ? edges[rnd_n(8)] : rnd_n(0x4000));
            }
            (void)cap;
            r = ((s2)(uintptr_t)*(uint32_t *)(uintptr_t)(desc + 0x2bc + which * 4))(desc, i % 5);
            put(" edit%u(%u)=%#x", which, i % 5, r);
            *(uint32_t *)(uintptr_t)(desc + 0x20) = which + 1;
            *(uint32_t *)(uintptr_t)(desc + 0x24) = i % 5;
            for (j = 0; j < 5; j++)
                put(" %.8s/%.8s", *(char **)(uintptr_t)(desc + 0x1e4 + j * 20), *(char **)(uintptr_t)(desc + 0x248 + j * 20));
        }
        if (rnd_n(2)) {                            /* stale statuses, with a tank that holds nothing */
            uint32_t j;
            for (j = 0; j < 5; j++)
                *(uint32_t *)(uintptr_t)(desc + (j + 0x18) * 20) = rnd_n(3);
        }
        put(" up=%#x", ((s1)(uintptr_t)*(uint32_t *)(uintptr_t)(desc + 0x94))(desc));
        put(" down=%#x", ((s1)(uintptr_t)*(uint32_t *)(uintptr_t)(desc + 0x98))(desc));
        if (rnd_n(2)) {
            *(uint32_t *)(uintptr_t)(desc + 0x20) = rnd_n(5);
            *(uint32_t *)(uintptr_t)(desc + 0x24) = rnd_n(5);
        }
    }
    /* the button pressed goes in first: the callback validates knowing it */
    *(uint32_t *)(uintptr_t)desc = rnd_n(4) ? 1 : rnd_n(3);
    if (rnd_n(2)) {
        uint32_t r = ((s1)(uintptr_t)*(uint32_t *)(uintptr_t)(desc + 4))(desc);
        put(" check=%#x", r);
        if (counting && r != 0) cover_dlg_refused++;
        /* the statuses, which a dialog reads now to mark the fields objected to */
        for (i = 0; i < sizeof dialog_kinds / sizeof dialog_kinds[0]; i++)
            if (dialog_kinds[i].id == id) {
                const uint8_t *table = cur->image + dialog_kinds[i].table;
                uint32_t j;
                for (j = 0; j < dialog_kinds[i].n; j++)
                {
                    uint32_t v = *(uint32_t *)(uintptr_t)(desc + *(const uint32_t *)(table + 20 * j + 4));
                    put(" %x", in_image(v) ? v - (uint32_t)(uintptr_t)cur->image : v);
                }
            }
    }
    if (id == 0x1204) {
        /* with text that does not parse, .text+0x3320 stores what ecx held on entry: nothing
         * to compare, so applied, this one parses */
        char *t = *(char **)(uintptr_t)(desc + 0xc4);
        if (t != NULL && !IsBadWritePtr(t, 8))
            snprintf(t, 8, "%d", (int)rnd_n(4000) - 2000);
    }
    if (counting && *(uint32_t *)(uintptr_t)desc == 1) cover_dlg_ok++;
    return 0;
}
static uint32_t STDCALL m24_18(void) { put(" m24+0x18"); return 0; }
static uint32_t STDCALL m24_1c(void) { put(" m24+0x1c"); return 0; }

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

static int util88_all;                             /* log every field of the descriptor */
static uint32_t STDCALL util_88(uint32_t buf, uint32_t desc, uint32_t n)
{
    uint32_t dest = *(uint32_t *)(uintptr_t)(desc + 12), i, h = 2166136261u;

    if (util88_all)
        for (i = 0; i < n; i++) {
            put(" f%u,%u,%u,", *(uint32_t *)(uintptr_t)(desc + i * 16), *(uint32_t *)(uintptr_t)(desc + i * 16 + 4),
                *(uint32_t *)(uintptr_t)(desc + i * 16 + 8));
            put_ptr(*(uint32_t *)(uintptr_t)(desc + i * 16 + 12));
        }
    i = 0;
    const uint8_t *b = (const uint8_t *)(uintptr_t)buf;
    uint8_t *d = (uint8_t *)(uintptr_t)dest;

    for (i = 0; i < (util88_all && n == 4 ? 0x38u : 0x58u); i++) {  /* the ADF's fields stop short of a relocated word */
        d[i] = (uint8_t)(b[i] + 0x11 + i);
        h = (h ^ d[i]) * 16777619u;
    }
    if (util88_all)                                 /* the real one writes only the fields: a text ends in the gap after it */
        for (i = 0; i < n; i++) {
            uint32_t count = *(uint32_t *)(uintptr_t)(desc + i * 16), end = *(uint32_t *)(uintptr_t)(desc + i * 16 + 12) + count - dest;

            if (count > 1 && *(uint32_t *)(uintptr_t)(desc + i * 16 + 4) == 0 && end < 0x58)
                d[end] = 0;
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
    for (i = 0; i < 48; i++) {
        uint32_t j;
        for (j = 4; j < sizeof module_stub[i]; j += 4)
            *(uint32_t *)(module_stub[i] + j) = 0xde000000u | (i << 16) | j;
        *(uint32_t *)module_stub[i] = i;
    }
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
    SET_SLOT(host, 0x018, h_frame_services);
    SET_SLOT(host, 0x01c, h_one);
    SET_SLOT(host, 0x298, h_push_module);
    SET_SLOT(host, 0x29c, h_pop_module);
    SET_SLOT(host, 0x134, h_attr_get);
    SET_SLOT(host, 0x138, h_attr_set);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x20, chain_send);
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
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
    SET_SLOT(util, 0x6c, util_rand);
    *(uint32_t *)fe = 18;
    SET_SLOT(fe, 0x20, fe_20);
    SET_SLOT(fe, 0x2c, fe_2c);
    SET_SLOT(fe, 0x54, fe_54);
    SET_SLOT(fe, 0x60, fe_60);
    SET_SLOT(fe, 0x64, fe_64);
    SET_SLOT(fe, 0x68, fe_68);
    SET_SLOT(fe, 0x6c, fe_6c);
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
    SET_SLOT(hg2d, 0x7c, hg2d_7c);
    SET_SLOT(hg2d, 0x18, step_hg2d_18);
    SET_SLOT(g3d, 0x60, step_g3d_60);
    SET_SLOT(sound, 0x20, step_sound_20);
    SET_SLOT(weatherm, 0x1c, step_weatherm_1c);
    SET_SLOT(util, 0x70, step_util_70);
    SET_SLOT(host, 0x034, h_clock);
    SET_SLOT(module_stub[21], 0x28, cmd_advdrv_28);
    SET_SLOT(module_stub[21], 0x2c, cmd_advdrv_2c);
    SET_SLOT(multip, 0x18, cmd_multip_18);
    SET_SLOT(multip, 0x1c, cmd_multip_1c);
    SET_SLOT(module_stub[37], 0x24, cmd_facil_24);
    SET_SLOT(module_stub[37], 0x28, cmd_facil_28);
    SET_SLOT(module_stub[37], 0x30, cmd_facil_30);
    SET_SLOT(fe, 0x78, cmd_fe_78);
    SET_SLOT(module_stub[7], 0x2c, cmd_demo_2c);
    SET_SLOT(module_stub[7], 0x30, cmd_demo_30);
    SET_SLOT(module_stub[7], 0x34, cmd_demo_34);
    SET_SLOT(module_stub[29], 0x34, cmd_analysis_34);
    SET_SLOT(controls, 0x58, cmd_controls_58);
    SET_SLOT(module_stub[15], 0x20, cmd_panels_20);
    SET_SLOT(module_stub[15], 0x24, cmd_panels_24);
    SET_SLOT(module_stub[10], 0x40, cmd_window_40);
    SET_SLOT(module_stub[10], 0x48, cmd_window_48);
    SET_SLOT(module_stub[10], 0x50, cmd_window_50);
    SET_SLOT(module_stub[10], 0x54, cmd_window_54);
    SET_SLOT(module_stub[10], 0x58, cmd_window_58);
    SET_SLOT(module_stub[10], 0x5c, cmd_window_5c);
    SET_SLOT(module_stub[10], 0x68, cmd_window_68);
    SET_SLOT(module_stub[10], 0x6c, cmd_window_6c);
    SET_SLOT(module_stub[10], 0x8c, cmd_window_8c);
    SET_SLOT(host, 0x10c, h_dialog);
    SET_SLOT(host, 0x188, h_split);
    SET_SLOT(host, 0x174, h_tempname);
    SET_SLOT(host, 0x14c, h_delete);
    SET_SLOT(host, 0x16c, h_rename);
    SET_SLOT(host, 0x170, h_pathlen);
    SET_SLOT(host, 0x178, h_nameok);
    SET_SLOT(host, 0x130, h_cwd);
    SET_SLOT(host, 0x150, h_free_space);
    SET_SLOT(host, 0x2bc, h_strcmp);
    SET_SLOT(host, 0x2c0, h_stricmp);
    SET_SLOT(util, 0x98, util_list);
    SET_SLOT(module_stub[24], 0x18, m24_18);
    SET_SLOT(module_stub[24], 0x1c, m24_1c);
    SET_SLOT(host, 0x048, cmd_host_48);
    SET_SLOT(host, 0x07c, cmd_host_7c);
    SET_SLOT(module_stub[6], 0x18, step_m6_18);
    SET_SLOT(module_stub[7], 0x18, step_m7_18);
    SET_SLOT(module_stub[9], 0x18, step_m9_18);
    SET_SLOT(module_stub[10], 0x1c, step_m10_1c);
    SET_SLOT(module_stub[26], 0x18, step_m26_18);
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
static uint8_t *numeric_real;

/* Retail NUMERIC for the exchange engine's text fields (UTIL +0x8c/+0x90): it
 * formats and parses, verified by numericdiff.  It keeps only what its init
 * sets, once, here.  Its UTIL is retail UTIL's own Linkage. */
static void use_retail_numeric(void)
{
    HMODULE module = LoadLibraryA("NUMERIC_retail.dll"), um = LoadLibraryA("UTIL_retail.dll");
    struct import *im;
    struct side *saved = cur;

    if (module == NULL || um == NULL) {
        printf("cannot load NUMERIC_retail.dll or UTIL_retail.dll (%lu)\n", GetLastError());
        exit(2);
    }
    numeric_real = (uint8_t *)GetProcAddress(module, "Linkage");
    for (im = (struct import *)GetProcAddress(module, "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host : im->id == 2 ? (void *)global :
                   (void *)GetProcAddress(um, "Linkage");
    cur = NULL;
    ((s0)*(void **)(numeric_real + 4))();
    cur = saved;
}

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
        case 17: im->link = numeric_real != NULL ? (void *)numeric_real : (void *)numeric_stub; break;
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
                   im->id < 48 ? (void *)module_stub[im->id] : (void *)stub_module;
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

static int handles_differ;
static int wx_norm;

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
        else if (v == *(uint32_t *)(cur->pristine + at))
            v = 0xc0de0000u;                       /* a code pointer as loaded: ours installs its own */
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
                                        0x3470, 0x35da, 0x7862, 0x7f54, 0x7f58, 0x7f5c, 0x7f60, 0xd128 };
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
    memset(img + 0x2ede4, 0, 0x10);                /* the Select Aircraft dialog's callbacks */
    if (wx_norm) {                                 /* the weather check's limits, named in the layers' records */
        uint32_t j, v;
        for (j = 0x7b40; j < 0x7f44; j += 4) {
            memcpy(&v, img + j, 4);
            if (in_image(v)) {
                v = 0xda000000u | (v - (uint32_t)(uintptr_t)cur->image);
                memcpy(img + j, &v, 4);
            }
        }
    }
    {                                              /* the Preferences dialog's copy of its descriptor */
        static const uint16_t code[] = { 0x950c, 0x9514, 0x951c, 0x9520, 0x95cc, 0x95dc };
        uint32_t j, v;
        for (j = 0; j < sizeof code / sizeof code[0]; j++)
            memset(img + code[j], 0, 4);
        memcpy(&v, img + 0x95d0, 4);
        if (in_image(v)) {
            v = 0xda000000u | (v - (uint32_t)(uintptr_t)cur->image);
            memcpy(img + 0x95d0, &v, 4);
        }
    }
    if (handles_differ) {                          /* the module's instance and its loaded bitmap */
        memset(img + 0x7fc0, 0, 4);
        memset(img + 0xdc84, 0, 4);
        {                                          /* the close box's routine: its own, retail's or ours */
            uint32_t v = *(uint32_t *)(img + 0xdc24);
            *(uint32_t *)(img + 0xdc24) = (uint8_t *)(uintptr_t)v >= cur->lo && (uint8_t *)(uintptr_t)v < cur->hi ? 0xc0de0001u : v;
        }
    }
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
    if (getenv("MAIN_GLOBAL")) {
        put("\n  G:");
        for (i = 0; i + 4 <= sizeof global; i += 4)
            if (*(uint32_t *)(global + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(global + i));
    }
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
    case 0x900a: return "FLIGHTS\\CHALLENGE";
    case 0x9016: return "start.stn";
    case 0x9006: return "TEXTURE";
    case 0x9100: return "N S E W";
    case 0xf461: return "No default flight";
    case 0xf462: return "Flight will not load";
    case 0xf4b0: return "Logbook";
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

static void setup_navdesc(void) { util88_all = 1; }

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
    if (rnd_n(8) == 0)                             /* +0x741f's +0x10 carries into +0x566 */
        *(uint16_t *)(global + 0x564) = (uint16_t)(0xffe8 + rnd_n(24));

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
         * thresholds and their neighbours on both sides: near above a layer
         * (0x34, 0x1a; 4, 2 with +0xe82 set), close below one (6, 3) */
        static const int16_t off[] = { -1, 0, 1, -1500, 1500, -1499, 1499, -1501, 1501,
                                       0x34, 0x35, 0x33, 0x1a, 0x1b, 0x19, 4, 5, 3, 2, 6, 7,
                                       -0x34, -0x35, -0x33, -0x1a, -0x1b, -0x19, -4, -5, -3, -2, -6, -7 };
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

/* ---- suite: frame (init, the frame callbacks and deinit: +0x04, +0x08) ---------- */

static int util48_zero;                          /* UTIL +0x48 answers 0 this op (frame) */
static unsigned long cover_frame_init, cover_frame_deinit, cover_frame_cb[2], cover_frame_scatter,
    cover_frame_recentred, cover_frame_moved;

/* Random bytes over an image range, leaving relocated words alone (a random
 * value there that pointed into retail's image would compare as a pointer on
 * that side only). */
static void seed_image(uint32_t lo, uint32_t hi)
{
    uint32_t i, r;

    for (i = lo; i < hi; i++) {
        uint8_t v = (uint8_t)rnd();
        for (r = 0; r < nrelocs; r++)
            if (relocs[r] <= i && i < relocs[r] + 4)
                break;
        if (r == nrelocs)
            cur->image[i] = v;
    }
}

/* A latitude inside slice 11's clamp: UTIL +0x44 and +0x48 index their
 * tables by it. */
static int32_t frame_lat(void)
{
    return (int32_t)(rnd() % (0x97c449u + 0x97c449u + 1)) - 0x97c449;
}

/* GLOBAL's clouds, winds and positions, afresh or (`some`) in part. */
static void frame_global(int some)
{
    uint32_t i;

    if (!some || rnd_n(2))                         /* the three wind bands */
        for (i = 0xeca; i < 0xeea; i += 2)
            *(uint16_t *)(global + i) = (uint16_t)rnd();
    if (!some || rnd_n(2)) {                       /* the layers and the puffs */
        *(uint16_t *)(global + 0xe6a) = (uint16_t)rnd();
        *(uint16_t *)(global + 0xe72) = (uint16_t)rnd();
        *(uint16_t *)(global + 0xe70) = (uint16_t)(rnd_n(4) ? rnd() | 1 : 0);
        *(uint16_t *)(global + 0xe78) = (uint16_t)(rnd_n(3) ? rnd() | 1 : 0);
        *(uint16_t *)(global + 0xe7a) = (uint16_t)(rnd_n(6) ? rnd() | 1 : 0);
        *(uint16_t *)(global + 0xe7c) = (uint16_t)rnd();
        *(uint16_t *)(global + 0xe7e) = (uint16_t)(rnd_n(6) ? rnd() | 1 : 0);
        *(uint16_t *)(global + 0xe80) = (uint16_t)rnd();
    }
    if (!some || rnd_n(2)) {                       /* the aircraft and its cell */
        for (i = 0x560; i < 0x578; i++)
            global[i] = (uint8_t)rnd();
        *(int32_t *)(global + 0x564) = frame_lat();
        for (i = 0xb38; i < 0xb48; i++)
            global[i] = (uint8_t)rnd();
        *(int32_t *)(global + 0xb3c) = frame_lat();
    }
    /* now and then a layer's or the puffs' altitude on a wind band's edge,
     * or one either side (+0x8da5's unsigned range tests) */
    for (i = 0; i < 3; i++)
        if (rnd_n(2)) {
            static const uint16_t alt[] = { 0xe6a, 0xe72, 0xe7a }, edge[] = { 0xeca, 0xecc, 0xed6, 0xed8, 0xee2, 0xee4 };
            *(uint16_t *)(global + alt[i]) =
                (uint16_t)(*(uint16_t *)(global + edge[rnd_n(6)]) + rnd_n(3) - 1);
        }
    *(uint16_t *)(global + 0xc1a) = (uint16_t)rnd();
    *(uint16_t *)(global + 0x2aa) = (uint16_t)rnd();
    *(uint16_t *)(global + 0x2ea) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x390) = rnd_n(4);
    *(uint16_t *)(global + 0xe44) = (uint16_t)rnd();   /* list 1's callback clears it */
    *(uint16_t *)(global + 0x116c) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x20) = rnd();
}

/* MAIN's own cloud state, afresh or in part. */
static void frame_image(int some)
{
    uint32_t i;

    if (!some || rnd_n(3) == 0) {
        seed_image(0x1a80, 0x1ad0);
        seed_image(0x1ab0, 0x1ac8);
        seed_image(0x1afe, 0x1b24);
        seed_image(0x1b2e, 0x1ce0);
        seed_image(0x1e44, 0x1ed8);
        *(int32_t *)(cur->image + 0x1b18) = frame_lat();
        *(int32_t *)(cur->image + 0x1b00) = frame_lat();   /* the centre +0x80cd re-centres from */
        *(uint16_t *)(cur->image + 0x1e72) = (uint16_t)rnd_n(2);
    }
    *(uint16_t *)(cur->image + 0x1e46) = (uint16_t)(rnd_n(3) ? 0 : rnd_n(4));
    if (rnd_n(2) == 0)
        *(uint16_t *)(cur->image + 0x1e44) = 1;   /* scatter the puffs afresh */
    for (i = 0; i < 16; i++)                       /* the puffs the drawing has released */
        *(uint16_t *)(cur->image + 0x3380 + i * 2) = (uint16_t)(rnd_n(3) ? 0 : 1);
}

static void op_frame(void)
{
    s2 cb;
    uint32_t k = rnd_n(10), which;
    int side = cur == &ours;

    if (op_number == 0) {
        frame_cb[side][0] = frame_cb[side][1] = 0;
        record_cb[side] = 0;
        frame_global(0);
        frame_image(0);
        if (counting) cover_frame_init++;
        put("\n#init");
        put(" =%#x", ((s0)SVC(0x04))() == GetUserDefaultLCID());
        return;
    }
    if (k == 0 && op_number > 3 && (frame_cb[side][0] || frame_cb[side][1])) {
        if (counting) cover_frame_deinit++;
        put("\n#deinit");
        put(" =%#x", ((s0)SVC(0x08))());
        return;
    }
    if (k == 3 && record_cb[side] != 0) {          /* list 0x14's listener, a record */
        put("\n#record");
        call_with_regs(record_cb[side], 0x22, rnd());
        put(" eax=%#x ecx=%#x edx=%#x esp%+d", cb_eax, cb_ecx, cb_edx, (int32_t)cb_esp);
        return;
    }
    if (k < 4) {
        frame_global(1);
        frame_image(1);
        put("\n#change");
        return;
    }
    which = k < 7 ? 0 : 1;
    cb = (s2)(uintptr_t)frame_cb[side][which];
    if (cb == NULL)
        return;
    if (counting) {
        cover_frame_cb[which]++;
        if (which == 0 && *(uint16_t *)(cur->image + 0x1e44) && *(uint16_t *)(global + 0xe7a) &&
            *(uint16_t *)(global + 0xe7e) && *(uint16_t *)(cur->image + 0x1e72))
            cover_frame_scatter++;
    }
    util48_zero = which == 1 && rnd_n(6) == 0;
    if (which == 1 && rnd_n(4) == 0) {
        /* each layer's origin one cell short of half a turn from the latitude
         * the grid will step it to (the FE model's cell edge), so the step
         * meets a distance of 0x80000000, where +0x8887's signed test and an
         * unsigned one part */
        int32_t t = (*(int32_t *)(global + 0x564) / 0x2000) * 0x2000;
        *(uint32_t *)(cur->image + 0x1a84) = (uint32_t)t + 0x80000000u - 0x1000000u / 0x258;
        *(uint32_t *)(cur->image + 0x1a9c) = (uint32_t)t + 0x80000000u - 0x1000000u / 0x320;
        *(uint16_t *)(global + 0x116c) &= 0xfffe;
    }
    put("\n#list%u", which ? 3 : 1);
    cb(rnd(), rnd());
    if (counting) {
        if (which == 1 && *(uint16_t *)(cur->image + 0x2080)) cover_frame_recentred++;
        if (which == 1 && *(uint16_t *)(cur->image + 0x1ab2) == *(uint16_t *)(global + 0xb46))
            cover_frame_moved++;
    }
}

/* UTIL +0x48 for the frame suite: retail's, except that now and then it
 * answers a planar size of 0 -- which real latitudes never give -- so that
 * +0x8609's `idiv` by it, a memory operand, meets the host's divide repair.
 * Its one caller reloads every register it reads afterwards. */
static uint32_t retail_util48;
static uint32_t STDCALL util48_mix(uint32_t a, uint32_t b, uint32_t out)
{
    if (!util48_zero)
        return ((s3)(uintptr_t)retail_util48)(a, b, out);
    put(" U48=0");
    *(uint32_t *)(uintptr_t)(out + 4) = 0;
    return 0;
}

static void setup_frame(void)
{
    static const uint32_t slots[] = { 0x2c, 0x44, 0x48 };
    static int loaded;

    if (!loaded) {
        use_retail_util(slots, sizeof slots / sizeof slots[0]);
        retail_util48 = *(uint32_t *)(util + 0x48);
        SET_SLOT(util, 0x48, util48_mix);
        loaded = 1;
    }
}

static void report_frame(void)
{
    printf("  frame: inits %lu, deinits %lu, list 1 %lu (puffs scattered %lu), list 3 %lu (puffs re-centred %lu);"
           " memory-operand divides repaired %lu\n",
           cover_frame_init, cover_frame_deinit, cover_frame_cb[0], cover_frame_scatter, cover_frame_cb[1],
           cover_frame_recentred, cover_div_memory);
    (void)cover_frame_moved;
}

/* ---- suite: tick (the timer tick, +0x24) ----------------------------------------- */

static unsigned long cover_tick, cover_tick_carry, cover_tick_third, cover_tick_sixth, cover_tick_flag;

/* Each op seeds the tick's GLOBAL fields (the fraction now and then just
 * short of its carry) and now and then the three countdowns (1, small, or 0,
 * which wraps), then ticks a few times. */
static void op_tick(void)
{
    uint32_t i, n = 1 + rnd_n(8);
    static const uint32_t cd[] = { 0x6dc0, 0x6dc4, 0x6dc8 };

    if (op_number == 0 || rnd_n(3) == 0)
        for (i = 0; i < 3; i++)
            *(uint32_t *)(cur->image + cd[i]) = rnd_n(4) == 0 ? 0 : rnd_n(3) == 0 ? 1 : 1 + rnd_n(20);
    *(uint32_t *)(global + 0x24c) = rnd();
    *(uint32_t *)(global + 0x250) = rnd();
    *(uint32_t *)(global + 0x254) = rnd_n(2) ? 0 : rnd();
    *(uint16_t *)(global + 0x25e) = (uint16_t)(rnd_n(2) ? 0x10000 - 0xe38 + rnd_n(5) - 2 : rnd());
    *(uint32_t *)(global + 0x260) = rnd();
    for (i = 0x1176; i < 0x1180; i += 2)
        *(uint16_t *)(global + i) = (uint16_t)rnd();
    put("\n#tick x%u", n);
    for (i = 0; i < n; i++) {
        uint16_t f = *(uint16_t *)(global + 0x25e);
        uint32_t c3 = *(uint32_t *)(cur->image + 0x6dc0), c6 = *(uint32_t *)(cur->image + 0x6dc4),
                 c18 = *(uint32_t *)(cur->image + 0x6dc8);
        ((s0)SVC(0x24))();
        if (counting) {
            cover_tick++;
            if ((uint16_t)(f + 0xe38) < 0xe38) cover_tick_carry++;
            if (c3 == 1) cover_tick_third++;
            if (c6 == 1) cover_tick_sixth++;
            if (c18 == 1) cover_tick_flag++;
        }
    }
}

static void setup_tick(void) { }

static void report_tick(void)
{
    printf("  tick %lu (carries %lu, every third %lu, every sixth %lu, every eighteenth %lu)\n",
           cover_tick, cover_tick_carry, cover_tick_third, cover_tick_sixth, cover_tick_flag);
}

/* ---- suite: step (the frame, +0x18) ---------------------------------------------- */

static unsigned long cover_step, cover_step_events, cover_step_mode[3], cover_step_slew, cover_step_drain,
    cover_step_slow;

/* Each op seeds the fields the frame branches on -- the tick's counts and
 * flags, the event count's low bits, the mode and its last state, the slew
 * countdown and what is pending, the crash and the replay flags, the second
 * marker -- and the frame time the host's clock gives (around one second,
 * where it saturates), then runs one frame. */
static void op_step(void)
{
    uint32_t mode;

    *(uint32_t *)(global + 0x254) = rnd_n(2) ? 0 : rnd();
    *(uint32_t *)(global + 0x258) = rnd();
    *(uint32_t *)(global + 0x794) = rnd_n(2) ? 0 : rnd();
    global[0xb6a] = (uint8_t)(rnd_n(2) ? 0 : rnd());
    global[0x281] = (uint8_t)rnd();
    global[0x282] = (uint8_t)rnd();
    *(uint32_t *)(global + 0x84c) = rnd_n(2) ? 0 : rnd();
    *(uint32_t *)(global + 0x850) = rnd_n(2) ? 0 : rnd();
    global[0x365] = (uint8_t)(rnd_n(3) ? rnd_n(4) : rnd());
    if (rnd_n(2))
        *(uint32_t *)(cur->image + 0x85b8) = *(uint32_t *)(global + 0x2ac);
    global[0x838] = (uint8_t)(rnd_n(3) ? 0 : rnd());
    *(uint32_t *)(global + 0x11f0) = rnd_n(3) ? rnd_n(2) : rnd();
    *(uint32_t *)(global + 0x11f4) = rnd_n(3) ? rnd_n(2) : rnd();
    *(uint16_t *)(global + 0x11fa) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x11fc) = rnd();
    *(uint32_t *)(global + 0x1200) = rnd();
    *(uint16_t *)(global + 0x2a0) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x758) = rnd_n(3) ? 0 : rnd();
    mode = rnd_n(4) ? rnd_n(3) : rnd();
    *(uint32_t *)(global + 0x264) = mode;
    if (rnd_n(2))
        *(uint32_t *)(global + 0x268) = rnd_n(2);
    *(uint32_t *)(global + 0xb20) = rnd_n(2) ? 0 : rnd();
    *(uint16_t *)(global + 0x5dc) = (uint16_t)(rnd_n(2) ? 0 : rnd());
    *(uint32_t *)(global + 0x628) = rnd_n(3) ? 0 : rnd();
    *(uint32_t *)(global + 0x26c) = rnd();
    if (rnd_n(2))
        *(uint32_t *)(global + 0x260) = rnd();
    *(uint16_t *)(global + 0x1206) = (uint16_t)(rnd_n(2) ? 0 : rnd());
    *(uint32_t *)(global + 0x24c) = rnd_n(3) ? rnd_n(4) : 0;
    *(uint32_t *)(global + 0x250) = rnd_n(3) ? rnd_n(3) : 0;
    if (rnd_n(4) == 0)
        *(uint32_t *)(cur->image + 0x85b0) = 0;
    *(int32_t *)(global + 0x564) = (int32_t)rnd();
    frame_ms = rnd_n(2) ? 998 + rnd_n(4) : rnd_n(3) ? rnd_n(2000) : rnd();
    if (counting) {
        cover_step++;
        if (*(uint32_t *)(global + 0x254)) cover_step_events++;
        if (mode < 3) cover_step_mode[mode]++;
        if (global[0x365] > 1 && *(uint32_t *)(cur->image + 0x85b8) != *(uint32_t *)(global + 0x2ac)) cover_step_slew++;
        if (*(uint32_t *)(global + 0x24c) || *(uint32_t *)(global + 0x250)) cover_step_drain++;
        if (frame_ms >= 1000) cover_step_slow++;
    }
    put("\n#step mode=%u 264=%u 268=%u 758=%#x", mode, *(uint32_t *)(global + 0x264),
        *(uint32_t *)(global + 0x268), *(uint32_t *)(global + 0x758));
    put(" =%#x", ((s0)SVC(0x18))());
}

static void setup_step(void) { }

static void report_step(void)
{
    printf("  step %lu (events %lu, modes 0/1/2 %lu/%lu/%lu, slew catch-ups due %lu, ticks drained %lu,"
           " a second or more %lu)\n", cover_step, cover_step_events, cover_step_mode[0], cover_step_mode[1],
           cover_step_mode[2], cover_step_slew, cover_step_drain, cover_step_slow);
}

/* ---- suite: commands (the menu command dispatcher, +0x20) ------------------------- */

/* The commands whose handlers are done, and the rest of the dispatcher's
 * ranges.  Not issued, their handlers still to come (cmd_pending in
 * main_commands.inc): 0x1101, 0x1102, 0x1106, 0x1109, 0x1201..0x1205, 0x1301,
 * 0x1306..0x1309, 0x1407, 0x1409, 0x1502, 0x1607 and 0x300..0x3ff. */
static const uint32_t cmd_list[] = {
    0x1103, 0x1104, 0x1105, 0x1107, 0x1180, 0x1181, 0x1206, 0x1207, 0x1303, 0x1304, 0x1305, 0x130a,
    0x1401, 0x1403, 0x1404, 0x1405, 0x1406, 0x1408, 0x140a, 0x1421, 0x1422, 0x1423, 0x1424, 0x1425,
    0x1426, 0x1427, 0x1428, 0x1429, 0x142a, 0x1501, 0x1506, 0x1507, 0x1508, 0x1520, 0x1540, 0x1541,
    0x1542, 0x1543, 0x1544, 0x1545, 0x1550, 0x1551, 0x1552, 0x1553, 0x1554, 0x1555, 0x1625,
    0x20000, 0x20001, 0x20002, 0x20003, 0x20004, 0x20005, 0x20006, 0x20007, 0x20008, 0x20009,
    0x2000a, 0x2000b, 0x2000c, 0x2000d, 0x2000e, 0x2000f, 0x20010, 0x20011, 0x20012, 0x20013, 0x20014,
};
/* Commands on the edges of the ranges, which must fall to the default. */
static const uint32_t cmd_edges[] = {
    0, 0xff, 0x400, 0x1100, 0x1108, 0x110a, 0x117f, 0x1182, 0x1200, 0x1208, 0x1300, 0x1302, 0x130b,
    0x1400, 0x1402, 0x140b, 0x1420, 0x142b, 0x1500, 0x1503, 0x1505, 0x1509, 0x151f, 0x1521, 0x153f,
    0x1546, 0x154f, 0x1556, 0x1606, 0x1608, 0x1624, 0x1626, 0x1fff, 0x1ffff, 0x20015, 0x7ffff,
    0x80000, 0x80001, 0x80002, 0x80004, 0x80006, 0x8ffff, 0x90000,
};
static unsigned long cover_cmd, cover_cmd_list, cover_cmd_edge, cover_cmd_window, cover_cmd_panel, cover_cmd_reload;

static void op_commands(void)
{
    uint8_t *rec = fixtures + 0x21000;
    uint32_t k = rnd_n(20), command, argument, r;

    if (k < 12)
        command = cmd_list[rnd_n(sizeof cmd_list / sizeof cmd_list[0])];
    else if (k < 15)
        command = cmd_edges[rnd_n(sizeof cmd_edges / sizeof cmd_edges[0])];
    else if (k < 17)
        command = 0x100 + rnd_n(0x100);
    else if (k < 19)
        command = 0x200 + rnd_n(0x100);
    else
        command = rnd_n(8) ? rnd() : 0x80005;
    if (command >= 0x300 && command < 0x400)
        command = 0x1103;                          /* the kneeboard pages, not done yet */
    if (command == 0x1101 || command == 0x1102 || command == 0x1106 || command == 0x1109 ||
        (command >= 0x1201 && command <= 0x1205) || command == 0x1301 ||
        (command >= 0x1306 && command <= 0x1309) || command == 0x1407 || command == 0x1409 ||
        command == 0x1502 || command == 0x1607)
        command = 0x1104;                          /* a random one that landed on a pending handler */
    *(uint32_t *)(rec + 4) = rnd_n(4);
    *(uint32_t *)(rec + 8) = rnd();
    argument = (uint32_t)(uintptr_t)rec;
    if ((command == 0x2000f || command == 0x20011) && rnd_n(2))
        argument = 0;                              /* these two test it */
    cmd_record_now = rnd_n(4) ? (uint32_t)(uintptr_t)rec : 0;
    *(uint32_t *)(global + 0x760) = rnd_n(2) ? 0 : rnd();
    *(uint32_t *)(global + 0x1240) = rnd_n(2) ? 0 : rnd();
    *(uint32_t *)(global + 0x119c) = rnd();
    *(uint32_t *)(global + 0x2ac) = rnd_n(2) ? 0 : rnd();
    *(uint32_t *)(global + 0x1230) = rnd_n(2) ? 0 : rnd();
    *(uint32_t *)(global + 0x254) = rnd();
    *(uint32_t *)(global + 0x250) = rnd();
    *(uint32_t *)(global + 0x24c) = rnd();
    *(uint32_t *)(cur->image + 0x7fc4) = (uint32_t)(uintptr_t)rec;
    if (counting) {
        cover_cmd++;
        if (command < 0x200 && command >= 0x100) cover_cmd_window++;
        else if (command < 0x300 && command >= 0x200) cover_cmd_panel++;
        else if (command == 0x80005) cover_cmd_reload++;
        else if (k < 12) cover_cmd_list++;
        else cover_cmd_edge++;
    }
    put("\n#command(%#x,", command);
    put_ptr(argument);
    put(")");
    r = ((s2)SVC(0x20))(command, argument);
    put(" =%#x c1a=%#x rec8=%#x", r, *(uint16_t *)(global + 0xc1a), *(uint32_t *)(rec + 8));
}

static void setup_commands(void)
{
    setup_situations();
}

static void report_commands(void)
{
    printf("  commands %lu (listed %lu, others and edges %lu, windows %lu, panels %lu, reloads %lu)\n",
           cover_cmd, cover_cmd_list, cover_cmd_edge, cover_cmd_window, cover_cmd_panel, cover_cmd_reload);
}

/* ---- suite: dialogs (the menu commands' dialogs, through +0x20) -------------------- */

static const uint32_t dialog_commands[] = { 0x1308, 0x1309, 0x1607, 0x1203, 0x1301, 0x1205, 0x1204, 0x1307, 0x1409, 0x1502, 0x1407, 0x1306, 0x1202, 0x1101 };
static unsigned long cover_dlg_cmd[14];

static void run_dialog(uint32_t k);
static void op_dialogs(void) { run_dialog(rnd_n(4)); }
static void op_clockdlg(void) { run_dialog(4); }
static void op_engines(void) { run_dialog(5); }
static void op_limits(void) { run_dialog(6); }
static void op_areas(void) { run_dialog(7); }
static void op_prefdlg(void) { run_dialog(8); }
static void op_viewdlg(void) { run_dialog(9); }
static void op_logbook(void);
static void op_wxdlg(void) { run_dialog(11); }
static void op_radiodlg(void) { run_dialog(12); }
static void op_create(void);

static void run_dialog(uint32_t k)
{
    uint32_t i, r;

    for (i = 0; i < 32; i++)
        global[rnd_n(sizeof global)] = (uint8_t)rnd();
    for (i = 0; i < 3; i++) {                      /* the fields the tables name, in range now and then */
        static const uint16_t f[] = { 0xb54, 0xb58, 0xb5c, 0xb60, 0x1180, 0x1184, 0x1188, 0x118c, 0x1190,
                                      0x34e, 0x27c, 0x38a, 0x362 };
        *(uint32_t *)(global + f[rnd_n(sizeof f / sizeof f[0])]) = rnd_n(4);
    }
    if (dialog_commands[k] == 0x1202) {            /* the radios' switches, off now and then */
        *(uint16_t *)(global + 0x37e) = (uint16_t)rnd();
        *(uint32_t *)(global + 0x7a0) = rnd_n(2);
        *(uint32_t *)(global + 0x7a4) = rnd_n(2);
        *(uint32_t *)(global + 0x35c) = rnd_n(2);
        global[0xc58] = (uint8_t)rnd_n(2);
        global[0xc68] = (uint8_t)rnd_n(2);
        global[0xb70] = (uint8_t)rnd_n(2);
    }
    if ((dialog_commands[k] == 0x1301 || dialog_commands[k] == 0x1101) && rnd_n(8)) {  /* a date and a light it can open on */
        static const uint16_t light[] = { 1, 2, 4, 3 };
        *(uint16_t *)(global + 0x23e) = (uint16_t)(rnd_n(4) ? 1 + rnd_n(366) : rnd_n(0x2e0));
        *(uint16_t *)(global + 0x240) = (uint16_t)(rnd_n(3) ? 1990 + rnd_n(16) : rnd_n(3) ? 2000 : 1900);
        *(uint16_t *)(global + 0x116e) = light[rnd_n(16) ? rnd_n(3) : 3];
        global[0x238] = (uint8_t)rnd_n(24);
        global[0x239] = (uint8_t)rnd_n(60);
        global[0x23b] = (uint8_t)rnd_n(24);
        global[0x23c] = (uint8_t)rnd_n(60);
        *(int16_t *)(global + 0x246) = (int16_t)((int)rnd_n(1500) - 750);
        *(uint32_t *)(cur->image + 0x7874) = rnd_n(2);  /* a `d' record seen: +0x74's first argument counts */
    }
    if (dialog_commands[k] == 0x1202) {            /* tuned in range, the markers' records in place */
        static const uint16_t words[] = { 0xc58, 0xc68 };
        uint32_t j;
        uint32_t *m = (uint32_t *)(fixtures + 0x62000);

        for (j = 0; j < 2; j++)
            global[words[j]] = (uint8_t)rnd_n(2);
        {   /* frequencies NUMERIC reads back: refused, .text+0x14840 looks up what its stack held */
            static const uint16_t bcd[] = { 0x1110, 0x1170, 0x1350, 0x800, 0x1795, 0x1000 };
            *(uint16_t *)(global + 0x350) = bcd[rnd_n(6)];
            *(uint16_t *)(global + 0x352) = bcd[rnd_n(6)];
        }
        *(uint32_t *)(global + 0x35c) = rnd_n(2);
        /* in BCD, or the load stops there */
        *(uint16_t *)(global + 0x34c) = (uint16_t)(rnd_n(8) ? (rnd_n(10) << 8) | (rnd_n(10) << 4) | rnd_n(10) : rnd());
        *(uint16_t *)(global + 0x356) = (uint16_t)(rnd_n(8) ? 0x100 * rnd_n(2) + rnd_n(10) : rnd());
        *(uint16_t *)(global + 0xc4e) = (uint16_t)rnd();
        *(uint16_t *)(global + 0xc5e) = (uint16_t)rnd();
        global[0xb64] = (uint8_t)rnd_n(2);
        global[0xb6f] = (uint8_t)rnd_n(2);
        global[0xb70] = (uint8_t)rnd_n(2);
        for (j = 0; j < 3; j++) {
            uint8_t *rec = fixtures + 0x62100 + j * 0x80;
            m[j] = (uint32_t)(uintptr_t)rec;
            memset(rec, 0x41 + j, 0x80);
            rec[0x1c] = (uint8_t)rnd();
            *(uint16_t *)(rec + 0x18) = (uint16_t)rnd_n(2);
            rec[0x22 + 5] = 0;
            rec[0x28 + 0x1b] = 0;
            rec[0x1a + 5] = 0;
            rec[0x20 + 0x1b] = 0;
        }
        *(uint32_t *)(global + 0x384) = (uint32_t)(uintptr_t)m;
    }
    if (dialog_commands[k] == 0x1306 || dialog_commands[k] == 0x1101) {  /* the row in range, and either dialog */
        *(uint32_t *)(global + 0x1234) = rnd_n(2);
        *(uint32_t *)(global + 0xf18) = rnd_n(2);
        global[0xf12] = (uint8_t)(rnd_n(4) ? 0 : 1);
        global[0xf13] = (uint8_t)(rnd_n(4) ? 0 : 1);
        *(uint16_t *)(global + 0xc18) = (uint16_t)rnd_n(2);
        /* the visibility, across the summary's roundings and past where it stops giving a number */
        *(uint16_t *)(global + 0xf8c) = (uint16_t)(rnd_n(8) ? rnd_n(450) : 3900 + rnd_n(300));
        for (i = 0xf1c; i < 0xf70; i += 2)
            *(uint16_t *)(global + i) = (uint16_t)(rnd_n(2) ? rnd_n(0x200) : rnd());
        for (i = 0xf1c + 2; i < 0xf1c + 0x12; i += 6)
            *(uint16_t *)(global + i) = (uint16_t)rnd_n(0x4000);
        for (i = 0; i < 7; i++) {                  /* and cloud heights: the same, in the cloud sort */
            static const uint8_t height[7] = { 0x1a, 0x18, 0xe, 0xc, 2, 0, 0x24 };
            *(uint16_t *)(global + 0xf4c + height[i]) = (uint16_t)rnd_n(0x2000);
            *(uint16_t *)(global + 0x1034 + height[i]) = (uint16_t)rnd_n(0x2000);
        }
        if (rnd_n(3) == 0) {                       /* no top cloud to begin with */
            *(uint16_t *)(global + 0xf4c + 0x24) = 0;
            *(uint16_t *)(global + 0x1034 + 0x24) = 0;
        }
        for (i = 0; i < 4; i++) {                  /* cloud cover NUMERIC reads back: refused, .text+0x1a010 compares its stack */
            static const uint8_t cover[4] = { 0x1c, 0x10, 4, 0x26 };
            static const uint16_t words[4] = { 0, 8, 20, 35 };  /* .data+0x79a8: the simple dialog's steps */
            uint16_t v = (uint16_t)(rnd_n(2) ? words[rnd_n(4)] : rnd_n(0xc8));

            *(uint16_t *)(global + 0xf4c + cover[i]) = v;
            *(uint16_t *)(global + 0x1034 + cover[i]) = v;
        }
        for (i = 0; i < 8; i += 2) {               /* temperature layers: heights NUMERIC can read back, or absent;
                                                    * refused, retail's lift would compare what its stack held */
            uint16_t v = (uint16_t)(rnd_n(4) ? rnd_n(0x600) : 0xd8f1);
            *(uint16_t *)(global + 0xf34 + i * 2) = v;
            *(uint16_t *)(global + 0x101c + i * 2) = v;
        }
        if (rnd_n(2)) {                            /* temperatures a hundred apart, in metres so that the text is the value */
            static const int16_t apart[6] = { 100, 99, 101, -100, -99, -101 };
            int32_t d = apart[rnd_n(6)];

            *(uint16_t *)(global + 0xc18) = 2;
            for (i = 0; i < 4; i++) {
                *(uint16_t *)(global + 0xf34 + i * 4) = (uint16_t)(1000 + (int32_t)i * d);
                *(uint16_t *)(global + 0x101c + i * 4) = (uint16_t)(1000 + (int32_t)i * d);
            }
        }
        for (i = 0; i < 3; i++)                    /* a wind's top unset */
            if (rnd_n(3) == 0) {
                *(uint16_t *)(global + 0xf1c + 4 + i * 8) = 0xffff;
                *(uint16_t *)(global + 0x1004 + 4 + i * 8) = 0xffff;
            }
        for (i = 0; i < 3; i++) {                  /* each row's wind types, an index into a table of 11 */
            *(uint16_t *)(global + 0xf7a + i * 6) = (uint16_t)rnd_n(11);
            *(uint16_t *)(global + 0x1062 + i * 6) = (uint16_t)rnd_n(11);
        }
    }
    if (dialog_commands[k] == 0x1409) {            /* a preset, and GLOBAL agreeing with it or not */
        static const uint16_t g[][3] = { { 0x5788, 0x39c, 2 }, { 0x578c, 0x39e, 2 }, { 0x5790, 0x398, 4 }, { 0x5794, 0xe84, 2 }, { 0x5798, 0xe82, 2 },
            { 0x579c, 0x3a4, 4 }, { 0x57a0, 0xdc0, 2 }, { 0x57a4, 0xcd0, 2 }, { 0x57a8, 0x11bc, 2 }, { 0x57ac, 0x11b6, 2 },
            { 0x57b0, 0x11b4, 2 }, { 0x57b4, 0x556, 2 }, { 0x57b8, 0x11b8, 2 } };
        uint32_t p = rnd_n(11), j;

        *(uint32_t *)(global + 0x123c) = rnd_n(4) ? p : 0;
        strcpy((char *)global + 0x24, "Flight");
        strcpy((char *)global + 0x638, "Plane1");
        *(uint32_t *)(global + 0x1238) = rnd_n(2);
        *(uint32_t *)(global + 0x35c) = rnd_n(2);
        *(uint16_t *)(global + 0x34c) = (uint16_t)(rnd_n(2) ? 0x1ff + rnd_n(3) : rnd());
        *(uint32_t *)(cur->image + 0x5cc0 + 0xd0 + 0x8) = 0;
        if (rnd_n(2)) {                            /* the stale preset test: the first differs, the rest agree or nearly */
            uint32_t at = p << 7, miss = rnd_n(2) ? rnd_n(13) : 99;

            *(uint32_t *)(cur->image + 0x5cc0 + 0xd0 + 8) = p;
            for (j = 0; j < 13; j++) {
                uint32_t v = *(uint32_t *)(cur->image + g[j][0] + at) ^ (j == miss);
                if (g[j][2] == 2) {
                    *(uint32_t *)(cur->image + g[j][0] + at) = (uint16_t)v;
                    *(uint16_t *)(global + g[j][1]) = (uint16_t)v;
                } else {
                    *(uint32_t *)(global + g[j][1]) = v;
                }
            }
        }
    }
    if (dialog_commands[k] == 0x1307) {            /* areas present or not, values it can show */
        static const uint16_t words[] = { 0xfb4, 0xfb8, 0x109c, 0x10a0 };
        *(uint16_t *)(global + 0xf90) = (uint16_t)(rnd_n(2) ? 0 : rnd_n(3));
        *(uint16_t *)(global + 0x1078) = (uint16_t)(rnd_n(2) ? 0 : rnd_n(3));
        *(uint32_t *)(global + 0xf18) = rnd_n(4);
        *(uint16_t *)(global + 0xc18) = (uint16_t)rnd_n(2);
        for (i = 0; i < 4; i++)
            *(uint32_t *)(global + words[i]) = rnd_n(0x10000);
        *(uint16_t *)(global + 0xfbe) = (uint16_t)(rnd_n(40) * 132);
        *(uint16_t *)(global + 0x10a6) = (uint16_t)(rnd_n(40) * 132);
        *(uint32_t *)(cur->image + 0x1514) = rnd_n(4);
    }
    if (dialog_commands[k] == 0x1204) {            /* units, and values in a sensible range */
        static const uint16_t flags[] = { 0x768, 0x76c, 0x770, 0x774, 0x7c0, 0x7c8, 0x7d8, 0x7fc, 0x800, 0x7c4, 0x804 };
        *(uint16_t *)(global + 0xc18) = (uint16_t)rnd_n(3);
        *(int32_t *)(global + 0x7f0) = (int32_t)(rnd_n(0x4000) << 10) * (rnd_n(4) ? 1 : -1);
        for (i = 0; i < sizeof flags / sizeof flags[0]; i++)
            *(uint32_t *)(global + flags[i]) = rnd_n(3) ? rnd_n(2) : rnd();
    }
    if (dialog_commands[k] == 0x1205) {            /* engines of a size, on tanks that hold something */
        static const uint16_t blocks[5] = { 0xb84, 0xb7c, 0xb94, 0xb9c, 0xb74 };
        *(int16_t *)(global + 0xaf4) = (int16_t)(rnd_n(8) ? 256 * (1 + rnd_n(200)) : rnd());
        *(uint16_t *)(global + 0xc18) = (uint16_t)(rnd_n(2) * (rnd_n(8) ? 1 : 0x100));
        *(uint16_t *)(global + 0xaec) = (uint16_t)rnd_n(6);
        *(uint32_t *)(global + 0x798) = 2 + rnd_n(3);
        for (i = 0; i < 5; i++) {
            uint32_t *b = (uint32_t *)(global + blocks[i]);
            b[0] = rnd_n(4) ? rnd_n(0x800000) : rnd();
            b[1] = rnd_n(4) ? rnd_n(0x10000) : 0;
        }
    }
    fixture_arg = (uint32_t)(uintptr_t)(fixtures + 0x22000);
    if (counting) cover_dlg_cmd[k]++;
    put("\n#command(%#x)", dialog_commands[k]);
    r = ((s2)SVC(0x20))(dialog_commands[k], rnd());
    put(" =%#x", r);
}

static uint32_t STDCALL ar_alert(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g, uint32_t h)
{
    uint32_t r = rnd_n(4) ? 0x40 : 0x80;

    /* an id, or a text on the stack */
    put(" alert(%#x,%u,%#x,%#x,%u,%u,%u,%u)->%#x", a, b, c < 0x10000 ? c : 0xbad, d < 0x10000 ? d : 0xbad, e, f, g, h, r);
    return r;
}

static char *STDCALL dlg_strcat(char *d, const char *t)
{
    return strcat(d, t);
}

static void setup_dialogs(void)
{
    static const uint32_t slots[] = { 0x8c, 0x90 };
    static int loaded;
    uint32_t i;

    if (!loaded) {
        use_retail_numeric();
        use_retail_util(slots, sizeof slots / sizeof slots[0]);
        loaded = 1;
    }
    SET_SLOT(host, 0x2b0, dlg_strcat);
    SET_SLOT(util, 0xa4, ar_alert);
    *(void **)(module_stub[17] + 0x18) = *(void **)(numeric_real + 0x18);
    *(void **)(module_stub[17] + 0x1c) = *(void **)(numeric_real + 0x1c);
    *(void **)(module_stub[17] + 0x20) = *(void **)(numeric_real + 0x20);
    for (i = 0; i < sizeof global; i++)
        global[i] = (uint8_t)rnd();
}

static void report_dialogs(void)
{
    printf("  dialogs: 0x1308/0x1309/0x1607/0x1203 %lu/%lu/%lu/%lu; opened %lu, none %lu, OK %lu, a check refused %lu,"
           " children %lu\n", cover_dlg_cmd[0], cover_dlg_cmd[1], cover_dlg_cmd[2], cover_dlg_cmd[3], cover_dlg,
           cover_dlg_noopen, cover_dlg_ok, cover_dlg_refused, cover_dlg_child);
}

/* ---- suite: flights (+0x50, +0xac, +0xc4, commands 0x1102 and 0x1106) ------------- */

static unsigned long cover_fl[5], cover_fl_replaced, cover_fl_appended;

/* A flight file: well formed (a title, a description, others) or damaged. */
static void flight_file(const char *name)
{
    struct vfile *f = make_file(name);

    if (rnd_n(6) == 0) {
        fill_situation(f);
    } else {
        uint32_t at = 0, i, n = rnd_n(4);

        if (rnd_n(4)) {                            /* the title */
            static const char t[] = "A Title";
            f->bytes[at] = 0; f->bytes[at + 1] = 0; f->bytes[at + 2] = 0x23; f->bytes[at + 3] = 0;
            memcpy(f->bytes + at + 4, t, sizeof t);
            at += 0x23;
        }
        for (i = 0; i < n; i++) {
            uint32_t tag = rnd_n(2) ? 3 + rnd_n(900) : 1000 + rnd_n(3000), body = rnd_n(0x40), len = body + (tag < 1000 ? 4 : 6);

            f->bytes[at++] = (uint8_t)tag;
            f->bytes[at++] = (uint8_t)(tag >> 8);
            f->bytes[at++] = (uint8_t)len;
            f->bytes[at++] = (uint8_t)(len >> 8);
            if (tag >= 1000) {
                f->bytes[at++] = 0;
                f->bytes[at++] = 0;
            }
            fill_random(f->bytes + at, body);
            at += body;
        }
        if (rnd_n(3)) {                            /* the description */
            f->bytes[at] = 2; f->bytes[at + 1] = 0; f->bytes[at + 2] = 0x20; f->bytes[at + 3] = 0;
            memcpy(f->bytes + at + 4, "Some words", 11);
            at += 0x20;
        }
        if (rnd_n(10) == 0) {                      /* cut off inside a record's header */
            uint32_t tag = rnd_n(2) ? 500 : 1500, k = rnd_n(tag < 1000 ? 2 : 4);

            f->bytes[at++] = (uint8_t)tag;
            f->bytes[at++] = (uint8_t)(tag >> 8);
            for (i = 0; i < k; i++)
                f->bytes[at++] = 0x10;
        } else {
            f->bytes[at++] = 0xff;
            f->bytes[at++] = 0xff;
        }
        f->size = at;
    }
    f->attr = rnd_n(8) == 0 ? 2 : 0;
}

static void op_flights(void)
{
    static const char *const dirs[] = { "FLIGHTS\\OTHER\\", "FLIGHTS\\", "FLIGHTS\\CHALLENGE\\" };
    char name[64], other[64];
    uint32_t k = rnd_n(100), r;
    struct vfile *f;

    if (rnd_n(2)) {
        snprintf(name, sizeof name, "%sF%u", dirs[rnd_n(3)], rnd_n(4));
        flight_file(name);
    }
    snprintf(other, sizeof other, "%sF%u", dirs[rnd_n(2)], rnd_n(4));
    if (rnd_n(2) && (f = vfile_find(other)) != NULL) {
        strcpy((char *)cur->image + 0x40dc, other);
    }
    if (rnd_n(2))
        strcpy((char *)global + 0x24, other);
    if (k < 40) {
        uint8_t *data = fixtures + 0x24000;
        uint32_t tag = rnd_n(3) == 0 ? 2 : rnd_n(3) == 0 ? 0 : rnd_n(2) ? 3 + rnd_n(900) : 1000 + rnd_n(3000);
        uint32_t body = rnd_n(0x80);
        uint32_t total = body + (tag < 1000 ? 4 : 6);

        fill_random(data, body);
        if (counting) cover_fl[0]++;
        put("\n#set(");
        put_name(other);
        put(",%#x,%#x)", tag, total);
        r = ((s4)SVC(0x50))((uint32_t)(uintptr_t)other, tag, (uint32_t)(uintptr_t)data, total);
        put(" =%#x", r);
    } else if (k < 60) {
        if (counting) cover_fl[1]++;
        put("\n#open");
        list_last_count = 1;
        r = ((s0)SVC(0xc4))();
        if (list_last_count != 0)                  /* with no flights retail answers what its stack held */
            put(" =%#x", r);
    } else if (k < 70) {
        if (counting) cover_fl[2]++;
        put("\n#command(0x1102)");
        ((s2)SVC(0x20))(0x1102, 0);
    } else if (k < 80) {
        if (counting) cover_fl[3]++;
        put("\n#command(0x1106)");
        ((s2)SVC(0x20))(0x1106, 0);
    } else {
        if (counting) cover_fl[4]++;
        put("\n#save");
        r = ((s0)SVC(0xac))();
        put(" =%#x", r);
    }
    dump_files();
    put(" g24='%.60s' def='%.60s'", (char *)global + 0x24, (char *)cur->image + 0x40dc);
}

static void setup_flights(void)
{
    uint32_t i, n = rnd_n(5);

    setup_situations();
    short_write_odds = 150;                        /* the disk fills mid-rewrite, even on the end tag */
    for (i = 0; i < n; i++) {
        char name[64];
        snprintf(name, sizeof name, "%sF%u", i & 1 ? "FLIGHTS\\" : "FLIGHTS\\OTHER\\", i);
        flight_file(name);
    }
}

static void report_clockdlg(void)
{
    printf("  clock dialog: %lu commands, %lu opened, %lu that loaded and were OK, %lu refused; fields %lu that passed %lu\n",
           cover_dlg_cmd[4], cover_dlg, cover_dlg_ok, cover_dlg_refused, cover_cd_field, cover_cd_field_ok);
}

static void report_engines(void)
{
    printf("  engines: %lu commands, %lu opened, %lu OK, %lu refused\n", cover_dlg_cmd[5], cover_dlg, cover_dlg_ok, cover_dlg_refused);
}

static void report_limits(void)
{
    printf("  limits: %lu commands, %lu opened, %lu OK, %lu refused\n", cover_dlg_cmd[6], cover_dlg, cover_dlg_ok, cover_dlg_refused);
}

static void report_areas(void)
{
    printf("  areas: %lu commands, %lu opened, %lu OK, %lu refused\n", cover_dlg_cmd[7], cover_dlg, cover_dlg_ok, cover_dlg_refused);
}

/* Host +0x31c: 1 reads the settings, 3 and 4 list the drivers and modes into its +0xe4,
 * 5 applies, 6 redraws. */
/* The descriptor's copy, whose code pointers are each side's own. */
static int32_t STDCALL pf_h21c(uint8_t *d, const uint8_t *s, uint32_t n)
{
    if (s == cur->image + 0x5cc0) {
        put(" descriptor(%#x)", n);
        memmove(d, s, n);
        return 0;
    }
    return h_memmove(d, s, n);
}

static uint32_t STDCALL pf_h31c(uint32_t op, uint8_t *block, uint32_t size)
{
    static const char *const names[] = { "Driver A", "Driver B", "640x480x16", "640x480x8", "800x600x16" };
    uint32_t r = rnd_n(4) ? 0 : 3;

    put(" display(%u,%#x)", op, size);
    if (op == 1) {
        strcpy((char *)block, rnd_n(2) ? "Driver A" : "None");
        strcpy((char *)block + 0xa8, rnd_n(2) ? "800x600x16" : "1x1");
        *(uint32_t *)(block + 0xd0) = rnd_n(4) ? rnd_n(0x800) : rnd();
    } else if (op == 3 || op == 4) {
        uint32_t *list = *(uint32_t **)(block + 0xe4), n = rnd_n(5), i;

        pf_entries = n;
        for (i = 0; i < n; i++) {
            char *t = arena_get(32);
            strcpy(t, names[op == 3 ? rnd_n(2) : 2 + rnd_n(3)]);
            list[i] = (uint32_t)(uintptr_t)t;
        }
        list[n] = 0;
    } else if (op == 5) {
        put(" [%.20s|%.20s|%x]", (char *)block, (char *)block + 0xa8, *(uint32_t *)(block + 0xd0));
    }
    return r;
}
static uint32_t STDCALL pf_m7_38(uint32_t *count, uint32_t *list)
{
    uint32_t n = rnd_n(4), i;
    uint8_t *p = arena_get(0xa08 * (n ? n : 1));

    for (i = 0; i < n; i++)
        sprintf((char *)p + i * 0xa08 + 0x900, "Plane%u", i);
    *count = n;
    *list = (uint32_t)(uintptr_t)p;
    put(" m7+0x38(%u)", n);
    return 0;
}
static uint32_t STDCALL pf_w80(uint32_t *list, uint32_t *count, uint32_t a)
{
    static uint32_t items[3] = { 0x11, 0x22, 0x33 };

    put(" window+0x80(%u)", a);
    *count = rnd_n(4);
    *list = (uint32_t)(uintptr_t)items;
    return 0;
}
#define PF_LOG1(name, text) static uint32_t STDCALL name(uint32_t a) { put(" " text "(%#x)", a); return 0; }
PF_LOG1(pf_w70, "window+0x70")
PF_LOG1(pf_w74, "window+0x74")
static uint32_t STDCALL pf_s3c(uint32_t a) { put(" sound+0x3c("); put_ptr(a); put(")"); return 0; }
static uint32_t STDCALL pf_s40(uint32_t a) { put(" sound+0x40("); put_ptr(a); put(")"); return 0; }
static uint32_t STDCALL pf_p34(void) { put(" panels+0x34"); return 0; }
static uint32_t STDCALL pf_h28(void) { put(" host+0x28"); return 0; }
static uint32_t STDCALL pf_h2a0(uint32_t id, char *buf, uint32_t len)
{
    put(" loadstring(%#x,%u)", id, len);
    if (id >= 0x2300 && id < 0x230b)               /* eleven presets; a twelfth's text would land on the descriptor */
        snprintf(buf, len, "Preset %u", id - 0x2300);
    else if (len != 0)
        buf[0] = 0;
    return 0;
}

static void setup_prefdlg(void)
{
    setup_situations();
    setup_dialogs();
    setup_prefs();
    SET_SLOT(host, 0x31c, pf_h31c);
    SET_SLOT(host, 0x21c, pf_h21c);
    SET_SLOT(host, 0x28, pf_h28);
    SET_SLOT(host, 0x2a0, pf_h2a0);
    SET_SLOT(module_stub[7], 0x38, pf_m7_38);
    SET_SLOT(module_stub[10], 0x80, pf_w80);
    SET_SLOT(module_stub[10], 0x70, pf_w70);
    SET_SLOT(module_stub[10], 0x74, pf_w74);
    SET_SLOT(sound, 0x3c, pf_s3c);
    SET_SLOT(sound, 0x40, pf_s40);
    SET_SLOT(module_stub[15], 0x34, pf_p34);
}

static void report_prefdlg(void)
{
    printf("  prefs dialog: %lu commands, %lu opened, %lu OK, %lu refused\n", cover_dlg_cmd[8], cover_dlg, cover_dlg_ok, cover_dlg_refused);
}

/* The windows this command's WINDOW hands over: items, each with a view block at +0x1c
 * and its draw routine at +0x18. */
static uint32_t STDCALL vw_draw(uint32_t a, uint32_t rec, uint32_t c, uint32_t d)
{
    put(" draw(%u,", a);
    put_ptr(rec);
    put(",%u,%u)", c, d);
    return 0;
}
static uint32_t STDCALL vw_w80(uint32_t *list, uint32_t *count, uint32_t a)
{
    uint32_t *items = arena_get(16), n = rnd_n(5), i, j;

    put(" window+0x80(%u)->%u", a, n);
    for (i = 0; i < n; i++) {
        uint8_t *item = fixtures + 0x60000 + i * 0x200, *v = item + 0x20;

        for (j = 0; j < 0xc0; j++)
            v[j] = (uint8_t)rnd();
        *(uint32_t *)item = rnd();
        *(uint32_t *)(item + 0x18) = (uint32_t)(uintptr_t)vw_draw;
        *(uint32_t *)(item + 0x1c) = (uint32_t)(uintptr_t)v;
        *(uint32_t *)(v + 0x10) = rnd_n(2);
        *(uint32_t *)(v + 0x14) = rnd_n(6);
        items[i] = (uint32_t)(uintptr_t)item;
    }
    *list = (uint32_t)(uintptr_t)items;
    *count = n;
    return 0;
}
static uint32_t STDCALL vw_w2(uint32_t a, uint32_t b) { put(" window(%x,%x)", in_image(a) ? a - (uint32_t)(uintptr_t)cur->image : a, b); return 0; }
/* WINDOW +0x24 closes a window's apply, and the preview's draw: the view block as written */
static uint32_t STDCALL vw_w24(uint32_t v, uint32_t b)
{
    put(" window+0x24(%08x,%x)", digest((const void *)(uintptr_t)v, 0xc0, 0x811c9dc5u), b);
    return 0;
}
static uint32_t STDCALL vw_m35_30(void) { uint32_t r = rnd(); put(" m35+0x30->%#x", r); return r; }
static uint32_t STDCALL vw_h74(uint32_t window, uint8_t *rec)
{
    put(" host+0x74(%#x)", window);
    if (rnd_n(8) == 0)
        return 5;
    *(uint32_t *)(rec + 0x60) = 20 + rnd_n(200);
    *(uint32_t *)(rec + 0x64) = 20 + rnd_n(200);
    return 0;
}
static uint32_t STDCALL vw_h78(uint32_t a) { put(" host+0x78("); put_ptr(a); put(")"); return 11; }
static uint32_t STDCALL vw_h84(uint32_t a) { put(" host+0x84("); put_ptr(a); put(")"); return 13; }
static uint32_t STDCALL vw_h27c(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { put(" host+0x27c("); put_ptr(a); put(","); put_ptr(b); put(",%u,%u)", c, d); return 0; }
static uint32_t STDCALL vw_h284(uint32_t a, uint32_t b) { put(" host+0x284("); put_ptr(a); put(","); put_ptr(b); put(")"); return 0; }
static uint32_t STDCALL vw_u2c(uint32_t a, int16_t *s, int16_t *c)
{
    *s = (int16_t)rnd();
    *c = (int16_t)rnd();
    put(" util+0x2c(%#x)->%d,%d", a & 0xffff, *s, *c);
    return 0;
}
static uint32_t STDCALL vw_hg34(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    put(" hg2d+0x34(");
    put_ptr(a);
    put(",%u,%u,%u,%u)", b, c, d, e);
    return 0;
}

static void setup_viewdlg(void)
{
    setup_dialogs();
    SET_SLOT(module_stub[10], 0x80, vw_w80);
    SET_SLOT(module_stub[10], 0x5c, vw_w2);
    SET_SLOT(module_stub[10], 0x58, vw_w2);
    SET_SLOT(module_stub[10], 0x24, vw_w24);
    SET_SLOT(multip, 0x30, vw_m35_30);
    SET_SLOT(host, 0x74, vw_h74);
    SET_SLOT(host, 0x78, vw_h78);
    SET_SLOT(host, 0x84, vw_h84);
    SET_SLOT(host, 0x27c, vw_h27c);
    SET_SLOT(host, 0x284, vw_h284);
    SET_SLOT(util, 0x2c, vw_u2c);
    SET_SLOT(hg2d, 0x34, vw_hg34);
}

static void report_viewdlg(void)
{
    printf("  view dialog: %lu commands, %lu opened\n", cover_dlg_cmd[9], cover_dlg);
}

/* The logbook's host: a growable array, and a directory walk over the files present. */
static uint32_t STDCALL lb_a_add(uint8_t *a)
{
    uint32_t n = *(uint32_t *)a, size = *(uint32_t *)(a + 0xc);
    uint8_t **data = (uint8_t **)(a + 0x18), *p;

    if (n >= *(uint32_t *)(a + 4)) {
        uint32_t cap = *(uint32_t *)(a + 4) + *(uint32_t *)(a + 8);
        p = arena_get(cap * size);
        memset(p, 0, cap * size);
        if (n != 0)
            memcpy(p, *data, n * size);
        *data = p;
        *(uint32_t *)(a + 4) = cap;
    }
    *(uint32_t *)a = n + 1;
    p = *data + n * size;
    memset(p, 0, size);
    put(" add(%u)", n);
    return (uint32_t)(uintptr_t)p;
}
static uint32_t STDCALL lb_a_remove(uint8_t *a, uint8_t *e)
{
    uint32_t n = *(uint32_t *)a, size = *(uint32_t *)(a + 0xc);
    uint8_t *data = *(uint8_t **)(a + 0x18);

    /* as host_list_remove_at: an element at or past the last moves nothing */
    int32_t i = size ? (int32_t)((uint32_t)((uint32_t)(uintptr_t)e - (uint32_t)(uintptr_t)data) / size) : -1, count = (int32_t)n - 1;

    put(" remove(%d)", i);
    *(uint32_t *)a = (uint32_t)count;
    if (i >= count)
        return 0;
    if (i < 0)                                     /* a stale row, before the block: the real one runs off too */
        stub_fault("wild");
    memmove(data + (uint32_t)i * size, data + (uint32_t)(i + 1) * size, (uint32_t)(count - i) * size);
    return size;
}
static uint32_t STDCALL lb_a_clear(uint8_t *a)
{
    *(uint32_t *)a = 0;
    *(uint32_t *)(a + 4) = 0;
    *(uint32_t *)(a + 0x18) = 0;
    return 0;
}
static uint32_t lb_walk;
static uint32_t STDCALL lb_h164(uint32_t *out, const char *dir, uint32_t a, uint32_t ext)
{
    put(" find(%s,%u,%#x)", dir, a, ext);
    lb_walk = 0;
    *out = 0x77;
    return rnd_n(16) == 0;
}
static uint32_t STDCALL lb_h160(uint32_t h, uint8_t *found)
{
    (void)h;
    for (; lb_walk < MAX_FILES; lb_walk++) {
        struct vfile *f = &files[lb_walk];

        if (f->used && strncmp(f->name, "FLIGHTS\\L", 9) == 0) {
            strcpy((char *)found, f->name + 8);
            *(uint32_t *)(found + 0x108) = f->size;
            lb_walk++;
            put(" next(%s)", (char *)found);
            return 0;
        }
    }
    put(" next-end");
    return 1;
}
static uint32_t STDCALL lb_h15c(uint32_t *h) { put(" find-close(%#x)", *h); return 0; }
static uint32_t lb_room_low;
static uint32_t STDCALL lb_h150(const char *drive, uint32_t *space)
{
    uint32_t k = rnd_n(16);

    put(" space(%s)", drive);
    if (k == 0)
        return 1;
    space[0] = lb_room_low && k <= 3 ? 0x1387 + rnd_n(2) : 0x100000;
    space[1] = lb_room_low && k == 3 ? 1 : 0;
    return 0;
}

/* A logbook file: title, heads, rows, totals */
static void lb_file(const char *name)
{
    struct vfile *f = make_file(name);
    uint32_t at = 0, n = rnd_n(5), i;
    static const char *const dates[] = { "01-02-98", "12-25-97", "07-04-00", "03-15-99", "01-02-98" };

    switch (rnd_n(6)) {                            /* .text+0x11480 gives up on a title at 30 characters */
    case 0: at += (uint32_t)sprintf((char *)f->bytes, "\r\n\r\n"); break;
    case 1: at += (uint32_t)sprintf((char *)f->bytes, "%-29s\r\n\r\n", "Title"); break;
    case 2: at += (uint32_t)sprintf((char *)f->bytes, "%-30s\r\n\r\n", "Title"); break;
    default: at += (uint32_t)sprintf((char *)f->bytes, "My Title %u\r\n\r\n", rnd_n(100)); break;
    }
    at += (uint32_t)sprintf((char *)f->bytes + at, "%-80s\r\n\r\n", "  DATE");
    for (i = 0; i < n; i++)
        at += (uint32_t)sprintf((char *)f->bytes + at, "%-8s   %-16s %-24s %4u.%u %4u.%u %4u.%u %4u.%u\r\n", dates[rnd_n(5)], "CESSNA", "KSEA-KPDX",
                                rnd_n(100), rnd_n(10), rnd_n(100), rnd_n(10), rnd_n(100), rnd_n(10), rnd_n(100), rnd_n(10));
    at += (uint32_t)sprintf((char *)f->bytes + at, "\r\n%-52s\r\n", "TOTALS");
    /* never cut short: a head with no end leaves .text+0xfd70's counts as its stack held them */
    f->size = at;
    f->attr = rnd_n(4) == 0 ? 2 : 0;
}

/* Retail's sscanf for the rows' dates: what it does not scan reads 0, as in ours, where
 * retail would compare its stack. */
static int lb_sscanf(const char *t, const char *fmt, int *a, int *b, int *c)
{
    *a = 0;
    *b = 0;
    *c = 0;
    return sscanf(t, fmt, a, b, c);
}

static uint32_t acft_n, acft_long;                  /* the logbook cuts a model's name at 16 characters */
static uint32_t STDCALL acft_1c(uint32_t *count, uint32_t *list);
static uint32_t STDCALL acft_30(uint32_t key, uint32_t a, char *path, uint32_t n);

static void setup_logbook(void)
{
    uint32_t i, n = rnd_n(4);

    open_makes = 1;
    stack_sources = 1;
    acft_long = 1;
    {
        DWORD old;
        void **iat = (void **)(retail.image + 0x2f0fc);

        VirtualProtect(iat, 4, PAGE_READWRITE, &old);
        *iat = (void *)lb_sscanf;
    }
    setup_situations();
    setup_dialogs();
    SET_SLOT(host, 0x2f8, lb_a_add);
    SET_SLOT(host, 0x300, lb_a_remove);
    SET_SLOT(host, 0x304, lb_a_clear);
    SET_SLOT(host, 0x164, lb_h164);
    SET_SLOT(host, 0x160, lb_h160);
    SET_SLOT(host, 0x15c, lb_h15c);
    SET_SLOT(host, 0x150, lb_h150);
    SET_SLOT(acontain, 0x1c, acft_1c);
    SET_SLOT(acontain, 0x30, acft_30);
    for (i = 0; i < n; i++) {
        char name[32];

        sprintf(name, "FLIGHTS\\LOG%u", i);
        lb_file(name);
    }
    if (rnd_n(8))                                  /* with none at all, .text+0xf3d0 reads a book that is not there */
        lb_file("FLIGHTS\\Logbook");
}

static void op_logbook(void)
{
    static const uint16_t words[4] = { 0x230, 0x232, 0x234, 0x236 };
    uint32_t i;

    /* asked directly, since .text+0xfd70 cannot be: with its flag set the reader does nothing */
    if (rnd_n(4) == 0) {
        uint32_t lines = 0x1111, n = 0x2222;
        uint8_t was = cur->image[0x3c86];

        cur->image[0x3c86] = 1;
        ((s3)internal(0x207f2480, "main_lb_file_read@12"))((uint32_t)(uintptr_t)"LOG0", (uint32_t)(uintptr_t)&lines, (uint32_t)(uintptr_t)&n);
        cur->image[0x3c86] = was;
        put(" flagged-read %x %x", lines, n);
    }
    /* a row without its CR left the read buffer flagged: the next read of a logbook would
     * not read, and .text+0xfd70 would go on with the counts its stack held */
    if (cur->image[0x3c86] != 0) {
        put(" flagged");
        return;
    }
    acft_n = rnd_n(4);
    lb_room_low = rnd_n(4) == 0;
    *(uint32_t *)(global + 0x128) = rnd_n(4) != 0;
    strcpy((char *)global + 0x12c, rnd_n(3) == 0 ? "" : rnd_n(2) ? "LOG0" : "Logbook");
    *(uint16_t *)(global + 0x23e) = (uint16_t)(1 + rnd_n(365));
    *(uint16_t *)(global + 0x240) = (uint16_t)(1990 + rnd_n(16));
    for (i = 0; i < 4; i++)
        *(uint16_t *)(global + words[i]) = (uint16_t)(rnd_n(4) ? rnd_n(0x20) : rnd_n(0x800));
    if (rnd_n(4) == 0) {                           /* the logbook's limit on the hours it adds */
        uint32_t sum = 0x7ce + rnd_n(3), day = rnd_n(sum + 1);

        *(uint16_t *)(global + 0x230) = (uint16_t)day;
        *(uint16_t *)(global + 0x234) = (uint16_t)(sum - day);
    }
    if (rnd_n(2)) {
        run_dialog(10);
    } else {
        put("\n#log");
        ((s0)SVC(0x90))();                         /* void: retail answers what eax held */
    }
    dump_files();
    put(" book=%s", (char *)global + 0x12c);
}

static void report_logbook(void)
{
    printf("  logbook: %lu commands, %lu opened\n", cover_dlg_cmd[10], cover_dlg);
}

static void setup_weather(void);

static void setup_wxdlg(void)
{
    wx_norm = 1;
    setup_weather();                               /* the host's sort; NUMERIC is the real one */
    setup_dialogs();
}

static void report_wxdlg(void)
{
    printf("  weather dialog: %lu commands, %lu opened\n", cover_dlg_cmd[11], cover_dlg);
}

static void setup_radios(void);

static void setup_radiodlg(void)
{
    setup_radios();
    setup_dialogs();
}

static void report_radiodlg(void)
{
    printf("  radios dialog: %lu commands, %lu opened\n", cover_dlg_cmd[12], cover_dlg);
}

static uint32_t STDCALL cr_dup(const char *t)
{
    char *p = arena_get((uint32_t)strlen(t) + 1);

    strcpy(p, t);
    return (uint32_t)(uintptr_t)p;
}
static uint32_t STDCALL cr_facil(uint32_t *strings)
{
    uint32_t r = rnd_n(2), i;

    put(" facil+0x24->%u", r);
    if (r != 0)
        for (i = 0; i < 6; i++)
            strings[i] = rnd_n(3) ? cr_dup(i & 1 ? "KSEA" : "Seattle") : 0;
    return r;
}
static uint32_t STDCALL acft_dialog(uint32_t id, uint32_t desc);
static uint32_t STDCALL cr_dialog(uint32_t id, uint32_t desc)
{
    return id == 0x1201 ? acft_dialog(id, desc) : h_dialog(id, desc);
}

static void op_create(void)
{
    acft_n = rnd_n(5);
    run_dialog(13);
}

static void setup_aircraft(void);
static void setup_create(void)
{
    setup_aircraft();
    setup_weather();
    setup_dialogs();
    SET_SLOT(host, 0x10c, cr_dialog);
    SET_SLOT(host, 0x2b4, cr_dup);
    SET_SLOT(module_stub[37], 0x24, cr_facil);
    wx_norm = 1;
    session_empty_strings = 1;                     /* a string not found leaves "", as LoadString does */
}

static void report_create(void)
{
    printf("  create: %lu commands, %lu opened\n", cover_dlg_cmd[13], cover_dlg);
}

static void report_flights(void)
{
    printf("  flights: set record %lu, open %lu (+0xc4) + %lu (0x1102), challenges %lu, save %lu; lists empty %lu\n",
           cover_fl[0], cover_fl[1], cover_fl[2], cover_fl[3], cover_fl[4], cover_list_empty);
    printf("  Open: delete %lu, edit %lu, OK %lu; Save: OK %lu, check refused %lu\n", cover_open_delete,
           cover_open_edit, cover_open_ok, cover_save_ok, cover_save_status);
}

/* ---- suite: aircraft (command 0x1201, slice 34) ------------------------------------ */

static uint32_t acft_n;
static unsigned long cover_acft_open, cover_acft_draw, cover_acft_moved, cover_acft_ok;

static uint32_t STDCALL acft_1c(uint32_t *count, uint32_t *list)
{
    uint32_t i;
    uint8_t *p = arena_get(0xa08 * (acft_n ? acft_n : 1));

    for (i = 0; i < acft_n; i++)
        sprintf((char *)p + i * 0xa08, acft_long && (i & 1) ? "Plane%u Long Model Name" : "Plane%u", i);
    *count = acft_n;
    *list = (uint32_t)(uintptr_t)p;
    put(" list(%u)", acft_n);
    return 0;
}
static uint32_t STDCALL acft_30(uint32_t key, uint32_t a, char *path, uint32_t n)
{
    (void)key;
    put(" m23+0x30(%u,%#x)", a, n);
    if (rnd_n(4) == 0) {
        path[0] = 0;
    } else {
        uint32_t i = rnd_n(acft_n + 2);

        sprintf(path, acft_long && (i & 1) ? "plane%u long model name" : "plane%u", i);
    }
    return 1;
}
static uint32_t STDCALL acft_28(const char *rec)
{
    put(" m23+0x28(%s)", rec);
    return 0;
}
static uint32_t STDCALL acft_38(uint32_t *obj) { put(" m23+0x38(%u)", (unsigned)(obj == (uint32_t *)(cur->image + 0x7f90))); return 0; }
static uint32_t STDCALL acft_24(uint32_t *obj) { put(" m23+0x24(%u)", (unsigned)(obj == (uint32_t *)(cur->image + 0x7f90))); return 0; }
static uint32_t STDCALL acft_20(uint32_t *obj, uint32_t rec)
{
    uint32_t *o = (uint32_t *)(fixtures + 0x60000), *e = (uint32_t *)(fixtures + 0x60100);

    put(" m23+0x20(");
    put_ptr(rec);
    put(")");
    o[1] = 0x1234;
    o[3] = rnd_n(20);
    obj[0] = (uint32_t)(uintptr_t)o;
    obj[1] = (uint32_t)(uintptr_t)e;
    return 0;
}
static uint32_t STDCALL acft_34(uint32_t *obj, uint32_t a)
{
    uint32_t r = rnd_n(8) != 0;
    (void)obj;
    put(" m23+0x34(%u)->%u", a, r);
    return r;
}
static uint32_t STDCALL acft_3c(uint32_t *obj)
{
    uint32_t r = rnd_n(8) ? 1 : rnd_n(3);
    (void)obj;
    put(" m23+0x3c->%u", r);
    return r;
}
static uint32_t STDCALL acft_h74(uint32_t window, uint8_t *rec)
{
    uint32_t w = 4 + rnd_n(8), h = 4 + rnd_n(8);

    put(" host+0x74(%#x,w=%u,h=%u)", window, w, h);
    if (rnd_n(8) == 0)
        return 7;
    *(uint32_t *)(rec + 0x84) = w;
    *(uint32_t *)(rec + 0x88) = h;
    *(uint32_t *)(rec + 0x8c) = 0x10;
    *(uint32_t *)(rec + 0x94) = 0x10 * h;
    *(uint32_t *)(rec + 0x98) = (uint32_t)(uintptr_t)(fixtures + 0x61000);
    return 0;
}
static uint32_t STDCALL acft_h78(uint32_t rec) { put(" host+0x78("); put_ptr(rec); put(")"); return 0; }
static uint32_t STDCALL acft_h84(uint8_t *rec)
{
    put(" host+0x84(%08x)", digest(fixtures + 0x61000, 0x100, 0x811c9dc5u));
    (void)rec;
    return 0;
}
static uint32_t STDCALL acft_h38(void) { uint32_t r = rnd(); put(" host+0x38->%#x", r); return r; }
static uint32_t STDCALL acft_w2c(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    put(" window+0x2c(%u,%u,%u,", a, b, c);
    put_ptr(d);
    put(")");
    return 0;
}
static uint32_t STDCALL acft_w30(void) { put(" window+0x30"); return 0; }
static uint32_t STDCALL acft_g18(uint32_t a, uint32_t *e)
{
    uint8_t *bits = fixtures + 0x61000;
    uint32_t i, n = rnd_n(3);

    put(" g3d+0x18(%#x) e=%x,%x,%x,%x,%x", a, e[2], e[7], e[8], e[9], e[26]);
    for (i = 0; i < n; i++)                        /* the model drawn: on the border now and then */
        bits[rnd_n(0x100)] = (uint8_t)rnd();
    if (rnd_n(2)) {                                /* or kept to a pixel off the corner */
        uint32_t w = 4 + rnd_n(8);
        bits[0x10 * rnd_n(12) + (rnd_n(2) ? 0 : w - 1)] = 7;
    }
    return 0;
}
static uint32_t STDCALL acft_dialog(uint32_t id, uint32_t desc)
{
    uint32_t *b = (uint32_t *)(uintptr_t)desc, window = 0x7000 + rnd_n(0x100), k, r;

    put(" dialog(%#x,", id);
    put_ptr(desc);
    put(")");
    if (rnd_n(8) == 0) {
        put(" no");
        return 0xffffffffu;
    }
    if (counting) cover_acft_open++;
    put(" a=%#x", ((s1)(uintptr_t)b[1])(desc));
    put(" b=%#x", ((s1)(uintptr_t)b[2])(desc));
    put(" c=%#x", ((s1)(uintptr_t)b[3])(desc));
    put(" which=%u", b[5]);
    r = ((s3)(uintptr_t)b[4])(1, desc, window);
    put(" open=%#x", r);
    if (r == 0) {
        for (k = rnd_n(6); k-- != 0; ) {
            uint32_t *e = (uint32_t *)(fixtures + 0x60100);
            uint32_t was = e[2];

            if (rnd_n(2))
                b[5] = rnd_n(acft_n + 3);
            put(" box=%#x", ((s3)(uintptr_t)b[4])(3, desc, window));
            put(" which=%u", b[5]);
            put(" 1194=");
            put_ptr(*(uint32_t *)(global + 0x1194));
            *(uint32_t *)(global + 0x1194) = 0;
            if (counting) {
                cover_acft_draw++;
                if (e[2] != was) cover_acft_moved++;
            }
        }
        put(" close=%#x", ((s3)(uintptr_t)b[4])(2, desc, window));
    }
    b[0] = rnd_n(2);
    if (counting && b[0]) cover_acft_ok++;
    return 0;
}

static void setup_aircraft(void)
{
    setup_situations();
    SET_SLOT(acontain, 0x1c, acft_1c);
    SET_SLOT(acontain, 0x20, acft_20);
    SET_SLOT(acontain, 0x24, acft_24);
    SET_SLOT(acontain, 0x28, acft_28);
    SET_SLOT(acontain, 0x30, acft_30);
    SET_SLOT(acontain, 0x34, acft_34);
    SET_SLOT(acontain, 0x38, acft_38);
    SET_SLOT(acontain, 0x3c, acft_3c);
    SET_SLOT(host, 0x74, acft_h74);
    SET_SLOT(host, 0x78, acft_h78);
    SET_SLOT(host, 0x84, acft_h84);
    SET_SLOT(host, 0x38, acft_h38);
    SET_SLOT(host, 0x10c, acft_dialog);
    SET_SLOT(module_stub[10], 0x2c, acft_w2c);
    SET_SLOT(module_stub[10], 0x30, acft_w30);
    SET_SLOT(g3d, 0x18, acft_g18);
}

static void op_aircraft(void)
{
    uint32_t i;

    acft_n = rnd_n(5);
    for (i = 0x5b0; i < 0x5d4; i++)
        global[i] = (uint8_t)rnd();
    *(uint32_t *)(cur->image + 0x2edf4) = rnd_n(3);
    put("\n#command(0x1201)");
    ((s2)SVC(0x20))(0x1201, rnd());
    put(" 608=%u", global[0x608]);
}

static void report_aircraft(void)
{
    printf("  aircraft: opened %lu, drawn %lu, moved back %lu, OK %lu\n", cover_acft_open, cover_acft_draw,
           cover_acft_moved, cover_acft_ok);
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

/* ---- suite: natives (the routines MAIN's G3D programs call, slice 28) ----------- */

/* G3D's opcode 0x8c calls them with its registers and keeps what they leave, so
 * every register goes in random and comes out compared.  The stubs' ecx and edx
 * are whatever C left, so these thunks fix them; G3D +0x40 leaves both alone. */
uint32_t clean_ret, clean_target, native_quad, native_quads;
void clean_g3d_1c(void); void clean_g3d_24(void); void clean_hg2d_b4(void);
void clean_hg2d_b8(void); void clean_util_44(void); void clean_util_48(void);
void native_g3d_40(void);
void *clean_t_g3d_1c, *clean_t_g3d_24, *clean_t_hg2d_b4, *clean_t_hg2d_b8, *clean_t_util_44, *clean_t_util_48;
#define CLEAN(name, ecx_value, edx_value)                                      \
    __asm__(".text\n_clean_" #name ":\n\t"                                     \
            "popl _clean_ret\n\tcall *_clean_t_" #name "\n\t"                  \
            "movl $" #ecx_value ", %ecx\n\tmovl $" #edx_value ", %edx\n\t"     \
            "pushl _clean_ret\n\tret\n")
CLEAN(g3d_1c, 0x11110001, 0x22220001);
CLEAN(g3d_24, 0x11110002, 0x22220002);
CLEAN(hg2d_b4, 0x11110003, 0x22220003);
CLEAN(hg2d_b8, 0x11110004, 0x22220004);
CLEAN(util_44, 0x11110005, 0x22220005);
CLEAN(util_48, 0x11110006, 0x22220006);
__asm__(".text\n_native_g3d_40:\n\tpushal\n\tcall _native_steer\n\tpopal\n\tmovl _native_quad, %eax\n\tret\n");
void native_steer(void);

/* regs: eax ebx ecx edx esi edi ebp, in and out. */
void native_call(uint32_t fn, uint32_t *regs);
__asm__(".text\n_native_call:\n\t"
        "pushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 24(%esp), %eax\n\tpushl %eax\n\t"           /* regs, for afterwards */
        "movl 24(%esp), %edx\n\tpushl %edx\n\t"           /* fn */
        "movl 0(%eax), %edx\n\tmovl %edx, _native_eax\n\t"
        "movl 4(%eax), %ebx\n\tmovl 8(%eax), %ecx\n\tmovl 12(%eax), %edx\n\t"
        "movl 16(%eax), %esi\n\tmovl 20(%eax), %edi\n\tmovl 24(%eax), %ebp\n\t"
        "movl _native_eax, %eax\n\t"
        "call *(%esp)\n\t"
        "addl $4, %esp\n\t"
        "xchgl %eax, (%esp)\n\t"                          /* eax kept, regs in eax */
        "movl %ebx, 4(%eax)\n\tmovl %ecx, 8(%eax)\n\tmovl %edx, 12(%eax)\n\t"
        "movl %esi, 16(%eax)\n\tmovl %edi, 20(%eax)\n\tmovl %ebp, 24(%eax)\n\t"
        "popl %edx\n\tmovl %edx, 0(%eax)\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");
uint32_t native_eax;

static uint32_t STDCALL natives_util_44(uint32_t a, uint32_t b, uint32_t out)
{
    uint8_t *o = (uint8_t *)(uintptr_t)out;
    uint32_t i;

    put(" util+0x44(");
    put_ptr(a);
    put(",");
    put_ptr(b);
    put(",");
    put_ptr(out);
    put(")");
    for (i = 0; i < 8; i++)
        o[i] = (uint8_t)(rnd_n(3) ? 0 : rnd());
    return 0x5a5a0044u;
}

static uint32_t STDCALL natives_g3d_1c(uint32_t cmd)
{
    put(" g3d+0x1c(");
    put_ptr(cmd);
    put(")=%08x", digest((uint8_t *)(uintptr_t)native_quad, 0x6d0, 0x811c9dc5u));
    if (getenv("MAIN_QUAD")) {
        uint32_t i;
        for (i = 0; i < 0x6d0; i += 4)
            if (i < 0x100 || i >= 0x640)
                put(" %x:%08x", i, *(uint32_t *)(uintptr_t)(native_quad + i));
        for (i = 0x33ba; i < 0x3470; i += 4)
            put(" d%x:%08x", i, *(uint32_t *)(cur->image + i));
    }
    return 0x5a5a001cu;
}

/* A column's quad: GLOBAL +0xce0 now and then on the height it is about to be
 * compared with. */
void native_steer(void)
{
    native_quads++;
    if (native_quads > 1 && rnd_n(8) == 0)
        *(uint32_t *)(global + 0xce0) = *(uint32_t *)(cur->image + 0x3458 + 4 * rnd_n(4));
}

static unsigned long cover_native[15], cover_native_faces, cover_native_quads, cover_native_nine, cover_native_block;

static void setup_natives(void)
{
    clean_t_g3d_1c = (void *)natives_g3d_1c;
    clean_t_g3d_24 = (void *)g3d_24;
    clean_t_hg2d_b4 = (void *)hg2d_b4;
    clean_t_hg2d_b8 = (void *)hg2d_b8;
    clean_t_util_44 = (void *)natives_util_44;
    clean_t_util_48 = (void *)util_geo48;
    SET_SLOT(g3d, 0x1c, clean_g3d_1c);
    SET_SLOT(g3d, 0x24, clean_g3d_24);
    SET_SLOT(g3d, 0x40, native_g3d_40);
    SET_SLOT(hg2d, 0xb4, clean_hg2d_b4);
    SET_SLOT(hg2d, 0xb8, clean_hg2d_b8);
    SET_SLOT(util, 0x44, clean_util_44);
    SET_SLOT(util, 0x48, clean_util_48);
}

static void op_natives(void)
{
    /* one of each's offsets in the image, and any other block's now and then */
    static const uint16_t at[15][4] = {
        { 0x2386, 0x23aa, 0x2466, 0x2716 }, { 0x23ce, 0x23f2, 0x2822, 0x290a },
        { 0x313a, 0x3186, 0x3244, 0x3374 }, { 0x262c, 0x262c, 0x262c, 0x262c },
        { 0x282e, 0x2916, 0x282e, 0x2916 }, { 0x243c, 0x2674, 0x26e4, 0x278e },
        { 0x27f4, 0x2876, 0x28e8, 0x295e }, { 0x2692, 0x2702, 0x2894, 0x297c },
        { 0x2812, 0x2812, 0x2812, 0x2812 }, { 0x24a8, 0x24a8, 0x24a8, 0x24a8 },
        { 0x2990, 0x2990, 0x2990, 0x2990 }, { 0x24ce, 0x29ca, 0x29ea, 0x24ce },
        { 0x307c, 0x307c, 0x307c, 0x307c }, { 0x269c, 0x270c, 0x3088, 0x2986 },
        { 0x34fa, 0x34fa, 0x34fa, 0x34fa },
    };
    uint32_t which = rnd_n(3) == 0 ? rnd_n(2) : rnd_n(15), regs[7], i, fn;
    uint8_t *quad = fixtures + 0x20000;

    /* GLOBAL round what they read: often on an edge */
    for (i = 0xb3c; i < 0xb52; i++)
        global[i] = (uint8_t)rnd();
    for (i = 0xc8c; i < 0xca0; i++)
        global[i] = (uint8_t)rnd();
    for (i = 0xe60; i < 0xe8a; i++)
        global[i] = (uint8_t)rnd();
    for (i = 0x560; i < 0x578; i++)
        global[i] = (uint8_t)rnd();
    for (i = 0x5b0; i < 0x5c0; i++)
        global[i] = (uint8_t)rnd();
    *(uint32_t *)(global + 0xce0) = rnd_n(3) ? rnd_n(0x40000) : rnd();
    *(uint16_t *)(global + 0xce2) = (uint16_t)rnd();
    *(uint32_t *)(global + 0xd4c) = rnd_n(2) ? rnd_n(0x400) : rnd();
    *(uint16_t *)(global + 0xdce) = (uint16_t)(rnd_n(5) ? 0 : rnd());
    *(uint16_t *)(global + 0xcb6) = (uint16_t)(rnd_n(5) ? rnd() | 1 : 0);
    *(uint16_t *)(global + 0xe86) = (uint16_t)rnd_n(3);
    *(uint16_t *)(global + 0xe6e) = (uint16_t)(rnd_n(3) == 0 ? 0xe000 + rnd_n(3) : rnd());
    *(uint16_t *)(global + 0xe76) = (uint16_t)(rnd_n(3) == 0 ? 0xe000 + rnd_n(3) : rnd());
    *(uint16_t *)(global + 0x574) = (uint16_t)(rnd_n(3) == 0 ? *(uint16_t *)(global + 0xe7a) + rnd_n(3) - 1 : rnd());
    if (rnd_n(3) == 0)
        *(uint16_t *)(global + 0xe80) = (uint16_t)(*(uint16_t *)(global + 0xe7a) - *(uint16_t *)(global + 0x574) + rnd_n(3) - 1);

    /* the image round what they read; the relocated words stay */
    seed_image(0x1a80, 0x1ab0);
    seed_image(0x1e4a, 0x1e56);
    seed_image(0x1eb4, 0x1ed0);
    seed_image(0x1ff5, 0x201d);
    seed_image(0x2052, 0x2068);
    seed_image(0x33ba, 0x3402);
    seed_image(0x3468, 0x3470);
    seed_image(0x349c, 0x34bc);
    for (i = 0; i < 3; i++)                        /* just on +0x7bc2's three thresholds */
        if (rnd_n(2))
            *(uint32_t *)(cur->image + 0x1e4a + 4 * i) = *(uint32_t *)(global + 0xd4c) + rnd_n(3) - 1;
    *(uint16_t *)(cur->image + 0x1ff5) = (uint16_t)(rnd_n(5) ? 1 : 0);
    *(uint16_t *)(cur->image + 0x1ff7) = (uint16_t)(rnd_n(5) ? 1 : 0);
    if (rnd_n(8))                                  /* a real divisor, now and then none */
        *(int32_t *)(cur->image + 0x3468) = (int32_t)(rnd_n(0x10000) + 1) * (rnd_n(2) ? 1 : -1);
    /* the aircraft near the layer record: a texel inside the map, most times */
    for (i = 0; i < 2; i++) {
        uint8_t *r = cur->image + 0x1a80 + i * 0x18;
        if (rnd_n(4)) {
            *(uint32_t *)(r + 4) = *(uint32_t *)(global + 0x5b4) - (rnd() & 0x3ffffff) + 0x2000000;
            *(uint16_t *)(r + 0xc) = (uint16_t)(*(uint16_t *)(global + 0x5bc) - rnd_n(0x40) + 0x20);
            *(uint16_t *)(r + 0xe) = *(uint16_t *)(global + 0x5be);
        }
    }
    *(uint32_t *)(cur->image + 0x2043) = rnd();
    hg2d_threshold = (uint8_t)(rnd_n(3) ? rnd_n(8) : rnd());
    if (op_number == 0) {
        uint32_t density = 1 + rnd_n(8);
        for (i = 0; i < 0x10000; i++)
            fixtures[i] = (uint8_t)(rnd_n(density) == 0 ? rnd_n(12) : 0);
        for (i = 0; i < 0x6d0; i++)
            quad[i] = (uint8_t)rnd();
    }
    native_quad = (uint32_t)(uintptr_t)quad;
    native_quads = 0;

    fn = *(uint32_t *)(cur->image + at[which][rnd_n(4)]);
    regs[0] = rnd();
    regs[1] = rnd();
    regs[2] = rnd();
    regs[3] = rnd();
    regs[4] = rnd();
    regs[5] = rnd_n(8) ? (uint32_t)(uintptr_t)global : (uint32_t)(uintptr_t)(fixtures + 0x30000);
    regs[6] = rnd();
    if (counting)
        cover_native[which]++;
    put("\n#native(%u) dce=%#x cb6=%#x own=%d", which, *(uint16_t *)(global + 0xdce), *(uint16_t *)(global + 0xcb6),
        (uint8_t *)(uintptr_t)fn >= cur->lo && (uint8_t *)(uintptr_t)fn < cur->hi);
    native_call(fn, regs);
    put(" ->");
    for (i = 0; i < 7; i++)
        put(" %08x", regs[i]);
    put(" c94=%08x c9c=%08x e88=%#x d4c=%08x", *(uint32_t *)(global + 0xc94), *(uint32_t *)(global + 0xc9c),
        *(uint16_t *)(global + 0xe88), *(uint32_t *)(global + 0xd4c));
    put(" quad=%08x", digest(quad, 0x6d0, 0x811c9dc5u));
    if (counting) {
        if (native_quads > 1) { cover_native_faces++; cover_native_quads += native_quads; }
        if (which == 3 || which == 4) {
            if (*(uint16_t *)(cur->image + 0x205e) != 0) cover_native_nine++;
            else cover_native_block++;
        }
    }
}

static void report_natives(void)
{
    uint32_t i;

    printf("  natives: calls");
    for (i = 0; i < 15; i++)
        printf(" %lu", cover_native[i]);
    printf("; faces drawn %lu with %lu quads; tiles nine %lu, block or none %lu\n",
           cover_native_faces, cover_native_quads, cover_native_nine, cover_native_block);
}

/* ---- suite: start (what +0x28 sets going, slice 29) ------------------------------- */

static uint32_t start_lists;                       /* for WINDOW +0x80: how many items */

static uint32_t STDCALL start_window_80(uint32_t *list, int32_t *n, uint32_t mode)
{
    uint32_t count = rnd_n(5), i;
    uint32_t *a;

    put(" window+0x80(%u)->%u", mode, count);
    a = arena_get(4 * (count ? count : 1));
    for (i = 0; i < count; i++) {
        uint8_t *item = fixtures + 0x20000 + 0x100 * (start_lists++ % 0x40);
        uint32_t *cls = (uint32_t *)(item + 0x40);
        *(uint32_t *)(item + 8) = rnd();
        *(uint16_t *)(item + 0xc) = (uint16_t)rnd();
        *(uint32_t *)(item + 0x1c) = (uint32_t)(uintptr_t)cls;
        cls[4] = rnd_n(2) ? 0 : rnd();
        a[i] = (uint32_t)(uintptr_t)item;
    }
    *list = (uint32_t)(uintptr_t)a;
    *n = (int32_t)count;
    return 0;
}
static uint32_t STDCALL start_m35_30(void)
{
    uint32_t r = rnd() & 7;
    put(" m35+0x30->%u", r);
    return r;
}
static uint32_t STDCALL start_m21_18(uint32_t *out)
{
    static const uint32_t v[] = { 0, 0x20, 0x1a, 0x21, 5 };
    uint32_t r = rnd_n(2) ? rnd() : 0;
    *out = v[rnd_n(5)];
    put(" m21+0x18->%#x,%#x", r, *out);
    return r;
}
static uint32_t STDCALL start_m15_38(void)
{
    uint32_t r = rnd_n(2) ? rnd() : 0;
    put(" m15+0x38->%#x", r);
    return r;
}
static uint32_t STDCALL start_h_264(uint32_t a, uint32_t b)
{
    put(" menutext(%#x,%#x)", a, b);
    return 0;
}
static uint32_t STDCALL start_h_50(uint32_t a)
{
    put(" host+0x50(%u)", a);
    return 0;
}
static uint32_t STDCALL start_season(uint32_t a, uint32_t b)
{
    put(" season(%#x,%#x)", a, b & 0xffff);
    return 0;
}

static unsigned long cover_start[8], cover_start_v[5], cover_start_minute, cover_start_rollover, cover_start_tally;

static void setup_start(void)
{
    SET_SLOT(module_stub[10], 0x80, start_window_80);
    SET_SLOT(multip, 0x30, start_m35_30);
    SET_SLOT(module_stub[21], 0x18, start_m21_18);
    SET_SLOT(module_stub[15], 0x38, start_m15_38);
    SET_SLOT(host, 0x264, start_h_264);
    SET_SLOT(host, 0x50, start_h_50);
    SET_SLOT(host, 0x44, h_local_time);
    start_lists = 0;
    (void)start_season;
}

static void op_start(void)
{
    uint32_t which = rnd_n(7), i;
    uint8_t *view = cur->image + 0; (void)view;

    /* the menus' inputs: each zero half the time */
    {
        static const uint16_t w32[] = { 0x1240, 0xb20, 0x268, 0x628, 0x758, 0x5d4, 0x5d8, 0x764, 0x850, 0x84c };
        static const uint16_t w8[] = { 0xb04, 0xb64, 0xb68, 0xb6f, 0xb70 };
        for (i = 0; i < sizeof w32 / sizeof w32[0]; i++)
            *(uint32_t *)(global + w32[i]) = rnd_n(2) ? 0 : (rnd_n(2) ? 1 : rnd());
        for (i = 0; i < sizeof w8 / sizeof w8[0]; i++)
            global[w8[i]] = (uint8_t)(rnd_n(2) ? 0 : rnd());
    }
    *(uint16_t *)(global + 0x5dc) = (uint16_t)(rnd_n(2) ? 0 : rnd());
    *(uint16_t *)(global + 0x129c) = (uint16_t)rnd_n(0x20);
    {
        static const uint16_t rates[] = { 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0xffff };
        *(uint16_t *)(global + 0xc1a) = rates[rnd_n(sizeof rates / sizeof rates[0])];
    }
    /* the aircraft records */
    {
        uint8_t *a = fixtures + 0x10000, *b = fixtures + 0x10100;
        *(uint32_t *)(a + 0x10) = rnd_n(3);
        *(uint32_t *)(a + 0x14) = rnd_n(6);
        *(uint32_t *)(b + 8) = rnd();
        *(uint16_t *)(b + 0xc) = (uint16_t)rnd();
        *(uint32_t *)(global + 0x11a0) = rnd_n(4) ? (uint32_t)(uintptr_t)a : 0;
        *(uint32_t *)(global + 0x119c) = rnd_n(4) ? (uint32_t)(uintptr_t)b : 0;
    }
    *(uint32_t *)(cur->image + 0xfda4) = rnd_n(4) ? rnd_n(9) : 0;
    *(uint32_t *)(cur->image + 0x7fc4) = (uint32_t)(uintptr_t)(fixtures + 0x10200);
    *(uint32_t *)(fixtures + 0x10208) = rnd();

    /* the clock */
    *(uint16_t *)(global + 0x242) = (uint16_t)(rnd_n(4) ? 1 : 0);
    *(uint16_t *)(global + 0x240) = (uint16_t)(1990 + rnd_n(16));
    if (rnd_n(2)) {
        *(uint16_t *)(global + 0x23e) = (uint16_t)(rnd_n(2) ? 0x16d + rnd_n(3) : 1 + rnd_n(0x16e));
        global[0x23a] = (uint8_t)(rnd_n(2) ? 0x3b + rnd_n(3) : rnd_n(0x3c));
        global[0x239] = (uint8_t)(rnd_n(2) ? 0x3b + rnd_n(2) : rnd_n(0x3c));
        global[0x23c] = (uint8_t)(rnd_n(2) ? 0x3b + rnd_n(2) : rnd_n(0x3c));
        global[0x238] = (uint8_t)(rnd_n(2) ? 0x17 + rnd_n(2) : rnd_n(0x18));
        global[0x23b] = (uint8_t)(rnd_n(2) ? 0x17 + rnd_n(2) : rnd_n(0x18));
    }
    *(uint32_t *)(global + 0x128) = rnd_n(3) ? rnd() : 0;
    *(uint16_t *)(global + 0x116e) = (uint16_t)(rnd_n(2) ? 4 : rnd());
    *(uint32_t *)(global + 0x268) = rnd_n(4) ? 0 : 1;
    global[0x365] = (uint8_t)(rnd_n(4) ? 0 : 1);
    *(uint16_t *)(cur->image + 0x6dd0) = (uint16_t)rnd();
    *(uint16_t *)(cur->image + 0x6dd2) = (uint16_t)(rnd_n(2) ? 0xffff - rnd_n(16) : rnd());
    cur->image[0x3c70] = (uint8_t)(rnd_n(2) ? 0xe0 : rnd());
    cur->image[0x3c71] = (uint8_t)(rnd_n(2) ? 0xe0 : rnd());
    cur->image[0x3c72] = (uint8_t)(rnd_n(2) ? 0xe0 : rnd());
    *(int16_t *)(global + 0x574) = (int16_t)rnd();
    for (i = 0; i < 4; i++) {
        static const uint16_t edge[] = { 0xe9a, 0xe9c, 0xea2, 0xea4 };
        *(int16_t *)(global + edge[i]) = rnd_n(3) == 0 ? 0 :
            (int16_t)(*(int16_t *)(global + 0x574) + (int16_t)rnd_n(5) - 2);
    }
    clock_words[0] = (uint16_t)(1990 + rnd_n(16));
    clock_words[1] = (uint16_t)(1 + rnd_n(12));
    clock_words[2] = (uint16_t)(1 + rnd_n(31));
    clock_words[3] = (uint16_t)rnd_n(24);
    clock_words[4] = (uint16_t)rnd_n(60);
    clock_words[5] = (uint16_t)rnd_n(60);
    *(uint32_t *)(cur->image + 0xcff8) = rnd_n(2) ? 0xe + rnd_n(4) : rnd_n(16);
    *(uint32_t *)(cur->image + 0x7874) = rnd_n(2);  /* a `d' record seen: +0x74's two directions */

    if (counting) cover_start[which]++;
    put("\n#start(%u)", which);
    switch (which) {
    case 0: ((s0)internal(0x207ebe70, "main_menu_state@0"))(); break;
    case 1: {
        uint32_t msg = rnd_n(3) ? 0x40 : rnd_n(0x80);
        uint32_t cb = cur == &retail ? (uint32_t)(uintptr_t)internal(0x207ebe40, "")
                                     : ((s0)internal(0, "main_menu_listener_at@0"))();
        put(" msg=%#x", msg);
        call_with_regs(cb, msg, rnd());
        put(" esp %u", cb_esp);
        if (msg != 0x40)                           /* any other message leaves every register */
            put(" regs %08x %08x %08x", cb_eax, cb_ecx, cb_edx);
        break;
    }
    case 2: ((s0)internal(0x207f7900, "main_start_menus@0"))(); break;
    case 3: ((s2)internal(0x207f7a20, "main_start_countdown@8"))(rnd(), rnd()); break;
    case 4: ((s0)internal(0x207f7b70, "main_clock_from_host@0"))(); break;
    case 5: ((s0)internal(0x207f2330, "main_clock_logbook@0"))(); break;
    default: {
        uint32_t b = rnd();
        put("->%#x", ((s2)internal(0x207f7c83, "main_clock_tick@8"))(rnd(), b));
        break;
    }
    }
    put(" clock %u:%u:%u %u:%u day %u year %u season %u text %.8s", global[0x238], global[0x239], global[0x23a],
        global[0x23b], global[0x23c], *(uint16_t *)(global + 0x23e), *(uint16_t *)(global + 0x240),
        *(uint16_t *)(global + 0x248), (char *)global + 0xc20);
    if (counting) {
        if (*(uint32_t *)(cur->image + 0xcff8) == 0 && which == 3) cover_start_minute++;
        if (*(uint16_t *)(global + 0x23e) == 1 && which == 6) cover_start_rollover++;
        if (*(uint8_t *)(cur->image + 0x3c71) == 0x40) cover_start_tally++;
    }
}

static void report_start(void)
{
    printf("  start: menus %lu, listener %lu, greyed %lu, countdown %lu (finished %lu), clock from host %lu, logbook %lu (tally carried %lu), tick %lu (new year %lu)\n",
           cover_start[0], cover_start[1], cover_start[2], cover_start[3], cover_start_minute, cover_start[4],
           cover_start[5], cover_start_tally, cover_start[6], cover_start_rollover);
}

/* ---- suite: radios (CHAIN lists 3 and 5, slice 30) ---------------------------------- */

static uint32_t STDCALL radios_fe_2c(uint32_t which, uint32_t pos, uint8_t *buf)
{
    uint32_t i, ok = rnd_n(5) != 0;
    (void)pos;
    put(" fe+0x2c(%#x)->%u", which & 0xffff, ok);
    for (i = 0; i < 0x190; i++)
        buf[i] = (uint8_t)rnd();
    i = rnd();                                     /* as FE: only a station it finds sets the word */
    if (ok && (i & 0x10000))
        *(uint16_t *)(global + 0x37e) = (uint16_t)i;
    return ok;
}
static uint32_t STDCALL radios_fe_28(uint32_t which, uint32_t cl, uint32_t pos, uint8_t *buf)
{
    uint32_t i, ok = rnd_n(5) != 0;
    (void)pos;
    put(" fe+0x28(%#x,%#x)->%u", which & 0xffff, cl & 0xff, ok);
    for (i = 0; i < 0x190; i++)
        buf[i] = (uint8_t)rnd();
    return ok;
}
static uint32_t STDCALL radios_fe_24(uint32_t which, uint32_t pos, uint32_t *out)
{
    uint32_t ok = rnd_n(3) != 0;
    (void)pos;
    put(" fe+0x24(%#x)->%u", which & 0xffff, ok);
    if (ok)
        *out = rnd();
    return ok;
}
static uint32_t STDCALL radios_fe_30(uint32_t pos)
{
    uint32_t r = rnd();
    (void)pos;
    put(" fe+0x30->%#x", r);
    return r;
}
static uint32_t radios_range;
static uint32_t STDCALL radios_fe_54(uint32_t pos, uint32_t range, uint32_t to)
{
    (void)pos;
    radios_range = range;
    put(" fe+0x54(%#x,", range);
    put_ptr(to);
    put(")");
    return 0;
}
static uint32_t radios_markers, radios_full;
static uint32_t STDCALL radios_fe_20(uint32_t n, uint32_t *rec, uint32_t *other)
{
    uint8_t *r = fixtures + 0x30000 + 0x40 * (radios_markers % 32);
    uint32_t i;

    (void)other;
    n &= 0xffff;                                   /* a word over two stale bytes */
    put(" fe+0x20(%u)", n);
    if (n == 0)
        radios_markers = radios_full ? 30 : rnd_n(3) == 0 ? rnd_n(32) : rnd_n(5);
    if (radios_markers == 0)                       /* the markers test ax alone, the season eax */
        return radios_range == 0x1d4c ? 0xffff0000u : 0;
    radios_markers--;
    for (i = 0; i < 0x40; i++)
        r[i] = (uint8_t)rnd();
    r[0] = radios_full ? (uint8_t)(4 + rnd_n(3)) : (uint8_t)(3 + rnd_n(5));
    *rec = (uint32_t)(uintptr_t)r;
    return 1;
}
static uint32_t STDCALL radios_util_60(uint32_t from, uint32_t to)
{
    put(" util+0x60(");
    put_ptr(from);
    put(",");
    put_ptr(to);
    put(")");
    memmove((void *)(uintptr_t)to, (void *)(uintptr_t)from, 8);
    return 0;
}
static uint32_t STDCALL radios_util_3c(uint32_t x, uint32_t y)
{
    uint32_t r = (x * 31 + y) >> 8;

    if (rnd_n(3) == 0) {                           /* on an edge of the to/from or the glide's cone */
        static const uint16_t edge[] = { 0x38e3, 0x471c, 0xc71d, 0xb8e4, 0xc001, 0x3fff };
        r = (uint32_t)(*(uint16_t *)(cur->image + 0x735f) + edge[rnd_n(6)] + rnd_n(3) - 1);
    }
    put(" util+0x3c(%#x,%#x)->%#x", x, y, r & 0xffff);
    return r;
}
static uint32_t radios_6c, radios_6c_set;
static uint32_t STDCALL radios_util_6c(void)
{
    uint32_t r = radios_6c_set ? radios_6c : rnd();
    put(" util+0x6c->%#x", r);
    return r;
}
uint32_t clean_t_util_6c_v, clean_t_util_60_v, clean_ret60;
void clean_util_6c(void);
void clean_util_60(void);
__asm__(".text\n_clean_util_60:\n\tpopl _clean_ret60\n\tcall *_clean_t_util_60_v\n\t"
        "movl $0x7fff2345, %ecx\n\tmovl $0x22220008, %edx\n\tpushl _clean_ret60\n\tret\n");
__asm__(".text\n_clean_util_6c:\n\tcall *_clean_t_util_6c_v\n\tmovl $0x7fff1234, %ecx\n\tmovl $0x22220007, %edx\n\tret\n");
static uint32_t radios_flat, radios_mag, radios_mag_set, radios_mag_stays;
static uint32_t STDCALL radios_util_18(int64_t *v)
{
    uint32_t r = radios_mag_set ? radios_mag : (uint32_t)((uint64_t)*v >> 31);

    radios_mag_set = radios_mag_stays;
    put(" Umag(%lld)->%u", (long long)*v, r);
    return r;
}
static uint32_t STDCALL radios_util_2c(uint32_t a, uint16_t *s, uint16_t *c)
{
    *s = (uint16_t)rnd();
    *c = radios_flat ? 0 : (uint16_t)rnd();
    put(" util+0x2c(%#x)->%#x,%#x", a & 0xffff, *s, *c);
    return 0;
}
static uint32_t STDCALL radios_sound_24(uint32_t a, uint32_t b)
{
    put(" sound+0x24(%#x,", a);
    put_ptr(b);
    put(")");
    return 0;
}
static uint32_t STDCALL radios_m24_20(uint32_t a)
{
    put(" m24+0x20(%#x)", a);
    return 0;
}

static unsigned long cover_radio[4], cover_radio_masked, cover_radio_lamps, cover_radio_flagged;

static void setup_radios(void)
{
    SET_SLOT(fe, 0x2c, radios_fe_2c);
    SET_SLOT(fe, 0x28, radios_fe_28);
    SET_SLOT(fe, 0x24, radios_fe_24);
    SET_SLOT(fe, 0x30, radios_fe_30);
    SET_SLOT(fe, 0x54, radios_fe_54);
    SET_SLOT(fe, 0x20, radios_fe_20);
    clean_t_util_60_v = (uint32_t)(uintptr_t)radios_util_60;
    SET_SLOT(util, 0x60, clean_util_60);
    SET_SLOT(util, 0x3c, radios_util_3c);
    clean_t_util_6c_v = (uint32_t)(uintptr_t)radios_util_6c;
    SET_SLOT(util, 0x6c, clean_util_6c);
    SET_SLOT(util, 0x2c, radios_util_2c);
    SET_SLOT(util, 0x18, radios_util_18);
    SET_SLOT(sound, 0x24, radios_sound_24);
    SET_SLOT(module_stub[24], 0x20, radios_m24_20);
    util88_all = 1;
}

static void op_radios(void)
{
    uint32_t which = rnd_n(10), i;

    for (i = 0x34c; i < 0x358; i++)
        global[i] = (uint8_t)rnd();
    *(uint16_t *)(global + 0x444) = (uint16_t)(rnd_n(4) ? rnd() & ~2u : rnd());
    global[0x388] = (uint8_t)(rnd_n(3) ? rnd() : 2);
    global[0x389] = (uint8_t)(rnd_n(3) ? rnd() : 2);
    global[0x38a] = (uint8_t)(rnd_n(3) ? 0 : 1);
    *(uint32_t *)(global + 0x38c) = rnd_n(3) ? 0 : 1;
    *(uint16_t *)(global + 0xc1a) = (uint16_t)(rnd_n(8) ? 0x100 << rnd_n(7) : 0);
    global[0xb70] = (uint8_t)(rnd_n(4) ? 0 : 1);
    *(uint32_t *)(global + 0x84c) = rnd_n(3);
    *(uint16_t *)(global + 0xc4e) = (uint16_t)rnd();
    *(uint16_t *)(global + 0xc5e) = (uint16_t)rnd();
    *(uint32_t *)(global + 0x7a8) = rnd_n(3) ? 1 : 0;
    *(uint32_t *)(global + 0x20) = rnd();
    *(uint32_t *)(global + 0x580) = rnd();
    for (i = 0x560; i < 0x578; i++)
        global[i] = (uint8_t)rnd();
    *(int32_t *)(global + 0x574) = rnd_n(2) ? (int32_t)rnd_n(0x3000) : (int32_t)rnd();
    *(uint16_t *)(global + 0xbac) = (uint16_t)rnd_n(2);
    *(uint16_t *)(global + 0xbae) = (uint16_t)rnd_n(2);
    *(uint16_t *)(global + 0xbb0) = (uint16_t)rnd_n(2);
    *(uint32_t *)(global + 0x2ac) = rnd_n(2) ? *(uint32_t *)(cur->image + 0x732e) : rnd();
    /* +0x74 is reached too: a day and a year it can use (the season suite goes wider) */
    *(uint16_t *)(global + 0x23e) = (uint16_t)(1 + rnd_n(365));
    *(uint16_t *)(global + 0x240) = (uint16_t)(1990 + rnd_n(16));

    seed_image(0x7248, 0x72dc);
    if (op_number == 0 || rnd_n(3) == 0) {
        seed_image(0x72dc, 0x7300);
        seed_image(0x7341, 0x7480);
        seed_image(0x7480, 0x7489);
        for (i = 0; i < 6; i++) {
            /* the boxes: each field near the aircraft or far */
            uint32_t *r = (uint32_t *)(cur->image + 0x748a + 0x18 * i);
            r[0] = *(uint32_t *)(global + 0x564) + rnd_n(0x200) - 0x100;
            r[1] = *(uint32_t *)(global + 0x56c) + rnd_n(0x200) - 0x100;
            r[2] = *(uint32_t *)(global + 0x574) + rnd_n(0x200) - 0x100;
            r[3] = rnd_n(0x200);
            r[4] = rnd_n(0x200);
        }
        *(uint16_t *)(cur->image + 0x746c) = (uint16_t)(rnd_n(8) ? rnd() | 0x100 : rnd_n(3));
        *(uint16_t *)(cur->image + 0x7474) = (uint16_t)rnd();
        cur->image[0x746b] = (uint8_t)rnd();
        cur->image[0x7476] = (uint8_t)(1 + rnd_n(3));
        cur->image[0x7477] = (uint8_t)(1 + rnd_n(3));
        *(uint16_t *)(cur->image + 0x7478) = (uint16_t)(1 + rnd_n(3));
        global[0xc43] = (uint8_t)(1 + rnd_n(3));
        cur->image[0x786e] = (uint8_t)(rnd_n(2) ? 0 : rnd());
        cur->image[0x786f] = (uint8_t)(rnd_n(2) ? 0 : rnd());
        cur->image[0x7334] = (uint8_t)rnd_n(3);
        cur->image[0x7050] = (uint8_t)rnd_n(4);
        /* the marker table */
        memset(cur->image + 0x7522, 0, 0x7842 - 0x7522);
        for (i = rnd_n(5); i-- != 0; ) {
            uint32_t *e = (uint32_t *)(cur->image + 0x7522 + 0x20 * i);
            e[0] = 4 + rnd_n(3);
            e[1] = rnd();
            e[2] = rnd();
            e[3] = rnd();
            e[4] = rnd();
            e[6] = *(uint32_t *)(global + 0x574) - rnd_n(0x1000);
            e[7] = rnd_n(0x100);
        }
    }

    /* steered: the boundaries each routine compares with */
    radios_full = rnd_n(6) == 0;
    radios_flat = 0;
    radios_mag_set = 0;
    radios_mag_stays = 0;
    radios_6c_set = 0;
    for (i = 0xc48; i < 0xc62; i++)
        global[i] = (uint8_t)(rnd_n(2) ? global[i] : rnd());
    if (rnd_n(2))                                  /* a needle that just reaches its limit */
        *(uint16_t *)(cur->image + (rnd_n(2) ? 0x746e : 0x7470)) = (uint16_t)(0xf0 + rnd_n(0x60));
    *(uint32_t *)(cur->image + 0x730c) = *(uint32_t *)(global + 0x574) + rnd_n(0x100);   /* below the slope's top: no angle */
    if (rnd_n(2)) {
        static const int16_t edge[] = { 0x641, (int16_t)0xf9bf };
        *(int16_t *)(cur->image + 0x738b) = (int16_t)(edge[rnd_n(2)] + (int16_t)rnd_n(3) - 1);
    }
    switch (rnd_n(8)) {
    case 0:                                        /* a station's height unknown: the sentinel */
        *(uint32_t *)(cur->image + 0x7355) = 0xffffd8f1u;
        break;
    case 1:                                        /* the bearing's scaling stops at its limit */
        *(uint32_t *)(cur->image + 0x7345) = *(uint32_t *)(global + 0x564) - 0xc0000000u;
        break;
    case 2: {                                      /* the mask: class, height and range on their edges */
        static const uint16_t cls[] = { 0, 0x19, 0x1a, 0x31, 0x32, 0x40, 0x10, 0x20 };
        static const int32_t alts[] = { 0xf4, 0xf5, 0x7be, 0x7bf, 0xb50, 0xb51, 0x1144, 0x1145, 0x2581, 0x2582 };
        static const uint32_t dists[] = { 0x15b4, 0x15b5, 0x1458c, 0x1458d, 0x2d370, 0x2d371, 0x3d0aa, 0x3d0ab, 0x3d0ac, 0x90000 };
        *(uint16_t *)(cur->image + 0x7361) = cls[rnd_n(8)];
        *(int32_t *)(cur->image + 0x7355) = *(int32_t *)(global + 0x574) - alts[rnd_n(10)];
        radios_mag = dists[rnd_n(10)];
        radios_mag_set = 1;
        cur->image[0x735d] = (uint8_t)(cur->image[0x735d] & 0x77);
        *(uint16_t *)(cur->image + 0x7359) = 1;
        cur->image[0x786e] = 0;
        break;
    }
    case 3: {                                      /* the DME's speed on its digit boundary */
        uint32_t r = rnd_n(4) ? rnd_n(0x100000) : rnd_n(8);      /* or no range at all */
        static const uint16_t dist[] = { 411, 412, 413 };
        if (rnd_n(2)) {                            /* the last reading blank: this one is not shown */
            cur->image[0x72e4] = 0;
            cur->image[0x72e5] = 0;
        }
        cur->image[0x72dc] &= 0xfe;
        radios_mag = r;
        radios_mag_set = 1;
        *(uint32_t *)(cur->image + 0x747a) = r + dist[rnd_n(3)];
        *(uint16_t *)(global + 0xc1a) = 0x100;
        *(uint16_t *)(cur->image + 0x738d) = 0;
        *(uint16_t *)(cur->image + 0x7359) = 1;
        cur->image[0x735d] |= 1;
        cur->image[0x7476] = 1;
        global[0xc43] = 1;
        global[0xb70] = 0;
        *(uint32_t *)(global + 0x84c) = 1;
        break;
    }
    case 4: {                                      /* one marker, alone, on its cone's edge */
        uint32_t *e = (uint32_t *)(cur->image + 0x7522);
        static const uint32_t over[] = { 0x100, 0xbe8, 0xbe7 };
        memset(cur->image + 0x7522, 0, 0x40);
        e[0] = 4 + rnd_n(3);
        e[6] = *(uint32_t *)(global + 0x574) - over[rnd_n(3)];
        e[7] = rnd_n(0x100);
        radios_flat = 1;
        radios_mag = 0x132 + rnd_n(3);
        radios_mag_set = 1;
        *(uint16_t *)(cur->image + 0x7359) = 0;
        *(uint16_t *)(cur->image + 0x73b1) = 0;
        *(uint16_t *)(cur->image + 0x7409) = 0;
        *(uint16_t *)(cur->image + 0x738d) = 0;
        *(uint16_t *)(cur->image + 0x73e5) = 0;
        cur->image[0x7334] = 0;
        *(uint32_t *)(global + 0x7a8) = 1;
        if (which < 4) which += 4;
        break;
    }
    case 5: {                                      /* a retune box exactly on its height */
        uint32_t *r = (uint32_t *)(cur->image + 0x748a + 0x18 * rnd_n(6));
        uint32_t k = rnd_n(0x100);
        r[0] = *(uint32_t *)(global + 0x564);
        r[1] = *(uint32_t *)(global + 0x56c);
        r[2] = *(uint32_t *)(global + 0x574) + k;
        r[3] = 0x7fffffff;
        r[4] = k + rnd_n(2);
        break;
    }
    case 6: {                                      /* the aircraft above the slope: an angle, or on its limit */
        uint32_t mode = rnd_n(3), h, r;
        if (mode == 0) {
            h = 0x100 + rnd_n(0x4000);
            r = h * 8 + rnd_n(0x100);
            *(uint16_t *)(cur->image + 0x738b) = (uint16_t)((h << 16) / (uint16_t)(r >> 1) + rnd_n(0x800) - 0x400);
        } else if (mode == 1) {                    /* twice the height just reaches half the range */
            h = 1 + rnd_n(0x7000);
            r = h * 4;
            *(uint16_t *)(cur->image + 0x738b) = 0;
        } else {                                   /* twice the height carries out of the word */
            h = 0x8000 + rnd_n(0x4000);
            r = 2 * (2 * (h - 0x8000) + 1 + rnd_n(0x1000));
            *(uint16_t *)(cur->image + 0x738b) = 0;
        }
        *(uint32_t *)(cur->image + 0x730c) = *(uint32_t *)(global + 0x574) - h;
        radios_mag = r;
        radios_mag_set = 1;
        radios_mag_stays = 1;
        *(uint16_t *)(cur->image + 0x7359) = 1;
        cur->image[0x735d] |= 0x40;
        cur->image[0x7469] = 1;
        if (which < 4) which += 4;
        break;
    }
    case 7: {                                      /* the mask: exactly on a line, or a unit from a whole step */
        static const struct { uint16_t cls; int32_t lo, hi; uint32_t a8, ac, b0, b4, b8, bc; } line[] = {
            { 0x10, 1, 0xf4, 0, 0x155c80c, 0, 0, 0x25f9c8b, 0 },
            { 0x10, 0xf5, 0x7be, 0xf4, 0x55476, 0xb4dc, 0x98, 0xf30b7, 0xb4dc },
            { 0x10, 0x7bf, 0x1000, 0x7be, 0xfffaab8au, 0xd908, 0x7be, 0xfff0cf49u, 0xfd34 },
            { 0x20, 0xf5, 0xb50, 0xf4, 0x6fbea, 0x12160, 0x98, 0xd80a2, 0x12160 },
            { 0x20, 0xb51, 0x1000, 0xb50, 0xfff90416u, 0x1458c, 0xb50, 0xfff27f5eu, 0x169b8 },
            { 0x40, 0xf5, 0x1144, 0xf4, 0x27ea84, 0x1458c, 0x98, 0x293b14, 0x169b8 },
            { 0x40, 0x1145, 0x2581, 0x1144, 0x725fc, 0x3d0a4, 0x1144, 0xe4bf8, 0x3f4d0 },
            { 0x40, 0x2582, 0x4000, 0x2581, 0xfff998a8u, 0x46154, 0x2581, 0xfff55715u, 0x51630 },
        };
        uint32_t l = rnd_n(8), k = 1 + rnd_n(8), e1, e2, span;
        int32_t alt = line[l].lo + (int32_t)rnd_n((uint32_t)(line[l].hi - line[l].lo + 1));

        e1 = (uint32_t)((uint64_t)((int64_t)(alt - (int32_t)line[l].a8) * (int32_t)line[l].ac) >> 16) + line[l].b0;
        e2 = (uint32_t)((uint64_t)((int64_t)(alt - (int32_t)line[l].b4) * (int32_t)line[l].b8) >> 16) + line[l].bc;
        span = (e2 - e1) >> 8;
        *(uint16_t *)(cur->image + 0x7361) = line[l].cls;
        *(int32_t *)(cur->image + 0x7355) = *(int32_t *)(global + 0x574) - alt;
        radios_mag = rnd_n(3) == 0 ? e1 : e2 - (span * k - 1);
        radios_mag_set = 1;
        radios_6c = (k - 1 + ((k - 1) >> 4) + 1) << 8;
        radios_6c_set = 1;
        cur->image[0x735d] = (uint8_t)(cur->image[0x735d] & 0x77);
        *(uint16_t *)(cur->image + 0x7359) = 1;
        cur->image[0x786e] = 0;
        break;
    }
    default:
        break;
    }

    if (counting) cover_radio[which < 4 ? 0 : which < 8 ? 1 : which - 6]++;
    put("\n#radio(%u) 735d=%#x 7359=%#x 7469=%u 735f=%#x", which, cur->image[0x735d], *(uint16_t *)(cur->image + 0x7359),
        cur->image[0x7469], *(uint16_t *)(cur->image + 0x735f));
    if (which < 4) {
        ((s2)internal(0x207f9261, "main_radio_list3@8"))(rnd(), rnd());   /* CHAIN ignores its eax */
    } else if (which < 8) {
        ((s2)internal(0x207f9344, "main_radio_list5@8"))(rnd(), rnd());
    } else if (which == 8) {
        ((s0)internal(0x207f916e, "main_radios_start@0"))();
        put(" 384=");                              /* an image pointer, in GLOBAL */
        put_ptr(*(uint32_t *)(global + 0x384));
        *(uint32_t *)(global + 0x384) = 0;
    } else {
        ((s0)internal(0x207f9208, "main_radios_stop@0"))();
    }
    put(" after 7469=%u", cur->image[0x7469]);
    put(" needles %d %d %d %d %d %d adf %#x gs %d text %.5s %.3s %.5s %.3s lamps %u%u%u",
        (int8_t)global[0xc48], global[0xc4b], (int8_t)global[0xc49], (int8_t)global[0xc59], global[0xc5b],
        *(uint16_t *)(global + 0xc50), *(uint16_t *)(global + 0xc6a), global[0xc4c], (char *)global + 0xc29,
        (char *)global + 0xc2e, (char *)global + 0xc33, (char *)global + 0xc38, *(uint16_t *)(global + 0xbac),
        *(uint16_t *)(global + 0xbae), *(uint16_t *)(global + 0xbb0));
    if (counting) {
        if (*(uint16_t *)(global + 0xbac) | *(uint16_t *)(global + 0xbae) | *(uint16_t *)(global + 0xbb0)) cover_radio_lamps++;
        if (cur->image[0x735d] & 0x80) cover_radio_flagged++;
        if (*(uint16_t *)(cur->image + 0x7359) == 0) cover_radio_masked++;
    }
}

static void report_radios(void)
{
    printf("  radios: list 3 %lu, list 5 %lu, start %lu, stop %lu; lamps lit %lu, localiser %lu, NAV1 dead %lu\n",
           cover_radio[0], cover_radio[1], cover_radio[2], cover_radio[3], cover_radio_lamps, cover_radio_flagged,
           cover_radio_masked);
}

/* ---- suite: weather (the layers at start, slice 31) --------------------------------- */

static uint32_t STDCALL weather_n_get(uint32_t v, uint32_t type, uint16_t *out)
{
    uint16_t r;

    if (in_image(v))                               /* a value field's own address */
        v -= (uint32_t)(uintptr_t)cur->image;
    r = (uint16_t)(v + type);
    put(" N1c(%#x,%#x)->%#x c18=%#x", v, type, r, *(uint16_t *)(global + 0xc18));
    *out = r;
    return 0;
}
static uint32_t STDCALL weather_n_put(uint16_t *in, uint32_t type, uint32_t to)
{
    if (in_image(to))
        to -= (uint32_t)(uintptr_t)cur->image;
    put(" N18(%#x,%#x,%#x)", *in, type, to);
    return 0;
}
static uint32_t STDCALL weather_h_2a4(uint32_t id, void **out)
{
    char *p = arena_get(0x40);
    put(" host+0x2a4(%#x)", id);
    if (id == 0x2032)                              /* the texts Create a Flight fills in */
        strcpy(p, "%s at %s. ");
    else if (id == 0x2041)
        strcpy(p, "%d mi");
    else if (id == 0x2042)
        strcpy(p, "%d.%02d mi");
    *out = p;
    return 0;
}
typedef int32_t (STDCALL *weather_cmp)(uint32_t, uint32_t);
static uint32_t STDCALL weather_h_22c(uint8_t *base, uint32_t n, uint32_t size, weather_cmp cmp)
{
    static uint8_t tmp[0x100];
    uint32_t i, j;

    put(" sort(");
    put_ptr((uint32_t)(uintptr_t)base);
    put(",%u,%#x)", n, size);
    for (i = 1; i < n; i++) {
        memcpy(tmp, base + i * size, size);
        for (j = i; j > 0; j--) {
            int32_t r = cmp((uint32_t)(uintptr_t)(base + (j - 1) * size), (uint32_t)(uintptr_t)tmp);

            put(" cmp=%d", r);
            if (r <= 0)
                break;
            memcpy(base + j * size, base + (j - 1) * size, size);
        }
        memcpy(base + j * size, tmp, size);
    }
    return 0;
}

static unsigned long cover_weather, cover_weather_swapped, cover_weather_lifted;

static void setup_weather(void)
{
    SET_SLOT(module_stub[17], 0x1c, weather_n_get);
    SET_SLOT(module_stub[17], 0x18, weather_n_put);
    SET_SLOT(host, 0x2a4, weather_h_2a4);
    SET_SLOT(host, 0x22c, weather_h_22c);
}

/* A value field for NUMERIC's model: few enough that layers meet, and a high
 * word a compare must not look at. */
static uint32_t weather_value(void)
{
    return rnd_n(8) ? 0x1000 + rnd_n(4) + (rnd_n(4) ? 0 : rnd_n(3) << 16) : 0;
}

static void op_weather(void)
{
    uint32_t i;

    /* the tables are full of relocated pointers, which stay */
    seed_image(0x7b50, 0x7f44);
    *(uint32_t *)(cur->image + 0x7b50) = rnd_n(2);
    *(uint32_t *)(cur->image + 0x7b54) = rnd_n(8) ? 0 : 1;
    *(uint32_t *)(cur->image + 0x7b58) = rnd_n(8) ? 0 : 1;
    for (i = 0; i < 3; i++) {
        uint8_t *r = cur->image + 0x7b64 + 0x58 * i;
        *(uint32_t *)(r + 0x54) = rnd_n(4) ? 1 : 0;
        *(uint32_t *)(r + 0x40) = rnd_n(13);
        *(uint16_t *)(r + 0x3c) = (uint16_t)rnd_n(10);
        *(uint32_t *)(r + 4) = weather_value();
        *(uint32_t *)(r + 0x18) = weather_value();
        *(uint32_t *)(r + 0x2c) = weather_value();
    }
    for (i = 0; i < 3; i++)
        *(uint32_t *)(cur->image + 0x7ca8 + 0x64 * i + 0x60) = rnd_n(4) ? 1 : 0;
    for (i = 0; i < 4; i++)
        *(uint32_t *)(cur->image + 0x7e38 + 0x2c * i + 0x28) = rnd_n(3) ? 1 : rnd_n(2) * 2;
    /* the relevant pointer fields go to fixture words, for NUMERIC's model */
    for (i = 0; i < 3; i++) {
        uint8_t *r = cur->image + 0x7ca8 + 0x64 * i;
        *(uint32_t *)(r + 4) = weather_value();
        *(uint32_t *)(r + 0x18) = weather_value();
        *(uint32_t *)(r + 0xc4 - 0x64) = *(uint32_t *)(r + 0x60);
    }
    for (i = 0; i < 4; i++)
        *(uint32_t *)(cur->image + 0x7e38 + 0x2c * i + 4) = weather_value();
    *(uint32_t *)(cur->image + 0x7de0) = rnd_n(3);  /* the layer the sort keeps the others above */
    *(uint32_t *)(cur->image + 0x7de8) = weather_value();
    *(uint32_t *)(cur->image + 0xd12c) = rnd();
    *(uint32_t *)(global + 0xf18) = rnd_n(2);      /* the table has two rows */
    global[0xf12] = (uint8_t)(rnd_n(4) ? 0 : 1);
    global[0xf13] = (uint8_t)rnd();
    *(uint16_t *)(global + 0xc18) = (uint16_t)rnd();
    for (i = 0xfc0; i < 0xfc4; i++)
        global[i] = (uint8_t)rnd();
    for (i = 0x10a8; i < 0x10ac; i++)
        global[i] = (uint8_t)rnd();
    *(uint32_t *)(cur->image + 0x7b5c + 0x140) = rnd_n(4);
    if (counting) cover_weather++;
    put("\n#weather mode %u", *(uint32_t *)(global + 0xf18));
    ((s0)internal(0x207fb5f0, "main_weather_load@0"))();
    put(" c18=%#x", *(uint16_t *)(global + 0xc18));
    if (counting) {
        uint8_t *a = global + 0xf1c;
        if (*(int16_t *)(a + 2) > *(int16_t *)(a + 0xa)) cover_weather_swapped++;
        if (*(uint32_t *)(cur->image + 0x7ca8 + 0x60) == 0) cover_weather_lifted++;
    }
}

static void report_weather(void)
{
    printf("  weather: %lu runs, winds in order %lu, a layer dropped %lu\n", cover_weather, cover_weather_swapped,
           cover_weather_lifted);
}

/* ---- suite: kneeboard (slice 32) --------------------------------------------------- */

static uint8_t *kb_rec(void) { return fixtures + 0x40000; }
static uint32_t kb_pages, kb_lines;

static uint32_t STDCALL kb_w78(uint32_t *to, const char *title, uint32_t a)
{
    uint8_t *r = kb_rec();
    uint32_t y, *rows = (uint32_t *)(fixtures + 0x41000);

    put(" window+0x78(%s,%u)", title, a);
    memset(r, 0, 0x200);
    *(uint32_t *)(r + 8) = rnd_n(2) ? 0x80000000u : 0;
    *(uint32_t *)(r + 0x38) = 0xbd;
    *(uint32_t *)(r + 0x3c) = 0x12a;
    *(uint32_t *)(r + 0x60) = 0xbd;
    *(uint32_t *)(r + 0x64) = 0x12a;
    *(uint32_t *)(r + 0x84) = 0xbd;
    *(uint32_t *)(r + 0x88) = 0x12a;
    *(uint32_t *)(r + 0x90) = (uint32_t)(uintptr_t)rows;
    *(uint32_t *)(r + 0x90 + 0) = (uint32_t)(uintptr_t)rows;
    *(uint32_t *)(r + 0x80 + 0x10) = (uint32_t)(uintptr_t)rows;
    for (y = 0; y < 0x12a; y++)
        rows[y] = (uint32_t)(uintptr_t)(fixtures + 0x42000 + y * 0xc0);
    *to = (uint32_t)(uintptr_t)r;
    return 0;
}
#define KB_LOG1(name, text) static uint32_t STDCALL name(uint32_t a) { put(" " text "("); put_ptr(a); put(")"); return 0; }
KB_LOG1(kb_w68, "window+0x68")
KB_LOG1(kb_w6c, "window+0x6c")
KB_LOG1(kb_w7c, "window+0x7c")
static uint32_t kb_h64_fails;
static uint32_t STDCALL kb_h64(uint32_t a)
{
    put(" host+0x64(");
    put_ptr(a);
    put(")");
    return kb_h64_fails;
}
KB_LOG1(kb_h68, "host+0x68")
KB_LOG1(kb_h7c, "host+0x7c")
KB_LOG1(kb_h25c, "host+0x25c")
static uint32_t STDCALL kb_h27c(uint32_t r, uint32_t c, uint32_t a, uint32_t b)
{
    put(" host+0x27c(");
    put_ptr(r);
    put(",");
    put_ptr(c);
    put(",%u,%u) d=%08x", a, b, digest((uint8_t *)(uintptr_t)c, 0x24, 0x811c9dc5u));
    return 0;
}
static uint32_t STDCALL kb_h284(uint32_t r, uint32_t c)
{
    put(" host+0x284(");
    put_ptr(r);
    put(",");
    put_ptr(c);
    put(")");
    return 0;
}
static uint32_t STDCALL kb_h318(uint32_t r, int32_t *rc, uint32_t a, uint32_t b, const char *text, uint32_t f, uint32_t z)
{
    (void)r;
    /* the title's bottom is retail's stack garbage on the way in; a line's is set */
    put(" text(%d,%d,%d,%d,%d,%d,%s,%u,%u)", rc[0], rc[1], rc[2], f == 2 ? rc[3] : 0, (int32_t)a, (int32_t)b, text, f, z);
    rc[2] = rc[0] + 6 * (int32_t)strlen(text);
    rc[3] = rc[1] + 12;
    return 0;
}
static uint32_t STDCALL kb_h88(uint32_t r, int32_t *rc)
{
    (void)r;
    put(" host+0x88(%d,%d,%d,%d) rows=%08x", rc[0], rc[1], rc[2], rc[3], digest(fixtures + 0x42000, 0x12a * 0xc0, 0x811c9dc5u));
    return 0;
}
static uint32_t STDCALL kb_h244(uint32_t a, uint32_t b, const char *t, uint32_t c)
{
    put(" add(%#x,%#x,%s,%u)", a, b, t, c);
    return 0;
}
static uint32_t STDCALL kb_hf0(const char *section, const char *key, char *out, uint32_t size, const char *path, uint32_t ext)
{
    int k = atoi(key);

    put(" ini(%s,%s,%#x,%s,%#x)", section, key, size, path, ext);
    if (strcmp(section, "lists") == 0 || strcmp(section, "CHECKLISTS") == 0) {
        if ((uint32_t)k >= kb_pages) {
            out[0] = 0;
            return rnd_n(3) ? 0xffffffffu : 0;     /* no key, or an empty one */
        }
        sprintf(out, rnd_n(3) ? "  Page %d , \tShort%d  " : "Page%d", k, k);
        return (uint32_t)strlen(out);
    }
    if ((uint32_t)k >= kb_lines) {
        out[0] = 0;
        return 0;
    }
    sprintf(out, "%s line %d", section, k);
    return (uint32_t)strlen(out);
}
static uint32_t STDCALL kb_hc8(const char *a, const char *b, char *out, uint32_t n)
{
    uint32_t have = rnd_n(3);

    put(" prefs-read(%s,%s,%#x)->%u", a, b, n, have);
    if (have == 0)
        return 0;
    sprintf(out, have == 1 ? "%d,%d,%d" : "%d, -1 ,%d", rnd_n(3), (int)rnd_n(500) - 50, (int)rnd_n(500) - 50);
    return 1;
}
static uint32_t STDCALL kb_hcc(const char *a, const char *b, const char *v)
{
    put(" prefs-write(%s,%s,%s)", a, b, v);
    return 0;
}
static uint32_t STDCALL kb_m23_30(uint32_t a, uint32_t b, char *path, uint32_t n)
{
    uint32_t ok = rnd_n(6) != 0;
    (void)a;
    put(" m23+0x30(%u,%#x)->%u", b, n, ok);
    if (ok)
        strcpy(path, "c:\\plane\\panel.cfg");
    return ok;
}
static uint32_t STDCALL kb_h2a8(char *d, const char *s)
{
    strcpy(d, s);
    return 0;
}
static uint32_t STDCALL kb_h218(uint8_t *d, int32_t v, uint32_t n)
{
    memset(d, v, n);
    return 0;
}
static uint32_t STDCALL kb_h3c(uint32_t a, uint32_t b)
{
    put(" post(%#x,%#x)", a, b);
    return 0;
}

static unsigned long cover_kb[9], cover_kb_pages;

static void kneeboard_stubs(void)
{
    uint32_t i;

    SET_SLOT(module_stub[10], 0x78, kb_w78);
    SET_SLOT(module_stub[10], 0x7c, kb_w7c);
    SET_SLOT(module_stub[10], 0x68, kb_w68);
    SET_SLOT(module_stub[10], 0x6c, kb_w6c);
    SET_SLOT(host, 0x64, kb_h64);
    SET_SLOT(host, 0x68, kb_h68);
    SET_SLOT(host, 0x7c, kb_h7c);
    SET_SLOT(host, 0x25c, kb_h25c);
    SET_SLOT(host, 0x27c, kb_h27c);
    SET_SLOT(host, 0x284, kb_h284);
    SET_SLOT(host, 0x318, kb_h318);
    SET_SLOT(host, 0x88, kb_h88);
    SET_SLOT(host, 0x244, kb_h244);
    SET_SLOT(host, 0xf0, kb_hf0);
    SET_SLOT(host, 0xc8, kb_hc8);
    SET_SLOT(host, 0xcc, kb_hcc);
    SET_SLOT(host, 0x2a8, kb_h2a8);
    SET_SLOT(host, 0x218, kb_h218);
    SET_SLOT(host, 0x3c, kb_h3c);
    SET_SLOT(acontain, 0x30, kb_m23_30);
    handles_differ = 1;
    for (i = 0; i < 256; i++)
        fixtures[0x3ff00 + i] = (uint8_t)(i * 3);
    *(uint32_t *)(hg2d + 0x90) = (uint32_t)(uintptr_t)(fixtures + 0x3ff00);
    IMG32(0x7fc0) = (uint32_t)(uintptr_t)cur->module;
}

static void setup_kneeboard(void)
{
    kneeboard_stubs();
    ((s0)internal(0x207e2320, "main_kb_create@0"))();
}

static void op_kneeboard(void)
{
    uint32_t which = rnd_n(9);
    uint8_t *r = kb_rec();

    kb_pages = rnd_n(5);
    kb_lines = rnd_n(6);
    *(uint32_t *)(r + 8) = (rnd_n(2) ? 0x80000000u : 0) | (rnd_n(3) == 0 ? 0x40000000u : 0) |
                           (rnd_n(2) ? 0x5000000u : 0);
    *(uint32_t *)(global + 0x129c) = rnd_n(4);
    if (rnd_n(2))
        *(uint32_t *)(cur->image + 0x7fc8) = rnd_n(3);
    if (counting) cover_kb[which]++;
    put("\n#kb(%u) pages=%u lines=%u", which, kb_pages, kb_lines);
    switch (which) {
    case 0: put("->%u", ((s0)internal(0x207e1c30, "main_kb_load@0"))()); break;
    case 1: ((s2)internal(0x207e1fc0, "main_kb_listener@8"))(0x60, 0); break;
    case 2: ((s2)internal(0x207e1fc0, "main_kb_listener@8"))(0x70, 0); break;
    case 3: {
        uint32_t x = rnd_n(2) ? 0x1f9c + rnd_n(3) + (rnd_n(2) ? 0xc8 : 0) - 1 : rnd_n(0x3000);
        uint32_t y = rnd_n(2) ? 0x179c + rnd_n(3) + (rnd_n(2) ? 0xc8 : 0) - 1 : rnd_n(0x3000);
        *(uint32_t *)(r + 0x20) = x;
        *(uint32_t *)(r + 0x24) = y;
        *(uint32_t *)(r + 0x28) = 0;
        *(uint32_t *)(r + 0x2c) = 0;
        ((s4)internal(0x207e1b80, "main_kb_window@16"))(rnd_n(2) ? 2 : rnd_n(4), (uint32_t)(uintptr_t)r,   /* its eax is junk */
            rnd_n(3) == 0 ? 0 : rnd_n(2) ? 1 + rnd_n(0xffff) : rnd() | 0x10000, rnd());
        break;
    }
    case 4: put("->%u", ((s2)internal(0x207e2590, "main_kb_close@8"))(0, 0)); break;
    case 8: {                                      /* a page from the menu */
        uint32_t page = rnd_n(3), list = page * 0x2108;

        *(uint32_t *)(r + 0x10) = 0;
        *(uint32_t *)(cur->image + 0x11eac + list) = rnd_n(4);
        ((s2)SVC(0x20))(0x300 + page, rnd());
        put(" 129c=%u", *(uint32_t *)(global + 0x129c));
        break;
    }
    case 5: {
        static char buf[0x100];
        static const char *const texts[] = { "", " a ", "\t\t", "a,b", " a b ,c", "abc", ",x", "  ,  " };
        strcpy(buf, texts[rnd_n(8)]);
        ((void (STDCALL *)(char *))internal(0x207e1a80, "main_kb_trim@4"))(buf);
        put("[%s]", buf);
        break;
    }
    case 6: {
        static char buf[0x100], out[0x100];
        static const char *const texts[] = { "", " a ", "a,b", " a b ,c", ",x", "  ,  ", "\tq\t,r," };
        uint32_t n;
        strcpy(buf, texts[rnd_n(7)]);
        memset(out, 0, sizeof out);
        n = ((s3)internal(0x207e1af0, "main_kb_field@12"))((uint32_t)(uintptr_t)buf, (uint32_t)(uintptr_t)out, 0x2c);
        put("[%s]->%d", out, n ? (int)(n - (uint32_t)(uintptr_t)buf) : -1);
        break;
    }
    default: {
        uint32_t page = rnd_n(3), list = page * 0x2108, n = rnd_n(6), j;

        *(uint32_t *)(r + 0x10) = 0;
        *(uint32_t *)(cur->image + 0xfda4) = 1;
        if (rnd_n(2))                              /* short enough that the rules reach its last rows */
            *(uint32_t *)(r + 0x3c) = 0x3c + rnd_n(0x60);
        sprintf((char *)cur->image + 0xdda4 + (page << 8), "Title %u", page);
        *(uint32_t *)(cur->image + 0x11eac + list) = n;
        for (j = 0; j < n; j++)
            sprintf((char *)cur->image + 0x10eac + list + j * 0x100, "Line %u", j);
        ((s1)internal(0x207e2620, "main_kb_draw@4"))(page);
        break;
    }
    }
    put(" page=%u count=%u flags=%#x", *(uint32_t *)(cur->image + 0x7fc8), *(uint32_t *)(cur->image + 0xfda4), *(uint32_t *)(r + 8));
    if (counting) cover_kb_pages += *(uint32_t *)(cur->image + 0xfda4);
}

static void report_kneeboard(void)
{
    printf("  kneeboard: load %lu, list 0x60 %lu, 0x70 %lu, window %lu, close %lu, trim %lu, field %lu, draw %lu; pages seen %lu\n",
           cover_kb[0], cover_kb[1], cover_kb[2], cover_kb[3], cover_kb[4], cover_kb[5], cover_kb[6], cover_kb[7], cover_kb_pages);
}

static uint32_t session_line, session_type;
#define SESSION_0(name, text) static uint32_t STDCALL name(void) { put(" " text); return 0; }
#define SESSION_1(name, text) static uint32_t STDCALL name(uint32_t a) { put(" " text "("); put_ptr(a); put(")"); return 0; }
static uint32_t STDCALL ses_h23c(uint32_t a)
{
    put(" menus(");
    put_ptr(a);
    put(")");
    *(uint32_t *)(global + 0xf18) = rnd_n(2);      /* the preferences have just been read: a mode the weather has a row for */
    return 0;
}
SESSION_0(ses_h240, "menus-off")
SESSION_0(ses_h114, "host+0x114")
SESSION_0(ses_h28, "host+0x28")
SESSION_0(ses_s18, "sound+0x18")
SESSION_0(ses_s1c, "sound+0x1c")
SESSION_0(ses_s20, "sound+0x20")
SESSION_0(ses_m16_4c, "m16+0x4c")
SESSION_0(ses_m16_50, "m16+0x50")
SESSION_0(ses_m23_2c, "m23+0x2c")
SESSION_1(ses_m23_28, "m23+0x28")
SESSION_0(ses_w18, "window+0x18")
SESSION_0(ses_h18, "hg2d+0x18")
SESSION_0(ses_wm24, "weatherm+0x24")
/* The radios leave an image pointer in GLOBAL +0x384; the digests want it side-free. */
static uint32_t STDCALL ses_chain_add(uint32_t list, uint32_t cb)
{
    uint32_t v = *(uint32_t *)(global + 0x384);

    if (in_image(v))
        *(uint32_t *)(global + 0x384) = v - (uint32_t)(uintptr_t)cur->image;
    return chain_add(list, cb);
}
static uint32_t STDCALL ses_h98(uint32_t name, char *out, uint32_t n)
{
    uint32_t r = session_line ? 0 : 1;
    (void)name;
    put(" option(%#x)->%u", n, r);
    if (r == 0)
        strcpy(out, "c:\\f\\mine.flt");
    return r;
}
static uint32_t STDCALL ses_h188(const char *in, char *file, char *dir, char *ext, uint32_t *type)
{
    put(" split(%s)", in);
    strcpy(file, "mine");
    strcpy(dir, "c:\\f\\");
    strcpy(ext, ".flt");
    *type = session_type;
    return 0;
}
static uint32_t STDCALL ses_h14c(const char *path, uint32_t ext)
{
    put(" delete(%s,%u)", path, ext);
    return 0;
}
static uint32_t STDCALL ses_h10c(uint32_t id, uint32_t *block)
{
    put(" dialog(%#x)", id);
    (void)block;
    return 0;
}
static uint32_t STDCALL ses_m7_28(uint32_t p, uint32_t a)
{
    put(" m7+0x28(");
    put_ptr(p);
    put(",%u)", a);
    return 0;
}

/* ---- suite: session (+0x28 and +0x2c, slice 33) ------------------------------------- */

static void setup_session(void)
{
    setup_situations();
    setup_prefs();
    setup_start();
    setup_radios();
    setup_weather();
    kneeboard_stubs();
    setup_prefs();                                 /* the CFG's readers, over the kneeboard's models */
    SET_SLOT(chain, 0x18, ses_chain_add);
    session_empty_strings = 1;
    SET_SLOT(host, 0x23c, ses_h23c);
    SET_SLOT(host, 0x240, ses_h240);
    SET_SLOT(host, 0x114, ses_h114);
    SET_SLOT(host, 0x28, ses_h28);
    SET_SLOT(host, 0x98, ses_h98);
    SET_SLOT(host, 0x188, ses_h188);
    SET_SLOT(host, 0x14c, ses_h14c);
    SET_SLOT(host, 0x10c, ses_h10c);
    SET_SLOT(sound, 0x18, ses_s18);
    SET_SLOT(sound, 0x1c, ses_s1c);
    SET_SLOT(sound, 0x20, ses_s20);
    SET_SLOT(controls, 0x4c, ses_m16_4c);
    SET_SLOT(controls, 0x50, ses_m16_50);
    SET_SLOT(acontain, 0x28, ses_m23_28);
    SET_SLOT(acontain, 0x2c, ses_m23_2c);
    SET_SLOT(module_stub[10], 0x18, ses_w18);
    SET_SLOT(module_stub[7], 0x28, ses_m7_28);
    SET_SLOT(hg2d, 0x18, ses_h18);
    SET_SLOT(weatherm, 0x24, ses_wm24);
    session_line = rnd_n(3) == 0;
    session_type = rnd_n(4);
    *(uint32_t *)(global + 0x1210) = rnd_n(3);
    *(uint32_t *)(global + 0x1218) = rnd_n(2);
    *(uint32_t *)(global + 0x121c) = rnd_n(2);
    *(uint32_t *)(global + 0x11d4) = rnd_n(2);
    *(uint32_t *)(cur->image + 0x7f48) = *(uint32_t *)(cur->image + 0x7f48);
}

static void op_session(void)
{
    static char mode[16];

    if (op_number != 0)                            /* one run a scenario */
        return;
    snprintf(mode, sizeof mode, "%u", 1 + rnd_n(11));   /* a real preset (the prefs suite goes wider) */
    cfg_set("PREFS", "PERFORMANCE_MODE", mode);
    clock_words[0] = (uint16_t)(1990 + rnd_n(16));
    clock_words[1] = (uint16_t)(1 + rnd_n(12));
    clock_words[2] = (uint16_t)(1 + rnd_n(31));
    clock_words[3] = (uint16_t)rnd_n(24);
    clock_words[4] = (uint16_t)rnd_n(60);
    clock_words[5] = (uint16_t)rnd_n(60);
    *(uint32_t *)(cur->image + 0x7874) = rnd_n(2); /* a `d' record seen: +0x74's first argument counts */
    kb_h64_fails = rnd_n(8) == 0;
    put("\n#session start");
    ((s0)SVC(0x28))();
    put(" c9c=%08x\n#session stop", *(uint32_t *)(global + 0xc9c));
    *(uint32_t *)(global + 0x11d8) = 0;            /* ACONTAIN's block, or none */
    kb_h64_fails = 0;
    *(uint32_t *)(cur->image + 0x7fc8) = rnd_n(3);
    if (rnd_n(2)) {                                /* the kneeboard shown and moved, which the prefs remember */
        *(uint32_t *)(kb_rec() + 8) |= 0x80000000u;
        *(uint32_t *)(kb_rec() + 8) &= 0xfeffffffu;
    }
    ((s0)SVC(0x2c))();
}

static void report_session(void)
{
}

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
    { "frame", op_frame, report_frame, 30, setup_frame, 0x00 },
    { "tick", op_tick, report_tick, 20, setup_tick, 0x00 },
    { "step", op_step, report_step, 20, setup_step, 0x00 },
    { "commands", op_commands, report_commands, 30, setup_commands, 0xff },
    { "dialogs", op_dialogs, report_dialogs, 20, setup_dialogs, 0x00 },
    { "flights", op_flights, report_flights, 12, setup_flights, 0xff },
    { "natives", op_natives, report_natives, 8, setup_natives, 0x00 },
    { "start", op_start, report_start, 20, setup_start, 0x00 },
    { "radios", op_radios, report_radios, 12, setup_radios, 0x00 },
    { "weather", op_weather, report_weather, 8, setup_weather, 0x00 },
    { "kneeboard", op_kneeboard, report_kneeboard, 12, setup_kneeboard, 0x00 },
    { "aircraft", op_aircraft, report_aircraft, 6, setup_aircraft, 0x00 },
    { "clockdlg", op_clockdlg, report_clockdlg, 8, setup_dialogs, 0x00 },
    { "engines", op_engines, report_engines, 8, setup_dialogs, 0x00 },
    { "limits", op_limits, report_limits, 8, setup_dialogs, 0x00 },
    { "areas", op_areas, report_areas, 8, setup_dialogs, 0x00 },
    { "prefdlg", op_prefdlg, report_prefdlg, 6, setup_prefdlg, 0x00 },
    { "viewdlg", op_viewdlg, report_viewdlg, 6, setup_viewdlg, 0x00 },
    { "logbook", op_logbook, report_logbook, 6, setup_logbook, 0x00 },
    { "wxdlg", op_wxdlg, report_wxdlg, 6, setup_wxdlg, 0x00 },
    { "radiodlg", op_radiodlg, report_radiodlg, 6, setup_radiodlg, 0x00 },
    { "create", op_create, report_create, 6, setup_create, 0x00 },
    { "session", op_session, report_session, 1, setup_session, 0x00 },
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

/* Retail never deletes the kneeboard's picture: every .text+0x1320 loads another
 * DIB section, and 30,000 of them fill the process's address space, after which
 * LoadImageA fails on whichever side is next. Drop each once the module has let
 * go of it, or at the end of its scenario. */
static void drop_kneeboard_bitmap(int last)
{
    static uint32_t held;
    uint32_t now = IMG32(0xdc84);

    if (held != 0 && (last || held != now))
        DeleteObject((HGDIOBJ)(uintptr_t)held);
    held = last ? 0 : now;
}

/* A string run off the end of the image lands in the module's import table (retail's
 * follows .data) or in ours's data beyond it, past what is compared, and shows only as
 * faults thousands of scenarios later: keep a copy and put it back each time. */
static void tail_keep(struct side *s)
{
    uint8_t *at = s->image + IMAGE_SIZE;
    MEMORY_BASIC_INFORMATION m;

    s->tail_size = 0;
    while (at + s->tail_size < s->hi && VirtualQuery(at + s->tail_size, &m, sizeof m) != 0 &&
           (m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) != 0)
        s->tail_size = (uint32_t)((uint8_t *)m.BaseAddress + m.RegionSize - at);
    if (s->tail_size > (uint32_t)(s->hi - at))
        s->tail_size = (uint32_t)(s->hi - at);
    s->tail = malloc(s->tail_size + 1);
    memcpy(s->tail, at, s->tail_size);
}

static void run_side(struct side *s)
{
    uint32_t i, nops;

    cur = s;
    s->length = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    if (s->tail == NULL)
        tail_keep(s);
    memcpy(s->image + IMAGE_SIZE, s->tail, s->tail_size);
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
        load_mark = 0;
        hung = 0;
        call_start = GetTickCount();
        in_main = 1;
        if (__builtin_setjmp(fault_jb) == 0)
            suite->op();
        in_main = 0;
        if (handles_differ)
            drop_kneeboard_bitmap(0);
        if (hung) {
            put(" HANG");
            if (getenv("MAIN_HANG") && cur->length > 300)
                printf("%s hang: %.300s\n", cur->name, cur->transcript + cur->length - 300);
            if (counting) cover_hangs++;
            break;
        }
        if ((cursor_wild() || walked_off) && load_mark != 0 && table_overwritten()) {
            /* 0x1101 loads twice in an op: the first wrote over the table, and the second
             * walked its self-pointers, wild now, into memory the two sides do not share.
             * Whether it faults there or reads on depends on the map, so compare up to it. */
            cur->length = load_mark;
            put(" TABLE OVERWRITTEN");
            if (counting) cover_table_overwritten++;
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
    if (handles_differ)
        drop_kneeboard_bitmap(1);
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
    if (getenv("MAIN_IMAGE")) {                    /* the words behind a digest */
        uint32_t k, shown = 0;
        for (k = i + 1; k < n && shown < 12; k++)
            if (a[k] != b[k] && k > 8) {
                printf("  at %u: retail %.24s / ours %.24s\n", k, a + k - 8, b + k - 8);
                shown++;
                k += 16;
            }
    }
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
    situations_suite = !strcmp(suite->name, "situations") || !strcmp(suite->name, "quicksave") ||
                       !strcmp(suite->name, "commands") || !strcmp(suite->name, "flights");
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
