static uint32_t projective_mip_magnitude(uint32_t factor)
{
    return factor ^ ((int32_t)factor < 0 ? 0xffffffffu : 0);
}

/* Distance-selected packed mip levels in software projective spans. */
static uint8_t projective_mip_sample(uint32_t texture_base, uint32_t *u_fraction, uint32_t *v_fraction,
                                     uint8_t *u, uint8_t *v, uint8_t u_step, uint8_t v_step)
{
    *u &= (uint8_t)WORD(0x4375);
    *v &= (uint8_t)(WORD(0x4375) >> 8);
    uint8_t texel = *(uint8_t *)POINTER(texture_base + *u + ((uint32_t)*v << 8));
    uint64_t carry = (uint64_t)*u_fraction + WORD(0x41a8);
    *u_fraction = (uint32_t)carry; *u = (uint8_t)(*u + u_step + (uint32_t)(carry >> 32));
    carry = (uint64_t)*v_fraction + WORD(0x41ac);
    *v_fraction = (uint32_t)carry; *v = (uint8_t)(*v + v_step + (uint32_t)(carry >> 32));
    return texel;
}

static uint32_t projective_mip_prepare(uint32_t start)
{
    uint32_t distance = (uint32_t)(((uint64_t)WORD(0x4361) * WORD(0x4369)) >> 21);
    if (distance >= 127) distance = 127;
    uint32_t level = WORD(0x439d + distance * 4), shift = level & 31;
    uint32_t texture_base = WORD(0x948) + WORD(0x4379 + level * 4);
    WORD(0x435d) = level;
    WORD(0x4375) = WORD(0x45ad + level * 4);
    WORD(start) >>= shift; WORD(start + 4) >>= shift;
    WORD(0x41a8) = (uint32_t)((int32_t)WORD(0x41a8) >> shift);
    WORD(0x41ac) = (uint32_t)((int32_t)WORD(0x41ac) >> shift);
    return texture_base;
}

static void projective_mip_pixels(uint32_t pixel_address, uint32_t count, uint32_t start, int direction, int tail)
{
    uint32_t texture_base = projective_mip_prepare(start);
    uint32_t u_fraction = WORD(start) << 16, v_fraction = WORD(start + 4) << 16;
    uint8_t u = (uint8_t)(WORD(start) >> 16), v = (uint8_t)(WORD(start + 4) >> 16);
    uint8_t u_step = (uint8_t)(WORD(0x41a8) >> 16), v_step = (uint8_t)(WORD(0x41ac) >> 16);
    uint32_t remaining = count;
    WORD(0x41a8) <<= 16; WORD(0x41ac) <<= 16;
    if (!tail && remaining && ((pixel_address & 2) != 0) == (direction > 0)) {
        uint8_t value = projective_mip_sample(texture_base, &u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t *pixel = POINTER(pixel_address);
        pixel[0] = value; pixel[1] = value;
        pixel_address += direction * 2; remaining--;
    }
    while (!tail && remaining >= 2) {
        uint8_t first = projective_mip_sample(texture_base, &u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t second = projective_mip_sample(texture_base, &u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t *pixel = POINTER(direction > 0 ? pixel_address : pixel_address - 2);
        if (direction > 0) {
            pixel[0] = pixel[1] = first; pixel[2] = pixel[3] = second;
        } else {
            pixel[0] = pixel[1] = second; pixel[2] = pixel[3] = first;
        }
        pixel_address += direction * 4; remaining -= 2;
    }
    while (remaining--) {
        uint8_t value = projective_mip_sample(texture_base, &u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t *pixel = POINTER(pixel_address);
        pixel[0] = value; pixel[1] = value;
        pixel_address += direction * 2;
    }
}

static void projective_mip_fog_pixels(uint32_t address, uint32_t count, uint32_t start, int direction, uint32_t shade)
{
    uint32_t texture_base = projective_mip_prepare(start);
    uint32_t u_fraction = WORD(start) << 16, v_fraction = WORD(start + 4) << 16;
    uint8_t u = (uint8_t)(WORD(start) >> 16), v = (uint8_t)(WORD(start + 4) >> 16);
    uint8_t u_step = (uint8_t)(WORD(0x41a8) >> 16), v_step = (uint8_t)(WORD(0x41ac) >> 16);
    WORD(0x41a8) <<= 16; WORD(0x41ac) <<= 16;
    while (count--) {
        uint8_t texel = projective_mip_sample(texture_base, &u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t painted = BYTE(0xd70 + texel + ((shade >> 8) & 255) * 256);
        shade = (shade + WORD(0x4358)) & 0xffff0fffu;
        uint8_t *pixel = POINTER(address);
        pixel[0] = pixel[1] = painted;
        address += direction * 2;
    }
}
