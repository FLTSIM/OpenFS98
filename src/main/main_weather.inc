/* MAIN.DLL slice 31: the weather's layers into the record the sim reads, MSVC C.
 *
 * .text+0x1a5f0 (run from +0x28): +0x1a4b0 builds the table at .data+0xd0c0 of
 * pointers into GLOBAL's weather, one row per mode, and has the host's +0x2a4
 * load string 0x2100; +0x1b260 converts the tables at .data+0x7b64 (winds, 0x58
 * bytes each), +0x7ca8 (clouds, 0x64) and +0x7e38 (temperatures, 0x2c) through
 * NUMERIC +0x1c into the row GLOBAL +0xf18 picks.  +0x1cf80 first sorts the
 * clouds with +0x1cf50 and lifts layers that overlap; +0x1d300 sorts the
 * temperatures.  The compares go to the host's +0x22c as ours; retail's other
 * code addresses in the tables are not called from here.
 *
 * Retail details, reproduced:
 *   - +0x1cf80 and +0x1ce10 read NUMERIC's answers as words of stack slots, so
 *     only the low halves count.
 *   - The third wind's `[r+0x40] < 10` test reads a dword, its write a word. */

#define NUMERIC_  M32(V(0x20800084))
#define N_GET(v, type, out)  CALL(NUMERIC_, 0x1c, f3)((v), (type), (out))
#define N_PUT(in, type, to)  CALL(NUMERIC_, 0x18, f3)((in), (type), (to))

/* .text+0x1ce10 */
static uint32_t weather_compare_values(uint32_t x, uint32_t y)
{
    uint32_t a, b;

    if (x == 0 || y == 0)
        return 0;
    /* a text NUMERIC refuses leaves the other's address here, as retail's process holds it */
    a = y - D < MAIN_IMAGE_SIZE ? y - D + MAIN_DATA_VA : y;
    N_GET(x, 0, ADDR(&a));
    b = x - D < MAIN_IMAGE_SIZE ? x - D + MAIN_DATA_VA : x;
    N_GET(y, 0, ADDR(&b));
    return (a & 0xffff) - (b & 0xffff);
}

/* .text+0x1cf50: the host's sort compares clouds. */
static uint32_t FS98_STDCALL weather_compare_clouds(uint32_t a, uint32_t b)
{
    if (M32(a + 0x60) == 1 && M32(b + 0x60) == 1)
        return weather_compare_values(M32(a + 4), M32(b + 4));
    return 0;
}

/* .text+0x1d300: and temperatures. */
static uint32_t FS98_STDCALL weather_compare_temps(uint32_t a, uint32_t b)
{
    uint32_t c = M32(a + 0x28);

    if (c == 1) {
        if (M32(b + 0x28) == 1)
            return weather_compare_values(M32(a + 4), M32(b + 4));
        if (M32(b + 0x28) == 0)
            return 0xffffffffu;
    }
    return 1;
}

/* .text+0x1cf80: sort the cloud layers and take out or lift the overlaps. */
static void weather_sort_clouds(uint32_t p)
{
    uint32_t edi = p + 8, esi, ebp = 0, n = 1, again = 0;
    uint32_t a12 = 0, a14 = 0, a16 = 0, a18 = 0, a1c = 0, i;    /* held from pass to pass, as a text NUMERIC refuses leaves them */

    HOST_(0x22c, f4)(edi, 3, 0x64, ADDR(weather_compare_clouds));
    do {
        if (M32(edi + 0x60) == 1) {
            N_GET(M32(edi + 4), 0x38, ADDR(&a12));
            N_GET(M32(edi + 0x18), 0x38, ADDR(&a18));
            esi = edi;
            if (ebp == 0 && M32(p + 0x140) == 1) {
                N_GET(M32(p + 0x148), 0x38, ADDR(&a1c));
                if ((uint16_t)a18 <= (uint16_t)a1c) {
                    M32(edi + 0x60) = ebp;
                    goto next;
                }
                if ((uint16_t)a12 < (uint16_t)a1c) {
                    a1c += 1;
                    N_PUT(ADDR(&a1c), 0x38, M32(edi + 4));
                }
                ebp = 1;
            }
            if ((uint16_t)n < 3) {
                for (i = n; (uint16_t)i < 3; i++) {
                    uint32_t d = M32(esi + 0xc4);

                    esi += 0x64;
                    if (d == 1) {
                        N_GET(M32(esi + 4), 0x38, ADDR(&a14));
                        if ((uint16_t)a14 < (uint16_t)a18) {
                            N_GET(M32(esi + 0x18), 0x38, ADDR(&a16));
                            if ((uint16_t)a16 <= (uint16_t)a18) {
                                M32(esi + 0x60) = 0;
                            } else {
                                a18 += 1;
                                N_PUT(ADDR(&a18), 0x38, M32(esi + 4));
                            }
                        }
                        again = 1;
                        break;
                    }
                }
            }
            if (again == 0)
                return;
        }
next:
        n++;
        edi += 0x64;
    } while ((uint16_t)(n - 1) < 2);
}

/* A wind's flags in .data at `or_at` and its speed, in the row at b. */
static void weather_wind(uint32_t r, uint32_t b, uint32_t b_at, uint32_t or_at)
{
    uint16_t w = (uint16_t)(M16(or_at) | (M8(r + 0x50) & 1));

    M16(or_at) = w;
    M16(b + b_at) = w;
    M16(b + b_at + 2) = (uint16_t)(M16(r + 0x48) * 0x24);
}

/* .text+0x1b260 */
static void weather_row(void)
{
    uint32_t gp, row, a, b, c, d, r, i, x;
    uint16_t cx, dx;

    weather_sort_clouds(M32(V(0x20807f4c)));
    gp = G;
    M32(V(0x20807f54)) = V(0x20807b64);
    M32(V(0x20807f58)) = V(0x20807ca8);
    M32(V(0x20807f5c)) = V(0x20807e38);
    row = V(0x2080d0c0) + M32(gp + 0xf18) * 0x1c;
    M32(V(0x20807f60)) = row;
    M16(M32(row + 0xc)) = M16(V(0x20807978) + M32(M32(V(0x20807f48)) + 0x140) * 2);
    a = M32(row);
    b = M32(row + 4);
    c = M32(row + 0x10);
    d = M32(row + 8);

    if (M32(V(0x20807b54)) == 0) {
        r = M32(V(0x20807f54));
        if (M32(r + 0x54) == 1) {
            N_GET(M32(r + 4), 0x38, a + 0x12);
            N_GET(M32(r + 0x18), 0x38, a + 0x10);
            N_GET(M32(r + 0x2c), 0x38, a + 0x16);
            M16(r + 0x3c) = (uint16_t)(M16(r + 0x3c) + 1);
            M16(a + 0x14) = (uint16_t)(M16(r + 0x3c) * 0x5555);
            M16(b + 0xc) = 0xa;
            weather_wind(r, b, 0xe, V(0x20807970));
        } else {
            M16(b + 0xe) = 0;
            M16(b + 0x10) = 0;
            M16(a + 0x10) = 0;
        }
        r += 0x58;
        M32(V(0x20807f54)) = r;
        if (M32(r + 0x54) == 1) {
            N_GET(M32(r + 4), 0x38, a + 0xa);
            N_GET(M32(r + 0x18), 0x38, a + 8);
            N_GET(M32(r + 0x2c), 0x38, a + 0xe);
            if ((int32_t)M32(r + 0x40) < 0xa)
                M16(a + 0xc) = M16(r + 0x3c) < 8 ? (uint16_t)(M16(r + 0x3c) << 0xd) : 0xffff;
            M16(b + 6) = M16(r + 0x40);
            weather_wind(r, b, 8, V(0x20807972));
        } else {
            M16(b + 8) = 0;
            M16(b + 0xa) = 0;
            M16(a + 8) = 0;
        }
        r += 0x58;
        M32(V(0x20807f54)) = r;
        if (M32(r + 0x54) == 1) {
            N_GET(M32(r + 4), 0x38, a + 2);
            N_GET(M32(r + 0x18), 0x38, a);
            N_GET(M32(r + 0x2c), 0x38, a + 6);
            if ((int32_t)M32(r + 0x40) < 0xa)
                M16(a + 4) = M16(r + 0x3c) < 8 ? (uint16_t)(M16(r + 0x3c) << 0xd) : 0xffff;
            M16(b) = M16(r + 0x40);
            weather_wind(r, b, 2, V(0x20807974));
            r -= 0x58;
            M32(V(0x20807f54)) = r;
            if (M32(r + 0x54) == 1) {
                cx = M16(a + 2);
                dx = M16(a + 0xa);
                if ((int16_t)cx < (int16_t)dx) {
                    static const uint8_t swap_a[][2] = { { 2, 0xa }, { 8, 0 }, { 0xe, 6 }, { 0xc, 4 } };
                    static const uint8_t swap_b[][2] = { { 6, 0 }, { 8, 2 }, { 0xa, 4 } };

                    for (i = 0; i < 4; i++) {
                        x = M16(a + swap_a[i][0]);
                        M16(a + swap_a[i][0]) = M16(a + swap_a[i][1]);
                        M16(a + swap_a[i][1]) = (uint16_t)x;
                    }
                    for (i = 0; i < 3; i++) {
                        x = M16(b + swap_b[i][0]);
                        M16(b + swap_b[i][0]) = M16(b + swap_b[i][1]);
                        M16(b + swap_b[i][1]) = (uint16_t)x;
                    }
                }
                if ((int16_t)M16(a) <= (int16_t)M16(a + 8))
                    M16(a) = 0;
                cx = M16(a + 8);
                if ((int16_t)M16(a + 2) < (int16_t)cx)
                    M16(a + 2) = (uint16_t)(cx + 1);
            }
        } else {
            M16(b + 2) = 0;
            M16(b + 4) = 0;
            M16(a) = 0;
        }
        if (M16(a) == 0 && M16(a + 8) == 0 && M16(a + 0x10) == 0)
            M16(M32(row + 0xc)) = 0x7d00;
    }

    if (M32(V(0x20807b58)) == 0) {
        static const uint8_t at[3][6] = {
            { 0x1a, 0x18, 0x1c, 0x1e, 0x20, 0x22 },
            { 0xe, 0xc, 0x10, 0x12, 0x14, 0x16 },
            { 2, 0, 4, 6, 8, 0xa },
        };

        r = M32(V(0x20807f58));
        for (i = 0; i < 3; i++) {
            if (M32(r + 0x60) == 1) {
                N_GET(M32(r + 4), 0x38, c + at[i][0]);
                N_GET(M32(r + 0x18), 0x38, c + at[i][1]);
                N_GET(M32(r + 0x2c), 0x21, c + at[i][2]);
                N_GET(M32(r + 0x40), 0x4b, c + at[i][3]);
                M16(c + at[i][4]) = (uint16_t)(M16(r + 0x5c) * 0x24);
                M16(c + at[i][5]) = M16(r + 0x54);
            } else {
                M16(c + at[i][4]) = 0;
                M16(c + at[i][1]) = 0;
            }
            if (i < 2) {
                r += 0x64;
                M32(V(0x20807f58)) = r;
            }
        }
        x = M32(V(0x20807f4c));
        if (M32(x + 0x140) != 0) {
            N_GET(M32(x + 0x148), 0x38, c + 0x24);
            N_GET(M32(x + 0x15c), 0x21, c + 0x26);
            N_GET(M32(x + 0x170), 0x4b, c + 0x28);
            M16(c + 0x2a) = (uint16_t)(M16(x + 0x18c) * 0x24);
            M16(c + 0x2c) = M16(x + 0x184);
        } else {
            M16(c + 0x2a) = 0;
            M16(c + 0x24) = 0;
        }
        if (M8(G + 0xf12) == 0) {
            static const uint8_t td[4] = { 0xc, 8, 4, 0 };

            HOST_(0x22c, f4)(M32(V(0x20807f5c)), 4, 0x2c, ADDR(weather_compare_temps));
            M32(V(0x20807f5c)) = V(0x20807e38);
            r = V(0x20807e38);
            for (i = 0; i < 4; i++) {
                if (M32(r + 0x28) == 1) {
                    N_GET(M32(r + 4), 0x38, d + td[i]);
                    N_GET(M32(r + 0x18), 0x3c, d + td[i] + 2);
                } else {
                    M16(d + td[i]) = 0xd8f1;
                }
                if (i < 3) {
                    r += 0x2c;
                    M32(V(0x20807f5c)) = r;
                }
            }
            x = M32(V(0x20807f50));
            N_GET(M32(x + 0xc4), 0, V(0x2080d138));
            if (M32(V(0x20807b50)) == 0)
                M16(d + 0x12) = (uint16_t)(M16(V(0x2080d138)) * 0x8e);
            else
                M16(d + 0x12) = (uint16_t)((uint32_t)M8(V(0x2080d138)) << 8);
            M16(d + 0x10) = M16(x + 0xbc);
        }
    }
    x = M32(V(0x20807f50));
    N_GET(M32(x + 0xf4), 0x24, d + 0x14);
    M16(d + 0x16) = M16(x + 0xd8);
    H_FREE(V(0x2080d0d8));
}

/* .text+0x1a4b0 */
static void weather_table(void)
{
    uint32_t gp = G;

    M32(V(0x2080d0c0)) = gp + 0xf1c;
    M32(V(0x2080d0c4)) = gp + 0xf7a;
    M32(V(0x2080d0c8)) = gp + 0xf34;
    M32(V(0x2080d0cc)) = gp + 0xf8c;
    M32(V(0x2080d0d0)) = gp + 0xf4c;
    M16(V(0x2080d0d4)) = M8(gp + 0xf12);
    M16(V(0x2080d0d6)) = M8(gp + 0xf13);
    HOST_(0x2a4, f2)(0x2100, V(0x2080d0d8));
    gp = G;
    M32(V(0x2080d0dc)) = gp + 0x1004;
    M32(V(0x2080d0e0)) = gp + 0x1062;
    M32(V(0x2080d0e4)) = gp + 0x101c;
    M32(V(0x2080d0e8)) = gp + 0x1074;
    M32(V(0x2080d0ec)) = gp + 0x1034;
    M16(V(0x2080d0f0)) = M16(gp + 0xfc0);
    M16(V(0x2080d0f2)) = M16(gp + 0xfc2);
    M32(V(0x2080d0f4)) = gp + 0xfe2;
    M32(V(0x2080d0f8)) = gp + 0x10ec;
    M32(V(0x2080d0fc)) = gp + 0x114a;
    M32(V(0x2080d100)) = gp + 0x1104;
    M32(V(0x2080d104)) = gp + 0x115c;
    M32(V(0x2080d108)) = gp + 0x111c;
    M16(V(0x2080d10c)) = M16(gp + 0x10a8);
    M16(V(0x2080d10e)) = M16(gp + 0x10aa);
    M32(V(0x2080d110)) = gp + 0x10ca;
}

/* .text+0x1a5f0 */
static void weather_load(void)
{
    uint16_t keep = M16(G + 0xc18);

    M16(G + 0xc18) = 0;
    M32(V(0x2080d12c)) = 0;
    weather_table();
    weather_row();
    M16(G + 0xc18) = keep;
}
