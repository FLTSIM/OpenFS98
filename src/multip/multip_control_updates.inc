/* CHAIN 1 control helpers, .text+0x2960/+0x29f0/+0x2bb0, and state
 * publication at +0x9860. The receive/control owner remains separate. */
static int32_t __attribute__((used, noinline)) multip_control_changes_impl(void)
{
    uint8_t *global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    uint32_t state = *(uint16_t *)(void *)(global + 0x5dc);
    if (M32(0xfc) != 0) {
        M32(0x948) = state; M32(0x94c) = A32(global, 0x268); M32(0xfc) = 0;
        return 0;
    }
    if (M32(0x948) != state) {
        M32(0x948) = state;
        multip_player_reset_collision((void *)(uintptr_t)M32(0xa3c));
    }
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    state = A32(global, 0x268);
    if (M32(0x94c) != state) {
        M32(0x94c) = state;
        multip_player_reset_collision((void *)(uintptr_t)M32(0xa3c));
    }
    return 0;
}
#if defined(__i386__)
/* The retail helper leaves entry ESP-0x12 untouched. The next publisher's
 * 1019 packet transmits that word, so retain it across the C helper frame. */
__attribute__((naked)) int32_t multip_control_changes(void)
{
    __asm__ volatile("movzwl -0x12(%esp), %edx\n\tpushl %edx\n\tcall _multip_control_changes_impl\n\t"
        "popl %edx\n\tmovw %dx, -0x12(%esp)\n\tret");
}
#else
int32_t multip_control_changes(void) { return multip_control_changes_impl(); }
#endif
int32_t multip_control_collisions(void)
{
    uint8_t *local = (uint8_t *)(uintptr_t)M32(0xa3c), *global, *players;
    uint32_t count, timer;
    int32_t distance;
    int16_t bearing;
    uint8_t octant;
    if (A32(local, 8) != 1) return 0;
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    if (*(uint16_t *)(void *)(global + 0x84a) != 0) return 0;
    if (global[0x838] != 0 || multip_motion_is_blocked())
        multip_player_reset_collision(local);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    count = M32(M_PLAYER_COUNT);
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    local = (uint8_t *)(uintptr_t)M32(0xa3c);
    timer = A32(local, 0xcc) - (int32_t)*(int16_t *)(void *)(global + 0x2aa);
    A32(local, 0xcc) = (int32_t)timer < 0 ? 0 : timer;
    while (count != 0) {
        uint8_t *player = players + --count * 256;
        global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
        timer = A32(player, 0xcc) - (int32_t)*(int16_t *)(void *)(global + 0x2aa);
        A32(player, 0xcc) = (int32_t)timer < 0 ? 0 : timer;
    }
    count = M32(M_PLAYER_COUNT);
    while (count != 0) {
        uint8_t *player = players + --count * 256;
        void *model;
        int32_t separation;
        if (A32(player, 0xc) != 0 || A32(player, 8) != 1) continue;
        model = (void *)(uintptr_t)A32(player, 0x28);
        if (model == NULL) continue;
        global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
        UTIL_FN(0x64, multip_range48_fn)(global + 0x560, (const uint8_t *)model + 4,
            1, &distance, &bearing, &octant);
        global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
        separation = (int32_t)((uint32_t)distance -
            A32((void *)(uintptr_t)A32(global, 0x11e0), 0xc) - A32(player, 0x30));
        if (separation > 0) continue;
        local = (uint8_t *)(uintptr_t)M32(0xa3c);
        if (A32(local, 0xcc) == 0 && A32(player, 0xcc) == 0) {
            if (global[0x838] != 0) continue;
            multip_handle_1011(0, NULL, 0);
            if (multip_message_1011(A32(player, 4)) != 0) return -1;
        } else if (multip_player_reset_collision(local) != 0) return -1;
    }
    return 0;
}
static int32_t __attribute__((used, noinline)) multip_state_publish_impl(const uint8_t *stack)
{
    void *local = (void *)(uintptr_t)M32(0xa3c);
    uint8_t *global;
    uint16_t status;
    int32_t result;
    if (local == NULL || A32(local, 8) != 1) return 0;
    __asm__ volatile("fldl %1\n\tfcompl %2\n\tfnstsw %0" : "=a"(status)
        : "m"(MD(0xa50)), "m"(*(double *)(void *)(s_multip_rdata + 0x1b8)) : "st");
    if (status & 0x100) return 0;
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    if (global[0x838] != 0 || multip_motion_is_blocked()) {
        MD(0xa50) -= *(double *)(void *)(s_multip_rdata + 0x1b8);
        return multip_message_1018_impl(0, *(const uint16_t *)(const void *)(stack + 0x26)) != 0 ? -1 : 0;
    }
    __asm__ volatile("fldl %1\n\tfcompl %2\n\tfnstsw %0" : "=a"(status)
        : "m"(MD(0xa78)), "m"(*(double *)(void *)(s_multip_rdata + 0x1c0)) : "st");
    if (status & 0x4000) return 0;
    MD(0xa50) -= *(double *)(void *)(s_multip_rdata + 0x1b8);
    result = multip_message_1019_impl(0, *(const uint16_t *)(const void *)(stack + 0x3e6));
    if (result != 0) return -1;
    if (M32(0xabc) != 0) {
        const uint8_t *model;
        uint32_t count;
        uint8_t sum = 0;
        global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
        count = A32((void *)(uintptr_t)A32(global, 0x11e0), 0x18) - 0x28;
        model = (const uint8_t *)(uintptr_t)A32(global, 0x11e4) + 0x28;
        while (count != 0) { sum += *model++; count--; }
        if (sum != M8(0xf28)) {
            M8(0xf28) = sum;
            if (multip_message_1000_impl(M32(0xaa0), stack) != 0) return -1;
        }
    }
    return 0;
}
#if defined(__i386__)
__attribute__((naked)) int32_t multip_state_publish(void)
{
    __asm__ volatile("movl %esp, %eax\n\tsubl $0x100c, %esp\n\tmovl %ebx, (%esp)\n\t"
        "movl %esi, 4(%esp)\n\tmovl %edi, 8(%esp)\n\tleal -0x3f4(%eax), %esi\n\t"
        "leal 12(%esp), %edi\n\tmovl $250, %ecx\n\trep movsl\n\tleal 12(%esp), %eax\n\t"
        "pushl %eax\n\tcall _multip_state_publish_impl\n\taddl $4, %esp\n\tmovl (%esp), %ebx\n\t"
        "movl 4(%esp), %esi\n\tmovl 8(%esp), %edi\n\taddl $0x100c, %esp\n\tret");
}
#else
int32_t multip_state_publish(void)
{ uint8_t stack[1000] = {0}; return multip_state_publish_impl(stack); }
#endif

/* .text+0x2bb0 -- preserve the nested publication stack depth. */
#if defined(__i386__)
__attribute__((naked)) int32_t multip_control_publish(void)
{ __asm__ volatile("call _multip_state_publish\n\tnegl %eax\n\tsbbl %eax, %eax\n\tret"); }

/* .text+0x2870 -- invoke the flight model on the caller's original stack.
 * The following state packet includes padding left there by those calls. */
__attribute__((naked)) uint32_t FS98_STDCALL multip_control_callback(
    uint32_t code __attribute__((unused)), uint32_t argument __attribute__((unused)))
{
    __asm__ volatile("pushl %ecx\n\tmovl _s_multip_image+4, %eax\n\tcall *0x38(%eax)\n\t"
        "movl %eax, _s_multip_image+0xad8\n\tmovl _s_multip_image+0xc, %ecx\n\t"
        "pushl $_s_multip_image+0xa78\n\tmovl 0x11d4(%ecx), %edx\n\tcall *0x2c(%edx)\n\t"
        "testl %eax, %eax\n\tjnz 8f\n\tmovl _s_multip_image+0xc, %eax\n\t"
        "movswl 0x2aa(%eax), %ecx\n\tmovl %ecx, (%esp)\n\tfildl (%esp)\n\t"
        "fmull _s_multip_rdata+0x18\n\tfsubl _s_multip_image+0xa78\n\tfstpl _s_multip_image+0xa80\n\t"
        "call _multip_motion_is_blocked\n\ttestl %eax, %eax\n\tjz 1f\n\t"
        "movl _s_multip_image+0xa80, %edx\n\tmovl _s_multip_image+0xa84, %eax\n\t"
        "movl %edx, _s_multip_image+0xa78\n\tmovl %eax, _s_multip_image+0xa7c\n\t"
        "1: movl _s_multip_image+0xc, %ecx\n\tpushl $_s_multip_image+0xa98\n\t"
        "pushl $_s_multip_image+0xa90\n\tpushl $_s_multip_image+0xa88\n\t"
        "movl 0x11d4(%ecx), %edx\n\tcall *0x30(%edx)\n\ttestl %eax, %eax\n\tjnz 8f\n\t"
        "call _multip_control_changes\n\ttestl %eax, %eax\n\tjnz 8f\n\t"
        "call _multip_control_publish\n\ttestl %eax, %eax\n\tjnz 8f\n\t"
        "call _multip_control_collisions\n\ttestl %eax, %eax\n\tjnz 8f\n\t"
        "cmpl $0, _s_multip_image+0x984\n\tjz 8f\n\tmovl _s_multip_image+4, %eax\n\t"
        "call *0x38(%eax)\n\tsubl _s_multip_image+0xadc, %eax\n\t"
        "cmpl _s_multip_image+0x194, %eax\n\tjbe 8f\n\tmovl _s_multip_image+0x1a4, %eax\n\t"
        "decl %eax\n\tjz 2f\n\tdecl %eax\n\tjnz 8f\n\tcall _multip_message_1008\n\tjmp 8f\n\t"
        "2: call _multip_message_1001\n\t8: popl %ecx\n\tret $8");
}
#else
int32_t multip_control_publish(void) { return multip_state_publish() != 0 ? -1 : 0; }
/* The portable callback can use the same arithmetic once its flight-model
 * interface is exposed by the native host. The x86 ABI owner is above. */
#endif
