/* Basic doubled-pixel software spans: selectors 0/3 (right to left) and
 * 1/2 (left to right), 0x20733bbb / 0x2073751e. No fog, keying or dithering. */
#include "hg2d_projective_tables.inc"

uint32_t hg2d_polygon_entry_sp;

static uint32_t projective_divide(int64_t dividend, uint32_t denominator)
{
    int64_t quotient;
    if (denominator) {
        quotient = dividend / (int32_t)denominator;
        if (quotient >= -2147483648LL && quotient <= 2147483647LL) return (uint32_t)quotient;
    }
    return ((dividend < 0) != ((int32_t)denominator < 0)) ? 0x80000001u : 0x7fffffffu;
}

static uint32_t projective_coordinate(uint32_t attribute, uint32_t factor, uint32_t axis)
{
    uint32_t value = (uint32_t)(((int64_t)(int32_t)attribute * (int32_t)factor) >> 16);
    value += WORD(0x41c8 + axis * 8);
    value -= WORD(0x41d4 + axis * 8);
    if ((int32_t)value < (int32_t)WORD(0x41e0 + axis * 4)) value = WORD(0x41e0 + axis * 4);
    else if ((int32_t)value > (int32_t)WORD(0x41e8 + axis * 4)) value = WORD(0x41e8 + axis * 4);
    return value;
}

static uint32_t projective_project(uint32_t attributes, uint32_t output)
{
    int64_t dividend = (int64_t)(int32_t)(WORD(0x41cc) - WORD(0x41d8)) * 256;
    uint32_t denominator = (uint32_t)((int32_t)WORD(attributes + 4) >> 8) | 1;
    uint32_t factor = 0u - projective_divide(dividend, denominator);
    WORD(output) = projective_coordinate(WORD(attributes), factor, 0);
    WORD(output + 4) = projective_coordinate(WORD(attributes + 8), factor, 1);
    return factor;
}

static uint8_t projective_sample(uint32_t *u_fraction, uint32_t *v_fraction,
                                 uint8_t *u, uint8_t *v, uint8_t u_step, uint8_t v_step)
{
    uint8_t value = *(uint8_t *)POINTER(WORD(0x948) + *u + ((uint32_t)*v << 8));
    uint64_t sum = (uint64_t)*u_fraction + WORD(0x41a8);
    *u_fraction = (uint32_t)sum; *u = (uint8_t)(*u + u_step + (uint32_t)(sum >> 32));
    sum = (uint64_t)*v_fraction + WORD(0x41ac);
    *v_fraction = (uint32_t)sum; *v = (uint8_t)(*v + v_step + (uint32_t)(sum >> 32));
    return value;
}

static void projective_pixels(uint32_t pixel_address, uint32_t count, uint32_t start, int direction, int tail)
{
    uint32_t u_fraction = WORD(start) << 16, v_fraction = WORD(start + 4) << 16;
    uint8_t u = (uint8_t)(WORD(start) >> 16), v = (uint8_t)(WORD(start + 4) >> 16);
    uint8_t u_step = (uint8_t)(WORD(0x41a8) >> 16), v_step = (uint8_t)(WORD(0x41ac) >> 16);
    uint32_t remaining = count;
    WORD(0x41a8) <<= 16; WORD(0x41ac) <<= 16;
    if (!tail && remaining && ((pixel_address & 2) != 0) == (direction > 0)) {
        uint8_t value = projective_sample(&u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t *pixel = POINTER(pixel_address);
        pixel[0] = value; pixel[1] = value;
        pixel_address += direction * 2; remaining--;
    }
    while (!tail && remaining >= 2) {
        uint8_t first = projective_sample(&u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t second = projective_sample(&u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t *pixel = POINTER(direction > 0 ? pixel_address : pixel_address - 2);
        if (direction > 0) {
            pixel[0] = pixel[1] = first; pixel[2] = pixel[3] = second;
        } else {
            pixel[0] = pixel[1] = second; pixel[2] = pixel[3] = first;
        }
        pixel_address += direction * 4; remaining -= 2;
    }
    while (remaining--) {
        uint8_t value = projective_sample(&u_fraction, &v_fraction, &u, &v, u_step, v_step);
        uint8_t *pixel = POINTER(pixel_address);
        pixel[0] = value; pixel[1] = value;
        pixel_address += direction * 2;
    }
}

static void projective_span(uint32_t *left, uint32_t *right, uint32_t y, int flat)
{
    uint32_t first_x = left[2] & 0xfffeffffu, last_x = right[2] & 0xfffeffffu;
    uint32_t component, count, right_factor, left_factor, minimum, maximum, ratio;
    uint32_t pixel_address, shift, block, remaining;
    int forward = WORD(0x4200) == 0x2073751e;
    int direction = forward ? 1 : -1;
    if (first_x > last_x) {
        uint32_t *swap = left; left = right; right = swap;
        count = first_x; first_x = last_x; last_x = count;
    }
    if (!forward) { uint32_t *swap = left; left = right; right = swap; }
    for (component = 0; component < 3; component++) WORD(0x4178 + component * 4) = left[4 + component];
    for (component = 0; component < 3; component++) WORD(0x4184 + component * 4) = right[4 + component];
    WORD(0x41f0) = forward ? first_x : last_x; WORD(0x41f4) = y;
    count = (uint32_t)((int32_t)(last_x - first_x) >> 17) + 1;
    right_factor = projective_project(0x4178, 0x4190);
    left_factor = projective_project(0x4184, 0x41a0);
    minimum = (int32_t)right_factor < 0 ? 0u - right_factor : right_factor;
    maximum = (int32_t)left_factor < 0 ? 0u - left_factor : left_factor;
    if (minimum > maximum) { uint32_t swap = minimum; minimum = maximum; maximum = swap; }
    ratio = (uint32_t)(((uint64_t)minimum << 32) / (maximum + 1));
    shift = ratio < 0x10000000 ? 4 : 5;
    block = 1u << shift;
    for (component = 0; component < 3; component++)
        WORD(0x41b0 + component * 4) = projective_divide(
            (int64_t)(int32_t)(WORD(0x4184 + component * 4) - WORD(0x4178 + component * 4)) * block, count);
    pixel_address = ADDRESS(draw_address(POINTER(WORD(0x8c4)), (uint16_t)(WORD(0x41f0) >> 16), (uint16_t)y));
    WORD(0x4164) = hg2d_polygon_entry_sp - (flat ? 68 : 64);
    remaining = count;
    while ((int32_t)remaining >= (int32_t)block) {
        for (component = 0; component < 3; component++) WORD(0x4178 + component * 4) += WORD(0x41b0 + component * 4);
        WORD(0x4198) = WORD(0x4190); WORD(0x419c) = WORD(0x4194);
        projective_project(0x4178, 0x4190);
        WORD(0x41a8) = (uint32_t)((int32_t)(WORD(0x4190) - WORD(0x4198)) >> shift);
        WORD(0x41ac) = (uint32_t)((int32_t)(WORD(0x4194) - WORD(0x419c)) >> shift);
        WORD(0x4160) = remaining;
        projective_pixels(pixel_address, block, 0x4198, direction, 0);
        pixel_address += direction * block * 2; remaining -= block;
    }
    if ((int32_t)remaining <= 0) return;
    WORD(0x41a8) = projective_divide((int32_t)(WORD(0x41a0) - WORD(0x4190)), remaining);
    WORD(0x41ac) = projective_divide((int32_t)(WORD(0x41a4) - WORD(0x4194)), remaining);
    WORD(0x4354) = projective_tails[forward][shift == 4][remaining];
    projective_pixels(pixel_address, remaining, 0x4190, direction, 1);
}
