/* Differential test: retail DYNAMIC.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven only as the sim drives
 * them: Linkage +0x18 every frame (with a chosen ecx -- see below), +0x04
 * and +0x08, and the three CHAIN lists DYNAMIC registers on (3, 4 and 16),
 * dispatched by retail CHAIN.  What DYNAMIC imports:
 *
 *   host      a stub.  Memory is a bump allocator over an arena, so heap
 *             pointers come out the same on both sides; what the host does
 *             not zero is filled with 0xa5.
 *   GLOBAL    one block, set up identically before each side runs and poked
 *             between calls: the dynamic-scenery switches and density, the
 *             frame time and sim rate, the user's aircraft and its tanks
 *   CHAIN     retail's, behind logging wrappers
 *   FE        a stub serving the dynamic-scenery blocks of the CD's twelve
 *             airport BGLs (orig/Scenery/<city>5.bgl, read here as FE reads them:
 *             header +0x52, then the area and record lists), often mutated,
 *             as FE does: the probe with a 1-byte buffer, then the read; with
 *             GLOBAL +0x3e0 bit 0 clear the probe moves past the record, as
 *             FE's does, so the read gets the next one
 *   G3D       a stub logging each draw with a checksum of the object
 *   UTIL      retail's, behind forwarders that fold every argument and
 *             result into a digest logged after each step of the script
 *   MULTIP    a stub answering +0x24 with the scenario's players
 *
 * A scenario is a random script against one or more blocks, run once against
 * each module under a random x87 precision control (the path interpolation
 * multiplies on the x87).  Compared: the transcript, the GLOBAL region, the
 * arena, and DYNAMIC's whole .data image -- with code pointers matched
 * through a pairing of retail's functions with ours, learnt as first seen.
 *
 * The programs come from scenery files and nothing checks them, so a bad one
 * faults or spins.  Memory is laid out so that both happen identically on
 * both sides and inside the harness's reach: the arena and GLOBAL are
 * VirtualAlloc regions with uncommitted guard space around them, so a
 * runaway program or generator faults at the edge rather than writing over
 * the harness.  A vectored handler catches access violations and divide
 * faults inside DYNAMIC, CHAIN or UTIL, logs them and unwinds to the script.
 * A watchdog thread catches a program that spins without faulting (an
 * empty opcode below 0xa, a loop without a wait): after HANG_MS in one call
 * it moves the thread out of the module and the transcript says HANG.  Where
 * a spin was cut is timing, so that scenario ends there and only its
 * transcripts are compared (method.md, "a hang is an outcome").  Unallocated
 * heap reads 0xa5, past every opcode, so a stray program counter stops.
 *
 * +0x18 runs its stepping loop on the caller's ecx when there is no object
 * array (see dyn_frame in dynamic.c), so each frame is called with a chosen
 * ecx: usually large, sometimes 0, 1, 2 or 3.
 *
 * Steering (method.md, "steer onto boundaries"), each piece added because a
 * deliberate break went uncaught without it:
 *   - the user's aircraft and MULTIP's players on the points the programs
 *     test: 0x3d's meeting points (low, on the ground, a kind it takes; a
 *     player at the truck, too), 0x40's and 0x43's boxes -- exactly on the
 *     long edge, found with retail UTIL's own rotation -- 0x33's positions,
 *     a live object read back from the side running, and on or just past
 *     each of the block's bounds;
 *   - an object running generated code switched off (OP_OFF);
 *   - mutations that keep a block's structure (see mutate()), aimed: empty
 *     opcodes, 0x31 onto the next object's flags, declarations inside
 *     programs and other opcodes in the declaration list, a path's second
 *     point emptied (Seattle's programs are the ones that reach paths);
 *   - a block placed so one of its 0x20s sits across a 64K boundary, where
 *     its `add si,7` keeps only the low half;
 *   - conditional jumps aimed at the time of day and weather words with
 *     values on, beside or of the other sign from GLOBAL's, and +0xef2 on a
 *     step of selector 5; 0x3d's wanted kind past 3; 0x40's speed about its
 *     clamps and its third leg's 0x5f;
 *   - four synthetic blocks: "many" (0xe6 declarations, past the 0xe3
 *     limit; the host grows its top block in place so that fits), "ease"
 *     (eases landing exactly, frame time held), "fuel" (a truck at a known
 *     point, the aircraft 99, 100 or 101 from it, tanks a whole number of
 *     refuel steps from full), "moves" (a circuit from a box, started at a
 *     bank from turn_hit_*, and a 0x41 whose two candidate points are
 *     mirror images).
 *
 * UTIL is reached through thunks that keep ecx and edx (and eax where UTIL
 * does): retail DYNAMIC counts loops in ecx across UTIL calls, and the
 * digesting forwarders are C (method.md, "a stub has to keep what the real
 * callee keeps").
 *
 * Debugging: DYNSTOP stops at the first difference, DYNDUMP=n prints
 * scenario n's retail transcript, DYNONLY=n runs only scenario n,
 * DYNBLOCK=b forces block b, DYNHANG prints retail's registers at a hang,
 * DYNTRACE logs each object's counter and the generator state after every
 * frame, DYNUTIL logs every UTIL argument and result, and DYNFILES writes
 * both transcripts to rlog.txt/olog.txt at the first difference.
 *
 * Build: make dynamicdiff     Run: make dynamicdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x250
#define GLOBAL_SIZE  0x20000        /* GLOBAL's 0x1300, and room for objects run past it */
#define GLOBAL_USED  0x1300
#define ARENA_SIZE   (4u << 20)
#define ARENA_START  0x10000        /* the first allocation: room for negative offsets */
#define GUARD        0x100000
#define HANG_MS      1000
#define MAXBLOBS     16
#define MAXOPS       40
#define MAXPLAYERS   8

struct import { uint32_t id; void *link; };
typedef void (STDCALL *cb_fn)(uint32_t, uint32_t);
struct dyn_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t r0c, r10, r14;
    void (*frame)(void);
    uint32_t r1c;
    void *ops[0x44];
};
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

static uint32_t digest;             /* UTIL's arguments and results */

static int util_verbose;
static void put(const char *fmt, ...);
static void fold(uint32_t v)
{
    digest = (digest ^ v) * 0x01000193u;
    if (util_verbose) put(" %x", v);
}

/* ---- the sides ---------------------------------------------------------------- */

struct side {
    const char *name;
    HMODULE dll;
    struct dyn_linkage *link;
    uint8_t *image;             /* Linkage - 0x40 */
    uint8_t *lo, *hi;           /* the whole mapped module */
    uint8_t pristine[IMAGE_SIZE];
    uint32_t fn[128];           /* code addresses in order of first sight */
    unsigned nfn;
};
static struct side retail, ours, *cur;
static int counting;            /* coverage is counted on the retail run */

static uint8_t *arena;          /* ARENA_SIZE, guarded both sides */
static uint32_t arena_used;
static uint8_t *global;         /* GLOBAL_SIZE, guarded after */
static uint8_t *chain_lo, *chain_hi, *util_lo, *util_hi;

#define G8(o)  (*(uint8_t  *)(global + (o)))
#define G16(o) (*(uint16_t *)(global + (o)))
#define G32(o) (*(uint32_t *)(global + (o)))
#define A32(a) (*(uint32_t *)(uintptr_t)(a))

static int in_image(const uint8_t *p, const struct side *sd)
{
    return p >= sd->image && p < sd->image + IMAGE_SIZE;
}

static int is_code(const struct side *sd, uint32_t v)
{
    return (uint8_t *)v >= sd->lo && (uint8_t *)v < sd->hi && !in_image((uint8_t *)v, sd);
}

static unsigned fn_id(struct side *sd, uint32_t v)
{
    unsigned k;
    for (k = 0; k < sd->nfn; k++)
        if (sd->fn[k] == v) return k;
    if (sd->nfn < 128) sd->fn[sd->nfn] = v;
    return sd->nfn++;
}

static void put_ptr(uint32_t v)
{
    uint8_t *q = (uint8_t *)v;
    if (!v)
        put("null");
    else if (q >= arena - GUARD && q < arena + ARENA_SIZE + GUARD)
        put("A%+d", (int)(q - arena));
    else if (q >= global && q < global + GLOBAL_SIZE + GUARD)
        put("G+%#x", (unsigned)(q - global));
    else if (cur && in_image(q, cur))
        put("img+%#x", (unsigned)(q - cur->image));
    else if (cur && is_code(cur, v))
        put("fn%u", fn_id(cur, v));
    else if (v < 0x10000)
        put("%#x", v);
    else
        put("?");
}

/* ---- the stub host -------------------------------------------------------------- */

static uint8_t *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;
    if (size > ARENA_SIZE || arena_used + n > ARENA_SIZE - ARENA_START) {
        /* as if the host's malloc failed: its retry and message box end the
         * sim, so nothing past here is comparable.  Fault instead. */
        put(" ARENA");
        return NULL;
    }
    p = arena + arena_used;
    arena_used += n;
    *(uint32_t *)p = size;
    return p + 16;
}

/* The block placed so that one 0x20 in it sits across a 64K boundary --
 * its `add si,7` then keeps only the low 16 bits -- when the scenario asks:
 * a heap may put a block anywhere. */
static uint32_t wrap_size, wrap_off, wrap_low;
static unsigned long cover_wrapped;

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t zero)
{
    uint8_t *p;
    if (wrap_size && size == wrap_size && !zero) {
        uint32_t at = (uint32_t)(arena + arena_used + 16) + wrap_off;
        arena_used += (wrap_low - at) & 0xffff;
        if (counting) cover_wrapped++;
    }
    p = arena_get(size);
    put(" alloc(%d,%#x,%d)=", fl, size, zero);
    put_ptr((uint32_t)p);
    if (!p) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, NULL);
    memset(p, zero ? 0 : 0xa5, size);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    uint8_t *p, *old = *pp;
    uint32_t n = old ? *(uint32_t *)(old - 16) : 0;
    put(" realloc(%d,%#x,", fl, size);
    put_ptr((uint32_t)old);
    put(")=");
    if (old && old + ((n + 15) & ~15u) == arena + arena_used
        && (uint32_t)(old - arena) + ((size + 15) & ~15u) <= ARENA_SIZE - ARENA_START) {
        /* the last block grows where it is, as a heap's top block does */
        arena_used = (uint32_t)(old - arena) + ((size + 15) & ~15u);
        *(uint32_t *)(old - 16) = size;
        if (size > n) memset(old + n, 0xa5, size - n);
        put_ptr((uint32_t)old);
        return 0;
    }
    p = arena_get(size);
    put_ptr((uint32_t)p);
    if (!p) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, NULL);
    memset(p, 0xa5, size);
    if (old)
        memcpy(p, old, n < size ? n : size);
    *pp = p;
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    put(" free(");
    put_ptr((uint32_t)*pp);
    put(")");
    *pp = NULL;
    return 0;
}

static int32_t STDCALL h_memset(void *d, int32_t c, uint32_t n)
{
    memset(d, c, n);
    return 0;
}

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    memmove(d, s, n);
    return 0;
}

/* ---- CHAIN, retail's behind wrappers -------------------------------------------- */

static struct chain_linkage *chain;
static uint8_t chain_fwd[0x24];

static int32_t STDCALL c_add(uint32_t list, cb_fn fn)
{
    put(" chain.add(%u,", list);
    put_ptr((uint32_t)fn);
    put(")");
    return chain->add(list, fn);
}

static int32_t STDCALL c_remove(uint32_t list, cb_fn fn)
{
    int32_t r;
    put(" chain.remove(%u,", list);
    put_ptr((uint32_t)fn);
    r = chain->remove(list, fn);
    put(")=%d", r);
    return r;
}

/* ---- UTIL, retail's behind digesting forwarders ----------------------------------- */

static uint8_t *util_link;
static uint8_t util_fwd[0x100];
static unsigned long cover_util[8];

#define UTIL(off, type) (*(type *)(util_link + (off)))
typedef uint32_t (STDCALL *u1_fn)(uint32_t);
typedef void     (STDCALL *sincos_fn)(uint32_t, int16_t *, int16_t *);
typedef void     (STDCALL *rot_fn)(int32_t, int32_t, uint32_t, uint32_t, int32_t *, int32_t *);
typedef uint32_t (STDCALL *u2_fn)(uint32_t, uint32_t);
typedef void     (STDCALL *conv_fn)(const uint8_t *, const uint8_t *, uint8_t *);

static void fold48(const uint8_t *p)
{
    fold(*(const uint16_t *)(p + 2));
    fold(*(const uint32_t *)(p + 4));
}

uint32_t STDCALL u_sqrt64(uint32_t p)
{
    uint32_t r;
    fold(0x18);
    fold(A32(p));
    fold(A32(p + 4));
    r = UTIL(0x18, u1_fn)(p);
    fold(r);
    if (counting) cover_util[0]++;
    return r;
}

uint32_t STDCALL u_sqrt32(uint32_t v)
{
    uint32_t r = UTIL(0x1c, u1_fn)(v);
    fold(0x1c);
    fold(v);
    fold(r);
    if (counting) cover_util[1]++;
    return r;
}

void STDCALL u_sincos(uint32_t a, int16_t *s, int16_t *c)
{
    UTIL(0x2c, sincos_fn)(a, s, c);
    fold(0x2c);
    fold(a & 0xffff);
    fold((uint16_t)*s);
    fold((uint16_t)*c);
    if (counting) cover_util[2]++;
}

void STDCALL u_rotate(int32_t x, int32_t y, uint32_t s, uint32_t c, int32_t *ox, int32_t *oy)
{
    UTIL(0x34, rot_fn)(x, y, s, c, ox, oy);
    fold(0x34);
    fold((uint32_t)x);
    fold((uint32_t)y);
    fold(s & 0xffff);
    fold(c & 0xffff);
    fold((uint32_t)*ox);
    fold((uint32_t)*oy);
    if (counting) cover_util[3]++;
}

uint32_t STDCALL u_asin(uint32_t v, uint32_t q)
{
    uint32_t r = UTIL(0x38, u2_fn)(v, q);
    fold(0x38);
    fold(v & 0xffff);
    fold(q);
    fold(r & 0xffff);
    if (counting) cover_util[4]++;
    return r;
}

uint32_t STDCALL u_atan2(uint32_t x, uint32_t y)
{
    uint32_t r = UTIL(0x3c, u2_fn)(x, y);
    fold(0x3c);
    fold(x);
    fold(y);
    fold(r);
    if (counting) cover_util[5]++;
    return r;
}

void STDCALL u_to_lon(const uint8_t *in, const uint8_t *pos, uint8_t *out)
{
    UTIL(0x44, conv_fn)(in, pos, out);
    fold(0x44);
    fold48(in);
    fold48(pos);
    fold48(out);
    if (counting) cover_util[6]++;
}

void STDCALL u_to_m(const uint8_t *in, const uint8_t *pos, uint8_t *out)
{
    UTIL(0x48, conv_fn)(in, pos, out);
    fold(0x48);
    fold48(in);
    fold48(pos);
    fold48(out);
    if (counting) cover_util[7]++;
}

/* UTIL's routines save the registers they use -- every one keeps ecx and
 * edx, and sincos, the rotation and the conversions keep eax too -- and
 * retail DYNAMIC relies on it: its arc loops count in ecx across calls that
 * reach UTIL.  The forwarders above are C, which may change ecx and edx, so
 * DYNAMIC reaches them through thunks that keep what UTIL keeps.  Each copies
 * its n arguments below the saved registers and calls the stdcall forwarder,
 * which pops them. */
#define THUNK_CD(name, target, n) \
    __asm__(".text\n_" #name ":\n\t" \
            "pushl %ecx\n\tpushl %edx\n\t" \
            ".rept " #n "\n\tpushl 8+4*" #n "(%esp)\n\t.endr\n\t" \
            "call _" #target "\n\t" \
            "popl %edx\n\tpopl %ecx\n\tret $4*" #n)
#define THUNK_ACD(name, target, n) \
    __asm__(".text\n_" #name ":\n\t" \
            "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t" \
            ".rept " #n "\n\tpushl 12+4*" #n "(%esp)\n\t.endr\n\t" \
            "call _" #target "\n\t" \
            "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $4*" #n)
THUNK_CD(t_sqrt64, u_sqrt64@4, 1);
THUNK_CD(t_sqrt32, u_sqrt32@4, 1);
THUNK_ACD(t_sincos, u_sincos@12, 3);
THUNK_ACD(t_rotate, u_rotate@24, 6);
THUNK_CD(t_asin, u_asin@8, 2);
THUNK_CD(t_atan2, u_atan2@8, 2);
THUNK_ACD(t_to_lon, u_to_lon@12, 3);
THUNK_ACD(t_to_m, u_to_m@12, 3);
void t_sqrt64(void), t_sqrt32(void), t_sincos(void), t_rotate(void), t_asin(void), t_atan2(void),
     t_to_lon(void), t_to_m(void);

/* ---- the blocks ------------------------------------------------------------------ */

struct blob { uint32_t size; uint8_t *data; char name[16]; };
static struct blob cd[MAXBLOBS];
static unsigned ncd;

/* operand lengths, read off the handlers; 0 for the empty slots */
static const uint8_t oplen[0x44] = {
    /* 00 */ 0, 5, 5, 5, 2, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 10 */ 0, 0, 5, 5, 5, 7, 7, 7, 9, 7, 7, 7, 9, 9, 9, 0xb,
    /* 20 */ 7, 0, 0, 0, 3, 3, 1, 3, 0, 1, 3, 7, 7, 7, 7, 0,
    /* 30 */ 1, 4, 8, 0x13, 7, 0x18, 0, 0, 1, 0x1f, 0x1f, 0x20, 0, 0x11, 5, 5,
    /* 40 */ 0x1a, 0x17, 2, 0x19
};

static uint32_t rd32(const uint8_t *p) { return *(const uint32_t *)p; }
static uint16_t rd16(const uint8_t *p) { return *(const uint16_t *)p; }

/* FE's walk: header +0x52, 9-byte areas (0x14 continues elsewhere), each
 * pointing at 11-byte records; type 4 is dynamic scenery */
static void load_bgl(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint8_t *d;
    long n;
    uint32_t base, p, q, sz, guard = 0;
    if (!f) return;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc((size_t)n + 64);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return; }
    fclose(f);
    base = rd32(d + 0x52);
    if (!base || base >= (uint32_t)n) { free(d); return; }
    p = base;
    while (p + 9 <= (uint32_t)n && d[p] && guard++ < 1000) {
        if (d[p] == 0x14) { p = rd32(d + p + 1) + base; continue; }
        q = rd32(d + p + 5) + base;
        while (q + 11 <= (uint32_t)n && d[q] && ncd < MAXBLOBS) {
            sz = rd16(d + q + 9);
            if (d[q] == 4 && q + 11 + sz <= (uint32_t)n) {
                cd[ncd].size = sz;
                cd[ncd].data = malloc(sz);
                memcpy(cd[ncd].data, d + q + 11, sz);
                snprintf(cd[ncd].name, sizeof cd[ncd].name, "%.15s", strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path);
                ncd++;
            }
            q += 11 + sz;
        }
        p += 9;
    }
    free(d);
}

/* the points a block's programs test: (lat, lon, alt, kind) */
struct poi { uint32_t lat, lon, alt, r, along; uint16_t want, hdg; uint8_t kind; };
#define MAXPOI 256
static struct poi pois[MAXBLOBS][MAXPOI];
static unsigned npoi[MAXBLOBS];

static void find_pois(unsigned b)
{
    const uint8_t *d = cd[b].data;
    uint32_t n = cd[b].size, p = 0x20, q, work[4096], nwork = 0;
    static uint8_t seen[0x2800];
    struct poi *o;
    memset(seen, 0, sizeof seen);
    /* every program, following its branches */
    while (p < n && d[p] <= 0x43 && oplen[d[p]]) {
        if ((d[p] == 0x32 || d[p] == 0x35) && nwork < 4096)
            work[nwork++] = p + (d[p] == 0x32 ? rd16(d + p + 3) : rd32(d + p + 1));
        p += oplen[d[p]];
    }
    while (nwork) {
        q = work[--nwork];
        while (q < n && q < sizeof seen && !seen[q]) {
            uint8_t op = d[q];
            seen[q] = 1;
            if (op > 0x43 || !oplen[op] || q + oplen[op] > n) break;
            o = npoi[b] < MAXPOI ? &pois[b][npoi[b]] : NULL;
            if (o) {
                o->kind = op;
                o->want = 0;
                switch (op) {
                case 0x33: o->lat = rd32(d + q + 3); o->lon = rd32(d + q + 9); o->alt = rd32(d + q + 0xf); o->r = 0; npoi[b]++; break;
                case 0x3d: o->lat = rd32(d + q + 1); o->lon = rd32(d + q + 5); o->alt = 0; o->r = rd32(d + q + 9);
                           o->want = rd16(d + q + 0xf); npoi[b]++; break;
                case 0x40: o->lat = rd32(d + q + 1); o->lon = rd32(d + q + 5); o->alt = rd32(d + q + 9); o->r = rd32(d + q + 0x12);
                           o->along = rd32(d + q + 0x16); o->hdg = rd16(d + q + 0x10); npoi[b]++; break;
                case 0x43: o->lat = rd32(d + q + 3); o->lon = rd32(d + q + 7); o->alt = rd32(d + q + 0xb); o->r = rd32(d + q + 0x11);
                           o->along = rd32(d + q + 0x15); o->hdg = rd16(d + q + 0xf); npoi[b]++; break;
                }
            }
            if (nwork < 4096) {
                if (op == 0x24 || op == 0x25 || (op >= 0x2b && op <= 0x2e) || op == 0x43)
                    work[nwork++] = q + (uint32_t)(int32_t)(int16_t)rd16(d + q + 1);
                if (op == 0x3d)
                    work[nwork++] = q + rd16(d + q + 0xd);
            }
            if (op == 0x26 || op == 0x24) break;
            q += oplen[op];
        }
    }
}

/* A block no airport has: Chicago's bounds and origin, then 0xe6 declarations
 * -- more than the 0xe3 objects DYNAMIC will make -- of an object that stops
 * where it is put.  The last block, cd[ncd]. */
static void make_many(void)
{
    uint32_t n = 0x20 + 7 + 0xe6 * 8 + 1 + 1, k, at;
    uint8_t *d = calloc(1, n);
    memcpy(d, cd[0].data, 0x20);
    at = 0x20;
    while (cd[0].data[at] != 0x34) at += oplen[cd[0].data[at]];
    memcpy(d + 0x20, cd[0].data + at, 7);
    for (k = 0; k < 0xe6; k++) {
        uint8_t *q = d + 0x27 + 8 * k;
        q[0] = 0x32;
        *(uint16_t *)(q + 1) = (uint16_t)(0xfe0b + k % 6);
        *(uint16_t *)(q + 3) = (uint16_t)(n - 1 - (uint32_t)(q - d));
        q[7] = (uint8_t)(0x30 | k % 5);
    }
    d[n - 2] = 0xff;            /* the declarations end */
    d[n - 1] = 0x0a;            /* the program: stop */
    cd[ncd].size = n;
    cd[ncd].data = d;
    strcpy(cd[ncd].name, "many");
}

/* Another: one object whose program eases pitch, bank and heading through
 * distances its rate covers in a whole number of steps of 0x1000 -- so the
 * last step lands exactly on the target -- and starts again.  Run with the
 * frame time held at 0x1000.  cd[ncd + 1]. */
/* And a fuel truck: placed at a point, it meets any aircraft within 200 of
 * it and drives out at once, so the aircraft's distance at 0x3e is where
 * the harness put it.  cd[ncd + 2]; the point is FUEL_LAT/FUEL_LON. */
#define FUEL_LAT 4652043u
#define FUEL_LON 0xc1b33f34u        /* -1045225932 */
static void make_fuel(void)
{
    uint8_t prog[0x13 + 3 + 0x11 + 3 + 3 + 5 + 3 + 3], *q = prog;
    uint32_t n = 0x20 + 7 + 8 + 1 + sizeof prog, at, m;
    uint8_t *d;
    memset(prog, 0, sizeof prog);
    q[0] = 0x33; *(uint32_t *)(q + 3) = FUEL_LAT; *(uint32_t *)(q + 9) = FUEL_LON; q += 0x13;
    q[0] = 0x07; *(uint16_t *)(q + 1) = 0x4000; q += 3;
    m = (uint32_t)(q - prog);
    q[0] = 0x3d; *(uint32_t *)(q + 1) = FUEL_LAT; *(uint32_t *)(q + 5) = FUEL_LON;
    *(uint32_t *)(q + 9) = 200; *(uint16_t *)(q + 0xd) = 0x11 + 6; *(uint16_t *)(q + 0xf) = 3;
    q += 0x11;
    q[0] = 0x27; *(uint16_t *)(q + 1) = 2; q += 3;
    q[0] = 0x24; *(int16_t *)(q + 1) = (int16_t)(m - (uint32_t)(q - prog)); q += 3;
    q[0] = 0x3e; *(uint32_t *)(q + 1) = 0x3c; q += 5;
    q[0] = 0x27; *(uint16_t *)(q + 1) = 3; q += 3;
    q[0] = 0x24; *(int16_t *)(q + 1) = (int16_t)(0 - (uint32_t)(q - prog)); q += 3;
    d = calloc(1, n);
    memcpy(d, cd[0].data, 0x20);
    at = 0x20;
    while (cd[0].data[at] != 0x34) at += oplen[cd[0].data[at]];
    memcpy(d + 0x20, cd[0].data + at, 7);
    d[0x27] = 0x32;
    *(uint16_t *)(d + 0x28) = 0xfe0d;
    *(uint16_t *)(d + 0x2a) = 9;
    d[0x2e] = 0x30;
    d[0x2f] = 0xff;
    memcpy(d + 0x30, prog, sizeof prog);
    cd[ncd + 2].size = n;
    cd[ncd + 2].data = d;
    strcpy(cd[ncd + 2].name, "fuel");
}

/* The starting banks (as words) from which a circuit's first 90-degree turn
 * gains, at some step, a heading in [0x37210000, 0x37220000) -- the one
 * window where the turn's threshold decides -- worked out by stepping the
 * bank and heading as emit_step does for all 65536 (positive ones only). */
static const uint16_t turn_hit_left[] = {
    0xaf, 0x20b, 0x34c, 0x477, 0x58f, 0x695, 0x791, 0xb1c, 0xbea, 0xcb4, 0xd75, 0x104e, 0x11a2, 0x12e8, 0x1386, 0x1422, 0x15e3, 0x1b34, 0x1c2f, 0x1d9d, 0x1e8a, 0x2057, 0x20c8, 0x2213, 0x23be, 0x2426, 0x248e, 0x24f6, 0x26ec, 0x2871, 0x2a49, 0x2cc4, 0x2e23, 0x2fcf, 0x311d, 0x3306
};
static const uint16_t turn_hit_right[] = {
    0xd2, 0x285, 0x48c, 0x72c, 0x72d, 0xfe4, 0xfe5, 0xfe6, 0xfe7, 0xfe8, 0xfe9, 0xfea, 0xfeb, 0x1561, 0x1562, 0x17fa, 0x17fb, 0x19fb, 0x1bad, 0x1d2d, 0x1fc7, 0x20f1, 0x2209, 0x230f, 0x240a, 0x24fc, 0x28c7, 0x2a2b, 0x2be1, 0x2dff, 0x30a0, 0x33ed, 0x381d, 0x3d87, 0x44b2, 0x4e7c, 0x5c79, 0x5c7a, 0x71d0, 0x71d1
};

/* And two manoeuvres: object A, at a bank set per scenario (at moves_bank_at),
 * flies a circuit when anything is in the box 100..300 north of it (speed and turn direction set per scenario at
 * MOVES_SPEED/MOVES_DIR), object B comes round onto heading 0 at a point due
 * north of it -- where 0x41's two candidate points are mirror images and
 * equally far.  cd[ncd + 3]. */
#define MOVES_A_LAT (FUEL_LAT + 3000)
#define MOVES_B_LAT (FUEL_LAT - 3000)
static uint32_t moves_speed_at, moves_dir_at, moves_to_at, moves_bank_at;
static void make_moves(void)
{
    uint8_t a[0x13 + 3 + 0x1a + 3 + 3], b[0x13 + 0x17 + 3 + 3], *q;
    uint32_t n, at, la;
    uint8_t *d;
    memset(a, 0, sizeof a);
    memset(b, 0, sizeof b);
    q = a;
    q[0] = 0x33; *(uint32_t *)(q + 3) = MOVES_A_LAT; *(uint32_t *)(q + 9) = FUEL_LON; q += 0x13;
    q[0] = 0x06; q += 3;            /* the bank it starts the circuit with */
    la = (uint32_t)(q - a);
    q[0] = 0x40; *(uint32_t *)(q + 1) = MOVES_A_LAT + 200; *(uint32_t *)(q + 5) = FUEL_LON;
    q[0xd] = 1; *(uint16_t *)(q + 0xe) = 0x60; *(uint32_t *)(q + 0x12) = 100; *(uint32_t *)(q + 0x16) = 400;
    q += 0x1a;
    q[0] = 0x27; *(uint16_t *)(q + 1) = 1; q += 3;
    q[0] = 0x24; *(int16_t *)(q + 1) = (int16_t)(la - (uint32_t)(q - a));
    q = b;
    q[0] = 0x33; *(uint32_t *)(q + 3) = MOVES_B_LAT; *(uint32_t *)(q + 9) = FUEL_LON; q += 0x13;
    q[0] = 0x41; *(uint32_t *)(q + 3) = MOVES_B_LAT + 40; *(uint32_t *)(q + 9) = FUEL_LON; q += 0x17;
    q[0] = 0x27; *(uint16_t *)(q + 1) = 4; q += 3;
    q[0] = 0x24; *(int16_t *)(q + 1) = (int16_t)(0 - (uint32_t)(q - b));
    n = 0x20 + 7 + 16 + 1 + sizeof a + sizeof b;
    d = calloc(1, n);
    memcpy(d, cd[0].data, 0x20);
    at = 0x20;
    while (cd[0].data[at] != 0x34) at += oplen[cd[0].data[at]];
    memcpy(d + 0x20, cd[0].data + at, 7);
    d[0x27] = 0x32; *(uint16_t *)(d + 0x28) = 0xfe0d;
    *(uint16_t *)(d + 0x2a) = (uint16_t)(0x38 - 0x27); d[0x2e] = 0x30;
    d[0x2f] = 0x32; *(uint16_t *)(d + 0x30) = 0xfe0b;
    *(uint16_t *)(d + 0x32) = (uint16_t)(0x38 + sizeof a - 0x2f); d[0x36] = 0x30;
    d[0x37] = 0xff;
    memcpy(d + 0x38, a, sizeof a);
    memcpy(d + 0x38 + sizeof a, b, sizeof b);
    moves_speed_at = 0x38 + la + 0xe;
    moves_bank_at = 0x38 + 0x13 + 1;
    moves_dir_at = 0x38 + la + 0xd;
    moves_to_at = 0x38 + sizeof a + 0x13 + 3;
    cd[ncd + 3].size = n;
    cd[ncd + 3].data = d;
    strcpy(cd[ncd + 3].name, "moves");
}

static void make_ease(void)
{
    static const uint8_t prog[] = {
        0x05, 0x00, 0x00,  0x06, 0x00, 0x00,  0x07, 0x00, 0x00,
        0x01, 0x01, 0x00, 0x01, 0x00,           /* pitch to 1: 8 steps */
        0x02, 0xff, 0xff, 0x01, 0x00,           /* bank to -1: 8 */
        0x03, 0x02, 0x00, 0x01, 0x00,           /* heading to 2: 16 */
        0x27, 0x01, 0x00,
        0x24, 0xe5, 0xff                        /* back 27 to the start */
    };
    uint32_t n = 0x20 + 7 + 8 + 1 + sizeof prog, at;
    uint8_t *d = calloc(1, n);
    memcpy(d, cd[0].data, 0x20);
    at = 0x20;
    while (cd[0].data[at] != 0x34) at += oplen[cd[0].data[at]];
    memcpy(d + 0x20, cd[0].data + at, 7);
    d[0x27] = 0x32;
    *(uint16_t *)(d + 0x28) = 0xfe0d;
    *(uint16_t *)(d + 0x2a) = 8 + 1;
    d[0x2e] = 0x30;
    d[0x2f] = 0xff;
    memcpy(d + 0x30, prog, sizeof prog);
    cd[ncd + 1].size = n;
    cd[ncd + 1].data = d;
    strcpy(cd[ncd + 1].name, "ease");
}

/* ---- FE, G3D, MULTIP ---------------------------------------------------------- */

struct fe_rec { uint32_t size; uint8_t *data; };
static struct fe_rec fe_recs[3];
static unsigned fe_n, fe_cursor;
static unsigned long cover_fe[4], cover_draw, cover_blocks;

static void STDCALL fe_search(uint8_t *pos, int32_t radius, uint8_t *box)
{
    put(" fe.search(");
    put_ptr((uint32_t)pos);
    put(",%d,", radius);
    put_ptr((uint32_t)box);
    put(")");
    *(int32_t *)(box + 0) = *(int32_t *)(pos + 4) - radius;
    *(int32_t *)(box + 4) = *(int32_t *)(pos + 4) + radius;
    *(int32_t *)(box + 8) = *(int32_t *)(pos + 0xc) - radius * 64;
    *(int32_t *)(box + 0xc) = *(int32_t *)(pos + 0xc) + radius * 64;
    fe_cursor = 0;
}

static int32_t STDCALL fe_read(int32_t kind, uint8_t *buf, int32_t size, int32_t *out)
{
    int32_t r;
    put(" fe.read(%d,", kind);
    put_ptr((uint32_t)buf);
    put(",%#x)", size);
    if (fe_cursor >= fe_n) {
        r = 0;
        if (counting) cover_fe[0]++;
    } else if ((uint32_t)size < fe_recs[fe_cursor].size) {
        *out = (int32_t)fe_recs[fe_cursor].size;
        if (!(G16(0x3e0) & 1)) fe_cursor++;
        r = -1;
        if (counting) cover_fe[1]++;
    } else {
        memcpy(buf, fe_recs[fe_cursor].data, fe_recs[fe_cursor].size);
        fe_cursor++;
        r = 1;
        if (counting) cover_fe[2]++;
    }
    put("=%d", r);
    return r;
}

static uint32_t crc(const uint8_t *p, unsigned n)
{
    uint32_t h = 0x811c9dc5u;
    while (n--) h = (h ^ *p++) * 0x01000193u;
    return h;
}

static void STDCALL g3d_draw(uint8_t *rec, uint32_t state)
{
    put(" draw(");
    put_ptr((uint32_t)rec);
    put(",%u)%08x", state, crc(rec - 0xf0, 0x120));
    if (counting) cover_draw++;
}

static uint8_t mp_players[MAXPLAYERS][0x100];
static uint8_t mp_pos[MAXPLAYERS][0x28];
static uint32_t mp_head[2];
static uint32_t speed_rec[4];

static uint32_t mp_get(void)
{
    put(" mp");
    return (uint32_t)mp_head;
}

/* ---- scenarios ----------------------------------------------------------------- */

static uint32_t rs = 0x6d2b79f5u;
static uint32_t rng(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 17;
    rs ^= rs << 5;
    return rs;
}

enum { OP_FRAME, OP_TICK, OP_L3, OP_L4, OP_L16, OP_POKE, OP_MOVE, OP_PLAYERS, OP_DEINIT,
       OP_INIT, OP_MEET, OP_OFF, OP_COUNT };
static const char *const op_names[OP_COUNT] = {
    "frame", "tick", "list3", "list4", "list16", "poke", "move", "players", "deinit", "init",
    "meet", "off"
};

struct op { int kind; uint32_t a, b, c, d; };
struct player { uint32_t id, active, flag, speed, height, kind, ground; uint32_t lat, lon, alt; uint16_t latf, lonf; };
struct scenario {
    unsigned nops, nrec, base, precision;
    uint32_t rec_size[3];
    uint8_t rec[3][0x2800];
    uint8_t g[GLOBAL_USED];
    uint32_t speed;
    uint32_t wrap_at;           /* an 0x20 the block is placed to put across 64K, + 1 */
    unsigned nplayers;
    struct player pl[MAXPLAYERS];
    struct op op[MAXOPS];
};

/* the opcodes a block's programs reach, following their branches and calls */
static uint32_t reachable(const uint8_t *d, uint32_t n, uint32_t decl_end, uint32_t *ops, uint32_t max)
{
    static uint8_t seen[0x2800];
    uint32_t work[2048], nwork = 0, nops = 0, p, q, guard;
    memset(seen, 0, sizeof seen);
    for (p = 0x20; p < decl_end; p += oplen[d[p]])
        if ((d[p] == 0x32 || d[p] == 0x35) && nwork < 2048)
            work[nwork++] = p + (d[p] == 0x32 ? rd16(d + p + 3) : rd32(d + p + 1));
    while (nwork) {
        q = work[--nwork];
        for (guard = 0; q < n && q < sizeof seen && !seen[q] && guard < 4000 && nops < max; guard++) {
            uint8_t op = d[q];
            seen[q] = 1;
            if (op > 0x43 || !oplen[op] || q + oplen[op] > n) break;
            ops[nops++] = q;
            if (nwork < 2048) {
                if (op == 0x24 || op == 0x25 || (op >= 0x2b && op <= 0x2e) || op == 0x43)
                    work[nwork++] = q + (uint32_t)(int32_t)(int16_t)rd16(d + q + 1);
                if (op == 0x3d)
                    work[nwork++] = q + rd16(d + q + 0xd);
            }
            if (op == 0x26 || op == 0x24) break;
            q += oplen[op];
        }
    }
    return nops;
}

static uint32_t decl_end_of(const uint8_t *d, uint32_t n)
{
    uint32_t p = 0x20;
    while (p < n && d[p] <= 0x43 && oplen[d[p]]) p += oplen[d[p]];
    return p;
}

static unsigned long cover_emptied;
static int force_empty;     /* mutate: only empty a path's second point */

/* a mutation that keeps the block's structure: operands, opcodes of the same
 * length, the declarations' switch bytes and library ids */
static void mutate(uint8_t *d, uint32_t n)
{
    uint32_t p = 0x20, decl_end, q, k, picks = 1 + rng() % 6, len;
    uint32_t ops[2048], nops = 0;
    while (p < n && d[p] <= 0x43 && oplen[d[p]]) {
        if ((d[p] == 0x32 || d[p] == 0x35) && rng() % 8 == 0) {
            if (rng() % 2) d[p + 7] = (uint8_t)rng();
            else if (d[p] == 0x32) *(uint16_t *)(d + p + 1) = (uint16_t)(0xfe0a + rng() % 8);
        } else if (d[p] == 0x32 && rng() % 40 == 0) {
            /* not a declaration: two parameter words, or four flag ops, on
             * whatever edi the declaration before left */
            if (rng() % 3 == 0) {
                d[p] = 0x31; d[p + 1] = (uint8_t)(rng() % 0x98);
                d[p + 4] = 0x31; d[p + 5] = (uint8_t)rng();
            } else if (rng() % 2) {     /* a wait, with S_RUN as the last step left it */
                d[p] = 0x27; d[p + 1] = (uint8_t)(1 + rng() % 4); d[p + 2] = 0;
                d[p + 3] = 0x42; d[p + 5] = 0x42; d[p + 7] = 0x30;
            } else {
                d[p] = 0x42; d[p + 2] = 0x04; d[p + 4] = 0x42; d[p + 6] = 0x04;
            }
            p += 8;
            continue;
        }
        p += oplen[d[p]];
    }
    decl_end = p;
    nops = reachable(d, n, decl_end, ops, 2048);
    if (!nops) return;
    for (k = 0; k < picks; k++) {
        q = ops[rng() % nops];
        len = oplen[d[q]];
        switch (force_empty ? 8 : rng() % 9) {
        case 8:     /* the second of a run of path points emptied: the first
                     * then ends the path on an empty slot */
            {
                uint32_t j, a, b2;
                for (j = 0; j < nops; j++) {
                    a = ops[(k + j) % nops];
                    if (d[a] < 0x39 || d[a] > 0x3b || a + 0x40 >= n) continue;
                    b2 = a + oplen[d[a]];
                    if (d[b2] >= 0x39 && d[b2] <= 0x3b) {
                        d[b2] = 0x3c;
                        cover_emptied++;
                        break;
                    }
                }
            }
            break;
        case 7:     /* a declaration inside a program: it makes an object of the
                     * bytes there, and edi is the last object after it */
            if (len >= 8 && rng() % 4 == 0) d[q] = 0x32;
            break;
        case 5:     /* an empty slot in its place: 0x0a-0x11 and the rest stop the
                     * step there; 0, 8 and 9, below 0xa, spin, as in retail */
            if (d[q] == 0x27 || d[q] == 0x29) break;
            d[q] = rng() % 12 ? (uint8_t[]){ 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
                                            0x21, 0x22, 0x23, 0x28, 0x2f, 0x36, 0x37, 0x3c }[rng() % 16]
                              : (uint8_t[]){ 0, 8, 9 }[rng() % 3];
            break;
        case 6:     /* a parameter word written past the object's eight: the
                     * index is not checked, and 0x8a is the next object's flags */
            if (d[q] == 0x31) {
                d[q + 1] = (uint8_t)(rng() % 2 ? 0x8a : rng());
                *(uint16_t *)(d + q + 2) = (uint16_t)rng();
            }
            break;
        case 0: {   /* another opcode of the same length -- not in place of a wait,
                     * hold or path point: a loop without one spins */
            if (d[q] == 0x27 || d[q] == 0x29 || (d[q] >= 0x39 && d[q] <= 0x3b)) break;
            uint8_t cand[0x44];
            unsigned m = 0, j;
            for (j = 1; j < 0x44; j++)
                if (oplen[j] == len && j != 0x32 && j != 0x35 && j != 0x26 && j != 0x24 && j != 0x25)
                    cand[m++] = (uint8_t)j;
            if (m) d[q] = cand[rng() % m];
            break;
        }
        case 1: case 2:     /* an operand byte */
            if (len > 1) d[q + 1 + rng() % (len - 1)] = (uint8_t)rng();
            break;
        case 3:             /* an operand byte nudged */
            if (len > 1) d[q + 1 + rng() % (len - 1)] += (uint8_t)(rng() % 5) - 2;
            break;
        default:            /* a wait or duration shortened */
            if (len >= 3 && d[q] != 0x24 && d[q] != 0x25 && d[q] < 0x2b)
                *(uint16_t *)(d + q + 1) = (uint16_t)(1 + rng() % 8);
            break;
        }
    }
}

static void put_user(uint8_t *g, uint32_t lat, uint32_t lon, uint32_t alt)
{
    *(uint32_t *)(g + 0x564) = lat;
    *(uint32_t *)(g + 0x56c) = lon;
    *(uint32_t *)(g + 0x574) = alt;
}

/* a place in or about block b */
static void place(unsigned b, uint32_t sel, uint32_t *lat, uint32_t *lon, uint32_t *alt)
{
    const uint8_t *d = cd[b].data;
    int32_t latmax = (int16_t)rd16(d), latmin = (int16_t)rd16(d + 2);
    int32_t lonmax = (int16_t)rd16(d + 4), lonmin = (int16_t)rd16(d + 6);
    struct poi *o;
    if (sel % 5 && npoi[b]) {
        o = &pois[b][rng() % npoi[b]];
        *lat = o->lat;
        *lon = o->lon;
        *alt = o->alt;
        switch (rng() % 6) {
        case 0: break;                                      /* exactly on it */
        case 1: *alt += rng() % 12; break;                  /* within 0x40/0x43's 10 */
        case 2: if (o->r) *lat += o->r * (rng() % 3) - o->r; break;   /* on the edge */
        default: *lat += rng() % 200 - 100; *lon += (rng() % 200 - 100) * 64; break;
        }
        return;
    }
    if (sel % 7 == 0) {     /* on a bound, or just past one: kept, or dropped */
        int32_t la = latmin + (int32_t)(rng() % (uint32_t)(latmax - latmin + 1));
        int32_t lo = lonmin + (int32_t)(rng() % (uint32_t)(lonmax - lonmin + 1));
        uint32_t fl = rng() & 0x1ff, fo = rng() & 0xffff;
        switch (rng() % 8) {
        case 0: la = latmax; break;
        case 1: la = latmax + 1; fl = 0; break;
        case 2: la = latmin; fl = 0; break;
        case 3: la = latmin - 1; fl = 0x1ff; break;
        case 4: lo = lonmax; break;
        case 5: lo = lonmax + 1; fo = 0; break;
        case 6: lo = lonmin; fo = 0; break;
        default: lo = lonmin - 1; fo = 0xffff; break;
        }
        *lat = (uint32_t)la << 9 | fl;
        *lon = (uint32_t)lo << 16 | fo;
        *alt = rng() % 300;
        return;
    }
    *lat = (uint32_t)(latmin + (int32_t)(rng() % (uint32_t)(latmax - latmin + 1))) << 9 | (rng() & 0x1ff);
    *lon = (uint32_t)(lonmin + (int32_t)(rng() % (uint32_t)(lonmax - lonmin + 1))) << 16 | (rng() & 0xffff);
    *alt = rng() % 4 ? rng() % 300 : rng();
}

static void make_player(unsigned base, struct player *p, unsigned k)
{
    uint32_t lat, lon, alt;
    p->id = rng() % 6 ? 100 + k : rng() % 4;     /* id 0 matches the user's record */
    p->active = rng() % 8 ? 1 : rng() % 3;
    p->flag = rng() % 10 ? 0 : 1;
    p->speed = rng() % 4 ? 5 + rng() % 40 : rng() % 200;
    p->height = rng() % 5 ? rng() % 0x7fff : rng();
    p->kind = rng() % 5 ? (uint32_t[]){ 0, 1, 3 }[rng() % 3] : rng() % 6;
    p->ground = rng() % 6 ? 0 : 1;
    place(base, rng(), &lat, &lon, &alt);
    p->lat = lat;
    p->lon = lon;
    p->alt = alt;
    p->latf = (uint16_t)rng();
    p->lonf = (uint16_t)rng();
}

static void make_global(struct scenario *s)
{
    uint8_t *g = s->g;
    uint32_t lat, lon, alt, k;
    memset(g, 0, GLOBAL_USED);
    *(uint32_t *)(g + 0x268) = rng() % 15 == 0;
    *(uint16_t *)(g + 0x2aa) = (uint16_t)(rng() % 6 ? 0x400 + rng() % 0x3000 : rng() % 3 ? rng() % 0x400 : rng());
    *(uint16_t *)(g + 0xc1a) = (uint16_t)(rng() % 3 ? 0x100u << rng() % 6 : rng() % 4 ? rng() % 0x4000 : rng());
    for (k = 0; k < 4; k++)
        *(uint32_t *)(g + 0x1180 + 4 * k) = rng() % 10 < 8 ? 1 : rng() % 6 ? 0 : rng();
    if (rng() % 20 == 0)
        for (k = 0; k < 4; k++) *(uint32_t *)(g + 0x1180 + 4 * k) = 0;
    *(uint32_t *)(g + 0x1190) = rng() % 6 ? 2 + rng() % 4 : rng() % 3 ? rng() % 8 : rng();
    *(uint16_t *)(g + 0x1172) = (uint16_t)(rng() % 3 ? rng() % 8 : rng());
    *(uint16_t *)(g + 0x1176) = (uint16_t)rng();
    *(uint16_t *)(g + 0x11b6) = (uint16_t)rng();
    *(uint16_t *)(g + 0x3e0) = (uint16_t)(rng() % 5 ? 1 : 0);
    place(s->base, rng(), &lat, &lon, &alt);
    put_user(g, lat, lon, alt);
    *(uint16_t *)(g + 0x562) = (uint16_t)rng();
    *(uint16_t *)(g + 0x56a) = (uint16_t)rng();
    *(uint16_t *)(g + 0x572) = (uint16_t)rng();
    for (k = 0x578; k < 0x584; k += 4)
        *(uint32_t *)(g + k) = rng();
    *(uint32_t *)(g + 0x2b4) = rng() % 4 ? rng() % 0x7fff : rng();
    *(uint16_t *)(g + 0x5dc) = (uint16_t)(rng() % 4 ? 0 : rng() % 3);
    g[0x609] = (uint8_t)(rng() % 5 ? (uint32_t[]){ 0, 1, 3 }[rng() % 3] : rng());
    s->speed = rng() % 4 ? 3 + rng() % 30 : rng() % 0x10000;
    *(uint32_t *)(g + 0x11e0) = rng() % 4 ? 1 : 0;   /* fixed up to speed_rec when run */
    for (k = 0; k < 5; k++) {
        static const uint16_t cap[5] = { 0xb78, 0xb80, 0xb98, 0xb88, 0xba0 };
        *(uint32_t *)(g + cap[k]) = rng() % 3 ? rng() % 0x200 : rng() % 4 ? 0 : rng();
        *(uint32_t *)(g + cap[k] - 4) = rng() % 4 ? rng() % 0x800000 : rng();
    }
    *(uint16_t *)(g + 0xc44) = (uint16_t)(rng() % 2 ? 0 : rng() % 3 ? 1 + rng() % 100 : rng());
    *(uint32_t *)(g + 0x348) = rng() % 3 ? rng() % 8 : 0;
    *(uint16_t *)(g + 0xef0) = (uint16_t)(rng() % 3 ? rng() % 360 : rng());
    *(uint16_t *)(g + 0xef2) = (uint16_t)rng();
    g[0x238] = (uint8_t)(rng() % 24);
    g[0x239] = (uint8_t)(rng() % 60);
}

/* the value a conditional jump's selector reads, as DYNAMIC computes it */
static uint16_t selected(const uint8_t *g, unsigned sel)
{
    switch (sel) {
    case 0: case 1: case 2: case 3: case 4: return (uint16_t)*(const uint32_t *)(g + 0x1180 + 4 * sel);
    case 5: return (uint16_t)((((uint32_t)*(const uint16_t *)(g + 0xef2) + 0x5b) & 0xffff) * 0x168 >> 16);
    case 6: return *(const uint16_t *)(g + 0xef0);
    case 7: return (uint16_t)(g[0x238] * 0x3c + g[0x239]);
    default: return 0;
    }
}

/* the block's conditional jumps pointed at a setting, compared with what
 * the scenario's GLOBAL holds, or one either side, or of the other sign;
 * 0x3d's wanted kind sometimes past the three the CD uses; and 0x40's
 * speed about its clamps and the speed whose eased value is the third
 * leg's 0x5f */
static void steer_conditions(struct scenario *s)
{
    uint8_t *d = s->rec[0];
    uint32_t n = s->rec_size[0], p = 0x20, q, guard;
    while (p < n && d[p] <= 0x43 && oplen[d[p]]) {
        if (d[p] == 0x32 || d[p] == 0x35) {
            q = p + (d[p] == 0x32 ? rd16(d + p + 3) : rd32(d + p + 1));
            for (guard = 0; q + 0x20 < n && guard < 400; guard++) {
                uint8_t op = d[q];
                if (op > 0x43 || !oplen[op]) break;
                if (op >= 0x2b && op <= 0x2e && rng() % 2) {
                    unsigned sel = rng() % 3 ? 5 + rng() % 3 : rng() % 8;
                    if (sel == 5 && rng() % 2) {
                        /* +0xef2 on a step of (x + 0x5b) * 360 >> 16 */
                        uint32_t k = 1 + rng() % 359;
                        *(uint16_t *)(s->g + 0xef2) = (uint16_t)((k * 0x10000 + 359) / 360 - 0x5b);
                    }
                    *(uint16_t *)(d + q + 3) = (uint16_t)sel;
                    *(uint16_t *)(d + q + 5) = (uint16_t)(selected(s->g, sel) + rng() % 3 - 1
                                                          + (rng() % 4 == 0 ? 0x8000 : 0));
                }
                if (op == 0x3d && rng() % 3 == 0)
                    *(uint16_t *)(d + q + 0xf) = (uint16_t)(rng() % 6);    /* kinds past 3 */
                if (op == 0x40 && rng() % 2)    /* about the speed clamps and the third leg's 0x5f */
                    *(uint16_t *)(d + q + 0xe) = (uint16_t[]){ 0x1d, 0x1e, 0x1f, 0x10d, 0x10e, 0x10f,
                                                               0x4b, 0x4c, 0x4d }[rng() % 9];
                if (op == 0x26 || op == 0x24) break;
                q += oplen[op];
            }
        }
        p += oplen[d[p]];
    }
}

static void make_scenario(struct scenario *s)
{
    unsigned k, n;
    memset(s, 0, sizeof *s);
    s->precision = rng() % 3;
    s->base = rng() % 10 < 3 ? 0 : rng() % ncd;    /* Chicago's has the fuel truck */
    if (rng() % 40 == 0) s->base = ncd;             /* past the object limit */
    if (rng() % 40 == 0) s->base = ncd + 1;         /* eases landing exactly */
    if (rng() % 25 == 0) s->base = ncd + 2;         /* the truck at a measured distance */
    if (rng() % 15 == 0) s->base = ncd + 3;         /* a circuit and a turn, set up */
    if (rng() % 30 == 0) s->base = 9;               /* Seattle: paths */
    if (getenv("DYNBLOCK")) s->base = (unsigned)atoi(getenv("DYNBLOCK"));  /* for debugging one */
    s->nrec = rng() % 12 ? 1 + (rng() % 4 == 0) + (rng() % 8 == 0) : 0;
    for (k = 0; k < s->nrec; k++) {
        unsigned b = k == 0 ? s->base : rng() % ncd;
        s->rec_size[k] = cd[b].size;
        memcpy(s->rec[k], cd[b].data, cd[b].size);
        if (rng() % 3 == 0 && b < ncd) mutate(s->rec[k], cd[b].size);
        if (k == 0 && b == 9 && rng() % 3 == 0) {
            /* only Seattle's programs reach runs of path points: end one
             * on an empty slot after its first point */
            force_empty = 1;
            mutate(s->rec[k], cd[b].size);
            force_empty = 0;
        }
        if (rng() % 40 == 0) s->rec_size[k] = 0x20 + rng() % (cd[b].size - 0x20);   /* cut short */
    }
    make_global(s);
    if (s->nrec && s->base < ncd && rng() % 4 == 0)
        steer_conditions(s);
    if (s->nrec && s->base < ncd && rng() % 8 == 0) {
        /* one of the block's turns across a 64K boundary */
        uint32_t ops[2048], m, j, pick[256], np = 0;
        m = reachable(s->rec[0], s->rec_size[0], decl_end_of(s->rec[0], s->rec_size[0]), ops, 2048);
        for (j = 0; j < m && np < 256; j++)
            if (s->rec[0][ops[j]] == 0x20) pick[np++] = ops[j];
        if (np) s->wrap_at = pick[rng() % np] + 1;
    }
    if (s->base == ncd + 3) {
        *(uint16_t *)(s->rec[0] + moves_speed_at) = rng() % 2
            ? (uint16_t[]){ 0x1d, 0x1e, 0x1f, 0x10d, 0x10e, 0x10f, 0x4b, 0x4c, 0x4d }[rng() % 9]
            : (uint16_t)(rng() % 0x140);
        s->rec[0][moves_dir_at] = (uint8_t)(rng() % 2);
        /* level mostly; banked steeply enough that the climb does not level it
         * before the first turn, whose heading steps then start elsewhere */
        *(uint16_t *)(s->rec[0] + moves_bank_at) = (uint16_t)(rng() % 3 ? 0 : rng());
        if (rng() % 2) {
            /* one of those, plus the easing steps the climb may take first */
            uint16_t h = s->rec[0][moves_dir_at]
                ? turn_hit_left[rng() % (sizeof turn_hit_left / 2)]
                : turn_hit_right[rng() % (sizeof turn_hit_right / 2)];
            *(uint16_t *)(s->rec[0] + moves_bank_at) = (uint16_t)(h + 0x16c * (rng() % 60));
        }
        *(uint32_t *)(s->rec[0] + moves_to_at) = MOVES_B_LAT + 20 + rng() % 60;
        *(uint32_t *)(s->g + 0x268) = 0;
        for (k = 0; k < 4; k++) *(uint32_t *)(s->g + 0x1180 + 4 * k) = 1;
        *(uint32_t *)(s->g + 0x1190) = 5;
    }
    if (s->base == ncd + 2) {
        /* one slice of 0x5000 a frame, so the fixed-rate refuel adds 0x7ffff
         * a step, and tanks a whole number of steps short of full */
        *(uint16_t *)(s->g + 0x2aa) = 0x5000;
        *(uint16_t *)(s->g + 0xc1a) = 0x100;
        for (k = 0; k < 5; k++)
            if (rng() % 2)
                *(uint32_t *)(s->g + (uint16_t[]){ 0xb74, 0xb7c, 0xb94, 0xb84, 0xb9c }[k]) =
                    0x7fffff - 0x7ffffu * (rng() % 4) + (rng() % 3 == 0 ? rng() % 3 - 1 : 0);
        if (rng() % 3) *(uint16_t *)(s->g + 0xc44) = 0;   /* the fixed rate */
        *(uint32_t *)(s->g + 0x268) = 0;
        for (k = 0; k < 4; k++) *(uint32_t *)(s->g + 0x1180 + 4 * k) = 1;
        *(uint32_t *)(s->g + 0x1190) = 5;
    }
    if (s->base == ncd + 1) {
        *(uint16_t *)(s->g + 0x2aa) = 0x1000;
        *(uint16_t *)(s->g + 0xc1a) = 0x100;
        *(uint32_t *)(s->g + 0x268) = 0;
        for (k = 0; k < 4; k++) *(uint32_t *)(s->g + 0x1180 + 4 * k) = 1;
        *(uint32_t *)(s->g + 0x1190) = 5;
    }
    s->nplayers = rng() % 3 ? rng() % 5 : rng() % (MAXPLAYERS + 1);
    for (k = 0; k < s->nplayers; k++)
        make_player(s->base, &s->pl[k], k);
    n = 4 + rng() % (MAXOPS - 4);
    /* as the sim: a frame registers DYNAMIC on CHAIN, list 4 loads the block */
    s->op[s->nops++].kind = OP_FRAME;
    s->op[s->nops].kind = OP_L4;
    s->nops++;
    while (s->nops < n) {
        struct op *o = &s->op[s->nops++];
        unsigned r = rng() % 100;
        o->a = rng();
        o->b = rng();
        o->c = rng();
        o->d = rng();
        if ((s->base == ncd + 1 || s->base == ncd + 2) && r >= 64 && r < 80) r = 40;   /* keep the frame time */
        if ((s->base == ncd + 2 || s->base == ncd + 3) && r >= 80 && r < 90) r = 94;   /* meet */
        if (r < 12) o->kind = OP_FRAME;
        else if (r < 50) o->kind = OP_TICK;
        else if (r < 56) o->kind = OP_L3;
        else if (r < 64) o->kind = OP_L4;
        else if (r < 70) o->kind = OP_L16;
        else if (r < 80) o->kind = OP_POKE;
        else if (r < 90) o->kind = OP_MOVE;
        else if (r < 93) o->kind = OP_PLAYERS;
        else if (r < 96) o->kind = OP_MEET;
        else if (r < 97) o->kind = OP_OFF;
        else if (r < 98) o->kind = OP_DEINIT;
        else o->kind = OP_INIT;
    }
}

/* ---- running ------------------------------------------------------------------- */

static unsigned long cover_ops[OP_COUNT], cover_fault, cover_hang, cover_frames,
                     cover_objects, cover_maxobj, cover_alloc[4], cover_pc[0x46],
                     cover_lock, cover_fuel, cover_g348;
static void *fault_jb[5];       /* __builtin_setjmp's */
static volatile int in_module;
static volatile DWORD call_start;
static HANDLE main_thread;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

static int in_reach(uint8_t *ip)
{
    return (cur && ip >= cur->lo && ip < cur->hi) || (ip >= chain_lo && ip < chain_hi)
        || (ip >= util_lo && ip < util_hi);
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;
    uint8_t *ip = (uint8_t *)e->ContextRecord->Eip;
    if (!in_module)
        return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_ACCESS_VIOLATION && (in_reach(ip) || e->ExceptionRecord->NumberParameters == 0)) {
        put(" FAULT(av,");
        if (e->ExceptionRecord->NumberParameters >= 2) {
            put("%lu,", e->ExceptionRecord->ExceptionInformation[0]);
            put_ptr(e->ExceptionRecord->ExceptionInformation[1]);
        }
        put(")");
    } else if ((code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW) && in_reach(ip)) {
        put(" FAULT(div)");
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    e->ContextRecord->Eip = (DWORD)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* the watchdog: a call spinning past HANG_MS inside the module is moved out */
static volatile int hung;
static int run_hung, rhung;
static unsigned long cover_hung_runs;
static DWORD WINAPI watchdog(void *arg)
{
    CONTEXT ctx;
    (void)arg;
    for (;;) {
        Sleep(50);
        if (!in_module || GetTickCount() - call_start < HANG_MS)
            continue;
        SuspendThread(main_thread);
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (in_module && GetTickCount() - call_start >= HANG_MS && GetThreadContext(main_thread, &ctx)
            && cur && (uint8_t *)ctx.Eip >= cur->lo && (uint8_t *)ctx.Eip < cur->hi) {
            hung = 1;
            if (getenv("DYNHANG") && cur == &retail) {
                uint8_t *q = (uint8_t *)ctx.Esi; if (q < arena || q >= arena + ARENA_SIZE) q = arena;
                printf("hang esi %08lx arena %p G %p edi %08lx eip %08lx: %02x %02x %02x %02x\n", ctx.Esi, (void *)arena, (void *)global, ctx.Edi, ctx.Eip, q[0], q[1], q[2], q[3]);
            }
            ctx.Eip = (DWORD)fault_land;
            SetThreadContext(main_thread, &ctx);
        }
        ResumeThread(main_thread);
    }
    return 0;
}

static void set_precision(unsigned p)
{
    static const uint16_t cw[3] = { 0x027f, 0x037f, 0x007f };
    uint16_t c = cw[p];
    __asm__ volatile("fldcw %0" : : "m"(c));
}

static void call_frame(void (*fn)(void), uint32_t ecx)
{
    __asm__ volatile("call *%1" : "+c"(ecx) : "r"(fn) : "eax", "edx", "memory", "cc");
}

static void set_players(const struct player *pl, unsigned n)
{
    unsigned k;
    memset(mp_players, 0, sizeof mp_players);
    memset(mp_pos, 0, sizeof mp_pos);
    for (k = 0; k < n; k++) {
        const struct player *p = &pl[k];
        uint8_t *r = mp_players[k], *q = mp_pos[k];
        *(uint32_t *)(r + 0x4) = p->id;
        *(uint32_t *)(r + 0x8) = p->active;
        *(uint32_t *)(r + 0xc) = p->flag;
        *(uint32_t *)(r + 0x28) = (uint32_t)q;
        *(uint32_t *)(r + 0x30) = p->speed;
        *(uint32_t *)(r + 0x34) = p->height;
        *(uint32_t *)(r + 0x38) = p->kind;
        *(uint32_t *)(r + 0xcc) = p->ground;
        *(uint16_t *)(q + 4 + 2) = p->latf;
        *(uint32_t *)(q + 4 + 4) = p->lat;
        *(uint16_t *)(q + 4 + 0xa) = p->lonf;
        *(uint32_t *)(q + 4 + 0xc) = p->lon;
        *(uint32_t *)(q + 4 + 0x14) = p->alt;
        *(uint32_t *)(q + 4 + 0x20) = p->id * 0x1000000u;
    }
    mp_head[0] = n;
    mp_head[1] = (uint32_t)mp_players;
}

static void poke(const struct op *o)
{
    unsigned k;
    switch (o->a % 10) {
    case 0: k = o->b % 4; G32(0x1180 + 4 * k) = !G32(0x1180 + 4 * k); break;
    case 1: G32(0x1190) = o->b % 6; break;
    case 2: G32(0x268) = o->b % 4 == 0; break;
    case 3: G16(0x2aa) = (uint16_t)(0x200 + o->b % 0x4000); G16(0xc1a) = (uint16_t)(0x100 << o->c % 7); break;
    case 4: G32(0x2b4) = o->b % 3 ? o->b % 0x7fff : o->b; G16(0x5dc) = (uint16_t)(o->c % 4 == 0); break;
    case 5: G8(0x609) = (uint8_t)(o->b % 4 ? o->b % 4 : o->b); break;
    case 6: G16(0xc44) = (uint16_t)(o->b % 2 ? 0 : o->c % 120); break;
    case 7: G8(0x238) = (uint8_t)(o->b % 24); G8(0x239) = (uint8_t)(o->c % 60);
            G16(0xef0) = (uint16_t)(o->d % 400); G16(0xef2) = (uint16_t)o->a; break;
    case 8: G16(0x3e0) = (uint16_t)(o->b & 1); break;
    case 9: for (k = 0; k < 4; k++) G32(0x1180 + 4 * k) = (o->b >> k) & 1; break;
    }
    put("\nPOKE %u", o->a % 10);
}

/* the user's aircraft, onto a point of the block or onto a live object */
static void move_user(const struct scenario *s, const struct op *o)
{
    uint32_t lat, lon, alt, objs, n;
    if (o->b % 4 == 0 && (objs = *(uint32_t *)(cur->image + 0x1f0)) != 0
        && (uint8_t *)objs >= arena && (uint8_t *)objs < arena + ARENA_SIZE
        && (n = *(uint32_t *)objs) != 0 && n < 0x100) {
        uint8_t *ob = (uint8_t *)objs + 4 + 0x120 * (o->c % n);
        if (ob + 0x120 <= arena + ARENA_SIZE && *(uint32_t *)(ob + 0x64)) {
            G32(0x564) = *(uint32_t *)(ob + 4) + o->d % 8 - 4;
            G32(0x56c) = *(uint32_t *)(ob + 0xc) + (o->d >> 8) % 512 - 256;
            G32(0x574) = *(uint32_t *)(ob + 0x14) + (o->d >> 16) % 3;
            put("\nMOVE obj%u", o->c % n);
            return;
        }
    }
    place(s->base, o->a, &lat, &lon, &alt);
    G32(0x564) = lat;
    G32(0x56c) = lon;
    G32(0x574) = alt;
    if (o->d % 3 == 0) {        /* ready to be met: low, on the ground, a kind 0x3d takes */
        G32(0x2b4) = o->c % 0x7fff;
        G16(0x5dc) = 0;
        G8(0x609) = (uint8_t)((uint32_t[]){ 0, 1, 3 }[o->c % 3]);
    }
    put("\nMOVE");
}

static void dispatch(uint32_t list);

/* The user's aircraft where 0x40's or 0x43's box test, rotated by UTIL as
 * DYNAMIC rotates it, puts it exactly `along` (or one either side) from the
 * centre: the inverse rotation, searched a few units about, then a
 * longitude offset whose metres through S_LON_TO_M come out right. */
static int edge(const struct poi *p, const struct op *o)
{
    int16_t sn, cs;
    int32_t want = (int32_t)p->along + (int32_t)(o->d % 3) - 1, x0, y0, x, y, ox, oy, u;
    uint32_t l2m = *(uint32_t *)(cur->image + 0x200);
    if (!l2m || p->along > 0x10000) return 0;
    if (o->b & 4) want = -want;
    UTIL(0x2c, sincos_fn)(p->hdg, &sn, &cs);
    x0 = (int32_t)((int64_t)want * cs / 32767);
    y0 = (int32_t)((int64_t)want * sn / 32767);
    for (y = y0 - 3; y <= y0 + 3; y++)
        for (x = x0 - 3; x <= x0 + 3; x++) {
            UTIL(0x34, rot_fn)(x, y, (uint16_t)sn, (uint16_t)cs, &ox, &oy);
            if (ox != want || (uint32_t)(oy < 0 ? -oy : oy) > p->r) continue;
            for (u = (int32_t)(((int64_t)x << 32) / l2m) - 3; u <= (int32_t)(((int64_t)x << 32) / l2m) + 3; u++)
                if ((int32_t)((int64_t)u * (int32_t)l2m >> 32) == x) {
                    G32(0x564) = p->lat + (uint32_t)y;
                    G32(0x56c) = p->lon + (uint32_t)u;
                    put("\nEDGE %d", want);
                    return 1;
                }
        }
    return 0;
}

/* the user's aircraft exactly where a 0x3d, 0x40 or 0x43 tests, in the state
 * it accepts -- low, on the ground, a kind 0x3d takes -- and the table of
 * aircraft brought up to date */
static void meet(const struct scenario *s, const struct op *o)
{
    unsigned k, m = 0, pick[MAXPOI];
    const struct poi *p;
    if (s->base == ncd + 2) {
        /* north of the truck by 99, 100 or 101 -- either side of 0x3e's
         * 100 m -- south by 100, on it, or past its 0x3d's 200 */
        static const int32_t dl[] = { 99, 100, 101, 100, -100, 0, 300 };
        int32_t dlat = dl[o->c % 7];
        G32(0x564) = FUEL_LAT + (uint32_t)dlat;
        G32(0x56c) = FUEL_LON;
        G32(0x574) = o->a % 3;
        G32(0x2b4) = o->d % 0x7fff;
        G16(0x5dc) = 0;
        G8(0x609) = (uint8_t)(o->b % 3 == 2 ? 3 : o->b % 3);
        if (o->b % 4 == 0) {
            /* a player there instead, the user away: its countdown, 0x50000,
             * is a whole number of the block's 0x5000 frames */
            struct player pl;
            memset(&pl, 0, sizeof pl);
            pl.id = 100;
            pl.active = 1;
            pl.speed = 5 + o->c % 20;
            pl.height = o->d % 0x7fff;
            pl.kind = (uint32_t[]){ 0, 1, 3 }[o->a % 3];
            pl.lat = FUEL_LAT + (uint32_t)dlat;
            pl.lon = FUEL_LON;
            set_players(&pl, 1);
            G32(0x564) = FUEL_LAT + 3000;
            put("\nMEET truck player %d", dlat);
            dispatch(3);
            return;
        }
        put("\nMEET truck %d", dlat);
        dispatch(3);
        return;
    }
    if (s->base == ncd + 3) {
        G32(0x564) = MOVES_A_LAT + 200 + o->c % 120 - 60;
        G32(0x56c) = FUEL_LON + o->d % 64 - 32;
        G32(0x574) = o->a % 11;
        put("\nMEET box");
        dispatch(3);
        return;
    }
    for (k = 0; k < npoi[s->base]; k++)
        if (pois[s->base][k].kind == 0x3d || (o->b % 3 == 0 && pois[s->base][k].kind >= 0x40))
            pick[m++] = k;
    if (!m) { put("\nMEET none"); return; }
    p = &pois[s->base][pick[o->c % m]];
    G32(0x564) = p->lat + (p->r ? o->d % (p->r + 1) - p->r / 2 : 0);
    G32(0x56c) = p->lon + (o->d >> 8) % 64 - 32;
    if (p->kind >= 0x40 && o->a % 2) {
        edge(p, o);     /* on the box's long edge, exactly, as UTIL rotates it */
    }
    G32(0x574) = p->alt + (p->kind == 0x3d ? o->a % 3 : o->a % 12);
    G32(0x2b4) = o->a % 9 ? o->d % 0x7fff : 0x8000;
    G16(0x5dc) = (uint16_t)(o->a % 11 == 0);
    if (p->kind == 0x3d)
        G8(0x609) = (uint8_t)(p->want == 1 ? 0 : p->want == 2 ? (o->b % 2 ? 1 : 3)
                              : (uint32_t[]){ 0, 1, 3, 2 }[o->b % 4]);
    put("\nMEET %02x", p->kind);
    dispatch(3);
}

/* an object's scenery class switched off -- one running code it generated,
 * when there is one -- and list 4 run, which frees it */
static void switch_off(const struct op *o)
{
    uint32_t objs = *(uint32_t *)(cur->image + 0x1f0), n, k, mask, pick = 0, found = 0;
    uint8_t *ob, *decl;
    if (!objs || (uint8_t *)objs < arena || (uint8_t *)objs >= arena + ARENA_SIZE
        || (n = *(uint32_t *)objs) == 0 || n > 0x100) { put("\nOFF none"); return; }
    for (k = 0; k < n; k++) {
        ob = (uint8_t *)objs + 4 + 0x120 * ((k + o->b) % n);
        if (*(uint32_t *)(ob + 0x64) && (!found || *(uint32_t *)(ob + 0x70))) {
            pick = (k + o->b) % n;
            found = 1 + (*(uint32_t *)(ob + 0x70) != 0);
            if (found == 2) break;
        }
    }
    if (!found) { put("\nOFF none"); return; }
    ob = (uint8_t *)objs + 4 + 0x120 * pick;
    decl = (uint8_t *)*(uint32_t *)(ob + 0x64);
    if (decl < arena || decl >= arena + ARENA_SIZE) { put("\nOFF none"); return; }
    if (o->c % 3) {
        mask = decl[7] >> 4;
        for (k = 0; k < 4; k++)
            if (mask >> k & 1) G32(0x1180 + 4 * k) = 0;
    } else {
        G32(0x1190) = (decl[7] & 0xf) ? (decl[7] & 0xf) - 1u : 0;
    }
    put("\nOFF obj%u%s", pick, found == 2 ? " code" : "");
    dispatch(4);
}

static void dispatch(uint32_t list)
{
    put(" [");
    chain->dispatch(list, 0, 0);
    put("]");
}

static void one_frame(struct side *sd, uint32_t ecx)
{
    call_start = GetTickCount();
    call_frame(sd->link->frame, ecx);
    if (getenv("DYNTRACE")) {
        put(" S%x/%x/%x/%x/%x/%x/%x", *(uint32_t *)(sd->image + 0x224), *(uint32_t *)(sd->image + 0x228),
            *(uint32_t *)(sd->image + 0x22c), *(uint32_t *)(sd->image + 0x230), *(uint32_t *)(sd->image + 0x234),
            *(uint32_t *)(sd->image + 0x238), *(uint32_t *)(sd->image + 0x23c));
    }
    if (getenv("DYNTRACE")) {
        uint32_t objs = *(uint32_t *)(sd->image + 0x1f0), k;
        if (objs && (uint8_t *)objs >= arena && (uint8_t *)objs < arena + ARENA_SIZE)
            for (k = 0; k < *(uint32_t *)objs && k < 4; k++) {
                uint8_t *ob = (uint8_t *)objs + 4 + 0x120 * k;
                uint8_t *pc = (uint8_t *)*(uint32_t *)(ob + 0x68);
                put(" [t%x.%04x r%04x/%04x c%x L%x]", *(uint32_t *)(ob + 0x88), *(uint16_t *)(ob + 0x86),
                    *(uint16_t *)(ob + 0x94), *(uint16_t *)(ob + 0x96), *(uint32_t *)(ob + 0x60),
                    *(uint32_t *)(sd->image + 0x204));
                put(" d%08x o%u:%d/%02x/%x.%04x", digest, k, (int)(pc - arena),
                    pc >= arena && pc < arena + ARENA_SIZE ? *pc : 0xff,
                    *(uint32_t *)(ob + 0x54), *(uint16_t *)(ob + 0x52));
            }
    }
    put(" c%x", *(uint32_t *)(sd->image + 0x20c));
    if (counting) cover_frames++;
}

static void count_state(struct side *sd)
{
    uint32_t objs = *(uint32_t *)(sd->image + 0x1f0), n, k, pc;
    if (*(uint32_t *)(sd->image + 0x24c)) cover_lock++;
    if (!objs || (uint8_t *)objs < arena || (uint8_t *)objs >= arena + ARENA_SIZE) return;
    n = *(uint32_t *)objs;
    if (n > 0x100) return;
    if (n > cover_maxobj) cover_maxobj = n;
    for (k = 0; k < n; k++) {
        uint8_t *ob = (uint8_t *)objs + 4 + 0x120 * k;
        if (ob + 0x120 > arena + ARENA_SIZE || !*(uint32_t *)(ob + 0x64)) continue;
        cover_objects++;
        pc = *(uint32_t *)(ob + 0x68);
        if ((uint8_t *)pc >= arena && (uint8_t *)pc < arena + ARENA_SIZE)
            cover_pc[*(uint8_t *)pc <= 0x44 ? *(uint8_t *)pc : 0x45]++;
    }
}

static void run(struct side *sd, const struct scenario *s, int count)
{
    volatile unsigned i;
    unsigned k;
    uint32_t tanks, keep;

    cur = sd;
    counting = count;
    memcpy(sd->image, sd->pristine, IMAGE_SIZE);
    memset(global, 0, GLOBAL_SIZE);
    memcpy(global, s->g, GLOBAL_USED);
    speed_rec[3] = s->speed;
    G32(0x11e0) = G32(0x11e0) ? (uint32_t)speed_rec : 0;
    /* what the host has not handed out reads 0xa5: past any opcode, so a
     * program counter that strays there stops rather than spinning on 0 */
    memset(arena - ARENA_START, 0xa5, ARENA_SIZE);
    arena_used = 0;
    for (k = 0; k < s->nrec; k++) {
        fe_recs[k].size = s->rec_size[k];
        fe_recs[k].data = (uint8_t *)s->rec[k];
    }
    fe_n = s->nrec;
    fe_cursor = 0;
    wrap_size = s->wrap_at ? s->rec_size[0] : 0;
    wrap_off = s->wrap_at - 1;
    wrap_low = 0xfff9 + s->wrap_at % 7;
    set_players(s->pl, s->nplayers);
    tlen = 0;
    tlog[0] = 0;
    digest = 0;
    util_verbose = getenv("DYNUTIL") != NULL;
    chain->init();
    set_precision(s->precision);
    tanks = G32(0xb74) ^ G32(0xb7c) ^ G32(0xb94) ^ G32(0xb84) ^ G32(0xb9c);

    run_hung = 0;
    put("\nINIT");
    sd->link->init();
    keep = rs;
    for (i = 0; i < s->nops; i++) {
        const struct op *o = &s->op[i];
        /* what an op draws at run time comes from the op, the same on both sides */
        rs = (o->a ^ o->d) | 1;
        if (count) cover_ops[o->kind]++;
        hung = 0;
        call_start = GetTickCount();
        in_module = 1;
        if (__builtin_setjmp(fault_jb)) {
            in_module = 0;
            if (hung) {
                /* where the spin was cut is timing: the state after it is
                 * not comparable, so the scenario ends here and only the
                 * transcript is compared */
                put(" HANG");
                run_hung = 1;
                in_module = 0;
                if (count) cover_hang++;
                break;
            } else if (count) {
                cover_fault++;
            }
            set_precision(s->precision);
            put(" u%08x", digest);
            continue;
        }
        switch (o->kind) {
        case OP_FRAME: {
            uint32_t ecx = o->b % 4 ? o->c | 0x10000 : o->c % 4;
            put("\nFRAME %u", ecx);
            one_frame(sd, ecx);
            break;
        }
        case OP_TICK: {
            unsigned n = 2 + o->a % 30;
            put("\nTICK %u", n);
            for (k = 0; k < n; k++) {
                if ((o->b >> (k % 32)) & 1) dispatch(3);
                one_frame(sd, 0x10000 + k);
                if ((o->c >> (k % 32)) & 1) dispatch(16);
                if (k % 8 == o->d % 8) dispatch(4);
                if (count) count_state(sd);
            }
            break;
        }
        case OP_L3:
            put("\nLIST3");
            dispatch(3);
            break;
        case OP_L4:
            put("\nLIST4");
            dispatch(4);
            if (count && *(uint32_t *)(sd->image + 0x1ec)) cover_blocks++;
            break;
        case OP_L16:
            put("\nLIST16");
            dispatch(16);
            break;
        case OP_POKE:
            poke(o);
            break;
        case OP_MOVE:
            move_user(s, o);
            break;
        case OP_PLAYERS: {
            struct player pl[MAXPLAYERS];
            unsigned n = o->b % (MAXPLAYERS + 1);
            for (k = 0; k < n; k++) make_player(s->base, &pl[k], k);
            set_players(pl, n);
            put("\nPLAYERS %u", n);
            break;
        }
        case OP_MEET:
            meet(s, o);
            break;
        case OP_OFF:
            switch_off(o);
            break;
        case OP_DEINIT:
            put("\nDEINIT");
            sd->link->deinit();
            break;
        case OP_INIT:
            put("\nINIT");
            sd->link->init();
            break;
        }
        in_module = 0;
        put(" u%08x", digest);
    }
    in_module = 0;
    rs = keep;
    if (count) {
        if ((G32(0xb74) ^ G32(0xb7c) ^ G32(0xb94) ^ G32(0xb84) ^ G32(0xb9c)) != tanks) cover_fuel++;
        if (G32(0x348)) cover_g348++;
    }
}

/* ---- comparing -------------------------------------------------------------------- */

static uint32_t pair_r[128], pair_o[128];
static unsigned npairs;

static int same_word(uint32_t r, uint32_t o)
{
    unsigned k;
    int rd, od;
    if (r == o)
        return 1;
    rd = in_image((uint8_t *)r, &retail);
    od = in_image((uint8_t *)o, &ours);
    if (rd || od)
        return rd && od && (uint8_t *)r - retail.image == (uint8_t *)o - ours.image;
    if (is_code(&retail, r) || is_code(&ours, o)) {
        if (!is_code(&retail, r) || !is_code(&ours, o)) return 0;
        for (k = 0; k < npairs; k++)
            if (pair_r[k] == r || pair_o[k] == o) return pair_r[k] == r && pair_o[k] == o;
        if (npairs < 128) { pair_r[npairs] = r; pair_o[npairs] = o; npairs++; }
        return 1;
    }
    return 0;
}

static long compare(const uint8_t *r, const uint8_t *o, unsigned n)
{
    unsigned i;
    if (!memcmp(r, o, n))
        return -1;
    for (i = 0; i + 4 <= n; i += 4)
        if (!same_word(*(const uint32_t *)(r + i), *(const uint32_t *)(o + i)))
            return (long)i;
    for (; i < n; i++)
        if (r[i] != o[i]) return (long)i;
    return -1;
}

static void dump_words(const char *what, const uint8_t *r, const uint8_t *o, unsigned n, long base)
{
    unsigned i, shown = 0;
    if (!memcmp(r, o, n)) return;
    for (i = 0; i + 4 <= n && shown < 10; i += 4)
        if (!same_word(*(const uint32_t *)(r + i), *(const uint32_t *)(o + i))) {
            printf("  %s%+ld: retail %08x ours %08x\n", what, (long)i + base, *(const uint32_t *)(r + i),
                   *(const uint32_t *)(o + i));
            shown++;
        }
}

/* ---- loading ---------------------------------------------------------------------- */

static uint8_t host_blob[0x320], global_dummy[4], fe_blob[0x60], g3d_blob[0x40], mp_blob[0x30];

static void bind_imports(struct import *im)
{
    for (; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host_blob; break;
        case 2:  im->link = global; break;
        case 4:  im->link = chain_fwd; break;
        case 14: im->link = g3d_blob; break;
        case 18: im->link = fe_blob; break;
        case 20: im->link = util_fwd; break;
        case 35: im->link = mp_blob; break;
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
    sd->link = (struct dyn_linkage *)GetProcAddress(sd->dll, "Linkage");
    if (!imports || !sd->link || sd->link->id != 6) { printf("%s: not DYNAMIC\n", path); return 0; }
    sd->image = (uint8_t *)sd->link - 0x40;
    if ((uint8_t *)imports != sd->image) { printf("%s: ImportTable is not at Linkage-0x40\n", path); return 0; }
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)sd->dll + ((IMAGE_DOS_HEADER *)sd->dll)->e_lfanew);
    sd->lo = (uint8_t *)sd->dll;
    sd->hi = sd->lo + nt->OptionalHeader.SizeOfImage;
    bind_imports(imports);
    memcpy(sd->pristine, sd->image, IMAGE_SIZE);
    return 1;
}

static void *load_support(const char *path, uint32_t id, uint8_t **lo, uint8_t **hi)
{
    HMODULE m = LoadLibraryA(path);
    IMAGE_NT_HEADERS *nt;
    struct import *im;
    void *l;
    if (!m) { printf("cannot load %s\n", path); exit(2); }
    l = (void *)GetProcAddress(m, "Linkage");
    if (!l || *(uint32_t *)l != id) { printf("%s: wrong module\n", path); exit(2); }
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)m + ((IMAGE_DOS_HEADER *)m)->e_lfanew);
    *lo = (uint8_t *)m;
    *hi = *lo + nt->OptionalHeader.SizeOfImage;
    for (im = (struct import *)GetProcAddress(m, "ImportTable"); im->id; im++)
        im->link = im->id == 1 ? (void *)host_blob : (void *)global;
    return l;
}

static uint8_t *guarded(uint32_t size, uint32_t before)
{
    uint8_t *r = VirtualAlloc(NULL, size + before + GUARD, MEM_RESERVE, PAGE_NOACCESS);
    if (!r || !VirtualAlloc(r + before, size, MEM_COMMIT, PAGE_READWRITE)) {
        printf("VirtualAlloc failed\n");
        exit(2);
    }
    return r + before;
}

#define SET(blob, off, fn) (*(void **)((blob) + (off)) = (void *)(fn))

int main(int argc, char **argv)
{
    static char rlog[sizeof tlog];
    static uint8_t rimage[IMAGE_SIZE], rglobal[GLOBAL_SIZE];
    static struct scenario s;
    static const char *const bgls[] = {
        "chicago5", "dallas5", "denver5", "kanci5", "losang5", "miami5", "newor5",
        "newyork5", "saltlk5", "seatac5", "sf5", "stlouis5"
    };
    uint8_t *rarena;
    unsigned v, n = argc > 1 ? (unsigned)atoi(argv[1]) : 20000, bad = 0, k;
    uint32_t rused;
    long at;
    char path[64];

    (void)global_dummy;
    AddVectoredExceptionHandler(1, on_vectored);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);

    /* the arena with room below its first allocation; GLOBAL; both guarded */
    arena = guarded(ARENA_SIZE, GUARD) + ARENA_START;
    global = guarded(GLOBAL_SIZE, GUARD);
    rarena = malloc(ARENA_SIZE);

    for (k = 0; k < sizeof bgls / sizeof bgls[0]; k++) {
        snprintf(path, sizeof path, "cdscenery\\%s.bgl", bgls[k]);
        load_bgl(path);
    }
    if (ncd != 12) { printf("expected 12 dynamic-scenery blocks, found %u\n", ncd); return 2; }
    for (k = 0; k < ncd; k++)
        find_pois(k);
    make_many();
    make_ease();
    make_fuel();
    make_moves();

    *(uint32_t *)host_blob = 1;
    SET(host_blob, 0x20c, h_alloc);
    SET(host_blob, 0x210, h_realloc);
    SET(host_blob, 0x214, h_free);
    SET(host_blob, 0x218, h_memset);
    SET(host_blob, 0x21c, h_memmove);
    *(uint32_t *)fe_blob = 18;
    SET(fe_blob, 0x40, fe_read);
    SET(fe_blob, 0x54, fe_search);
    *(uint32_t *)g3d_blob = 14;
    SET(g3d_blob, 0x38, g3d_draw);
    *(uint32_t *)mp_blob = 35;
    SET(mp_blob, 0x24, mp_get);

    chain = (struct chain_linkage *)load_support("CHAIN_retail.dll", 4, &chain_lo, &chain_hi);
    *(uint32_t *)chain_fwd = 4;
    SET(chain_fwd, 0x18, c_add);
    SET(chain_fwd, 0x1c, c_remove);
    util_link = (uint8_t *)load_support("UTIL_retail.dll", 20, &util_lo, &util_hi);
    *(uint32_t *)util_fwd = 20;
    SET(util_fwd, 0x18, t_sqrt64);
    SET(util_fwd, 0x1c, t_sqrt32);
    SET(util_fwd, 0x2c, t_sincos);
    SET(util_fwd, 0x34, t_rotate);
    SET(util_fwd, 0x38, t_asin);
    SET(util_fwd, 0x3c, t_atan2);
    SET(util_fwd, 0x44, t_to_lon);
    SET(util_fwd, 0x48, t_to_m);

    if (!load_side(&retail, "DYNAMIC_retail.dll", "retail")) return 2;
    if (!load_side(&ours, "DYNAMIC_ours.dll", "openfs98")) return 2;
    if ((at = compare(retail.pristine, ours.pristine, IMAGE_SIZE)) >= 0) {
        printf("initial images differ at +%#lx\n", at);
        dump_words("img", retail.pristine, ours.pristine, IMAGE_SIZE, 0);
        bad++;
    }

    for (v = 0; v < n; v++) {
        make_scenario(&s);
        if (getenv("DYNONLY") && v != (unsigned)atoi(getenv("DYNONLY"))) continue;
        run(&retail, &s, 1);
        memcpy(rlog, tlog, tlen + 1);
        memcpy(rimage, retail.image, IMAGE_SIZE);
        memcpy(rglobal, global, GLOBAL_SIZE);
        rused = arena_used;
        rhung = run_hung;
        cover_hung_runs += rhung;
        memcpy(rarena, arena - ARENA_START, ARENA_SIZE);
        run(&ours, &s, 0);
        if (strcmp(rlog, tlog) || (!rhung && (0
            || compare(rimage, ours.image, IMAGE_SIZE) >= 0
            || memcmp(rglobal, global, GLOBAL_SIZE)
            || rused != arena_used || compare(rarena, arena - ARENA_START, ARENA_SIZE) >= 0))) {
            if (bad++ < 4) {
                unsigned a = 0;
                while (rlog[a] && rlog[a] == tlog[a]) a++;
                printf("scenario %u (block %s, %u records) differs", v, cd[s.base].name, s.nrec);
                if (rlog[a] || tlog[a])
                    printf(" in the transcript at byte %u:\n  retail ...%.600s\n  ours   ...%.600s\n",
                           a, rlog + (a > 400 ? a - 400 : 0), tlog + (a > 400 ? a - 400 : 0));
                else
                    printf(":\n");
                dump_words("img", rimage, ours.image, IMAGE_SIZE, 0);
                for (k = 0; k < GLOBAL_SIZE && rglobal[k] == global[k]; k++) ;
                if (k < GLOBAL_SIZE) printf("  global+%#x: retail %02x ours %02x\n", k, rglobal[k], global[k]);
                if (rused != arena_used) printf("  arena used: retail %#x ours %#x\n", rused, arena_used);
                dump_words("A", rarena, arena - ARENA_START, ARENA_SIZE, -(long)ARENA_START);
            }
            if (getenv("DYNFILES")) {
                FILE *f = fopen("rlog.txt", "wb");
                fputs(rlog, f);
                fclose(f);
                f = fopen("olog.txt", "wb");
                fputs(tlog, f);
                fclose(f);
            }
            if (getenv("DYNSTOP")) break;       /* for deliberate breaks: the first is enough */
        }
        if (getenv("DYNDUMP") && v == (unsigned)atoi(getenv("DYNDUMP"))) printf("%s\n", rlog);
    }

    printf("ops:");
    for (k = 0; k < OP_COUNT; k++) printf(" %s %lu", op_names[k], cover_ops[k]);
    printf("\nframes %lu, blocks loaded %lu, fe none/probe/read %lu/%lu/%lu, draws %lu\n",
           cover_frames, cover_blocks, cover_fe[0], cover_fe[1], cover_fe[2], cover_draw);
    printf("live objects seen %lu, most at once %lu; user held %lu; tanks changed %lu; +0x348 set %lu\n",
           cover_objects, cover_maxobj, cover_lock, cover_fuel, cover_g348);
    printf("util sqrt64 %lu sqrt32 %lu sincos %lu rotate %lu asin %lu atan2 %lu tolon %lu tom %lu\n",
           cover_util[0], cover_util[1], cover_util[2], cover_util[3], cover_util[4], cover_util[5],
           cover_util[6], cover_util[7]);
    printf("objects stopped at:");
    for (k = 0; k < 0x46; k++) if (cover_pc[k]) printf(" %02x:%lu", k, cover_pc[k]);
    printf("\npath points emptied %lu, turns put across 64K %lu", cover_emptied, cover_wrapped);
    printf("\nfaults %lu, hangs %lu (scenarios ended by one, state not compared: %lu)\n",
           cover_fault, cover_hang, cover_hung_runs);
    printf("%u scenarios, %u differ\n", n, bad);
    if (!bad) printf("IDENTICAL\n");
    (void)cover_alloc;
    return bad != 0;
}
