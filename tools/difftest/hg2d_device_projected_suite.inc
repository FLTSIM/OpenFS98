/* Ordinary projected vertices, texture-plane attributes and distance/fog paths. */
static uint32_t device_projected_vertex_entry[2];
static void run_device_projected(void)
{
    static const uint32_t positions[] = {0,1,0xffffffff,0x7fffffff,0x80000000,0x00010001,0xffff8000,16777217,0xfeffffff};
    static const uint32_t counts[] = {0xffffffff,0,1,2,3,0x7ffffffe,0x7fffffff,0xfffffffe};
    static const uint32_t capacities[] = {0,99,100,101,0xffffffff,0x80000000,0x1000};
    static const uint16_t thresholds[] = {0,1,100,0x1fff,0x7cff,0x7d00,0x8000,0xffff};
    static const uint32_t distances[] = {0,1,255,256,25599,25600,25601,0xc34fff,0xc35000,0xc35001,0x80000000,0xffffffff};
    static const uint32_t scales[] = {0,150,200,0x10000,0x80000000,0x7fffffff};
    uint32_t variant, index, result;
    uint16_t saved_control, control, after, status;
    draw_reset(); device_points_compare = 1;
    memset(device_vtables[1], 0, sizeof device_vtables[1]); device_objects[1][0] = ADDRESS(device_vtables[1]);
    device_vtables[1][23] = ADDRESS(device_cached_state); device_vtables[1][29] = ADDRESS(device_draw_primitive);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    device_state_result_offset = 0x5f34; device_render_result = 0x88760000; device_draw_result = 0x8876ffff;
    memset(device_vertex_record, 0, sizeof device_vertex_record); device_vertex_record[7] = ADDRESS(device_vertex_view);
    memset(device_vertex_imports, 0, sizeof device_vertex_imports); device_vertex_imports[0x44 / 4] = ADDRESS(device_vertex_distance_stub);
    WORD(images[side], 0x2c) = ADDRESS(device_vertex_imports);
    __asm__ volatile("fnstcw %0" : "=m"(saved_control));
    for (variant = 0; variant < 240; variant++) {
        uint32_t kind = 0;
        uint32_t arguments[7] = {ADDRESS(device_vertex_record),positions[(scenario + variant) % 9],positions[(scenario + variant * 3) % 9],positions[(scenario + variant * 5) % 9],random_word(),positions[(scenario + variant * 7) % 9],0};
        static const uint32_t intensities[] = {0,1,0x7effffff,0x7f000000,0x7f7fffff,0x7f800000,0x7fffffff,0x80000000,0xffffffff};
        arguments[6] = intensities[(scenario + variant / 3) % 9];
        WORD(global, 0x2f0) = counts[(scenario + variant) % 8];
        WORD(images[side], 0x8850) = 3 + (variant & 3);
        WORD(images[side], 0x5ee8) = capacities[(scenario + variant / 4) % 7];
        WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860 + 32);
        WORD(images[side], 0x5ee0) = ADDRESS(images[side] + 0x8860);
        WORD(images[side], 0x8848) = ADDRESS(images[side] + 0x8860 + 96);
        memset(device_vertex_texture, 0, sizeof device_vertex_texture);
        device_vertex_texture[1] = variant & 8 ? 0x100000 : 0;
        device_vertex_texture[2] = 0xaabbccdd; device_vertex_texture[3] = ADDRESS(device_objects[3]);
        device_vertex_texture[4] = 10; device_vertex_texture[6] = variant & 8 ? 0x98 : 0;
        WORD(global, 0x2ac) = WORD(images[side], 0x5f48) = 10;
        WORD(images[side], 0x5f2c) = WORD(images[side], 0x5f40) = 0;
        WORD(global, 0xc8c) = ADDRESS(device_vertex_texture);
        GLOBAL_HALF(0xcb6) = variant & 4 ? 0x100 : 0;
        GLOBAL_HALF(0xcba) = variant & 16 ? 0x4000 : 0x3fff;
        WORD(global, 0x3c0) = (variant & 32 ? 0x6800 : 0xf000) | (variant & 255);
        WORD(images[side], 0x885c) = variant & 16 ? ADDRESS(device_vertex_texture) : 0;
        WORD(images[side], 0x8840) = variant & 32 ? 0x12345678 : 0;
        WORD(images[side], 0x8858) = variant & 64 ? 0x10203040 : 0;
        WORD(images[side], 0x289e0) = variant & 1 ? 1 : 0x10001;
        WORD(images[side], 0x8844) = 0; WORD(images[side], 0x884c) = variant & 32 ? 0x12008000 : 0;
        WORD(images[side], 0x5f24) = variant & 64;
        for (index = 0; index < 80; index++) WORD(images[side], 0x8860 + index * 4) = 0x11223344 + index;
        for (index = 0; index < 4; index++) device_vertex_view[index] = positions[(scenario + variant + index * 2) % 9];
        GLOBAL_HALF(0xdce) = variant & 128 ? 0x100 : 0;
        GLOBAL_HALF(0xe88) = thresholds[(scenario + variant / 3) % 8];
        WORD(global, 0xd4c) = distances[(scenario + variant / 2) % 12];
        GLOBAL_HALF(0xc90) = variant & 4 ? 0x100 : 0;
        GLOBAL_HALF(0xdca) = variant & 16 ? 0x100 : 0;
        GLOBAL_HALF(0xf10) = variant & 64 ? 0x100 : 0;
        WORD(global, 0x398) = variant & 1;
        GLOBAL_HALF(0x1172) = variant & 2 ? 1 : 0;
        for (index = 0; index < 6; index++) GLOBAL_HALF(0xcfa + index * 2) = (uint16_t)random_word();
        for (index = 0; index < 3; index++) { WORD(global, 0xcdc + index * 4) = random_word(); WORD(global, 0xc94 + index * 4) = random_word(); WORD(global, 0xd0c + index * 4) = random_word(); }
        WORD(global, 0xca4) = positions[(scenario + variant) % 9]; WORD(global, 0xcac) = positions[(scenario + variant + 1) % 9];
        WORD(global, 0xca8) = positions[(scenario + variant + 2) % 9]; WORD(global, 0xcb0) = positions[(scenario + variant + 3) % 9];
        global[0xd42] = (uint8_t)(variant * 7);
        WORD(global, 0x320) = positions[(scenario + variant + 4) % 9];
        WORD(global, 0x30c) = scales[(scenario + variant / 7) % 6];
        GLOBAL_HALF(0xcd4) = variant & 4 ? 0 : 150; GLOBAL_HALF(0xca0) = variant & 8 ? 0x98 : 0;
        device_vertex_distance_result = distances[(scenario + variant / 5) % 12];
        device_vertex_ecx_result = 0xabcd1234 ^ variant; device_vertex_edx_result = 0xdcba4321 ^ scenario;
        /* Exercise all 24 UV bits with a zero projection factor and
         * same-high-byte clip bounds, rather than relying on random clamps. */
        if (variant == 33) {
            WORD(global, 0x2f0) = 1; WORD(images[side], 0x8840) = 0x12345678;
            WORD(global, 0xcdc) = 0x123456; WORD(global, 0xce0) = 0; WORD(global, 0xce4) = 0x654321;
            WORD(global, 0xc94) = WORD(global, 0xc98) = WORD(global, 0xc9c) = 0;
            WORD(global, 0xca4) = WORD(global, 0xca8) = 0;
            WORD(global, 0xcac) = WORD(global, 0xcb0) = 0xffffff;
        }
        touched = NULL; check_carry = 0;
        control = (saved_control & ~0xf00u) | ((variant % 3 == 0 ? 0 : variant % 3 == 1 ? 0x200 : 0x300)) | ((variant % 4) << 10);
        __asm__ volatile("fldcw %0; fnclex" : : "m"(control));
        draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
        result = invoke_draw(device_projected_vertex_entry[side], arguments, 4, 0xaabbccdd);
        draw_active = 0;
        __asm__ volatile("fnstcw %0; fnstsw %1" : "=m"(after), "=m"(status));
        __asm__ volatile("fldcw %0; fnclex" : : "m"(saved_control));
        event(draw_fault_code); event(after); event(status & 0x3f);
        for (index = 0; index < 7; index++) {
            uint32_t value = draw_registers[index];
            if (index == 0 && kind != 2) value -= ADDRESS(images[side]);
            if (index == 3 && kind == 2 && (GLOBAL_HALF(0xdce) || GLOBAL_HALF(0xe88) >= 0x7d00 || !WORD(global, 0xd4c)) &&
                !(WORD(images[side], 0x289e0) == 1 && (int32_t)arguments[6] >> 23 >= 254)) value -= ADDRESS(images[side]);
            event(value);
        }
        for (index = 0; index < 13; index++) event(device_vertex_texture[index]);
        compare(kind == 2 ? result : result - ADDRESS(images[side])); CHECK(!draw_fault_code);
        CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
    }
    device_points_compare = 0;
}
