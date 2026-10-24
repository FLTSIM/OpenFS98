/* 0x2078bd30: configured texture format, optional indexed mode, then the
 * fixed alpha-capable fallback order. Every attempt enumerates afresh. */
uint32_t STDCALL __attribute__((used)) hg2d_device_select_format(uint32_t flags)
{
    uint32_t configured = 0xffffffffu;
    uint32_t *out = (uint32_t *)(hg2d_image + 0x28ae0);
    (*(call_three *)((uintptr_t)WORD(4) + 0xa8))(ADDRESS(hg2d_image + 0x5e80),
        ADDRESS(hg2d_image + 0x5e8c), ADDRESS(&configured));
    if (configured != 0xffffffffu && hg2d_device_formats(WORD(0x289d0), configured, 1, out)) return 1;
    if ((flags & 0x400) && hg2d_device_formats(WORD(0x289d0), 8, 1, out)) return 1;
    if (hg2d_device_formats(WORD(0x289d0), 15, 1, out)) return 1;
    if (hg2d_device_formats(WORD(0x289d0), 12, 4, out)) return 1;
    return hg2d_device_formats(WORD(0x289d0), 16, 0, out);
}
