/* +0xa4, 0x2078b350: the optional software triangle fan fallback. */
static uint32_t fan_slope(uint32_t delta, uint32_t height, int unsigned_delta)
{
    int64_t numerator = unsigned_delta ? (int64_t)delta : (int64_t)(int32_t)delta;
    long double quotient = (long double)numerator * 65536.0L / (int32_t)height;
    int64_t converted;
#if defined(__i386__) || defined(__x86_64__)
    uint16_t control, truncate;
    /* Match __ftol's truncation and integer-indefinite result without a C
     * conversion of infinity/NaN. Arithmetic retains the caller's precision. */
    __asm__ volatile("fnstcw %1\n\tmovzwl %1,%%eax\n\torl $0xc00,%%eax\n\t"
                     "movw %%ax,%2\n\tfldcw %2\n\tfistpq %0\n\tfldcw %1"
                     : "=m"(converted), "=m"(control), "=m"(truncate)
                     : "t"(quotient) : "eax", "st", "memory");
#else
    converted = height ? (int64_t)quotient : (-9223372036854775807LL - 1);
#endif
    return (uint32_t)converted;
}

static void fan_span(uint32_t row, const uint32_t *first, const uint32_t *last, uint32_t texture)
{
    uint32_t count, u, v, u_step, v_step, address;
    if ((int32_t)first[0] > (int32_t)last[0]) { const uint32_t *swap = first; first = last; last = swap; }
    int32_t low_x = (int32_t)first[0] >> 16, high_x = (int32_t)last[0] >> 16;
    count = (uint32_t)(high_x - low_x + 1);
    u = first[2]; v = first[4];
    u_step = (uint32_t)((int64_t)(int32_t)(last[2] - u) / count);
    v_step = (uint32_t)((int64_t)(int32_t)(last[4] - v) / count);
    address = row + (uint32_t)low_x;
    while (count--) {
        uint8_t value = (int32_t)texture < 255 ? (uint8_t)texture :
                        *(uint8_t *)POINTER(texture + ((u >> 16) & 255) + ((v >> 16) & 255) * 256);
        *(uint8_t *)POINTER(address++) = value;
        u += u_step; v += v_step;
    }
}

static void fan_triangle(void *surface, const uint32_t *top, const uint32_t *middle,
                          const uint32_t *bottom, uint32_t texture)
{
    const uint32_t *swap;
    uint32_t long_edge[6], short_edge[6], component, height, row;
    int32_t rows, stride = (int32_t)SURFACE_WORD(surface, 0x0c);
    if ((int32_t)top[1] > (int32_t)middle[1]) { swap = top; top = middle; middle = swap; }
    if ((int32_t)middle[1] > (int32_t)bottom[1]) { swap = middle; middle = bottom; bottom = swap; }
    if ((int32_t)top[1] > (int32_t)middle[1]) { swap = top; top = middle; middle = swap; }
    if (!((top[1] ^ bottom[1]) & 0xffff0000u)) return;
    height = bottom[1] - top[1];
    long_edge[0] = top[0]; long_edge[2] = top[3]; long_edge[4] = top[4];
    long_edge[1] = fan_slope(bottom[0] - top[0], height, 0);
    long_edge[3] = fan_slope(bottom[3] - top[3], height, 1);
    long_edge[5] = fan_slope(bottom[4] - top[4], height, 1);
    row = ((uint32_t *)POINTER(SURFACE_WORD(surface, 0x10)))[(int32_t)top[1] >> 16];
    for (;;) {
        rows = ((int32_t)middle[1] >> 16) - ((int32_t)top[1] >> 16);
        if (rows > 0) {
            height = middle[1] - top[1];
            short_edge[0] = top[0]; short_edge[2] = top[3]; short_edge[4] = top[4];
            short_edge[1] = fan_slope(middle[0] - top[0], height, 0);
            short_edge[3] = fan_slope(middle[3] - top[3], height, 1);
            short_edge[5] = fan_slope(middle[4] - top[4], height, 1);
            /* The span ignores these gradients, but keep the floating work:
             * degenerate edges can set x87 exception status during division. */
            volatile uint32_t horizontal_u = fan_slope(long_edge[3] - short_edge[3], long_edge[1] - short_edge[1], 0);
            volatile uint32_t horizontal_v = fan_slope(long_edge[5] - short_edge[5], long_edge[1] - short_edge[1], 0);
            (void)horizontal_u; (void)horizontal_v;
            do {
                fan_span(row, short_edge, long_edge, texture);
                for (component = 0; component < 6; component += 2) {
                    short_edge[component] += short_edge[component + 1];
                    long_edge[component] += long_edge[component + 1];
                }
                row += stride;
            } while (--rows);
        }
        if (middle == bottom) return;
        top = middle; middle = bottom;
    }
}

uint32_t STDCALL __attribute__((used)) fan_draw_body(void *surface, uint32_t kind, uint32_t format,
    const uint32_t *vertices, uint32_t count, uint32_t flags, uint32_t entry_eax)
{
    uint32_t index, texture;
    (void)flags;
    if (kind != 6 || format != 100) return entry_eax;
    if (!GLOBAL_HALF(0xcb6) || GLOBAL_HALF(0xca0)) return ADDRESS(GLOBAL);
    texture = *(uint32_t *)POINTER(GLOBAL_WORD(0xc8c));
    if ((int32_t)(count - 1) <= 1) return count;
    for (index = 1; index < count - 1; index++)
        fan_triangle(surface, vertices, vertices + index * 5, vertices + (index + 1) * 5, texture);
    return ADDRESS(vertices + (count - 1) * 5);
}
void fan_draw(void);
__asm__(".text\n_fan_draw:\n\tpushl %eax\n\t"
        "pushl 28(%esp)\n\tpushl 28(%esp)\n\tpushl 28(%esp)\n\t"
        "pushl 28(%esp)\n\tpushl 28(%esp)\n\tpushl 28(%esp)\n\t"
        "call _fan_draw_body@28\n\tret $24\n");
