/* File and FE boundaries model the sizes used by retail FE +0x1c. */
static uint32_t resource_fe_success, resource_haze_success, resource_open_success, resource_read_size;
static uint8_t resource_name[64];

static void resource_string(const uint8_t *name)
{
    uint32_t length = 0;
    do { event(name[length]); CHECK(length < 63); } while (name[length++]);
}

static uint32_t STDCALL resource_open(uint32_t *out, const uint8_t *name, uint32_t flags, uint32_t reserved)
{
    CHECK(ADDRESS(out) == ADDRESS(images[side] + 0x3fe6));
    CHECK(flags == 9 && reserved == 0);
    event(30); resource_string(name); event(flags); event(reserved);
    if (resource_open_success) *out = 0x12345678;
    return resource_open_success ? 0 : 1;
}

static uint32_t STDCALL resource_read(uint32_t file, uint8_t *out, uint32_t size, uint32_t *read_size)
{
    CHECK(ADDRESS(out) == ADDRESS(images[side] + 0x30a0) && size == 0x15c);
    CHECK(ADDRESS(read_size) == ADDRESS(images[side] + 0x3fea));
    event(31); event(file); event(size);
    for (uint32_t index = 0; index < resource_read_size; index++)
        out[index] = (uint8_t)(index * 11 + scenario * 3);
    *read_size = resource_read_size;
    return resource_read_size == size ? 0 : 1;
}

static uint32_t STDCALL resource_close(uint32_t file)
{
    event(32); event(file);
    return 0x13579bdf;
}

static uint32_t STDCALL resource_fe_load(uint8_t *out, const uint8_t *name, uint32_t selector, uint32_t kind)
{
    uint32_t size = kind == 9 ? 0xc0 : 0x1000;
    CHECK(kind == 9 || kind == 10);
    CHECK(ADDRESS(out) == ADDRESS(images[side] + (kind == 9 ? 0x32e0 : 0xd70)));
    event(33); event(ADDRESS(out) - ADDRESS(images[side])); resource_string(name); event(selector); event(kind);
    if (!(kind == 9 ? resource_fe_success : resource_haze_success)) return 0;
    for (uint32_t index = 0; index < size; index++)
        out[index] = (uint8_t)(index * 17 + (index >> 8) * 13 + selector + scenario);
    if (kind == 10) GLOBAL_HALF(0xcd2) = 0;
    return 1;
}

static void run_resources(void)
{
    static const uint16_t fogs[] = {0,1,3999,4000,23999,24000,28000,0x7fff,0x8000,0xffff};
    uint32_t index, result;
    palette_reset();
    resource_compare = 1;
    WORD(images[side], 0x1c) = ADDRESS(fe);
    fe[0x1c / 4] = ADDRESS(resource_fe_load);
    host[0x17c / 4] = ADDRESS(resource_open);
    host[0x19c / 4] = ADDRESS(resource_read);
    host[0x13c / 4] = ADDRESS(resource_close);
    layout((uint16_t)(scenario % 24));
    GLOBAL_HALF(0xcba) = 0x7f00 | (scenario & 255);
    GLOBAL_HALF(0xcbc) = scenario % 65;
    GLOBAL_HALF(0xe88) = fogs[scenario % 10];
    resource_fe_success = resource_haze_success = 1;
    resource_open_success = scenario & 1;
    resource_read_size = scenario % 3 == 0 ? 0 : scenario % 3 == 1 ? 0x15c : 137;
    call(0x58, 0, 0, 0);
    call(0x58, 0, 0, 0);
    for (index = 0; index < 12; index++) {
        uint32_t length = 1 + (scenario + index) % 7;
        for (uint32_t letter = 0; letter < length; letter++) resource_name[letter] = 'a' + (letter + index) % 26;
        memcpy(resource_name + length, index & 1 ? ".a.b" : ".pal", 5);
        resource_fe_success = index % 4 != 0;
        resource_haze_success = index % 3 != 0;
        GLOBAL_HALF(0xcd2) = 0x4321;
        GLOBAL_HALF(0xe88) = fogs[(scenario + index) % 10];
        result = call(0x68, ADDRESS(resource_name), index ^ scenario, 0);
        CHECK(result == resource_fe_success);
        if (resource_fe_success) {
            CHECK(!memcmp(images[side] + 0x3fd8, resource_name, length + 1));
            CHECK(!memcmp(images[side] + 0x3fd8 + length + 1, "haz", 4));
            CHECK(GLOBAL_HALF(0xcd2) == (resource_haze_success ? 0 : 0x4321));
        }
    }
    resource_fe_success = resource_haze_success = 1;
    HALF(images[side], 0x34c0 + 200 * 2) = 0x123;
    call(0x68, ADDRESS(resource_name), scenario, 0);
    /* Identity words and grey/RGB boundaries independently expose neutral
     * classification, its darkness cutoff and its inclusive ramp search. */
    for (index = 0; index < 256; index++) {
        HALF(images[side], 0x34c0 + index * 2) = (uint16_t)index;
        images[side][0x39c2 + index * 3] = (uint8_t)index;
        images[side][0x39c3 + index * 3] = (uint8_t)index;
        images[side][0x39c4 + index * 3] = (uint8_t)index;
    }
    images[side][0x39c5] = 3; images[side][0x39c6] = 3; images[side][0x39c7] = 4;
    images[side][0x39c8] = 4; images[side][0x39c9] = 3; images[side][0x39ca] = 4;
    images[side][0x39cb] = 130; images[side][0x39cc] = images[side][0x39cd] = 0;
    images[side][0x39ce] = 131; images[side][0x39cf] = images[side][0x39d0] = 0;
    images[side][0x39d1] = 0; images[side][0x39d2] = 0; images[side][0x39d3] = 130;
    resource_fe_success = resource_haze_success = 1;
    GLOBAL_HALF(0xe88) = 24000;
    call(0x68, ADDRESS(resource_name), scenario, 0);
    for (index = 0; index < 256; index++) {
        uint32_t texel = images[side][0xd70 + index];
        uint32_t brightness = images[side][0x39c2 + texel * 3] + images[side][0x39c3 + texel * 3] + images[side][0x39c4 + texel * 3];
        uint32_t expected = brightness <= 10 ? 0 : brightness <= 96 ? 32 : brightness > 129 ? 43 : (brightness + 2) / 3;
        CHECK(images[side][0x1d8c + index] == expected);
        texel = images[side][0xe70 + index];
        if (texel == 3) CHECK(images[side][0x258c + index] == 40);
        if (texel == 4) CHECK(images[side][0x258c + index] == 4);
    }
    fe[0x1c / 4] = ADDRESS(fe_load);
    resource_compare = 0;
}
