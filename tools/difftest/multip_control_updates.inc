/* Control changes, collision cooldowns and periodic state publication. */
struct control_update_results {
    struct weather_fixture weather;
    uint8_t players[4][256], local[256], models[4][64];
    uint32_t states[7][10], ranges;
};
static struct control_update_results s_control_update;
static void STDCALL control_range(const void *from, const void *to, int32_t flags,
    int32_t *distance, int16_t *bearing, uint8_t *octant)
{
    uint32_t *entry = weather_log(50), which = s_control_update.ranges++;
    entry[1] = from == s_weather.global + 0x560; entry[2] = draw_hash(to, 24); entry[3] = flags;
    *distance = s_weather.number % 32 == 0 ? 0 : (int32_t)(s_weather.number + which) % 7 - 3;
    *bearing = (int16_t)s_weather.number; *octant = 3;
    if (s_weather.number % 32 != 0 && (s_weather.number & 0x80)) s_weather.global[0x838] = which & 1;
}
__attribute__((naked)) static int32_t control_publish_painted(
    void *function __attribute__((unused)), uint32_t seed __attribute__((unused)))
{
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tmovl 16(%esp), %ebx\n\t"
        "movl 20(%esp), %eax\n\tsubl $0x800, %esp\n\tleal 8(%esp), %edi\n\t"
        "movl $250, %ecx\n\t1: stosl\n\taddl $0x193751, %eax\n\tloop 1b\n\t"
        "addl $0x400, %esp\n\tcall *%ebx\n\taddl $0x400, %esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
static void run_control_updates(const struct multip_module *module, uint32_t number,
    struct control_update_results *results)
{
    uint8_t host[0x320] = {0}, model[512];
    void *util[26] = {0};
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint8_t *local;
    uint32_t index, step, random_state = number ^ 0x19374685u;
    static const uint32_t s_timers[] = {0, 1, 65536, 0x7fffffff, 0x80000000, 0xffffffff};
    static const double s_intervals[] = {0, 0.999999, 1, 2, 10, -1};
    if (!event) exit(2);
    memset(&s_control_update, 0, sizeof(s_control_update)); memset(&s_weather, 0, sizeof(s_weather));
    memset(queues, 0, sizeof(queues)); memset(s_transfer_aircraft, 0, sizeof(s_transfer_aircraft));
    s_weather.number = number; s_weather.fail_call = number % 3 == 0 ? 1 + number / 3 % 45 : 0;
    s_weather.fail_free = number % 7 == 0 ? 1 : 0;
    for (index = 0; index < sizeof(s_weather.global); index++) s_weather.global[index] = random_u32(&random_state);
    for (index = 0; index < sizeof(model); index++) model[index] = random_u32(&random_state);
    for (index = 0; index < 4; index++) {
        put_u32(s_control_update.players[index], 4, 0x9000 + index);
        put_u32(s_control_update.players[index], 8, (number / (1 + index)) % 3);
        put_u32(s_control_update.players[index], 0xc, number & (1u << index));
        put_u32(s_control_update.players[index], 0x28, number & (0x10u << index) ? 0 : (uint32_t)(uintptr_t)s_control_update.models[index]);
        put_u32(s_control_update.players[index], 0x30, number % 5);
        put_u32(s_control_update.players[index], 0xcc, s_timers[(number / 4 + index) % 6]);
        memcpy(s_control_update.models[index], model + index * 64, 64);
    }
    local = number & 8 ? s_control_update.players[0] : s_control_update.local;
    put_u32(local, 8, number / 16 % 3); put_u32(local, 0xcc, s_timers[number % 6]);
    put_u32(s_transfer_aircraft, 0xc, number % 5);
    put_u32(s_transfer_aircraft, 0x18, 108 + number % 129);
    put_u32(s_weather.global, 0x11e0, (uint32_t)(uintptr_t)s_transfer_aircraft);
    put_u32(s_weather.global, 0x11e4, (uint32_t)(uintptr_t)model);
    *(uint16_t *)(void *)(s_weather.global + 0x5dc) = number & 1 ? 1 : 0;
    *(uint16_t *)(void *)(s_weather.global + 0x5de) = 0x1234;
    *(int16_t *)(void *)(s_weather.global + 0x2aa) = (const int16_t[]){0,1,-1,32767,-32768}[(number / 5) % 5];
    *(uint16_t *)(void *)(s_weather.global + 0x84a) = number & 0x100 ? 1 : 0;
    put_u32(s_weather.global, 0x268, number & 2 ? 1 : 0); s_weather.global[0x838] = number & 4 ? 1 : 0;
    memcpy(module->data, module->initial_data, 0x1000); texture_setup_host(host);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_weather.global);
    util[0x64 / 4] = control_range; put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_control_update.players);
    put_u32(module->data, M_PLAYER_COUNT, number % 5);
    put_u32(module->data, 0xa3c, (uint32_t)(uintptr_t)local);
    put_u32(module->data, 0xfc, number & 1); put_u32(module->data, 0x948, number & 0x200 ? 1 : 0);
    put_u32(module->data, 0x94c, number & 0x400 ? 1 : 0);
    put_u32(module->data, 0x160, number / 7 % 5); put_u32(module->data, 0xad4, number & 0x800);
    put_u32(module->data, 0xad8, random_u32(&random_state)); put_u32(module->data, 0xe08, random_u32(&random_state));
    put_u32(module->data, 0xaa0, 0x9002); put_u32(module->data, 0xabc, number & 0x100);
    *(double *)(void *)(module->data + 0xa50) = s_intervals[number / 8 % 6];
    *(double *)(void *)(module->data + 0xa78) = number & 0x1000 ? 0 : 1;
    if (number / 8 % 8 >= 6) {
        uint64_t bits = number / 8 % 8 == 6 ? UINT64_C(0x7ff0000000000000) : UINT64_C(0x7ff8000000001234);
        memcpy(module->data + 0xa50, &bits, 8);
    }
    if (number % 17 == 0) {
        uint64_t bits = UINT64_C(0x7ff8000000001234); memcpy(module->data + 0xa78, &bits, 8);
    }
    for (index = 0; index < 3; index++) *(double *)(void *)(module->data + 0xa88 + index * 8) = (int32_t)(number ^ index) * 0.01;
    put_u32(module->data, 0xf00, number % 4); put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    if (number % 32 == 0) {
        put_u32(local, 8, 1); put_u32(local, 0xcc, 0);
        put_u32(module->data, 0x160, 0); put_u32(module->data, M_PLAYER_COUNT, 2);
        put_u32(module->data, 0xad4, 0); put_u32(s_weather.global, 0x268, 0);
        *(uint16_t *)(void *)(s_weather.global + 0x5dc) = 1;
        *(uint16_t *)(void *)(s_weather.global + 0x84a) = 0;
        *(uint16_t *)(void *)(s_weather.global + 0x2aa) = 0;
        s_weather.global[0x838] = 0; put_u32(s_transfer_aircraft, 0xc, 0);
        for (index = 0; index < 2; index++) {
            put_u32(s_control_update.players[index], 8, 1);
            put_u32(s_control_update.players[index], 0xc, 0);
            put_u32(s_control_update.players[index], 0x28, (uint32_t)(uintptr_t)s_control_update.models[index]);
            put_u32(s_control_update.players[index], 0x30, 0); put_u32(s_control_update.players[index], 0xcc, 0);
        }
    }
    for (step = 0; step < 7; step++) {
        uint32_t result;
        if (step == 1) *(uint16_t *)(void *)(s_weather.global + 0x5dc) ^= 1;
        if (step == 4) {
            *(uint16_t *)(void *)(s_weather.global + 0x5dc) = number & 1 ? 1 : 0;
            put_u32(s_weather.global, 0x268, number & 2 ? 1 : 0);
            if (number & 0x4000) put_u32(module->data, 0xa3c, 0);
        }
        if (step == 6) { model[0x80] ^= 1; *(double *)(void *)(module->data + 0xa50) += 1.0; }
        if (step < 4) result = ((multip_worker_fn)send_internal(module, step < 2 ? 201 : 202))();
        else result = control_publish_painted(send_internal(module, 203), number ^ step);
        s_control_update.states[step][0] = result;
        s_control_update.states[step][1] = get_u32(module->data, 0x948);
        s_control_update.states[step][2] = get_u32(module->data, 0x94c);
        s_control_update.states[step][3] = get_u32(module->data, 0xe08);
        memcpy(s_control_update.states[step] + 4, module->data + 0xa50, 8);
        s_control_update.states[step][6] = module->data[0xf28];
        s_control_update.states[step][7] = WaitForSingleObject(event, 0);
        s_control_update.states[step][8] = get_u32(local, 0xcc);
        s_control_update.states[step][9] = draw_hash(s_control_update.players, sizeof(s_control_update.players));
    }
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event); put_u32(s_weather.global, 0x11e0, 0x12340000); put_u32(s_weather.global, 0x11e4, 0x23450000);
    memcpy(&s_control_update.weather, &s_weather, sizeof(s_weather)); memcpy(results, &s_control_update, sizeof(*results));
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->weather.heap[index], byte, address);
        }
    }
}
static uint32_t test_control_updates(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct control_update_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("control_updates", "retail", number); run_control_updates(retail, number, &s_retail);
        multip_test_context("control_updates", "ours", number); run_control_updates(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("control_updates scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                        (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP control/collision/state publishing: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
