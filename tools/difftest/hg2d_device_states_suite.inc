/* Device initialization queries and both cached state setters. */
static uint32_t device_states_entry[2], device_state_entry[2], device_draw_state_entry[2];
static uint32_t device_query_values[64], device_query_mode, device_query_calls, device_set_calls;
static uint32_t device_state_result_offset;

static uint32_t STDCALL device_query_state(uint32_t instance, uint32_t state, uint32_t *out)
{
    CHECK(instance == ADDRESS(device_objects[1]) && state >= 1 && state < 64);
    CHECK(ADDRESS(out) == ADDRESS(images[side] + 0x28b00 + state * 4));
    event(41); event(state); event(*out);
    device_query_calls++;
    if (device_query_mode != 2 && (device_query_mode != 1 || (state & 1))) *out = device_query_values[state];
    return device_query_mode ? 0x88760100 + state : 0;
}

static uint32_t STDCALL device_cached_state(uint32_t instance, uint32_t state, uint32_t value)
{
    CHECK(instance == ADDRESS(device_objects[1]));
    event(WORD(images[side], 0x28b00 + state * 4));
    event(42); event(state); event(value);
    event(WORD(images[side], device_state_result_offset));
    device_set_calls++;
    return device_render_result ^ state ^ value;
}

static void device_states_check(uint32_t entry, uint32_t first, uint32_t second, uint32_t arguments_count)
{
    uint32_t arguments[2] = {first, second};
    touched = NULL; check_carry = 0;
    draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
    invoke_draw(entry, arguments, arguments_count, 0xaabbccdd);
    draw_active = 0;
    event(draw_fault_code); compare(0);
    CHECK(!draw_fault_code);
    CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
}

static void run_device_states(void)
{
    static const uint32_t defaults[][2] = {{7,0},{14,0},{9,1},{22,1},{29,0},{28,0},{26,0},{4,1},{1,0},{21,2},{17,0},{18,0},{3,1},{27,0},{41,1},{29,0},{39,0},{19,5},{20,6}};
    uint32_t variant, state, mode, calls, value;
    palette_reset();
    memset(device_vtables[1], 0, sizeof device_vtables[1]);
    device_objects[1][0] = ADDRESS(device_vtables[1]);
    device_vtables[1][22] = ADDRESS(device_query_state); device_vtables[1][23] = ADDRESS(device_cached_state);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    device_render_result = 0x88760000 | (scenario & 65535);
    for (variant = 0; variant < 12; variant++) {
        uint32_t flags = variant & 1 ? 0xffffffff : 0xffffffdf;
        device_query_mode = variant % 3; device_query_calls = device_set_calls = 0;
        device_state_result_offset = 0x5f20;
        WORD(images[side], 0x5f20) = 0xaabbccdd; WORD(images[side], 0x5f34) = 0x12345678;
        WORD(images[side], 0x289e4) = random_word(); WORD(images[side], 0x289e8) = random_word();
        for (state = 0; state < 64; state++) {
            WORD(images[side], 0x28b00 + state * 4) = random_word();
            device_query_values[state] = random_word();
        }
        /* Half the fixtures already have every requested default, so failed
         * queries and cache hits must not replace the HRESULT scratch. */
        if (variant >= 6) for (state = 0; state < sizeof defaults / sizeof defaults[0]; state++) {
            uint32_t id = defaults[state][0], expected = defaults[state][1];
            if (id == 26) expected = !!(flags & 0x20);
            if (id == 17) expected = WORD(images[side], 0x289e4);
            if (id == 18) expected = WORD(images[side], 0x289e8);
            WORD(images[side], 0x28b00 + id * 4) = device_query_values[id] = expected;
        }
        device_states_check(device_states_entry[side], flags, 0, 1);
        CHECK(device_query_calls == 63);
        CHECK(variant < 6 ? device_set_calls > 0 : device_set_calls == 0);
        CHECK(WORD(images[side], 0x5f34) == 0x12345678);
        for (state = 0; state < sizeof defaults / sizeof defaults[0]; state++) {
            uint32_t id = defaults[state][0], expected = defaults[state][1];
            if (id == 26) expected = !!(flags & 0x20);
            if (id == 17) expected = WORD(images[side], 0x289e4);
            if (id == 18) expected = WORD(images[side], 0x289e8);
            CHECK(WORD(images[side], 0x28b00 + id * 4) == expected);
        }
    }
    /* Includes both ends of the cache and arbitrary full-width values. */
    for (mode = 0; mode < 2; mode++) for (state = 0; state < 64; state++) {
        uint32_t entry = mode ? device_draw_state_entry[side] : device_state_entry[side];
        device_state_result_offset = mode ? 0x5f34 : 0x5f20;
        value = random_word(); calls = device_set_calls;
        WORD(images[side], 0x28b00 + state * 4) = value;
        device_states_check(entry, state, value, 2);
        CHECK(device_set_calls == calls);
        device_states_check(entry, state, value ^ 0x80000001, 2);
        CHECK(device_set_calls == calls + 1);
        CHECK(WORD(images[side], device_state_result_offset) == (device_render_result ^ state ^ (value ^ 0x80000001)));
        device_states_check(entry, state, value ^ 0x80000001, 2);
        CHECK(device_set_calls == calls + 1);
    }
}
