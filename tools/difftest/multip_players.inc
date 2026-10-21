/* Stable-address heap/list banks let both DLLs expose the same pointer bugs. */
struct player_fixture {
    uint32_t number, call_count, block_count, model_count, bank, ticks, signal, queued[2];
    uint32_t log[256][6], sizes[32], live[32];
    uint8_t heap[32][128], banks[2][6][256];
    uint8_t headers[12][36], models[12][128], global[0x1200];
    uint32_t returns[12], state[12][9], record_hash[12][6];
};
static struct player_fixture s_players;
static const struct multip_module *s_players_module;
static uint32_t *player_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_players.call_count >= 256) exit(2);
    entry = s_players.log[s_players.call_count++];
    entry[0] = tag;
    return entry;
}
static int player_fail(void) { return s_players.number % 67 == s_players.call_count; }
static uint32_t player_address(const void *address)
{
    uintptr_t address_value = (uintptr_t)address, base = (uintptr_t)s_players_module->data;
    return address_value >= base && address_value < base + 0x1000 ?
        0xe000 + address_value - base : (uint32_t)address_value;
}
static int32_t STDCALL player_alloc(int32_t kind, uint32_t size, void **out, int32_t zero)
{
    uint32_t *entry = player_log(1), block_index = s_players.block_count;
    entry[1] = kind; entry[2] = size; entry[3] = zero;
    if (player_fail()) { *out = NULL; return 1; }
    if (block_index >= 32 || size > 128) exit(2);
    *out = s_players.heap[block_index];
    memset(*out, zero ? 0 : 0x97, size);
    s_players.sizes[block_index] = size; s_players.live[block_index] = 1;
    s_players.block_count++;
    return 0;
}
static int32_t STDCALL player_free(void **block_out)
{
    uint32_t *entry = player_log(2), index;
    entry[1] = player_address(*block_out);
    if (player_fail()) return 1;
    for (index = 0; index < s_players.block_count; index++)
        if (*block_out == s_players.heap[index]) s_players.live[index] = 0;
    *block_out = NULL;
    return 0;
}
static int32_t STDCALL player_copy(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = player_log(3), index;
    entry[1] = player_address(dest); entry[2] = size; entry[3] = 2166136261u;
    for (index = 0; index < size; index++)
        entry[3] = (entry[3] ^ ((const uint8_t *)source)[index]) * 16777619u;
    memmove(dest, source, size);
    return player_fail();
}
static int32_t STDCALL player_strcpy(char *dest, const char *source)
{
    uint32_t *entry = player_log(4);
    entry[1] = player_address(dest); entry[2] = profile_hash(source);
    strcpy(dest, source); return player_fail();
}
static void *STDCALL player_append(void *list)
{
    uint32_t *entry = player_log(5), count = get_u32(list, 0);
    uint32_t next_bank = s_players.bank ^ 1;
    if (count >= 6) exit(2);
    entry[1] = player_address(list); entry[2] = count;
    memcpy(s_players.banks[next_bank], s_players.banks[s_players.bank], count * 256);
    memset(s_players.banks[next_bank][count], 0, 256);
    s_players.bank = next_bank;
    put_u32(list, 0, count + 1); put_u32(list, 4, 6);
    put_u32(list, 0x18, (uint32_t)(uintptr_t)s_players.banks[next_bank]);
    return s_players.banks[next_bank][count];
}
static void STDCALL player_remove(void *list, void *record)
{
    uint32_t *entry = player_log(6), count = get_u32(list, 0);
    uint8_t *data = (uint8_t *)(uintptr_t)get_u32(list, 0x18);
    uint32_t index = ((uint8_t *)record - data) / 256;
    entry[1] = count; entry[2] = index;
    if (index >= count) exit(2);
    memmove(record, (uint8_t *)record + 256, (count - index - 1) * 256);
    put_u32(list, 0, count - 1);
}
static int32_t STDCALL player_title(void *aircraft, uint32_t kind, char *title, uint32_t size)
{
    uint32_t *entry = player_log(7);
    entry[1] = aircraft == s_players.global + 0x11e0; entry[2] = kind; entry[3] = size;
    strcpy(title, "fixture aircraft"); return player_fail();
}
static int32_t STDCALL player_select(void *aircraft, const char *title)
{
    uint32_t *entry = player_log(8), index = s_players.model_count++;
    if (index >= 12) exit(2);
    entry[1] = profile_hash(title);
    put_u32(aircraft, 0, (uint32_t)(uintptr_t)s_players.headers[index]);
    put_u32(aircraft, 4, (uint32_t)(uintptr_t)s_players.models[index]);
    put_u32(s_players.headers[index], 0x18, 128);
    put_u32(s_players.headers[index], 4, 0x12345678);
    put_u32(s_players.headers[index], 8, 64);
    put_u32(s_players.models[index], 0, 128);
    return !player_fail();
}
static int32_t STDCALL player_model(void *aircraft, uint32_t kind)
{
    uint32_t *entry = player_log(9); (void)aircraft; entry[1] = kind; return !player_fail();
}
static int32_t STDCALL player_unload(void *aircraft)
{ (void)aircraft; player_log(10); return !player_fail(); }
static int32_t STDCALL player_release(void *aircraft)
{ (void)aircraft; player_log(11); return !player_fail(); }
static uint32_t STDCALL player_dispatch(uint32_t code, uint32_t event, void *list)
{
    uint32_t *entry = player_log(12);
    entry[1] = code; entry[2] = event; entry[3] = player_address(list);
    entry[4] = get_u32(list, 0); entry[5] = player_address((void *)(uintptr_t)get_u32(list, 4));
    return 0xabcdef01;
}
static int32_t STDCALL player_description(void *object, void *desc, uint32_t *size)
{
    uint32_t *entry = player_log(13); (void)object;
    entry[1] = desc != NULL; entry[2] = *size; *size = 80;
    if (desc == NULL) return player_fail() ? -1 : (int32_t)0x8877001eu;
    memset(desc, 0, 80); return player_fail();
}
static int32_t STDCALL player_set_description(void *object, void *desc, uint32_t flags)
{
    uint32_t *entry = player_log(14); (void)object;
    entry[1] = get_u32(desc, 0x40); entry[2] = flags;
    return player_fail() ? -1 : s_players.number & 1 ? (int32_t)0x80004001u : 0;
}
static int32_t STDCALL player_enum(void *object, const GUID *instance,
    session_enum_callback callback, void *context, uint32_t flags)
{
    static const uint32_t s_errors[] = {0,1,0x80004005u,0x80070057u,0xffffffffu};
    uint32_t *entry = player_log(22), name_fields[4] = {16,0,0,0};
    uint32_t index;
    int32_t keep_going;
    entry[1] = directplay_identity(object); entry[2] = player_address(instance);
    entry[3] = context != NULL; entry[4] = flags;
    entry[5] = callback(0x100, 0, NULL, 0, context);
    name_fields[2] = (uint32_t)(uintptr_t)"existing peer";
    keep_going = callback(0x5678, 1, name_fields, 0, context);
    entry = player_log(23); entry[1] = keep_going;
    entry[2] = callback(0x200, 2, NULL, 0, context);
    name_fields[2] = (uint32_t)(uintptr_t)"enumerated peer";
    for (index = 0; index < 3 && keep_going != 0; index++) {
        keep_going = callback(0x8001 + index, 1, name_fields, 0x40, context);
        entry = player_log(24); entry[1] = index; entry[2] = keep_going;
    }
    return (int32_t)s_errors[s_players.number % 5];
}
static int32_t STDCALL player_group_add(void *object, uint32_t group, uint32_t player_address_value)
{
    uint32_t *entry = player_log(25);
    entry[1] = directplay_identity(object); entry[2] = group; entry[3] = player_address_value;
    return (int32_t)0x80004005u;
}
static int32_t STDCALL player_group_remove(void *object, uint32_t group, uint32_t player_address_value)
{
    uint32_t *entry = player_log(26);
    entry[1] = directplay_identity(object); entry[2] = group; entry[3] = player_address_value;
    return (int32_t)0x80070057u;
}
static uint32_t STDCALL player_ticks(void)
{
    player_log(27); return ++s_players.ticks;
}

static void player_snapshot(uint32_t step, uint32_t result)
{
    static const uint32_t s_offsets[] = {0x660,0xa3c,0x9a0,0x9a4,0xa5c,0xa60,0xef8,0xefc,0xf1c};
    uint32_t index;
    s_players.returns[step] = result;
    for (index = 0; index < 9; index++)
        s_players.state[step][index] = get_u32(s_players_module->data, s_offsets[index]);
    for (index = 0; index < get_u32(s_players_module->data, M_PLAYER_COUNT); index++) {
        const uint8_t *record = (const uint8_t *)(uintptr_t)get_u32(s_players_module->data, M_PLAYERS) + index * 256;
        uint32_t byte, hash = 2166136261u;
        uint32_t name_address = player_address((const void *)(uintptr_t)get_u32(record, 0));
        for (byte = 0; byte < 256; byte++)
            hash = (hash ^ (byte < 4 ? (uint8_t)(name_address >> (byte * 8)) : record[byte])) * 16777619u;
        s_players.record_hash[step][index] = hash;
    }
}

typedef int32_t (STDCALL *player_remote_fn)(const void *, uint32_t);
typedef void *(*player_append_fn)(void);
static void run_players(const struct multip_module *module, uint32_t number, struct player_fixture *results)
{
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint8_t host[0x320] = {0}; void *aircraft_linkage[16] = {0}, *chain[9] = {0};
    uint32_t name_fields[4] = {16,0,0,0}, index, random_state = number ^ 0x87234516u;
    void *function, *record;
    if (event == NULL) exit(2);
    memset(queues, 0, sizeof(queues));
    memset(&s_players, 0, sizeof(s_players)); s_players.number = number; s_players_module = module;
    memcpy(module->data, module->initial_data, 0x1000);
    function = player_alloc; memcpy(host + 0x20c, &function, 4);
    function = player_free; memcpy(host + 0x214, &function, 4);
    function = player_copy; memcpy(host + 0x21c, &function, 4);
    function = player_strcpy; memcpy(host + 0x2a8, &function, 4);
    function = player_append; memcpy(host + 0x2f8, &function, 4);
    function = player_remove; memcpy(host + 0x300, &function, 4);
    function = player_ticks; memcpy(host + 0x38, &function, 4);
    aircraft_linkage[8] = (void *)player_select; aircraft_linkage[9] = (void *)player_release;
    aircraft_linkage[12] = (void *)player_title; aircraft_linkage[13] = (void *)player_model; aircraft_linkage[14] = (void *)player_unload;
    chain[8] = (void *)player_dispatch;
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x58 / 4] = (void *)player_description;
    s_directplay_vtable[0x7c / 4] = (void *)player_set_description;
    s_directplay_vtable[0x30 / 4] = (void *)player_enum;
    s_directplay_vtable[0x0c / 4] = (void *)player_group_add;
    s_directplay_vtable[0x1c / 4] = (void *)player_group_remove;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_players.global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft_linkage);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_CONNECTED, 1); put_u32(module->data, 0xaa8, number & 2);
    put_u32(module->data, M_LOCAL_PLAYER, 0); put_u32(module->data, M_PLAYER_COUNT, 0);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_players.banks[0]);
    put_u32(module->data, M_PLAYER_MODE, 1); put_u32(module->data, M_ACTIVE_PLAYER_ID, 0x2001);
    for (index = 0; index < 4; index++) put_u32(module->data, 0x998 + index * 4, random_u32(&random_state));
    put_u32(module->data, 0x160, random_u32(&random_state));
    put_u32(module->data, 0xaa0, random_u32(&random_state));
    put_u32(module->data, 0xaa4, random_u32(&random_state));
    put_u32(module->data, 0xac0, number % 3);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    s_players.ticks = number & 1 ? 0xffffffffu : random_u32(&random_state);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    for (index = 0; index < 36; index++) s_players.global[0x560 + index] = random_u32(&random_state);
    name_fields[2] = (uint32_t)(uintptr_t)"remote name";
    player_snapshot(0, ((player_remote_fn)send_internal(module, 75))(name_fields, 0x2001));
    if (get_u32(module->data, M_PLAYER_COUNT) != 0)
        put_u32((uint8_t *)(uintptr_t)get_u32(module->data, M_PLAYERS), 8, number % 3);
    player_snapshot(1, ((player_remote_fn)send_internal(module, 75))(name_fields, 0x2002));
    player_snapshot(2, ((player_remote_fn)send_internal(module, 75))(name_fields, 0x2001));
    player_snapshot(3, ((message_id_fn)send_internal(module, 77))(0x2002));
    player_snapshot(4, ((message_id_fn)send_internal(module, 77))(0x2001));
    player_snapshot(5, ((multip_worker_fn)send_internal(module, 78))());
    put_u32(module->data, M_LOCAL_PLAYER, 0);
    /* Clear partial survivors before independent append/local-removal cases. */
    put_u32(module->data, M_PLAYER_COUNT, 0);
    record = ((player_append_fn)send_internal(module, 73))(); put_u32(record, 4, 0x1234);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)record);
    record = ((player_append_fn)send_internal(module, 73))(); put_u32(record, 4, 0x5678);
    player_snapshot(6, get_u32(module->data, M_LOCAL_PLAYER));
    player_snapshot(7, ((metadata_collision_fn)send_internal(module, 74))(record));
    put_u32(module->data, M_LOCAL_PLAYER, get_u32(module->data, M_PLAYERS));
    put_u32((uint8_t *)(uintptr_t)get_u32(module->data, M_LOCAL_PLAYER), 0xc, 1);
    player_snapshot(8, number & 1 ? ((message_id_fn)send_internal(module, 77))(0x1234) :
        ((multip_worker_fn)send_internal(module, 76))());
    player_snapshot(9, ((multip_worker_fn)send_internal(module, 81))());
    /* Membership is an independent helper case with a valid local record.
     * The enumeration snapshot above already captures a lost/stale pointer. */
    put_u32(module->data, M_LOCAL_PLAYER, get_u32(module->data, M_PLAYERS));
    player_snapshot(10, ((multip_worker_fn)send_internal(module, 82))());
    player_snapshot(11, ((message_id_fn)send_internal(module, 83))(number & 4 ? 0x5678 : 0x9876));
    s_players.signal = WaitForSingleObject(event, 0);
    for (index = 0; index < 2; index++) {
        s_players.queued[index] = queues[index].count;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    memcpy(results, &s_players, sizeof(*results));
    CloseHandle(event);
}
static uint32_t test_players(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct player_fixture s_retail, s_ours; uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("players", "retail", number);
        run_players(retail, number, &s_retail);
        multip_test_context("players", "ours", number);
        run_players(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("player scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf(" first byte %lu: retail %u ours %u\n", (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            } bad++;
        }
    }
    printf("MULTIP player lifecycle helpers: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad); return bad;
}
