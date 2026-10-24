/* Surface descriptors, cached failures, allocation accounting and reuse. */
static uint32_t device_surface_entry[2], device_allocate_entry[2], device_reuse_entry[2];
static uint32_t device_surface_fail, device_surface_calls, device_surface_palette_calls, device_surface_modify;

static uint32_t STDCALL device_surface_factory(uint32_t instance, uint32_t *description, uint32_t *out, uint32_t outer)
{
    event(50); event(device_tag(instance)); event(outer); event(*out);
    for (uint32_t index = 0; index < 27; index++) event(description[index]);
    device_surface_calls++;
    if (device_surface_modify) description[21] = description[21] == 8 ? 16 : 8;
    if (!device_surface_fail) *out = ADDRESS(device_objects[0]);
    return device_surface_fail ? 0x88761234 : 0;
}

static uint32_t STDCALL device_surface_palette(uint32_t instance, uint32_t palette)
{
    event(51); event(device_tag(instance)); event(device_tag(palette)); device_surface_palette_calls++;
    return 0x88764321;
}

static uint32_t device_surface_check(uint32_t width, uint32_t height, uint32_t level, uint32_t flags)
{
    uint32_t arguments[5] = {width,height,level,flags,ADDRESS(images[side] + 0x28040)}, result;
    touched = NULL; check_carry = 0;
    draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
    result = invoke_draw(device_surface_entry[side], arguments, 5, 0xaabbccdd);
    draw_active = 0; event(draw_fault_code); compare(result);
    CHECK(!draw_fault_code);
    CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
    return result;
}

static void run_device_surfaces(void)
{
    static const uint32_t levels[] = {0,1,7,8,9,30,31,32,33,63,0xffffffff};
    static const uint32_t dimensions[] = {0,1,255,256,257,0x7fffffff,0x80000000,0xffffffff};
    uint32_t variant, index, level, result, target, calls, total;
    palette_reset();
    for (index = 0; index < 5; index++) { device_objects[index][0] = ADDRESS(device_vtables[index]); memset(device_vtables[index], 0, sizeof device_vtables[index]); }
    device_vtables[2][6] = ADDRESS(device_surface_factory); device_vtables[0][31] = ADDRESS(device_surface_palette);
    WORD(images[side], 0x289f0) = ADDRESS(device_objects[2]);
    for (variant = 0; variant < 96; variant++) {
        WORD(images[side], 0x28cc8) = variant & 1;
        WORD(images[side], 0x5f28) = variant & 2;
        WORD(images[side], 0x289f4) = variant & 4 ? ADDRESS(device_objects[4]) : 0;
        for (index = 0; index < 8; index++) WORD(images[side], 0x28ae0 + index * 4) = random_word();
        WORD(images[side], 0x28aec) = variant & 8 ? 8 : 16;
        device_surface_fail = variant & 16; device_surface_modify = variant & 32;
        device_surface_calls = device_surface_palette_calls = 0;
        WORD(images[side], 0x28040) = 0xaabbccdd;
        result = device_surface_check(dimensions[(scenario + variant) % 8], dimensions[(scenario + variant * 3) % 8],
            levels[(scenario + variant) % 11], variant & 64 ? 0 : 0x4000);
        CHECK(result == (device_surface_fail ? 0x88761234u : 0u));
        CHECK(device_surface_calls == 1);
        CHECK(device_surface_palette_calls == (!device_surface_fail && (variant & 4) && (!!(variant & 8) != !!device_surface_modify) ? 1u : 0u));
    }
    device_surface_modify = 0;
    for (variant = 0; variant < 72; variant++) {
        level = variant % 9; target = POOL + ((scenario + variant) % 200) * 0x34;
        device_surface_calls = device_surface_palette_calls = 0; device_surface_fail = variant & 8;
        WORD(images[side], 0x8820 + level * 4) = variant & 16 ? 1 : 0;
        WORD(images[side], 0x8800 + level * 4) = 0xffffffff;
        WORD(images[side], 0x289ec) = variant & 32 ? 0x7fffffff : 0xfffffffe;
        WORD(images[side], 0x289d4) = variant & 32 ? 0x7fffffff : 0x80000000;
        total = WORD(images[side], 0x289ec) + (1u << (((8 - level) * 2) & 31));
        touched = NULL; check_carry = 0;
        result = invoke(device_allocate_entry[side], ADDRESS(images[side] + target), level, 0, 0xaabbccdd, 2); compare(result);
        CHECK(result == (!(variant & 16) && !device_surface_fail ? 1u : 0u));
        CHECK(device_surface_calls == (variant & 16 ? 0u : 1u));
        if (result) {
            CHECK(!WORD(images[side], 0x8800 + level * 4));
            CHECK(WORD(images[side], target + 12) == ADDRESS(device_objects[0]));
            CHECK(WORD(images[side], 0x289ec) == total);
        }
        calls = device_surface_calls;
        result = invoke(device_allocate_entry[side], ADDRESS(images[side] + target), level, 0, 0xaabbccdd, 2); compare(result);
        CHECK(device_surface_calls == (device_surface_fail || (variant & 16) ? calls : calls + 1));
    }
    for (variant = 0; variant < 96; variant++) {
        uint32_t phase = variant % 4, frame = (scenario + variant) % 5;
        level = (scenario + variant / 4) % 9;
        WORD(global, 0x2ac) = frame;
        WORD(images[side], 0x5f40) = variant & 8 ? 7 : 0;
        for (index = 0; index < 200; index++) {
            uint32_t *texture = (uint32_t *)(images[side] + POOL + index * 0x34);
            for (uint32_t field = 0; field < 13; field++) texture[field] = random_word();
            texture[2] = phase == 3 || index % 23 ? 0 : 0xaabb0000 | index;
            texture[3] = 0xccdd0000 | index;
            texture[4] = phase == 2 ? frame - 1 : frame - 2;
            texture[5] = variant & 16 ? 0 : index + 1;
            texture[7] = phase ? level - 1 : level;
        }
        /* A tie at the end must retain the earlier candidate. */
        WORD(images[side], POOL + 199 * 0x34 + 8) = phase == 3 ? 0 : 0xaabb0199;
        WORD(images[side], POOL + 199 * 0x34 + 20) = variant & 16 ? 0 : 185;
        target = variant & 32 ? POOL + 184 * 0x34 : 0x28040;
        device_states_check(device_reuse_entry[side], ADDRESS(images[side] + target), level, 2);
    }
    memset(images[side] + POOL, 0, 0x8800 - POOL);
    WORD(global, 0x2ac) = 3;
    for (index = 1; index < 4; index++) {
        uint32_t *texture = (uint32_t *)(images[side] + POOL + index * 0x34);
        texture[2] = 100 + index; texture[3] = 200 + index;
        texture[4] = 0; texture[5] = index == 3 ? 100 : 10; texture[7] = index == 3 ? 1 : 2;
    }
    device_states_check(device_reuse_entry[side], ADDRESS(images[side] + 0x28040), 2, 2);
    CHECK(WORD(images[side], 0x28048) == 101 && WORD(images[side], 0x2804c) == 201 && WORD(images[side], 0x2805c) == 2);
    CHECK(!WORD(images[side], POOL + 0x34 + 8));
    WORD(images[side], POOL + 0x34 + 8) = 101;
    WORD(images[side], POOL + 0x34 + 12) = 201;
    WORD(images[side], POOL + 0x34 + 16) = 1;
    WORD(images[side], POOL + 0x34 + 28) = 2;
    WORD(images[side], POOL + 2 * 0x34 + 16) = 2;
    WORD(images[side], POOL + 2 * 0x34 + 20) = 100;
    device_states_check(device_reuse_entry[side], ADDRESS(images[side] + 0x28040), 2, 2);
    CHECK(WORD(images[side], 0x28048) == 101);
    WORD(images[side], POOL + 2 * 0x34 + 16) = 0;
    device_states_check(device_reuse_entry[side], ADDRESS(images[side] + POOL + 2 * 0x34), 2, 2);
    CHECK(!WORD(images[side], POOL + 2 * 0x34 + 8) && !WORD(images[side], POOL + 2 * 0x34 + 12));
}
