/* 0x2078bd00: palette realization with a bound device. */
typedef uint32_t (STDCALL *call_five)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

static void device_palette_textures(void)
{
    uint32_t offset, index;
    for (index = 0; index < 8; index++) WORD(0x8820 + index * 4) = 0;
    if (WORD(0x28aec) == 8) {
        for (offset = POOL; offset < POOL_END; offset += 0x34) {
            uint32_t *texture = (uint32_t *)(hg2d_image + offset);
            if (texture[3]) texture_release(texture);
        }
        WORD(0x289ec) = WORD(0x289d4) = WORD(0x5f40) = 0;
    } else {
        for (offset = POOL; offset < POOL_END; offset += 0x34)
            if (WORD(offset + 12)) WORD(offset + 4) |= 0x20000;
    }
}

static void device_palette_create(void)
{
    uint8_t entries[1024];
    uint32_t index, palette = 0, instance = WORD(0x289f0);
    uint32_t *object = POINTER(instance), *vtable = POINTER(object[0]);
    for (index = 0; index < 256; index++) {
        entries[index * 4] = BYTE(0x39c2 + index * 3);
        entries[index * 4 + 1] = BYTE(0x39c3 + index * 3);
        entries[index * 4 + 2] = BYTE(0x39c4 + index * 3);
        entries[index * 4 + 3] = 0;
    }
    ((call_five)(uintptr_t)vtable[5])(instance, 4, ADDRESS(entries), ADDRESS(&palette), 0);
    instance = WORD(0x289f4);
    if (instance) {
        object = POINTER(instance); vtable = POINTER(object[0]);
        ((call_one)(uintptr_t)vtable[2])(instance);
    }
    /* CreatePalette failure is ignored; the zero-initialized output replaces
     * the old palette even when the method left it untouched. */
    WORD(0x289f4) = palette;
}

static void device_fog_colour(void)
{
    uint32_t base = GLOBAL_HALF(0xcba) * 180u >> 16;
    uint32_t first = base, second = base, third = base, packed;
    uint32_t depth = GLOBAL_HALF(0xe88), intensity = GLOBAL_HALF(0x12a0);
    WORD(0x28860) = 0;
    if (intensity >= 100 && depth >= 0x1c52 && depth < 0x7d00) {
        uint32_t angle = GLOBAL_WORD(0x5d0), colour;
        if (GLOBAL[0x238] < 12) { angle -= 0x40000000u; colour = 0x1e48c0; }
        else { angle += 0x40000000u; colour = 0x133e9f; }
        WORD(0x28860) = 1;
        uint32_t magnitude = (int32_t)angle < 0 ? 0u - angle : angle;
        if ((int32_t)magnitude <= 0x30000000) {
            int32_t distance = (int32_t)(0x30000000u - magnitude) >> 16;
            uint32_t product = (uint32_t)distance * intensity;
            int32_t amount = (int32_t)product / 12288;
            uint32_t scaled = (uint32_t)amount * 850u;
            int32_t blend = (int32_t)scaled / 1024;
            uint32_t inverse = 1024u - (uint32_t)blend;
            first = (uint32_t)((int32_t)(base * inverse + (uint32_t)blend * (colour & 255)) >> 10);
            second = (uint32_t)((int32_t)(base * inverse + (uint32_t)blend * ((colour >> 8) & 255)) >> 10);
            third = (uint32_t)((int32_t)(base * inverse + (uint32_t)blend * ((colour >> 16) & 255)) >> 10);
        }
    }
    packed = (((first << 8) | second) << 8) | third;
    if (WORD(0x5f24)) {
        if (WORD(0x28b88) != packed) {
            uint32_t instance = WORD(0x289d0), *object = POINTER(instance), *vtable = POINTER(object[0]);
            WORD(0x28b88) = packed;
            WORD(0x5f20) = ((call_three)(uintptr_t)vtable[23])(instance, 0x22, packed);
        }
    } else WORD(0x28b88) = packed;
}

static uint32_t device_colour_scale(void)
{
    uint32_t index, value = 0, brightness = GLOBAL_HALF(0xcba);
    for (index = 0; index < 28; index++) {
        uint32_t source = WORD(0x5e10 + index * 4);
        uint32_t red = ((source >> 16) & 255) * brightness >> 16;
        uint32_t green = ((source >> 8) & 255) * brightness >> 16;
        uint32_t blue = (source & 255) * brightness >> 16;
        value = source & 0xff000000u;
        WORD(0x28960 + index * 4) = value | (red << 16) | (green << 8) | blue;
    }
    return value;
}

static uint32_t device_palette_realize(void)
{
    if (!WORD(0x28940)) return 0;
    device_palette_textures();
    if (WORD(0x28aec) == 8) device_palette_create();
    device_fog_colour();
    return device_colour_scale();
}
