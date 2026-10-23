/* Table-shaped discs, all 48 profiles, word clipping and doubled rows. */
static uint32_t disc_record[40], disc_view[4];

static void disc_submit(uint32_t radius, uint32_t x, uint32_t y)
{
    uint32_t arguments[4] = {ADDRESS(disc_record), radius, x, y};
    drawing_arguments(0x84, arguments, 4);
    CHECK(!draw_fault_code);
}

static void run_discs(void)
{
    uint32_t index, x, y;
    draw_reset();
    memset(disc_record, 0, sizeof disc_record);
    disc_record[7] = ADDRESS(disc_view);
    memcpy(disc_record + 32, surface, sizeof surface);
    disc_view[0] = 32; disc_view[1] = 32;
    disc_view[2] = (uint32_t)-24; disc_view[3] = 24;
    WORD(global, 0x1298) = 0;
    WORD(global, 0x3c0) = 0x6917;
    memcpy(fill_expected, canvas, CANVAS_SIZE);
    disc_submit(0, 16, 12);
    for (y = 10; y <= 13; y++) {
        static const uint32_t widths[4] = {2, 3, 2, 1};
        for (x = 16 - widths[y - 10]; x <= 16 + widths[y - 10]; x++)
            fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = images[side][0x90c];
    }
    CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    for (index = 0; index < 96; index++) {
        WORD(global, 0x3c0) = (index & 1 ? 0x6800 : 0x6900) | (scenario & 255);
        disc_submit(index | (scenario << 16), 32, 24);
    }
    for (index = 0; index < 24; index++) {
        WORD(global, 0x3c0) = (index & 1 ? 0x6800 : 0x6900) | (scenario & 255);
        disc_view[0] = 8 + index % 3;
        disc_view[1] = index & 2 ? 0 : 32;
        disc_view[2] = (uint32_t)-(int32_t)(index % 9);
        disc_view[3] = index & 4 ? index % 9 : 24;
        disc_submit((scenario + index) % 128, (index & 8 ? 0 : 63), index & 16 ? 0 : 47);
    }
    disc_view[0] = 32; disc_view[1] = 32;
    disc_view[2] = 24; disc_view[3] = 24; /* Reversed vertical clip range. */
    disc_submit(0xffff, 32, 24);
    disc_view[2] = (uint32_t)-24;
    disc_submit(0xffff, 32, 24);
    for (index = 0; index < 16; index++) {
        WORD(global, 0x1298) = 1;
        WORD(global, 0x3c0) = 0x6901;
        disc_submit(index * 7, index % 4 + 16, 24);
    }
    WORD(global, 0x1298) = 0;
    for (index = 0; index < 8; index++) {
        WORD(global, 0x3c0) = 0xf000 | (scenario + index) % 32;
        GLOBAL_HALF(0xcc8) = index & 1 ? 0x1700 : 0x3000;
        GLOBAL_HALF(0xcc2) = index & 1 ? 0x3000 : 0x1700;
        disc_submit(index, 32, 24);
    }
    disc_view[0] = 0;
    WORD(global, 0x3c0) = 0x6817;
    for (index = 0; index < 4; index++) disc_submit(index, 32, 12 + index);
    disc_view[0] = 32;
    disc_view[2] = (uint32_t)-64; disc_view[3] = 64;
    disc_record[32 + 2] = 128;
    WORD(global, 0x1298) = 1; WORD(global, 0x3c0) = 0x6917;
    disc_submit(47, 32, 64);

}
