/* Device +0x28, 0x2078e2e0: end the scene, draw through its DC, resume it. */
uint32_t STDCALL __attribute__((used)) hg2d_device_text(uint32_t surface, uint32_t x, uint32_t y,
    uint32_t unused, const char *const *text, uint32_t flags)
{
    uint32_t length, result, *device, *table, *target, *target_table;
    HDC dc;
    (void)surface; (void)unused;
    if (!text || !*text) return ADDRESS(text);
    length = (uint32_t)lstrlenA(*text);
    if (!length) return 0;
    hg2d_device_flush();
    device = POINTER(WORD(0x289d0)); table = POINTER(device[0]);
    ((call_one)(uintptr_t)table[11])(ADDRESS(device));
    target = POINTER(WORD(0x28940)); target_table = POINTER(target[0]);
    result = ((call_two)(uintptr_t)target_table[17])(ADDRESS(target), ADDRESS(&dc));
    if (!result) {
        uint32_t index, colour;
        if (flags & 1) {
            SIZE extent;
            GetTextExtentPointA(dc, *text, (int)length, &extent);
            /* Retail subtracts the sign before SAR, even for negative sizes. */
            x -= (uint32_t)((int32_t)((uint32_t)extent.cx - ((int32_t)extent.cx < 0 ? 0xffffffffu : 0)) >> 1);
            y -= (uint32_t)((int32_t)((uint32_t)extent.cy - ((int32_t)extent.cy < 0 ? 0xffffffffu : 0)) >> 1);
        }
        draw_colour(); index = BYTE(0x90c);
        colour = BYTE(0x39c2 + index * 3) | (uint32_t)BYTE(0x39c3 + index * 3) << 8 |
                 (uint32_t)BYTE(0x39c4 + index * 3) << 16;
        SetTextColor(dc, colour); SetBkMode(dc, TRANSPARENT);
        TextOutA(dc, (int)x, (int)y, *text, (int)length);
        ((call_two)(uintptr_t)target_table[26])(ADDRESS(target), ADDRESS(dc));
    }
    device = POINTER(WORD(0x289d0)); table = POINTER(device[0]);
    return ((call_one)(uintptr_t)table[10])(ADDRESS(device));
}
