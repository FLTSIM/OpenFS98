/* Public registration and CHAIN startup/shutdown, .text+0x20..+0x250. */
typedef int32_t (FS98_STDCALL *multip_init_chain_fn)(uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_module_register_fn)(void *, uint32_t *);
typedef int32_t (FS98_STDCALL *multip_module_unregister_fn)(uint32_t);
typedef void *(FS98_STDCALL *multip_application_fn)(void);
typedef int32_t (FS98_STDCALL *multip_resource_path_fn)(uint32_t, char *, uint32_t);

static int32_t FS98_STDCALL multip_init_callback(uint32_t code, void *event);
static int32_t FS98_STDCALL multip_shutdown_callback(uint32_t code, void *event);

static int32_t multip_init(void)
{
    int32_t result;
    HOST_FN(0x298, multip_module_register_fn)((void *)(uintptr_t)(D + MULTIP_LINKAGE),
        (uint32_t *)(uintptr_t)(D + 0xa34));
    M32(0xa38) = ADDR(HOST_FN(0x58, multip_application_fn)());
    result = (*(multip_init_chain_fn *)(uintptr_t)(M32(0x1c) + 0x18))(
        0x14, (void *)multip_init_callback);
    if (result == 0) {
        M32(M_DIRECTPLAY_ENUMERATE) = 0;
        M32(M_DIRECTPLAY_CREATE) = 0;
        M32(M_DIRECTPLAY_LOBBY) = 0;
    }
    return result;
}

static int32_t multip_deinit(void)
{
    return HOST_FN(0x29c, multip_module_unregister_fn)(M32(0xa34));
}

#if defined(__i386__)
/* Preserve the two saved registers and the caller's code cell used as the
 * allocation output. Lobby completion reads an unwritten deep stack word. */
__attribute__((naked, used)) static int32_t FS98_STDCALL multip_init_callback(
    uint32_t code __attribute__((unused)), void *event __attribute__((unused)))
{
    __asm__ volatile(
        "movl 4(%esp), %eax\n\tpushl %esi\n\tpushl %edi\n\t"
        "cmpl $0x36, %eax\n\tje 2f\n\tcmpl $0x3a, %eax\n\tjne 9f\n\t"
        "movl 16(%esp), %eax\n\txorl %edi, %edi\n\tmovl 8(%eax), %esi\n\t"
        "movl 0x110(%esi), %eax\n\ttestl %eax, %eax\n\tjz 9f\n\t"
        "pushl $1\n\tleal 16(%esp), %ecx\n\tpushl %ecx\n\tpushl %eax\n\tpushl $1\n\t"
        "movl _s_multip_image+4, %eax\n\tcall *0x20c(%eax)\n\ttestl %eax, %eax\n\tjnz 9f\n\t"
        "pushl 0x110(%esi)\n\taddl $0x114, %esi\n\tpushl %esi\n\tpushl 20(%esp)\n\t"
        "movl _s_multip_image+4, %eax\n\tcall *0x21c(%eax)\n\ttestl %eax, %eax\n\tjnz 9f\n\t"
        "movl 12(%esp), %eax\n\tmovl 8(%eax), %esi\n\taddl %eax, %esi\n\tmovl %esi, 8(%eax)\n\t"
        "movl 12(%esp), %ecx\n\tmovl 8(%ecx), %eax\n\tmovl 0x30(%eax), %edx\n\taddl %ecx, %edx\n\tmovl %edx, 0x30(%eax)\n\t"
        "movl 12(%esp), %eax\n\tmovl 8(%eax), %ecx\n\tmovl %edi, 0x34(%ecx)\n\t"
        "movl 12(%esp), %eax\n\tmovl 12(%eax), %ecx\n\taddl %eax, %ecx\n\tmovl %ecx, 12(%eax)\n\t"
        "movl 12(%esp), %ecx\n\tmovl 12(%ecx), %eax\n\tmovl 8(%eax), %esi\n\taddl %ecx, %esi\n\tmovl %esi, 8(%eax)\n\t"
        "movl 12(%esp), %ecx\n\tmovl 12(%ecx), %eax\n\tmovl 12(%eax), %edx\n\taddl %ecx, %edx\n\tmovl %edx, 12(%eax)\n\t"
        "movl 12(%esp), %eax\n\tmovl 0x20(%eax), %ecx\n\taddl %eax, %ecx\n\tmovl %ecx, 0x20(%eax)\n\t"
        "movl _s_multip_image+0xa48, %eax\n\ttestl %eax, %eax\n\tjz 1f\n\t"
        "pushl 12(%esp)\n\tcall _multip_lobby_start_connection@4\n\ttestl %eax, %eax\n\tjnz 9f\n\t"
        "1: leal 12(%esp), %eax\n\tpushl %eax\n\tmovl _s_multip_image+4, %eax\n\tcall *0x214(%eax)\n\tjmp 9f\n\t"
        "2: call _multip_profile_load\n\ttestl %eax, %eax\n\tjnz 9f\n\t"
        "pushl $0x104\n\tpushl $_s_multip_image+0xb00\n\tpushl $0xf60a\n\tmovl _s_multip_image+4, %eax\n\tcall *0x2a0(%eax)\n\t"
        "movl _s_multip_image+0x18c, %eax\n\txorl %edi, %edi\n\ttestl %eax, %eax\n\tjz 3f\n\t"
        "movl _s_multip_image+0x190, %eax\n\torb $0x40, %al\n\tmovl %eax, _s_multip_image+0x190\n\t"
        "3: movl _s_multip_image+0x1c, %eax\n\tpushl $_multip_shutdown_callback@8\n\tpushl $0x14\n\t"
        "movl %edi, _s_multip_image+0x9a0\n\tmovl %edi, _s_multip_image+0x9a4\n\tmovl %edi, _s_multip_image+0x984\n\t"
        "movl %edi, _s_multip_image+0xa40\n\tmovl $1, _s_multip_image+0x1a4\n\tcall *0x18(%eax)\n\t"
        "testl %eax, %eax\n\tjnz 9f\n\tcall _multip_workers_setup\n\ttestl %eax, %eax\n\tjnz 9f\n\tcall _multip_lobby_start\n\t"
        "9: popl %edi\n\tpopl %esi\n\tret $8");
}

/* Other event codes leave incoming EAX untouched. */
__attribute__((naked, used)) static int32_t FS98_STDCALL multip_shutdown_callback(
    uint32_t code __attribute__((unused)), void *event __attribute__((unused)))
{
    __asm__ volatile(
        "cmpl $0x37, 4(%esp)\n\tjne 1f\n\tcall _multip_session_disconnect\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "call _multip_profile_save\n\ttestl %eax, %eax\n\tjnz 1f\n\tcall _multip_textures_cleanup\n\t"
        "testl %eax, %eax\n\tjnz 1f\n\tcall _multip_workers_teardown\n\t1: ret $8");
}
#else
static int32_t FS98_STDCALL multip_init_callback(uint32_t code, void *event)
{
    int32_t result;
    uint8_t *payload;
    void *connection;
    if (code == 0x36) {
        result = multip_profile_load();
        if (result != 0) return result;
        HOST_FN(0x2a0, multip_resource_path_fn)(0xf60a, (char *)(uintptr_t)(D + 0xb00), 260);
        if (M32(0x18c) != 0) M32(0x190) |= 0x40;
        M32(0x9a0) = 0;
        M32(0x9a4) = 0;
        M32(0x984) = 0;
        M32(0xa40) = 0;
        M32(0x1a4) = 1;
        result = (*(multip_init_chain_fn *)(uintptr_t)(M32(0x1c) + 0x18))(0x14, (void *)multip_shutdown_callback);
        if (result != 0) return result;
        result = multip_workers_setup();
        return result != 0 ? result : multip_lobby_start();
    }
    if (code != 0x3a) return code;
    payload = (uint8_t *)(uintptr_t)A32(event, 8);
    if (A32(payload, 0x110) == 0) return 0;
    result = HOST_FN(0x20c, multip_alloc_fn)(1, A32(payload, 0x110), &connection, 1);
    if (result != 0) return result;
    result = HOST_FN(0x21c, multip_copy_fn)(connection, payload + 0x114, A32(payload, 0x110));
    if (result != 0) return result;
    A32(connection, 8) += ADDR(connection);
    A32((void *)(uintptr_t)A32(connection, 8), 0x30) += ADDR(connection);
    A32((void *)(uintptr_t)A32(connection, 8), 0x34) = 0;
    A32(connection, 12) += ADDR(connection);
    A32((void *)(uintptr_t)A32(connection, 12), 8) += ADDR(connection);
    A32((void *)(uintptr_t)A32(connection, 12), 12) += ADDR(connection);
    A32(connection, 0x20) += ADDR(connection);
    if (M32(M_DIRECTPLAY_LOADED) != 0) {
        result = multip_lobby_start_connection(connection);
        if (result != 0) return result;
    }
    return HOST_FN(0x214, multip_free_fn)(&connection);
}
static int32_t FS98_STDCALL multip_shutdown_callback(uint32_t code, void *event)
{
    int32_t result;
    (void)event;
    if (code != 0x37) return 0; /* Incoming EAX has no portable ABI equivalent. */
    result = multip_session_disconnect();
    if (result != 0) return result;
    result = multip_profile_save();
    if (result != 0) return result;
    result = multip_textures_cleanup();
    return result != 0 ? result : multip_workers_teardown();
}
#endif
