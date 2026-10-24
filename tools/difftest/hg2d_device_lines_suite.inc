/* Shared fixed-line kernel, integer wrappers and the four-edge frame. */
static uint32_t device_line_entry[5][2], device_line_view[40], device_line_rect[4];
static void run_device_lines(void)
{
    static const uint32_t positions[] = {0,1,0xffffffff,0x7fffffff,0x80000000,0x00010001,0xffff8000,16777217,0xfeffffff};
    static const uint32_t capacities[] = {0,1,2,7,8,0xffffffff,0x80000000,0x1000};
    static const uint16_t thresholds[] = {0,1,100,0x1fff,0x7cff,0x7d00,0x8000,0xffff};
    static const uint32_t distances[] = {0,1,255,256,25599,25600,25601,0x1fff00,0x1fff000,0x7fffffff,0x80000000,0xffffffff};
    static const uint32_t scales[] = {0,150,200,0x10000,0x80000000,0x7fffffff};
    uint32_t variant, index, kind;
    uint16_t saved_control, control, after, status;
    draw_reset(); device_points_compare = 1;
    memset(device_vtables[1], 0, sizeof device_vtables[1]); device_objects[1][0] = ADDRESS(device_vtables[1]);
    device_vtables[1][23] = ADDRESS(device_cached_state); device_vtables[1][29] = ADDRESS(device_draw_primitive);
    WORD(images[side], 0x289d0) = ADDRESS(device_objects[1]);
    device_state_result_offset = 0x5f34; device_render_result = 0x88760000; device_draw_result = 0x8876ffff;
    WORD(global, 0x1194) = ADDRESS(device_line_view);
    memset(device_line_view, 0, sizeof device_line_view);
    device_line_view[0x60 / 4] = 64; device_line_view[0x64 / 4] = 48;
    device_line_view[0x1c / 4] = ADDRESS(device_line_rect);
    device_line_rect[0] = 2; device_line_rect[1] = 33; device_line_rect[2] = 5; device_line_rect[3] = 19;
    __asm__ volatile("fnstcw %0" : "=m"(saved_control));
    for (variant = 0; variant < 240; variant++) {
        uint32_t arguments[5] = {0x12345678,positions[(scenario + variant) % 9],positions[(scenario + variant * 3) % 9],positions[(scenario + variant * 5) % 9],positions[(scenario + variant * 7) % 9]}, result;
        kind = variant % 5;
        if (kind == 4 && (variant & 16)) { arguments[1] = arguments[2] = 0; arguments[3] = 64; arguments[4] = 48; }
        WORD(images[side], 0x28a04) = variant & 1 ? 0x20 : 0;
        WORD(images[side], 0x8850) = variant & 2 ? 2 : 1;
        WORD(images[side], 0x5ee8) = capacities[(scenario + variant / 4) % 8];
        WORD(images[side], 0x5ee4) = ADDRESS(images[side] + 0x8860 + (variant & 8 ? 64 : 0));
        WORD(images[side], 0x5ee0) = ADDRESS(images[side] + 0x8860 + 32);
        WORD(images[side], 0x8848) = 0xdeadbeef; WORD(images[side], 0x885c) = 0;
        WORD(images[side], 0x884c) = variant & 32 ? 0x12008000 : 0;
        WORD(images[side], 0x5f24) = variant & 64;
        for (index = 0; index < 128; index++) WORD(images[side], 0x8860 + index * 4) = 0x11223344 + index;
        WORD(global, 0x2ec) = (variant & 16 ? 0x6900 : 0xf000) | (variant & 255);
        GLOBAL_HALF(0xdce) = variant & 128 ? 0x100 : 0;
        GLOBAL_HALF(0xe88) = thresholds[(scenario + variant / 3) % 8];
        WORD(global, 0xd4c) = distances[(scenario + variant / 5) % 12];
        WORD(global, 0x30c) = scales[(scenario + variant / 7) % 6];
        GLOBAL_HALF(0xcd4) = variant & 4 ? 0 : 150;
        GLOBAL_HALF(0xca0) = variant & 8 ? 0x98 : 0;
        touched = NULL; check_carry = 0;
        control = (saved_control & ~0xf00u) | ((variant % 3 == 0 ? 0 : variant % 3 == 1 ? 0x200 : 0x300)) | ((variant % 4) << 10);
        __asm__ volatile("fldcw %0; fnclex" : : "m"(control));
        draw_fault_code = draw_fault_address = draw_fault_access = 0; draw_active = 1;
        result = invoke_draw(device_line_entry[kind][side], arguments, kind == 1 || kind == 2 ? 4 : 5, 0xaabbccdd);
        draw_active = 0;
        __asm__ volatile("fnstcw %0; fnstsw %1" : "=m"(after), "=m"(status));
        __asm__ volatile("fldcw %0; fnclex" : : "m"(saved_control));
        event(draw_fault_code); event(after); event(status & 0x3f);
        for (index = 0; index < 7; index++) {
            uint32_t value = index == 2 && (variant & 1) ? draw_registers[index] - ADDRESS(images[side]) : draw_registers[index];
            event(value);
        }
        compare(result); CHECK(!draw_fault_code);
        CHECK(draw_registers[1] == 0xb0b0b0b0 && draw_registers[4] == 0xe0e0e0e0 && draw_registers[5] == 0xf0f0f0f0 && draw_registers[6] == 0xbebebebe);
        if (variant & 1) CHECK(result == 2 && draw_registers[3] == WORD(images[side], 0x5ee8));
    }
    device_points_compare = 0;
}
