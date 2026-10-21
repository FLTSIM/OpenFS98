/* System notifications preserve partial player setup and host migration. */
struct system_results {
    struct player_fixture players;
    uint32_t groups[2], host, signals, queues[2];
};
static int32_t STDCALL system_strcmp(const char *a, const char *b)
{
    uint32_t *entry = player_log(25);
    entry[1] = profile_hash(a); entry[2] = profile_hash(b);
    return strcmp(a, b);
}
static uint32_t STDCALL system_confirm(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
    uint32_t e, uint32_t f, uint32_t g, uint32_t h)
{
    uint32_t *entry = player_log(26);
    entry[1] = a; entry[2] = b; entry[3] = c; entry[4] = d; entry[5] = e | f | g | h;
    return 0x40;
}
static void run_system(const struct multip_module *module, uint32_t number, struct system_results *results)
{
    uint8_t host[0x320] = {0}, message[64] = {0};
    void *aircraft[16] = {0}, *chain[9] = {0}, *util[42] = {0}, *function;
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t index, random_state = number ^ 0x76241583u, name_fields[4] = {16,0,0,0};
    typedef int32_t (STDCALL *system_notification_fn)(const void *, uint32_t);
    typedef int32_t (STDCALL *system_data_fn)(const void *, uint32_t, uint32_t);

    if (!event) exit(2);
    memset(results, 0, sizeof(*results)); memset(&s_players, 0, sizeof(s_players)); memset(&s_local, 0, sizeof(s_local)); memset(queues, 0, sizeof(queues));
    s_players.number = number; s_players_module = module; s_players.ticks = random_u32(&random_state);
    memcpy(module->data, module->initial_data, 0x1000);
    function = player_alloc; memcpy(host + 0x20c, &function, 4);
    function = player_free; memcpy(host + 0x214, &function, 4);
    function = local_memset; memcpy(host + 0x218, &function, 4);
    function = player_copy; memcpy(host + 0x21c, &function, 4);
    function = player_strcpy; memcpy(host + 0x2a8, &function, 4);
    function = system_strcmp; memcpy(host + 0x2bc, &function, 4);
    function = player_append; memcpy(host + 0x2f8, &function, 4);
    function = player_remove; memcpy(host + 0x300, &function, 4);
    function = player_ticks; memcpy(host + 0x38, &function, 4);
    aircraft[8] = player_select; aircraft[9] = player_release; aircraft[12] = player_title;
    aircraft[13] = player_model; aircraft[14] = player_unload; chain[8] = player_dispatch;
    util[0xa4 / 4] = system_confirm;
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x58 / 4] = player_description; s_directplay_vtable[0x7c / 4] = player_set_description;
    s_directplay_vtable[0x54 / 4] = local_get_name; s_directplay_vtable[0x78 / 4] = local_set_name;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_players.global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_CONNECTED, 1); put_u32(module->data, 0xaa8, number & 1);
    put_u32(module->data, M_PLAYER_COUNT, 1); put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_players.banks[0]);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_players.banks[0][0]);
    put_u32(s_players.banks[0][0], 0, (uint32_t)(uintptr_t)"system local");
    put_u32(s_players.banks[0][0], 4, 0x8000); put_u32(s_players.banks[0][0], 8, number % 5);
    put_u32(s_players.banks[0][0], 0xc, 1);
    put_u32(module->data, 0x160, random_u32(&random_state));
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 4; index++) put_u32(module->data, 0x998 + 4 * index, random_u32(&random_state));
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    /* Group names come from the retail initialized image, not guessed labels. */
    name_fields[2] = (uint32_t)(uintptr_t)(number % 3 == 2 ? "unknown" :
        (const char *)module->data + (number % 3 == 0 ? 0x1b0 : 0x1c8));
    player_snapshot(0, ((player_remote_fn)send_internal(module, 115))(name_fields, 0xa001));
    put_u32(message, 4, 0); put_u32(message, 8, 0xa002); memcpy(message + 0x18, name_fields, 16);
    player_snapshot(1, ((system_notification_fn)send_internal(module, 116))(message, number % 65));
    name_fields[2] = (uint32_t)(uintptr_t)"network peer";
    put_u32(message, 4, 1); put_u32(message, 8, 0x8001); memcpy(message + 0x18, name_fields, 16);
    player_snapshot(2, ((system_notification_fn)send_internal(module, 116))(message, 0));
    player_snapshot(3, ((system_notification_fn)send_internal(module, 116))(message, 64));
    /* The creation snapshots above retain stale-list pointer behavior. Test
     * destruction independently with the current local record, so a missing
     * local-ID guard produces an observable removal rather than fixture abort. */
    put_u32(module->data, M_LOCAL_PLAYER, get_u32(module->data, M_PLAYERS));
    put_u32(message, 8, number % 3 == 0 ? 0x8000 : number % 3 == 1 ? 0x8001 : 0xdead);
    player_snapshot(4, ((system_data_fn)send_internal(module, 118))(message, 0, random_u32(&random_state)));
    player_snapshot(5, ((system_notification_fn)send_internal(module, 117))(message, 0));
    put_u32(message, 4, (number / 3) % 4);
    player_snapshot(6, ((system_notification_fn)send_internal(module, 117))(message, 64));
    player_snapshot(7, ((system_notification_fn)send_internal(module, 119))(message, 0));
    put_u32(message, 4, 2 + number % 3); put_u32(message, 0x20, 0);
    player_snapshot(8, ((system_notification_fn)send_internal(module, 116))(message, 0));
    results->groups[0] = get_u32(module->data, 0xaa0); results->groups[1] = get_u32(module->data, 0xaa4);
    results->host = get_u32(module->data, 0xaa8); results->signals = WaitForSingleObject(event, 0);
    for (index = 0; index < 2; index++) {
        results->queues[index] = queues[index].count;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
    memcpy(&results->players, &s_players, sizeof(s_players));
    for (index = 0; index < s_players.block_count; index++) if (s_local.name_blocks[index])
        put_u32(results->players.heap[index], 12, get_u32(s_players.heap[index], 12) != 0 ? 0xf0000000u : 0);
}
static uint32_t test_system(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct system_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("system", "retail", number); run_system(retail, number, &s_retail);
        multip_test_context("system", "ours", number); run_system(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("system scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP DirectPlay system notifications: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
