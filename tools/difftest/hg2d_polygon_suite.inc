/* +0x44 projection and plain edges. No rasterizer or textured attributes yet. */
static uint32_t polygon_record[8], polygon_view[4];

static void polygon_begin(void)
{
    WORD(global, 0x2f0) = 0xffffffffu;
    WORD(global, 0xc14) = ADDRESS(images[side] + 0x104);
}

static void polygon_submit(uint32_t x, uint32_t y, uint32_t depth)
{
    CHECK(drawing(0x44, ADDRESS(polygon_record), x, y, depth, 0, 0) == 0xaabbccdd);
    CHECK(!draw_fault_code);
}

static void run_polygons(void)
{
    static const uint32_t values[] = {
        0, 1, 2, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff
    };
    uint32_t index, x, y, depth;
    draw_reset();
    polygon_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
    polygon_begin();
    polygon_submit(2, 3, 1);
    CHECK(WORD(images[side], 0x8f8) == 0x20000);
    CHECK(WORD(images[side], 0x8fc) == 0x30000);
    CHECK(WORD(global, 0xc14) == ADDRESS(images[side] + 0x104));
    polygon_submit(6, 3, 1);
    CHECK(WORD(images[side], 0x104) == 3 && WORD(images[side], 0x108) == 3);
    CHECK(WORD(images[side], 0x10c) == 0x28000 && WORD(images[side], 0x110) == 0x40000);
    polygon_submit(6, 7, 1);
    CHECK(WORD(images[side], 0x114) == 3 && WORD(images[side], 0x118) == 7);
    CHECK(WORD(images[side], 0x11c) == 0x60000 && !WORD(images[side], 0x120));
    polygon_submit(2, 3, 1); /* Reversed nonflat edge, signed-complement slope. */
    /* Subpixel projection, equal integer rows, both edge directions. */
    polygon_submit(9, 10, 3);
    polygon_submit(8, 10, 3);
    polygon_submit(9, 11, 3);
    polygon_submit(8, 9, 3);
    for (index = 0; index < 24; index++) {
        if (!(index % 6)) polygon_begin();
        polygon_view[0] = values[random_word() % 9];
        polygon_view[1] = random_word();
        polygon_view[2] = values[random_word() % 9];
        polygon_view[3] = random_word();
        x = random_word(); y = random_word(); depth = values[index % 9];
        polygon_submit(x, y, depth);
    }
    /* Include ordinary quotients amid the overflow and zero-divide vectors. */
    polygon_begin();
    polygon_view[0] = 32; polygon_view[1] = 32;
    polygon_view[2] = 24; polygon_view[3] = 24;
    for (index = 0; index < 24; index++) {
        /* Either flag alone still selects plain edges; their conjunction is
         * the deferred textured-attribute path. Test word-sized flags. */
        WORD(global, 0xc90) = index & 1 ? 0x10000 : 1;
        WORD(global, 0x2f4) = index & 1 ? 1 : 0x10000;
        x = random_word() % 65 - 32; y = random_word() % 65 - 32;
        depth = random_word() % 65 + 1;
        polygon_submit(x, y, depth);
    }
    /* First vertex, INT32_MIN quotient (valid), and both saturation signs. */
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
    for (index = 0; index < 9; index++) {
        polygon_begin();
        polygon_submit(values[index], values[8 - index], index & 1 ? 0 : 1);
    }
    polygon_begin();
    polygon_submit(0xffff8000u, 0x8000, 1);
    CHECK(WORD(images[side], 0x900) == 0x80000000u);
    CHECK(WORD(images[side], 0x904) == 0x7fffffffu);
    polygon_begin();
    polygon_submit(0, 0xffff, 0x10000);
    polygon_submit(0x7fffffffu, 0x10000, 0x10000); /* Clamped steep slope. */
    polygon_submit(0x80000000u, 0xffff, 0x10000);
    /* A view overlapping projection scratch makes load/store order visible. */
    polygon_record[7] = ADDRESS(images[side] + 0x8d8);
    WORD(images[side], 0x8d8) = 4; WORD(images[side], 0x8dc) = 3;
    WORD(images[side], 0x8e0) = 2; WORD(images[side], 0x8e4) = 1;
    polygon_begin();
    polygon_submit(2, 3, 1);
    CHECK(WORD(images[side], 0x900) == 0x90000);
    CHECK(WORD(images[side], 0x904) == 0xa0000);
    polygon_record[7] = ADDRESS(polygon_view);
    if (!(scenario % 16)) {
        polygon_begin();
        for (index = 0; index < 125; index++)
            polygon_submit(index * 3, index * 5, index + 1);
        CHECK(WORD(global, 0x2f0) == 124);
        CHECK(WORD(global, 0xc14) == ADDRESS(images[side] + 0x824));
        CHECK(WORD(images[side], 0x8d4) == 125);
        CHECK(WORD(images[side], 0x8d0) == 1);
    }
    polygon_compare = 0;
}
