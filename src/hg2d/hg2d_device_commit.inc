/* 0x2078da30: close a device vertex writer, publishing its current cursor.
 * EAX is the cursor, ECX the signed vertex delta, EDX remaining capacity. */
uint32_t hg2d_device_commit_count;
uint32_t STDCALL __attribute__((used)) device_commit_body(void)
{
    uint32_t next = WORD(0x8848), previous = WORD(0x5ee4);
    WORD(0x5ee4) = next;
    hg2d_device_commit_count = (uint32_t)((int32_t)(next - previous) >> 5);
    WORD(0x5ee8) -= hg2d_device_commit_count;
    return next;
}
void hg2d_device_commit(void), hg2d_device_nop(void);
__asm__(".text\n.globl _hg2d_device_commit\n_hg2d_device_commit:\n\t"
        "call _device_commit_body@0\n\tmovl _hg2d_device_commit_count,%ecx\n\t"
        "movl _hg2d_image+0x5ee8,%edx\n\tret $4\n"
        ".globl _hg2d_device_nop\n_hg2d_device_nop:\n\tret $12\n");
