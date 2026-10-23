/* Affine +0x4c, both span backends, public calls and full image state. */
static uint8_t affine_pixels[0x10000];
static uint32_t affine_texture[1];
static uint32_t affine_backend;

static void affine_case(uint32_t selector)
{
    polygon_begin();
    WORD(global, 0xcb6) = 1;
    WORD(global, 0xc8c) = ADDRESS(affine_texture);
    GLOBAL_HALF(0xc92) = (selector & 8) ? 1 : 0;
    GLOBAL_HALF(0xca0) = (selector & 2) ? 128 : 0;
    global[0xcce] = (selector & 4) != 0;
    GLOBAL_HALF(0xdce) = (selector & 1) ? 0 : 1;
    WORD(global, 0xd4c) = 400 * 256;
    WORD(global, 0x1298) = 0;
    WORD(images[side], 0x4678) = affine_backend;
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
}

static void affine_vertex(int32_t x, int32_t y)
{
    textured_submit((uint32_t)x, (uint32_t)y, 1, (uint32_t)(y * 3 + x * 2) << 16, (uint32_t)(x * 5) << 16);
}

static void affine_finish(void)
{
    drawing(0x4c, draw_surface(), 0, 0, 0, 0, 0);
    CHECK(!draw_fault_code);
}

static void run_affine(void)
{
    uint32_t selector, index, width;
    draw_reset();
    polygon_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    affine_texture[0] = ADDRESS(affine_pixels);
    for (index = 0; index < sizeof affine_pixels; index++)
        affine_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    /* Fallback reproduces the saved-register rotation in +0x4c. */
    affine_case(0);
    WORD(global, 0xcb6) = 0x10000;
    affine_vertex(4, 4); affine_vertex(12, 4);
    affine_vertex(12, 8); affine_vertex(4, 8);
    affine_finish();
    affine_case(8);
    memset(affine_pixels, 0xa5, sizeof affine_pixels);
    memcpy(fill_expected, canvas, CANVAS_SIZE);
    affine_vertex(4, 4); affine_vertex(12, 4); affine_vertex(12, 8); affine_vertex(4, 8);
    affine_finish();
    for (index = 4; index <= 8; index++) {
        uint32_t x;
        for (x = 4; x <= 12; x++) fill_expected[row_storage[16 + index] - ADDRESS(canvas) + x] = 0xa5;
    }
    CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    for (index = 0; index < sizeof affine_pixels; index++)
        affine_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    for (selector = 0; selector < 16; selector++) {
        affine_case(selector);
        width = 1 + (scenario + selector * 7) % 33;
        if (scenario & 1) {
            affine_vertex(4, 6); affine_vertex(4 + width, 6);
            affine_vertex(4 + width, 3); affine_vertex(4, 3);
        } else {
            affine_vertex(3, 3); affine_vertex(3 + width, 3);
            affine_vertex(3 + width, 6); affine_vertex(3, 6);
        }
        affine_finish();
    }
    for (index = 0; index < 6; index++) {
        affine_case((scenario + index) & 15);
        if (index == 0) {
            affine_vertex(4, 4); affine_vertex(24, 12); affine_vertex(8, 24);
        } else if (index == 1) {
            affine_vertex(4, 4); affine_vertex(24, 24); affine_vertex(4, 24); affine_vertex(24, 4);
        } else if (index == 2) {
            affine_vertex(7, 10);
        } else if (index == 3) {
            affine_vertex(3, 9); affine_vertex(24, 9);
        } else if (index == 4) {
            affine_vertex(8, 8); affine_vertex(8, 16); affine_vertex(8, 24);
        } else {
            GLOBAL_HALF(0xdce) = 0;
            WORD(global, 0xd4c) = 5000 * 256;
            WORD(global, 0x1298) = scenario & 1;
            affine_vertex(4, 4); affine_vertex(12, 4); affine_vertex(12, 8); affine_vertex(4, 8);
        }
        affine_finish();
    }
    for (index = 0; index < 4; index++) {
        affine_case(8 | (index & 1));
        textured_submit(4, 4, 1, 0, 0);
        textured_submit(5, 4, 1, 0x7fffffff, 0x80000000);
        textured_submit(5, 8, 1, 0x80000000, 0x7fffffff);
        textured_submit(4, 8, 1, 0xffffffff, 1);
        affine_finish();
    }
    affine_case(8);
    textured_submit(4, 4, 1, 0, 0);
    textured_submit(4, 4, 1, 0x7fffffff, 0x80000000);
    affine_finish();
    affine_case(8);
    textured_submit(4, 5, 1, 7, 5);
    textured_submit(20, 9, 1, 11, 9);
    textured_submit(8, 14, 1, 19, 14);
    affine_finish();
    for (index = 0; index < 3; index++) {
        affine_case(8);
        affine_vertex(0, 3); affine_vertex(1498 + index, 3);
        affine_vertex(1498 + index, 4); affine_vertex(0, 4);
        affine_finish();
    }
    polygon_compare = 0;
}

static void run_optimized(void)
{
    uint32_t selector, index;
    draw_reset();
    polygon_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    affine_texture[0] = ADDRESS(affine_pixels);
    for (index = 0; index < sizeof affine_pixels; index++)
        affine_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    affine_backend = 16;
    for (selector = 0; selector < 16; selector++) {
        affine_case(selector);
        uint32_t start = (scenario + selector) % 4 + 3;
        uint32_t end = start + (scenario * 3 + selector) % 33;
        if (scenario & 1) {
            affine_vertex(start, 7); affine_vertex(end, 7);
            affine_vertex(end, 3); affine_vertex(start, 3);
        } else {
            affine_vertex(start, 3); affine_vertex(end, 3);
            affine_vertex(end, 7); affine_vertex(start, 7);
        }
        affine_finish();
        affine_case(selector);
        textured_submit(4, 5, 1, random_word() % 0xc00000, random_word() % 0xc00000);
        textured_submit(24, 12, 1, random_word() % 0xc00000, random_word() % 0xc00000);
        textured_submit(8, 24, 1, random_word() % 0xc00000, random_word() % 0xc00000);
        affine_finish();
        affine_case(selector);
        affine_vertex(start, 9); affine_vertex(end, 9);
        affine_finish();
    }
    affine_case(8);
    memset(affine_pixels, 0xa5, sizeof affine_pixels);
    memcpy(fill_expected, canvas, CANVAS_SIZE);
    affine_vertex(4, 4); affine_vertex(12, 4); affine_vertex(12, 8); affine_vertex(4, 8);
    affine_finish();
    for (index = 4; index <= 8; index++) {
        uint32_t x;
        for (x = 4; x <= 12; x++) fill_expected[row_storage[16 + index] - ADDRESS(canvas) + x] = 0xa5;
    }
    CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    /* Bits above the usual V byte overlap U: keyed spans OR the fields. */
    affine_case(10);
    affine_pixels[0x100] = 0xe1; affine_pixels[0x200] = 0xe2;
    textured_submit(4, 9, 1, 0x01000000, 0x00010000);
    textured_submit(16, 9, 1, 0x01000000, 0x00010000);
    affine_finish();
    /* Texture aliases the destination two pixels behind: batch reads precede stores. */
    affine_case(8 | (scenario & 1));
    affine_texture[0] = ADDRESS(canvas);
    index = row_storage[16 + 9] - ADDRESS(canvas) + 2;
    textured_submit(4, 9, 1, (index & 255) << 16, (index & 0xff00) << 8);
    textured_submit(31, 9, 1, ((index & 255) + 28) << 16, (index & 0xff00) << 8);
    affine_finish();
    affine_texture[0] = ADDRESS(affine_pixels);
    /* No edge starts on the chosen row: setup's EAX remains observable. */
    affine_case(8);
    textured_submit(4, 4, 1, 0, 0);
    WORD(images[side], 0x8c8) = 1u << 16;
    WORD(images[side], 0x8cc) = 2u << 16;
    affine_finish();
    affine_backend = 0;
    polygon_compare = 0;
}
