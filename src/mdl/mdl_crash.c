/* The three crash models (CESSNA182RRG_C, CESSNA182S_C, EXTRA300S_C .MDL), decompiled.
 *
 * One source, built three times (CRASH_MODEL 0, 1, 2) over each model's generated data image
 * (tools/gen_mdl_crash.py: retail's .data at its RVAs, 64-KiB aligned, with the relocated code
 * pointers bound to the entry points here). Retail's three are the same hand-written assembly
 * assembled with different tables: C182S keeps its flag and phase words two bytes later in the
 * state and seeds its debris attitude from the aircraft, the Extra has 17 debris pieces to the
 * Cessnas' 13 (so its variables after the piece rates sit 0x10 later) and its own piece list.
 *
 * Retail addresses are named by C182RG RVA, as the gauges name theirs: V32(0x5038) is the dword
 * the models keep at image + 0x5038. The variables after the piece rates move with the piece
 * count, and are written T(k), k from the C182 address 0x50c2.
 *
 * What the crash does, in retail's terms: when GLOBAL byte +0x838 becomes 6 the update copies the
 * aircraft's position into the model state and steps an animation picked from the attitude the
 * flight model reported at the crash (pitch, bank) or from the kind of impact (GLOBAL +0x834):
 * a level slide, a banked or rolled slide that tears off a wing, a nose-in, or a steep dive or an
 * impact that throws 13 or 17 pieces (0x24-byte position records in the state, from +0x84). It
 * ends by setting +0x838 to 4. CHAIN list 15 draws the wreck program, or the debris one once the
 * phase word is 2.
 *
 * Arithmetic is retail's 16- and 32-bit integer arithmetic, carries included. Positions are
 * retail's 48-bit numbers: a fraction word and a signed dword above it.
 *
 * Register outputs, the part of the contract C does not give for free (mdl_crash_entry.S):
 *   - the update returns with edi = GLOBAL (retail loads it, WINDOW 0x20906a49 reads it);
 *   - the CHAIN callback returns with G3D's eax, ecx and edx (its last act is the G3D call, and
 *     CHAIN hands a callback's edx to the next one);
 *   - the rest are dead: WINDOW pops ebx/esi before reading them, ACONTAIN reloads eax, ecx and
 *     edx after attach, init and detach. Retail's update leaves ebx and esi changed; ours keeps
 *     them.
 * Omitted, as having no effect: retail's stack-alignment wrapper around every imported call
 * (it writes below esp and pushes a 4 the callee never reads), and dead loads.
 */
#include <windows.h>
#include <stdint.h>
#include "fs98/linkage.h"

#ifndef CRASH_MODEL
#error "CRASH_MODEL must be 0 (C182RG), 1 (C182S) or 2 (Extra)"
#endif

extern uint8_t mdl_image[];
extern struct fs98_import ImportTable[];
extern uint8_t mdl_header[];
void crash_chain_entry(void);   /* mdl_crash_entry.S: the CHAIN list 15 callback */

#define STDCALL __attribute__((stdcall))

/* ---- memory ------------------------------------------------------------------------------ */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }
static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }

#define GLOBAL ((uint8_t *)ImportTable[1].link)
#define HOST   ((uint8_t *)ImportTable[0].link)
#define CHAIN  ((uint8_t *)ImportTable[2].link)
#define G3D    ((uint8_t *)ImportTable[3].link)
#define UTIL   ((uint8_t *)ImportTable[4].link)
#define SERVICE(link, off) ((void *)(uintptr_t)rd32((link) + (off)))

#define V(rva) (mdl_image + (rva))
#define V16(rva) rd16(V(rva))
#define V32(rva) rd32(V(rva))
#define SET_V16(rva, v) wr16(V(rva), (v))
#define SET_V32(rva, v) wr32(V(rva), (v))
#define G16(off) rd16(GLOBAL + (off))
#define G32(off) rd32(GLOBAL + (off))
#define SET_G16(off, v) wr16(GLOBAL + (off), (v))
#define SET_G32(off, v) wr32(GLOBAL + (off), (v))
#define S16(off) rd16(st + (off))
#define S32(off) rd32(st + (off))
#define SET_S16(off, v) wr16(st + (off), (v))
#define SET_S32(off, v) wr32(st + (off), (v))

/* GLOBAL fields */
#define G_GROUND   0x20     /* ground elevation, metres * 256 */
#define G_DELTA    0x2aa    /* the frame's time step */
#define G_POS      0x560    /* the aircraft's position record, 0x24 bytes: */
#define G_ALT      0x572    /*   altitude, 48-bit (+0x12) */
#define G_PITCH    0x578    /*   +0x18 */
#define G_BANK     0x57c    /*   +0x1c */
#define G_HEADING  0x580    /*   +0x20 */
#define G_IMPACT   0x834    /* 6 and 12 are impacts with their own animations */
#define G_CRASH    0x838    /* signed byte; 6 while the crash animates, 4 when it is over */

/* A position record, as GLOBAL +0x560 and each debris piece */
#define P_NORTH    0x02     /* 48-bit */
#define P_EAST     0x0a     /* 48-bit, longitude */
#define P_ALT      0x12     /* 48-bit */
#define P_PITCH    0x18
#define P_BANK     0x1c
#define P_HEADING  0x20

/* ---- the models' differences ------------------------------------------------------------- */

#if CRASH_MODEL == 2
#define CRASH_PIECES 17
#define CRASH_FLAGS 0x7e
#define PROGRAM_DEBRIS 0x50f6
#define PROGRAM_WRECK  0x51be
#elif CRASH_MODEL == 1
#define CRASH_PIECES 13
#define CRASH_FLAGS 0x80
#define PROGRAM_DEBRIS 0x14eae
#define PROGRAM_WRECK  0x14f28
#else
#define CRASH_PIECES 13
#define CRASH_FLAGS 0x7e
#define PROGRAM_DEBRIS 0x50e6
#define PROGRAM_WRECK  0x5160
#endif
#define CRASH_PHASE (CRASH_FLAGS + 2)           /* 2: draw the debris */
#define PIECE(i) (0x84 + 0x24 * (i))            /* state offset of debris piece i */
#define RATE(i) (0x508e + 4 * (i))              /* its sink rate */
#define T(k) (0x508e + 4 * CRASH_PIECES + (k))  /* T(0) is 0x50c2 in the Cessnas */

#define FLAG(bit) SET_S16(CRASH_FLAGS, S16(CRASH_FLAGS) | (bit))

/* ---- 48-bit and 16-bit arithmetic -------------------------------------------------------- */

/* add a 48-bit value (fraction word, signed dword) to the one at p, as add/adc */
static void add48(uint8_t *p, uint16_t fraction, uint32_t whole)
{
    uint32_t low = (uint32_t)rd16(p) + fraction;
    wr16(p, low);
    wr32(p + 2, rd32(p + 2) + whole + (low >> 16));
}
/* ... a signed 32-bit value, sign-extended (movsx edx, dx after a 16-bit imul) */
static void add48_32(uint8_t *p, uint32_t value)
{
    add48(p, (uint16_t)value, (uint32_t)(int32_t)(int16_t)(value >> 16));
}
/* subtract, as sub/sbb */
static void sub48(uint8_t *p, uint16_t fraction, uint32_t whole)
{
    uint32_t low = rd16(p);
    wr16(p, low - fraction);
    wr32(p + 2, rd32(p + 2) - whole - (low < fraction));
}

static uint32_t imul16(uint16_t a, uint16_t b)   /* dx:ax of a 16-bit imul */
{
    return (uint32_t)((int32_t)(int16_t)a * (int16_t)b);
}
static uint16_t sar16(uint16_t v) { return (uint16_t)((int16_t)v >> 1); }
static uint16_t abs16(uint16_t v) { return (int16_t)v < 0 ? (uint16_t)-v : v; }   /* cwd/xor/sub */

/* ground elevation as a 48-bit altitude: retail's cdq/shld 24/shl 8 */
static uint16_t ground_fraction(void) { return (uint16_t)(G32(G_GROUND) << 8); }
static uint32_t ground_whole(void) { return (uint32_t)((int32_t)G32(G_GROUND) >> 8); }

static void copy_bytes(uint8_t *to, const uint8_t *from, uint32_t n)   /* rep movsb */
{
    while (n--)
        *to++ = *from++;
}

/* ---- services ---------------------------------------------------------------------------- */

typedef uint32_t (STDCALL *service1)(uint32_t);
typedef uint32_t (STDCALL *service2)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *service3)(uint32_t, uint32_t, uint32_t);
#define PTR(p) ((uint32_t)(uintptr_t)(p))

static void chain_add(void) { ((service2)SERVICE(CHAIN, 0x18))(15, PTR(crash_chain_entry)); }
static void chain_remove(void) { ((service2)SERVICE(CHAIN, 0x1c))(15, PTR(crash_chain_entry)); }
static void host_move(void *to, const void *from, uint32_t n)
{
    ((service3)SERVICE(HOST, 0x21c))(PTR(to), PTR(from), n);
}
/* UTIL +0x2c: the sine and cosine of a 16-bit angle */
static void sincos(uint16_t angle, uint16_t *sine, uint16_t *cosine)
{
    int16_t s, c;
    ((service3)SERVICE(UTIL, 0x2c))(angle, PTR(&s), PTR(&c));
    *sine = (uint16_t)s;
    *cosine = (uint16_t)c;
}
/* UTIL +0x44: a 48-bit eastward step in metres as longitude at the aircraft's latitude, added
 * to the 48-bit longitude at p. Returns the dword of the step, which retail leaves in eax. */
static uint32_t east48(uint8_t *p, uint32_t metres)
{
    uint8_t in[8] = {0}, out[8] = {0};
    wr16(in + 2, metres);
    wr32(in + 4, (uint32_t)(int32_t)(int16_t)(metres >> 16));
    ((service3)SERVICE(UTIL, 0x44))(PTR(in), PTR(GLOBAL + G_POS), PTR(out));
    add48(p, rd16(out + 2), rd32(out + 4));
    return rd32(out + 4);
}

/* ---- helpers ----------------------------------------------------------------------------- */

/* 0x2993: v with its magnitude limited to limit, sign kept */
static uint16_t clamp(uint16_t v, uint16_t limit)
{
    uint16_t sign = (int16_t)v < 0 ? 0xffff : 0, a = (uint16_t)((v ^ sign) - sign);
    if (a > limit)
        a = limit;
    return (uint16_t)((a ^ sign) - sign);
}

/* 0x2a13: move position p north by speed * the frame's step */
static void step_north(uint8_t *p, uint16_t speed)
{
    add48_32(p + P_NORTH, imul16(speed, G16(G_DELTA)));
}

/* 0x29b2: ... east. Returns what retail leaves in eax. */
static uint32_t step_east(uint8_t *p, uint16_t speed)
{
    return east48(p + P_EAST, imul16(speed, G16(G_DELTA)));
}

/* 0x2a32: a piece falls: its rate grows by 15 a step, its altitude moves by rate >> 15 */
static void sink(uint8_t *p, uint32_t rate_rva)
{
    uint32_t rate = V32(rate_rva) + G16(G_DELTA) * 0xfffffff1u;
    SET_V32(rate_rva, rate);
    add48_32(p + P_ALT, imul16((uint16_t)(rate >> 15), G16(G_DELTA)));
}

/* 0x2a61: the larger magnitude plus half the smaller */
static uint16_t spread(uint16_t a, uint16_t b)
{
    uint16_t small = abs16(a), large = abs16(b), t;
    if (large < small) {
        t = large; large = small; small = t;
    }
    return (uint16_t)(large + (small >> 1));
}

/* 0x2a90: a timer: two steps a frame; 1 while below limit, else reset and 0 (retail's carry) */
static int tick(uint32_t rva, uint32_t limit)
{
    SET_V32(rva, V32(rva) + G16(G_DELTA));
    SET_V32(rva, V32(rva) + G16(G_DELTA));
    if (V32(rva) < limit)
        return 1;
    SET_V32(rva, 0);
    return 0;
}

/* 0x2ab2: reduce the magnitude of the word at rva by amount, unless that would cross zero */
static void shrink(uint32_t rva, uint16_t amount)
{
    uint16_t v = V16(rva), sign = (int16_t)v < 0 ? 0xffff : 0;
    uint16_t a = (uint16_t)(((v ^ sign) - sign) - amount);
    if ((int16_t)a < 0)
        return;
    SET_V16(rva, (a ^ sign) - sign);
}

/* 0x2e19: subtract amount from the word at rva while it is not below it. Retail returns with
 * eax's high half untouched; callers that read it get the caller's own leftover. */
static void decay(uint32_t rva, uint16_t amount)
{
    if ((int16_t)V16(rva) >= (int16_t)amount)
        SET_V16(rva, V16(rva) - amount);
}

/* the 16-bit product, shifted right 15, as shrd ax, dx, 15 */
static uint16_t q15(uint32_t product) { return (uint16_t)(product >> 15); }
/* times scale, halved: imul, then sar dx / rcr ax */
static uint32_t scaled(uint16_t v, uint16_t scale) { return (uint32_t)((int32_t)imul16(v, scale) >> 1); }

/* 0x2ad6: put a piece where the aircraft is, then offset it along the aircraft's axes by scale.
 * Returns what retail leaves in eax: the high half is UTIL's Linkage address, which retail had
 * loaded to call +0x44 (UTIL preserves eax). */
static uint32_t place(uint16_t scale, uint8_t *piece)
{
    uint16_t sp, cp, sb, cb, sh, ch;
    uint32_t t, r, ax;
    copy_bytes(piece, GLOBAL + G_POS, 0x24);
    sincos((uint16_t)(G32(G_PITCH) >> 16), &sp, &cp);
    sincos((uint16_t)(G32(G_BANK) >> 16), &sb, &cb);
    sincos((uint16_t)(G32(G_HEADING) >> 16), &sh, &ch);
    t = imul16(sh, cb);
    r = imul16(q15(imul16(ch, sp)), sb) - t;
    add48_32(piece + P_NORTH, scaled(q15(r), scale));
    t = imul16(ch, cb);
    r = imul16(q15(imul16(sh, sp)), sb) + t;
    east48(piece + P_EAST, scaled(q15(r), scale));
    ax = scaled(q15(imul16(cp, sb)), scale);
    add48_32(piece + P_ALT, ax);
    return (PTR(UTIL) & 0xffff0000u) | (ax & 0xffff);
}

/* the product doubled, as rol ax, 1 / rcl dx, 1: bit 15 comes round into bit 0 */
static uint32_t doubled(uint32_t product)
{
    uint16_t low = (uint16_t)product, high = (uint16_t)(product >> 16);
    uint16_t new_low = (uint16_t)(low << 1 | low >> 15);
    uint16_t new_high = (uint16_t)(high << 1 | low >> 15);
    return (uint32_t)new_high << 16 | new_low;
}

/* 0x2c8f: put a piece at the aircraft's north and east, offset by scale against its heading */
static void place_flat(uint16_t scale, uint8_t *piece)
{
    uint16_t sh, ch;
    uint32_t v;
    wr16(piece + P_NORTH, G16(G_POS + P_NORTH));
    wr32(piece + P_NORTH + 2, G32(G_POS + P_NORTH + 2));
    wr16(piece + P_EAST, G16(G_POS + P_EAST));
    wr32(piece + P_EAST + 2, G32(G_POS + P_EAST + 2));
    sincos((uint16_t)(G32(G_HEADING) >> 16), &sh, &ch);
    v = doubled(imul16(scale, sh));
    sub48(piece + P_NORTH, (uint16_t)v, (uint32_t)(int32_t)(int16_t)(v >> 16));
    east48(piece + P_EAST, doubled(imul16(scale, ch)));
}

/* 0x2d75: the aircraft's height above the crash altitude for its pitch: 5 (11 nose down) times
 * the sine of the pitch, less floor, or the crash altitude itself */
static void lift(uint8_t *p, uint16_t floor)
{
    uint16_t factor = 5, sine, cosine;
    uint32_t pitch = rd32(p + P_PITCH), product;
    uint16_t dx;
    if ((int32_t)pitch <= 0) {
        pitch = (uint32_t)-pitch;
        factor = 0xb;
    }
    sincos((uint16_t)(pitch >> 16), &sine, &cosine);
    product = imul16(sine, factor);
    dx = (uint16_t)(product >> 16);
    if ((int16_t)dx > (int16_t)floor) {
        uint32_t low = (uint32_t)(uint16_t)product + V16(0x504e);
        dx = (uint16_t)(dx - floor);
        SET_G16(G_ALT, low);
        SET_G32(G_ALT + 2, (uint32_t)(int32_t)(int16_t)dx + V32(0x5050) + (low >> 16));
    } else {
        SET_G16(G_ALT, V16(0x504e));
        SET_G32(G_ALT + 2, V32(0x5050));
    }
}

/* 0x30c4: is the piece on or below the ground? (not at stage 2). Retail's carry: the sign of
 * the 48-bit difference, not overflow-aware. */
static int grounded(const uint8_t *piece)
{
    uint16_t fraction = ground_fraction();
    if (V32(0x5038) == 2)
        return 0;
    return (int32_t)(ground_whole() - rd32(piece + P_ALT + 2) - (fraction < rd16(piece + P_ALT))) >= 0;
}

/* the altitude at p lowered by 0x3789, then pulled up to the ground the way retail compares:
 * whole and fraction separately, unsigned. RETAIL BUG (kept): CRASH-GROUND-COMPARE */
static void settle_altitude(uint8_t *p, uint16_t fraction, uint32_t whole)
{
    sub48(p, 0x3789, 0);
    if (rd32(p + 2) > whole)
        return;
    if (rd16(p) > fraction)
        return;
    wr16(p, fraction);
    wr32(p + 2, whole);
}

/* 0x30ef: a piece lying on the ground levels out and settles (not at stage 2) */
static void settle(uint8_t *piece)
{
    uint32_t step, v;
    int k;
    if (V32(0x5038) == 2)
        return;
    step = G16(G_DELTA) * 0x3334u;
    for (k = 0; k < 2; k++) {
        uint8_t *a = piece + (k ? P_BANK : P_PITCH);
        v = rd32(a);
        if (v == 0)
            continue;
        if ((int32_t)v < 0) {
            wr32(a, v + step);
            if ((int32_t)(v + step) < 0)
                continue;
        } else {
            wr32(a, v - step);
            if ((int32_t)(v - step) >= 0)
                continue;
        }
        wr32(a, 0);
    }
    settle_altitude(piece + P_ALT, ground_fraction(), ground_whole());
}

/* 0x316d: the aircraft settles too, onto the ground plus GLOBAL +0x120c.
 * RETAIL BUG (kept): CRASH-GROUND-OFFSET-CARRY -- the fraction's carry is dropped. */
static void settle_aircraft(void)
{
    uint32_t offset = G32(0x120c);
    if (V32(0x5038) == 2)
        return;
    settle_altitude(GLOBAL + G_ALT, (uint16_t)(ground_fraction() + offset),
                    ground_whole() + (offset >> 16));
}

/* 0x31be: put a piece on the ground */
static void to_ground(uint8_t *piece)
{
    wr16(piece + P_ALT, ground_fraction());
    wr32(piece + P_ALT + 2, ground_whole());
}

/* 0x1715: the end of a crash: after 0x27ffb of time steps, GLOBAL +0x838 becomes 4 */
static void end_after(uint32_t limit)
{
    SET_V32(0x508a, V32(0x508a) + G16(G_DELTA));
    if ((int32_t)V32(0x508a) < (int32_t)limit)
        return;
    SET_V32(0x508a, 0);
    GLOBAL[G_CRASH] = 4;
}
static void end_crash(void) { end_after(0x27ffb); }

/* the 48-bit altitude less 0x1a000 (sub ax, 0xa000 / sbb edx, 1) */
static void drop_piece(uint8_t *alt) { sub48(alt, 0xa000, 1); }
/* ... of the aircraft, into a piece */
static void below_aircraft(uint8_t *alt)
{
    wr16(alt, G16(G_ALT));
    wr32(alt + 2, G32(G_ALT + 2));
    drop_piece(alt);
}
/* ... of the crash, into a piece */
static void below_crash(uint8_t *alt)
{
    wr16(alt, V16(0x504e));
    wr32(alt + 2, V32(0x5050));
    drop_piece(alt);
}

/* ---- the debris -------------------------------------------------------------------------- */

/* 0x2e33: every piece where the aircraft is, and their attitudes */
static void gather(uint8_t *st)
{
    int i;
    for (i = 0; i < CRASH_PIECES; i++) {
        uint8_t *p = st + PIECE(i);
        wr16(p + P_NORTH, G16(G_POS + P_NORTH));
        wr32(p + P_NORTH + 2, G32(G_POS + P_NORTH + 2));
        wr16(p + P_EAST, G16(G_POS + P_EAST));
        wr32(p + P_EAST + 2, G32(G_POS + P_EAST + 2));
        wr16(p + P_ALT, G16(G_ALT));
        wr32(p + P_ALT + 2, G32(G_ALT + 2));
#if CRASH_MODEL == 1
        wr32(p + P_PITCH, G32(G_PITCH));
        wr32(p + P_BANK, G32(G_BANK));
        wr32(p + P_HEADING, G32(G_HEADING));
#endif
    }
#if CRASH_MODEL != 1
    SET_S32(0x108, 0xc0000000u);
    SET_S32(0x10c, 0xc0000000u);
    SET_S32(0x12c, 0xc0000000u);
    SET_S32(0x130, 0xc0000000u);
    SET_S32(0x150, 0xc0000000u);
    SET_S32(0x154, 0x40000000u);
    SET_S32(0x174, 0xc0000000u);
    SET_S32(0x178, 0x40000000u);
    SET_S32(0x9c, 0x80000000u);
    SET_S32(0xc0, 0x80000000u);
    SET_S32(0xe4, 0x80000000u);
#endif
}

/* A piece of the debris pass (0x1f0e), as retail unrolls it for each piece: unless it is
 * grounded it moves by its north and east speeds and sinks, and unless that grounds it, it
 * tumbles; a grounded piece stops tumbling and lies on the ground. */
struct spin { uint16_t field; uint16_t rate; int8_t sign; };
struct stop { uint16_t field; uint32_t value; };
struct piece {
    uint16_t piece;          /* state offset */
    uint16_t north, east;    /* the module words with its speeds */
    uint16_t sinks;          /* the piece the sink is applied to (its own, but for one) */
    uint8_t rate;            /* its RATE() */
    uint8_t spins;
    struct spin spin[3];
    uint8_t stops;
    struct stop stop[2];
};

/* 1 if the piece flew this frame, 0 if it was or became grounded */
static int debris_piece(uint8_t *st, const struct piece *d)
{
    uint8_t *p = st + d->piece;
    int k;
    if (!grounded(p)) {
        step_north(p, V16(d->north));
        step_east(p, V16(d->east));
        sink(st + d->sinks, RATE(d->rate));
        if (!grounded(p)) {
            for (k = 0; k < d->spins; k++) {
                uint8_t *f = st + d->spin[k].field;
                uint32_t change = G16(G_DELTA) * (uint32_t)d->spin[k].rate;
                wr32(f, d->spin[k].sign > 0 ? rd32(f) + change : rd32(f) - change);
            }
            return 1;
        }
    }
    for (k = 0; k < d->stops; k++)
        SET_S32(d->stop[k].field, d->stop[k].value);
    to_ground(p);
    return 0;
}

#define UP(f, r) {(f), (r), 1}
#define DOWN(f, r) {(f), (r), -1}
#define ZERO2(f) 2, {{(f) + P_PITCH, 0}, {(f) + P_BANK, 0}}
/* RETAIL BUG (kept): CRASH-PIECE-STOP-FIELD -- piece 0x114, grounded, zeroes 0x108/0x10c, the
 * pitch and bank of piece 0xf0, and keeps its own tumbling */
#define ZERO_F0 2, {{0x108, 0}, {0x10c, 0}}
#define NOSTOP 0, {{0, 0}}

#if CRASH_MODEL == 2
/* The Extra's 17, in retail's order. */
static const struct piece debris[] = {
    {0xa8,  0x507a, 0x5076, 0xa8,  1, 3, {DOWN(0xc0, 0x4000), DOWN(0xc4, 0x4000), DOWN(0xc8, 0x6000)}, ZERO2(0xa8)},
    {0xcc,  0x5080, 0x507c, 0xcc,  2, 3, {DOWN(0xe4, 0x6000), DOWN(0xe8, 0x6000), DOWN(0xec, 0x8000)}, ZERO2(0xcc)},
    {0xf0,  0x5074, 0x5076, 0xf0,  3, 3, {UP(0x108, 0x6000), UP(0x10c, 0x6000), UP(0x110, 0x6000)}, ZERO2(0xf0)},
    {0x114, 0x5074, 0x5070, 0x114, 4, 2, {UP(0x12c, 0x8000), UP(0x130, 0x8000)}, ZERO_F0},
    {0x138, 0x507a, 0x5070, 0x138, 5, 3, {DOWN(0x150, 0x6000), DOWN(0x154, 0x6000), DOWN(0x158, 0x6000)}, ZERO2(0x138)},
    {0x15c, 0x5074, 0x5070, 0x15c, 6, 2, {DOWN(0x174, 0x8000), DOWN(0x178, 0x8000)}, ZERO2(0x15c)},
    {0x1c8, 0x5080, 0x507c, 0x1c8, 9, 1, {UP(0x1e0, 0x8000)}, ZERO2(0x1c8)},
    {0x180, 0x5080, 0x5076, 0x180, 7, 3, {DOWN(0x198, 0x6000), DOWN(0x19c, 0x6000), DOWN(0x1a0, 0x6000)}, ZERO2(0x180)},
    {0x1a4, 0x5080, 0x5076, 0x1a4, 8, 3, {DOWN(0x1bc, 0x6000), DOWN(0x1c0, 0x6000), DOWN(0x1c4, 0x6000)}, ZERO2(0x1a4)},
    {0x1ec, 0x5074, 0x5076, 0x1ec, 10, 3, {UP(0x204, 0x6000), UP(0x208, 0x6000), UP(0x20c, 0x6000)},
     2, {{0x204, 0}, {0x208, 0x40000000u}}},
    {0x210, 0x507a, 0x507c, 0x210, 11, 1, {DOWN(0x228, 0x4000)}, NOSTOP},
    {0x234, 0x5074, 0x5076, 0x234, 12, 1, {DOWN(0x24c, 0x4000)}, NOSTOP},
    {0x258, 0x5080, 0x5070, 0x258, 13, 1, {DOWN(0x270, 0x4000)}, NOSTOP},
    {0x27c, 0x5080, 0x5076, 0x27c, 14, 1, {UP(0x294, 0x4000)}, NOSTOP},
    /* RETAIL BUG (kept): CRASH-EXTRA-PIECE-2A0 -- it sinks piece 0x234 and tumbles 0x234's pitch */
    {0x2a0, 0x5074, 0x5076, 0x234, 15, 1, {DOWN(0x24c, 0x4000)}, NOSTOP},
    {0x2c4, 0x5080, 0x5070, 0x2c4, 16, 1, {DOWN(0x2dc, 0x4000)}, NOSTOP},
};
#define DEBRIS_SKIP_FROM 0x210   /* flying, it goes on to 0x27c ... */
#define DEBRIS_SKIP_TO   0x27c   /* RETAIL BUG (kept): CRASH-EXTRA-SKIP -- 0x234, 0x258 skipped */
#else
/* The Cessnas' 13, in retail's order. */
static const struct piece debris[] = {
    {0xa8,  0x507a, 0x5076, 0xa8,  1, 3, {DOWN(0xc0, 0x4000), DOWN(0xc4, 0x4000), DOWN(0xc8, 0x6000)}, ZERO2(0xa8)},
    {0xcc,  0x5080, 0x507c, 0xcc,  2, 3, {DOWN(0xe4, 0x6000), DOWN(0xe8, 0x6000), DOWN(0xec, 0x8000)}, ZERO2(0xcc)},
    {0xf0,  0x5074, 0x5076, 0xf0,  3, 3, {UP(0x108, 0x6000), UP(0x10c, 0x6000), UP(0x110, 0x6000)}, ZERO2(0xf0)},
    {0x114, 0x5074, 0x5070, 0x114, 4, 2, {UP(0x12c, 0x8000), UP(0x130, 0x8000)}, ZERO_F0},
    {0x138, 0x507a, 0x5070, 0x138, 5, 3, {DOWN(0x150, 0x6000), DOWN(0x154, 0x6000), DOWN(0x158, 0x6000)}, ZERO2(0x138)},
    {0x15c, 0x5074, 0x5070, 0x15c, 6, 2, {DOWN(0x174, 0x8000), DOWN(0x178, 0x8000)}, ZERO2(0x15c)},
    {0x1a4, 0x5080, 0x507c, 0x1a4, 8, 1, {UP(0x1bc, 0x8000)}, ZERO2(0x1a4)},
    {0x180, 0x5080, 0x5076, 0x180, 7, 3, {DOWN(0x198, 0x6000), DOWN(0x19c, 0x6000), DOWN(0x1a0, 0x6000)}, ZERO2(0x180)},
    {0x1c8, 0x5074, 0x5076, 0x1c8, 9, 3, {UP(0x1e0, 0x6000), UP(0x1e4, 0x6000), UP(0x1e8, 0x6000)},
     2, {{0x1e0, 0}, {0x1e4, 0x40000000u}}},
    {0x1ec, 0x507a, 0x5076, 0x1ec, 10, 1, {UP(0x204, 0x4000)}, NOSTOP},
    {0x210, 0x507a, 0x507c, 0x210, 11, 1, {DOWN(0x228, 0x4000)}, NOSTOP},
    {0x234, 0x5080, 0x5076, 0x234, 12, 1, {UP(0x24c, 0x4000)}, NOSTOP},
};
#endif

/* 0x1f0e: one frame of the debris. The first piece carries the aircraft with it. */
static void debris_pass(uint8_t *st)
{
    uint8_t *p = st + PIECE(0);
    unsigned i, n = sizeof debris / sizeof debris[0];
    SET_V32(0x5038, V32(0x5038) + 1);
    if (!grounded(p)) {
        step_north(p, V16(0x5074));
        step_north(GLOBAL + G_POS, V16(0x5074));
        step_east(p, V16(0x5070));
        step_east(GLOBAL + G_POS, V16(0x5070));
        sink(p, RATE(0));
        if (!grounded(p)) {
            SET_G16(G_ALT, rd16(p + P_ALT));
            SET_G32(G_ALT + 2, rd32(p + P_ALT + 2));
            SET_S32(0x9c, S32(0x9c) + G16(G_DELTA) * 0x4000u);
            SET_S32(0xa0, S32(0xa0) + G16(G_DELTA) * 0x4000u);
            SET_S32(0xa4, S32(0xa4) + G16(G_DELTA) * 0x6000u);
            goto rest;
        }
    }
    SET_S32(0x9c, 0);
    SET_S32(0xa0, 0);
    to_ground(p);
    SET_G16(G_ALT, rd16(p + P_ALT));
    SET_G32(G_ALT + 2, rd32(p + P_ALT + 2));
rest:
    for (i = 0; i < n; i++) {
        int flew = debris_piece(st, &debris[i]);
#ifdef DEBRIS_SKIP_FROM
        if (flew && debris[i].piece == DEBRIS_SKIP_FROM)
            while (debris[i + 1].piece != DEBRIS_SKIP_TO)
                i++;
#endif
        /* the last piece ends the crash only when it is grounded */
        if (i == n - 1 && !flew)
            end_crash();
    }
}

/* the nine speed words the debris starts with, and the piece rates from them */
#if CRASH_MODEL == 2
static const uint8_t group_a[] = {0, 1, 2, 7, 8, 3, 5}, group_b[] = {4, 6, 9, 10},
                     group_c[] = {11, 12, 13, 14, 15, 16};
#else
static const uint8_t group_a[] = {0, 1, 2, 7, 3, 5}, group_b[] = {4, 6, 8, 9},
                     group_c[] = {10, 11, 12};
#endif
static void set_rates(const uint8_t *group, unsigned n, uint32_t value)
{
    unsigned i;
    for (i = 0; i < n; i++)
        SET_V32(RATE(group[i]), value);
}
#define SET_RATES(group, value) set_rates(group, sizeof group, (value))

/* the north/east speeds of the nine debris speed words, from a clamped crash speed: v, and the
 * means 3/4 and 7/8 of the way to it, as retail's sar/add sequence */
static void fan(uint16_t speed, uint32_t full, uint32_t inner, uint32_t outer)
{
    uint16_t bx;
    SET_V16(full, speed);
    bx = (uint16_t)(sar16(speed) + speed);
    bx = sar16(bx);
    SET_V16(inner, bx);
    bx = (uint16_t)(bx + speed);
    bx = sar16(bx);
    SET_V16(outer, bx);
}

/* ---- the animations ---------------------------------------------------------------------- */

/* 0x12eb: |bank| up to 10 degrees, pitch below 15: the aircraft slides to a stop */
static void crash_slide(uint8_t *st)
{
    uint16_t ax;
    uint32_t product, v, rise;
    if ((int32_t)V32(0x5038) <= 0) {
        SET_V32(0x5038, V32(0x5038) + 1);
        SET_V16(0x5068, V16(0x5068) << 1);
        ax = (uint16_t)(abs16(V16(0x506c)) << 1);
        SET_V16(0x506c, ax);
        SET_V16(0x5072, ax);
        SET_V16(0x506e, V16(0x506e) << 1);
        FLAG(4);
        FLAG(8);
        if ((int32_t)V32(0x5058) > 0x38e0000)
            FLAG(0x10);
        else if ((int32_t)V32(0x5058) < (int32_t)0xfc720000u)
            FLAG(0x20);
    }
    if ((int16_t)V16(0x5072) < 5) {
        SET_G16(G_ALT, V16(0x504e));
        SET_G32(G_ALT + 2, V32(0x5050));
        SET_G32(G_PITCH, 0);
        SET_G32(G_BANK, 0);
        end_crash();
        return;
    }
    /* a bounce: back at or below the crash altitude, the speeds halve */
    if ((int32_t)G32(G_ALT + 2) < (int32_t)V32(0x5050) ||
        ((int32_t)G32(G_ALT + 2) == (int32_t)V32(0x5050) && G16(G_ALT) <= V16(0x504e))) {
        SET_V16(0x5068, sar16(V16(0x5068)));
        ax = sar16(V16(0x5072));
        SET_V16(0x506c, ax);
        SET_V16(0x5072, ax);
        SET_V16(0x506e, sar16(V16(0x506e)));
        SET_G16(G_ALT, V16(0x504e));
        SET_G32(G_ALT + 2, V32(0x5050));
    }
    step_north(GLOBAL + G_POS, V16(0x506e));
    step_north(GLOBAL + G_POS, V16(0x5068));
    /* the climb at 0x506c (with its fraction at 0x506a) falls by 20 a step */
    product = (uint32_t)(uint16_t)(0xa << 1) * G16(G_DELTA);
    v = ((uint32_t)V16(0x506c) << 16 | V16(0x506a)) - product;
    SET_V16(0x506a, v);
    SET_V16(0x506c, v >> 16);
    /* RETAIL BUG (kept): CRASH-SLIDE-CBW -- the pitch takes the climb's low byte (cbw) */
    SET_G32(G_PITCH, (uint32_t)-((int32_t)(int8_t)V16(0x506c) * 0x640000));
    ax = (uint16_t)((V16(0x506c) << 1) + (V16(0x506a) >> 15));
    rise = imul16(ax, G16(G_DELTA));
    add48_32(GLOBAL + G_ALT, rise);
}

/* 0x1479: |bank| up to 30 degrees: one wing goes, the aircraft yaws round to a stop */
static void crash_wing(uint8_t *st)
{
    uint32_t eax;
    if ((int32_t)V32(0x5038) <= 0) {
        SET_V32(0x5038, V32(0x5038) + 1);
        SET_V16(T(0x20), spread(V16(0x5068), V16(0x506e)) << 3);
        FLAG(4);
        SET_S32(0x174, 0);
        SET_S32(0x12c, 0);
        if ((int32_t)V32(0x5058) <= 0) {
            FLAG(2);
            SET_G32(G_BANK, 0xfc720000u);
            SET_V32(T(0x18), 0xf1c80000u);
            SET_S32(0x178, 0);
            below_aircraft(st + 0x16e);
        } else {
            FLAG(1);
            SET_G32(G_BANK, 0x38e0000);
            SET_V32(T(4), 0xe38);
            SET_S32(0x130, 0);
            below_aircraft(st + 0x126);
        }
    }
    if (!tick(0x5082, 0x20000)) {
        if ((int32_t)V32(0x5038) <= 1) {
            SET_V32(0x5038, V32(0x5038) + 1);
            SET_V16(T(0x1e), V16(T(0x1e)) >> 1);
            SET_V16(T(0x1c), V16(T(0x1c)) - 1);
        }
        if (!tick(0x5086, 0x40000) && (int32_t)V32(0x5038) <= 2) {
            SET_V32(0x5038, V32(0x5038) + 1);
            SET_V16(T(0x1e), V16(T(0x1e)) >> 1);
            SET_V16(T(0x1c), V16(T(0x1c)) - 1);
        }
    }
    if (V16(T(0x20)) < 0x12c) {
        if ((int32_t)G32(G_PITCH) <= 0xb60000) {
            end_crash();
            return;
        }
        SET_G32(G_PITCH, G32(G_PITCH) - 0x12c0000);
        SET_S32(0x108, G32(G_PITCH));
        SET_S32(0x150, G32(G_PITCH));
        if ((int32_t)G32(G_BANK) <= 0)
            SET_G32(G_BANK, G32(G_BANK) + 0x12c0000);
        else
            SET_G32(G_BANK, G32(G_BANK) - 0x12c0000);
        return;
    }
    shrink(0x506e, V16(T(0x1c)));
    step_north(GLOBAL + G_POS, V16(0x506e));
    shrink(0x5068, V16(T(0x1c)));
    eax = step_east(GLOBAL + G_POS, V16(0x5068));
    decay(T(0x20), V16(T(0x1e)));
    if ((int32_t)V32(0x5058) <= 0) {
        /* RETAIL BUG (kept): CRASH-HEADING-LEFTOVER -- the heading turns by the high half of
         * eax, left over from the east step (0x2e19 returns nothing there) */
        SET_G32(G_HEADING, G32(G_HEADING) + (eax >> 16));
        SET_S32(0x158, G32(G_HEADING));
        SET_S32(0x17c, G32(G_HEADING));
        place(1, st + 0x138);
        SET_S32(0x154, V32(T(0xc)));
        place_flat(4, st + 0x15c);
    } else {
        SET_S32(0x110, G32(G_HEADING));
        SET_S32(0x134, G32(G_HEADING));
        place(0xffff, st + 0xf0);
        SET_S32(0x10c, V32(T(4)));
        place_flat(0xfffc, st + 0x114);
    }
}

/* 0x1740: |bank| over 30 degrees: the aircraft cartwheels, both wings go */
static void crash_roll(uint8_t *st)
{
    if ((int32_t)V32(0x5038) <= 0) {
        SET_V32(0x5038, V32(0x5038) + 1);
        SET_V16(0x5070, sar16(V16(0x5068)));
        SET_V16(0x5074, sar16(V16(0x506e)));
        FLAG(4);
        SET_G32(G_PITCH, 0xe380000);
        if ((int32_t)V32(0x5058) <= 0) {
            FLAG(2);
            FLAG(0x20);
            SET_G32(G_PITCH, 0xf1c80000u);
            place(2, st + 0x15c);
            drop_piece(st + 0x16e);
            SET_S32(0x174, 0);
            SET_S32(0x178, 0);
        } else {
            FLAG(1);
            FLAG(0x10);
            SET_G32(G_BANK, 0xe380000);
            place(0xfffe, st + 0x114);
            drop_piece(st + 0x126);
            SET_S32(0x12c, 0);
            SET_S32(0x130, 0);
        }
    }
    if ((int16_t)spread(V16(0x5070), V16(0x5074)) >= 3) {
        shrink(0x5074, 2);
        step_north(GLOBAL + G_POS, V16(0x5074));
        shrink(0x5070, 2);
        step_east(GLOBAL + G_POS, V16(0x5070));
        if ((int32_t)V32(0x5038) <= 1) {
            int left = (int32_t)V32(0x5058) <= 0;
            if (tick(0x5082, 0x20000)) {
                place(left ? 1 : 0xffff, st + 0x138);
                return;
            }
            if (left) {
                drop_piece(st + 0x14a);
                SET_S32(0x150, 0);
                SET_S32(0x154, 0);
            } else {
                drop_piece(st + 0x102);
                SET_S32(0x108, 0);
                SET_S32(0x10c, 0);
            }
            SET_V32(0x5038, V32(0x5038) + 1);
        }
    }
    SET_V32(0x508a, V32(0x508a) + G16(G_DELTA));
    if ((int32_t)V32(0x508a) >= 0x50000) {
        SET_V32(0x508a, 0);
        GLOBAL[G_CRASH] = 4;
        return;
    }
    settle(st + 0xf0);
    settle(st + 0x114);
    settle(st + 0x138);
    settle(st + 0x15c);
    settle_aircraft();
    if ((int32_t)G32(G_PITCH) > 0xc80000)
        SET_G32(G_PITCH, G32(G_PITCH) - 0xc80000);
    else if ((int32_t)G32(G_PITCH) <= (int32_t)0xff380000u)
        SET_G32(G_PITCH, G32(G_PITCH) + 0xc80000);
    if ((int32_t)V32(0x5058) >= 0) {
        if ((int32_t)G32(G_BANK) >= (int32_t)0xf5560000u)
            SET_G32(G_BANK, G32(G_BANK) - 0x12c0000);
    } else {
        if ((int32_t)G32(G_BANK) <= 0xaaa0000)
            SET_G32(G_BANK, G32(G_BANK) + 0x12c0000);
    }
}

/* 0x1a00: pitch 15 to 40 degrees: nose in, the aircraft goes over on its back.
 * eax is what retail's update left in eax: GLOBAL +0xd48. */
static void crash_nose(uint8_t *st, uint32_t eax)
{
    int32_t pitch;
    if ((int32_t)V32(0x5038) <= 0) {
        SET_V32(0x5038, V32(0x5038) + 1);
        SET_V16(0x5070, sar16(V16(0x5068)));
        SET_V16(0x5076, sar16(V16(0x5070)));
        SET_V16(0x5074, sar16(V16(0x506e)));
        SET_V16(0x507a, sar16(V16(0x5074)));
        SET_V16(T(0x22), spread(V16(0x5068), V16(0x506e)) << 4);
        SET_V32(T(0), 0x80000000u - V32(0x5054));
        SET_V32(T(0x14), 0);
        SET_V32(T(0x18), 0);
        FLAG(0x40);
        FLAG(4);
        FLAG(8);
        if ((int32_t)V32(0x5058) <= 0) {
            FLAG(2);
            FLAG(0x20);
            eax = place(8, st + 0x15c);
            drop_piece(st + 0x16e);
            SET_S32(0x174, 0);
            SET_S32(0x178, 0);
        } else {
            FLAG(1);
            FLAG(0x10);
            eax = place(0xfff8, st + 0x114);
            drop_piece(st + 0x126);
            SET_S32(0x12c, 0);
            SET_S32(0x130, 0);
        }
    }
    if ((int16_t)V16(T(0x22)) >= 0x320) {
        decay(T(0x22), 0x20);
        /* RETAIL BUG (kept): CRASH-PITCH-LEFTOVER -- the pitch turns by the high half of eax:
         * GLOBAL +0xd48's after the first frame, UTIL's Linkage address in the first */
        SET_G32(G_PITCH, G32(G_PITCH) + (eax >> 16));
        lift(GLOBAL + G_POS, 1);
        shrink(0x5074, 2);
        step_north(GLOBAL + G_POS, V16(0x5074));
        shrink(0x5070, 2);
        step_east(GLOBAL + G_POS, V16(0x5070));
        if (G32(G_PITCH) > V32(T(0)) && (int32_t)V32(0x5038) <= 1) {
            if ((int32_t)V32(0x5058) <= 0) {
                FLAG(1);
                FLAG(0x10);
                SET_V32(T(0x14), 0xe38);
                place(0xfff8, st + 0x114);
                below_crash(st + 0x126);
                SET_S32(0x12c, 0);
                SET_S32(0x130, 0);
            } else {
                FLAG(2);
                FLAG(0x20);
                SET_V32(T(0x18), 0xf1c80000u);
                place(8, st + 0x15c);
                below_crash(st + 0x16e);
                SET_S32(0x174, 0);
                SET_S32(0x178, 0);
            }
            SET_V32(0x5038, V32(0x5038) + 1);
        }
        place(1, st + 0x138);
        place(0xffff, st + 0xf0);
        return;
    }
    FLAG(0x80);
    if ((int32_t)G32(G_BANK) > 0x1f40000)
        SET_G32(G_BANK, G32(G_BANK) - 0x1f40000);
    else if ((int32_t)G32(G_BANK) <= (int32_t)0xfe0c0000u)
        SET_G32(G_BANK, G32(G_BANK) + 0x1f40000);
    pitch = (int32_t)G32(G_PITCH);
    if (pitch > 0xaaa0000 ? pitch > 0x75560000 : pitch < (int32_t)0xf5560000u && pitch < (int32_t)0x8aaa0000u) {
        /* over on its back */
        SET_G32(G_PITCH, 0x78e40000);
        SET_S32(0x108, 0x78e40000);
        SET_S32(0x150, 0x78e40000);
        SET_S32(0x10c, 0);
        SET_S32(0x154, 0);
    } else if (pitch > 0xaaa0000 || pitch < (int32_t)0xf5560000u) {
        /* still going over */
        SET_G32(G_PITCH, G32(G_PITCH) + 0x71c0000);
        lift(GLOBAL + G_POS, 1);
        place(1, st + 0x138);
        place(0xffff, st + 0xf0);
        return;
    } else {
        /* down the right way up */
        SET_G32(G_PITCH, 0x38e0000);
        SET_S32(0x108, 0x38e0000);
        SET_S32(0x150, 0x38e0000);
        SET_S32(0x10c, V32(T(0x14)));
        SET_S32(0x154, V32(T(0x18)));
    }
    end_crash();
}

/* 0x1dfb: pitch 40 degrees or more: the aircraft breaks up */
static void crash_dive(uint8_t *st)
{
    uint16_t ax;
    SET_S16(CRASH_PHASE, 2);
    if (V32(0x5038) == 0) {
        fan(clamp(V16(0x5068), 8), 0x5070, 0x507c, 0x5076);
        fan(clamp(V16(0x506e), 8), 0x5074, 0x5080, 0x507a);
        ax = (uint16_t)-V16(0x506c);
        ax = sar16((uint16_t)(sar16(ax) + ax));
        SET_V16(0x5072, ax);
        SET_RATES(group_a, (uint32_t)ax << 16);
        ax = (uint16_t)(ax + 1);
        SET_V16(0x5078, ax);
        SET_RATES(group_b, (uint32_t)ax << 16);
        ax = (uint16_t)(ax + 1);
        SET_V16(0x507e, ax);
        SET_RATES(group_c, (uint32_t)ax << 16);
        gather(st);
        SET_V32(0x5038, V32(0x5038) + 1);
    }
    debris_pass(st);
}

/* the speeds an impact throws the debris at, away from the side the aircraft moved */
static void impact_speeds(uint16_t a, uint16_t b, uint16_t c, int below)
{
    uint32_t sets[2][3] = {{0x5070, 0x5076, 0x507c}, {0x5074, 0x507a, 0x5080}};
    uint16_t speeds[2] = {V16(0x5068), V16(0x506e)};
    int k, j;
    for (k = 0; k < 2; k++) {
        SET_V16(sets[k][0], a);
        SET_V16(sets[k][1], b);
        SET_V16(sets[k][2], c);
        if (below ? (int16_t)speeds[k] >= 0 : (int16_t)speeds[k] <= 0)
            for (j = 0; j < 3; j++)
                SET_V16(sets[k][j], (uint16_t)-V16(sets[k][j]));
    }
}

/* 0x27dd: GLOBAL +0x834 is 6 */
static void crash_impact_6(uint8_t *st)
{
    SET_S16(CRASH_PHASE, 2);
    if (V32(0x5038) == 0) {
        SET_V32(0x5038, V32(0x5038) + 1);
        impact_speeds(3, 2, 1, 1);
        SET_RATES(group_a, 0);
        SET_RATES(group_b, 0);
        SET_RATES(group_c, 0);
        gather(st);
    }
    debris_pass(st);
}

/* 0x28bf: GLOBAL +0x834 is 12 */
static void crash_impact_12(uint8_t *st)
{
    SET_S16(CRASH_PHASE, 2);
    if (V32(0x5038) == 0) {
        SET_V32(0x5038, V32(0x5038) + 1);
        impact_speeds(2, 1, 0, 0);
        SET_RATES(group_a, 0);
        SET_RATES(group_b, 0);
        SET_RATES(group_c, 0);
        gather(st);
    }
    debris_pass(st);
}

/* 0x1279: which animation */
static void crash_step(uint8_t *st, uint32_t eax)
{
    uint32_t bank;
    if (G32(G_IMPACT) == 6)
        crash_impact_6(st);
    else if (G32(G_IMPACT) == 12)
        crash_impact_12(st);
    else if (V32(0x5054) >= 0xaaa0000)
        if (V32(0x5054) < 0x1c700000)
            crash_nose(st, eax);
        else
            crash_dive(st);
    else {
        bank = (int32_t)V32(0x5058) < 0 ? (uint32_t)-V32(0x5058) : V32(0x5058);
        if (bank <= 0x71c0000)
            crash_slide(st);
        else if (bank <= 0x15550000)
            crash_wing(st);
        else
            crash_roll(st);
    }
}

/* ---- the entry points -------------------------------------------------------------------- */

/* Linkage +0x1c, once until detach: register the draw, take the flight model's snapshot */
void STDCALL crash_init(uint8_t *st)
{
    if (V32(0x5034) != 0)
        return;
    SET_V32(0x5034, 1);
    chain_add();
    ((service1)SERVICE((uint8_t *)(uintptr_t)G32(0x11d4), 0x18))(PTR(V(0x503c)));
    SET_V32(0x5038, 0);
    SET_S16(CRASH_FLAGS, 0);
    SET_S16(CRASH_PHASE, 1);
    SET_G16(0x898, 0);
    SET_G32(0x26c, 1);
    if (G32(G_IMPACT) != 6 && G32(G_IMPACT) != 12)
        copy_bytes(GLOBAL + G_POS, V(0x503c), 0x24);
    SET_V16(0x5068, clamp(V16(0x5060), 0x32));
    SET_V16(0x506c, clamp(V16(0x5062), 0x14));
    SET_V16(0x506e, clamp(V16(0x5064), 0x32));
}

/* Linkage +0x20 */
void crash_detach(void)
{
    chain_remove();
    if ((int8_t)GLOBAL[G_CRASH] >= 6)
        GLOBAL[G_CRASH] = 5;
    SET_G32(0x26c, 0);
    SET_V32(0x5034, 0);
}

/* header +0x10, called by WINDOW each frame */
void STDCALL crash_update(uint8_t *st)
{
    if (GLOBAL[G_CRASH] != 6)
        return;
    SET_S16(0x60, G16(0x11b6));
#if CRASH_MODEL == 1
    SET_S16(0x7e, G16(0x11b8));
#endif
    host_move(st + 4, GLOBAL + G_POS, 0x24);
    host_move(st + 0x28, GLOBAL + 0x5b0, 0x18);
    SET_S32(0x68, G32(0xd48));
    crash_step(st, G32(0xd48));
}

/* the CHAIN list 15 callback's choice: the program and state for G3D +0x18, which
 * crash_chain_entry calls with them. Returns G3D's Linkage. */
void *STDCALL crash_chain_prepare(uint32_t *call)
{
    uint8_t *st = (uint8_t *)(uintptr_t)G32(0x11e4);
    call[0] = PTR(V(rd16(st + CRASH_PHASE) == 2 ? PROGRAM_DEBRIS : PROGRAM_WRECK));
    call[1] = PTR(st);
    return G3D;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
