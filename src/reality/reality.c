/* REALITY.DLL -- module id 32: crashes, damage, and the crash analysis.
 *
 * Two frame callbacks.  CHAIN list 1 watches GLOBAL +0x832, the crash code
 * MAIN sets: on a crash it puts the reason up in WINMSG's slot 5, pauses,
 * plays the crash (SOUND event 0xe), turns a cockpit view to the spot plane,
 * greys the menu, and starts a countdown in GLOBAL +0x838.  CHAIN list 3
 * runs the countdown: at 3 the flight is reloaded (MAIN +0x44), at 2 the
 * crash-analysis dialog may run, at 1 the sim goes on (or a command is
 * posted).  List 3 also rolls the dice for stress damage from UTIL's
 * generator, setting GLOBAL's damage flags at +0xb64..+0xb73.  +0x18 clears
 * those flags; +0x1c, from WINDOW's frame, draws the cracked windscreen into
 * a cockpit view.
 *
 * The crash-analysis dialog (host +0x10c, 0x14050001) shows the descent rate
 * and a graph of the last 22 seconds' height above the ground, read back from
 * DEMO's replay ring, drawn into a dialog control through HG2D's lines.
 *
 * Retail is two programs, like WINDOW's: hand-written assembly from
 * .text+0x20 to +0x8a4, with ATC's stack-alignment wrapper around every call
 * (no observable effect, so not reproduced), and MSVC C from +0xa50.  The
 * assembly keeps GLOBAL in edi and passes values in registers; where that
 * shows, the comments say so.  The whole 0x510-byte .data image is kept at
 * retail's offsets, and .rdata is carried whole because the scale search
 * reads it without a bound.  docs/modules/reality.md has the details.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define FS98_DATA_IMAGE  reality_image
#define FS98_RDATA_IMAGE reality_rdata
#include "fs98/arena.h"

#define RE_IMAGE_SIZE 0x510
#define RE_IMPORTS    0x048
#define RE_LINKAGE    0x0b0

struct reality_image {
    uint32_t jump[8];                           /* +0x00 code pointers, installed */
    uint8_t  state[0x28];                       /* +0x20 */
    struct { uint32_t id, link; } import[13];   /* +0x48 */
    uint32_t linkage[8];                        /* +0xb0 */
    uint16_t crack[0x4e][2];                    /* +0xd0 */
    uint32_t rest[(RE_IMAGE_SIZE - 0x208) / 8][2];   /* +0x208: meaning not yet known */
} __attribute__((packed));
_Static_assert(sizeof(struct reality_image) == RE_IMAGE_SIZE, "image");
_Static_assert(__builtin_offsetof(struct reality_image, import) == RE_IMPORTS, "import");
_Static_assert(__builtin_offsetof(struct reality_image, linkage) == 0xb0, "linkage");
_Static_assert(__builtin_offsetof(struct reality_image, crack) == 0xd0, "crack");
_Static_assert(__builtin_offsetof(struct reality_image, rest) == 0x208, "rest");

extern struct reality_image reality_image;

struct reality_rdata {
    uint32_t feet[12][2];                       /* +0x00 range, step */
    uint32_t metric[12][2];                     /* +0x60 */
    double   k[15];                             /* +0xc0 */
    uint32_t export_dir[25];                    /* +0x138: retail's export directory; the scale search reads on into it */
    uint8_t  zero[0x1000 - 0x19c];
} __attribute__((packed));
_Static_assert(sizeof(struct reality_rdata) == 0x1000, "rdata");
_Static_assert(__builtin_offsetof(struct reality_rdata, metric) == 0x60, "metric");
_Static_assert(__builtin_offsetof(struct reality_rdata, k) == 0xc0, "k");
_Static_assert(__builtin_offsetof(struct reality_rdata, export_dir) == 0x138, "export_dir");
static const struct reality_rdata reality_rdata __attribute__((aligned(8))) = {
    .feet = {
        {0x3, 0x2},
        {0x7, 0x5},
        {0xf, 0xa},
        {0x1e, 0x14},
        {0x4b, 0x32},
        {0x96, 0x64},
        {0x12c, 0xc8},
        {0x2ee, 0x1f4},
        {0x5dc, 0x3e8},
        {0xbb8, 0x7d0},
        {0x1d4c, 0x1388},
        {0xffff, 0x2710},
    },
    .metric = {
        {0xa, 0x2},
        {0x19, 0x5},
        {0x32, 0xa},
        {0x64, 0x14},
        {0xfa, 0x32},
        {0x1f4, 0x64},
        {0x3e8, 0xc8},
        {0x9c4, 0x1f4},
        {0x1388, 0x3e8},
        {0x2710, 0x7d0},
        {0x61a8, 0x1388},
        {0xff37, 0x2710},
    },
    .k = {
        0x1.381d897c00000p-2,
        0x1.0000000000000p-16,
        -0x1.6000000000000p+4,
        0x1.0000000000000p+1,
        0x1.4000000000000p+2,
        0x1.8624ebda79eafp+0,
        0x0.0p+0,
        0x1.0000000000000p+0,
        0x1.5555555555555p-5,
        0x1.7c20000000000p+12,
        0x1.7e10000000000p+12,
        -0x1.fae0000000000p+12,
        0x1.4800000000000p+5,
        0x1.0000000000000p-13,
        0x1.5555555555555p-13,
    },
    .export_dir = {
        0x0, 0x0, 0x0, 0x33e94d61, 0x0,
        0x317c, 0x1, 0x2, 0x2, 0x3168,
        0x3170, 0x3178, 0x4048, 0x40b0, 0x3188,
        0x3194, 0x10000, 0x4c414552, 0x2e595449, 0x4c4c44,
        0x6f706d49, 0x61547472, 0x656c62, 0x6b6e694c, 0x656761,
    },
};

#define IMPORT(i)  M32(D + RE_IMPORTS + 8 * (i) + 4)
#define HOST       IMPORT(0)
#define G          IMPORT(1)
#define CHAIN      IMPORT(2)
#define CONTROLS   IMPORT(3)
#define DEMO       IMPORT(4)
#define HG2D       IMPORT(5)
#define MAIN       IMPORT(6)
#define NUMERIC    IMPORT(7)
#define SOUND      IMPORT(8)
#define UTIL       IMPORT(9)
#define WINDOW     IMPORT(10)
#define WINMSG     IMPORT(11)

typedef uint32_t (FS98_STDCALL *f0)(void);
typedef uint32_t (FS98_STDCALL *f1)(uint32_t);
typedef uint32_t (FS98_STDCALL *f2)(uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f3)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f4)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint32_t (FS98_STDCALL *f5)(uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t);

#define CALL(m, off, fn) (*(fn *)(uintptr_t)((m) + (off)))
#define HOST_(off, fn)   CALL(HOST, off, fn)

#define H_POST        HOST_(0x03c, f2)   /* (command, parameter) */
#define H_ATTACH      HOST_(0x074, f2)   /* (hwnd, rec): a dialog control's surface */
#define H_RELEASE     HOST_(0x078, f1)   /* (rec) */
#define H_FLUSH       HOST_(0x084, f1)   /* (rec) */
#define H_DIALOG      HOST_(0x10c, f2)   /* (id, block) */
#define H_ALLOC       HOST_(0x20c, f4)   /* (flags, size, &out, zero) */
#define H_FREE        HOST_(0x214, f1)   /* (&p) */
#define H_MEMSET      HOST_(0x218, f3)   /* (dst, byte, n) */
#define H_LOADSTRING  HOST_(0x2a0, f3)   /* (id, buf, len) */
#define H_STRCPY      HOST_(0x2a8, f2)
#define H_LIST_ADD    HOST_(0x2f8, f1)   /* (list): a new zeroed element */
#define H_LIST_EMPTY  HOST_(0x304, f1)   /* (list) */

#define CHAIN_ADD     CALL(CHAIN, 0x18, f2)    /* (list, callback) */
#define CHAIN_REMOVE  CALL(CHAIN, 0x1c, f2)
#define CONTROLS_PAUSE CALL(CONTROLS, 0x5c, f2) /* (8, on) */
#define DEMO_BACK     CALL(DEMO, 0x3c, f2)     /* (index < 0, out) */
#define HG2D_LINE_H   CALL(HG2D, 0x2c, f4)     /* (&surface, x1, x2, y) */
#define HG2D_LINE_V   CALL(HG2D, 0x30, f4)     /* (&surface, x, y1, y2) */
#define HG2D_LINE     CALL(HG2D, 0x34, f5)     /* (&surface, x1, y1, x2, y2) */
#define MAIN_GREY     CALL(MAIN, 0x30, f0)     /* the menu's items 0x1001..6 off */
#define MAIN_UNGREY   CALL(MAIN, 0x34, f0)
#define MAIN_RELOAD   CALL(MAIN, 0x44, f0)
#define NUM_FORMAT    CALL(NUMERIC, 0x18, f3)  /* (&value, format, out) */
#define SOUND_EVENT   CALL(SOUND, 0x24, f2)    /* (code, arg) */
#define UTIL_RAND     CALL(UTIL, 0x6c, f0)
#define UTIL_RAND32   CALL(UTIL, 0x70, f0)
#define UTIL_EXCHANGE CALL(UTIL, 0x90, f3)     /* (table, n, records) */
#define UTIL_FIXED48  CALL(UTIL, 0xc8, f2)     /* (&double, &48-bit) */
#define WINDOW_MODE   CALL(WINDOW, 0x58, f2)   /* (rec, mode) */
#define WINMSG_SHOW   CALL(WINMSG, 0x18, f4)   /* (text, slot, time, append) */

#define GLB8(o)  M8(G + (o))
#define GLB16(o) M16(G + (o))
#define GLB32(o) M32(G + (o))

/* ---- the state, at retail's offsets (D + offset) ------------------------- */

#define S_DAMAGE    (D + 0x000)    /* eight routines, by GLOBAL +0x258 & 0xe */
#define S_POSITION  (D + 0x020)    /* GLOBAL +0x624 at the crash */
#define S_CODE      (D + 0x024)    /* word: GLOBAL +0x830 at the reload */
#define S_HELD      (D + 0x026)    /* word: GLOBAL +0x846 as the reload left it */
#define S_STRESS    (D + 0x028)    /* frames to the next roll */
#define S_PLAYING   (D + 0x02c)    /* the crash is still sounding */
#define S_SOUND     (D + 0x030)    /* SOUND 0xe's block: flags, code, -, callback, splash */
#define S_CRACK     (D + 0x0d0)    /* the crack: x, y words; y bit 15 a move; x < 0 ends */
#define S_XLIMIT    (D + 0x20c)
#define S_YLIMIT    (D + 0x210)
#define S_RATE_TEXT (D + 0x218)    /* "-" then the descent rate */
#define S_BLOCK     (D + 0x228)    /* the dialog's block */
#define S_LABELLED  (D + 0x254)
#define S_GRAPH     (D + 0x258)    /* x from, x to, y from, y to, points, count */
#define S_EXCHANGE  (D + 0x280)    /* three descriptors */
#define S_SAMPLES   (D + 0x2c0)    /* host array of 0x18-byte replay entries */
#define S_POINTS    (D + 0x2e0)    /* host array of 0x10-byte (x, y) doubles */
#define S_DESCENT   (D + 0x334)    /* word: metres a second, for metric */
#define S_YLABELS   (D + 0x338)    /* 11 of 8 bytes */
#define S_XLABELS   (D + 0x390)    /* 13 of 8 bytes */
#define S_EMPTY     (D + 0x434)    /* "" */
#define S_MIN       (D + 0x438)    /* double */
#define S_REC       (D + 0x440)    /* the graph's window record, 0xa4 */
#define S_VIEW      (D + 0x45c)    /*   its +0x1c, a view block */
#define S_SURFACE   (D + 0x4c0)    /*   its +0x80 */
#define S_WIDTH     (D + 0x4c4)    /*   its +0x84, +0x88 */
#define S_HEIGHT    (D + 0x4c8)
#define S_BITS_SIZE (D + 0x4d4)    /*   its +0x94, +0x98 */
#define S_BITS      (D + 0x4d8)
#define S_MAX       (D + 0x4e8)    /* double */
#define S_SCALE     (D + 0x4f0)    /* double: height a grid step */
#define S_HIGHEST   (D + 0x4f8)    /* 48-bit at +2 */
#define S_STEP      (D + 0x500)
#define S_LOWEST    (D + 0x508)    /* 48-bit at +2 */

#define RE_RD(o)   MD(RDATA + (o))     /* an .rdata double */

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

/* ---- the assembly: .text+0x020..+0x8a4 ------------------------------------- */

static uint32_t FS98_STDCALL on_list1(uint32_t a, uint32_t b);
static uint32_t FS98_STDCALL on_list3(uint32_t a, uint32_t b);

/* .text+0x020 -- +0x04 */
static uint32_t FS98_STDCALL reality_init(void)
{
    CHAIN_ADD(1, fs98_handle(on_list1));
    return CHAIN_ADD(3, fs98_handle(on_list3));
}

/* .text+0x079 -- +0x08 */
static uint32_t FS98_STDCALL reality_deinit(void)
{
    CHAIN_REMOVE(1, fs98_handle(on_list1));
    return CHAIN_REMOVE(3, fs98_handle(on_list3));
}

/* .text+0x2bd -- a damage flag set, unless it already says something */
static void damage(uint32_t flag)
{
    if (M8(flag) == 0)
        M8(flag) = 1;
}

/* .text+0x265..+0x2bc -- the eight damages, ebx the flag */
static void damage_0(void) { damage(G + 0xb65); }
static void damage_1(void) { GLB8(0x364) |= 3; }
static void damage_2(void) { damage(G + 0xb6d); }
static void damage_3(void) { damage(G + 0xb66); }
static void damage_4(void) { GLB8(0x364) |= 0xc; }
static void damage_5(void) { damage(G + 0xb70); }
static void damage_6(void) { damage(G + 0xb71); }
static void damage_7(void) { damage(G + 0xb73); }

/* .text+0x1f1 -- stress damage: string 0x276 for 0x6c frames, then one of
 * the eight, through the table by GLOBAL +0x258 & 0xe (its even values,
 * read as dword indices times two). */
static void stress_damage(void)
{
    char text[0x40];

    H_LOADSTRING(0x276, fs98_handle(text), 0x40);
    WINMSG_SHOW(fs98_handle(text), 5, 0x6c, 0);
    ((void (*)(void))(uintptr_t)M32(S_DAMAGE + (GLB32(0x258) & 0xe) * 2))();
}

/* .text+0x1a8 -- every 0x79 frames a roll of UTIL's generator: under 100
 * and at least GLOBAL +0x372, damage. */
static void stress_roll(void)
{
    uint32_t r;

    M32(S_STRESS) = M32(S_STRESS) - 1;
    if ((int32_t)M32(S_STRESS) >= 0)
        return;
    M32(S_STRESS) = 0x78;
    r = UTIL_RAND() & 0xffff00ffu;      /* xor ah, ah */
    if ((uint8_t)r >= 0x64)
        return;
    if ((uint16_t)r < GLB16(0x372))
        return;
    stress_damage();
}

/* .text+0x144 -- not while the sim is in slew-like state +0x5dc.  Retail
 * then compares GLOBAL +0xc44 with 0x32 and goes the same way either side:
 * left out. */
static void stress(void)
{
    if (GLB16(0x5dc) != 0)
        return;
    stress_roll();
    if ((GLB16(0xc46) & 8) && GLB8(0x280) != 0) {
        if (UTIL_RAND32() <= 0xd1b71)
            GLB8(0x374) = 1;
    }
}

/* .text+0x2c6 -- the crash sound's callback: done, unpause */
static void FS98_STDCALL sound_done(void)
{
    M32(S_PLAYING) = 0;
    CONTROLS_PAUSE(8, 0);
}

/* .text+0x2f7 -- list 1: the crash, if MAIN says there was one (GLOBAL
 * +0x832, read as its low byte); otherwise the descent rate kept for the
 * analysis. */
static void crash_check(void)
{
    char text[0x40];
    uint32_t code, rec;
    int32_t v;

    code = GLB8(0x832);
    if (code == 0)
        goto steady;
    if (GLB32(0x760) == 0 && GLB16(0x18) == 5)
        goto done;
    if (GLB8(0x365) != 0 || GLB32(0x270) != 0 || GLB8(0x838) != 0)
        goto done;
    if (GLB32(0x83c) != 0)
        goto steady;
    if (GLB16(0x830) == 0)
        goto done;
    M32(S_POSITION) = GLB32(0x624);
    if (code == 0xe)
        code = 6;
    if ((code == 0x10 || code == 0x12 || code == 0x14) && GLB16(0x84a) == 0)
        goto done;
    if ((code & 1) || code > 0x14)
        code = 4;
    GLB32(0x834) = code;
    M32(S_SOUND + 4) = code;
    M32(S_SOUND) = 1;
    H_LOADSTRING(code + 0x258, fs98_handle(text), 0x40);
    WINMSG_SHOW(fs98_handle(text), 5, 0x3c, 0);
    CONTROLS_PAUSE(8, 1);
    if (GLB32(0xb24) != 0) {
        M32(S_PLAYING) = 1;
        M32(S_SOUND) |= 0x10;
        M32(S_SOUND + 0xc) = fs98_handle(sound_done);
        M32(S_SOUND) |= 4;
    }
    M32(S_SOUND + 0x10) = 1;
    M32(S_SOUND) |= 8;
    if (SOUND_EVENT(0xe, S_SOUND) != 0)
        M32(S_PLAYING) = 0;
    GLB8(0x838) = 5;
    GLB8(0x840) = 1;
    if (GLB16(0x846) != 0 && GLB32(0x82c) != 0) {
        rec = GLB32(0x1194);
        if (rec != 0 && M32(rec + 4) == 3 && M32(GLB32(0x1198) + 0x10) == 0) {
            WINDOW_MODE(GLB32(0x1194), 4);
            GLB8(0x838) = 7;
        }
    }
    MAIN_GREY();
done:
    GLB16(0x832) = 0;
    return;

steady:
    if (GLB8(0x838) != 0)
        return;
    v = (int32_t)GLB32(0x2c8);
    /* RETAIL BUG (kept): REAL-7 -- the upper clamp replaces only ax (mov ax), so a rate over 0xa674 keeps its
     * high word. */
    if (v > 0xa674)
        v = (int32_t)(((uint32_t)v & 0xffff0000u) | 0xa674);   /* mov ax */
    if (v < -0xa674)
        v = -0xa674;
    GLB16(0x842) = (uint16_t)((int64_t)v * (int32_t)0xc4d9b366 >> 32);
    GLB16(0x844) = (uint16_t)(GLB32(0x2b8) >> 7);
}

/* .text+0x0d2 -- CHAIN list 1 */
static uint32_t FS98_STDCALL on_list1(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    crash_check();
    return 0;
}

/* .text+0x594 -- a hold (GLOBAL +0x83c) counted down; while it lasts no
 * crash, and at its end a crash code 8 forgets +0x830. */
static void hold(void)
{
    int32_t n = (int32_t)(GLB32(0x83c) - 1);

    if (n < 0)
        return;
    GLB32(0x83c) = (uint32_t)n;
    if (n != 0)
        GLB16(0x832) = 0;
    else if (GLB16(0x832) == 8)
        GLB16(0x830) = 0;
}

static uint32_t analysis(void);

/* .text+0x5c4 -- the crash's countdown in GLOBAL +0x838, a byte.  ah holds
 * the count before the step; the dialog at 2 returns in eax, so the test for
 * 1 that follows reads its second byte. */
static void countdown(void)
{
    uint8_t al = GLB8(0x838), ah = al;
    uint16_t held;

    al--;
    if ((int8_t)al < 0)
        return;
    if (ah == 6 && GLB16(0x846) != 0)
        return;
    if (ah == 3 && M32(S_PLAYING) != 0)
        return;
    GLB8(0x838) = al;
    if (ah == 4) {
        CONTROLS_PAUSE(8, 0);
        return;
    }
    if (ah == 3) {
        GLB8(0x840) = 0;
        M16(S_CODE) = GLB16(0x830);
        held = GLB16(0x846);
        MAIN_RELOAD();
        M16(S_HELD) = GLB16(0x846);
        GLB16(0x846) = held;
        return;
    }
    if (ah == 2 && M16(S_CODE) == 2)
        ah = (uint8_t)(analysis() >> 8);   /* RETAIL BUG (kept): REAL-1 -- the test for 1 below reads the dialog's result's second byte */
    if (ah != 1)
        return;
    GLB16(0x832) = 0;
    GLB16(0x846) = M16(S_HELD);
    M32(S_SOUND + 0x10) = 0;
    M32(S_SOUND) = 8;
    SOUND_EVENT(0xe, S_SOUND);
    MAIN_UNGREY();
    if (GLB16(0x5dc) != 0)
        H_POST(0x100c3, 0);
    else
        H_POST(0x100c2, 0);
}

/* .text+0x0ea -- CHAIN list 3 */
static uint32_t FS98_STDCALL on_list3(uint32_t a, uint32_t b)
{
    (void)a;
    (void)b;
    hold();
    countdown();
    stress();
    if (GLB16(0x344) != 0 && GLB32(0x2b4) <= 0x7fff && GLB16(0x5dc) == 0)
        GLB32(0x348) = GLB32(0x348) + 1;
    else
        GLB32(0x348) = 0;
    return 0;
}

/* .text+0x78a -- +0x18: the damage flags that say 1 cleared */
static void FS98_STDCALL clear_damage(void)
{
    static const uint16_t flags[] = {
        0xb64, 0xb65, 0xb66, 0xb67, 0xb68, 0xb69, 0xb6a, 0xb6b, 0xb6c, 0xb6d,
        0xb70, 0xb71, 0xb72, 0xb73, 0xb6e, 0xb6f,
    };
    uint32_t i;

    GLB8(0x374) = 0;
    GLB8(0x364) = 0;
    for (i = 0; i < sizeof flags / sizeof flags[0]; i++)
        if (GLB8(flags[i]) == 1)
            GLB8(flags[i]) = 0;
}

/* .text+0xa22 -- a crack point (ebx, ecx) in 1/256ths onto the view:
 * x * width / 256 from the left, y * height / 256 from the top, each
 * product's low 32 bits. */
static void crack_place(uint32_t *x, uint32_t *y)
{
    uint32_t view = GLB32(0x1198);

    *x = (M32(view) * 2 * *x >> 8) + M32(view + 4) - M32(view);
    *y = ((0 - M32(view + 8)) * 2 * *y >> 8) + M32(view + 0xc) + M32(view + 8);
}

/* .text+0x9f4, +0xa0b -- 0 to the limit, as unsigned after a sign test */
static uint32_t clamp(uint32_t v, uint32_t limit)
{
    if ((int32_t)v < 0)
        v = 0;
    if (v >= limit)
        v = limit;
    return v;
}

/* .text+0x988 -- a line (esi, edi)-(ebx, ecx) into the current window */
static void crack_line(uint32_t x2, uint32_t y2, uint32_t x1, uint32_t y1)
{
    uint32_t s = GLB32(0x1194) + 0x80;

    x2 = clamp(x2, M32(S_XLIMIT));
    y2 = clamp(y2, M32(S_YLIMIT));
    x1 = clamp(x1, M32(S_XLIMIT));
    y1 = clamp(y1, M32(S_YLIMIT));
    HG2D_LINE(s, x1, y1, x2, y2);
}

/* .text+0x8b0 -- +0x1c: the cracked windscreen, in a cockpit view.  Each
 * stroke is drawn four times a pixel off in colour 0, then once in 0xf. */
static void FS98_STDCALL crack(void)
{
    uint32_t view, p = S_CRACK, x, y, px = 0, py = 0;
    int32_t sx, sy;

    GLB16(0xcc2) = 0xffff;
    view = GLB32(0x1198);
    if (M32(view + 0x14) != 0 || M32(view + 0x10) == 1)
        return;
    M32(S_XLIMIT) = M32(view) * 2 - 1;
    M32(S_YLIMIT) = (0 - M32(view + 8) * 2) - 1;
    for (;;) {
        sx = (int16_t)M16(p);
        sy = (int16_t)M16(p + 2);
        p += 4;
        if (sx < 0)
            return;
        x = (uint32_t)sx;
        y = (uint32_t)sy;
        if (sy < 0) {
            y &= 0x7fff;
            crack_place(&x, &y);
            px = x;
            py = y;
            continue;
        }
        crack_place(&x, &y);
        GLB32(0x2ec) = 0;
        crack_line(x - 1, y, px - 1, py);
        crack_line(x + 1, y, px + 1, py);
        crack_line(x, y - 1, px, py - 1);
        crack_line(x, y + 1, px, py + 1);
        GLB32(0x2ec) = 0xf;
        crack_line(x, y, px, py);
        px = x;
        py = y;
    }
}

/* ---- the crash analysis: .text+0xa50.. -------------------------------------- */

/* .text+0xb20 -- the replay ring's last 22 seconds and the next two: 25
 * entries back from DEMO (the last three ask for indices it refuses), the
 * highest and lowest heights, and each as an (x, y) point.  x is the index
 * read as 64 bits with a zero top half, so the points' x is near 2^32. */
static void collect(void)
{
    uint32_t i, e, pt, index;

    H_LIST_EMPTY(S_SAMPLES);
    M32(S_LOWEST + 4) = 0x10000;
    M16(S_LOWEST + 2) = 0;
    M32(S_HIGHEST + 4) = 0;
    M16(S_HIGHEST + 2) = 0;
    for (i = 0; i <= 0x18; i++) {
        e = H_LIST_ADD(S_SAMPLES);
        index = i - 0x16;
        DEMO_BACK(index, e);
        if ((int32_t)M32(e + 0x14) > (int32_t)M32(S_HIGHEST + 4) ||
            (M32(e + 0x14) == M32(S_HIGHEST + 4) && M16(e + 0x12) > M16(S_HIGHEST + 2))) {
            M32(S_HIGHEST + 4) = M32(e + 0x14);
            M16(S_HIGHEST + 2) = M16(e + 0x12);
        }
        if ((int32_t)M32(e + 0x14) < (int32_t)M32(S_LOWEST + 4) ||
            (M32(e + 0x14) == M32(S_LOWEST + 4) && M16(e + 0x12) < M16(S_LOWEST + 2))) {
            M32(S_LOWEST + 4) = M32(e + 0x14);
            M16(S_LOWEST + 2) = M16(e + 0x12);
        }
        pt = H_LIST_ADD(S_POINTS);
        /* RETAIL BUG (kept): REAL-5 -- x is the index read as 64 bits with a zero top half (-22 is
         * 4,294,967,274); nothing reads it but the host's array. */
        MD(pt) = (double)(long double)(uint64_t)index;
        MD(pt + 8) = (double)((long double)(int32_t)M32(e + 0x14) +
                              (long double)(int32_t)M16(e + 0x12) * (long double)RE_RD(0xc8));
    }
}

/* .text+0xc20 -- the heights as doubles, and the graph's ranges */
static void bounds(void)
{
    UTIL_FIXED48(S_MIN, S_LOWEST);
    UTIL_FIXED48(S_MAX, S_HIGHEST);
    M32(S_GRAPH + 0x10) = M32(S_MIN);
    M32(S_GRAPH + 0x14) = M32(S_MIN + 4);
    M32(S_GRAPH + 0x18) = M32(S_MAX);
    M32(S_GRAPH + 0x00) = 0;
    M32(S_GRAPH + 0x04) = 0xc0360000;   /* -22.0 */
    M32(S_GRAPH + 0x08) = 0;
    M32(S_GRAPH + 0x0c) = 0x40000000;   /* 2.0 */
    M32(S_GRAPH + 0x1c) = M32(S_MAX + 4);
    M32(S_GRAPH + 0x20) = M32(S_POINTS + 0x18);
    M32(S_GRAPH + 0x24) = M32(S_POINTS);
}

/* .text+0xaa0 -- the dialog's contents */
static uint32_t prepare(void)
{
    M32(S_LABELLED) = 0;
    M16(S_DESCENT) = (uint16_t)ftol((long double)(int16_t)GLB16(0x842) * (long double)RE_RD(0xc0));
    collect();
    UTIL_EXCHANGE(S_EXCHANGE, 3, S_BLOCK);
    bounds();
    return 0;
}

/* .text+0xb00 */
static uint32_t nothing(void)
{
    return 0;
}

/* .text+0xa50 -- the crash analysis: the dialog, then the points let go.
 * A dialog that returns 0 having set its block's first word to 1 makes
 * nothing of it. */
static uint32_t analysis(void)
{
    uint32_t r = prepare();

    if (r == 0) {
        r = H_DIALOG(0x14050001, S_BLOCK);
        if (r == 0 && M32(S_BLOCK) == 1)
            r = nothing();
    }
    H_LIST_EMPTY(S_POINTS);
    return r;
}

/* .text+0xb10 -- the block's +0x04 */
static uint32_t FS98_STDCALL block_check(uint32_t block)
{
    (void)block;
    return 0;
}

/* .text+0xcc0 -- the exchange's text callback: the descent rate, in feet a
 * minute or (units 1, 2) metres a second.  Only on a read. */
static uint32_t FS98_STDCALL rate_text(uint32_t a, uint32_t b, uint32_t mode)
{
    uint32_t units;

    (void)a;
    (void)b;
    if (mode != 0)
        return 0;
    units = GLB16(0xc18);
    if (units == 0) {
        NUM_FORMAT(G + 0x842, 0x10000, S_RATE_TEXT + 1);
        return 0;
    }
    if ((int32_t)units > 0 && (int32_t)units <= 2)
        NUM_FORMAT(S_DESCENT, 0x10000, S_RATE_TEXT + 1);
    return 0;
}

/* .text+0x2210 -- sample `index` at x as a point on the surface */
static void plot(uint32_t index, int32_t x, uint32_t out)
{
    double h;
    long double a, xx, t, u;

    UTIL_FIXED48(fs98_handle(&h), M32(S_SAMPLES + 0x18) + index * 0x18 + 0x10);
    a = (long double)h - (long double)MD(S_MIN);
    xx = (long double)x;
    a = a / (long double)MD(S_SCALE);
    xx = xx - (long double)RE_RD(0xd0);
    a = a * (long double)RE_RD(0x108);
    xx = xx * (long double)RE_RD(0x100);
    a = (long double)RE_RD(0x110) - a;
    xx = xx * (long double)RE_RD(0x118);
    xx = (long double)RE_RD(0x120) - xx;
    t = xx * (long double)(int32_t)M32(S_WIDTH);
    t = t * (long double)RE_RD(0x128);
    M32(out) = (uint32_t)ftol(t);
    u = (long double)(int32_t)M32(S_HEIGHT) * a;
    u = u * (long double)RE_RD(0x130);
    M32(out + 4) = (uint32_t)ftol(u);
}

/* .text+0x2150 -- the axes' labels: thirteen seconds -22 to 2 (NUMERIC 0xf
 * from the 8-byte local's low dword), then eleven heights, every other one
 * blank, from 0 in steps of the scale's (0x1b, from its high dword). */
static void labels(void)
{
    volatile struct { int32_t lo, hi; } v = { -0x16, 0 };
    uint32_t i, p;

    for (i = 0, p = S_XLABELS; p < S_XLABELS + 13 * 8; p += 8, i += 4) {
        NUM_FORMAT(fs98_handle(&v), 0x1000f, p);
        M32(M32(D + 0x248) + i) = p;
        v.lo += 2;
    }
    /* RETAIL BUG (kept): REAL-6 -- the heights' labels start at 0 from the high dword of the 8-byte local whose
     * low dword the seconds' labels just used, and go up by the scale's step. */
    for (i = 0, p = S_YLABELS; p < S_YLABELS + 11 * 8; p += 8, i++) {
        if ((i & 1) == 1) {
            H_STRCPY(p, S_EMPTY);
        } else {
            NUM_FORMAT(fs98_handle(&v.hi), 0x1001b, p);
            v.hi += (int32_t)M32(S_STEP);
        }
        M32(M32(D + 0x250) + 4 * i) = p;
    }
    M32(S_LABELLED) = 1;
}

/* .text+0x2020 -- the height scale: the first table entry whose limit the
 * range does not pass.  Feet (units 0, 1) search an 8-byte table; metric
 * (2) steps 0x30 through the same kind of table, so it sees every sixth
 * entry -- 10, 1000, then .rdata+0xc0 read as a limit.  A range of 0
 * scales by 1. */
static void scale(void)
{
    long double range = (long double)MD(S_MAX) - (long double)MD(S_MIN);
    uint32_t units = GLB16(0xc18), i = 0;

    /* RETAIL BUG (kept): REAL-4 -- a unit past 2 takes neither branch, so neither S_STEP nor S_SCALE is
     * written and both stay as they were (0 the first time).  A zero range forces S_SCALE to 1 below, for
     * the curve only: labels() still uses the stale S_STEP. */
    if ((int32_t)units >= 0) {
        if ((int32_t)units <= 1) {
            /* RETAIL BUG (kept): REAL-3 -- no bound: past the table's last limit the search reads on through
             * .rdata (the metric table, the doubles, retail's export directory) while the range passes each
             * dword.  The feet search stops at .rdata+0xe8 up to 3,181,878,959; only the metric search (step
             * 0x30, below) skips that limit.  Not reproduced (REAL-3): past 2,545,942,528 the metric search
             * runs through .rdata's zero slack into .data and the code pointers there, which a rebuild cannot
             * match (the harness keeps heights within 10^9 of each other). */
            while (range > (long double)(uint64_t)M32(RDATA + 8 * i))
                i++;
            M32(S_STEP) = M32(RDATA + 4 + 8 * i);
            MD(S_SCALE) = (double)((long double)(uint64_t)M32(S_STEP) * (long double)RE_RD(0xe8));
        } else if (units == 2) {
            /* RETAIL BUG (kept): REAL-2 -- steps 0x30 bytes through the metric table (same 8-byte layout as the
             * feet one), seeing only 10, 1000 and .rdata+0xc0's low word as limits: a range over 1,000 m (and up
             * to that word, 2,545,942,528) takes its step from that double's high word, an oversized scale that
             * compresses the curve (a big span still shows a slope).  RETAIL BUG (kept): REAL-3 -- and this
             * search has no bound either (the reads past 2,545,942,528 are not reproduced, as above). */
            while (range > (long double)(uint64_t)M32(RDATA + 0x60 + 0x30 * i))
                i++;
            M32(S_STEP) = M32(RDATA + 0x64 + 0x30 * i);
            MD(S_SCALE) = (double)((long double)(uint64_t)M32(S_STEP) * (long double)RE_RD(0xe0));
        }
    }
    if (range == (long double)RE_RD(0xf0)) {
        M32(S_SCALE) = 0;
        M32(S_SCALE + 4) = 0x3ff00000;
    }
    labels();
}

/* .text+0xea0 -- the grid: 0xf000, verticals every 0x2a3/0x2000 of the
 * width, horizontals every 0x260/0x1800 of the height */
static uint32_t grid(void)
{
    uint32_t x, i, w, h;

    GLB32(0x2ec) = 0xf000;
    for (x = 0x29; x < 0x2270; x += 0x2a3) {
        h = M32(S_HEIGHT);
        HG2D_LINE_V(S_SURFACE, (M32(S_WIDTH) * x) >> 13,
                    (uint32_t)((int32_t)(h * 31) / 6144),
                    (uint32_t)((int32_t)(h * 6113) / 6144));
    }
    for (i = 0; i < 0x1a20; i += 0x260) {
        w = M32(S_WIDTH);
        h = M32(S_HEIGHT);
        HG2D_LINE_H(S_SURFACE, (uint32_t)((int32_t)(w * 41) / 8192),
                    (uint32_t)((int32_t)(w * 8151) / 8192),
                    (0x17e1 - i) * h / 6144);
    }
    return 0;
}

/* .text+0xf90 -- the heights, joined in 0xf00f, from a first point at the
 * first sample (so the first stroke has no length) */
static uint32_t curve(void)
{
    uint32_t n = M32(S_SAMPLES), i = 0;
    uint32_t from[2], to[2];

    if (n == 0)
        return 0;
    GLB32(0x2ec) = 0xf00f;
    plot(0, -0x16, fs98_handle(from));
    while (i < n) {
        plot(i, (int32_t)i - 0x16, fs98_handle(to));
        GLB32(0x2ec) = 0xf00f;
        HG2D_LINE(S_SURFACE, from[0], from[1], to[0], to[1]);
        i++;
        from[0] = to[0];
        from[1] = to[1];
    }
    return 0;
}

/* .text+0xe20 -- the control painted: cleared to 0xff, scaled, drawn */
static uint32_t paint(void)
{
    if (H_MEMSET(M32(S_BITS), 0xff, M32(S_BITS_SIZE)) != 0)
        return 0xffffffffu;
    scale();
    grid();
    curve();
    H_FLUSH(S_REC);
    return 0;
}

/* .text+0xd70 -- the control attached: a record and a view block, the
 * half-extents from the size the host gave it, then painted */
static uint32_t attach(uint32_t hwnd)
{
    uint32_t view;

    H_MEMSET(S_REC, 0, 0xa4);
    if (H_ALLOC(1, 0xc0, S_VIEW, 1) != 0)
        return 0xffffffffu;
    if (H_ATTACH(hwnd, S_REC) != 0)
        return 0xffffffffu;
    view = M32(S_VIEW);
    M32(view) = (uint32_t)((int32_t)M32(S_WIDTH) / 2);
    view = M32(S_VIEW);
    M32(view + 8) = (uint32_t)-((int32_t)M32(S_HEIGHT) / 2);
    view = M32(S_VIEW);
    M32(view + 4) = (uint32_t)((int32_t)M32(S_WIDTH) / 2);
    view = M32(S_VIEW);
    M32(view + 0xc) = (uint32_t)((int32_t)M32(S_HEIGHT) / 2);
    paint();
    return 0;
}

/* .text+0xe70 -- the control let go */
static uint32_t detach(uint32_t hwnd)
{
    (void)hwnd;
    H_RELEASE(S_REC);
    return H_FREE(S_VIEW) != 0 ? 0xffffffffu : 0;
}

/* .text+0xd20 -- the block's +0x08: the graph control (0 attach, 1 paint,
 * 2 let go) */
static uint32_t FS98_STDCALL control(uint32_t mode, uint32_t b, uint32_t hwnd)
{
    (void)b;
    switch (mode) {
    case 0:
        return attach(hwnd);
    case 1:
        paint();
        return 0;
    case 2:
        detach(hwnd);
        return 0;
    default:
        return 0xffffffffu;
    }
}

/* ---- the image -------------------------------------------------------------- */

struct reality_image reality_image = {
    .state = { [8] = 0x78, [19] = 0x80 },
    .import = { {1,0},{2,0},{4,0},{16,0},{7,0},{13,0},{3,0},{17,0},{39,0},{20,0},{10,0},{22,0},{0,0} },
    .linkage = { 32 },
    .crack = {
        {0x8, 0x8002},
        {0x13, 0x39},
        {0x9, 0x60},
        {0x1b, 0x93},
        {0x14, 0x80e5},
        {0x10, 0xad},
        {0x2e, 0x60},
        {0x28, 0x1c},
        {0x20, 0x2},
        {0x30, 0x8002},
        {0x28, 0x1c},
        {0x28, 0x8100},
        {0x31, 0xcd},
        {0x40, 0x100},
        {0xee, 0x80a9},
        {0xf0, 0x100},
        {0x53, 0x8048},
        {0x58, 0x78},
        {0x69, 0x59},
        {0x6c, 0x1a},
        {0x81, 0x2},
        {0x63, 0x80ad},
        {0x79, 0x78},
        {0xc, 0x8100},
        {0x31, 0xa1},
        {0x69, 0x8059},
        {0x79, 0x78},
        {0x90, 0x43},
        {0xa0, 0x93},
        {0xbc, 0xa4},
        {0x99, 0x806a},
        {0xb6, 0x63},
        {0xa3, 0x8093},
        {0x96, 0xbc},
        {0x9e, 0xcd},
        {0x9e, 0x100},
        {0x96, 0x80bc},
        {0x83, 0xd9},
        {0x86, 0xfd},
        {0x79, 0x8078},
        {0x63, 0xad},
        {0x6b, 0xd9},
        {0x68, 0x100},
        {0x44, 0x8039},
        {0x53, 0x48},
        {0x5b, 0x30},
        {0x63, 0x80ad},
        {0x54, 0xd9},
        {0x58, 0x100},
        {0x63, 0x80ad},
        {0x50, 0x90},
        {0xb6, 0x8063},
        {0xb9, 0x2d},
        {0xe8, 0x2},
        {0xb9, 0x802d},
        {0xb1, 0x2},
        {0xbb, 0x8090},
        {0xe3, 0x65},
        {0xbb, 0x8090},
        {0xb9, 0xd9},
        {0xb0, 0x100},
        {0xb9, 0x80d9},
        {0xc3, 0x100},
        {0xf8, 0x8002},
        {0xe8, 0x35},
        {0xc8, 0x6c},
        {0xe8, 0x8035},
        {0xf8, 0x60},
        {0xe9, 0x90},
        {0xee, 0xa9},
        {0xc8, 0x100},
        {0x31, 0x80cd},
        {0x31, 0xa1},
        {0x49, 0x18},
        {0x36, 0x1c},
        {0x38, 0x2},
        {0xe9, 0x8090},
        {0x100, 0xa4},
    },
    .rest = {   /* meaning not yet known */
        [0] = {0xffffffff, 0x0},
        [2] = {0x2d, 0x0},
        [7] = {0x0, 0xd},
        [8] = {0x0, 0xb},
        [15] = {0x1, 0xc},
        [16] = {0x2006, 0xc18},
        [17] = {0x1, 0x8},
        [18] = {0x14, 0x2001},
        [20] = {0x8, 0x18},
        [21] = {0x1100f, 0x844},
        [22] = {0x1, 0x0},
        [24] = {0x20, 0x18},
        [28] = {0x20, 0x10},
    },
};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _reality_image + 0x48\n\t"
        ".globl _Linkage\n\t.set _Linkage, _reality_image + 0xb0");

static void reality_install(void)
{
    static void (*const table[8])(void) = {
        damage_0, damage_1, damage_2, damage_3, damage_4, damage_5, damage_6, damage_7,
    };
    uint32_t i;

    for (i = 0; i < 8; i++)
        M32(S_DAMAGE + 4 * i) = fs98_handle(table[i]);
    M32(D + RE_LINKAGE + 0x04) = fs98_handle(reality_init);
    M32(D + RE_LINKAGE + 0x08) = fs98_handle(reality_deinit);
    M32(D + RE_LINKAGE + 0x18) = fs98_handle(clear_damage);
    M32(D + RE_LINKAGE + 0x1c) = fs98_handle(crack);
    M32(S_BLOCK + 0x04) = fs98_handle(block_check);
    M32(S_BLOCK + 0x08) = fs98_handle(control);
    M32(S_EXCHANGE + 0x20) = fs98_handle(rate_text);
    M32(D + 0x23c) = S_RATE_TEXT;
    M32(D + 0x240) = D + 0x428;
    M32(D + 0x248) = D + 0x300;
    M32(D + 0x250) = D + 0x3f8;
}

/* .text+0x000 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        reality_install();
    }
    return TRUE;
}
