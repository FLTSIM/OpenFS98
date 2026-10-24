/* Selection options: configuration overrides, caps and cached fog states. */
static uint32_t device_config_entry[4][2], device_option_write, device_option_value, device_option_calls;
static uint32_t STDCALL device_option(const char *section, const char *key, uint32_t *out)
{
    event(44);
    for (const char *cursor = section; *cursor; cursor++) event((uint8_t)*cursor);
    for (const char *cursor = key; *cursor; cursor++) event((uint8_t)*cursor);
    event(ADDRESS(out) - ADDRESS(images[side])); event(*out);
    if (device_option_write) *out = device_option_value;
    device_option_calls++;
    return 0x76543210;
}

static void run_device_config(void)
{
    static const uint32_t settings[] = {0,1,0x40,0x80,0x100,0xffffffff,2,0x80000000};
    static const uint32_t blend_caps[] = {0,0x80,0x200,0x280};
    static const uint32_t mip_caps[] = {0,8,16,24};
    uint32_t variant, flags, blend, mip, format, fog, fog28, fog35, expected;
    palette_reset();
    memset(device_vtables[1], 0, sizeof device_vtables[1]);
    device_objects[1][0] = ADDRESS(device_vtables[1]); device_vtables[1][23] = ADDRESS(device_cached_state);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    host[0xa8 / 4] = ADDRESS(device_option);
    device_render_result = 0x88761234; device_state_result_offset = 0x5f20;
    for (variant = 0; variant < 64; variant++) {
        flags = ((variant & 1) ? 0x10 : 0) | ((variant & 2) ? 0x80 : 0) |
                ((variant & 4) ? 0x800 : 0) | ((variant & 8) ? 0x1000 : 0);
        flags |= random_word() & ~0x1890u;
        blend = blend_caps[(scenario + variant / 4) % 4];
        mip = mip_caps[(scenario + variant / 2) % 4];
        format = (scenario + variant) % 3 ? 16 : 8;
        fog = (scenario + variant / 8) & 1;
        fog28 = (scenario + variant) % 3; fog35 = (scenario + variant / 2) % 3;
        WORD(images[side], 0x28a80) = blend | (random_word() & ~0x280u);
        WORD(images[side], 0x28a88) = mip | (random_word() & ~0x18u);
        WORD(images[side], 0x28a6c) = (fog ? 0x80 : 0) | (random_word() & ~0x80u);
        WORD(images[side], 0x28aec) = format;
        WORD(images[side], 0x28b70) = fog28; WORD(images[side], 0x28b8c) = fog35;
        WORD(images[side], 0x5f24) = 0xdeadbeef;
        device_option_value = settings[(scenario + variant) % 8];
        device_option_write = (scenario + variant / 8) % 3 != 0;
        device_option_calls = 0;
        device_states_check(device_config_entry[0][side], 0, 0, 0);
        expected = device_option_write && !device_option_value ? 0 : blend & 0x200 ? 1 : blend & 0x80 ? 2 : 0;
        CHECK(WORD(images[side], 0x289e0) == expected && device_option_calls == 1);
        device_states_check(device_config_entry[1][side], flags, 0, 1);
        CHECK(WORD(images[side], 0x5f28) == (mip && (flags & 0x80) ? 1u : 0u));
        expected = format != 8 && (flags & 0x800) ? 1 : 0;
        if (device_option_write) {
            if (device_option_value == 0x100) expected = 0;
            if (device_option_value == 0x80) expected = 1;
            if (device_option_value == 0x40) expected = 2;
        }
        CHECK(WORD(images[side], 0x5f2c) == expected && device_option_calls == 2);
        device_states_check(device_config_entry[2][side], flags, 0, 1);
        expected = flags & 0x10 ? 2 : 1;
        CHECK(WORD(images[side], 0x289d8) == expected && WORD(images[side], 0x289dc) == expected && WORD(images[side], 0x289e4) == expected);
        CHECK(WORD(images[side], 0x289e8) == expected + (mip && (flags & 0x80) ? 2u : 0u));
        device_states_check(device_config_entry[3][side], flags, 0, 1);
        CHECK(WORD(images[side], 0x5f24) == ((flags & 0x1000) && fog ? 1u : 0u));
        CHECK(WORD(images[side], 0x28b70) == (!(flags & 0x1000) || fog ? 0u : fog28));
        CHECK(WORD(images[side], 0x28b8c) == ((flags & 0x1000) && fog ? 0u : fog35));
    }
}
