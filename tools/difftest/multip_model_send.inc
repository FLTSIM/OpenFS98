/* Aircraft send startup/cancel, duplicate recipients, chunking and teardown. */
struct model_send_results {
    struct weather_fixture weather;
    uint32_t states[20][12];
};
static int32_t STDCALL model_send_title(void *aircraft, uint32_t kind, char *out, uint32_t capacity)
{
    uint32_t *entry = weather_log(9), index, length = s_weather.number % 80;
    entry[1] = aircraft == s_weather.global + 0x11e0; entry[2] = kind; entry[3] = capacity;
    for (index = 0; index < length; index++) out[index] = 'A' + (index + s_weather.number) % 26;
    out[length] = 0; return -1;
}
static int32_t model_send_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t *entry = weather_log(tag);
    entry[1] = code; entry[2] = (void *)callback == send_internal(s_texture_module, 184);
    entry[3] = get_u32(s_texture_module->data, 0xe20);
    return texture_fail() ? -1 : 0;
}
static int32_t STDCALL model_send_add_chain(uint32_t code, track_callback_fn callback)
{ return model_send_registration(42, code, callback); }
static int32_t STDCALL model_send_remove_chain(uint32_t code, track_callback_fn callback)
{ return model_send_registration(43, code, callback); }
static void run_model_send(const struct multip_module *module, uint32_t number,
    struct model_send_results *results)
{
    uint8_t host[0x320] = {0}, header[128], model[128];
    void *aircraft[13] = {0}, *chain[8] = {0};
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t index, step, random_state = number ^ 0x3192547u;
    uint16_t control_word, original_control;
    typedef int32_t (STDCALL *packet_fn)(uint32_t, const void *, uint32_t);
    if (!event) exit(2);
    memset(results, 0, sizeof(*results)); memset(&s_weather, 0, sizeof(s_weather));
    memset(queues, 0, sizeof(queues)); memset(s_transfer_aircraft, 0, sizeof(s_transfer_aircraft));
    s_texture_module = module; s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + number / 3 % 160 : 0;
    s_weather.fail_free = number % 7 == 0 ? 1 + number / 7 % 12 : 0;
    for (index = 0; index < sizeof(header); index++) {
        header[index] = random_u32(&random_state); model[index] = random_u32(&random_state);
    }
    for (index = 0; index < sizeof(s_weather.player); index++) s_weather.player[index] = random_u32(&random_state);
    put_u32(s_weather.player, 0x24, 32 + number % 33);
    put_u32(s_weather.player, 0x2c, number % 65);
    put_u32(s_transfer_aircraft, 4, (uint32_t)(uintptr_t)header);
    put_u32(s_transfer_aircraft, 8, number % 33);
    put_u32(s_transfer_aircraft, 0x18, 0x1234 + number);
    put_u32(s_weather.global, 0x11e0, (uint32_t)(uintptr_t)s_transfer_aircraft);
    put_u32(s_weather.global, 0x11e4, (uint32_t)(uintptr_t)model);
    aircraft[0x30 / 4] = model_send_title;
    chain[0x18 / 4] = model_send_add_chain; chain[0x1c / 4] = model_send_remove_chain;
    memcpy(module->data, module->initial_data, 0x1000); texture_setup_host(host);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_weather.global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0xa3c, (uint32_t)(uintptr_t)s_weather.player);
    put_u32(module->data, 0xe10, 0); put_u32(module->data, 0xe14, 0);
    put_u32(module->data, 0xe20, 0); put_u32(module->data, 0xacc, number & 1);
    put_u32(module->data, 0x170, (const uint32_t[]){0, 1, 100, 1000, 0xffffffffu}[number % 5]);
    put_u32(module->data, 0x16c, 1 + number % 129);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    __asm__ volatile("fnstcw %0" : "=m"(original_control));
    control_word = (original_control & ~0xf00u) | (const uint16_t[]){0, 0x200, 0x300}[number % 3] | ((number / 3) % 4) << 10;
    __asm__ volatile("fldcw %0" : : "m"(control_word));
    for (step = 0; step < 20; step++) {
        uint32_t result = 0, address;
        if (step == 0 || step == 8 || step == 15) result = ((multip_worker_fn)send_internal(module, 182))();
        else if (step < 4 || step == 11) result = ((message_id_fn)send_internal(module, 180))(step == 2 ? 0x9002 : step == 11 ? 0x9003 : 0x9001);
        else if (step == 4 || step == 5 || step == 13) {
            if (step == 13) {
                *(double *)(void *)(module->data + 0xe30) = 1.0;
                *(double *)(void *)(module->data + 0xe28) = 0.0;
            }
            __asm__ volatile("fnclex");
            if (get_u32(module->data, 0xe20) != 0)
                result = tick_call_painted(send_internal(module, 184), 0x12, 0x56, number ^ 0xabcd0000u, 0);
            address = weather_address((void *)(uintptr_t)result);
            if (address != 0xffffffffu) result = address;
        } else if (step == 6 || step == 7 || step == 14) result = ((message_id_fn)send_internal(module, 183))(step == 6 ? 0xdead : step == 7 ? 0x9001 : 0x9003);
        else if (step == 9 || step == 17) result = ((multip_worker_fn)send_internal(module, 178))();
        else if (step == 10 || step == 16) result = ((multip_worker_fn)send_internal(module, 181))();
        else if (step == 12) result = ((message_id_fn)send_internal(module, 179))(0x9004);
        else result = ((packet_fn)send_internal(module, 185))(0x9005, NULL, number);
        results->states[step][0] = result;
        results->states[step][1] = weather_address((void *)(uintptr_t)get_u32(module->data, 0xe10));
        results->states[step][2] = weather_address((void *)(uintptr_t)get_u32(module->data, 0xe14));
        results->states[step][3] = get_u32(module->data, 0xe18);
        results->states[step][4] = get_u32(module->data, 0xe1c);
        results->states[step][5] = get_u32(module->data, 0xe20);
        memcpy(results->states[step] + 6, module->data + 0xe28, 16);
        results->states[step][10] = texture_receive_hash();
        results->states[step][11] = WaitForSingleObject(event, 0);
    }
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(original_control));
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
    put_u32(s_weather.global, 0x11e0, 0x12340000); put_u32(s_weather.global, 0x11e4, 0x23450000);
    memcpy(&results->weather, &s_weather, sizeof(s_weather));
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->weather.heap[index], byte, address);
        }
    }
}
static uint32_t test_model_send(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct model_send_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("model_send", "retail", number); run_model_send(retail, number, &s_retail);
        multip_test_context("model_send", "ours", number); run_model_send(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("model_send scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                        (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP multipart model sender: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
