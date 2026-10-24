/* Compose allocation, staging upload, texture interfaces and handle lookup. */
static uint32_t device_prepare_entry[2], device_texture_entry[2], device_batch_entry[2], prepare_interfaces[2], prepare_vtables[2][6];
static uint32_t prepare_factory_fail, prepare_stage_fail, prepare_lost, prepare_main, prepare_handle_calls;
static uint32_t prepare_result, prepare_handle, prepare_load_calls;
static uint32_t prepare_tag(uint32_t instance)
{
    for (uint32_t index = 0; index < 2; index++) if (instance == ADDRESS(prepare_interfaces + index)) return index;
    CHECK(0); return 0;
}

static uint32_t STDCALL prepare_factory(uint32_t instance, const uint32_t *description, uint32_t *out, uint32_t outer)
{
    uint32_t stage = !(description[26] & 0x4000);
    event(57); event(device_tag(instance)); event(outer);
    for (uint32_t index = 0; index < 27; index++) event(description[index]);
    if (stage ? prepare_stage_fail : prepare_factory_fail) return 0x88760001;
    {
        uint32_t level = 0;
        while (level < 8 && (256u >> level) != description[3]) level++;
        upload_levels[stage ? 2 : 0] = level;
    }
    *out = ADDRESS(upload_objects + (stage ? 2 : 0)); return 0;
}

static uint32_t STDCALL prepare_palette(uint32_t instance, uint32_t palette)
{
    event(58); event(upload_tag(instance)); event(device_tag(palette)); return 0x88760002;
}

static uint32_t STDCALL prepare_query(uint32_t instance, const uint32_t *guid, uint32_t *out)
{
    uint32_t tag = upload_tag(instance), interface_index = tag == 2 ? 1 : 0;
    event(59); event(tag);
    for (uint32_t index = 0; index < 4; index++) event(guid[index]);
    if (!interface_index) prepare_main = tag;
    *out = ADDRESS(prepare_interfaces + interface_index);
    return 0x88760003; /* QueryInterface status is ignored when it publishes. */
}

static uint32_t STDCALL prepare_is_lost(uint32_t instance)
{
    event(60); event(upload_tag(instance)); return prepare_lost ? 0x88760004 : 0;
}

static uint32_t STDCALL prepare_colour_key(uint32_t instance, uint32_t flags, const uint32_t *key)
{
    event(61); event(upload_tag(instance)); event(flags); event(key != NULL);
    if (key) { event(key[0]); event(key[1]); }
    return 0x88760005;
}

static uint32_t STDCALL prepare_surface_release(uint32_t instance)
{
    uint32_t result = upload_release(instance);
    event(WORD(images[side], 0x289ec)); event(WORD(images[side], 0x8820));
    return result;
}

static uint32_t STDCALL prepare_interface_release(uint32_t instance)
{
    event(62); event(prepare_tag(instance)); return 17;
}

static uint32_t STDCALL prepare_load(uint32_t instance, uint32_t source)
{
    event(63); event(prepare_tag(instance)); event(prepare_tag(source)); prepare_load_calls++;
    {
        uint32_t destination[27], source_description[27];
        upload_description(destination, prepare_main); upload_description(source_description, 2);
        uint32_t bytes = upload_format == 0 ? 1 : upload_format == 4 ? 4 : 2;
        for (uint32_t y = 0; y < destination[2]; y++)
            memcpy((void *)(uintptr_t)(destination[9] + y * destination[4]),
                   (const void *)(uintptr_t)(source_description[9] + y * source_description[4]), destination[3] * bytes);
    }
    return 0x88760006;
}

static uint32_t STDCALL prepare_get_handle(uint32_t instance, uint32_t device, uint32_t *out)
{
    event(64); event(prepare_tag(instance)); event(device_tag(device)); event(ADDRESS(out) - ADDRESS(images[side])); event(*out);
    prepare_handle_calls++; *out = prepare_handle; return prepare_result;
}

static void run_device_prepare_mode(uint32_t selector)
{
    static const uint32_t keys[] = {0,1,25,26,152,255,256,0xffffffff};
    uint32_t variant, index, surface, pool = POOL + (scenario % 200) * 0x34;
    palette_reset(); upload_objects_init();
    for (index = 0; index < 4; index++) {
        upload_vtables[index][0] = ADDRESS(prepare_query); upload_vtables[index][27] = ADDRESS(prepare_is_lost);
        upload_vtables[index][29] = ADDRESS(prepare_colour_key); upload_vtables[index][31] = ADDRESS(prepare_palette);
        upload_vtables[index][2] = ADDRESS(prepare_surface_release);
    }
    for (index = 0; index < 2; index++) {
        memset(prepare_vtables[index], 0, sizeof prepare_vtables[index]);
        prepare_interfaces[index] = ADDRESS(prepare_vtables[index]);
        prepare_vtables[index][2] = ADDRESS(prepare_interface_release); prepare_vtables[index][3] = ADDRESS(prepare_get_handle);
        prepare_vtables[index][5] = ADDRESS(prepare_load);
    }
    memset(device_vtables[2], 0, sizeof device_vtables[2]); device_objects[2][0] = ADDRESS(device_vtables[2]);
    device_objects[1][0] = ADDRESS(device_vtables[1]); device_objects[4][0] = ADDRESS(device_vtables[4]);
    device_vtables[2][6] = ADDRESS(prepare_factory);
    for (index = 0; index < 768; index++) images[side][0x39c2 + index] = (uint8_t)random_word();
    for (variant = 0; variant < 24; variant++) {
        uint32_t expected = variant % 4 != 0;
        upload_format = (scenario + variant) % 5; upload_start = (scenario + variant / 5) % 4;
        upload_count = 1; upload_self = 0; upload_negative = (scenario + variant) & 1; upload_fail = variant == 21 ? 1 : 0;
        upload_describes = 0; memset(upload_locks, 0, sizeof upload_locks); memset(upload_releases, 0, sizeof upload_releases);
        prepare_factory_fail = variant == 14; prepare_stage_fail = variant == 15; prepare_lost = variant == 13;
        prepare_handle_calls = prepare_load_calls = 0; prepare_result = 0x88765555;
        prepare_handle = variant & 1 ? 0x12345678 : 0;
        for (surface = 0; surface < 4; surface++) {
            upload_levels[surface] = upload_start; upload_caps[surface] = variant & 8 && surface < 2 ? 0x4000 : 0;
            upload_seed(upload_pixels[surface], UPLOAD_BYTES, 1, surface + variant);
        }
        upload_seed(mip_build_heap, sizeof mip_build_heap, 17, scenario + variant);
        memset(images[side] + POOL, 0, 0x8800 - POOL);
        WORD(images[side], pool) = expected ? ADDRESS(mip_build_heap) : 0;
        WORD(images[side], pool + 4) = 0x40000; WORD(images[side], pool + 8) = 0xaabbccdd;
        WORD(images[side], pool + 12) = variant % 4 == 1 || variant % 4 == 2 ? ADDRESS(upload_objects) : 0;
        WORD(images[side], pool + 28) = upload_start + (variant % 4 == 2 ? 1 : 0);
        WORD(images[side], pool + 24) = keys[(scenario + variant) % 8];
        for (index = 0; index < 9; index++) WORD(images[side], 0x8800 + index * 4) = 100;
        for (index = 0; index < 8; index++) WORD(images[side], 0x8820 + index * 4) = 0;
        WORD(images[side], 0x289ec) = WORD(images[side], 0x289d4) = 0x12345678;
        WORD(images[side], 0x289f0) = ADDRESS(device_objects[2]); WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
        WORD(images[side], 0x289f4) = variant & 1 ? ADDRESS(device_objects[4]) : 0;
        WORD(images[side], 0x28cc8) = WORD(images[side], 0x5f28) = 0;
        {
            uint32_t description[27]; upload_description(description, 0);
            memcpy(images[side] + 0x28ae0, description + 18, 32);
        }
        if (!selector) device_states_check(device_prepare_entry[side], ADDRESS(images[side] + pool), upload_start, 2);
        else {
            static const uint32_t distances[] = {0,0xbb7ff,0xbb800,0xbb801,0x3a97ff,0x3a9800,0x3a9801,0xffffffff};
            static const uint32_t frames[] = {0,1,2,100,0xffffffff};
            static const uint32_t pressures[] = {0,1,30,31,0x80000000};
            static const uint32_t peaks[] = {0,1,0x2aaaaaab,0x80000000,0xffffffff};
            uint32_t frame = frames[(scenario + variant) % 5], distance = distances[(scenario + variant / 3) % 8];
            uint32_t arguments[2] = {ADDRESS(images[side] + pool),keys[(scenario + variant + 3) % 8]}, result;
            WORD(global, 0x2ac) = frame; WORD(global, 0xd4c) = variant & 1 ? distance : distance - 1;
            WORD(images[side], 0x5f48) = variant & 2 ? frame : frame - 1;
            WORD(images[side], 0x5f40) = pressures[(scenario + variant / 2) % 5];
            WORD(images[side], 0x5f2c) = (scenario + variant / 4) % 3;
            WORD(images[side], 0x289d4) = peaks[(scenario + variant / 5) % 5];
            WORD(images[side], 0x28cc4) = variant & 4 ? 1 : 0;
            WORD(images[side], pool + 16) = frame - (variant % 3);
            WORD(images[side], pool + 20) = distance;
            WORD(images[side], pool + 4) |= variant & 8 ? 0x100 : 0;
            if (variant % 6 == 0) { WORD(images[side], pool + 24) = 1; arguments[1] = 25; }
            if (variant % 6 == 1) { WORD(images[side], pool + 24) = 25; arguments[1] = 1; }
            if (variant % 6 == 2) { WORD(images[side], pool + 24) = 25; arguments[1] = 0; }
            if (variant % 6 == 3) { WORD(images[side], pool + 24) = 26; arguments[1] = 0; }
            touched = NULL; check_carry = 0;
            draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
            if (selector == 1) {
                result = invoke_draw(device_texture_entry[side], arguments, 2, 0xaabbccdd);
                draw_active = 0; event(draw_fault_code); compare(result);
                CHECK(!draw_fault_code);
                CHECK(result == WORD(images[side], pool + 8));
                CHECK(WORD(images[side], pool + 16) == frame && !(WORD(images[side], pool + 4) & 0x20000));
            } else {
                uint16_t saved_control, control, after;
                uint32_t texture = variant & 1 ? arguments[0] : 0, mode = (scenario + variant) % 10;
                arguments[0] = mode; arguments[1] = texture;
                WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860 + (variant & 2 ? 96 : 0));
                WORD(images[side], 0x5ee0) = ADDRESS(images[side] + 0x8860 + 32);
                WORD(images[side], 0x5ee8) = 13; WORD(images[side], 0x8850) = 1; WORD(images[side], 0x8840) = 0xaabbccdd;
                WORD(images[side], 0x885c) = ADDRESS(images[side] + pool);
                GLOBAL_HALF(0xca0) = (uint16_t)keys[(scenario + variant) % 8];
                device_vtables[1][23] = ADDRESS(device_cached_state); device_vtables[1][29] = ADDRESS(device_draw_primitive);
                device_state_result_offset = 0x5f34; device_render_result = 0x88760000; device_draw_result = 0x8876ffff;
                __asm__ volatile("fnstcw %0" : "=m"(saved_control));
                control = (saved_control & ~0xf00u) | ((variant % 3 == 0 ? 0 : variant % 3 == 1 ? 0x200 : 0x300)) | ((variant % 4) << 10);
                __asm__ volatile("fldcw %0" : : "m"(control));
                device_flush_compare = 1;
                invoke_draw(device_batch_entry[side], arguments, 2, 0xaabbccdd);
                draw_active = 0;
                __asm__ volatile("fnstcw %0" : "=m"(after));
                __asm__ volatile("fldcw %0" : : "m"(saved_control));
                event(draw_fault_code); event(after); compare(0); device_flush_compare = 0;
                CHECK(!draw_fault_code && after == (control & ~0x300u));
                CHECK(WORD(images[side], 0x5ee8) == 0x1000 && WORD(images[side], 0x8850) == mode && WORD(images[side], 0x885c) == texture);
                CHECK(WORD(images[side], 0x8840) == (texture ? WORD(images[side], pool + 8) : 0));
            }
        }
        upload_compare_pixels(variant);
        if (!selector) {
            if (prepare_lost || prepare_factory_fail || (prepare_stage_fail && (upload_caps[0] & 0x4000))) expected = 0;
            CHECK(prepare_handle_calls == expected);
            if (prepare_handle_calls) CHECK(WORD(images[side], pool + 8) == prepare_handle && WORD(images[side], 0x5f3c) == prepare_result);
        }
    }
}

static void run_device_prepare(void) { run_device_prepare_mode(0); }
static void run_device_texture(void) { run_device_prepare_mode(1); }
static void run_device_batch(void) { run_device_prepare_mode(2); }
