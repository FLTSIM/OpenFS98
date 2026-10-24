/* Polygon close/fill through the public vertex and close entries. */
static uint8_t fill_expected[CANVAS_SIZE];

static void fill_begin(uint32_t mode, uint32_t slow)
{
    uint32_t index;
    polygon_begin();
    for (index = 0x104; index < 0x884; index += 16) {
        WORD(images[side], index) = 0xffffffffu;
        WORD(images[side], index + 4) = 47;
        WORD(images[side], index + 8) = 0;
        WORD(images[side], index + 12) = 0;
    }
    WORD(global, 0xc90) = 0x10000;
    WORD(global, 0x2f4) = slow ? slow : 0x10000;
    WORD(global, 0x1298) = 0;
    WORD(global, 0x3c0) = (mode ? 0x6800 : 0x6900) | 0xa5;
    GLOBAL_HALF(0xb34) = mode >= 2 ? (uint16_t)(0u - (0x100 + scenario % 0x7f00)) : 0;
    if (mode >= 2 && !(scenario % 16)) GLOBAL_HALF(0xb34) = 0x8000;
    GLOBAL_HALF(0xdce) = mode == 2;
    if (mode >= 2) images[side][0xb4e + mode * 17] ^= 0x3f;
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
}

static void fill_close(uint32_t remap)
{
    uint32_t unused = WORD(global, 0x2f0) + 1;
    /* The fast scanner deliberately reads one record beyond the closing
     * edge. Its stale intersection is odd and ignored for a plain rectangle,
     * but its x advance and scratch write must still be observed. */
    WORD(images[side], 0x104 + unused * 16) = 0;
    WORD(images[side], 0x108 + unused * 16) = 47;
    WORD(images[side], 0x10c + unused * 16) = 20u << 16;
    WORD(images[side], 0x110 + unused * 16) = 123;
    WORD(global, 0x1298) = remap;
    if (remap) polygon_compare = 2;
    drawing(0x40, draw_surface(), 0, 0, 0, 0, 0);
    CHECK(!draw_fault_code);
}

static void run_fills(void)
{
    static const int32_t shapes[][12][2] = {
        {{4,4},{12,4},{12,8},{4,8}},
        {{3,3},{24,8},{9,24}},
        {{16,2},{28,12},{16,28},{2,12}},
        {{2,3},{26,3},{26,10},{10,10},{10,23},{2,23}},
        {{4,4},{28,28},{4,28},{28,4}},
        {{2,4},{5,24},{8,4},{11,24},{14,4},{17,24},{20,4},{23,24},{26,4},{29,24},{32,4},{35,24}},
        {{5,12},{5,12},{5,12},{5,12}},
        {{3,15},{18,15}},
        {{7,13}},
        {{-3,5},{12,5},{12,12},{-3,12}},
        {{9,5},{10,5},{10,15},{9,15}},
        {{10,10},{10,19},{10,25}},
        {{4,46},{12,46},{12,50},{4,50}},
        {{4,-1},{12,5},{12,9},{4,5}},
    };
    static const uint32_t counts[] = {4,3,4,6,4,12,4,2,1,4,4,3,4,4};
    uint32_t index, shape, vertex, choice;
    draw_reset();
    polygon_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    for (index = 0; index < 256; index++) {
        images[side][0xd70 + index] = (uint8_t)index;
        images[side][0xa4e + index] = (uint8_t)(index * 37 + 1);
        images[side][0xb4e + index] = (uint8_t)(index + scenario);
        images[side][0x4135 + index] = (uint8_t)(index * 13 + 7);
    }
    HALF(images[side], 0x34c0 + 0xa5 * 2) = 0xa5;
    WORD(images[side], 0x298c) = 3;
    memset(canvas, 0x17, CANVAS_SIZE);
    memset(fill_expected, 0x17, CANVAS_SIZE);
    for (shape = 0; shape < sizeof counts / sizeof counts[0]; shape++) {
        choice = shape ? (scenario + shape) % 4 : 0;
        fill_begin(choice, shape == 2 || shape == 10 ? 1 : 0);
        for (vertex = 0; vertex < counts[shape]; vertex++) {
            uint32_t at = scenario & 1 ? counts[shape] - 1 - vertex : vertex;
            polygon_submit((uint32_t)shapes[shape][at][0], (uint32_t)shapes[shape][at][1], 1);
        }
        if (!shape) GLOBAL_HALF(0xdce) = 1;
        fill_close(0);
        if (!shape) {
            uint32_t x, y;
            for (y = 4; y <= 8; y++)
                for (x = 4; x <= 12; x++)
                    fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = 0xa5;
            for (index = 0; index < CANVAS_SIZE; index++)
                if (canvas[index] != fill_expected[index]) {
                    printf("rectangle fixture pixel +%x: expected %02x, got %02x\n", index, fill_expected[index], canvas[index]);
                    CHECK(0);
                }
        }
    }
    /* Fractional projections and arbitrary winding over bounded screen rows. */
    for (shape = 0; shape < 4; shape++) {
        fill_begin((scenario + shape) % 4, shape & 1);
        polygon_view[0] = polygon_view[2] = 3;
        polygon_view[1] = 16; polygon_view[3] = 16;
        if (shape == 2) {
            static const uint32_t colours[] = {0x6703, 0xe0a5, 0xf030, 0x6910};
            WORD(global, 0x3c0) = colours[scenario % 4];
            GLOBAL_HALF(0xcc8) = (uint16_t)random_word();
        }
        for (vertex = 0; vertex < 3 + shape; vertex++)
            polygon_submit(random_word() % 21 - 10, random_word() % 21 - 10, 3 + random_word() % 4);
        fill_close(0);
    }
    /* Polygon span callers restore EBP around the mapper, unlike the
     * primitive entry. Test both map banks and out-of-range fallback. */
    for (shape = 0; shape < 8; shape++) {
        static const uint32_t remaps[] = {1, 15, 16, 17, 19, 20, 31, 0xffffffffu};
        fill_begin(0, 0);
        polygon_submit(4, 4, 1); polygon_submit(12, 4, 1);
        polygon_submit(12, 8, 1); polygon_submit(4, 8, 1);
        fill_close(remaps[shape]);
    }
    /* Exercise the maximum post-clamp shadow length and both signs around
     * 0x8000. Alias rows within the guarded 64K canvas so these wide spans
     * test the public callers without requiring a huge framebuffer. */
    surface[1] = 32768; surface[3] = 0;
    for (index = 0; index < 160; index++) row_storage[index] = ADDRESS(canvas);
    for (shape = 0; shape < 50; shape++) {
        static const uint32_t boundaries[] = {0,1,0x7ffe,0x7fff,0x8000,0xffff};
        uint32_t first = shape >= 48 ? 0 : boundaries[shape % 6], last = shape >= 48 ? 0x7fff : boundaries[(shape / 6) % 6];
        fill_begin(shape & 1 ? 2 : 3, shape & 2 ? 1 : 0);
        polygon_submit(first, 0, 1); polygon_submit(last, 0, 1);
        polygon_submit(last, 1, 1); polygon_submit(first, 1, 1);
        memcpy(fill_expected, canvas, CANVAS_SIZE);
        fill_close(0);
        CHECK(!memcmp(canvas + 0x8000, fill_expected + 0x8000, CANVAS_SIZE - 0x8000));
    }
    polygon_compare = 0;
}
