/* Mip dithering masks the biased coordinate register itself before each fetch.
 * Removing the bias after masking retains the wrap for the next sample. */
static uint8_t projective_mip_dither_texel(uint32_t texture, uint64_t *u, uint64_t *v, uint32_t bias)
{
    uint64_t sampled_u = (*u + bias) & (((uint64_t)(uint8_t)WORD(0x4375) << 32) | 0xffffffffu);
    uint64_t sampled_v = (*v + bias) & (((uint64_t)(uint8_t)(WORD(0x4375) >> 8) << 32) | 0xffffffffu);
    *u = (sampled_u - bias) & 0xffffffffffULL;
    *v = (sampled_v - bias) & 0xffffffffffULL;
    return *(uint8_t *)POINTER(texture + (uint32_t)(sampled_u >> 32) + (uint32_t)(sampled_v >> 32) * 256);
}

static void projective_mip_dither_pixels(uint32_t address, uint32_t count, uint32_t start,
                          uint32_t y, int direction, int tail, int fog, uint32_t shade)
{
    uint32_t texture = projective_mip_prepare(start), remaining = count;
    uint64_t u = ((uint64_t)(uint8_t)(WORD(start) >> 16) << 32) | (uint32_t)(WORD(start) << 16);
    uint64_t v = ((uint64_t)(uint8_t)(WORD(start + 4) >> 16) << 32) | (uint32_t)(WORD(start + 4) << 16);
    uint8_t u_integer = (uint8_t)(WORD(0x41a8) >> 16), v_integer = (uint8_t)(WORD(0x41ac) >> 16);
    WORD(0x41a8) = WORD(0x41a8) * 65536u;
    WORD(0x41ac) = WORD(0x41ac) * 65536u;
    if (fog) {
        for (uint32_t index = 0; index < count; index++) {
            if (tail && index + 1 == count && WORD(0x45d5)) return;
            uint8_t texel = projective_mip_dither_texel(texture, &u, &v, dither_bias(address, y));
            *(uint8_t *)POINTER(address) = BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256);
            dither_advance(&u, &v, u_integer, v_integer);
            shade = (shade + WORD(0x4358)) & 0xffff0fffu;
            address += direction;
        }
        return;
    }
    if (tail) {
        for (uint32_t index = 0; index < count; index++) {
            if (index + 1 == count && WORD(0x45d5)) return;
            *(uint8_t *)POINTER(address) = projective_mip_dither_texel(texture, &u, &v, dither_bias(address, y));
            dither_advance(&u, &v, u_integer, v_integer);
            address += direction;
        }
        return;
    }
    if (remaining && ((address & 1) != 0) == (direction > 0)) {
        *(uint8_t *)POINTER(address) = projective_mip_dither_texel(texture, &u, &v, dither_bias(address, y));
        dither_advance(&u, &v, u_integer, v_integer);
        address += direction; remaining--;
    }
    while (remaining >= 2) {
        uint8_t first = projective_mip_dither_texel(texture, &u, &v, dither_bias(address, y));
        dither_advance(&u, &v, u_integer, v_integer);
        uint8_t second = projective_mip_dither_texel(texture, &u, &v, dither_bias(address + direction, y));
        dither_advance(&u, &v, u_integer, v_integer);
        uint8_t *output = POINTER(direction > 0 ? address : address - 1);
        output[0] = direction > 0 ? first : second;
        output[1] = direction > 0 ? second : first;
        address += direction * 2; remaining -= 2;
    }
    if (remaining) {
        *(uint8_t *)POINTER(address) = projective_mip_dither_texel(texture, &u, &v, dither_bias(address, y));
        dither_advance(&u, &v, u_integer, v_integer);
    }
}

