/* MAIN.DLL: command 0x1201, Select Aircraft: the list and its turning preview.
 * MSVC C, .text+0x110, +0x250, +0x5a0, +0x640..+0x690. */

#define ACFT_DLG    V(0x2082ede0)        /* the dialog's block */
#define ACFT_WHICH  V(0x2082edf4)
#define ACFT_COUNT  V(0x2082edf8)
#define ACFT_LIST   V(0x2082edfc)        /* records of 0xa08, ACONTAIN's */
#define ACFT_REC    V(0x2082ed20)        /* the preview's host window, 0xa4 bytes */
#define ACFT_BM     V(0x2082eda0)        /* its bitmap: +0xc stride, +0x18 bits */
#define ACFT_W      V(0x2082eda4)
#define ACFT_H      V(0x2082eda8)
#define ACFT_SIZE   V(0x2082edb4)
#define ACFT_BITS   V(0x2082edb8)
#define ACFT_EYE    V(0x2082ed3c)        /* its block of 0xc0 */
#define ACFT_OBJ    V(0x20807f90)        /* the aircraft loaded: +0, +4 its records */
#define ACFT_SHOWN  V(0x20807fa0)

/* .text+0x690 */
static uint32_t FS98_STDCALL aircraft_minus1(uint32_t a)
{
    (void)a;
    return 0xffffffffu;
}

/* .text+0x5a0: 1 if anything was drawn on the border of the rectangle, which
 * is why a model too big for the window is moved back. */
static uint32_t FS98_STDCALL aircraft_border(uint32_t bm, uint32_t c, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint32_t stride, bits, at, row, i;

    if ((int32_t)x < (int32_t)w) {
        stride = M32(bm + 0xc);
        bits = M32(bm + 0x18);
        row = y * stride + bits;
        for (i = x; (int32_t)i < (int32_t)w; i++) {
            if (M8(i + row) != (uint8_t)c)
                return 1;
            if (M8((h - 1) * stride + i + bits) != (uint8_t)c)
                return 1;
        }
    }
    if ((int32_t)y >= (int32_t)h)
        return 0;
    stride = M32(bm + 0xc);
    at = y * stride + M32(bm + 0x18) + w;
    for (i = y; (int32_t)i < (int32_t)h; i++) {
        if (M8(x - w + at) != (uint8_t)c)
            return 1;
        if (M8(at - 1) != (uint8_t)c)
            return 1;
        at += stride;
    }
    return 0;
}

/* .text+0x250: the preview: 1 opens it, 2 closes it, 3 draws `rec` a step on. */
static uint32_t FS98_STDCALL aircraft_preview(uint32_t message, uint32_t rec, uint32_t window)
{
    uint32_t p;

    if (message == 1) {
        H_MEMSET(ACFT_REC, 0, 0xa4);
        H_ALLOC(1, 0xc0, ACFT_EYE, 1);
        if (HOST_(0x74, f2)(window, ACFT_REC) != 0)
            return 0xffffffffu;
        p = M32(ACFT_EYE);
        M32(p) = (uint32_t)((int32_t)M32(ACFT_W) / 2);
        M32(p + 8) = 1u - (uint32_t)((int32_t)M32(ACFT_H) / 2);
        M32(p + 4) = (uint32_t)((int32_t)M32(ACFT_W) / 2);
        M32(p + 0xc) = (uint32_t)((int32_t)M32(ACFT_H) / 2);
        M32(ACFT_SHOWN) = 0;
        CALL(ACONTAIN, 0x20, f2)(ACFT_OBJ, 0);
        return 0;
    }
    if (message == 2) {
        HOST_(0x78, f1)(ACFT_REC);
        H_FREE(ACFT_EYE);
        M32(ACFT_SHOWN) = 0;
        CALL(ACONTAIN, 0x24, f1)(ACFT_OBJ);
        return 0;
    }
    if (message != 3)
        return 0;
    H_MEMSET(M32(ACFT_BITS), 0x96, M32(ACFT_SIZE));
    M32(G + 0x1194) = ACFT_REC;
    M32(G + 0x1198) = M32(ACFT_EYE);
    M16(G + 0x120a) = 0;
    M32(G + 0xb28) = 0x24540000u;
    M32(G + 0xb2c) = 0xb1e00000u;
    M32(G + 0xb30) = 0x24540000u;
    H_MEMMOVE(G + 0x5b0, M32(ACFT_EYE) + 0x58, 0x18);
    H_MEMMOVE(G + 0x5c8, M32(ACFT_EYE) + 0x70, 0xc);
    CALL(WINDOW_, 0x2c, f4)(0, 0, 0, G + 0xce8);
    M16(G + 0xd44) = 6;
    M16(G + 0xd46) = 0x7fff;
    CALL(WINDOW_, 0x30, f0)();
    if (rec != M32(ACFT_SHOWN)) {
        CALL(ACONTAIN, 0x38, f1)(ACFT_OBJ);
        CALL(ACONTAIN, 0x24, f1)(ACFT_OBJ);
        M32(ACFT_SHOWN) = rec;
        CALL(ACONTAIN, 0x20, f2)(ACFT_OBJ, rec);
        if (CALL(ACONTAIN, 0x34, f2)(ACFT_OBJ, 1) == 0)
            return 0;
        p = M32(ACFT_OBJ + 4);
        M32(p + 8) = 0x20;
        M32(p + 0x1c) = 0x4000000;
        M32(p + 0x20) = 0x4000000;
        M32(p + 0x68) = 0x8002;
    }
    if (CALL(ACONTAIN, 0x3c, f1)(ACFT_OBJ) != 1)
        return 0;
    p = HOST_(0x38, f0)();
    M32(M32(ACFT_OBJ + 4) + 0x24) = p << 0x11;
    CALL(G3D, 0x18, f2)(M32(M32(ACFT_OBJ) + 4), M32(ACFT_OBJ + 4));
    HOST_(0x84, f1)(ACFT_REC);
    if (aircraft_border(ACFT_BM, 0xffffff96u, 0, 0, M32(ACFT_W), M32(ACFT_H)) == 0)
        return 0;
    p = M32(ACFT_OBJ + 4);
    if (M32(M32(ACFT_OBJ) + 0xc) < 0xa)
        M32(p + 8) += 3;
    else
        M32(p + 8) += 0x1e;
    return 0;
}

/* .text+0x640: the dialog's picture box; a choice past the list goes back to
 * the first. */
static uint32_t FS98_STDCALL aircraft_box(uint32_t message, uint32_t block, uint32_t window)
{
    if (M32(block + 0x14) > M32(block + 0x18))
        M32(block + 0x14) = 0;
    return aircraft_preview(message, M32(block + 0x1c) + M32(block + 0x14) * 0xa08, window);
}

/* .text+0x110, command 0x1201 */
static uint32_t command_1201(void)
{
    char path[0x104];
    uint32_t i;

    M32(ACFT_DLG) = 0;
    M32(ACFT_DLG + 4) = ADDR(main_nothing);
    M32(ACFT_DLG + 8) = ADDR(aircraft_minus1);
    M32(ACFT_DLG + 0xc) = ADDR(main_nothing);
    M32(ACFT_DLG + 0x10) = ADDR(aircraft_box);
    CALL(ACONTAIN, 0x1c, f2)(ACFT_COUNT, ACFT_LIST);
    CALL(ACONTAIN, 0x30, f4)(G + 0x11e0, 1, ADDR(path), 0x104);
    if (path[0] == 0) {
        M32(ACFT_WHICH) = 0;
    } else {
        for (i = 0; (int32_t)i < (int32_t)M32(ACFT_COUNT); i++)
            if (H_STRICMP(ADDR(path), M32(ACFT_LIST) + i * 0xa08) == 0) {
                M32(ACFT_WHICH) = i;
                break;
            }
    }
    H_DIALOG(0x1201, ACFT_DLG);
    if (M32(ACFT_DLG) != 0) {
        M8(G + 0x608) = (uint8_t)M32(ACFT_WHICH);
        CALL(ACONTAIN, 0x28, f1)(M32(ACFT_LIST) + M32(ACFT_WHICH) * 0xa08);
        flight_quicksave();
    }
    H_FREE(ACFT_LIST);
    return M32(ACFT_DLG);
}
