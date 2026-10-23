/* Perspective +0x50: three attributes and 40-byte records, no rasterization. */
static uint32_t perspective_submit(uint32_t x, uint32_t y, uint32_t depth,
                               uint32_t first_attribute, uint32_t second_attribute)
{
    uint32_t arguments[7] = {ADDRESS(polygon_record), x, y, depth,
                             first_attribute, second_attribute, first_attribute ^ 0x60000};
    uint32_t result = drawing_arguments(0x50, arguments, 7);
    CHECK(!draw_fault_code);
    return result;
}

static void run_perspective(void)
{
    static const uint32_t boundaries[] = {
        0, 1, 2, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff
    };
    static const uint32_t enables[] = {1, 2, 0x8000, 0xffff};
    uint32_t index;
    draw_reset();
    polygon_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
    WORD(global, 0xcb6) = 0x10000; /* Only the low word enables affine attributes. */
    polygon_begin();
    for (index = 0; index < 8; index++)
        CHECK(perspective_submit(index * 3, index * 2, index + 1, random_word(), random_word()) == 0xaabbccdd);
    CHECK(WORD(global, 0xc14) == ADDRESS(images[side] + 0x104 + 7 * 16));
    WORD(global, 0xcb6) = enables[scenario % 4] | 0x10000;
    polygon_begin();
    WORD(images[side], 0x8d0) = 0x12345678;
    WORD(images[side], 0x8d4) = 0x87654321;
    CHECK(perspective_submit(2, 3, 1, 0x10000, 0x20000) == 0x30000);
    CHECK(draw_registers[2] == 0x70000 && draw_registers[3] == 0);
    CHECK(WORD(images[side], 0x934) == 0x70000 && WORD(images[side], 0x938) == 0x70000);
    CHECK(WORD(images[side], 0x914) == 0x20000 && WORD(images[side], 0x918) == 0x10000);
    CHECK(perspective_submit(6, 3, 1, 0x50000, 0xa0000) == ADDRESS(global));
    CHECK(WORD(images[side], 0x104) == 3 && WORD(images[side], 0x108) == 3);
    CHECK(WORD(images[side], 0x10c) == 0x20000 && WORD(images[side], 0x110) == 0x40000);
    CHECK(WORD(images[side], 0x114) == 0x20000 && WORD(images[side], 0x118) == 0x10000);
    CHECK(WORD(images[side], 0x11c) == 0x80000 && WORD(images[side], 0x120) == 0x40000);
    CHECK(draw_registers[2] == 2 && draw_registers[3] == 3);
    perspective_submit(6, 7, 1, 0x90000, 0x120000);
    CHECK(WORD(images[side], 0x124) == 0x70000 && WORD(images[side], 0x128) == 0xfffc0000);
    CHECK(WORD(images[side], 0x12c) == 3 && WORD(images[side], 0x130) == 7);
    CHECK(WORD(images[side], 0x134) == 0x60000 && WORD(images[side], 0x138) == 0);
    CHECK(WORD(images[side], 0x13c) == 0xbfffe && WORD(images[side], 0x140) == 0x5ffff);
    CHECK(WORD(images[side], 0x144) == 0x20000 && WORD(images[side], 0x148) == 0x10000);
    CHECK(WORD(images[side], 0x14c) == 0x5fffd && WORD(images[side], 0x150) == 0x30000);
    CHECK(draw_registers[2] == 0x30000 && draw_registers[3] == 0x2fffd);
    perspective_submit(2, 3, 1, 0x10000, 0x20000);
    perspective_submit(9, 10, 3, 0x12345, 0x87654);
    perspective_submit(8, 10, 3, 0x80000001, 0xffffffff);
    perspective_submit(9, 11, 3, 0x7fffffff, 0);
    CHECK(WORD(images[side], 0x8d0) == 0x12345678 && WORD(images[side], 0x8d4) == 0x87654321);
    for (index = 0; index < 32; index++) {
        if (!(index % 8)) polygon_begin();
        polygon_view[0] = boundaries[random_word() % 9];
        polygon_view[1] = random_word();
        polygon_view[2] = boundaries[random_word() % 9];
        polygon_view[3] = random_word();
        perspective_submit(random_word(), random_word(), boundaries[index % 9], random_word(), random_word());
    }
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
    for (index = 0; index < 9; index++) {
        polygon_begin();
        perspective_submit(boundaries[index], boundaries[8 - index], index & 1 ? 0 : 1, index, ~index);
    }
    polygon_begin();
    perspective_submit(0xffff8000u, 0xffff8000u, 1, 0, 0);
    CHECK(draw_registers[0] == 0x80000000u && draw_registers[3] == 0);
    polygon_begin();
    CHECK(perspective_submit(0, (uint32_t)-10, 3, 0, 0) == (uint32_t)-218453);
    CHECK(draw_registers[3] == 0xffffffffu);
    polygon_begin();
    CHECK(perspective_submit(0, 10, (uint32_t)-3, 0, 0) == (uint32_t)-218453);
    CHECK(draw_registers[3] == 1);
    polygon_begin();
    perspective_submit(0, 0xffff, 0x10000, 0, 0);
    perspective_submit(0x7fffffff, 0x10000, 0x10000, 0x7fffffff, 0x80000000);
    polygon_record[7] = ADDRESS(images[side] + 0x8d8);
    WORD(images[side], 0x8d8) = 4; WORD(images[side], 0x8dc) = 3;
    WORD(images[side], 0x8e0) = 2; WORD(images[side], 0x8e4) = 1;
    polygon_begin();
    CHECK(perspective_submit(2, 3, 1, 7, 11) == 0xa0000);
    CHECK(draw_registers[2] == (7 ^ 0x60000) && WORD(images[side], 0x900) == 0x90000);
    polygon_record[7] = ADDRESS(polygon_view);
    if (!(scenario % 16)) {
        polygon_begin();
        for (index = 0; index < 49; index++)
            perspective_submit(index * 3, index * 5, index + 1, index * 29, index * 43);
        CHECK(WORD(global, 0x2f0) == 48);
        CHECK(WORD(global, 0xc14) == ADDRESS(images[side] + 0x884));
        CHECK(WORD(images[side], 0x92c) == 48 * 43 && WORD(images[side], 0x930) == 48 * 29);
    }
    CHECK(WORD(images[side], 0x8d0) == 0x12345678 && WORD(images[side], 0x8d4) == 0x87654321);
    polygon_compare = 0;
}
