/* Reassemble model chunks, publish the cache and replace the peer aircraft. */
struct model_receive_results {
    struct texture_results texture;
    uint8_t player[256];
    uint32_t states[12][6];
};
static uint32_t s_title_mode;
static uint8_t s_title_headers[2][64], s_title_models[2][128];
static void *s_title_aircraft;
static int32_t STDCALL title_select(void *object, const char *title)
{
    uint32_t which = object == s_title_aircraft, *entry = weather_log(46);
    entry[1] = which; entry[2] = profile_hash(title);
    if (s_weather.number % 3 != 0) return 0;
    put_u32(object, 0, (uint32_t)(uintptr_t)s_title_headers[which]);
    put_u32(object, 4, (uint32_t)(uintptr_t)s_title_models[which]);
    put_u32(object, 8, 0x12340000u + which);
    return which && (s_weather.number & 0x10) ? 0 : 1;
}
static int32_t STDCALL title_model(void *object, uint32_t kind)
{
    uint32_t *entry = weather_log(47); entry[1] = object == s_title_aircraft; entry[2] = kind;
    return s_weather.number & 0x20 ? 0 : 1;
}
static int32_t STDCALL title_release(void *object)
{
    uint32_t *entry = weather_log(48); entry[1] = object == s_title_aircraft;
    return s_weather.number & 0x40 ? 0 : 1;
}
static int32_t STDCALL title_exists(const char *path)
{
    uint32_t *entry = weather_log(49); entry[1] = profile_hash(path);
    return s_weather.number % 3 == 1 ? (s_weather.number & 0x100 ? -1 : 1) : 0;
}
static void run_model_receive(const struct multip_module *module, uint32_t number,
    struct model_receive_results *results)
{
    uint8_t host[0x320] = {0}, packet[160] = {0}, player[256] = {0}, model[108];
    void *aircraft[41] = {0};
    struct send_queue queues[2];
    HANDLE event = NULL;
    uint32_t index, step, random_state = number ^ 0x4921358u;
    typedef int32_t (STDCALL *packet_fn)(uint32_t, const void *, uint32_t);
    typedef void *(STDCALL *find_fn)(uint32_t);
    memset(results, 0, sizeof(*results)); memset(&s_weather, 0, sizeof(s_weather));
    memset(&s_texture, 0, sizeof(s_texture)); memset(s_cache_cursors, 0, sizeof(s_cache_cursors));
    s_texture_module = module; s_command_module = module; s_weather.number = number;
    s_cache_reads = s_cache_writes = 0;
    s_weather.fail_call = number % 3 == 0 ? 1 + number / 3 % 100 : 0;
    s_weather.fail_free = number % 7 == 0 ? 1 + number / 7 % 12 : 0;
    s_weather.blocks = 2; s_weather.sizes[0] = 32; s_weather.sizes[1] = 128;
    s_weather.live[0] = s_weather.live[1] = 1;
    for (index = 0; index < 108; index++) {
        model[index] = random_u32(&random_state);
        s_weather.heap[1][index] = random_u32(&random_state);
    }
    put_u32(player, 4, 0x9001);
    put_u32(player, 0x20, (uint32_t)(uintptr_t)s_weather.heap[0]);
    put_u32(player, 0x28, (uint32_t)(uintptr_t)s_weather.heap[1]);
    put_u32(player, 0x2c, 108);
    memcpy(module->data, module->initial_data, 0x1000); cache_setup_host(host);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xde8, 0);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)player);
    put_u32(module->data, M_PLAYER_COUNT, 1);
    put_u32(module->data, 0xa0, 0);
    put_u32(module->data, 0xb8, (uint32_t)(uintptr_t)s_texture.names[0]);
    if (s_title_mode) {
        memset(queues, 0, sizeof(queues)); memset(s_title_headers, 0, sizeof(s_title_headers));
        memset(s_title_models, 0, sizeof(s_title_models));
        s_title_aircraft = player + 0x10;
        for (index = 0; index < 2; index++) {
            put_u32(s_title_headers[index], 4, (uint32_t)(uintptr_t)(s_title_headers[index] + 32));
            put_u32(s_title_headers[index], 8, 16); put_u32(s_title_headers[index], 0xc, 0x1234);
            put_u32(s_title_headers[index], 0x18, 1);
            memcpy(s_title_models[index], model, 108); put_u32(s_title_models[index], 0, 128);
        }
        aircraft[0x20 / 4] = title_select; aircraft[0x24 / 4] = title_release;
        aircraft[0x34 / 4] = title_model; aircraft[0x38 / 4] = title_release; aircraft[0xa0 / 4] = title_exists;
        put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft);
        put_u32(module->data, 0x44, (uint32_t)(uintptr_t)aircraft);
        put_u32(module->data, 0xad0, number & 8);
        strcpy(s_texture.files[0].path, "Cache\\MODEL\\Cache"); s_texture.files[0].exists = 1; s_texture.files[0].size = 124;
        put_u32(s_texture.files[0].bytes, 0, 0x1234); put_u32(s_texture.files[0].bytes, 4, 0x5678);
        put_u32(s_texture.files[0].bytes, 8, 0); put_u32(s_texture.files[0].bytes, 12, 108);
        memcpy(s_texture.files[0].bytes + 16, model, 108);
        event = CreateEventA(NULL, FALSE, FALSE, NULL); if (!event) exit(2);
        put_u32(module->data, 0xf00, number % 4); put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
        for (index = 0; index < 2; index++) {
            InitializeCriticalSection(&queues[index].lock);
            put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
        }
    }
    for (step = 0; step < (s_title_mode ? 3u : 12u); step++) {
        uint32_t result = 0;
        memset(packet, 0, sizeof(packet));
        s_command_outcome = 0;
        if (__builtin_setjmp(s_command_jump) == 0) {
            s_command_active = 1;
            if (s_title_mode) {
                put_u32(packet, 8, number ^ 0x12345678u); strcpy((char *)packet + 12, "MODEL");
                if (step == 0) result = ((int32_t (STDCALL *)(const char *))send_internal(module, 195))("MODEL");
                else result = ((packet_fn)send_internal(module, 196))(step == 1 ? 0xdead : 0x9001, packet, number % 80);
            } else if (step < 4) {
                put_u32(packet, 8, 108);
                put_u32(packet, 0xc, (const uint32_t[]){2, 1, 0, 0xffffffffu}[number % 4]);
                put_u32(packet, 0x10 + 0x14, 0); put_u32(packet, 0x10 + 0x1c, 108);
                put_u32(packet, 0x10 + 0x20, number ^ 0x12340000u);
                put_u32(packet, 0x10 + 0x28, number ^ 0x56780000u);
                strcpy((char *)packet + 0x40, "MODEL");
                result = ((packet_fn)send_internal(module, 192))(step == 3 ? 0x9001 : 0x9001 + step,
                    packet, number % 80);
            } else if (step < 7) {
                uint32_t offset = step == 4 ? 54 : 0, size = step == 6 ? 108 : 54;
                put_u32(packet, 8, size); put_u32(packet, 0xc, offset);
                memcpy(packet + 0x10, model + offset, size);
                result = ((packet_fn)send_internal(module, 193))(step == 6 ? 0x9002 : 0x9001,
                    packet, number % 80);
            } else if (step == 7) result = ((message_id_fn)send_internal(module, 191))(0x9002);
            else if (step == 8) result = weather_address(((find_fn)send_internal(module, 190))(0x9001));
            else result = ((packet_fn)send_internal(module, 194))(step == 9 ? 0x9003 : step == 10 ? 0x9001 : 0xdead,
                packet, number % 80);
            s_command_active = 0;
        }
        results->states[step][0] = result;
        results->states[step][1] = s_command_outcome;
        results->states[step][2] = weather_address((void *)(uintptr_t)get_u32(module->data, 0xde8));
        results->states[step][3] = texture_receive_hash();
        results->states[step][4] = s_weather.calls;
        results->states[step][5] = s_weather.frees;
    }
    if (s_title_mode) {
        memcpy(s_weather.global, s_title_headers, sizeof(s_title_headers));
        memcpy(s_weather.global + sizeof(s_title_headers), s_title_models, sizeof(s_title_models));
        for (index = 0; index < 2; index++) {
            s_weather.queues[index][0] = queues[index].count;
            s_weather.queues[index][1] = weather_address(queues[index].head);
            s_weather.queues[index][2] = weather_address(queues[index].tail);
            s_weather.queues[index][3] = queues[index].lock.RecursionCount;
            while (queues[index].lock.RecursionCount) LeaveCriticalSection(&queues[index].lock);
            DeleteCriticalSection(&queues[index].lock);
        }
        CloseHandle(event);
    }
    memcpy(results->player, player, sizeof(player));
    memcpy(&s_texture.weather, &s_weather, sizeof(s_weather)); memcpy(&results->texture, &s_texture, sizeof(s_texture));
    for (index = 0; index < 256; index += 4) {
        uint32_t address = weather_address((void *)(uintptr_t)get_u32(player, index));
        if (address != 0xffffffffu) put_u32(results->player, index, address);
    }
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->texture.weather.heap[index], byte, address);
        }
    }
}
static uint32_t test_model_receive(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct model_receive_results s_retail, s_ours;
    uint32_t number, bad = 0, faults = 0;
    void *handler = AddVectoredExceptionHandler(1, command_fault);
    if (!handler) exit(2);
    for (number = 0; number < scenarios; number++) {
        uint32_t step;
        multip_test_context(s_title_mode ? "title_receive" : "model_receive", "retail", number); run_model_receive(retail, number, &s_retail);
        multip_test_context(s_title_mode ? "title_receive" : "model_receive", "ours", number); run_model_receive(ours, number, &s_ours);
        for (step = 0; step < 12; step++) faults += s_retail.states[step][1] != 0;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("model_receive scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                        (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
                for (byte = 0; byte < 12; byte++) if (memcmp(s_retail.states[byte], s_ours.states[byte], 24)) {
                    printf(" first differing action %lu\n", (unsigned long)byte); break;
                }
            }
            bad++;
        }
    }
    RemoveVectoredExceptionHandler(handler);
    printf("model receive outcomes: %lu pointer faults\n", (unsigned long)faults);
    printf("MULTIP %s: %lu scenarios, %lu differ\n", s_title_mode ? "aircraft title receive" : "multipart model receive", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
static uint32_t test_title_receive(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    uint32_t bad;
    s_title_mode = 1; bad = test_model_receive(retail, ours, scenarios); s_title_mode = 0;
    return bad;
}
