/* Multipart receives use the texture fixture's memory filesystem. Snapshot
 * each transition, including detached nodes and failed frees, before reuse. */
struct texture_receive_results {
    struct texture_results texture;
    uint32_t states[32][5];
};
static uint32_t texture_receive_hash(void)
{
    uint8_t heap[1024];
    uint32_t block, byte, hash = 2166136261u;
    for (block = 0; block < s_weather.blocks; block++) {
        memcpy(heap, s_weather.heap[block], sizeof(heap));
        for (byte = 0; byte < s_weather.sizes[block]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(heap, byte));
            if (address != 0xffffffffu) put_u32(heap, byte, address);
        }
        hash = (hash ^ draw_hash(heap, s_weather.sizes[block])) * 16777619u;
        hash = (hash ^ s_weather.live[block]) * 16777619u;
    }
    return hash;
}
static void run_texture_receive(const struct multip_module *module,
    uint32_t number, struct texture_receive_results *results)
{
    uint8_t host[0x320] = {0}, packet[96] = {0};
    uint32_t step, index, result, owner_id, texture;
    void *owner;
    typedef void *(STDCALL *owner_fn)(uint32_t);
    typedef void *(STDCALL *file_fn)(const void *, uint32_t);
    typedef int32_t (STDCALL *add_fn)(uint32_t, const void *, uint32_t, uint32_t, uint32_t);
    typedef int32_t (STDCALL *remove_fn)(void *, uint32_t);
    typedef int32_t (STDCALL *clear_fn)(void *);
    typedef int32_t (STDCALL *packet_fn)(uint32_t, const void *, uint32_t);
    memset(results, 0, sizeof(*results));
    memset(&s_texture, 0, sizeof(s_texture)); memset(&s_weather, 0, sizeof(s_weather));
    s_texture_module = module; s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + (number / 3) % 120 : 0;
    s_weather.fail_free = number % 5 == 0 ? 1 + (number / 5) % 12 : 0;
    memcpy(module->data, module->initial_data, 0x1000);
    texture_setup_host(host);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xdec, 0);
    put_u32(module->data, 0xc0, 0);
    put_u32(module->data, 0xd8, (uint32_t)(uintptr_t)s_texture.names[1]);
    for (step = 0; step < 32; step++) {
        result = 0;
        owner_id = step < 4 ? 0x9001 + (step == 3 ? 1 : step) : 0x9001;
        texture = step < 7 ? step - 3 : 1;
        owner = ((owner_fn)send_internal(module, 153))(owner_id);
        memset(packet, 0, sizeof(packet));
        memcpy(packet + 8, "PART.BMP", 9); packet[22] = (uint8_t)(number | 1);
        put_u32(packet, 0x18, 32);
        put_u32(packet, 0x1c, step == 10 ? 8 : 7);
        put_u32(packet, 0x20, (const uint32_t[]){1, 2, 0, 0xffffffffu}[number % 4]);
        if (step < 4 || step == 22)
            result = ((message_id_fn)send_internal(module, 155))(step == 22 ? 0x9010 : owner_id);
        else if (step < 9 || step == 23)
            result = ((add_fn)send_internal(module, 156))(step == 8 ? 0x9999 : step == 23 ? 0x9010 : owner_id,
                packet + 8, 32, step == 23 ? 7 : texture, 2);
        else if (step < 12 || step == 24)
            result = ((packet_fn)send_internal(module, 160))(step == 11 ? 0x9003 : step == 24 ? 0x9010 : 0x9002,
                packet, number % 40);
        else if (step < 15 || (step >= 25 && step <= 27)) {
            put_u32(packet, 8, 7); put_u32(packet, 0xc, number % 3 == 0 ? 0 : 8);
            put_u32(packet, 0x10, step & 1 ? 16 : 0);
            for (index = 0x14; index < sizeof(packet); index++) packet[index] = number + step + index;
            result = ((packet_fn)send_internal(module, 161))(step < 15 ? 0x9002 : 0x9010, packet, number % 40);
        } else if (step < 18) {
            if (owner) result = ((remove_fn)send_internal(module, 157))(owner, step == 15 ? 2 : step == 16 ? 3 : 1);
        } else if (step == 19) {
            owner = ((owner_fn)send_internal(module, 153))(0x9003);
            if (owner) result = ((clear_fn)send_internal(module, 159))(owner);
        } else if (step < 22)
            result = ((message_id_fn)send_internal(module, 158))(step == 18 ? 0x9002 : step == 20 ? 0x9003 : 0x9999);
        else if (step < 31) {
            put_u32(packet, 8, step == 29 ? 0xdead : 7);
            result = ((packet_fn)send_internal(module, 162))(step == 30 ? 0x9999 : 0x9010, packet, number % 40);
        } else {
            owner = ((owner_fn)send_internal(module, 153))(0x9010);
            result = weather_address(((file_fn)send_internal(module, 154))(owner, 7));
        }
        results->states[step][0] = result;
        results->states[step][1] = weather_address((void *)(uintptr_t)get_u32(module->data, 0xdec));
        results->states[step][2] = texture_receive_hash();
        results->states[step][3] = s_weather.calls;
        results->states[step][4] = s_weather.frees;
    }
    memcpy(&s_texture.weather, &s_weather, sizeof(s_weather));
    memcpy(&results->texture, &s_texture, sizeof(s_texture));
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->texture.weather.heap[index], byte, address);
        }
    }
}
static uint32_t test_texture_receive(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct texture_receive_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("texture_receive", "retail", number); run_texture_receive(retail, number, &s_retail);
        multip_test_context("texture_receive", "ours", number); run_texture_receive(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf("texture_receive scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                            (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP multipart texture receive: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
