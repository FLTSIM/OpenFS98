/* 0x2078e550: create a texture surface with the selected format. */
uint32_t STDCALL __attribute__((used)) hg2d_device_surface(uint32_t width, uint32_t height,
    uint32_t level, uint32_t flags, uint32_t *out)
{
    uint32_t description[27] = {0}, instance, result;
    int32_t divisor = (int32_t)(1u << (level & 31));
    description[0] = 0x6c; description[1] = 0x1007;
    __builtin_memcpy(description + 18, hg2d_image + 0x28ae0, 32);
    description[3] = (uint32_t)((int32_t)width / divisor);
    description[2] = (uint32_t)((int32_t)height / divisor);
    description[26] = !WORD(0x28cc8) && (flags & 0x4000) ? 0x4005000 : 0x1800;
    if (WORD(0x5f28)) {
        description[26] |= 0x400008;
        description[1] = 0x21007; description[6] = 8u - level;
    }
    instance = WORD(0x289f0);
    result = ((call_four)device_method(instance, 6))(instance, ADDRESS(description), ADDRESS(out), 0);
    if (!result && WORD(0x289f4) && description[21] == 8) {
        instance = *out;
        ((call_two)device_method(instance, 31))(instance, WORD(0x289f4));
    }
    return result;
}

/* 0x2078e640: failed allocations are remembered per level. Peak memory
 * accounting uses a signed comparison, with shifts masked as on x86. */
uint32_t STDCALL __attribute__((used)) hg2d_device_allocate(uint32_t *texture, uint32_t level)
{
    uint32_t output = level, amount, total;
    if (WORD(0x8820 + level * 4)) return 0;
    if (hg2d_device_surface(256, 256, level, 0x4000, &output)) {
        WORD(0x8820 + level * 4) = 1;
        return 0;
    }
    WORD(0x8800 + level * 4)++;
    texture[3] = output; texture[2] = 0; texture[7] = level;
    amount = 1u << (((8u - level) * 2) & 31);
    total = WORD(0x289ec) + amount;
    WORD(0x289ec) = total;
    if ((int32_t)total > (int32_t)WORD(0x289d4)) WORD(0x289d4) = total;
    return 1;
}

/* 0x2078e400: move the most distant eligible surface from another texture.
 * Three passes prefer the exact level, then older frames, then the previous
 * frame. Equal distances keep the earlier pool entry. */
void STDCALL __attribute__((used)) hg2d_device_reuse(uint32_t *texture, uint32_t level)
{
    uint32_t pass, offset, selected = 0, distance = 0, found = 0;
    for (pass = 0; pass < 3; pass++) {
        for (offset = POOL; offset < POOL_END; offset += 0x34) {
            uint32_t *candidate = (uint32_t *)(hg2d_image + offset);
            uint32_t frame = GLOBAL_WORD(0x2ac) - (pass == 2 ? 0u : 1u);
            if (!candidate[2]) continue;
            if (pass == 0 ? candidate[7] != level : (int32_t)candidate[7] > (int32_t)level) continue;
            if (candidate[4] >= frame || candidate[5] <= distance) continue;
            found = 1; selected = offset; distance = candidate[5];
        }
        if (found) break;
        if (pass == 2) {
            if (!WORD(0x5f40)) WORD(0x5f40) = 1;
            if ((int32_t)level < 1) { level++; pass--; }
        }
    }
    if (found) {
        uint32_t *candidate = (uint32_t *)(hg2d_image + selected);
        texture[3] = candidate[3]; texture[2] = candidate[2]; texture[7] = candidate[7];
        candidate[3] = candidate[2] = candidate[7] = 0;
    }
}
