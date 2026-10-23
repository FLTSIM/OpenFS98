/* Basic software projective spans through public +0x44/+0x40. */
static int projective_key_mode, projective_fog_mode;

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
    if (projective_key_mode) {
        images[side][0x435d] = 0xa5;
        WORD(global, 0xca0) = 0xaabbcc01;
        GLOBAL_HALF(0xcb4) = scenario & 1 ? 2 : 0;
        GLOBAL_HALF(0x1172) = GLOBAL_HALF(0x324) = 1;
    }
    if (projective_fog_mode) {
        GLOBAL_HALF(0xdce) = 0; GLOBAL_HALF(0xe88) = 1000;
        GLOBAL_HALF(0x30c) = 0;
    }
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
    if (projective_fog_mode)
        CHECK(WORD(images[side], 0x4200) == 0x207352a4 || WORD(images[side], 0x4200) == 0x20738c03);
    else if (projective_key_mode)
        CHECK(WORD(images[side], 0x4200) == 0x2073ae7b || WORD(images[side], 0x4200) == 0x2073e1ec);
    else
        CHECK(WORD(images[side], 0x4200) == 0x20733bbb || WORD(images[side], 0x4200) == 0x2073751e);
}

static void run_projective(void)
{
    static const uint32_t widths[] = {0,1,2,3,4,7,8,15,16,31,32,33,61,62,63,64,65,66,95,127,128,129,159};
    uint32_t index, selector, alignment, width, x, y;
    matrix_reset();
    if (projective_fog_mode)
        for (index = 0; index < 0x1000; index++) images[side][0xd70 + index] = (uint8_t)index;
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

static void run_projective_keyed(void)
{
    uint32_t selector, level, boundary, x, y, last_x;
    uint8_t key, value;
    projective_key_mode = 1;
    run_projective();
    /* Independently check skipped, equal and greater raw texels across both
     * directions, full blocks and every possible tail residue. */
    for (selector = 0; selector < 2; selector++) for (level = 0; level < 4; level++) {
        key = level == 0 ? 1 : level == 1 ? 128 : level == 2 ? 255 : 1 + scenario % 255;
        for (boundary = 0; boundary < 3; boundary++) {
            projective_case(selector, scenario & 1);
            global[0xca0] = key;
            value = (uint8_t)(key + boundary - 1);
            memset(matrix_pixels, value, sizeof matrix_pixels);
            memcpy(fill_expected, canvas, CANVAS_SIZE);
            last_x = 4 + 2 * (scenario % 81);
            projective_point(4, 12); projective_point(last_x, 12);
            projective_point(last_x, 13); projective_point(4, 13);
            projective_finish();
            if (value >= key)
                for (y = 12; y <= 13; y++) for (x = 4; x <= last_x + 1; x++)
                    fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = value;
            CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
        }
    }
    projective_key_mode = 0;
}

static void run_projective_fog(void)
{
    static const uint32_t distances[] = {0, 1020, 1024, 4096, 15356, 15360, 16384};
    uint32_t index, selector, variant, x, y, row, distance, level;
    projective_fog_mode = 1;
    run_projective();
    for (index = 0; index < 0x1000; index++)
        images[side][0xd70 + index] = (uint8_t)(index * 13 + (index >> 8) * 19 + scenario);
    for (selector = 0; selector < 4; selector++) for (variant = 0; variant < 7; variant++) {
        projective_case(selector, variant & 1);
        GLOBAL_HALF(0xe88) = variant < 4 ? 1 : variant == 4 ? 3000 : variant == 5 ? 6000 : 1000;
        if (variant & 2) WORD(global, 0xce0) = 0u - WORD(global, 0xce0);
        if (variant == 6) WORD(global, 0xd10) = random_word();
        projective_point(3 + (scenario & 3), 4); projective_point(7 + scenario % 160, 4);
        projective_point(7 + scenario % 160, 8); projective_point(3 + (scenario & 3), 8);
        projective_finish();
    }
    /* Constant B gives an exact factor. Setup scale 64 makes positive levels
     * distance/4; a negative factor uses complement magnitude and loses one. */
    memset(matrix_pixels, 0xa5, sizeof matrix_pixels);
    for (selector = 0; selector < 2; selector++) for (index = 0; index < 14; index++) {
        projective_case(selector, 0);
        distance = distances[index % 7];
        WORD(global, 0xce0) = index < 7 ? 0u - distance : distance;
        GLOBAL_HALF(0xe88) = 2048;
        GLOBAL_HALF(0xcfc) = GLOBAL_HALF(0xd02) = 0; WORD(global, 0xd10) = 0;
        WORD(global, 0x1298) = scenario & 1;
        row = scenario & 2 ? 50 : 12;
        memcpy(fill_expected, canvas, CANVAS_SIZE);
        projective_point(4, row); projective_point(12, row);
        projective_point(12, row + 1); projective_point(4, row + 1);
        projective_finish();
        level = distance ? distance / 4 - (index >= 7) : 0;
        if (level > 4095) level = 4095;
        CHECK(WORD(images[side], 0x41c4) == level);
        if ((level >> 8) != 15 || row < surface[2])
            for (y = row; y <= row + 1; y++) for (x = 4; x <= 12 + ((level >> 8) != 15); x++) {
                uint32_t address = row_storage[16 + y] - ADDRESS(canvas) + x;
                fill_expected[address] = (level >> 8) == 15 ?
                    (scenario & 1 ? images[side][0xd70 + 256 + fill_expected[address]] : images[side][0x1d06]) :
                    level < 256 ? 0xa5 : images[side][0xd70 + (level >> 8) * 256 + 0xa5];
            }
        CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    }
    /* Exact starting levels 260/280 and gentle B gradients produce block
     * shade steps -1/-2. Retail suppresses both, retaining row 1 for 32 samples. */
    for (variant = 0; variant < 2; variant++) {
        projective_case(1, 0);
        GLOBAL_HALF(0xe88) = 2048;
        WORD(global, 0xce0) = 0u - (variant ? 287840 : 267280);
        GLOBAL_HALF(0xcfc) = variant ? 32 : 1; GLOBAL_HALF(0xd02) = 0;
        WORD(global, 0xd10) = 0x10000;
        memcpy(fill_expected, canvas, CANVAS_SIZE);
        projective_point(0, 9); projective_point(64, 9);
        projective_finish();
        CHECK(WORD(images[side], 0x41c0) == (variant ? 280u : 260u));
        for (x = 0; x <= 65; x++)
            fill_expected[row_storage[16 + 9] - ADDRESS(canvas) + x] =
                images[side][0xd70 + (variant && x >= 64 ? 0 : 256) + 0xa5];
        CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    }
    /* Destination aliases the visible shade step. Its first stores change
     * the next increment, making the row-byte mask observable. */
    projective_case(1, 0);
    GLOBAL_HALF(0xe88) = 2048;
    WORD(global, 0xce0) = (uint32_t)-0x1000;
    GLOBAL_HALF(0xcfc) = GLOBAL_HALF(0xd02) = 0; WORD(global, 0xd10) = 0;
    uint32_t saved_row = row_storage[16 + 9];
    row_storage[16 + 9] = ADDRESS(images[side] + 0x4354);
    projective_point(4, 9); projective_point(12, 9);
    projective_finish();
    row_storage[16 + 9] = saved_row;
    polygon_compare = 0;
    projective_fog_mode = 0;
}
