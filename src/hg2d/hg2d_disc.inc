/* +0x84, 0x20787ce0: table-shaped disc, word clipping and doubled profiles. */
static void disc_span(uint16_t first, uint16_t last, uint16_t doubled_y, uint32_t *registers)
{
    uint32_t count, phase, y = doubled_y >> 1;
    uint8_t *pixel;
    void *surface = POINTER(WORD(0x4d4a));
    if ((int16_t)first < 0 || (int16_t)last < 0) {
        if ((int16_t)first < 0) first = 0;
        if ((int16_t)last < 0) last = 0;
        if ((int16_t)first > (int16_t)last) {
            uint16_t temporary = first; first = last; last = temporary;
        }
    }
    registers[2] = first; registers[1] = last;
    if (!WORD(0x4780) && y >= SURFACE_WORD(surface, 8)) return;
    count = (uint32_t)last - first + 1;
    pixel = draw_address(surface, first, y);
    if (WORD(0x4780)) {
        if ((int32_t)(count - 1) < 0) { registers[1] = count - 1; return; }
        phase = (y ^ first) & 1;
        pixel += phase; count -= phase;
        do { *pixel = BYTE(0x90c); pixel += 2; count -= 2; } while ((int32_t)count > 0);
        registers[2] = BYTE(0x90c); registers[1] = count;
    } else {
        if (GLOBAL_WORD(0x1298)) {
            registers[2] = GLOBAL_WORD(0x1298); registers[1] = count;
            draw_map_pixels_state(pixel, count, POINTER(WORD(0x1d88)), registers);
            return;
        }
        draw_horizontal_pixels(surface, first, last, y);
        registers[2] = BYTE(0x90c) * (count >= 8 ? 0x1010101u : 1);
        registers[1] = 0;
    }
}

void STDCALL __attribute__((used)) disc_body(void *record, uint32_t radius, uint32_t x,
                                            uint32_t y, uint32_t *registers)
{
    const uint32_t *view = POINTER(SURFACE_WORD(record, 0x1c));
    uint32_t profile, mode, pass;
    uint16_t mapped_pair, rows, remaining, row, first, last;
    WORD(0x4d4a) = ADDRESS(record) + 0x80;
    WORD(0x8d8) = view[1]; WORD(0x8dc) = view[3];
    WORD(0x8e0) = view[0]; WORD(0x8e4) = view[2];
    HALF(0x4d40) = (uint16_t)(WORD(0x8d8) - WORD(0x8e0));
    HALF(0x4d42) = (uint16_t)(WORD(0x8d8) + WORD(0x8e0));
    HALF(0x4d44) = (uint16_t)((WORD(0x8dc) + WORD(0x8e4)) * 2);
    HALF(0x4d46) = (uint16_t)((WORD(0x8dc) - WORD(0x8e4)) * 2);
    mode = GLOBAL_WORD(0x3c0);
    WORD(0x4780) = ((mode >> 8) & 0xff) == 0x68;
    if (WORD(0x4780)) mode += 0x100;
    registers[2] = colour_translate((uint16_t)mode, GLOBAL_HALF(0xcc8), &mapped_pair) |
                   (WORD(0x4780) ? 0xff00 : 0);
    registers[1] = WORD(0x8d8);
    registers[0] = (registers[0] & 0xffff0000u) | (y & 0xffff);
    /* The colour helper sets DL before the caller replaces DX with y. */
    BYTE(0x4d48) = 0;
    radius &= 0xffff;
    if (radius >= 48) {
        radius >>= 1;
        BYTE(0x4d48) = 1; BYTE(0x4d49) = 2;
        if (radius >= 48) radius = 47;
    }
    profile = WORD(0x4784 + radius * 4);
    rows = *(uint8_t *)POINTER(profile++);
    row = (uint16_t)(((uint16_t)y - rows * (BYTE(0x4d48) ? 2 : 1)) * 2);
    HALF(0x4d3c) = (uint16_t)x; HALF(0x4d3e) = (uint16_t)y;
    for (pass = 0; pass < 2; pass++) {
        if (pass) profile--;
        remaining = rows;
        for (;;) {
            uint16_t width = *(uint8_t *)POINTER(profile);
            if (pass) profile--; else profile++;
            if (BYTE(0x4d48)) width = (uint8_t)(width * 2);
            if (!pass) width++;
            first = (uint16_t)(HALF(0x4d3c) - width);
            last = (uint16_t)(HALF(0x4d3c) + width);
            if ((int16_t)first < (int16_t)HALF(0x4d40)) first = HALF(0x4d40);
            if ((int16_t)last > (int16_t)HALF(0x4d42)) last = HALF(0x4d42);
            registers[1] = (registers[1] & 0xffff0000u) | first;
            registers[0] = (registers[0] & 0xffff0000u) | last;
            if ((int16_t)first <= (int16_t)last && (int16_t)row >= (int16_t)HALF(0x4d44)) {
                if ((int16_t)row > (int16_t)HALF(0x4d46)) return;
                disc_span(first, last, row, registers);
            }
            row += 2;
            if (BYTE(0x4d48)) {
                /* These are INC/DEC SI, not ESI, except the reverse reset. */
                profile = (profile & 0xffff0000u) | (uint16_t)(profile + (pass ? 1 : -1));
                if (--BYTE(0x4d49)) continue;
                BYTE(0x4d49) = 2;
                if (pass) profile--;
                else profile = (profile & 0xffff0000u) | (uint16_t)(profile + 1);
            }
            if (!--remaining) break;
        }
    }
}

void disc(void);
__asm__(".text\n_disc:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "movl %esp,%eax\n\tpushl %eax\n\t"
        ".rept 4\n\tpushl 32(%esp)\n\t.endr\n\t"
        "call _disc_body@20\n\t"
        "popl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $16\n");
