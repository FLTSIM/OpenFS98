/* Buffered draw submission: all modes, cache hits and x87 precision changes. */
static uint32_t device_flush_entry[2], device_draw_calls, device_draw_result, device_flush_mutate_mode;
static uint32_t STDCALL device_flush_state(uint32_t instance, uint32_t state, uint32_t value)
{
    uint32_t result = device_cached_state(instance, state, value);
    if (device_flush_mutate_mode && state != 28 && state != 29) WORD(images[side], 0x8850) = 9;
    return result;
}

static uint32_t STDCALL device_draw_primitive(uint32_t instance, uint32_t kind, uint32_t vertex_type, const uint32_t *vertices, uint32_t count, uint32_t flags)
{
    uint16_t control;
    CHECK(instance == ADDRESS(device_objects[1]));
    event(47); event(kind); event(vertex_type); event(ADDRESS(vertices) - ADDRESS(images[side])); event(count); event(flags);
    __asm__ volatile("fnstcw %0" : "=m"(control)); event(control);
    if (count <= 17) for (uint32_t index = 0; index < count * 8; index++) event(vertices[index]);
    for (uint32_t index = 0; index < 64; index++) event(WORD(images[side], 0x28b00 + index * 4));
    for (uint32_t index = 0; index < 8; index++) event(WORD(images[side], 0x8840 + index * 4) == ADDRESS(images[side] + POOL) ? 0x20797f60 : WORD(images[side], 0x8840 + index * 4));
    device_draw_calls++;
    return device_draw_result;
}

static void run_device_flush(void)
{
    static const uint16_t precisions[] = {0,0x200,0x300};
    static const uint32_t counts[] = {0,1,2,3,17,0xffffffff};
    uint16_t saved_control, control, after;
    uint32_t variant, index, mode, count, calls;
    palette_reset(); device_flush_compare = 1;
    memset(device_vtables[1], 0, sizeof device_vtables[1]);
    device_objects[1][0] = ADDRESS(device_vtables[1]); device_vtables[1][23] = ADDRESS(device_flush_state);
    device_vtables[1][29] = ADDRESS(device_draw_primitive);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    device_state_result_offset = 0x5f34; device_render_result = 0x8876abcd; device_draw_result = 0x8876fedc;
    __asm__ volatile("fnstcw %0" : "=m"(saved_control));

    for (variant = 0; variant < 240; variant++) {
        device_flush_mutate_mode = variant & 128;
        mode = variant % 12; count = counts[(scenario + variant / 12) % 6];
        WORD(images[side], 0x5ee0) = ADDRESS(images[side] + 0x8860 + 8);
        WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860) + count * 32 + (variant & 31);
        WORD(images[side], 0x5ee8) = 0x34567890;
        WORD(images[side], 0x8850) = mode;
        WORD(images[side], 0x8840) = 0xaabbccdd;
        WORD(images[side], 0x8844) = variant & 1 ? 0 : 0x80000000;
        WORD(images[side], 0x884c) = variant & 2 ? 0 : 0x80000000;
        WORD(images[side], 0x8858) = variant & 4 ? 0x01000000 : 0x00123456;
        WORD(images[side], 0x885c) = variant & 8 ? ADDRESS(images[side] + POOL) : 0;
        WORD(images[side], POOL + 4) = variant & 16 ? 0x100000 : 0;
        WORD(images[side], 0x5f24) = variant & 32;
        WORD(images[side], 0x289e4) = (scenario + variant) % 4 + 1;
        WORD(images[side], 0x289e8) = (scenario + variant) % 6 + 1;
        for (index = 0; index < 64; index++) WORD(images[side], 0x28b00 + index * 4) = variant & 64 ? 0 : random_word();
        WORD(images[side], 0x5f20) = 0x12345678; WORD(images[side], 0x5f34) = 0x23456789;
        for (index = 0; index < 17 * 8; index++) WORD(images[side], 0x8860 + index * 4) = random_word();
        control = (saved_control & ~0xf00u) | precisions[(scenario + variant) % 3] | (((scenario + variant / 3) % 4) << 10);
        __asm__ volatile("fldcw %0" : : "m"(control));
        calls = device_draw_calls = device_set_calls = 0;
        {
            uint32_t arguments[1] = {0};
            touched = NULL; check_carry = 0;
            draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
            invoke_draw(device_flush_entry[side], arguments, 0, 0xaabbccdd);
            draw_active = 0;
            __asm__ volatile("fnstcw %0" : "=m"(after));
            __asm__ volatile("fldcw %0" : : "m"(saved_control));
            event(draw_fault_code); event(after); compare(0);
            CHECK(!draw_fault_code);
            CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
        }
        CHECK(after == ((control & ~0x100u) | 0x200));
        CHECK(WORD(images[side], 0x5f20) == 0x12345678);
        if (count && mode >= 1 && mode <= 8) calls = 1;
        CHECK(device_draw_calls == calls);
        if (count) {
            CHECK(!WORD(images[side], 0x8840) && !WORD(images[side], 0x8850) && !WORD(images[side], 0x885c));
            CHECK(WORD(images[side], 0x5ee8) == 0x1000);
        }
    }
    device_flush_compare = 0;
}
