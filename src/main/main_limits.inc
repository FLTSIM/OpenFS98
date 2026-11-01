/* MAIN.DLL: command 0x1204, the aircraft's limits dialog. MSVC C, .text+0x2ee0..+0x3b60:
 * height, visibility and speed fields in feet or metres (GLOBAL +0xc18 == 2) with the
 * checks that hold each one to its table's range. */

#define LM_DLG  V(0x20800768)

/* MSVCRT's, as retail imports them */
extern int __cdecl sprintf(char *, const char *, ...);
extern int __cdecl sscanf(const char *, const char *, ...);

static const double lm_ft = 3.28084, lm_16 = 1.52587890625e-07, lm_half = -0.5, lm_6 = 1.52587890625e-06, lm_m = 0.3047999902464003,
                    lm_0 = 0.0, lm_p5 = 0.5, lm_3 = 0.00152587890625, lm_01 = 0.01, lm_1 = 0.1, lm_10 = 10.0, lm_k = 1000.0,
                    lm_nk = -1000.0, lm_02 = 0.02, lm_50 = 50.0, lm_c = 100.0;
#define N_CHECK  CALL(NUMERIC_, 0x20, f4)   /* (text, type, &value, limits) */

/* The argument's slot as scratch, as retail's process holds it */
#define LM_SLOT(block)  ((block) - D + MAIN_DATA_VA)

/* .text+0x2fd0: the speed field's value re-expressed in the stall field. */
static uint32_t lm_speed(uint32_t block)
{
    uint32_t slot = LM_SLOT(block), r;
    long double x;

    r = N_CHECK(M32(block + 0x4c), 0x1003f, ADDR(&slot), V(0x20800748));
    if (r != 0)
        return r;
    x = (long double)slot;
    if (M16(G + 0xc18) != 2)
        x *= lm_ft;
    x *= lm_16;
    x -= lm_half;
    slot = main_ftol(x) * 100;
    return N_PUT(ADDR(&slot), 0x1001a, M32(V(0x208007b4)));
}

/* .text+0x34d0: round the field to two places */
static uint32_t lm_round_visibility(uint32_t block)
{
    uint32_t slot = LM_SLOT(block), r;
    long double x;

    r = N_CHECK(M32(block + 0x90), 0x1c, ADDR(&slot), V(0x20800760));
    if (r != 0)
        return r;
    x = (long double)slot * lm_3 - lm_half;
    x = (long double)main_ftol(x) * lm_01;
    sprintf((char *)(uintptr_t)M32(block + 0x90), (const char *)(uintptr_t)V(0x20800924), (double)x);
    return N_CHECK(M32(block + 0x90), 0x1c, ADDR(&slot), V(0x20800760));
}

/* .text+0x3580: and to tenths of the upper word, held to the maximum */
static uint32_t lm_round_ceiling(uint32_t block)
{
    uint32_t slot = LM_SLOT(block), r, v;
    long double x;

    r = N_CHECK(M32(block + 0x7c), 0x40, ADDR(&slot), V(0x20800758));
    if (r != 0)
        return r;
    x = (long double)(slot >> 16) * lm_1 - lm_half;
    x = (long double)main_ftol(x) * lm_10;
    v = main_ftol(x) << 16;
    slot = v > M32(V(0x2080075c)) ? M32(V(0x2080075c)) : v;
    N_PUT(ADDR(&slot), 0x10040, M32(block + 0x7c));
    return N_CHECK(M32(block + 0x7c), 0x40, ADDR(&slot), V(0x20800758));
}

/* A round half away from zero, as .text+0x3640 does it twice */
static int32_t lm_round(long double x)
{
    x -= x < lm_0 ? lm_p5 : lm_half;
    return (int16_t)main_ftol(x);
}

/* .text+0x3640: the speed, to fifties under a thousand and to the hundred above,
 * held to the limits for the units. */
static uint32_t lm_round_speed(uint32_t block)
{
    uint32_t slot = LM_SLOT(block), r, limits = M16(G + 0xc18) == 2 ? V(0x20800754) : V(0x20800750), clamp;
    long double x;

    r = N_CHECK(M32(block + 0xc4), 0xf, ADDR(&slot), limits);
    if (r != 0)
        return r;
    x = (long double)(int32_t)(int16_t)slot;
    if (x < lm_k && !(x <= lm_nk))
        x = (long double)lm_round(x * lm_02) * lm_50;
    else
        x = (long double)lm_round(x * lm_01) * lm_c;
    slot = main_ftol(x);
    /* metres only when +0xc18 is 2, but the clamp takes any non-zero for them */
    clamp = M16(G + 0xc18) != 0 ? V(0x20800754) : V(0x20800750);
    if ((int16_t)slot > (int16_t)M16(clamp + 2))
        slot = (slot & 0xffff0000u) | M16(clamp + 2);
    else if ((int16_t)slot < (int16_t)M16(clamp))
        slot = M32(clamp);
    N_PUT(ADDR(&slot), 0x1000f, M32(block + 0xc4));
    return N_CHECK(M32(block + 0xc4), 0xf, ADDR(&slot), limits);
}

/* .text+0x3490 */
static uint32_t lm_round_all(uint32_t block)
{
    uint32_t a = lm_round_visibility(block), b, c;

    M32(block + 0x8c) = a;
    b = lm_round_ceiling(block);
    M32(block + 0x78) = b;
    c = lm_round_speed(block);
    M32(block + 0xc0) = c;
    return c | b | a;
}

/* .text+0x3450 */
static uint32_t FS98_STDCALL lm_check(uint32_t block)
{
    lm_speed(block);
    UTIL_PUT(V(0x20800850), 7, block, 1);
    return lm_round_all(block);
}

/* .text+0x3060 */
static uint32_t lm_load_fields(void)
{
    uint32_t gp = G, slot;
    double l8;
    long double x;

    M32(V(0x208007c4)) = 0;
    M32(V(0x20800824)) = M32(gp + 0x7ec);
    if (M32(gp + 0x768) == 1)
        M32(V(0x208007c4)) = 8;
    l8 = (double)((long double)(int32_t)M32(gp + 0x7f0) * lm_6);
    if (M16(gp + 0xc18) == 2) {
        l8 = (double)((long double)l8 * lm_m);
        H_LOADSTRING(0xf560, V(0x20800810), 0x10);
        N_PUT(V(0x20800754), 0x1000f, M32(V(0x20800834)));
        N_PUT(V(0x20800756), 0x1000f, M32(V(0x20800838)));
    } else {
        H_LOADSTRING(0xf561, V(0x20800810), 0x10);
        N_PUT(V(0x20800750), 0x1000f, M32(V(0x20800834)));
        N_PUT(V(0x20800752), 0x1000f, M32(V(0x20800838)));
    }
    x = (long double)l8;
    x -= l8 < lm_0 ? lm_p5 : lm_half;
    slot = main_ftol(x) * 10;
    N_PUT(ADDR(&slot), 0x1000f, M32(V(0x2080082c)));
    gp = G;
    M32(V(0x208007dc)) = 0;
    if (M32(gp + 0x76c) == 1) {
        M32(V(0x208007dc)) = 1;
        M32(V(0x208007c4)) |= 2;
    }
    if (M32(gp + 0x770) == 1) {
        M32(V(0x208007c4)) |= 4;
        M32(V(0x208007dc)) |= 1;
    }
    M32(V(0x20800808)) = M32(gp + 0x7e4);
    M32(V(0x2080080c)) = M32(gp + 0x7dc);
    N_PUT(gp + 0x7e8, 0x1001c, M32(V(0x208007f8)));
    N_PUT(V(0x20800760), 0x1001c, M32(V(0x20800800)));
    N_PUT(V(0x20800764), 0x1001c, M32(V(0x20800804)));
    N_PUT(G + 0x7e0, 0x10040, M32(V(0x208007e4)));
    N_PUT(V(0x20800758), 0x10040, M32(V(0x208007ec)));
    N_PUT(V(0x2080075c), 0x10040, M32(V(0x208007f0)));
    gp = G;
    if (M32(gp + 0x774) == 1)
        M32(V(0x208007c4)) |= 1;
    M32(V(0x208007cc)) = M32(gp + 0x810);
    M32(V(0x208007d4)) = M32(gp + 0x80c);
    if (M32(gp + 0x774) == 1) {
        M32(V(0x208007c4)) |= 0x10;
        if (M8(V(0x208007c4)) & 6)
            M32(V(0x208007c4)) |= 0x20;
    }
    lm_round_visibility(LM_DLG);
    lm_round_ceiling(LM_DLG);
    lm_round_speed(LM_DLG);
    return 0;
}

/* .text+0x2f20 */
static uint32_t lm_load(void)
{
    uint32_t r;

    M32(V(0x20800770)) = M16(G + 0xc18);
    UTIL_GET(V(0x20800850), 7, LM_DLG);
    H_LOADSTRING(M32(V(0x20800770)) == 2 ? 0xf562 : 0xf563, V(0x20800798), 0x10);
    N_PUT(V(0x20800748), 0x1003f, M32(V(0x208007bc)));
    N_PUT(V(0x2080074c), 0x1003f, M32(V(0x208007c0)));
    r = lm_load_fields();
    lm_speed(LM_DLG);
    return r;
}

/* .text+0x3320 */
static void lm_apply(void)
{
    uint32_t gp;
    uint32_t local = 0;

    if (M32(LM_DLG) == 1)
        UTIL_PUT(V(0x20800850), 7, LM_DLG, 0);
    gp = G;
    M32(gp + 0x810) = M32(V(0x208007cc));
    M32(gp + 0x80c) = M32(V(0x208007d4));
    M32(gp + 0x7dc) = M32(V(0x2080080c));
    M32(gp + 0x7ec) = M32(V(0x20800824));
    M32(gp + 0x7e4) = M32(V(0x20800808));
    N_GET(M32(V(0x208007f8)), 0x1c, G + 0x7e8);
    N_GET(M32(V(0x208007e4)), 0x40, G + 0x7e0);
    N_GET(M32(V(0x2080082c)), 0xf, ADDR(&local));
    gp = G;
    if (M16(gp + 0xc18) == 2)
        M32(gp + 0x7f0) = (uint32_t)((int32_t)(int16_t)((int32_t)(int16_t)local / 50) * 0xa40ad8);
    else
        M32(gp + 0x7f0) = (uint32_t)((int32_t)(int16_t)local << 0x10);
}

/* .text+0x2ee0, command 0x1204 */
static void command_1204(void)
{
    if (lm_load() == 0 && H_DIALOG(0x1204, LM_DLG) == 0 && M32(LM_DLG) == 1)
        lm_apply();
}

/* .text+0x37f0: an exchange: 0 show, 1 store, 2 check */
static uint32_t FS98_STDCALL lm_field_speed(uint32_t a, uint32_t block, uint32_t mode)
{
    uint32_t r, slot;

    (void)a;
    if (mode == 0) {
        UTIL_GET(V(0x20800910), 1, LM_DLG);
    } else if (mode == 1) {
        if (M32(block + 0x44) != 0)
            return UTIL_PUT(V(0x208008e0), 1, LM_DLG, 0);
    } else if (mode == 2) {
        if (M32(block + 0x44) != 0) {
            slot = LM_SLOT(block);
            r = N_CHECK(M32(block + 0x4c), 0x3f, ADDR(&slot), V(0x20800748));
            M32(block + 0x48) = r;
            return r;
        }
    }
    return 0;
}

/* .text+0x3880 */
static uint32_t FS98_STDCALL lm_field_flaps(uint32_t a, uint32_t block, uint32_t mode)
{
    (void)a;
    if (mode == 0)
        UTIL_GET(V(0x208008f8), 1, LM_DLG);
    else if (mode == 1) {
        if (M32(block + 0x18) == 3)
            return UTIL_PUT(V(0x208008f8), 1, block, 0);
    } else if (mode == 2)
        return UTIL_PUT(V(0x208008f8), 1, LM_DLG, 1);
    return 0;
}

/* .text+0x3900: three flags as a radio group */
static uint32_t FS98_STDCALL lm_field_group3(uint32_t a, uint32_t block, uint32_t mode)
{
    uint32_t gp = G;

    (void)a;
    if (mode == 0) {
        if (M32(gp + 0x7c0) == 1)
            M32(block + 0x18) = 1;
        else if (M32(gp + 0x7d8) == 1)
            M32(block + 0x18) = 2;
        else if (M32(gp + 0x7c8) == 1)
            M32(block + 0x18) = 3;
        else
            M32(block + 0x18) = 0;
    } else if (mode == 1 && M32(block + 0x18) <= 3) {
        M32(gp + 0x7c0) = M32(block + 0x18) == 1;
        M32(gp + 0x7d8) = M32(block + 0x18) == 2;
        M32(gp + 0x7c8) = M32(block + 0x18) == 3;
    }
    return 0;
}

/* .text+0x3a50: and four; the first choice sets two of them. */
static uint32_t FS98_STDCALL lm_field_group4(uint32_t a, uint32_t block, uint32_t mode)
{
    uint32_t gp = G, v;

    (void)a;
    if (mode == 0) {
        if (M32(gp + 0x7fc) == 1)
            M32(block + 0xe0) = 1;
        else if (M32(gp + 0x800) == 1)
            M32(block + 0xe0) = 2;
        else if (M32(gp + 0x7c4) == 1)
            M32(block + 0xe0) = 3;
        else if (M32(gp + 0x804) == 1)
            M32(block + 0xe0) = 4;
        else
            M32(block + 0xe0) = 0;
    } else if (mode == 1 && M32(block + 0xe0) <= 4) {
        v = M32(block + 0xe0);
        M32(gp + 0x7fc) = v == 1;
        M32(gp + 0x800) = v == 1 || v == 2;
        M32(gp + 0x7c4) = v == 3;
        M32(gp + 0x804) = v == 4;
    }
    return 0;
}
