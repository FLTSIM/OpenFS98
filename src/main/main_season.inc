/* MAIN.DLL slice 6: +0x74, the season, the zone and the clock.
 *
 * Retail .text+0x18781 with its helpers .text+0x18994 (the minutes-to-hours
 * normaliser) and .text+0x18394 (the position snapshot), assembly.  Loading
 * calls it (ADVDRV and MULTIP too); slice 2 still reaches it through the seam
 * the harness patches in, so this slice is driven on its own, through the
 * export main_season, against models of the two FE services it uses.
 *
 * It sets the time-zone offset from the longitude (15 degrees an hour,
 * halved), then lets FE's nearest `d' records override it by priority, then
 * trims it by a daylight rule whose season window is a .text table.  Finally
 * it converts between local and zulu time: with its first argument zero (and
 * a `d' record ever seen) local drives zulu, otherwise zulu drives local.
 */

#define S_FOUND_D   V(0x20807870)   /* a `d' record was seen this pass */
#define S_HAVE_D    V(0x20807874)   /* a `d' record has ever been seen */
#define S_TZ_MIN    V(0x20807486)   /* the zone offset, in minutes */
#define S_TZ_PRIO   V(0x20807488)   /* the chosen record's priority */
#define S_TZ_DST    V(0x20807489)   /* its daylight rule, 1..5 */
#define S_POS_SNAP  V(0x208074ea)   /* the position snapshot */
#define S_FE_REC    V(0x2080751a)   /* FE +0x20's record pointer */
#define S_FE_STATE  V(0x2080751e)   /* FE +0x20's enumeration state */
#define DST_TABLE   0x207f8eb0u     /* in retail's .text: 5 {start,end,off} */

/* a word of the daylight table, read as retail's process holds it (.text). */
static uint16_t dst_word(uint32_t a)
{
    return (uint16_t)(retail_byte_at(a) | (uint16_t)retail_byte_at(a + 1) << 8);
}

/* .text+0x18394: GLOBAL's position (+0x564/+0x56c/+0x574) into a buffer. */
static void season_snapshot(uint32_t buf)
{
    uint32_t gp = G;

    M32(buf) = M32(gp + 0x564);
    M32(buf + 4) = M32(gp + 0x56c);
    M32(buf + 8) = M32(gp + 0x574);
}

/* .text+0x18994: a signed count of minutes into the day's hours and minutes,
 * al the hours and ah the minutes, under the host's divide repair (a count
 * that still will not fit after one wrap divides into all ones).  The divisor
 * is 0x3c, which the one caller leaves in bl. */
static uint16_t season_hm(int16_t minutes)
{
    uint16_t ax = (uint16_t)minutes;

    if ((int16_t)ax < 0)
        ax = (uint16_t)(ax + 0x5a0);
    if ((int16_t)ax >= (int16_t)0x5a0)
        ax = (uint16_t)(ax - 0x5a0);
    if (ax / 60u > 0xff)
        return 0xffff;
    return (uint16_t)((ax % 60u) << 8 | ax / 60u);
}

/* .text+0x18781 == Linkage +0x74: (local_to_zulu, day_of_year). */
static uint32_t FS98_STDCALL nav_season(uint32_t a, uint32_t b)
{
    uint32_t gp = G;
    uint16_t ax, day = (uint16_t)b;
    uint8_t dst;

    M32(S_FOUND_D) = 0;

    /* the zone offset from the longitude */
    ax = (uint16_t)(((M32(gp + 0x56c) >> 16) & 0xffff) / 0x555u);
    if (ax >= 0x17)
        ax = (uint16_t)(ax - 0x30);
    ax = (uint16_t)(-(int16_t)ax);
    ax = (uint16_t)((int16_t)ax >> 1);
    ax = (uint16_t)(ax * 0x3cu);
    M16(S_TZ_MIN) = ax;
    M8(S_TZ_PRIO) = 0;
    M8(S_TZ_DST) = 0;
    M16(gp + 0x244) = 0;

    season_snapshot(S_POS_SNAP);

    /* FE's nearest zone records; the best priority wins */
    CALL(FE, 0x54, f3)(gp + 0x560, 5, gp + 0x3e4);
    ax = 0;
    for (;;) {
        uint32_t rec;

        if (CALL(FE, 0x20, f3)(ax, S_FE_REC, S_FE_STATE) == 0)
            break;
        rec = M32(S_FE_REC);
        if (M8(rec) == 0x64) {
            M32(S_FOUND_D) = 1;
            if (M8(rec + 0x10) > M8(S_TZ_PRIO)) {
                M16(S_TZ_MIN) = M16(rec + 0xe);
                M8(S_TZ_PRIO) = M8(rec + 0x10);
                M8(S_TZ_DST) = M8(rec + 0x11);
            }
        }
        ax = 1;
    }

    /* a daylight rule the day of the year falls inside trims the offset */
    M16(gp + 0x244) = 0;
    dst = M8(S_TZ_DST);
    if ((int8_t)dst > 0 && (int8_t)dst <= 5) {
        uint32_t e = DST_TABLE + 6u * (uint8_t)(dst - 1);
        if ((int16_t)day >= (int16_t)dst_word(e) && (int16_t)day < (int16_t)dst_word(e + 2)) {
            M16(gp + 0x244) = 1;
            M16(S_TZ_MIN) = (uint16_t)(M16(S_TZ_MIN) - dst_word(e + 4));
        }
    }
    M16(gp + 0x246) = M16(S_TZ_MIN);

    if (M32(S_HAVE_D) != 0 && a == 0) {
        uint16_t m = (uint16_t)(0x3cu * M8(gp + 0x23b));
        m = (uint16_t)(m + M8(gp + 0x23c));
        m = (uint16_t)(m - M16(gp + 0x246));
        m = season_hm((int16_t)m);
        M8(gp + 0x239) = (uint8_t)(m >> 8);
        M8(gp + 0x238) = (uint8_t)m;
    } else {
        uint16_t m = (uint16_t)(0x3cu * M8(gp + 0x238));
        m = (uint16_t)(m + M8(gp + 0x239));
        m = (uint16_t)(m + M16(gp + 0x246));
        m = season_hm((int16_t)m);
        M8(gp + 0x23c) = (uint8_t)(m >> 8);
        M8(gp + 0x23b) = (uint8_t)m;
    }

    if (M32(S_FOUND_D) != 0)
        M32(S_HAVE_D) = 1;
    return 0;
}

/* .text+0x16ddf == Linkage +0xa4: the zulu clock as digits.  Each of the
 * three bytes at GLOBAL +0x238 (hours), +0x239 (minutes) and +0x23a
 * (seconds) becomes a two-character decimal at +0xc20, +0xc23 and +0xc26.
 * `aam 0xa` splits the byte into tens and units, which `xchg`/`add 0x3030`
 * put low-then-high as ASCII; a byte past 99 keeps retail's result. */
static uint16_t clock_digits(uint8_t v)
{
    return (uint16_t)((uint16_t)((v % 10) + 0x30) << 8 | ((v / 10) + 0x30));
}

static uint32_t FS98_STDCALL nav_clock_ascii(void)
{
    uint32_t gp = G;

    M16(gp + 0xc26) = clock_digits(M8(gp + 0x23a));
    M16(gp + 0xc23) = clock_digits(M8(gp + 0x239));
    M16(gp + 0xc20) = clock_digits(M8(gp + 0x238));
    return 0;
}
