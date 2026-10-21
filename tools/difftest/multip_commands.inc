/* Command outcomes include retail's unguarded pointer faults and empty/no-
 * eligible spins. The watchdog recovers the main thread at the call boundary. */
static void *s_command_jump[5];
static volatile LONG s_command_active, s_command_shutdown;
static volatile DWORD s_command_started;
static volatile uint32_t s_command_outcome;
static HANDLE s_command_main;
static uint32_t s_command_confirm;
static const struct multip_module *s_command_module;
static void command_escape(void) { __builtin_longjmp(s_command_jump, 1); }
static DWORD WINAPI command_watchdog(void *unused)
{
    (void)unused;
    while (!s_command_shutdown) {
        Sleep(5);
        if (s_command_active && GetTickCount() - s_command_started > 50) {
            CONTEXT context;
            SuspendThread(s_command_main);
            if (s_command_active) {
                memset(&context, 0, sizeof(context)); context.ContextFlags = CONTEXT_CONTROL;
                if (!GetThreadContext(s_command_main, &context)) ExitProcess(2);
                s_command_outcome = 2; s_command_active = 0;
                context.Eip = (DWORD)(uintptr_t)command_escape;
                if (!SetThreadContext(s_command_main, &context)) ExitProcess(2);
            }
            ResumeThread(s_command_main);
        }
    }
    return 0;
}
static LONG CALLBACK command_fault(EXCEPTION_POINTERS *exception)
{
    uintptr_t ip = exception->ContextRecord->Eip, base = (uintptr_t)s_command_module->module_handle;
    if (s_command_active && exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ip >= base + 0x1000 && ip < base + 0xd000) {
        s_command_outcome = 1; s_command_active = 0;
        exception->ContextRecord->Eip = (DWORD)(uintptr_t)command_escape;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

__attribute__((naked)) static uint32_t command_call_painted(
    void *function __attribute__((unused)), uint32_t code __attribute__((unused)),
    uint32_t argument __attribute__((unused)), uint32_t padding __attribute__((unused)))
{
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 16(%esp), %esi\n\tmovl 20(%esp), %edi\n\tmovl 24(%esp), %edx\n\tmovl 28(%esp), %ebx\n\t"
        "subl $0x200, %esp\n\tmovl %ebx, 0xcc(%esp)\n\tmovl %ebx, 0xd8(%esp)\n\t"
        "notl %ebx\n\tmovl %ebx, 0xd4(%esp)\n\tmovl %ebx, 0xe0(%esp)\n\t"
        "movl $0x12345678, 0xd0(%esp)\n\tmovl $0xabcdef98, 0xdc(%esp)\n\t"
        "addl $0x100, %esp\n\ttestl %edi, %edi\n\tjz 1f\n\tpushl %edx\n\tpushl %edi\n\t"
        "call *%esi\n\tjmp 2f\n1:\tcall *%esi\n2:\taddl $0x100, %esp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
struct command_results {
    struct motion_fixture motion;
    struct player_fixture players;
    uint32_t returns[16], outcomes[16], state[16][12], signals, queues[2];
};
static uint32_t STDCALL command_confirm(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
    uint32_t e, uint32_t f, uint32_t g, uint32_t h)
{
    uint32_t *entry = motion_log(0xa4);
    entry[1] = a; entry[2] = b; entry[3] = c; entry[4] = d | e | f | g | h;
    return s_command_confirm;
}
static void STDCALL command_vector_add(void *dest, const void *a, const void *b)
{
    uint32_t *entry = motion_log(0x74);
    entry[1] = draw_hash(a, 24); entry[2] = draw_hash(b, 24);
    (*(void (STDCALL **)(void *, const void *, const void *))(s_motion_util + 0x74))(dest, a, b);
}
static int32_t command_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t *entry = motion_log(tag);
    entry[1] = code; entry[2] = (void *)callback == send_internal(s_motion_module, code == 0x19 ? 54 : 101);
    entry[3] = get_u32(s_motion_module->data, 0x96c);
    return (int32_t)s_motion.chain_result;
}
static int32_t STDCALL command_add(uint32_t code, track_callback_fn callback)
{ return command_registration(3, code, callback); }
static int32_t STDCALL command_remove(uint32_t code, track_callback_fn callback)
{ return command_registration(4, code, callback); }
static void command_snapshot(struct command_results *results, uint32_t step, uint32_t result)
{
    static const uint32_t s_offsets[] = {0x96c,0xa58,0xa5c,0xa60,0xa64,0x99c,0x9a0,0x9a4,0xf1c,0xf20};
    uint32_t index;
    results->returns[step] = result; results->outcomes[step] = s_command_outcome;
    for (index = 0; index < 10; index++) results->state[step][index] = get_u32(s_motion_module->data, s_offsets[index]);
    results->state[step][10] = get_u32(s_motion.global, 0x11b0);
    results->state[step][11] = get_u32(s_motion.global, 0x11dc);
}
static uint32_t command_invoke(uint32_t entry, uint32_t code, uint32_t padding)
{
    uint32_t result = 0;
    s_command_outcome = 0;
    if (__builtin_setjmp(s_command_jump) == 0) {
        s_command_started = GetTickCount(); s_command_active = 1;
        if (entry == 100 || entry == 101)
            result = command_call_painted(send_internal(s_command_module, entry), code, 0x87654321u, padding);
        else
            result = ((multip_worker_fn)send_internal(s_command_module, entry))();
        s_command_active = 0;
    }
    return s_command_outcome ? 0 : result;
}
static void run_commands(const struct multip_module *module, uint32_t number, struct command_results *results)
{
    uint8_t host[0x320] = {0}, object[32] = {0};
    void *util[51] = {0}, *chain[9] = {0}, *function;
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t random_state = number ^ 0x81364972u, index, byte;

    if (event == NULL) exit(2);
    memset(results, 0, sizeof(*results)); memset(&s_motion, 0, sizeof(s_motion));
    memset(&s_players, 0, sizeof(s_players)); memset(&s_local, 0, sizeof(s_local));
    memset(queues, 0, sizeof(queues));
    s_command_module = module; s_motion_module = module; s_players_module = module;
    s_motion.number = number; s_players.number = number;
    memcpy(module->data, module->initial_data, 0x1000);
    function = player_alloc; memcpy(host + 0x20c, &function, 4);
    function = player_free; memcpy(host + 0x214, &function, 4);
    function = local_memset; memcpy(host + 0x218, &function, 4);
    function = motion_copy; memcpy(host + 0x21c, &function, 4);
    function = local_strcpy; memcpy(host + 0x2a8, &function, 4);
    util[0xc4 / 4] = motion_signed_write; util[0x44 / 4] = motion_add;
    util[0x74 / 4] = command_vector_add; util[0xa4 / 4] = command_confirm;
    chain[0x18 / 4] = command_add; chain[0x1c / 4] = command_remove;
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x54 / 4] = local_get_name; s_directplay_vtable[0x78 / 4] = local_set_name;
    s_directplay_vtable[0x58 / 4] = player_description; s_directplay_vtable[0x7c / 4] = player_set_description;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_motion.global);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_CONNECTED, 1); put_u32(module->data, 0xaa8, number & 1);
    put_u32(module->data, 0x998, random_u32(&random_state));
    put_u32(module->data, 0x99c, number & 2 ? 0xffffffffu : 0);
    put_u32(module->data, 0x9a0, number & 4 ? 0 : 1);
    put_u32(module->data, 0x9a4, get_u32(module->data, 0x99c) - (number & 0x200 ? 0 : 1));
    put_u32(module->data, 0x160, random_u32(&random_state));
    put_u32(module->data, 0x168, number % 8);
    put_u32(module->data, 0xa10, random_u32(&random_state));
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_motion.players);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_motion.players[0]);
    put_u32(module->data, M_PLAYER_COUNT, 4);
    put_u32(module->data, M_PLAYER_MODE, (number / 2) % 3);
    put_u32(module->data, M_ACTIVE_PLAYER_ID, (number / 6) % 5 + 0x5000);
    put_u32(module->data, 0xa58, (number / 30) % 3);
    put_u32(module->data, 0xa64, (number / 90) % 4 == 0);
    for (index = 0; index < 4; index++) {
        uint8_t *player = s_motion.players[index], *model = s_motion.models[index];
        for (byte = 0; byte < 256; byte++) player[byte] = random_u32(&random_state);
        for (byte = 0; byte < 64; byte++) model[byte] = random_u32(&random_state);
        put_u32(player, 0, (uint32_t)(uintptr_t)"command peer");
        put_u32(player, 4, 0x5000 + index); put_u32(player, 8, (number + index) % 5);
        put_u32(player, 0xc, index == number % 4);
        put_u32(player, 0x28, (uint32_t)(uintptr_t)model); put_u32(player, 0x30, number % 500);
        put_u32(model, 8, (int32_t)random_u32(&random_state) % 8000000);
    }
    put_u32(s_motion.global, 0x11b0, random_u32(&random_state) | 0x8f100000u);
    put_u32(s_motion.global, 0x11dc, random_u32(&random_state));
    put_u32(s_motion.global, 0x11e0, (uint32_t)(uintptr_t)object);
    put_u32(object, 0xc, number % 1000);
    s_motion.chain_result = (number / 3) % 3 == 0;
    s_command_confirm = number & 8 ? 0x41 : 0x20;
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    put_u32(module->data, 0xf00, number % 3);
    command_snapshot(results, 0, command_invoke(97, 0, number));
    command_snapshot(results, 1, command_invoke(97, 0, number));
    command_snapshot(results, 2, command_invoke(98, 0, number));
    command_snapshot(results, 3, command_invoke(100, 0, random_u32(&random_state)));
    command_snapshot(results, 4, command_invoke(101, 0x1016e, random_u32(&random_state)));
    command_snapshot(results, 5, command_invoke(99, 0, number));
    command_snapshot(results, 6, command_invoke(99, 0, number));
    command_snapshot(results, 7, command_invoke(102, 0, number));
    command_snapshot(results, 8, command_invoke(103, 0, number));
    s_motion.chain_result = 0;
    command_snapshot(results, 9, command_invoke(103, 0, number));
    command_snapshot(results, 10, command_invoke(103, 0, number));
    command_snapshot(results, 11, command_invoke(101, 0x1016c, number));
    command_snapshot(results, 12, command_invoke(101, 0x1016d, number));
    command_snapshot(results, 13, command_invoke(101, 0x1016f, number));
    command_snapshot(results, 14, command_invoke(101, number & 1 ? 0xffffffffu : 0x1016b, number));
    /* Exhaust absent, empty and no-eligible outcomes without omitting them. */
    if (number % 128 == 0) {
        for (index = 0; index < 4; index++) {
            put_u32(s_motion.players[index], 8, 2); put_u32(s_motion.players[index], 0xc, 0);
        }
        put_u32(module->data, M_PLAYER_COUNT, (number / 128) % 3);
        if ((number / 384) % 2) put_u32(module->data, M_PLAYERS, 0);
        command_snapshot(results, 15, command_invoke(98, 0, number));
    }
    results->signals = WaitForSingleObject(event, 0);
    for (index = 0; index < 2; index++) {
        results->queues[index] = queues[index].count;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
    for (index = 0; index < s_players.block_count; index++)
        if (s_local.name_blocks[index]) put_u32(s_players.heap[index], 12,
            get_u32(s_players.heap[index], 12) != 0 ? 0xf0000000u : 0);
    /* The module's global-object pointer refers to this fixture's stack. */
    put_u32(s_motion.global, 0x11e0, 0);
    memcpy(&results->motion, &s_motion, sizeof(s_motion));
    memcpy(&results->players, &s_players, sizeof(s_players));
}
static uint32_t test_commands(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct command_results s_retail, s_ours;
    uint32_t number, bad = 0, hangs = 0, faults = 0;
    void *fault_handler, *divide_handler;
    HANDLE watchdog;
    s_motion_util_module = LoadLibraryA("UTIL_retail.dll");
    if (!s_motion_util_module) exit(2);
    s_motion_util = (uint8_t *)GetProcAddress(s_motion_util_module, "Linkage");
    fault_handler = AddVectoredExceptionHandler(1, command_fault);
    divide_handler = AddVectoredExceptionHandler(1, motion_divide);
    if (!fault_handler || !divide_handler || !s_motion_util) exit(2);
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_command_main,
        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, 0)) exit(2);
    s_command_shutdown = 0;
    watchdog = CreateThread(NULL, 0, command_watchdog, NULL, 0, NULL);
    if (!watchdog) exit(2);
    for (number = 0; number < scenarios; number++) {
        multip_test_context("commands", "retail", number); run_commands(retail, number, &s_retail);
        multip_test_context("commands", "ours", number); run_commands(ours, number, &s_ours);
        hangs += s_retail.outcomes[15] == 2; faults += s_retail.outcomes[15] == 1;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("commands scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    s_command_shutdown = 1; WaitForSingleObject(watchdog, INFINITE);
    CloseHandle(watchdog); CloseHandle(s_command_main);
    RemoveVectoredExceptionHandler(fault_handler); RemoveVectoredExceptionHandler(divide_handler);
    FreeLibrary(s_motion_util_module); s_motion_util_module = NULL;
    printf("command outcomes: %lu spins, %lu pointer faults\n", (unsigned long)hangs, (unsigned long)faults);
    printf("MULTIP view commands/CHAIN 0x0c: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
