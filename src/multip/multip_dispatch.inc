/* Application and DirectPlay receive dispatch, .text+0x47f0..+0x4cb0. */
/* .text+0x0e40 -- rejected join closes before clearing connected. */
int32_t FS98_STDCALL multip_handle_1005(uint32_t id, const void *packet, uint32_t size)
{
    int32_t result;
    (void)id; (void)packet; (void)size;
    if (UTIL_FN(0xa4, multip_confirm_fn)(0xc0, 1, 0xf530, 0xf531, 0, 0, 0, 0) & 0x40) {
        if (multip_message_1002() != 0)
            return -1;
        M32(0x984) = 1;
        return 0;
    }
    result = DP_FN(0x10, multip_dp_close_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY));
    M32(0x984) = 0; M32(M_CONNECTED) = 0;
    /* Retail clears connected first, so disconnect is a no-op here. */
    if (multip_session_disconnect() != 0) return -1;
    return result;
}
/* .text+0x0ec0 */
int32_t FS98_STDCALL multip_handle_1006(uint32_t id, const void *packet, uint32_t size)
{
    (void)id; (void)packet; (void)size;
    UTIL_FN(0xa4, multip_confirm_fn)(2, 1, 0xf532, 0xf533, 0, 0, 0, 0);
    return multip_session_disconnect() != 0 ? -1 : 0;
}
/* .text+0x17d0 -- display the alert only after successful disconnect. */
int32_t FS98_STDCALL multip_handle_1016(uint32_t id, const void *packet, uint32_t size)
{
    (void)id; (void)packet; (void)size;
    if (multip_session_disconnect() != 0) return -1;
    UTIL_FN(0xa4, multip_confirm_fn)(2, 1, 0xf538, 0, 0, 0, 0, 0);
    return 0;
}
/* .text+0x81f0 */
int32_t FS98_STDCALL multip_system_lost(const void *message, uint32_t size)
{
    (void)message; (void)size;
    UTIL_FN(0xa4, multip_confirm_fn)(2, 1, 0xf53e, 0, 0, 0, 0, 0);
    return multip_session_disconnect() != 0 ? -1 : 0;
}

int32_t FS98_STDCALL multip_system_dispatch(const void *packet, uint32_t size, uint32_t timestamp)
{
    switch (A32(packet, 0)) {
    case 3: return multip_system_created(packet, size);
    case 5: return multip_system_destroyed(packet, size);
    case 0x31: return multip_system_lost(packet, size);
    case 0x101: return multip_system_host(packet, size);
    case 0x102: return multip_system_player_data(packet, size, timestamp);
    default: return 0;
    }
}

#if defined(__i386__)
/* Retain retail's 12-byte frame and exact handler nesting. The 1013 weather
 * reply and 1019 position consume words left below their entry stack. */
__attribute__((naked)) int32_t FS98_STDCALL multip_app_dispatch(
    uint32_t id __attribute__((unused)), const void *packet __attribute__((unused)),
    uint32_t size __attribute__((unused)), uint32_t timestamp __attribute__((unused)))
{
    __asm__ volatile(
        "movl 8(%esp), %eax\n\tsubl $12, %esp\n\tmovl (%eax), %ecx\n\t"
#include "multip_dispatch_cases.inc"
        "movl 16(%esp), %ecx\n\tmovl %ecx, (%esp)\n\t"
        "movl %eax, 4(%esp)\n\tmovl 24(%esp), %edx\n\tmovl %edx, 8(%esp)\n\t"
        "movl %esp, %eax\n\tpushl %eax\n\tpushl $5\n\tpushl $0x1d\n\t"
        "movl _s_multip_image+0x1c, %ecx\n\tcall *0x20(%ecx)\n\t"
        "xorl %eax, %eax\n\taddl $12, %esp\n\tret $16\n\t"
        );
}
/* .text+0x4890 -- normalize handler failures only at the top dispatcher. */
__attribute__((naked)) int32_t FS98_STDCALL multip_receive_dispatch(
    uint32_t id __attribute__((unused)), const void *packet __attribute__((unused)),
    uint32_t size __attribute__((unused)), uint32_t timestamp __attribute__((unused)))
{
    __asm__ volatile("movl 4(%esp), %eax\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "movl 16(%esp), %eax\n\tmovl 12(%esp), %ecx\n\tmovl 8(%esp), %edx\n\t"
        "pushl %eax\n\tpushl %ecx\n\tpushl %edx\n\tcall _multip_system_dispatch@12\n\tjmp 2f\n\t"
        "1: movl 16(%esp), %ecx\n\tmovl 12(%esp), %edx\n\tpushl %ecx\n\t"
        "movl 12(%esp), %ecx\n\tpushl %edx\n\tpushl %ecx\n\tpushl %eax\n\t"
        "call _multip_app_dispatch@16\n\t2: negl %eax\n\tsbbl %eax, %eax\n\tret $16");
}
#else
int32_t FS98_STDCALL multip_app_dispatch(uint32_t id, const void *packet, uint32_t size, uint32_t timestamp)
{
    uint32_t arguments[3];
    switch (A32(packet, 0)) {
#define MULTIP_DISPATCH_C
#include "multip_dispatch_cases.inc"
#undef MULTIP_DISPATCH_C
    default:
        arguments[0] = id; arguments[1] = ADDR(packet); arguments[2] = size;
        CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 5, arguments);
        return 0;
    }
}
int32_t FS98_STDCALL multip_receive_dispatch(uint32_t id, const void *packet, uint32_t size, uint32_t timestamp)
{
    return (id == 0 ? multip_system_dispatch(packet, size, timestamp) :
        multip_app_dispatch(id, packet, size, timestamp)) != 0 ? -1 : 0;
}
#endif

/* .text+0x89a0 -- timeout checks walk backwards and use unsigned elapsed time. */
int32_t multip_receive_expire(void)
{
    uint32_t count = M32(M_PLAYER_COUNT);
    uint8_t *players = (uint8_t *)(uintptr_t)M32(M_PLAYERS), *player;
    while (count != 0) {
        player = players + --count * 256;
        if (A32(player, 8) == 4 && HOST_FN(0x38, multip_ticks_fn)() - A32(player, 0xfc) > M32(0x17c))
            if (multip_message_1007(A32(player, 4)) != 0) return -1;
    }
    return 0;
}

/* .text+0x47f0 -- failures are returned raw and leave the dequeued record
 * owned by nobody. A NULL payload also leaves its record unfreed. */
#if defined(__i386__)
__attribute__((naked)) static uint32_t FS98_STDCALL multip_receive_callback(
    uint32_t code __attribute__((unused)), uint32_t argument __attribute__((unused)))
{
    __asm__ volatile("subl $8, %esp\n\tpushl %esi\n\torl $-1, %esi\n\t"
        "movl $1, 4(%esp)\n\tcall _multip_receive_expire\n\ttestl %eax, %eax\n\tjnz 3f\n\t"
        "1: cmpl $0, 4(%esp)\n\tjz 3f\n\t"
        "leal 4(%esp), %eax\n\tleal 8(%esp), %ecx\n\tpushl %eax\n\tpushl %ecx\n\t"
        "call _multip_receive_take@8\n\ttestl %eax, %eax\n\tjnz 3f\n\t"
        "cmpl $0, 4(%esp)\n\tjz 2f\n\tmovl 8(%esp), %eax\n\tmovl 8(%eax), %ecx\n\t"
        "testl %ecx, %ecx\n\tjz 2f\n\tpushl 16(%eax)\n\tpushl 12(%eax)\n\tpushl %ecx\n\tpushl (%eax)\n\t"
        "call _multip_receive_dispatch@16\n\ttestl %eax, %eax\n\tjnz 3f\n\t"
        "movl 8(%esp), %ecx\n\taddl $8, %ecx\n\tpushl %ecx\n\t"
        "movl _s_multip_image+4, %edx\n\tcall *0x214(%edx)\n\ttestl %eax, %eax\n\tjnz 3f\n\t"
        "leal 8(%esp), %eax\n\tpushl %eax\n\tmovl _s_multip_image+4, %ecx\n\t"
        "call *0x214(%ecx)\n\ttestl %eax, %eax\n\tjnz 3f\n\t"
        "2: decl %esi\n\tjnz 1b\n\t3: popl %esi\n\taddl $8, %esp\n\tret $8");
}
#else
static uint32_t FS98_STDCALL multip_receive_callback(uint32_t code, uint32_t argument)
{
    uint32_t remaining = 0xffffffffu, received = 1, result;
    struct multip_receive_record *record;
    (void)code; (void)argument;
    result = (uint32_t)multip_receive_expire();
    if (result != 0) return result;
    do {
        if (received == 0) return 0;
        result = (uint32_t)multip_receive_take((void **)&record, &received);
        if (result != 0) return result;
        if (received != 0 && record->payload != NULL) {
            result = (uint32_t)multip_receive_dispatch(record->from_id, record->payload, record->size, record->timestamp);
            if (result != 0) return result;
            result = (uint32_t)HOST_FN(0x214, multip_free_fn)(&record->payload);
            if (result != 0) return result;
            result = (uint32_t)HOST_FN(0x214, multip_free_fn)((void **)&record);
            if (result != 0) return result;
        }
    } while (--remaining != 0);
    return 0;
}

#endif
