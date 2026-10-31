/* MAIN.DLL slice 30: the radios, assembly, CHAIN lists 3 and 5.
 *
 * Retail .text+0x1816e (registers them) and +0x18208 (removes them), +0x18261
 * (list 3: every second time, a tuning change or a move of far enough looks the
 * stations up again) and +0x18344 (list 5: the needles), with what they reach,
 * +0x18394..+0x19f0f.  FE (18) finds stations, UTIL (20) marshals them and does
 * the geometry, SOUND (39) hears the idents.
 *
 * Retail details, reproduced:
 *   - +0x18b3d's cx survives UTIL +0x6c only as that routine leaves it: +0x19704
 *     saves ebx, esi and edi but not ecx, so a glide slope check that reaches it
 *     hands the course on in what is left there.
 *   - So does its localiser's: UTIL +0x60 runs between the bearing and GLOBAL
 *     +0xc50, which then holds what that call left in cx.
 *   - +0x197b1 compares two record pointers with constants (0x12160, 0xb4dc), as
 *     signed values, where it meant a distance; a module's address always passes.
 *   - +0x1923d's NAV2 mirror steps the "retune" word with `inc` where NAV1's sets
 *     it to 1.
 *   - +0x18261 saves GLOBAL +0x37e across the second lookup only.
 *   - FE's first arguments, like SOUND's, are words pushed over stale bytes, and
 *     FE +0x28's second is a byte over a stale register; the harness logs what
 *     reaches them.
 *   - The word and byte divides can overflow and lean on the host's repair. */

#define FE_      M32(V(0x2080005c))
#define MARKERS  M32(V(0x2080002c))

/* A .text descriptor for UTIL +0x88, its pointers into .data ours. */
struct radio_field { uint32_t a, b, c, at; };

static void radio_marshal(uint32_t buf, const struct radio_field *f, uint32_t n)
{
    uint32_t desc[12 * 4], i;

    for (i = 0; i < n; i++) {
        desc[i * 4] = f[i].a;
        desc[i * 4 + 1] = f[i].b;
        desc[i * 4 + 2] = f[i].c;
        desc[i * 4 + 3] = V(f[i].at);
    }
    CALL(UTIL, 0x88, f3)(buf, ADDR(desc), n);
}

/* .text+0x18394 */
static void radio_mark(uint32_t rec)
{
    uint32_t gp = G;

    M32(rec) = M32(gp + 0x564);
    M32(rec + 4) = M32(gp + 0x56c);
    M32(rec + 8) = M32(gp + 0x574);
}

/* .text+0x1962c: the range from `rec` to `pos`, out[0..2] the offsets. */
static uint32_t radio_range(uint32_t rec, uint32_t pos, uint32_t *out)
{
    uint32_t gp = G, d, borrow;
    struct { uint16_t w0, w1; uint32_t hi; } dlon;
    uint32_t metres[2];
    uint64_t sum;

    borrow = M16(pos + 2) < M16(rec + 2);
    out[2] = M32(pos + 4) - M32(rec + 4) - borrow;
    d = M32(rec + 0x14);
    if (d == 0xffffd8f1u)
        d = (uint32_t)((int32_t)M32(gp + 0x20) >> 8);
    out[1] = 0u - (d - M32(pos + 0x14));
    borrow = M16(pos + 0xa) < M16(rec + 0xa);
    dlon.w0 = 0;
    dlon.w1 = (uint16_t)(M16(pos + 0xa) - M16(rec + 0xa));
    dlon.hi = M32(pos + 0xc) - M32(rec + 0xc) - borrow;
    metres[0] = metres[1] = 0;
    CALL(UTIL, 0x48, f3)(ADDR(&dlon), pos, ADDR(metres));
    out[0] = metres[1];
    sum = (uint64_t)((int64_t)(int32_t)out[0] * (int32_t)out[0]);
    sum += (uint64_t)((int64_t)(int32_t)out[1] * (int32_t)out[1]);
    sum += (uint64_t)((int64_t)(int32_t)out[2] * (int32_t)out[2]);
    return CALL(UTIL, 0x18, f1)(ADDR(&sum));
}

/* .text+0x195ab: the bearing of (x, y), scaled up first. */
static uint16_t radio_bearing(uint32_t x, uint32_t y)
{
    if ((x | y) != 0) {
        while ((int16_t)(y >> 16) > -0x4000 && (int16_t)(y >> 16) < 0x4000 &&
               (int16_t)(x >> 16) > -0x4000 && (int16_t)(x >> 16) < 0x4000) {
            y += y;
            x += x;
        }
    }
    return (uint16_t)CALL(UTIL, 0x3c, f2)(x, y);
}

/* .text+0x19507: the range and bearing; the range lands in .data+0x72e0 too. */
static void radio_fix(uint32_t rec, uint32_t pos, uint32_t bearing, uint32_t range)
{
    uint32_t o[3], r = radio_range(rec, pos, o);

    M32(range) = r;
    M32(V(0x208072e0)) = r;
    M16(bearing) = radio_bearing(o[0], o[2]);
}

/* .text+0x19549: the glide angle. */
static uint16_t radio_glide(uint32_t rec, uint32_t pos)
{
    uint32_t o[3], r = radio_range(rec, pos, o), h = o[1];
    uint16_t cx, bx;

    if (M8(V(0x2080735d)) & 0x10)
        M32(V(0x208072e0)) = r;
    cx = (uint16_t)(r >> 1);
    if ((int32_t)h <= 0)
        h = 0;
    bx = (uint16_t)h;
    if ((uint32_t)bx + bx > 0xffff || (uint16_t)(bx + bx) >= cx)
        return 0x7fff;
    return (uint16_t)(((uint32_t)(uint16_t)h << 16) / cx);
}

/* .text+0x1943f: ahead (bx), abeam or behind, as the to/from flag. */
static void radio_quadrant(uint16_t a, uint16_t *out, uint8_t *flag)
{
    int16_t bx = (int16_t)a;
    uint8_t al;

    if (bx >= 0) {
        if (bx <= 0x38e3) {
            al = 2;
        } else if (bx <= 0x471c) {
            al = 0;
        } else {
            bx = (int16_t)(0u - (uint16_t)(bx - 0x7fff));
            al = 1;
        }
    } else if (bx >= (int16_t)0xc71d) {
        al = 2;
    } else if (bx >= (int16_t)0xb8e4) {
        al = 0;
    } else {
        bx = (int16_t)(0u - (uint16_t)(bx + 0x7fff));
        al = 1;
    }
    *out = (uint16_t)bx;
    *flag = al;
}

/* .text+0x1949e: the needle, limited to 127. */
static uint16_t radio_needle(uint16_t bx, uint8_t cl)
{
    int16_t m = (int16_t)M16(V((cl & 0x80) ? 0x20807470 : 0x2080746e));
    uint32_t p = (uint32_t)((int32_t)(int16_t)bx * m);
    uint16_t dx = (uint16_t)(p >> 16), ax;

    dx = (uint16_t)(dx + (((uint16_t)p >> 15) & 1));
    ax = dx;
    if ((int16_t)ax < 0) {
        if ((int16_t)ax < (int16_t)0xff81)
            ax = 0xff81;
    } else if ((int16_t)ax > 0x7f) {
        return 0xff81;
    }
    return (uint16_t)(0u - ax);
}

/* .text+0x194fd's digits: three of `v` at `p`. */
static void radio_digits(uint16_t v, uint32_t p)
{
    uint8_t d = 0x30;

    while ((int16_t)v >= 0x64) {
        v = (uint16_t)(v - 0x64);
        d++;
    }
    M8(p) = d;
    d = 0x30;
    while ((int16_t)v >= 0xa) {
        v = (uint16_t)(v - 0xa);
        d++;
    }
    M8(p + 1) = d;
    M8(p + 2) = (uint8_t)(v + 0x30);
}

/* .text+0x19704: is the terrain masking the station?  Clears *flag if so. */
static void radio_mask_step(uint32_t a8, uint32_t ac, uint32_t b0, uint32_t b4, uint32_t b8,
                            uint32_t bc, uint32_t dist, uint32_t alt, uint8_t *flag)
{
    int64_t p;
    uint32_t e1, e2, top, span;
    uint16_t w, ax, dx;

    *flag = 1;
    p = (int64_t)(int32_t)(alt - a8) * (int32_t)ac;
    e1 = (uint32_t)((uint64_t)p >> 16) + b0;
    if ((int32_t)e1 >= (int32_t)dist)
        return;
    p = (int64_t)(int32_t)(alt - b4) * (int32_t)b8;
    e2 = (uint32_t)((uint64_t)p >> 16) + bc;
    top = e2 - dist;
    span = (e2 - e1) >> 8;
    if (span == 0)
        return;
    w = (uint16_t)(top / span);
    main_xcall0(SLOT_OF(UTIL, 0x6c));
    ax = (uint16_t)(AX >> 8);
    dx = (uint16_t)(w >> 4);
    ax = (uint16_t)(ax - dx);
    if ((int16_t)ax < 0 || (int16_t)ax <= (int16_t)w)
        return;
    *flag = 0;
}

/* .text+0x197b1: the station's class picks the mask's lines. */
static void radio_mask(uint32_t dist, uint32_t rec, uint32_t alt)
{
    uint8_t flag;
    uint16_t cls;

    if ((int32_t)alt <= 0xf4) {
        if ((int32_t)dist <= 0x15b4)
            return;
        radio_mask_step(0, 0x155c80c, 0, 0, 0x25f9c8b, 0, dist, alt, &flag);
    } else {
        cls = M16(rec + 0x20);
        if (cls == 0)
            return;
        if (cls <= 0x19) {
            if ((int32_t)alt <= 0x7be) {
                if ((int32_t)rec <= 0xb4dc)
                    return;
                radio_mask_step(0xf4, 0x55476, 0xb4dc, 0x98, 0xf30b7, 0xb4dc, dist, alt, &flag);
            } else {
                radio_mask_step(0x7be, 0xfffaab8au, 0xd908, 0x7be, 0xfff0cf49u, 0xfd34, dist, alt, &flag);
            }
        } else if (cls < 0x32) {
            if ((int32_t)alt <= 0xb50) {
                if ((int32_t)rec <= 0x12160)
                    return;
                radio_mask_step(0xf4, 0x6fbea, 0x12160, 0x98, 0xd80a2, 0x12160, dist, alt, &flag);
            } else {
                radio_mask_step(0xb50, 0xfff90416u, 0x1458c, 0xb50, 0xfff27f5eu, 0x169b8, dist, alt, &flag);
            }
        } else if ((int32_t)alt <= 0x1144) {
            if ((int32_t)dist <= 0x1458c)
                return;
            radio_mask_step(0xf4, 0x27ea84, 0x1458c, 0x98, 0x293b14, 0x169b8, dist, alt, &flag);
        } else if ((int32_t)alt <= 0x2581) {
            if ((int32_t)dist <= 0x2d370)
                return;
            radio_mask_step(0x1144, 0x725fc, 0x3d0a4, 0x1144, 0xe4bf8, 0x3f4d0, dist, alt, &flag);
        } else {
            if ((int32_t)dist <= 0x2d370)
                return;
            radio_mask_step(0x2581, 0xfff998a8u, 0x46154, 0x2581, 0xfff55715u, 0x51630, dist, alt, &flag);
        }
    }
    if (flag == 0)
        M16(rec + 0x18) = 0;
}

/* .text+0x19eb2: an OBS copied out and its range taken, when ax says. */
static void radio_obs(uint16_t ax, uint32_t from)
{
    uint32_t o[3];

    if (ax & 1) {
        CALL(UTIL, 0x60, f2)(from, V(0x20807316));
        M32(V(0x208072e0)) = radio_range(V(0x20807316), G + 0x560, o);
    }
}

static const struct radio_field radio_nav1[] = {
    { 3, 3, 6, 0x20807341 }, { 1, 1, 2, 0x20807359 }, { 1, 1, 2, 0x2080735b }, { 1, 0, 0, 0x2080735d },
    { 1, 1, 2, 0x20807361 }, { 1, 1, 2, 0x2080735f }, { 5, 0, 0, 0x20807363 }, { 0x18, 0, 0, 0x20807369 },
    { 8, 0, 0, 0x20807382 }, { 1, 1, 2, 0x2080738b }, { 1, 1, 2, 0x2080738d }, { 8, 0, 0, 0x2080738f },
};
static const struct radio_field radio_nav2[] = {
    { 3, 3, 6, 0x20807399 }, { 1, 1, 2, 0x208073b1 }, { 1, 1, 2, 0x208073b3 }, { 1, 0, 0, 0x208073b5 },
    { 1, 1, 2, 0x208073b9 }, { 1, 1, 2, 0x208073b7 }, { 5, 0, 0, 0x208073bb }, { 0x18, 0, 0, 0x208073c1 },
    { 8, 0, 0, 0x208073da }, { 1, 1, 2, 0x208073e3 }, { 1, 1, 2, 0x208073e5 }, { 8, 0, 0, 0x208073e7 },
};
static const struct radio_field radio_adf[] = {
    { 3, 3, 6, 0x208073f1 }, { 1, 1, 2, 0x20807409 }, { 5, 0, 0, 0x2080740b }, { 0x18, 0, 0, 0x20807411 },
};

/* .text+0x183c4: the two NAV radios tuned afresh. */
static void radio_tune_nav(void)
{
    uint32_t gp = G;
    uint16_t keep;

    M8(gp + 0x388) &= 0xfc;
    radio_mark(V(0x2080748a));
    M16(gp + 0x37e) = 0;
    M8(V(0x2080735d)) = 0;
    M16(V(0x20807359)) = 0;
    M8(V(0x208073b5)) = 0;
    M16(V(0x208073b1)) = 0;
    if (CALL(FE_, 0x2c, f3)(M16(gp + 0x350), G + 0x560, V(0x208070b8)) != 0)
        radio_marshal(V(0x208070b8), radio_nav1, 12);
    keep = M16(G + 0x37e);
    if (CALL(FE_, 0x2c, f3)(M16(G + 0x352), G + 0x560, V(0x208070b8)) != 0)
        radio_marshal(V(0x208070b8), radio_nav2, 12);
    M16(G + 0x37e) = keep;
    M16(V(0x20807431)) = M16(V(0x20807359));
    M16(V(0x20807449)) = M16(V(0x20807359));
    CALL(SOUND_, 0x24, f2)(0x17, V(0x2080742d));
    if (M8(V(0x2080735d)) & 0x11)
        CALL(SOUND_, 0x24, f2)(0x17, V(0x20807445));
    M16(V(0x2080743d)) = M16(V(0x208073b1));
    M16(V(0x20807455)) = M16(V(0x208073b1));
    CALL(SOUND_, 0x24, f2)(0x17, V(0x20807439));
    if (M8(V(0x208073b5)) & 0x11)
        CALL(SOUND_, 0x24, f2)(0x17, V(0x20807451));
}

/* .text+0x1859f: the ADF. */
static void radio_tune_adf(void)
{
    uint32_t gp = G;
    uint16_t w;
    uint8_t cl;

    M8(gp + 0x389) &= 0x7c;
    radio_mark(V(0x208074a2));
    w = M16(gp + 0x356);
    cl = (uint8_t)((w & 0xf) | ((w >> 8) << 4));
    if (CALL(FE_, 0x28, f4)(M16(gp + 0x34c), cl, gp + 0x560, V(0x20807054)) == 0) {
        M16(V(0x20807409)) = 0;
        return;
    }
    radio_marshal(V(0x20807054), radio_adf, 4);
    M16(V(0x20807461)) = M16(V(0x20807409));
    CALL(SOUND_, 0x24, f2)(0x17, V(0x2080745d));
}

/* .text+0x18679: the marker beacons in range, into the table at .data+0x7522. */
static void radio_tune_markers(void)
{
    uint32_t gp = G, n = 0, p, e;
    uint8_t kind;

    radio_mark(V(0x20807502));
    CALL(FE_, 0x54, f3)(gp + 0x560, 0x1d4c, gp + 0x3e4);
    M8(V(0x20807522)) = 0;
    M32(V(0x20807862)) = V(0x20807522);
    while ((uint16_t)CALL(FE_, 0x20, f3)(n, V(0x20807866), V(0x2080786a)) != 0) {
        p = M32(V(0x20807866));
        kind = M8(p);
        if ((int8_t)kind >= 4 && (int8_t)kind <= 6 &&
            (int32_t)(M32(V(0x20807862)) - V(0x20807842)) < 0) {
            e = M32(V(0x20807862));
            M32(e) = M8(p);
            M32(e + 0x1c) = M8(p + 0xa);
            M8(e + 0x20) = 0;
            CALL(UTIL, 0x60, f2)(M32(V(0x20807866)) + 2, e + 4);
            M32(V(0x20807862)) += 0x20;
        }
        n = 1;
    }
}

/* .text+0x189c6: the wind's station. */
static void radio_tune_atis(void)
{
    uint32_t gp = G;

    M32(gp + 0x38c) = 0;
    radio_mark(V(0x208074d2));
    M16(gp + 0x2a0) = (uint16_t)CALL(FE_, 0x30, f1)(gp + 0x560);
}

/* .text+0x18a17: the COM's. */
static void radio_tune_com(void)
{
    uint32_t gp = G, out = 0;

    M8(gp + 0x38a) = 0;
    radio_mark(V(0x208074ba));
    if (CALL(FE_, 0x24, f3)(M16(gp + 0x34e), gp + 0x560, ADDR(&out)) != 0)
        CALL(MARKERS, 0x20, f1)(out);
}

/* .text+0x18989: the season again. */
static void radio_tune_season(void)
{
    nav_season(0, M16(G + 0x23e));
}

/* .text+0x18aa5: a table's retune, when the aircraft has moved beyond its box.
 * Its routine is a retail code address at +0x14, mapped here. */
static void radio_retune(uint32_t rec)
{
    uint32_t gp = G, d, o[2];

    d = M32(rec) - M32(gp + 0x564);
    if ((int32_t)d < 0)
        d = 0u - d;
    if ((int32_t)(d - M32(rec + 0xc)) < 0) {
        struct { uint16_t w0, w1; uint32_t hi; } dlon;

        dlon.w0 = 0;
        dlon.w1 = 0;
        dlon.hi = M32(rec + 4) - M32(gp + 0x56c);
        o[0] = o[1] = 0;
        CALL(UTIL, 0x48, f3)(ADDR(&dlon), gp + 0x560, ADDR(o));
        d = o[1];
        if ((int32_t)d < 0)
            d = 0u - d;
        if ((int32_t)(d - M32(rec + 0xc)) < 0) {
            d = M32(rec + 8) - M32(gp + 0x574);
            if ((int32_t)d < 0)
                d = 0u - d;
            if ((int32_t)(d - M32(rec + 0x10)) < 0)
                return;
        }
    }
    switch (M32(rec + 0x14)) {
    case 0x207f93c4: radio_tune_nav(); break;
    case 0x207f959f: radio_tune_adf(); break;
    case 0x207f9679: radio_tune_markers(); break;
    case 0x207f99c6: radio_tune_atis(); break;
    case 0x207f9989: radio_tune_season(); break;
    case 0x207f9a17: radio_tune_com(); break;
    default: break;
    }
}

/* .text+0x18261: CHAIN list 3. */
static uint32_t FS98_STDCALL radio_list3(uint32_t a, uint32_t b)
{
    uint32_t gp = G;

    (void)a;
    M32(V(0x20807050)) += 1;
    if (M32(V(0x20807050)) & 1)
        return b;
    if (M16(gp + 0x444) & 2) {
        M16(gp + 0x444) &= 0xfffd;
        radio_tune_com();
        radio_tune_adf();
        radio_tune_nav();
        radio_tune_atis();
        radio_tune_markers();
        nav_season(0, M16(gp + 0x23e));
    }
    if (M8(gp + 0x388) & 2)
        radio_tune_nav();
    if (M8(gp + 0x389) & 2)
        radio_tune_adf();
    if (M32(gp + 0x38c) != 0)
        radio_tune_atis();
    if (M8(gp + 0x38a) != 0)
        radio_tune_com();
    radio_retune(V(0x208074ba));
    radio_retune(V(0x208074a2));
    radio_retune(V(0x2080748a));
    radio_retune(V(0x20807502));
    radio_retune(V(0x208074d2));
    radio_retune(V(0x208074ea));
    return b;
}

/* .text+0x18f2a: the glide slope's needle. */
static void radio_glide_needle(void)
{
    uint32_t gp = G, range;
    uint16_t bearing, angle, cx;
    uint32_t keep;
    uint8_t al = 0;

    if (M16(V(0x20807359)) == 0 || !(M8(V(0x2080735d)) & 0x40) || M8(V(0x20807469)) != 1) {
        M8(gp + 0xc4c) = 0;
        M8(gp + 0xc49) = 0;
        return;
    }
    CALL(UTIL, 0x60, f2)(V(0x20807382), V(0x208072f8));
    if (M32(G + 0x84c) == 0) {
        CALL(UTIL, 0x60, f2)(V(0x20807382), G + 0x858);
        M16(G + 0x870) = (uint16_t)(M16(V(0x2080735f)) + 0x8000);
        M16(G + 0x872) = (uint16_t)(((int32_t)(int16_t)0xeba0 * (int16_t)M16(V(0x2080738b))) >> 16);
    }
    angle = radio_glide(V(0x208072f8), G + 0x560);
    keep = M32(V(0x208072e0));
    radio_fix(V(0x208072f8), G + 0x560, ADDR(&bearing), ADDR(&range));
    M32(V(0x208072e0)) = keep;
    cx = (uint16_t)(bearing - M16(V(0x2080735f)));
    if ((int16_t)cx < (int16_t)0xc001 || (int16_t)cx > 0x3fff) {
        int16_t ax = (int16_t)(angle - M16(V(0x2080738b)));

        M8(G + 0xc4c) = 1;
        if (ax >= 0x641)
            ax = 0x641;
        if (ax <= (int16_t)0xf9bf)
            ax = (int16_t)0xf9bf;
        al = (uint8_t)(((int32_t)ax * 0x1330) >> 16);
    } else {
        M8(G + 0xc4c) = 0;
    }
    M8(G + 0xc49) = al;
}

/* The DME text and the ground speed, NAV1's and NAV2's by turns (.text+0x18d8a,
 * +0x1923d). */
struct radio_dme {
    uint32_t mode, freq, flags, tenth, whole, rate, text, speed, range_was, tick;
    int inc;
};

static void radio_dme(const struct radio_dme *r)
{
    uint32_t gp = G, q, q1, q2;
    uint8_t al, dl, old_al, old_dl, cl;
    uint16_t word;

    if (M8(gp + 0xb70) != 0 || M16(r->freq) == 0 || !(M8(r->flags) & 1)) {
        if (M16(r->mode) != 1) {
            if (r->inc)
                M16(r->mode) = (uint16_t)(M16(r->mode) + 1);
            else
                M16(r->mode) = 1;
            M32(r->whole) = 0xffffffffu;
            M32(r->rate) = 0xffffffffu;
            M16(gp + r->speed) = 0x3b3b;
            M8(gp + r->speed + 2) = 0x3b;
            M16(gp + r->text) = 0x3b3b;
            M8(gp + r->text + 3) = 0x3b;
        }
        return;
    }
    if (M16(r->mode) & 1)
        M8(r->tenth + 1) = 0x96;
    M16(r->mode) = (uint16_t)(M16(r->mode) >> 1);
    q = M32(V(0x208072e0));
    M32(r->whole) = q;
    SET_AX(q);
    SET_DX(q >> 16);
    div16(0x73c);
    q1 = AX;
    SET_AX(0);
    div16(0x73c);
    q2 = AX;
    M32(r->whole) = (uint16_t)(q1 * M16(V(0x20807474)));
    M32(r->whole) += ((uint32_t)q2 * M16(V(0x20807474))) >> 16;
    dl = (uint8_t)(q1 >= 0x64 ? 0x63 : q1);
    al = (uint8_t)(((uint32_t)(uint8_t)(q2 >> 8) * M8(V(0x2080746b))) >> 8);
    old_dl = M8(r->tenth + 1);
    M8(r->tenth + 1) = dl;
    old_al = M8(r->tenth);
    M8(r->tenth) = al;
    if ((old_al | old_dl) != 0) {
        cl = (uint8_t)(M8(r->tenth) + 0x30);
        al = M8(r->tenth + 1);
        word = (uint16_t)(((uint32_t)(al % 10) << 8 | al / 10) + 0x3030);
        if (word != M16(gp + r->text) || cl != M8(gp + r->text + 3)) {
            M8(gp + r->text + 3) = cl;
            M16(gp + r->text) = word;
        }
    }
    M8(r->tick) = (uint8_t)(M8(r->tick) - 1);
    if (M8(r->tick) == 0) {
        uint32_t was;

        M8(r->tick) = 4;
        was = M32(r->range_was);
        M32(r->range_was) = M32(V(0x208072e0));
        q = was - M32(V(0x208072e0));
        if ((int32_t)q < 0)
            q = 0u - q;
        eax = q;
        mul32(0x100);
        div32(M16(gp + 0xc1a));
        q = eax;
        q = (uint32_t)(((uint64_t)q * 0x1f1a0u)) >> 0x13;
        M32(r->rate) = q;
        radio_digits((uint16_t)q >= 0x3e7 ? 0x3e7 : (uint16_t)q, gp + r->speed);
    }
}

/* .text+0x18b3d: NAV1. */
static void radio_nav1_needle(void)
{
    uint32_t gp = G, range;
    uint16_t bearing, out, ax, bx;
    uint8_t al, flag;

    M8(gp + 0xc4a) = 0;
    if (M16(V(0x20807359)) == 0) {
        bx = 0xff88;
        M8(V(0x2080735d)) |= 8;
        al = 0;
    } else {
        M8(V(0x2080735d)) &= 0xf7;
        M8(gp + 0xc4a) |= 0x80;
        radio_fix(V(0x20807341), gp + 0x560, ADDR(&bearing), ADDR(&range));
        SET_CX(bearing);
        if (M8(V(0x2080735d)) & 0x80) {
            M8(gp + 0xc4a) |= 2;
            if (!(M8(V(0x2080735d)) & 0x20))
                M8(gp + 0xc4a) |= 1;
            if (M32(gp + 0x84c) != 1) {
                main_xcall2(SLOT_OF(UTIL, 0x60), V(0x20807382), gp + 0x858);
                M16(gp + 0x870) = (uint16_t)(M16(V(0x2080735f)) + 0x8000);
                M16(gp + 0x872) = 0;
            }
            ax = M16(V(0x2080735f));
        } else if (M8(V(0x2080735d)) & 8) {
            bx = 0xff88;
            al = 0;
            goto have_flag;
        } else {
            if (M8(V(0x2080786e)) == 0)
                radio_mask(range, V(0x20807341), M32(gp + 0x574) - M32(V(0x20807355)));
            M8(V(0x2080786e)) = (uint8_t)(M8(V(0x2080786e)) + 1);
            if (M8(V(0x2080786e)) == 0x40)
                M8(V(0x2080786e)) = 0;
            SET_CX(CX + M16(V(0x2080735b)));
            SET_DX(M16(gp + 0xc4e));
            SET_AX(0);
            div16(M16(V(0x2080746c)));
            ax = AX;
            if (M32(gp + 0x84c) != 1) {
                M16(gp + 0x870) = (uint16_t)(ax + 0x8000);
                M32(gp + 0x85c) = M32(V(0x20807345));
                M16(gp + 0x85a) = M16(V(0x20807343));
                M32(gp + 0x864) = M32(V(0x2080734d));
                M16(gp + 0x862) = M16(V(0x2080734b));
            }
        }
        M16(gp + 0xc50) = CX;
        radio_quadrant((uint16_t)(CX - ax), &out, &flag);
        bx = out;
        al = flag;
        M8(V(0x20807469)) = al;
        if (al == 2)
            M8(gp + 0xc4a) |= 4;
        if (M8(gp + 0xc4a) & 2)
            al = 1;
    }
have_flag:
    M8(gp + 0xc4b) = al;
    if ((M8(V(0x2080735d)) & 0x80) && (M8(V(0x2080735d)) & 0x20) && M8(V(0x20807469)) != 1) {
        M8(gp + 0xc4b) = 0;
        bx = 0xfc18;
    }
    M16(V(0x20807482)) = bx;
    M8(gp + 0xc48) = (uint8_t)radio_needle(bx, M8(V(0x2080735d)));
    radio_glide_needle();
    radio_obs(M16(V(0x2080738d)), V(0x2080738f));
    M8(gp + 0xc43) = (uint8_t)(M8(gp + 0xc43) - 1);
    if (M8(gp + 0xc43) == 0) {
        static const struct radio_dme d = {
            0x208072dc, 0x20807359, 0x2080735d, 0x208072e4, 0x208072e8, 0x208072ec,
            0xc29, 0xc2e, 0x2080747a, 0x20807476, 0
        };
        struct radio_dme r = d;

        M8(gp + 0xc43) = 0xc;
        r.mode = V(r.mode);
        r.freq = V(r.freq);
        r.flags = V(r.flags);
        r.tenth = V(r.tenth);
        r.whole = V(r.whole);
        r.rate = V(r.rate);
        r.range_was = V(r.range_was);
        r.tick = V(r.tick);
        radio_dme(&r);
    }
}

/* .text+0x1908c: NAV2. */
static void radio_nav2_needle(void)
{
    uint32_t gp = G, range;
    uint16_t bearing, out, ax, bx;
    uint8_t al, flag;

    M8(gp + 0xc5a) = 0;
    M8(gp + 0xc5c) = 0;
    if (M16(V(0x208073b1)) == 0) {
        bx = 0xff88;
        M8(V(0x208073b5)) |= 8;
        al = 0;
    } else {
        M8(V(0x208073b5)) &= 0xf7;
        M8(gp + 0xc5a) |= 0x80;
        radio_fix(V(0x20807399), gp + 0x560, ADDR(&bearing), ADDR(&range));
        SET_CX(bearing);
        if (M8(V(0x208073b5)) & 0x80) {
            M8(gp + 0xc5a) |= 2;
            if (!(M8(V(0x208073b5)) & 0x20))
                M8(gp + 0xc5a) |= 1;
            ax = M16(V(0x208073b7));
        } else if (M8(V(0x208073b5)) & 8) {
            bx = 0xff88;
            al = 0;
            goto have_flag;
        } else {
            if (M8(V(0x2080786f)) == 0)
                radio_mask(range, V(0x20807399), M32(gp + 0x574) - M32(V(0x208073ad)));
            M8(V(0x2080786f)) = (uint8_t)(M8(V(0x2080786f)) + 1);
            if (M8(V(0x2080786f)) == 0x40)
                M8(V(0x2080786f)) = 0;
            SET_CX(CX + M16(V(0x208073b3)));
            SET_DX(M16(gp + 0xc5e));
            SET_AX(0);
            div16(M16(V(0x2080746c)));
            ax = AX;
        }
        M16(gp + 0xc60) = CX;
        radio_quadrant((uint16_t)(CX - ax), &out, &flag);
        bx = out;
        al = flag;
        M8(V(0x2080746a)) = al;
        if (al == 2)
            M8(gp + 0xc5a) |= 4;
        if (M8(V(0x208073b5)) & 0x80)
            al = 1;
    }
have_flag:
    M8(gp + 0xc5b) = al;
    if ((M8(V(0x208073b5)) & 0x80) && (M8(V(0x208073b5)) & 0x20) && M8(V(0x2080746a)) != 1) {
        M8(gp + 0xc5b) = 0;
        bx = 0xfc18;
    }
    M16(V(0x20807484)) = bx;
    M8(gp + 0xc59) = (uint8_t)radio_needle(bx, M8(V(0x208073b5)));
    radio_obs(M16(V(0x208073e5)), V(0x208073e7));
    M16(V(0x20807478)) = (uint16_t)(M16(V(0x20807478)) - 1);
    if (M16(V(0x20807478)) == 0) {
        static const struct radio_dme d = {
            0x208072de, 0x208073b1, 0x208073b5, 0x208072e6, 0x208072f0, 0x208072f4,
            0xc33, 0xc38, 0x2080747e, 0x20807477, 1
        };
        struct radio_dme r = d;

        M16(V(0x20807478)) = 0xc;
        r.mode = V(r.mode);
        r.freq = V(r.freq);
        r.flags = V(r.flags);
        r.tenth = V(r.tenth);
        r.whole = V(r.whole);
        r.rate = V(r.rate);
        r.range_was = V(r.range_was);
        r.tick = V(r.tick);
        radio_dme(&r);
    }
}

/* .text+0x199e8: the ADF's needle, once a frame. */
static void radio_adf_needle(void)
{
    uint32_t gp = G, range;
    uint16_t bearing;

    if (M16(V(0x20807409)) == 0 || (M8(gp + 0x389) & 0x40))
        return;
    radio_fix(V(0x208073f1), gp + 0x560, ADDR(&bearing), ADDR(&range));
    M16(gp + 0xc6a) = (uint16_t)((((uint32_t)bearing << 16) - M32(gp + 0x580)) >> 16) + 0x8000;
}

/* .text+0x19a3e: the marker lamps. */
static void radio_markers(void)
{
    uint32_t gp = G, e = V(0x20807522), range;
    uint16_t lit = (uint16_t)(M16(gp + 0xbac) | M16(gp + 0xbae) | M16(gp + 0xbb0));

    for (;; e += 0x20) {
        struct { uint16_t w0, w1; uint32_t w2; uint16_t w3, w4; uint32_t w5; uint16_t w6, w7; uint32_t w8; } here;
        uint32_t d;
        uint16_t bearing, s, c, cx, ax, bx;
        uint32_t p;

        if (M32(gp + 0x7a8) == 0 || M32(e) == 0) {
            if (lit != 0) {
                M16(gp + 0xbac) = 0;
                M16(gp + 0xbae) = 0;
                M16(gp + 0xbb0) = 0;
                CALL(SOUND_, 0x24, f2)(0x12, 0);
            }
            return;
        }
        d = M32(gp + 0x574) - M32(e + 0x18);
        M16(V(0x20807332)) = (uint16_t)d;
        if ((int32_t)d < 0 || (int32_t)(d - 0xbe8) >= 0)
            continue;
        memset(&here, 0, sizeof here);
        here.w1 = M16(e + 6);
        here.w2 = M32(e + 8);
        here.w4 = M16(e + 0xe);
        here.w5 = M32(e + 0x10);
        here.w7 = M16(gp + 0x572);
        here.w8 = M32(gp + 0x574);
        radio_fix(ADDR(&here), gp + 0x560, ADDR(&bearing), ADDR(&range));
        cx = (uint16_t)((bearing & 0xff) | (uint16_t)(((bearing >> 8) - M8(e + 0x1c) + 0x40) & 0xff) << 8);
        CALL(UTIL, 0x2c, f3)(cx, ADDR(&s), ADDR(&c));
        cx = c;
        if ((int16_t)cx < 0)
            cx = (uint16_t)(0u - cx);
        ax = (uint16_t)(((uint32_t)0x1cc * cx) >> 16);
        ax = (uint16_t)(ax + 0x133);
        p = (uint32_t)ax * M16(V(0x20807332));
        ax = (uint16_t)(p >> 8);
        bx = (uint16_t)((uint16_t)range - ax);
        if ((int16_t)bx >= 0)
            continue;
        if ((int8_t)M8(e) > 5) {
            if (M16(gp + 0xbb0) == 0) {
                M16(gp + 0xbac) = 0;
                M16(gp + 0xbae) = 0;
                M16(gp + 0xbb0) = 1;
                CALL(SOUND_, 0x24, f2)(0xf, 0);
            }
        } else if (M8(e) == 5) {
            if (M16(gp + 0xbae) == 0) {
                M16(gp + 0xbac) = 0;
                M16(gp + 0xbae) = 1;
                M16(gp + 0xbb0) = 0;
                CALL(SOUND_, 0x24, f2)(0x10, 0);
            }
        } else if (M16(gp + 0xbac) == 0) {
            M16(gp + 0xbac) = 1;
            M16(gp + 0xbae) = 0;
            M16(gp + 0xbb0) = 0;
            CALL(SOUND_, 0x24, f2)(0x11, 0);
        }
        return;
    }
}

/* .text+0x18344: CHAIN list 5. */
static uint32_t FS98_STDCALL radio_list5(uint32_t a, uint32_t b)
{
    uint32_t gp = G;

    (void)a;
    radio_nav1_needle();
    radio_nav2_needle();
    if (M32(gp + 0x2ac) != M32(V(0x2080732e))) {
        M32(V(0x2080732e)) = M32(gp + 0x2ac);
        radio_adf_needle();
    }
    M8(V(0x20807334)) = (uint8_t)(M8(V(0x20807334)) - 1);
    if ((int8_t)M8(V(0x20807334)) < 0) {
        M8(V(0x20807334)) = 5;
        radio_markers();
    }
    return b;
}

/* .text+0x1816e */
static void radios_start(void)
{
    uint32_t gp = G;

    M8(V(0x20807368)) = 0;
    M8(V(0x20807381)) = 0;
    M8(V(0x208073c0)) = 0;
    M8(V(0x208073d9)) = 0;
    M8(V(0x20807410)) = 0;
    M8(V(0x20807429)) = 0;
    CHAIN_ADD(3, ADDR(radio_list3));
    CHAIN_ADD(5, ADDR(radio_list5));
    M32(gp + 0x384) = V(0x20807335);
    M8(gp + 0xc43) = 1;
}

/* .text+0x18208 */
static void radios_stop(void)
{
    CHAIN_REMOVE(3, ADDR(radio_list3));
    CHAIN_REMOVE(5, ADDR(radio_list5));
}
