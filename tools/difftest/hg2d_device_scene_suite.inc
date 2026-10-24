/* Lazy dispatch caches and public scene/selection entrypoints. */
static uint32_t device_dispatch_entry[2], device_begin_entry[2], device_end_entry[2], device_select_entry[2];
static uint32_t device_scene_record[40], device_scene_view[4], device_scene_descriptor[8];
static void device_scene_map(HMODULE module)
{
    static const uint32_t retail[] = {0x2078ced0,0x2078d060,0x2078cf90,0x2078e2e0,0x2078d070,0x2078d090,
        0x2078d0b0,0x2078d0e0,0x2078d2b0,0x2078da30,0x2078d370,0x2078d7a0,0x2078da30,0x2078da60,
        0x2078da30,0x2078dcd0,0x2078da30,0x2078ded0};
    static const char *names[] = {"hg2d_device_point","hg2d_device_nop","hg2d_device_depth_point","hg2d_device_text",
        "hg2d_device_horizontal","hg2d_device_vertical","hg2d_device_line","hg2d_device_fixed_line","hg2d_device_frame",
        "hg2d_device_commit","hg2d_device_projected_vertex","hg2d_device_uv_vertex","hg2d_device_commit",
        "hg2d_device_intensity_vertex","hg2d_device_commit","hg2d_device_palette_vertex","hg2d_device_commit","hg2d_device_disc"};
    for (uint32_t index = 0; index < 18; index++) {
        device_linkage_map[index][0] = retail[index];
        device_linkage_map[index][1] = ADDRESS(GetProcAddress(module, names[index]));
        CHECK(device_linkage_map[index][1]);
    }
}
static uint32_t STDCALL device_scene_move(uint32_t destination, uint32_t source, uint32_t length)
{
    event(0xd401); event(destination - ADDRESS(images[side])); event(source - ADDRESS(images[side])); event(length);
    CHECK(length == 0xc0 && source == ADDRESS(images[side] + 0x38));
    CHECK(destination == ADDRESS(images[side] + 0x28880) || destination == ADDRESS(images[side] + 0x28c00));
    memmove((void *)(uintptr_t)destination, (const void *)(uintptr_t)source, length);
    for (uint32_t index = 0; index < length; index += 4) event(device_linkage_value(WORD(images[side], source - ADDRESS(images[side]) + index)));
    return 0;
}
static uint32_t device_scene_check(uint32_t entry, uint32_t first, uint32_t second, uint32_t count)
{
    uint32_t arguments[2] = {first,second}, result, index;
    touched = NULL; check_carry = 0; draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
    result = invoke_draw(entry, arguments, count, 0xaabbccdd); draw_active = 0;
    event(draw_fault_code);
    for (index = 0; index < 7; index++) if (index != 0 && index != 2 && index != 3) event(draw_registers[index]);
    for (index = 0; index < 0xc0; index += 4) event(device_linkage_value(WORD(images[side], 0x38 + index)));
    for (index = 0; index < 40; index++) event(device_scene_record[index]);
    for (index = 0; index < 4; index++) event(device_scene_view[index]);
    compare(result); CHECK(!draw_fault_code);
    CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
    return result;
}
static void run_device_scene(void)
{
    static const uint32_t values[] = {0,1,2,3,63,64,0xffffffff,0x80000000,0x7fffffff};
    uint32_t variant, index;
    draw_reset(); device_linkage_compare = device_points_compare = 1;
    host[0x21c / 4] = ADDRESS(device_scene_move);
    memset(device_vtables[1], 0, sizeof device_vtables[1]); device_objects[1][0] = ADDRESS(device_vtables[1]);
    device_vtables[1][10] = ADDRESS(device_text_begin_scene); device_vtables[1][11] = ADDRESS(device_text_end_scene);
    device_vtables[1][23] = ADDRESS(device_cached_state); device_vtables[1][29] = ADDRESS(device_draw_primitive);
    device_state_result_offset = 0x5f20; device_render_result = 0x88760000; device_draw_result = 0x8876ffff;
    for (variant = 0; variant < 160; variant++) {
        memset(device_scene_record, 0, sizeof device_scene_record); memset(device_scene_view, 0, sizeof device_scene_view);
        device_scene_record[7] = ADDRESS(device_scene_view);
        device_scene_record[2] = (variant & 1 ? 0 : 0x4000) | (variant & 2 ? 0x20000000 : 0);
        device_scene_record[0x9c / 4] = ADDRESS(device_objects[0]);
        device_scene_record[0x60 / 4] = values[(scenario + variant) % 9];
        device_scene_record[0x64 / 4] = values[(scenario + variant / 2) % 9];
        device_scene_record[0x30 / 4] = values[(scenario + variant / 3) % 9];
        device_scene_record[0x34 / 4] = values[(scenario + variant / 5) % 9];
        WORD(images[side], 0x28940) = variant % 5 == 1 ? 0 : ADDRESS(device_objects[variant % 5 == 2 ? 3 : 0]);
        WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
        WORD(global, 0x12a4) = variant & 4 ? 0xabcddd00 : 0x10203210;
        WORD(global, 0xd48) = random_word();
        device_text_begin_result = variant & 8 ? 0x88760001 : 0;
        WORD(images[side], 0x5f20) = 0x12345678;
        WORD(images[side], 0x28860) = variant & 16 ? 1 : 0;
        WORD(images[side], 0x5f24) = variant & 32 ? 1 : 0;
        GLOBAL_HALF(0xcba) = (uint16_t)random_word(); GLOBAL_HALF(0x12a0) = 100;
        GLOBAL_HALF(0xe88) = 0x1c52; global[0x238] = 0; WORD(global, 0x5d0) = 0x40000000;
        WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860 + (variant & 64 ? 32 : 0));
        WORD(images[side], 0x8850) = 1; WORD(images[side], 0x885c) = 0;
        device_scene_check(device_begin_entry[side], ADDRESS(device_scene_record), 0, 1);
        if (variant & 32) device_scene_record[2] ^= 0x4000;
        if (variant & 64) WORD(global, 0x12a4) ^= 1;
        device_scene_check(device_end_entry[side], ADDRESS(device_scene_record), 0, 1);
        /* Repeated switches exercise the slot +0x40 identity test. */
        if (variant % 16 == 0) {
            for (index = 0; index < 2; index++)
                device_scene_check(device_dispatch_entry[side], ADDRESS(images[side] + (index ? 0x28c00 : 0x28880)), 0, 1);
        }
    }
    /* Test the full 0xc0 copy, including slots shared by both stock tables. */
    device_scene_check(device_dispatch_entry[side], ADDRESS(images[side] + 0x28c00), 0, 1);
    WORD(images[side], 0x28880 + 0xbc) = 0x12345678;
    device_scene_check(device_dispatch_entry[side], ADDRESS(images[side] + 0x28880), 0, 1);
    device_scene_check(device_dispatch_entry[side], ADDRESS(images[side] + 0x28c00), 0, 1);
    device_linkage_compare = device_points_compare = 0;
}

static uint32_t device_selection_bits, device_selection_caps;
static uint32_t STDCALL device_selection_description(uint32_t instance, uint32_t *out)
{
    event(0xd402); event(device_tag(instance)); event(out[0]); if (!side) CHECK(out[0] == 0x6c);
    /* Only dwSize is initialized by selection before GetSurfaceDesc. */
    memset(out, 0, 0x6c); out[0] = 0x6c; out[2] = device_create_height; out[3] = device_create_width;
    out[21] = device_selection_bits; out[26] = device_selection_caps;
    return 0x8876ffff;
}
static void run_device_selection(void)
{
    static const uint32_t formats[][8] = {
        {32,0x41,0,16,0x7c00,0x3e0,0x1f,0x8000}, {32,0x20,0,8,0,0,0,0},
    };
    uint32_t variant, index, result, surface;
    palette_reset(); layout((uint16_t)(scenario % 24));
    device_linkage_compare = 1; host[0x21c / 4] = ADDRESS(device_scene_move);
    for (index = 0; index < 5; index++) {
        memset(device_vtables[index], 0, sizeof device_vtables[index]);
        device_objects[index][0] = ADDRESS(device_vtables[index]); device_vtables[index][2] = ADDRESS(device_shutdown_release);
    }
    device_vtables[0][0] = device_vtables[2][0] = ADDRESS(device_create_query);
    device_vtables[0][1] = ADDRESS(device_create_addref); device_vtables[0][22] = ADDRESS(device_selection_description);
    device_vtables[1][36] = ADDRESS(device_create_get_dd);
    device_vtables[1][6] = ADDRESS(device_create_add_viewport); device_vtables[1][13] = ADDRESS(device_create_current_viewport);
    device_vtables[1][3] = ADDRESS(device_create_caps); device_vtables[1][9] = ADDRESS(device_create_enum);
    device_vtables[1][22] = ADDRESS(device_query_state); device_vtables[1][23] = ADDRESS(device_cached_state);
    device_vtables[2][5] = ADDRESS(device_create_palette_callback);
    device_vtables[3][8] = ADDRESS(device_create_device); device_vtables[3][6] = ADDRESS(device_create_viewport);
    device_vtables[4][17] = ADDRESS(device_create_set_viewport);
    host[0x31c / 4] = ADDRESS(device_create_host); host[0xa8 / 4] = ADDRESS(device_format_option);
    for (variant = 0; variant < 144; variant++) {
        device_create_failure = variant < 2 ? 0 : (variant - 2) % 11;
        device_create_label = (scenario + variant / 11) % 5;
        device_create_flags = (scenario + variant) & 1 ? 0x1cb0 : 0;
        device_create_soft = (scenario + variant / 2) & 1;
        device_create_width = random_word(); device_create_height = random_word();
        device_create_null = (scenario + variant) % 17 == 0;
        device_create_existing = variant & 16;
        device_create_formats = (scenario + variant / 3) & 1;
        memcpy(device_enum_surfaces[0] + 18, formats[device_create_formats], 32);
        device_enum_reverse = 0; device_enum_result = 0x8876aabb;
        device_option_write = 0; device_option_value = 0;
        device_query_mode = variant % 3; device_query_calls = device_set_calls = 0;
        device_render_result = 0x8876abcd; device_state_result_offset = 0x5f20;
        device_factory_success = variant % 3;
        device_create_events = device_shutdown_releases = 0;
        for (index = 0; index < 6; index++) WORD(images[side], device_interface_offsets[index]) = 0;
        if (device_create_existing) {
            WORD(images[side], 0x28940) = ADDRESS(device_objects[4]);
            WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
        }
        WORD(images[side], 0x289f4) = variant & 8 ? ADDRESS(device_objects[4]) : 0;
        surface = variant == 0 ? 0 : ADDRESS(device_objects[0]);
        if (variant < 2) WORD(images[side], 0x28940) = surface;
        for (index = 0; index < 64; index++) device_query_values[index] = random_word();
        for (index = 0; index < 8; index++) WORD(images[side], 0x8820 + index * 4) = random_word();
        for (index = 0; index < 9; index++) WORD(images[side], 0x8800 + index * 4) = 200 + index;
        WORD(images[side], 0x289ec) = WORD(images[side], 0x289d4) = WORD(images[side], 0x5f40) = 0x12345678;
        for (index = 0; index < 200; index++) {
            uint32_t *texture = (uint32_t *)(images[side] + POOL + index * 0x34);
            memset(texture, 0, 0x34); texture[0] = ADDRESS(supplied); texture[7] = index % 9;
            texture[3] = (index == 199 || index % (scenario % 16 ? 23 : 1) == 0) ? ADDRESS(device_objects[index % 5]) : 0;
        }
        GLOBAL_HALF(0xcba) = (uint16_t)random_word(); GLOBAL_HALF(0x12a0) = 100;
        GLOBAL_HALF(0xe88) = 0x1c52; global[0x238] = 0; WORD(global, 0x5d0) = 0x40000000;
        memset(device_scene_record, 0, sizeof device_scene_record); memset(device_scene_view, 0, sizeof device_scene_view);
        device_scene_record[7] = ADDRESS(device_scene_view); device_scene_record[2] = random_word();
        memset(device_scene_descriptor, 0, sizeof device_scene_descriptor);
        device_scene_descriptor[0] = variant & 1 ? 1 : 2;
        device_scene_descriptor[7] = variant % 7 == 1 ? 0 : surface;
        device_selection_bits = variant % 4 == 0 ? 8 : variant % 4 == 1 ? 16 : variant % 4 == 2 ? 0 : 32;
        device_selection_caps = variant & 4 ? 0x10 : 0;
        WORD(global, 0x12a4) = variant & 8 ? 0xabcdef9f : 0x1234000f;
        global[0x444] = variant & 16 ? 0x80 : 0;
        result = device_scene_check(device_select_entry[side], ADDRESS(device_scene_record),
            variant % 13 == 0 ? 0 : ADDRESS(device_scene_descriptor), 2);
        CHECK(result <= 2);
        if (variant % 13 == 0) CHECK(result == 1 && !(device_scene_record[2] & 0xc000));
        else if (result == 2) {
            CHECK((device_scene_record[2] & 0xc000) == (device_selection_bits > 8 ? 0xc000 : 0x4000));
            CHECK((WORD(global, 0x12a4) & 0x86) == 0x84);
        } else CHECK(!(device_scene_record[2] & 0xc000));
    }
    device_linkage_compare = 0;
}
