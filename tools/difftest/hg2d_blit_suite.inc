/* Nine-argument +0x80 packed-coordinate scanline drawing. */
static uint32_t blit_record[40];

static uint32_t blit_call(uint32_t row_mode, uint32_t first, uint32_t last,
                          uint32_t u, uint32_t v, uint32_t end_u, uint32_t end_v, const uint8_t *texture)
{
    uint32_t arguments[9] = {ADDRESS(blit_record),row_mode,first,last,u,v,end_u,end_v,ADDRESS(texture)};
    uint32_t result = drawing_arguments(0x80,arguments,9);
    CHECK(!draw_fault_code);
    return result;
}

static void run_blits(void)
{
    static const uint32_t lengths[] = {1,2,3,4,5,7,8,9,15,16,17};
    uint32_t index, mode, alignment, length;
    draw_reset();
    memset(blit_record,0,sizeof blit_record);
    blit_record[0x88 / 4] = 48;
    blit_record[0x90 / 4] = surface[4];
    for (index = 0; index < sizeof affine_pixels; index++)
        affine_pixels[index] = (uint8_t)(index * 17 + (index >> 8) * 29 + scenario);
    for (index = 0; index < 256; index++) images[side][0x94e + index] = (uint8_t)(index * 37 + scenario);
    HALF(images[side],0x34ca) = 0x105;
    images[side][0x94e + 0x105] = 0x5a;
    images[side][0x29a0] = 10; images[side][0x29a1] = 20;
    images[side][0x29a2] = 30; images[side][0x29a3] = 40;
    for (mode = 0; mode < 16; mode++)
        for (alignment = 0; alignment < 4; alignment++) {
            length = lengths[(scenario + mode + alignment) % 11];
            blit_call(0xabcd0000u | (mode << 12) | (mode + alignment),alignment,alignment + length - 1,
                      0x400,0x7600,0x2800,0x8600,affine_pixels);
        }
    memcpy(fill_expected,canvas,CANVAS_SIZE);
    blit_call(0x1007,3,14,0,0,0,0,affine_pixels);
    for (index = 3; index <= 14; index++) fill_expected[row_storage[16 + 7] - ADDRESS(canvas) + index] = 0x5a;
    CHECK(!memcmp(fill_expected,canvas,CANVAS_SIZE));
    for (mode = 0; mode <= 8; mode++) {
        blit_call((mode << 12) | 48,4,8,0,0,0,0,affine_pixels);
        blit_call((mode << 12) | 4,5,3,0x8000,0x9000,0x1000,0x2000,affine_pixels);
        blit_call((mode << 12) | 4,5,4,0x1000,0x9000,0x8000,0x2000,affine_pixels);
    }
    blit_call(0x200,0,4,0,0,0,0,affine_pixels);
    blit_call(0x2005,4,6,0,0x7f00,0,0x7f00,affine_pixels);
    blit_call(0x4005,4,6,0,0x7f00,0,0x7f00,affine_pixels);
    blit_record[0x88 / 4] = 0xffffffffu;
    blit_call(0,0,4,0,0,0,0,affine_pixels);
    blit_record[0x88 / 4] = 48;
    for (index = 0; index < 24; index++) {
        mode = random_word() % 16;
        blit_call((mode << 12) | (random_word() % 48),random_word() % 16,16 + random_word() % 32,
                  random_word(),random_word(),random_word(),random_word(),affine_pixels);
    }
    for (index = 0; index < 2; index++) {
        uint32_t source = row_storage[16] - ADDRESS(canvas) + 2;
        blit_call(index << 15,4,19,(source & 0xff) << 8,source & 0xff00,
                  ((source & 0xff) + 16) << 8,source & 0xff00,canvas);
    }
}
