/* Plain polygon submission, +0x44 (0x20732aa5), and edge builder
 * 0x207329dc. Filling is in hg2d_fill.inc; textured attributes are deferred. */
static uint32_t polygon_project(uint32_t coordinate, uint32_t scale, uint32_t depth)
{
    int64_t product = (int64_t)(int32_t)coordinate * (int32_t)(scale << 16);
    int64_t quotient;
    /* Host filter 0x4119a0 repairs signed DIV32 to +/-INT32_MAX.
     * INT64_MIN / -1 cannot occur: both multiplicands are signed dwords. */
    if (depth) {
        quotient = product / (int32_t)depth;
        if (quotient >= -2147483648LL && quotient <= 2147483647LL)
            return (uint32_t)quotient;
    }
    return ((product < 0) != ((int32_t)depth < 0)) ? 0x80000001u : 0x7fffffffu;
}

static void polygon_edge(uint32_t low_x, uint32_t low_y, uint32_t high_x, uint32_t high_y)
{
    uint32_t *edge = POINTER(GLOBAL_WORD(0xc14));
    uint32_t advance;
    WORD(0x8e8) = low_x;
    WORD(0x8ec) = low_y;
    WORD(0x8f0) = high_x;
    WORD(0x8f4) = high_y;
    edge[0] = (uint32_t)((int32_t)low_y >> 16);
    edge[1] = (uint32_t)((int32_t)high_y >> 16);
    if (edge[0] == edge[1]) {
        edge[2] = low_x + 0x8000;
        edge[3] = high_x - low_x;
    } else {
        edge[3] = draw_slope(high_x - low_x, high_y - low_y);
        advance = (low_y & 0xffff) ^ 0xffff;
        edge[2] = low_x + (uint32_t)(((int64_t)(int32_t)edge[3] * advance) >> 16);
    }
    GLOBAL_WORD(0xc14) = ADDRESS(edge) + 16;
}

void STDCALL __attribute__((used)) polygon_vertex_body(void *record, uint32_t x, uint32_t y, uint32_t depth)
{
    const uint32_t *view = POINTER(SURFACE_WORD(record, 0x1c));
    uint32_t previous_x, previous_y, temporary, offset_x, offset_y, scale_x, scale_y;
    offset_x = view[1]; WORD(0x8d8) = offset_x;
    offset_y = view[3]; WORD(0x8dc) = offset_y;
    scale_x = view[0]; WORD(0x8e0) = scale_x;
    scale_y = view[2]; WORD(0x8e4) = scale_y;
    x = polygon_project(x, scale_x, depth) + (offset_x << 16);
    y = polygon_project(y, scale_y, depth) + (offset_y << 16);
    GLOBAL_WORD(0x2f0)++;
    if (!GLOBAL_WORD(0x2f0)) {
        WORD(0x8f8) = WORD(0x900) = x;
        WORD(0x8fc) = WORD(0x904) = y;
        WORD(0x8c8) = WORD(0x8cc) = y;
        WORD(0x8d0) = WORD(0x8d4) = depth;
        return;
    }
    if ((int32_t)depth < (int32_t)WORD(0x8d0)) WORD(0x8d0) = depth;
    if ((int32_t)depth > (int32_t)WORD(0x8d4)) WORD(0x8d4) = depth;
    /* Retail reserves 0x60 bytes even though plain edges occupy only 16.
     * Rejected vertices still increment the count and update depth bounds. */
    if (GLOBAL_WORD(0xc14) + 0x60 >= ADDRESS(hg2d_image + 0x884)) return;
    if (y < WORD(0x8c8)) WORD(0x8c8) = y;
    if ((int32_t)y > (int32_t)WORD(0x8cc)) WORD(0x8cc) = y;
    previous_x = WORD(0x900);
    previous_y = WORD(0x904);
    WORD(0x900) = x;
    WORD(0x904) = y;
    if ((int32_t)y < (int32_t)previous_y) {
        temporary = x; x = previous_x; previous_x = temporary;
        temporary = y; y = previous_y; previous_y = temporary;
    }
    if (GLOBAL_HALF(0xc90) && GLOBAL_HALF(0x2f4)) __builtin_trap();
    polygon_edge(previous_x, previous_y, x, y);
}
DRAW_ENTRY(polygon_vertex, polygon_vertex_body, 4, 16, "");
