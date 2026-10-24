/* Surface/device creation, failure cleanup and the composed setup helpers. */
static uint32_t device_create_entry[2], device_create_failure, device_create_label, device_create_flags;
static uint32_t device_create_soft, device_create_width, device_create_height, device_create_formats;
static uint32_t device_create_events, device_create_null, device_create_existing;
static const char *device_create_labels[] = {"(Direct3D MMX)","(Direct3D RGB)","(Direct3D)","(direct3d)","unknown"};

static void device_create_event(uint32_t stage, uint32_t instance)
{
    event(48); event(stage); event(device_tag(instance)); device_create_events |= 1u << stage;
    for (uint32_t index = 0; index < 6; index++) {
        uint32_t pointer = WORD(images[side], device_interface_offsets[index]);
        event(pointer ? device_tag(pointer) : 0);
    }
}

static uint32_t STDCALL device_create_query(uint32_t instance, const uint32_t *guid, uint32_t *out)
{
    uint32_t stage = instance == ADDRESS(device_objects[0]) ? 1 : 3;
    device_create_event(stage, instance);
    for (uint32_t index = 0; index < 4; index++) event(guid[index]);
    if (stage == 3) event(ADDRESS(out) - ADDRESS(images[side]));
    if (device_create_failure == stage) return 0x88760000 | stage;
    *out = ADDRESS(device_objects[stage == 1 ? 1 : 3]);
    return 0;
}

static uint32_t STDCALL device_create_get_dd(uint32_t instance, uint32_t *out)
{
    device_create_event(2, instance); event(ADDRESS(out) - ADDRESS(images[side]));
    /* Failed calls may still publish an interface; cleanup must release it. */
    *out = ADDRESS(device_objects[2]);
    return device_create_failure == 2 ? 0x88760002 : 0;
}

static uint32_t STDCALL device_create_addref(uint32_t instance)
{
    device_create_event(12, instance); return 100;
}

static uint32_t STDCALL device_create_host(uint32_t selector, uint8_t *out, uint32_t size)
{
    event(49); event(selector); event(size);
    memset(out, 0, 0xe8); strcpy((char *)out + 0x80, device_create_labels[device_create_label]);
    WORD(out, 0xd0) = device_create_flags;
    return 0x12345678;
}

static uint32_t STDCALL device_create_device(uint32_t instance, const uint32_t *guid, uint32_t surface, uint32_t *out)
{
    device_create_event(4, instance); event(device_tag(surface)); event(ADDRESS(out) - ADDRESS(images[side]));
    for (uint32_t index = 0; index < 4; index++) event(guid[index]);
    *out = device_create_null ? 0 : ADDRESS(device_objects[1]);
    return device_create_failure == 4 ? 0x88760004 : 0;
}

static uint32_t STDCALL device_create_viewport(uint32_t instance, uint32_t *out, uint32_t outer)
{
    device_create_event(5, instance); event(outer); event(ADDRESS(out) - ADDRESS(images[side]));
    *out = ADDRESS(device_objects[4]);
    return device_create_failure == 5 ? 0x88760005 : 0;
}

static uint32_t STDCALL device_create_add_viewport(uint32_t instance, uint32_t viewport)
{
    device_create_event(6, instance); event(device_tag(viewport));
    return device_create_failure == 6 ? 0x88760006 : 0;
}

static uint32_t STDCALL device_create_description(uint32_t instance, uint32_t *out)
{
    device_create_event(13, instance);
    for (uint32_t index = 0; index < 27; index++) event(out[index]);
    out[2] = device_create_height; out[3] = device_create_width;
    return 0x8876dead; /* This method's HRESULT is ignored. */
}

static uint32_t STDCALL device_create_set_viewport(uint32_t instance, const uint32_t *viewport)
{
    device_create_event(7, instance);
    for (uint32_t index = 0; index < 11; index++) event(viewport[index]);
    return device_create_failure == 7 ? 0x88760007 : 0;
}

static uint32_t STDCALL device_create_current_viewport(uint32_t instance, uint32_t viewport)
{
    device_create_event(8, instance); event(device_tag(viewport));
    return device_create_failure == 8 ? 0x88760008 : 0;
}

static uint32_t STDCALL device_create_caps(uint32_t instance, uint32_t *hardware, uint32_t *software)
{
    device_create_event(9, instance); event(ADDRESS(hardware) - ADDRESS(images[side]));
    for (uint32_t index = 0; index < 51; index++) { event(hardware[index]); event(software[index]); }
    for (uint32_t index = 0; index < 51; index++) { hardware[index] = random_word(); software[index] = random_word(); }
    hardware[0] = software[0] = 0xcc;
    hardware[1] = device_create_soft ? 0 : 0x12345678;
    software[1] = device_create_failure == 9 ? 0 : 0x23456789;
    if (device_create_failure == 9) hardware[1] = 0;
    return 0x8876dead; /* Capabilities are inspected despite the HRESULT. */
}

static uint32_t STDCALL device_create_enum(uint32_t instance, uint32_t callback, uint32_t context)
{
    device_create_event(10, instance);
    device_enum_count = device_create_failure == 10 ? 0 : 1;
    return device_enumerate_formats(instance, callback, context);
}

static uint32_t STDCALL device_create_palette_callback(uint32_t instance, uint32_t flags, const uint8_t *entries, uint32_t *out, uint32_t outer)
{
    device_create_event(11, instance); event(flags); event(outer); event(*out);
    for (uint32_t index = 0; index < 1024; index++) event(entries[index]);
    if (device_factory_success) *out = ADDRESS(device_objects[4]);
    return device_factory_success ? 0 : 0x88760011;
}

static void run_device_create(void)
{
    static const uint32_t formats[][8] = {
        {32,0x41,0,16,0x7c00,0x3e0,0x1f,0x8000}, {32,0x20,0,8,0,0,0,0},
    };
    uint32_t variant, index, result, surface, success;
    palette_reset(); layout((uint16_t)(scenario % 24));
    for (index = 0; index < 5; index++) {
        memset(device_vtables[index], 0, sizeof device_vtables[index]);
        device_objects[index][0] = ADDRESS(device_vtables[index]); device_vtables[index][2] = ADDRESS(device_shutdown_release);
    }
    device_vtables[0][0] = device_vtables[2][0] = ADDRESS(device_create_query);
    device_vtables[0][1] = ADDRESS(device_create_addref); device_vtables[0][22] = ADDRESS(device_create_description);
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
        touched = NULL; check_carry = 0;
        result = invoke(device_create_entry[side], surface, 0, 0, 0xaabbccdd, 1); compare(result);
        success = variant < 2 ? 1 : !(device_create_failure && (device_create_events & (1u << device_create_failure))) &&
            (device_create_label < 3 ? !device_create_null : !!device_create_existing);
        /* Indexed-only devices also require the alpha capability and the
         * optional indexed mode; derive their acceptance from the transcript. */
        if (variant >= 2 && success && device_create_formats)
            success = (device_create_flags & 0x400) && (images[side][0x28a84] & 8) ? 1 : 0;
        CHECK(result == success);
        if (variant < 2) CHECK(!device_create_events);
        if (!result) for (index = 0; index < 6; index++) CHECK(!WORD(images[side], device_interface_offsets[index]));
        if (result && variant >= 2) CHECK(WORD(images[side], 0x28cc8) == device_create_soft);
    }
}
