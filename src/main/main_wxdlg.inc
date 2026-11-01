/* MAIN.DLL: command 0x1306, the weather: a simple dialog (0x1302) or, with GLOBAL +0x1234
 * set, the advanced one (0x13040001). MSVC C, .text+0x19fb0..+0x1a4af and +0x1a630..
 * +0x1d2ef, around what start uses (slice 31). Winds at .data+0x7b64 (0x58 each), clouds
 * at +0x7ca8 (0x64 each), temperatures at +0x7e38 (0x2c each).
 *
 * The check reads a value it may not have: a text NUMERIC refuses with anything but
 * status 0xb leaves the word as the last call left it. */

#define WX_W(i)    (V(0x20807878) + (i) * 22)   /* a wind type's limits */
#define WX_R       V(0x20807f54)
#define WX_C       V(0x20807f58)
#define WX_T       V(0x20807f5c)
#define WX_ROW     V(0x20807f60)
#define WX_CLOUD   M32(V(0x20807f4c))
#define WX_MISC    M32(V(0x20807f50))
#define WX_COVER   V(0x208079a8)
#define WX_DEFAULT_BLOCK V(0x20807b40)
#define WX_SEEN    V(0x2080d128)
#define WX_E1      V(0x20807af0)
#define WX_E2      V(0x20807ae8)

/* A wind speed's /36 and a word's visibility */
static uint32_t wx_36(uint32_t v)
{
    int32_t q = (int32_t)(((int64_t)(int32_t)v * 0x38e38e39) >> 32) >> 3;

    return (uint32_t)(q + (int32_t)((uint32_t)q >> 31));
}

/* One wind layer out of the row; the `first` keeps its speed for a test by wind type */
static void wx_wind_load(uint32_t a, uint32_t b, uint8_t present, uint8_t floor, uint8_t ceil, uint8_t speed, uint8_t top, uint8_t cover,
                         uint8_t type, uint8_t flag, uint8_t knots, int first, int advance)
{
    uint32_t r = M32(WX_R), t, l10, l14;
    uint16_t ax;

    M32(r + 0x54) = M16(a + present) != 0;
    M32(r + 0x40) = M16(b + type);
    t = WX_W(M32(r + 0x40));
    l10 = M16(a + floor);
    if (M16(a + floor) < M16(t))
        l10 = M16(t);
    M16(M32(WX_SEEN)) = (uint16_t)l10;
    if (advance)
        M32(WX_SEEN) += 2;
    N_PUT(ADDR(&l10), 0x38, M32(r + 4));
    t = WX_W(M32(r + 0x40));
    ax = (uint16_t)(M16(t) + M16(t + 4));
    l14 = ax;
    if ((int32_t)(int16_t)M16(a + ceil) >= (int32_t)(uint32_t)ax)
        l14 = M16(a + ceil);
    N_PUT(ADDR(&l14), 0x38, M32(r + 0x18));
    N_PUT(a + speed, 0x38, M32(r + 0x2c));
    r = M32(WX_R);
    if (first && (int32_t)M32(r + 0x40) >= 0xa) {
        int32_t q = (int32_t)(((int64_t)(int32_t)(uint32_t)M16(a + top) * 0x2fffa001) >> 32) >> 0xc;

        M16(r + 0x3c) = (uint16_t)(q + (int32_t)((uint32_t)q >> 31));
    } else {
        ax = M16(a + top);
        M16(r + 0x3c) = ax != 0xffff ? (uint16_t)(ax >> 0xd) : 8;
    }
    (void)cover;
    M32(r + 0x50) = (M8(b + flag) & 1) != 0;
    M32(r + 0x48) = wx_36(M16(b + knots));
}

/* One cloud layer */
static void wx_cloud_load(uint32_t c, uint32_t r, uint8_t present, uint8_t top, uint8_t base, uint8_t cover, uint8_t turb, uint8_t speed, uint8_t dir)
{
    M32(r + 0x60) = M16(c + present) != 0;
    N_PUT(c + top, 0x38, M32(r + 4));
    N_PUT(c + base, 0x38, M32(r + 0x18));
    N_PUT(c + cover, 0x21, M32(r + 0x2c));
    N_PUT(c + turb, 0x4b, M32(r + 0x40));
    M32(r + 0x5c) = wx_36(M16(c + speed));
    M32(r + 0x54) = M16(c + dir);
}

/* A temperature */
static void wx_temp_load(uint32_t d, uint8_t at)
{
    uint32_t r = M32(WX_T);

    M32(r + 0x28) = M16(d + at) != 0xd8f1;
    N_PUT(d + at, 0x38, M32(r + 4));
    N_PUT(d + at + 2, 0x3c, M32(r + 0x18));
}

/* .text+0x1d100 */
static uint32_t FS98_STDCALL wx_units_up(uint32_t b)
{
    uint32_t slot = b - D + MAIN_DATA_VA;

    N_GET(M32(b + 0x3e4), 0x24, ADDR(&slot));
    N_PUT(ADDR(&slot), 0x25, M32(b + 0x3d0));
    return 0;
}

/* .text+0x1d140 */
static uint32_t FS98_STDCALL wx_units_down(uint32_t b)
{
    uint32_t slot = b - D + MAIN_DATA_VA;

    N_GET(M32(b + 0x3d0), 0x25, ADDR(&slot));
    N_PUT(ADDR(&slot), 0x24, M32(b + 0x3e4));
    return 0;
}

/* .text+0x1a630: the advanced dialog's fields from GLOBAL */
static uint32_t wx_load(void)
{
    uint32_t gp, row, a, b, c, d, x;

    weather_table();
    gp = G;
    M32(V(0x20807b4c)) = M32(gp + 0xf18);
    M32(V(0x20807b50)) = M16(gp + 0xc18);
    M32(WX_R) = V(0x20807b64);
    M32(WX_C) = V(0x20807ca8);
    M32(WX_T) = V(0x20807e38);
    M32(WX_SEEN) = V(0x2080d118);
    row = V(0x2080d0c0) + M32(gp + 0xf18) * 0x1c;
    M32(WX_ROW) = row;
    M32(V(0x20807b48)) = M32(row + 0x18);
    M32(V(0x20807b54)) = M16(row + 0x14);
    M32(V(0x20807b58)) = M16(row + 0x16);
    x = M32(V(0x20807f48));
    M32(x + 0x140) = 0;
    while (M16(V(0x20807978) + M32(x + 0x140) * 2) < M16(M32(row + 0xc)) && (int32_t)M32(x + 0x140) < 0x15)
        M32(x + 0x140) += 1;
    a = M32(row);
    b = M32(row + 4);
    wx_wind_load(a, b, 0x10, 0x12, 0x10, 0x16, 0x14, 0, 0xc, 0xe, 0x10, 1, 1);
    M32(WX_R) += 0x58;
    wx_wind_load(a, b, 8, 0xa, 8, 0xe, 0xc, 0, 6, 8, 0xa, 0, 1);
    M32(WX_R) += 0x58;
    wx_wind_load(a, b, 0, 2, 0, 6, 4, 0, 0, 2, 4, 0, 0);
    c = M32(row + 0x10);
    wx_cloud_load(c, M32(WX_C), 0x18, 0x1a, 0x18, 0x1c, 0x1e, 0x20, 0x22);
    M32(WX_C) += 0x64;
    wx_cloud_load(c, M32(WX_C), 0xc, 0xe, 0xc, 0x10, 0x12, 0x14, 0x16);
    M32(WX_C) += 0x64;
    wx_cloud_load(c, M32(WX_C), 0, 2, 0, 4, 6, 8, 0xa);
    x = WX_CLOUD;
    M32(x + 0x140) = M16(c + 0x24) != 0;
    N_PUT(c + 0x24, 0x38, M32(x + 0x148));
    N_PUT(c + 0x26, 0x21, M32(x + 0x15c));
    N_PUT(c + 0x28, 0x4b, M32(x + 0x170));
    M32(x + 0x18c) = wx_36(M16(c + 0x2a));
    M32(x + 0x184) = M16(c + 0x2c);
    d = M32(row + 8);
    wx_temp_load(d, 0xc);
    M32(WX_T) += 0x2c;
    wx_temp_load(d, 8);
    M32(WX_T) += 0x2c;
    wx_temp_load(d, 4);
    M32(WX_T) += 0x2c;
    wx_temp_load(d, 0);
    /* a layer closer than 100 to the next in use is lifted; the loop's own pointer goes
     * up by one layer as its index jumps to the one it compared with */
    {
        uint32_t ebp = V(0x20807e64), esi;
        uint32_t l1c = 0, l18 = 0;                 /* held from pass to pass: a text NUMERIC refuses leaves them */
        uint16_t ax = 0, di = 0;

        do {
            if (M32(ebp - 4) == 1) {
                int found = 0;

                di = (uint16_t)(ax + 1);
                esi = ebp;
                while (di < 4) {
                    if (M32(esi + 0x28) == 1) {
                        found = 1;
                        break;
                    }
                    di++;
                    esi += 0x2c;
                }
                if (found) {
                    N_GET(M32(ebp - 0x28), 0x38, ADDR(&l1c));
                    N_GET(M32(esi + 4), 0x38, ADDR(&l18));
                    if ((int32_t)(l18 & 0xffff) - (int32_t)(l1c & 0xffff) < 0x64) {
                        l1c += 0x65;
                        N_PUT(ADDR(&l1c), 0x38, M32(esi + 4));
                    }
                    ax = (uint16_t)(di - 1);
                }
                if (di == 4)
                    break;
            }
            ax++;
            ebp += 0x2c;
        } while (ax < 3);
    }
    row = M32(WX_ROW);
    d = M32(row + 8);
    if (M32(V(0x20807b50)) == 0) {
        uint32_t v = M16(d + 0x12);
        int32_t q = (int32_t)(((int64_t)(int32_t)v * (int32_t)0xe6c2b449) >> 32);

        q = (q + (int32_t)v) >> 7;
        M16(V(0x2080d138)) = (uint16_t)(q + (int32_t)((uint32_t)q >> 31));
    } else {
        M16(V(0x2080d138)) = M8(d + 0x13);
    }
    x = WX_MISC;
    N_PUT(V(0x2080d138), 0, M32(x + 0xc4));
    M32(x + 0xbc) = M16(d + 0x10);
    N_PUT(d + 0x14, 0x24, M32(x + 0xf4));
    wx_units_up(WX_DEFAULT_BLOCK);
    M32(WX_MISC + 0xd8) = M16(M32(M32(WX_ROW) + 8) + 0x16);
    return 0;
}

/* .text+0x1a010: the simple dialog's, from those */
static uint32_t wx_simple_load(void)
{
    static const uint8_t vis[21] = { 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0 };
    uint32_t r, l, i, x, a;
    uint32_t word;

    M32(V(0x2080d12c)) = 1;
    M32(V(0x2080d13c)) = 0;
    l = wx_load();
    M32(V(0x208079b8)) = M32(V(0x20807b48));
    M32(V(0x208079bc)) = M32(V(0x20807b54));
    M32(V(0x208079c0)) = M32(V(0x20807b58));
    M32(V(0x208079c8)) = M32(V(0x20807c9c)) > 0x14 ? 0 : vis[M32(V(0x20807c9c))];
    r = V(0x20807b64);
    M32(WX_R) = r;
    a = 0;
    M32(V(0x208079c4)) = 0;
    for (i = 0; i < 3; i++) {
        if (M32(r + 0x54) == 1) {
            if (M32(r + 0x40) == 0xa) {
                a = 4;
                M32(V(0x208079c4)) = a;
            } else if (M16(r + 0x3c) == 0) {
                a = 1;
                M32(V(0x208079c4)) = a;
            } else if (M16(r + 0x3c) < 5 && (int32_t)a < 2) {
                a = 2;
                M32(V(0x208079c4)) = a;
            } else if ((int32_t)a < 4) {
                a = 3;
                M32(V(0x208079c4)) = a;
            }
        }
        r += 0x58;
        M32(WX_R) = r;
    }
    M32(V(0x2080d134)) = a;
    M32(V(0x208079cc)) = 0;
    r = V(0x20807ca8);
    M32(WX_C) = r;
    word = 0;
    for (i = 0; i < 3; i++) {
        N_GET(M32(M32(WX_C) + 0x2c), 0x21, ADDR(&word));
        r = M32(WX_C);
        if (M32(r + 0x60) == 1) {
            a = M32(V(0x208079cc));
            if (M16(WX_COVER + a * 2) < (uint16_t)word) {
                do {
                    if ((int32_t)a >= 3)
                        break;
                    a++;
                } while (M16(WX_COVER + a * 2) < (uint16_t)word);
                M32(V(0x208079cc)) = a;
            }
        }
        M32(WX_C) = r + 0x64;
    }
    x = WX_CLOUD;
    if (M32(x + 0x140) != 0) {
        N_GET(M32(x + 0x15c), 0x21, ADDR(&word));
        a = M32(V(0x208079cc));
        if (M16(WX_COVER + a * 2) < (uint16_t)word) {
            do {
                if ((int32_t)a >= 3)
                    break;
                a++;
            } while (M16(WX_COVER + a * 2) < (uint16_t)word);
            M32(V(0x208079cc)) = a;
        }
    }
    M32(V(0x2080d130)) = M32(V(0x208079cc));
    return l;
}

/* .text+0x1d180: a wind layer's defaults for its type */
static uint32_t FS98_STDCALL wx_wind_defaults(uint32_t b)
{
    uint32_t r = V(0x20807b64) + (uint32_t)M16(b + 0x20) * 0x58, t, slot = b - D + MAIN_DATA_VA, top;
    uint16_t cx;

    M32(WX_R) = r;
    N_GET(M32(r + 4), 0x38, ADDR(&slot));
    r = M32(WX_R);
    t = WX_W(M32(r + 0x40));
    cx = (uint16_t)slot;
    if (cx > M16(t + 2)) {
        cx = M16(t + 2);
        slot = (slot & 0xffff0000u) | cx;
    } else if (cx < M16(t)) {
        cx = M16(t);
        slot = (slot & 0xffff0000u) | cx;
    }
    top = (uint16_t)(M16(t + 4) + cx);
    N_PUT(ADDR(&slot), 0x38, M32(r + 4));
    N_PUT(ADDR(&top), 0x38, M32(M32(WX_R) + 0x18));
    r = M32(WX_R);
    H_MEMMOVE(M32(r + 0x2c), V(0x20807f7c), 5);
    r = M32(WX_R);
    t = WX_W(M32(r + 0x40));
    M32(r + 0x50) = M16(t + 0x12);
    if (M32(r + 0x40) == 0xa) {
        M16(r + 0x3c) = M16(V(0x2080795e));
        M32(r + 0x48) = M16(t + 0x10);
    } else {
        M16(r + 0x3c) = M16(t + 8);
        M32(r + 0x48) = M16(t + 0xe);
    }
    M16(V(0x20807970) + M32(V(0x20807b60)) * 2) |= M16(t + 0x14);
    return 0;
}

/* .text+0x1a220: the simple dialog's answers */
static uint32_t wx_simple_apply(void)
{
    static const uint8_t vis[5] = { 0x14, 0x10, 0xc, 6, 2 };
    uint32_t l8 = 0x17d0, lc = 0x168, a, esi = V(0x20807b64), x;

    if (M32(V(0x208079c8)) <= 4)
        M32(M32(V(0x20807f48)) + 0x140) = vis[M32(V(0x208079c8))];
    a = M32(V(0x208079c4));
    if (a != M32(V(0x2080d134))) {
        if (a == 4) {
            M32(V(0x20807c68)) = 0;
            M32(V(0x20807b60)) = 0;
            M32(V(0x20807ba4)) = 0xa;
        } else {
            M32(V(0x20807bb8)) = 0;
            esi = V(0x20807bbc);
            M32(V(0x20807b60)) = 1;
            M32(V(0x20807bfc)) = 0;
        }
        if (a == 0) {
            M32(esi + 0x54) = 0;
        } else {
            M32(esi + 0x54) = 1;
            wx_wind_defaults(WX_DEFAULT_BLOCK);
            a = M32(V(0x208079c4));
            M16(esi + 0x3c) = M16(V(0x20807f68) + a * 2);
            N_PUT(V(0x20807b20) + a * 6, 0x38, M32(esi + 4));
            N_PUT(V(0x20807b22) + M32(V(0x208079c4)) * 6, 0x38, M32(esi + 0x18));
            N_PUT(V(0x20807b24) + M32(V(0x208079c4)) * 6, 0x38, M32(esi + 0x2c));
        }
        M32(esi + 0xac) = 0;
    }
    if (M32(V(0x208079cc)) != M32(V(0x2080d130))) {
        x = WX_CLOUD;
        M32(x + 0x140) = 1;
        N_PUT(ADDR(&l8), 0x38, M32(x + 0x148));
        N_PUT(WX_COVER + M32(V(0x208079cc)) * 2, 0x21, M32(WX_CLOUD + 0x15c));
        N_PUT(ADDR(&lc), 0x4b, M32(WX_CLOUD + 0x170));
        M32(WX_CLOUD + 0x18c) = 0;
        M32(WX_CLOUD + 0x184) = 0;
        M32(V(0x20807d08)) = 0;
        M32(V(0x20807d6c)) = 0;
        M32(WX_C) = V(0x20807d70);
        M32(V(0x20807dd0)) = 0;
    }
    if (M32(V(0x2080d13c)) != 0)
        weather_row();
    H_FREE(V(0x2080d0d8));
    return 0;
}

/* .text+0x19fb0, command 0x1306 (+0x1a470 the advanced one) */
static uint32_t command_1306(void)
{
    if (M32(G + 0x1234) != 0) {
        M32(V(0x2080d12c)) = 0;
        wx_load();
        if (H_DIALOG(0x13040001, WX_DEFAULT_BLOCK) == 0 && M32(WX_DEFAULT_BLOCK) == 1)
            weather_row();
        return M32(WX_DEFAULT_BLOCK);
    }
    wx_simple_load();
    if (H_DIALOG(0x1302, V(0x208079b0)) == 0 && M32(V(0x208079b0)) == 1) {
        M32(V(0x2080d13c)) = 1;
        wx_simple_apply();
    }
    return M32(V(0x208079b0));
}

/* .text+0x1ce70: temperatures too near each other */
static uint32_t wx_temps_apart(uint32_t b)
{
    uint32_t esi = b + 0x2f8, n = 0, i, slot = 0, other = 0;

    for (i = 0; i < 4; i++)
        if (M32(esi + 0x28 + i * 0x2c) == 1)
            n++;
    if ((uint16_t)n <= 1)
        return 0;
    N_GET(M32(b + 0x2fc + M32(WX_MISC + 4) * 0x2c), 0x38, ADDR(&slot));
    for (i = 0; i < 4; i++, esi += 0x2c) {
        if (M32(esi + 0x28) == 1 && i != M32(WX_MISC + 4)) {
            int32_t diff;

            N_GET(M32(esi + 4), 0x38, ADDR(&other));
            diff = (int32_t)(slot & 0xffff) - (int32_t)(other & 0xffff);
            if (diff < 0)
                diff = -diff;
            if (diff < 0x64) {
                M32(esi) = 0x100e;
                return 0x100e;
            }
        }
    }
    return 0;
}

/* The limits behind an error, shown in the dialog */
static void wx_show_limits(uint32_t rec, uint32_t lo_at, uint32_t hi_at, uint32_t low, uint32_t high, uint32_t type)
{
    M32(rec + lo_at) = WX_E1;
    M32(rec + hi_at) = WX_E2;
    N_PUT(low, type, M32(rec + lo_at));
    N_PUT(high, type, M32(rec + hi_at));
}

/* .text+0x1bd50: the advanced dialog's check, layer by layer */
static uint32_t FS98_STDCALL wx_check(uint32_t b)
{
    volatile uint16_t l14[2] = { 1, 0x7710 }, l18[2] = { 1, 0x7710 }, l1c[2] = { 1, 0x76ab }, l20[2] = { 0, 0x7710 };
    volatile uint16_t l24[2] = { 0, 0x64 }, l28[2] = { 0xa100, 0x4b00 }, l2c[2] = { 0x34f0, 0x4a10 }, l30[2] = { 0, 0xc7 }, l38[2] = { 0xb6, 0 };
    uint32_t l10 = 0, v34 = 0, i, r, t, x, st;

    M32(WX_R) = b + 0x24;
    M32(WX_C) = b + 0x168;
    M32(WX_T) = b + 0x2f8;
    M32(WX_SEEN) = V(0x2080d118);
    for (i = 0; i < 3; i++) {
        r = M32(WX_R);
        if (M32(V(0x20807b54)) == 0 && M32(r + 0x54) == 1) {
            t = WX_W(M32(r + 0x40));
            M16(t) -= 1;
            M32(r) = N_CHECK(M32(r + 4), 0x38, ADDR(&v34), t);
            r = M32(WX_R);
            t = WX_W(M32(r + 0x40));
            M16(t) += 1;
            if (M32(r) != 0) {
                M32(r + 0xc) = WX_E1;
                M32(r + 0x10) = WX_E2;
                N_PUT(t, 0x10038, M32(r + 0xc));
                N_PUT(t + 2, 0x10038, M32(M32(WX_R) + 0x10));
                return M32(M32(WX_R));
            }
            l14[0] = (uint16_t)(M16(t + 4) + (uint16_t)v34);
            l14[1] = (uint16_t)(M16(t + 6) + (uint16_t)v34);
            if ((uint16_t)v34 != M16(M32(WX_SEEN)))
                st = N_GET(M32(r + 0x18), 0x38, ADDR(&l10));
            else
                st = N_CHECK(M32(r + 0x18), 0x38, ADDR(&l10), ADDR(l14));
            M32(M32(WX_R) + 0x14) = st;
            r = M32(WX_R);
            l14[0] = (uint16_t)(l14[0] + 1);
            if (M32(r + 0x14) != 0) {
                wx_show_limits(r, 0x20, 0x24, ADDR(&l14[0]), ADDR(&l14[1]), 0x10038);
                return M32(M32(WX_R) + 0x14);
            }
            if ((uint16_t)l10 < l14[0])
                N_PUT(ADDR(&l14[0]), 0x38, M32(r + 0x18));
            else if ((uint16_t)l10 > l14[1])
                N_PUT(ADDR(&l14[1]), 0x38, M32(r + 0x18));
            M16(M32(WX_SEEN)) = (uint16_t)v34;
            r = M32(WX_R);
            t = WX_W(M32(r + 0x40));
            l24[1] = M16(t + 0xc);
            if (l24[1] > 0) {
                M32(r + 0x28) = N_CHECK(M32(r + 0x2c), 0x38, ADDR(&l10), ADDR(l24));
                r = M32(WX_R);
            }
            if (M32(r + 0x28) != 0) {
                wx_show_limits(r, 0x34, 0x38, ADDR(&l24[0]), ADDR(&l24[1]), 0x10038);
                return M32(M32(WX_R) + 0x28);
            }
            t = WX_W(M32(r + 0x40));
            if ((int32_t)M32(r + 0x48) < (int32_t)(uint32_t)M16(t + 0xe))
                M32(r + 0x48) = M16(t + 0xe);
            if ((int32_t)M32(r + 0x48) > (int32_t)(uint32_t)M16(t + 0x10))
                M32(r + 0x48) = M16(t + 0x10);
        }
        if (M32(V(0x20807b58)) == 0 && M32(M32(WX_C) + 0x60) == 1) {
            x = M32(WX_C);
            M32(x) = N_GET(M32(x + 4), 0x38, ADDR(&l10));
            x = M32(WX_C);
            M32(x + 0xc) = WX_E1;
            M32(x + 0x10) = WX_E2;
            if (M32(x) == 0xb || (uint16_t)l10 > 0x76ab) {
                N_PUT(ADDR(&l1c[0]), 0x10038, M32(x + 0xc));
                N_PUT(ADDR(&l1c[1]), 0x10038, M32(M32(WX_C) + 0x10));
                M32(M32(WX_C)) = 0xb;
                return 0xb;
            }
            if ((uint16_t)l10 < 1) {
                l1c[0] = (uint16_t)(l1c[0] + 1);
                N_PUT(ADDR(&l1c[0]), 0x10038, M32(x + 0xc));
                N_PUT(ADDR(&l1c[1]), 0x10038, M32(M32(WX_C) + 0x10));
                M32(M32(WX_C)) = 0xc;
                return 0xc;
            }
            l18[0] = (uint16_t)(l10 + 0x65);
            M32(x + 0x14) = N_GET(M32(x + 0x18), 0x38, ADDR(&l10));
            x = M32(WX_C);
            if (M32(x + 0x14) == 0xb || (uint16_t)l10 > 0x7710) {
                wx_show_limits(x, 0x20, 0x24, ADDR(&l18[0]), ADDR(&l18[1]), 0x10038);
                M32(M32(WX_C) + 0x14) = 0xb;
                return 0xb;
            }
            if ((uint16_t)l10 < l18[0]) {
                N_PUT(ADDR(&l18[0]), 0x38, M32(x + 0x18));
                M32(M32(WX_C) + 0x14) = 0;
                x = M32(WX_C);
            }
            M32(x + 0x28) = N_GET(M32(x + 0x2c), 0x21, ADDR(&l10));
            x = M32(WX_C);
            if (M32(x + 0x28) == 0xb || (uint16_t)l10 > 0xc7) {
                wx_show_limits(x, 0x34, 0x38, ADDR(&l30[0]), ADDR(&l30[1]), 0x21);
                M32(M32(WX_C) + 0x28) = 0xb;
                return 0xb;
            }
            M32(x + 0x3c) = N_GET(M32(x + 0x40), 0x4b, ADDR(&l10));
            x = M32(WX_C);
            if (M32(x + 0x3c) != 0) {
                M32(x + 0x48) = V(0x20807af8);
                M32(x + 0x4c) = V(0x20807b00);
                return M32(x + 0x3c);
            }
        }
        if (M32(V(0x20807b54)) == 0 && M32(V(0x20807b58)) == 0 && M32(M32(WX_T) + 0x28) == 1) {
            x = M32(WX_T);
            M32(x) = N_GET(M32(x + 4), 0x38, ADDR(&l10));
            x = M32(WX_T);
            if (M32(x) != 0) {
                wx_show_limits(x, 0xc, 0x10, ADDR(&l20[0]), ADDR(&l20[1]), 0x10038);
                return M32(M32(WX_T));
            }
            l20[0] = (uint16_t)(l10 + 0x65);
            M32(x + 0x14) = N_GET(M32(x + 0x18), 0x3c, ADDR(&l10));
            x = M32(WX_T);
            if (M32(x + 0x14) != 0) {
                wx_show_limits(x, 0x20, 0x24, ADDR(&l28[0]), ADDR(&l28[1]), 0x3c);
                return M32(M32(WX_T) + 0x14);
            }
        }
        M32(WX_C) += 0x64;
        M32(WX_R) += 0x58;
        M32(WX_SEEN) += 2;
        M32(WX_T) += 0x2c;
    }
    x = M32(WX_T);
    if (M32(V(0x20807b54)) == 0 && M32(V(0x20807b58)) == 0 && M32(x + 0x28) != 0) {
        M32(x) = N_GET(M32(x + 4), 0x38, ADDR(&l10));
        x = M32(WX_T);
        if (M32(x) != 0) {
            wx_show_limits(x, 0xc, 0x10, ADDR(&l20[0]), ADDR(&l20[1]), 0x10038);
            return M32(M32(WX_T));
        }
        M32(x + 0x14) = N_GET(M32(x + 0x18), 0x3c, ADDR(&l10));
        x = M32(WX_T);
        if (M32(x + 0x14) != 0) {
            wx_show_limits(x, 0x20, 0x24, ADDR(&l28[0]), ADDR(&l28[1]), 0x3c);
            return M32(M32(WX_T) + 0x14);
        }
    }
    st = wx_temps_apart(b);
    if (st != 0)
        return st;
    x = WX_MISC;
    N_GET(M32(x + 0xc4), 0, ADDR(&l10));
    M32(x + 0xc0) = 0;
    if ((uint16_t)l10 > (M32(V(0x20807b50)) == 0 ? 0x24 : 0x14)) {
        M32(x + 0xc0) = 0xb;
        M32(x + 0xcc) = V(0x20807b08);
        M32(x + 0xd0) = M32(V(0x20807b50)) == 0 ? V(0x20807b18) : V(0x20807b10);
        return 0xb;
    }
    M32(x + 0xdc) = N_GET(M32(x + 0xe0), 0x25, ADDR(&l10));
    x = WX_MISC;
    if (M32(x + 0xdc) != 0) {
        wx_show_limits(x, 0xe8, 0xec, ADDR(&l2c[0]), ADDR(&l2c[1]), 0x25);
        return M32(WX_MISC + 0xdc);
    }
    M32(x + 0xf0) = N_GET(M32(x + 0xf4), 0x24, ADDR(&l10));
    x = WX_MISC;
    if (M32(x + 0xf0) != 0) {
        M32(x + 0xfc) = WX_E1;
        M32(x + 0x100) = WX_E2;
        N_PUT(ADDR(&l2c[0]), 0x24, M32(x + 0xfc));
        N_PUT(ADDR(&l2c[1]), 0x24, M32(WX_MISC + 0x100));
        return M32(WX_MISC + 0xf0);
    }
    if (M8(G + 0xf13) != 0)
        return 0;
    x = WX_CLOUD;
    if (M32(x + 0x140) == 0)
        return 0;
    M32(x + 0x144) = N_GET(M32(x + 0x148), 0x38, ADDR(&l10));
    x = WX_CLOUD;
    if (M32(x + 0x144) != 0) {
        M32(x + 0x150) = WX_E1;
        M32(x + 0x154) = WX_E2;
        N_PUT(ADDR(&l18[0]), 0x10038, M32(x + 0x150));
        N_PUT(ADDR(&l18[1]), 0x10038, M32(WX_CLOUD + 0x154));
        return M32(WX_CLOUD + 0x144);
    }
    if ((uint16_t)l10 < 0xa) {
        v34 = 0xa;
        N_PUT(ADDR(&v34), 0x38, M32(x + 0x148));
        x = WX_CLOUD;
    }
    M32(x + 0x158) = N_GET(M32(x + 0x15c), 0x21, ADDR(&l10));
    x = WX_CLOUD;
    if (M32(x + 0x158) == 0xb || (uint16_t)l10 > 0xc7) {
        M32(x + 0x164) = WX_E1;
        M32(x + 0x168) = WX_E2;
        N_PUT(ADDR(&l30[0]), 0x21, M32(x + 0x164));
        N_PUT(ADDR(&l30[1]), 0x21, M32(WX_CLOUD + 0x168));
        M32(WX_CLOUD + 0x158) = 0xb;
        return 0xb;
    }
    M32(x + 0x16c) = N_GET(M32(x + 0x170), 0x4b, ADDR(&l10));
    x = WX_CLOUD;
    if (M32(x + 0x16c) == 0)
        return 0;
    M32(x + 0x178) = WX_E1;
    M32(x + 0x17c) = WX_E2;
    N_PUT(ADDR(&l38[0]), 0x4b, M32(x + 0x178));
    N_PUT(ADDR(&l38[1]), 0x4b, M32(WX_CLOUD + 0x17c));
    return M32(WX_CLOUD + 0x16c);
}

/* .text+0x1cab0: Add, on the page +0x404 names */
static uint32_t FS98_STDCALL wx_add(uint32_t b)
{
    char text[8];
    uint32_t low = 0, base = 0x2dc, high = 0xfffffc00u, r, slot, top;
    uint16_t di;
    uint32_t page = M32(b + 0x404), i, n = 0, tenth = 0, e, x, t;

    memcpy(text, (const void *)(uintptr_t)V(0x20807f74), 6);
    text[6] = 0;
    if (page == 2) {
        r = b + 0x2f8 + (uint32_t)M16(b + 0x2f4) * 0x2c;
        M32(WX_T) = r;
        N_GET(ADDR(text), 0x38, ADDR(&low));
        N_PUT(ADDR(&base), 0x38, M32(M32(WX_T) + 4));
        M32(M32(WX_T) + 0x28) = 1;
        slot = base;
        while (wx_temps_apart(b) != 0 && (uint16_t)slot < (uint16_t)low) {
            slot += 0x6e;
            N_PUT(ADDR(&slot), 0x38, M32(M32(WX_T) + 4));
        }
        if ((uint16_t)slot > (uint16_t)low)
            N_PUT(ADDR(&low), 0x38, M32(M32(WX_T) + 4));
        N_PUT(ADDR(&high), 0x3c, M32(M32(WX_T) + 0x18));
        return 0;
    }
    if (page == 1) {
        di = M16(b + 0x164);
        r = b + 0x168 + (uint32_t)di * 100;
        M32(WX_C) = r;
        if (di == 3) {
            x = WX_CLOUD;
            slot = 0x2fa;
            N_PUT(ADDR(&slot), 0x38, M32(x + 0x148));
            M32(b + 0x2a0) = 1;
            return 0;
        }
        slot = 0x1c9;
        low = 0x2fa;
        N_PUT(ADDR(&slot), 0x38, M32(r + 4));
        N_PUT(ADDR(&low), 0x38, M32(M32(WX_C) + 0x18));
        M32(M32(WX_C) + 0x60) = 1;
        return 0;
    }
    if (page != 0)
        return 0;
    di = M16(b + 0x20);
    e = b + 0x24 + (uint32_t)di * 0x58;
    if (di != 0)
        M32(e + 0x54) = 1;
    x = b + 0x24;
    M32(WX_R) = x;
    for (i = 0; i < 3; i++) {
        if (M32(x + 0x54) == 1) {
            n++;
            if (M32(x + 0x40) == 0xa)
                tenth = 1;
        }
        x += 0x58;
        M32(WX_R) = x;
    }
    M32(WX_R) = e;
    if ((uint16_t)n > 1 && tenth == 0 && di == 0) {
        M32(e + 0x54) = 1;
        return 0x1008;
    }
    slot = b - D + MAIN_DATA_VA;
    N_GET(M32(e + 4), 0x38, ADDR(&slot));
    r = M32(WX_R);
    t = WX_W(M32(r + 0x40));
    top = (uint16_t)(M16(t + 4) + (uint16_t)slot);
    N_PUT(ADDR(&top), 0x38, M32(r + 0x18));
    return 0;
}

/* .text+0x1cd80: Delete */
static uint32_t FS98_STDCALL wx_delete(uint32_t b)
{
    uint32_t page;

    if (ALERT(0xc0, 1, 0xf510, 0, 0, 0, 0, 0) != 0x40)
        return 0;
    page = M32(b + 0x404);
    if (page == 2)
        M32(b + 0x320 + M32(b + 0x2f4) * 0x2c) = 0;
    else if (page == 1) {
        if (M32(b + 0x164) == 3)
            M32(b + 0x2a0) = 0;
        else
            M32(b + 0x1c8 + M32(b + 0x164) * 100) = 0;
    } else if (page == 0)
        M32(b + 0x78 + M32(b + 0x20) * 0x58) = 0;
    return 0;
}
