/* MAIN.DLL slice 15: +0x9c, the altitude band and its 3D submission.
 *
 * Retail .text+0x6de9, assembly, with its tail helper at .text+0x7200.
 *
 * The service classifies the aircraft's reference altitude (GLOBAL +0x5c4,
 * a signed word) against three bands built from two centre/width pairs
 * (GLOBAL +0xe6a/+0xe6c and +0xe72/+0xe74): it leaves the band index 0..4 in
 * GLOBAL +0xe86 and a derived value in the scratch word .data+0x1a76.  The
 * band edges are laid out in .data+0x1a94 and +0x1aac as centre-half-width;
 * which edges apply, and whether a middle band interpolates (an unsigned
 * 16x16 multiply whose high word is taken), is chosen by the +0xe78 flag.
 * A gate word GLOBAL +0xe70 of zero drops straight to band 4.
 *
 * The helper then stages a run of 2D/3D draw commands: it patches a handful
 * of static command blocks in .data from GLOBAL fields and hands each, in
 * turn, to G3D +0x1c once and +0x24 one to three times.  G3D (module id 14)
 * is verified in its own harness; the draws are fire-and-forget (their
 * results are never read), so the harness logs the block each is handed --
 * identical on both sides -- and only MAIN's own classification arithmetic,
 * its .data patches and the order of the draws are under test.  The staging
 * word .data+0x1a72 holds the block pointer handed to G3D; it is a run-time
 * image pointer, normalised in put_state like a relocation.  The hand-rolled
 * stack-alignment dance around each call, and the save/restore of GLOBAL
 * +0x3a8 across the first call (which leaves it unchanged), have no
 * observable effect and are left out.
 */

static void band_submit(void);

static uint32_t FS98_STDCALL nav_band(void)
{
    uint32_t gp = G;
    uint16_t bx, bp, si;
    int16_t xs = 0;

    M16(gp + 0x1174) = 0;
    M16(gp + 0xc90) = 0;
    M16(V(0x20801a76)) = 0xffff;

    M16(V(0x20801a94)) = (uint16_t)(M16(gp + 0xe6a) - (M16(gp + 0xe6c) >> 1));
    M16(V(0x20801aac)) = (uint16_t)(M16(gp + 0xe72) - (M16(gp + 0xe74) >> 1));
    M16(V(0x20801a78)) = 1;

    if (M16(gp + 0xcb4) == 2 &&
        (uint16_t)(M16(gp + 0x5ce) + 0x1000) >= 0x2000)
        M16(V(0x20801a78)) = 2;

    bp = 4;
    si = 0xffff;
    if (M16(gp + 0xe70) != 0) {
        M16(V(0x20802037)) = M16(gp + 0x5c4);
        xs = (int16_t)M16(V(0x20802037));
        bx = M16(V(0x20801a94));
        if (M16(gp + 0xe78) == 0) {
            if (xs < (int16_t)bx) {
                bp = 0;
                si = (uint16_t)~M16(gp + 0xe6e);
            } else {
                bx = (uint16_t)(bx + M16(gp + 0xe6c));
                if (xs > (int16_t)bx) {
                    bp = 4;
                    si = 0xffff;
                } else {
                    bp = 1;
                    si = (uint16_t)~M16(gp + 0xe6e);
                }
            }
        } else {
            int done = 0;
            bp = 0;
            if (xs < (int16_t)bx) {
                /* interpolate, band 0 */
            } else {
                bp = 1;
                bx = (uint16_t)(bx + M16(gp + 0xe6c));
                if (xs <= (int16_t)bx) {
                    /* interpolate, band 1 */
                } else {
                    bp = 2;
                    bx = M16(V(0x20801aac));
                    if (xs < (int16_t)bx) {
                        si = (uint16_t)~M16(gp + 0xe76);
                        done = 1;
                    } else {
                        bp = 3;
                        bx = (uint16_t)(bx + M16(gp + 0xe74));
                        if (xs <= (int16_t)bx) {
                            si = (uint16_t)~M16(gp + 0xe76);
                            done = 1;
                        } else {
                            bp = 4;
                            si = 0xffff;
                            done = 1;
                        }
                    }
                }
            }
            if (!done) {
                uint16_t a = (uint16_t)~M16(gp + 0xe6e);
                uint16_t d = (uint16_t)~M16(gp + 0xe76);
                si = (uint16_t)(((uint32_t)a * d) >> 16);
            }
        }
    }

    M16(gp + 0xe86) = bp;
    M16(V(0x20801a76)) = si;
    band_submit();
    return 0;
}

static void band_submit(void)
{
    uint32_t gp = G;
    uint32_t cmd;

    M16(V(0x20802de6)) = 0x69b0;
    M16(V(0x20801a70)) = 1;
    if (M16(gp + 0xdce) != 0) {
        M16(V(0x20802de6)) = M16(gp + 0x3bc);
        M16(V(0x20801a70)) = 2;
    }

    M32(V(0x20801e6a)) = M32(gp + 0x5d0);
    M32(V(0x20801e62)) = 0;
    M32(V(0x20801e66)) = 0;
    M32(gp + 0xcdc) = 0;
    M32(gp + 0xce0) = 0;
    M32(gp + 0xce4) = 0;
    M32(gp + 0xd4c) = 0;
    M16(gp + 0xd42) = M16(gp + 0xd44);

    M32(V(0x20801a72)) = V(0x2080210e);
    CALL(G3D, 0x1c, f1)(V(0x2080210e));

    if (!(M16(gp + 0xdce) == 0 && M32(gp + 0x394) != 0)) {
        M16(V(0x20802e0a)) = M16(gp + 0xb50);
        M32(V(0x20801a72)) = V(0x20802de2);
        CALL(G3D, 0x24, f1)(V(0x20802de2));
    }

    cmd = 0x2080298c;
    M32(V(0x20801a72)) = V(cmd);
    if (M16(gp + 0xe86) >= 3) {
        /* band 3 or 4: the ground-plane block */
    } else if (M16(gp + 0xe86) != 0) {
        if (M16(gp + 0xe78) != 0) {
            cmd = 0x208027b0;
            M32(V(0x20801a72)) = V(cmd);
        }
    } else {
        cmd = 0x208023f8;
        M32(V(0x20801a72)) = V(cmd);
        if (M16(gp + 0xe78) != 0) {
            M32(V(0x20801a72)) = V(0x208028a4);
            CALL(G3D, 0x24, f1)(V(0x208028a4));
            cmd = 0x20802712;
            M32(V(0x20801a72)) = V(cmd);
        }
    }
    CALL(G3D, 0x24, f1)(V(cmd));
}
