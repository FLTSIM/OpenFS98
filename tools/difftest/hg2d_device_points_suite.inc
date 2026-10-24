/* Native device points, including register results and integer-to-float RC. */
static uint32_t device_point_entry[2][2];
static void run_device_points(void)
{
    static const uint32_t positions[] = {0,1,0xffffffff,0x7fffffff,0x80000000,16777215,16777216,16777217,0xfeffffff};
    static const uint32_t capacities[] = {0,1,2,0xffffffff,0x80000000,0x1000};
    static const uint32_t depths[] = {0,99,100,101,0x7fff,0x8000,0xffff,0x12340064};
    uint32_t variant, index, kind, before;
    uint16_t saved_control, control, after, status;
    draw_reset(); device_points_compare = 1;
    memset(device_vtables[1], 0, sizeof device_vtables[1]); device_objects[1][0] = ADDRESS(device_vtables[1]);
    device_vtables[1][23] = ADDRESS(device_cached_state); device_vtables[1][29] = ADDRESS(device_draw_primitive);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    device_state_result_offset = 0x5f34; device_render_result = 0x88760000; device_draw_result = 0x8876ffff;
    __asm__ volatile("fnstcw %0" : "=m"(saved_control));
    for (variant = 0; variant < 192; variant++) {
        uint32_t arguments[4] = {0x12345678,positions[(scenario + variant) % 9],positions[(scenario + variant * 3) % 9],depths[(scenario + variant) % 8]}, result;
        kind = variant & 1;
        WORD(images[side], 0x28a04) = variant & 2 ? 0x20 : 0;
        WORD(images[side], 0x8850) = variant & 4 ? 1 : 2;
        WORD(images[side], 0x5ee8) = capacities[(scenario + variant / 4) % 6];
        WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860 + (variant & 8 ? 64 : 0));
        WORD(images[side], 0x5ee0) = ADDRESS(images[side] + 0x8860 + 32);
        WORD(images[side], 0x8848) = 0xdeadbeef; WORD(images[side], 0x885c) = 0;
        for (index = 0; index < 32; index++) WORD(images[side], 0x8860 + index * 4) = 0x11223344 + index;
        WORD(global, 0x2ec) = (variant & 16 ? 0x6900 : 0xf000) | (variant & 255);
        GLOBAL_HALF(0xdce) = variant & 32 ? 1 : 0;
        touched = NULL; check_carry = 0;
        control = (saved_control & ~0xf00u) | ((variant % 3 == 0 ? 0 : variant % 3 == 1 ? 0x200 : 0x300)) | ((variant % 4) << 10);
        __asm__ volatile("fldcw %0; fnclex" : : "m"(control));
        draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
        before = step; result = invoke_draw(device_point_entry[kind][side], arguments, kind ? 4 : 3, 0xaabbccdd);
        draw_active = 0;
        __asm__ volatile("fnstcw %0; fnstsw %1" : "=m"(after), "=m"(status));
        __asm__ volatile("fldcw %0; fnclex" : : "m"(saved_control));
        event(draw_fault_code); event(after); event(status & 0x3f);
        for (index = 0; index < 7; index++) {
            uint32_t value = index == 0 ? normalize_result(draw_registers[index]) : draw_registers[index];
            event(value);
        }
        compare(normalize_result(result)); CHECK(!draw_fault_code);
        CHECK(draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
        if (!kind || !(variant & 2)) CHECK(draw_registers[1] == 0xb0b0b0b0);
        if (variant & 2) {
            uint32_t *vertex = (uint32_t *)(uintptr_t)(WORD(images[side], 0x5ee4) - 32);
            CHECK(vertex[2] == 0x3f800000 && vertex[3] == 0x3f800000 && vertex[5] == 0xff000000);
            CHECK(draw_registers[2] == 1 && draw_registers[3] == WORD(images[side], 0x5ee8));
        } else CHECK(result == 0xaabbccdd && draw_registers[2] == 0xc0c0c0c0 && draw_registers[3] == 0xd0d0d0d0);
        CHECK(step == before + 1);
    }
    device_points_compare = 0;
}
