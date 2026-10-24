/* 0x2078f7d0: choose texture resolution, manage pressure/history and keys,
 * upload dirty surfaces, and return the device texture handle. */
uint32_t STDCALL __attribute__((used)) hg2d_device_texture(uint32_t *texture, uint32_t key)
{
    uint32_t frame = GLOBAL_WORD(0x2ac), level, distance, flags;
    if (WORD(0x5f48) != frame) {
        WORD(0x5f48) = frame;
        if (WORD(0x5f40)) {
            int32_t threshold = (int32_t)(WORD(0x289d4) * 3) / 4;
            if ((int32_t)WORD(0x28cc4) <= threshold) {
                uint32_t old = WORD(0x5f40);
                WORD(0x5f40) = old + 1;
                if ((int32_t)old > 30) { WORD(0x5f40) = 0; device_textures_clear(); }
            } else WORD(0x5f40) = 1;
        }
        WORD(0x28cc4) = 0;
    }
    flags = texture[1];
    if (flags & 0x10ff00) texture[5] = 0;
    else if (texture[4] < GLOBAL_WORD(0x2ac) - 2 || GLOBAL_WORD(0xd4c) < texture[5]) texture[5] = GLOBAL_WORD(0xd4c);
    distance = texture[5];
    level = WORD(0x5f2c) + (distance < 0xbb800 ? 0 : distance < 0x3a9800 ? 1 : 2);
    if (texture[4] != GLOBAL_WORD(0x2ac)) WORD(0x28cc4) += 1u << (((8u - level) * 2) & 31);
    if (WORD(0x5f40) && distance && (int32_t)level < 2) level++;
    if (texture[2] && ((int32_t)level < (int32_t)texture[7] || (WORD(0x5f40) && (int32_t)level > (int32_t)texture[7]))) {
        if (texture[4] == GLOBAL_WORD(0x2ac)) level = texture[7];
        else texture_release(texture);
    }
    if (key || texture[6] >= 26) {
        uint32_t next_key = (int32_t)key >= 1 && (int32_t)key < 26 ? 1 : key;
        uint32_t previous_key = texture[6];
        if ((int32_t)previous_key >= 1 && (int32_t)previous_key < 26) previous_key = 1;
        if (next_key != previous_key && (key || WORD(0x28aec) == 8)) {
            texture[6] = key; texture[1] |= 0x20000;
        }
    }
    if (!texture[2] || !texture[3]) texture[1] |= 0x20000;
    if (texture[1] & 0x20000) {
        hg2d_device_prepare(texture, level);
        texture[1] &= ~0x20000u;
    }
    texture[4] = GLOBAL_WORD(0x2ac);
    return texture[2];
}
