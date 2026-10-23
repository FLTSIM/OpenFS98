/* Matrix-derived attributes behind +0x44, setup and far fills through +0x40. */
static uint32_t matrix_texture[13];
static uint8_t matrix_pixels[0x10000];

static void matrix_case(void)
{
    polygon_begin();
    polygon_compare = 1;
    WORD(images[side], 0x1d88) = ADDRESS(span_map);
    WORD(global, 0xc90) = 0x10001;
    WORD(global, 0x2f4) = 0x10001;
    WORD(global, 0x1298) = 0;
    WORD(global, 0xc8c) = ADDRESS(matrix_texture);
    matrix_texture[0] = ADDRESS(matrix_pixels); matrix_texture[1] = 0x40000;
    GLOBAL_HALF(0xcfa) = 2; GLOBAL_HALF(0xcfc) = (uint16_t)-3; GLOBAL_HALF(0xcfe) = 7;
    GLOBAL_HALF(0xd00) = 5; GLOBAL_HALF(0xd02) = 4; GLOBAL_HALF(0xd04) = (uint16_t)-2;
    WORD(global, 0xd0c) = 10; WORD(global, 0xd10) = 100; WORD(global, 0xd14) = (uint32_t)-10;
    global[0xd42] = 1;
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
}

static void matrix_reset(void)
{
    draw_reset();
    polygon_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    matrix_case();
}

static void run_matrix_vertices(void)
{
    static const uint32_t values[] = {0,1,2,0xffff,0x10000,0x7fffffff,0x80000000,0xffffffff};
    static const uint8_t shifts[] = {0,1,13,31,32,33,63,255};
    uint32_t index, component;
    matrix_reset();
    polygon_submit(2, 3, 16);
    polygon_submit(6, 3, 16);
    CHECK(WORD(images[side], 0x104) == 0 && WORD(images[side], 0x108) == 0);
    CHECK(WORD(images[side], 0x10c) == 8192 && WORD(images[side], 0x110) == 16384);
    CHECK(WORD(images[side], 0x114) == 19466 && WORD(images[side], 0x118) == 6244 && WORD(images[side], 0x11c) == 8182);
    CHECK(WORD(images[side], 0x120) == 8192 && WORD(images[side], 0x124) == (uint32_t)-12288 && WORD(images[side], 0x128) == 28672);
    CHECK(WORD(images[side], 0x4168) == 4096 && WORD(images[side], 0x416c) == 6144);
    CHECK(WORD(images[side], 0x4170) == 12288 && WORD(images[side], 0x4174) == 6144);
    polygon_submit(6, 35, 16);
    polygon_submit(2, 3, 16);
    /* Attribute sign restoration deliberately leaves -1 instead of zero. */
    matrix_case();
    polygon_view[0] = polygon_view[2] = 0x8000;
    global[0xd42] = 0;
    for (component = 0; component < 3; component++) {
        GLOBAL_HALF(0xcfa + component * 2) = 0;
        GLOBAL_HALF(0xd00 + component * 2) = 0;
        WORD(global, 0xd0c + component * 4) = 0;
    }
    GLOBAL_HALF(0xd00) = 0xffff;
    polygon_submit(0, 0, 0x80000000);
    polygon_submit(0, 0x10000, 0x80000000);
    CHECK(WORD(images[side], 0x114) == 0xffffffff && WORD(images[side], 0x120) == 0xffffffff);
    CHECK(WORD(images[side], 0x4174) == 1);
    matrix_case();
    global[0xd42] = 1;
    GLOBAL_HALF(0xcfa) = GLOBAL_HALF(0xd00) = 0x8000;
    WORD(global, 0xd0c) = 0;
    polygon_submit((uint32_t)-2, (uint32_t)-3, 1);
    polygon_submit((uint32_t)-6, (uint32_t)-3, 1);
    CHECK(WORD(images[side], 0x114) == 0xc0000000); /* Sum wraps before SAR. */
    matrix_case();
    global[0xd42] = 0;
    GLOBAL_HALF(0xcfa) = 0x7fff; GLOBAL_HALF(0xd00) = 0; WORD(global, 0xd0c) = 0;
    polygon_submit(0, 0xffff, 0x10000);
    polygon_submit(0x10000, 0x10000, 0x10000);
    CHECK(WORD(images[side], 0x120) == 0x7fff0000); /* Integer slope cap. */
    for (index = 0; index < 48; index++) {
        if (!(index % 8)) matrix_case();
        global[0xd42] = shifts[index % 8];
        for (component = 0; component < 3; component++) {
            GLOBAL_HALF(0xcfa + component * 2) = (uint16_t)random_word();
            GLOBAL_HALF(0xd00 + component * 2) = (uint16_t)random_word();
            WORD(global, 0xd0c + component * 4) = random_word();
        }
        polygon_view[0] = values[random_word() % 8]; polygon_view[2] = values[random_word() % 8];
        polygon_view[1] = random_word(); polygon_view[3] = random_word();
        polygon_submit(random_word(), random_word(), values[index % 8]);
    }
    if (!(scenario % 16)) {
        matrix_case();
        for (index = 0; index < 61; index++) polygon_submit(index * 3, index * 5, index + 1);
        CHECK(WORD(global, 0x2f0) == 60);
        CHECK(WORD(global, 0xc14) == ADDRESS(images[side] + 0x834));
    }
    polygon_compare = 0;
}

static void matrix_selector_case(uint32_t selector)
{
    matrix_case();
    WORD(global, 0x320) = 0x100000;
    GLOBAL_HALF(0xe88) = 100;
    GLOBAL_HALF(0xdce) = !(selector & 4);
    GLOBAL_HALF(0x30c) = 200; GLOBAL_HALF(0xcd4) = 150;
    WORD(global, 0x2e8) = selector & 1 ? 0x10001 : 0x18000;
    WORD(global, 0xcdc) = 0x11223344 ^ scenario; WORD(global, 0xce4) = 0x44556677 + scenario;
    WORD(global, 0xc94) = 0x12345678; WORD(global, 0xc9c) = 0x01020304 ^ scenario;
    WORD(global, 0xca4) = 0x76543210; WORD(global, 0xca8) = 0xfedcba98;
    WORD(global, 0xcac) = 0x456789ab; WORD(global, 0xcb0) = 0x23456789;
    WORD(global, 0xce0) = selector & 2 ? 20 : (uint32_t)-20;
    WORD(global, 0xc98) = (uint32_t)-1;
    WORD(global, 0xca0) = selector & 8 ? 0x10080 : 0x10000;
    WORD(global, 0xcb4) = 0x10000 | (selector & 16 ? 2 : selector & 32 ? 1 : 0);
    WORD(global, 0x1172) = 0x10001; WORD(global, 0x324) = 0x10001;
    WORD(global, 0xcce) = selector & 32 ? 0x10001 : 0x10000;
}

static void matrix_setup_only(uint32_t depth)
{
    polygon_submit(4, 4, depth);
    WORD(images[side], 0x8c8) = 1u << 16; WORD(images[side], 0x8cc) = 2u << 16;
    drawing(0x40, draw_surface(), 0, 0, 0, 0, 0);
    CHECK(!draw_fault_code);
}

static void matrix_far_case(void)
{
    matrix_selector_case(0);
    GLOBAL_HALF(0xdce) = 0;
    polygon_view[0] = polygon_view[2] = 0x8000;
    WORD(global, 0xc8c) = 0; /* Far setup must not dereference the texture. */
    images[side][0x1d06] = (uint8_t)(0x5a + scenario);
    images[side][0x435c] = 0xa7; WORD(images[side], 0x948) = 0x12345678;
}

static void matrix_far_point(int32_t x, int32_t y)
{
    polygon_submit(0u - ((uint32_t)x << 16), 0u - ((uint32_t)y << 16), 0x7fffffff);
}

static void matrix_finish(uint32_t remap)
{
    WORD(global, 0x1298) = remap;
    polygon_compare = remap ? 2 : 1;
    drawing(0x40, draw_surface(), 0, 0, 0, 0, 0);
    CHECK(!draw_fault_code);
    CHECK(WORD(images[side], 0x4200) == 0x20784d41);
    CHECK(images[side][0x435c] == 0xa7 && WORD(images[side], 0x948) == 0x12345678);
}

static void run_matrix_fills(void)
{
    static const uint32_t remaps[] = {0,1,15,16,17,19,31,0xffffffff};
    static const uint32_t scales[] = {0,1,0xffff,0x10000,0x200000,0x7fffffff,0x80000000,0xffffffff};
    uint32_t index, x, y;
    matrix_reset();
    for (index = 0; index < 64; index++) {
        matrix_selector_case(index);
        matrix_setup_only(1);
        CHECK(WORD(images[side], 0x4200) == WORD(images[side], 0x4204 + index * 4));
    }
    for (index = 0; index < 8; index++) {
        matrix_selector_case(4);
        WORD(global, 0x320) = scales[index];
        GLOBAL_HALF(0xe88) = index & 1 ? 0 : 0x7cff;
        GLOBAL_HALF(0x30c) = index & 2 ? 0x8000 : 1;
        GLOBAL_HALF(0xcd4) = index & 4 ? 0 : 0xffff;
        matrix_setup_only(index == 7 ? (uint32_t)-1 : 1);
    }
    {
        static const uint32_t flags[][5] = {
            {2,1,1,1,48}, {2,2,1,1,32}, {2,1,0,1,32}, {2,1,1,0x10000,16},
            {0x8000,1,1,1,0}, {0xffff,1,1,1,0}, {0x7fff,1,1,0x100,32}, {0x10001,1,1,0x10000,0}
        };
        for (index = 0; index < 8; index++) {
            matrix_selector_case(0);
            WORD(global, 0xcb4) = flags[index][0]; WORD(global, 0x1172) = flags[index][1];
            WORD(global, 0x324) = flags[index][2]; WORD(global, 0xcce) = flags[index][3];
            matrix_setup_only(1);
            CHECK(WORD(images[side], 0x4200) == WORD(images[side], 0x4204 + flags[index][4] * 4));
        }
    }
    matrix_selector_case(24);
    matrix_texture[1] = 0; /* Keying skips hardware upload even without 0x40000. */
    matrix_setup_only(1);
    CHECK(WORD(images[side], 0x4200) == WORD(images[side], 0x4204 + 24 * 4));
    for (index = 0; index < 3; index++) {
        matrix_selector_case(4);
        GLOBAL_HALF(0xe88) = index == 2 ? 100 : index == 0 ? 0x7d00 : 0xffff;
        matrix_setup_only(index == 2 ? 0 : 1);
        CHECK(WORD(images[side], 0x4200) == WORD(images[side], 0x4204));
    }
    matrix_selector_case(4);
    matrix_setup_only(0x7ffffffe);
    CHECK(WORD(images[side], 0x4200) == WORD(images[side], 0x4204 + 4 * 4));
    matrix_selector_case(4);
    WORD(global, 0x320) = 1; GLOBAL_HALF(0xe88) = 1; GLOBAL_HALF(0x30c) = 1; GLOBAL_HALF(0xcd4) = 0xffff;
    matrix_setup_only(1);
    CHECK(WORD(images[side], 0x41f8) == 0xffffffff);
    matrix_selector_case(0);
    polygon_submit(4, 4, 1);
    WORD(images[side], 0x8c8) = 0x80000000; WORD(images[side], 0x8cc) = 2u << 16;
    drawing(0x40, draw_surface(), 0, 0, 0, 0, 0);
    CHECK(!draw_fault_code && WORD(images[side], 0x8c8) == 0xffff8000);
    matrix_far_case();
    memcpy(fill_expected, canvas, CANVAS_SIZE);
    matrix_far_point(4, 4); matrix_far_point(12, 4); matrix_far_point(12, 8); matrix_far_point(4, 8);
    matrix_finish(0);
    for (y = 4; y <= 8; y++) for (x = 4; x <= 12; x++)
        fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = (uint8_t)(0x5a + scenario);
    CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    for (index = 0; index < 8; index++) {
        matrix_far_case();
        if (index & 1) {
            matrix_far_point(3, 8); matrix_far_point(3 + (scenario + index) % 33, 8);
            matrix_far_point(3 + (scenario + index) % 33, 3); matrix_far_point(3, 3);
        } else {
            matrix_far_point(4, 4); matrix_far_point(24, 12); matrix_far_point(8, 24);
        }
        matrix_finish(remaps[index]);
    }
    matrix_far_case();
    global[0xd42] = 0;
    GLOBAL_HALF(0xcfa) = 0x4000; GLOBAL_HALF(0xd00) = 0;
    GLOBAL_HALF(0xcfc) = GLOBAL_HALF(0xd02) = 0; WORD(global, 0xd10) = 5;
    GLOBAL_HALF(0xcfe) = GLOBAL_HALF(0xd04) = 0; WORD(global, 0xd14) = 10;
    matrix_far_point(4, 5); matrix_far_point(20, 9); matrix_far_point(8, 14);
    matrix_finish(0);
    matrix_far_case();
    matrix_far_point(-3, 4); matrix_far_point(12, 4); matrix_far_point(12, 8); matrix_far_point(-3, 8);
    matrix_finish(1);
    matrix_far_case();
    matrix_far_point(4, 46); matrix_far_point(12, 46); matrix_far_point(12, 50); matrix_far_point(4, 50);
    matrix_finish(0);
    polygon_compare = 0;
}
