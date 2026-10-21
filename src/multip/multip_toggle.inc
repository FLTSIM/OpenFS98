/* Public +0x18, .text+0x8e60. Keep the caller's completion stack depth. */
#if defined(__i386__)
__attribute__((naked)) static int32_t multip_session_toggle(void)
{
    __asm__ volatile("call _multip_directplay_load\n\ttestl %eax, %eax\n\tjz 1f\n\torl $-1, %eax\n\tret\n\t"
        "1: cmpl $0, _s_multip_image+0xaac\n\tje 2f\n\tmovl _s_multip_image+0x44, %eax\n\t"
        "pushl $0\n\tpushl $0\n\tpushl $0\n\tpushl $0\n\tpushl $0\n\tpushl $0xf53c\n\tpushl $1\n\tpushl $0xc0\n\t"
        "call *0xa4(%eax)\n\ttestb $0x40, %al\n\tje 5f\n\tjmp _multip_session_disconnect\n\t"
        "2: call _multip_configuration_dialog\n\ttestl %eax, %eax\n\tjnz 3f\n\tret\n\t"
        "3: movl _s_multip_image+0x1a4, %eax\n\ttestl %eax, %eax\n\tjnz 4f\n\t"
        "call _multip_directplay_unload\n\tnegl %eax\n\tsbbl %eax, %eax\n\tret\n\t"
        "4: cmpl $3, %eax\n\tje 5f\n\tcmpl $0, _s_multip_image+0xaa8\n\tje 6f\n\t"
        "call _multip_session_host\n\ttestl %eax, %eax\n\tjz 7f\n\txorl %eax, %eax\n\tret\n\t"
        "6: call _multip_session_join\n\ttestl %eax, %eax\n\tjz 7f\n\txorl %eax, %eax\n\tret\n\t"
        "7: call _multip_session_complete\n\ttestl %eax, %eax\n\tjz 5f\n\txorl %eax, %eax\n\tret\n\t"
        "5: movl $1, %eax\n\tret");
}
#else
static int32_t multip_session_toggle(void)
{
    if (multip_directplay_load() != 0) return -1;
    if (M32(M_CONNECTED) != 0) {
        if (UTIL_FN(0xa4, multip_confirm_fn)(0xc0, 1, 0xf53c, 0, 0, 0, 0, 0) & 0x40)
            return multip_session_disconnect();
        return 1;
    }
    if (multip_configuration_dialog() == 0) return 0;
    if (M32(0x1a4) == 0) return multip_directplay_unload() != 0 ? -1 : 0;
    if (M32(0x1a4) == 3) return 1;
    if (M32(0xaa8) != 0) { if (multip_session_host() != 0) return 0; }
    else if (multip_session_join() != 0) return 0;
    return multip_session_complete() != 0 ? 0 : 1;
}
#endif
