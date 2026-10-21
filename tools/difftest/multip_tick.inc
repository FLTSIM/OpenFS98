/* Tick callback ABI, FPU state, registration ordering and weather/sync queues. */
struct tick_fixture {
    struct weather_fixture weather;
    uint8_t players[4][256], models[5][64], navigation[32], instrument[32];
    uint32_t returns[18], state[18][7], chain_calls, fail_chain, range_calls;
    uint32_t chain_log[32][4], range_log[8][5];
};
static struct tick_fixture s_tick;
static const struct multip_module *s_tick_module;
static uint32_t STDCALL tick_ticks(void)
{
    uint32_t index;
    weather_log(4); s_weather.ticks++;
    for (index = 0; index < 4; index++) {
        put_u32(s_tick.players[index], 0x3c, s_weather.ticks ^ index);
        put_u32(s_tick.players[index], 0x40, s_weather.ticks + index);
    }
    return s_weather.ticks;
}
static int32_t tick_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t index, *entry;
    if (s_tick.chain_calls >= 32) exit(2);
    entry = s_tick.chain_log[s_tick.chain_calls++];
    entry[0] = tag; entry[1] = code; entry[2] = 0xffffffffu;
    for (index = 107; index <= 110; index++) if ((void *)callback == send_internal(s_tick_module, index)) entry[2] = index;
    entry[3] = get_u32(s_tick_module->data, 0x940);
    return s_tick.chain_calls == s_tick.fail_chain ? -1 : 0;
}
static int32_t STDCALL tick_add(uint32_t code, track_callback_fn callback) { return tick_registration(1, code, callback); }
static int32_t STDCALL tick_remove(uint32_t code, track_callback_fn callback) { return tick_registration(2, code, callback); }
static void STDCALL tick_range(const void *from, const void *to, int32_t flags,
    int32_t *distance, int16_t *bearing, uint8_t *octant)
{
    uint32_t *entry, index;
    if (s_tick.range_calls >= 8) exit(2);
    entry = s_tick.range_log[s_tick.range_calls++];
    entry[0] = from == s_weather.global + 0x560; entry[1] = draw_hash(to, 24); entry[2] = flags;
    *distance = s_weather.number * 13; *bearing = (int16_t)(s_weather.number * 12347); *octant = 3;
    if (s_weather.number & 0x100) for (index = 0; index < 4; index++)
        put_u32(s_tick.players[index], 0x28, (uint32_t)(uintptr_t)s_tick.models[4]);
    if (s_weather.number & 0x200) put_u32(s_tick_module->data, 0xab0, 0);
}
__attribute__((naked)) static uint32_t tick_call_painted(void *function __attribute__((unused)),
    uint32_t code __attribute__((unused)), uint32_t argument __attribute__((unused)),
    uint32_t incoming __attribute__((unused)), uint32_t padding __attribute__((unused)))
{
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tmovl 16(%esp), %ebx\n\t"
        "movl 20(%esp), %esi\n\tmovl 24(%esp), %edi\n\tmovl 28(%esp), %edx\n\tmovl 32(%esp), %ecx\n\t"
        "subl $0x400, %esp\n\tmovl %ecx, 0x244(%esp)\n\tnotl %ecx\n\t"
        "movl %ecx, 0x240(%esp)\n\tmovl %ecx, 0x248(%esp)\n\taddl $0x300, %esp\n\t"
        "pushl %edi\n\tpushl %esi\n\tmovl %edx, %eax\n\tcall *%ebx\n\taddl $0x100, %esp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
static void tick_snapshot(uint32_t step, uint32_t result)
{
    static const uint32_t s_offsets[] = {0x940,0x944,0xa68,0xf1c,0xab8,0xa50,0xa54};
    uint32_t index;
    s_tick.returns[step] = result;
    for (index = 0; index < 7; index++) s_tick.state[step][index] = get_u32(s_tick_module->data, s_offsets[index]);
}
static uint32_t tick_invoke(uint32_t entry, uint32_t seed, uint32_t padding)
{
    if (entry >= 113) return ((multip_worker_fn)send_internal(s_tick_module, entry))();
    __asm__ volatile("fnclex");
    return tick_call_painted(send_internal(s_tick_module, entry), 0x02, 0x87654321u, seed, padding);
}
static void run_tick(const struct multip_module *module, uint32_t number, struct tick_fixture *results)
{
    uint8_t host[0x220] = {0};
    void *chain[9] = {0}, *util[26] = {0}, *function;
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t random_state = number ^ 0x63481729u, index, byte, seed = random_u32(&random_state), padding;
    uint16_t control_word, original_control;
    static const uint32_t s_counters[] = {0,1,2,0xfffffffeu,0xffffffffu};
    static const double s_intervals[] = {0.0,1.0,99.99,100.0,100.001,-1.0,1e100};
    double value;

    if (!event) exit(2);
    memset(&s_tick, 0, sizeof(s_tick)); memset(&s_weather, 0, sizeof(s_weather)); memset(queues, 0, sizeof(queues));
    s_tick_module = module; s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + (number / 3) % 100 : 0;
    s_weather.fail_free = number % 5 == 0 ? 1 + (number / 5) % 5 : 0;
    s_tick.fail_chain = number % 11; s_weather.ticks = random_u32(&random_state);
    memcpy(module->data, module->initial_data, 0x1000);
    function = weather_alloc; memcpy(host + 0x20c, &function, 4);
    function = weather_free; memcpy(host + 0x214, &function, 4);
    function = weather_copy; memcpy(host + 0x21c, &function, 4);
    function = tick_ticks; memcpy(host + 0x38, &function, 4);
    chain[0x18 / 4] = tick_add; chain[0x1c / 4] = tick_remove; util[0x64 / 4] = tick_range;
    for (index = 0; index < sizeof(s_weather.global); index++) s_weather.global[index] = random_u32(&random_state);
    for (index = 0; index < 4; index++) {
        for (byte = 0; byte < 256; byte++) s_tick.players[index][byte] = random_u32(&random_state);
        put_u32(s_tick.players[index], 4, 0x7000 + index);
        put_u32(s_tick.players[index], 0x28, (uint32_t)(uintptr_t)s_tick.models[index]);
    }
    for (index = 0; index < sizeof(s_tick.models); index++) ((uint8_t *)s_tick.models)[index] = random_u32(&random_state);
    for (index = 0; index < sizeof(s_tick.instrument); index++) s_tick.instrument[index] = random_u32(&random_state);
    put_u32(s_weather.global, 0x384, (uint32_t)(uintptr_t)s_tick.navigation);
    put_u32(s_tick.navigation, 8, (uint32_t)(uintptr_t)s_tick.instrument);
    put_u32(s_weather.global, 0x84c, number & 1);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_weather.global);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_tick.players);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_tick.players[0]);
    put_u32(module->data, M_PLAYER_COUNT, number % 5);
    put_u32(module->data, M_PLAYER_MODE, (number / 5) % 3);
    put_u32(module->data, M_ACTIVE_PLAYER_ID, 0x7000 + (number / 15) % 5);
    put_u32(module->data, 0xab0, number & 2); put_u32(module->data, 0xab8, number & 4);
    put_u32(module->data, 0xa68, number & 8); put_u32(module->data, 0xaa8, number & 16);
    put_u32(module->data, 0xf1c, s_counters[(number / 4) % 5]);
    put_u32(module->data, 0x164, s_counters[(number / 20) % 5]);
    put_u32(module->data, 0x944, s_counters[(number / 100) % 5]);
    for (index = 0; index < 2; index++) {
        uint32_t sample = (index == 0 ? number : number / 10) % 10;
        uint32_t offset = index == 0 ? 0xa50 : 0x158;
        if (sample < 7) {
            value = s_intervals[sample]; memcpy(module->data + offset, &value, 8);
        } else {
            uint64_t bits = sample == 7 ? 0x7ff8000000001234ull : sample == 8 ? 0x7ff0000000000000ull : 0xfff0000000000000ull;
            memcpy(module->data + offset, &bits, 8);
        }
    }
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    __asm__ volatile("fnstcw %0" : "=m"(original_control));
    control_word = (uint16_t)((original_control & ~0xf00u) | ((number / 49) % 3 == 0 ? 0 : (number / 49) % 3 == 1 ? 0x200 : 0x300) | ((number / 147) % 4) << 10);
    __asm__ volatile("fldcw %0" : : "m"(control_word));
    tick_snapshot(0, tick_invoke(113, seed, 0));
    tick_snapshot(1, tick_invoke(107, seed, 0));
    tick_snapshot(2, tick_invoke(108, seed, 0));
    tick_snapshot(3, tick_invoke(108, seed ^ 0xffffffffu, 0));
    padding = random_u32(&random_state);
    tick_snapshot(4, tick_invoke(109, seed, padding));
    tick_snapshot(5, tick_invoke(109, seed, ~padding));
    tick_snapshot(6, tick_invoke(110, seed, 0));
    put_u32(s_weather.global, 0x84c, 0); put_u32(module->data, 0xab0, 1); put_u32(module->data, 0xab8, 1);
    put_u32(module->data, M_PLAYER_MODE, 1); put_u32(module->data, M_ACTIVE_PLAYER_ID, 0x7000);
    tick_snapshot(7, tick_invoke(110, seed, 0));
    put_u32(s_tick.players[0], 0x28, 0);
    tick_snapshot(8, tick_invoke(110, seed, 0));
    tick_snapshot(9, ((track_show_fn)send_internal(module, 111))(seed));
    tick_snapshot(10, ((message_value_fn)send_internal(module, 112))(seed, padding));
    tick_snapshot(11, tick_invoke(114, seed, 0));
    s_tick.fail_chain = 0;
    tick_snapshot(12, tick_invoke(114, seed, 0));
    tick_snapshot(13, tick_invoke(114, seed, 0));
    tick_snapshot(14, tick_invoke(113, seed, 0));
    tick_snapshot(15, tick_invoke(114, seed, 0));
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(original_control));
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
    memcpy(&s_tick.weather, &s_weather, sizeof(s_weather)); memcpy(results, &s_tick, sizeof(*results));
    for (index = 0; index < s_weather.blocks; index++) for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
        uint32_t normalized = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
        if (normalized != 0xffffffffu) put_u32(results->weather.heap[index], byte, normalized);
    }
}
static uint32_t test_tick(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct tick_fixture s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("tick", "retail", number); run_tick(retail, number, &s_retail);
        multip_test_context("tick", "ours", number); run_tick(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("tick scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP tick callbacks/CHAIN 0x02: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
