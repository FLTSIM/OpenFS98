/* Affine textured vertices, +0x48 (0x20787a2c), edge builder 0x20785780.
 * The fifth argument becomes the second edge attribute, the sixth the first.
 * registers[] holds the incoming/outgoing EDX, ECX, EAX for the entry shim. */
static void textured_edge(uint32_t low_x, uint32_t low_y, uint32_t high_x, uint32_t high_y,
                          uint32_t *registers)
{
    uint32_t *edge = POINTER(GLOBAL_WORD(0xc14));
    uint32_t height, advance, adjustment;
    WORD(0x8e8) = low_x; WORD(0x8ec) = low_y;
    WORD(0x8f0) = high_x; WORD(0x8f4) = high_y;
    edge[0] = (uint32_t)((int32_t)low_y >> 16);
    edge[1] = (uint32_t)((int32_t)high_y >> 16);
    edge[4] = WORD(0x924); edge[5] = WORD(0x928);
    edge[2] = low_x;
    if ((uint16_t)edge[0] == (uint16_t)edge[1]) {
        edge[3] = high_x - low_x;
        edge[6] = WORD(0x91c) - WORD(0x924);
        edge[7] = WORD(0x920) - WORD(0x928);
        registers[1] = (uint32_t)((int32_t)low_x >> 16);
        registers[0] = (uint32_t)((int32_t)low_y >> 16);
    } else {
        height = high_y - low_y;
        edge[3] = draw_slope(high_x - low_x, height);
        edge[6] = draw_slope(WORD(0x91c) - WORD(0x924), height);
        edge[7] = draw_slope(WORD(0x920) - WORD(0x928), height);
        advance = (low_y & 0xffff) ^ 0xffff;
        edge[2] += (uint32_t)(((int64_t)(int32_t)edge[3] * advance) >> 16);
        edge[4] += (uint32_t)(((int64_t)(int32_t)edge[6] * advance) >> 16);
        adjustment = (uint32_t)(((int64_t)(int32_t)edge[7] * advance) >> 16);
        edge[5] += adjustment;
        registers[1] = edge[7]; registers[0] = adjustment;
    }
    GLOBAL_WORD(0xc14) = ADDRESS(edge) + 32;
    registers[2] = ADDRESS(GLOBAL);
}

void STDCALL __attribute__((used)) textured_vertex_body(void *record, uint32_t x, uint32_t y,
    uint32_t depth, uint32_t first_attribute, uint32_t second_attribute, uint32_t *registers)
{
    const uint32_t *view;
    uint32_t screen_x, screen_y, previous_x, previous_y, temporary;
    if (!GLOBAL_HALF(0xcb6)) {
        polygon_vertex_body(record, x, y, depth);
        return;
    }
    view = POINTER(SURFACE_WORD(record, 0x1c));
    WORD(0x8d8) = view[1]; WORD(0x8dc) = view[3];
    WORD(0x8e0) = view[0]; WORD(0x8e4) = view[2];
    screen_x = polygon_project(x, WORD(0x8e0), depth, 0) + (WORD(0x8d8) << 16);
    screen_y = polygon_project(y, WORD(0x8e4), depth, registers) + (WORD(0x8dc) << 16);
    GLOBAL_WORD(0x2f0) += 1;
    if (GLOBAL_WORD(0x2f0) == 0) {
        WORD(0x8f8) = WORD(0x900) = screen_x;
        WORD(0x8fc) = WORD(0x904) = screen_y;
        WORD(0x8c8) = WORD(0x8cc) = screen_y;
        WORD(0x914) = WORD(0x92c) = second_attribute;
        WORD(0x918) = WORD(0x930) = first_attribute;
        registers[2] = screen_y; registers[1] = WORD(0x8dc) << 16;
        return;
    }
    /* Retail's capacity cmp/jb branches to its very next instruction.
     * It has no effect: unlike +0x44, this path keeps appending edges. */
    if (screen_y < WORD(0x8c8)) WORD(0x8c8) = screen_y;
    if (screen_y > WORD(0x8cc)) WORD(0x8cc) = screen_y;
    WORD(0x91c) = second_attribute; WORD(0x920) = first_attribute;
    WORD(0x924) = WORD(0x92c); WORD(0x928) = WORD(0x930);
    WORD(0x92c) = second_attribute; WORD(0x930) = first_attribute;
    previous_x = WORD(0x900); previous_y = WORD(0x904);
    WORD(0x900) = screen_x; WORD(0x904) = screen_y;
    if (screen_y < previous_y) {
        temporary = screen_x; screen_x = previous_x; previous_x = temporary;
        temporary = screen_y; screen_y = previous_y; previous_y = temporary;
        temporary = WORD(0x924); WORD(0x924) = WORD(0x91c); WORD(0x91c) = temporary;
        temporary = WORD(0x928); WORD(0x928) = WORD(0x920); WORD(0x920) = temporary;
    }
    textured_edge(previous_x, previous_y, screen_x, screen_y, registers);
}

void textured_vertex(void);
__asm__(".text\n_textured_vertex:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "movl %esp,%eax\n\tpushl %eax\n\t"
        ".rept 6\n\tpushl 40(%esp)\n\t.endr\n\t"
        "call _textured_vertex_body@28\n\t"
        "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $24\n");
