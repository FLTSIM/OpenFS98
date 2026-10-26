/* MAIN.DLL slice 19: +0x98, the cloud layers' draw.
 *
 * Retail .text+0x6f59, assembly, with the routines it reaches: .text+0x741f
 * (the layers' visibility), +0x7744 and +0x7777 (the layer block's submit),
 * +0x8275 (the cloud puffs), +0x9146 and +0x918b (the layer flag in GLOBAL
 * +0xf10), +0x9d57 (the layer's map texel under the aircraft), +0x9f39 (the
 * layer's vertical bands and where the aircraft is in them) and +0xa04b.
 *
 * +0x98(which): 1 and anything above 3 copy the aircraft's position into
 * the first layer's command block (.data+0x1aca on, and +0x1a92/+0x1a94, the
 * band edge slice 15 left) and submit it to G3D +0x24 -- block +0x2368 for
 * 1, +0x238c above 3; 2 and 3 do the same for the second layer (+0x1ae2 on,
 * +0x1aaa/+0x1aac; blocks +0x23b0 and +0x23d4).  0, unless GLOBAL +0xdce is
 * set, submits the puffs and works out which layer the aircraft is in or near
 * (from slice 15's band edges and each layer's texel under the aircraft),
 * picks the layer block's colour (.data+0x22cc), submits it, and sets GLOBAL
 * +0xf10; it always clears GLOBAL +0xc90.
 *
 * G3D (module id 14), HG2D (13) and UTIL (20) are verified in their own
 * harnesses; maindiff's stubs log what each is handed and hand back what the
 * routines read (HG2D +0x88's two bytes, +0xb4's texel row, UTIL +0x48's
 * planar offset).  The hand-rolled stack alignment around each call has no
 * observable effect and is left out, as in slice 15.
 *
 * Retail details, reproduced:
 *   - +0x9f39 scales the band's width by .data+0x346e with `imul`, then takes
 *     `shl ax,1` / `rol dx,1`: dx's own top bit comes round, not ax's.
 *   - +0x9d57's two 32-bit divides run into overflow for an aircraft far from
 *     the layer's origin and lean on the host's divide repair (0x7fffffff, or
 *     0x80000001 when the signs differ); only the quotient's middle bytes are
 *     used.
 *   - The texel index is the two quotients' second bytes, less the layer's
 *     origin bytes, as bh:bl -- each byte wraps on its own.
 *   - +0x918b's second layer test reads the first layer's +0xe6e, not
 *     +0xe76, and the position it compares (.data+0x208c) is the one the
 *     first test stored. */

/* a signed `idiv r32` under the host's divide repair: the quotient, or on
 * overflow or a divisor of 0, 0x7fffffff, or 0x80000001 when the divisor and
 * the dividend's high half differ in sign. */
static uint32_t idiv32(int64_t num, int32_t den)
{
    int64_t q = den != 0 ? num / den : 0x100000000LL;

    if (q > 0x7fffffffLL || q < -0x80000000LL) {
        int neg = (den < 0) != ((int32_t)(uint32_t)((uint64_t)num >> 32) < 0);
        return neg ? 0x80000001u : 0x7fffffffu;
    }
    return (uint32_t)(int32_t)q;
}

#define HG2D M32(V(0x20800064))
#define UTIL M32(V(0x208000a4))

/* .text+0x918b: GLOBAL +0xf10 from the layers, at most every ten ticks of
 * GLOBAL +0x260 since .data+0x2088 was last set. */
static void clouds_918b(void)
{
    uint32_t gp = G;
    uint16_t ax = 0, bx;

    if ((int32_t)(M32(gp + 0x260) - M32(V(0x20802088))) < 10)
        return;
    if (M16(gp + 0xe70) != 0) {
        if (M16(gp + 0xe6e) >= 0xc000) {
            M16(V(0x2080208c)) = M16(gp + 0x574);
            bx = M16(V(0x20801a94));
            if ((int16_t)M16(V(0x2080208c)) < (int16_t)bx)
                goto store;
            bx = (uint16_t)(bx + M16(gp + 0xe6c));
            if ((int16_t)M16(V(0x2080208c)) <= (int16_t)bx) {
                ax = 0x8005;
                goto store;
            }
        }
        if (M16(gp + 0xe78) != 0 && M16(gp + 0xe6e) >= 0xc000) {
            bx = M16(V(0x20801aac));
            if ((int16_t)M16(V(0x2080208c)) >= (int16_t)bx) {
                bx = (uint16_t)(bx + M16(gp + 0xe74));
                if ((int16_t)M16(V(0x2080208c)) <= (int16_t)bx)
                    ax = 0x8005;
            }
        }
    }
store:
    M16(gp + 0xf10) = ax;
}

/* .text+0x9146: by the view's kind (GLOBAL +0x1198's record, +0x14) either
 * restart the ten-tick wait and take GLOBAL +0xf10 from the layer block's
 * flags, or let +0x918b decide. */
static void clouds_9146(void)
{
    uint32_t gp = G;
    uint32_t k = M32(M32(gp + 0x1198) + 0x14);

    if (k == 0 || k == 1 || k == 4) {
        M32(V(0x20802088)) = M32(gp + 0x260);
        M16(gp + 0xf10) = (uint16_t)(M16(V(0x20801afa)) | M16(V(0x20801eb4)));
    } else {
        clouds_918b();
    }
}

/* .text+0x7777: the layer block (.data+0x29f0) to G3D +0x38, in grey when
 * the light (GLOBAL +0xcba) is at most 0x4000 and GLOBAL +0x282 is set. */
static void clouds_7777(void)
{
    uint32_t gp = G;

    if (M16(gp + 0xcba) <= 0x4000 && M8(gp + 0x282) != 0)
        M8(V(0x208022cc)) = 0x7e;
    CALL(G3D, 0x38, f2)(V(0x208029f0), 0);
    clouds_9146();
}

/* .text+0x7744: inside a layer. */
static void clouds_7744(void)
{
    M16(V(0x20801ff5)) = 0;
    M16(V(0x20801ff7)) = 0;
    M16(V(0x20801afa)) = 0x8007;
    M16(V(0x208022cc)) = M16(V(0x20801af8));
    clouds_7777();
}

/* .text+0x8275: the puffs, GLOBAL +0xe7c / 0x1000 rounded of them (at most
 * 16), each a pair of blocks from the table at .data+0x208e to G3D +0x38. */
static void clouds_8275(void)
{
    uint32_t gp = G;
    uint32_t n, i;

    if (M16(gp + 0xe7e) == 0 || M16(gp + 0xe7a) == 0)
        return;
    n = ((uint32_t)M16(gp + 0xe7c) * 0x10 + 0x1000) >> 16;
    for (i = 0; i < n; i++) {
        CALL(G3D, 0x38, f2)(M32(V(0x2080208e) + i * 8), 0);
        CALL(G3D, 0x38, f2)(M32(V(0x20802092) + i * 8), 0);
    }
}

/* .text+0x9d57: layer `which`'s block to G3D +0x24, then, when GLOBAL +0xcb6
 * is set, the layer's texel under the aircraft: answers 0x100 | the texel in
 * the low half (over the upper half of what HG2D +0xb4 returned), or 0 in
 * ax (the upper half left as it was) when +0xcb6 is clear. */
static uint32_t clouds_9d57(uint16_t which, uint32_t eax_in)
{
    uint32_t gp = G;
    uint32_t q1, q2, rec, row, at;
    uint16_t w0, w1, w2;
    uint32_t a, b, c;
    uint8_t bl, bh;

    M16(V(0x20803474)) = which;
    M32(V(0x208035da)) = V(0x208022f8);
    M32(V(0x20803498)) = 0x258;
    M32(V(0x20803470)) = V(0x20801a80);
    if (which == 2) {
        M32(V(0x208035da)) = V(0x2080231a);
        M32(V(0x20803498)) = 0x320;
        M32(V(0x20803470)) = V(0x20801a98);
    }
    CALL(G3D, 0x24, f1)(M32(V(0x208035da)));
    if (M16(gp + 0xcb6) == 0)
        return eax_in & 0xffff0000u;

    M32(V(0x20803498)) = 0x1000000u / M32(V(0x20803498));
    rec = M32(V(0x20803470));
    q1 = idiv32((int64_t)(int32_t)(M32(gp + 0x5b4) - M32(rec + 4)) * 0x10000,
                (int32_t)M32(V(0x20803498)));
    M16(V(0x20803438)) = (uint16_t)((int32_t)q1 >> 16);

    a = M16(gp + 0x5ba);
    b = M16(gp + 0x5bc);
    c = M16(gp + 0x5be);
    w0 = (uint16_t)(a - M16(rec + 0xa));
    b = b - M16(rec + 0xc) - (a < M16(rec + 0xa));
    w1 = (uint16_t)b;
    w2 = (uint16_t)(c - M16(rec + 0xe) - ((b >> 16) != 0));
    M16(V(0x2080203b)) = w0;
    M16(V(0x2080203d)) = w1;
    M16(V(0x2080203f)) = w2;
    CALL(UTIL, 0x48, f3)(V(0x20802039), gp + 0x5b0, V(0x20802041));

    q2 = idiv32((int64_t)(((uint64_t)(uint32_t)(int32_t)(int16_t)M16(V(0x20802047)) << 32) |
                          M32(V(0x20802043))),
                (int32_t)M32(V(0x20803498)));
    M16(V(0x2080343a)) = (uint16_t)((int32_t)q2 >> 16);

    bl = (uint8_t)(q2 >> 8);
    bh = (uint8_t)(q1 >> 8);
    if (M16(V(0x20803474)) == 1) {
        bl = (uint8_t)(bl - M8(V(0x20801ec0)));
        bh = (uint8_t)(bh - M8(V(0x20801ec4)));
    } else {
        bl = (uint8_t)(bl - M8(V(0x20801ec8)));
        bh = (uint8_t)(bh - M8(V(0x20801ecc)));
    }
    row = CALL(HG2D, 0xb4, f1)(M32(gp + 0xc8c));
    at = row + ((uint32_t)bh << 8 | bl);
    a = (row & 0xffff0000u) | 0x100 | M8(at);
    CALL(HG2D, 0xb8, f2)(M32(gp + 0xc8c), 0);
    return a;
}

/* .text+0x9f39: is the aircraft (GLOBAL +0x5c4) inside layer `which`'s
 * band?  Answers 1, or 0 with *dist its signed distance outside it (0 when
 * the layer has no texel there, or one below the threshold HG2D +0x88 gave). */
static uint16_t clouds_9f39(uint16_t which, uint16_t *dist)
{
    uint32_t gp = G;
    uint16_t ax, bx, dx, w;
    uint32_t r, p, ix;

    CALL(HG2D, 0x88, f2)(0xa200, V(0x208035de));
    M8(V(0x208033b8)) = M8(V(0x208035df));
    M8(V(0x208033b9)) = M8(V(0x208035df));

    ax = M16(gp + 0xe6a);
    bx = M16(gp + 0xe6c);
    if (which == 2) {
        ax = M16(gp + 0xe72);
        bx = M16(gp + 0xe74);
    }
    bx = (uint16_t)(bx >> 1);
    ax = (uint16_t)(ax - bx);
    M16(V(0x208035d0)) = ax;
    M16(V(0x208035d2)) = ax;
    M16(V(0x208035d4)) = ax;
    M16(V(0x208035d6)) = ax;
    bx = (uint16_t)(bx << 1);
    ax = (uint16_t)(ax + bx);
    M16(V(0x208035d8)) = ax;
    w = (uint16_t)(ax - M16(V(0x208035d0)));
    p = (uint32_t)((int32_t)(int16_t)w * (int16_t)M16(V(0x2080346e)));
    dx = (uint16_t)(p >> 16);
    dx = (uint16_t)(dx << 1 | dx >> 15);
    ax = (uint16_t)(w - dx);
    M16(V(0x208035d2)) = (uint16_t)(M16(V(0x208035d2)) + dx);
    M16(V(0x208035d4)) = (uint16_t)(M16(V(0x208035d4)) + dx);
    M16(V(0x208035d6)) = (uint16_t)(M16(V(0x208035d6)) + dx);
    ax = (uint16_t)((int16_t)ax / (int16_t)M16(V(0x20803468)));
    M16(V(0x208035d4)) = (uint16_t)(M16(V(0x208035d4)) + ax);
    ax = (uint16_t)(ax << 1);
    M16(V(0x208035d6)) = (uint16_t)(M16(V(0x208035d6)) + ax);

    r = clouds_9d57(which, which);
    if ((uint8_t)(r >> 8) == 0 || (uint8_t)r < M8(V(0x208033b9))) {
        *dist = 0;
        return 0;
    }
    ix = (uint8_t)((uint8_t)r - M8(V(0x208033b9)));
    if (ix >= 3)
        ix = 3;
    ix = (ix + 1) * 2;
    ax = M16(gp + 0x5c4);
    if (ax < M16(V(0x208035d0))) {
        *dist = (uint16_t)(0u - (uint16_t)(ax - M16(V(0x208035d0))));
        return 0;
    }
    if (ax <= M16(V(0x208035d0) + ix)) {
        *dist = (uint16_t)ix;
        return 1;
    }
    *dist = (uint16_t)(0u - (uint16_t)(ax - M16(V(0x208035d0) + ix)));
    return 0;
}

/* The near-a-layer flags: GLOBAL +0x5c4's distance outside a band from `lo`
 * of width `width`, set when inside or within 1500 either way. */
static void clouds_near(uint32_t gp, uint16_t lo, uint16_t width, uint32_t flag)
{
    uint16_t bx = M16(gp + 0x5c4), ax = lo;

    if ((int16_t)bx <= (int16_t)ax) {
        bx = (uint16_t)(0u - (uint16_t)(bx - ax));
    } else {
        ax = (uint16_t)(ax + width);
        if (bx >= ax)
            bx = (uint16_t)(0u - (uint16_t)(bx - ax));
        else
            bx = 0;
    }
    if (bx == 0 || ((int16_t)bx > (int16_t)0xfa24 && (int16_t)bx < 0x5dc))
        M16(flag) = 1;
}

/* .text+0x741f: the layers. */
static void clouds_741f(void)
{
    uint32_t gp;
    uint16_t bx = 0, cx = 0, lim;
    uint32_t t;

    M16(V(0x20801ff5)) = 0;
    M16(V(0x20801ff7)) = 0;
    if (M16(V(0x20801eb4)) & 0x8000) {
        M16(V(0x20801eb4)) = (uint16_t)(M16(V(0x20801eb4)) & 0x7fff);
        M16(V(0x20801af8)) = 0x690e;
        goto inside;
    }
    M16(V(0x20801af8)) = 0x69af;
    M16(V(0x20801afa)) = 4;
    gp = G;
    t = (uint32_t)M16(gp + 0x564) + 0x10;
    M16(V(0x20801e2e)) = M16(gp + 0x562);
    M16(V(0x20801e30)) = (uint16_t)t;
    M16(V(0x20801e32)) = (uint16_t)(M16(gp + 0x566) + (t >> 16));
    M16(V(0x20801e36)) = M16(gp + 0x56a);
    M32(V(0x20801e38)) = M32(gp + 0x56c);
    M16(V(0x20801e3e)) = M16(gp + 0x572);
    M32(V(0x20801e40)) = M32(gp + 0x574);
    if (M32(gp + 0x398) == 0) {
        if (M16(gp + 0xe86) == 1) {
            if (M16(gp + 0xe6e) >= 0xe001)
                goto inside;
            goto flag;
        }
        if (M16(gp + 0xe86) == 3) {
            if (M16(gp + 0xe76) >= 0xe001)
                goto inside;
            goto flag;
        }
        goto clear;
    }

    if (M16(gp + 0xe82) != 0 && M16(gp + 0xe70) != 0)
        clouds_near(gp, M16(V(0x20801a94)), M16(gp + 0xe6c), V(0x20801ff5));
    if (M16(gp + 0xe82) != 0 && M16(gp + 0xe78) != 0)
        clouds_near(gp, M16(V(0x20801aac)), M16(gp + 0xe74), V(0x20801ff7));
    if (M16(gp + 0xe70) == 0)
        goto clear;
    if (clouds_9f39(1, &bx) != 0)
        goto inside;
    if (M16(gp + 0xe78) == 0)
        goto first;
    cx = bx;
    if (clouds_9f39(2, &bx) != 0)
        goto inside;

    /* the second layer's distance: below it, within 0x34 (or 4 with GLOBAL
     * +0xe82 set) is near; above it, within 6 is close */
    if ((int16_t)bx < 0) {
        bx = (uint16_t)(0u - bx);
        lim = M16(gp + 0xe82) == 0 ? 0x34 : 4;
        if (bx <= lim)
            goto l_near;
    } else if (bx != 0 && bx <= 6) {
        goto close;
    }
    bx = cx;
first:
    if ((int16_t)bx < 0) {
        bx = (uint16_t)(0u - bx);
        lim = M16(gp + 0xe82) == 0 ? 0x34 : 4;
        if (bx > lim)
            goto clear;
        goto l_near;
    }
    if (bx == 0 || bx > 6)
        goto clear;
    goto close;

l_near:
    lim = M16(gp + 0xe82) == 0 ? 0x1a : 2;
    if (bx > lim)
        goto l_far;
    goto nearer;
close:
    if (bx > 3)
        goto l_far;
nearer:
    M16(V(0x208022cc)) = 0x68af;
    if (M8(V(0x20801ff9) + (M16(gp + 0x2a8) & 0x1f)) != 0)
        M16(V(0x208022cc)) = 0x69af;
    goto shown;
l_far:
    M16(V(0x208022cc)) = 0x68af;
    if (M8(V(0x20801ff9) + (M16(gp + 0x2a8) & 0x1f)) == 0)
        goto clear;
shown:
    if (M16(gp + 0xe84) != 0) {
        clouds_7777();
        goto done;
    }
clear:
    M16(V(0x20801afa)) = 0;
flag:
    clouds_9146();
    goto done;
inside:
    clouds_7744();
done:
    M16(V(0x20801eb4)) = 0;
}

/* .text+0x6f59 == Linkage +0x98. */
static uint32_t FS98_STDCALL clouds_draw(uint32_t which)
{
    uint32_t gp = G, block;

    if (which == 0) {
        if (M16(gp + 0xdce) == 0) {
            clouds_8275();
            clouds_741f();
        }
        M16(G + 0xc90) = 0;
        return 0;
    }
    if (which == 1 || which > 3) {
        block = which == 1 ? 0x20802368 : 0x2080238c;
        M16(V(0x20801aca)) = M16(gp + 0x5b2);
        M32(V(0x20801acc)) = M32(gp + 0x5b4);
        M16(V(0x20801ad2)) = M16(gp + 0x5ba);
        M32(V(0x20801ad4)) = M32(gp + 0x5bc);
        M16(V(0x20801ada)) = M16(V(0x20801a92));
        M32(V(0x20801adc)) = M32(V(0x20801a94));
    } else {
        block = which == 2 ? 0x208023b0 : 0x208023d4;
        M16(V(0x20801ae2)) = M16(gp + 0x5b2);
        M32(V(0x20801ae4)) = M32(gp + 0x5b4);
        M16(V(0x20801aea)) = M16(gp + 0x5ba);
        M32(V(0x20801aec)) = M32(gp + 0x5bc);
        M16(V(0x20801af2)) = M16(V(0x20801aaa));
        M32(V(0x20801af4)) = M32(V(0x20801aac));
    }
    CALL(G3D, 0x24, f1)(V(block));
    return 0;
}

/* ---- slice 20: +0x94, the cloud layers' textures ------------------------------ */

#define H_ATTR_GET  HOST_(0x134, f3)   /* (&flags, path, ext) */
#define H_ATTR_SET  HOST_(0x138, f3)   /* (flags, path, ext) */
#define H_JOIN      HOST_(0x184, f4)   /* (out, drive, dir, name) */

/* .text+0x77c7: make layer `layer`'s texture for coverage `id` (GLOBAL +0xe6e
 * or +0xe76): submit the layer's block, then -- when GLOBAL +0xcb6 is set --
 * read `<string 0x9006>\clouds` (extension 4, 0x10000 bytes) into the texel
 * map HG2D +0xb4 hands out, remap every texel by a curve the coverage's high
 * byte sets, and write the map to `cloudN` (N the layer), clearing its
 * read-only flag first.  Answers retail's carry: 0 when written, 1 when
 * +0xcb6 is clear or the write's open or write fails.
 *
 * Retail never checks the read's open: with no `clouds` the map is remapped
 * as it was.  And it leaves edi on the map when the write's open or write
 * fails (it put GLOBAL back in edi only on the way to the end, or before the
 * early return), so `*edi` -- the register its caller goes on reading GLOBAL
 * through -- is handed back. */
static uint32_t clouds_texture(uint16_t layer, uint16_t id, uint32_t *edi)
{
    uint32_t gp = G, map, i;
    uint8_t bl, bh, cl, ah, al;

    M8(V(0x20801ff2)) = (uint8_t)layer;
    M16(V(0x20801ff0)) = id;
    M8(V(0x20801ff4)) = 5;
    M8(V(0x20801ff3)) = 0xa0;
    M16(V(0x20801a7e)) = 0;
    if (id >= 0xe001) {
        M8(V(0x20801ff3)) = 0xa4;
        M8(V(0x20801ff4)) = 6;
        M16(V(0x20801a7e)) = 1;
    }
    M32(V(0x20801a72)) = V(0x208022f8);
    if (layer == 2)
        M32(V(0x20801a72)) = V(0x2080231a);
    CALL(G3D, 0x24, f1)(M32(V(0x20801a72)));
    *edi = gp;
    if (M16(gp + 0xcb6) == 0)
        return 1;

    H_LOADSTRING(0x9006, V(0x20801ee2), 0x100);
    H_JOIN(V(0x20801ee2), 0, V(0x20801ee2), V(0x20801fe2));
    H_FILE_OPEN(V(0x20801ede), V(0x20801ee2), 4, 0);
    map = CALL(HG2D, 0xb4, f1)(M32(G + 0xc8c));
    *edi = map;
    H_FILE_READ(M32(V(0x20801ede)), map, 0x10000, V(0x20802049));
    H_FILE_CLOSE(M32(V(0x20801ede)));

    /* the curve: past coverage 0xe0 the steepest; else 0xed * the coverage's
     * high byte, its high byte again, picks a shoulder (below 0x80) or a
     * knee */
    bl = M8(V(0x20801ff1));
    if (bl > 0xe0) {
        bl = 0x80;
    } else {
        bl = (uint8_t)((0xedu * bl) >> 8);
        if (bl < 0x80) {
            bl = (uint8_t)(bl + bl);
            bh = (uint8_t)~bl;
            for (i = 0; i < 0x10000; i++) {
                al = 0;
                ah = M8(map + i);
                if (ah >= bh) {
                    uint32_t q;
                    ah = (uint8_t)(ah - bh);
                    q = bl != 0 ? ((uint32_t)ah << 8) / bl : 0x100;
                    al = q > 0xff ? 0xff : (uint8_t)q;     /* the host's divide repair */
                }
                cl = M8(V(0x20801ff4));
                al = (uint8_t)((uint8_t)(al >> (cl & 0x1f)) + M8(V(0x20801ff3)));
                M8(map + i) = al;
            }
            goto written;
        }
    }
    bl = (uint8_t)(bl - 0x80);
    bl = (uint8_t)(bl + bl);
    bh = (uint8_t)~bl;
    for (i = 0; i < 0x10000; i++) {
        ah = M8(map + i);
        al = ah < bh ? (uint8_t)(((uint32_t)ah << 8) / bh) : 0xff;
        cl = M8(V(0x20801ff4));
        al = (uint8_t)((uint8_t)(al >> (cl & 0x1f)) + M8(V(0x20801ff3)));
        M8(map + i) = al;
    }
written:
    M8(V(0x20801fee)) = (uint8_t)(M8(V(0x20801ff2)) + 0x30);
    H_LOADSTRING(0x9006, V(0x20801ee2), 0x100);
    H_JOIN(V(0x20801ee2), 0, V(0x20801ee2), V(0x20801fe9));
    H_ATTR_GET(V(0x2080204d), V(0x20801ee2), 4);
    M32(V(0x2080204d)) = M32(V(0x2080204d)) & 0xfffffffdu;
    H_ATTR_SET(M32(V(0x2080204d)), V(0x20801ee2), 4);
    if (H_FILE_OPEN(V(0x20801ede), V(0x20801ee2), 4, 1) != 0)
        return 1;
    if (H_FILE_WRITE(M32(V(0x20801ede)), map, 0x10000, V(0x20802049)) != 0)
        return 1;                              /* the file stays open */
    H_FILE_CLOSE(M32(V(0x20801ede)));
    *edi = G;
    CALL(HG2D, 0xb8, f2)(M32(G + 0xc8c), 2);
    return 0;
}

/* .text+0x7140 == Linkage +0x94: remake each layer's texture whose coverage's
 * high byte has changed since the last (.data+0x1eda, +0x1edc), when GLOBAL
 * +0x398 is set, keeping the new one only when it was written; then the cloud
 * base for the haze (GLOBAL +0xcd4): the first layer's +0xe6a, at most 0x2bc
 * once GLOBAL +0xe8a reaches 0x7d00, or 0 with no first layer.  WEATHERM
 * calls it.  Everything after a failed write is read through, and the result
 * written into, the texel map (clouds_texture's edi). */
static uint32_t FS98_STDCALL clouds_textures(void)
{
    uint32_t e = G;
    uint16_t ax;

    if (M32(e + 0x398) != 0) {
        if (M16(e + 0xe70) != 0) {
            ax = (uint16_t)(M16(e + 0xe6e) & 0xff00);
            if (ax != M16(V(0x20801eda)) && clouds_texture(1, M16(e + 0xe6e), &e) == 0)
                M16(V(0x20801eda)) = ax;
        }
        if (M16(e + 0xe78) != 0) {
            ax = (uint16_t)(M16(e + 0xe76) & 0xff00);
            if (ax != M16(V(0x20801edc)) && clouds_texture(2, M16(e + 0xe76), &e) == 0)
                M16(V(0x20801edc)) = ax;
        }
    }
    ax = 0;
    if (M16(e + 0xe70) != 0) {
        ax = M16(e + 0xe6a);
        if (M16(e + 0xe8a) >= 0x7d00 && (int16_t)ax > 0x2bc)
            ax = 0x2bc;
    }
    M16(e + 0xcd4) = ax;
    return 0;
}
