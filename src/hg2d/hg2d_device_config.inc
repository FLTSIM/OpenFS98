/* Device options used by selection, at 0x2078bdf0..0x2078c01e. */
void STDCALL __attribute__((used)) hg2d_device_blend(void)
{
    WORD(0x289e0) = 0xffffffffu;
    (*(call_three *)((uintptr_t)WORD(4) + 0xa8))(ADDRESS(hg2d_image + 0x5e80),
        ADDRESS(hg2d_image + 0x5e9c), ADDRESS(hg2d_image + 0x289e0));
    if (WORD(0x289e0)) {
        uint32_t capabilities = WORD(0x28a80);
        WORD(0x289e0) = capabilities & 0x200 ? 1 : ((capabilities & 255) >> 6) & 2;
    }
}

void STDCALL __attribute__((used)) hg2d_device_fog(uint32_t flags)
{
    if (!(flags & 0x1000)) {
        WORD(0x5f24) = 0;
        hg2d_device_state(28, 0);
    } else if (BYTE(0x28a6c) & 0x80) {
        hg2d_device_state(28, 0);
        hg2d_device_state(35, 0);
        WORD(0x5f24) = 1;
    } else WORD(0x5f24) = 0;
}

void STDCALL __attribute__((used)) hg2d_device_filter(uint32_t flags)
{
    uint32_t filter = flags & 0x10 ? 2 : 1;
    WORD(0x289d8) = WORD(0x289dc) = filter;
    WORD(0x289e8) = WORD(0x5f28) ? filter + 2 : filter;
    WORD(0x289e4) = filter;
}

void STDCALL __attribute__((used)) hg2d_device_mipmap(uint32_t flags)
{
    WORD(0x5f28) = (BYTE(0x28a88) & 0x18) && (flags & 0x80) ? 1 : 0;
    WORD(0x5f2c) = WORD(0x28aec) != 8 && (flags & 0x800) ? 1 : 0;
    WORD(0x5f30) = 0xffffffffu;
    (*(call_three *)((uintptr_t)WORD(4) + 0xa8))(ADDRESS(hg2d_image + 0x5e80),
        ADDRESS(hg2d_image + 0x5ea8), ADDRESS(hg2d_image + 0x5f30));
    if (WORD(0x5f30) == 0x100) WORD(0x5f2c) = 0;
    if (WORD(0x5f30) == 0x80) WORD(0x5f2c) = 1;
    if (WORD(0x5f30) == 0x40) WORD(0x5f2c) = 2;
}
