/* 0x2078d0e0 and its integer, horizontal, vertical and frame wrappers.
 * The register array is the entry shim's saved EDX, ECX, EAX. */
static void device_fixed_float(uint32_t *destination, uint32_t value)
{
#if defined(__i386__)
    const float scale = 0x1p-16f;
    __asm__ volatile("fildl %1; fmuls %2; fstps %0" : "=m"(*destination) : "m"(value), "m"(scale));
#else
    *(float *)destination = (float)((double)(int32_t)value / 65536.0);
#endif
}

static void device_fixed_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t *registers)
{
    uint32_t *vertex, colour, specular = 0xff000000, previous, next, count;
    uint32_t distance, threshold;
    if (!(BYTE(0x28a04) & 0x20)) return;
    if (WORD(0x8850) != 2 || (int32_t)WORD(0x5ee8) < 2) hg2d_device_batch(2, 0);
    WORD(0x8848) = WORD(0x5ee4); WORD(0x5ee0) = WORD(0x5ee4);
    draw_colour();
    colour = BYTE(0x90c);
    colour = (uint32_t)BYTE(0x39c2 + colour * 3) << 16 |
             (uint32_t)BYTE(0x39c3 + colour * 3) << 8 | BYTE(0x39c4 + colour * 3);
    vertex = POINTER(WORD(0x8848));
    device_fixed_float(vertex, x1); device_fixed_float(vertex + 1, y1);
    vertex[2] = vertex[3] = 0x3f800000; vertex[4] = colour;
    distance = GLOBAL_WORD(0xd4c); threshold = GLOBAL_HALF(0xe88);
    if (!GLOBAL_HALF(0xdce) && threshold < 0x7d00 && distance) {
        uint32_t amount = 0;
        if ((int32_t)distance >> 8 >= (int32_t)threshold) {
            uint32_t scale = GLOBAL_WORD(0x30c), maximum = GLOBAL_HALF(0xcd4);
            amount = matrix_signed_divide((uint32_t)((int32_t)distance >> 4), threshold);
            if ((int32_t)scale > (int32_t)maximum)
                amount = (uint32_t)((int64_t)(int32_t)amount * maximum / (int32_t)scale);
            if ((int32_t)amount > 254) amount = 254;
            if (GLOBAL_HALF(0xca0) == 0x98 && threshold == 0x1fff) amount = 0;
        }
        WORD(0x884c) |= amount;
        specular = (0u - (amount + 1)) << 24;
    }
    vertex[5] = specular;
    device_fixed_float(vertex + 8, x2); device_fixed_float(vertex + 9, y2);
    WORD(0x8848) += 64;
    vertex[10] = vertex[11] = 0x3f800000; vertex[12] = colour;
    vertex[13] = vertex[5];
    next = WORD(0x8848); previous = WORD(0x5ee4);
    count = (uint32_t)((int32_t)(next - previous) >> 5);
    WORD(0x5ee8) -= count; WORD(0x5ee4) = next;
    registers[2] = count; registers[1] = next; registers[0] = WORD(0x5ee8);
}

void STDCALL __attribute__((used)) device_lines_body(uint32_t surface, uint32_t first, uint32_t second,
    uint32_t third, uint32_t fourth, uint32_t kind, uint32_t *registers)
{
    if (kind == 4) {
        uint32_t *view = POINTER(GLOBAL_WORD(0x1194)), left, top, right, bottom;
        registers[2] = (ADDRESS(GLOBAL) & 0xffffff00u) | BYTE(0x28a04);
        registers[0] = ADDRESS(view);
        if (!(BYTE(0x28a04) & 0x20)) return;
        if (!first && !second && third == view[0x60 / 4] && fourth == view[0x64 / 4]) {
            uint32_t *rectangle = POINTER(view[0x1c / 4]);
            first = rectangle[1] - rectangle[0]; second = rectangle[3] + rectangle[2];
        }
        left = (first << 16) + 0x4000; top = (second << 16) + 0x4000;
        right = ((third + first) << 16) - 0xc000; bottom = ((fourth + second) << 16) - 0xc000;
        device_fixed_line(left, top, right, top, registers);
        device_fixed_line(left, top, left, bottom, registers);
        device_fixed_line(right, bottom, right, top, registers);
        device_fixed_line(right, bottom, left, bottom, registers);
        return;
    }
    if (kind == 1) { fourth = third; third = second; second = fourth; }
    if (kind == 2) { fourth = third; third = first; }
    if (kind) {
        first <<= 16; second <<= 16; third <<= 16; fourth <<= 16;
        /* These wrapper loads happen even when the fixed-line gate fails. */
        registers[2] = first; registers[1] = surface; registers[0] = second;
    }
    device_fixed_line(first, second, third, fourth, registers);
}

#define DEVICE_LINE_ENTRY(name, kind, count, bytes, extra) \
    void name(void); \
    __asm__(".text\n.globl _" #name "\n_" #name ":\n\t" \
        "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\tmovl %esp,%eax\n\tpushl %eax\n\tpushl $" #kind "\n\t" \
        extra ".rept " #count "\n\tpushl 40(%esp)\n\t.endr\n\t" \
        "call _device_lines_body@28\n\tpopl %edx\n\tpopl %ecx\n\tpopl %eax\n\tret $" #bytes "\n")
DEVICE_LINE_ENTRY(hg2d_device_fixed_line, 0, 5, 20, "");
DEVICE_LINE_ENTRY(hg2d_device_horizontal, 1, 4, 16, "pushl $0\n\t");
DEVICE_LINE_ENTRY(hg2d_device_vertical, 2, 4, 16, "pushl $0\n\t");
DEVICE_LINE_ENTRY(hg2d_device_line, 3, 5, 20, "");
DEVICE_LINE_ENTRY(hg2d_device_frame, 4, 5, 20, "");
