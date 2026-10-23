/* +0x4c, 0x20787bbe: affine textured closing and both rasterization backends. */
static uint32_t affine_divide(uint32_t delta, uint32_t count)
{
    /* CDQ precedes SHL EAX,7: only the low dividend dword is shifted. */
    int64_t dividend;
    int64_t quotient;
    /* Only the sign of delta belongs in the high dword. */
    dividend = (delta & 0x80000000u ? -0x100000000LL : 0) + (uint32_t)(delta << 7);
    if (!count) return (delta & 0x80000000u ? 0x80000001u : 0x7fffffffu) * 2;
    quotient = dividend / count;
    if (quotient < -2147483648LL || quotient > 2147483647LL)
        return (delta & 0x80000000u ? 0x80000001u : 0x7fffffffu) * 2;
    return (uint32_t)quotient * 2;
}

static uint8_t affine_texel(uint32_t first, uint32_t second, uint32_t bias)
{
    uint32_t index = (((first + bias) >> 24) << 8) | ((second + bias) >> 24);
    return *(uint8_t *)POINTER(WORD(0x948) + index);
}

/* One low-resolution sample writes two pixels. The threshold is on the raw
 * texel, before shading. Preserve EAX's partial byte writes on skipped keys. */
static void affine_double(uint8_t *pixel, uint32_t *first, uint32_t *second,
                          uint32_t selector, uint32_t *registers)
{
    uint8_t value = affine_texel(*first, *second, 0);
    *first += WORD(0x4618); *second += WORD(0x461c);
    if (!(selector & 1)) registers[2] = (registers[2] & 0xffffff00u) | value;
    if ((selector & 2) && value < BYTE(0x946)) return;
    if (selector & 1) value = BYTE(0xd70 + WORD(0x100) * 256 + value);
    registers[2] = (registers[2] & 0xffff0000u) | value * 0x101u;
    pixel[0] = value; pixel[1] = value;
}

static void affine_pair(uint8_t *pixel, uint32_t *first, uint32_t *second,
                        uint32_t selector, uint32_t y, uint32_t *registers)
{
    uint32_t bias = selector & 4 ? (y & 1 ? 0 : 0x400000) : 0;
    uint8_t one = affine_texel(*first, *second, bias), two;
    *first += WORD(0x4618); *second += WORD(0x461c);
    if (selector & 4) bias += 0x800000;
    two = affine_texel(*first, *second, bias);
    *first += WORD(0x4618); *second += WORD(0x461c);
    if (selector & 1) {
        one = BYTE(0xd70 + WORD(0x100) * 256 + one);
        two = BYTE(0xd70 + WORD(0x100) * 256 + two);
    }
    registers[2] = (registers[2] & 0xffff0000u) | one | ((uint32_t)two << 8);
    pixel[0] = one; pixel[1] = two;
}

static void affine_single(uint8_t *pixel, uint32_t *first, uint32_t *second,
                          uint32_t selector, uint32_t bias, uint32_t *registers)
{
    uint8_t value = affine_texel(*first, *second, bias);
    *first += WORD(0x4618); *second += WORD(0x461c);
    if (selector & 1) value = BYTE(0xd70 + WORD(0x100) * 256 + value);
    registers[2] = (registers[2] & 0xffffff00u) | value;
    *pixel = value;
}

/* Optimized dithering rounds the two coordinate formats separately. The
 * phase follows the destination byte's parity, including alignment prefixes. */
static uint32_t affine_optimized_index(uint32_t first, uint32_t second,
                                      uint32_t selector, uint32_t y, uint32_t address)
{
    uint32_t bias = 0;
    if ((selector & 12) == 12) {
        bias = (y & 1 ? 0 : 0x400000) + (address & 1 ? 0x800000 : 0);
    }
    return (((first + bias) & 0xff000000u) + second + (bias >> 8)) >> 16;
}

/* The optimized backend uses true signed deltas and patches texture/map
 * bases into retail instructions. C reads those bases directly instead. */
static void affine_optimized(uint32_t *left, uint32_t *right, uint32_t y,
    uint32_t selector, int flat, uint32_t *registers)
{
    uint32_t count, first, second, step_first, step_second, index, remainder;
    uint8_t *pixel;
    int keyed = (selector & 2) != 0;
    WORD(0x5e04) = ADDRESS(registers) + 32 - (flat ? 44 : 40);
    if (left[2] > right[2]) { uint32_t *swap = left; left = right; right = swap; }
    count = (right[2] >> 16) - (left[2] >> 16) + 1;
    pixel = draw_address(POINTER(WORD(0x8c4)), left[2] >> 16, y);
    step_second = (uint32_t)((int64_t)(int32_t)(right[5] - left[5]) / count);
    step_first = (uint32_t)((int64_t)(int32_t)(right[4] - left[4]) / count);
    remainder = (uint32_t)((int64_t)(int32_t)(right[4] - left[4]) % count);
    step_first <<= 8;
    first = left[4] << 8; second = left[5];
    registers[2] = step_first;
    if (keyed) {
        registers[0] = (remainder & 0xffffff00u) | BYTE(0x946);
        while (count--) {
            uint32_t at = ((first & 0xff000000u) | second) >> 16;
            uint8_t value = *(uint8_t *)POINTER(WORD(0x948) + at);
            first += step_first; second += step_second;
            if (value >= BYTE(0x946)) {
                if (selector & 1) {
                    value = BYTE(0xd70 + WORD(0x100) * 256 + value);
                    pixel[1] = value; /* Retail's shaded-key path writes after INC EDI. */
                } else {
                    pixel[0] = value;
                }
            }
            pixel++;
        }
    } else {
        while ((ADDRESS(pixel) & 3) && count) {
            uint32_t at = affine_optimized_index(first, second, selector, y, ADDRESS(pixel));
            uint8_t value = *(uint8_t *)POINTER(WORD(0x948) + at);
            if (selector & 1) value = BYTE(0xd70 + WORD(0x100) * 256 + value);
            first += step_first; second += step_second;
            *pixel++ = value; registers[0] = (at & 0xff00) | value;
            count--;
        }
        if (count >= 4) {
            do {
                uint8_t values[4];
                for (index = 0; index < 4; index++) {
                    uint32_t at = affine_optimized_index(first, second, selector, y, ADDRESS(pixel) + index);
                    values[index] = *(uint8_t *)POINTER(WORD(0x948) + at);
                    if (selector & 1) values[index] = BYTE(0xd70 + WORD(0x100) * 256 + values[index]);
                    first += step_first; second += step_second;
                }
                registers[0] = values[0] | ((uint32_t)values[1] << 8) |
                               ((uint32_t)values[2] << 16) | ((uint32_t)values[3] << 24);
                for (index = 0; index < 4; index++) pixel[index] = values[index];
                pixel += 4; count -= 4;
                WORD(0x5e08) = count - 4;
            } while (count >= 4);
        }
        while (count--) {
            uint32_t at = affine_optimized_index(first, second, selector, y, ADDRESS(pixel));
            uint8_t value = *(uint8_t *)POINTER(WORD(0x948) + at);
            if (selector & 1) value = BYTE(0xd70 + WORD(0x100) * 256 + value);
            first += step_first; second += step_second;
            *pixel++ = value; registers[0] = (at & 0xff00) | value;
        }
    }
    registers[1] = second;
}

static void affine_span(uint32_t *left, uint32_t *right, uint32_t y, uint32_t selector,
                        int flat, uint32_t *registers)
{
    uint32_t first_x, last_x, count, first, second, pair;
    uint8_t *pixel;
    int doubled = (selector & 2) || !(selector & 8);
    if (WORD(0x4634) == 0x20787805) {
        uint32_t initial = left[2], final = right[2], temporary;
        BYTE(0x90c) = BYTE(0x1c7a);
        if (initial > final) { temporary = initial; initial = final; final = temporary; }
        initial >>= 16; final >>= 16;
        registers[2] = initial; registers[1] = final;
        if (y >= SURFACE_WORD(POINTER(WORD(0x8c4)), 8)) return;
        if (GLOBAL_WORD(0x1298)) {
            registers[2] = GLOBAL_WORD(0x1298); registers[1] = final - initial + 1;
            draw_map_pixels_state(draw_address(POINTER(WORD(0x8c4)), initial, y),
                                  registers[1], POINTER(WORD(0x1d88)), registers);
            return;
        }
        draw_horizontal_pixels(POINTER(WORD(0x8c4)), initial, final, y);
        registers[2] = BYTE(0x90c) * (final - initial + 1 >= 8 ? 0x1010101u : 1);
        registers[1] = 0;
        return;
    }
    if (selector & 16) {
        affine_optimized(left, right, y, selector, flat, registers);
        return;
    }
    first_x = (uint16_t)(left[2] >> 16);
    last_x = (uint16_t)(right[2] >> 16);
    if (doubled) { first_x &= ~1u; last_x &= ~1u; }
    if (first_x > last_x) {
        uint32_t *temporary_edge = left; left = right; right = temporary_edge;
        count = first_x; first_x = last_x; last_x = count;
    }
    count = last_x - first_x + (doubled ? 2 : 1);
    WORD(0x4630) = count;
    registers[1] = last_x; registers[0] = count;
    if (count > 1500) return;
    pixel = draw_address(POINTER(WORD(0x8c4)), first_x, y);
    WORD(0x462c) = ADDRESS(pixel);
    WORD(0x4618) = affine_divide(right[4] - left[4], doubled ? count / 2 : count);
    WORD(0x461c) = affine_divide(right[5] - left[5], doubled ? count / 2 : count);
    first = left[4] << 8; second = left[5] << 8;
    registers[0] = WORD(0x4618); registers[2] = WORD(0x461c);
    if (doubled) {
        if (ADDRESS(pixel) & 2) {
            affine_double(pixel, &first, &second, selector, registers);
            pixel += 2; WORD(0x4630) -= 2;
        }
        for (;;) {
            WORD(0x4630) -= 8;
            if ((int32_t)WORD(0x4630) < 0) break;
            for (pair = 0; pair < 4;) {
                if (!(selector & 3)) {
                    uint8_t values[4];
                    uint32_t saved_eax, index;
                    affine_double(values, &first, &second, selector, registers);
                    saved_eax = registers[2];
                    affine_double(values + 2, &first, &second, selector, registers);
                    registers[2] = saved_eax;
                    for (index = 0; index < 4; index++) pixel[index] = values[index];
                    pixel += 4; pair += 2;
                } else {
                    affine_double(pixel, &first, &second, selector, registers);
                    pixel += 2; pair++;
                }
            }
        }
        WORD(0x4630) += 8;
        for (pair = 0; pair < 4; pair++) {
            WORD(0x4630) -= 2;
            if ((int32_t)WORD(0x4630) < 0) break;
            affine_double(pixel, &first, &second, selector, registers);
            pixel += 2;
        }
    } else {
        if (ADDRESS(pixel) & 1) {
            uint32_t bias = selector & 4 ? (y & 1 ? 0x800000 : 0xc00000) : 0;
            affine_single(pixel, &first, &second, selector, bias, registers);
            pixel++; WORD(0x4630)--;
            if (!WORD(0x4630)) goto finished;
        }
        for (;;) {
            WORD(0x4630) -= 8;
            if ((int32_t)WORD(0x4630) < 0) break;
            for (pair = 0; pair < 4; pair++) {
                affine_pair(pixel, &first, &second, selector, y, registers);
                pixel += 2;
            }
        }
        WORD(0x4630) += 8;
        for (pair = 0; pair < 4; pair++) {
            WORD(0x4630) -= 2;
            if ((int32_t)WORD(0x4630) < 0) {
                WORD(0x4630)++;
                if ((int32_t)WORD(0x4630) >= 0)
                    affine_single(pixel, &first, &second, selector, (selector & 4) && !(y & 1) ? 0x400000 : 0, registers);
                break;
            }
            affine_pair(pixel, &first, &second, selector, y, registers);
            pixel += 2;
        }
    }
finished:
    registers[1] = first;
}

static uint32_t *affine_find(uint32_t y, uint32_t *other, uint32_t selector, uint32_t *registers)
{
    uint32_t *edge;
    for (;;) {
        edge = (uint32_t *)(hg2d_image + 0x104);
        for (;;) {
            if (edge != other && edge[0] == y) break;
            edge += 8;
            if (ADDRESS(edge) >= WORD(0x884)) return 0;
        }
        if (edge[1] != y) return edge;
        if (y == WORD(0x8cc)) {
            WORD(0x4640) = edge[2]; WORD(0x4648) = edge[4]; WORD(0x464c) = edge[5];
            WORD(0x4660) = edge[2] + edge[3];
            WORD(0x4668) = edge[4] + edge[6]; WORD(0x466c) = edge[5] + edge[7];
            registers[2] = WORD(0x4660); registers[1] = WORD(0x466c);
            affine_span((uint32_t *)(hg2d_image + 0x4638), (uint32_t *)(hg2d_image + 0x4658), y, selector, 1, registers);
        }
        edge[0] = 0xffffffffu;
    }
}

static void affine_walk(uint32_t *registers)
{
    uint32_t selector, y = WORD(0x8c8);
    uint32_t *left, *right;
    WORD(0x884) = GLOBAL_WORD(0xc14);
    WORD(0x948) = *(uint32_t *)POINTER(GLOBAL_WORD(0xc8c));
    draw_distance(0);
    registers[2] = WORD(0x100); registers[0] = 0;
    if (WORD(0x100) == 15) {
        selector = 0;
        WORD(0x4634) = 0x20787805;
    } else {
        HALF(0x946) = GLOBAL_HALF(0xca0);
        selector = (WORD(0x100) != 0) | ((BYTE(0x946) != 0) << 1) |
                   ((GLOBAL[0xcce] != 0) << 2) | ((GLOBAL_HALF(0xc92) >= 1) << 3);
        selector |= WORD(0x4678);
        registers[0] = selector; registers[2] = HALF(0x946);
        registers[1] = WORD(0x46fc + selector * 4);
        if (registers[1]) registers[2] = WORD(0x948);
        WORD(0x4634) = WORD(0x467c + selector * 4);
    }
    left = affine_find(y, 0, selector, registers);
    if (!left) return;
    right = affine_find(y, left, selector, registers);
    if (!right) return;
    for (;;) {
        if ((int32_t)y >= (int32_t)left[1]) {
            left[0] = 0xffffffffu; left = affine_find(y, right, selector, registers);
            if (!left) return;
        }
        if ((int32_t)y >= (int32_t)right[1]) {
            right[0] = 0xffffffffu; right = affine_find(y, left, selector, registers);
            if (!right) return;
        }
        affine_span(left, right, y, selector, 0, registers);
        left[2] += left[3]; left[4] += left[6]; left[5] += left[7];
        right[2] += right[3]; right[4] += right[6]; right[5] += right[7];
        registers[2] = right[7];
        y++;
    }
}

void STDCALL __attribute__((used)) affine_close_body(void *surface, uint32_t *registers)
{
    uint32_t high_x = WORD(0x8f8), high_y = WORD(0x8fc);
    uint32_t low_x = WORD(0x900), low_y = WORD(0x904), temporary;
    WORD(0x8c4) = ADDRESS(surface);
    WORD(0x924) = WORD(0x92c); WORD(0x928) = WORD(0x930);
    WORD(0x91c) = WORD(0x914); WORD(0x920) = WORD(0x918);
    if (high_y < low_y) {
        temporary = high_x; high_x = low_x; low_x = temporary;
        temporary = high_y; high_y = low_y; low_y = temporary;
        temporary = WORD(0x924); WORD(0x924) = WORD(0x91c); WORD(0x91c) = temporary;
        temporary = WORD(0x928); WORD(0x928) = WORD(0x920); WORD(0x920) = temporary;
    }
    textured_edge(low_x, low_y, high_x, high_y, registers);
    registers[1] = GLOBAL_WORD(0xc14);
    WORD(0x8c8) >>= 16; WORD(0x8cc) >>= 16;
    affine_walk(registers);
}

void affine_close(void);
__asm__(".text\n_affine_close:\n\tpushl %ebp\n\tmovl %esp,%ebp\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"
        "movl 8(%ebp),%eax\n\tmovl _hg2d_image+0xc,%edi\n\t"
        "testw $0xffff,0xcb6(%edi)\n\tjnz 1f\n\t"
        "pushl 8(%ebp)\n\tcall _polygon_close\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret $4\n"
        "1:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\tmovl %esp,%eax\n\t"
        "pushl %eax\n\tpushl 8(%ebp)\n\tcall _affine_close_body@8\n\t"
        "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tpopl %ebp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret $4\n");
