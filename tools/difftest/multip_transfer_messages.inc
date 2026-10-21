/* Multipart constructors through real queues; compare every allocated byte,
 * failure leak, signal and packet, including title callback side effects. */
static uint8_t s_transfer_aircraft[64];
static int32_t STDCALL transfer_title(void *aircraft, uint32_t kind, char *out, uint32_t capacity)
{
    uint32_t *entry = weather_log(9), index, length = s_weather.number % 260;
    entry[1] = aircraft == s_weather.global + 0x11e0;
    entry[2] = kind; entry[3] = capacity;
    for (index = 0; index < length; index++) out[index] = 'A' + (s_weather.number + index) % 26;
    out[length] = 0;
    put_u32(s_transfer_aircraft, 8, 0xfffffff0u + s_weather.number);
    put_u32(s_transfer_aircraft, 0x18, 123 + s_weather.number);
    s_weather.player[0x10] ^= 0x5a;
    return -1;
}
static void run_transfer_messages(const struct multip_module *module,
    uint32_t number, struct weather_fixture *results)
{
    uint8_t host[0x320] = {0}, source[256], name[15] = "PART.BMP";
    void *aircraft_linkage[13] = {0};
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t index, step, random_state = number ^ 0x5241738u;
    typedef int32_t (STDCALL *model_header_fn)(uint32_t, uint32_t, uint32_t);
    typedef int32_t (STDCALL *model_chunk_fn)(uint32_t, const void *, uint32_t, uint32_t);
    typedef int32_t (STDCALL *texture_header_fn)(uint32_t, uint32_t, const void *, uint32_t, uint32_t);
    typedef int32_t (STDCALL *texture_chunk_fn)(uint32_t, uint32_t, const void *, uint32_t, uint32_t);
    if (!event) exit(2);
    memset(&s_weather, 0, sizeof(s_weather)); memset(queues, 0, sizeof(queues));
    memset(s_transfer_aircraft, 0, sizeof(s_transfer_aircraft));
    s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + number / 3 % 35 : 0;
    s_weather.fail_free = number % 7 == 0 ? 1 + number / 7 % 5 : 0;
    for (index = 0; index < sizeof(source); index++) {
        source[index] = random_u32(&random_state);
        s_weather.player[index] = random_u32(&random_state);
    }
    for (index = 9; index < sizeof(name); index++) name[index] = random_u32(&random_state);
    memcpy(module->data, module->initial_data, 0x1000);
    texture_setup_host(host);
    aircraft_linkage[0x30 / 4] = transfer_title;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_weather.global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft_linkage);
    put_u32(module->data, 0xa3c, (uint32_t)(uintptr_t)s_weather.player);
    put_u32(s_weather.global, 0x11e0, (uint32_t)(uintptr_t)s_transfer_aircraft);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    for (step = 0; step < 5; step++) {
        uint32_t id = random_u32(&random_state), offset = random_u32(&random_state), size = number % 257;
        if (step == 0) s_weather.returns[step] = ((model_header_fn)send_internal(module, 163))(id, 0x1234, offset);
        else if (step == 1) s_weather.returns[step] = ((model_chunk_fn)send_internal(module, 164))(id, source, offset, size);
        else if (step == 2) s_weather.returns[step] = ((message_id_fn)send_internal(module, 165))(id);
        else if (step == 3) s_weather.returns[step] = ((texture_header_fn)send_internal(module, 166))(id, offset, name, number ^ 0xfffffff0u, number % 4);
        else s_weather.returns[step] = ((texture_chunk_fn)send_internal(module, 167))(id, number ^ 0xabcdef00u, source, offset, size);
        s_weather.signals[step] = WaitForSingleObject(event, 0);
    }
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    put_u32(s_weather.global, 0x11e0, 0x12340000);
    CloseHandle(event); memcpy(results, &s_weather, sizeof(*results));
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->heap[index], byte, address);
        }
    }
}
static uint32_t test_transfer_messages(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct weather_fixture s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("transfer_messages", "retail", number); run_transfer_messages(retail, number, &s_retail);
        multip_test_context("transfer_messages", "ours", number); run_transfer_messages(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("transfer_messages scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                        (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP multipart transfer messages: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
