/* MAIN.DLL slice 14: +0xc0, the eye's reticle.
 *
 * Retail .text+0x16a0, assembly.  The view record's +0x1c points to a box
 * {extent at +0, lo at +4, hi at +0xc, done-flag at +0x10, two guards at
 * +0x14 and +0x18}.  Unless the box is already done or either guard is set,
 * it stamps GLOBAL +0x2ec with 0xf00f and, by the reticle style in GLOBAL
 * +0x600, hands HG2D the segments of a frame (style 1: four +0x1c segments)
 * or a cross (styles 2 and 3: four +0x34 segments), each scaled from the
 * extent (style 1 by >>6, style 2 by >>5, style 3 by >>4).  Styles 0 and
 * above 3 draw nothing.
 *
 * HG2D (module id 13) is verified elsewhere; the harness logs the segments it
 * is handed, identical on both sides, so only MAIN's own coordinate
 * arithmetic and the box's gating are under test.  The calls' results are
 * discarded and `eax` at exit is leftover, so no return value is compared.
 * The hand-rolled push sequence's stack alignment has no observable effect
 * and is left out.
 */

#define HG2D M32(V(0x20800064))

static uint32_t FS98_STDCALL nav_eye(uint32_t view)
{
    uint32_t gp = G;
    uint32_t box = M32(view + 0x1c);
    uint32_t ctx = view + 0x80;
    int32_t extent = (int32_t)M32(box);
    int32_t lo = (int32_t)M32(box + 4);
    int32_t hi = (int32_t)M32(box + 0xc);
    int32_t d;

    if (M32(box + 0x10) == 1 || M32(box + 0x14) != 0 || M32(box + 0x18) != 0)
        return 0;
    M32(gp + 0x2ec) = 0xf00f;

    switch (M16(gp + 0x600)) {
    case 1:
        d = (int32_t)((uint32_t)extent >> 6);
        CALL(HG2D, 0x1c, f3)(ctx, (uint32_t)(lo - d), (uint32_t)hi);
        CALL(HG2D, 0x1c, f3)(ctx, (uint32_t)(lo + d), (uint32_t)hi);
        CALL(HG2D, 0x1c, f3)(ctx, (uint32_t)lo, (uint32_t)(hi - d));
        CALL(HG2D, 0x1c, f3)(ctx, (uint32_t)lo, (uint32_t)(hi + d));
        break;
    case 2:
    case 3:
        d = M16(gp + 0x600) == 2 ? (int32_t)((uint32_t)extent >> 5)
                                 : (int32_t)((uint32_t)extent >> 4);
        CALL(HG2D, 0x34, f5)(ctx, (uint32_t)(lo - 2 * d), (uint32_t)hi, (uint32_t)(lo - d), (uint32_t)hi);
        CALL(HG2D, 0x34, f5)(ctx, (uint32_t)(lo - d), (uint32_t)hi, (uint32_t)lo, (uint32_t)(hi + d));
        CALL(HG2D, 0x34, f5)(ctx, (uint32_t)lo, (uint32_t)(hi + d), (uint32_t)(lo + d), (uint32_t)hi);
        CALL(HG2D, 0x34, f5)(ctx, (uint32_t)(lo + d), (uint32_t)hi, (uint32_t)(lo + 2 * d), (uint32_t)hi);
        break;
    }
    return 0;
}
