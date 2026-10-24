/* Device texture-format fallback order through the real matcher callback. */
static uint32_t device_select_format_entry[2];
static uint32_t STDCALL device_format_option(const char *section, const char *key, uint32_t *out)
{
    event(46);
    for (const char *cursor = section; *cursor; cursor++) event((uint8_t)*cursor);
    for (const char *cursor = key; *cursor; cursor++) event((uint8_t)*cursor);
    event(*out);
    if (device_option_write) *out = device_option_value;
    device_option_calls++;
    return 0x76543210;
}

static void run_device_select_format(void)
{
    static const uint32_t formats[][8] = {
        {32,0x20,0,8,0,0,0,0}, {32,0x41,0,16,0x7c00,0x3e0,0x1f,0x8000},
        {32,0x41,0,16,0xf00,0xf0,0xf,0xf000}, {32,0x40,0,16,0xf800,0x7e0,0x1f,0},
        {32,0x41,0,32,0xff0000,0xff00,0xff,0xff000000},
    };
    static const uint32_t settings[] = {0xffffffff,8,16,12,0x22b,0x115c,0x22b8,0};
    uint32_t variant, index, profile, result, flags;
    palette_reset();
    memset(device_vtables[1], 0, sizeof device_vtables[1]);
    device_objects[1][0] = ADDRESS(device_vtables[1]); device_vtables[1][9] = ADDRESS(device_enumerate_formats);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    host[0xa8 / 4] = ADDRESS(device_format_option);
    device_enum_result = 0x88761234;
    for (variant = 0; variant < 128; variant++) {
        profile = variant / 16; device_enum_reverse = (scenario + variant) & 1;
        device_enum_count = profile < 5 ? 1 : profile == 5 ? 0 : 5;
        for (index = 0; index < 5; index++) {
            memset(device_enum_surfaces[index], 0x5a, sizeof device_enum_surfaces[index]);
            memcpy(device_enum_surfaces[index] + 18, formats[profile < 5 ? profile : index], 32);
        }
        if (profile == 7) for (index = 0; index < 5; index++) device_enum_surfaces[index][19] = 0;
        device_option_value = settings[(scenario + variant) % 8];
        device_option_write = (scenario + variant / 8) % 3 != 0;
        device_option_calls = device_enum_calls = 0;
        WORD(images[side], 0x28a84) = ((scenario + variant / 2) & 1) ? 8 : 0;
        flags = variant & 8 ? 0x400 : 0;
        touched = NULL; check_carry = 0;
        result = invoke(device_select_format_entry[side], flags, 0, 0, 0xaabbccdd, 1);
        compare(result);
        CHECK(device_option_calls == 1 && device_enum_calls >= 1 && device_enum_calls <= 5);
        {
            uint32_t expected = profile == 5 || profile == 7 ? 0 : 1;
            if (!profile) expected = WORD(images[side], 0x28a84) &&
                ((flags & 0x400) || (device_option_write && (device_option_value == 8 || device_option_value == 0))) ? 1 : 0;
            CHECK(result == expected);
        }
        if (profile < 5 && result) CHECK(!memcmp(images[side] + 0x28ae0, formats[profile], 32));
    }
}
