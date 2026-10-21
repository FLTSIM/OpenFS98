/* Position/state packet constructors, .text+0x59d0/+0x6870/+0x6950. */
int32_t multip_motion_is_blocked(void)
{
    const uint8_t *global = (const uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    return *(const uint16_t *)(const void *)(global + 0x5dc) != 0 ||
        A32(global, 0x268) != 0 || M32(0xad4) != 0;
}
static uint32_t multip_state_angles(const void *global)
{
    return (A32(global, 0x578) & 0xffc00000u) |
        ((A32(global, 0x57c) >> 10) & 0x003ff000u) |
        ((A32(global, 0x580) >> 20) & 0x00000ffcu);
}
static void multip_state_coordinates(uint8_t *out, const void *global)
{
    A32(out, 0) = A32(global, 0x564); A32(out, 4) = A32(global, 0x56c);
    A32(out, 8) = A32(global, 0x574);
    *(uint16_t *)(void *)(out + 12) = *(const uint16_t *)((const uint8_t *)global + 0x562);
    *(uint16_t *)(void *)(out + 14) = *(const uint16_t *)((const uint8_t *)global + 0x56a);
    *(uint16_t *)(void *)(out + 16) = *(const uint16_t *)((const uint8_t *)global + 0x572);
}
static int32_t __attribute__((used, noinline)) multip_message_1000_impl(uint32_t id, const uint8_t *stack)
{
    uint8_t packet[1000];
    const void *global = (const void *)(uintptr_t)M32(IMPORT_GLOBAL);
    void *aircraft = (void *)(uintptr_t)A32(global, 0x11e0);
    uint32_t size = A32(aircraft, 0x18), index;
    /* The retail copy omits 0x6c bytes without reducing the sent length.
     * Preserve all uninitialized tail bytes from the original caller stack. */
    for (index = 0; index < sizeof(packet); index++) packet[index] = stack[index];
    A32(packet, 0) = 0x1000; A32(packet, 4) = size + 12;
    A32(packet, 8) = M32(0xad8);
    A32(packet, 0x10) = A32((void *)(uintptr_t)A32(global, 0x11e0), 0x18);
    if (HOST_FN(0x21c, multip_copy_fn)(packet + 0x14,
        (const uint8_t *)(uintptr_t)A32(global, 0x11e4) + 0x6c,
        A32((void *)(uintptr_t)A32(global, 0x11e0), 0x18) - 0x6c) != 0) return -1;
    A32(packet, 0xc) = M32(0xe08); M32(0xe08)++;
    return multip_send(id, packet, size + 20, 0) != 0 ? -1 : 0;
}
static int32_t __attribute__((used, noinline)) multip_message_1018_impl(uint32_t id, uint32_t padding)
{
    uint8_t packet[40];
    const void *global = (const void *)(uintptr_t)M32(IMPORT_GLOBAL);
    A32(packet, 0) = 0x1018; A32(packet, 4) = 32; A32(packet, 8) = M32(0xad8);
    A32(packet, 0x10) = multip_state_angles(global);
    multip_state_coordinates(packet + 0x14, global);
    *(uint16_t *)(void *)(packet + 0x26) = (uint16_t)padding;
    A32(packet, 0xc) = M32(0xe08); M32(0xe08)++;
    return multip_send(id, packet, sizeof(packet), 0) != 0 ? -1 : 0;
}
static uint32_t multip_state_velocity(uint32_t offset)
{
    uint16_t original_control, truncate_control;
    int64_t value;
    __asm__ volatile("fnstcw %0" : "=m"(original_control));
    truncate_control = original_control | 0xc00;
    __asm__ volatile("fldl %1\n\tfmull %2\n\tfldcw %3\n\tfistpll %0\n\tfldcw %4"
        : "=m"(value) : "m"(MD(offset)), "m"(*(double *)(void *)(s_multip_rdata + 0x170)),
          "m"(truncate_control), "m"(original_control) : "st");
    return (uint32_t)value;
}
static int32_t __attribute__((used, noinline)) multip_message_1019_impl(uint32_t id, uint32_t padding)
{
    uint8_t packet[56];
    const void *global;
    A32(packet, 0) = 0x1019; A32(packet, 4) = 48;
    if (multip_motion_is_blocked()) {
        A32(packet, 0x10) = 0; A32(packet, 0x14) = 0; A32(packet, 0x18) = 0;
    } else {
        A32(packet, 0x10) = multip_state_velocity(0xa88);
        A32(packet, 0x14) = multip_state_velocity(0xa90);
        A32(packet, 0x18) = multip_state_velocity(0xa98);
    }
    global = (const void *)(uintptr_t)M32(IMPORT_GLOBAL);
    A32(packet, 8) = M32(0xe08); M32(0xe08)++;
    A32(packet, 0xc) = M32(0xad8); A32(packet, 0x1c) = A32(global, 0x2b4);
    A32(packet, 0x20) = multip_state_angles(global);
    multip_state_coordinates(packet + 0x24, global);
    *(uint16_t *)(void *)(packet + 0x36) = (uint16_t)padding;
    return multip_send(id, packet, sizeof(packet), 0) != 0 ? -1 : 0;
}
#if defined(__i386__)
__attribute__((naked)) int32_t FS98_STDCALL multip_message_1000(uint32_t id __attribute__((unused)))
{
    __asm__ volatile("movl %esp, %eax\n\tsubl $0x100c, %esp\n\tmovl %ebx, (%esp)\n\t"
        "movl %esi, 4(%esp)\n\tmovl %edi, 8(%esp)\n\tmovl %eax, %ebx\n\t"
        "leal -0x3e8(%eax), %esi\n\tleal 12(%esp), %edi\n\tmovl $250, %ecx\n\trep movsl\n\t"
        "leal 12(%esp), %eax\n\tpushl %eax\n\tpushl 4(%ebx)\n\tcall _multip_message_1000_impl\n\t"
        "addl $8, %esp\n\tmovl (%esp), %ebx\n\tmovl 4(%esp), %esi\n\tmovl 8(%esp), %edi\n\t"
        "addl $0x100c, %esp\n\tret $4");
}
__attribute__((naked)) int32_t FS98_STDCALL multip_message_1018(uint32_t id __attribute__((unused)))
{ __asm__ volatile("movzwl -0x3c2(%esp), %eax\n\tpushl %eax\n\tpushl 8(%esp)\n\tcall _multip_message_1018_impl\n\taddl $8, %esp\n\tret $4"); }
__attribute__((naked)) int32_t FS98_STDCALL multip_message_1019(uint32_t id __attribute__((unused)))
{ __asm__ volatile("movzwl -2(%esp), %eax\n\tpushl %eax\n\tpushl 8(%esp)\n\tcall _multip_message_1019_impl\n\taddl $8, %esp\n\tret $4"); }
#else
int32_t FS98_STDCALL multip_message_1000(uint32_t id)
{ uint8_t stack[1000] = {0}; return multip_message_1000_impl(id, stack); }
int32_t FS98_STDCALL multip_message_1018(uint32_t id) { return multip_message_1018_impl(id, 0); }
int32_t FS98_STDCALL multip_message_1019(uint32_t id) { return multip_message_1019_impl(id, 0); }
#endif
