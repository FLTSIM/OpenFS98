/* Basic software projective spans through public +0x44/+0x40. */
static void projective_case(uint32_t selector, int sharp)
{
    int negative = (selector & 2) != 0;
    matrix_case();
    polygon_view[0] = polygon_view[2] = 0x1000;
    global[0xd42] = 0;
    GLOBAL_HALF(0xcfa) = 0x100; GLOBAL_HALF(0xd00) = 0x20;
    GLOBAL_HALF(0xcfc) = negative ? (uint16_t)-(sharp ? 0x1000 : 64) : sharp ? 0x1000 : 64;
    GLOBAL_HALF(0xd02) = sharp ? 0 : negative ? (uint16_t)-16 : 16;
    GLOBAL_HALF(0xcfe) = (uint16_t)-32; GLOBAL_HALF(0xd04) = 0x100;
    WORD(global, 0xd0c) = 0x10000;
    WORD(global, 0xd10) = negative ? 0u - (sharp ? 128 : 0x40000) : sharp ? 128 : 0x40000;
    WORD(global, 0xd14) = 0x20000;
    WORD(global, 0x320) = 0x100000;
    GLOBAL_HALF(0xdce) = 1;
    GLOBAL_HALF(0xe88) = 100;
    WORD(global, 0xcdc) = 0; WORD(global, 0xce0) = negative ? 0x400000 : (uint32_t)-0x400000;
    WORD(global, 0xce4) = 0x2000;
    WORD(global, 0xc94) = WORD(global, 0xc98) = 0; WORD(global, 0xc9c) = 0x1000;
    WORD(global, 0xca4) = WORD(global, 0xca8) = 0;
    WORD(global, 0xcac) = WORD(global, 0xcb0) = 0xffffff;
    GLOBAL_HALF(0x2e8) = selector & 1;
    GLOBAL_HALF(0xca0) = 0; GLOBAL_HALF(0xcb4) = 0; GLOBAL_HALF(0xcce) = 0;
}

static void projective_point(uint32_t x, uint32_t y)
{
    polygon_submit(x, y, 0x1000);
}

static void projective_finish(void)
{
    polygon_compare = WORD(global, 0x1298) ? 2 : 1;
    drawing(0x40, draw_surface(), 0, 0, 0, 0, 0);
    CHECK(!draw_fault_code);
    CHECK(WORD(images[side], 0x4200) == 0x20733bbb || WORD(images[side], 0x4200) == 0x2073751e);
}

static void run_projective(void)
{
    static const uint32_t widths[] = {0,1,2,3,4,7,8,15,16,31,32,33,61,62,63,64,65,66,95,127,128,129,159};
    uint32_t index, selector, alignment, width, x, y;
    matrix_reset();
    memset(matrix_pixels, 0xa5, sizeof matrix_pixels);
    for (selector = 0; selector < 2; selector++) {
        projective_case(selector, 0);
        memcpy(fill_expected, canvas, CANVAS_SIZE);
        projective_point(4, 4); projective_point(12, 4); projective_point(12, 8); projective_point(4, 8);
        projective_finish();
        for (y = 4; y <= 8; y++) for (x = 4; x <= 13; x++)
            fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = 0xa5;
        CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    }
    for (index = 0; index < sizeof matrix_pixels; index++)
        matrix_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    for (selector = 0; selector < 4; selector++) for (alignment = 0; alignment < 4; alignment++) {
        projective_case(selector, 0);
        width = scenario & 1 ? widths[(scenario + selector * 7 + alignment) % (sizeof widths / sizeof widths[0])] :
                2 * ((scenario / 2 + selector * 7 + alignment) % 81);
        x = alignment + 2; y = 3 + (scenario % 4);
        if (scenario & 1) {
            projective_point(x, y + 3); projective_point(x + width, y + 3);
            projective_point(x + width, y); projective_point(x, y);
        } else {
            projective_point(x, y); projective_point(x + width, y);
            projective_point(x + width, y + 3); projective_point(x, y + 3);
        }
        projective_finish();
    }
    for (selector = 0; selector < 4; selector++) for (alignment = 0; alignment < 2; alignment++) {
        projective_case(selector, 1);
        projective_point(0, 3 + alignment); projective_point(62 + scenario % 70, 3 + alignment);
        projective_point(62 + scenario % 70, 6 + alignment); projective_point(0, 6 + alignment);
        projective_finish();
    }
    for (index = 0; index < 6; index++) {
        projective_case((scenario + index) & 3, index & 1);
        if (index == 0) {
            projective_point(4, 4); projective_point(24, 12); projective_point(8, 24);
        } else if (index == 1) {
            projective_point(4, 4); projective_point(24, 24); projective_point(4, 24); projective_point(24, 4);
        } else if (index == 2) {
            projective_point(7, 10);
        } else if (index == 3) {
            projective_point(3, 9); projective_point(24, 9);
        } else if (index == 4) {
            projective_point(8, 8); projective_point(8, 16); projective_point(8, 24);
        } else {
            polygon_submit(9, 9, 0x2000); polygon_submit(33, 11, 0x2000); polygon_submit(17, 33, 0x2000);
        }
        projective_finish();
    }
    for (index = 0; index < 4; index++) {
        projective_case(index & 3, 0);
        GLOBAL_HALF(0xcfa) = (uint16_t)random_word(); GLOBAL_HALF(0xd00) = (uint16_t)random_word();
        GLOBAL_HALF(0xcfe) = (uint16_t)random_word(); GLOBAL_HALF(0xd04) = (uint16_t)random_word();
        WORD(global, 0xd0c) = random_word(); WORD(global, 0xd14) = random_word();
        projective_point(4, 5); projective_point(80, 12); projective_point(8, 24);
        projective_finish();
    }
    /* The correction-block threshold is strict: 1/(15+1) selects 32,
     * whereas 1/(16+1) selects 16. Count 65 exposes the final block residue. */
    for (index = 0; index < 2; index++) {
        projective_case(0, 0);
        WORD(global, 0xce0) = (uint32_t)-15;
        GLOBAL_HALF(0xcfc) = 512; GLOBAL_HALF(0xd02) = 0;
        WORD(global, 0xd10) = index ? 0xef00 : 0xff00;
        projective_point(0, 9); projective_point(128, 9);
        projective_finish();
        CHECK(WORD(images[side], 0x4160) == (index ? 17u : 33u));
    }
    for (index = 0; index < 3; index++) {
        projective_case(index & 1, 0);
        WORD(global, 0xd0c) = index ? random_word() : (uint32_t)-0x10000;
        WORD(global, 0xca4) = index == 0 ? (uint32_t)-0x20000 : index == 1 ? 0x100000 : 0x80000000;
        WORD(global, 0xcac) = index == 1 ? 0xff00 : index == 0 ? 0x20000 : 0x7fffffff;
        projective_point(3, 3); projective_point(80, 3); projective_point(80, 7); projective_point(3, 7);
        projective_finish();
    }
    /* Projective spans ignore logical height and existing-pixel remapping. */
    projective_case(1, 0);
    WORD(global, 0x1298) = 1;
    memset(matrix_pixels, 0x5c, sizeof matrix_pixels);
    memcpy(fill_expected, canvas, CANVAS_SIZE);
    projective_point(4, 50); projective_point(12, 50); projective_point(12, 53); projective_point(4, 53);
    projective_finish();
    for (y = 50; y <= 53; y++) for (x = 4; x <= 13; x++)
        fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = 0x5c;
    CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    for (index = 0; index < sizeof matrix_pixels; index++)
        matrix_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    /* A full forward block reads both texels before storing its pixel pair. */
    projective_case(1, 0);
    for (index = 0; index < CANVAS_SIZE; index++) canvas[index] = (uint8_t)(index * 13 + scenario * 7);
    matrix_texture[0] = ADDRESS(canvas);
    index = row_storage[16 + 9] - ADDRESS(canvas);
    WORD(global, 0xce0) = (uint32_t)-0x1000;
    GLOBAL_HALF(0xcfa) = (uint16_t)-256; GLOBAL_HALF(0xd00) = 0;
    GLOBAL_HALF(0xcfc) = GLOBAL_HALF(0xd02) = 0; WORD(global, 0xd10) = 0;
    GLOBAL_HALF(0xcfe) = GLOBAL_HALF(0xd04) = 0;
    WORD(global, 0xd0c) = ((index & 255) + 7) << 12;
    WORD(global, 0xd14) = (index >> 8) << 12;
    projective_point(4, 9); projective_point(67, 9);
    projective_point(67, 10); projective_point(4, 10);
    projective_finish();
    matrix_texture[0] = ADDRESS(matrix_pixels);
    projective_case(3, 1);
    GLOBAL_HALF(0xcfc) = 0x1000; WORD(global, 0xd10) = 128;
    WORD(global, 0xca4) = WORD(global, 0xca8) = (uint32_t)-0xffffff;
    projective_point(0, 3); projective_point(127, 3); projective_point(127, 7); projective_point(0, 7);
    projective_finish();
    /* Zero projective numerator and both signs of repaired wide division. */
    projective_case(0, 0);
    WORD(global, 0xce0) = 0;
    projective_point(4, 9); projective_point(80, 9);
    projective_finish();
    projective_case(0, 0);
    WORD(global, 0xce0) = (uint32_t)-0x800000;
    GLOBAL_HALF(0xcfa) = GLOBAL_HALF(0xd00) = 0;
    GLOBAL_HALF(0xcfc) = GLOBAL_HALF(0xd02) = 0; WORD(global, 0xd10) = 0;
    projective_point(4, 9); projective_point(12, 9);
    projective_finish();
    CHECK(WORD(images[side], 0x4190) == 0); /* Valid INT32_MIN quotient, then NEG wraps. */
    for (index = 0; index < 2; index++) {
        projective_case(index ? 2 : 0, 0);
        WORD(global, 0xce0) = index ? 0x7fffffff : 0x80000000;
        polygon_view[0] = polygon_view[2] = 1;
        GLOBAL_HALF(0xcfa) = 0x7fff; GLOBAL_HALF(0xd00) = 0;
        GLOBAL_HALF(0xcfe) = 0x7fff; GLOBAL_HALF(0xd04) = 0;
        GLOBAL_HALF(0xcfc) = GLOBAL_HALF(0xd02) = 0; WORD(global, 0xd10) = 0;
        polygon_submit(0, 4, 1); polygon_submit(1, 4, 1);
        projective_finish();
    }
    polygon_compare = 0;
}
