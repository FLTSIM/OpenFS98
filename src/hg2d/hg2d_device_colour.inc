/* 0x2078cd50: ARGB colour and alpha for device vertices. */
void STDCALL __attribute__((used)) hg2d_device_colour(void)
{
    uint32_t colour;
    if (GLOBAL_HALF(0xf10) && !GLOBAL_HALF(0xdce) && GLOBAL_WORD(0x398) && GLOBAL_HALF(0x1172) == 1) colour = 0xeeffffff;
    else if (GLOBAL_HALF(0xc90) && GLOBAL_HALF(0xca0) == 0x98 && !GLOBAL_HALF(0xe82)) colour = 0xccffffff;
    else if (GLOBAL_HALF(0xc90) && ((uint32_t *)POINTER(GLOBAL_WORD(0xc8c)))[2]) colour = 0xffffffff;
    else if (GLOBAL_HALF(0xdca)) colour = GLOBAL_HALF(0xb34) < 0x8000 ? 0x80000000 : 0x80ffffff;
    else if (GLOBAL_WORD(0x1298)) {
        uint32_t index = GLOBAL_WORD(0x1298);
        colour = WORD(0x28960 + (index < 28 ? index : 3) * 4);
    } else if ((GLOBAL_WORD(0x3c0) & 0xff00) == 0x6800 && GLOBAL_HALF(0xb34) >= 0x8000)
        colour = GLOBAL_HALF(0xdce) ? 0x80ff3f00 : 0x80ffffff;
    else {
        uint32_t index = BYTE(0x90c);
        colour = (uint32_t)BYTE(0x39c2 + index * 3) << 16 |
                 (uint32_t)BYTE(0x39c3 + index * 3) << 8 | BYTE(0x39c4 + index * 3);
        if ((GLOBAL_WORD(0x3c0) & 0xff00) == 0x6800) colour |= 0x80000000;
    }
    WORD(0x5f38) = colour;
}
