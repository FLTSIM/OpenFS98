/* +0x44/+0x40 alternate texture edges, 0x20733386, and setup 0x20733130.
 * Basic projective spans and the far-depth fill are implemented; other spans trap. */
static uint32_t matrix_signed_divide(uint32_t value, uint32_t denominator)
{
    int64_t quotient;
    if (denominator) {
        quotient = (int64_t)(int32_t)value / (int32_t)denominator;
        if (quotient >= -2147483648LL && quotient <= 2147483647LL) return (uint32_t)quotient;
    }
    return (((int32_t)value < 0) != ((int32_t)denominator < 0)) ? 0x80000001u : 0x7fffffffu;
}

static uint32_t matrix_coordinate(uint32_t position, uint32_t offset, uint32_t scale)
{
    uint32_t value = matrix_signed_divide(position - (offset << 16), scale << 1);
    uint32_t sign = (int32_t)value < 0 ? 0xffffffffu : 0;
    value ^= sign;
    if (value > 0x7fff) value = 0x7fff;
    return value ^ sign;
}

static uint32_t matrix_attribute(uint32_t x, uint32_t y, uint32_t component)
{
    uint32_t first = (uint32_t)((int16_t)x * (int16_t)GLOBAL_HALF(0xcfa + component * 2));
    uint32_t second = (uint32_t)((int16_t)y * (int16_t)GLOBAL_HALF(0xd00 + component * 2));
    return (uint32_t)((int32_t)(first + second) >> (GLOBAL[0xd42] & 31)) + GLOBAL_WORD(0xd0c + component * 4);
}

static uint32_t matrix_slope(uint32_t delta, uint32_t height)
{
    uint32_t sign = (int32_t)delta < 0 ? 0xffffffffu : 0;
    uint32_t magnitude = delta ^ sign;
    uint32_t quotient = magnitude / height, remainder = magnitude % height;
    uint32_t fraction = (uint32_t)(((uint64_t)remainder << 32) / height);
    if (quotient > 0x7fff) quotient = 0x7fff;
    /* Unlike the x slope, attribute slopes omit SUB ECX,sign after XOR. */
    return ((quotient << 16) | (fraction >> 16)) ^ sign;
}

static void matrix_edge(uint32_t low_x, uint32_t low_y, uint32_t high_x, uint32_t high_y)
{
    uint32_t *edge = POINTER(GLOBAL_WORD(0xc14));
    uint32_t component, height, advance;
    WORD(0x8e8) = low_x; WORD(0x8ec) = low_y;
    WORD(0x8f0) = high_x; WORD(0x8f4) = high_y;
    edge[0] = (uint32_t)((int32_t)low_y >> 16);
    edge[1] = (uint32_t)((int32_t)high_y >> 16);
    edge[2] = low_x;
    height = edge[0] == edge[1] ? 1 : high_y - low_y;
    edge[3] = edge[0] == edge[1] ? high_x - low_x : draw_slope(high_x - low_x, height);
    WORD(0x4168) = matrix_coordinate(low_x, WORD(0x8d8), WORD(0x8e0));
    WORD(0x416c) = matrix_coordinate(low_y, WORD(0x8dc), WORD(0x8e4));
    WORD(0x4170) = matrix_coordinate(high_x, WORD(0x8d8), WORD(0x8e0));
    WORD(0x4174) = matrix_coordinate(high_y, WORD(0x8dc), WORD(0x8e4));
    for (component = 0; component < 3; component++)
        edge[4 + component] = matrix_attribute(WORD(0x4168), WORD(0x416c), component);
    for (component = 0; component < 3; component++)
        WORD(0x4184 + component * 4) = matrix_attribute(WORD(0x4170), WORD(0x4174), component);
    for (component = 0; component < 3; component++)
        edge[7 + component] = matrix_slope(WORD(0x4184 + component * 4) - edge[4 + component], height);
    if (edge[0] == edge[1]) {
        /* Retail calculates three slopes with height=1, then overwrites them
         * with raw deltas. The calculations leave no other observable state. */
        for (component = 0; component < 3; component++)
            edge[7 + component] = WORD(0x4184 + component * 4) - edge[4 + component];
    } else {
        advance = (low_y & 0xffff) ^ 0xffff;
        edge[2] += (uint32_t)(((int64_t)(int32_t)edge[3] * advance) >> 16);
        for (component = 0; component < 3; component++)
            edge[4 + component] += (uint32_t)(((int64_t)(int32_t)edge[7 + component] * advance) >> 16);
    }
    GLOBAL_WORD(0xc14) = ADDRESS(edge) + 40;
}

static uint32_t matrix_unsigned_divide(uint64_t value, uint32_t denominator)
{
    uint64_t quotient;
    if (!denominator) return 0xffffffffu;
    quotient = value / denominator;
    return quotient > 0xffffffffu ? 0xffffffffu : (uint32_t)quotient;
}

static void matrix_setup(void)
{
    uint64_t denominator = (uint64_t)0xd00 * GLOBAL_WORD(0x320), numerator = 0x200000;
    uint32_t index, selector = 0, ratio, scale, maximum;
    uint32_t *texture;
    while (denominator >> 32) { denominator >>= 1; numerator >>= 1; }
    ratio = matrix_unsigned_divide(numerator << 16, (uint32_t)denominator);
    WORD(0x4369) = ratio;
    WORD(0x436d) = matrix_unsigned_divide(0x3e00000, ratio);
    WORD(0x4371) = matrix_unsigned_divide(0x2400000, ratio);
    for (index = 0; index < 3; index++) WORD(0x41c8 + index * 4) = GLOBAL_WORD(0xcdc + index * 4);
    for (index = 0; index < 3; index++) WORD(0x41d4 + index * 4) = GLOBAL_WORD(0xc94 + index * 4);
    for (index = 0; index < 4; index++) WORD(0x41e0 + index * 4) = GLOBAL_WORD(0xca4 + index * 4);
    /* 0x20787fb9 returns only 0/INT32_MAX or carry+INT32_MAX. Its intervening
     * multiply/shift block has no incoming control-flow path from this call. */
    if (GLOBAL_HALF(0xe88) < 0x7d00 && !GLOBAL_HALF(0xdce) && (int32_t)WORD(0x8d4) > 0) {
        if (WORD(0x8d0) == 0x7fffffffu) {
            WORD(0x4200) = 0x20784d41;
            return;
        }
        denominator = (uint64_t)GLOBAL_HALF(0xe88) * GLOBAL_WORD(0x320);
        numerator = 0x200000;
        while (denominator >> 32) { denominator >>= 1; numerator >>= 1; }
        WORD(0x41fc) = (uint32_t)denominator; WORD(0x41f8) = (uint32_t)numerator;
        scale = (uint32_t)((int16_t)GLOBAL_HALF(0x30c) < 0 ? -(int32_t)(int16_t)GLOBAL_HALF(0x30c) : GLOBAL_HALF(0x30c));
        maximum = GLOBAL_HALF(0xcd4);
        if (scale > maximum) {
            denominator = (uint64_t)scale * WORD(0x41fc);
            numerator = (uint64_t)maximum << 21;
            while ((denominator >> 32) || (numerator >> 32)) { denominator >>= 1; numerator >>= 1; }
            WORD(0x41fc) = (uint32_t)denominator; WORD(0x41f8) = (uint32_t)numerator;
        }
        WORD(0x41f8) = matrix_unsigned_divide((uint64_t)WORD(0x41f8) << 16, WORD(0x41fc));
        selector = 4;
    }
    selector |= ((int16_t)GLOBAL_HALF(0x2e8) > 0);
    selector |= ((int32_t)WORD(0x41cc) > (int32_t)GLOBAL_WORD(0xc98)) << 1;
    BYTE(0x435c) = GLOBAL[0xca0];
    selector |= (BYTE(0x435c) != 0) << 3;
    selector |= (GLOBAL_HALF(0xcb4) == 2 && (GLOBAL_HALF(0x1172) & 1) && GLOBAL_HALF(0x324)) << 4;
    selector |= ((int16_t)GLOBAL_HALF(0xcb4) >= 1 && GLOBAL_HALF(0xcce) != 0) << 5;
    WORD(0x4200) = WORD(0x4204 + selector * 4);
    texture = POINTER(GLOBAL_WORD(0xc8c));
    if ((selector & 16) && !(selector & 8) && !(texture[1] & 0x40000) && !texture_mip_build(texture, 1)) {
        selector &= ~16u;
        WORD(0x4200) = WORD(0x4204 + 4 * selector);
    }
    WORD(0x948) = texture[0];
}

static void matrix_span(uint32_t *left, uint32_t *right, uint32_t y, int flat)
{
    if (WORD(0x4200) == 0x20733bbb || WORD(0x4200) == 0x2073751e ||
        WORD(0x4200) == 0x2073ae7b || WORD(0x4200) == 0x2073e1ec ||
        WORD(0x4200) == 0x207352a4 || WORD(0x4200) == 0x20738c03 ||
        WORD(0x4200) == 0x2073c402 || WORD(0x4200) == 0x2073f771 ||
        WORD(0x4200) == 0x20749d95 || WORD(0x4200) == 0x207541ee ||
        WORD(0x4200) == 0x2074e49d || WORD(0x4200) == 0x207588f0 ||
        WORD(0x4200) == 0x2075e63f || WORD(0x4200) == 0x20766058 ||
        WORD(0x4200) == 0x20761af9 || WORD(0x4200) == 0x20769510 ||
        WORD(0x4200) == 0x20741559 || WORD(0x4200) == 0x2074597a ||
        WORD(0x4200) == 0x207431a7 || WORD(0x4200) == 0x207475c4 ||
        WORD(0x4200) == 0x2076da6d || WORD(0x4200) == 0x207793d8 ||
        WORD(0x4200) == 0x20772c04 || WORD(0x4200) == 0x2077e569) {
        projective_span(left, right, y, flat);
        return;
    }
    if (WORD(0x4200) != 0x20784d41) __builtin_trap(); /* Other projective spans remain deferred. */
    matrix_far_span(left, right, y);
}

static void matrix_far_span(uint32_t *left, uint32_t *right, uint32_t y)
{
    uint32_t first = left[2], last = right[2], temporary;
    void *surface = POINTER(WORD(0x8c4));
    BYTE(0x90c) = BYTE(0x1d06);
    if (first > last) { temporary = first; first = last; last = temporary; }
    first = (uint32_t)((int32_t)first >> 16); last = (uint32_t)((int32_t)last >> 16);
    if (GLOBAL_WORD(0x1298)) {
        if (y >= SURFACE_WORD(surface, 8)) return;
        if ((int16_t)first < 0 || (int16_t)last < 0) {
            if ((int16_t)first < 0) first = 0;
            if ((int16_t)last < 0) last = 0;
            if ((int16_t)first > (int16_t)last) {
                uint16_t swap = (uint16_t)first;
                first = (first & 0xffff0000u) | (uint16_t)last;
                last = (last & 0xffff0000u) | swap;
            }
        }
        draw_map_pixels(draw_address(surface, first, y), last - first + 1, POINTER(WORD(0x1d88)));
    } else draw_horizontal_pixels(surface, first, last, y);
}

static uint32_t *matrix_find(uint32_t y, uint32_t *other)
{
    uint32_t *edge;
    for (;;) {
        edge = (uint32_t *)(hg2d_image + 0x104);
        for (;;) {
            if (edge != other && edge[0] == y) break;
            edge += 10;
            if (ADDRESS(edge) >= WORD(0x884)) return 0;
        }
        if (edge[1] != y) return edge;
        if (y == WORD(0x8cc)) {
            WORD(0x430c) = edge[2]; WORD(0x4314) = edge[4]; WORD(0x4318) = edge[5]; WORD(0x431c) = edge[6];
            WORD(0x4334) = edge[2] + edge[3]; WORD(0x433c) = edge[4] + edge[7];
            WORD(0x4340) = edge[5] + edge[8]; WORD(0x4344) = edge[6] + edge[9];
            matrix_span((uint32_t *)(hg2d_image + 0x4304), (uint32_t *)(hg2d_image + 0x432c), y, 1);
        }
        edge[0] = 0xffffffffu;
    }
}

static void matrix_close(void)
{
    uint32_t high_x = WORD(0x8f8), high_y = WORD(0x8fc), low_x = WORD(0x900), low_y = WORD(0x904), temporary;
    uint32_t y, component;
    uint32_t *left, *right;
    if (high_y < low_y) {
        temporary = high_x; high_x = low_x; low_x = temporary;
        temporary = high_y; high_y = low_y; low_y = temporary;
    }
    matrix_edge(low_x, low_y, high_x, high_y);
    matrix_setup();
    WORD(0x8c8) = (uint32_t)((int32_t)WORD(0x8c8) >> 16);
    WORD(0x8cc) = (uint32_t)((int32_t)WORD(0x8cc) >> 16);
    WORD(0x884) = GLOBAL_WORD(0xc14);
    y = WORD(0x8c8);
    left = matrix_find(y, 0); if (!left) return;
    right = matrix_find(y, left); if (!right) return;
    for (;;) {
        if ((int32_t)y >= (int32_t)left[1]) {
            left[0] = 0xffffffffu; left = matrix_find(y, right); if (!left) return;
        }
        if ((int32_t)y >= (int32_t)right[1]) {
            right[0] = 0xffffffffu; right = matrix_find(y, left); if (!right) return;
        }
        matrix_span(left, right, y, 0);
        left[2] += left[3]; right[2] += right[3];
        for (component = 0; component < 3; component++) {
            left[4 + component] += left[7 + component]; right[4 + component] += right[7 + component];
        }
        y++;
    }
}

/* 0x20784d69: ordinary polygon spans synthesize two matrix records.
 * The three stores replace only high words, retaining edge scratch fractions. */
static void matrix_plane_span(uint32_t first, uint32_t last, uint32_t y, uint32_t stack_depth)
{
    uint32_t component, entry_sp = hg2d_polygon_entry_sp;
    HALF(0x8ea) = (uint16_t)first;
    HALF(0x8f2) = (uint16_t)last;
    HALF(0x8ee) = (uint16_t)y;
    WORD(0x4168) = matrix_coordinate(WORD(0x8e8), WORD(0x8d8), WORD(0x8e0));
    WORD(0x4170) = matrix_coordinate(WORD(0x8f0), WORD(0x8d8), WORD(0x8e0));
    WORD(0x416c) = matrix_coordinate(WORD(0x8ec), WORD(0x8dc), WORD(0x8e4));
    for (component = 0; component < 3; component++)
        WORD(0x4314 + component * 4) = matrix_attribute(WORD(0x4168), WORD(0x416c), component);
    for (component = 0; component < 3; component++)
        WORD(0x433c + component * 4) = matrix_attribute(WORD(0x4170), WORD(0x416c), component);
    WORD(0x430c) = WORD(0x8e8);
    WORD(0x4334) = WORD(0x8f0);
    /* Callers have different saved words beneath the projective entry. */
    hg2d_polygon_entry_sp = entry_sp + 64 - stack_depth;
    matrix_span((uint32_t *)(hg2d_image + 0x4304), (uint32_t *)(hg2d_image + 0x432c), (uint16_t)y, 0);
    hg2d_polygon_entry_sp = entry_sp;
}
