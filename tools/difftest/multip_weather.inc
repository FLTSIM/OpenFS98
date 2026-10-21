/* Weather copy order, trailing caller-stack bytes and per-peer sync packets. */
struct weather_fixture {
    uint32_t calls, blocks, frees, ticks, number, fail_call, fail_free;
    uint32_t log[512][6], sizes[128], live[128], returns[8], signals[8], queues[2][4];
    uint8_t heap[128][1024], global[0x1200], player[256];
};
static struct weather_fixture s_weather;
static uint32_t weather_address(const void *pointer)
{
    uintptr_t value = (uintptr_t)pointer, base = (uintptr_t)s_weather.heap;
    if (value >= base && value < base + sizeof(s_weather.heap)) return 0xc000 + value - base;
    return value == 0 ? 0 : 0xffffffffu;
}
static uint32_t *weather_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_weather.calls >= 512) exit(2);
    entry = s_weather.log[s_weather.calls++]; entry[0] = tag; return entry;
}
static int32_t STDCALL weather_alloc(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    uint32_t *entry = weather_log(1), block = s_weather.blocks;
    entry[1] = flags; entry[2] = size; entry[3] = zero;
    if (s_weather.calls == s_weather.fail_call) { *out = NULL; return -1; }
    if (block >= 128 || size > 1024) { fprintf(stderr, "weather heap limit: block %lu size %lu scenario %lu\n", (unsigned long)block, (unsigned long)size, (unsigned long)s_weather.number); exit(2); }
    *out = s_weather.heap[block]; s_weather.sizes[block] = size; s_weather.live[block] = 1;
    memset(*out, zero ? 0 : 0xa5, size); s_weather.blocks++;
    return 0;
}
static int32_t STDCALL weather_copy(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = weather_log(2);
    entry[1] = weather_address(dest); entry[2] = size; entry[3] = draw_hash(source, size);
    if (s_weather.calls == s_weather.fail_call) return -1;
    memmove(dest, source, size); return 0;
}
static int32_t STDCALL weather_free(void **pointer)
{
    uint32_t *entry = weather_log(3), index;
    entry[1] = weather_address(*pointer);
    s_weather.frees++;
    if (s_weather.frees == s_weather.fail_free) return -1;
    for (index = 0; index < s_weather.blocks; index++) if (*pointer == s_weather.heap[index]) {
        if (!s_weather.live[index]) { fprintf(stderr, "weather double free: block %lu scenario %lu\n", (unsigned long)index, (unsigned long)s_weather.number); exit(2); }
        s_weather.live[index] = 0; *pointer = NULL; return 0;
    }
    fprintf(stderr, "weather unknown free: pointer %p scenario %lu\n", *pointer, (unsigned long)s_weather.number); exit(2);
}
static uint32_t STDCALL weather_ticks(void)
{
    weather_log(4);
    put_u32(s_weather.player, 0x3c, get_u32(s_weather.player, 0x3c) ^ 0x12345678u);
    put_u32(s_weather.player, 0x40, ++s_weather.ticks);
    return s_weather.ticks;
}
__attribute__((naked)) static int32_t weather_call_painted(
    void *function __attribute__((unused)), uint32_t id __attribute__((unused)), uint32_t padding __attribute__((unused)))
{
    __asm__ volatile("movl 4(%esp), %eax\n\tmovl 8(%esp), %edx\n\tmovl 12(%esp), %ecx\n\t"
        "subl $0x400, %esp\n\tmovl %ecx, 0x250(%esp)\n\tnotl %ecx\n\t"
        "movl %ecx, 0x24c(%esp)\n\tmovl %ecx, 0x254(%esp)\n\t"
        "addl $0x300, %esp\n\tpushl %edx\n\tcall *%eax\n\taddl $0x100, %esp\n\tret");
}
static uint32_t STDCALL weather_main(uint32_t mode, uint32_t value)
{
    uint32_t *entry = weather_log(5); entry[1] = mode; entry[2] = value; return 0xffffffffu;
}
static uint32_t STDCALL weather_refresh(void) { weather_log(6); return 0xffffffffu; }
__attribute__((naked)) static int32_t weather_request_painted(
    void *function __attribute__((unused)), uint32_t id __attribute__((unused)), uint32_t padding __attribute__((unused)))
{
    __asm__ volatile("movl 4(%esp), %eax\n\tmovl 8(%esp), %edx\n\tmovl 12(%esp), %ecx\n\t"
        "subl $0x400, %esp\n\tmovl %ecx, 0x240(%esp)\n\tnotl %ecx\n\t"
        "movl %ecx, 0x23c(%esp)\n\tmovl %ecx, 0x244(%esp)\n\t"
        "addl $0x300, %esp\n\tpushl $0\n\tpushl $0\n\tpushl %edx\n\t"
        "call *%eax\n\taddl $0x100, %esp\n\tret");
}
static void run_weather(const struct multip_module *module, uint32_t number, struct weather_fixture *results)
{
    uint8_t host[0x220] = {0}, packet[0x25c];
    void *main_linkage[30] = {0}, *weather_linkage[10] = {0};
    typedef int32_t (STDCALL *weather_receive_fn)(uint32_t, const void *, uint32_t);
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    void *function;
    uint32_t index, step, byte, random_state = number ^ 0x17264839u;

    if (!event) exit(2);
    memset(&s_weather, 0, sizeof(s_weather)); memset(queues, 0, sizeof(queues));
    memcpy(module->data, module->initial_data, 0x1000);
    s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + (number / 3) % 140 : 0;
    s_weather.fail_free = number % 5 == 0 ? 1 + (number / 5) % 3 : 0;
    s_weather.ticks = random_u32(&random_state);
    for (index = 0; index < sizeof(s_weather.global); index++) s_weather.global[index] = random_u32(&random_state);
    for (index = 0; index < sizeof(s_weather.player); index++) s_weather.player[index] = random_u32(&random_state);
    function = weather_alloc; memcpy(host + 0x20c, &function, 4);
    function = weather_free; memcpy(host + 0x214, &function, 4);
    function = weather_copy; memcpy(host + 0x21c, &function, 4);
    function = weather_ticks; memcpy(host + 0x38, &function, 4);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_weather.global);
    put_u32(module->data, 0xf00, number % 4);
    main_linkage[0x74 / 4] = weather_main; weather_linkage[0x24 / 4] = weather_refresh;
    put_u32(module->data, 0x34, (uint32_t)(uintptr_t)main_linkage);
    put_u32(module->data, 0x4c, (uint32_t)(uintptr_t)weather_linkage);
    put_u32(module->data, 0xaa8, number & 1);
    for (index = 0; index < sizeof(packet); index++) packet[index] = random_u32(&random_state);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
    }
    for (step = 0; step < 8; step++) {
        uint32_t id = random_u32(&random_state), padding = random_u32(&random_state);
        if (step == 6) s_weather.returns[step] = ((weather_receive_fn)send_internal(module, 120))(id, packet, 0);
        else if (step == 7) s_weather.returns[step] = weather_request_painted(send_internal(module, 121), id, padding);
        else if (step == 3) s_weather.returns[step] = ((multip_worker_fn)send_internal(module, 104))();
        else if (step == 1 || step == 4) s_weather.returns[step] = ((track_show_fn)send_internal(module, 106))((uint32_t)(uintptr_t)s_weather.player);
        else s_weather.returns[step] = weather_call_painted(send_internal(module, 105), id, padding);
        s_weather.signals[step] = WaitForSingleObject(event, 0);
    }
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
    memcpy(results, &s_weather, sizeof(*results));
    for (index = 0; index < s_weather.blocks; index++)
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t value = get_u32(s_weather.heap[index], byte);
            uint32_t normalized = weather_address((const void *)(uintptr_t)value);
            if (normalized != 0xffffffffu) put_u32(results->heap[index], byte, normalized);
        }
}
static uint32_t test_weather(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct weather_fixture s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("weather", "retail", number); run_weather(retail, number, &s_retail);
        multip_test_context("weather", "ours", number); run_weather(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("weather scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP weather/sync messages: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
