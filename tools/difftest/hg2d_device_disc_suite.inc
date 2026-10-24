/* Device discs use UTIL sincos and a stack-based 26-vertex triangle fan. */
static uint32_t device_disc_entry[2], device_disc_imports[16], device_disc_draws;
static uint32_t device_disc_vertex_states[STATE_CAPACITY][26][5];
static uint16_t device_disc_status_states[STATE_CAPACITY];
static uint32_t STDCALL device_disc_sincos(uint32_t angle, int16_t *sine, int16_t *cosine)
{
    typedef uint32_t (STDCALL *sincos_call)(uint32_t, int16_t *, int16_t *);
    uint32_t result;
    event(0xd301); event(angle);
    result = (*(sincos_call *)(uintptr_t)(util_linkage + 0x2c))(angle, sine, cosine);
    event((uint16_t)*sine); event((uint16_t)*cosine);
    return result;
}
static uint32_t STDCALL device_disc_primitive(uint32_t instance, uint32_t kind, uint32_t type,
    const uint32_t *vertices, uint32_t count, uint32_t flags)
{
    if (kind != 6) return device_draw_primitive(instance, kind, type, vertices, count, flags);
    CHECK(instance == ADDRESS(device_objects[1]));
    event(0xd302); event(kind); event(type); event(count); event(flags);
    /* Retail writes five fields and leaves specular/UV stack bytes undefined.
     * Compare every initialized vertex field, never invent padding values. */
    for (uint32_t vertex = 0; vertex < count && vertex < 26; vertex++)
        for (uint32_t field = 0; field < 5; field++) {
            uint32_t value = vertices[vertex * 8 + field];
            if (!side) device_disc_vertex_states[step][vertex][field] = value;
            else if (device_disc_vertex_states[step][vertex][field] != value)
                printf("disc vertex %u field %u differs: %08x/%08x\n", vertex, field, device_disc_vertex_states[step][vertex][field], value);
            event(value);
        }
    for (uint32_t index = 0; index < 64; index++) event(WORD(images[side], 0x28b00 + index * 4));

    device_disc_draws++;
    return device_draw_result;
}
static void run_device_disc(void)
{
    static const uint32_t values[] = {0,1,3,16,32,33,48,63,64,65,0xffff,0x10001,0xffff0001};
    uint32_t variant, index, result;
    uint16_t saved_control, control, after, status;
    draw_reset();
    memset(device_vtables[1], 0, sizeof device_vtables[1]); device_objects[1][0] = ADDRESS(device_vtables[1]);
    device_vtables[1][23] = ADDRESS(device_cached_state); device_vtables[1][29] = ADDRESS(device_disc_primitive);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    device_state_result_offset = 0x5f34; device_render_result = 0x88760000; device_draw_result = 0x8876ffff;
    memset(device_vertex_record, 0, sizeof device_vertex_record); device_vertex_record[7] = ADDRESS(device_vertex_view);
    memset(device_disc_imports, 0, sizeof device_disc_imports); device_disc_imports[0x2c / 4] = ADDRESS(device_disc_sincos);
    WORD(images[side], 0x24) = ADDRESS(device_disc_imports);
    __asm__ volatile("fnstcw %0" : "=m"(saved_control));
    for (variant = 0; variant < 160; variant++) {
        uint32_t arguments[4] = {ADDRESS(device_vertex_record),values[(scenario + variant) % 13],
            values[(scenario + variant * 3) % 13],values[(scenario + variant * 5) % 13]};
        if (variant % 4 == 0) { arguments[1] = variant & 4 ? 0 : 10; arguments[2] = arguments[3] = 24; }
        device_vertex_view[0] = variant & 16 ? 32768 : 32;
        device_vertex_view[2] = variant & 32 ? 0xffff8000 : 0xffffffe8;
        WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860 + (variant & 8 ? 32 : 0));
        WORD(images[side], 0x5ee0) = ADDRESS(images[side] + 0x8860); WORD(images[side], 0x5ee8) = 100;
        WORD(images[side], 0x8850) = 1; WORD(images[side], 0x8840) = 0; WORD(images[side], 0x885c) = 0;
        WORD(images[side], 0x5f24) = variant & 64 ? 0x100 : 0;
        for (index = 0; index < 64; index++) WORD(images[side], 0x28b00 + index * 4) = variant & 128 ? 0 : random_word();
        WORD(global, 0x3c0) = (variant & 64 ? 0x6800 : 0xf000) | (variant & 255);
        GLOBAL_HALF(0xdca) = variant & 1 ? 0x100 : 0;
        GLOBAL_HALF(0xb34) = variant & 2 ? 0x8000 : 0;
        device_disc_draws = 0; touched = NULL; check_carry = 0;
        control = (saved_control & ~0xf00u) | (variant % 3 == 0 ? 0 : variant % 3 == 1 ? 0x200 : 0x300) | ((variant % 4) << 10);
        __asm__ volatile("fldcw %0; fnclex" : : "m"(control));
        draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
        result = invoke_draw(device_disc_entry[side], arguments, 4, 0xaabbccdd); draw_active = 0;
        __asm__ volatile("fnstcw %0; fnstsw %1" : "=m"(after), "=m"(status));
        __asm__ volatile("fldcw %0; fnclex" : : "m"(saved_control));
        if (!side) device_disc_status_states[step] = status & 0x3f;
        else if (device_disc_status_states[step] != (status & 0x3f)) printf("disc x87 status differs: %x/%x\n", device_disc_status_states[step], status & 0x3f);
        event(draw_fault_code); event(after); event(status & 0x3f); event(device_disc_draws);
        for (index = 0; index < 7; index++) if (index != 0 && index != 2 && index != 3) event(draw_registers[index]);
        compare(result); CHECK(!draw_fault_code);
        if (device_disc_draws) CHECK(WORD(images[side], 0x28b24) == 1 && !WORD(images[side], 0x28b04) && !WORD(images[side], 0x28b6c));
        CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
    }
}
