/* Gouraud vertices +0x74 (0x20785375), close +0x78 (0x20785472). */
static void gouraud_edge(uint32_t low_x, uint32_t low_y, uint32_t high_x, uint32_t high_y,
                         uint16_t low_colour, uint16_t high_colour, uint32_t *registers)
{
    uint32_t *edge = POINTER(GLOBAL_WORD(0xc14));
    uint32_t advance, difference = ((uint32_t)high_colour << 8) - ((uint32_t)low_colour << 8);
    WORD(0x8e8) = low_x; WORD(0x8ec) = low_y;
    WORD(0x8f0) = high_x; WORD(0x8f4) = high_y;
    WORD(0x4604) = (uint32_t)low_colour << 8;
    edge[0] = (uint32_t)((int32_t)low_y >> 16);
    edge[1] = (uint32_t)((int32_t)high_y >> 16);
    edge[4] = WORD(0x4604);
    if (edge[0] == edge[1]) {
        edge[2] = low_x + 0x8000;
        edge[3] = high_x - low_x;
        edge[5] = difference;
        registers[1] = (uint32_t)((int32_t)low_x >> 16);
        registers[0] = (uint32_t)((int32_t)low_y >> 16);
    } else {
        edge[2] = low_x;
        edge[3] = draw_slope(high_x - low_x, high_y - low_y);
        edge[5] = draw_slope(difference, high_y - low_y);
        advance = (low_y & 0xffff) ^ 0xffff;
        edge[2] += (uint32_t)(((int64_t)(int32_t)edge[3] * advance) >> 16);
        difference = (uint32_t)(((int64_t)(int32_t)edge[5] * advance) >> 16);
        edge[4] += difference;
        registers[1] = edge[5]; registers[0] = difference;
    }
    GLOBAL_WORD(0xc14) = ADDRESS(edge) + 24;
    registers[2] = ADDRESS(GLOBAL);
}

void STDCALL __attribute__((used)) gouraud_vertex_body(void *record, uint32_t x, uint32_t y,
    uint32_t depth, uint32_t colour, uint32_t *registers)
{
    const uint32_t *view = POINTER(SURFACE_WORD(record, 0x1c));
    uint32_t previous_x, previous_y, swap;
    uint16_t previous_colour;
    WORD(0x8d8) = view[1]; WORD(0x8dc) = view[3];
    WORD(0x8e0) = view[0]; WORD(0x8e4) = view[2];
    x = polygon_project(x, WORD(0x8e0), depth, 0) + (WORD(0x8d8) << 16);
    y = polygon_project(y, WORD(0x8e4), depth, registers) + (WORD(0x8dc) << 16);
    GLOBAL_WORD(0x2f0) += 1;
    if (GLOBAL_WORD(0x2f0) == 0) {
        WORD(0x8f8) = WORD(0x900) = x; WORD(0x8fc) = WORD(0x904) = y;
        WORD(0x8c8) = WORD(0x8cc) = y;
        HALF(0x90d) = HALF(0x90f) = (uint16_t)colour;
        registers[2] = y; registers[1] = x;
        return;
    }
    /* As in +0x48, retail's capacity branch targets the next instruction. */
    if (y < WORD(0x8c8)) WORD(0x8c8) = y;
    if (y > WORD(0x8cc)) WORD(0x8cc) = y;
    previous_x = WORD(0x900); previous_y = WORD(0x904);
    WORD(0x900) = x; WORD(0x904) = y;
    previous_colour = HALF(0x90d); HALF(0x90d) = (uint16_t)colour;
    if (y < previous_y) {
        swap = x; x = previous_x; previous_x = swap;
        swap = y; y = previous_y; previous_y = swap;
        swap = (uint16_t)colour; colour = previous_colour; previous_colour = (uint16_t)swap;
    }
    gouraud_edge(previous_x, previous_y, x, y, previous_colour, (uint16_t)colour, registers);
}

static void gouraud_sample(uint32_t *phase, uint32_t increment, uint32_t *registers)
{
    uint32_t value;
    registers[2] = (registers[2] << 8) | (*phase >> 24);
    *phase += increment;
    value = BYTE(0xd70 + (BYTE(0x100) << 8) + (uint8_t)registers[2]);
    registers[2] = (registers[2] & 0xffffff00u) | value;
}

static void gouraud_span(uint32_t first_x, uint32_t last_x, uint32_t y, uint32_t *registers)
{
    uint32_t first = first_x >> 16, last = last_x >> 16, count, increment, one, two;
    int32_t delta = (int32_t)((uint32_t)(uint16_t)(HALF(0x45e2) - HALF(0x45e0)) << 16);
    uint8_t *pixel = draw_address(POINTER(WORD(0x8c4)), first, y);
    count = (uint16_t)(last - first);
    if ((int16_t)count < 0) { registers[2] = first_x; return; }
    count = (uint16_t)(count + 1);
    /* Host signed division repair excludes INT32_MIN on faults. */
    increment = count ? (uint32_t)((int64_t)delta / count) : (delta < 0 ? 0x80000001u : 0x7fffffffu);
    increment *= 2;
    registers[2] = increment;
    one = (uint32_t)HALF(0x45e0) << 16;
    two = one + WORD(0x45f4 + ((y << 2) & 15));
    one += WORD(0x45e4 + ((y << 2) & 15));
    if (ADDRESS(pixel) & 1) {
        gouraud_sample(&two, increment, registers);
        *pixel++ = (uint8_t)registers[2];
        count--;
        if ((int32_t)count <= 0) goto done;
    }
    for (;;) {
        uint32_t pair;
        count -= 4;
        if ((int32_t)count < 0) break;
        for (pair = 0; pair < 2; pair++) {
            gouraud_sample(&two, increment, registers);
            gouraud_sample(&one, increment, registers);
            pixel[0] = (uint8_t)registers[2]; pixel[1] = (uint8_t)(registers[2] >> 8);
            pixel += 2;
        }
    }
    count += 4;
    if ((int32_t)count > 0) {
        gouraud_sample(&one, increment, registers);
        *pixel++ = (uint8_t)registers[2];
        if (--count) {
            gouraud_sample(&two, increment, registers);
            *pixel++ = (uint8_t)registers[2];
            if (--count) {
                gouraud_sample(&one, increment, registers);
                *pixel = (uint8_t)registers[2];
            }
        }
    }
done:
    registers[1] = one; registers[0] = two;
}

static void gouraud_draw(uint32_t *left, uint32_t *right, uint32_t y, int flat, uint32_t *registers)
{
    uint32_t first = left[2], last;
    uint16_t first_colour = (uint16_t)(left[4] >> 8), last_colour;
    if (flat) {
        last = first + left[3];
        last_colour = (uint16_t)(first_colour + (uint16_t)(left[5] >> 8));
    } else {
        last = right[2]; last_colour = (uint16_t)(right[4] >> 8);
    }
    registers[1] = (registers[1] & 0xffff0000u) | first_colour;
    registers[0] = (registers[0] & 0xffff0000u) | last_colour;
    if ((int32_t)first > (int32_t)last) {
        uint32_t swap = first; first = last; last = swap;
        first_colour = (uint16_t)registers[0]; last_colour = (uint16_t)registers[1];
        registers[1] = (registers[1] & 0xffff0000u) | first_colour;
        registers[0] = (registers[0] & 0xffff0000u) | last_colour;
    }
    HALF(0x45e0) = first_colour; HALF(0x45e2) = last_colour;
    gouraud_span(first, last, y, registers);
}

static uint32_t *gouraud_find(uint32_t y, uint32_t *other, uint32_t *registers)
{
    uint32_t *edge;
    for (;;) {
        edge = (uint32_t *)(hg2d_image + 0x104);
        for (;;) {
            if (edge != other && edge[0] == y) break;
            edge += 6;
            if (ADDRESS(edge) >= WORD(0x884)) return 0;
        }
        if (edge[1] != y) return edge;
        if (y == WORD(0x8cc)) gouraud_draw(edge, 0, y, 1, registers);
        edge[0] = 0xffffffffu;
    }
}

void STDCALL __attribute__((used)) gouraud_close_body(void *surface, uint32_t *registers)
{
    uint32_t high_x = WORD(0x8f8), high_y = WORD(0x8fc), low_x = WORD(0x900), low_y = WORD(0x904), swap;
    uint16_t high_colour = HALF(0x90f), low_colour = HALF(0x90d), colour_swap;
    uint32_t *left, *right, y;
    WORD(0x8c4) = ADDRESS(surface);
    if (high_y < low_y) {
        swap = high_x; high_x = low_x; low_x = swap;
        swap = high_y; high_y = low_y; low_y = swap;
        colour_swap = high_colour; high_colour = low_colour; low_colour = colour_swap;
    }
    gouraud_edge(low_x, low_y, high_x, high_y, low_colour, high_colour, registers);
    registers[1] = GLOBAL_WORD(0xc14);
    WORD(0x8c8) >>= 16; WORD(0x8cc) >>= 16;
    registers[2] = draw_distance((uint8_t)ADDRESS(GLOBAL));
    registers[2] |= WORD(0x100) << 8;
    WORD(0x884) = GLOBAL_WORD(0xc14);
    y = WORD(0x8c8);
    left = gouraud_find(y, 0, registers);
    if (!left) return;
    right = gouraud_find(y, left, registers);
    if (!right) return;
    for (;;) {
        if ((int32_t)y >= (int32_t)left[1]) {
            left[0] = 0xffffffffu; left = gouraud_find(y, right, registers);
            if (!left) return;
        }
        if ((int32_t)y >= (int32_t)right[1]) {
            right[0] = 0xffffffffu; right = gouraud_find(y, left, registers);
            if (!right) return;
        }
        gouraud_draw(left, right, y, 0, registers);
        left[2] += left[3]; left[4] += left[5];
        right[2] += right[3]; right[4] += right[5];
        registers[2] = right[5];
        y++;
    }
}

void gouraud_vertex(void);
__asm__(".text\n_gouraud_vertex:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "movl %esp,%eax\n\tpushl %eax\n\t.rept 5\n\tpushl 36(%esp)\n\t.endr\n\t"
        "call _gouraud_vertex_body@24\n\tpopl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $20\n");
void gouraud_close(void);
__asm__(".text\n_gouraud_close:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "movl %esp,%eax\n\tpushl %eax\n\tpushl 20(%esp)\n\tcall _gouraud_close_body@8\n\t"
        "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $4\n");
