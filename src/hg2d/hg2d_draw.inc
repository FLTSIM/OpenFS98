/* Integer and 16.16 drawing primitives, retail 0x20731057..0x2073188a
 * and 0x20732f28..0x207330f0. Surfaces are host-owned 32-bit records. */
#define SURFACE_WORD(surface, offset) (*(uint32_t *)((uint8_t *)(surface) + (offset)))

static uint8_t *draw_address(const void *surface, uint32_t x, uint32_t y)
{
    uint32_t rows = SURFACE_WORD(surface, 0x10);
    uint32_t row = *(uint32_t *)POINTER(rows + y * 4);
    return POINTER(row + x);
}

/* 0x2073225c / 0x2073229e, both read the current GLOBAL colour. */
uint32_t hg2d_draw_colour_used;

static uint8_t draw_colour(void)
{
    uint16_t colour = (uint16_t)GLOBAL_WORD(0x2ec), pair;
    uint32_t mode = colour >> 8;
    hg2d_draw_colour_used = 1;
    if (mode != 0x69 && mode != 0xe0 && mode != 0xf0)
        colour = (uint16_t)(0x6700 | (colour & 0xff));
    return (uint8_t)colour_translate(colour, GLOBAL_HALF(0xcc2), &pair);
}

/* 0x207880ac: depth-scaled colour for +0x24, signed word comparisons. */
static uint16_t draw_divide_word(uint32_t numerator, uint16_t denominator)
{
    uint32_t quotient;
    /* The host's 0x4119a0 repairs unsigned DIV16 to AX=DX=0xffff. */
    if (!denominator) return 0xffff;
    quotient = numerator / denominator;
    return (uint16_t)(quotient > 0xffff ? 0xffff : quotient);
}

static uint8_t draw_depth(uint8_t colour, uint16_t depth)
{
    uint16_t threshold, value, scale, maximum;
    if (GLOBAL_HALF(0xdce) || GLOBAL_WORD(0x390) < 2)
        return colour;
    threshold = GLOBAL_HALF(0xe88);
    if ((int16_t)depth < (int16_t)threshold)
        return colour;
    value = draw_divide_word(depth, threshold);
    scale = GLOBAL_HALF(0x30c);
    maximum = GLOBAL_HALF(0xcd4);
    if ((int16_t)scale > (int16_t)maximum)
        value = draw_divide_word((uint32_t)value * maximum, scale);
    if (value >= 15) value = 15;
    return BYTE(0xd70 + value * 256 + colour);
}

/* 0x20788026 / 0x2078809a, the distance-derived shade for +0x38. */
static uint8_t draw_distance(uint8_t colour)
{
    uint32_t threshold = GLOBAL_HALF(0xe88), value = 0;
    uint32_t distance = GLOBAL_WORD(0xd4c) >> 8;
    if (threshold < 0x7d00 && !GLOBAL_HALF(0xdce) && distance >= threshold) {
        uint32_t scale = GLOBAL_HALF(0x30c), maximum = GLOBAL_HALF(0xcd4);
        value = threshold ? distance / threshold : 0xffffffffu;
        if (scale > maximum)
            value = (uint32_t)(((uint64_t)value * maximum) / scale);
        if (value > 15) value = 15;
    }
    WORD(0x100) = value;
    return BYTE(0xd70 + value * 256 + colour);
}

void STDCALL __attribute__((used)) draw_point_body(void *surface, uint32_t x, uint32_t y)
{
    uint8_t *pixel = draw_address(surface, x, y);
    *pixel = draw_colour();
}

/* +0x24, 0x207313c3. Dithering adds right and upper neighbours. */
static uint32_t STDCALL draw_depth_point(void *surface, uint32_t x, uint32_t y, uint32_t depth)
{
    uint8_t *pixel, colour;
    HALF(0x29ae) = (uint16_t)depth;
    pixel = draw_address(surface, x, y);
    colour = draw_depth(draw_colour(), (uint16_t)depth);
    *pixel = colour;
    if (GLOBAL_HALF(0x127e) & 0xff) {
        if ((int16_t)(x + 1) < (int16_t)SURFACE_WORD(surface, 4))
            pixel[1] = colour;
        if ((int16_t)(y - 1) >= 0)
            *(uint8_t *)POINTER(ADDRESS(pixel) - SURFACE_WORD(surface, 0xc)) = colour;
    }
    return colour;
}

/* The eight-pixel mapper reads both words before looking up/storing bytes.
 * Preserve that ordering even if a caller aliases the map and pixels. */
static void draw_map_span(uint8_t *pixel, uint32_t count)
{
    uint8_t *map = POINTER(WORD(0x1d88));
    int short_span = count < 16;
    if (count >= 16) {
        while (ADDRESS(pixel) & 3) {
            *pixel = map[*pixel];
            pixel++;
            count--;
        }
        while (count >= 8) {
            uint8_t values[8];
            uint32_t index;
            for (index = 0; index < 8; index++) values[index] = pixel[index];
            values[0] = map[values[0]];
            values[4] = map[values[4]];
            values[1] = map[values[1]];
            values[5] = map[values[5]];
            values[3] = map[values[3]];
            values[7] = map[values[7]];
            values[2] = map[values[2]];
            values[6] = map[values[6]];
            for (index = 0; index < 8; index++) pixel[index] = values[index];
            pixel += 8;
            count -= 8;
        }
    }
    if (count || short_span)
        do {
            *pixel = map[*pixel];
            pixel++;
        } while (--count);
    /* Retail's mapper overwrites EBP with its count, ends with EBP zero,
     * then its caller's leave reads address zero. Preserve the fault after
     * the pixels are mapped; exception register contexts are not modelled. */
    __asm__ volatile("movl 0, %%eax" ::: "eax", "memory");
}

static void draw_horizontal_pixels(void *surface, uint32_t first, uint32_t last, uint32_t y)
{
    uint32_t count;
    uint8_t *pixel;
    if (y >= SURFACE_WORD(surface, 8)) return;
    if ((int16_t)first < 0 || (int16_t)last < 0) {
        if ((int16_t)first < 0) first = 0;
        if ((int16_t)last < 0) last = 0;
        if ((int16_t)first > (int16_t)last) {
            uint16_t temporary = (uint16_t)first;
            first = (first & 0xffff0000u) | (uint16_t)last;
            last = (last & 0xffff0000u) | temporary;
        }
    }
    count = last - first + 1;
    pixel = draw_address(surface, first, y);
    if (GLOBAL_WORD(0x1298)) {
        draw_map_span(pixel, count);
        return;
    }
    do { *pixel++ = BYTE(0x90c); } while (--count);
}

void STDCALL __attribute__((used)) draw_horizontal_body(void *surface, uint32_t first, uint32_t last, uint32_t y)
{
    draw_colour();
    draw_horizontal_pixels(surface, first, last, y);
}

static void draw_vertical_pixels(void *surface, uint32_t x, uint32_t first, uint32_t last)
{
    uint32_t delta = last - first, start = first, count;
    uint8_t *pixel;
    if ((int32_t)delta < 0) {
        delta = 0u - delta;
        start = last;
    }
    pixel = draw_address(surface, x, start);
    count = delta + 1;
    while (count--) {
        *pixel = BYTE(0x90c);
        pixel = POINTER(ADDRESS(pixel) + SURFACE_WORD(surface, 0xc));
    }
}

void STDCALL __attribute__((used)) draw_vertical_body(void *surface, uint32_t x, uint32_t first, uint32_t last)
{
    draw_colour();
    draw_vertical_pixels(surface, x, first, last);
}

/* +0x34, 0x2073128b. Nondegenerate lines exclude the final endpoint. */
void STDCALL __attribute__((used)) draw_line_body(void *surface, uint32_t x, uint32_t y, uint32_t end_x, uint32_t end_y)
{
    uint32_t width = end_x - x, height, temporary, length, index;
    int32_t error, row_step;
    uint8_t *pixel;
    draw_colour();
    WORD(0x908) = SURFACE_WORD(surface, 0xc);
    if ((int32_t)width < 0) {
        temporary = x; x = end_x; end_x = temporary;
        temporary = y; y = end_y; end_y = temporary;
        width = 0u - width;
    }
    height = y - end_y;
    if (!(width | height)) {
        draw_point_body(surface, x, y);
        return;
    }
    row_step = (int32_t)SURFACE_WORD(surface, 0xc);
    if ((int32_t)height > 0)
        row_step = -row_step;
    else
        height = 0u - height;
    pixel = draw_address(surface, x, y);
    length = width > height ? width : height;
    error = -(int32_t)(length >> 1);
    for (index = 0; index < length; index++) {
        *pixel = BYTE(0x90c);
        error += (int32_t)(width > height ? height : width);
        if (error >= 0) {
            error -= (int32_t)length;
            pixel = POINTER(ADDRESS(pixel) + (uint32_t)row_step + 1);
        } else {
            pixel = POINTER(ADDRESS(pixel) + (width > height ? 1u : (uint32_t)row_step));
        }
    }
}

/* +0x3c, 0x2073106e: the four inclusive edges, zero dimensions do nothing. */
void STDCALL __attribute__((used)) draw_frame_body(void *surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!width || !height) return;
    draw_horizontal_body(surface, x, x + width - 1, y);
    draw_horizontal_body(surface, x, x + width - 1, y + height - 1);
    draw_vertical_body(surface, x, y, y + height - 1);
    draw_vertical_body(surface, x + width - 1, y, y + height - 1);
}

/* +0x38: signed-complement division and word-sized carry stepping. */
static uint32_t draw_slope(uint32_t minor, uint32_t major)
{
    uint32_t sign = (int32_t)minor < 0 ? 0xffffffffu : 0;
    uint32_t magnitude = minor ^ sign;
    uint32_t quotient = magnitude / major, remainder = magnitude % major;
    uint32_t fraction = (uint32_t)(((uint64_t)remainder << 32) / major);
    if (quotient >= 0x7fff) quotient = 0x7fff;
    return (((quotient << 16) | (fraction >> 16)) ^ sign) - sign;
}

void STDCALL __attribute__((used)) draw_fixed_line_body(void *surface, uint32_t x, uint32_t y, uint32_t end_x, uint32_t end_y)
{
    uint32_t width = end_x - x, height = end_y - y, temporary, slope, fraction, count, index;
    int vertical;
    uint8_t *pixel;
    WORD(0x8c4) = ADDRESS(surface);
    WORD(0x908) = SURFACE_WORD(surface, 0xc);
    draw_colour();
    BYTE(0x90c) = draw_distance(BYTE(0x90c));
    if ((int32_t)width < 0) width = 0u - width;
    if ((int32_t)height < 0) height = 0u - height;
    if (!(width | height)) return;
    vertical = width <= height;
    if ((int32_t)(vertical ? end_y - y : end_x - x) < 0) {
        temporary = x; x = end_x; end_x = temporary;
        temporary = y; y = end_y; end_y = temporary;
    }
    WORD(0x8e8) = x;
    WORD(0x8ec) = y;
    slope = draw_slope(vertical ? end_x - x : end_y - y, vertical ? height : width);
    count = (vertical ? height : width) >> 16;
    if (vertical) {
        uint32_t advance = (y & 0xffff) ^ 0xffff;
        x += (uint32_t)(((int64_t)(int32_t)slope * advance) >> 16);
        pixel = draw_address(surface, (uint32_t)((int32_t)x >> 16), y >> 16);
        fraction = x & 0xffff;
    } else {
        pixel = draw_address(surface, x >> 16, y >> 16);
        fraction = y & 0xffff;
    }
    for (index = 0;; index++) {
        uint32_t sum;
        int32_t step_y;
        *pixel = BYTE(0x90c);
        count = (count - 1) & 0xffff;
        if ((int16_t)count <= 0) break;
        sum = fraction + (slope & 0xffff);
        fraction = sum & 0xffff;
        if (vertical) {
            step_y = (int32_t)WORD(0x908) + (sum >> 16);
            if ((int32_t)slope < 0) step_y--;
            pixel = POINTER(ADDRESS(pixel) + (uint32_t)step_y);
        } else if (((sum >> 16) != 0) == ((int32_t)slope >= 0)) {
            step_y = (int32_t)WORD(0x908);
            if ((int32_t)slope < 0) step_y = -step_y;
            pixel = POINTER(ADDRESS(pixel) + (uint32_t)step_y + 1);
        } else {
            pixel++;
        }
    }
}

/* +0x28: ten 8-bit glyph rows; header bit 7 drops the glyph two rows. */
static uint32_t STDCALL draw_text(void *surface, uint32_t x, uint32_t y, uint32_t depth, const uint8_t **text, uint32_t flags)
{
    uint32_t index, source = ADDRESS(*text), glyph, width, row, column, total = 0;
    uint8_t code, colour;
    HALF(0x29ae) = (uint16_t)depth;
    WORD(0x29b0) = x;
    WORD(0x29b4) = y;
    WORD(0x29b8) = SURFACE_WORD(surface, 4);
    WORD(0x29bc) = SURFACE_WORD(surface, 8);
    WORD(0x29c0) = ADDRESS(surface);
    WORD(0x29c4) = SURFACE_WORD(surface, 0xc);
    HALF(0x29d0) = (uint16_t)flags;
    WORD(0x29cc) = source;
    draw_colour();
    if (flags & 1) {
        for (index = 0; (code = *(uint8_t *)POINTER(source + index)) != 0; index++) {
            glyph = 0x29d2 + (uint8_t)(code - 0x20) * 11;
            total += (BYTE(glyph) & 15) + 1;
        }
        WORD(0x29b0) -= total >> 1;
        WORD(0x29b4) -= 6;
    }
    while ((code = *(uint8_t *)POINTER(WORD(0x29cc))) != 0) {
        if (code == 0xe1) code = 0xa6;
        if (code == 0xfd) code = 0xbd;
        glyph = 0x29d2 + (uint8_t)(code - 0x20) * 11;
        width = BYTE(glyph) & 15;
        WORD(0x29c8) = width;
        x = WORD(0x29b0);
        y = WORD(0x29b4);
        if ((int32_t)x >= 0 && (int32_t)(x + 8) < (int32_t)WORD(0x29b8) &&
            (int32_t)y >= 0 && (int32_t)(y + 12) < (int32_t)WORD(0x29bc)) {
            uint8_t *pixel = draw_address(surface, x, y);
            colour = BYTE(0x90c);
            if (BYTE(glyph) & 0x80)
                pixel = POINTER(ADDRESS(pixel) + WORD(0x29c4) * 2);
            for (row = 0; row < 10; row++) {
                uint8_t bits = BYTE(glyph + 1 + row);
                for (column = 0; column < 8; column++)
                    if (bits & (0x80 >> column)) pixel[column] = colour;
                pixel = POINTER(ADDRESS(pixel) + WORD(0x29c4));
            }
        }
        WORD(0x29b0) += width + 1;
        WORD(0x29cc)++;
    }
    return 0;
}

uint32_t STDCALL __attribute__((used)) draw_flush_body(uint32_t entry_eax)
{
    return HALF(0x36c0) ? palette_realize() : entry_eax;
}
void draw_flush(void);
__asm__(".text\n_draw_flush:\n\tpushl %eax\n\tcall _draw_flush_body@4\n\tret\n");

/* Retail restores incoming eax/ecx, and its line entries restore edx too. */
#define DRAW_COLOUR_EDX "cmpl $0,_hg2d_draw_colour_used\n\tje 1f\n\t" \
                        "movl _hg2d_image+0xc,%eax\n\tmovb 0xcc6(%eax),%dl\n1:\n\t"
#define DRAW_ENTRY(name, body, count, bytes, leftovers) \
    void name(void); \
    __asm__(".text\n_" #name ":\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t" \
            "movl $0,_hg2d_draw_colour_used\n\t" \
            ".rept " #count "\n\tpushl " #bytes "+12(%esp)\n\t.endr\n\t" \
            "call _" #body "@" #bytes "\n\t" \
            "popl %edx\n\tpopl %ecx\n\t" leftovers "popl %eax\n\tret $" #bytes "\n")
DRAW_ENTRY(draw_point, draw_point_body, 3, 12, DRAW_COLOUR_EDX);
DRAW_ENTRY(draw_horizontal, draw_horizontal_body, 4, 16, DRAW_COLOUR_EDX);
DRAW_ENTRY(draw_vertical, draw_vertical_body, 4, 16, DRAW_COLOUR_EDX);
DRAW_ENTRY(draw_line, draw_line_body, 5, 20, "");
DRAW_ENTRY(draw_fixed_line, draw_fixed_line_body, 5, 20, "");
DRAW_ENTRY(draw_frame, draw_frame_body, 5, 20, DRAW_COLOUR_EDX);
void draw_nop(void);
__asm__(".text\n_draw_nop:\n\tret $12\n");
