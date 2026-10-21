/* Paint the caller slots read by +0x8820 and its +0x8700 caller. This also
 * proves the reserved DPNAME word instead of normalizing it away. */
__attribute__((naked)) static int32_t local_call_painted(void *function __attribute__((unused)),
    uint32_t flags __attribute__((unused)), uint32_t start __attribute__((unused)))
{
    __asm__ volatile("movl 4(%esp), %eax\n\tmovl 8(%esp), %edx\n\tmovl 12(%esp), %ecx\n\t"
        "subl $0x900, %esp\n\tmovl %edx, 0xfc(%esp)\n\tnotl %edx\n\t"
        "movl %edx, 0xf8(%esp)\n\tmovl %edx, 0x100(%esp)\n\ttestl %ecx, %ecx\n\tjz 1f\n\t"
        "movl %edx, 0xfc(%esp)\n\tnotl %edx\n\tmovl %edx, 0xf8(%esp)\n1:\t"
        "addl $0x500, %esp\n\tcall *%eax\n\taddl $0x400, %esp\n\tret");
}

struct local_controls {
    uint32_t create_calls;
    uint32_t name_blocks[32];
    uint32_t ticks;
};
struct local_results {
    struct player_fixture players;
    struct local_controls controls;
    uint32_t signals[3];
    uint32_t queue_state[3][4];
    uint32_t request_state[3][2];
};
static struct local_controls s_local;

static int32_t STDCALL local_memset(void *dest, int32_t value, uint32_t size)
{
    uint32_t *entry = player_log(15);
    entry[1] = size == 256 ? player_address(dest) : 0xf0000000u;
    entry[2] = value; entry[3] = size;
    memset(dest, value, size);
    return player_fail();
}
static int32_t STDCALL local_strcpy(char *dest, const char *source)
{
    uint32_t *entry = player_log(16);
    uintptr_t dest_address = (uintptr_t)dest;
    uintptr_t fixture_address = (uintptr_t)&s_players;
    entry[1] = dest_address >= fixture_address && dest_address < fixture_address + sizeof(s_players) ?
        player_address(dest) : 0xf0000000u;
    entry[2] = profile_hash(source);
    strcpy(dest, source);
    return player_fail();
}
static int32_t STDCALL local_create(void *object, uint32_t *id,
    const void *name, HANDLE event, void *data, uint32_t size, uint32_t flags)
{
    static const uint32_t s_results[] = {0,0,1,0x80004005u,0x80070057u,0,0x80004001u,0xffffffffu};
    uint32_t *entry = player_log(17);
    int32_t result = player_fail() ? -1 : (int32_t)s_results[s_players.number % 8];
    entry[1] = player_address(id); entry[2] = get_u32(name, 0);
    entry[3] = get_u32(name, 4);
    entry[4] = profile_hash((const char *)(uintptr_t)get_u32(name, 8));
    entry[5] = profile_hash((const char *)(uintptr_t)get_u32(name, 12));
    entry = player_log(18);
    entry[1] = (uint32_t)(uintptr_t)event; entry[2] = data != NULL;
    entry[3] = size; entry[4] = flags; entry[5] = directplay_identity(object);
    *id = 0x4500 + s_local.create_calls++;
    return result;
}
static int32_t STDCALL local_get_name(void *object, uint32_t id, void *name, uint32_t *size)
{
    uint32_t *entry = player_log(19), index;
    entry[1] = directplay_identity(object); entry[2] = id;
    entry[3] = name != NULL; entry[4] = *size;
    *size = 32;
    if (name != NULL) {
        memset(name, 0, 32);
        put_u32(name, 0, 16);
        for (index = 0; index < s_players.block_count; index++)
            if (name == s_players.heap[index]) s_local.name_blocks[index] = 1;
    }
    /* The probe HRESULT is ignored; its size is still used. */
    return player_fail() ? -1 : name == NULL ? (int32_t)0x8877001eu : 0;
}
static int32_t STDCALL local_set_name(void *object, uint32_t id, void *name, uint32_t flags)
{
    uint32_t *entry = player_log(20);
    entry[1] = directplay_identity(object); entry[2] = id;
    entry[3] = flags; entry[4] = get_u32(name, 0);
    entry[5] = profile_hash((const char *)(uintptr_t)get_u32(name, 12));
    return player_fail();
}
static uint32_t STDCALL local_ticks(void)
{
    player_log(21);
    return ++s_local.ticks;
}

static void run_local(const struct multip_module *module, uint32_t number,
                      struct local_results *results)
{
    static const uint32_t s_types[] = {0,1,2,3,4,0xffffffffu};
    uint8_t host[0x320] = {0};
    void *chain[9] = {0};
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    void *function;
    uint32_t random_state = number ^ 0x54216783u;
    uint32_t index, step, flags, result, bank;

    if (event == NULL) exit(2);
    memset(results, 0, sizeof(*results));
    memset(&s_players, 0, sizeof(s_players));
    memset(&s_local, 0, sizeof(s_local));
    memset(queues, 0, sizeof(queues));
    s_players.number = number; s_players_module = module;
    s_local.ticks = random_u32(&random_state);
    memcpy(module->data, module->initial_data, 0x1000);
    function = player_alloc; memcpy(host + 0x20c, &function, 4);
    function = player_free; memcpy(host + 0x214, &function, 4);
    function = local_memset; memcpy(host + 0x218, &function, 4);
    function = player_copy; memcpy(host + 0x21c, &function, 4);
    function = local_strcpy; memcpy(host + 0x2a8, &function, 4);
    function = player_append; memcpy(host + 0x2f8, &function, 4);
    function = local_ticks; memcpy(host + 0x38, &function, 4);
    chain[8] = (void *)player_dispatch;
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x18 / 4] = (void *)local_create;
    s_directplay_vtable[0x54 / 4] = (void *)local_get_name;
    s_directplay_vtable[0x78 / 4] = (void *)local_set_name;
    s_directplay_vtable[0x58 / 4] = (void *)player_description;
    s_directplay_vtable[0x7c / 4] = (void *)player_set_description;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_players.global);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_CONNECTED, 1); put_u32(module->data, 0xaa8, (number / 48) & 1);
    put_u32(module->data, M_LOCAL_PLAYER, 0); put_u32(module->data, M_PLAYER_COUNT, 0);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_players.banks[0]);
    put_u32(module->data, 0x1a4, s_types[(number / 8) % 6]);
    put_u32(module->data, 0x160, random_u32(&random_state));
    put_u32(module->data, 0xdf4, 0x12345678);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    for (index = 0; index < 4; index++)
        put_u32(module->data, 0x998 + index * 4, index >= 2 && (number & 0x80) ?
            0xffffffffu : random_u32(&random_state));
    for (index = 0; index < number % 21; index++)
        module->data[0x9a8 + index] = 'A' + random_u32(&random_state) % 26;
    module->data[0x9a8 + number % 21] = 0;
    for (step = 0; step < 3; step++) {
        flags = number % 3 == 0 ? 0 : random_u32(&random_state);
        result = local_call_painted(send_internal(module, step == 0 ? 79 : 80), flags, step != 0);
        player_snapshot(step, result);
        results->signals[step] = WaitForSingleObject(event, 0);
        results->request_state[step][0] = get_u32(module->data, 0x984);
        results->request_state[step][1] = get_u32(module->data, 0xadc);
        for (index = 0; index < 2; index++) {
            results->queue_state[step][index * 2] = queues[index].count;
            results->queue_state[step][index * 2 + 1] = queues[index].lock.RecursionCount;
        }
    }
    memcpy(&results->players, &s_players, sizeof(s_players));
    memcpy(&results->controls, &s_local, sizeof(s_local));
    for (bank = 0; bank < 2; bank++)
        for (index = 0; index < 6; index++)
            put_u32(results->players.banks[bank][index], 0,
                player_address((void *)(uintptr_t)get_u32(s_players.banks[bank][index], 0)));
    for (index = 0; index < s_players.block_count; index++)
        if (s_local.name_blocks[index])
            put_u32(results->players.heap[index], 12,
                get_u32(s_players.heap[index], 12) != 0 ? 0xf0000000u : 0);
    for (index = 0; index < 2; index++) {
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
}

static uint32_t test_local(const struct multip_module *retail, const struct multip_module *ours,
                           uint32_t scenarios)
{
    static struct local_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("local", "retail", number);
        run_local(retail, number, &s_retail);
        multip_test_context("local", "ours", number);
        run_local(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("local player scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf(" first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP local player creation/start: %lu scenarios, %lu differ\n",
        (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
