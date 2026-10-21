/* Ordered profile host calls plus bytewise changes against each loaded image.
 * Keep inputs independent of the generated implementation field tables. */
#define PROFILE_LOG_SIZE 128
#define PROFILE_STEPS 4

struct profile_call {
    uint32_t operation;
    uint32_t destination;
    uint32_t size;
    uint32_t before;
    uint32_t value;
    char section[16];
    char key[48];
    char text[300];
};

struct profile_results {
    uint32_t count;
    struct profile_call calls[PROFILE_LOG_SIZE];
    uint32_t returns[PROFILE_STEPS];
    uint8_t changes[PROFILE_STEPS][0x1000];
};

static struct profile_results *s_profile_results;
static const struct multip_module *s_profile_module;
static uint32_t s_profile_number;
static uint32_t s_profile_step;

static const uint32_t s_profile_values[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 29, 30, 31, 44, 45,
    46, 99, 100, 101, 254, 255, 256, 4999, 5000, 5001,
    0x7fffffffu, 0x80000000u, 0xffffffffu, 11, 18, 255
};

static uint32_t profile_hash(const char *key)
{
    uint32_t value = 2166136261u;

    while (*key != 0) {
        value ^= (uint8_t)*key++;
        value *= 16777619u;
    }
    return value;
}

static struct profile_call *profile_log(uint32_t operation, const char *section,
                                        const char *key)
{
    struct profile_call *call;

    if (s_profile_results->count >= PROFILE_LOG_SIZE) {
        fprintf(stderr, "profile fixture log exhausted\n");
        exit(2);
    }
    call = &s_profile_results->calls[s_profile_results->count++];
    call->operation = operation;
    snprintf(call->section, sizeof(call->section), "%s", section);
    snprintf(call->key, sizeof(call->key), "%s", key);
    return call;
}

static uint32_t profile_destination(const void *pointer)
{
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t base = (uintptr_t)s_profile_module->data;

    return address >= base && address < base + 0x1000 ? (uint32_t)(address - base) : 0xffffffffu;
}

static int32_t STDCALL profile_read_int(const char *section, const char *key, uint32_t *value)
{
    struct profile_call *call = profile_log(1, section, key);
    uint32_t hash = profile_hash(key);

    call->destination = profile_destination(value);
    call->before = *value;
    if ((s_profile_number + hash + s_profile_step) % 5 != 0)
        *value = s_profile_values[(s_profile_number + hash + s_profile_step) % 32];
    call->value = *value;
    return (int32_t)((s_profile_number + hash) % 3) - 1;
}

static uint32_t STDCALL profile_read_string(const char *section, const char *key,
                                            char *value, uint32_t size)
{
    struct profile_call *call = profile_log(2, section, key);
    uint32_t hash = profile_hash(key);
    uint32_t length = (s_profile_number + hash + s_profile_step * 17) % 300;
    uint32_t index;

    call->destination = profile_destination(value);
    call->size = size;
    if (size == 0 || size > sizeof(call->text)) {
        fprintf(stderr, "profile fixture invalid string capacity\n");
        exit(2);
    }
    if ((s_profile_number + hash + s_profile_step) % 5 != 0) {
        if (length >= size) length = size - 1;
        for (index = 0; index < length; index++)
            value[index] = (char)('A' + (index + s_profile_number) % 26);
        value[length] = 0;
    }
    snprintf(call->text, sizeof(call->text), "%s", value);
    return (uint32_t)strlen(value);
}

static int32_t STDCALL profile_write_int(const char *section, const char *key, uint32_t value)
{
    struct profile_call *call = profile_log(3, section, key);

    call->value = value;
    return (int32_t)((s_profile_number + profile_hash(key)) % 3) - 1;
}

static int32_t STDCALL profile_write_string(const char *section, const char *key, const char *value)
{
    struct profile_call *call = profile_log(4, section, key);

    call->destination = profile_destination(value);
    snprintf(call->text, sizeof(call->text), "%s", value);
    return (int32_t)((s_profile_number + profile_hash(key)) % 3) - 1;
}

static void run_profile(const struct multip_module *module, uint32_t number,
                         struct profile_results *results)
{
    static const uint32_t s_seed_offsets[] = {
        0x998, 0x99c, 0x1a8, 0x160, 0x168, 0xa10, 0x16c, 0x170, 0x174, 0x178, 0x194
    };
    uint8_t host[0xd0] = {0};
    uint32_t step;
    uint32_t index;
    void *function;
    multip_worker_fn load = (multip_worker_fn)send_internal(module, 25);
    multip_worker_fn save = (multip_worker_fn)send_internal(module, 26);

    s_profile_results = results;
    s_profile_module = module;
    s_profile_number = number;
    memset(results, 0, sizeof(*results));
    memcpy(module->data, module->initial_data, sizeof(module->initial_data));
    function = profile_read_int; memcpy(host + 0xa8, &function, 4);
    function = profile_read_string; memcpy(host + 0xc8, &function, 4);
    function = profile_write_int; memcpy(host + 0xac, &function, 4);
    function = profile_write_string; memcpy(host + 0xcc, &function, 4);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    for (index = 0; index < sizeof(s_seed_offsets) / 4; index++)
        put_u32(module->data, s_seed_offsets[index], s_profile_values[(number + index) % 32]);
    for (step = 0; step < PROFILE_STEPS; step++) {
        s_profile_step = step;
        results->returns[step] = step & 1 ? save() : load();
        for (index = 0xa0; index < 0x1000; index++)
            results->changes[step][index] = module->data[index] ^ module->initial_data[index];
    }
}

static uint32_t test_profile(const struct multip_module *retail,
                             const struct multip_module *ours, uint32_t scenarios)
{
    static struct profile_results s_retail_results;
    static struct profile_results s_our_results;
    uint32_t number;
    uint32_t bad = 0;
    uint32_t index;
    uint32_t shown;

    for (number = 0; number < scenarios; number++) {
        run_profile(retail, number, &s_retail_results);
        run_profile(ours, number, &s_our_results);
        if (memcmp(&s_retail_results, &s_our_results, sizeof(s_our_results)) != 0) {
            if (bad < 4) {
                const uint32_t *retail_words = (const uint32_t *)&s_retail_results;
                const uint32_t *our_words = (const uint32_t *)&s_our_results;
                printf("profile scenario %lu differs\n", (unsigned long)number);
                shown = 0;
                for (index = 0; index < sizeof(s_our_results) / 4 && shown < 20; index++)
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
    printf("MULTIP profile load/save: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
