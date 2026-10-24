/* Palette resources, +0x58/+0x68, and the haze lookup builders. */
static void resource_remap_rows(uint32_t source)
{
    uint32_t row, index;
    for (row = 0; row < 16; row++, source += 256) {
        for (index = 0; index < 256; index++) {
            BYTE(0xc4e + index) = BYTE(source + index);
            BYTE(source + index) = 0;
        }
        for (index = 0; index < 256; index++)
            BYTE(source + HALF(0x34c0 + index * 2)) = BYTE(0x34c0 + BYTE(0xc4e + index) * 2);
    }
}

static uint32_t resource_neutral_row(void)
{
    uint32_t index, result = 0, source = WORD(0x4d68), destination = WORD(0x4d6c);
    WORD(0x4d58) = HALF(0x34c0 + WORD(0x4d50) * 2);
    WORD(0x4d5c) = HALF(0x34c0 + WORD(0x4d54) * 2);
    for (index = 0; index < 256; index++) {
        uint32_t texel = *(uint8_t *)POINTER(source + index), offset = texel * 3;
        int32_t red = BYTE(0x39c2 + offset), green = BYTE(0x39c3 + offset), blue = BYTE(0x39c4 + offset);
        int32_t tolerance = (int32_t)WORD(0x4d60);
        result = texel;
        if (red - green <= tolerance && red - green + tolerance >= 0 &&
            blue - green <= tolerance && blue - green + tolerance >= 0 &&
            red - blue <= tolerance && red - blue + tolerance >= 0) {
            uint32_t brightness = red + green + blue;
            if ((int32_t)brightness <= (int32_t)WORD(0x4d64)) result = HALF(0x34c0);
            else {
                result = WORD(0x4d58);
                for (;;) {
                    uint32_t next = result * 3;
                    uint16_t sum = BYTE(0x39c2 + next) + BYTE(0x39c3 + next) + BYTE(0x39c4 + next);
                    if ((int16_t)brightness <= (int16_t)sum || result == WORD(0x4d5c)) break;
                    result++;
                }
            }
        }
        *(uint8_t *)POINTER(destination + index) = (uint8_t)result;
    }
    return result;
}

static uint32_t resource_neutral_maps(void)
{
    static const uint8_t first[] = {32,44,56,68,77,86,95,104,32,86,95,104};
    static const uint8_t last[] = {43,55,67,76,85,94,103,112,40,91,100,110};
    uint32_t index, result = 0;
    WORD(0x4d64) = 10;
    for (index = 0; index < 12; index++) {
        WORD(0x4d68) = ADDRESS(hg2d_image + (index < 8 ? 0xd70 : 0xe70));
        WORD(0x4d60) = index < 8 ? 256 : 130;
        WORD(0x4d6c) = ADDRESS(hg2d_image + 0x1d8c + index * 256);
        WORD(0x4d50) = first[index]; WORD(0x4d54) = last[index];
        result = resource_neutral_row();
    }
    return result;
}

static uint32_t resource_haze_refresh(void)
{
    uint16_t divisor = HALF(0xd4e), quotient = divisor ? GLOBAL_HALF(0xe88) / divisor : 0xffff;
    uint32_t row;
    if ((int16_t)quotient > 6) quotient = 6;
    uint16_t source = (uint16_t)(0xa00 - ((quotient & 255) << 8));
    for (row = 0xa00; row < 0xf00; row += 256, source = (uint16_t)(source + 256)) {
        uint8_t value = BYTE(0xd70 + source + HALF(0x361e));
        for (uint32_t colour = 0x140; colour < 0x150; colour += 2)
            BYTE(0xd70 + row + HALF(0x34c0 + colour)) = value;
    }
    return resource_neutral_maps();
}

static uint32_t resource_haze_load(uint32_t name, uint32_t selector)
{
    call_four load = *(call_four *)((uintptr_t)WORD(0x1c) + 0x1c);
    WORD(0xd58) = name; WORD(0xd5c) = selector;
    load(ADDRESS(hg2d_image + 0xd70), name, selector, 10);
    resource_remap_rows(0xd70);
    return resource_haze_refresh();
}

/* +0x68, 0x207325a0. FE failure returns before changing the haze name. */
static uint32_t STDCALL resource_palette_load(const uint8_t *name, uint32_t selector)
{
    call_four load = *(call_four *)((uintptr_t)WORD(0x1c) + 0x1c);
    uint32_t destination = 0x3fd8;
    if (!load(ADDRESS(hg2d_image + 0x32e0), ADDRESS(name), selector, 9)) return 0;
    do { BYTE(destination++) = *name++; } while (BYTE(destination - 1) != '.');
    HALF(destination) = 0x6168; HALF(destination + 2) = 0x007a;
    resource_haze_load(ADDRESS(hg2d_image + 0x3fd8), selector);
    BYTE(0x4004) = 0;
    palette_refresh();
    return 1;
}

/* +0x58, 0x20731890. Retail ignores all three file-service return values. */
static uint32_t STDCALL resource_palette_default(void)
{
    call_four open = *(call_four *)((uintptr_t)WORD(4) + 0x17c);
    call_four read = *(call_four *)((uintptr_t)WORD(4) + 0x19c);
    call_one close = *(call_one *)((uintptr_t)WORD(4) + 0x13c);
    open(ADDRESS(hg2d_image + 0x3fe6), ADDRESS(hg2d_image + 0x3fc4), 9, 0);
    read(WORD(0x3fe6), ADDRESS(hg2d_image + 0x30a0), 0x15c, ADDRESS(hg2d_image + 0x3fea));
    close(WORD(0x3fe6));
    HALF(0x36c0) = 1;
    palette_force_body();
    return resource_haze_load(ADDRESS(hg2d_image + 0x3fd0), 0);
}
