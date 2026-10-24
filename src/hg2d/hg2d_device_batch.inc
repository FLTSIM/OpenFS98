/* 0x2078cce0: flush, bind a new mode/texture, and select x87 24-bit
 * precision for subsequent vertex construction. No control word is restored. */
void STDCALL __attribute__((used)) hg2d_device_batch(uint32_t mode, uint32_t texture)
{
    hg2d_device_flush();
    WORD(0x5ee8) = 0x1000;
    WORD(0x5ee0) = WORD(0x5ee4) = ADDRESS(hg2d_image + 0x8860);
    WORD(0x8840) = 0;
    WORD(0x8850) = mode; WORD(0x885c) = texture;
    if (texture) WORD(0x8840) = hg2d_device_texture(POINTER(texture), GLOBAL_HALF(0xca0));
#if defined(__i386__)
    uint16_t control;
    __asm__ volatile("fstcw %0" : "=m"(control));
    control &= ~0x300u;
    __asm__ volatile("fldcw %0" : : "m"(control));
#endif
}
