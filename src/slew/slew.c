/* SLEW.DLL -- module id 9.
 *
 * Slew is FS98's free-movement mode: the aircraft is dragged around the world
 * directly instead of being flown.  The module is one service -- a per-tick
 * integrator -- hung off Linkage slot 0x18, and it is small (1270 bytes of
 * .text in the retail build) precisely because it does nothing but read rate
 * inputs out of GLOBAL's shared state and accumulate them into the position
 * and attitude that live there too.
 *
 * It imports the host, GLOBAL and UTIL.  UTIL's services are called here
 * through their retail slots without being understood: this is a hybrid
 * decompilation, so UTIL stays the retail binary and only its calling contract
 * has to be right.  Slot 0x2c returns a sine/cosine pair for a 16 bit angle,
 * slot 0x44 converts a position, slot 0x18 is a 64 bit square root.  They are
 * declared below as opaque stdcall functions -- stdcall because the retail
 * call sites restore the stack with `add esp,[esp]` over their own alignment
 * padding only, meaning the callee had already popped the arguments.
 *
 * Three retail behaviours below look like bugs.  They are reproduced, because
 * the point of this module is to behave as FS98 behaves; each is marked
 * QUIRK and explained in docs/modules/slew.md.
 */

#include <windows.h>

#include "fs98/linkage.h"
#include "fs98/global.h"

/* ---- UTIL services, called but not reimplemented -------------------------- */

typedef void (__stdcall *util_sincos_fn)(int16_t angle, int16_t *a, int16_t *b);
typedef void (__stdcall *util_xform_fn)(void *out_a, void *pos, void *out_b);
typedef int32_t (__stdcall *util_sqrt64_fn)(void *value_be);

#define UTIL_SINCOS(u)  (*(util_sincos_fn *)((unsigned char *)(u) + 0x2c))
#define UTIL_XFORM(u)   (*(util_xform_fn  *)((unsigned char *)(u) + 0x44))
#define UTIL_SQRT64(u)  (*(util_sqrt64_fn *)((unsigned char *)(u) + 0x18))

/* ---- module state --------------------------------------------------------- */

struct slew_private {
    int32_t unused_3000;
    int32_t alt_ceiling;   /* 0x3004: 30480 metres == 100000 feet */
    int32_t vel_x;         /* 0x3008 */
    int32_t vel_y;         /* 0x300c */
    int32_t vel_z;         /* 0x3010 */
    int16_t rate_side;     /* 0x3014 */
    int16_t rate_fwd;      /* 0x3016 */
    /* Sum of squares.  UTIL slot 0x18 reads this as an ordinary little-endian
     * 64 bit value, so 0x3018 is the low half and 0x301c the high half, and
     * each product's halves go to the right places.  What is wrong is the
     * direction of the carry: retail adds the high halves first and carries
     * into the *low* half, where a correct 64 bit add would do the opposite.
     * The low half's overflow is therefore dropped and a spurious carry is
     * injected above it.  Reproduced exactly; the square root is handed
     * whatever these bytes say. */
    uint32_t sq_3018;      /* low half;  receives each product's low dword  */
    uint32_t sq_301c;      /* high half; receives each product's high dword */
    int16_t fast;          /* 0x3020 */
    int16_t pad_3022;
};

static struct slew_private priv = { 0, 30480, 0, 0, 0, 0, 0, 0u, 0u, 0, 0 };

struct fs98_import ImportTable[] = {
    { FS98_MOD_HOST,   0 },
    { FS98_MOD_GLOBAL, 0 },
    { FS98_MOD_UTIL,   0 },
    { 0, 0 }
};

static void slew_noop(void) { }
static void slew_update(void);

struct {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    void *reserved[3];       /* slots 0x0c, 0x10, 0x14 -- null in the retail module */
    void (*update)(void);    /* slot 0x18 */
} Linkage = { FS98_MOD_SLEW, slew_noop, slew_noop, { 0, 0, 0 }, slew_update };

#define GLOBAL_LINK (ImportTable[1].link)
#define UTIL_LINK   (ImportTable[2].link)

/* ---- 48 bit position arithmetic -------------------------------------------
 *
 * Position is a 16 bit low half plus a 32 bit high half, updated with add/adc
 * and sub/sbb pairs, so the carry between the halves has to be explicit here.
 */

static void pos48_add(void *g, int lo_off, int hi_off, uint16_t lo, int32_t hi)
{
    uint32_t t = (uint32_t)FS98_G_U16(g, lo_off) + lo;
    FS98_G_U16(g, lo_off) = (uint16_t)t;
    FS98_G_S32(g, hi_off) += hi + (int32_t)(t >> 16);
}

static void pos48_sub(void *g, int lo_off, int hi_off, uint16_t lo, int32_t hi)
{
    uint32_t t = (uint32_t)FS98_G_U16(g, lo_off) - lo;
    FS98_G_U16(g, lo_off) = (uint16_t)t;
    FS98_G_S32(g, hi_off) -= hi + (int32_t)((t >> 16) & 1);
}

/* Fast slew multiplies the step by sixteen, as four 48 bit doublings. */
static void pos48_shl4(uint16_t *lo, int32_t *hi)
{
    int i;
    for (i = 0; i < 4; i++) {
        uint32_t t = (uint32_t)*lo << 1;
        *hi = (*hi << 1) + (int32_t)(t >> 16);
        *lo = (uint16_t)t;
    }
}

/* A velocity becomes a 48 bit position step as bits 16..63 of velocity*scale*2,
 * with the high half kept to 16 bits and sign extended -- the retail code does
 * the doubling with `adc dx,dx`, which is 16 bit. */
static void step48(int32_t vel, int32_t scale, uint16_t *lo, int32_t *hi)
{
    uint64_t p = (uint64_t)((int64_t)vel * scale * 2);
    *lo = (uint16_t)(p >> 16);
    *hi = (int16_t)(p >> 32);
}

/* The slew speed ramp.  Starting from zero, `value += value/4 + 1` is applied
 * once per speed step, so the notches get progressively further apart, and the
 * whole thing saturates at 32767 if the steps keep coming.  Step 26 gives 887;
 * anything from roughly 47 steps up is flat out.
 *
 * The retail loop looks at first glance as though it reloads its counter --
 * it branches back to a `movzx ecx,cx` -- but CX *is* ECX's own low half, and
 * `loop` has already decremented it, so that instruction is a no-op and the
 * loop runs exactly as many times as the step count says.
 */
static int16_t slew_rate_step(int16_t step)
{
    uint16_t v = 0;
    int n;

    if (step == 0)
        return 0;

    for (n = (step < 0) ? -(int)step : (int)step; n > 0; n--) {
        int32_t t = (int32_t)(int16_t)v + (int32_t)(int16_t)((v >> 2) + 1);
        if (t > 32767 || t < -32768) {   /* the retail `jo` path */
            v = 32767;
            break;
        }
        v = (uint16_t)t;
    }
    return (step < 0) ? (int16_t)-(int16_t)v : (int16_t)v;
}

/* ---- the tick ------------------------------------------------------------- */

static void slew_update(void)
{
    void *g = GLOBAL_LINK;
    void *u = UTIL_LINK;
    int16_t sin_h, cos_h;
    int32_t scale, prev;
    uint16_t lo;
    int32_t hi;
    unsigned char lon_in[8];
    unsigned char lon_out[8];
    int32_t speed;

    if (FS98_G_U8(g, FS98_G_SLEW_INACTIVE) != 0)
        return;

    /* consume the "zero the attitude" request */
    prev = FS98_G_S32(g, FS98_G_ATT_ZERO);
    FS98_G_S32(g, FS98_G_ATT_ZERO) = 0;
    if (prev != 0) {
        FS98_G_S32(g, FS98_G_PITCH)   = 0;
        FS98_G_S32(g, FS98_G_BANK)    = 0;
        FS98_G_S32(g, FS98_G_HEADING) = 0;
    }

    /* And the "recentre the rates" request.  The retail test is
     * `test WORD PTR [edi+0x5de], 0xff` -- a 16 bit operand against an 8 bit
     * mask, so only the low byte of the field asks for a reset and a value
     * like 0xc600 does nothing. */
    if ((FS98_G_U16(g, FS98_G_ATT_RESET) & 0x00ff) != 0) {
        FS98_G_S16(g, FS98_G_ATT_RESET)    = 0;
        FS98_G_S16(g, FS98_G_RATE_FWD)     = 0;
        FS98_G_S16(g, FS98_G_RATE_SIDE)    = 0;
        FS98_G_S16(g, FS98_G_RATE_BANK)    = 0;
        FS98_G_S16(g, FS98_G_RATE_HEADING) = 0;
        FS98_G_S16(g, FS98_G_RATE_PITCH)   = 0x4000;
        FS98_G_S16(g, FS98_G_RATE_VERT)    = 0x4000;
    }

    /* fast slew, gated on the current aircraft */
    priv.fast = 0;
    if (FS98_G_S32(g, FS98_G_MODE) >= 2) {
        unsigned char *ac = (unsigned char *)FS98_G_PTR(g, FS98_G_AIRCRAFT);
        if (ac && *(int32_t *)(ac + 0x10) == 1 && *(uint16_t *)(ac + 0x26) >= 0x700)
            priv.fast = 1;
    }

    /* Body-axis rates rotated into world axes by heading.  vel_x drives
     * latitude directly; vel_y goes through UTIL slot 0x44 first, which is
     * what makes a degree of longitude narrow with latitude. */
    priv.rate_fwd  = slew_rate_step((int8_t)(FS98_G_S16(g, FS98_G_RATE_FWD)  >> 8));
    priv.rate_side = slew_rate_step((int8_t)(FS98_G_S16(g, FS98_G_RATE_SIDE) >> 8));

    UTIL_SINCOS(u)(FS98_G_S16(g, FS98_G_HEADING_HI), &sin_h, &cos_h);

    priv.vel_x = (int32_t)priv.rate_side * sin_h + (int32_t)priv.rate_fwd * cos_h;
    priv.vel_y = (int32_t)priv.rate_side * cos_h - (int32_t)priv.rate_fwd * sin_h;

    scale = FS98_G_S16(g, FS98_G_TIME_SCALE);

    /* longitude: the step is handed to UTIL at +2/+4 of its input buffer and
     * the converted step comes back at +2/+4 of the output buffer */
    /* left uninitialised, as the retail code leaves them: UTIL reads +2/+4 of
     * the input and writes +2/+4 of the output, and touches nothing else */
    step48(priv.vel_y, scale, &lo, &hi);
    *(uint16_t *)(lon_in + 2) = lo;
    *(int32_t  *)(lon_in + 4) = hi;

    UTIL_XFORM(u)(lon_in, (unsigned char *)g + FS98_G_POS_BASE, lon_out);

    lo = *(uint16_t *)(lon_out + 2);
    hi = *(int32_t  *)(lon_out + 4);
    if (priv.fast)
        pos48_shl4(&lo, &hi);
    pos48_add(g, FS98_G_LON_LO, FS98_G_LON_HI, lo, hi);

    /* latitude */
    step48(priv.vel_x, scale, &lo, &hi);
    if (priv.fast)
        pos48_shl4(&lo, &hi);
    pos48_sub(g, FS98_G_LAT_LO, FS98_G_LAT_HI, lo, hi);

    /* altitude */
    {
        int32_t rate = (int32_t)FS98_G_S16(g, FS98_G_RATE_VERT) - 0x4000;
        uint64_t p = (uint64_t)((int64_t)rate * scale * 4);
        priv.vel_z = rate << 9;
        pos48_sub(g, FS98_G_ALT_LO, FS98_G_ALT_HI,
                  (uint16_t)(p >> 8), (int32_t)(p >> 24));
    }

    /* Floor: terrain height plus an offset, as a 48 bit value.  Hitting it
     * also cancels any descent -- the rate is clamped back to its 0x4000
     * centre, compared unsigned exactly as the retail code does. */
    {
        int32_t  ground = FS98_G_S32(g, FS98_G_GROUND_ALT);
        uint32_t off    = (uint32_t)FS98_G_S32(g, FS98_G_ALT_OFFSET);
        uint32_t t      = (uint32_t)(uint16_t)(ground << 8) + (uint16_t)off;
        uint16_t f_lo   = (uint16_t)t;
        int32_t  f_hi   = (ground >> 8) + (int32_t)(off >> 16) + (int32_t)(t >> 16);

        if (f_hi > FS98_G_S32(g, FS98_G_ALT_HI) ||
            (f_hi == FS98_G_S32(g, FS98_G_ALT_HI) &&
             f_lo > FS98_G_U16(g, FS98_G_ALT_LO))) {
            FS98_G_S32(g, FS98_G_ALT_HI) = f_hi;
            FS98_G_U16(g, FS98_G_ALT_LO) = f_lo;
            if (FS98_G_U16(g, FS98_G_RATE_VERT) >= 0x4000)
                FS98_G_U16(g, FS98_G_RATE_VERT) = 0x4000;
        }
    }

    /* Ceiling: 30480 metres, which is 100000 feet exactly.  Hitting it cancels
     * any climb. */
    if (priv.alt_ceiling <= FS98_G_S32(g, FS98_G_ALT_HI)) {
        FS98_G_S32(g, FS98_G_ALT_HI) = priv.alt_ceiling;
        FS98_G_U16(g, FS98_G_ALT_LO) = 0;
        if (FS98_G_U16(g, FS98_G_RATE_VERT) <= 0x4000)
            FS98_G_U16(g, FS98_G_RATE_VERT) = 0x4000;
    }

    /* Attitude.  Pitch wraps through the vertical, and when it does, bank and
     * heading both flip by half a turn -- the usual gimbal handling. */
    {
        int32_t dp = ((int32_t)FS98_G_S16(g, FS98_G_RATE_PITCH) - 0x4000) << 8;
        int32_t p;
        if ((int32_t)(FS98_G_S32(g, FS98_G_BANK) + 0x40000000) < 0)
            dp = -dp;
        p = dp + FS98_G_S32(g, FS98_G_PITCH);
        if (!((p >= 0 && p <= 0x40000000) ||
              (p <  0 && p >= (int32_t)0xc0000000))) {
            p = -p + (int32_t)0x80000000;
            FS98_G_S32(g, FS98_G_BANK)    += (int32_t)0x80000000;
            FS98_G_S32(g, FS98_G_HEADING) += (int32_t)0x80000000;
        }
        FS98_G_S32(g, FS98_G_PITCH)    = p;
        FS98_G_S32(g, FS98_G_BANK)    += (int32_t)FS98_G_S16(g, FS98_G_RATE_BANK) << 14;
        FS98_G_S32(g, FS98_G_HEADING) -= (int32_t)FS98_G_S16(g, FS98_G_RATE_HEADING) << 16;
    }

    FS98_G_S32(g, 0x0d2c) = 0;

    /* published vertical speed */
    priv.vel_z = -priv.vel_z;
    {
        int32_t vs = priv.vel_z >> 8;
        if (vs > 0x7fff)
            vs = 0x7fff;
        FS98_G_S32(g, FS98_G_VERT_SPEED) = vs;
    }

    /* QUIRK 2.  Three conditional negations stood here, meant to take the
     * magnitude of each velocity component before squaring.  Each tests a
     * 32 bit operand against 0x0000ffff, so the sign flag is always clear and
     * the negation never runs.  Harmless either way -- the values are squared
     * immediately, which discards the sign regardless.  Omitted as dead. */

    {
        const int32_t v[3] = { priv.vel_x, priv.vel_y, priv.vel_z };
        uint64_t p = (uint64_t)((int64_t)v[0] * v[0]);
        int i;
        priv.sq_301c = (uint32_t)(p >> 32);
        priv.sq_3018 = (uint32_t)p;
        for (i = 1; i < 3; i++) {
            uint64_t t;
            p = (uint64_t)((int64_t)v[i] * v[i]);
            t = (uint64_t)priv.sq_301c + (uint32_t)(p >> 32);
            priv.sq_301c = (uint32_t)t;
            priv.sq_3018 += (uint32_t)p + (uint32_t)(t >> 32);
        }
    }

    speed = UTIL_SQRT64(u)(&priv.sq_3018);

    {
        int32_t s = speed >> 8;
        if (priv.fast)
            s <<= 4;
        FS98_G_S32(g, FS98_G_SPEED_OUT) = s;
    }

    /* QUIRK 3.  The clamp tests against 0x3fffff but assigns 0x3ffff, a factor
     * of sixteen apart, so a fast-slew speed above the threshold is replaced by
     * a value sixteen times smaller instead of being limited to it.  Reproduced
     * as written. */
    if (priv.fast) {
        if ((uint32_t)speed >= 0x3fffff)
            speed = 0x3ffff;
        speed <<= 4;
    }
    speed >>= 8;
    FS98_G_S32(g, FS98_G_GROUNDSPEED_A) = speed;
    FS98_G_S32(g, FS98_G_GROUNDSPEED_B) = speed;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
