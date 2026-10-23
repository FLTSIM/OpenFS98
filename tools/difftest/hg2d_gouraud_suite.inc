/* +0x74/+0x78 shaded edges and polygons through the public entries. */
static uint32_t gouraud_submit(uint32_t x, uint32_t y, uint32_t depth, uint32_t colour)
{
    uint32_t result = drawing(0x74, ADDRESS(polygon_record), x, y, depth, colour, 0);
    CHECK(!draw_fault_code);
    return result;
}

static void run_gouraud(void)
{
    static const uint32_t values[] = {0,1,2,0xffff,0x10000,0x7fffffff,0x80000000,0xffffffff};
    uint32_t index, shape, width;
    draw_reset();
    polygon_compare = 1;
    memset(polygon_record, 0, sizeof polygon_record);
    polygon_record[7] = ADDRESS(polygon_view);
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
    polygon_begin();
    CHECK(gouraud_submit(2,3,1,0x12341000) == 0x30000);
    CHECK(draw_registers[2] == 0x20000 && draw_registers[3] == 0);
    CHECK(HALF(images[side],0x90d) == 0x1000 && HALF(images[side],0x90f) == 0x1000);
    CHECK(gouraud_submit(6,3,1,0x3000) == ADDRESS(global));
    CHECK(WORD(images[side],0x10c) == 0x28000 && WORD(images[side],0x110) == 0x40000);
    CHECK(WORD(images[side],0x114) == 0x100000 && WORD(images[side],0x118) == 0x200000);
    gouraud_submit(6,7,1,0x5000);
    CHECK(WORD(images[side],0x12c) == 0x37fff8 && WORD(images[side],0x130) == 0x80000);
    for (index = 0; index < 24; index++) {
        if (!(index % 6)) polygon_begin();
        polygon_view[0] = values[random_word() % 8]; polygon_view[1] = random_word();
        polygon_view[2] = values[random_word() % 8]; polygon_view[3] = random_word();
        gouraud_submit(random_word(), random_word(), values[index % 8], random_word());
    }
    polygon_view[0] = polygon_view[2] = 1;
    polygon_view[1] = polygon_view[3] = 0;
    if (!(scenario % 16)) {
        polygon_begin();
        for (index = 0; index < 81; index++)
            gouraud_submit(index * 3, index * 5, index + 1, index * 613);
        CHECK(WORD(global,0xc14) == ADDRESS(images[side] + 0x884));
    }
    for (shape = 0; shape < 12; shape++) {
        polygon_begin();
        GLOBAL_HALF(0xdce) = shape & 1;
        WORD(global,0xd4c) = (scenario % 25) * 100 * 256;
        width = 1 + (shape * 7 + scenario) % 32;
        if (shape == 0) {
            GLOBAL_HALF(0xdce) = 1;
            for (index = 0; index < 256; index++) images[side][0xd70 + index] = (uint8_t)index;
            memcpy(fill_expected, canvas, CANVAS_SIZE);
            gouraud_submit(4,4,1,0x1000); gouraud_submit(12,4,1,0x1000);
            gouraud_submit(12,8,1,0x1000); gouraud_submit(4,8,1,0x1000);
        } else if (shape == 1) {
            gouraud_submit(4,4,1,0x1000); gouraud_submit(24,12,1,0x8000); gouraud_submit(8,24,1,0xe000);
        } else if (shape == 2) {
            gouraud_submit(4,4,1,0); gouraud_submit(24,24,1,0xffff);
            gouraud_submit(4,24,1,0x4000); gouraud_submit(24,4,1,0xc000);
        } else if (shape == 3) {
            gouraud_submit(7,13,1,0x7080);
        } else if (shape == 4) {
            gouraud_submit(4,12,1,0); gouraud_submit(24,12,1,0xff00);
        } else if (scenario & 1) {
            gouraud_submit(4,8,1,random_word()); gouraud_submit(4 + width,8,1,random_word());
            gouraud_submit(4 + width,4,1,random_word()); gouraud_submit(4,4,1,random_word());
        } else {
            gouraud_submit(3,4,1,random_word()); gouraud_submit(3 + width,4,1,random_word());
            gouraud_submit(3 + width,8,1,random_word()); gouraud_submit(3,8,1,random_word());
        }
        drawing(0x78,draw_surface(),0,0,0,0,0);
        CHECK(!draw_fault_code);
        if (shape == 0) {
            uint32_t x, y;
            for (y = 4; y <= 8; y++)
                for (x = 4; x <= 12; x++) fill_expected[row_storage[16 + y] - ADDRESS(canvas) + x] = 0x10;
            CHECK(!memcmp(fill_expected, canvas, CANVAS_SIZE));
        }
    }
    polygon_begin();
    gouraud_submit(4u << 16, 5u << 16, 0x10000, 0x1000);
    gouraud_submit((4u << 16) + 5, 6u << 16, 0x10000, 0x4000);
    gouraud_submit(8u << 16, 9u << 16, 0x10000, 0x9000);
    drawing(0x78,draw_surface(),0,0,0,0,0);
    CHECK(!draw_fault_code);
    polygon_compare = 0;
}
