/* The CHAIN 0x19 display callback, including its leftover return value. */
struct track_fixture {
    uint32_t call_count;
    uint32_t chain_result;
    uint32_t copy_result;
    uint32_t draw_result;
    uint32_t memory_calls;
    uint32_t number;
    uint32_t log[32][6];
    uint32_t results[16];
    uint32_t state[16][8];
    uint8_t names[16][20];
    uint8_t view[0x60];
    uint8_t position[24];
    uint32_t local[7];
};
static struct track_fixture s_track;
static const struct multip_module *s_track_module;
static uint8_t *s_track_global;

typedef uint32_t (STDCALL *track_callback_fn)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *track_show_fn)(uint32_t);

static uint32_t *track_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_track.call_count >= 32) exit(2);
    entry = s_track.log[s_track.call_count++];
    entry[0] = tag;
    return entry;
}

static int32_t STDCALL track_copy(char *dest, const char *source)
{
    uint32_t *entry = track_log(1);
    entry[1] = dest == (char *)s_track_module->data + 0xf08;
    entry[2] = profile_hash(source);
    strcpy(dest, source);
    return (int32_t)s_track.copy_result;
}

static int32_t track_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t *entry = track_log(tag);
    entry[1] = code;
    entry[2] = (void *)callback == send_internal(s_track_module, 54);
    entry[3] = get_u32(s_track_module->data, 0xf20);
    entry[4] = get_u32(s_track_module->data, 0xf1c);
    return (int32_t)s_track.chain_result;
}

static int32_t STDCALL track_add(uint32_t code, track_callback_fn callback)
{
    return track_registration(2, code, callback);
}
static int32_t STDCALL track_remove(uint32_t code, track_callback_fn callback)
{
    return track_registration(3, code, callback);
}
static uint32_t STDCALL track_draw(uint32_t window, uint32_t a,
    uint32_t b, const char *name_text, uint32_t c)
{
    uint32_t *entry = track_log(4);
    entry[1] = window; entry[2] = a;
    entry[3] = b; entry[4] = profile_hash(name_text); entry[5] = c;
    return s_track.draw_result;
}

static int32_t STDCALL track_memory(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = track_log(5);
    uintptr_t source_address = (uintptr_t)source;
    uintptr_t global_address = (uintptr_t)s_track_global;
    uint32_t index;
    entry[1] = (uint8_t *)dest - s_track_global;
    entry[2] = source_address >= global_address && source_address < global_address + 0x1200 ?
        source_address - global_address : 0x80000000u;
    entry[3] = size;
    entry[4] = 2166136261u;
    for (index = 0; index < size; index++)
        entry[4] = (entry[4] ^ ((const uint8_t *)source)[index]) * 16777619u;
    memmove(dest, source, size);
    s_track.memory_calls++;
    return s_track.number % 7 == s_track.memory_calls;
}

static void track_snapshot(const struct multip_module *module,
    const uint8_t *global, uint32_t step, uint32_t return_value)
{
    uint32_t *state_fields = s_track.state[step];
    s_track.results[step] = return_value;
    state_fields[0] = get_u32(module->data, 0xf1c);
    state_fields[1] = get_u32(module->data, 0xf20);
    state_fields[2] = get_u32(module->data, M_PLAYER_MODE);
    state_fields[3] = get_u32(module->data, M_ACTIVE_PLAYER_ID);
    state_fields[4] = get_u32(global, 0x11b0);
    state_fields[5] = get_u32(global, 0x11dc);
    state_fields[6] = get_u32(global, 0x5dc);
    state_fields[7] = get_u32(global, 0xdc2);
    memcpy(s_track.names[step], module->data + 0xf08, 20);
}

static void run_tracking(const struct multip_module *module, uint32_t number,
                         struct track_fixture *results)
{
    uint8_t host[0x320] = {0};
    void *chain[9] = {0};
    uint8_t global[0x1200];
    uint8_t local[0x100] = {0};
    uint8_t object[32];
    uint8_t model[40];
    void *function;
    uint32_t random_state = number ^ 0x18234567u;
    uint32_t index;
    uint32_t window = random_u32(&random_state);
    uint32_t result;
    track_callback_fn callback = (track_callback_fn)send_internal(module, 54);

    memset(&s_track, 0, sizeof(s_track));
    s_track.number = number;
    s_track_module = module;
    s_track_global = global;
    memcpy(module->data, module->initial_data, 0x1000);
    for (index = 0; index < sizeof(global); index += 4)
        put_u32(global, index, random_u32(&random_state));
    put_u32(global, 0x119c, window);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)global);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)local);
    put_u32(local, 0, (uint32_t)(uintptr_t)"local track name");
    put_u32(local, 4, 0x1234);
    put_u32(module->data, M_PLAYER_MODE, random_u32(&random_state));
    put_u32(module->data, M_ACTIVE_PLAYER_ID, random_u32(&random_state));
    put_u32(module->data, 0xf20, number % 3);
    put_u32(module->data, 0xa10, number & 4 ? random_u32(&random_state) : number % 3);
    function = track_copy; memcpy(host + 0x2a8, &function, 4);
    function = track_draw; memcpy(host + 0x8c, &function, 4);
    function = track_memory; memcpy(host + 0x21c, &function, 4);
    chain[0x18 / 4] = (void *)track_add;
    chain[0x1c / 4] = (void *)track_remove;
    s_track.chain_result = number & 1 ? random_u32(&random_state) | 1 : 0;
    s_track.copy_result = random_u32(&random_state);
    s_track.draw_result = random_u32(&random_state);
    result = ((track_show_fn)send_internal(module, 53))(0x1234);
    track_snapshot(module, global, 0, result);
    result = ((track_show_fn)send_internal(module, 53))(0x1234);
    track_snapshot(module, global, 1, result);
    track_snapshot(module, global, 2, callback(0x19, window));
    track_snapshot(module, global, 3, callback(0x7777, window ^ 1));
    result = ((multip_worker_fn)send_internal(module, 51))();
    track_snapshot(module, global, 4, result);
    track_snapshot(module, global, 5, callback(0x19, window));
    s_track.chain_result = 0;
    track_snapshot(module, global, 6, callback(0x19, window));
    result = ((multip_worker_fn)send_internal(module, 52))();
    track_snapshot(module, global, 7, result);
    track_snapshot(module, global, 8, callback(0x19, window));
    for (index = 0; index < sizeof(object); index += 4)
        put_u32(object, index, random_u32(&random_state));
    for (index = 0; index < sizeof(model); index += 4)
        put_u32(model, index, random_u32(&random_state));
    put_u32(global, 0x11e0, (uint32_t)(uintptr_t)object);
    put_u32(global, 0x11e4, number & 16 ? (uint32_t)(uintptr_t)model : 0);
    result = ((multip_worker_fn)send_internal(module, 57))();
    track_snapshot(module, global, 9, result);
    for (index = 0; index < 7; index++)
        s_track.local[index] = index == 2 ? get_u32(local, 0x28) != 0 :
            get_u32(local, 0x20 + index * 4);
    put_u32(module->data, M_PLAYER_COUNT, number & 8 ? 0 : 1);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)local);
    put_u32(module->data, M_PLAYER_MODE, number % 3);
    put_u32(module->data, M_ACTIVE_PLAYER_ID, number & 2 ? 0x1234 : 0xdead);
    put_u32(module->data, 0xa58, (number / 3) % 3);
    result = ((multip_local_fn)send_internal(module, 55))() == local;
    track_snapshot(module, global, 10, result);
    put_u32(global, 0x5dc, (number & 1 ? 0x10000 : 0) | (number & 4 ? 1 : 0));
    put_u32(global, 0x268, number & 2);
    put_u32(module->data, 0xad4, number & 8);
    result = ((multip_worker_fn)send_internal(module, 56))();
    track_snapshot(module, global, 11, result);
    result = ((multip_worker_fn)send_internal(module, 58))();
    track_snapshot(module, global, 12, result);
    memcpy(s_track.view, global + 0x560, sizeof(s_track.view));
    memcpy(s_track.position, global + 0xd74, sizeof(s_track.position));
    memcpy(results, &s_track, sizeof(*results));
}

static uint32_t test_tracking(const struct multip_module *retail,
                              const struct multip_module *ours, uint32_t scenarios)
{
    static struct track_fixture s_retail;
    static struct track_fixture s_ours;
    uint32_t number;
    uint32_t bad = 0;
    for (number = 0; number < scenarios; number++) {
        run_tracking(retail, number, &s_retail);
        run_tracking(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("tracking scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP track-name display: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
