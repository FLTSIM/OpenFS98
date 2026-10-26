/* MAIN.DLL slice 24: +0x18, the frame (MSVC C, .text+0xc000, with
 * .text+0xc230, +0xc330, +0xc430 and +0xc450).
 *
 * The host calls it once a frame.  In order: the half-second events when
 * slice 23's tick has raised GLOBAL +0x254 (+0xc230: UTIL +0x70, CHAIN list 3
 * with the event count +0x258, and by its low bits list 4 and WEATHERM +0x1c,
 * list 6, or a lamp copied; then the slew catch-up, +0xc450; then module 26's
 * step while GLOBAL +0x84c and +0x850 are set); CHAIN list 1 with the frame
 * count +0x2ac (slice 22's clouds among its callbacks); module 6's step; the
 * mode (+0xc330: pause and slew raise list 0x14 messages 0x38/0x39 on the
 * change, list 0xd while either holds) deciding between module 7's step, the
 * crash test (+0x628), module 9's step while GLOBAL +0x5dc is set, or the
 * normal +0x270; the latitude clamp (slice 11); module 10's step (+0xc430);
 * HG2D's; a once-a-second flag (+0x1204, from the tick's +0x260); G3D +0x60
 * when +0x1206 asks; the frame count; the frame time from the host's clock
 * (+0x34) as a fraction of a second in GLOBAL +0x2aa; CHAIN list 2 for every
 * tick and list 5 for every third tick slice 23 counted, each count drained
 * with InterlockedDecrement; and SOUND's step.  Its eax is SOUND's. */

#define M6   M32(V(0x2080004c))
#define M7   M32(V(0x20800044))
#define M9   M32(V(0x2080009c))
#define M10  M32(V(0x208000b4))
#define M26  M32(V(0x20800074))
#define WEATHERM_ M32(V(0x208000ac))
#define SOUND_    M32(V(0x20800094))

/* .text+0xc450: after a slew, once GLOBAL +0x365's countdown reaches 1 on a
 * new frame, put back the position (+0x11fa/+0x11fc) or the heading
 * (+0x1200 with +0x2a0 above it) that +0x11f0 / +0x11f4 say is pending. */
static void step_slew(void)
{
    uint32_t gp = G;

    if (M8(gp + 0x365) == 0 || M32(gp + 0x2ac) == M32(V(0x208085b8)))
        return;
    M32(V(0x208085b8)) = M32(gp + 0x2ac);
    if (M32(gp + 0x268) != 0 || M8(gp + 0x838) != 0)
        return;
    M8(gp + 0x365) = (uint8_t)(M8(gp + 0x365) >> 1);
    if (M8(G + 0x365) != 1)
        return;
    if (M32(G + 0x11f0) == 1) {
        M32(G + 0x11f0) = 0;
        M16(G + 0x572) = M16(G + 0x11fa);
        M32(G + 0x574) = M32(G + 0x11fc);
    }
    if (M32(G + 0x11f4) == 1) {
        M32(G + 0x11f4) = 0;
        M32(G + 0x580) = M32(G + 0x1200) + ((uint32_t)M16(G + 0x2a0) << 16);
    }
}

/* .text+0xc230: the half-second events. */
static void step_events(void)
{
    uint32_t gp = G, c;

    if (M32(gp + 0x254) == 0)
        return;
    InterlockedExchange((LONG *)(uintptr_t)(gp + 0x254), 0);
    CALL(UTIL, 0x70, f0)();
    CHAIN_SEND(3, M32(G + 0x258), 0);
    gp = G;
    c = M32(gp + 0x258);
    if (c & 1) {
        if (M32(gp + 0x794) != 0 && M8(gp + 0xb6a) == 0)
            M8(gp + 0x282) = M8(gp + 0x281);
    } else if (!(c & 2)) {
        CHAIN_SEND(4, 0, 0);
        CALL(WEATHERM_, 0x1c, f0)();
        if (M8(G + 0x258) & 4)
            CHAIN_SEND(6, 0, 0);
    }
    step_slew();
    gp = G;
    if (M32(gp + 0x84c) != 0 && M32(gp + 0x850) != 0) {
        CALL(M26, 0x18, f0)();
        M32(G + 0x258) = M32(G + 0x258) + 1;
        return;
    }
    M32(gp + 0x84c) = 0;
    M32(G + 0x258) = M32(G + 0x258) + 1;
}

/* .text+0xc330: the mode (GLOBAL +0x264, 0 running, 1 paused, 2 slewing),
 * telling list 0x14 of a change (0x38 into a mode, 0x39 out of one), and
 * whether it holds (+0x268), with list 0xd while it does; +0xb24 says
 * whether the crash test is armed. */
static uint32_t step_mode(void)
{
    uint32_t gp = G, c = M32(gp + 0x264);

    if (c == 0 && M32(gp + 0x268) != 0) {
        CHAIN_SEND(0x14, 0x39, c);
        gp = G;
    } else if ((c == 1 || c == 2) && M32(gp + 0x268) == 0) {
        CHAIN_SEND(0x14, 0x38, 0);
        gp = G;
    }
    M32(gp + 0x268) = M32(gp + 0x264) != 0 ? 1 : 0;
    gp = G;
    if (M32(gp + 0x268) != 0) {
        CHAIN_SEND(0xd, 0, 0);
        if (M32(G + 0x264) != 2)
            M32(G + 0xb24) = 0;
        return M32(G + 0x268);
    }
    if (M32(gp + 0xb20) != 0 && M16(gp + 0x5dc) == 0) {
        M32(gp + 0xb24) = 1;
        return M32(G + 0x268);
    }
    M32(gp + 0xb24) = 0;
    return M32(G + 0x268);
}

/* .text+0xc430 */
static void step_m10(void)
{
    CALL(M10, 0x1c, f0)();
    M8(G + 0x282) = 0;
}

/* .text+0xc000 == Linkage +0x18. */
static uint32_t FS98_STDCALL main_step(void)
{
    uint32_t gp, t;

    step_events();
    CHAIN_SEND(1, M32(G + 0x2ac), 0);
    CALL(M6, 0x18, f0)();
    if (M32(G + 0x758) != 0) {
        if (step_mode() == 0)
            CALL(M7, 0x18, f0)();
        M32(G + 0x270) = 1;
    } else if (step_mode() == 0) {
        gp = G;
        if (M32(gp + 0x628) != 0) {
            M32(gp + 0x270) = 1;
        } else if (M16(gp + 0x5dc) != 0) {
            CALL(M9, 0x18, f0)();
            M32(G + 0x270) = 1;
        } else {
            M16(gp + 0x5de) = 1;
            M32(G + 0x270) = M32(G + 0x26c);
        }
    }
    lat_clamp();
    step_m10();
    CALL(HG2D, 0x18, f0)();
    M16(G + 0x1204) = 0;
    gp = G;
    if (M32(gp + 0x260) != M32(V(0x208085b4))) {
        M32(V(0x208085b4)) = M32(gp + 0x260);
        M16(gp + 0x1204) = (uint16_t)(M16(gp + 0x1204) - 1);
    }
    M16(G + 0x1208) = M16(G + 0x1206);
    if (M16(G + 0x1206) != 0) {
        M16(G + 0x1206) = 0;
        CALL(G3D, 0x60, f0)();
    }
    M32(G + 0x2ac) = M32(G + 0x2ac) + 1;
    M32(G + 0x2a4) = HOST_(0x34, f0)();
    t = M32(G + 0x2a4);
    if (t < 1000) {
        M16(G + 0x2aa) = (uint16_t)((((uint64_t)(t * 0x7fffu) * 0x10624dd3u) >> 32) >> 6);
        M32(G + 0x274) = (uint32_t)(int32_t)(int16_t)M16(G + 0x2aa);
    } else {
        M16(G + 0x2aa) = 0x7fff;
    }
    while (M32(G + 0x24c) != 0) {
        InterlockedDecrement((LONG *)(uintptr_t)(G + 0x24c));
        CHAIN_SEND(2, 0, 0);
    }
    while (M32(G + 0x250) != 0) {
        InterlockedDecrement((LONG *)(uintptr_t)(G + 0x250));
        CHAIN_SEND(5, 0, 0);
    }
    if (M32(V(0x208085b0)) == 0)
        M32(V(0x208085b0)) = 1;
    return CALL(SOUND_, 0x20, f0)();
}
