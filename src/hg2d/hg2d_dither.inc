/* Unkeyed software dither spans, 0x20749d95 / 0x207541ee. */
static uint8_t dither_texel(uint32_t texture, uint64_t u, uint64_t v, uint32_t bias)
{
    uint32_t column = (uint8_t)((u + bias) >> 32);
    uint32_t row = (uint8_t)((v + bias) >> 32);
    return *(uint8_t *)POINTER(texture + column + row * 256);
}

static void dither_advance(uint64_t *u, uint64_t *v, uint8_t u_integer, uint8_t v_integer)
{
    *u = (*u + ((uint64_t)u_integer << 32) + WORD(0x41a8)) & 0xffffffffffULL;
    *v = (*v + ((uint64_t)v_integer << 32) + WORD(0x41ac)) & 0xffffffffffULL;
}

static uint32_t dither_bias(uint32_t address, uint32_t y)
{
    return ((address & 1 ? 0x80000000u : 0) + (y & 1 ? 0 : 0x40000000u));
}

static void dither_pixels(uint32_t address, uint32_t count, uint32_t start,
                          uint32_t y, int direction, int tail)
{
    uint32_t texture = WORD(0x948), remaining = count;
    uint64_t u = ((uint64_t)(uint8_t)(WORD(start) >> 16) << 32) | (uint32_t)(WORD(start) << 16);
    uint64_t v = ((uint64_t)(uint8_t)(WORD(start + 4) >> 16) << 32) | (uint32_t)(WORD(start + 4) << 16);
    uint8_t u_integer = (uint8_t)(WORD(0x41a8) >> 16), v_integer = (uint8_t)(WORD(0x41ac) >> 16);
    WORD(0x41a8) = WORD(0x41a8) * 65536u;
    WORD(0x41ac) = WORD(0x41ac) * 65536u;
    if (tail) {
        for (uint32_t index = 0; index < count; index++) {
            if (index + 1 == count && WORD(0x45d5)) return;
            *(uint8_t *)POINTER(address) = dither_texel(texture, u, v, dither_bias(address, y));
            dither_advance(&u, &v, u_integer, v_integer);
            address += direction;
        }
        return;
    }
    if (remaining && ((address & 1) != 0) == (direction > 0)) {
        *(uint8_t *)POINTER(address) = dither_texel(texture, u, v, dither_bias(address, y));
        dither_advance(&u, &v, u_integer, v_integer);
        address += direction; remaining--;
    }
    while (remaining >= 2) {
        uint8_t first = dither_texel(texture, u, v, dither_bias(address, y));
        dither_advance(&u, &v, u_integer, v_integer);
        uint8_t second = dither_texel(texture, u, v, dither_bias(address + direction, y));
        dither_advance(&u, &v, u_integer, v_integer);
        uint8_t *output = POINTER(direction > 0 ? address : address - 1);
        output[0] = direction > 0 ? first : second;
        output[1] = direction > 0 ? second : first;
        address += direction * 2; remaining -= 2;
    }
    if (remaining) {
        *(uint8_t *)POINTER(address) = dither_texel(texture, u, v, dither_bias(address, y));
        dither_advance(&u, &v, u_integer, v_integer);
    }
}
