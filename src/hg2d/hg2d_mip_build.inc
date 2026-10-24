/* 0x207900e0: grow an indexed texture and build its packed mip pyramid.
 * These software levels use host allocation, independently of DirectDraw. */
static uint8_t texture_mip_choice(const uint8_t *samples)
{
    uint32_t average[3] = {0,0,0}, index, channel, best = 0, minimum = 0xffffffffu;
    for (index = 0; index < 4; index++)
        for (channel = 0; channel < 3; channel++) average[channel] += BYTE(0x39c2 + samples[index] * 3 + channel);
    for (channel = 0; channel < 3; channel++) average[channel] >>= 2;
    for (index = 0; index < 4; index++) {
        uint32_t distance = 0;
        for (channel = 0; channel < 3; channel++) {
            int32_t difference = (int32_t)average[channel] - BYTE(0x39c2 + samples[index] * 3 + channel);
            distance += (uint32_t)(difference * difference);
        }
        if (distance < minimum) { minimum = distance; best = index; }
    }
    return samples[best];
}

static uint32_t texture_mip_build(uint32_t *texture, uint32_t level)
{
    uint32_t source, destination, size, x, y, index;
    call_three grow = *(call_three *)((uintptr_t)WORD(4) + 0x210);
    if (!texture_inside(ADDRESS(texture)) || (int32_t)level < 0 || level > 8) return 0;
    if (!(texture[1] & 0x40000) && (int32_t)level > 0) {
        if (grow(4, 0x18000, ADDRESS(texture))) return 0;
        destination = texture[0] + 0x10000;
        for (index = 0; index < 0x8000; index++) *(uint8_t *)POINTER(destination + index) = 0;
        source = texture[0];
        for (size = 256; size > 1; size >>= 1) {
            for (y = 0; y < size; y += 2) for (x = 0; x < size; x += 2) {
                uint32_t at = source + y * 256 + x;
                uint8_t samples[4] = {*(uint8_t *)POINTER(at), *(uint8_t *)POINTER(at + 1),
                                      *(uint8_t *)POINTER(at + 256), *(uint8_t *)POINTER(at + 257)};
                *(uint8_t *)POINTER(destination + (y >> 1) * 256 + (x >> 1)) = texture_mip_choice(samples);
            }
            source = destination;
            destination += size >> 1;
        }
        texture[1] |= 0x40000;
    }
    return texture[0] + WORD(0x5ef0 + level * 4);
}
