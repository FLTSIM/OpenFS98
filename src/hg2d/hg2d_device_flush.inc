/* 0x2078c800: submit the buffered transformed vertices. Every flush selects
 * x87 53-bit precision, including an empty flush; it does not restore it. */
typedef uint32_t (STDCALL *call_six)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

static uint32_t device_primitive(uint32_t kind, uint32_t vertices, uint32_t count)
{
    uint32_t instance = WORD(0x289d0), *object = POINTER(instance), *vtable = POINTER(object[0]);
    return ((call_six)(uintptr_t)vtable[29])(instance, kind, 3, vertices, count, 0xd);
}

void STDCALL __attribute__((used)) hg2d_device_flush(void)
{
    uint32_t mode;
    uint32_t count = (uint32_t)((int32_t)(WORD(0x5ee4) - ADDRESS(hg2d_image + 0x8860)) >> 5);
#if defined(__i386__)
    uint16_t control;
    __asm__ volatile("fstcw %0" : "=m"(control));
    control = (control & ~0x100u) | 0x200u;
    __asm__ volatile("fldcw %0" : : "m"(control));
#endif
    if (!count) return;
    if (WORD(0x5f24)) {
        hg2d_device_draw_state(28, WORD(0x884c) != 0);
        WORD(0x884c) = 0;
    }
    hg2d_device_draw_state(29, WORD(0x8844) != 0);
    WORD(0x8844) = 0;
    mode = WORD(0x8850);
    switch (mode) {
    case 1:
    case 2:
        hg2d_device_draw_state(1, 0);
        hg2d_device_draw_state(27, 0);
        device_primitive(mode, ADDRESS(hg2d_image + 0x8860), count);
        break;
    case 3:
    case 4:
    case 8:
        hg2d_device_draw_state(9, mode == 8 ? 2 : 1);
        hg2d_device_draw_state(1, 0);
        hg2d_device_draw_state(27, mode == 4 ? 1 : 0);
        device_primitive(4, ADDRESS(hg2d_image + 0x8860), count);
        break;
    case 5:
    case 6:
    case 7: {
        uint32_t *texture = POINTER(WORD(0x885c));
        if (mode == 5 && texture && (texture[1] & 0x100000)) {
            hg2d_device_draw_state(17, 1);
            hg2d_device_draw_state(18, 1);
        } else {
            hg2d_device_draw_state(17, WORD(0x289e4));
            hg2d_device_draw_state(18, WORD(0x289e8));
        }
        hg2d_device_draw_state(3, WORD(0x8858) & 0xff000000u ? 1 : 3);
        WORD(0x8858) = 0;
        hg2d_device_draw_state(21, mode == 6 ? 4 : 2);
        if (mode != 7) hg2d_device_draw_state(9, 2);
        hg2d_device_draw_state(1, WORD(0x8840));
        if (mode == 7) hg2d_device_draw_state(9, 2);
        hg2d_device_draw_state(27, mode == 6 ? 1 : 0);
        device_primitive(4, ADDRESS(hg2d_image + 0x8860), count);
        break;
    }
    }
    WORD(0x5ee8) = 0x1000;
    WORD(0x5ee0) = WORD(0x5ee4) = ADDRESS(hg2d_image + 0x8860);
    WORD(0x8840) = WORD(0x885c) = WORD(0x8850) = 0;
}
