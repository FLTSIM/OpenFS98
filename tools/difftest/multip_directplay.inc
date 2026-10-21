/* DirectPlay export cells and COM identities are fixture data, never live
 * networking. Model each interface separately to expose query/release order. */
#define DIRECTPLAY_STEPS 8
#define DIRECTPLAY_LOG_SIZE 64

struct directplay_object {
    void **vtable;
    uint32_t identity;
};

struct directplay_trace {
    uint32_t count;
    uint32_t log[DIRECTPLAY_LOG_SIZE][16];
    int32_t references[4];
    uint32_t factory_result;
    uint32_t first_query_result;
    uint32_t second_query_result;
    uint32_t load_result;
    uint32_t unload_result;
    uint32_t export_mask;
    uint32_t write_on_failure;
    uint32_t release_result;
};

struct directplay_results {
    struct directplay_trace trace;
    uint32_t returns[DIRECTPLAY_STEPS];
    uint32_t state[DIRECTPLAY_STEPS][7];
    int32_t references[DIRECTPLAY_STEPS][4];
};

static struct directplay_trace s_directplay_trace;
static const struct multip_module *s_directplay_module;
static void *s_directplay_vtable[32];
static struct directplay_object s_directplay_objects[4];

static const GUID s_test_iid_directplay2a = {
    0x9d460580, 0xa822, 0x11cf, {0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82}
};
static const GUID s_test_iid_directplay3a = {
    0x133efe41, 0x32dc, 0x11d0, {0x9c, 0xfb, 0x00, 0xa0, 0xc9, 0x0a, 0x43, 0xcb}
};

typedef int32_t (STDCALL *directplay_create_fn)(const GUID *);

static uint32_t directplay_identity(const void *pointer)
{
    uint32_t index;

    if (pointer == NULL)
        return 0;
    for (index = 0; index < 4; index++)
        if (pointer == &s_directplay_objects[index])
            return index + 1;
    return 0xffffffffu;
}

static uint32_t directplay_data_address(const void *pointer)
{
    uintptr_t base = (uintptr_t)s_directplay_module->data;
    uintptr_t address = (uintptr_t)pointer;

    if (pointer == NULL)
        return 0;
    return address >= base && address < base + 0x1000 ?
        0x10000u + (uint32_t)(address - base) : 0xffffffffu;
}

static uint32_t *directplay_log(uint32_t operation)
{
    uint32_t *entry;

    if (s_directplay_trace.count >= DIRECTPLAY_LOG_SIZE) {
        fprintf(stderr, "DirectPlay fixture call budget exhausted\n");
        exit(2);
    }
    entry = s_directplay_trace.log[s_directplay_trace.count++];
    entry[0] = operation;
    entry[13] = directplay_identity((void *)(uintptr_t)
        get_u32(s_directplay_module->data, 0x988));
    entry[14] = get_u32(s_directplay_module->data, 0xa44);
    entry[15] = get_u32(s_directplay_module->data, 0xa48);
    return entry;
}

static int32_t STDCALL directplay_factory(const GUID *provider, void **out, void *outer)
{
    uint32_t *entry = directplay_log(1);

    entry[1] = provider != NULL;
    if (provider != NULL)
        memcpy(entry + 2, provider, 16);
    entry[6] = directplay_data_address(out);
    entry[7] = outer != NULL;
    entry[8] = s_directplay_trace.factory_result;
    if (s_directplay_trace.factory_result == 0 ||
        s_directplay_trace.write_on_failure == 2) {
        *out = &s_directplay_objects[0];
        s_directplay_trace.references[0]++;
    } else
        *out = NULL;
    return (int32_t)s_directplay_trace.factory_result;
}

static int32_t STDCALL directplay_query(void *object, const GUID *iid, void **out)
{
    uint32_t *entry = directplay_log(2);
    uint32_t identity;
    uint32_t result;

    entry[1] = directplay_identity(object);
    memcpy(entry + 2, iid, 16);
    entry[6] = directplay_data_address(out);
    if (memcmp(iid, &s_test_iid_directplay2a, 16) == 0) {
        identity = 2;
        result = s_directplay_trace.first_query_result;
    } else if (memcmp(iid, &s_test_iid_directplay3a, 16) == 0) {
        identity = 3;
        result = s_directplay_trace.second_query_result;
    } else {
        identity = 0;
        result = 0x80004002u;
    }
    entry[7] = result;
    if (result == 0 || s_directplay_trace.write_on_failure == 2) {
        *out = identity != 0 ? &s_directplay_objects[identity - 1] : NULL;
        if (identity != 0)
            s_directplay_trace.references[identity - 1]++;
    } else if (s_directplay_trace.write_on_failure == 0)
        *out = NULL;
    return (int32_t)result;
}

static uint32_t STDCALL directplay_release(void *object)
{
    uint32_t *entry = directplay_log(3);
    uint32_t identity = directplay_identity(object);

    entry[1] = identity;
    if (identity == 0 || identity > 4 ||
        s_directplay_trace.references[identity - 1] == 0) {
        fprintf(stderr, "DirectPlay fixture invalid release\n");
        exit(2);
    }
    entry[2] = --s_directplay_trace.references[identity - 1];
    return s_directplay_trace.release_result;
}

/* These exports are populated but not invoked by this helper slice. */
static int32_t STDCALL directplay_enumerate(void *callback, void *context)
{
    (void)callback;
    (void)context;
    directplay_log(6);
    return 0;
}

static int32_t STDCALL directplay_lobby(void *guid, void **out, void *outer,
                                        void *reserved, uint32_t flags)
{
    (void)guid;
    (void)out;
    (void)outer;
    (void)reserved;
    (void)flags;
    directplay_log(7);
    return 0;
}

static uint32_t directplay_export_identity(uint32_t address)
{
    if (address == 0)
        return 0;
    if (address == (uint32_t)(uintptr_t)directplay_factory)
        return 1;
    if (address == (uint32_t)(uintptr_t)directplay_enumerate)
        return 2;
    if (address == (uint32_t)(uintptr_t)directplay_lobby)
        return 3;
    return 0xffffffffu;
}

static int32_t directplay_exports(void *descriptor, uint32_t unloading)
{
    void *exports[3] = {(void *)directplay_factory, (void *)directplay_enumerate,
                         (void *)directplay_lobby};
    uint32_t *entry = directplay_log(unloading ? 5 : 4);
    uint32_t index;
    uint32_t cell;
    uint32_t offset;
    uint32_t result = unloading ? s_directplay_trace.unload_result :
                                   s_directplay_trace.load_result;

    entry[1] = directplay_data_address(descriptor);
    if (entry[1] < 0x10000 || entry[1] > 0x10ff4)
        return -1;
    for (index = 0; index < 3; index++) {
        memcpy(&cell, (uint8_t *)descriptor + index * 4, 4);
        offset = directplay_data_address((void *)(uintptr_t)cell);
        entry[2 + index] = offset;
        if (offset < 0x10000 || offset > 0x10ffc)
            continue;
        entry[5 + index] = directplay_export_identity(*(uint32_t *)(uintptr_t)cell);
        if (unloading || result == 0 || s_directplay_trace.write_on_failure == 2)
            *(uint32_t *)(uintptr_t)cell = !unloading &&
                (s_directplay_trace.export_mask & (1u << index)) != 0 ?
                (uint32_t)(uintptr_t)exports[index] : 0;
    }
    entry[8] = result;
    return (int32_t)result;
}

static int32_t STDCALL directplay_load_exports(void *descriptor)
{
    return directplay_exports(descriptor, 0);
}

static int32_t STDCALL directplay_unload_exports(void *descriptor)
{
    return directplay_exports(descriptor, 1);
}

static void directplay_snapshot(struct directplay_results *results,
                                uint32_t step, int32_t result)
{
    uint8_t *data = s_directplay_module->data;
    static const uint32_t s_offsets[] = {0x98c, 0x990, 0x980};
    uint32_t index;

    results->returns[step] = result;
    results->state[step][6] = ((multip_status_fn)get_slot(s_directplay_module, 0x30))();
    results->state[step][0] = directplay_identity((void *)(uintptr_t)get_u32(data, 0x988));
    results->state[step][1] = get_u32(data, 0xa44);
    results->state[step][2] = get_u32(data, 0xa48);
    for (index = 0; index < 3; index++)
        results->state[step][3 + index] = directplay_export_identity(get_u32(data, s_offsets[index]));
    memcpy(results->references[step], s_directplay_trace.references,
           sizeof(results->references[step]));
}

static void directplay_script(uint32_t number, uint32_t step)
{
    static const uint32_t s_results[] = {0, 1, 0x80004002u, 0x80004005u};
    uint32_t state = number ^ (step * 0x9e3779b9u);

    s_directplay_trace.factory_result = number % 8 == 0 ?
        s_results[random_u32(&state) % 4] : 0;
    s_directplay_trace.first_query_result = number % 3 == 0 ?
        s_results[random_u32(&state) % 4] : 0;
    s_directplay_trace.second_query_result = s_results[random_u32(&state) % 4];
    s_directplay_trace.load_result = s_results[random_u32(&state) % 4];
    s_directplay_trace.unload_result = s_results[random_u32(&state) % 4];
    s_directplay_trace.export_mask = (number + step) % 8;
    s_directplay_trace.write_on_failure = (number + step) % 3;
    s_directplay_trace.release_result = random_u32(&state);
}

static void run_directplay(const struct multip_module *module, uint32_t number,
                           struct directplay_results *results)
{
    GUID provider;
    uint8_t host[0x12c] = {0};
    uint32_t index;
    uint32_t step;
    uint32_t state = number ^ 0xabcd7654u;
    multip_worker_fn load = (multip_worker_fn)send_internal(module, 22);
    multip_worker_fn unload = (multip_worker_fn)send_internal(module, 23);
    directplay_create_fn create = (directplay_create_fn)send_internal(module, 24);
    void *load_function = directplay_load_exports;
    void *unload_function = directplay_unload_exports;

    s_directplay_module = module;
    memset(results, 0, sizeof(*results));
    memset(&s_directplay_trace, 0, sizeof(s_directplay_trace));
    for (index = 0; index < sizeof(provider); index++)
        ((uint8_t *)&provider)[index] = (uint8_t)random_u32(&state);
    memcpy(host + 0x124, &load_function, 4);
    memcpy(host + 0x128, &unload_function, 4);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, M_CONNECTED, 0);
    put_u32(module->data, 0xa44, random_u32(&state));
    put_u32(module->data, 0xa48, number % 3 == 0 ? 0 : random_u32(&state));
    put_u32(module->data, 0x988, number % 2 == 0 ?
            (uint32_t)(uintptr_t)&s_directplay_objects[3] : 0);
    s_directplay_trace.references[3] = number % 2 == 0;
    put_u32(module->data, 0x98c, (uint32_t)(uintptr_t)directplay_factory);
    put_u32(module->data, 0x990, number & 1 ? (uint32_t)(uintptr_t)directplay_enumerate : 0);
    put_u32(module->data, 0x980, number & 2 ? (uint32_t)(uintptr_t)directplay_lobby : 0);
    for (step = 0; step < DIRECTPLAY_STEPS; step++) {
        directplay_script(number, step);
        if (step < 2)
            directplay_snapshot(results, step, load());
        else if (step == 2 || step == 3 || step == 6) {
            /* Independently seed the callable factory: incomplete loader
             * scripts are compared separately, never invoked as null code. */
            put_u32(module->data, 0x98c, (uint32_t)(uintptr_t)directplay_factory);
            directplay_snapshot(results, step, create(number & 1 ? &provider : NULL));
        } else
            directplay_snapshot(results, step, unload());
    }
    memcpy(&results->trace, &s_directplay_trace, sizeof(s_directplay_trace));
}

static uint32_t test_directplay(const struct multip_module *retail,
                                const struct multip_module *ours, uint32_t scenarios)
{
    struct directplay_results retail_results;
    struct directplay_results our_results;
    uint32_t number;
    uint32_t bad = 0;
    uint32_t index;
    uint32_t shown;

    s_directplay_vtable[0] = (void *)directplay_query;
    s_directplay_vtable[2] = (void *)directplay_release;
    for (index = 0; index < 4; index++) {
        s_directplay_objects[index].vtable = s_directplay_vtable;
        s_directplay_objects[index].identity = index + 1;
    }
    for (number = 0; number < scenarios; number++) {
        run_directplay(retail, number, &retail_results);
        run_directplay(ours, number, &our_results);
        if (memcmp(&retail_results, &our_results, sizeof(our_results)) != 0) {
            if (bad < 8) {
                const uint32_t *retail_words = (const uint32_t *)&retail_results;
                const uint32_t *our_words = (const uint32_t *)&our_results;
                printf("DirectPlay scenario %lu differs\n", (unsigned long)number);
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
    printf("MULTIP DirectPlay loader/lifetime: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
