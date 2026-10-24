/* Hardware texture and six-interface teardown, including repeated releases. */
static uint32_t device_shutdown_entry[2], device_shutdown_releases;
static const uint32_t device_interface_offsets[] = {0x28940,0x28ccc,0x289f4,0x289d0,0x28cc0,0x289f0};
static uint32_t STDCALL device_shutdown_release(uint32_t instance)
{
    event(43); event(device_tag(instance)); device_shutdown_releases++;
    for (uint32_t index = 0; index < 6; index++) {
        uint32_t pointer = WORD(images[side], device_interface_offsets[index]);
        event(pointer ? device_tag(pointer) : 0);
    }
    for (uint32_t index = 0; index < 9; index++) event(WORD(images[side], 0x8800 + index * 4));
    event(WORD(images[side], 0x289ec)); event(WORD(images[side], 0x289d4)); event(WORD(images[side], 0x5f40));
    event(WORD(images[side], 0x8820)); event(WORD(images[side], 0x883c));
    return 0x12340000 | device_shutdown_releases;
}

static void run_device_shutdown(void)
{
    uint32_t variant, index, expected;
    palette_reset();
    for (index = 0; index < 5; index++) {
        memset(device_vtables[index], 0, sizeof device_vtables[index]);
        device_objects[index][0] = ADDRESS(device_vtables[index]);
        device_vtables[index][2] = ADDRESS(device_shutdown_release);
    }
    for (variant = 0; variant < 16; variant++) {
        expected = 0; device_shutdown_releases = 0;
        for (index = 0; index < 6; index++) {
            uint32_t enabled = variant < 7 ? index < variant : (variant == 7 ? 0 : 1);
            WORD(images[side], device_interface_offsets[index]) = enabled ? ADDRESS(device_objects[(index + variant) % 5]) : 0;
            expected += enabled;
        }
        for (index = 0; index < 9; index++) WORD(images[side], 0x8800 + index * 4) = 0xfedc0000 + index;
        for (index = 0; index < 8; index++) WORD(images[side], 0x8820 + index * 4) = random_word();
        WORD(images[side], 0x289ec) = 0x45678901; WORD(images[side], 0x289d4) = 0x56789012; WORD(images[side], 0x5f40) = 0x67890123;
        for (index = 0; index < 200; index++) {
            uint32_t *texture = (uint32_t *)(images[side] + POOL + index * 0x34);
            for (uint32_t field = 0; field < 13; field++) texture[field] = random_word();
            texture[7] = index % 9;
            texture[3] = variant >= 7 && (index % (scenario % 16 ? 23 : 1) == 0 || index == 199) ? ADDRESS(device_objects[index % 5]) : 0;
            expected += texture[3] != 0;
        }
        for (index = 0; index < 2; index++) {
            device_states_check(device_shutdown_entry[side], 0, 0, 0);
            CHECK(device_shutdown_releases == expected);
            for (uint32_t slot = 0; slot < 6; slot++) CHECK(!WORD(images[side], device_interface_offsets[slot]));
            for (uint32_t slot = 0; slot < 8; slot++) CHECK(!WORD(images[side], 0x8820 + slot * 4));
            CHECK(!WORD(images[side], 0x289ec) && !WORD(images[side], 0x289d4) && !WORD(images[side], 0x5f40));
        }
    }
}
