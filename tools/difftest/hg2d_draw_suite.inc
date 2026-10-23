/* Primitive pixel comparison. Includes the entire 64 KB canvas and padding,
 * nonlinear row tables, negative pitch, and recoverable retail faults. */
#define CANVAS_SIZE 0x10000u
static uint8_t *canvas;
static uint8_t span_map[256], text_bytes[64];
static uint32_t surface[5], row_storage[160];
static const uint8_t *text_pointer;
static uint32_t draw_fault_code, draw_fault_address, draw_fault_access;
static volatile uint32_t draw_active;
static uint32_t draw_faults, draw_div_repairs, draw_calls[42];
uint32_t draw_recover_sp, draw_entry_function;
uint32_t draw_registers[7];
static uint32_t register_states[STATE_CAPACITY][7];
static uint32_t draw_ecx_pointer;
void draw_resume(void);
uint32_t invoke_draw(uint32_t function, const uint32_t *arguments, uint32_t count, uint32_t entry_eax);
__asm__(".text\n_invoke_draw:\n\tpushl %ebp\n\tmovl %esp,%ebp\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl %esp,_draw_recover_sp\n\tmovl 8(%ebp),%eax\n\t"
        "movl %eax,_draw_entry_function\n\tmovl 12(%ebp),%esi\n\t"
        "movl 16(%ebp),%ecx\n\ttestl %ecx,%ecx\n\tjz 2f\n"
        "1:\n\tpushl -4(%esi,%ecx,4)\n\tloop 1b\n"
        "2:\n\tmovl 20(%ebp),%eax\n\t"
        "movl $0xb0b0b0b0,%ebx\n\tmovl $0xc0c0c0c0,%ecx\n\t"
        "movl $0xd0d0d0d0,%edx\n\tmovl $0xe0e0e0e0,%esi\n\t"
        "movl $0xf0f0f0f0,%edi\n\tmovl $0xbebebebe,%ebp\n\t"
        "call *_draw_entry_function\n"
        ".globl _draw_resume\n_draw_resume:\n\t"
        "movl %eax,_draw_registers\n\tmovl %ebx,_draw_registers+4\n\t"
        "movl %ecx,_draw_registers+8\n\tmovl %edx,_draw_registers+12\n\t"
        "movl %esi,_draw_registers+16\n\tmovl %edi,_draw_registers+20\n\t"
        "movl %ebp,_draw_registers+24\n\tpushfl\n\tpopl _returned_flags\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");

static LONG WINAPI draw_exception(EXCEPTION_POINTERS *exception)
{
    if (!draw_active) return EXCEPTION_CONTINUE_SEARCH;
    if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        exception->ExceptionRecord->ExceptionCode == EXCEPTION_INT_OVERFLOW) {
        CONTEXT *context = exception->ContextRecord;
        const uint8_t *instruction = (const uint8_t *)(uintptr_t)context->Eip;
        if (context->Eip == 0x2073aefa || context->Eip == 0x2073af90 ||
            context->Eip == 0x2073b0fb || context->Eip == 0x2073bbed ||
            context->Eip == 0x2073e269 || context->Eip == 0x2073e2ff ||
            context->Eip == 0x2073e46a || context->Eip == 0x2073ef5c ||
            context->Eip == 0x20733c3a || context->Eip == 0x20733cd0 ||
            context->Eip == 0x20733e3b || context->Eip == 0x20734a66 ||
            context->Eip == 0x2073759b || context->Eip == 0x20737631 ||
            context->Eip == 0x2073779c || context->Eip == 0x207383c6 ||
            context->Eip == 0x20733426 || context->Eip == 0x20733457 ||
            context->Eip == 0x20733488 || context->Eip == 0x207334b9 ||
            context->Eip == 0x2073374c || context->Eip == 0x2073377d ||
            context->Eip == 0x207337ae || context->Eip == 0x207337df ||
            context->Eip == 0x20732aea || context->Eip == 0x20732af6 ||
            context->Eip == 0x20787a98 || context->Eip == 0x20787aa4 || context->Eip == 0x207853b7 || context->Eip == 0x207853c3 || context->Eip == 0x207886ec || context->Eip == 0x207886f8) {
            int negative = ((int32_t)context->Edx < 0) != ((int32_t)context->Ebx < 0);
            CHECK(instruction[0] == 0xf7 && instruction[1] == 0xfb);
            context->Eax = negative ? 0x80000001u : 0x7fffffffu;
            context->Edx = 0x7fffffffu;
            context->Eip += 2;
            draw_div_repairs++;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        {
            static const uint32_t affine_divides[] = {
                0x2073b041, 0x2073b05b, 0x2073b075, 0x2073b626, 0x2073b639,
                0x2073bb33, 0x2073bb4d, 0x2073bb67, 0x2073bf08, 0x2073bf1b,
                0x2073e3b0, 0x2073e3ca, 0x2073e3e4, 0x2073e995, 0x2073e9a8,
                0x2073eea2, 0x2073eebc, 0x2073eed6, 0x2073f277, 0x2073f28a,
                0x20733d81, 0x20733d9b, 0x20733db5, 0x20734597, 0x207345aa,
                0x207349ac, 0x207349c6, 0x207349e0, 0x20734ea2, 0x20734eb5,
                0x207376e2, 0x207376fc, 0x20737716, 0x20737ef7, 0x20737f0a,
                0x2073830c, 0x20738326, 0x20738340, 0x20738801, 0x20738814,
                0x20785970, 0x20785985, 0x20785b5a, 0x20785b6f,
                0x20785dcf, 0x20785de4, 0x20786005, 0x2078601a,
                0x207862b5, 0x207862ca, 0x20786559, 0x2078656e,
                0x2078690f, 0x20786924, 0x20786fa3, 0x20786fb8, 0x20785679, 0x20788d62, 0x20788d80, 0x20788d95
            };
            uint32_t index;
            for (index = 0; index < sizeof affine_divides / sizeof affine_divides[0]; index++)
                if (context->Eip == affine_divides[index]) {
                    int negative = ((int32_t)context->Edx < 0) != ((int32_t)context->Ebp < 0);
                    CHECK(instruction[0] == 0xf7 && instruction[1] == 0xfd);
                    context->Eax = negative ? 0x80000001u : 0x7fffffffu;
                    context->Edx = 0x7fffffffu;
                    context->Eip += 2;
                    draw_div_repairs++;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
        }
        if (context->Eip == 0x2078b052 || context->Eip == 0x2078b079) {
            int negative = ((int32_t)context->Edx < 0) != ((int32_t)context->Esi < 0);
            CHECK(instruction[0] == 0xf7 && instruction[1] == 0xfe);
            context->Eax = negative ? 0x80000001u : 0x7fffffffu;
            context->Edx = 0x7fffffffu;
            context->Eip += 2;
            draw_div_repairs++;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (context->Eip == 0x20733161 || context->Eip == 0x20733171 ||
            context->Eip == 0x2073317f || context->Eip == 0x207332a2) {
            CHECK(instruction[0] == 0xf7 && instruction[1] == 0xf3);
            context->Eax = context->Edx = 0xffffffffu;
            context->Eip += 2;
            draw_div_repairs++;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (context->Eip == 0x207880da || context->Eip == 0x207880f3) {
            CHECK(instruction[0] == 0x66 && instruction[1] == 0xf7 && instruction[2] == 0xf1);
            context->Eax |= 0xffff;
            context->Edx |= 0xffff;
            context->Eip += 3;
            draw_div_repairs++;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (context->Eip == 0x20788057 || context->Eip == 0x2078806d) {
            CHECK(instruction[0] == 0xf7 && instruction[1] == 0xf1);
            context->Eax = context->Edx = 0xffffffffu;
            context->Eip += 2;
            draw_div_repairs++;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    draw_fault_code = exception->ExceptionRecord->ExceptionCode;
    draw_fault_access = 0;
    draw_fault_address = 0;
    if (draw_fault_code == EXCEPTION_ACCESS_VIOLATION) {
        draw_fault_access = (uint32_t)exception->ExceptionRecord->ExceptionInformation[0];
        draw_fault_address = (uint32_t)exception->ExceptionRecord->ExceptionInformation[1];
    }
    exception->ContextRecord->Esp = draw_recover_sp;
    exception->ContextRecord->Eip = ADDRESS(draw_resume);
    exception->ContextRecord->Eax = 0;
    draw_active = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static uint32_t drawing_arguments(uint32_t slot, const uint32_t *arguments, uint32_t count)
{
    uint32_t result, reg;
    draw_fault_code = draw_fault_address = draw_fault_access = 0;
    touched = canvas;
    check_carry = 0;
    draw_active = 1;
    result = invoke_draw(WORD(images[side], 0x38 + slot), arguments, count, 0xaabbccdd);
    draw_active = 0;
    event(draw_fault_code); event(draw_fault_access); event(draw_fault_address);
    if (!draw_fault_code) {
        if (side == 0)
            memcpy(register_states[step], draw_registers, sizeof draw_registers);
        else for (reg = 0; reg < 7; reg++) {
            /* These three entries expose unspecified caller-saved leftovers. */
            if ((slot == 0x18 || slot == 0x24 || slot == 0x28) && (reg == 2 || reg == 3)) continue;
            uint32_t expected = register_states[step][reg], actual = draw_registers[reg];
            if (draw_ecx_pointer && reg == 2) {
                expected -= ADDRESS(images[0]); actual -= ADDRESS(images[1]);
            }
            if (expected != actual) {
                printf("drawing scenario %u step %u slot %x differs: register %u %08x/%08x\n",
                       scenario, step, slot, reg, expected, actual);
                exit(1);
            }
        }
    }
    compare(result);
    if (slot >= 0x18) draw_calls[(slot - 0x18) / 4]++;
    if (draw_fault_code) draw_faults++;
    return result;
}

static uint32_t drawing(uint32_t slot, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f)
{
    uint32_t arguments[6] = {a, b, c, d, e, f}, count;
    switch (slot) {
    case 0x08: case 0x18: count = 0; break;
    case 0x40: case 0x4c: case 0x54: case 0x78: count = 1; break;
    case 0x1c: case 0x20: count = 3; break;
    case 0x24: case 0x2c: case 0x30: case 0x44: count = 4; break;
    case 0x28: case 0x48: count = 6; break;
    default: count = 5; break;
    }
    return drawing_arguments(slot, arguments, count);
}

static uint32_t draw_surface(void) { return ADDRESS(surface); }
#define DRAW(slot, x, y, z, w) drawing(slot, draw_surface(), (uint32_t)(x), (uint32_t)(y), (uint32_t)(z), (uint32_t)(w), 0)

static void draw_reset(void)
{
    uint32_t index, origin;
    int32_t pitch;
    palette_reset();
    layout((uint16_t)(scenario % 12));
    for (index = 0; index < CANVAS_SIZE; index++)
        canvas[index] = (uint8_t)(index * 13 + scenario * 7);
    for (index = 0; index < sizeof span_map; index++)
        span_map[index] = (uint8_t)(index * 37 + scenario + 1);
    /* Seed every wrapped glyph index, including control codes. This overlaps
     * the first palette words, just as retail's out-of-range glyphs do. */
    for (index = 0; index < 256 * 11; index++)
        images[side][0x29d2 + index] = (uint8_t)random_word();
    for (index = 0; index < 0x1000; index++)
        images[side][0xd70 + index] = (uint8_t)(index * 3 + (index >> 8) * 19 + scenario);
    pitch = (int32_t)(128 + (scenario % 5) * 8);
    if (scenario & 1) pitch = -pitch;
    origin = pitch > 0 ? 4096 : 50000;
    surface[0] = 0x12345678;
    surface[1] = 64;
    surface[2] = 48;
    surface[3] = (uint32_t)pitch;
    surface[4] = ADDRESS(row_storage + 16);
    for (index = 0; index < 160; index++) {
        int32_t row = (int32_t)index - 16;
        row_storage[index] = ADDRESS(canvas) + origin + row * pitch;
        if (scenario % 3 == 0 && row >= 0)
            row_storage[index] += (row & 1) ? 7 : 0;
    }
    global[0xcc6] = (uint8_t)random_word();
    GLOBAL_HALF(0xcc2) = (uint16_t)random_word();
    GLOBAL_HALF(0xe88) = 100;
    GLOBAL_HALF(0x30c) = 200;
    GLOBAL_HALF(0xcd4) = 150;
    WORD(global, 0x390) = 2;
    WORD(global, 0xd4c) = (scenario % 25) * 100 * 256;
    WORD(images[side], 0x1d88) = ADDRESS(span_map);
    WORD(global, 0x2ec) = 0x6900 | (scenario & 0xff);
    GLOBAL_HALF(0xdce) = 0;
    GLOBAL_HALF(0x127e) = 0;
    WORD(global, 0x1298) = 0;
}

static void run_drawing(void)
{
    static const uint32_t lengths[] = {1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33};
    static const int32_t offsets[9][2] = {{0,0}, {8,3}, {3,8}, {-8,3}, {-3,8}, {8,-3}, {3,-8}, {-8,-3}, {-3,-8}};
    static const uint16_t depths[] = {0, 99, 100, 101, 1499, 1500, 0x7fff, 0x8000, 0xffff};
    uint32_t index, alignment, sx, sy, ex, ey, depth, choice;
    draw_reset();
    CHECK(DRAW(0x1c, 2, 2, 0, 0) == 0xaabbccdd);
    CHECK(DRAW(0x20, 0xffffffff, 0xffffffff, 0, 0) == 0xaabbccdd);
    DRAW(0x1c, surface[1] + 1, surface[2], 0, 0);
    DRAW(0x1c, 0xffffffff, 0xffffffff, 0, 0);
    DRAW(0x2c, -1, 1, 2, 0);
    DRAW(0x2c, 1, -1, 3, 0);
    DRAW(0x2c, -4, -2, 4, 0);
    DRAW(0x2c, 0x8001, 1, 2, 0); /* Positive dword, negative low word. */
    DRAW(0x2c, 20, 1, surface[2], 0); /* Row rejection before a reversed span. */
    DRAW(0x2c, 1, 9, 0xffffffff, 0);
    DRAW(0x2c, surface[1] + 1, surface[1] + 8, 2, 0);
    for (alignment = 0; alignment < 4; alignment++)
        for (index = 0; index < sizeof lengths / sizeof lengths[0]; index++)
            DRAW(0x2c, alignment, alignment + lengths[index] - 1, 5 + alignment, 0);
    DRAW(0x30, 3, 3, 3, 0);
    DRAW(0x30, 4, 2, 10, 0);
    DRAW(0x30, 5, 10, 2, 0);
    DRAW(0x30, surface[1] + 1, 0, surface[2], 0);
    for (index = 0; index < 9; index++) {
        sx = sy = 16;
        ex = sx + offsets[index][0]; ey = sy + offsets[index][1];
        DRAW(0x34, sx, sy, ex, ey);
        DRAW(0x34, ex, ey, sx, sy);
        sx = (sx << 16) | (random_word() & 0xffff);
        sy = (sy << 16) | (random_word() & 0xffff);
        ex = (ex << 16) | (random_word() & 0xffff);
        ey = (ey << 16) | (random_word() & 0xffff);
        DRAW(0x38, sx, sy, ex, ey);
        DRAW(0x38, ex, ey, sx, sy);
    }
    DRAW(0x34, 8, 8, 16, 8);
    DRAW(0x34, 8, 8, 8, 16);
    DRAW(0x34, 8, 8, 16, 16);
    DRAW(0x38, 8u << 16, 8u << 16, 8u << 16, 8u << 16);
    DRAW(0x38, 8u << 16, 8u << 16, (8u << 16) + 1, (8u << 16) + 1);
    DRAW(0x38, 16u << 16, 16u << 16, (16u << 16) - 2, 18u << 16);
    DRAW(0x38, (16u << 16) + 2, 16u << 16, (17u << 16) + 1, 17u << 16);
    DRAW(0x3c, 3, 3, 0, 8);
    DRAW(0x3c, 3, 3, 8, 0);
    DRAW(0x3c, 3, 3, 1, 1);
    DRAW(0x3c, 3, 3, 2, 2);
    DRAW(0x3c, 3, 3, 30, 14);
    for (index = 0; index < sizeof depths / sizeof depths[0]; index++) {
        GLOBAL_HALF(0x127e) = (uint16_t)(index % 3 == 0 ? 0x100 : index % 3);
        depth = depths[index];
        DRAW(0x24, index & 1 ? surface[1] - 1 : 2, index & 2 ? 0 : 2, depth, 0);
    }
    GLOBAL_HALF(0x127e) = 1;
    DRAW(0x24, 2, 1, 100, 0);
    for (index = 0; index < 32; index++) {
        choice = random_word() % 5;
        sx = 3 + random_word() % 24; sy = 3 + random_word() % 24;
        ex = 3 + random_word() % 24; ey = 3 + random_word() % 24;
        WORD(global, 0x2ec) = random_word();
        if (choice == 0) DRAW(0x1c, sx, sy, 0, 0);
        if (choice == 1) DRAW(0x30, sx, sy, ey, 0);
        if (choice == 2) DRAW(0x34, sx, sy, ex, ey);
        if (choice == 3) DRAW(0x38, (sx << 16) | 0xffff, (sy << 16) | 1, ex << 16, ey << 16);
        if (choice == 4) {
            GLOBAL_HALF(0xdce) = (uint16_t)(index % 2);
            DRAW(0x24, sx, sy, random_word(), 0);
        }
    }
    /* The host resumes after unsigned divides with a saturated quotient.
     * Signed comparisons can select the word division with negative scales. */
    {
        static const uint16_t thresholds[] = {0, 1, 0x7cff, 0x7d00, 0x8000, 0xffff};
        static const uint16_t values[] = {0, 1, 0x7fff, 0x8000, 0xffff};
        uint32_t threshold_index, value_index;
        GLOBAL_HALF(0xdce) = 0;
        WORD(global, 0xd4c) = 0x007d0000;
        GLOBAL_HALF(0x30c) = (uint16_t)(scenario % 3 == 0 ? 200 : scenario % 3 == 1 ? 0 : 0x7fff);
        GLOBAL_HALF(0xcd4) = (uint16_t)(scenario % 3 == 0 ? 150 : scenario % 3 == 1 ? 0xffff : 0x8000);
        for (threshold_index = 0; threshold_index < 6; threshold_index++) {
            GLOBAL_HALF(0xe88) = thresholds[threshold_index];
            for (value_index = 0; value_index < 5; value_index++)
                DRAW(0x24, 3, 3, values[value_index], 0);
            DRAW(0x38, 8u << 16, 8u << 16, 10u << 16, 12u << 16);
        }
    }
    /* Short text, empty strings, substitutions, centering and strict gates. */
    text_pointer = text_bytes;
    text_bytes[0] = 0;
    drawing(0x28, draw_surface(), 10, 10, 0x12345678, ADDRESS(&text_pointer), 0);
    text_bytes[0] = 'A'; text_bytes[1] = 0xe1; text_bytes[2] = 0xfd; text_bytes[3] = 1; text_bytes[4] = 0;
    drawing(0x28, draw_surface(), 10, 10, 0x12345678, ADDRESS(&text_pointer), 0);
    drawing(0x28, draw_surface(), 32, 22, 0, ADDRESS(&text_pointer), 0xffff0001);
    text_bytes[1] = 0;
    drawing(0x28, draw_surface(), surface[1] - 8, 10, 0, ADDRESS(&text_pointer), 0);
    drawing(0x28, draw_surface(), surface[1] - 9, surface[2] - 12, 0, ADDRESS(&text_pointer), 0);
    drawing(0x28, draw_surface(), surface[1] - 9, surface[2] - 13, 0, ADDRESS(&text_pointer), 0);
    drawing(0x28, draw_surface(), 0xffffffff, 0xffffffff, 0, ADDRESS(&text_pointer), 0);
    /* Palette flush has both a preserved-eax no-op and a software realization. */
    HALF(images[side], 0x36c0) = 0;
    CHECK(drawing(0x18, 0, 0, 0, 0, 0, 0) == 0xaabbccdd);
    HALF(images[side], 0x36c0) = 1;
    CHECK(drawing(0x18, 0, 0, 0, 0, 0, 0) == 0);
    /* Both short loops execute before decrementing a zero-length span. A
     * reversed nonnegative pair writes/maps until the canvas guard faults. */
    DRAW(0x2c, 5, 4, 1, 0);
    CHECK(draw_fault_code == EXCEPTION_ACCESS_VIOLATION && draw_fault_access == 1 &&
          draw_fault_address == ADDRESS(canvas) + CANVAS_SIZE);
    WORD(global, 0x1298) = 1;
    DRAW(0x2c, 5, 4, 1, 0);
    CHECK(draw_fault_code == EXCEPTION_ACCESS_VIOLATION && draw_fault_access == 0 &&
          draw_fault_address == ADDRESS(canvas) + CANVAS_SIZE);
    /* Retail remaps pixels successfully, then faults through its lost EBP. */
    WORD(global, 0x1298) = 1;
    for (alignment = 0; alignment < 4; alignment++) {
        DRAW(0x2c, alignment, alignment + 32, 1, 0);
        CHECK(draw_fault_code == EXCEPTION_ACCESS_VIOLATION && draw_fault_address == 0 && draw_fault_access == 0);
    }
    alignment = (4 - row_storage[17]) & 3;
    WORD(images[side], 0x1d88) = row_storage[17] + alignment;
    for (index = 0; index < 256; index++)
        *(uint8_t *)(uintptr_t)(WORD(images[side], 0x1d88) + index) = (uint8_t)index;
    *(uint8_t *)(uintptr_t)WORD(images[side], 0x1d88) = 1;
    *(uint8_t *)(uintptr_t)(WORD(images[side], 0x1d88) + 1) = 0;
    DRAW(0x2c, alignment, alignment + 32, 1, 0); /* Aliased map and pixels. */
    CHECK(draw_fault_code == EXCEPTION_ACCESS_VIOLATION);
    CHECK(*(uint8_t *)(uintptr_t)WORD(images[side], 0x1d88) == 0);
    CHECK(*(uint8_t *)(uintptr_t)(WORD(images[side], 0x1d88) + 1) == 1);
}
