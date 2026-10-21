/* Texture sender list ownership, timed chunks and mutable 1015 requests. */
struct texture_send_results {
    struct texture_results texture;
    uint32_t states[24][10], packet[12];
};
static int32_t texture_send_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t *entry = weather_log(tag);
    entry[1] = code; entry[2] = (void *)callback == send_internal(s_texture_module, 176);
    entry[3] = get_u32(s_texture_module->data, 0xe38);
    return texture_fail() ? -1 : 0;
}
static int32_t STDCALL texture_send_add_chain(uint32_t code, track_callback_fn callback)
{ return texture_send_registration(40, code, callback); }
static int32_t STDCALL texture_send_remove_chain(uint32_t code, track_callback_fn callback)
{ return texture_send_registration(41, code, callback); }
static void run_texture_send(const struct multip_module *module,
    uint32_t number, struct texture_send_results *results)
{
    uint8_t host[0x320] = {0}, name[15] = "READ.BMP", other[15] = "HAVE.BMP", packet[48] = {0};
    void *chain[8] = {0}, *file;
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t step, index, result;
    uint16_t control_word, original_control;
    typedef int32_t (STDCALL *cache_fn)(const void *);
    typedef int32_t (STDCALL *recipient_fn)(const void *, uint32_t);
    typedef int32_t (STDCALL *send_fn)(uint32_t, const void *);
    typedef int32_t (STDCALL *packet_fn)(uint32_t, void *, uint32_t);
    typedef void *(STDCALL *find_fn)(const void *);
    if (!event) exit(2);
    memset(results, 0, sizeof(*results)); memset(&s_texture, 0, sizeof(s_texture));
    memset(&s_weather, 0, sizeof(s_weather)); memset(queues, 0, sizeof(queues));
    s_texture_module = module; s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + number / 3 % 160 : 0;
    s_weather.fail_free = number % 7 == 0 ? 1 + number / 7 % 12 : 0;
    memcpy(module->data, module->initial_data, 0x1000);
    texture_setup_host(host);
    chain[0x18 / 4] = texture_send_add_chain; chain[0x1c / 4] = texture_send_remove_chain;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0xe38, number & 1);
    put_u32(module->data, 0xe3c, 0);
    put_u32(module->data, 0xe40, number | 0xfffffff0u);
    put_u32(module->data, 0x170, (const uint32_t[]){0, 1, 100, 1000, 0xffffffffu}[number % 5]);
    put_u32(module->data, 0x16c, 1 + number % 32);
    put_u32(module->data, 0xac4, number & 2);
    *(double *)(void *)(module->data + 0xe48) = 0.01;
    *(double *)(void *)(module->data + 0xe50) = (const double[]){0, 0.099, 1, -1}[number % 4];
    strcpy(s_texture.files[0].path, "C:\\FS98\\Texture\\READ.BMP");
    strcpy(s_texture.files[1].path, "C:\\FS98\\Texture\\HAVE.BMP");
    s_texture.files[0].exists = s_texture.files[1].exists = 1;
    s_texture.files[0].size = number % 33; s_texture.files[1].size = 0;
    for (index = 0; index < 128; index++) s_texture.files[0].bytes[index] = number + index;
    name[14] = number | 1;
    put_u32(packet, 8, 2); memcpy(packet + 12, name, 15); memcpy(packet + 27, other, 15);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    __asm__ volatile("fnstcw %0" : "=m"(original_control));
    control_word = (original_control & ~0xf00u) | (const uint16_t[]){0, 0x200, 0x300}[number % 3] | ((number / 3) % 4) << 10;
    __asm__ volatile("fldcw %0" : : "m"(control_word));
    for (step = 0; step < 24; step++) {
        result = 0;
        if (step == 2) name[14] ^= 0x80;
        if (step == 0 || step == 18) result = ((multip_worker_fn)send_internal(module, 174))();
        else if (step == 1 || step == 2) result = ((cache_fn)send_internal(module, 169))(name);
        else if (step == 3 || step == 4 || step == 6)
            result = ((send_fn)send_internal(module, 175))(step == 6 ? 0x9003 : 0x9001, step == 6 ? other : name);
        else if (step == 5) result = ((recipient_fn)send_internal(module, 171))(name, 0x9002);
        else if (step == 7) result = weather_address(((find_fn)send_internal(module, 168))(name));
        else if ((step >= 8 && step <= 10) || step == 20) {
            uint32_t raw;
            if (step == 10) {
                *(double *)(void *)(module->data + 0xe50) = 1.0;
                *(double *)(void *)(module->data + 0xe48) = 0.0;
            }
            __asm__ volatile("fnclex");
            raw = tick_call_painted(send_internal(module, 176), 0x13, 0x54, number ^ 0xabcd0000u, 0);
            result = weather_address((void *)(uintptr_t)raw);
            if (result == 0xffffffffu) result = raw;
        } else if (step == 11 || step == 12) {
            file = ((find_fn)send_internal(module, 168))(name);
            if (file) result = ((recipient_fn)send_internal(module, 172))(file, step == 11 ? 0xdead : 0x9002);
        } else if (step == 13 || step == 14 || step == 21 || step == 22)
            result = ((cache_fn)send_internal(module, 170))(step & 1 ? other : name);
        else if (step == 15 || step == 16 || step == 23) result = ((multip_worker_fn)send_internal(module, 173))();
        else if (step == 19) result = ((packet_fn)send_internal(module, 177))(0x9004, packet, number % 48);
        results->states[step][0] = result;
        results->states[step][1] = weather_address((void *)(uintptr_t)get_u32(module->data, 0xe3c));
        results->states[step][2] = get_u32(module->data, 0xe38);
        results->states[step][3] = get_u32(module->data, 0xe40);
        memcpy(results->states[step] + 4, module->data + 0xe48, 16);
        results->states[step][8] = texture_receive_hash();
        results->states[step][9] = WaitForSingleObject(event, 0);
    }
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(original_control));
    memcpy(results->packet, packet, sizeof(packet));
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event); memcpy(&s_texture.weather, &s_weather, sizeof(s_weather));
    memcpy(&results->texture, &s_texture, sizeof(s_texture));
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->texture.weather.heap[index], byte, address);
        }
    }
}
static uint32_t test_texture_send(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct texture_send_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("texture_send", "retail", number); run_texture_send(retail, number, &s_retail);
        multip_test_context("texture_send", "ours", number); run_texture_send(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("texture_send scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                        (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
                for (byte = 0; byte < 24; byte++) if (memcmp(s_retail.states[byte], s_ours.states[byte], 40)) {
                    printf(" first state difference at action %lu: returns %#lx/%#lx\n", (unsigned long)byte,
                        (unsigned long)s_retail.states[byte][0], (unsigned long)s_ours.states[byte][0]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP multipart texture sender: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
