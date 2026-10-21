/* Aircraft receive state with partial copies, sequence boundaries, signed
 * velocity conversion and the three uninitialized 1019 position paddings. */
struct updates_results {
    struct weather_fixture weather;
    uint8_t records[4][256], models[5][256];
    uint32_t returns[12], hashes[12][9];
};
static struct updates_results s_updates;
static uint32_t STDCALL updates_ticks(void)
{
    uint32_t index;
    weather_log(4); s_weather.ticks++;
    if (s_weather.number & 0x100) for (index = 0; index < 4; index++)
        put_u32(s_updates.records[index], 0x28, (uint32_t)(uintptr_t)s_updates.models[4]);
    return s_weather.ticks;
}
__attribute__((naked)) static int32_t updates_call_painted(
    void *function __attribute__((unused)), uint32_t id __attribute__((unused)), const void *packet __attribute__((unused)),
    uint32_t size __attribute__((unused)), uint32_t latitude __attribute__((unused)), uint32_t longitude __attribute__((unused)), uint32_t altitude __attribute__((unused)))
{
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tmovl 16(%esp), %ebx\n\t"
        "movl 20(%esp), %esi\n\tmovl 24(%esp), %edi\n\tmovl 28(%esp), %edx\n\tsubl $0x400, %esp\n\t"
        "movl 0x420(%esp), %eax\n\tmovl %eax, 0x2cc(%esp)\n\tnotl %eax\n\tmovl %eax, 0x2c8(%esp)\n\t"
        "movl 0x424(%esp), %eax\n\tmovl %eax, 0x2d4(%esp)\n\tnotl %eax\n\tmovl %eax, 0x2d0(%esp)\n\t"
        "movl 0x428(%esp), %eax\n\tmovl %eax, 0x2dc(%esp)\n\tnotl %eax\n\tmovl %eax, 0x2d8(%esp)\n\t"
        "addl $0x300, %esp\n\tpushl %edx\n\tpushl %edi\n\tpushl %esi\n\tcall *%ebx\n\t"
        "addl $0x100, %esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
static void updates_snapshot(uint32_t step, uint32_t result)
{
    uint32_t index;
    s_updates.returns[step] = result;
    for (index = 0; index < 4; index++) s_updates.hashes[step][index] = draw_hash(s_updates.records[index], 256);
    for (index = 0; index < 5; index++) s_updates.hashes[step][index + 4] = draw_hash(s_updates.models[index], 256);
}
static void run_updates(const struct multip_module *module, uint32_t number, struct updates_results *results)
{
    uint8_t host[0x220] = {0}, packet[256], position[36];
    void *function;
    uint32_t index, byte, random_state = number ^ 0x61723845u, id;
    uint16_t original_control, control_word;
    static const uint32_t s_sequences[] = {0,1,2,0xfffffffeu,0xffffffffu};
    typedef int32_t (STDCALL *update_handler_fn)(uint32_t, const void *, uint32_t);
    typedef int32_t (STDCALL *update_init_fn)(void *, const void *, uint32_t);
    typedef int32_t (STDCALL *update_sync_fn)(uint32_t, const void *, uint32_t, uint32_t);

    memset(&s_updates, 0, sizeof(s_updates)); memset(&s_weather, 0, sizeof(s_weather));
    memcpy(module->data, module->initial_data, 0x1000);
    s_weather.number = number; s_weather.ticks = random_u32(&random_state);
    s_weather.fail_call = number % 3 == 0 ? 1 + (number / 3) % 50 : 0;
    function = weather_copy; memcpy(host + 0x21c, &function, 4);
    function = app_players_memset; memcpy(host + 0x218, &function, 4);
    function = updates_ticks; memcpy(host + 0x38, &function, 4);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_updates.records[0]);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_updates.records);
    put_u32(module->data, M_PLAYER_COUNT, number % 5);
    put_u32(module->data, 0x160, random_u32(&random_state));
    for (index = 0; index < sizeof(packet); index++) packet[index] = random_u32(&random_state);
    for (index = 0; index < sizeof(position); index++) position[index] = random_u32(&random_state);
    for (index = 0; index < 5; index++) for (byte = 0; byte < 256; byte++) s_updates.models[index][byte] = random_u32(&random_state);
    for (index = 0; index < 4; index++) {
        for (byte = 0; byte < 256; byte++) s_updates.records[index][byte] = random_u32(&random_state);
        put_u32(s_updates.records[index], 4, index == 0 ? 0xa000 : 0xa001);
        put_u32(s_updates.records[index], 0x28, (uint32_t)(uintptr_t)s_updates.models[index]);
        put_u32(s_updates.records[index], 0x2c, 0x6c + (number % 5) * 16);
        put_u32(s_updates.records[index], 0x4c, (number + index) % 2);
        put_u32(s_updates.records[index], 0x50, s_sequences[(number / 5 + index) % 5]);
        put_u32(s_updates.records[index], 0x40, number & 2 ? 0 : random_u32(&random_state));
        put_u32(s_updates.records[index], 0x48, s_sequences[(number / 25 + index) % 5]);
    }
    id = number % 3 == 0 ? 0xa000 : number % 3 == 1 ? 0xa001 : 0xdead;
    put_u32(packet, 0x10, 0x6c + (number % 5) * 16);
    put_u32(packet, 0xc, s_sequences[(number / 125) % 5]);
    put_u32(packet, 8, s_sequences[(number / 625) % 5]);
    __asm__ volatile("fnstcw %0" : "=m"(original_control));
    control_word = (uint16_t)((original_control & ~0x300u) | ((number / 7) % 3 == 0 ? 0 : (number / 7) % 3 == 1 ? 0x200 : 0x300));
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(control_word));
    updates_snapshot(0, ((update_init_fn)send_internal(module, 137))(s_updates.records[0], position, random_u32(&random_state)));
    updates_snapshot(1, ((update_handler_fn)send_internal(module, 136))(id, packet, 0));
    updates_snapshot(2, ((update_handler_fn)send_internal(module, 138))(id, packet, number % 257));
    updates_snapshot(3, updates_call_painted(send_internal(module, 139), id, packet, 0,
        random_u32(&random_state), random_u32(&random_state), random_u32(&random_state)));
    put_u32(packet, 0x10, random_u32(&random_state));
    updates_snapshot(4, ((update_sync_fn)send_internal(module, 140))(id, packet, 0, random_u32(&random_state)));
    updates_snapshot(5, ((update_handler_fn)send_internal(module, 138))(id, packet, 0));
    updates_snapshot(6, updates_call_painted(send_internal(module, 139), id, packet, 0,
        random_u32(&random_state), random_u32(&random_state), random_u32(&random_state)));
    put_u32(packet, 0xc, number & 1 ? 0 : 0xffffffffu);
    updates_snapshot(7, ((update_sync_fn)send_internal(module, 140))(id, packet, 0, random_u32(&random_state)));
    put_u32(s_updates.records[1], 0x28, number & 4 ? 0 : (uint32_t)(uintptr_t)s_updates.models[1]);
    updates_snapshot(8, ((update_init_fn)send_internal(module, 137))(s_updates.records[1], position, random_u32(&random_state)));
    put_u32(s_updates.records[0], 0x28, 0);
    updates_snapshot(9, updates_call_painted(send_internal(module, 139), 0xa000, packet, 0,
        random_u32(&random_state), random_u32(&random_state), random_u32(&random_state)));
    put_u32(s_updates.records[0], 0x28, (uint32_t)(uintptr_t)s_updates.models[0]);
    put_u32(packet, 0x10, get_u32(s_updates.records[0], 0x2c) + 1);
    updates_snapshot(10, ((update_handler_fn)send_internal(module, 136))(0xa000, packet, 0));
    updates_snapshot(11, ((update_sync_fn)send_internal(module, 140))(0xa000, packet, 0, random_u32(&random_state)));
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(original_control));
    memcpy(&s_updates.weather, &s_weather, sizeof(s_weather)); memcpy(results, &s_updates, sizeof(*results));
}
static uint32_t test_updates(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct updates_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("updates", "retail", number); run_updates(retail, number, &s_retail);
        multip_test_context("updates", "ours", number); run_updates(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("updates scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP aircraft state receive/timing: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
