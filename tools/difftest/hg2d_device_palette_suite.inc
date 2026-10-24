/* Bound-device palette callbacks, texture invalidation and fog render state. */
static uint32_t device_objects[5][2], device_vtables[5][40];
static uint32_t device_factory_success, device_render_result, device_palette_calls, device_state_calls;
static uint8_t device_palette_entries[1024];

static uint32_t device_tag(uint32_t instance)
{
    for (uint32_t index = 0; index < 5; index++)
        if (instance == ADDRESS(device_objects[index])) return index + 1;
    CHECK(0);
    return 0;
}

static uint32_t STDCALL device_release(uint32_t instance)
{
    event(38); event(device_tag(instance));
    event(WORD(images[side], 0x8820)); event(WORD(images[side], 0x289ec));
    event(WORD(images[side], 0x289f4) ? device_tag(WORD(images[side], 0x289f4)) : 0);
    return 17;
}

static uint32_t STDCALL device_create_palette(uint32_t instance, uint32_t flags, const uint8_t *entries, uint32_t *out, uint32_t outer)
{
    CHECK(instance == ADDRESS(device_objects[0]));
    event(39); event(flags); event(outer); event(*out);
    memcpy(device_palette_entries, entries, sizeof device_palette_entries);
    for (uint32_t index = 0; index < sizeof device_palette_entries; index++) event(entries[index]);
    device_palette_calls++;
    if (device_factory_success) *out = ADDRESS(device_objects[3]);
    return device_factory_success ? 0 : 0x88760001;
}

static uint32_t STDCALL device_set_state(uint32_t instance, uint32_t state, uint32_t value)
{
    CHECK(instance == ADDRESS(device_objects[1]));
    event(40); event(state); event(value); event(WORD(images[side], 0x28b88));
    device_state_calls++;
    return device_render_result;
}

static void run_device_palettes(void)
{
    static const uint16_t brightnesses[] = {0,1,255,256,0x7fff,0x8000,0xff00,0xffff};
    static const uint16_t intensities[] = {0,99,100,101,1024,0x7fff,0xffff};
    static const uint16_t depths[] = {0,0x1c51,0x1c52,0x1c53,0x7cff,0x7d00,0xffff};
    static const uint32_t angles[] = {0,1,0x10000000,0x30000000,0x40000000,0x70000000,0x80000000,0xb0000000,0xc0000000,0xffffffff};
    static const uint32_t formats[] = {8,16,32,0};
    uint32_t variant, index, pass, format, result;
    palette_reset();
    layout((uint16_t)(scenario % 24));
    for (index = 0; index < 5; index++) {
        memset(device_vtables[index], 0, sizeof device_vtables[index]);
        device_objects[index][0] = ADDRESS(device_vtables[index]); device_objects[index][1] = index + 1;
        device_vtables[index][2] = ADDRESS(device_release);
    }
    device_vtables[0][5] = ADDRESS(device_create_palette); device_vtables[1][23] = ADDRESS(device_set_state);
    for (variant = 0; variant < 28; variant++) {
        format = formats[variant % 4];
        WORD(images[side], 0x28940) = variant ? ADDRESS(device_objects[1]) : 0;
        WORD(images[side], 0x28aec) = format;
        WORD(images[side], 0x289f0) = ADDRESS(device_objects[0]);
        WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
        WORD(images[side], 0x289f4) = variant & 5 ? ADDRESS(device_objects[2]) : 0;
        WORD(images[side], 0x289ec) = 0x12345; WORD(images[side], 0x289d4) = 0x23456; WORD(images[side], 0x5f40) = 0x34567;
        WORD(images[side], 0x5f24) = variant & 1;
        WORD(images[side], 0x28b88) = 0xdeadbeef;
        WORD(images[side], 0x5f20) = 0xf00dcafe;
        GLOBAL_HALF(0xcba) = brightnesses[(scenario + variant) % 8];
        GLOBAL_HALF(0x12a0) = intensities[(scenario + variant * 2) % 7];
        GLOBAL_HALF(0xe88) = depths[(scenario * 3 + variant) % 7];
        WORD(global, 0x5d0) = angles[(scenario + variant) % 10];
        global[0x238] = (uint8_t)((scenario + variant) % 24);
        GLOBAL_HALF(0xcbe) = variant % 3;
        GLOBAL_HALF(0xcbc) = scenario % 65;
        for (index = 0; index < 28; index++) WORD(images[side], 0x5e10 + index * 4) = random_word();
        for (index = 0; index < 9; index++) WORD(images[side], 0x8800 + index * 4) = 100 + index;
        for (index = 0; index < 8; index++) WORD(images[side], 0x8820 + index * 4) = 0xaabbccdd;
        for (index = 0; index < 200; index++) {
            uint32_t *texture = (uint32_t *)(images[side] + POOL + index * 0x34);
            memset(texture, 0, 0x34);
            texture[0] = ADDRESS(supplied); texture[1] = 0x40000 | ((index & 1) ? 0x20000 : 0);
            texture[2] = 0x12345678; texture[7] = index % 9;
            if (index % (scenario % 16 == 0 ? 1 : 23) == 0) texture[3] = ADDRESS(device_objects[4]);
        }
        device_palette_calls = device_state_calls = 0;
        device_factory_success = (scenario + variant) % 3 != 0;
        device_render_result = 0x88760000 | ((scenario + variant) & 255);
        for (pass = 0; pass < 2; pass++) {
            result = call(0x5c, 0, 0, 0);
            CHECK(result == (variant ? WORD(images[side], 0x5e7c) & 0xff000000u : 0));
            CHECK(device_palette_calls == (variant && format == 8 ? pass + 1 : 0));
            CHECK(device_state_calls == (variant & 1 ? 1u : 0u));
            if (variant) for (index = 0; index < 28; index++) {
                uint32_t source = WORD(images[side], 0x5e10 + index * 4), expected = source & 0xff000000u;
                for (uint32_t channel = 0; channel < 3; channel++)
                    expected |= (uint32_t)(((uint64_t)((source >> (channel * 8)) & 255) * GLOBAL_HALF(0xcba)) / 65536) << (channel * 8);
                CHECK(WORD(images[side], 0x28960 + index * 4) == expected);
            }
            if (variant && format == 8) for (index = 0; index < 256; index++) {
                CHECK(device_palette_entries[index * 4] == images[side][0x39c2 + index * 3]);
                CHECK(device_palette_entries[index * 4 + 1] == images[side][0x39c3 + index * 3]);
                CHECK(device_palette_entries[index * 4 + 2] == images[side][0x39c4 + index * 3]);
                CHECK(device_palette_entries[index * 4 + 3] == 0);
            }
        }
    }
}
