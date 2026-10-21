/* Controlled Win32 execution: thread creation records callbacks; the fixture
 * invokes them synchronously and scripts waits. No OS threads are launched. */
#define WORKER_LOG_SIZE 512
#define WORKER_STEPS 24

struct worker_trace {
    uint32_t log_count;
    uint32_t log[WORKER_LOG_SIZE][8];
    uint32_t event_calls;
    uint32_t thread_calls;
    uint32_t set_calls;
    uint32_t close_calls;
    uint32_t fail_event;
    uint32_t fail_thread;
    uint32_t fail_set;
    uint32_t fail_close;
    uint32_t signalled[32];
    uint32_t alive[32];
    uint32_t thread_started[2];
    uint32_t receive_remaining;
    uint32_t wait_step;
    uint32_t number;
    uint32_t role;
};

struct worker_results {
    struct worker_trace trace;
    struct send_results heap;
    uint32_t returns[WORKER_STEPS];
    uint32_t state[WORKER_STEPS][14];
};

static struct worker_trace s_worker_trace;
static const struct multip_module *s_worker_module;

typedef int32_t (*multip_worker_fn)(void);
typedef DWORD (WINAPI *multip_worker_thread_fn)(void *);

static uint32_t worker_address(const void *pointer)
{
    uint32_t address = (uint32_t)(uintptr_t)pointer;
    uint32_t index;
    uint32_t base;

    if (pointer == NULL)
        return 0;
    base = (uint32_t)(uintptr_t)s_worker_module->data;
    if (address >= base && address < base + 0x1000)
        return 0x200000u + address - base;
    for (index = 0; index < s_send_trace.blocks; index++) {
        base = (uint32_t)(uintptr_t)s_send_trace.heap[index].words;
        if (address >= base && address < base + SEND_BLOCK_SIZE)
            return 0x100000u + index * SEND_BLOCK_SIZE + address - base;
    }
    return 0x300000u;
}

static uint32_t *worker_log(uint32_t operation)
{
    uint32_t *entry;

    if (s_worker_trace.log_count >= WORKER_LOG_SIZE) {
        fprintf(stderr, "worker fixture call budget exhausted\n");
        exit(2);
    }
    entry = s_worker_trace.log[s_worker_trace.log_count++];
    entry[0] = operation;
    return entry;
}

static void WINAPI worker_cs_init(CRITICAL_SECTION *lock)
{
    worker_log(10)[1] = worker_address(lock);
    memset(lock, 0, sizeof(*lock));
    lock->LockCount = -1;
}

static void WINAPI worker_cs_delete(CRITICAL_SECTION *lock)
{
    worker_log(11)[1] = worker_address(lock);
}

static void WINAPI worker_cs_enter(CRITICAL_SECTION *lock)
{
    worker_log(12)[1] = worker_address(lock);
    lock->LockCount++;
    lock->RecursionCount++;
    lock->OwningThread = (HANDLE)1;
}

static void WINAPI worker_cs_leave(CRITICAL_SECTION *lock)
{
    worker_log(13)[1] = worker_address(lock);
    if (lock->RecursionCount == 0) {
        fprintf(stderr, "worker fixture unbalanced unlock\n");
        exit(2);
    }
    lock->LockCount--;
    lock->RecursionCount--;
    if (lock->RecursionCount == 0)
        lock->OwningThread = NULL;
}

static HANDLE WINAPI worker_event(LPSECURITY_ATTRIBUTES attributes, BOOL manual,
                                   BOOL initial, LPCSTR name)
{
    uint32_t *entry = worker_log(14);
    uint32_t index = ++s_worker_trace.event_calls;

    entry[1] = attributes != NULL;
    entry[2] = manual;
    entry[3] = initial;
    entry[4] = name != NULL;
    if (index == s_worker_trace.fail_event)
        return NULL;
    s_worker_trace.alive[index] = 1;
    s_worker_trace.signalled[index] = initial;
    return (HANDLE)(uintptr_t)(0x10000 + index);
}

static uint32_t worker_handle(HANDLE handle)
{
    uint32_t value = (uint32_t)(uintptr_t)handle;

    return value >= 0x10001 && value < 0x10020 ? value - 0x10000 : 0;
}

static BOOL WINAPI worker_set(HANDLE handle)
{
    uint32_t *entry = worker_log(15);
    uint32_t index = worker_handle(handle);

    entry[1] = (uint32_t)(uintptr_t)handle;
    s_worker_trace.set_calls++;
    if (s_worker_trace.set_calls == s_worker_trace.fail_set || index == 0 ||
        !s_worker_trace.alive[index])
        return FALSE;
    s_worker_trace.signalled[index] = 1;
    return TRUE;
}

static BOOL WINAPI worker_close(HANDLE handle)
{
    uint32_t *entry = worker_log(16);
    uint32_t index = worker_handle(handle);

    entry[1] = (uint32_t)(uintptr_t)handle;
    s_worker_trace.close_calls++;
    if (s_worker_trace.close_calls == s_worker_trace.fail_close || index == 0 ||
        !s_worker_trace.alive[index])
        return FALSE;
    s_worker_trace.alive[index] = 0;
    return TRUE;
}

static HANDLE WINAPI worker_thread_create(LPSECURITY_ATTRIBUTES attributes,
    SIZE_T stack_size, LPTHREAD_START_ROUTINE start, void *parameter,
    DWORD flags, LPDWORD thread_id)
{
    uint32_t *entry = worker_log(17);
    uint32_t role = start == (LPTHREAD_START_ROUTINE)send_internal(s_worker_module, 9) ? 0 :
        start == (LPTHREAD_START_ROUTINE)send_internal(s_worker_module, 14) ? 1 : 2;
    uint32_t index = 8 + ++s_worker_trace.thread_calls;

    entry[1] = worker_address(attributes);
    entry[2] = attributes->nLength;
    entry[3] = (attributes->lpSecurityDescriptor != NULL || attributes->bInheritHandle) |
        (get_u32(s_worker_module->data, role == 0 ? 0xe04 : 0xe70) << 16);
    entry[4] = (uint32_t)stack_size;
    entry[5] = role;
    entry[6] = parameter != NULL || flags != 0;
    entry[7] = worker_address(thread_id);
    if (role > 1 || index >= 32) {
        fprintf(stderr, "worker fixture invalid thread callback\n");
        exit(2);
    }
    if (s_worker_trace.thread_calls == s_worker_trace.fail_thread)
        return NULL;
    *thread_id = 0x5000 + index;
    s_worker_trace.alive[index] = 1;
    s_worker_trace.thread_started[role] = 1;
    return (HANDLE)(uintptr_t)(0x10000 + index);
}

static DWORD WINAPI worker_wait_many(DWORD count, const HANDLE *handles,
                                      BOOL all, DWORD timeout)
{
    uint32_t *entry = worker_log(18);
    uint32_t step = s_worker_trace.wait_step++;
    uint32_t index;
    DWORD result;

    entry[1] = count;
    entry[2] = (uint32_t)(uintptr_t)handles[0];
    entry[3] = (uint32_t)(uintptr_t)handles[1];
    entry[4] = all;
    entry[5] = timeout;
    entry[6] = worker_address(handles);
    if (step > 8) {
        fprintf(stderr, "worker fixture runaway wait loop\n");
        exit(2);
    }
    index = worker_handle(handles[0]);
    if (index != 0 && s_worker_trace.signalled[index]) {
        s_worker_trace.signalled[index] = 0;
        result = WAIT_OBJECT_0;
    } else if (s_worker_trace.number % 4 == 0 || step >= 3)
        result = WAIT_OBJECT_0;
    else if (step == 0)
        result = s_worker_trace.number % 3 == 0 ? WAIT_TIMEOUT : WAIT_FAILED;
    else
        result = WAIT_OBJECT_0 + 1;
    entry[7] = result;
    return result;
}

static DWORD worker_execute(uint32_t role)
{
    multip_worker_thread_fn thread = (multip_worker_thread_fn)
        send_internal(s_worker_module, role == 0 ? 9 : 14);
    uint32_t *entry = worker_log(19);

    entry[1] = role;
    s_worker_trace.role = role;
    s_worker_trace.wait_step = 0;
    entry[2] = thread(NULL);
    s_worker_trace.thread_started[role] = 0;
    return entry[2];
}

static DWORD WINAPI worker_wait_one(HANDLE handle, DWORD timeout)
{
    uint32_t *entry = worker_log(20);
    uint32_t role = handle == (HANDLE)(uintptr_t)get_u32(s_worker_module->data, 0xdfc) ? 0 : 1;

    entry[1] = (uint32_t)(uintptr_t)handle;
    entry[2] = timeout;
    if (s_worker_trace.thread_started[role] && s_worker_trace.number % 2 == 0)
        worker_execute(role);
    entry[3] = s_worker_trace.number % 3 == 0 ? WAIT_TIMEOUT :
        s_worker_trace.number % 3 == 1 ? WAIT_FAILED : WAIT_OBJECT_0;
    return entry[3];
}

static int32_t STDCALL worker_alloc(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    uint32_t *entry = worker_log(21);
    int32_t result;

    entry[1] = flags;
    entry[2] = size;
    entry[3] = zero;
    entry[4] = worker_address(out);
    result = send_alloc(flags, size, out, zero);
    entry[5] = result;
    entry[6] = send_normalise(*out);
    return result;
}

static int32_t STDCALL worker_free(void **pointer)
{
    uint32_t *entry = worker_log(22);

    entry[1] = worker_address(pointer);
    entry[2] = send_normalise(*pointer);
    if (*pointer == NULL)
        return 0;
    entry[3] = send_free(pointer);
    return (int32_t)entry[3];
}

static int32_t STDCALL worker_copy(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = worker_log(23);

    entry[1] = send_normalise(dest);
    entry[2] = size;
    entry[3] = send_copy(dest, source, size);
    return (int32_t)entry[3];
}

static int32_t STDCALL worker_send(void *object, uint32_t from_id, uint32_t to_id,
                                   uint32_t flags, void *payload, uint32_t size)
{
    uint32_t *entry = worker_log(24);

    entry[1] = from_id;
    entry[2] = to_id;
    entry[3] = flags;
    entry[4] = size;
    entry[5] = fake_dp_send(object, from_id, to_id, flags, payload, size);
    return (int32_t)entry[5];
}

static int32_t STDCALL worker_receive(void *object, uint32_t *from_id,
    uint32_t *to_id, uint32_t flags, void *payload, uint32_t *size)
{
    uint32_t *entry = worker_log(25);
    int32_t result;

    entry[1] = flags;
    entry[2] = *size;
    if (payload == NULL) {
        s_receive_steps = 0;
        s_receive_resizes = s_worker_trace.receive_remaining != 0 ? 1 : 0;
        s_receive_result = s_worker_trace.receive_remaining != 0 ? 0 : 0x887700beu;
    }
    result = fake_dp_receive(object, from_id, to_id, flags, payload, size);
    if (result == 0)
        s_worker_trace.receive_remaining--;
    entry[3] = result;
    entry[4] = *size;
    return result;
}

static uint32_t STDCALL worker_ticks(void)
{
    worker_log(26)[1] = s_receive_timestamp;
    return receive_ticks();
}

static void worker_patch(HMODULE module, const char *name, void *replacement)
{
    uint8_t *base = (uint8_t *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *descriptor = (IMAGE_IMPORT_DESCRIPTOR *)(base +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (; descriptor->Name != 0; descriptor++) {
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + descriptor->OriginalFirstThunk);
        IMAGE_THUNK_DATA *slots = (IMAGE_THUNK_DATA *)(base + descriptor->FirstThunk);
        if (_stricmp((const char *)base + descriptor->Name, "KERNEL32.dll") != 0)
            continue;
        for (; names->u1.AddressOfData != 0; names++, slots++) {
            IMAGE_IMPORT_BY_NAME *import;
            DWORD protection;
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;
            import = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (strcmp((const char *)import->Name, name) != 0)
                continue;
            if (!VirtualProtect(&slots->u1.Function, 4, PAGE_READWRITE, &protection))
                break;
            slots->u1.Function = (DWORD)(uintptr_t)replacement;
            VirtualProtect(&slots->u1.Function, 4, protection, &protection);
            return;
        }
    }
    /* A mutation can remove the only use of an import. With no call site
     * left, there is nothing to patch; the missing call must fail comparison. */
    if (GetProcAddress(module, "multip_test_entries") != NULL)
        return;
    fprintf(stderr, "worker fixture missing retail import %s\n", name);
    exit(2);
}

static void worker_install(const struct multip_module *module)
{
    worker_patch(module->module_handle, "InitializeCriticalSection", worker_cs_init);
    worker_patch(module->module_handle, "DeleteCriticalSection", worker_cs_delete);
    worker_patch(module->module_handle, "EnterCriticalSection", worker_cs_enter);
    worker_patch(module->module_handle, "LeaveCriticalSection", worker_cs_leave);
    worker_patch(module->module_handle, "CreateEventA", worker_event);
    worker_patch(module->module_handle, "CreateThread", worker_thread_create);
    worker_patch(module->module_handle, "SetEvent", worker_set);
    worker_patch(module->module_handle, "CloseHandle", worker_close);
    worker_patch(module->module_handle, "WaitForSingleObject", worker_wait_one);
    worker_patch(module->module_handle, "WaitForMultipleObjects", worker_wait_many);
}

static void worker_state(struct worker_results *results, uint32_t step, int32_t result)
{
    uint32_t index;
    uint32_t value;

    results->returns[step] = result;
    results->state[step][13] = get_u32(s_worker_module->data, 0xf24);
    for (index = 0; index < 13; index++) {
        value = get_u32(s_worker_module->data,
                       index < 6 ? 0xdf0 + index * 4 : 0xe58 + (index - 6) * 4);
        if (index == 2 || index == 8 || index == 9)
            value = send_normalise((void *)(uintptr_t)value);
        results->state[step][index] = value;
    }
}

static int32_t worker_call(uint32_t index)
{
    return ((multip_worker_fn)send_internal(s_worker_module, index))();
}

static void run_workers(const struct multip_module *module, uint32_t number,
                         struct worker_results *results)
{
    uint8_t local_player[0x100] = {0};
    uint8_t payload[12];
    uint32_t index;
    uint32_t step = 0;
    uint32_t block;
    uint32_t *words;
    multip_send_fn send = (multip_send_fn)get_slot(module, 0x20);

    s_worker_module = module;
    memset(results, 0, sizeof(*results));
    memset(&s_worker_trace, 0, sizeof(s_worker_trace));
    memset(&s_send_trace, 0, sizeof(s_send_trace));
    memset(module->data + 0xdf0, 0, 0x18);
    memset(module->data + 0xe58, 0, 0x1c);
    put_u32(module->data, 0xf24, 0);
    s_worker_trace.number = number;
    s_worker_trace.fail_event = number % 5 == 0 ? 1 + (number / 5) % 8 : 0;
    s_worker_trace.fail_thread = number % 7 == 0 ? 1 + (number / 7) % 4 : 0;
    s_worker_trace.fail_set = number % 11 == 0 ? 1 + (number / 11) % 8 : 0;
    s_worker_trace.fail_close = number % 13 == 0 ? 1 + (number / 13) % 6 : 0;
    s_worker_trace.receive_remaining = number % 4;
    s_send_trace.fail_call = number % 3 == 0 ? 1 + (number / 3) % 6 : 0;
    s_send_free_calls = 0;
    s_send_fail_free = number % 2 == 0 ? 1 + (number / 2) % 12 : 0;
    s_send_retry_count = number % 2;
    s_send_terminal_result = number % 2 ? 0 : 0x80004005u;
    s_receive_timestamp = 0xfedc0000u ^ number;
    s_receive_seed = 0x12340000u + number;
    put_u32(local_player, 4, number ^ 0x98543210u);
    memset(payload, (uint8_t)number, sizeof(payload));
    put_u32(module->data, 4, (uint32_t)(uintptr_t)s_send_host);
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_send_object);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)local_player);
    put_u32(module->data, 0xf00, number % 3);
    worker_state(results, step++, worker_call(number % 2 == 0 ? 18 : 5));
    worker_state(results, step++, number % 2 == 0 ? 0 : worker_call(10));
    worker_state(results, step++, number % 19 == 0 ? worker_call(5) : 0);
    worker_state(results, step++, number % 19 == 0 ? worker_call(10) : 0);
    s_send_trace.fail_call = 0;
    worker_state(results, step++, worker_call(number % 2 == 0 ? 20 : 7));
    worker_state(results, step++, worker_call(number % 2 == 0 ? 20 : 7));
    worker_state(results, step++, number % 2 == 0 ? 0 : worker_call(12));
    worker_state(results, step++, number % 2 == 0 ? 0 : worker_call(12));
    for (index = 0; index < 4; index++)
        worker_state(results, step++, send(0x1234 + index, payload,
                     index == 0 ? 0 : sizeof(payload), index & 1));
    if (number % 17 == 0)
        s_send_trace.fail_call = s_send_trace.calls + 1 + (number / 17) % 8;
    worker_state(results, step++, s_worker_trace.thread_started[0] && number % 3 != 0 ? worker_execute(0) : 0);
    worker_state(results, step++, s_worker_trace.thread_started[1] && number % 3 != 0 ? worker_execute(1) : 0);
    worker_state(results, step++, number % 5 == 0 ? worker_call(7) : 0);
    worker_state(results, step++, number % 5 == 0 ? worker_call(12) : 0);
    worker_state(results, step++, worker_call(number % 2 == 0 ? 21 : 8));
    worker_state(results, step++, number % 2 == 0 ? 0 : worker_call(13));
    worker_state(results, step++, worker_call(number % 2 == 0 ? 21 : 8));
    worker_state(results, step++, number % 2 == 0 ? 0 : worker_call(13));
    worker_state(results, step++, worker_call(number % 2 == 0 ? 19 : 11));
    worker_state(results, step++, number % 2 == 0 ? 0 : worker_call(6));
    worker_state(results, step++, worker_call(number % 2 == 0 ? 19 : 11));
    worker_state(results, step++, number % 2 == 0 ? 0 : worker_call(6));
    snapshot_send_heap(&results->heap);
    for (block = 0; block < s_send_trace.blocks; block++) {
        if (s_send_trace.sizes[block] != sizeof(struct send_queue) ||
            !s_send_trace.zeroes[block])
            continue;
        words = results->heap.heap[block].words;
        words[2] = send_normalise((void *)(uintptr_t)words[2]);
    }
    memcpy(&results->trace, &s_worker_trace, sizeof(s_worker_trace));
}

static uint32_t test_workers(const struct multip_module *retail,
                             const struct multip_module *ours, uint32_t scenarios)
{
    struct worker_results retail_results;
    struct worker_results our_results;
    uint32_t number;
    uint32_t bad = 0;
    uint32_t index;
    uint32_t shown;
    void *alloc_function = worker_alloc;
    void *free_function = worker_free;
    void *copy_function = worker_copy;
    void *ticks_function = worker_ticks;

    worker_install(retail);
    worker_install(ours);
    memcpy(s_send_host + 0x20c, &alloc_function, sizeof(alloc_function));
    memcpy(s_send_host + 0x214, &free_function, sizeof(free_function));
    memcpy(s_send_host + 0x21c, &copy_function, sizeof(copy_function));
    memcpy(s_send_host + 0x38, &ticks_function, sizeof(ticks_function));
    s_send_vtable[0x64 / 4] = (void *)worker_receive;
    s_send_vtable[0x68 / 4] = (void *)worker_send;
    for (number = 0; number < scenarios; number++) {
        run_workers(retail, number, &retail_results);
        run_workers(ours, number, &our_results);
        if (memcmp(&retail_results, &our_results, sizeof(our_results)) != 0) {
            if (bad < 8) {
                const uint32_t *retail_words = (const uint32_t *)&retail_results;
                const uint32_t *our_words = (const uint32_t *)&our_results;
                printf("worker scenario %lu differs\n", (unsigned long)number);
                shown = 0;
                for (index = 0; index < sizeof(our_results) / 4 && shown < 20; index++)
                    if (retail_words[index] != our_words[index]) {
                        printf("  word %lu: retail %#lx, ours %#lx\n",
                               (unsigned long)index, (unsigned long)retail_words[index],
                               (unsigned long)our_words[index]);
                        shown++;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP worker lifecycles: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
