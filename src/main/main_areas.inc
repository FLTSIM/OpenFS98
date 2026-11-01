/* MAIN.DLL: command 0x1307, the weather areas dialog (dialog 0x1303, its copy dialog
 * 0x13050001). MSVC C, .text+0x5010..+0x6441. Up to three areas of 0x72 bytes at
 * .data+0x83f8, each with its fields' control records at 0xc8 apart from .data+0x12b0;
 * only the first two go back to GLOBAL by name, and the second is loaded from the
 * same place as the first (+0x1004), as retail does. */

#define AR_DLG      V(0x20801508)
#define AR_WHICH    V(0x20801514)
#define AR_COUNT    V(0x20801518)
#define AR_REC      V(0x208012b0)
#define AR_AREA     V(0x208083f8)
#define AR_FLAG1    V(0x208083ec)
#define AR_FLAG2    V(0x208083f0)

typedef uint32_t (FS98_STDCALL *f8)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

static const double ar_ft = -0.00053974, ar_half = 0.5, ar_m = -0.001, ar_toft = 1852.7439137362433, ar_tom = 1000.0;

static uint32_t ar_visibility(uint32_t v, double scale)
{
    return main_ftol((long double)ar_half - (long double)v * scale);
}

/* .text+0x5050: GLOBAL's visibility and cloud words to the dialog's */
static void ar_load_values(void)
{
    uint32_t gp = G;
    double scale = M16(gp + 0xc18) == 0 ? ar_ft : ar_m;

    M16(V(0x2080838e)) = 0;
    M32(V(0x20808390)) = M32(gp + 0xf98);
    M32(V(0x20808398)) = M32(gp + 0xfa0);
    M16(V(0x20808396)) = 0;
    M16(V(0x2080839e)) = 0;
    M32(V(0x208083a0)) = M32(gp + 0xfa8);
    M32(V(0x208083a8)) = M32(gp + 0xfb0);
    M16(V(0x208083a6)) = 0;
    M16(V(0x208083ba)) = 0;
    M32(V(0x208083bc)) = M32(gp + 0x1080);
    M32(V(0x208083c4)) = M32(gp + 0x1088);
    M16(V(0x208083c2)) = 0;
    M16(V(0x208083ca)) = 0;
    M32(V(0x208083cc)) = M32(gp + 0x1090);
    M32(V(0x208083d4)) = M32(gp + 0x1098);
    M16(V(0x208083d2)) = 0;
    M32(V(0x208083b4)) = M16(gp + 0xfbe) / 132u;
    M32(V(0x208083e0)) = M16(gp + 0x10a6) / 132u;
    M32(V(0x208083ac)) = ar_visibility(M32(gp + 0xfb4), scale);
    M32(V(0x208083b0)) = ar_visibility(M32(gp + 0xfb8), scale);
    M32(V(0x208083d8)) = ar_visibility(M32(gp + 0x109c), scale);
    M32(V(0x208083dc)) = ar_visibility(M32(gp + 0x10a0), scale);
}

/* .text+0x5280: and back */
static void ar_store_values(void)
{
    uint32_t gp = G;
    double scale = M16(gp + 0xc18) == 0 ? ar_toft : ar_tom;

    M32(gp + 0xf98) = M32(V(0x20808390));
    M32(gp + 0xfa0) = M32(V(0x20808398));
    M32(gp + 0xfa8) = M32(V(0x208083a0));
    M32(gp + 0xfb0) = M32(V(0x208083a8));
    M32(gp + 0x1080) = M32(V(0x208083bc));
    M32(gp + 0x1088) = M32(V(0x208083c4));
    M32(gp + 0x1090) = M32(V(0x208083cc));
    M32(gp + 0x1098) = M32(V(0x208083d4));
    M16(gp + 0xfbe) = (uint16_t)(M32(V(0x208083b4)) * 132);
    M16(gp + 0x10a6) = (uint16_t)(M32(V(0x208083e0)) * 132);
    M32(gp + 0xfb4) = main_ftol((long double)M32(V(0x208083ac)) * scale);
    M32(gp + 0xfb8) = main_ftol((long double)M32(V(0x208083b0)) * scale);
    M32(gp + 0x109c) = main_ftol((long double)M32(V(0x208083d8)) * scale);
    M32(gp + 0x10a0) = main_ftol((long double)M32(V(0x208083dc)) * scale);
}

/* .text+0x5490: an area laid out round the aircraft, named `text` */
static void ar_default(uint32_t p, uint32_t text)
{
    M32(p + 8) = M32(G + 0x564) + 0x274e8;
    M32(p + 0x10) = M32(G + 0x56c) - 1;
    M32(p + 0x18) = M32(G + 0x564) - 0x274e8;
    M32(p + 0x20) = M32(G + 0x56c) - 1;
    M32(p + 0x24) = 0x9d2a;
    M32(p + 0x28) = 0x3edd;
    M16(p + 0x2c) = 0x4000;
    M16(p + 0x2e) = 0x528;
    H_STRNCPY(p + 0x52, text, 0x1f);
}

/* .text+0x5510 */
static void ar_clear(uint32_t p)
{
    M32(p + 8) = 0;
    M32(p + 0x10) = 0;
    M32(p + 0x18) = 0;
    M32(p + 0x20) = 0;
    M32(p + 0x24) = 0;
    M32(p + 0x28) = 0;
    M16(p + 0x2c) = 0;
    M16(p + 0x2e) = 0;
    M8(p + 0x52) = 0;
}

/* .text+0x5610: a text record: its text, then its status */
static void ar_copy_text(uint32_t d, uint32_t s)
{
    H_MEMMOVE(M32(d + 4), M32(s + 4), M32(d + 8));
    M32(d) = M32(s);
}

/* .text+0x5640 */
static void ar_copy_value(uint32_t d, uint32_t s)
{
    M32(d + 4) = M32(s + 4);
    M32(d) = M32(s);
}

/* .text+0x5540: one area's records over another's; the name only if asked */
static void ar_copy_records(uint32_t d, uint32_t s, uint32_t all)
{
    static const uint8_t texts[8] = { 0x18, 0x2c, 0x40, 0x54, 0x68, 0x7c, 0x90, 0xa4 };
    uint32_t i;

    M32(d) = 1;
    if (all != 0)
        ar_copy_text(d + 4, s + 4);
    for (i = 0; i < 8; i++)
        ar_copy_text(d + texts[i], s + texts[i]);
    ar_copy_value(d + 0xb8, s + 0xb8);
    ar_copy_value(d + 0xc0, s + 0xc0);
}

/* .text+0x5fc0, +0x5ff0, +0x6020: a field within its limits */
static uint32_t ar_check_text(uint32_t rec, uint32_t limits)
{
    uint32_t slot = rec;
    uint32_t r = N_CHECK(M32(rec + 4), 0, ADDR(&slot), limits);

    M32(rec) = r;
    return r;
}

/* .text+0x5e20: the dialog's check */
static uint32_t FS98_STDCALL ar_check(uint32_t block)
{
    static const uint16_t offs[3] = { 0x68, 0x130, 0x1f8 };
    static const uint16_t offs2[3] = { 0x7c, 0x144, 0x20c };
    static const uint16_t offs3[3] = { 0xa4, 0x16c, 0x234 };
    uint32_t a, b, c, d, e, bad, i;

    a = UTIL_PUT(V(0x20801590), 1, block, 1);
    b = UTIL_PUT(V(0x20801688), 4, M32(block + 0x14) + 0xc8, 1);
    c = UTIL_PUT(V(0x208016d8), 7, M32(block + 0x14) + 0xc8, 1);
    d = UTIL_PUT(V(0x20801768), 4, M32(block + 0x14) + 0x190, 1);
    e = UTIL_PUT(V(0x208017b8), 7, M32(block + 0x14) + 0x190, 1);
    bad = (a | b | c | d | e) != 0;
    for (i = 0; i < 3; i++)
        if (ar_check_text(M32(block + 0x14) + offs[i], V(0x20801844)) != 0)
            bad = 1;
    for (i = 0; i < 3; i++)
        if (ar_check_text(M32(block + 0x14) + offs2[i], V(0x20801848)) != 0)
            bad = 1;
    for (i = 0; i < 3; i++)
        if (ar_check_text(M32(block + 0x14) + offs3[i], V(0x2080184c)) != 0)
            bad = 1;
    return bad;
}

/* .text+0x6050: Add */
static uint32_t FS98_STDCALL ar_add(uint32_t block)
{
    uint32_t n = M32(block + 0x10), at;

    if (n >= 3)
        return 0xffffffffu;
    at = n;
    M32(block + 0xc) = n;
    n++;
    M32(block + 0x10) = n;
    M32(block + 0x18) = n < 3;
    M32(block + 0x1c) = 1 < n;
    M32(V(0x208083e8) + at * 4) = 1;
    return 0;
}

/* .text+0x60a0: Delete, after asking. Taking the middle of three moves the last down. */
static uint32_t FS98_STDCALL ar_delete(uint32_t block)
{
    uint32_t at = M32(block + 0xc), n, rec;

    if (at == 0)
        return 0;
    if (at >= 3)
        return 0xffffffffu;
    if (CALL(UTIL, 0xa4, f8)(0xc0, 1, 0xf511, 0, 0, 0, 0, 0) != 0x40)
        return 0;
    at = M32(block + 0xc);
    if (at == 1 && M32(block + 0x10) == 3) {
        rec = M32(block + 0x14);
        ar_copy_records(rec + 0xc8, rec + 0x190, 1);
        M32(AR_FLAG2) = 0;
        H_MEMMOVE(V(0x2080846a), V(0x208084dc), 0x72);
    } else {
        M32(V(0x208083e8) + at * 4) = 0;
        H_MEMSET(AR_AREA + M32(block + 0xc) * 0x72, 0, 0x72);
    }
    n = M32(block + 0x10) - 1;
    M32(block + 0x10) = n;
    M32(block + 0xc) = n - 1;
    M32(block + 0x18) = n < 3;
    M32(block + 0x1c) = 1 < n;
    return 0;
}

/* .text+0x6200: the copy dialog's answer */
static void ar_copy_apply(void)
{
    uint32_t s = M32(V(0x20801554)), d = M32(V(0x20801558)), sa, da, off;

    if (M32(V(0x20801538)) != 1)
        return;
    sa = s * 0x72;
    da = d * 0x72;
    if (M32(V(0x20801560)) == 1) {
        if (d != 0)
            ar_copy_records(AR_REC + d * 0xc8, AR_REC + s * 0xc8, 0);
        H_MEMMOVE(AR_AREA + da, AR_AREA + sa, 0x72);
        return;
    }
    if (M32(V(0x20801568)) == 1) {
        H_MEMMOVE(V(0x208083f8) + da, V(0x208083f8) + sa, 0x18);
        H_MEMMOVE(V(0x20808410) + da, V(0x20808410) + sa, 0x12);
    }
    if (M32(V(0x20801570)) == 1)
        H_MEMMOVE(V(0x2080843c) + da, V(0x2080843c) + sa, 0x2e);
    if (M32(V(0x20801578)) == 1)
        for (off = 0; off < 0x14; off += 2)
            M16(V(0x20808422) + da + off) = M16(V(0x20808422) + sa + off);
    if (M32(V(0x20801580)) == 1) {
        M16(V(0x20808436) + da) = M16(V(0x20808436) + sa);
        M16(V(0x20808438) + da) = M16(V(0x20808438) + sa);
    }
    if (M32(V(0x20801588)) == 1)
        H_MEMMOVE(V(0x2080843a) + da, V(0x2080843a) + sa, 2);
}

/* .text+0x6190: Copy (+0x61d0 is its load) */
static uint32_t FS98_STDCALL ar_copy(uint32_t block)
{
    uint32_t r;

    (void)block;
    M32(V(0x20801540)) = M32(AR_WHICH);
    M32(V(0x20801538)) = 0;
    M32(V(0x20801544)) = M32(AR_COUNT);
    r = H_DIALOG(0x13050001, V(0x20801538));
    if (r == 0 && M32(V(0x20801538)) == 1)
        ar_copy_apply();
    return r;
}

/* One area out of GLOBAL, or laid out fresh */
static void ar_load_area(uint32_t at, uint32_t present, uint32_t text)
{
    if (M16(present) == 0) {
        ar_default(present, text);
        H_MEMSET(at, 0, 0x72);
        M16(at + 0x3e) = 0x3f50;
        M16(at + 0x42) = 0x7d00;
        M16(at + 0x2a) = 0xd8f1;
        M16(at + 0x2e) = 0xd8f1;
        M16(at + 0x32) = 0xd8f1;
        M16(at + 0x36) = 0xd8f1;
    } else {
        H_MEMMOVE(at, G + 0x1004, 0x18);
        H_MEMMOVE(at + 0x18, G + 0x1062, 0x12);
        H_MEMMOVE(at + 0x2a, G + 0x101c, 0x18);
        H_MEMMOVE(at + 0x42, G + 0x1074, 2);
        H_MEMMOVE(at + 0x44, G + 0x1034, 0x2e);
        M32(AR_COUNT) += 1;
    }
}

/* .text+0x5660 */
static void ar_load(void)
{
    uint32_t gp;

    H_LOADSTRING(0x2100, V(0x20808550), 0x1e);
    H_LOADSTRING(0x2101, V(0x2080856e), 0x1e);
    H_LOADSTRING(0x2102, V(0x2080858c), 0x1e);
    gp = G;
    M32(AR_DLG) = 0;
    M32(AR_FLAG1) = M16(gp + 0xf90);
    M32(AR_FLAG2) = M16(gp + 0x1078);
    H_MEMMOVE(V(0x208083f8), gp + 0xf1c, 0x18);
    H_MEMMOVE(V(0x20808410), G + 0xf7a, 0x12);
    H_MEMMOVE(V(0x20808422), G + 0xf34, 0x18);
    H_MEMMOVE(V(0x2080843a), G + 0xf8c, 2);
    H_MEMMOVE(V(0x2080843c), G + 0xf4c, 0x2e);
    M32(AR_COUNT) = 1;
    ar_load_area(V(0x2080846a), G + 0xf90, V(0x2080856e));
    ar_load_area(V(0x208084dc), G + 0x1078, V(0x2080858c));
    if (M32(AR_WHICH) >= M32(AR_COUNT))
        M32(AR_WHICH) = 0;
    ar_load_values();
    gp = G;
    M32(V(0x20801510)) = M16(gp + 0xc18);
    M32(V(0x20801520)) = M32(AR_COUNT) < 3;
    M32(V(0x20801524)) = 1 < M32(AR_COUNT);
    if ((M32(gp + 0xf18) == 2 && M32(AR_COUNT) < 3) || (M32(gp + 0xf18) == 1 && M32(AR_COUNT) < 2))
        M32(gp + 0xf18) = 0;
    M32(AR_WHICH) = M32(G + 0xf18);
    H_MEMSET(V(0x20808360), 0, 0x2c);
    UTIL_GET(V(0x208015a8), 4, V(0x208012b0));
    UTIL_GET(V(0x208015f8), 7, V(0x208012b0));
    UTIL_GET(V(0x20801688), 4, V(0x20801378));
    UTIL_GET(V(0x208016d8), 7, V(0x20801378));
    UTIL_GET(V(0x20801768), 4, V(0x20801440));
    UTIL_GET(V(0x208017b8), 7, V(0x20801440));
}

/* .text+0x5ac0 */
static void ar_apply(void)
{
    static const struct { uint16_t to, from, n; } back[] = {
        { 0xf1c, 0x83f8, 0x18 }, { 0xf7a, 0x8410, 0x12 }, { 0xf34, 0x8422, 0x18 }, { 0xf8c, 0x843a, 2 }, { 0xf4c, 0x843c, 0x2e },
        { 0x1004, 0x846a, 0x18 }, { 0x1062, 0x8482, 0x12 }, { 0x101c, 0x8494, 0x18 }, { 0x1074, 0x84ac, 2 }, { 0x1034, 0x84ae, 0x2e },
        { 0x10ec, 0x84dc, 0x18 }, { 0x114a, 0x84f4, 0x12 }, { 0x1104, 0x8506, 0x18 }, { 0x115c, 0x851e, 2 }, { 0x111c, 0x8520, 0x2e },
    };
    uint32_t a = 0, b = 0, c = 0, d = 0, i, gp;

    if (M32(AR_DLG) != 1)
        return;
    gp = G;
    M32(gp + 0xf18) = M32(AR_WHICH);
    M8(gp + 0xf12) = M8(M32(V(0x2080151c)) + 0xbc);
    M8(G + 0xf13) = M8(M32(V(0x2080151c)) + 0xc4);
    if (M32(AR_FLAG1) == 1) {
        a = UTIL_PUT(V(0x20801688), 4, V(0x20801378), 0);
        b = UTIL_PUT(V(0x208016d8), 7, V(0x20801378), 0);
    }
    if (M32(AR_FLAG2) == 1) {
        c = UTIL_PUT(V(0x20801768), 4, V(0x20801440), 0);
        d = UTIL_PUT(V(0x208017b8), 7, V(0x20801440), 0);
    }
    if ((a | b | c | d) != 0)
        return;
    M16(G + 0xf90) = (uint16_t)M32(AR_FLAG1);
    M16(G + 0x1078) = (uint16_t)M32(AR_FLAG2);
    ar_store_values();
    if (M16(G + 0xf90) == 0)
        ar_clear(G + 0xf90);
    if (M16(G + 0x1078) == 0)
        ar_clear(G + 0x1078);
    for (i = 0; i < sizeof back / sizeof back[0]; i++)
        H_MEMMOVE(G + back[i].to, D + back[i].from, back[i].n);
}

/* .text+0x5010, command 0x1307 */
static void command_1307(void)
{
    ar_load();
    if (H_DIALOG(0x1303, AR_DLG) == 0 && M32(AR_DLG) == 1)
        ar_apply();
}
