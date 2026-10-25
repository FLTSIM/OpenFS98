/* MAIN.DLL slice 3: +0x70, the range and bearing between two positions.
 *
 * Retail .text+0x19507 (the service) with its two helpers .text+0x1962c
 * (the delta vector and its magnitude) and .text+0x195ab (the angle of the
 * delta).  ADVDRV and LANDME call +0x70.  It is plain assembly with a normal
 * stack frame -- no inbound registers -- so it transliterates as ordinary
 * stdcall C, unlike the register-threaded reader of slice 2.
 *
 * The geometry it leans on is UTIL's: +0x48 scales a longitude by the
 * latitude, +0x18 is the magnitude of a 3-vector, +0x3c the angle of a
 * 2-vector.  Those are verified in UTIL; maindiff drives this slice against
 * logging models of them, so what the suite compares is MAIN's own glue --
 * the 48-bit subtractions, the squares and the normalising shift.
 */

#define UTIL M32(V(0x208000a4))

/* .text+0x1962c: out <- the vector from `from` to `to`, and the return is
 * its magnitude.  `from` and `to` are position records; `out` is three
 * dwords {x, y, z}.  The low word of the longitude delta is dropped (retail
 * keeps only the high dword); the latitude delta is handed whole to UTIL
 * +0x48, which turns it into the planar x. */
static uint32_t nav_delta(uint32_t from, uint32_t to, uint32_t out)
{
    uint32_t gp = G;
    int32_t x, y, z;
    int64_t sum;
    uint8_t latbuf[8];     /* the latitude delta, at +2, as UTIL +0x48 reads it */
    uint32_t scratch[2];   /* UTIL +0x48 writes the planar x into the second dword */

    /* z: the high dword of the 48-bit longitude delta (sub ax; sbb edx). */
    {
        uint16_t tlo = (uint16_t)M16(to + 2), flo = (uint16_t)M16(from + 2);
        uint32_t borrow = tlo < flo ? 1u : 0u;
        z = (int32_t)(M32(to + 4) - M32(from + 4) - borrow);
    }
    M32(out + 8) = (uint32_t)z;

    /* y: from the record's field +0x14, or -- when it holds the adventure
     * sentinel 0xffffd8f1 -- GLOBAL +0x20 arithmetic-shifted down a byte;
     * then minus the other record's +0x14, negated. */
    {
        int32_t e = (int32_t)M32(from + 0x14);
        if ((uint32_t)e == 0xffffd8f1u)
            e = (int32_t)M32(gp + 0x20) >> 8;
        e -= (int32_t)M32(to + 0x14);
        y = -e;
    }
    M32(out + 4) = (uint32_t)y;

    /* x: the 48-bit latitude delta, kept whole (word at +2, dword at +4),
     * handed to UTIL +0x48 with the `to` record; it writes the planar x. */
    {
        uint16_t tlo = (uint16_t)M16(to + 0xa), flo = (uint16_t)M16(from + 0xa);
        uint32_t borrow = tlo < flo ? 1u : 0u;
        *(uint16_t *)(latbuf + 2) = (uint16_t)(tlo - flo);
        *(uint32_t *)(latbuf + 4) = M32(to + 0xc) - M32(from + 0xc) - borrow;
    }
    CALL(UTIL, 0x48, f3)(ADDR(latbuf), to, ADDR(scratch));
    x = (int32_t)scratch[1];
    M32(out) = (uint32_t)x;

    /* the magnitude: UTIL +0x18 of x^2 + y^2 + z^2 (signed 64-bit). */
    sum  = (int64_t)x * x;
    sum += (int64_t)y * y;
    sum += (int64_t)z * z;
    return CALL(UTIL, 0x18, f1)(ADDR(&sum));
}

/* .text+0x195ab: the angle of the vector (x, z).  Both are scaled up
 * together while each stays within +-0x4000 in its high word (atan2 wants
 * its inputs large for precision), then UTIL +0x3c takes the angle.  A zero
 * vector skips the scaling. */
static uint32_t nav_bearing(int32_t x, int32_t z)
{
    if ((x | z) != 0) {
        for (;;) {
            int16_t xh = (int16_t)((uint32_t)x >> 16);
            int16_t zh = (int16_t)((uint32_t)z >> 16);
            if (zh <= (int16_t)0xc000 || zh >= (int16_t)0x4000)
                break;
            if (xh <= (int16_t)0xc000 || xh >= (int16_t)0x4000)
                break;
            x = (int32_t)((uint32_t)x << 1);
            z = (int32_t)((uint32_t)z << 1);
        }
    }
    return CALL(UTIL, 0x3c, f2)((uint32_t)x, (uint32_t)z);
}

/* .text+0x19507 == Linkage +0x70: (from, to, &bearing, &range).  The range
 * also lands in .data+0x72e0; the full angle is returned, its low word
 * stored through &bearing. */
static uint32_t FS98_STDCALL nav_range_bearing(uint32_t from, uint32_t to,
                                               uint32_t bearing_out, uint32_t range_out)
{
    uint32_t out[3];
    uint32_t range = nav_delta(from, to, ADDR(out));
    uint32_t bearing;

    M32(range_out) = range;
    M32(V(0x208072e0)) = range;
    bearing = nav_bearing((int32_t)out[0], (int32_t)out[2]);
    M16(bearing_out) = (uint16_t)bearing;
    return bearing;
}

/* MAIN.DLL slice 4: two nav read-outs.
 *
 * +0x78 (.text+0x19c94) and +0x80 (.text+0x19f10), both (which, &out), hand
 * back values the nav machine left in .data.  Small demultiplexers; +0x80
 * refreshes two of its words through MULTIP first.  ADVDRV and PANELS call
 * +0x80; +0x78 is reached from the host. */

#define MULTIP M32(V(0x2080007c))
#define FE       M32(V(0x2080005c))
#define H_MEMSET HOST_(0x218, f3)   /* (dst, val, n) */

/* .text+0x19c94 == Linkage +0x78: which 1 and 2 copy the words at
 * .data+0x7482 and +0x7484 (set by +0x74's slice); anything else answers
 * -1 and writes nothing. */
static uint32_t FS98_STDCALL nav_readout(uint32_t which, uint32_t out)
{
    if (which == 1) {
        M16(out) = M16(V(0x20807482));
        return 0;
    }
    if (which == 2) {
        M16(out) = M16(V(0x20807484));
        return 0;
    }
    return 0xffffffffu;
}

/* .text+0x19f10 == Linkage +0x80: which 1 and 2 refresh .data+0x72e8 and
 * +0x72ec through MULTIP +0x38 and +0x34 (and answer -1 if it fails), which
 * 3 and 4 just read +0x72f0 and +0x72f4; which 0 or above 4 writes nothing
 * and answers 0. */
static uint32_t FS98_STDCALL nav_select(uint32_t which, uint32_t out)
{
    switch (which) {
    case 1:
        if (CALL(MULTIP, 0x38, f1)(V(0x208072e8)) != 0)
            return 0xffffffffu;
        M32(out) = M32(V(0x208072e8));
        return 0;
    case 2:
        if (CALL(MULTIP, 0x34, f1)(V(0x208072ec)) != 0)
            return 0xffffffffu;
        M32(out) = M32(V(0x208072ec));
        return 0;
    case 3:
        M32(out) = M32(V(0x208072f0));
        return 0;
    case 4:
        M32(out) = M32(V(0x208072f4));
        return 0;
    default:
        return 0;
    }
}

/* .text+0x19cd5 == Linkage +0x7c: (which, arg2, &out).  FE +0x2c looks the
 * station up into a local buffer (its first argument is pushed as a word over
 * two stale bytes, so only the low word reaches FE); UTIL +0x88 formats it,
 * through a .text descriptor {3, 3, 6, &.data+0x7248}, into .data+0x7248; a
 * HOST memmove hands the 0x58-byte result back.  A failed lookup zeroes the
 * output and answers -1.  The descriptor's fourth word is a relocation into
 * .data, so ours builds the descriptor with its own .data address rather than
 * reading retail's unrelocated pointer out of .text. */
static uint32_t FS98_STDCALL nav_describe(uint32_t which, uint32_t arg2, uint32_t out)
{
    uint8_t buf[0x190];
    uint32_t desc[4];

    if (CALL(FE, 0x2c, f3)(which, arg2, ADDR(buf)) == 0) {
        H_MEMSET(out, 0, 0x58);
        return 0xffffffffu;
    }
    desc[0] = 3;
    desc[1] = 3;
    desc[2] = 6;
    desc[3] = V(0x20807248);
    CALL(UTIL, 0x88, f3)(ADDR(buf), ADDR(desc), 0xa);
    return H_MEMMOVE(out, V(0x20807248), 0x58);
}
