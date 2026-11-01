/* MAIN.DLL: command 0x1205, the aircraft's settings dialog: its engines. MSVC C,
 * .text+0x3bf0..+0x4f40. Up to five engines, each read and shown through a block in
 * GLOBAL (+0x41d0): throttle, fuel and manifold, three ways of saying the same power,
 * and the check at +0x4010 works out which of them the user last edited. */

#define EN_DLG  V(0x208009f8)
#define M64F(a) (*(volatile double *)(uintptr_t)(uint32_t)(a))

static const double en_scale = 0.00390625, en_gal = 0.11984, en_16 = 65536.0, en_pct = 83886.07, en_inv = 1.1920930376163764e-05,
                    en_half = -0.5, en_100 = 100.0, en_l = 3.785, en_01 = 0.01, en_101 = 101.0, en_200 = 200.0;

/* _ftol: fistp to 64 bits, the low word in eax */
static uint32_t main_ftol(long double x)
{
    return (uint32_t)(int64_t)x;
}

/* GLOBAL +0xaf4 scaled, and for metric too */
static long double en_size(void)
{
    long double t = (long double)(int32_t)(int16_t)M16(G + 0xaf4) * en_scale;

    if (M16(G + 0xc18) != 0)
        t *= en_gal;
    return t;
}

/* .text+0x4140: n low bits set */
static uint32_t en_mask(uint32_t n)
{
    uint32_t r = 0;

    if ((int32_t)n > 0)
        do {
            r = r * 2 | 1;
        } while (--n != 0);
    return r;
}

/* .text+0x4160: the Master switch: on the way out it stands for all of them only
 * if they were all set on the way in. */
static uint32_t FS98_STDCALL en_all(uint32_t a, uint32_t b, uint32_t mode)
{
    (void)a;
    (void)b;
    if (mode == 0) {
        if (M32(G + 0x888) == en_mask(M16(G + 0xaec)))
            M32(V(0x20800b30)) = 1;
    } else if (mode == 1 && M32(V(0x20800b30)) == 1) {
        M32(G + 0x888) = en_mask(M16(G + 0xaec));
    }
    return 0;
}

/* .text+0x41d0: engine i's block in GLOBAL */
static uint32_t en_block(uint32_t i)
{
    switch (i) {
    case 0: return G + 0xb84;
    case 1: return G + 0xb7c;
    case 2: return G + 0xb94;
    case 3: return G + 0xb9c;
    case 4: return G + 0xb74;
    }
    return 0;
}

/* .text+0x4260: the throttle's exchange: mode 0 shows it, 1 stores the percent, 2
 * reads the field for the check. */
static uint32_t FS98_STDCALL en_throttle(uint32_t i, uint32_t block, uint32_t mode)
{
    uint32_t p = en_block(i), slot = i, a = 0, b = 0, c = 0, at = i * 20, r;
    double l18, l20;
    long double x, y;

    if (p == 0)
        return 1;
    l18 = (double)en_size();
    if (mode == 2) {
        r = N_GET(M32(block + 0x180 + at), 0xc, ADDR(&slot));
        M32(block + (i + 0x13) * 20) = r;
        return r;
    }
    if (mode == 1) {
        if (M32(p + 4) == 0)
            return 0;
        r = N_GET(M32(V(0x20800b78) + at), 0xc, ADDR(&slot));
        if ((uint8_t)slot > 0x64)
            slot = (slot & 0xffffff00u) | 0x64;
        M32(p) = main_ftol((long double)(int32_t)(slot & 0xff) * en_pct);
        return r != 0 ? 1 : 0;
    }
    if (mode != 0)
        return 0;
    if (M32(p + 4) == 0) {
        slot &= 0xffffff00u;
    } else {
        x = (long double)M32(p + 4);
        y = (long double)M32(p) * en_inv - en_half;
        if (y > en_100)
            y = en_100;
        M64F(V(0x20808188) + i * 8) = (double)y;
        if (M16(G + 0xc18) != 0)
            x *= en_l;
        l20 = (double)(x * y * en_01);
        slot = (slot & 0xffffff00u) | (main_ftol(y) & 0xff);
        b = main_ftol((long double)l20 * l18);
        c = main_ftol(x);
        a = main_ftol(l20);
        if ((uint16_t)a > (uint16_t)c)
            a = c;
    }
    N_PUT(ADDR(&slot), 0x1000c, M32(V(0x20800b78) + at));
    N_PUT(ADDR(&a), 0x10000, M32(V(0x20800bdc) + at));
    N_PUT(ADDR(&b), 0x1001a, M32(V(0x20800c40) + at));
    N_PUT(ADDR(&c), 0x10000, M32(V(0x20800ca0) + i * 4));
    return 0;
}

static uint32_t FS98_STDCALL en_throttle0(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_throttle(0, b, c); }
static uint32_t FS98_STDCALL en_throttle1(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_throttle(1, b, c); }
static uint32_t FS98_STDCALL en_throttle2(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_throttle(2, b, c); }
static uint32_t FS98_STDCALL en_throttle3(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_throttle(3, b, c); }
static uint32_t FS98_STDCALL en_throttle4(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_throttle(4, b, c); }

/* .text+0x45b0: the fuel's limit, as text beside the field */
static void en_fuel_limit(uint32_t i, uint32_t p)
{
    uint32_t slot;
    long double x = (long double)M32(p + 4);

    if (x == 0) {
        H_STRCPY(V(0x208082a0) + i * 9, V(0x20801234));
        return;
    }
    if (M16(G + 0xc18) != 0)
        x *= en_l;
    slot = main_ftol(x);
    N_PUT(ADDR(&slot), 0x10000, V(0x208082a0) + i * 9);
}

/* .text+0x4660: the fuel's exchange: mode 2 checks it against the limit. */
static uint32_t FS98_STDCALL en_fuel(uint32_t i, uint32_t block, uint32_t mode)
{
    uint32_t p = en_block(i), status, limit, slot = i, r;
    long double x;

    if (p == 0)
        return 1;
    if (mode != 2)
        return 0;
    status = block + (i + 0x18) * 20;
    M32(status) = 0;
    if (M32(p + 4) == 0)
        return 0;
    x = (long double)M32(p + 4);
    if (M16(G + 0xc18) != 0)
        x *= en_l;
    limit = main_ftol(x);
    en_fuel_limit(i, p);
    r = N_GET(M32(block + 0x1e4 + i * 20), 0, ADDR(&slot));
    M32(status) = r;
    if (r != 0)
        return r;
    if ((uint16_t)slot > (uint16_t)limit) {
        M32(status) = 6;
        return 6;
    }
    return 0;
}

static uint32_t FS98_STDCALL en_fuel0(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_fuel(0, b, c); }
static uint32_t FS98_STDCALL en_fuel1(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_fuel(1, b, c); }
static uint32_t FS98_STDCALL en_fuel2(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_fuel(2, b, c); }
static uint32_t FS98_STDCALL en_fuel3(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_fuel(3, b, c); }
static uint32_t FS98_STDCALL en_fuel4(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_fuel(4, b, c); }

/* .text+0x47d0: the manifold's limit */
static uint32_t en_power_limit(uint32_t i, uint32_t p)
{
    uint32_t slot;
    long double x = (long double)M32(p + 4), t;

    if (x == 0) {
        H_STRCPY(V(0x20808310) + i * 9, V(0x20801234));
        return 0;
    }
    if (M16(G + 0xc18) != 0)
        x *= en_l;
    t = en_size();
    slot = main_ftol(t * x);
    N_PUT(ADDR(&slot), 0x1001a, V(0x20808310) + i * 9);
    return slot;
}

/* .text+0x48b0: the manifold's exchange: mode 2 checks it. */
static uint32_t FS98_STDCALL en_power(uint32_t i, uint32_t block, uint32_t mode)
{
    uint32_t p = en_block(i), status, limit, value = 0, r;
    double a, b;
    long double x;

    if (p == 0)
        return 1;
    if (mode != 2)
        return 0;
    status = block + (i + 0x1d) * 20;
    M32(status) = 0;
    if (M32(p + 4) == 0)
        return 0;
    a = (double)(long double)M32(p + 4);
    if (M16(G + 0xc18) != 0)
        a = (double)((long double)a * en_l);
    b = (double)((long double)(int32_t)(int16_t)M16(G + 0xaf4) * en_scale);
    if (M16(G + 0xc18) != 0)
        b = (double)((long double)b * en_gal);
    limit = en_power_limit(i, p);
    r = N_GET(M32(block + 0x248 + i * 20), 0x1a, ADDR(&value));
    M32(status) = r;
    if (r != 0)
        return r;
    x = (long double)value;
    x /= b;
    x *= en_100;
    x /= a;
    if (x > en_101)
        x = en_101;
    if ((uint8_t)main_ftol(x) > 0x64 || value > limit) {
        M32(status) = 6;
        return 6;
    }
    return 0;
}

static uint32_t FS98_STDCALL en_power0(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_power(0, b, c); }
static uint32_t FS98_STDCALL en_power1(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_power(1, b, c); }
static uint32_t FS98_STDCALL en_power2(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_power(2, b, c); }
static uint32_t FS98_STDCALL en_power3(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_power(3, b, c); }
static uint32_t FS98_STDCALL en_power4(uint32_t a, uint32_t b, uint32_t c) { (void)a; return en_power(4, b, c); }

/* .text+0x4a80: its argument's stack slot is the scratch word, and NUMERIC writes only
 * the low half: the rest of the block's address goes along to the text. */
static uint32_t FS98_STDCALL en_units_up(uint32_t block)
{
    uint32_t slot = block - D + MAIN_DATA_VA;

    N_GET(M32(block + 0x84), 0x24, ADDR(&slot));
    N_PUT(ADDR(&slot), 0x10025, M32(block + 0x70));
    return 0;
}

/* .text+0x4ac0 */
static uint32_t FS98_STDCALL en_units_down(uint32_t block)
{
    uint32_t slot = block - D + MAIN_DATA_VA;

    N_GET(M32(block + 0x70), 0x25, ADDR(&slot));
    N_PUT(ADDR(&slot), 0x10024, M32(block + 0x84));
    return 0;
}

/* .text+0x4b00: the percent `v` and the fields that follow from it; the field that
 * moved (`mode` 2 fuel, 3 manifold) is left alone. */
static uint32_t en_show(double v, uint32_t mode, uint32_t block, uint32_t i)
{
    uint32_t cap = 0, slot, fuel, power, r;
    long double w, t;
    long double lv = v;

    r = N_GET(M32(block + 0x2a8 + i * 4), 0, ADDR(&cap));
    if (r != 0)
        return 1;
    t = en_size();
    if (lv > en_200)
        lv = en_200;
    slot = cap & 0xffff;
    if ((uint16_t)cap == 0) {
        lv = 0;
        w = 0;
    } else {
        double l = (double)(long double)(int32_t)(cap & 0xffff);

        w = (long double)l * lv * en_01;
    }
    slot = (slot & 0xffffff00u) | (main_ftol(lv) & 0xff);
    fuel = main_ftol(w);
    power = main_ftol(w * t);
    N_PUT(ADDR(&slot), 0x1000c, M32(block + 0x180 + i * 20));
    if (mode != 2)
        N_PUT(ADDR(&fuel), 0x10000, M32(block + i * 20 + 0x1e4));
    if (mode != 3)
        N_PUT(ADDR(&power), 0x1001a, M32(block + i * 20 + 0x248));
    return 0;
}

/* .text+0x4c40: the throttle edited */
static uint32_t en_from_throttle(uint32_t block, uint32_t i)
{
    uint32_t slot = i;

    if (N_GET(M32(block + 0x180 + i * 20), 0xc, ADDR(&slot)) != 0)
        return 1;
    return en_show((double)(long double)(int32_t)(slot & 0xff), 1, block, i);
}

/* .text+0x4ca0: the fuel edited */
static uint32_t en_from_fuel(uint32_t block, uint32_t i)
{
    uint32_t cap = i, fuel = 0, power;
    double pct, a;
    long double t, x;

    if (N_GET(M32(block + 0x2a8 + i * 4), 0, ADDR(&cap)) != 0)
        return 1;
    if (N_GET(M32(block + 0x1e4 + i * 20), 0, ADDR(&fuel)) != 0)
        return 1;
    t = en_size();
    a = (double)(long double)(int32_t)(cap & 0xffff);
    if ((uint16_t)cap == 0) {
        x = 0;
        pct = 0;
    } else {
        x = (long double)(int32_t)(fuel & 0xffff);
        pct = (double)(x * en_100 / a);
    }
    power = main_ftol(x * t);
    if ((long double)pct > en_200)
        pct = 200.0;
    N_PUT(ADDR(&power), 0x1001a, M32(block + i * 20 + 0x248));
    return en_show(pct, 2, block, i);
}

/* .text+0x4df0: the manifold edited */
static uint32_t en_from_power(uint32_t block, uint32_t i)
{
    uint32_t cap = i, value = 0, fuel;
    double pct, a;
    long double t, x;

    if (N_GET(M32(block + 0x2a8 + i * 4), 0, ADDR(&cap)) != 0)
        return 1;
    if (N_GET(M32(block + 0x248 + i * 20), 0x1a, ADDR(&value)) != 0)
        return 1;
    t = en_size();
    a = (double)(long double)(int32_t)(cap & 0xffff);
    if ((uint16_t)cap == 0) {
        x = 0;
        pct = 0;
    } else {
        x = ((long double)value - en_half) / t;
        pct = (double)(x * en_100 / a);
    }
    if ((long double)pct > en_200)
        pct = 200.0;
    fuel = main_ftol(x);
    N_PUT(ADDR(&fuel), 0x10000, M32(block + i * 20 + 0x1e4));
    return en_show(pct, 3, block, i);
}

static uint32_t FS98_STDCALL en_edit_throttle(uint32_t block, uint32_t i) { return en_from_throttle(block, i); }
static uint32_t FS98_STDCALL en_edit_fuel(uint32_t block, uint32_t i) { return en_from_fuel(block, i); }
static uint32_t FS98_STDCALL en_edit_power(uint32_t block, uint32_t i) { return en_from_power(block, i); }

/* .text+0x4010: the check: the edit (+0x20 which field, +0x24 which engine) is
 * carried over first; the status it leaves is moved to the field that was edited. */
static uint32_t FS98_STDCALL en_check(uint32_t block)
{
    uint32_t mode = M32(block + 0x20), i = M32(block + 0x24), r;
#define EN_STATUS(n)  M32(block + (i + (n)) * 20)

    if (mode == 1)
        en_from_throttle(block, i);
    else if (mode == 2)
        en_from_fuel(block, i);
    else if (mode == 3)
        en_from_power(block, i);
    r = UTIL_PUT(V(0x20800d48), 0x3f, block, 1);
    mode = M32(block + 0x20);
    i = M32(block + 0x24);
    if (mode == 1) {
        if (EN_STATUS(0x18) != 0 || EN_STATUS(0x1d) != 0)
            EN_STATUS(0x13) = 1;
        EN_STATUS(0x18) = 0;
        EN_STATUS(0x1d) = 0;
    } else if (mode == 2) {
        if (EN_STATUS(0x13) != 0 || EN_STATUS(0x1d) != 0)
            EN_STATUS(0x18) = 1;
        EN_STATUS(0x13) = 0;
        EN_STATUS(0x1d) = 0;
    } else if (mode == 3) {
        if (EN_STATUS(0x18) != 0 || EN_STATUS(0x13) != 0)
            EN_STATUS(0x1d) = 1;
        EN_STATUS(0x18) = 0;
        EN_STATUS(0x13) = 0;
    }
#undef EN_STATUS
    return r;
}

/* Nine flags the dialog holds the other way up from GLOBAL (.text+0x3c80, +0x3ef0) */
static void en_flip(void)
{
    uint32_t a;

    for (a = V(0x20800cc8); a <= V(0x20800d08); a += 8)
        M32(a) = M32(a) == 0;
}

/* .text+0x3c80 */
static uint32_t en_load(void)
{
    uint32_t gp = G, slot;
    uint8_t cl;
    long double x;

    M32(V(0x20800ae4)) = 0;
    M16(V(0x20800b08)) = M16(gp + 0xaec);
    M32(V(0x20800b5c)) = 0;
    cl = M8(gp + 0x609);
    if (cl == 2) {
        M32(V(0x20800ae4)) = 1;
        M32(V(0x20800b5c)) = 1;
    } else if (cl == 3) {
        M32(V(0x20800ae4)) = 1;
    }
    M32(V(0x20800b3c)) = M32(gp + 0x780);
    M32(V(0x20800b50)) = M32(gp + 0x798) == 3;
    UTIL_GET(V(0x20800d48), 0x3f, EN_DLG);
    gp = G;
    M32(V(0x20800a50)) = M32(gp + 0x5d8);
    x = (long double)(int32_t)(int16_t)M16(gp + 0xaf4) * en_scale;
    if (M16(gp + 0xc18) != 0)
        x *= en_gal;
    x *= en_16;
    slot = main_ftol(x);
    N_PUT(ADDR(&slot), 0x1001c, M32(V(0x20800b70)));
    gp = G;
    M32(V(0x20800a04)) = M16(gp + 0xc18);
    M32(V(0x20800a08)) = M8(gp + 0x609);
    M32(V(0x20800a0c)) = M32(gp + 0x5d4);
    M16(V(0x20800a10)) = M16(gp + 0xb04);
    M16(V(0x20800a12)) = M16(gp + 0xb02);
    M16(V(0x20800a14)) = 0xffff;
    if (!(M8(gp + 0xb05) & 2))
        M16(V(0x20800a14)) = 0xfffb;
    if (!(M8(gp + 0xb02) & 0x10))
        M16(V(0x20800a14)) &= 0xffd7;
    en_flip();
    N_PUT(V(0x20800930), 0x10025, V(0x20808280));
    N_PUT(V(0x20800932), 0x10025, V(0x20808290));
    N_PUT(V(0x20800930), 0x10024, V(0x208082d0));
    N_PUT(V(0x20800932), 0x10024, V(0x20808350));
    return 0;
}

/* .text+0x3ef0 */
static void en_apply(void)
{
    if (M32(EN_DLG) != 1)
        return;
    en_flip();
    UTIL_PUT(V(0x20800d48), 0x3f, EN_DLG, 0);
    if (M32(V(0x20800a50)) != M32(G + 0x5d8))
        H_POST(0x10017, 0);
    CHAIN_SEND(0x14, 0x70, 0);
    flight_quicksave();
}

/* .text+0x3bf0: run with 0x1206 only; its switch offers 0x1206..0x1209 a tab each. */
static void command_1205(uint32_t id)
{
    static const uint8_t tab[4] = { 0, 2, 3, 5 };

    en_load();
    if (id - 0x1206 > 3)
        return;
    M32(V(0x20800a00)) = tab[id - 0x1206];
    if (H_DIALOG(id, EN_DLG) == 0 && M32(EN_DLG) == 1)
        en_apply();
}
