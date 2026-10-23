/* +0x50/+0x54 textured shading, complete canvas/image and seven registers. */
static uint8_t perspective_pixels[0x10000];
static uint32_t perspective_texture[1];

static void perspective_fill_case(uint32_t dither, uint32_t distance)
{
    polygon_begin();
    WORD(global, 0xcb6) = 1;
    WORD(global, 0xc8c) = ADDRESS(perspective_texture);
    WORD(global, 0xcce) = dither;
    GLOBAL_HALF(0xdce) = distance ? 0 : 1;
    WORD(global, 0xd4c) = distance == 2 ? 5000 * 256 : 400 * 256;
    /* These select other affine paths but are ignored by +0x54. */
    GLOBAL_HALF(0xca0) = 128;
    GLOBAL_HALF(0xc92) = scenario & 1;
    WORD(global, 0x1298) = scenario & 1;
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
}

static void perspective_fill_vertex(uint32_t x, uint32_t y, uint32_t u, uint32_t v,
                                    uint32_t shade, uint32_t depth)
{
    uint32_t arguments[7] = {ADDRESS(polygon_record), x, y, depth, v, u, shade};
    drawing_arguments(0x50, arguments, 7);
    CHECK(!draw_fault_code);
}

static void perspective_fill_point(uint32_t x, uint32_t y)
{
    perspective_fill_vertex(x, y, (x * 3 + y * 2) << 16, (y * 5 + x) << 16,
                            0x06000000 + (x + y) * 0x01321234, 1);
}

static void perspective_fill_finish(void)
{
    drawing(0x54, draw_surface(), 0, 0, 0, 0, 0);
    CHECK(!draw_fault_code);
}

static void run_perspective_fill(void)
{
    static const uint32_t dithers[] = {0, 1, 0x100, 0x10000};
    uint32_t index, mode, alignment, first, last, x, y;
    draw_reset();
    polygon_compare = 1;
    shaded_map_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    perspective_texture[0] = ADDRESS(perspective_pixels);
    for (index = 0; index < sizeof perspective_pixels; index++)
        perspective_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    for (index = 0; index < 0x2000; index++)
        images[side][0x4e00 + index] = (uint8_t)(index * 19 + (index >> 8) * 7 + scenario);
    /* Plain fallback retains the closer's saved-register rotation. */
    perspective_fill_case(0, 0);
    WORD(global, 0x1298) = 0;
    WORD(global, 0xcb6) = 0x10000;
    perspective_fill_point(4, 4); perspective_fill_point(12, 4);
    perspective_fill_point(12, 8); perspective_fill_point(4, 8);
    perspective_fill_finish();
    /* Independent constant texture/remap rectangle, including all padding. */
    perspective_fill_case(0, 0);
    memset(perspective_pixels, 0xa5, sizeof perspective_pixels);
    memset(images[side] + 0x4e00, 0x37, 0x2000);
    memcpy(fill_expected, canvas, CANVAS_SIZE);
    perspective_fill_point(4, 4); perspective_fill_point(12, 4);
    perspective_fill_point(12, 8); perspective_fill_point(4, 8);
    perspective_fill_finish();
    for (y = 4; y <= 8; y++)
        for (x = 4; x <= 12; x++)
            fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = images[side][0xd70 + 0x37];
    CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    for (index = 0; index < sizeof perspective_pixels; index++)
        perspective_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    for (index = 0; index < 0x2000; index++)
        images[side][0x4e00 + index] = (uint8_t)(index * 19 + (index >> 8) * 7 + scenario);
    for (mode = 0; mode < 4; mode++) for (alignment = 0; alignment < 4; alignment++) {
        perspective_fill_case(dithers[mode], mode % 3);
        first = 3 + alignment; last = first + (scenario * 3 + mode * 7 + alignment) % 33;
        y = 3 + scenario % 4;
        if (scenario & 1) {
            perspective_fill_point(first, y + 4); perspective_fill_point(last, y + 4);
            perspective_fill_point(last, y); perspective_fill_point(first, y);
        } else {
            perspective_fill_point(first, y); perspective_fill_point(last, y);
            perspective_fill_point(last, y + 4); perspective_fill_point(first, y + 4);
        }
        perspective_fill_finish();
    }
    for (index = 0; index < 6; index++) {
        perspective_fill_case(index & 1, index % 3);
        if (index == 0) {
            perspective_fill_point(4, 4); perspective_fill_point(24, 12); perspective_fill_point(8, 24);
        } else if (index == 1) {
            perspective_fill_point(4, 4); perspective_fill_point(24, 24);
            perspective_fill_point(4, 24); perspective_fill_point(24, 4);
        } else if (index == 2) {
            perspective_fill_point(7, 10);
        } else if (index == 3) {
            perspective_fill_point(3, 9); perspective_fill_point(24, 9);
        } else if (index == 4) {
            perspective_fill_point(8, 8); perspective_fill_point(8, 16); perspective_fill_point(8, 24);
        } else {
            perspective_fill_point(4, 4); perspective_fill_point(24, 4); perspective_fill_point(24, 24);
            perspective_fill_point(12, 12); perspective_fill_point(4, 24);
        }
        perspective_fill_finish();
    }
    for (index = 0; index < 8; index++) {
        perspective_fill_case(index & 1, index % 3);
        perspective_fill_vertex(4, 5, random_word(), random_word(), random_word(), 1);
        perspective_fill_vertex(24, 12, random_word(), random_word(), random_word(), 1);
        perspective_fill_vertex(8, 24, random_word(), random_word(), random_word(), 1);
        perspective_fill_finish();
    }
    perspective_fill_case(1, 1);
    perspective_fill_vertex(9, 10, 0x7fffffff, 0x80000000, 0x7fffffff, 3);
    perspective_fill_vertex(12, 14, 0x80000000, 0x7fffffff, 0x80000000, 3);
    perspective_fill_vertex(24, 17, 0xffffffff, 1, 0xffffffff, 3);
    perspective_fill_finish();
    /* Across 1,024 scenarios, independently check all 32 lookup rows and
     * all 256 texels. The upper rows intentionally read past the normal
     * sixteen-row table, so their fixture bytes are also deterministic. */
    for (index = 0; index < 8; index++) {
        uint32_t bank = (scenario % 4) * 8 + index;
        uint32_t shade = 0x06000000u + (bank << 27);
        uint8_t value;
        perspective_fill_case(0, 0);
        perspective_pixels[0] = (uint8_t)((scenario >> 2) + index * 29);
        value = images[side][0x4e00 + bank * 256 + perspective_pixels[0]];
        value = images[side][0xd70 + value];
        perspective_fill_vertex(4, 9, 0, 0, shade, 1);
        perspective_fill_vertex(4, 9, 0, 0, shade, 1);
        perspective_fill_finish();
        CHECK(canvas[row_storage[16 + 9] - ADDRESS(canvas) + 4] == value);
    }
    perspective_pixels[0] = (uint8_t)scenario;
    perspective_fill_case(1, 1);
    GLOBAL_HALF(0xe88) = 0;
    perspective_fill_point(4, 4); perspective_fill_point(12, 4);
    perspective_fill_point(12, 8); perspective_fill_point(4, 8);
    perspective_fill_finish();
    GLOBAL_HALF(0xe88) = 100;
    /* A one-pixel flat span reaches both signed shifted-dividend repairs. */
    perspective_fill_case(1, 1);
    perspective_fill_vertex(4, 4, 0, 0, 0x06000000, 1);
    perspective_fill_vertex(4, 4, 0x7fffffff, 0x80000000, 0x7fffffff, 1);
    perspective_fill_finish();
    /* A plausible half-record makes an incorrect 20-byte search observable. */
    perspective_fill_case(0, 0);
    perspective_fill_vertex(4, 5, 7, 5, 0x06000000, 1);
    perspective_fill_vertex(20, 9, 47, 13, 0x10000000, 1);
    perspective_fill_vertex(8, 14, 100, 19, 0x20000000, 1);
    perspective_fill_finish();
    /* The second texture read must precede the first destination write. */
    perspective_fill_case(0, 0);
    perspective_texture[0] = ADDRESS(canvas);
    index = row_storage[16 + 9] - ADDRESS(canvas) + 3;
    perspective_fill_vertex(4, 9, (index & 0xff00) << 8, (index & 255) << 16, 0x06000000, 1);
    perspective_fill_vertex(11, 9, (index & 0xff00) << 8, ((index & 255) + 8) << 16, 0x06000000, 1);
    perspective_fill_finish();
    perspective_texture[0] = ADDRESS(perspective_pixels);
    /* +0x54 has no 1,500-pixel software affine width rejection. */
    perspective_fill_case(0, 0);
    perspective_fill_vertex(0, 9, 0, 0, 0x06000000, 1);
    perspective_fill_vertex(1500, 9, 0x100000, 0x200000, 0x60000000, 1);
    perspective_fill_finish();
    /* No edge begins on the chosen row: keep the texture setup return. */
    perspective_fill_case(1, 0);
    perspective_fill_point(4, 4);
    WORD(images[side], 0x8c8) = 1u << 16;
    WORD(images[side], 0x8cc) = 2u << 16;
    draw_ecx_pointer = 1;
    perspective_fill_finish();
    draw_ecx_pointer = 0;
    CHECK(draw_registers[2] == WORD(global, 0xc14));
    CHECK(draw_registers[0] == ADDRESS(perspective_pixels));
    polygon_compare = 0;
    shaded_map_compare = 0;
}
