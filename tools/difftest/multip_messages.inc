/* Fixed-header application messages through the real port's send queues. */
struct message_results {
    struct send_results heap;
    uint32_t returns[15];
    uint32_t signals[15];
    uint32_t state[15][4];
    uint32_t ticks;
};
static uint32_t s_message_ticks;
static char s_message_title[80];
static uint8_t *s_message_global;
static uint32_t s_message_number;
static uint32_t STDCALL message_ticks(void) { return ++s_message_ticks; }

static int32_t STDCALL message_title(void *aircraft, uint32_t kind, char *out, uint32_t capacity)
{
    uint32_t *entry = s_send_trace.log[s_send_trace.calls++];
    entry[0] = 10; entry[1] = aircraft == s_message_global + 0x11e0;
    entry[2] = kind; entry[3] = capacity;
    strcpy(out, s_message_title);
    return (int32_t)s_message_number;
}
static uint32_t STDCALL message_strlen(const char *text)
{
    uint32_t *entry = s_send_trace.log[s_send_trace.calls++];
    entry[0] = 11; entry[1] = profile_hash(text);
    return strlen(text);
}
static int32_t STDCALL message_strcpy(char *dest, const char *source)
{
    uint32_t *entry = s_send_trace.log[s_send_trace.calls++];
    entry[0] = 12; entry[1] = profile_hash(source);
    strcpy(dest, source);
    return (int32_t)s_message_number;
}

typedef int32_t (STDCALL *message_id_fn)(uint32_t);
typedef int32_t (STDCALL *message_value_fn)(uint32_t, uint32_t);

static void run_messages(const struct multip_module *module, uint32_t number,
                         struct message_results *results)
{
    struct send_queue queues[2];
    uint8_t host[0x320] = {0};
    uint8_t global[0x1200] = {0};
    void *aircraft_linkage[16] = {0};
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t random_state = number ^ 0x15673824u;
    uint32_t index;
    uint32_t id;
    void *function;

    if (event == NULL) exit(2);
    memset(results, 0, sizeof(*results));
    memset(&s_send_trace, 0, sizeof(s_send_trace));
    memset(queues, 0, sizeof(queues));
    memcpy(module->data, module->initial_data, 0x1000);
    s_send_trace.fail_call = number % 3 == 0 ? 1 + (number / 3) % 89 : 0;
    s_send_free_calls = 0;
    s_send_fail_free = number % 5 == 0 ? 1 + (number / 5) % 4 : 0;
    s_message_ticks = random_u32(&random_state);
    function = message_ticks; memcpy(s_send_host + 0x38, &function, 4);
    function = send_alloc; memcpy(s_send_host + 0x20c, &function, 4);
    function = send_free; memcpy(s_send_host + 0x214, &function, 4);
    function = send_copy; memcpy(s_send_host + 0x21c, &function, 4);
    memcpy(host, s_send_host, sizeof(s_send_host));
    function = message_strlen; memcpy(host + 0x2b8, &function, 4);
    function = message_strcpy; memcpy(host + 0x2a8, &function, 4);
    aircraft_linkage[0x30 / 4] = (void *)message_title;
    s_message_global = global;
    s_message_number = number;
    for (index = 0; index < number % 80; index++)
        s_message_title[index] = 'A' + random_u32(&random_state) % 26;
    s_message_title[number % 80] = 0;
    global[0x609] = (uint8_t)random_u32(&random_state);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)global);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft_linkage);
    for (index = 0; index < 2; index++) InitializeCriticalSection(&queues[index].lock);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0xe60, number % 7 == 0 ? 0 : (uint32_t)(uintptr_t)&queues[0]);
    put_u32(module->data, 0xe64, (uint32_t)(uintptr_t)&queues[1]);
    put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, 0x984, random_u32(&random_state));
    put_u32(module->data, 0xadc, random_u32(&random_state));
    for (index = 0; index < 15; index++) {
        id = random_u32(&random_state);
        if (index == 14)
            results->returns[index] = ((multip_worker_fn)send_internal(module, 84))();
        else if (index < 4 || index == 11)
            results->returns[index] = ((multip_worker_fn)send_internal(module, 59 + index))();
        else if (index < 6)
            results->returns[index] = ((message_value_fn)send_internal(module, 59 + index))(
                id, random_u32(&random_state));
        else
            results->returns[index] = ((message_id_fn)send_internal(module, 59 + index))(id);
        results->signals[index] = WaitForSingleObject(event, 0);
        results->state[index][0] = get_u32(module->data, 0x984);
        results->state[index][1] = get_u32(module->data, 0xadc);
        results->state[index][2] = queues[0].count;
        results->state[index][3] = queues[1].count;
    }
    for (index = 0; index < 2; index++) {
        results->heap.queues[index][0] = queues[index].count;
        results->heap.queues[index][1] = send_normalise(queues[index].head);
        results->heap.queues[index][2] = send_normalise(queues[index].tail);
        results->heap.queues[index][3] = queues[index].lock.LockCount;
        results->heap.queues[index][4] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount != 0)
            LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    snapshot_send_heap(&results->heap);
    results->ticks = s_message_ticks;
    CloseHandle(event);
}

static uint32_t test_messages(const struct multip_module *retail,
                              const struct multip_module *ours, uint32_t scenarios)
{
    static struct message_results s_retail;
    static struct message_results s_ours;
    uint32_t number;
    uint32_t bad = 0;
    for (number = 0; number < scenarios; number++) {
        run_messages(retail, number, &s_retail);
        run_messages(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("message scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP application message constructors: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
