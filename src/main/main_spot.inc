/* MAIN.DLL slice 21: +0xa8, the spot plane's eye.
 *
 * Retail .text+0x6450, assembly, with .text+0x669a..+0x6c87: +0x669a (place
 * the eye afresh), +0x66db and +0x6740 (the aircraft it follows, by the
 * spot view's mode), +0x67fc (the offset from the view's direction), +0x68ee
 * and +0x69eb (the slewed transition), +0x6a8a (walk the eye to where G3D's
 * distance is least), +0x6c1c and +0x6c4d (measure through G3D +0x1c), and
 * +0x6c88 (has the followed aircraft moved).
 *
 * The eye is a 48-bit position at .data+0x1868 stepped by UTIL's vector
 * services (+0x74 add, +0x78 sub, +0x7c copy, +0x80 equal, +0x84 shift; +0x2c
 * sincos, +0x44 metres to longitude, +0x60 unpack) and measured by G3D +0x1c,
 * which leaves its distance in GLOBAL +0xd4c (GLOBAL +0xe88, held at 0x7fff
 * across the call, restored after).  WINDOW, CONTROLS and ADVDRV call it.
 *
 * Retail details, reproduced:
 *   - The transition's step count is 3, 7 or 15 for GLOBAL +0xe60 0, 1 or 2;
 *     for anything else retail's `loop` counts with whatever G3D +0x1c left in
 *     ecx.  The game sets only 0 to 2.  Not reproduced: this transliteration
 *     takes no steps there, and maindiff keeps +0xe60 in 0..2.
 *   - The followed aircraft's height term in mode 2 is an unsigned 32x32
 *     product whose high dword becomes the count kept in .data+0x1924.
 *   - +0x69eb raises .data+0x1888 by a quarter turn across its walk and puts
 *     it back; nothing reads it in between.
 *   - The angle handed to UTIL +0x2c is pushed as a word over two stale
 *     bytes, so only its low word counts (as in slice 12).
 *
 * The stack-alignment shims are left out, as in slices 15 and 19. */

#define SPOT_UTIL(off, fn) CALL(UTIL, off, fn)
#define V48_ADD   SPOT_UTIL(0x74, f3)   /* (dst, a, b) */
#define V48_SUB   SPOT_UTIL(0x78, f3)   /* (dst, a, b) */
#define COPY24    SPOT_UTIL(0x7c, f2)   /* (dst, src) */
#define V48_EQUAL SPOT_UTIL(0x80, f2)
#define SHR48     SPOT_UTIL(0x84, f3)   /* (dst, src, n) */

/* .text+0x6c4d: measure `block` through G3D +0x1c with GLOBAL +0xe88 held at
 * 0x7fff. */
static void spot_measure(uint32_t block)
{
    uint32_t gp = G;
    uint16_t saved = M16(gp + 0xe88);

    M16(gp + 0xe88) = 0x7fff;
    CALL(G3D, 0x1c, f1)(block);
    M16(G + 0xe88) = saved;
}

/* .text+0x6c1c: the eye's block (.data+0x1946), its colour .data+0x195c 0x3b,
 * or 0x22 while .data+0x187c is below GLOBAL +0x20 >> 8 (signed). */
static void spot_eye(void)
{
    uint32_t gp = G;
    uint16_t dx = 0x3b;

    if ((int32_t)M32(V(0x2080187c)) < (int32_t)M32(gp + 0x20) >> 8)
        dx = 0x22;
    M16(V(0x2080195c)) = dx;
    spot_measure(V(0x20801946));
}

/* .text+0x6740: the aircraft the spot view follows, by GLOBAL +0xe64 (4, or 2
 * with +0xe66 choosing the second of GLOBAL +0x384's pair).  Answers 1
 * (carry) when there is none; otherwise *eax the height term shifted up a
 * word, *ecx the count, *ebx its position. */
static int spot_target(uint32_t *eax, uint32_t *ecx, uint32_t *ebx)
{
    uint32_t gp = G, list = M32(gp + 0x384), a, d, b;
    uint64_t p;

    a = (uint32_t)(int32_t)(int16_t)M16(gp + 0xc4e);
    b = M32(list);
    if (M16(gp + 0xe64) != 4) {
        if (M16(gp + 0xe64) != 2)
            return 1;
        if (M16(gp + 0xe66) != 0) {
            a = (uint32_t)(int32_t)(int16_t)M16(gp + 0xc5e);
            b = M32(list + 4);
        }
    }
    if (b == 0)
        return 1;
    p = (uint64_t)a * M32(V(0x20801860));
    a = (uint32_t)p;
    d = (uint32_t)(p >> 32);
    if (M16(b + 0x18) == 0)
        return 1;
    a -= (uint32_t)(int32_t)(int16_t)M16(b + 0x1a);
    if (M16(gp + 0xe64) == 4) {
        if (!(M8(b + 0x1c) & 0x80))
            return 1;
        p = (uint64_t)((int64_t)(int16_t)M16(b + 0x4a) * (int32_t)M32(V(0x20801864)));
        d = (uint32_t)((int32_t)(uint32_t)p >> 8);
        a = (uint32_t)(int32_t)(int16_t)M16(b + 0x1e);
        SPOT_UTIL(0x60, f2)(b + 0x41, V(0x2080188c));
        b = V(0x2080188c);
    } else if (M8(b + 0x1c) & 0x80) {
        return 1;
    }
    *ecx = d;
    *eax = a << 16;
    *ebx = b;
    return 0;
}

/* .text+0x66db */
static int spot_follow(void)
{
    uint32_t eax, ecx, ebx;

    if (spot_target(&eax, &ecx, &ebx))
        return 1;
    M16(V(0x20801924)) = (uint16_t)ecx;
    M32(V(0x20801888)) = eax;
    COPY24(V(0x208018a4), ebx);
    COPY24(V(0x208018bc), ebx);
    return 0;
}

/* .text+0x67fc: the step (.data+0x18d4..) from the view's direction (.data
 * +0x1888's high word) through UTIL +0x2c, scaled by GLOBAL +0xe5e (0..2 to
 * shifts 3..1, else 0), the longitude part through UTIL +0x44. */
static void spot_offset(void)
{
    uint32_t gp = G, eax;
    uint8_t cl = 0;

    if (M16(gp + 0xe5e) == 0)
        cl = 3;
    if (M16(gp + 0xe5e) == 1)
        cl = 2;
    if (M16(gp + 0xe5e) == 2)
        cl = 1;
    SPOT_UTIL(0x2c, f3)((uint16_t)((int32_t)M32(V(0x20801888)) >> 16), V(0x20801928), V(0x2080192a));
    eax = (uint32_t)(int32_t)(int16_t)M16(V(0x2080192a)) << cl << 8;
    M16(V(0x208018d6)) = (uint16_t)eax;
    M32(V(0x208018d8)) = (uint32_t)((int32_t)eax >> 16);
    eax = (uint32_t)(int32_t)(int16_t)M16(V(0x20801928)) << cl << 8;
    M16(V(0x208018de)) = (uint16_t)eax;
    M32(V(0x208018e0)) = (uint32_t)((int32_t)eax >> 16);
    SPOT_UTIL(0x44, f3)(V(0x208018dc), gp + 0x560, V(0x208018dc));
    eax = (0u - ((uint32_t)M16(V(0x20801924)) << 4)) << cl;
    M16(V(0x208018e6)) = (uint16_t)eax;
    M32(V(0x208018e8)) = (uint32_t)((int32_t)eax >> 16);
}

/* .text+0x68ee: arm the slewed transition: the eye's start (.data+0x1904)
 * and the step back toward the aircraft (.data+0x18ec), each component
 * shifted down 4 (a sixteenth) by UTIL +0x84. */
static void spot_arm(void)
{
    COPY24(V(0x20801904), V(0x208018bc));
    V48_SUB(V(0x208018ec), V(0x208018bc), G + 0x560);
    SHR48(V(0x208018ec), V(0x208018ec), 4);
    SHR48(V(0x208018f4), V(0x208018f4), 4);
    SHR48(V(0x208018fc), V(0x208018fc), 4);
    M32(V(0x2080191c)) = 1;
}

/* .text+0x69eb: the transition: from the start, fifteen steps when G3D's
 * distance is past 0x40000, else the transition ends. */
static void spot_slew(void)
{
    uint32_t saved, n;

    COPY24(V(0x20801868), V(0x20801904));
    spot_eye();
    if (M32(G + 0xd4c) <= 0x40000) {
        M32(V(0x2080191c)) = 0;
        return;
    }
    saved = M32(V(0x20801888));
    M32(V(0x20801888)) = saved + 0x40000000u;
    for (n = 15; n != 0; n--) {
        V48_SUB(V(0x20801868), V(0x20801868), V(0x208018ec));
        spot_eye();
    }
    M32(V(0x20801888)) = saved;
}

/* .text+0x6a8a: walk the eye along the step while G3D's distance (measuring
 * .data+0x192e) falls: forward first, and only if the first step forward did
 * not help, backward. */
static void spot_walk(void)
{
    uint32_t prev, moved = 0;

    COPY24(V(0x20801868), V(0x208018bc));
    spot_measure(V(0x2080192e));
    prev = M32(G + 0xd4c);
    for (;;) {
        V48_ADD(V(0x20801868), V(0x20801868), V(0x208018d4));
        spot_measure(V(0x2080192e));
        if (prev < M32(G + 0xd4c))
            break;
        if (prev == M32(G + 0xd4c))
            goto done;
        COPY24(V(0x208018bc), V(0x20801868));
        prev = M32(G + 0xd4c);
        moved = 1;
    }
    if (moved == 1)
        goto done;
    COPY24(V(0x20801868), V(0x208018bc));
    prev = M32(G + 0xd4c);
    for (;;) {
        V48_SUB(V(0x20801868), V(0x20801868), V(0x208018d4));
        spot_measure(V(0x2080192e));
        if (prev <= M32(G + 0xd4c))
            goto done;
        COPY24(V(0x208018bc), V(0x20801868));
        prev = M32(G + 0xd4c);
    }
done:
    M32(V(0x20801920)) = 0;
}

/* .text+0x669a: place the eye afresh. */
static int spot_place(void)
{
    if (spot_follow())
        return 1;
    spot_offset();
    spot_walk();
    if (M16(G + 0xe62) & 2) {
        if (M16(G + 0xe64) == 2)
            M32(V(0x208018d0)) = (uint32_t)(int32_t)(int16_t)M16(G + 0xe5c);
        spot_arm();
    }
    M16(G + 0xe62) = (uint16_t)(M16(G + 0xe62) & 0xfffe);
    return 0;
}

/* .text+0x6c88: has the followed aircraft moved (its position, count or
 * height term changed)?  If so, or there is none, flag GLOBAL +0xe62 bit 0. */
static void spot_check(void)
{
    uint32_t eax, ecx, ebx;

    if (spot_target(&eax, &ecx, &ebx) == 0 &&
        V48_EQUAL(ebx, V(0x208018a4)) == 1 &&
        eax == M32(V(0x20801888)) &&
        (uint16_t)ecx == M16(V(0x20801924)))
        return;
    M16(G + 0xe62) = (uint16_t)(M16(G + 0xe62) | 1);
}

/* .text+0x6450 == Linkage +0xa8. */
static uint32_t FS98_STDCALL spot_view(void)
{
    uint32_t gp = G, view = M32(gp + 0x1198), saved, n, i;

    if (view == 0 || M32(view + 0x10) != 0 ||
        (M32(view + 0x14) != 1 && M32(view + 0x14) != 0) || M32(view + 0x18) != 0)
        return 0;
    spot_check();
    if ((M16(gp + 0xe62) & 1) && spot_place())
        return 0;
    M16(V(0x20801926)) = M16(gp + 0xe68);
    if (M32(V(0x2080191c)) != 0) {
        if (!(M16(gp + 0xe62) & 2))
            M32(V(0x2080191c)) = 0;
        else
            spot_slew();
    }
    if (M16(gp + 0xe64) == 2)
        M32(V(0x208018d0)) = (uint32_t)(int32_t)(int16_t)M16(gp + 0xe5c);
    COPY24(V(0x20801868), V(0x208018bc));
    spot_eye();
    saved = M32(gp + 0xd4c);
    V48_ADD(V(0x20801868), V(0x20801868), V(0x208018d4));
    spot_eye();
    if (saved >= M32(gp + 0xd4c))
        M32(V(0x20801920)) = 1;
    n = M16(gp + 0xe60) == 0 ? 3 : M16(gp + 0xe60) == 1 ? 7 : M16(gp + 0xe60) == 2 ? 15 : 0;
    for (i = n; i != 0; i--) {
        V48_ADD(V(0x20801868), V(0x20801868), V(0x208018d4));
        spot_eye();
    }
    COPY24(V(0x20801868), V(0x208018bc));
    V48_SUB(V(0x20801868), V(0x20801868), V(0x208018d4));
    spot_eye();
    if (saved >= M32(gp + 0xd4c))
        M32(V(0x20801920)) = 1;
    for (i = n; i != 0; i--) {
        V48_SUB(V(0x20801868), V(0x20801868), V(0x208018d4));
        spot_eye();
    }
    if (M32(V(0x20801920)) != 0)
        spot_walk();
    return 0;
}
