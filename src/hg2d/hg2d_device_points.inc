/* Device +0x1c/+0x24, 0x2078ced0 / 0x2078cf90. Preserve the compiled
 * entries' observable EAX/ECX/EDX results with a small entry shim. */
uint32_t hg2d_device_point_active, hg2d_device_point_ebx_active, hg2d_device_point_ebx;
void STDCALL __attribute__((used)) device_point_body(uint32_t surface, uint32_t x, uint32_t y, uint32_t depth, uint32_t shaded)
{
    uint32_t *vertex, colour, previous, depth_active = 0, lookup = 0;
    (void)surface;
    hg2d_device_point_active = hg2d_device_point_ebx_active = 0;
    if (!(BYTE(0x28a04) & 0x20)) return;
    if (WORD(0x8850) != 1 || (int32_t)WORD(0x5ee8) < 1) hg2d_device_batch(1, 0);
    WORD(0x8848) = WORD(0x5ee4); WORD(0x5ee0) = WORD(0x5ee4);
    draw_colour();
    if (shaded) {
        if (!GLOBAL_HALF(0xdce) && GLOBAL_WORD(0x390) >= 2 && (int16_t)depth >= (int16_t)GLOBAL_HALF(0xe88)) {
            uint16_t value = draw_divide_word((uint16_t)depth, GLOBAL_HALF(0xe88));
            if ((int16_t)GLOBAL_HALF(0x30c) > (int16_t)GLOBAL_HALF(0xcd4))
                value = draw_divide_word((uint32_t)value * GLOBAL_HALF(0xcd4), GLOBAL_HALF(0x30c));
            if (value >= 15) value = 15;
            lookup = ((uint32_t)value << 8) | BYTE(0x90c); depth_active = 1;
        }
        BYTE(0x90c) = draw_depth(BYTE(0x90c), (uint16_t)depth);
    }
    vertex = POINTER(WORD(0x8848));
#if defined(__i386__)
    __asm__ volatile("fildl %1; fstps %0" : "=m"(vertex[0]) : "m"(x));
    __asm__ volatile("fildl %1; fstps %0" : "=m"(vertex[1]) : "m"(y));
#else
    ((float *)vertex)[0] = (float)(int32_t)x; ((float *)vertex)[1] = (float)(int32_t)y;
#endif
    WORD(0x8848) += 32;
    vertex[2] = vertex[3] = 0x3f800000;
    colour = BYTE(0x90c);
    vertex[4] = (uint32_t)BYTE(0x39c2 + colour * 3) << 16 |
                (uint32_t)BYTE(0x39c3 + colour * 3) << 8 | BYTE(0x39c4 + colour * 3);
    vertex[5] = 0xff000000;
    previous = WORD(0x5ee4); WORD(0x5ee4) = WORD(0x8848);
    WORD(0x5ee8) -= (uint32_t)((int32_t)(WORD(0x8848) - previous) >> 5);
    hg2d_device_point_active = 1;
    hg2d_device_point_ebx_active = depth_active; hg2d_device_point_ebx = lookup;
}
#define DEVICE_POINT_RESULT \
    "popl %edx\n\tpopl %ecx\n\tcmpl $0,_hg2d_device_point_active\n\tje 1f\n\t" \
    "movl _hg2d_image+0x5ee4,%eax\n\tmovl %eax,(%esp)\n\t" \
    "movl $1,%ecx\n\tmovl _hg2d_image+0x5ee8,%edx\n\t" \
    "cmpl $0,_hg2d_device_point_ebx_active\n\tje 1f\n\tmovl _hg2d_device_point_ebx,%ebx\n1:\n\tpopl %eax\n\t"
void hg2d_device_point(void), hg2d_device_depth_point(void);
__asm__(".text\n.globl _hg2d_device_point\n.globl _hg2d_device_depth_point\n_hg2d_device_point:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "pushl $0\n\tpushl $0\n\t.rept 3\n\tpushl 32(%esp)\n\t.endr\n\t"
        "call _device_point_body@20\n\t" DEVICE_POINT_RESULT "ret $12\n"
        "_hg2d_device_depth_point:\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n\t"
        "pushl $1\n\t.rept 4\n\tpushl 32(%esp)\n\t.endr\n\t"
        "call _device_point_body@20\n\t" DEVICE_POINT_RESULT "ret $16\n");
