/* Untextured polygon close/fill, +0x40 (0x20732697).
 * Span selector words retain retail addresses as observable image state. */
static void polygon_shadow_map(void)
{
    uint8_t factor = (uint8_t)((uint16_t)(0u - GLOBAL_HALF(0xb34)) >> 8);
    uint32_t index;
    if (factor == BYTE(0x4034)) return;
    BYTE(0x4034) = factor;
    for (index = 0; index < 256; index++) BYTE(0x4035 + index) = BYTE(0xb4e + index);
    for (index = 0; index < 256; index++) {
        uint8_t value = BYTE(0x4035 + index);
        if (value < 32) {
            uint16_t product = (uint16_t)((int16_t)value * (int8_t)factor * 2);
            BYTE(0x4035 + index) = BYTE(0x4135 + (product >> 8));
        }
    }
}

static void polygon_span(uint32_t first, uint32_t last, uint32_t y)
{
    void *surface = POINTER(WORD(0x8c4));
    uint8_t *pixel, *map;
    uint32_t count, phase;
    if ((int16_t)first < 0 || (int16_t)last < 0) {
        if ((int16_t)first < 0) first = 0;
        if ((int16_t)last < 0) last = 0;
        if ((int16_t)first > (int16_t)last) {
            uint16_t temporary = (uint16_t)first;
            first = (first & 0xffff0000u) | (uint16_t)last;
            last = (last & 0xffff0000u) | temporary;
        }
    }
    if (WORD(0x402c) == 0x20731134) {
        if (GLOBAL_WORD(0x1298)) {
            if (y >= SURFACE_WORD(surface, 8)) return;
            /* Unlike the primitive caller, polygon spans save/restore EBP. */
            draw_map_pixels(draw_address(surface, first, y), last - first + 1, POINTER(WORD(0x1d88)));
        } else {
            draw_horizontal_pixels(surface, first, last, y);
        }
        return;
    }
    pixel = draw_address(surface, first, y);
    count = last - first;
    if (WORD(0x402c) == 0x20732da7) {
        if ((int32_t)count < 0) return;
        count++;
        phase = (y ^ first) & 1;
        pixel += phase;
        count -= phase;
        /* Retail writes once even when a one-pixel span loses its sole
         * sample to the checkerboard phase. It writes the right neighbour. */
        do {
            *pixel = BYTE(0x90c);
            pixel += 2;
            count -= 2;
        } while ((int32_t)count > 0);
        return;
    }
    /* The shadow span's early negative-word return loses its saved ESI.
     * Ordered, clipped spans used here do not take that corrupt-stack path. */
    if ((int16_t)count < 0) __builtin_trap();
    count++;
    map = hg2d_image + (HALF(0x94c) ? 0xa4e : 0x4035);
    if (ADDRESS(pixel) & 1) {
        *pixel = map[*pixel];
        count--;
        if ((int32_t)count <= 0) goto shadow_tail;
        pixel++;
    }
    count -= 2;
    while ((int32_t)count > 0) {
        uint8_t first_value = pixel[0], last_value = pixel[1];
        first_value = map[first_value];
        last_value = map[last_value];
        pixel[0] = first_value; pixel[1] = last_value;
        pixel += 2;
        count -= 2;
    }
shadow_tail:
    count++;
    if ((int32_t)count > 0) *pixel = map[*pixel];
}

static void polygon_flat_span(uint32_t *edge, uint32_t y, int unsigned_order)
{
    uint32_t first = edge[2], last = first + edge[3], temporary;
    if ((int32_t)last < 0) last = 0;
    if (unsigned_order ? first > last : (int32_t)first > (int32_t)last) {
        temporary = first; first = last; last = temporary;
    }
    polygon_span(first >> 16, last >> 16, y);
}

/* 0x20732be8: scan count+2 records, including the unused trailing record. */
static void polygon_scan(uint32_t y)
{
    uint32_t index, used = 0, pairs, end;
    uint32_t *edge = (uint32_t *)(hg2d_image + 0x104);
    uint16_t *intersections = (uint16_t *)(hg2d_image + 0x888);
    for (index = 0; index < WORD(0x4028) + 2; index++, edge += 4) {
        if ((uint16_t)edge[0] > (uint16_t)y) continue;
        if ((uint16_t)edge[1] != (uint16_t)y) {
            intersections[used++] = (uint16_t)(edge[2] >> 16);
            edge[2] += edge[3];
            if ((int32_t)edge[2] < 0) edge[2] = 0;
        } else if ((uint16_t)edge[0] == (uint16_t)edge[1]) {
            /* Publish inactivity before the span accesses surface memory. */
            edge[0] = 0xffffffffu;
            polygon_flat_span(edge, y, 1);
        } else {
            edge[0] = 0xffffffffu;
        }
    }
    pairs = used / 2;
    if (!pairs) return;
    if (pairs <= 2) {
        /* Retail sorts these in registers, leaving the scratch words alone. */
        int16_t values[4], temporary;
        for (index = 0; index < pairs * 2; index++) values[index] = (int16_t)intersections[index];
        for (end = pairs * 2; end > 1; end--)
            for (index = 1; index < end; index++)
                if (values[index - 1] > values[index]) {
                    temporary = values[index - 1]; values[index - 1] = values[index]; values[index] = temporary;
                }
        for (index = 0; index < pairs; index++)
            polygon_span((uint16_t)values[index * 2], (uint16_t)values[index * 2 + 1], y);
        return;
    }
    WORD(0x4030) = ADDRESS(intersections);
    /* 0x20732d07: repeated minimum exchanges, in-place signed word sort. */
    for (end = pairs * 2 - 1; end; end--) {
        int16_t value = (int16_t)intersections[end];
        for (index = 0; index < end; index++) {
            if (value <= (int16_t)intersections[index]) {
                int16_t temporary = (int16_t)intersections[index];
                intersections[index] = (uint16_t)value; value = temporary;
            }
        }
        intersections[end] = (uint16_t)value;
    }
    for (index = 0; index < pairs; index++)
        polygon_span((uint32_t)(int32_t)(int16_t)intersections[index * 2],
                     (uint32_t)(int32_t)(int16_t)intersections[index * 2 + 1], y);
}

/* 0x207328e2 / 0x2073295f: find an edge beginning exactly on this row. */
static uint32_t *polygon_find_edge(uint32_t y, uint32_t *other)
{
    uint32_t *edge;
    for (;;) {
        edge = (uint32_t *)(hg2d_image + 0x104);
        for (;;) {
            if (edge != other && edge[0] == y) break;
            edge += 4;
            if (ADDRESS(edge) >= WORD(0x884)) return 0;
        }
        if (edge[1] != y) return edge;
        if (y == WORD(0x8cc)) polygon_flat_span(edge, y, 0);
        edge[0] = 0xffffffffu;
    }
}

static void polygon_walk(void)
{
    uint32_t y = WORD(0x8c8), first, last, temporary;
    uint32_t *left, *right;
    WORD(0x884) = GLOBAL_WORD(0xc14);
    left = polygon_find_edge(y, 0);
    if (!left) return;
    right = polygon_find_edge(y, left);
    if (!right) return;
    for (;;) {
        if ((int32_t)y >= (int32_t)left[1]) {
            left[0] = 0xffffffffu;
            left = polygon_find_edge(y, right);
            if (!left) return;
        }
        if ((int32_t)y >= (int32_t)right[1]) {
            right[0] = 0xffffffffu;
            right = polygon_find_edge(y, left);
            if (!right) return;
        }
        first = left[2] >> 16; last = right[2] >> 16;
        if ((int32_t)first > (int32_t)last) {
            temporary = first; first = last; last = temporary;
        }
        polygon_span(first, last, y);
        left[2] += left[3];
        if ((int32_t)left[2] < 0) left[2] = 0;
        right[2] += right[3];
        if ((int32_t)right[2] < 0) right[2] = 0;
        y++;
    }
}

void STDCALL __attribute__((used)) polygon_close_body(void *surface)
{
    uint32_t first_x, first_y, last_x, last_y, temporary, mode, y;
    uint16_t colour, pair;
    int stipple;
    WORD(0x8c4) = ADDRESS(surface);
    mode = GLOBAL_WORD(0x1298);
    if (mode) {
        GLOBAL_WORD(0x3c0) = 0x6900;
        if ((int32_t)mode < 16) {
            WORD(0x1d88) = ADDRESS(hg2d_image + 0xd70) + (mode << 8);
        } else {
            mode -= 16;
            if ((int32_t)mode > (int32_t)WORD(0x298c))
                WORD(0x1d88) = ADDRESS(hg2d_image + 0xd70 + 3 * 256);
            else
                WORD(0x1d88) = ADDRESS(hg2d_image + 0x1d8c) + (mode << 8);
        }
    }
    if (GLOBAL_HALF(0xc90) && GLOBAL_HALF(0x2f4)) {
        matrix_close();
        return;
    }
    if (GLOBAL_HALF(0xc90)) __builtin_trap();
    colour = (uint16_t)GLOBAL_WORD(0x3c0);
    stipple = colour >> 8 == 0x68;
    if (stipple) colour += 0x100;
    colour = (uint16_t)colour_translate(colour, GLOBAL_HALF(0xcc8), &pair);
    BYTE(0x90c) = draw_distance((uint8_t)colour);
    WORD(0x402c) = stipple ? 0x20732da7 : 0x20731134;
    if (stipple && (int16_t)GLOBAL_HALF(0xb34) < 0) {
        HALF(0x94c) = GLOBAL_HALF(0xdce);
        WORD(0x402c) = 0x20732e1a;
        polygon_shadow_map();
    }
    first_x = WORD(0x8f8); first_y = WORD(0x8fc);
    last_x = WORD(0x900); last_y = WORD(0x904);
    /* Closing edge uses unsigned y ordering, unlike vertex submission. */
    if (first_y < last_y) {
        temporary = first_x; first_x = last_x; last_x = temporary;
        temporary = first_y; first_y = last_y; last_y = temporary;
    }
    polygon_edge(last_x, last_y, first_x, first_y);
    WORD(0x8c8) = (uint32_t)((int32_t)WORD(0x8c8) >> 16);
    WORD(0x8cc) = (uint32_t)((int32_t)WORD(0x8cc) >> 16);
    if (GLOBAL_HALF(0x2f4) || (int32_t)GLOBAL_WORD(0x2f0) < 3) {
        polygon_walk();
        return;
    }
    WORD(0x4020) = WORD(0x8cc);
    WORD(0x4028) = GLOBAL_WORD(0x2f0);
    y = WORD(0x8c8);
    do {
        polygon_scan(y);
        y++;
    } while (WORD(0x4020) >= y);
}
/* Capture the public entry stack for projective spans' visible saved ESP.
 * Keep the ordinary DRAW_ENTRY register/argument contract unchanged. */
void polygon_close(void);
__asm__(".text\n_polygon_close:\n\tmovl %esp,_hg2d_polygon_entry_sp\n\t"
        "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "movl $0,_hg2d_draw_colour_used\n\tpushl 16(%esp)\n\t"
        "call _polygon_close_body@4\n\tpopl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $4\n");
