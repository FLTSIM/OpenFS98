/* +0x54, 0x20788851: close the 40-byte edges and shade affine texture samples.
 * The third attribute selects the first shade lookup; distance selects the
 * second. The established perspective entry names are retained for +0x50. */
static void perspective_sample(uint32_t first, uint32_t second, uint32_t bias,
                               uint32_t *one, uint32_t *two, int second_phase, int high_byte)
{
    uint32_t index = (((first + bias) >> 24) << 8) | ((second + bias) >> 24);
    uint32_t phase, shift = high_byte ? 8 : 0, mask = 0xffu << shift;
    uint8_t value = *(uint8_t *)POINTER(WORD(0x948) + index);
    *one = (*one & ~mask) | ((uint32_t)value << shift);
    if (second_phase) {
        *two += WORD(0x4dc4);
        phase = *two;
    } else {
        *one += WORD(0x4dc4);
        phase = *one;
    }
    index = ((phase >> 19) & 0xff00) | value;
    value = BYTE(0x4e00 + index);
    value = BYTE(0xd70 + (BYTE(0x100) << 8) + value);
    *one = (*one & ~mask) | ((uint32_t)value << shift);
}

static void perspective_pair(uint32_t *first, uint32_t *second, uint32_t bias,
                             uint32_t *one, uint32_t *two, uint8_t *pixel)
{
    perspective_sample(*first, *second, bias, one, two, 0, 0);
    *first += WORD(0x4dd0); *second += WORD(0x4dd4);
    perspective_sample(*first, *second, bias + (HALF(0x944) ? 0x800000 : 0), one, two, 1, 1);
    *first += WORD(0x4dd0); *second += WORD(0x4dd4);
    /* Both samples and shade lookups precede the word store. */
    pixel[0] = (uint8_t)*one; pixel[1] = (uint8_t)(*one >> 8);
}

static void perspective_span(uint32_t *left, uint32_t *right, uint32_t y, uint32_t *registers)
{
    uint32_t first_x = (uint16_t)(left[2] >> 16), last_x = (uint16_t)(right[2] >> 16);
    uint32_t count, first, second, one, two, bias, pair;
    if (first_x > last_x) {
        uint32_t *temporary_edge = left; left = right; right = temporary_edge;
        count = first_x; first_x = last_x; last_x = count;
    }
    count = last_x - first_x + 1;
    WORD(0x4ddc) = count;
    WORD(0x4dd8) = ADDRESS(draw_address(POINTER(WORD(0x8c4)), first_x, y));
    WORD(0x4dcc) = ((uint32_t)((int64_t)(int32_t)(right[8] - left[8]) / count) * 2) & 0xffff0000u;
    WORD(0x4dc8) = left[8];
    WORD(0x4dd0) = affine_divide(right[4] - left[4], count);
    WORD(0x4dd4) = affine_divide(right[5] - left[5], count);
    first = left[4] << 8; second = left[5] << 8;
    WORD(0x4dc4) = WORD(0x4dcc);
    one = left[8] - 0x06000000u;
    two = one + WORD(0x4df0 + (y & 3) * 4) - WORD(0x4dcc);
    one += WORD(0x4de0 + (y & 3) * 4) - WORD(0x4dcc);
    bias = HALF(0x944) && !(y & 1) ? 0x400000 : 0;
    if (WORD(0x4dd8) & 1) {
        perspective_sample(first, second, HALF(0x944) ? bias + 0x800000 : 0, &one, &two, 1, 0);
        *(uint8_t *)POINTER(WORD(0x4dd8)) = (uint8_t)one;
        first += WORD(0x4dd0); second += WORD(0x4dd4);
        WORD(0x4dd8)++; WORD(0x4ddc)--;
        if (!WORD(0x4ddc)) goto finished;
    }
    for (;;) {
        WORD(0x4ddc) -= 8;
        if ((int32_t)WORD(0x4ddc) < 0) break;
        for (pair = 0; pair < 4; pair++)
            perspective_pair(&first, &second, bias, &one, &two, POINTER(WORD(0x4dd8) + pair * 2));
        WORD(0x4dd8) += 8;
    }
    WORD(0x4ddc) += 8;
    for (pair = 0; pair < 4; pair++) {
        WORD(0x4ddc) -= 2;
        if ((int32_t)WORD(0x4ddc) < 0) {
            WORD(0x4ddc)++;
            if ((int32_t)WORD(0x4ddc) >= 0) {
                perspective_sample(first, second, bias, &one, &two, 0, 0);
                *(uint8_t *)POINTER(WORD(0x4dd8)) = (uint8_t)one;
                first += WORD(0x4dd0); second += WORD(0x4dd4);
            }
            break;
        }
        perspective_pair(&first, &second, bias, &one, &two, POINTER(WORD(0x4dd8)));
        WORD(0x4dd8) += 2;
    }
finished:
    registers[2] = one; registers[1] = first; registers[0] = WORD(0x4dd0);
}

static uint32_t *perspective_find(uint32_t y, uint32_t *other, uint32_t *registers)
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
            WORD(0x4d7c) = edge[2]; WORD(0x4d84) = edge[4];
            WORD(0x4d88) = edge[5]; WORD(0x4d94) = edge[8];
            WORD(0x4da4) = edge[2] + edge[3]; WORD(0x4dac) = edge[4] + edge[6];
            WORD(0x4db0) = edge[5] + edge[7]; WORD(0x4dbc) = edge[8] + edge[9];
            perspective_span((uint32_t *)(hg2d_image + 0x4d74), (uint32_t *)(hg2d_image + 0x4d9c), y, registers);
        }
        edge[0] = 0xffffffffu;
    }
}

static void perspective_walk(uint32_t *registers)
{
    uint32_t y = WORD(0x8c8);
    uint32_t *left, *right;
    HALF(0x944) = GLOBAL_HALF(0xcce);
    WORD(0x948) = *(uint32_t *)POINTER(GLOBAL_WORD(0xc8c));
    WORD(0x4d70) = 0x20788d29;
    WORD(0x884) = registers[1]; registers[2] = WORD(0x948);
    left = perspective_find(y, 0, registers);
    if (!left) return;
    right = perspective_find(y, left, registers);
    if (!right) return;
    for (;;) {
        if ((int32_t)y >= (int32_t)left[1]) {
            left[0] = 0xffffffffu; left = perspective_find(y, right, registers);
            if (!left) return;
        }
        if ((int32_t)y >= (int32_t)right[1]) {
            right[0] = 0xffffffffu; right = perspective_find(y, left, registers);
            if (!right) return;
        }
        perspective_span(left, right, y, registers);
        left[2] += left[3]; left[4] += left[6]; left[5] += left[7]; left[8] += left[9];
        right[2] += right[3]; right[4] += right[6]; right[5] += right[7]; right[8] += right[9];
        registers[2] = right[9];
        y++;
    }
}

void STDCALL __attribute__((used)) perspective_close_body(void *surface, uint32_t *registers)
{
    uint32_t high_x = WORD(0x8f8), high_y = WORD(0x8fc);
    uint32_t low_x = WORD(0x900), low_y = WORD(0x904), temporary;
    WORD(0x8c4) = ADDRESS(surface);
    WORD(0x924) = WORD(0x92c); WORD(0x928) = WORD(0x930); WORD(0x940) = WORD(0x938);
    WORD(0x91c) = WORD(0x914); WORD(0x920) = WORD(0x918); WORD(0x93c) = WORD(0x934);
    if (high_y < low_y) {
        temporary = high_x; high_x = low_x; low_x = temporary;
        temporary = high_y; high_y = low_y; low_y = temporary;
        temporary = WORD(0x924); WORD(0x924) = WORD(0x91c); WORD(0x91c) = temporary;
        temporary = WORD(0x928); WORD(0x928) = WORD(0x920); WORD(0x920) = temporary;
        temporary = WORD(0x940); WORD(0x940) = WORD(0x93c); WORD(0x93c) = temporary;
    }
    perspective_edge(low_x, low_y, high_x, high_y, registers);
    registers[1] = GLOBAL_WORD(0xc14);
    WORD(0x8c8) >>= 16; WORD(0x8cc) >>= 16;
    draw_distance(0);
    perspective_walk(registers);
}

void perspective_close(void);
__asm__(".text\n_perspective_close:\n\tpushl %ebp\n\tmovl %esp,%ebp\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"
        "movl 8(%ebp),%eax\n\tmovl %eax,_hg2d_image+0x8c4\n\t"
        "movl _hg2d_image+0xc,%edi\n\t"
        "testw $0xffff,0xcb6(%edi)\n\tjnz 1f\n\t"
        "pushl 8(%ebp)\n\tcall _polygon_close\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret $4\n"
        "1:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\tmovl %esp,%eax\n\t"
        "pushl %eax\n\tpushl 8(%ebp)\n\tcall _perspective_close_body@8\n\t"
        "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tpopl %ebp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret $4\n");
