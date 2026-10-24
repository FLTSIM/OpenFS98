/* Software projective dither sampling helpers. */
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

/* Fog variants use individual byte stores, before both coordinate and shade
 * advances. They do not prefetch the next texel as the plain paired loop does. */
static void dither_fog_pixels(uint32_t address, uint32_t count, uint32_t start,
                              uint32_t y, int direction, int tail, uint32_t level)
{
    uint32_t texture = WORD(0x948);
    uint64_t sample_u = ((uint64_t)(uint8_t)(WORD(start) >> 16) << 32) | (uint32_t)(WORD(start) << 16);
    uint64_t sample_v = ((uint64_t)(uint8_t)(WORD(start + 4) >> 16) << 32) | (uint32_t)(WORD(start + 4) << 16);
    uint8_t u_integer = (uint8_t)(WORD(0x41a8) >> 16), v_integer = (uint8_t)(WORD(0x41ac) >> 16);
    WORD(0x41a8) *= 65536u;
    WORD(0x41ac) *= 65536u;
    for (uint32_t pixel = 0; pixel < count; pixel++) {
        if (tail && pixel + 1 == count && WORD(0x45d5)) return;
        uint8_t texel = dither_texel(texture, sample_u, sample_v, dither_bias(address, y));
        *(uint8_t *)POINTER(address) = BYTE(0xd70 + texel + ((level >> 8) & 255) * 256);
        dither_advance(&sample_u, &sample_v, u_integer, v_integer);
        level = (level + WORD(0x4358)) & 0xffff0fffu;
        address += direction;
    }
}

/* Keyed full blocks omit the dither bias; only their tail code applies it. */
static void dither_key_pixels(uint32_t address, uint32_t count, uint32_t start,
                              uint32_t y, int direction, int tail, int fog, uint32_t level)
{
    uint32_t texture = WORD(0x948);
    uint64_t key_u = ((uint64_t)(uint8_t)(WORD(start) >> 16) << 32) | (uint32_t)(WORD(start) << 16);
    uint64_t key_v = ((uint64_t)(uint8_t)(WORD(start + 4) >> 16) << 32) | (uint32_t)(WORD(start + 4) << 16);
    uint8_t u_integer = (uint8_t)(WORD(0x41a8) >> 16), v_integer = (uint8_t)(WORD(0x41ac) >> 16);
    WORD(0x41a8) *= 65536u;
    WORD(0x41ac) *= 65536u;
    for (uint32_t key_pixel = 0; key_pixel < count; key_pixel++) {
        if (tail && key_pixel + 1 == count && WORD(0x45d5)) return;
        uint8_t indexed = dither_texel(texture, key_u, key_v, tail ? dither_bias(address, y) : 0);
        if (indexed >= BYTE(0x435c)) {
            uint8_t painted = fog ? BYTE(0xd70 + indexed + ((level >> 8) & 255) * 256) : indexed;
            *(uint8_t *)POINTER(address) = painted;
        }
        dither_advance(&key_u, &key_v, u_integer, v_integer);
        if (fog) level = (level + WORD(0x4358)) & 0xffff0fffu;
        address += direction;
    }
}
