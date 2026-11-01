/* MAIN.DLL: command 0x1502, the view options dialog (dialog 0x1508), a record of 0xb4
 * per window and a preview. MSVC C, .text+0x16e40..+0x17c97, with +0x177c0. */

#define VW_DLG    V(0x20806e98)
#define VW_COUNT  V(0x20806eb0)
#define VW_LIST   V(0x20806eb4)
#define VW_WIN    V(0x2080d008)
#define VW_EYE    V(0x2080d024)
#define VW_RING   V(0x2080d0b4)
#define VW_ANGLE  V(0x20806ec0)
#define VW_ITEM   0xb0

/* A text field's buffer, and its status and format beside it */
static void vw_field(uint32_t rec, uint32_t at, uint32_t low, uint32_t high, uint32_t from, uint32_t type)
{
    H_ALLOC(1, 8, rec + at, 1);
    M32(rec + at - 4) = 0;
    M32(rec + at + 4) = 7;
    M32(rec + at + 8) = low;
    M32(rec + at + 0xc) = high;
    N_PUT(from, type, M32(rec + at));
}

/* .text+0x16f60 */
static uint32_t vw_load(void)
{
    uint32_t list = 0, n = 0, i, rec, item, v;

    N_PUT(V(0x20806e20), 0x1003a, V(0x20806e28));
    N_PUT(V(0x20806e24), 0x1003a, V(0x20806e38));
    N_PUT(V(0x20806e48), 0x1003a, V(0x20806e50));
    N_PUT(V(0x20806e4c), 0x1003a, V(0x20806e60));
    N_PUT(V(0x20806e70), 0x10013, V(0x20806e78));
    N_PUT(V(0x20806e74), 0x10013, V(0x20806e88));
    M32(V(0x20806ea4)) = ((CALL(M32(V(0x2080007c)), 0x30, f0)() & 0xff) >> 2) & 1;
    M32(V(0x20806ea8)) = M16(G + 0xc18);
    CALL(WINDOW_, 0x80, f3)(ADDR(&list), ADDR(&n), 3);
    M32(VW_COUNT) = n;
    M32(V(0x20806eac)) = 0;
    H_ALLOC(5, n * 0xb4, VW_LIST, 1);
    for (i = 0; (int32_t)i < (int32_t)n; i++) {
        rec = M32(VW_LIST) + i * 0xb4;
        item = M32(list + i * 4);
        M32(rec) = M32(item);
        v = M32(item + 0x1c);
        M32(rec + 4) = M32(v + 0x10);
        M32(rec + 8) = M32(v + 0x14);
        M32(rec + 0x70) = M32(v + 0xbc);
        M32(rec + 0x74) = M32(v + 0x18);
        M16(rec + 0x78) = M16(v + 0x28);
        M32(rec + 0xa4) = M8(v + 0x30);
        M32(rec + 0xa8) = M8(v + 0x31) == 0;
        M32(rec + 0xac) = (uint32_t)M16(v + 0x26) >> 5;
        M32(rec + VW_ITEM) = item;
        vw_field(rec, 0x10, V(0x20806e78), V(0x20806e88), v + 0x1c, 0x13);
        vw_field(rec, 0x24, V(0x20806e78), V(0x20806e88), v + 0x1e, 0x13);
        vw_field(rec, 0x38, V(0x20806e78), V(0x20806e88), v + 0x20, 0x13);
        vw_field(rec, 0x60, V(0x20806e78), V(0x20806e88), v + 0x24, 0x13);
        vw_field(rec, 0x4c, V(0x20806e78), V(0x20806e88), v + 0x22, 0x13);
        vw_field(rec, 0x80, V(0x20806e28), V(0x20806e38), v + 0x2c, 0x1003a);
        vw_field(rec, 0x94, V(0x20806e50), V(0x20806e60), v + 0x34, 0x1003a);
    }
    H_FREE(ADDR(&list));
    M32(V(0x20806eb8)) = M16(G + 0x600);
    M32(V(0x20806ebc)) = M16(G + 0x552);
    return 0;
}

/* .text+0x17480 */
static void vw_apply(void)
{
    uint32_t i, rec, v;

    for (i = 0; (int32_t)i < (int32_t)M32(VW_COUNT); i++) {
        rec = M32(VW_LIST) + i * 0xb4;
        v = M32(M32(rec + VW_ITEM) + 0x1c);
        M32(v + 0xbc) = M32(rec + 0x70);
        M32(v + 0x18) = M32(rec + 0x74);
        M16(v + 0x28) = M16(rec + 0x78);
        M8(v + 0x30) = M8(rec + 0xa4);
        M8(v + 0x31) = M32(rec + 0xa8) == 0;
        M16(v + 0x26) = (uint16_t)(M16(rec + 0xac) << 5);
        N_GET(M32(rec + 0x10), 0x10013, v + 0x1c);
        N_GET(M32(rec + 0x24), 0x10013, v + 0x1e);
        N_GET(M32(rec + 0x38), 0x10013, v + 0x20);
        N_GET(M32(rec + 0x60), 0x10013, v + 0x24);
        N_GET(M32(rec + 0x4c), 0x10013, v + 0x22);
        N_GET(M32(rec + 0x80), 0x1003a, v + 0x2c);
        N_GET(M32(rec + 0x94), 0x1003a, v + 0x34);
        CALL(WINDOW_, 0x5c, f2)(M32(rec + VW_ITEM), M32(rec + 4));
        CALL(WINDOW_, 0x58, f2)(M32(rec + VW_ITEM), M32(rec + 8));
        CALL(WINDOW_, 0x24, f2)(M32(M32(rec + VW_ITEM) + 0x1c), M32(rec + 0x74));
    }
    M16(G + 0x600) = M16(V(0x20806eb8));
    M16(G + 0x552) = M16(V(0x20806ebc));
}

/* .text+0x16e40, command 0x1502 */
static uint32_t command_1502(void)
{
    static const uint8_t held[7] = { 0x10, 0x24, 0x38, 0x60, 0x4c, 0x80, 0x94 };
    uint32_t r = vw_load(), i, j;

    r = H_DIALOG(0x1508, VW_DLG);
    if (r == 0 && M32(VW_DLG) == 1) {
        vw_apply();
        r = 0;
    }
    for (i = 0; (int32_t)i < (int32_t)M32(VW_COUNT); i++)
        for (j = 0; j < 7; j++)
            H_FREE(M32(VW_LIST) + i * 0xb4 + held[j]);
    H_FREE(VW_LIST);
    return r;
}

/* .text+0x177c0: the argument slot takes the value */
static uint32_t vw_limit(uint32_t rec, uint32_t type, uint32_t limits)
{
    uint32_t slot = type, r = N_CHECK(M32(rec + 4), type, ADDR(&slot), limits);

    M32(rec) = r;
    return r != 0 ? 0xffffffffu : 0;
}

/* .text+0x17670: the check: one field by the window's type (+8) */
static uint32_t FS98_STDCALL vw_check(uint32_t block)
{
    uint32_t rec = M32(block + 0x1c) + M32(block + 0x14) * 0xb4, type = M32(rec + 8), slot = block, r;

    switch (type) {
    case 0:
        r = N_GET(M32(rec + 0x10), 0x13, ADDR(&slot));
        M32(M32(block + 0x1c) + M32(block + 0x14) * 0xb4 + 0xc) = r;
        return r;
    case 1:
        r = N_GET(M32(rec + 0x24), 0x13, ADDR(&slot));
        M32(M32(block + 0x1c) + M32(block + 0x14) * 0xb4 + 0x20) = r;
        return r;
    case 2:
        r = N_GET(M32(rec + 0x38), 0x13, ADDR(&slot));
        M32(M32(block + 0x1c) + M32(block + 0x14) * 0xb4 + 0x34) = r;
        return r;
    case 4:
        vw_limit(rec + 0x90, 0x1003a, V(0x20806e48));
        vw_limit(M32(block + 0x1c) + M32(block + 0x14) * 0xb4 + 0x7c, 0x1003a, V(0x20806e20));
        r = N_GET(M32(M32(block + 0x1c) + M32(block + 0x14) * 0xb4 + 0x60), 0x13, ADDR(&slot));
        M32(M32(block + 0x1c) + M32(block + 0x14) * 0xb4 + 0x5c) = r;
        return r;
    case 3:
        r = N_GET(M32(rec + 0x4c), 0x13, ADDR(&slot));
        M32(M32(block + 0x1c) + M32(block + 0x14) * 0xb4 + 0x48) = r;
        return r;
    }
    return 0;
}

/* .text+0x17800: the preview: 1 opens, 2 closes, 3 draws. The compass ring goes on
 * only for a window of the type that has one. */
static uint32_t FS98_STDCALL vw_preview(uint32_t message, uint32_t block, uint32_t window)
{
    uint32_t rec = M32(block + 0x1c) + M32(block + 0x14) * 0xb4, ok, p, r;
    uint16_t a = 0, b = 0;

    M32(V(0x2080d0ac)) = rec + 0x78;
    ok = M32(block + 0x14) != 0xffffffffu && M32(block + 0x18) != 0;
    if (message == 1) {
        if (ok) {
            M16(V(0x2080d000)) = M16(G + 0x116e);
            M16(G + 0x116e) = 1;
            H_MEMSET(VW_WIN, 0, 0xa4);
            H_ALLOC(1, 0xc0, VW_EYE, 1);
            if (HOST_(0x74, f2)(window, VW_WIN) != 0)
                return 0xffffffffu;
            M32(V(0x2080d020)) = M32(M32(rec + VW_ITEM) + 0x18);
            H_MEMMOVE(M32(VW_EYE), M32(M32(rec + VW_ITEM) + 0x1c), 0xc0);
            M32(G + 0x1194) = VW_WIN;
            M32(G + 0x1198) = M32(VW_EYE);
            M32(VW_RING) = 0;
            return G;
        }
        H_MEMSET(VW_WIN, 0, 0xa4);
        H_ALLOC(1, 0xc0, VW_EYE, 1);
        return HOST_(0x74, f2)(window, VW_WIN) != 0 ? 0xffffffffu : 0;
    }
    if (message == 2) {
        if (ok) {
            if (M32(VW_RING) != 0) {
                HOST_(0x284, f2)(VW_WIN, VW_ANGLE);
                M32(VW_RING) = 0;
            }
            HOST_(0x78, f1)(VW_WIN);
            H_FREE(VW_EYE);
            M16(G + 0x116e) = M16(V(0x2080d000));
            return G;
        }
        HOST_(0x78, f1)(VW_WIN);
        return H_FREE(VW_EYE);
    }
    if (message != 3)
        return ok;
    if (!ok)
        return 0;
    if (M32(VW_RING) == 0 && M32(rec + 4) == 0 && M32(rec + 8) == 4) {
        HOST_(0x27c, f4)(VW_WIN, VW_ANGLE, 0, 0);
        M32(VW_RING) = 1;
    }
    if (M32(VW_RING) != 0 && (M32(rec + 4) != 0 || M32(rec + 8) != 4)) {
        HOST_(0x284, f2)(VW_WIN, VW_ANGLE);
        M32(VW_RING) = 0;
    }
    if (M32(V(0x2080d0b8)) != M32(block + 0x14)) {
        M32(V(0x2080d0b8)) = M32(block + 0x14);
        H_MEMMOVE(M32(VW_EYE), M32(M32(rec + VW_ITEM) + 0x1c), 0xc0);
    }
    p = M32(VW_EYE);
    M16(p + 0x56) = M16(p + 0x14);
    M32(p + 0x18) = M32(rec + 0x74);
    M16(p + 0x28) = M16(rec + 0x78);
    M8(p + 0x30) = M8(rec + 0xa4);
    M8(p + 0x31) = M32(rec + 0xa8) == 0;
    M16(p + 0x26) = (uint16_t)(M16(rec + 0xac) << 5);
    N_GET(M32(rec + 0x10), 0x13, p + 0x1c);
    N_GET(M32(rec + 0x24), 0x13, p + 0x1e);
    N_GET(M32(rec + 0x38), 0x13, p + 0x20);
    N_GET(M32(rec + 0x60), 0x13, p + 0x24);
    N_GET(M32(rec + 0x4c), 0x13, p + 0x22);
    N_GET(M32(rec + 0x80), 0x3a, p + 0x2c);
    N_GET(M32(rec + 0x94), 0x3a, p + 0x34);
    M16(G + 0x2aa) = 0;
    CALL(WINDOW_, 0x5c, f2)(VW_WIN, M32(rec + 4));
    CALL(WINDOW_, 0x58, f2)(VW_WIN, M32(rec + 8));
    CALL(WINDOW_, 0x24, f2)(M32(VW_EYE), M32(rec + 0x74));
    ((f4)(uintptr_t)M32(V(0x2080d020)))(0, VW_WIN, 0, 0);
    if (M32(rec + 4) == 0 && M32(rec + 8) == 4) {
        int32_t cx = (int32_t)M32(V(0x2080d068)) >> 1, cy = (int32_t)M32(V(0x2080d06c)) >> 1, radius;

        radius = cx >= cy ? cy - 0xa : cx - 0xa;
        CALL(UTIL, 0x2c, f3)(M16(M32(V(0x2080d0ac))), ADDR(&a), ADDR(&b));
        M32(G + 0x2ec) = 0x6974;
        CALL(HG2D, 0x34, f5)(V(0x2080d088), (uint16_t)(((radius * (int16_t)a) >> 0xf) + cx),
                             (uint16_t)(cy - ((radius * (int16_t)b) >> 0xf)), (uint32_t)cx, (uint32_t)cy);
    }
    r = HOST_(0x84, f1)(VW_WIN);
    return r;
}
