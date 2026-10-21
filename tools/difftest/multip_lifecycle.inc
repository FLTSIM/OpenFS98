/* Integrated completion, receive pumping and disconnect with deterministic
 * Win32 workers. No network, OS threads or real Sleep calls are used. */
struct lifecycle_results {
    struct player_fixture players;
    struct local_controls local;
    uint32_t returns[16], state[16][26], queues[16][6];
};
static struct lifecycle_results s_lifecycle;
static uint32_t s_lifecycle_number, s_lifecycle_clock;
/* The entry fixture exposes a deep stack slot so a CHAIN stub can leave a
 * different word there before local startup, independent of C stub frames. */
static uint32_t *volatile s_lifecycle_reserved_word;
static uint32_t lifecycle_callback_id(void *callback)
{
    static const uint32_t s_entries[] = {85,107,108,109,110,89,205,220,101,94,9,14};
    uint32_t index;
    for (index = 0; index < sizeof(s_entries) / sizeof(s_entries[0]); index++)
        if (callback == send_internal(s_players_module, s_entries[index])) return s_entries[index];
    return 0xffffffffu;
}
static int32_t lifecycle_register(uint32_t tag, uint32_t code, void *callback)
{
    uint32_t *entry = player_log(tag);
    entry[1] = code; entry[2] = lifecycle_callback_id(callback);
    entry[3] = get_u32(s_players_module->data, 0x950);
    if (tag == 40 && code == 0x13 && s_lifecycle_reserved_word != NULL)
        *s_lifecycle_reserved_word = 0x89abcdefu;
    return player_fail() ? (int32_t)0x81234567u : 0;
}
static int32_t STDCALL lifecycle_add(uint32_t code, void *callback)
{ return lifecycle_register(40, code, callback); }
static int32_t STDCALL lifecycle_remove(uint32_t code, void *callback)
{ return lifecycle_register(41, code, callback); }
static uint32_t STDCALL lifecycle_dispatch(uint32_t code, uint32_t event, void *data)
{
    uint32_t *entry = player_log(42);
    entry[1] = code; entry[2] = event;
    if (data != NULL) {
        entry[3] = get_u32(data, 0);
        entry[4] = event == 5 ? get_u32((void *)(uintptr_t)get_u32(data, 4), 0) : get_u32(data, 4) != 0;
        entry[5] = event == 5 ? get_u32(data, 8) : 0;
    }
    return 0x87654321u;
}
static uint32_t STDCALL lifecycle_ticks(void)
{
    static const uint32_t s_steps[] = {0,1,3000,0x7fffffffu,0x80000000u,0xfffffff0u};
    player_log(43);
    s_lifecycle_clock += s_steps[s_lifecycle_number % 6];
    return s_lifecycle_clock;
}
static uint32_t STDCALL lifecycle_confirm(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
    uint32_t e, uint32_t f, uint32_t g, uint32_t h)
{
    uint32_t *entry = player_log(44);
    entry[1] = a; entry[2] = b; entry[3] = c; entry[4] = d; entry[5] = e | f | g | h;
    return s_lifecycle_number & 1 ? 0x40 : 0x1040u & ~0x40u;
}
static uint32_t STDCALL lifecycle_progress(void **out, uint32_t kind, uint32_t text,
    uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t *entry = player_log(45);
    entry[1] = kind; entry[2] = text; entry[3] = a | b | c | d | e;
    *out = (void *)0x6789;
    return 0x81234567u;
}
static uint32_t STDCALL lifecycle_progress_close(void **out)
{ player_log(46)[1] = (uint32_t)(uintptr_t)*out; *out = NULL; return 1; }
static int32_t STDCALL lifecycle_close(void *object)
{
    uint32_t *entry = player_log(47);
    entry[1] = directplay_identity(object); entry[2] = get_u32(s_players_module->data, M_CONNECTED);
    return s_lifecycle_number % 3 == 0 ? 0 : s_lifecycle_number % 3 == 1 ? 1 : (int32_t)0x81234567u;
}
static uint32_t STDCALL lifecycle_release(void *object)
{ player_log(48)[1] = directplay_identity(object); return 0x87654321u; }
static int32_t STDCALL lifecycle_unload(void *exports)
{
    player_log(49)[1] = player_address(exports);
    return player_fail();
}
static int32_t STDCALL lifecycle_destroy(void *object, uint32_t id)
{ uint32_t *entry = player_log(50); entry[1] = directplay_identity(object); entry[2] = id; return -1; }
static int32_t STDCALL lifecycle_send(void *object, uint32_t from, uint32_t to,
    uint32_t flags, const void *packet, uint32_t size)
{
    uint32_t *entry = player_log(51);
    entry[1] = from; entry[2] = to; entry[3] = flags; entry[4] = size; entry[5] = draw_hash(packet, size);
    (void)object;
    return (int32_t)0x80004005u;
}
static int32_t STDCALL lifecycle_enum(void *object, const GUID *instance,
    session_enum_callback callback, void *context, uint32_t flags)
{
    uint32_t *entry = player_log(52);
    entry[1] = directplay_identity(object); entry[2] = player_address(instance);
    entry[3] = flags; entry[4] = callback(0x100, 0, NULL, 0, context);
    return player_fail() ? -1 : 0;
}
static HANDLE WINAPI lifecycle_thread(LPSECURITY_ATTRIBUTES attributes, SIZE_T stack,
    LPTHREAD_START_ROUTINE callback, void *argument, DWORD flags, DWORD *id)
{
    uint32_t *entry = player_log(53);
    entry[1] = player_address(attributes); entry[2] = stack; entry[3] = lifecycle_callback_id((void *)callback);
    entry[4] = argument != NULL; entry[5] = flags;
    *id = 0x4500 + entry[3];
    return player_fail() ? NULL : (HANDLE)(uintptr_t)(0x5500 + entry[3]);
}
static BOOL WINAPI lifecycle_set(HANDLE handle)
{ player_log(54)[1] = (uint32_t)(uintptr_t)handle; return !player_fail(); }
static DWORD WINAPI lifecycle_wait(HANDLE handle, DWORD timeout)
{ uint32_t *entry = player_log(55); entry[1] = (uint32_t)(uintptr_t)handle; entry[2] = timeout; return WAIT_TIMEOUT; }
static BOOL WINAPI lifecycle_close_handle(HANDLE handle)
{ player_log(56)[1] = (uint32_t)(uintptr_t)handle; return !player_fail(); }
static void WINAPI lifecycle_sleep(DWORD delay)
{ player_log(57)[1] = delay; }
static void lifecycle_install(const struct multip_module *module)
{
    worker_patch(module->module_handle, "CreateThread", lifecycle_thread);
    worker_patch(module->module_handle, "SetEvent", lifecycle_set);
    worker_patch(module->module_handle, "WaitForSingleObject", lifecycle_wait);
    worker_patch(module->module_handle, "CloseHandle", lifecycle_close_handle);
    worker_patch(module->module_handle, "Sleep", lifecycle_sleep);
}
static void lifecycle_snapshot(uint32_t step, uint32_t result, struct send_queue *queues)
{
    static const uint32_t s_offsets[] = {0x940,0x950,0x954,0x958,0x968,0x96c,0xa40,
        0xaac,0x984,0xa64,0x9a0,0x9a4,0xa3c,0x988,0xa5c,0xa60,0xf24,0xe04,0xe70,
        0xf1c,0xdfc,0xe68,0x660,0xadc};
    uint32_t index;
    s_lifecycle.returns[step] = result;
    for (index = 0; index < 24; index++)
        s_lifecycle.state[step][index] = player_address((void *)(uintptr_t)get_u32(s_players_module->data, s_offsets[index]));
    s_lifecycle.state[step][24] = get_u32(s_players.global, 0x11b0);
    s_lifecycle.state[step][25] = get_u32(s_players.global, 0x11dc);
    for (index = 0; index < 3; index++) {
        s_lifecycle.queues[step][index * 2] = queues[index].count;
        s_lifecycle.queues[step][index * 2 + 1] = queues[index].lock.RecursionCount;
    }
}
static void lifecycle_seed_receive(struct send_queue *queue, uint32_t sender, uint32_t tag, int payload)
{
    uint32_t index = s_players.block_count;
    uint8_t *record = s_players.heap[index], *packet = s_players.heap[index + 1];
    struct send_node *node = (struct send_node *)s_players.heap[index + 2];
    if (index + 3 > 32) {
        fprintf(stderr, "lifecycle receive heap exhausted\n");
        exit(2);
    }
    s_players.block_count += 3;
    s_players.live[index] = s_players.live[index + 1] = s_players.live[index + 2] = 1;
    s_players.sizes[index] = 20; s_players.sizes[index + 1] = 16; s_players.sizes[index + 2] = 8;
    put_u32(packet, 0, tag); put_u32(packet, 4, 0); put_u32(packet, 8, 0x4500);
    put_u32(record, 0, sender); put_u32(record, 8, payload ? (uint32_t)(uintptr_t)packet : 0);
    put_u32(record, 12, s_lifecycle_number % 17); put_u32(record, 16, 0xfffffffeu);
    node->record = record; node->next = NULL;
    if (queue->tail != NULL) queue->tail->next = node; else queue->head = node;
    queue->tail = node; queue->count++;
}
__attribute__((naked)) static int32_t lifecycle_complete_painted(void *function __attribute__((unused)))
{
    __asm__ volatile("movl 4(%esp), %eax\n\tsubl $0x1000, %esp\n\t"
        "movl $0x6739c15a, 0x3f4(%esp)\n\tleal 0x3f4(%esp), %edx\n\t"
        "movl %edx, _s_lifecycle_reserved_word\n\tmovl $0x12345678, 0x3f0(%esp)\n\t"
        "movl $0xabcdef01, 0x3f8(%esp)\n\taddl $0x800, %esp\n\tcall *%eax\n\t"
        "movl $0, _s_lifecycle_reserved_word\n\taddl $0x800, %esp\n\tret");
}
static void run_lifecycle(const struct multip_module *module, uint32_t number, struct lifecycle_results *results)
{
    uint8_t host[0x320] = {0}, packet[16] = {0};
    void *chain[9] = {0}, *util[44] = {0}, *aircraft[16] = {0}, *function;
    struct send_queue queues[3];
    uint32_t index, bank, sender;
    typedef int32_t (*lifecycle_fn)(void);
    typedef uint32_t (STDCALL *lifecycle_receive_fn)(uint32_t, uint32_t);
    typedef int32_t (STDCALL *lifecycle_system_fn)(const void *, uint32_t, uint32_t);
    memset(&s_lifecycle, 0, sizeof(s_lifecycle)); memset(&s_players, 0, sizeof(s_players));
    memset(&s_local, 0, sizeof(s_local)); memset(queues, 0, sizeof(queues));
    memcpy(module->data, module->initial_data, 0x1000);
    s_players_module = module; s_players.number = number; s_lifecycle_number = number;
    s_lifecycle_clock = number & 8 ? 0xfffffff0u : 0;
    function = player_alloc; memcpy(host + 0x20c, &function, 4);
    function = player_free; memcpy(host + 0x214, &function, 4);
    function = local_memset; memcpy(host + 0x218, &function, 4);
    function = player_copy; memcpy(host + 0x21c, &function, 4);
    function = local_strcpy; memcpy(host + 0x2a8, &function, 4);
    function = player_append; memcpy(host + 0x2f8, &function, 4);
    function = player_remove; memcpy(host + 0x300, &function, 4);
    function = lifecycle_ticks; memcpy(host + 0x38, &function, 4);
    function = lifecycle_unload; memcpy(host + 0x128, &function, 4);
    chain[6] = lifecycle_add; chain[7] = lifecycle_remove; chain[8] = lifecycle_dispatch;
    util[0xa4 / 4] = lifecycle_confirm; util[0xa8 / 4] = lifecycle_progress; util[0xac / 4] = lifecycle_progress_close;
    aircraft[8] = player_select; aircraft[9] = player_release; aircraft[12] = player_title;
    aircraft[13] = player_model; aircraft[14] = player_unload;
    memset(s_directplay_vtable, 0, sizeof(s_directplay_vtable));
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[2] = lifecycle_release; s_directplay_vtable[4] = lifecycle_close;
    s_directplay_vtable[0x18 / 4] = local_create; s_directplay_vtable[0x24 / 4] = lifecycle_destroy;
    s_directplay_vtable[0x30 / 4] = lifecycle_enum;
    s_directplay_vtable[0x54 / 4] = local_get_name; s_directplay_vtable[0x78 / 4] = local_set_name;
    s_directplay_vtable[0x58 / 4] = player_description; s_directplay_vtable[0x7c / 4] = player_set_description;
    s_directplay_vtable[0x68 / 4] = lifecycle_send;
    s_directplay_vtable[0x0c / 4] = player_group_add; s_directplay_vtable[0x1c / 4] = player_group_remove;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_players.global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_CONNECTED, 1); put_u32(module->data, 0xaa8, (number / 8) & 1);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_players.banks[0]);
    put_u32(module->data, M_PLAYER_COUNT, number & 0x40 ? 1 : 0); put_u32(module->data, M_LOCAL_PLAYER, 0);
    put_u32(s_players.banks[0][0], 0, (uint32_t)(uintptr_t)"lifecycle peer");
    put_u32(s_players.banks[0][0], 4, 0x6001); put_u32(s_players.banks[0][0], 8, 4);
    put_u32(s_players.banks[0][0], 0xfc, s_lifecycle_clock - number % 4);
    put_u32(module->data, 0x1a4, 1 + (number / 16) % 3);
    put_u32(module->data, 0xac0, (number / 32) & 1);
    put_u32(module->data, 0x160, 10); put_u32(module->data, 0x17c, number % 3);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xdf0, 0x100); put_u32(module->data, 0xdf4, 0x101);
    put_u32(module->data, 0xe58, 0x102); put_u32(module->data, 0xe5c, 0x103);
    for (index = 0; index < 3; index++) InitializeCriticalSection(&queues[index].lock);
    put_u32(module->data, 0xdf8, (uint32_t)(uintptr_t)&queues[2]);
    for (index = 0; index < 2; index++) put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    strcpy((char *)module->data + 0x9a8, "lifecycle local");
    s_command_module = module; s_command_outcome = 0;
    if (__builtin_setjmp(s_command_jump) != 0) {
        lifecycle_snapshot(15, s_command_outcome, queues);
        goto finish;
    }
    s_command_active = 1;
    lifecycle_snapshot(0, ((lifecycle_fn)send_internal(module, 208))(), queues);
    lifecycle_snapshot(1, ((lifecycle_fn)send_internal(module, 208))(), queues);
    lifecycle_snapshot(2, ((lifecycle_fn)send_internal(module, 209))(), queues);
    lifecycle_snapshot(3, ((lifecycle_fn)send_internal(module, 209))(), queues);
    lifecycle_snapshot(4, lifecycle_complete_painted(send_internal(module, 211)), queues);
    /* A local record exists even on early creation failure. Timeout probing
     * includes the local record, uses wrapping clocks and updates ping time. */
    if (get_u32(module->data, M_LOCAL_PLAYER) != 0) {
        put_u32((void *)(uintptr_t)get_u32(module->data, M_LOCAL_PLAYER), 8, number & 2 ? 4 : 1);
        put_u32((void *)(uintptr_t)get_u32(module->data, M_LOCAL_PLAYER), 0xfc, s_lifecycle_clock - number % 4);
    }
    lifecycle_seed_receive(&queues[2], 0x8001, number & 1 ? 0x100b : 0xdeadbeefu, number % 3 != 0);
    lifecycle_seed_receive(&queues[2], 0, number % 4 == 0 ? 5 : number % 4 == 1 ? 0xffffffffu : 0x102, 1);
    lifecycle_snapshot(5, ((lifecycle_receive_fn)send_internal(module, 220))(9, 0), queues);
    put_u32(packet, 0, number % 5 == 1 ? 0x1006 : number % 5 == 2 ? 0x1016 : number % 5 == 3 ? 0x31 : 0x1005);
    sender = number % 5 == 3 ? 0 : 0x8001;
    lifecycle_snapshot(6, dispatch_painted(send_internal(module, (number & 4) || sender == 0 ? 218 : 217), sender, packet, number % 17, 0), queues);
    lifecycle_snapshot(7, ((lifecycle_fn)send_internal(module, 210))(), queues);
    lifecycle_snapshot(8, ((lifecycle_fn)send_internal(module, 210))(), queues);
    put_u32(packet, 0, 0x1006);
    lifecycle_snapshot(9, dispatch_painted(send_internal(module, 218), 0x8001, packet, 0, 0), queues);
    put_u32(packet, 0, 0x1016);
    lifecycle_snapshot(10, dispatch_painted(send_internal(module, 217), 0x8001, packet, 0, 0), queues);
    put_u32(packet, 0, 0x31);
    lifecycle_snapshot(11, ((lifecycle_system_fn)send_internal(module, 216))(packet, 0, 0), queues);
finish:
    s_command_active = 0; s_lifecycle_reserved_word = NULL;
    memcpy(&s_lifecycle.players, &s_players, sizeof(s_players)); memcpy(&s_lifecycle.local, &s_local, sizeof(s_local));
    for (bank = 0; bank < 2; bank++) for (index = 0; index < 6; index++)
        put_u32(s_lifecycle.players.banks[bank][index], 0,
            player_address((void *)(uintptr_t)get_u32(s_players.banks[bank][index], 0)));
    for (index = 0; index < s_players.block_count; index++) if (s_local.name_blocks[index])
        put_u32(s_lifecycle.players.heap[index], 12, get_u32(s_players.heap[index], 12) != 0);
    for (index = 0; index < 3; index++) {
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    memcpy(results, &s_lifecycle, sizeof(*results));
}
static uint32_t test_lifecycle(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct lifecycle_results s_retail, s_ours;
    uint32_t number, bad = 0, faults = 0;
    void *handler = AddVectoredExceptionHandler(1, command_fault);
    if (handler == NULL) exit(2);
    s_player_fail_modulus = 181;
    lifecycle_install(retail); lifecycle_install(ours);
    for (number = 0; number < scenarios; number++) {
        multip_test_context("lifecycle", "retail", number); run_lifecycle(retail, number, &s_retail);
        multip_test_context("lifecycle", "ours", number); run_lifecycle(ours, number, &s_ours);
        faults += s_retail.returns[15] != 0;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("lifecycle scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    s_player_fail_modulus = 67;
    RemoveVectoredExceptionHandler(handler);
    printf("lifecycle outcomes: %lu pointer faults\n", (unsigned long)faults);
    printf("MULTIP receive/session lifecycle: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
