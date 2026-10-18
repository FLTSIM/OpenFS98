/* ANALYSIS.DLL -- module id 29: landing analysis, manoeuvre analysis and
 * course tracking.
 *
 * A frame callback on CHAIN list 5 records the aircraft's position (GLOBAL
 * +0x560, 24 bytes) into 1,000 samples of 0x1c bytes, every few frames, for
 * the mode in GLOBAL +0x814: 1 landing analysis, 2 course tracking, 3
 * manoeuvre analysis.  Landing arms above about 34 m and starts recording
 * at about 100 ft; on the ground it opens the landing dialog.  Manoeuvre
 * ends on command 0x1002b (seen on CHAIN list 0xc) and opens its dialog.
 * Course tracking with GLOBAL +0x81c draws the trail in the 3D world from
 * CHAIN list 0xe: a display list built in a heap block and handed to G3D
 * +0x28.  The two dialogs (host +0x10c, 0x14050002 and 0x14050003) draw the
 * descent and the ground track into a dialog control through HG2D's lines;
 * the options dialog (0x1405) moves GLOBAL +0x814..+0x828 through UTIL's
 * exchange.
 *
 * Retail is two programs, like REALITY's: hand-written assembly from
 * .text+0x20 to +0x912, with ATC's stack-alignment wrapper around every
 * import call (no observable effect, so not reproduced), and MSVC C from
 * +0x920.  The whole 0x32c-byte .data image is kept at retail's offsets.
 *
 * The assembly reads registers that callees leave, and three of them show:
 *
 *   - The change test counts `repz cmpsw` with only cx set, so ecx's top
 *     half is the caller's: the frame's entry ecx (retail CHAIN passes the
 *     dispatch's first argument there), or what a CHAIN add or the host's
 *     memset left.  The frame is entered through a stub that passes ecx in,
 *     and those calls go through thunks that hand ecx back.
 *   - The compaction keeps its slot in ecx across host +0x21c, which loads
 *     ecx itself and calls MSVCRT memmove.  What memmove leaves is a
 *     property of the C runtime: wine's builtin leaves 0, the CD's 1997
 *     MSVCRT.DLL 7.  Decided: reproduce the CD's 7, on every platform
 *     (AN_MEMMOVE_ECX).
 *
 * docs/modules/analysis.md has the details.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define AN_IMAGE_SIZE 0x32c
#define AN_IMPORTS    0x088
#define AN_LINKAGE    0x0c8

/* ecx after host +0x21c's memmove of a 0x1c-byte record: the dword count
 * the CD's MSVCRT.DLL (22 Jan 1997) leaves (docs/preflight.md, round 8) */
#define AN_MEMMOVE_ECX 7u

typedef volatile uint8_t  an_v8;
typedef volatile uint16_t __attribute__((may_alias)) an_v16;
typedef volatile uint32_t __attribute__((may_alias)) an_v32;
typedef volatile double   __attribute__((may_alias)) an_vd;

struct analysis_image {
    uint8_t bytes[AN_IMAGE_SIZE];
};

extern struct analysis_image analysis_image;

static const uint8_t analysis_rdata[0x48] __attribute__((aligned(8))) = {
#include "analysis_rdata.inc"
};

#define D       ((uint32_t)(uintptr_t)analysis_image.bytes)
#define R       ((uint32_t)(uintptr_t)analysis_rdata)
#define M8(a)   (*(an_v8  *)(uintptr_t)(uint32_t)(a))
#define M16(a)  (*(an_v16 *)(uintptr_t)(uint32_t)(a))
#define M32(a)  (*(an_v32 *)(uintptr_t)(uint32_t)(a))
#define MD(a)   (*(an_vd  *)(uintptr_t)(uint32_t)(a))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

#define IMPORT(i)  M32(D + AN_IMPORTS + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define G          IMPORT(1)
#define CHAIN      IMPORT(2)
#define HG2D       IMPORT(3)
#define G3D        IMPORT(4)
#define NUMERIC    IMPORT(5)
#define UTIL       IMPORT(6)

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);
typedef uint32_t (FS98_STDCALL *f6)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_ATTACH      HOST_(0x074, f2)   /* (hwnd, rec): a dialog control's surface */
#define H_RELEASE     HOST_(0x078, f1)   /* (rec) */
#define H_FLUSH       HOST_(0x084, f1)   /* (rec) */
#define H_DIALOG      HOST_(0x10c, f2)   /* (id, block) */
#define H_ALLOC       HOST_(0x20c, f4)   /* (flags, size, &out, zero) */
#define H_FREE        HOST_(0x214, f1)   /* (&p) */
#define H_MEMSET      HOST_(0x218, f3)   /* (dst, byte, n) */
#define H_MEMMOVE     HOST_(0x21c, f3)   /* (dst, src, n) */
#define H_STRCPY      HOST_(0x2a8, f2)

#define CHAIN_REMOVE  CALL(CHAIN, 0x1c, f2)    /* (list, callback) */
#define G3D_LIST      CALL(G3D, 0x28, f1)      /* (display list) */
#define HG2D_LINE_H   CALL(HG2D, 0x2c, f4)     /* (&surface, x1, x2, y) */
#define HG2D_LINE_V   CALL(HG2D, 0x30, f4)     /* (&surface, x, y1, y2) */
#define HG2D_LINE     CALL(HG2D, 0x34, f5)     /* (&surface, x1, y1, x2, y2) */
#define NUM_FORMAT    CALL(NUMERIC, 0x18, f3)  /* (&value, format, out) */
#define UTIL_LONM     CALL(UTIL, 0x48, f3)     /* longitude to metres */
#define UTIL_RANGE48  CALL(UTIL, 0x64, f6)     /* (a, b, flags, &dist, &bearing, &scale) */
#define UTIL_PUT      CALL(UTIL, 0x8c, f4)     /* (table, n, records, mode) */
#define UTIL_GET      CALL(UTIL, 0x90, f3)     /* (table, n, records) */
#define UTIL_BC       CALL(UTIL, 0xbc, f2)
#define UTIL_C0       CALL(UTIL, 0xc0, f2)
#define UTIL_C4       CALL(UTIL, 0xc4, f2)
#define UTIL_C8       CALL(UTIL, 0xc8, f2)

#define GLB8(o)  M8(G + (o))
#define GLB16(o) M16(G + (o))
#define GLB32(o) M32(G + (o))

/* ---- the state, at retail's offsets (D + offset) ------------------------- */

#define S_ENDED     (D + 0x000)    /* command 0x1002b seen in manoeuvre mode */
#define S_RESET     (D + 0x004)    /* a reset asked for (nothing here sets it) */
#define S_SCRATCH   (D + 0x008)    /* a record, for the compaction */
#define S_COUNT     (D + 0x024)
#define S_RECORD    (D + 0x028)    /* the record being stored: flag, then */
#define S_LAST      (D + 0x02c)    /*   the last sample's 24 bytes */
#define S_SAMPLES   (D + 0x044)    /* 1000 records of 0x1c */
#define S_CMD_ON    (D + 0x048)    /* registered on list 0xc */
#define S_TRAIL_ON  (D + 0x04c)    /* registered on list 0xe */
#define S_LIST      (D + 0x050)    /* the trail's display list, 0x59e4 */
#define S_COUNTDOWN (D + 0x070)    /* byte: frames to the next sample */
#define S_INTERVAL  (D + 0x071)    /* byte: manoeuvre's, rol'd at a compaction */
#define S_LANDING   (D + 0x072)    /* byte: 0, 1 armed, 2 recording */
#define S_FRAMES    (D + 0x073)    /* bytes 5, 14, 29, 50, by GLOBAL +0x824 */
#define S_SEGMENTS  (D + 0x076)    /* dwords 50, 250, 500, by GLOBAL +0x828 */
#define S_SPEED_TEXT (D + 0x108)   /* the descent at touchdown */
#define S_HSCALE_TEXT (D + 0x138)
#define S_LBLOCK    (D + 0x148)    /* the landing dialog's block */
#define S_ZERO_TEXT (D + 0x168)    /* "+0" */
#define S_DRAWN     (D + 0x170)
#define S_MSCALE_TEXT (D + 0x178)  /* the block shows it from +1 */
#define S_MBLOCK    (D + 0x188)    /* the manoeuvre dialog's block */
#define S_OBLOCK    (D + 0x1a0)    /* the options' block and records */
#define S_OTABLE    (D + 0x1e0)    /* three exchange descriptors */
#define S_UNSIGNED  (D + 0x220)    /* the last longitude extent was unsigned */
#define S_CHOICE    (D + 0x224)    /* the options' last selection */
#define S_MIDLAT    (D + 0x228)    /* doubles */
#define S_LATSPAN   (D + 0x230)
#define S_MIDLON    (D + 0x238)
#define S_LONSPAN   (D + 0x240)
#define S_LONM      (D + 0x248)
#define S_MCOUNT    (D + 0x250)
#define S_LATMAX    (D + 0x258)    /* 48-bit at +2 */
#define S_LONA      (D + 0x260)
#define S_LATMIN    (D + 0x268)
#define S_LONB      (D + 0x270)
#define S_MSCALE    (D + 0x278)    /* double */
#define S_REC       (D + 0x280)    /* the graph's window record, 0xa4 */
#define S_VIEW      (D + 0x29c)    /*   its +0x1c, a view block */
#define S_SURFACE   (D + 0x300)    /*   its +0x80 */
#define S_WIDTH     (D + 0x304)    /*   its +0x84, +0x88 */
#define S_HEIGHT    (D + 0x308)
#define S_BITS_SIZE (D + 0x314)    /*   its +0x94, +0x98 */
#define S_BITS      (D + 0x318)
#define S_BASE      (D + 0x324)    /* the last sample's height, 8.16 */
#define S_RANGE     (D + 0x328)    /* first to last sample */

#define RD(o)   MD(R + (o))        /* an .rdata double */

#define W  M32(S_WIDTH)
#define H  M32(S_HEIGHT)

/* ---- registers across calls ------------------------------------------------ */

/* A stdcall call that hands back the ecx the callee left. */
static uint32_t call2_ecx(uint32_t fn, uint32_t a, uint32_t b)
{
    uint32_t ecx;

    __asm__ volatile("pushl %3\n\tpushl %2\n\tcall *%1"
                     : "=c"(ecx) : "r"(fn), "r"(a), "r"(b)
                     : "eax", "edx", "memory", "cc");
    return ecx;
}

static uint32_t call3_ecx(uint32_t fn, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t ecx;

    __asm__ volatile("pushl %4\n\tpushl %3\n\tpushl %2\n\tcall *%1"
                     : "=c"(ecx) : "r"(fn), "r"(a), "r"(b), "r"(c)
                     : "eax", "edx", "memory", "cc");
    return ecx;
}

#define CHAIN_ADD_ECX(list, cb) call2_ecx(M32(CHAIN + 0x18), (list), ADDR(cb))
#define H_MEMSET_ECX(d, b, n)   call3_ecx(M32(HOST + 0x218), (d), (b), (n))

/* ---- x87 ------------------------------------------------------------------- */

/* MSVCRT _ftol: truncate whatever the precision control left, 64 bits. */
static int64_t ftol(long double v)
{
    uint16_t cw, chop;
    int64_t r;

    __asm__ volatile("fnstcw %0" : "=m"(cw));
    chop = cw | 0x0c00;
    __asm__ volatile("fldcw %1\n\tfistpll %0\n\tfldcw %2"
                     : "=m"(r) : "m"(chop), "m"(cw), "t"(v) : "st");
    return r;
}

/* ---- the assembly: .text+0x020..+0x912 ------------------------------------- */

void an_frame(void);
static uint32_t FS98_STDCALL on_command(uint32_t command, uint32_t arg);
static uint32_t FS98_STDCALL on_trail(uint32_t a, uint32_t b);

/* .text+0x889 -- `size` bytes of record `index` from `offset` to dst */
static void sample_out(uint32_t index, uint32_t dst, uint32_t offset, uint32_t size)
{
    H_MEMMOVE(dst, 0x1c * index + M32(S_SAMPLES) + offset, size);
}

/* .text+0x8d1 -- a record to slot `index` */
static void sample_in(uint32_t index, uint32_t src)
{
    H_MEMMOVE(0x1c * index + M32(S_SAMPLES), src, 0x1c);
}

/* .text+0x804 -- forget the run.  Returns ecx as the host's memset leaves
 * it. */
static uint32_t reset(void)
{
    uint32_t ecx;

    H_MEMSET_ECX(S_SCRATCH, 0, 0x1c);
    ecx = H_MEMSET_ECX(M32(S_SAMPLES), 0, 0x6d60);
    GLB32(0x820) = 0;
    M32(S_RESET) = 0;
    M32(S_COUNT) = 0;
    return ecx;
}

/* .text+0x020 -- +0x04.  The allocation is not checked. */
static uint32_t FS98_STDCALL analysis_init(void)
{
    H_ALLOC(1, 0x6d60, S_SAMPLES, 0);
    reset();
    CHAIN_ADD_ECX(5, an_frame);
    return 0;
}

/* .text+0x08e -- +0x08 */
static void FS98_STDCALL analysis_deinit(void)
{
    CHAIN_REMOVE(5, ADDR(an_frame));
    if (M32(S_CMD_ON) != 0) {
        CHAIN_REMOVE(0xc, ADDR(on_command));
        M32(S_CMD_ON) = 0;
    }
    if (M32(S_TRAIL_ON) != 0) {
        CHAIN_REMOVE(0xe, ADDR(on_trail));
        M32(S_TRAIL_ON) = 0;
    }
    if (M32(S_LIST) != 0)
        H_FREE(S_LIST);
    if (M32(S_SAMPLES) != 0)
        H_FREE(S_SAMPLES);
    GLB16(0x1a) &= 0xfdff;
    GLB32(0x814) = 0;
}

/* .text+0x1b2 -- +0x18 */
static void FS98_STDCALL nothing_18(void)
{
}

/* .text+0x1b9 -- +0x1c: a whole record.  Retail keeps eax and edx, so it
 * returns the caller's eax; nothing reads it. */
static void FS98_STDCALL get_record(uint32_t index, uint32_t out)
{
    if (index < 1000)
        sample_out(index, out, 0, 0x1c);
}

/* .text+0x1e2 -- +0x20: its 24 bytes of position */
static void FS98_STDCALL get_position(uint32_t index, uint32_t out)
{
    if (index < 1000)
        sample_out(index, out, 4, 0x18);
}

/* .text+0x207 -- +0x24: its flag */
static void FS98_STDCALL get_flag(uint32_t index, uint32_t out)
{
    if (index < 1000)
        sample_out(index, out, 0, 1);
}

/* .text+0x22c -- +0x28 */
static void FS98_STDCALL put_record(uint32_t index, uint32_t src)
{
    if (index < 1000)
        sample_in(index, src);
}

/* .text+0x24d -- +0x2c */
static uint32_t FS98_STDCALL get_count(void)
{
    return M32(S_COUNT);
}

/* .text+0x259 -- +0x30 */
static void FS98_STDCALL set_count(uint32_t n)
{
    if (n < 1000)
        M32(S_COUNT) = n;
}

/* .text+0x277 -- CHAIN list 0xc, while a manoeuvre runs: its end */
static uint32_t FS98_STDCALL on_command(uint32_t command, uint32_t arg)
{
    if (GLB32(0x814) == 3 && M32(arg) == 0 && command == 0x1002b)
        M32(S_ENDED) = 1;
    return 0;
}

/* .text+0x5cd -- the buffer full: every second sample kept.  The slot is
 * ecx, which host +0x21c's memmove leaves at AN_MEMMOVE_ECX, so each kept
 * record lands there and the count ends one past it.  The manoeuvre's
 * interval doubles, a rol: after 0x80 it is 1 again. */
static void compact(void)
{
    uint32_t ebx;

    for (ebx = 2; ebx < 1000; ebx += 2) {
        sample_out(ebx, S_SCRATCH, 0, 0x1c);
        sample_in(AN_MEMMOVE_ECX, S_SCRATCH);
    }
    M32(S_COUNT) = AN_MEMMOVE_ECX + 1;
    M8(S_INTERVAL) = (uint8_t)((M8(S_INTERVAL) << 1) | (M8(S_INTERVAL) >> 7));
}

/* .text+0x53b -- a sample, if the position moved.  The test is `mov cx,0xc`
 * / `repz cmpsw`: it compares (ecx's top half : 12) words, running past the
 * 24 bytes into GLOBAL +0x578 against the image's +0x44 when the top half
 * is not 0. */
static void sample(uint32_t ecx)
{
    uint32_t n = (ecx & 0xffff0000u) | 0xc, s = G + 0x560, d = S_LAST;
    uint8_t flag;

    for (;;) {
        int same = M16(s) == M16(d);
        s += 2;
        d += 2;
        n--;
        if (!same)
            break;
        if (n == 0)
            return;
    }
    H_MEMMOVE(S_LAST, G + 0x560, 0x18);
    flag = 0x81;
    if (GLB8(0x366) != 0)
        flag |= 2;
    M8(S_RECORD) = flag;
    sample_in(M32(S_COUNT), S_RECORD);
    M32(S_COUNT) = M32(S_COUNT) + 1;
    if (M32(S_COUNT) >= 1000)
        compact();
}

static void landing_dialog(void);
static void manoeuvre_dialog(void);

/* .text+0x2ae -- CHAIN list 5, every frame, ecx as the dispatch left it */
static void __attribute__((used)) an_frame_ecx(uint32_t ecx, uint32_t a, uint32_t b)
{
    uint32_t mode, edx, ebx;
    uint16_t ax;
    uint8_t c, al;

    (void)a;
    (void)b;
    if (M32(S_RESET) != 0 || GLB32(0x820) != 0)
        ecx = reset();
    if (GLB32(0x814) == 0)
        return;
    c = GLB8(0x838);
    if (c != 0 && c <= 4) {
        if (M32(S_COUNT) == 0)
            return;
        if (GLB32(0x814) == 3)
            goto manoeuvre_end;
        if (GLB32(0x814) == 1)
            goto landed;
    }
    if (GLB32(0x268) != 0)
        return;
    mode = GLB32(0x814);
    if (mode == 1) {
        if (GLB8(0x365) != 0)
            return;
        /* the height above the ground: GLOBAL +0x574:+0x572 less +0x20 (24.8),
         * with `sub ah,bl` / `sar ebx,8` / `sbb`: the borrow sbb takes is the
         * sar's carry, bit 7 of the ground's low byte */
        edx = GLB32(0x574);
        ax = GLB16(0x572);
        ebx = GLB32(0x20);
        ax = (uint16_t)((ax & 0xff) | (((ax >> 8) - (ebx & 0xff)) & 0xff) << 8);
        edx = edx - (uint32_t)((int32_t)ebx >> 8) - ((ebx >> 7) & 1);
        if (M8(S_LANDING) > 1)
            goto take;
        if (M8(S_LANDING) == 0) {
            if (edx < 0x22)
                return;
            M8(S_LANDING) = M8(S_LANDING) + 1;
        }
        if (edx > 0xff)
            return;
        if (edx > 0x1e)
            return;
        if (edx == 0x1e && ax > 0x7ae5)
            return;
        M8(S_LANDING) = M8(S_LANDING) + 1;
        ecx = reset();
    } else if (mode == 2) {
        if (GLB32(0x81c) != 0 && M32(S_TRAIL_ON) == 0) {
            ecx = CHAIN_ADD_ECX(0xe, on_trail);
            M32(S_TRAIL_ON) = 1;
        }
        if (GLB32(0x818) == 0)
            return;
    } else if (mode == 3) {
        if (M32(S_CMD_ON) == 0) {
            ecx = CHAIN_ADD_ECX(0xc, on_command);
            M32(S_CMD_ON) = 1;
        }
    }
take:
    M8(S_COUNTDOWN) = M8(S_COUNTDOWN) - 1;
    if ((int8_t)M8(S_COUNTDOWN) >= 0)
        return;
    mode = GLB32(0x814);
    if (mode < 2)
        al = 2;
    else if (mode == 2)
        al = M8(S_FRAMES + GLB32(0x824));
    else
        al = M8(S_INTERVAL);
    M8(S_COUNTDOWN) = al;
    sample(ecx);
    mode = GLB32(0x814);
    if (mode == 2)
        return;
    if (mode < 2) {
        if (GLB32(0x2b8) > 0xc7f)
            return;
        if (GLB8(0x366) == 0)
            return;
        goto landed;
    }
    if (M32(S_ENDED) == 0) {
        al = GLB8(0x838);
        if (al == 0 || al > 4)
            return;
    }
manoeuvre_end:
    M32(S_ENDED) = 0;
    if (M32(S_CMD_ON) != 0) {
        CHAIN_REMOVE(0xc, ADDR(on_command));
        M32(S_CMD_ON) = 0;
    }
    manoeuvre_dialog();
    goto off;
landed:
    M8(S_LANDING) = 0;
    landing_dialog();
    reset();
off:
    GLB32(0x814) = 0;
    GLB16(0x1a) &= 0xfdff;
    reset();
}

/* The Linkage-less entry CHAIN calls: stdcall (a, b), ecx passed on, and b
 * returned as retail returns it. */
__asm__(".text\n"
        "_an_frame:\n\t"
        "pushl 8(%esp)\n\t"
        "pushl 8(%esp)\n\t"
        "pushl %ecx\n\t"
        "call _an_frame_ecx\n\t"
        "addl $12, %esp\n\t"
        "movl 8(%esp), %eax\n\t"
        "ret $8");

/* .text+0x773 -- one segment of the trail, for sample `index`: a 0x2f/0x2e
 * point record with the sample's three 48-bit coordinates, then a 0x0e
 * record numbered by the toggle.  Returns the list's new end. */
static uint32_t segment(uint32_t toggle, uint32_t index, uint32_t p)
{
    uint8_t pos[0x18] __attribute__((aligned(4)));
    uint32_t q = ADDR(pos);

    get_position(index, q);
    M16(p) = 0x2f;
    M16(p + 2) = 0x2e;
    M16(p + 4) = 0;
    M16(p + 6) = 0;
    M16(p + 8) = 0;
    M32(p + 0xa) = 0x200;
    M16(p + 0xe) = M16(q + 2);
    M32(p + 0x10) = M32(q + 4);
    M16(p + 0x14) = M16(q + 0xa);
    M32(p + 0x16) = M32(q + 0xc);
    M16(p + 0x1a) = M16(q + 0x12);
    M32(p + 0x1c) = M32(q + 0x14);
    p += 0x20;
    M16(p) = 0xe;
    M16(p + 2) = (uint16_t)toggle;
    M16(p + 4) = 0;
    M16(p + 6) = 0;
    M16(p + 8) = 0;
    return p + 0xa;
}

/* .text+0x662 -- the trail's display list: 0x76, then from the newest
 * sample back, 0x51 0xf00f and a first segment closed by 0x0f, then
 * segments closed by 0x10, the toggle flipping, for as many segments as
 * .data+0x76[GLOBAL +0x828] says (0 there runs on until the samples do),
 * then 0. */
static void build_list(void)
{
    uint32_t p, n, t, len;

    if (M32(S_LIST) == 0) {
        H_ALLOC(1, 0x59e4, S_LIST, 0);
        H_MEMSET(M32(S_LIST), 0, 0x59e4);
    }
    p = M32(S_LIST);
    M16(p) = 0x76;
    p += 2;
    n = M32(S_COUNT);
    if (n >= 2) {
        n--;
        t = 0;
        len = M32(S_SEGMENTS + 4 * GLB32(0x828));
        M16(p) = 0x51;
        M16(p + 2) = 0xf00f;
        p += 4;
        p = segment(t, n, p);
        M16(p) = 0xf;
        M16(p + 2) = (uint16_t)t;
        p += 4;
        for (;;) {
            n--;
            if ((int32_t)n < 0)
                break;
            len--;
            if (len == 0)
                break;
            t ^= 1;
            p = segment(t, n, p);
            M16(p) = 0x10;
            M16(p + 2) = (uint16_t)t;
            p += 4;
        }
    }
    M16(p) = 0;
}

/* .text+0x60a -- CHAIN list 0xe: the trail drawn, in course tracking */
static uint32_t FS98_STDCALL on_trail(uint32_t a, uint32_t b)
{
    (void)a;
    if (GLB32(0x81c) != 0 && GLB32(0x814) == 2) {
        build_list();
        G3D_LIST(M32(S_LIST));
    }
    return b;
}

/* ---- the dialogs: .text+0x920.. -------------------------------------------- */

/* .text+0x990 */
static uint32_t nothing(void)
{
    return 0;
}

/* .text+0x2250 -- both graph blocks' +0x04 */
static uint32_t FS98_STDCALL block_check(uint32_t block)
{
    (void)block;
    return 0;
}

/* .text+0xae0 -- the control let go, for both graphs */
static uint32_t detach(uint32_t hwnd)
{
    (void)hwnd;
    H_RELEASE(S_REC);
    return H_FREE(S_VIEW) != 0 ? 0xffffffffu : 0;
}

/* .text+0x9e0, +0x25d0 -- the control attached: a record and a view block,
 * the half-extents from the size the host gave it, then painted */
static uint32_t attach(uint32_t hwnd, uint32_t (*paint)(void))
{
    uint32_t view;

    H_MEMSET(S_REC, 0, 0xa4);
    if (H_ALLOC(1, 0xc0, S_VIEW, 1) != 0)
        return 0xffffffffu;
    if (H_ATTACH(hwnd, S_REC) != 0)
        return 0xffffffffu;
    view = M32(S_VIEW);
    M32(view) = (uint32_t)((int32_t)W / 2);
    view = M32(S_VIEW);
    M32(view + 8) = (uint32_t)-((int32_t)H / 2);
    view = M32(S_VIEW);
    M32(view + 4) = (uint32_t)((int32_t)W / 2);
    view = M32(S_VIEW);
    M32(view + 0xc) = (uint32_t)((int32_t)H / 2);
    paint();
    return 0;
}

/* ---- the landing analysis ---- */

/* .text+0x20a0 -- a sample as a point: x from how far along the track it is
 * (its distance back from the last sample, out of the whole range), y from
 * its height over the last sample's.  A zero range divides by zero, which
 * the host's filter repairs to all ones. */
static void l_point(uint32_t s, uint32_t ref, uint32_t out)
{
    uint32_t dist, bearing, scale, ecx, eax, edi, esi;
    int32_t v;

    UTIL_RANGE48(s, ref, 9, ADDR(&dist), ADDR(&bearing), ADDR(&scale));
    ecx = M32(s + 0x14);
    if (ecx & 0x80000000u)
        v = 0;
    else if ((int32_t)ecx > 0x100)
        v = 0x1000000;
    else
        v = (int32_t)(M16(s + 0x12) + ((ecx & 0xff) << 16) - M32(S_BASE));
    v /= 256;
    edi = (uint32_t)v * 839;
    if ((int32_t)edi < 0)
        edi = 0;
    else if ((int32_t)edi > 0x1000000)
        edi = 0x1000000;
    esi = M32(S_RANGE);
    ecx = esi - dist;
    if (ecx > esi)
        ecx = esi;
    eax = ecx * 7692;
    eax = esi != 0 ? eax / esi : 0xffffffffu;
    M32(out) = ((eax + 0xfa) * W) >> 13;
    v = (int32_t)edi / 256;
    v = (int32_t)((uint32_t)v * 20) / 256;
    M32(out + 4) = (uint32_t)((int32_t)((0x160c - (uint32_t)v) * H) / 6144);
    if ((int32_t)M32(out) > (int32_t)W)
        M32(out) = W - 1;
    if ((int32_t)M32(out + 4) > (int32_t)H)
        M32(out + 4) = H - 1;
}

/* .text+0xe00 -- the descent.  The locals are retail's frame: a record at
 * +0x38, the last sample's position at +0x20, the sample being drawn at
 * +0x54, the two before it at +0x6c and +0x84.  A run of one sample gets a
 * second, a touchdown mark (0x83) two units north on the ground.  The
 * first on-ground sample (0x83) gives the descent: the height lost between
 * the two samples before it, times 120, made negative.  A sample with any
 * other flag ends the curve. */
static uint32_t l_curve(void)
{
    uint8_t frame[0x9c] __attribute__((aligned(4)));
    uint32_t b = ADDR(frame), edi = 0, ebx, esi;
    uint32_t v;
    uint8_t al;

    if (H_MEMSET(b + 0x6c, 0, 0x18) != 0)
        return 0xffffffffu;
    if (H_MEMSET(b + 0x84, 0, 0x18) != 0)
        return 0xffffffffu;
    GLB32(0x2ec) = 0xf00f;
    ebx = get_count();
    if (ebx == 1) {
        get_record(0, b + 0x38);
        ebx = 2;
        M8(b + 0x38) = 0x83;
        M32(b + 0x40) = M32(b + 0x40) + 2;
        M32(b + 0x50) = (uint32_t)((int32_t)GLB32(0x20) >> 8);
        M16(b + 0x4e) = (uint16_t)edi;
        put_record(1, b + 0x38);
        set_count(2);
    }
    get_position(ebx - 1, b + 0x20);
    M32(S_BASE) = ((M32(b + 0x34) & 0xff) << 16) + (M32(b + 0x32) & 0xffff);
    get_position(0, b + 0x54);
    UTIL_RANGE48(b + 0x54, b + 0x20, 9, S_RANGE, b + 0x1a, b + 0x03);
    l_point(b + 0x54, b + 0x20, b + 0x10);
    M32(b + 0x1c) = ((M32(S_RANGE) << 8) / 5) >> 8;
    NUM_FORMAT(b + 0x1c, 0x10000, S_HSCALE_TEXT);
    esi = 0;
    M32(b + 4) = esi;
    if (ebx == 0)
        return 0;
    do {
        H_MEMMOVE(b + 0x84, b + 0x6c, 0x18);
        H_MEMMOVE(b + 0x6c, b + 0x54, 0x18);
        get_position(esi, b + 0x54);
        get_flag(esi, b + 0x02);
        al = M8(b + 0x02);
        if (al == 0x83 && edi == 0) {
            v = ((M32(b + 0x98) - M32(b + 0x80)) << 8) - ((M32(b + 0x7e) & 0xffff) >> 8) +
                ((M32(b + 0x96) & 0xffff) >> 8);
            edi = 1;
            v *= 120;
            M32(b + 4) = v;
            if ((int32_t)v > 0)
                M32(b + 4) = 0 - v;
            NUM_FORMAT(b + 4, 0x1000a, S_SPEED_TEXT);
            M32(S_LBLOCK + 0xc) = 1;
            al = M8(b + 0x02);
        }
        if (al == 0x81 || al == 0x83) {
            l_point(b + 0x54, b + 0x20, b + 0x08);
            HG2D_LINE(S_SURFACE, M32(b + 0x10), M32(b + 0x14), M32(b + 0x08), M32(b + 0x0c));
            esi++;
            M32(b + 0x10) = M32(b + 0x08);
            M32(b + 0x14) = M32(b + 0x0c);
        } else {
            esi = ebx + 1;
        }
    } while (esi < ebx);
    return 0;
}

/* .text+0xb10 -- the landing graph's grid, 0xf000: its frame and the
 * glide-path marks, in 8192ths of the width and 6144ths of the height,
 * signed or unsigned as MSVC divided them */
static uint32_t l_grid(void)
{
    uint32_t esi, edi, ebx, ebp;

    GLB32(0x2ec) = 0xf000;
    HG2D_LINE_V(S_SURFACE, (uint32_t)((int32_t)(W * 250) / 8192),
                (uint32_t)((int32_t)(H * 500) / 6144), (uint32_t)((int32_t)(H * 5644) / 6144));
    HG2D_LINE_H(S_SURFACE, (uint32_t)((int32_t)(W * 250) / 8192),
                (uint32_t)((int32_t)(W * 7942) / 8192), (uint32_t)((int32_t)(H * 5644) / 6144));
    HG2D_LINE_H(S_SURFACE, (W * 200) >> 13, (W * 300) >> 13, (H * 3635) / 6144);
    HG2D_LINE_H(S_SURFACE, (W * 200) >> 13, (W * 300) >> 13, (H * 1626) / 6144);
    for (esi = 0x1e0c; esi <= 0x7830; esi += 0x1e0c)
        HG2D_LINE_V(S_SURFACE, ((esi / 5 + 0xfa) * W) >> 13, (H * 5594) / 6144,
                    (H * 5694) / 6144);
    esi = (W * 250) >> 13;
    edi = (W * 1788) >> 13;
    HG2D_LINE_H(S_SURFACE, esi, edi, (uint32_t)((int32_t)(H * 6000) / 6144));
    ebx = (uint32_t)((int32_t)(H * 5950) / 6144);
    ebp = (uint32_t)((int32_t)(H * 6050) / 6144);
    HG2D_LINE_V(S_SURFACE, esi, ebx, ebp);
    HG2D_LINE_V(S_SURFACE, edi, ebx, ebp);
    return 0;
}

/* .text+0xa90 -- the control painted: cleared to 0xff, the grid, the curve */
static uint32_t l_paint(void)
{
    if (H_MEMSET(M32(S_BITS), 0xff, M32(S_BITS_SIZE)) != 0)
        return 0xffffffffu;
    l_grid();
    l_curve();
    H_FLUSH(S_REC);
    return 0;
}

/* .text+0x9a0 -- the landing block's +0x08: 0 attach, 1 paint, 2 let go;
 * anything else 0 */
static uint32_t FS98_STDCALL l_control(uint32_t mode, uint32_t b, uint32_t hwnd)
{
    (void)b;
    switch (mode) {
    case 0:
        return attach(hwnd, l_paint);
    case 1:
        l_paint();
        return 0;
    case 2:
        detach(hwnd);
        return 0;
    default:
        return 0;
    }
}

/* .text+0x960 */
static uint32_t l_prepare(void)
{
    M32(S_LBLOCK + 0xc) = 0;
    H_STRCPY(S_SPEED_TEXT, S_ZERO_TEXT);
    return 0;
}

/* .text+0x920 -- +0x38, the landing analysis */
static void landing_dialog(void)
{
    if (l_prepare() != 0)
        return;
    if (H_DIALOG(0x14050002, S_LBLOCK) == 0 && M32(S_LBLOCK) == 1)
        nothing();
}

static void FS98_STDCALL landing_entry(void)
{
    landing_dialog();
}

/* ---- the manoeuvre analysis ---- */

/* .text+0x22f0 -- a sample into the extents: the highest and lowest
 * latitudes, and the longitudes' two ends, compared unsigned while the
 * whole part is between 0x4000 and 0xc000 (S_UNSIGNED says which way the
 * last one went).  The highest latitude's tie-break compares the whole
 * part with the stored fraction word, not the fraction with it. */
static void extents(uint32_t p)
{
    uint32_t ecx = M32(p + 4);

    if ((int32_t)ecx < (int32_t)M32(S_LATMIN + 4) ||
        (ecx == M32(S_LATMIN + 4) && M16(p + 2) < M16(S_LATMIN + 2))) {
        M32(S_LATMIN + 4) = ecx;
        M16(S_LATMIN + 2) = M16(p + 2);
    }
    ecx = M32(p + 4);
    if ((int32_t)ecx > (int32_t)M32(S_LATMAX + 4) ||
        (ecx == M32(S_LATMAX + 4) && (int32_t)ecx > (int32_t)(uint32_t)M16(S_LATMAX + 2))) {
        M32(S_LATMAX + 4) = ecx;
        M16(S_LATMAX + 2) = M16(p + 2);
    }
    ecx = M32(p + 0xc);
    if (ecx > 0x4000 && ecx < 0xc000) {
        if (ecx > M32(S_LONA + 4) ||
            (ecx == M32(S_LONA + 4) && M16(p + 0xa) > M16(S_LONA + 2))) {
            M32(S_UNSIGNED) = 1;
            M32(S_LONA + 4) = M32(p + 0xc);
            M16(S_LONA + 2) = M16(p + 0xa);
        }
    } else if ((int32_t)ecx > (int32_t)M32(S_LONA + 4) ||
               (ecx == M32(S_LONA + 4) && M16(p + 0xa) > M16(S_LONA + 2))) {
        M32(S_UNSIGNED) = 0;
        M32(S_LONA + 4) = M32(p + 0xc);
        M16(S_LONA + 2) = M16(p + 0xa);
    }
    ecx = M32(p + 0xc);
    if (ecx > 0x4000 && ecx < 0xc000) {
        if (ecx < M32(S_LONB + 4) ||
            (ecx == M32(S_LONB + 4) && M16(p + 0xa) < M16(S_LONB + 2))) {
            M32(S_UNSIGNED) = 1;
            M32(S_LONB + 4) = ecx;
            M16(S_LONB + 2) = M16(p + 0xa);
        }
    } else if ((int32_t)ecx < (int32_t)M32(S_LONB + 4) ||
               (ecx == M32(S_LONB + 4) && M16(p + 0xa) < M16(S_LONB + 2))) {
        M32(S_UNSIGNED) = 0;
        M32(S_LONB + 4) = M32(p + 0xc);
        M16(S_LONB + 2) = M16(p + 0xa);
    }
}

/* .text+0x2460 -- the extents as doubles: spans, midpoints, and the
 * longitude span in metres at the middle latitude.  fst keeps the
 * unrounded difference on the stack for the midpoint. */
static void m_compute(void)
{
    uint8_t frame[0x28] __attribute__((aligned(8)));
    uint32_t b = ADDR(frame);
    long double t;

    M32(S_DRAWN) = 1;
    UTIL_C8(S_LATSPAN, S_LATMAX);
    UTIL_C8(b + 8, S_LATMIN);
    t = (long double)MD(S_LATSPAN) - (long double)MD(b + 8);
    MD(S_LATSPAN) = (double)t;
    MD(S_MIDLAT) = (double)(t * (long double)RD(0x00) + (long double)MD(b + 8));
    if (M32(S_UNSIGNED) == 1) {
        UTIL_C0(S_LONSPAN, S_LONA);
        UTIL_C0(b, S_LONB);
    } else {
        UTIL_C8(S_LONSPAN, S_LONA);
        UTIL_C8(b, S_LONB);
    }
    t = (long double)MD(S_LONSPAN) - (long double)MD(b);
    MD(S_LONSPAN) = (double)t;
    MD(S_MIDLON) = (double)(t * (long double)RD(0x00) + (long double)MD(b));
    UTIL_BC(S_LONSPAN, b + 0x18);
    UTIL_C4(S_MIDLAT, b + 0x10);
    UTIL_LONM(b + 0x18, b + 0x10, b + 0x20);
    UTIL_C0(S_LONM, b + 0x20);
}

/* .text+0x2260 -- the run's extents, from the first sample's.  The loop
 * starts at index -1, which +0x20 refuses, so the first sample is taken
 * twice -- and with no samples at all retail reads its uninitialised frame
 * once; here that frame is zeros (docs/modules/analysis.md). */
static void m_extents(void)
{
    uint8_t pos[0x18] __attribute__((aligned(4))) = { 0 };
    uint32_t q = ADDR(pos), esi = 0;

    M32(S_MCOUNT) = get_count();
    if (M32(S_MCOUNT) != 0) {
        get_position(0, q);
        M32(S_LATMIN + 4) = M32(q + 4);
        M16(S_LATMIN + 2) = M16(q + 2);
        M32(S_LATMAX + 4) = M32(q + 4);
        M16(S_LATMAX + 2) = M16(q + 2);
        M32(S_LONB + 4) = M32(q + 0xc);
        M16(S_LONB + 2) = M16(q + 0xa);
        M32(S_LONA + 4) = M32(q + 0xc);
        M16(S_LONA + 2) = M16(q + 0xa);
    }
    do {
        get_position(esi - 1, q);
        extents(q);
        esi++;
    } while (esi <= M32(S_MCOUNT));
    m_compute();
}

/* .text+0x2b50 -- sample `index` as a point: north and east of the middle,
 * in units of the scale, onto the surface */
static void m_point(uint32_t index, uint32_t out)
{
    uint8_t frame[0x48] __attribute__((aligned(8)));
    uint32_t b = ADDR(frame);
    long double y, x;

    MD(b + 0x20) = (double)((long double)(int32_t)W * (long double)RD(0x28));
    MD(b + 0x28) = (double)((long double)(int32_t)H * (long double)RD(0x30));
    get_position(index, b + 0x30);
    UTIL_C8(b + 0x18, b + 0x30);
    MD(b + 0x18) = (double)((long double)MD(b + 0x18) - (long double)MD(S_MIDLAT));
    if (M32(S_UNSIGNED) == 1)
        UTIL_C0(b, b + 0x38);
    else
        UTIL_C8(b, b + 0x38);
    MD(b) = (double)((long double)MD(b) - (long double)MD(S_MIDLON));
    UTIL_C4(b, b + 8);
    UTIL_LONM(b + 8, b + 0x30, b + 0x10);
    UTIL_C8(b, b + 0x10);
    y = (long double)MD(b) / (long double)MD(S_MSCALE);
    x = (long double)MD(b + 0x18) / (long double)MD(S_MSCALE);
    y = y - (long double)RD(0x38);
    x = (long double)RD(0x40) - x;
    M32(out) = (uint32_t)ftol((long double)MD(b + 0x20) * y);
    M32(out + 4) = (uint32_t)ftol((long double)MD(b + 0x28) * x);
}

/* .text+0x2a30 -- the track, joined in 0xf00f */
static uint32_t m_curve(void)
{
    uint32_t from[2], to[2], i = 0;

    if (M32(S_DRAWN) != 1 || M32(S_MCOUNT) == 0)
        return 0;
    GLB32(0x2ec) = 0xf00f;
    m_point(0, ADDR(from));
    while (i < M32(S_MCOUNT)) {
        m_point(i, ADDR(to));
        HG2D_LINE(S_SURFACE, from[0], from[1], to[0], to[1]);
        i++;
        from[1] = to[1];
        from[0] = to[0];
    }
    return 0;
}

/* .text+0x2ac0 -- the scale: the larger of a tenth of the longitude span
 * and an eighth of the latitude span (that one through a double), formatted
 * as 512 times it -- an 8-byte value whose low dword _ftol replaced -- and
 * kept as 1/710 of it */
static void m_scale(void)
{
    uint8_t q[8] __attribute__((aligned(8)));
    long double a, b, m;
    int64_t e;

    a = (long double)MD(S_LONM) * (long double)RD(0x08);
    b = (long double)MD(S_LATSPAN) * (long double)RD(0x10);
    MD(ADDR(q)) = (double)b;
    if (a > (long double)MD(ADDR(q))) {
        e = ftol(a * (long double)RD(0x18));
        m = a;
    } else {
        e = ftol((long double)MD(ADDR(q)) * (long double)RD(0x18));
        m = (long double)MD(ADDR(q));
    }
    M32(ADDR(q)) = (uint32_t)e;
    MD(S_MSCALE) = (double)(m * (long double)RD(0x20));
    NUM_FORMAT(ADDR(q), 0x1003a, S_MSCALE_TEXT);
    M32(S_MBLOCK + 0x10) = 1;
}

/* .text+0x26d0 -- the manoeuvre graph's grid, 0xf000: a cross through the
 * middle, seven and nine ticks, and a box.  The values MSVC put through
 * fild/_ftol are integers and come back unchanged; those round trips are
 * left out. */
static uint32_t m_grid(void)
{
    uint32_t esi, x1, x2, y1, y2;

    GLB32(0x2ec) = 0xf000;
    HG2D_LINE_V(S_SURFACE, (uint32_t)((int32_t)W / 2), (uint32_t)((int32_t)(H * 232) / 6144),
                (uint32_t)((int32_t)(H * 5912) / 6144));
    HG2D_LINE_H(S_SURFACE, (uint32_t)((int32_t)(W * 546) / 8192),
                (uint32_t)((int32_t)(W * 7646) / 8192), (uint32_t)((int32_t)H / 2));
    x1 = (uint32_t)((int32_t)(W * 4046) / 8192);
    x2 = (uint32_t)((int32_t)(W * 4146) / 8192);
    for (esi = 0x1630; esi <= 0x9b50; esi += 0x1630)
        HG2D_LINE_H(S_SURFACE, x1, x2, (((esi >> 3) + 0xe8) * H) / 6144);
    y1 = (uint32_t)((int32_t)(H * 3022) / 6144);
    y2 = (uint32_t)((int32_t)(H * 3122) / 6144);
    for (esi = 0x1bbc; esi <= 0xf99c; esi += 0x1bbc)
        HG2D_LINE_V(S_SURFACE, ((esi / 10 + 0x222) * W) >> 13, y1, y2);
    x1 = (uint32_t)((int32_t)(W * 546) / 8192);
    x2 = (uint32_t)((int32_t)(W * 1966) / 8192);
    HG2D_LINE_H(S_SURFACE, x1, x2, (uint32_t)((int32_t)(H * 5912) / 6144));
    y1 = (uint32_t)((int32_t)(H * 5862) / 6144);
    y2 = (uint32_t)((int32_t)(H * 5962) / 6144);
    HG2D_LINE_V(S_SURFACE, x1, y1, y2);
    HG2D_LINE_V(S_SURFACE, x2, y1, y2);
    return 0;
}

/* .text+0x2680 */
static uint32_t m_paint(void)
{
    if (H_MEMSET(M32(S_BITS), 0xff, M32(S_BITS_SIZE)) != 0)
        return 0xffffffffu;
    m_scale();
    m_grid();
    m_curve();
    H_FLUSH(S_REC);
    return 0;
}

/* .text+0x2590 -- the manoeuvre block's +0x08 */
static uint32_t FS98_STDCALL m_control(uint32_t mode, uint32_t b, uint32_t hwnd)
{
    (void)b;
    switch (mode) {
    case 0:
        return attach(hwnd, m_paint);
    case 1:
        m_paint();
        return 0;
    case 2:
        detach(hwnd);
        return 0;
    default:
        return 0;
    }
}

/* .text+0x2220 */
static uint32_t m_prepare(void)
{
    M32(S_MBLOCK + 0x10) = 0;
    M32(S_MBLOCK + 0xc) = GLB16(0xc18);
    m_extents();
    return 0;
}

/* .text+0x21e0 -- +0x3c, the manoeuvre analysis */
static void manoeuvre_dialog(void)
{
    if (m_prepare() != 0)
        return;
    if (H_DIALOG(0x14050003, S_MBLOCK) == 0 && M32(S_MBLOCK) == 1)
        nothing();
}

static void FS98_STDCALL manoeuvre_entry(void)
{
    manoeuvre_dialog();
}

/* ---- the options ---- */

/* .text+0x2de0 -- the records filled: on (+0xc), the selection (+0x14: 0
 * landing, 1 manoeuvre, 2 course), the reset (+0x1c), GLOBAL +0x818 and
 * +0x81c (+0x24, +0x2c); the two lists' statuses cleared */
static void o_read(uint32_t p)
{
    switch (GLB32(0x814)) {
    case 0:
        M32(p + 0xc) = 0;
        M32(p + 0x14) = M32(S_CHOICE);
        break;
    case 1:
        M32(p + 0xc) = 1;
        M32(p + 0x14) = 0;
        break;
    case 2:
        M32(p + 0xc) = 1;
        M32(p + 0x14) = 2;
        break;
    case 3:
        M32(p + 0xc) = 1;
        M32(p + 0x14) = 1;
        break;
    }
    M32(p + 0x1c) = 0;
    M32(p + 0x24) = GLB32(0x818);
    M32(p + 0x8) = 0;
    M32(p + 0x2c) = GLB32(0x81c);
    M32(p + 0x10) = 0;
    M32(p + 0x18) = 0;
    M32(p + 0x20) = 0;
    M32(p + 0x28) = 0;
    M32(p + 0x30) = 0;
    M32(p + 0x38) = 0;
}

/* .text+0x2e80 -- the two lists' values above 2 refused (status 0xb) */
static uint32_t o_validate(uint32_t p)
{
    uint32_t r = 0;

    if ((int32_t)M32(p + 0x3c) > 2) {
        M32(p + 0x38) = 0xb;
        r = 1;
    }
    if ((int32_t)M32(p + 0x34) > 2) {
        M32(p + 0x30) = 0xb;
        r = 1;
    }
    return r;
}

/* .text+0x2eb0 -- the records committed.  "Off" clears the mode, and the
 * selection then sets it again: only an unknown selection leaves it off. */
static uint32_t o_commit(uint32_t p)
{
    uint32_t r = 0;

    if (M32(p + 0xc) == 0) {
        GLB32(0x814) = 0;
        GLB16(0x1a) &= 0xfdff;
    }
    switch (M32(p + 0x14)) {
    case 0:
        GLB32(0x814) = 1;
        GLB8(0x1b) |= 2;
        break;
    case 1:
        GLB32(0x814) = 3;
        GLB8(0x1b) |= 2;
        break;
    case 2:
        GLB32(0x814) = 2;
        GLB8(0x1b) |= 2;
        GLB32(0x818) = M32(p + 0x24);
        GLB32(0x81c) = M32(p + 0x2c);
        break;
    default:
        r = 1;
        break;
    }
    if (r == 0)
        GLB32(0x820) = M32(p + 0x1c);
    M32(S_CHOICE) = M32(p + 0x14);
    return r;
}

/* .text+0x2d90 -- the exchange table's callback (kind, base, mode) */
static uint32_t FS98_STDCALL o_exchange(uint32_t kind, uint32_t base, uint32_t mode)
{
    (void)kind;
    switch (mode) {
    case 0:
        o_read(base);
        return 0;
    case 1:
        return o_commit(base);
    case 2:
        return o_validate(base);
    default:
        return 0;
    }
}

/* .text+0x2d70 -- the options block's +0x04: the records validated */
static uint32_t FS98_STDCALL o_check(uint32_t block)
{
    return UTIL_PUT(S_OTABLE, 3, block, 1);
}

/* .text+0x2d40 -- the records written back; -1 if a handler objected */
static uint32_t o_write(void)
{
    uint32_t r = 0;

    if (M32(S_OBLOCK) == 1)
        r = UTIL_PUT(S_OTABLE, 3, S_OBLOCK, 0);
    return r == 1 ? 0xffffffffu : 0;
}

/* .text+0x2ce0 -- +0x34, the options */
static void FS98_STDCALL options_dialog(void)
{
    UTIL_GET(S_OTABLE, 3, S_OBLOCK);
    if (H_DIALOG(0x1405, S_OBLOCK) == 0 && M32(S_OBLOCK) == 1)
        o_write();
}

/* ---- the image -------------------------------------------------------------- */

struct analysis_image analysis_image = {{
#include "analysis_data.inc"
}};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _analysis_image + 0x88\n\t"
        ".globl _Linkage\n\t.set _Linkage, _analysis_image + 0xc8");

static void analysis_install(void)
{
    M32(D + AN_LINKAGE + 0x04) = ADDR(analysis_init);
    M32(D + AN_LINKAGE + 0x08) = ADDR(analysis_deinit);
    M32(D + AN_LINKAGE + 0x18) = ADDR(nothing_18);
    M32(D + AN_LINKAGE + 0x1c) = ADDR(get_record);
    M32(D + AN_LINKAGE + 0x20) = ADDR(get_position);
    M32(D + AN_LINKAGE + 0x24) = ADDR(get_flag);
    M32(D + AN_LINKAGE + 0x28) = ADDR(put_record);
    M32(D + AN_LINKAGE + 0x2c) = ADDR(get_count);
    M32(D + AN_LINKAGE + 0x30) = ADDR(set_count);
    M32(D + AN_LINKAGE + 0x34) = ADDR(options_dialog);
    M32(D + AN_LINKAGE + 0x38) = ADDR(landing_entry);
    M32(D + AN_LINKAGE + 0x3c) = ADDR(manoeuvre_entry);
    M32(S_LBLOCK + 0x04) = ADDR(block_check);
    M32(S_LBLOCK + 0x08) = ADDR(l_control);
    M32(S_LBLOCK + 0x10) = D + 0x108;
    M32(S_LBLOCK + 0x14) = D + 0x118;
    M32(S_LBLOCK + 0x18) = D + 0x128;
    M32(S_LBLOCK + 0x1c) = D + 0x138;
    M32(S_MBLOCK + 0x04) = ADDR(block_check);
    M32(S_MBLOCK + 0x08) = ADDR(m_control);
    M32(S_MBLOCK + 0x14) = S_MSCALE_TEXT + 1;
    M32(S_OBLOCK + 0x04) = ADDR(o_check);
    M32(S_OTABLE + 0x0c) = ADDR(o_exchange);
}

/* .text+0x000 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        analysis_install();
    }
    return TRUE;
}
