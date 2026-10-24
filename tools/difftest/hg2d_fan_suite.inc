/* Optional software fan rendering, including caller-selected x87 precision. */
static uint32_t fan_vertices[16][5], fan_texture[13];
static uint8_t fan_pixels[0x10000];

static void fan_case(void)
{
    GLOBAL_HALF(0xcb6) = 1; GLOBAL_HALF(0xca0) = 0;
    WORD(global, 0xc8c) = ADDRESS(fan_texture);
    fan_texture[0] = ADDRESS(fan_pixels);
}

static void fan_point(uint32_t index, uint32_t x, uint32_t y, uint32_t u, uint32_t v)
{
    fan_vertices[index][0] = x; fan_vertices[index][1] = y;
    fan_vertices[index][2] = random_word(); fan_vertices[index][3] = u; fan_vertices[index][4] = v;
}

static uint32_t fan_finish(uint32_t kind, uint32_t format, uint32_t count)
{
    uint32_t result = drawing(0xa4, draw_surface(), kind, format, ADDRESS(fan_vertices), count, 0x12345678);
    CHECK(!draw_fault_code);
    return result;
}

static void run_fans(uint32_t rounding)
{
    uint32_t index, permutation, precision, variant, x, y;
    uint16_t saved_control, control;
    static const uint8_t orders[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    static const uint32_t counts[] = {0,1,2,0x7fffffff,0x80000001,0xffffffff};
    __asm__ volatile("fnstcw %0" : "=m"(saved_control));
    control = (uint16_t)((saved_control & ~0xc00u) | (rounding << 10));
    __asm__ volatile("fnclex; fldcw %0" : : "m"(control));
    draw_reset(); fan_case();
    for (index = 0; index < sizeof fan_pixels; index++) fan_pixels[index] = (uint8_t)(index * 13 + (index >> 8) * 29 + scenario);
    for (index = 0; index < 6; index++) {
        CHECK(fan_finish(index == 0 ? 0 : index == 1 ? 7 : 6, index == 2 ? 99 : 100, 0) ==
              (index < 3 ? 0xaabbccddu : 0));
    }
    WORD(global, 0xcb6) = 0x10000;
    CHECK(fan_finish(6, 100, 0) == ADDRESS(global));
    GLOBAL_HALF(0xcb6) = 1; GLOBAL_HALF(0xca0) = 0x100;
    CHECK(fan_finish(6, 100, 0) == ADDRESS(global));
    fan_case();
    GLOBAL_HALF(0xcb6) = 0x100;
    CHECK(fan_finish(6, 100, 0) == 0);
    for (index = 0; index < sizeof counts / sizeof counts[0]; index++) {
        if (counts[index] == 0x7fffffff) continue;
        CHECK(fan_finish(6, 100, counts[index]) == counts[index]);
    }
    memcpy(fill_expected, canvas, CANVAS_SIZE);
    fan_texture[0] = 0x5c;
    fan_point(0, 4u << 16, 4u << 16, 0, 0);
    fan_point(1, 12u << 16, 4u << 16, 0x100000, 0);
    fan_point(2, 4u << 16, 8u << 16, 0, 0x100000);
    CHECK(fan_finish(6, 100, 3) == ADDRESS(fan_vertices[2]));
    for (y = 4; y < 8; y++) for (x = 4; x <= 12 - (y - 4) * 2; x++)
        fill_expected[row_storage[16 + 4] - ADDRESS(canvas) + (int32_t)surface[3] * (int32_t)(y - 4) + x] = 0x5c;
    CHECK(!memcmp(canvas, fill_expected, CANVAS_SIZE));
    for (precision = 0; precision < 3; precision++) {
        control = (uint16_t)((saved_control & ~0xf00u) | (rounding << 10) | (precision == 0 ? 0 : precision == 1 ? 0x200 : 0x300));
        __asm__ volatile("fnclex; fldcw %0" : : "m"(control));
        for (permutation = 0; permutation < 6; permutation++) for (variant = 0; variant < 8; variant++) {
            uint32_t points[3][5] = {
                {3u << 16, 3u << 16, 0, random_word(), random_word()},
                {35u << 16, 12u << 16, 0, random_word(), random_word()},
                {8u << 16, 25u << 16, 0, random_word(), random_word()},
            };
            if (variant == 1) points[1][1] = points[0][1];
            if (variant == 2) points[1][1] = points[2][1];
            if (variant == 3) points[0][1] = points[1][1] = points[2][1] = 0x48000;
            if (variant == 4) for (index = 0; index < 3; index++) points[index][0] = 8u << 16;
            if (variant >= 5) for (index = 0; index < 3; index++) {
                points[index][0] += random_word() & 0xffff;
                points[index][1] += random_word() & 0xffff;
            }
            for (index = 0; index < 3; index++) memcpy(fan_vertices[index], points[orders[permutation][index]], sizeof points[0]);
            fan_texture[0] = variant == 6 ? 254 : variant == 7 ? 0x800000a5 : ADDRESS(fan_pixels);
            fan_finish(6, 100, 3);
        }
    }
    control = (uint16_t)((saved_control & ~0xc00u) | (rounding << 10));
    __asm__ volatile("fnclex; fldcw %0" : : "m"(control));
    fan_case();
    for (variant = 3; variant < 9; variant++) {
        for (index = 0; index < variant; index++)
            fan_point(index, (5 + (index * 17) % 31) << 16, (4 + (index * 7) % 23) << 16, random_word(), random_word());
        fan_finish(6, 100, variant);
    }
    /* Texture and destination share the canvas; each fetch precedes its store. */
    fan_texture[0] = ADDRESS(canvas);
    for (index = 0; index < 4; index++)
        fan_point(index, (4 + (index & 1) * 24) << 16, (6 + (index >> 1) * 8) << 16,
                  random_word(), random_word());
    fan_finish(6, 100, 4);
    fan_texture[0] = ADDRESS(fan_pixels);
    fan_point(0, (0u - 2) << 16, (0u - 3) << 16, random_word(), random_word());
    fan_point(1, 24u << 16, 4u << 16, random_word(), random_word());
    fan_point(2, 4u << 16, 10u << 16, random_word(), random_word());
    uint32_t negative[3][5];
    memcpy(negative, fan_vertices, sizeof negative);
    for (permutation = 0; permutation < 6; permutation++) {
        for (index = 0; index < 3; index++) memcpy(fan_vertices[index], negative[orders[permutation][index]], sizeof negative[0]);
        fan_finish(6, 100, 3);
    }
    /* 255 is a texture pointer, while signed values below 255 are colours. */
    fan_texture[0] = 255;
    fan_point(0, 4u << 16, 4u << 16, 0, 0);
    fan_point(1, 12u << 16, 4u << 16, 0, 0);
    fan_point(2, 4u << 16, 8u << 16, 0, 0);
    __asm__ volatile("fnclex");
    drawing(0xa4, draw_surface(), 6, 100, ADDRESS(fan_vertices), 3, 0);
    CHECK(draw_fault_code == EXCEPTION_ACCESS_VIOLATION && draw_fault_access == 0 && draw_fault_address == 255);
    __asm__ volatile("fnclex; fldcw %0" : : "m"(saved_control));
}
