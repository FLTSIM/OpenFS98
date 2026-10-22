/* Receive-one and receive-dequeue fixtures share the deterministic heap. */
#define RECEIVE_ACTIONS 3

struct receive_results {
    struct send_results heap;
    uint32_t returns[RECEIVE_ACTIONS];
    uint32_t received[RECEIVE_ACTIONS];
    uint32_t counts[RECEIVE_ACTIONS];
    uint32_t take_returns[RECEIVE_ACTIONS + 1];
    uint32_t take_received[RECEIVE_ACTIONS + 1];
    uint32_t take_records[RECEIVE_ACTIONS + 1];
};

static uint32_t s_receive_steps;
static uint32_t s_receive_resizes;
static uint32_t s_receive_result;
static uint32_t s_receive_seed;
static uint32_t s_receive_timestamp;

typedef int32_t (STDCALL *multip_receive_fn)(uint32_t *);
typedef int32_t (STDCALL *multip_receive_take_fn)(void **, uint32_t *);

static int32_t STDCALL receive_realloc(int32_t flags, uint32_t size, void **pointer)
{
    uint32_t call = s_send_trace.calls++;
    uint32_t block = send_normalise(*pointer);
    uint32_t index = s_send_trace.blocks;
    uint32_t old_size;

    s_send_trace.log[call][0] = 7;
    s_send_trace.log[call][1] = flags;
    s_send_trace.log[call][2] = size;
    s_send_trace.log[call][3] = block;
    if (block == 0 || block > index || !s_send_trace.live[block - 1] ||
        index >= SEND_BLOCKS || size > SEND_BLOCK_SIZE) {
        fprintf(stderr, "receive fixture invalid realloc\n");
        exit(2);
    }
    if (s_send_trace.calls == s_send_trace.fail_call)
        return -1;
    old_size = s_send_trace.sizes[block - 1];
    memset(s_send_trace.heap[index].words, 0xa5, size);
    memcpy(s_send_trace.heap[index].words, *pointer,
           old_size < size ? old_size : size);
    s_send_trace.sizes[index] = size;
    s_send_trace.live[index] = 1;
    s_send_trace.live[block - 1] = 0;
    s_send_trace.blocks++;
    *pointer = s_send_trace.heap[index].words;
    return 0;
}

static uint32_t STDCALL receive_ticks(void)
{
    uint32_t call = s_send_trace.calls++;

    s_send_trace.log[call][0] = 6;
    s_send_trace.log[call][1] = s_receive_timestamp;
    return s_receive_timestamp;
}

static int32_t STDCALL fake_dp_receive(void *object, uint32_t *from_id,
                                       uint32_t *to_id, uint32_t flags,
                                       void *payload, uint32_t *size)
{
    uint32_t call = s_send_trace.calls++;
    uint32_t index = s_send_trace.packet_count++;
    uint32_t step = s_receive_steps++;
    struct send_packet *packet;
    uint32_t byte_index;
    uint32_t actual_size;

    if (index >= SEND_PACKETS || *size > 96) {
        fprintf(stderr, "receive fixture runaway call/invalid capacity\n");
        exit(2);
    }
    packet = &s_send_trace.packets[index];
    packet->object_ok = object == &s_send_object;
    packet->from_id = *from_id;
    packet->to_id = *to_id;
    packet->flags = flags;
    packet->size = *size;
    s_send_trace.log[call][0] = 5;
    s_send_trace.log[call][1] = send_normalise(payload);
    s_send_trace.log[call][2] = index;
    *from_id = s_receive_seed ^ step;
    *to_id = s_receive_seed + step + 17;
    if (step < s_receive_resizes) {
        if (payload != NULL && *size != 0)
            memset(payload, 0x80 + step, *size < 4 ? *size : 4);
        *size = 16 * (step + 1);
        packet->result = 0x8877001eu;
    } else {
        packet->result = s_receive_result;
        actual_size = s_receive_resizes == 0 ? 0 : 16 * s_receive_resizes - 3;
        if (payload != NULL && *size >= actual_size) {
            for (byte_index = 0; byte_index < actual_size; byte_index++)
                ((uint8_t *)payload)[byte_index] = (uint8_t)(s_receive_seed + byte_index);
            memcpy(packet->payload, payload, actual_size);
        } else if (actual_size != 0) {
            fprintf(stderr, "receive fixture undersized final buffer\n");
            exit(2);
        }
        *size = actual_size;
    }
    packet->out_from_id = *from_id;
    packet->out_to_id = *to_id;
    packet->out_size = *size;
    return (int32_t)packet->result;
}

static void run_receive(const struct multip_module *module, uint32_t number,
                         struct receive_results *results)
{
    struct send_queue queue;
    multip_receive_fn receive = (multip_receive_fn)send_internal(module, 3);
    multip_receive_take_fn take = (multip_receive_take_fn)send_internal(module, 4);
    uint32_t action;
    uint32_t mode;
    void *record;

    memset(results, 0, sizeof(*results));
    memset(&s_send_trace, 0, sizeof(s_send_trace));
    memset(&queue, 0, sizeof(queue));
    InitializeCriticalSection(&queue.lock);
    /* Rotate call positions independently of the failure enable period. */
    s_send_trace.fail_call = number % 3 == 0 ? 1 + (number / 3) % 22 : 0;
    s_send_free_calls = 0;
    s_send_fail_free = number % 5 == 0 ? 1 + (number / 5) % 6 : 0;
    put_u32(module->data, 0x004, (uint32_t)(uintptr_t)s_send_host);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_send_object);
    put_u32(module->data, 0xdf8, number % 13 == 0 ? 0 :
            (uint32_t)(uintptr_t)&queue);
    for (action = 0; action < RECEIVE_ACTIONS; action++) {
        mode = (number + action) % 8;
        s_receive_resizes = mode == 6 ? 4 : mode % 3;
        s_receive_result = mode == 0 || mode == 4 || mode == 5 ?
            0x887700beu : mode == 7 ? 0x80004005u : 0;
        s_receive_steps = 0;
        s_receive_seed = (number * 0x1234567u) ^ action;
        s_receive_timestamp = number ^ (action * 0x81234567u);
        results->received[action] = 0xbadc0deu;
        results->returns[action] = receive(&results->received[action]);
        results->counts[action] = queue.count;
    }
    for (action = 0; action <= RECEIVE_ACTIONS; action++) {
        record = NULL;
        results->take_received[action] = 0xbadc0deu;
        results->take_returns[action] = take(&record, &results->take_received[action]);
        results->take_records[action] = send_normalise(record);
    }
    results->heap.queues[0][0] = queue.count;
    results->heap.queues[0][1] = send_normalise(queue.head);
    results->heap.queues[0][2] = send_normalise(queue.tail);
    results->heap.queues[0][3] = queue.lock.LockCount;
    results->heap.queues[0][4] = queue.lock.RecursionCount;
    snapshot_send_heap(&results->heap);
    while (queue.lock.RecursionCount != 0)
        LeaveCriticalSection(&queue.lock);
    DeleteCriticalSection(&queue.lock);
}

static uint32_t test_receive(const struct multip_module *retail,
                             const struct multip_module *ours, uint32_t scenarios)
{
    struct receive_results retail_results;
    struct receive_results our_results;
    uint32_t number;
    uint32_t bad = 0;
    uint32_t index;
    void *realloc_function = receive_realloc;
    void *ticks_function = receive_ticks;

    {
        void *alloc_function = send_alloc, *free_function = send_free, *copy_function = send_copy;
        memcpy(s_send_host + 0x20c, &alloc_function, 4);
        memcpy(s_send_host + 0x214, &free_function, 4);
        memcpy(s_send_host + 0x21c, &copy_function, 4);
    }
    s_send_vtable[0x64 / 4] = (void *)fake_dp_receive;
    memcpy(s_send_host + 0x210, &realloc_function, sizeof(realloc_function));
    memcpy(s_send_host + 0x38, &ticks_function, sizeof(ticks_function));
    for (number = 0; number < scenarios; number++) {
        run_receive(retail, number, &retail_results);
        run_receive(ours, number, &our_results);
        if (memcmp(&retail_results, &our_results, sizeof(our_results)) != 0) {
            if (bad < 8) {
                const uint32_t *retail_words = (const uint32_t *)&retail_results;
                const uint32_t *our_words = (const uint32_t *)&our_results;
                printf("receive scenario %lu differs\n", (unsigned long)number);
                for (index = 0; index < sizeof(our_results) / 4; index++)
                    if (retail_words[index] != our_words[index])
                        printf("  word %lu: retail %#lx, ours %#lx\n",
                               (unsigned long)index, (unsigned long)retail_words[index],
                               (unsigned long)our_words[index]);
            }
            bad++;
        }
    }
    printf("MULTIP receive-one/dequeue: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
