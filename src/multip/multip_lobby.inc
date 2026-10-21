/* DirectPlay lobby connection owners, .text+0x9990..+0x9c60. */
typedef int32_t (FS98_STDCALL *multip_lobby_settings_fn)(void *, uint32_t, uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_lobby_connect_fn)(void *, uint32_t, void **, void *);
typedef int32_t (FS98_STDCALL *multip_lobby_create_fn)(void *, void **, void *, void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_lobby_get_fn)(void **, uint32_t *, uint32_t);
#define LOBBY_FN(object, offset, type) (*(type *)(void *)(*(uint8_t **)(object) + (offset)))

/* .text+0x9bd0 -- the second QueryInterface HRESULT is consumed. Unlike
 * normal creation, this owner does not first clear the queried flag. */
static int32_t FS98_STDCALL __attribute__((used)) multip_lobby_connect(void *lobby)
{
    void *connected;
    if (LOBBY_FN(lobby, 0xc, multip_lobby_connect_fn)(lobby, 0, &connected, NULL) != 0) return -1;
    if (LOBBY_FN(connected, 0, multip_dp_query_fn)(connected, &s_iid_directplay3a,
        (void **)(uintptr_t)(D + M_DIRECTPLAY)) == 0) M32(M_DIRECTPLAY_QUERIED) = 1;
    LOBBY_FN(connected, 8, multip_dp_release_fn)(connected);
    return 0;
}
static int32_t FS98_STDCALL __attribute__((used, noinline)) multip_lobby_host_prepare(void *lobby, void *connection)
{
    const uint32_t *application = (const uint32_t *)(uintptr_t)M32(0xa38);
    uint32_t index;
    (void)lobby;
    M32(0x9c0) = 80; M32(0x9c4) = M32(0x190);
    for (index = 0; index < 4; index++) M32(0x9d8 + index * 4) = application[index];
    M32(0x9f0) = D + 0xae0; M32(0x9e8) = M32(0x998) + M32(0x99c);
    return HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)A32(connection, 8),
        (void *)(uintptr_t)(D + 0x9c0), 80) != 0 ? -1 : 0;
}
static void FS98_STDCALL __attribute__((used, noinline)) multip_lobby_apply_prepare(void *connection)
{
    void *name = (void *)(uintptr_t)A32(connection, 12);
    void *description;
    HOST_FN(0x2a8, multip_strcpy_fn)((char *)(uintptr_t)(D + 0x9a8),
        (const char *)(uintptr_t)A32(name, 8));
    description = (void *)(uintptr_t)A32(connection, 8);
    A32(description, 4) |= M32(0x190);
}
#if defined(__i386__)
/* Keep lobby/completion nesting: local player startup consumes a deep caller
 * stack word. C preparation helpers do not reach that stack slot. */
__attribute__((naked, used)) static int32_t FS98_STDCALL multip_lobby_host(
    void *lobby __attribute__((unused)), void *connection __attribute__((unused)))
{
    __asm__ volatile("pushl 8(%esp)\n\tpushl 8(%esp)\n\tcall _multip_lobby_host_prepare@8\n\t"
        "testl %eax, %eax\n\tjnz 1f\n\tpushl 4(%esp)\n\tcall _multip_lobby_connect@4\n\t"
        "testl %eax, %eax\n\tjnz 1f\n\tmovl $1, %eax\n\tmovl %eax, _s_multip_image+0xaa8\n\t"
        "movl %eax, _s_multip_image+0xaac\n\tmovl %eax, _s_multip_image+0x1a4\n\t"
        "call _multip_session_create_groups\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "call _multip_session_complete\n\tnegl %eax\n\tsbbl %eax, %eax\n\tret $8\n\t"
        "1: orl $-1, %eax\n\tret $8");
}
__attribute__((naked, used)) static int32_t FS98_STDCALL multip_lobby_join(void *lobby __attribute__((unused)))
{
    __asm__ volatile("pushl 4(%esp)\n\tcall _multip_lobby_connect@4\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "call _multip_session_enum_groups\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "movl $0, _s_multip_image+0xaa8\n\tmovl $1, _s_multip_image+0xaac\n\t"
        "call _multip_session_complete\n\tnegl %eax\n\tsbbl %eax, %eax\n\tret $4\n\t"
        "1: orl $-1, %eax\n\tret $4");
}
__attribute__((naked, used)) static int32_t FS98_STDCALL multip_lobby_apply(
    void *lobby __attribute__((unused)), void *connection __attribute__((unused)))
{
    __asm__ volatile("pushl %esi\n\tmovl 12(%esp), %esi\n\tpushl %edi\n\tmovl 12(%esp), %edi\n\t"
        "pushl %esi\n\tcall _multip_lobby_apply_prepare@4\n\tpushl %esi\n\tpushl $0\n\tpushl $0\n\t"
        "movl (%edi), %edx\n\tpushl %edi\n\tcall *0x30(%edx)\n\tmovl 4(%esi), %eax\n\t"
        "testb $2, %al\n\tjz 2f\n\tpushl %esi\n\tpushl %edi\n\tcall _multip_lobby_host@8\n\tjmp 3f\n\t"
        "2: testb $1, %al\n\tjz 1f\n\tpushl %edi\n\tcall _multip_lobby_join@4\n\t"
        "3: testl %eax, %eax\n\tjnz 1f\n\tmovl $1, _s_multip_image+0xaac\n\tpopl %edi\n\tpopl %esi\n\tret $8\n\t"
        "1: popl %edi\n\torl $-1, %eax\n\tpopl %esi\n\tret $8");
}
__attribute__((naked)) int32_t multip_lobby_start(void)
{
    __asm__ volatile("subl $12, %esp\n\tmovl $0, 8(%esp)\n\tmovl $0, (%esp)\n\t"
        "call _multip_directplay_load\n\ttestl %eax, %eax\n\tjnz 1f\n\tmovl _s_multip_image+4, %edx\n\t"
        "leal 8(%esp), %eax\n\tpushl $0\n\tleal 4(%esp), %ecx\n\tpushl %eax\n\tpushl %ecx\n\t"
        "call *0x120(%edx)\n\ttestl %eax, %eax\n\tjnz 1f\n\tmovl (%esp), %eax\n\ttestl %eax, %eax\n\tjz 2f\n\t"
        "pushl $0\n\tpushl $0\n\tleal 12(%esp), %eax\n\tpushl $0\n\tpushl %eax\n\tpushl $0\n\t"
        "call *_s_multip_image+0x980\n\tpushl (%esp)\n\tpushl 8(%esp)\n\tcall _multip_lobby_apply@8\n\t"
        "testl %eax, %eax\n\tjnz 1f\n\tmovl 4(%esp), %eax\n\tpushl %eax\n\tmovl (%eax), %ecx\n\tcall *8(%ecx)\n\t"
        "2: xorl %eax, %eax\n\taddl $12, %esp\n\tret\n\t1: orl $-1, %eax\n\taddl $12, %esp\n\tret");
}
/* .text+0x9a20 -- even complete success returns -1. */
__attribute__((naked)) int32_t FS98_STDCALL multip_lobby_start_connection(void *connection __attribute__((unused)))
{
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp), %esi\n\ttestl %esi, %esi\n\tjz 1f\n\t"
        "movl $1, _s_multip_image+0x1a4\n\tcall _multip_directplay_load\n\ttestl %eax, %eax\n\tjnz 1f\n\t"
        "pushl %eax\n\tpushl %eax\n\tpushl %eax\n\tleal 20(%esp), %eax\n\tpushl %eax\n\tpushl $0\n\t"
        "call *_s_multip_image+0x980\n\tpushl %esi\n\tpushl 12(%esp)\n\tcall _multip_lobby_apply@8\n\t"
        "testl %eax, %eax\n\tjnz 1f\n\tmovl 8(%esp), %eax\n\tpushl %eax\n\tmovl (%eax), %edx\n\tcall *8(%edx)\n\t"
        "1: orl $-1, %eax\n\tpopl %esi\n\tret $4");
}
#else
static int32_t FS98_STDCALL multip_lobby_host(void *lobby, void *connection)
{
    if (multip_lobby_host_prepare(lobby, connection) != 0 || multip_lobby_connect(lobby) != 0) return -1;
    M32(0xaa8) = 1; M32(M_CONNECTED) = 1; M32(0x1a4) = 1;
    if (multip_session_create_groups() != 0) return -1;
    return multip_session_complete() != 0 ? -1 : 0;
}
static int32_t FS98_STDCALL multip_lobby_join(void *lobby)
{
    if (multip_lobby_connect(lobby) != 0 || multip_session_enum_groups() != 0) return -1;
    M32(0xaa8) = 0; M32(M_CONNECTED) = 1;
    return multip_session_complete() != 0 ? -1 : 0;
}
static int32_t FS98_STDCALL multip_lobby_apply(void *lobby, void *connection)
{
    uint32_t flags;
    int32_t result = -1;
    multip_lobby_apply_prepare(connection);
    LOBBY_FN(lobby, 0x30, multip_lobby_settings_fn)(lobby, 0, 0, connection);
    flags = A32(connection, 4);
    if (flags & 2) result = multip_lobby_host(lobby, connection);
    else if (flags & 1) result = multip_lobby_join(lobby);
    if (result == 0) M32(M_CONNECTED) = 1;
    return result != 0 ? -1 : 0;
}
int32_t multip_lobby_start(void)
{
    void *connection = NULL, *lobby = NULL;
    uint32_t size = 0;
    if (multip_directplay_load() != 0 || HOST_FN(0x120, multip_lobby_get_fn)(&connection, &size, 0) != 0) return -1;
    if (connection != NULL) {
        ((multip_lobby_create_fn)(uintptr_t)M32(M_DIRECTPLAY_LOBBY))(NULL, &lobby, NULL, NULL, 0);
        if (multip_lobby_apply(lobby, connection) != 0) return -1;
        LOBBY_FN(lobby, 8, multip_dp_release_fn)(lobby);
    }
    return 0;
}
int32_t FS98_STDCALL multip_lobby_start_connection(void *connection)
{
    void *lobby = NULL;
    if (connection != NULL) {
        M32(0x1a4) = 1;
        if (multip_directplay_load() == 0) {
            ((multip_lobby_create_fn)(uintptr_t)M32(M_DIRECTPLAY_LOBBY))(NULL, &lobby, NULL, NULL, 0);
            if (multip_lobby_apply(lobby, connection) == 0) LOBBY_FN(lobby, 8, multip_dp_release_fn)(lobby);
        }
    }
    return -1;
}
#endif
