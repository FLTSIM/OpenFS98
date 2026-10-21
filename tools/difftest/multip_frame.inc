/* Exhaust the CHAIN 0x14 switch, including its leftover eax results. */
struct frame_results {
    struct player_fixture players;
    uint32_t returns[264];
    uint32_t state[264][11];
    uint32_t queues[2][2];
    uint32_t signal;
};
static uint32_t s_frame_chain_result;

static uint32_t STDCALL frame_strlen(const char *text)
{
    uint32_t *entry = player_log(28);
    entry[1] = profile_hash(text);
    return strlen(text);
}
static int32_t frame_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t *entry = player_log(tag);
    entry[1] = code;
    entry[2] = (void *)callback == send_internal(s_players_module, 85);
    entry[3] = get_u32(s_players_module->data, 0x954);
    return (int32_t)s_frame_chain_result;
}
static int32_t STDCALL frame_add(uint32_t code, track_callback_fn callback)
{ return frame_registration(29, code, callback); }
static int32_t STDCALL frame_remove(uint32_t code, track_callback_fn callback)
{ return frame_registration(30, code, callback); }

static void frame_snapshot(struct frame_results *results, uint32_t step, uint32_t return_value)
{
    uint8_t *data = s_players_module->data;
    const uint8_t *local = s_players.banks[0][0];
    uint32_t index;
    results->returns[step] = return_value;
    results->state[step][0] = get_u32(data, 0x954);
    results->state[step][1] = get_u32(data, 0xad4);
    results->state[step][2] = get_u32(local, 0xcc);
    for (index = 0; index < 7; index++)
        results->state[step][3 + index] = get_u32(local, 0x20 + index * 4);
    results->state[step][10] = s_players.block_count;
}
static void run_frame(const struct multip_module *module, uint32_t number, struct frame_results *results)
{
    uint8_t host[0x320] = {0};
    void *chain[9] = {0}, *aircraft_linkage[16] = {0};
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t index, step, result, random_state = number ^ 0x75618324u;
    void *function;
    track_callback_fn callback = (track_callback_fn)send_internal(module, 85);

    if (event == NULL) exit(2);
    memset(results, 0, sizeof(*results));
    memset(&s_players, 0, sizeof(s_players));
    memset(queues, 0, sizeof(queues));
    s_players_module = module; s_players.number = number;
    memcpy(module->data, module->initial_data, 0x1000);
    function = player_alloc; memcpy(host + 0x20c, &function, 4);
    function = player_free; memcpy(host + 0x214, &function, 4);
    function = player_copy; memcpy(host + 0x21c, &function, 4);
    function = local_strcpy; memcpy(host + 0x2a8, &function, 4);
    function = frame_strlen; memcpy(host + 0x2b8, &function, 4);
    aircraft_linkage[0x30 / 4] = (void *)player_title;
    chain[0x18 / 4] = (void *)frame_add;
    chain[0x1c / 4] = (void *)frame_remove;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_players.global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft_linkage);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_players.banks[0][0]);
    put_u32(module->data, 0x160, random_u32(&random_state));
    put_u32(module->data, 0xad4, random_u32(&random_state));
    put_u32(module->data, 0x954, number % 3);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    put_u32(s_players.global, 0x11e0, (uint32_t)(uintptr_t)s_players.headers[0]);
    put_u32(s_players.global, 0x11e4, (uint32_t)(uintptr_t)s_players.models[0]);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    s_frame_chain_result = number % 3 == 0 ? 0x80004005u : 0;
    frame_snapshot(results, 0, ((multip_worker_fn)send_internal(module, 86))());
    frame_snapshot(results, 1, ((multip_worker_fn)send_internal(module, 86))());
    frame_snapshot(results, 2, ((multip_worker_fn)send_internal(module, 87))());
    s_frame_chain_result = 0;
    frame_snapshot(results, 3, ((multip_worker_fn)send_internal(module, 87))());
    frame_snapshot(results, 4, ((multip_worker_fn)send_internal(module, 87))());
    s_frame_chain_result = number & 1 ? 0 : 2;
    frame_snapshot(results, 5, ((multip_worker_fn)send_internal(module, 86))());
    for (step = 0; step < 256; step++) {
        for (index = 4; index <= 0x18; index += 4)
            put_u32(s_players.headers[0], index, random_u32(&random_state));
        s_players.global[0x609] = (uint8_t)(step * 7);
        result = callback(step, random_u32(&random_state));
        frame_snapshot(results, 6 + step, result);
    }
    frame_snapshot(results, 262, callback(0xffffffffu, 0));
    frame_snapshot(results, 263, callback(0x80000021u, 0));
    results->signal = WaitForSingleObject(event, 0);
    memcpy(&results->players, &s_players, sizeof(s_players));
    for (index = 0; index < 2; index++) {
        results->queues[index][0] = queues[index].count;
        results->queues[index][1] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
}
static uint32_t test_frame(const struct multip_module *retail, const struct multip_module *ours,
                           uint32_t scenarios)
{
    static struct frame_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("frame", "retail", number);
        run_frame(retail, number, &s_retail);
        multip_test_context("frame", "ours", number);
        run_frame(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("session frame scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf(" first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP session CHAIN 0x14: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
