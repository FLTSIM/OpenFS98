/* Send fixtures: real critical sections/events and a deterministic host heap.
 * Compare enqueue and internal send/drain calls, including leaked records. */
#define SEND_ACTIONS 6
#define SEND_BLOCKS 64
#define SEND_BLOCK_SIZE 128
#define SEND_LOG_CALLS 256
#define SEND_PACKETS 32

struct send_node {
    void *record;
    struct send_node *next;
};

struct send_queue {
    uint32_t count;
    struct send_node *head;
    struct send_node *tail;
    CRITICAL_SECTION lock;
};

struct send_heap_block {
    uint32_t words[SEND_BLOCK_SIZE / 4];
};

struct send_packet {
    uint32_t object_ok;
    uint32_t from_id;
    uint32_t to_id;
    uint32_t flags;
    uint32_t size;
    uint32_t result;
    uint32_t out_from_id;
    uint32_t out_to_id;
    uint32_t out_size;
    uint8_t payload[96];
};

struct send_trace {
    uint32_t calls;
    uint32_t fail_call;
    uint32_t blocks;
    uint32_t log[SEND_LOG_CALLS][4];
    uint32_t sizes[SEND_BLOCKS];
    uint32_t zeroes[SEND_BLOCKS];
    struct send_heap_block heap[SEND_BLOCKS];
    uint32_t live[SEND_BLOCKS];
    uint32_t packet_count;
    struct send_packet packets[SEND_PACKETS];
};

struct send_results {
    uint32_t returns[SEND_ACTIONS];
    uint32_t signals[SEND_ACTIONS];
    uint32_t queues[2][5];
    uint32_t drain_returns[4];
    uint32_t drain_counts[4][2];
    uint32_t calls;
    uint32_t blocks;
    uint32_t log[SEND_LOG_CALLS][4];
    uint32_t sizes[SEND_BLOCKS];
    uint32_t zeroes[SEND_BLOCKS];
    struct send_heap_block heap[SEND_BLOCKS];
    uint32_t live[SEND_BLOCKS];
    uint32_t packet_count;
    struct send_packet packets[SEND_PACKETS];
};

static struct send_trace s_send_trace;
static uint8_t s_send_host[0x220];
static void *s_send_vtable[27];
static void **s_send_object = s_send_vtable;
static uint32_t s_send_free_calls;
static uint32_t s_send_fail_free;
static uint32_t s_send_retry_count;
static uint32_t s_send_terminal_result;

typedef int32_t (STDCALL *multip_send_one_fn)(struct send_queue *);
typedef int32_t (*multip_send_drain_fn)(void);

typedef uint32_t (STDCALL *multip_send_fn)(uint32_t, const void *,
                                         uint32_t, uint32_t);

static uint32_t send_normalise(const void *pointer)
{
    uint32_t index;

    if (pointer == NULL)
        return 0;
    for (index = 0; index < s_send_trace.blocks; index++)
        if (pointer == s_send_trace.heap[index].words)
            return index + 1;
    return 0xffffffffu;
}

static int32_t STDCALL send_alloc(int32_t flags, uint32_t size,
                                  void **out, int32_t zero)
{
    uint32_t call = s_send_trace.calls++;
    uint32_t index = s_send_trace.blocks;

    s_send_trace.log[call][0] = 1;
    s_send_trace.log[call][1] = flags;
    s_send_trace.log[call][2] = size;
    s_send_trace.log[call][3] = zero;
    if (s_send_trace.calls == s_send_trace.fail_call) {
        *out = NULL;
        return -1;
    }
    if (index >= SEND_BLOCKS || size > SEND_BLOCK_SIZE) {
        fprintf(stderr, "send fixture heap exhausted\n");
        exit(2);
    }
    s_send_trace.blocks++;
    s_send_trace.live[index] = 1;
    s_send_trace.sizes[index] = size;
    s_send_trace.zeroes[index] = zero;
    *out = s_send_trace.heap[index].words;
    memset(*out, zero ? 0 : 0xa5, size);
    return 0;
}

static int32_t STDCALL send_copy(void *dest, const void *source, uint32_t size)
{
    uint32_t call = s_send_trace.calls++;

    s_send_trace.log[call][0] = 2;
    s_send_trace.log[call][1] = send_normalise(dest);
    s_send_trace.log[call][2] = size;
    if (s_send_trace.calls == s_send_trace.fail_call)
        return -1;
    memmove(dest, source, size);
    return 0;
}

static int32_t STDCALL send_free(void **pointer)
{
    uint32_t call = s_send_trace.calls++;
    uint32_t block = send_normalise(*pointer);

    s_send_trace.log[call][0] = 3;
    s_send_trace.log[call][1] = block;
    if (block == 0 || block > s_send_trace.blocks ||
        !s_send_trace.live[block - 1]) {
        fprintf(stderr, "send fixture invalid/double free\n");
        exit(2);
    }
    s_send_free_calls++;
    if (s_send_free_calls == s_send_fail_free)
        return -1;
    s_send_trace.live[block - 1] = 0;
    *pointer = NULL;
    return 0;
}

static int32_t STDCALL fake_dp_send(void *object, uint32_t from_id,
                                    uint32_t to_id, uint32_t flags,
                                    void *payload, uint32_t size)
{
    uint32_t index = s_send_trace.packet_count++;
    uint32_t call = s_send_trace.calls++;
    struct send_packet *packet;

    if (index >= SEND_PACKETS || size > 96) {
        fprintf(stderr, "send fixture runaway send/invalid size\n");
        exit(2);
    }
    packet = &s_send_trace.packets[index];
    packet->object_ok = object == &s_send_object;
    packet->from_id = from_id;
    packet->to_id = to_id;
    packet->flags = flags;
    packet->size = size;
    packet->result = index % (s_send_retry_count + 1) < s_send_retry_count ?
        0x8877010eu : s_send_terminal_result;
    memcpy(packet->payload, payload, size);
    s_send_trace.log[call][0] = 4;
    s_send_trace.log[call][1] = index;
    return (int32_t)packet->result;
}

/* Retail internal entry addresses are relative to the loaded PE, so this
 * remains valid if Wine relocates it. The port exports a test-only table. */
static void *send_internal(const struct multip_module *module, uint32_t index)
{
    static const uint32_t s_retail_rvas[] = {
        0x89d0, 0x8970, 0x8aa0, 0x6820, 0x6980,
        0x66a0, 0x66e0, 0x6730, 0x6770, 0x67a0,
        0x8700, 0x8750, 0x87b0, 0x87f0, 0x8840,
        0x5ce0, 0x5d50, 0x5ed0, 0xae00, 0xae20, 0xae40, 0xae70,
        0xacf0, 0xad20, 0xad50, 0x9f30, 0xa3e0,
        0x3380, 0x33c0, 0x3340, 0x4940, 0x4890, 0xa7a0, 0xa7f0,
        0xb7a0, 0xaf40, 0xb020, 0xb050, 0xb100, 0xb1a0, 0xb590, 0xb8d0,
        0x9a20, 0x9aa0, 0x9b30, 0xb2a0, 0xb360, 0xb4d0, 0xb540,
        0xb470, 0xb3c0, 0xa710, 0x9cb0, 0xa6b0, 0xa740,
        0xb830, 0xb680, 0xaea0, 0xb6b0,
        0x6a80, 0x6ae0, 0x6b40, 0x6ba0, 0x6c00, 0x6c50,
        0x6ca0, 0x6ce0, 0x6dd0, 0x6e10, 0x6e50, 0x6ec0, 0x6f30, 0x6d20,
        0x9930, 0x9980, 0x92c0, 0xb8a0, 0x94b0, 0x9f00, 0x9820, 0x9700,
        0x96b0, 0xa960, 0x7010, 0x7060, 0x3c10, 0x3bc0, 0x3be0,
        0x3d30, 0x3d20, 0x3cd0, 0x3cf0
    };
    void **entries = (void **)GetProcAddress(module->module_handle,
                                            "multip_test_entries");

    return entries != NULL ? entries[index] :
        (uint8_t *)module->module_handle + s_retail_rvas[index];
}

static void snapshot_send_heap(struct send_results *results)
{
    uint32_t index;

    results->calls = s_send_trace.calls;
    results->blocks = s_send_trace.blocks;
    results->packet_count = s_send_trace.packet_count;
    memcpy(results->packets, s_send_trace.packets, sizeof(results->packets));
    memcpy(results->live, s_send_trace.live, sizeof(results->live));
    memcpy(results->log, s_send_trace.log, sizeof(results->log));
    memcpy(results->sizes, s_send_trace.sizes, sizeof(results->sizes));
    memcpy(results->zeroes, s_send_trace.zeroes, sizeof(results->zeroes));
    memcpy(results->heap, s_send_trace.heap, sizeof(results->heap));
    for (index = 0; index < s_send_trace.blocks; index++) {
        uint32_t *words = results->heap[index].words;
        /* Only zero-filled records/nodes contain pointers. Payload stays raw. */
        if (s_send_trace.zeroes[index]) {
            if (s_send_trace.sizes[index] == 20)
                words[2] = send_normalise((void *)(uintptr_t)words[2]);
            else {
                words[1] = send_normalise((void *)(uintptr_t)words[1]);
                if (s_send_trace.sizes[index] == 8)
                    words[0] = send_normalise((void *)(uintptr_t)words[0]);
            }
        }
    }
}

static void run_send(const struct multip_module *module, uint32_t number,
                      struct send_results *results, uint32_t delivery)
{
    struct send_queue queues[2];
    uint8_t payload[96];
    uint8_t local_player[0x100] = {0};
    uint32_t actions = delivery ? number % (SEND_ACTIONS + 1) : SEND_ACTIONS;
    uint32_t state = 0x14253647u ^ number;
    uint32_t index;
    uint32_t action;
    uint32_t flags;
    uint32_t size;
    uint32_t missing_queue = number % 11;
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    multip_send_fn send = (multip_send_fn)get_slot(module, 0x20);

    if (event == NULL) {
        fprintf(stderr, "cannot create send fixture event\n");
        exit(2);
    }
    memset(results, 0, sizeof(*results));
    memset(&s_send_trace, 0, sizeof(s_send_trace));
    /* Host failure injection starts after the first two sends. */
    s_send_trace.fail_call = !delivery && number % 3 == 0 ?
        9 + number % 16 : 0;
    s_send_free_calls = 0;
    s_send_fail_free = delivery && number % 3 == 0 ? 1 + (number / 3) % 18 : 0;
    s_send_retry_count = number % 4;
    s_send_terminal_result = number % 3 == 0 ? 0x80004005u :
        number % 3 == 1 ? 0x8877001eu : 0;
    put_u32(local_player, 4, random_u32(&state));
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)local_player);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_send_object);
    memset(queues, 0, sizeof(queues));
    for (index = 0; index < 2; index++)
        InitializeCriticalSection(&queues[index].lock);
    put_u32(module->data, 0x004, (uint32_t)(uintptr_t)s_send_host);
    put_u32(module->data, 0xe60, missing_queue == 0 ? 0 :
            (uint32_t)(uintptr_t)&queues[0]);
    put_u32(module->data, 0xe64, missing_queue == 1 ? 0 :
            (uint32_t)(uintptr_t)&queues[1]);
    put_u32(module->data, 0xe5c, number % 7 == 0 ? 0 :
            (uint32_t)(uintptr_t)event);
    put_u32(module->data, 0xf00, number % 4);
    put_u32(module->data, M_CONNECTED, number & 1);

    for (action = 0; action < actions; action++) {
        for (index = 0; index < sizeof(payload); index++)
            payload[index] = (uint8_t)random_u32(&state);
        flags = action == 0 ? 0 : action == 1 ? 1 :
                action == 2 ? 2 : random_u32(&state);
        if (delivery && action >= 3 && ((action + number) & 1) == 0)
            flags = 0;
        size = action == 0 ? 0 : random_u32(&state) % sizeof(payload);
        results->returns[action] = send(random_u32(&state), payload, size, flags);
        results->signals[action] = WaitForSingleObject(event, 0);
        /* Prove enqueue owns a copy of the input before inspecting it. */
        memset(payload, 0xcc, sizeof(payload));
    }
    if (delivery) {
        multip_send_one_fn send_one =
            (multip_send_one_fn)send_internal(module, 0);
        multip_send_drain_fn drain =
            (multip_send_drain_fn)send_internal(module, 1);
        multip_send_drain_fn drain_all =
            (multip_send_drain_fn)send_internal(module, 2);
        for (action = 0; action < 4; action++) {
            if (action == 0 && number % 5 == 0)
                results->drain_returns[action] = drain_all();
            else if (action == 0)
                results->drain_returns[action] = send_one(missing_queue == 1 ?
                    NULL : &queues[1]);
            else if (action == 1)
                results->drain_returns[action] = drain();
            else
                results->drain_returns[action] = drain_all();
            for (index = 0; index < 2; index++)
                results->drain_counts[action][index] = queues[index].count;
        }
    }
    for (index = 0; index < 2; index++) {
        results->queues[index][0] = queues[index].count;
        results->queues[index][1] = send_normalise(queues[index].head);
        results->queues[index][2] = send_normalise(queues[index].tail);
        results->queues[index][3] = queues[index].lock.LockCount;
        results->queues[index][4] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount != 0)
            LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    snapshot_send_heap(results);
    CloseHandle(event);
}

static uint32_t test_send(const struct multip_module *retail,
                           const struct multip_module *ours, uint32_t scenarios,
                           uint32_t delivery)
{
    struct send_results retail_results;
    struct send_results our_results;
    uint32_t number;
    uint32_t bad = 0;
    uint32_t index;
    void *alloc_function = send_alloc;
    void *copy_function = send_copy;
    void *free_function = send_free;

    if (GetProcAddress(ours->module_handle, "multip_test_entries") == NULL) {
        fprintf(stderr, "MULTIP differential build lacks internal entries\n");
        exit(2);
    }
    s_send_vtable[0x68 / 4] = (void *)fake_dp_send;
    memcpy(s_send_host + 0x214, &free_function, sizeof(free_function));
    memcpy(s_send_host + 0x20c, &alloc_function, sizeof(alloc_function));
    memcpy(s_send_host + 0x21c, &copy_function, sizeof(copy_function));
    for (number = 0; number < scenarios; number++) {
        run_send(retail, number, &retail_results, delivery);
        run_send(ours, number, &our_results, delivery);
        if (memcmp(&retail_results, &our_results, sizeof(our_results)) != 0) {
            if (bad < 8) {
                const uint32_t *retail_words = (const uint32_t *)&retail_results;
                const uint32_t *our_words = (const uint32_t *)&our_results;
                printf("send %s scenario %lu differs\n",
                       delivery ? "delivery" : "enqueue", (unsigned long)number);
                for (index = 0; index < sizeof(our_results) / 4; index++)
                    if (retail_words[index] != our_words[index])
                        printf("  word %lu: retail %#lx, ours %#lx\n",
                               (unsigned long)index,
                               (unsigned long)retail_words[index],
                               (unsigned long)our_words[index]);
            }
            bad++;
        }
    }
    printf("MULTIP %s: %lu scenarios, %lu differ\n",
           delivery ? "send-one/drain/flush" : "+0x20 enqueue",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
