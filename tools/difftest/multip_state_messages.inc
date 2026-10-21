/* Paint the complete retail packet stack region, including both compact
 * padding words and the full-state packet's 0x6c-byte unsent-data gap. */
__attribute__((naked)) static int32_t state_call_painted(
    void *function __attribute__((unused)), uint32_t id __attribute__((unused)),
    uint32_t seed __attribute__((unused)))
{
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tmovl 16(%esp), %ebx\n\t"
        "movl 20(%esp), %esi\n\tmovl 24(%esp), %eax\n\tsubl $0x800, %esp\n\t"
        "leal 0x10(%esp), %edi\n\tmovl $250, %ecx\n\t1: stosl\n\taddl $0x193751, %eax\n\tloop 1b\n\t"
        "addl $0x400, %esp\n\tpushl %esi\n\tcall *%ebx\n\taddl $0x400, %esp\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
static void run_state_messages(const struct multip_module *module, uint32_t number,
    struct weather_fixture *results)
{
    uint8_t host[0x320] = {0}, model[512];
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t index, step, random_state = number ^ 0x87614592u;
    uint16_t original_control, control_word;
    static const uint64_t s_velocity_bits[] = {0, UINT64_C(0x3fb999999999999a), UINT64_C(0xbfb999999999999a),
        UINT64_C(0x400921fb54442d18), UINT64_C(0xc00921fb54442d18), UINT64_C(0x4310000000000000),
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000), UINT64_C(0x7ff8000000001234)};
    if (!event) exit(2);
    memset(&s_weather, 0, sizeof(s_weather)); memset(queues, 0, sizeof(queues));
    memset(s_transfer_aircraft, 0, sizeof(s_transfer_aircraft));
    s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + number / 3 % 16 : 0;
    for (index = 0; index < sizeof(s_weather.global); index++) s_weather.global[index] = random_u32(&random_state);
    for (index = 0; index < sizeof(model); index++) model[index] = random_u32(&random_state);
    put_u32(s_transfer_aircraft, 0x18, 0x6c + number % 257);
    put_u32(s_weather.global, 0x11e0, (uint32_t)(uintptr_t)s_transfer_aircraft);
    put_u32(s_weather.global, 0x11e4, (uint32_t)(uintptr_t)model);
    *(uint16_t *)(void *)(s_weather.global + 0x5dc) = number & 1 ? 0x1234 : 0;
    *(uint16_t *)(void *)(s_weather.global + 0x5de) = 0x5678;
    put_u32(s_weather.global, 0x268, number & 2 ? number | 1 : 0);
    memcpy(module->data, module->initial_data, 0x1000); texture_setup_host(host);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_weather.global);
    put_u32(module->data, 0xad4, number & 4 ? number | 1 : 0);
    put_u32(module->data, 0xad8, random_u32(&random_state));
    put_u32(module->data, 0xe08, number % 4 == 0 ? 0xffffffffu : random_u32(&random_state));
    for (index = 0; index < 3; index++) {
        uint64_t bits = s_velocity_bits[(number / 8 + index) % 9];
        memcpy(module->data + 0xa88 + index * 8, &bits, 8);
    }
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    __asm__ volatile("fnstcw %0" : "=m"(original_control));
    control_word = (original_control & ~0xf00u) | (const uint16_t[]){0, 0x200, 0x300}[number / 8 % 3] | ((number / 24) % 4) << 10;
    __asm__ volatile("fldcw %0" : : "m"(control_word));
    s_weather.returns[0] = ((multip_worker_fn)send_internal(module, 197))();
    for (step = 0; step < 3; step++) {
        uint32_t id = random_u32(&random_state), seed = random_u32(&random_state);
        __asm__ volatile("fnclex");
        s_weather.returns[step + 1] = state_call_painted(send_internal(module, 198 + step), id, seed);
        s_weather.signals[step] = WaitForSingleObject(event, 0);
    }
    __asm__ volatile("fnstcw %0" : "=m"(control_word));
    s_weather.returns[4] = control_word;
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(original_control));
    s_weather.returns[5] = get_u32(module->data, 0xe08);
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
    memcpy(results, &s_weather, sizeof(s_weather));
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->heap[index], byte, address);
        }
    }
}
static uint32_t test_state_messages(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct weather_fixture s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("state_messages", "retail", number); run_state_messages(retail, number, &s_retail);
        multip_test_context("state_messages", "ours", number); run_state_messages(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("state_messages scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                        (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP aircraft state message constructors: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
