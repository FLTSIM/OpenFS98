/* Affine +0x48 vertex records; +0x4c has its own rasterization suite. */
static uint32_t textured_submit(uint32_t x, uint32_t y, uint32_t depth,
                               uint32_t first_attribute, uint32_t second_attribute)
{
    uint32_t result = drawing(0x48, ADDRESS(polygon_record), x, y, depth, first_attribute, second_attribute);
    CHECK(!draw_fault_code);
    return result;
}

static void run_textured(void)
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
        CHECK(textured_submit(index * 3, index * 2, index + 1, random_word(), random_word()) == 0xaabbccdd);
    CHECK(WORD(global, 0xc14) == ADDRESS(images[side] + 0x104 + 7 * 16));
    WORD(global, 0xcb6) = enables[scenario % 4] | 0x10000;
    polygon_begin();
    WORD(images[side], 0x8d0) = 0x12345678;
    WORD(images[side], 0x8d4) = 0x87654321;
    CHECK(textured_submit(2, 3, 1, 0x10000, 0x20000) == 0x30000);
    CHECK(draw_registers[2] == 0 && draw_registers[3] == 0);
    CHECK(WORD(images[side], 0x914) == 0x20000 && WORD(images[side], 0x918) == 0x10000);
    CHECK(textured_submit(6, 3, 1, 0x50000, 0xa0000) == ADDRESS(global));
    CHECK(WORD(images[side], 0x104) == 3 && WORD(images[side], 0x108) == 3);
    CHECK(WORD(images[side], 0x10c) == 0x20000 && WORD(images[side], 0x110) == 0x40000);
    CHECK(WORD(images[side], 0x114) == 0x20000 && WORD(images[side], 0x118) == 0x10000);
    CHECK(WORD(images[side], 0x11c) == 0x80000 && WORD(images[side], 0x120) == 0x40000);
    CHECK(draw_registers[2] == 2 && draw_registers[3] == 3);
    textured_submit(6, 7, 1, 0x90000, 0x120000);
    CHECK(WORD(images[side], 0x124) == 3 && WORD(images[side], 0x128) == 7);
    CHECK(WORD(images[side], 0x12c) == 0x60000 && WORD(images[side], 0x130) == 0);
    CHECK(WORD(images[side], 0x134) == 0xbfffe && WORD(images[side], 0x138) == 0x5ffff);
    CHECK(WORD(images[side], 0x13c) == 0x20000 && WORD(images[side], 0x140) == 0x10000);
    CHECK(draw_registers[2] == 0x10000 && draw_registers[3] == 0xffff);
    textured_submit(2, 3, 1, 0x10000, 0x20000);
    textured_submit(9, 10, 3, 0x12345, 0x87654);
    textured_submit(8, 10, 3, 0x80000001, 0xffffffff);
    textured_submit(9, 11, 3, 0x7fffffff, 0);
    CHECK(WORD(images[side], 0x8d0) == 0x12345678 && WORD(images[side], 0x8d4) == 0x87654321);
    for (index = 0; index < 32; index++) {
        if (!(index % 8)) polygon_begin();
        polygon_view[0] = boundaries[random_word() % 9];
        polygon_view[1] = random_word();
        polygon_view[2] = boundaries[random_word() % 9];
        polygon_view[3] = random_word();
        textured_submit(random_word(), random_word(), boundaries[index % 9], random_word(), random_word());
    }
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
    for (index = 0; index < 9; index++) {
        polygon_begin();
        textured_submit(boundaries[index], boundaries[8 - index], index & 1 ? 0 : 1, index, ~index);
    }
    polygon_begin();
    textured_submit(0xffff8000u, 0xffff8000u, 1, 0, 0);
    CHECK(draw_registers[0] == 0x80000000u && draw_registers[3] == 0);
    polygon_begin();
    CHECK(textured_submit(0, (uint32_t)-10, 3, 0, 0) == (uint32_t)-218453);
    CHECK(draw_registers[3] == 0xffffffffu);
    polygon_begin();
    CHECK(textured_submit(0, 10, (uint32_t)-3, 0, 0) == (uint32_t)-218453);
    CHECK(draw_registers[3] == 1);
    polygon_begin();
    textured_submit(0, 0xffff, 0x10000, 0, 0);
    textured_submit(0x7fffffff, 0x10000, 0x10000, 0x7fffffff, 0x80000000);
    polygon_record[7] = ADDRESS(images[side] + 0x8d8);
    WORD(images[side], 0x8d8) = 4; WORD(images[side], 0x8dc) = 3;
    WORD(images[side], 0x8e0) = 2; WORD(images[side], 0x8e4) = 1;
    polygon_begin();
    CHECK(textured_submit(2, 3, 1, 7, 11) == 0xa0000);
    CHECK(draw_registers[2] == 0x10000 && WORD(images[side], 0x900) == 0x90000);
    polygon_record[7] = ADDRESS(polygon_view);
    if (!(scenario % 16)) {
        polygon_begin();
        for (index = 0; index < 61; index++)
            textured_submit(index * 3, index * 5, index + 1, index * 29, index * 43);
        CHECK(WORD(global, 0x2f0) == 60);
        CHECK(WORD(global, 0xc14) == ADDRESS(images[side] + 0x884));
        CHECK(WORD(images[side], 0x92c) == 60 * 43 && WORD(images[side], 0x930) == 60 * 29);
    }
    CHECK(WORD(images[side], 0x8d0) == 0x12345678 && WORD(images[side], 0x8d4) == 0x87654321);
    polygon_compare = 0;
}
