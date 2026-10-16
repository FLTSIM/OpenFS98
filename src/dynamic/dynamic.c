/* DYNAMIC.DLL -- module id 6: dynamic scenery.
 *
 * The aircraft that taxi, take off and land at the big airports, and the
 * fuel truck that drives out to the user's aircraft.  Each is a program in a
 * small bytecode that the airport's scenery file carries; FE (+0x40) hands
 * DYNAMIC the block of them nearest the aircraft, CHAIN list 4 runs the
 * block's declarations -- one per object, each switched on or off by GLOBAL's
 * dynamic-scenery settings -- and every frame (+0x18) steps each live object's
 * program a number of times scaled from the frame time and sim rate.  CHAIN
 * list 16 hands the objects to G3D to draw and list 3 keeps a table of the
 * aircraft about -- the user's, and MULTIP's players -- which the programs
 * read.  Three opcodes write programs of their own at run time: a drive to an
 * aircraft and back, a take-off and circuit, a turn onto a heading.
 *
 * Retail is hand-written assembly, transliterated here routine by routine;
 * the comments carry retail's addresses and docs/modules/dynamic.md has the
 * walk through.  Before calling into it, know that:
 *
 *   - Everything lives in one .data image laid out exactly as retail's,
 *     ImportTable at +0x00 and Linkage at +0x40, and the code addresses its
 *     state by retail's offsets.  Linkage+0x20 on is the interpreter's
 *     dispatch table, which the interpreter calls through: opcode n is
 *     handler n, as in retail.
 *   - The assembly keeps the program counter in esi and the object in edi
 *     across every handler and helper, and handlers change them -- a
 *     declaration run from inside a program moves edi to another object, or
 *     to GLOBAL, and the caller carries on with it.  So esi and edi are file
 *     variables here, named after the registers, and every routine reads and
 *     writes them as retail's does.  A handler returns the carry flag.
 *   - Memory is read and written through volatile accessors on 32-bit
 *     addresses, one access per instruction, in retail's order.  The
 *     programs come from scenery files and nothing checks them; a bad one
 *     faults, or hangs, at the same instruction it would in retail.  The
 *     divides are x86 divides for the same reason (see the div helpers), and
 *     the one floating-point step is retail's x87 sequence (dyn_scale).
 *   - +0x18 depends on ecx at entry: when there are no objects its stepping
 *     loop runs on the caller's ecx (see dyn_frame).  It is entered through a
 *     stub that passes ecx on.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define DYN_IMAGE_SIZE 0x250        /* .data's virtual size */
#define DYN_NOPS       0x44

typedef int (*dyn_op_fn)(void);     /* returns the carry flag */
typedef void (FS98_STDCALL *dyn_cb_fn)(uint32_t a, uint32_t b);

struct dyn_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;
    void    *dialogs;                   /* +0x14: none */
    void    (*frame)(void);             /* +0x18: MAIN, every frame; reads ecx */
    uint32_t reserved_1c;
    dyn_op_fn ops[DYN_NOPS];            /* +0x20: the dispatch table */
};

struct dyn_image {
    struct fs98_import import[8];       /* +0x000 ImportTable */
    struct dyn_linkage linkage;         /* +0x040 Linkage */
    uint8_t data[DYN_IMAGE_SIZE - 0x170]; /* +0x170 on */
};

extern struct dyn_image dyn_image;

typedef char (*dyn_layout_check)[sizeof(struct dyn_image) == DYN_IMAGE_SIZE &&
                                 sizeof(struct dyn_linkage) == 0x130 ? 1 : -1];

/* ---- memory, as the registers address it ------------------------------------ */

typedef volatile uint8_t  __attribute__((may_alias)) dyn_v8;
typedef volatile uint16_t __attribute__((may_alias)) dyn_v16;
typedef volatile uint32_t __attribute__((may_alias)) dyn_v32;

#define M8(a)   (*(dyn_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(dyn_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(dyn_v32 *)(uintptr_t)(uint32_t)(a))
#define SX16(v) ((uint32_t)(int32_t)(int16_t)(v))      /* movsx */
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

/* The image, by retail's offsets (0x206a6xxx is D + 0xxx). */
#define D           ADDR(&dyn_image)
#define S_PLAYERS   M32(D + 0x1e8)  /* count, then 0x48-byte aircraft records */
#define S_BLOCK     M32(D + 0x1ec)  /* FE's block: bounds, declarations, programs */
#define S_OBJECTS   M32(D + 0x1f0)  /* count, then 0x120-byte objects */
#define S_ORIGIN    (D + 0x1f4)     /* the latitude op_origin set, 48-bit */
#define S_M_TO_LON  M32(D + 0x1fc)  /* longitude per metre there, 16.16 */
#define S_LON_TO_M  M32(D + 0x200)  /* 2^48 / that */
#define S_LEFT      M32(D + 0x204)  /* time left in this move, at least S_DT */
#define S_DT        M32(D + 0x208)  /* this step's time */
#define S_CLOCK     M32(D + 0x20c)
#define S_BOX_N     M32(D + 0x214)  /* where the next FE search is due */
#define S_BOX_S     M32(D + 0x218)
#define S_BOX_E     M32(D + 0x21c)
#define S_BOX_W     M32(D + 0x220)
#define S_REVERSE   M32(D + 0x224)  /* emit_step writes backwards */
#define S_REPEAT    M32(D + 0x228)  /* emit_step's next record covers this many */
#define S_SPEED     M32(D + 0x22c)  /* emit_step's speed, eased to S_SPEED_TO */
#define S_PITCH_TO  M32(D + 0x230)
#define S_BANK_TO   M32(D + 0x234)
#define S_HDG_TO    M32(D + 0x238)  /* the heading once level again */
#define S_SPEED_TO  M32(D + 0x23c)
#define S_INITED    M32(D + 0x240)  /* the CHAIN callbacks are in */
#define S_RUN       M32(D + 0x244)  /* the program counter may advance */
#define S_FIRST     M32(D + 0x248)  /* this move's first step */
#define S_LOCK      M32(D + 0x24c)  /* an object has the user's aircraft */
#define S_LIB       (D + 0x170)     /* six library ids, 0x14 each */
#define S_LIB_END   (D + 0x1e8)

#define G           ADDR(dyn_image.import[1].link)

#define SLOT(i, off, type) (*(type *)((uint8_t *)dyn_image.import[i].link + (off)))

/* Every prototype read from the callee's `ret N`. */
typedef int32_t  (FS98_STDCALL *chain_add_fn)(uint32_t list, dyn_cb_fn cb);  /* ret 8 */
typedef void     (FS98_STDCALL *fe_search_fn)(uint32_t pos, int32_t radius,
                                              uint32_t box);              /* ret 0xc */
typedef int32_t  (FS98_STDCALL *fe_read_fn)(int32_t kind, uint32_t buf, int32_t size,
                                            int32_t *out);                /* ret 0x10 */
typedef void     (FS98_STDCALL *g3d_draw_fn)(uint32_t object, uint32_t state); /* ret 8 */
typedef uint32_t (*multip_players_fn)(void);                              /* ret */
typedef uint32_t (FS98_STDCALL *util_sqrt64_fn)(uint32_t p);              /* ret 4 */
typedef uint32_t (FS98_STDCALL *util_sqrt32_fn)(uint32_t v);              /* ret 4 */
typedef void     (FS98_STDCALL *util_sincos_fn)(int16_t angle, int16_t *s,
                                                int16_t *c);              /* ret 0xc */
typedef void     (FS98_STDCALL *util_rotate2d_fn)(int32_t x, int32_t y, int16_t s,
                                                  int16_t c, int32_t *ox,
                                                  int32_t *oy);           /* ret 0x18 */
typedef uint32_t (FS98_STDCALL *util_asin_fn)(int16_t v, int32_t quadrant); /* ret 8 */
typedef uint32_t (FS98_STDCALL *util_atan2_fn)(int32_t x, int32_t y);     /* ret 8 */
typedef void     (FS98_STDCALL *util_convert_fn)(uint32_t in48, uint32_t pos48,
                                                 uint32_t out48);         /* ret 0xc */

#define HOST_ALLOC      SLOT(0, FS98_HOST_S_ALLOC, fs98_host_alloc_fn)
#define HOST_REALLOC    SLOT(0, FS98_HOST_S_REALLOC, fs98_host_realloc_fn)
#define HOST_FREE       SLOT(0, FS98_HOST_S_FREE, fs98_host_free_fn)
#define CHAIN_ADD       SLOT(2, 0x18, chain_add_fn)
#define CHAIN_REMOVE    SLOT(2, 0x1c, chain_add_fn)
#define FE_SEARCH       SLOT(3, 0x54, fe_search_fn)
#define FE_READ         SLOT(3, 0x40, fe_read_fn)
#define G3D_DRAW        SLOT(4, 0x38, g3d_draw_fn)
#define UTIL_SQRT64     SLOT(5, 0x18, util_sqrt64_fn)
#define UTIL_SQRT32     SLOT(5, 0x1c, util_sqrt32_fn)
#define UTIL_SINCOS     SLOT(5, 0x2c, util_sincos_fn)
#define UTIL_ROTATE2D   SLOT(5, 0x34, util_rotate2d_fn)
#define UTIL_ASIN       SLOT(5, 0x38, util_asin_fn)
#define UTIL_ATAN2      SLOT(5, 0x3c, util_atan2_fn)
#define UTIL_TO_LON     SLOT(5, 0x44, util_convert_fn)
#define UTIL_TO_METRES  SLOT(5, 0x48, util_convert_fn)
#define MULTIP_PLAYERS  SLOT(6, 0x24, multip_players_fn)

#define ALLOC(size, pp, zero) HOST_ALLOC(1, (size), (void **)(uintptr_t)(pp), (zero))
#define REALLOC(size, pp)     HOST_REALLOC(1, (size), (void **)(uintptr_t)(pp))
#define FREE(pp)              HOST_FREE((void **)(uintptr_t)(pp))

/* The registers the assembly keeps across routines. */
static uint32_t esi;                /* the program counter */
static uint32_t edi;                /* the object */

/* ---- arithmetic as the processor does it -------------------------------------- */

/* div: edx:eax / d, unsigned.  The processor's own instruction, so a zero
 * divisor or a quotient past 32 bits faults as retail's does. */
static uint32_t udiv(uint32_t hi, uint32_t lo, uint32_t d, uint32_t *rem)
{
    uint32_t q, r;
    __asm__ volatile("divl %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    if (rem) *rem = r;
    return q;
}

/* idiv: edx:eax / d, signed */
static uint32_t sdiv(uint32_t hi, uint32_t lo, uint32_t d)
{
    uint32_t q, r;
    __asm__ volatile("idivl %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    return q;
}

/* idiv r/m16: dx:ax / d, signed, quotient in ax */
static uint16_t sdiv16(uint16_t hi, uint16_t lo, uint16_t d)
{
    uint16_t q, r;
    __asm__ volatile("idivw %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    return q;
}

static uint64_t smul(uint32_t a, uint32_t b)     /* imul r/m32: edx:eax */
{
    return (uint64_t)((int64_t)(int32_t)a * (int32_t)b);
}

static uint64_t umul(uint32_t a, uint32_t b)     /* mul r/m32 */
{
    return (uint64_t)a * b;
}

#define LO(x) ((uint32_t)(x))
#define HI(x) ((uint32_t)((x) >> 32))

static uint32_t iabs(uint32_t v)                /* cdq / xor / sub */
{
    return (int32_t)v < 0 ? 0u - v : v;
}

/* .text+0x3fd3 -- a 48-bit difference (ax frac, edx int) scaled by ecx/2^15
 * on the x87: fild the 64-bit value, fimul ecx, fistp.  Under any precision
 * control but 64-bit the product rounds, as retail's does. */
static void dyn_scale(uint16_t *ax, uint32_t *edx, uint32_t ecx)
{
    uint32_t lo, hi;
    uint64_t q;
    int32_t m = (int32_t)ecx;
    lo = ((uint32_t)*ax << 16 >> 16) | (*edx << 16);    /* shl eax,16 / shrd eax,edx,16 */
    hi = (uint32_t)((int32_t)*edx >> 16);               /* sar edx,16 */
    q = (uint64_t)hi << 32 | lo;
    __asm__ volatile("fildq %0\n\tfimull %1\n\tfistpq %0\n\tfwait"
                     : "+m"(q) : "m"(m) : "st");
    lo = (uint32_t)q;
    hi = (uint32_t)(q >> 32);
    *edx = (hi << 1) | (lo >> 31);                      /* shld edx,eax,1 */
    *ax = (uint16_t)(lo >> 15);                         /* shr eax,15 */
}

/* ---- forward ---------------------------------------------------------------------- */

static void FS98_STDCALL dyn_draw(uint32_t a, uint32_t b);
static void FS98_STDCALL dyn_load(uint32_t a, uint32_t b);
static void FS98_STDCALL dyn_players(uint32_t a, uint32_t b);
static int  path_end(uint32_t *ecx);
static int  timer(uint32_t *ecx);
static int  blocked(void);
static void step_object(void);

#define DISPATCH(op) (((dyn_op_fn)M32(ADDR(&dyn_image.linkage.ops[0]) + 4 * (op)))())

/* ---- the entries ------------------------------------------------------------------- */

/* .text+0x1020 -- +0x04 */
static void dyn_init(void)
{
}

/* .text+0x1027 -- +0x08, and +0x18 once the switches are all off: out of
 * CHAIN, and everything freed. */
static void dyn_teardown(void)
{
    if (S_INITED == 0)
        return;
    CHAIN_REMOVE(0x10, dyn_draw);
    CHAIN_REMOVE(4, dyn_load);
    CHAIN_REMOVE(3, dyn_players);
    if (S_PLAYERS != 0) {
        FREE(&S_PLAYERS);
        S_PLAYERS = 0;
    }
    if (S_OBJECTS != 0) {
        FREE(&S_OBJECTS);
        S_OBJECTS = 0;
    }
    if (S_BLOCK != 0) {
        FREE(&S_BLOCK);
        S_BLOCK = 0;
        S_BOX_N = 0;
        S_BOX_S = 0;
        S_BOX_E = 0;
        S_BOX_W = 0;
    }
    S_INITED = 0;
}

/* .text+0x15b2 -- step every live object once.  Returns ecx as retail leaves
 * it: zero, unless there is no object array, when it is untouched. */
static uint32_t step_all(uint32_t ecx)
{
    uint32_t saved, sum;
    uint16_t ax, dx;
    unsigned cl;

    edi = S_OBJECTS;
    if (edi == 0)
        return ecx;
    ecx = M32(edi);
    edi += 4;
    if (ecx == 0)
        return ecx;
    do {
        if (M32(edi + 0x64) != 0) {
            saved = ecx;
            step_object();
            if (M32(edi + 0x98) == 0) {
                /* not on a path: the position is its base, and the
                 * longitude its base plus the eastward metres */
                M16(edi + 0x2) = M16(edi + 0x4a);
                M32(edi + 0x4) = M32(edi + 0x4c);
                M16(edi + 0x12) = M16(edi + 0x5a);
                M32(edi + 0x14) = M32(edi + 0x5c);
                UTIL_TO_LON(edi + 0x50, S_ORIGIN, edi + 0x8);
                sum = (uint32_t)M16(edi + 0xa) + M16(edi + 0x42);
                M16(edi + 0xa) = (uint16_t)sum;
                M32(edi + 0xc) = M32(edi + 0xc) + M32(edi + 0x44) + (sum >> 16);
            }
            ax = (uint16_t)(M16(edi + 0x24) & 0xff8f);
            dx = (uint16_t)((uint16_t)(M16(G + 0x1172) << 4) & 0x70);
            M16(edi + 0x24) = (uint16_t)(ax | dx);
            ax = M16(G + 0x1176);
            cl = (edi >> 7) & 0xf;
            M16(edi + 0x26) = (uint16_t)(cl ? (ax >> cl) | (ax << (16 - cl)) : ax);
            ecx = saved;
        }
        edi += 0x120;
    } while (--ecx != 0);
    return ecx;
}

/* CHAIN_ADD, returning the ecx CHAIN leaves: +0x18's stepping loop counts on
 * it the frame DYNAMIC registers. */
static uint32_t chain_add_ecx(uint32_t list, dyn_cb_fn cb)
{
    uint32_t ecx;
    __asm__ volatile("pushl %2\n\tpushl %1\n\tcall *%3"
                     : "=c"(ecx) : "g"(list), "g"(cb), "r"(CHAIN_ADD)
                     : "eax", "edx", "memory", "cc");
    return ecx;
}

/* .text+0x1188 -- +0x18, every frame.  Steps the objects in slices of at most
 * about 0x6000 of time.  The loop ends when the time runs out, or on
 * `loop`'s ecx reaching zero: step_all leaves ecx zero whenever there are
 * objects, so `loop` carries on, but with no object array it is ecx as the
 * loop found it -- the caller's, or on the frame that registers with CHAIN,
 * whatever CHAIN's add left there -- counting down. */
static void __attribute__((used)) dyn_frame_ecx(uint32_t ecx)
{
    uint32_t total, step, eax, ebx;
    uint64_t p;

    if (M32(G + 0x268) != 0)
        return;
    if ((M32(G + 0x118c) | M32(G + 0x1188) | M32(G + 0x1184) | M32(G + 0x1180)) == 0) {
        dyn_teardown();
        return;
    }
    if (S_INITED == 0) {
        CHAIN_ADD(0x10, dyn_draw);
        CHAIN_ADD(4, dyn_load);
        if (S_PLAYERS == 0) {
            ALLOC(0x50, &S_PLAYERS, 1);
            M32(S_PLAYERS) = 1;
        }
        ecx = chain_add_ecx(3, dyn_players);
        S_INITED = 1;
    }
    eax = udiv(0, (uint32_t)M16(G + 0xc1a) << 16, 0x100, NULL);
    p = umul(eax, M16(G + 0x2aa));
    total = (LO(p) >> 16) | (HI(p) << 16);
    ebx = udiv(0, total, 0x6000, NULL) + 1;
    step = udiv(0, total, ebx, NULL) + 1;
    for (;;) {
        if ((int32_t)total <= 0)
            break;
        eax = total > step ? step : total;
        total -= eax;
        S_DT = eax;
        S_CLOCK = S_CLOCK + S_DT;
        ecx = step_all(ecx);
        if (--ecx == 0)
            break;
    }
}

/* The Linkage slot: MAIN calls it with whatever ecx it has. */
void dyn_frame(void);
__asm__(".text\n"
        "_dyn_frame:\n\t"
        "pushl %ecx\n\t"
        "call _dyn_frame_ecx\n\t"
        "addl $4, %esp\n\t"
        "ret");

/* .text+0x1309 -- CHAIN list 3: the table of aircraft.  Record 0 is the
 * user's, from GLOBAL; the rest are MULTIP's players, matched by id, added
 * as they appear and cleared when they are not seen. */
static void FS98_STDCALL dyn_players(uint32_t a, uint32_t b)
{
    uint32_t eax, ebx, ecx, edx, s, d, left, off, k;
    (void)a; (void)b;

    ebx = G;
    if (S_LOCK != 0)
        M32(ebx + 0x348) = 0;
    d = S_PLAYERS + 4;
    eax = M8(ebx + 0x609);
    M32(d + 0x40) = eax;
    M32(d + 0x30) = M16(ebx + 0x5dc);
    M32(d + 0x2c) = M32(ebx + 0x2b4);
    if ((int32_t)M32(d + 0x3c) > 0)
        M32(d + 0x3c) = M32(d + 0x3c) - 1;
    eax = M32(ebx + 0x11e0);
    eax = eax == 0 ? 0xa : M32(eax + 0xc);
    M32(d + 0x44) = eax + 3;
    M32(d + 0x24) = 1;
    for (k = 0; k < 0x24; k++)
        M8(d + k) = M8(ebx + 0x560 + k);
    ebx = S_PLAYERS;
    ecx = M32(ebx);
    if (--ecx != 0) {
        ebx += 0x4c;
        do {
            M32(ebx + 0x24) = 0;
            ebx += 0x48;
        } while (--ecx != 0);
    }

    eax = MULTIP_PLAYERS();
    ecx = M32(eax);
    if (ecx != 0) {
        left = ecx;
        s = M32(eax + 4);
        do {
            if (M32(s + 0x8) == 1 && M32(s + 0xc) == 0) {
                eax = M32(s + 0x4);
                ebx = S_PLAYERS;
                edx = M32(ebx);
                ebx += 4;
                for (;;) {
                    if (eax == M32(ebx + 0x28))
                        goto found;
                    ebx += 0x48;
                    if (--edx == 0)
                        break;
                }
                off = ebx - S_PLAYERS;
                ebx = off + 0x48;
                REALLOC(ebx, &S_PLAYERS);
                ebx = S_PLAYERS;
                M32(ebx) = M32(ebx) + 1;
                ebx += off;
                for (k = 0; k < 0x48; k++)
                    M8(ebx + k) = 0;
            found:
                M32(ebx + 0x24) = 1;
                M32(ebx + 0x28) = M32(s + 0x4);
                M32(ebx + 0x44) = M32(s + 0x30);
                M32(ebx + 0x30) = M32(s + 0xcc);
                M32(ebx + 0x40) = M32(s + 0x38);
                M32(ebx + 0x2c) = M32(s + 0x34);
                eax = M32(s + 0x28) + 4;
                for (k = 0; k < 0x24; k++)
                    M8(ebx + k) = M8(eax + k);
            }
            s += 0x100;
        } while (--left != 0);
    }

    ebx = S_PLAYERS;
    edx = M32(ebx);
    ebx += 4;
    do {
        if (M32(ebx + 0x24) == 0)
            for (k = 0; k < 0x48; k++)
                M8(ebx + k) = 0;
        ebx += 0x48;
    } while (--edx != 0);
}

/* .text+0x3b2e -- the block: kept while the aircraft is inside its bounds;
 * otherwise, and when the aircraft leaves the box about the last search,
 * FE is asked for the one about the aircraft now. */
static void load_block(void)
{
    uint32_t eax;
    uint8_t probe;
    int32_t size;

    esi = S_BLOCK;
    if (esi != 0) {
        edi = G;
        eax = M32(edi + 0x564) >> 9;
        if ((int16_t)eax > (int16_t)M16(esi) || (int16_t)eax < (int16_t)M16(esi + 2))
            goto reload;
        eax = M32(edi + 0x56c) >> 16;
        if ((int16_t)eax > (int16_t)M16(esi + 4))
            goto reload;
        if ((int16_t)eax >= (int16_t)M16(esi + 6))
            return;
    reload:
        FREE(&S_BLOCK);
        S_BLOCK = 0;
        if (S_OBJECTS != 0) {
            FREE(&S_OBJECTS);
            S_OBJECTS = 0;
        }
    } else {
        eax = M32(edi + 0x564);
        if (!((int32_t)eax > (int32_t)S_BOX_N) && !((int32_t)eax < (int32_t)S_BOX_S)) {
            eax = M32(edi + 0x56c);
            if (!((int32_t)eax > (int32_t)S_BOX_E) && (int32_t)eax >= (int32_t)S_BOX_W)
                return;
        }
    }
    eax = M32(edi + 0x564) + 0x1388;
    S_BOX_N = eax;
    S_BOX_S = eax - 0x2710;
    eax = M32(edi + 0x56c) + 0xe0000;
    S_BOX_E = eax;
    S_BOX_W = eax - 0x1c0000;
    FE_SEARCH(edi + 0x560, 0x1388, edi + 0x3e4);
    if (FE_READ(0, ADDR(&probe), 1, &size) >= 0)
        return;
    ALLOC((uint32_t)size, &S_BLOCK, 0);
    FE_READ(0, S_BLOCK, size, &size);
    ALLOC(8, &S_OBJECTS, 1);
}

/* .text+0x3b0f -- the block's declarations, from +0x20 until an opcode past
 * the table.  Whatever handler they name runs, on whatever edi is. */
static void run_declarations(void)
{
    esi = S_BLOCK;
    if (esi == 0)
        return;
    esi += 0x20;
    for (;;) {
        uint32_t ebx = M8(esi);
        if (ebx > 0x43)
            return;
        DISPATCH(ebx);
    }
}

/* .text+0x14ca -- CHAIN list 4 */
static void FS98_STDCALL dyn_load(uint32_t a, uint32_t b)
{
    (void)a; (void)b;
    edi = G;
    if (M32(edi + 0x268) == 0) {
        load_block();
        run_declarations();
    }
}

/* .text+0x14f6 -- CHAIN list 16: each live object whose switch is on and
 * whose density level is in range goes to G3D. */
static void FS98_STDCALL dyn_draw(uint32_t a, uint32_t b)
{
    uint32_t s, ecx, edx, ebx, g;
    uint16_t ax;
    (void)a; (void)b;

    s = S_OBJECTS;
    if (s == 0)
        return;
    ecx = M32(s);
    if (ecx == 0)
        return;
    s += 4;
    g = G;
    edx = M32(g + 0x118c);
    edx = (edx << 1) | M32(g + 0x1188);
    edx = (edx << 1) | M32(g + 0x1184);
    edx = (edx << 1) | M32(g + 0x1180);
    ebx = M32(g + 0x1190);
    edx <<= 12;
    ebx <<= 8;
    do {
        if (M32(s + 0x64) != 0) {
            ax = M16(s + 0x24);
            if ((uint16_t)(ax & (uint16_t)edx) != 0) {
                ax &= 0x700;
                if (!((uint16_t)ebx < ax)) {
                    M16(s + 0x2e) = M16(g + 0x11b6);
                    G3D_DRAW(s + 0xf0, 0);
                }
            }
        }
        s += 0x120;
    } while (--ecx != 0);
}

/* ---- the interpreter -------------------------------------------------------------- */

/* .text+0x168a -- one step of the object at edi.  While a handler returns
 * carry clear and S_RUN is still set, the program counter is stored and the
 * next opcode runs.  Otherwise S_RUN goes, and opcodes below 0xa -- the
 * easing moves -- still run on from where the handler left esi, without
 * storing it: that is how several eases proceed in one step.  With S_RUN
 * clear only opcodes to 0x23 run at all. */
static void step_object(void)
{
    uint32_t ebx;
    uint16_t ax;

    esi = M32(edi + 0x68);
    S_RUN = 1;
    ax = (uint16_t)(M16(G + 0x2aa) + M16(G + 0x2aa));
    M16(edi + 0x28) = (uint16_t)(M16(edi + 0x28) - ax);
    ax = (uint16_t)(ax + ax);
    M16(edi + 0x2a) = (uint16_t)(M16(edi + 0x2a) - ax);
    if (blocked())
        return;
    for (;;) {
        ebx = M8(esi);
        if (ebx > 0x43)
            return;
        if (S_RUN == 0 && ebx > 0x23)
            return;
        if (!DISPATCH(ebx) && S_RUN != 0) {
            M32(edi + 0x68) = esi;
            continue;
        }
        S_RUN = 0;
        if (ebx >= 0xa)
            return;
    }
}

/* .text+0x20db -- carry set when something is in the way: the user's
 * aircraft, then each other live object, close enough (their sizes and two
 * more), within 5 of height, and within 60 degrees of the heading. */
static int blocked(void)
{
    uint32_t ebx, ecx, edx, eax, r, dlat, dlon;

    eax = M32(edi + 0x4);
    eax = (eax & 0xffff0000u) | (uint16_t)(eax | M16(edi + 0x2) | M16(edi + 0xa));
    eax |= M32(edi + 0xc);
    if (eax == 0)
        return 0;
    if (M16(edi + 0x24) & 1)
        return 0;
    ebx = G;
    edx = M32(ebx + 0x11e0);
    edx = edx == 0 ? 0xa : M32(edx + 0xc);
    ecx = 0xffffffffu;
    ebx += 0x560;
    goto check;
next_object:
    edx = M16(ebx + 0x2c);
check:
    r = edx + M16(edi + 0x2c) + 2;
    dlat = M32(ebx + 0x4) - M32(edi + 0x4);
    if (iabs(dlat) > r)
        goto next;
    dlon = HI(smul(M32(ebx + 0xc) - M32(edi + 0xc), S_LON_TO_M));
    if (iabs(dlon) > r)
        goto next;
    if (iabs(M32(ebx + 0x14) - M32(edi + 0x14)) > 5)
        goto next;
    eax = UTIL_ATAN2((int32_t)dlon, (int32_t)dlat);
    eax = (eax << 16) - M32(edi + 0x20);
    if (iabs(eax) > 0x2aa80000u)
        goto next;
    eax = dlon * dlon + dlat * dlat;
    if (UTIL_SQRT32(eax) < r)
        return 1;
next:
    if ((int32_t)ecx < 0) {
        ebx = S_OBJECTS;
        ecx = M32(ebx);
        ebx += 4;
        goto test;
    }
    for (;;) {
        ebx += 0x120;
        if (--ecx == 0)
            return 0;
    test:
        if (M32(ebx + 0x64) != 0 && edi != ebx)
            goto next_object;
    }
}

/* ---- easing: opcodes 1-3, 5-7 ---------------------------------------------------- */

/* .text+0x170c -- ease the angle at edi+off toward [esi+1] at [esi+3] per
 * unit time.  Carry set while moving; on arrival it is set exactly, and
 * carry clears, only if S_RUN is still set. */
static int ease(uint32_t off)
{
    uint32_t eax, edx, ecx;
    int cf;

    eax = LO(smul(SX16(M16(esi + 3)), S_DT));
    eax += eax;
    edx = iabs(eax);
    ecx = iabs((SX16(M16(esi + 1)) << 16) - M32(edi + off));
    if ((int32_t)edx < (int32_t)ecx) {
        M32(edi + off) = M32(edi + off) + eax;
        cf = 1;
    } else if (S_RUN != 0) {
        M32(edi + off) = SX16(M16(esi + 1)) << 16;
        cf = 0;
    } else {
        cf = 1;
    }
    esi += 5;
    return cf;
}

static int op_ease_pitch(void)   { return ease(0x18); }     /* .text+0x16f9 */
static int op_ease_bank(void)    { return ease(0x1c); }     /* .text+0x1700 */
static int op_ease_heading(void) { return ease(0x20); }     /* .text+0x1707 */

/* .text+0x1754, +0x1763, +0x1772 */
static int op_set_pitch(void)
{
    M32(edi + 0x18) = (uint32_t)M16(esi + 1) << 16;
    esi += 3;
    return 0;
}

static int op_set_bank(void)
{
    M32(edi + 0x1c) = (uint32_t)M16(esi + 1) << 16;
    esi += 3;
    return 0;
}

static int op_set_heading(void)
{
    M32(edi + 0x20) = (uint32_t)M16(esi + 1) << 16;
    esi += 3;
    return 0;
}

/* .text+0x16f7 -- the empty slots: carry set, esi where it was.  Below 0xa
 * that is a step that never ends, as in retail. */
static int op_halt(void)
{
    return 1;
}

/* .text+0x1cfe -- flag 4 on or off */
static int op_flag4(void)
{
    uint8_t al = M8(esi + 1);
    uint16_t bx;
    if (al != 0)
        al = 4;
    bx = (uint16_t)(M16(edi + 0x24) & 0xfffb);
    bx |= (uint16_t)(al & 4);
    M16(edi + 0x24) = bx;
    esi += 2;
    return 0;
}

/* .text+0x221d -- flag 1 (hidden from blocked()) on or off */
static int op_hide(void)
{
    M16(edi + 0x24) = (uint16_t)(M16(edi + 0x24) & 0xfffe);
    if (M8(esi + 1) != 0)
        M16(edi + 0x24) = (uint16_t)(M16(edi + 0x24) | 1);
    esi += 2;
    return 0;
}

/* .text+0x1781 -- place the object: position, its bases, no eastward offset */
static int op_place(void)
{
    uint16_t ax;
    uint32_t ecx;

    M32(edi + 0x98) = 0;
    M32(edi + 0x60) = 0;
    ax = M16(esi + 1);
    M16(edi + 0x2) = ax;
    M16(edi + 0x4a) = ax;
    ecx = M32(esi + 3);
    M32(edi + 0x4) = ecx;
    M32(edi + 0x4c) = ecx;
    ax = M16(esi + 7);
    M16(edi + 0xa) = ax;
    M16(edi + 0x42) = ax;
    ecx = M32(esi + 9);
    M32(edi + 0xc) = ecx;
    M32(edi + 0x44) = ecx;
    M16(edi + 0x52) = 0;
    M32(edi + 0x54) = 0;
    ax = M16(esi + 0xd);
    M16(edi + 0x12) = ax;
    M16(edi + 0x5a) = ax;
    ecx = M32(esi + 0xf);
    M32(edi + 0x14) = ecx;
    M32(edi + 0x5c) = ecx;
    M16(edi + 0x24) = (uint16_t)(M16(edi + 0x24) & 0xfffe);
    esi += 0x13;
    return 0;
}

/* .text+0x17e8 -- the latitude the metric conversions are about, and the
 * longitude scale there: 1 m converted, and 2^48 over that.  Returns the
 * carry of `add esi,7`. */
static int op_origin(void)
{
    uint8_t in[8], out[8];
    uint32_t eax;

    *(uint16_t *)(in + 2) = 0;
    *(uint32_t *)(in + 4) = 1;
    M16(S_ORIGIN + 2) = M16(esi + 1);
    eax = M32(esi + 3);
    if (eax != M32(S_ORIGIN + 4)) {
        M32(S_ORIGIN + 4) = eax;
        UTIL_TO_LON(ADDR(in), S_ORIGIN, ADDR(out));
        S_M_TO_LON = (*(uint32_t *)(out + 4) << 16) | *(uint16_t *)(out + 2);
        S_LON_TO_M = udiv(0x10000, 0, S_M_TO_LON, NULL);
    }
    esi += 7;
    return esi < 7;
}

/* .text+0x186a -- one of the object's parameter words; the index is not
 * checked.  Returns the carry of `add esi,4`. */
static int op_param(void)
{
    uint32_t ebx = M8(esi + 1);
    M16(edi + 0x30 + ebx * 2) = M16(esi + 2);
    esi += 4;
    return esi < 4;
}

/* ---- timing ---------------------------------------------------------------------- */

/* .text+0x1df2 -- leave a path: the bases become the position, and the
 * eastward metres are recomputed from the longitude.  It uses ecx as
 * scratch and leaves the altitude's whole part in it, which ramped() picks
 * up (see there); *ecx gets it when the path is left. */
static int path_end(uint32_t *ecx)
{
    uint8_t in[8];
    uint16_t ax;
    uint32_t edx;

    if (M32(edi + 0x98) == 0)
        return 0;
    M32(edi + 0x98) = 0;
    M32(edi + 0x60) = 0;
    M16(edi + 0x4a) = M16(edi + 0x2);
    M32(edi + 0x4c) = M32(edi + 0x4);
    M16(edi + 0x5a) = M16(edi + 0x12);
    *ecx = M32(edi + 0x14);
    M32(edi + 0x5c) = *ecx;
    ax = M16(edi + 0xa);
    edx = M32(edi + 0xc) - M32(edi + 0x44) - (ax < M16(edi + 0x42));
    ax = (uint16_t)(ax - M16(edi + 0x42));
    *(uint16_t *)(in + 2) = ax;
    *(uint32_t *)(in + 4) = edx;
    UTIL_TO_METRES(ADDR(in), S_ORIGIN, edi + 0x50);
    return 0;
}

/* .text+0x1894 -- the countdown at edi+0x60, armed from [esi+1] (in units of
 * 0x8000) on a move's first step.  Carry set while time is left.  S_LEFT is
 * what is left, never less than one step.  ecx as path_end leaves it. */
static int timer(uint32_t *ecx)
{
    uint32_t eax;
    int cf;

    path_end(ecx);
    eax = M32(edi + 0x60);
    if (eax == 0) {
        S_FIRST = 1;
        eax = M16(esi + 1);
        if (eax == 0)
            goto zero;
        eax <<= 15;
        cf = 1;
    } else {
        S_FIRST = 0;
        eax -= S_DT;
        if ((int32_t)eax < 0)
            goto zero;
        eax |= 1;
        cf = 1;
    }
    goto store;
zero:
    eax = 0;
    cf = 0;
store:
    M32(edi + 0x60) = eax;
    if (!(eax > S_DT))
        eax = S_DT;
    S_LEFT = eax;
    return cf;
}

/* .text+0x187b -- wait [esi+1] */
static int op_wait(void)
{
    uint32_t ecx;
    int cf = 1;
    if (S_RUN != 0)
        cf = timer(&ecx);
    esi += 3;
    return cf;
}

/* ---- moves: opcodes 0x12-0x1f ---------------------------------------------------- */

/* Each moves one of three 48-bit bases -- +0x4a/+0x4c latitude (index 0),
 * +0x52/+0x54 eastward metres (8), +0x5a/+0x5c altitude (0x10) -- toward a
 * target at +0x7e/+0x80 (plus the index), by the time left. */

/* .text+0x196e -- the base moves by (target - base) / S_LEFT * S_DT */
static void approach(uint16_t ax, uint32_t edx, uint32_t ebx)
{
    uint32_t eax, borrow, sum;
    uint64_t p;

    borrow = ax < M16(edi + ebx + 0x4a);
    ax = (uint16_t)(ax - M16(edi + ebx + 0x4a));
    edx = edx - M32(edi + ebx + 0x4c) - borrow;
    eax = (uint32_t)ax | (edx << 16);
    edx = (uint32_t)((int32_t)edx >> 16);
    eax = sdiv(edx, eax, S_LEFT);
    p = smul(eax, S_DT);
    edx = (HI(p) << 16) | (LO(p) >> 16);
    sum = (uint32_t)M16(edi + ebx + 0x4a) + (uint16_t)LO(p);
    M16(edi + ebx + 0x4a) = (uint16_t)sum;
    M32(edi + ebx + 0x4c) = M32(edi + ebx + 0x4c) + edx + (sum >> 16);
}

/* .text+0x1a82 -- arrive: the base is the target.  Carry clear. */
static int arrive(uint32_t ebx)
{
    uint16_t ax = M16(edi + ebx + 0x7e);
    uint32_t edx = M32(edi + ebx + 0x80);
    M16(edi + ebx + 0x4a) = ax;
    M32(edi + ebx + 0x4c) = edx;
    return 0;
}

/* .text+0x1a5a -- on the first step the target's whole part is set to the
 * base's plus ax; then approach.  Carry set. */
static int move(uint16_t ax, uint32_t ebx)
{
    if (S_FIRST != 0)
        M32(edi + ebx + 0x80) = SX16(ax) + M32(edi + ebx + 0x4c);
    approach(M16(edi + ebx + 0x7e), M32(edi + ebx + 0x80), ebx);
    return 1;
}

/* .text+0x1a4f -- timed: move while time is left, else arrive */
static int timed(uint16_t ax, uint32_t ebx)
{
    uint32_t ecx;
    if (!timer(&ecx))       /* push ax / pop ax round it: ax survives */
        return arrive(ebx);
    return move(ax, ebx);
}

/* .text+0x1abd -- timed, the time left shaped by a rate that falls each
 * step from [esi+3], first step by ([esi+3] - 0x1000) / [esi+1].  The
 * offset is kept in ecx across the timer -- but when the object has just
 * come off a path the timer's path_end leaves the altitude in ecx, so the
 * first step's target is the base plus the altitude's low word, not the
 * offset.  Retail's; kept. */
static int ramped(uint16_t ax, uint32_t ebx)
{
    uint32_t ecx = ax, eax;
    uint32_t prod;
    uint16_t dx, lo;
    uint64_t p;

    if (!timer(&ecx))
        return arrive(ebx);
    if (S_FIRST != 0) {
        lo = M16(esi + 3);
        M16(edi + 0x94) = lo;
        lo = (uint16_t)(lo - 0x1000);
        M16(edi + 0x96) = sdiv16((uint16_t)((int16_t)lo >> 15), lo, M16(esi + 1));
    }
    prod = (uint32_t)((int32_t)(int16_t)M16(edi + 0x96) * (int16_t)(uint16_t)S_DT);
    dx = (uint16_t)((uint16_t)(prod >> 16) << 1 | (uint16_t)prod >> 15);
    M16(edi + 0x94) = (uint16_t)(M16(edi + 0x94) - dx);
    p = smul(SX16(M16(edi + 0x94)), S_LEFT);
    eax = (LO(p) >> 12) | (HI(p) << 20);
    if (eax < S_DT)
        eax = S_DT;
    S_LEFT = eax;
    return move((uint16_t)ecx, ebx);
}

/* .text+0x1a2b, +0x1a32, +0x1a39 -- one axis */
static int move1(uint32_t ebx)
{
    int cf = timed(M16(esi + 3), ebx);
    esi += 5;
    return cf;
}

static int op_move_e(void)   { return move1(8); }
static int op_move_alt(void) { return move1(0x10); }
static int op_move_n(void)   { return move1(0); }

/* .text+0x199b, +0x19a7, +0x19b3 -- two axes on one timer */
static int move2(uint32_t ebx, uint32_t second)
{
    int cf = timed(M16(esi + 3), ebx);
    uint16_t ax = M16(esi + 5);
    cf = cf ? move(ax, second) : arrive(second);
    esi += 7;
    return cf;
}

static int op_move_e_n(void)   { return move2(8, 0); }
static int op_move_e_alt(void) { return move2(8, 0x10); }
static int op_move_alt_n(void) { return move2(0x10, 0); }

/* .text+0x19e2 -- all three */
static int op_move_3(void)
{
    int cf = timed(M16(esi + 3), 8);
    if (cf) {
        move(M16(esi + 5), 0x10);
        cf = move(M16(esi + 7), 0);
    } else {
        arrive(0x10);
        cf = arrive(0);
    }
    esi += 9;
    return cf;
}

/* .text+0x1a99, +0x1aa0, +0x1aa7 */
static int ramp1(uint32_t ebx)
{
    int cf = ramped(M16(esi + 5), ebx);
    esi += 7;
    return cf;
}

static int op_ramp_e(void)   { return ramp1(8); }
static int op_ramp_alt(void) { return ramp1(0x10); }
static int op_ramp_n(void)   { return ramp1(0); }

/* .text+0x1b2f, +0x1b3b, +0x1b47 */
static int ramp2(uint32_t ebx, uint32_t second)
{
    int cf = ramped(M16(esi + 5), ebx);
    uint16_t ax = M16(esi + 7);
    cf = cf ? move(ax, second) : arrive(second);
    esi += 9;
    return cf;
}

static int op_ramp_e_n(void)   { return ramp2(8, 0); }
static int op_ramp_e_alt(void) { return ramp2(8, 0x10); }
static int op_ramp_alt_n(void) { return ramp2(0x10, 0); }

/* .text+0x1b76 */
static int op_ramp_3(void)
{
    int cf = ramped(M16(esi + 5), 8);
    if (cf) {
        move(M16(esi + 7), 0x10);
        cf = move(M16(esi + 9), 0);
    } else {
        arrive(0x10);
        cf = arrive(0);
    }
    esi += 0xb;
    return cf;
}

/* ---- turning: opcode 0x20 --------------------------------------------------------- */

/* .text+0x1c95 -- the offset of a turn's centre from the object: [esi+5]
 * along the perpendicular of the heading, to the side the turn goes (the
 * sign of edi+0x94).  eax the latitude part, ebx the eastward. */
static void turn_offset(uint32_t *peax, uint32_t *pebx)
{
    int16_t s, c;
    uint32_t ebx, eax, r;

    ebx = (0x40000000u - M32(edi + 0x20)) >> 16;
    UTIL_SINCOS((int16_t)ebx, &s, &c);
    r = SX16(M16(esi + 5));
    eax = LO(smul((uint32_t)(int32_t)s, r));
    eax += eax;
    ebx = eax;
    eax = LO(smul((uint32_t)(int32_t)c, r));
    eax += eax;
    if (M16(edi + 0x94) & 0x8000)
        eax = 0u - eax;
    else
        ebx = 0u - ebx;
    *peax = eax;
    *pebx = ebx;
}

/* .text+0x1c5e -- the bases are the centre plus the offset for the heading
 * now */
static void turn_apply(void)
{
    uint32_t eax, ebx, edx, sum;

    turn_offset(&eax, &ebx);
    edx = (uint32_t)((int32_t)eax >> 16);
    sum = (uint32_t)(uint16_t)eax + M16(edi + 0x7e);
    M16(edi + 0x4a) = (uint16_t)sum;
    M32(edi + 0x4c) = edx + M32(edi + 0x80) + (sum >> 16);
    edx = (uint32_t)((int32_t)ebx >> 16);
    sum = (uint32_t)(uint16_t)ebx + M16(edi + 0x86);
    M16(edi + 0x52) = (uint16_t)sum;
    M32(edi + 0x54) = edx + M32(edi + 0x88) + (sum >> 16);
}

/* .text+0x1bbf -- turn to [esi+3] about a circle of radius [esi+5], timed.
 * The step is `add si,7`: only the low 16 bits of the program counter. */
static int op_turn(void)
{
    uint32_t eax, ebx, edx;
    uint16_t cx;
    int cf;

    uint32_t ecx;

    if (timer(&ecx)) {
        if (S_FIRST != 0) {
            eax = ((uint32_t)M16(esi + 3) << 16) - M32(edi + 0x20);
            M16(edi + 0x94) = (uint16_t)(eax >> 16);
            turn_offset(&eax, &ebx);
            edx = (uint32_t)((int32_t)eax >> 16);
            cx = (uint16_t)(M16(edi + 0x4a) - (uint16_t)eax);
            M16(edi + 0x7e) = cx;
            M32(edi + 0x80) = M32(edi + 0x4c) - edx - (M16(edi + 0x4a) < (uint16_t)eax);
            edx = (uint32_t)((int32_t)ebx >> 16);
            cx = (uint16_t)(M16(edi + 0x52) - (uint16_t)ebx);
            M16(edi + 0x86) = cx;
            M32(edi + 0x88) = M32(edi + 0x54) - edx - (M16(edi + 0x52) < (uint16_t)ebx);
        }
        eax = ((uint32_t)M16(esi + 3) << 16) - M32(edi + 0x20);
        eax = sdiv((uint32_t)((int32_t)eax >> 31), eax, S_LEFT);
        eax = LO(smul(eax, S_DT));
        M32(edi + 0x20) = M32(edi + 0x20) + eax;
        turn_apply();
        cf = 1;
    } else {
        M32(edi + 0x20) = (uint32_t)M16(esi + 3) << 16;
        turn_apply();
        cf = 0;
    }
    esi = (esi & 0xffff0000u) | (uint16_t)(esi + 7);
    return cf;
}

/* ---- control ------------------------------------------------------------------- */

/* .text+0x18f2 */
static int op_jump(void)
{
    esi += SX16(M16(esi + 1));
    return 0;
}

/* .text+0x18fa -- one level: the return address goes to edi+0x6c */
static int op_call(void)
{
    uint32_t eax = SX16(M16(esi + 1)) + esi;
    esi += 3;
    M32(edi + 0x6c) = esi;
    esi = eax;
    return 0;
}

/* .text+0x190a -- from generated code, free it and go back to edi+0x74;
 * otherwise to edi+0x6c */
static int op_return(void)
{
    if (M32(edi + 0x70) != 0) {
        FREE(edi + 0x70);
        M32(edi + 0x70) = 0;
        esi = M32(edi + 0x74);
    } else {
        esi = M32(edi + 0x6c);
    }
    return 0;
}

/* .text+0x18ed -- carry set, past it: a step stops here without storing the
 * counter, so the next step stops here again -- until op_release moves the
 * stored counter on. */
static int op_hold(void)
{
    esi += 1;
    return 1;
}

/* .text+0x1d1f */
static int op_nop(void)
{
    esi += 1;
    return 0;
}

/* .text+0x194a -- the object a declaration ([esi+1] from here) made: if it
 * is holding at op_hold, move it past */
static int op_release(void)
{
    uint32_t ebx, eax;

    ebx = SX16(M16(esi + 1)) + esi;
    ebx = M16(ebx + 5);
    if (ebx != 0) {
        ebx += S_OBJECTS;
        eax = M32(ebx + 0x68);
        if (M8(eax) == 0x29)
            M32(ebx + 0x68) = M32(ebx + 0x68) + 1;
    }
    esi += 3;
    return 0;
}

/* .text+0x1d4a -- the compare the conditional jumps share: the setting
 * [esi+3] names against [esi+5], as 16-bit values.  Returns the target;
 * esi moves past.  dx/ax are the operands. */
static uint32_t condition(uint16_t *pdx, uint16_t *pax)
{
    uint32_t ecx, ebx, edx, g;

    ecx = SX16(M16(esi + 1)) + esi;
    ebx = M16(esi + 3);
    g = G;
    switch (ebx) {
    case 0: edx = M32(g + 0x1180); break;
    case 1: edx = M32(g + 0x1184); break;
    case 2: edx = M32(g + 0x1188); break;
    case 3: edx = M32(g + 0x118c); break;
    case 4: edx = M32(g + 0x1190); break;
    case 5: edx = (((uint32_t)M16(g + 0xef2) + 0x5b) & 0xffff) * 0x168 >> 16; break;
    case 6: edx = M16(g + 0xef0); break;
    case 7: edx = (uint32_t)M8(g + 0x238) * 0x3c + M8(g + 0x239); break;
    default: edx = 0; break;
    }
    *pax = M16(esi + 5);
    esi += 7;
    *pdx = (uint16_t)edx;
    return ecx;
}

/* .text+0x1d24, +0x1d2d, +0x1d36, +0x1d3f */
static int op_jne(void)
{
    uint16_t dx, ax;
    uint32_t to = condition(&dx, &ax);
    if (dx != ax) esi = to;
    return 0;
}

static int op_je(void)
{
    uint16_t dx, ax;
    uint32_t to = condition(&dx, &ax);
    if (dx == ax) esi = to;
    return 0;
}

static int op_jle(void)
{
    uint16_t dx, ax;
    uint32_t to = condition(&dx, &ax);
    if ((int16_t)dx <= (int16_t)ax) esi = to;
    return 0;
}

static int op_jge(void)
{
    uint16_t dx, ax;
    uint32_t to = condition(&dx, &ax);
    if (!((int16_t)dx < (int16_t)ax)) esi = to;
    return 0;
}

/* ---- declarations ------------------------------------------------------------- */

/* .text+0x3efe -- the draw record G3D takes, from +0xf0: fixed words, a
 * pointer back to the object, and the 16-byte library object id -- inline
 * for 0x35, from the table at 0x206a6170 by [esi+1] for 0x32 (the first
 * entry when it is not there). */
static void draw_record(void)
{
    uint32_t ebx;
    uint16_t ax;

    M16(edi + 0xf0) = 0x76;
    M16(edi + 0xf2) = 0x89;
    M32(edi + 0xf4) = edi;
    M16(edi + 0xf8) = 0x3a;
    M16(edi + 0xfa) = 0x24;
    M16(edi + 0x104) = 0x63;
    M16(edi + 0x116) = 0x89;
    M16(edi + 0x11c) = 0x22;
    if (M8(esi) != 0x32) {
        M32(edi + 0x106) = M32(esi + 0x8);
        M32(edi + 0x10a) = M32(esi + 0xc);
        M32(edi + 0x10e) = M32(esi + 0x10);
        M32(edi + 0x112) = M32(esi + 0x14);
        return;
    }
    ebx = S_LIB;
    ax = M16(esi + 1);
    for (;;) {
        if (ax == M16(ebx))
            break;
        ebx += 0x14;
        if (!(ebx < S_LIB_END)) {
            ebx = S_LIB;
            break;
        }
    }
    M32(edi + 0x106) = M32(ebx + 0x4);
    M32(edi + 0x10a) = M32(ebx + 0x8);
    M32(edi + 0x10e) = M32(ebx + 0xc);
    M32(edi + 0x112) = M32(ebx + 0x10);
}

/* .text+0x3fb1 -- the objects moved: each draw record's pointer back.
 * Leaves edi past the last. */
static void draw_records_fix(void)
{
    uint32_t ecx;

    edi = S_OBJECTS;
    if (edi == 0)
        return;
    ecx = M32(edi);
    edi += 4;
    if (ecx == 0)
        return;
    do {
        M32(edi + 0xf4) = edi;
        edi += 0x120;
    } while (--ecx != 0);
}

/* .text+0x3d58 -- 0x32 and 0x35, a declaration: [esi+7]'s low nibble is a
 * density level, its high nibble a mask of the four scenery switches.  On,
 * and without an object yet, it takes the first free slot (growing the
 * array, to 0xe3) and writes the slot's offset into [esi+5]; off, with one,
 * it frees the object's generated code and clears it.  Programs start at
 * [esi+3] (0x32, 16-bit) or [esi+1] (0x35, 32-bit) from here. */
static int op_object(void)
{
    uint32_t ebx, ecx, eax, k;
    uint16_t cx;
    uint8_t al;

    edi = G;
    al = M8(esi + 7);
    ebx = M32(edi + 0x1190);
    cx = (uint16_t)(al & 0xf);
    if (cx > (uint16_t)ebx)
        goto off;
    ebx = M32(edi + 0x118c);
    ebx = (ebx << 1) | M32(edi + 0x1188);
    ebx = (ebx << 1) | M32(edi + 0x1184);
    ebx = (ebx << 1) | M32(edi + 0x1180);
    ebx <<= 4;
    if (((uint16_t)ebx & (uint16_t)al) != 0)
        goto on;
off:
    eax = M16(esi + 5);
    if (eax == 0)
        goto done;
    M16(esi + 5) = 0;
    edi = S_OBJECTS;
    if (edi == 0)
        goto done;
    edi += eax;
    if (M32(edi + 0x78) != 4)
        S_LOCK = 0;
    if (M32(edi + 0x70) != 0)
        FREE(edi + 0x70);
    for (k = 0; k < 0x120; k++)
        M8(edi++) = 0;
    goto done;
on:
    if (M16(esi + 5) != 0)
        goto done;
    ebx = S_OBJECTS;
    ecx = M32(ebx);
    if (ecx != 0) {
        edi = ebx + 4;
        do {
            if (M32(edi + 0x64) == 0)
                goto use;
            edi += 0x120;
        } while (--ecx != 0);
    }
    eax = M32(ebx);
    if (eax >= 0xe3)
        goto done;
    eax = (eax + 1) * 0x120 + 8;
    REALLOC(eax, &S_OBJECTS);
    ebx = S_OBJECTS;
    M32(ebx) = M32(ebx) + 1;
    edi = (M32(ebx) - 1) * 0x120 + ebx + 4;
    for (k = 0; k < 0x120; k++)
        M8(edi + k) = 0;
use:
    M16(esi + 5) = (uint16_t)(edi - ebx);
    M32(edi + 0x64) = esi;
    eax = M32(esi + 1);
    if (M8(esi) == 0x32)
        eax = M16(esi + 3);
    M32(edi + 0x68) = eax + esi;
    M16(edi + 0x24) = (uint16_t)((uint16_t)M8(esi + 7) << 8 | 4);
    M16(edi + 0x2c) = 6;
    for (k = 0x30; k < 0x40; k += 2)
        M16(edi + k) = 0xf004;
    draw_record();
    draw_records_fix();
done:
    esi += M8(esi) == 0x32 ? 8 : 0x18;
    return 0;
}

/* ---- paths: opcodes 0x38-0x3b ------------------------------------------------------ */

/* A path is a run of 0x1f-byte points -- 0x39 and 0x3a, or 0x3b with a
 * repeat count before the point -- each a 48-bit latitude, longitude and
 * altitude at +1/+7/+0xd and pitch, bank and heading at +0x13/+0x17/+0x1b.
 * The object moves between the point before (snapshot at edi+0x9c: its time,
 * then a copy of edi+0..+0x23) and the next (edi+0xc8 and edi+0xcc) by the
 * clock. */

/* .text+0x2021 */
static void load_target(void)
{
    M32(edi + 0xc4) = esi;
    M16(edi + 0xce) = M16(esi + 1);
    M32(edi + 0xd0) = M32(esi + 3);
    M16(edi + 0xd6) = M16(esi + 7);
    M32(edi + 0xd8) = M32(esi + 9);
    M16(edi + 0xde) = M16(esi + 0xd);
    M32(edi + 0xe0) = M32(esi + 0xf);
    M32(edi + 0xe4) = M32(esi + 0x13);
    M32(edi + 0xe8) = M32(esi + 0x17);
    M32(edi + 0xec) = M32(esi + 0x1b);
}

/* .text+0x207f -- one 48-bit coordinate, ecx/0x8000 of the way */
static void lerp48(uint32_t ebx, uint32_t ecx)
{
    uint16_t ax;
    uint32_t edx, sum;

    ax = M16(edi + ebx + 0xce);
    edx = M32(edi + ebx + 0xd0) - M32(edi + ebx + 0xa4) - (ax < M16(edi + ebx + 0xa2));
    ax = (uint16_t)(ax - M16(edi + ebx + 0xa2));
    dyn_scale(&ax, &edx, ecx);
    sum = (uint32_t)ax + M16(edi + ebx + 0xa2);
    M16(edi + ebx + 0x2) = (uint16_t)sum;
    M32(edi + ebx + 0x4) = edx + M32(edi + ebx + 0xa4) + (sum >> 16);
}

/* .text+0x20bb -- one angle */
static void lerp32(uint32_t ebx, uint32_t ecx)
{
    uint64_t p = smul(M32(edi + ebx + 0xe4) - M32(edi + ebx + 0xb8), ecx);
    M32(edi + ebx + 0x18) = ((LO(p) >> 15) | (HI(p) << 17)) + M32(edi + ebx + 0xb8);
}

/* .text+0x1ed5 -- the point at esi, `duration` from the last.  Carry set
 * while on the way. */
static int path_point(uint32_t duration)
{
    uint32_t eax, ebx, ecx, k;
    uint64_t n;
    uint8_t op;

    if (M32(edi + 0x98) == 0) {
        M32(edi + 0x98) = 1;
        eax = S_CLOCK;
        M32(edi + 0x60) = eax;
        M32(edi + 0xc8) = eax;
        load_target();
        goto next;
    }
    eax = S_CLOCK;
    if (eax < M32(edi + 0xc8))
        goto between;
    if (esi == M32(edi + 0xc4))
        goto next;
    for (k = 0; k < 0x28; k++)
        M8(edi + 0x9c + k) = M8(edi + 0xc8 + k);
    load_target();
    M32(edi + 0x60) = M32(edi + 0x60) + duration;
    M32(edi + 0xc8) = M32(edi + 0x60);
between:
    ecx = 0x8000;
    eax = S_CLOCK - M32(edi + 0x9c);
    ebx = M32(edi + 0xc8) - M32(edi + 0x9c);
    n = (uint64_t)eax << 15;
    eax = udiv(HI(n), LO(n), ebx, NULL);
    if (!(eax > ecx))
        ecx = eax;
    lerp48(0, ecx);
    lerp48(8, ecx);
    lerp48(0x10, ecx);
    lerp32(0, ecx);
    lerp32(4, ecx);
    lerp32(8, ecx);
    return 1;
next:
    esi += 0x1f;
    op = M8(esi);
    if (op >= 0x39 && op <= 0x3b)
        return 0;
    /* the path ends: the object is at the last point */
    M16(edi + 0x2) = M16(edi + 0xce);
    M32(edi + 0x4) = M32(edi + 0xd0);
    M16(edi + 0xa) = M16(edi + 0xd6);
    M32(edi + 0xc) = M32(edi + 0xd8);
    M16(edi + 0x12) = M16(edi + 0xde);
    M32(edi + 0x14) = M32(edi + 0xe0);
    M32(edi + 0x18) = M32(edi + 0xe4);
    M32(edi + 0x1c) = M32(edi + 0xe8);
    M32(edi + 0x20) = M32(edi + 0xec);
    return 0;
}

static int op_path(void)      { return path_point(0x8000); }   /* .text+0x1eaa */
static int op_path_slow(void) { return path_point(0x28000); }  /* .text+0x1eb5 */

/* .text+0x1ec0 -- a point [esi+1] times 0x8000 on */
static int op_path_n(void)
{
    uint32_t ecx = (uint32_t)M8(esi + 1) * 0x8000;
    esi += 1;
    return path_point(ecx);
}

/* .text+0x1e74 -- start a path from where the object is */
static int op_path_mark(void)
{
    uint32_t eax, k;

    M32(edi + 0x98) = 1;
    eax = S_CLOCK;
    M32(edi + 0x60) = eax;
    M32(edi + 0xc8) = eax;
    M32(edi + 0xc4) = esi;
    for (k = 0; k < 0x24; k++)
        M8(edi + 0xcc + k) = M8(edi + k);
    esi += 1;
    return 0;
}

/* ---- generated paths ------------------------------------------------------------- */

/* .text+0x3732 -- the generator's pen starts where the object is */
static void pen_from_object(void)
{
    uint32_t k;
    for (k = 0; k < 0x24; k++)
        M8(edi + 0xa0 + k) = M8(edi + k);
}

/* .text+0x37ea -- one generated point.  The pen (edi+0xa0 on) eases its
 * pitch toward S_PITCH_TO and its bank toward S_BANK_TO by at most 0x16c0000;
 * the bank turns the heading, and level at last the heading becomes
 * S_HDG_TO.  The speed eases a unit toward S_SPEED_TO.  The pen moves
 * S_REPEAT times speed along the heading, climbing by the pitch, and the
 * point is written at esi: forward, or backward when S_REVERSE is set (and
 * then on the reverse heading). */
static void emit_step(void)
{
    int16_t s, c;
    uint32_t eax, edx, ebx, sum, sign;
    uint64_t p;

    eax = M32(edi + 0xb8) - S_PITCH_TO;
    if (eax != 0) {
        sign = (int32_t)eax < 0 ? 0xffffffffu : 0;
        eax = (eax ^ sign) - sign;
        if (!((int32_t)eax < 0x16c0000))
            eax = 0x16c0000;
        eax = (eax ^ sign) - sign;
        M32(edi + 0xb8) = M32(edi + 0xb8) - eax;
    }
    eax = M32(edi + 0xbc) - S_BANK_TO;
    if (eax != 0) {
        sign = (int32_t)eax < 0 ? 0xffffffffu : 0;
        eax = (eax ^ sign) - sign;
        if ((int32_t)eax > 0x16c0000) {
            eax = (0x16c0000u ^ sign) - sign;
            M32(edi + 0xbc) = M32(edi + 0xbc) - eax;
        } else {
            eax = S_BANK_TO;
            M32(edi + 0xbc) = eax;
            if (eax == 0)
                M32(edi + 0xc0) = S_HDG_TO;
        }
    }
    eax = M32(edi + 0xbc);
    if (eax != 0)
        M32(edi + 0xc0) = M32(edi + 0xc0) - (uint32_t)((int32_t)eax >> 3);
    eax = S_SPEED - S_SPEED_TO;
    if (eax != 0)
        S_SPEED = S_SPEED - (((int32_t)eax < 0 ? 0xffffffffu : 0) | 1);

    if (S_REPEAT == 1) {
        M8(esi) = 0x39;
    } else {
        if (S_REVERSE != 0)
            esi -= 1;
        M8(esi) = 0x3b;
        M8(esi + 1) = (uint8_t)S_REPEAT;
        esi += 1;
    }
    ebx = M32(edi + 0xc0);
    if (S_REVERSE != 0)
        ebx -= 0x80000000u;
    ebx = (uint32_t)((int32_t)ebx >> 16);
    UTIL_SINCOS((int16_t)ebx, &s, &c);
    eax = (uint32_t)(int32_t)c * 2;
    eax = LO(smul(eax, S_REPEAT));
    p = smul(eax, S_SPEED);
    edx = (HI(p) << 16) | (LO(p) >> 16);
    sum = (uint32_t)(uint16_t)LO(p) + M16(edi + 0xa2);
    M16(edi + 0xa2) = (uint16_t)sum;
    M16(esi + 1) = (uint16_t)sum;
    edx = edx + M32(edi + 0xa4) + (sum >> 16);
    M32(edi + 0xa4) = edx;
    M32(esi + 3) = edx;
    eax = (uint32_t)(int32_t)s * 2;
    eax = LO(smul(eax, S_REPEAT));
    eax = LO(smul(eax, S_SPEED));
    p = smul(eax, S_M_TO_LON);
    eax = (uint32_t)((int32_t)LO(p) >> 16);
    edx = HI(p);
    sum = (uint32_t)(uint16_t)eax + M16(edi + 0xaa);
    M16(edi + 0xaa) = (uint16_t)sum;
    M16(esi + 7) = (uint16_t)sum;
    edx = edx + M32(edi + 0xac) + (sum >> 16);
    M32(edi + 0xac) = edx;
    M32(esi + 9) = edx;
    ebx = M32(edi + 0xb8);
    if (ebx != 0) {
        if (S_REVERSE != 0)
            ebx = 0u - ebx;
        ebx = (uint32_t)((int32_t)ebx >> 16);
        UTIL_SINCOS((int16_t)ebx, &s, &c);
        eax = (uint32_t)(int32_t)s * 2;
        eax = LO(smul(eax, S_REPEAT));
        eax = 0u - eax;
        p = smul(eax, S_SPEED);
        edx = (HI(p) << 16) | (LO(p) >> 16);
        sum = (uint32_t)(uint16_t)LO(p) + M16(edi + 0xb2);
        M16(edi + 0xb2) = (uint16_t)sum;
        M16(esi + 0xd) = (uint16_t)sum;
        edx = edx + M32(edi + 0xb4) + (sum >> 16);
        M32(edi + 0xb4) = edx;
        M32(esi + 0xf) = edx;
    } else {
        M16(esi + 0xd) = M16(edi + 0xb2);
        M32(esi + 0xf) = M32(edi + 0xb4);
    }
    M32(esi + 0x13) = M32(edi + 0xb8);
    edx = M32(edi + 0xbc);
    M32(esi + 0x17) = edx;
    M32(esi + 0x1b) = M32(edi + 0xc0);
    M32(esi + 0x17) = edx;
    M32(esi + 0x1b) = M32(edi + 0xc0);
    if (S_REVERSE != 0) {
        if (S_REPEAT != 1)
            esi -= 1;
        esi -= 0x1f;
    } else {
        esi += 0x1f;
    }
    S_REPEAT = 1;
}

/* .text+0x3a50 -- one point on an arc about rec (+2/+4 latitude, +0xa/+0xc
 * longitude), radius `dist` at `angle`; altitude from the pen, level, with
 * `heading`.  `count` above 1 makes it 0x3b with that repeat. */
static void emit_arc(uint32_t rec, uint32_t angle, uint32_t dist, uint32_t heading,
                     uint32_t count)
{
    int16_t s, c;
    uint32_t eax, edx, sum;
    uint64_t p;

    if (count == 1) {
        M8(esi) = 0x39;
    } else {
        M8(esi) = 0x3b;
        M8(esi + 1) = (uint8_t)count;
        esi += 1;
    }
    UTIL_SINCOS((int16_t)(angle >> 16), &s, &c);
    eax = (uint32_t)(int32_t)s * 2;
    eax = LO(smul(eax, dist));
    p = smul(eax, S_M_TO_LON);
    eax = LO(p) >> 16;
    edx = HI(p);
    sum = (uint32_t)(uint16_t)eax + M16(rec + 0xa);
    M16(esi + 7) = (uint16_t)sum;
    M32(esi + 9) = edx + M32(rec + 0xc) + (sum >> 16);
    eax = (uint32_t)(int32_t)c * 2;
    p = smul(eax, dist);
    edx = (HI(p) << 16) | (LO(p) >> 16);
    sum = (uint32_t)(uint16_t)LO(p) + M16(rec + 0x2);
    M16(esi + 1) = (uint16_t)sum;
    M32(esi + 3) = edx + M32(rec + 0x4) + (sum >> 16);
    M16(esi + 0xd) = M16(edi + 0xb2);
    M32(esi + 0xf) = M32(edi + 0xb4);
    M32(esi + 0x13) = 0;
    M32(esi + 0x17) = 0;
    M32(esi + 0x1b) = heading;
    esi += 0x1f;
}

/* The rounding the generators share: n = round(a / unit), at least 1.
 * `add edx,edx` / `cmp edx,unit` / `cmc` / `adc eax,0`, then 1 for 0. */
static uint32_t arc_steps(uint32_t a, uint32_t unit)
{
    uint32_t r, q = udiv(0, a, unit, &r);
    r += r;
    q += !(r < unit);
    if (q == 0)
        q = 1;
    return q;
}

/* The forward byte copy the generators close with: rep movsb, dst below src */
static void copy_down(uint32_t dst, uint32_t src, uint32_t n)
{
    while (n--)
        M8(dst++) = M8(src++);
}

/* The steering loop the generators share: aim the pen at a point (dlat at
 * *dlat, the longitude difference through S_LON_TO_M at *dlon), offset by
 * the angle whose sine is speed/distance, and turn up to 0x0ccc0000 a step
 * at a time, emitting a point at each half.  Returns the last turn in ecx.
 * The two versions differ in what they subtract; see the callers. */

/* .text+0x22ff -- 0x3e, drive to an aircraft and back: generates a program
 * (into a block at edi+0x70, run at once, which returns to after this op)
 * that turns toward the aircraft linked at edi+0x78 -- or, when it is gone,
 * too high or 100 m off, to a point 40 m ahead -- circles it at its speed
 * radius, stops for 0x3f (refuel, with [esi+1]), circles back, and joins a
 * reversed copy of the way out. */
static int op_to_aircraft(void)
{
    uint8_t fr[0x84];
#define F32(o) M32(ADDR(fr) + 0x84 - (o))
#define F16(o) M16(ADDR(fr) + 0x84 - (o))
#define FA(o)  (ADDR(fr) + 0x84 - (o))
    int16_t s, c;
    uint32_t eax, ebx, ecx, edx, sum, n;
    uint64_t p;

    F32(0x3c) = M32(esi + 1);
    esi += 5;
    M32(edi + 0x74) = esi;
    F32(0x38) = 0xe88;
    ALLOC(F32(0x38), edi + 0x70, 0);
    ebx = M32(edi + 0x78) + S_PLAYERS;
    F32(0x20) = ebx;
    if (M32(ebx + 0x24) != 0 && !(M32(ebx + 0x2c) > 0x7fff)) {
        eax = M32(ebx + 0x4) - M32(edi + 0x4);
        F32(0x8) = eax;
        F32(0xc) = eax * eax;
        edx = HI(smul(M32(ebx + 0xc) - M32(edi + 0xc), S_LON_TO_M));
        F32(0x4) = edx;
        eax = edx * edx + F32(0xc);
        if (eax < 0x2710)
            goto have_target;
    }
    if (M32(edi + 0x78) == 4) {
        S_LOCK = 0;
        ebx = F32(0x20);
        M32(ebx + 0x38) = 0;
    }
    /* a point 40 m ahead, as a record on the stack */
    ebx = M32(edi + 0x20);
    F32(0x64) = ebx;
    ebx >>= 16;
    UTIL_SINCOS((int16_t)ebx, &s, &c);
    F16(0x16) = (uint16_t)s;
    F16(0x18) = (uint16_t)c;
    eax = (uint32_t)(int32_t)(int16_t)F16(0x16) * 0x14;
    eax <<= 1;
    p = smul(eax, S_M_TO_LON);
    eax = LO(p) >> 16;
    sum = (uint32_t)(uint16_t)eax + M16(edi + 0xa);
    F16(0x7a) = (uint16_t)sum;
    F32(0x78) = HI(p) + M32(edi + 0xc) + (sum >> 16);
    eax = (uint32_t)(int32_t)(int16_t)F16(0x18) * 0x14;
    eax <<= 1;
    edx = (uint32_t)((int32_t)eax >> 16);
    sum = (uint32_t)(uint16_t)eax + M16(edi + 0x2);
    F16(0x82) = (uint16_t)sum;
    F32(0x80) = edx + M32(edi + 0x4) + (sum >> 16);
    F16(0x72) = M16(edi + 0x12);
    F32(0x70) = M32(edi + 0x14);
    F32(0x40) = 5;
    ebx = FA(0x84);
    F32(0x20) = ebx;
    M32(edi + 0x78) = 0;
have_target:
    eax = M32(ebx + 0x44);
    F32(0x14) = eax;
    eax = udiv(0, eax * 0x6487e, 3, NULL);
    ecx = eax;
    F32(0x34) = udiv(0x10000, 0, ecx, NULL);

    /* the way back, written backwards from the block's end */
    S_REVERSE = 1;
    pen_from_object();
    esi = M32(edi + 0x70) + F32(0x38) - 0x1f;
    S_SPEED = 0;
    S_SPEED_TO = 0;
    M32(edi + 0xc0) = M32(edi + 0xc0) - 0x80000000u;
    emit_step();
    S_SPEED = 9;
    S_SPEED_TO = 3;
    F32(0x2c) = 0;
    for (;;) {
        ebx = F32(0x20);
        eax = M32(ebx + 0x4) - M32(edi + 0xa4);
        F32(0x8) = eax;
        F32(0xc) = eax * eax;
        edx = HI(smul(M32(ebx + 0xc) - M32(edi + 0xac), S_LON_TO_M));
        F32(0x4) = edx;
        F32(0xc) = F32(0xc) + edx * edx;
        F32(0xc) = UTIL_SQRT32(F32(0xc));
        eax = UTIL_ATAN2((int32_t)F32(0x4), (int32_t)F32(0x8));
        F16(0xe) = (uint16_t)eax;
        eax = udiv(F32(0x14), 0, F32(0xc), NULL);
        eax >>= 0x11;
        eax = UTIL_ASIN((int16_t)eax, 1);
        F16(0xe) = (uint16_t)(F16(0xe) - (uint16_t)eax);
        ecx = (uint32_t)F16(0xe) << 16;
        ecx -= M32(edi + 0xc0);
        ecx -= 0x80000000u;
        if (F32(0x2c) != 0)
            break;
        F32(0x2c) = 1;
        if ((int32_t)ecx > 0xccc0000) {
            F32(0x2c) = 0;
            ecx = 0xccc0000;
        }
        if ((int32_t)ecx < (int32_t)0xf3340000u) {
            F32(0x2c) = 0;
            ecx = 0xf3340000u;
        }
        ecx = (uint32_t)((int32_t)ecx >> 1);
        M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;
        emit_step();
        M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;
    }
    ecx += M32(edi + 0xc0);
    F32(0x24) = ecx;
    esi += 0x1f;
    F32(0x1c) = esi;

    /* the way out, forwards from the start */
    S_REVERSE = 0;
    pen_from_object();
    esi = M32(edi + 0x70);
    S_SPEED = 0;
    S_SPEED_TO = 0;
    emit_step();
    S_SPEED = 9;
    S_SPEED_TO = 3;
    F32(0x2c) = 0;
    for (;;) {
        ebx = F32(0x20);
        eax = M32(ebx + 0x4) - M32(edi + 0xa4);
        F32(0x8) = eax;
        F32(0xc) = eax * eax;
        edx = HI(smul(M32(ebx + 0xc) - M32(edi + 0xac), S_LON_TO_M));
        F32(0x4) = edx;
        F32(0xc) = F32(0xc) + edx * edx;
        F32(0xc) = UTIL_SQRT32(F32(0xc));
        eax = UTIL_ATAN2((int32_t)F32(0x4), (int32_t)F32(0x8));
        F16(0xe) = (uint16_t)eax;
        eax = udiv(F32(0x14), 0, F32(0xc), NULL);
        eax >>= 0x11;
        eax = UTIL_ASIN((int16_t)eax, 1);
        F16(0xe) = (uint16_t)(F16(0xe) + (uint16_t)eax);
        ecx = (uint32_t)F16(0xe) << 16;
        ecx -= M32(edi + 0xc0);
        if (F32(0x2c) != 0)
            break;
        F32(0x2c) = 1;
        if ((int32_t)ecx > 0xccc0000) {
            F32(0x2c) = 0;
            ecx = 0xccc0000;
        }
        if ((int32_t)ecx < (int32_t)0xf3340000u) {
            F32(0x2c) = 0;
            ecx = 0xf3340000u;
        }
        ecx = (uint32_t)((int32_t)ecx >> 1);
        M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;
        emit_step();
        M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;
    }
    M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;

    /* round the aircraft to the stop */
    M8(esi) = 0x42;
    M8(esi + 1) = 1;
    esi += 2;
    ebx = M32(edi + 0xc0) + 0x40000000u;
    F32(0x28) = ebx;
    ebx = (uint32_t)((int32_t)ebx >> 16);
    UTIL_SINCOS((int16_t)ebx, &s, &c);
    F16(0x16) = (uint16_t)s;
    F16(0x18) = (uint16_t)c;
    eax = LO(smul(SX16(F16(0x16)), F32(0x14)));
    F32(0x4) = F32(0x4) + (uint32_t)((int32_t)eax >> 15);
    eax = LO(smul(SX16(F16(0x18)), F32(0x14)));
    F32(0x8) = F32(0x8) + (uint32_t)((int32_t)eax >> 15);
    eax = F32(0x4);
    F32(0xc) = eax * eax;
    eax = F32(0x8);
    F32(0xc) = F32(0xc) + eax * eax;
    eax = UTIL_SQRT32(F32(0xc));
    eax = udiv(0, eax, 3, NULL);
    emit_arc(F32(0x20), F32(0x28), F32(0x14), M32(edi + 0xc0), eax);
    M8(esi) = 0x42;
    M8(esi + 1) = 0;
    esi += 2;
    ebx = F32(0x20);
    eax = F32(0x28) - M32(ebx + 0x20);
    n = arc_steps(eax, F32(0x34));
    F32(0x30) = udiv(0, eax, n, NULL);
    ecx = n;
    do {
        eax = F32(0x30);
        F32(0x28) = F32(0x28) - eax;
        M32(edi + 0xc0) = M32(edi + 0xc0) - eax;
        emit_arc(F32(0x20), F32(0x28), F32(0x14), M32(edi + 0xc0), 1);
    } while (--ecx != 0);
    M8(esi) = 0x3f;
    M32(esi + 1) = F32(0x3c);
    esi += 5;
    emit_arc(F32(0x20), F32(0x28), F32(0x14), M32(edi + 0xc0), 1);
    eax = M32(edi + 0xc0) - F32(0x24);
    n = arc_steps(eax, F32(0x34));
    F32(0x30) = udiv(0, eax, n, NULL);
    ecx = n;
    do {
        eax = F32(0x30);
        F32(0x28) = F32(0x28) - eax;
        M32(edi + 0xc0) = M32(edi + 0xc0) - eax;
        emit_arc(F32(0x20), F32(0x28), F32(0x14), M32(edi + 0xc0), 1);
    } while (--ecx != 0);

    /* join the way back: one 0x3b over the gap, then the reversed points */
    ebx = F32(0x1c);
    esi -= 0x1f;
    eax = M32(ebx + 3) - M32(esi + 3);
    ecx = eax * eax;
    eax = HI(smul(M32(ebx + 9) - M32(esi + 9), S_LON_TO_M));
    ecx += eax * eax;
    eax = UTIL_SQRT32(ecx);
    eax = arc_steps(eax, 3);
    esi += 0x1f;
    M8(esi) = 0x3b;
    M8(esi + 1) = (uint8_t)eax;
    esi += 2;
    ebx += 1;
    ecx = M32(edi + 0x70) + F32(0x38) - ebx;
    copy_down(esi, ebx, ecx);
    esi += ecx;
    M8(esi) = 0x26;
    esi += 1;
    esi -= M32(edi + 0x70);
    REALLOC(esi, edi + 0x70);
    esi = M32(edi + 0x70);
    return 0;
#undef F32
#undef F16
#undef FA
}

/* .text+0x297e -- 0x3f, refuel the aircraft linked at edi+0x78.  The user's
 * tanks (GLOBAL +0xb74..+0xba0, level and capacity) fill at a fixed rate,
 * or, when GLOBAL +0xc44 is set, in proportion to the room left in each, at
 * a rate from [esi+1]; a player just waits out a countdown at its +0x34.
 * Carry set while it goes on.  Each tank's share is its room halved over
 * the total, as a 32.32 fraction whose low bit is lost (see the rcr).  The
 * proportional total leaves out the first tank -- its room is stored and then overwritten by the second's -- so a
 * first tank with more room than twice the rest overflows the divide, as
 * in retail. */
static int op_refuel(void)
{
    static const uint16_t tank[5][2] = {    /* capacity, level */
        { 0xb78, 0xb74 }, { 0xb80, 0xb7c }, { 0xb98, 0xb94 },
        { 0xb88, 0xb84 }, { 0xba0, 0xb9c }
    };
    uint32_t rec, ebx, eax, ecx = 0, edx, have, full, room[5], total, rate, k;
    uint64_t p;

    path_end(&ecx);
    ebx = M32(edi + 0x78);
    if (ebx == 0)
        goto done;
    ebx += S_PLAYERS;
    rec = ebx;
    if (M32(ebx + 0x24) == 0 || M32(ebx + 0x2c) > 0x7fff)
        goto finished;
    if (M32(edi + 0x78) != 4) {
        eax = M32(ebx + 0x34);
        if (eax == 0)
            eax = 0x50000;
        if (eax > S_DT) {
            M32(ebx + 0x34) = eax - S_DT;
            return 1;
        }
        M32(ebx + 0x34) = 0;
        goto finished;
    }
    ebx = G;
    if (M16(ebx + 0xc44) == 0) {
        p = umul(0xccccc, S_DT);
        eax = (LO(p) >> 15) | (HI(p) << 17);
        ecx = 0x7fffff;
        have = 0;
        full = 0;
        for (k = 0; k < 5; k++) {
            if (M32(ebx + tank[k][0]) != 0) {
                have++;
                edx = M32(ebx + tank[k][1]) + eax;
                if (!(edx < ecx)) {
                    full++;
                    edx = ecx;
                }
                M32(ebx + tank[k][1]) = edx;
            }
        }
        if (have > full)
            return 1;
        goto fill;
    }
    for (k = 0; k < 5; k++) {
        ecx = M32(ebx + tank[k][0]);
        p = umul(M32(ebx + tank[k][1]), ecx);
        eax = (LO(p) >> 7) | (HI(p) << 25);
        room[k] = (ecx << 16) - eax;
    }
    total = room[1] + room[2] + room[3] + room[4];
    if (M32(rec + 0x34) == 0) {
        p = umul(0x3c, total);
        ecx = M32(esi + 1);
        eax = udiv(HI(p), LO(p), ecx, NULL);
        ecx = ecx * 0x444;
        if (eax > 0xa0000) {
            p = umul(eax - 0xa0000, M16(ebx + 0xc44));
            eax = udiv(HI(p), LO(p), 0x64, NULL);
            ecx = eax + 0xa0000;
            ecx = udiv(total >> 16, total << 16, ecx, NULL);
        }
        M32(rec + 0x34) = ecx;
    }
    p = umul(M32(rec + 0x34), S_DT);
    rate = (LO(p) >> 15) | (HI(p) << 17);
    if (total < rate)
        goto fill;
    for (k = 0; k < 5; k++) {
        edx = room[k];
        if (edx == 0)
            continue;
        /* shr edx,1 / xor eax,eax / rcr eax,1: the xor clears the carry
         * the shift left, so the low bit of the room is dropped, not
         * rotated into eax */
        eax = udiv(edx >> 1, 0, total, NULL);
        p = umul(eax, rate);
        eax = (LO(p) >> 24) | (HI(p) << 8);
        edx = HI(p) >> 24;
        M32(ebx + tank[k][1]) = M32(ebx + tank[k][1]) + udiv(edx, eax, M32(ebx + tank[k][0]), NULL);
    }
    return 1;
fill:
    M32(ebx + 0xb74) = 0x7fffff;
    M32(ebx + 0xb7c) = 0x7fffff;
    M32(ebx + 0xb94) = 0x7fffff;
    M32(ebx + 0xb84) = 0x7fffff;
    M32(ebx + 0xb9c) = 0x7fffff;
finished:
    if (M32(edi + 0x78) == 4) {
        S_LOCK = 0;
        M32(G + 0x348) = 5;
    }
    M32(rec + 0x38) = 0;
    M32(rec + 0x3c) = 0x5a;
done:
    esi += 5;
    return 0;
}

/* .text+0x2232 -- 0x3d, wait for an aircraft: the first in the table that
 * nobody has (+0x38), not just served (+0x3c), low (+0x2c), on the ground
 * (+0x30), within [esi+9] of [esi+1]/[esi+5], of a kind [esi+0xf] takes
 * (1: kind 0, 2: kinds 1 and 3, 3: any of those) is linked to this object,
 * and the program calls [esi+0xd] on. */
static int op_meet(void)
{
    uint32_t ebx, ecx, eax;
    uint16_t want;

    ebx = S_PLAYERS;
    ecx = M32(ebx);
    ebx += 4;
    for (;;) {
        if (M32(ebx + 0x38) != 0 || M32(ebx + 0x3c) != 0 || M32(ebx + 0x2c) > 0x7fff
            || M32(ebx + 0x30) != 0)
            goto next;
        if (iabs(M32(ebx + 0x4) - M32(esi + 1)) > M32(esi + 9))
            goto next;
        if (HI(umul(iabs(M32(ebx + 0xc) - M32(esi + 5)), S_LON_TO_M)) > M32(esi + 9))
            goto next;
        eax = M32(ebx + 0x40);
        want = M16(esi + 0xf);
        if (eax == 0) {
            if (want == 1)
                goto match;
        } else if (eax == 1 || eax == 3) {
            if (want == 2)
                goto match;
        } else {
            goto next;
        }
        if (want == 3)
            goto match;
    next:
        if (--ecx == 0) {
            esi += 0x11;
            return 0;
        }
        ebx += 0x48;
        continue;
    match:
        eax = ebx - S_PLAYERS;
        M32(edi + 0x78) = eax;
        if (eax == 4)
            S_LOCK = 1;
        M32(ebx + 0x38) = edi - S_OBJECTS;
        eax = M16(esi + 0xd) + esi;
        esi += 0x11;
        M32(edi + 0x6c) = esi;
        esi = eax;
        return 0;
    }
}

/* The box test 0x40 and 0x43 share, against the user's aircraft and then
 * each other live object: within r of (lat, lon) north and east, 0 to 10
 * above alt, and, rotated by the box's heading, within `along` one way and r
 * the other.  1 when something is in it. */
static int in_box(uint32_t lat, uint32_t lon, uint32_t alt, uint32_t r, uint32_t along,
                  int16_t s, int16_t c)
{
    uint32_t ebx, ecx, eax, dlat, dlon;
    int32_t ox, oy;

    ebx = G + 0x560;
    ecx = 0xffffffffu;
    for (;;) {
        dlat = M32(ebx + 0x4) - lat;
        if (iabs(dlat) > r)
            goto next;
        dlon = HI(smul(M32(ebx + 0xc) - lon, S_LON_TO_M));
        if (iabs(dlon) > r)
            goto next;
        eax = M32(ebx + 0x14) - alt;
        if ((int32_t)eax < 0 || (int32_t)eax > 10)
            goto next;
        UTIL_ROTATE2D((int32_t)dlon, (int32_t)dlat, s, c, &ox, &oy);
        if (iabs((uint32_t)ox) > along)
            goto next;
        if (!(iabs((uint32_t)oy) > r))
            return 1;
    next:
        if ((int32_t)ecx < 0) {
            ebx = S_OBJECTS;
            ecx = M32(ebx);
            ebx += 4;
            goto test;
        }
        for (;;) {
            ebx += 0x120;
            if (--ecx == 0)
                return 0;
        test:
            if (M32(ebx + 0x64) != 0 && edi != ebx)
                break;
        }
    }
}

/* .text+0x361e -- 0x43: past it (0x19 bytes) when something is in the box,
 * else jump [esi+1] */
static int op_jnear(void)
{
    int16_t s, c;
    uint32_t eax;

    UTIL_SINCOS((int16_t)M16(esi + 0xf), &s, &c);
    if (in_box(M32(esi + 3), M32(esi + 7), M32(esi + 0xb), M32(esi + 0x11), M32(esi + 0x15),
               s, c))
        eax = 0x19;
    else
        eax = SX16(M16(esi + 1));
    esi += eax;
    return 0;
}

/* .text+0x3746 and +0x3798 -- a 90-degree turn by banking: bank to 0xf1c8
 * (left) or 0x0e38 (right) until the heading has gone 0x3721 round, then
 * level, to the heading 90 degrees round */
static void turn_left(void)
{
    uint32_t from;

    S_BANK_TO = 0xf1c80000u;
    from = M32(edi + 0xc0);
    S_HDG_TO = from + 0x40000000u;
    do
        emit_step();
    while ((int32_t)(M32(edi + 0xc0) - from) < 0x37210000);
    S_BANK_TO = 0;
    do
        emit_step();
    while (M32(edi + 0xbc) != 0);
}

static void turn_right(void)
{
    uint32_t from;

    S_BANK_TO = 0x0e380000u;
    from = M32(edi + 0xc0);
    S_HDG_TO = from - 0x40000000u;
    do
        emit_step();
    while ((int32_t)(from - M32(edi + 0xc0)) < 0x37210000);
    S_BANK_TO = 0;
    do
        emit_step();
    while (M32(edi + 0xbc) != 0);
}

/* .text+0x31fe -- 0x40, a circuit, when something is in the box at [esi+1]
 * (as 0x43's, heading [esi+0x10], r [esi+0x12], along [esi+0x16]); else
 * past it.  The generated program climbs out at pitch 0xfa50 to 250 above
 * the start, levels, makes four 90-degree turns ([esi+0xd] picks left or
 * right) at speed [esi+0xe] (30..270) -- the third leg's length from the
 * speed -- then descends on a reversed copy of a climb written backwards
 * from the block's end, joined by one 0x3b.  It returns to this op. */
static int op_circuit(void)
{
    uint8_t fr[0x2c];
#define F32(o) M32(ADDR(fr) + 0x2c - (o))
#define FA(o)  (ADDR(fr) + 0x2c - (o))
    void (*turn)(void);
    int16_t s, c;
    uint32_t eax, ebx, ecx, out;

    UTIL_SINCOS((int16_t)M16(esi + 0x10), &s, &c);
    if (!in_box(M32(esi + 1), M32(esi + 5), M32(esi + 9), M32(esi + 0x12), M32(esi + 0x16),
                s, c)) {
        esi += 0x1a;
        return 0;
    }
    turn = M8(esi + 0xd) != 0 ? turn_left : turn_right;
    eax = M16(esi + 0xe);
    if (eax < 0x1e)
        eax = 0x1e;
    if (eax > 0x10e)
        eax = 0x10e;
    F32(0x20) = eax;
    S_REVERSE = 0;
    M32(edi + 0x74) = esi;
    F32(0x18) = 0x3c8c;
    ALLOC(F32(0x18), edi + 0x70, 0);
    esi = M32(edi + 0x70);
    S_SPEED = F32(0x20);
    S_SPEED_TO = (S_SPEED >> 2) + S_SPEED;
    F32(0x1c) = M32(edi + 0x14) + 0xfa;
    pen_from_object();
    S_PITCH_TO = 0xfa500000u;
    S_BANK_TO = 0;
    S_HDG_TO = M32(edi + 0xc0);
    do
        emit_step();
    while (M32(edi + 0xb8) != 0xfa500000u);
    if (M16(edi + 0x24) & 4) {
        M8(esi) = 4;
        M8(esi + 1) = 0;
        esi += 2;
    }
    do
        emit_step();
    while ((int32_t)M32(edi + 0xb4) < (int32_t)F32(0x1c));
    S_PITCH_TO = 0;
    do
        emit_step();
    while (M32(edi + 0xb8) != 0);
    S_REPEAT = 0xa;
    emit_step();
    turn();
    S_REPEAT = 0x1e;
    emit_step();
    turn();
    ecx = 0x40;
    eax = S_SPEED_TO;
    if (eax > 0x5f)
        eax = (eax - 0x5f) >> 3;
    else
        eax = 0x5f - eax;
    ecx += eax;
    S_REPEAT = ecx;
    emit_step();
    turn();
    S_REPEAT = 0x1e;
    emit_step();
    turn();

    out = esi;
    esi = M32(edi + 0x70) + F32(0x18) - 0x1f;
    pen_from_object();
    S_REVERSE = 1;
    M8(esi) = 0x39;
    M16(esi + 1) = M16(edi + 0xa2);
    M32(esi + 3) = M32(edi + 0xa4);
    M16(esi + 7) = M16(edi + 0xaa);
    M32(esi + 9) = M32(edi + 0xac);
    M16(esi + 0xd) = M16(edi + 0xb2);
    M32(esi + 0xf) = M32(edi + 0xb4);
    M32(esi + 0x13) = M32(edi + 0xb8);
    M32(esi + 0x17) = M32(edi + 0xbc);
    M32(esi + 0x1b) = M32(edi + 0xc0);
    esi -= 0x1f;
    S_SPEED = F32(0x20);
    S_PITCH_TO = 0x5b00000;
    S_BANK_TO = 0;
    do
        emit_step();
    while ((int32_t)M32(edi + 0xb4) < (int32_t)F32(0x1c));
    S_PITCH_TO = 0;
    if (M16(edi + 0x24) & 4) {
        esi += 0x1d;
        M8(esi) = 4;
        M8(esi + 1) = 0;
        esi -= 0x1f;
    }
    do
        emit_step();
    while (M32(edi + 0xb8) != 0);
    do
        emit_step();
    while (S_SPEED != S_SPEED_TO);
    S_REVERSE = 0;

    ebx = out - 0x1f;
    esi += 0x1f;
    {
        uint32_t dlon = HI(smul(M32(esi + 9) - M32(ebx + 9), S_LON_TO_M));
        uint64_t sq = smul(dlon, dlon);
        uint64_t sq2 = smul(M32(esi + 3) - M32(ebx + 3), M32(esi + 3) - M32(ebx + 3));
        uint32_t lo = LO(sq) + LO(sq2);
        F32(0x28) = lo;
        F32(0x24) = HI(sq) + HI(sq2) + (lo < LO(sq));
    }
    eax = UTIL_SQRT64(FA(0x28));
    eax = udiv(0, eax, S_SPEED_TO, NULL);
    if (eax == 0)
        eax++;
    ebx += 0x1f;
    M8(ebx) = 0x3b;
    M8(ebx + 1) = (uint8_t)eax;
    ebx += 2;
    esi += 1;
    ecx = M32(edi + 0x70) + F32(0x18) - esi;
    copy_down(ebx, esi, ecx);
    esi = ebx + ecx;
    M8(esi) = 0x26;
    esi += 1;
    esi -= M32(edi + 0x70);
    REALLOC(esi, edi + 0x70);
    esi = M32(edi + 0x70);
    return 0;
#undef F32
#undef FA
}

/* .text+0x2cf4 -- 0x41, come round onto a heading: to a point beside the
 * target [esi+1]/[esi+7] -- the object's size off it, on the side nearer
 * the object -- then an arc about it onto heading [esi+0x13], ending at the
 * target itself, at altitude [esi+0xd].  Generated, and returns past the op. */
static int op_turn_to(void)
{
    uint8_t fr[0x6c];
#define F32(o) M32(ADDR(fr) + 0x6c - (o))
#define F16(o) M16(ADDR(fr) + 0x6c - (o))
#define FA(o)  (ADDR(fr) + 0x6c - (o))
    int16_t s, c;
    uint32_t eax, ebx, ecx, edx, sum, n;
    uint64_t p;
    uint16_t cx;

    F32(0x64) = esi;
    M32(edi + 0x74) = esi + 0x17;
    F32(0x4) = 0x1838;
    ALLOC(F32(0x4), edi + 0x70, 0);
    eax = M16(edi + 0x2c);
    F32(0x20) = eax;
    ecx = udiv(0, eax * 0x6487e, 3, NULL);
    F32(0x50) = udiv(0x10000, 0, ecx, NULL);
    ebx = (M32(esi + 0x13) + 0x40000000u) >> 16;
    UTIL_SINCOS((int16_t)ebx, &s, &c);
    F16(0x4a) = (uint16_t)s;
    F16(0x4c) = (uint16_t)c;
    p = smul(SX16(F16(0x4a)) * 2, F32(0x20));
    F16(0x2e) = (uint16_t)LO(p);
    F32(0x2c) = (HI(p) << 16) | (LO(p) >> 16);
    UTIL_TO_LON(FA(0x30), S_ORIGIN, FA(0x38));
    p = smul(SX16(F16(0x4c)) * 2, F32(0x20));
    F16(0x3e) = (uint16_t)LO(p);
    F32(0x3c) = (HI(p) << 16) | (LO(p) >> 16);
    eax = M32(esi + 3) - M32(edi + 0x4);
    F32(0x28) = eax;
    eax += F32(0x3c);
    F32(0x44) = eax * eax;
    edx = HI(smul(M32(esi + 9) - M32(edi + 0xc), S_LON_TO_M));
    F32(0x24) = edx;
    eax = edx + F32(0x2c);
    F32(0x44) = F32(0x44) + eax * eax;
    F32(0x44) = UTIL_SQRT32(F32(0x44));
    eax = F32(0x28) - F32(0x3c);
    F32(0x48) = eax * eax;
    eax = F32(0x24) - F32(0x2c);
    F32(0x48) = F32(0x48) + eax * eax;
    eax = UTIL_SQRT32(F32(0x48));
    F32(0x48) = eax;
    if (eax > F32(0x44)) {
        F32(0x5c) = 0;
        sum = (uint32_t)M16(esi + 7) + F16(0x36);
        F16(0x12) = (uint16_t)sum;
        F32(0x10) = M32(esi + 9) + F32(0x34) + (sum >> 16);
        sum = (uint32_t)M16(esi + 1) + F16(0x3e);
        F16(0x1a) = (uint16_t)sum;
        F32(0x18) = M32(esi + 3) + F32(0x3c) + (sum >> 16);
    } else {
        F32(0x5c) = 1;
        F16(0x12) = (uint16_t)(M16(esi + 7) - F16(0x36));
        F32(0x10) = M32(esi + 9) - F32(0x34) - (M16(esi + 7) < F16(0x36));
        F16(0x1a) = (uint16_t)(M16(esi + 1) - F16(0x3e));
        F32(0x18) = M32(esi + 3) - F32(0x3c) - (M16(esi + 1) < F16(0x3e));
    }
    pen_from_object();
    esi = M32(edi + 0x70);
    M8(esi) = 0x42;
    M8(esi + 1) = 1;
    esi += 2;
    S_SPEED = 0;
    S_SPEED_TO = 0;
    emit_step();
    S_SPEED = 3;
    S_SPEED_TO = 3;
    F32(0x60) = 0;
    for (;;) {
        eax = F32(0x18) - M32(edi + 0xa4);
        F32(0x28) = eax;
        F32(0x68) = eax * eax;
        edx = HI(smul(F32(0x10) - M32(edi + 0xac), S_LON_TO_M));
        F32(0x24) = edx;
        F32(0x68) = F32(0x68) + edx * edx;
        F32(0x68) = UTIL_SQRT32(F32(0x68));
        eax = UTIL_ATAN2((int32_t)F32(0x24), (int32_t)F32(0x28));
        F16(0x56) = (uint16_t)eax;
        eax = udiv(F32(0x20), 0, F32(0x68), NULL);
        eax >>= 0x11;
        eax = UTIL_ASIN((int16_t)eax, 1);
        if (F32(0x5c) == 0)
            eax = (eax & 0xffff0000u) | (uint16_t)(0u - eax);
        F16(0x56) = (uint16_t)(F16(0x56) + (uint16_t)eax);
        ecx = (uint32_t)F16(0x56) << 16;
        ecx -= M32(edi + 0xc0);
        if (F32(0x60) != 0)
            break;
        F32(0x60) = 1;
        if ((int32_t)ecx > 0xccc0000) {
            F32(0x60) = 0;
            ecx = 0xccc0000;
        }
        if ((int32_t)ecx < (int32_t)0xf3340000u) {
            F32(0x60) = 0;
            ecx = 0xf3340000u;
        }
        ecx = (uint32_t)((int32_t)ecx >> 1);
        M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;
        emit_step();
        M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;
    }
    M32(edi + 0xc0) = M32(edi + 0xc0) + ecx;
    ebx = M32(edi + 0xc0);
    ebx = F32(0x5c) != 0 ? ebx + 0x40000000u : ebx - 0x40000000u;
    F32(0x6c) = ebx;
    ebx = (uint32_t)((int32_t)ebx >> 16);
    UTIL_SINCOS((int16_t)ebx, &s, &c);
    F16(0x4a) = (uint16_t)s;
    F16(0x4c) = (uint16_t)c;
    eax = LO(smul(SX16(F16(0x4a)), F32(0x20)));
    F32(0x24) = F32(0x24) + (uint32_t)((int32_t)eax >> 15);
    eax = LO(smul(SX16(F16(0x4c)), F32(0x20)));
    F32(0x28) = F32(0x28) + (uint32_t)((int32_t)eax >> 15);
    eax = F32(0x24);
    F32(0x68) = eax * eax;
    eax = F32(0x28);
    F32(0x68) = F32(0x68) + eax * eax;
    eax = UTIL_SQRT32(F32(0x68));
    eax = udiv(0, eax, 3, NULL);
    ebx = F32(0x64);
    cx = M16(ebx + 0xd);
    edx = M32(ebx + 0xf);
    M16(edi + 0xb2) = cx;
    M32(edi + 0xb4) = edx;
    emit_arc(FA(0x1c), F32(0x6c), F32(0x20), M32(edi + 0xc0), eax);
    ebx = F32(0x64);
    if (F32(0x5c) == 0)
        eax = M32(ebx + 0x13) - M32(edi + 0xc0);
    else
        eax = M32(edi + 0xc0) - M32(ebx + 0x13);
    n = arc_steps(eax, F32(0x50));
    eax = udiv(0, eax, n, NULL);
    if (F32(0x5c) == 0)
        eax = 0u - eax;
    F32(0x54) = eax;
    ecx = n;
    do {
        eax = F32(0x54);
        F32(0x6c) = F32(0x6c) - eax;
        M32(edi + 0xc0) = M32(edi + 0xc0) - eax;
        emit_arc(FA(0x1c), F32(0x6c), F32(0x20), M32(edi + 0xc0), 1);
    } while (--ecx != 0);
    esi -= 0x1f;
    ebx = F32(0x64);
    M16(esi + 7) = M16(ebx + 7);
    M32(esi + 9) = M32(ebx + 9);
    M16(esi + 1) = M16(ebx + 1);
    M32(esi + 3) = M32(ebx + 3);
    M32(esi + 0x1b) = M32(ebx + 0x13);
    esi += 0x1f;
    M8(esi) = 0x42;
    M8(esi + 1) = 0;
    esi += 2;
    M8(esi) = 0x26;
    esi += 1;
    esi -= M32(edi + 0x70);
    REALLOC(esi, edi + 0x70);
    esi = M32(edi + 0x70);
    return 0;
#undef F32
#undef F16
#undef FA
}

/* ---- the image ------------------------------------------------------------------- */

struct dyn_image dyn_image = {
    {
        { FS98_MOD_HOST,   0 },
        { FS98_MOD_GLOBAL, 0 },
        { FS98_MOD_CHAIN,  0 },
        { FS98_MOD_FE,     0 },
        { FS98_MOD_G3D,    0 },
        { FS98_MOD_UTIL,   0 },
        { FS98_MOD_MULTIP, 0 },
        { 0, 0 }
    },
    {
        FS98_MOD_DYNAMIC, dyn_init, dyn_teardown, 0, 0, 0, dyn_frame, 0,
        {
#define DYN_OPS
#include "dynamic_data.inc"
#undef DYN_OPS
        }
    },
    {
#include "dynamic_data.inc"
    }
};

/* The two exports are the image's first 0x40 bytes and the 0x130 at +0x40,
 * where retail has them. */
__asm__(".globl _ImportTable\n\t.set _ImportTable, _dyn_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _dyn_image + 0x40");

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
