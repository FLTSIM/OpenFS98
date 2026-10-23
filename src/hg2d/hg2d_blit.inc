/* +0x80, 0x2078aff0: packed-coordinate scanline mapper, nine arguments. */
static uint32_t blit_divide(int32_t numerator, int32_t denominator)
{
    if (!denominator) return numerator < 0 ? 0x80000001u : 0x7fffffffu;
    return (uint32_t)((int64_t)numerator / denominator);
}

static uint8_t blit_sample(const uint8_t *texture, uint32_t coordinate)
{
    return texture[(coordinate & 0xff00) + (coordinate >> 24)];
}

void STDCALL __attribute__((used)) blit_body(void *surface, uint32_t row_mode,
    uint32_t first_x, uint32_t last_x, uint32_t first_u, uint32_t first_v,
    uint32_t last_u, uint32_t last_v, const uint8_t *texture, uint32_t *registers)
{
    uint32_t row = row_mode & 0xfff, mode = (row_mode & 0xffff) >> 12;
    uint32_t coordinate, increment, step_v, index;
    int32_t count;
    uint8_t *pixel;
    registers[1] = row_mode & 0xffff;
    registers[2] = SURFACE_WORD(surface, 0x88); registers[0] = ADDRESS(surface);
    if ((int32_t)row >= (int32_t)registers[2]) return;
    first_x &= 0xffff; last_x &= 0xffff;
    first_u &= 0xffff; first_v &= 0xffff;
    last_u &= 0xffff; last_v &= 0xffff;
    pixel = POINTER(*(uint32_t *)POINTER(SURFACE_WORD(surface, 0x90) + row * 4) + first_x);
    count = (int32_t)last_x - first_x + 1;
    step_v = blit_divide((int32_t)last_v - first_v, count);
    coordinate = (first_u << 16) + first_v;
    increment = (blit_divide((int32_t)last_u - first_u, count) << 16) + step_v;
    registers[2] = mode; registers[0] = step_v;
    if (mode == 1) {
        uint32_t colour = HALF(0x34ca);
        uint8_t value = BYTE(0x94e + colour);
        registers[2] = (colour & 0xff00) | value;
        if (count <= 0) return;
        registers[2] = value * 0x1010101u; registers[0] = (uint32_t)count;
        for (index = 0; index < (uint32_t)count; index++) ((volatile uint8_t *)pixel)[index] = value;
        return;
    }
    if (mode == 2 || mode == 4) {
        uint32_t at = mode == 2 ? 0x29a0 : 0x29a2;
        uint8_t lower = BYTE(0x94e + BYTE(at)), upper = BYTE(0x94e + BYTE(at + 1));
        registers[2] = (uint32_t)count;
        for (index = 0; index < (uint32_t)(count > 0 ? count : 0); index++) {
            uint8_t value = (int32_t)first_v <= 0x7f00 ? lower : upper;
            registers[2] = (registers[2] & 0xffffff00u) | value;
            *pixel++ = value;
            first_v += step_v;
        }
        return;
    }
    if (mode == 8) {
        if (count <= 0) { registers[0] = (uint32_t)count; return; }
        for (index = 0; index < (uint32_t)count; index++) {
            uint8_t value = blit_sample(texture, coordinate), mapped = BYTE(0x94e + value);
            coordinate += increment;
            registers[0] = mapped;
            if (value) *pixel = mapped;
            pixel++;
        }
        registers[2] = 0;
        return;
    }
    if (mode != 0 || count <= 0) return;
    if (count > 4) {
        while (ADDRESS(pixel) & 3) {
            uint8_t value = blit_sample(texture, coordinate);
            registers[0] = coordinate >> 24;
            coordinate += increment;
            *pixel++ = BYTE(0x94e + value);
            count--;
        }
        while (count >= 4) {
            uint8_t values[4];
            for (index = 0; index < 4; index++) {
                values[index] = BYTE(0x94e + blit_sample(texture, coordinate));
                coordinate += increment;
            }
            for (index = 0; index < 4; index++) pixel[index] = values[index];
            registers[0] = (uint32_t)values[3] << 24;
            pixel += 4; count -= 4;
        }
    }
    while (count > 0) {
        uint8_t value = blit_sample(texture, coordinate);
        registers[0] = coordinate >> 24;
        coordinate += increment;
        *pixel++ = BYTE(0x94e + value);
        count--;
    }
    registers[2] = 0;
}

void blit_span(void);
__asm__(".text\n_blit_span:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "movl %esp,%eax\n\tpushl %eax\n\t.rept 9\n\tpushl 52(%esp)\n\t.endr\n\t"
        "call _blit_body@40\n\tpopl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $36\n");
