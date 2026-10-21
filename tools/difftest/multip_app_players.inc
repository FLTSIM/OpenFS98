/* Player-type application handlers: host/client paths, duplicate IDs, fallback
 * publication errors, capacity wrapping and queued response bytes. */
struct app_players_results {
    struct weather_fixture weather;
    uint8_t records[4][256];
    uint32_t returns[16], state[16][12], signals[16];
};
static struct app_players_results s_app_players;
static const struct multip_module *s_app_players_module;
static int32_t STDCALL app_players_memset(void *dest, int32_t value, uint32_t size)
{
    uint32_t *entry = weather_log(7); entry[1] = value; entry[2] = size;
    memset(dest, value, size); return s_weather.calls == s_weather.fail_call;
}
static int32_t STDCALL app_players_get_name(void *object, uint32_t id, void *name, uint32_t *size)
{
    uint32_t *entry = weather_log(8); entry[1] = directplay_identity(object);
    entry[2] = id; entry[3] = name != NULL; entry[4] = *size; *size = 32;
    if (name) { memset(name, 0, 32); put_u32(name, 0, 16); }
    return s_weather.calls == s_weather.fail_call ? -1 : name ? 0 : (int32_t)0x8877001eu;
}
static int32_t STDCALL app_players_set_name(void *object, uint32_t id, void *name, uint32_t flags)
{
    uint32_t *entry = weather_log(9); entry[1] = directplay_identity(object);
    entry[2] = id; entry[3] = flags; entry[4] = profile_hash((const char *)(uintptr_t)get_u32(name, 12));
    return s_weather.calls == s_weather.fail_call ? -1 : 0;
}
static int32_t STDCALL app_players_get_description(void *object, void *description, uint32_t *size)
{
    uint32_t *entry = weather_log(10); entry[1] = directplay_identity(object);
    entry[2] = description != NULL; entry[3] = *size; *size = 80;
    if (description) memset(description, 0x49, 80);
    return s_weather.calls == s_weather.fail_call ? -1 : description ? 0 : (int32_t)0x8877001eu;
}
static int32_t STDCALL app_players_set_description(void *object, void *description, uint32_t flags)
{
    uint32_t *entry = weather_log(11); entry[1] = directplay_identity(object);
    entry[2] = get_u32(description, 0x40); entry[3] = flags;
    return s_weather.calls == s_weather.fail_call ? -1 : s_weather.number & 0x80 ? (int32_t)0x80004001u : 0;
}
static uint32_t STDCALL app_players_confirm(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
    uint32_t e, uint32_t f, uint32_t g, uint32_t h)
{
    uint32_t *entry = weather_log(12); entry[1] = a; entry[2] = b; entry[3] = c; entry[4] = d;
    entry[5] = e | f | g | h; return 0x40;
}
static int32_t STDCALL app_players_title(void *aircraft, uint32_t kind, char *out, uint32_t size)
{
    uint32_t *entry = weather_log(13); entry[1] = aircraft == s_weather.global + 0x11e0;
    entry[2] = kind; entry[3] = size; strcpy(out, "application aircraft"); return -1;
}
static uint32_t STDCALL app_players_strlen(const char *text)
{
    uint32_t *entry = weather_log(14); entry[1] = profile_hash(text); return strlen(text);
}
static int32_t STDCALL app_players_strcpy(char *dest, const char *source)
{
    uint32_t *entry = weather_log(15); entry[1] = profile_hash(source); strcpy(dest, source); return -1;
}
static void app_players_snapshot(uint32_t step, uint32_t result)
{
    static const uint32_t s_offsets[] = {0xaa8,0x998,0x99c,0x9a0,0x9a4,0x984,0xa64};
    uint32_t index;
    s_app_players.returns[step] = result;
    for (index = 0; index < 7; index++) s_app_players.state[step][index] = get_u32(s_app_players_module->data, s_offsets[index]);
    for (index = 0; index < 4; index++) s_app_players.state[step][index + 7] = get_u32(s_app_players.records[index], 8);
    s_app_players.state[step][11] = get_u32(s_weather.global, 0x832);
}
static void run_app_players(const struct multip_module *module, uint32_t number, struct app_players_results *results)
{
    uint8_t host[0x2c0] = {0}, packet[16] = {0};
    void *util[42] = {0}, *aircraft[13] = {0}, *function;
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t index, step, byte, random_state = number ^ 0x94216738u;
    static const uint32_t s_ids[] = {0x9000,0x9001,0xdead,0};
    typedef int32_t (STDCALL *app_handler_fn)(uint32_t, const void *, uint32_t);

    if (!event) exit(2);
    memset(&s_app_players, 0, sizeof(s_app_players)); memset(&s_weather, 0, sizeof(s_weather)); memset(queues, 0, sizeof(queues));
    s_app_players_module = module; s_weather.number = number;
    s_weather.fail_call = number % 7 == 0 ? 1 + (number / 7) % 180 : 0;
    s_weather.fail_free = number % 11 == 0 ? 1 + (number / 11) % 8 : 0;
    memcpy(module->data, module->initial_data, 0x1000);
    function = weather_alloc; memcpy(host + 0x20c, &function, 4);
    function = weather_free; memcpy(host + 0x214, &function, 4);
    function = weather_copy; memcpy(host + 0x21c, &function, 4);
    function = app_players_memset; memcpy(host + 0x218, &function, 4);
    function = app_players_strcpy; memcpy(host + 0x2a8, &function, 4);
    function = app_players_strlen; memcpy(host + 0x2b8, &function, 4);
    util[0xa4 / 4] = app_players_confirm; aircraft[0x30 / 4] = app_players_title;
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x54 / 4] = app_players_get_name; s_directplay_vtable[0x78 / 4] = app_players_set_name;
    s_directplay_vtable[0x58 / 4] = app_players_get_description; s_directplay_vtable[0x7c / 4] = app_players_set_description;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_weather.global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_CONNECTED, 1);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_app_players.records[0]);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_app_players.records);
    put_u32(module->data, M_PLAYER_COUNT, number % 5);
    for (index = 0; index < 4; index++) {
        put_u32(s_app_players.records[index], 0, (uint32_t)(uintptr_t)"application peer");
        put_u32(s_app_players.records[index], 4, index == 0 ? 0x9000 : 0x9001);
        put_u32(s_app_players.records[index], 0xc, index == 0 ? 1 : 0);
    }
    for (index = 0; index < sizeof(s_weather.global); index++) s_weather.global[index] = random_u32(&random_state);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    put_u32(module->data, 0xf00, number % 4);
    for (step = 0; step < 16; step++) {
        uint32_t id = s_ids[(number + step) % 4], result, entry = step < 13 ? 122 + step : step == 13 ? 135 : step == 14 ? 126 : 131;
        /* Each handler gets independent boundary state; the heap, ordered
         * imports, queues and post-call snapshots retain every partial effect. */
        put_u32(module->data, 0xaa8, (number / 4) % 3);
        put_u32(module->data, 0xa64, (number / 12 + step) % 3);
        put_u32(module->data, 0x984, random_u32(&random_state));
        for (index = 0; index < 4; index++) put_u32(s_app_players.records[index], 8, (number / 36 + step + index) % 5);
        for (index = 0; index < 4; index++) put_u32(module->data, 0x998 + index * 4, number & 0x100 ? 0xffffffffu : index + 2);
        if ((number / 180) % 3 == 0) put_u32(module->data, 0x9a0, get_u32(module->data, 0x998));
        if ((number / 540) % 3 == 0) put_u32(module->data, 0x9a4, get_u32(module->data, 0x99c));
        put_u32(packet, 8, step == 14 ? 0xdead : s_ids[(number / 4 + step) % 4]);
        if (step == 14) id = 0x9000;
        if (entry == 135) result = ((message_id_fn)send_internal(module, entry))(id);
        else result = ((app_handler_fn)send_internal(module, entry))(id, packet, number % 17);
        app_players_snapshot(step, result);
        s_app_players.signals[step] = WaitForSingleObject(event, 0);
    }
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
    memcpy(&s_app_players.weather, &s_weather, sizeof(s_weather)); memcpy(results, &s_app_players, sizeof(*results));
    for (index = 0; index < s_weather.blocks; index++) {
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t normalized = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (normalized != 0xffffffffu) put_u32(results->weather.heap[index], byte, normalized);
        }
        if (s_weather.sizes[index] == 32 && get_u32(s_weather.heap[index], 0) == 16)
            put_u32(results->weather.heap[index], 12, get_u32(s_weather.heap[index], 12) != 0 ? 0xf0000000u : 0);
    }
}
static uint32_t test_app_players(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct app_players_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("app_players", "retail", number); run_app_players(retail, number, &s_retail);
        multip_test_context("app_players", "ours", number); run_app_players(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("application player scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP application player types: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
