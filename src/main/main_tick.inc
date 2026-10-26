/* MAIN.DLL slice 23: +0x24, the timer tick (MSVC C, .text+0x16a60).
 *
 * The host calls it on its timer.  GLOBAL +0x24c counts every tick and
 * +0x250 every third (InterlockedIncrement, as retail calls it); a 16-bit
 * fraction at +0x25e gains 0xe38 a tick and carries into +0x260; every sixth
 * tick the word at +0x1176 rotates left one and the four at +0x1178..+0x117e
 * advance by 0x506, 0x708, 0x90a and 0xb0c; every eighteenth sets +0x254 to 1
 * (InterlockedExchange).  The three countdowns are .data+0x6dc0, +0x6dc4 and
 * +0x6dc8.  eax at exit is leftover; the host ignores it. */
static uint32_t FS98_STDCALL main_tick(void)
{
    uint32_t gp = G;
    uint16_t w;

    InterlockedIncrement((LONG *)(uintptr_t)(gp + 0x24c));
    M16(G + 0x25e) = (uint16_t)(M16(G + 0x25e) + 0xe38);
    if (M16(G + 0x25e) < 0xe38)
        M32(G + 0x260) = M32(G + 0x260) + 1;
    if (--M32(V(0x20806dc0)) == 0) {
        M32(V(0x20806dc0)) = 3;
        InterlockedIncrement((LONG *)(uintptr_t)(G + 0x250));
    }
    if (--M32(V(0x20806dc4)) == 0) {
        M32(V(0x20806dc4)) = 6;
        w = M16(G + 0x1176);
        M16(G + 0x1176) = (uint16_t)(w << 1 | w >> 15);
        M16(G + 0x1178) = (uint16_t)(M16(G + 0x1178) + 0x506);
        M16(G + 0x117a) = (uint16_t)(M16(G + 0x117a) + 0x708);
        M16(G + 0x117c) = (uint16_t)(M16(G + 0x117c) + 0x90a);
        M16(G + 0x117e) = (uint16_t)(M16(G + 0x117e) + 0xb0c);
    }
    if (--M32(V(0x20806dc8)) == 0) {
        M32(V(0x20806dc8)) = 0x12;
        InterlockedExchange((LONG *)(uintptr_t)(G + 0x254), 1);
    }
    return 0;
}
