/* Provider enumeration is synthetic: no DirectPlay DLL or network is used.
 * Stable banks preserve appended partial records and the cleanup name leaks. */
#define PROVIDER_RECORDS 16
struct provider_results {
    uint32_t calls, blocks, log[160][6], live[PROVIDER_RECORDS];
    uint8_t heap[PROVIDER_RECORDS][80], records[PROVIDER_RECORDS][28];
    uint32_t returns[5], counts[5], hashes[5];
    uint8_t lists[5][28];
};
static struct provider_results s_providers;
static const struct multip_module *s_provider_module;
static uint32_t s_provider_fail_call, s_provider_number;
typedef int32_t (STDCALL *provider_callback_fn)(const GUID *, const char *, uint32_t, uint32_t, void *);

static uint32_t provider_address(const void *pointer)
{
    uintptr_t address = (uintptr_t)pointer, base = (uintptr_t)s_provider_module->data;
    if (address >= base && address < base + 0x1000)
        return 0xe000 + address - base;
    return (uint32_t)address;
}
static uint32_t *provider_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_providers.calls >= 160) {
        fprintf(stderr, "provider fixture log exhausted\n"); exit(2);
    }
    entry = s_providers.log[s_providers.calls++];
    entry[0] = tag;
    entry[5] = get_u32(s_provider_module->data, 0x348);
    return entry;
}
static int provider_fail(void) { return s_providers.calls == s_provider_fail_call; }
static void *STDCALL provider_append(void *list)
{
    uint32_t count = get_u32(list, 0);
    provider_log(1)[1] = provider_address(list);
    if (count >= PROVIDER_RECORDS) {
        fprintf(stderr, "provider fixture list exhausted\n"); exit(2);
    }
    put_u32(list, 0, count + 1);
    memset(s_providers.records[count], 0, 28);
    return s_providers.records[count];
}
static uint32_t STDCALL provider_clear_list(void *list)
{
    provider_log(2)[1] = provider_address(list);
    put_u32(list, 0, 0);
    /* Host +0x304 retains the backing array and returns the list pointer. */
    return (uint32_t)(uintptr_t)list;
}
static uint32_t STDCALL provider_strlen(const char *text)
{
    uint32_t *entry = provider_log(3);
    entry[1] = strlen(text);
    return entry[1];
}
static int32_t STDCALL provider_alloc(int32_t flags, uint32_t size, void **out, int32_t zero)
{
    uint32_t *entry = provider_log(4), index = s_providers.blocks;
    entry[1] = flags; entry[2] = size; entry[3] = provider_address(out); entry[4] = zero;
    if (provider_fail()) { *out = NULL; return -1; }
    if (index >= PROVIDER_RECORDS || size > 80) {
        fprintf(stderr, "provider fixture heap exhausted\n"); exit(2);
    }
    s_providers.blocks++;
    s_providers.live[index] = 1;
    *out = s_providers.heap[index];
    memset(*out, zero ? 0 : 0xa5, size);
    return 0;
}
static int32_t STDCALL provider_free(void **out)
{
    uint32_t *entry = provider_log(5), index;
    entry[1] = provider_address(out); entry[2] = provider_address(*out);
    if (provider_fail()) return -1;
    for (index = 0; index < s_providers.blocks; index++)
        if (*out == s_providers.heap[index] && s_providers.live[index]) {
            s_providers.live[index] = 0; *out = NULL; return 0;
        }
    fprintf(stderr, "provider fixture invalid free\n"); exit(2);
}
static int32_t STDCALL provider_strcpy(char *dest, const char *source)
{
    uint32_t *entry = provider_log(6);
    entry[1] = provider_address(dest); entry[2] = strlen(source);
    if (provider_fail()) return -1;
    strcpy(dest, source);
    return 0;
}
static int32_t STDCALL provider_copy(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = provider_log(7);
    entry[1] = provider_address(dest); entry[2] = size;
    if (provider_fail()) return -1;
    memmove(dest, source, size);
    return 0;
}
static int32_t STDCALL provider_enumerate(provider_callback_fn callback, void *context)
{
    static const uint32_t s_results[] = {0, 1, 0x80004005u, 0x88770082u};
    uint32_t *entry = provider_log(8), index, byte_index;
    uint32_t state = s_provider_number ^ 0x9abc1234;
    uint32_t count = (s_provider_number / 7) % 5;
    entry[1] = callback == (provider_callback_fn)send_internal(s_provider_module, 222);
    entry[2] = context != NULL; entry[3] = s_results[(s_provider_number / 11) % 4];
    for (index = 0; index < count; index++) {
        GUID guid;
        char description[64];
        uint32_t length = random_u32(&state) % 64;
        uint32_t major_version = random_u32(&state), minor_version = random_u32(&state);
        for (byte_index = 0; byte_index < 16; byte_index++)
            ((uint8_t *)&guid)[byte_index] = (uint8_t)random_u32(&state);
        memset(description, 'A' + index, length); description[length] = 0;
        entry[4] = callback(&guid, description, major_version, minor_version, context);
        if (entry[4] == 0) break;
    }
    return (int32_t)entry[3];
}
static void provider_snapshot(uint32_t step, int32_t result)
{
    uint32_t index, byte_index, hash = 2166136261u;
    s_providers.returns[step] = result;
    s_providers.counts[step] = get_u32(s_provider_module->data, 0x348);
    memcpy(s_providers.lists[step], s_provider_module->data + 0x348, 28);
    for (index = 0; index < PROVIDER_RECORDS; index++) {
        for (byte_index = 0; byte_index < 28; byte_index++)
            hash = (hash ^ s_providers.records[index][byte_index]) * 16777619u;
        for (byte_index = 0; byte_index < 80; byte_index++)
            hash = (hash ^ s_providers.heap[index][byte_index]) * 16777619u;
        hash = (hash ^ s_providers.live[index]) * 16777619u;
    }
    s_providers.hashes[step] = hash;
}
static void run_providers(const struct multip_module *module, uint32_t number,
    struct provider_results *results)
{
    uint8_t host[0x308] = {0};
    uint32_t index, initial_count = number % 5;
    GUID guid = {0x12345678, 0xabcd, 0x9876, {1,2,3,4,5,6,7,8}};
    multip_send_drain_fn clear = (multip_send_drain_fn)send_internal(module, 221);
    provider_callback_fn callback = (provider_callback_fn)send_internal(module, 222);
    multip_send_drain_fn enumerate = (multip_send_drain_fn)send_internal(module, 223);
    s_provider_module = module; s_provider_number = number;
    memset(&s_providers, 0, sizeof(s_providers));
    memset(s_providers.records, 0xa5, sizeof(s_providers.records));
    memcpy(module->data, module->initial_data, sizeof(module->initial_data));
#define PROVIDER_HOST(offset, function) put_u32(host, offset, (uint32_t)(uintptr_t)function)
    PROVIDER_HOST(0x20c, provider_alloc); PROVIDER_HOST(0x214, provider_free);
    PROVIDER_HOST(0x21c, provider_copy); PROVIDER_HOST(0x2a8, provider_strcpy);
    PROVIDER_HOST(0x2b8, provider_strlen); PROVIDER_HOST(0x2f8, provider_append);
    PROVIDER_HOST(0x304, provider_clear_list);
#undef PROVIDER_HOST
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x990, (uint32_t)(uintptr_t)provider_enumerate);
    put_u32(module->data, 0x348, initial_count);
    put_u32(module->data, 0x360, (uint32_t)(uintptr_t)s_providers.records);
    s_providers.blocks = initial_count;
    for (index = 0; index < initial_count; index++) {
        s_providers.live[index] = 1;
        memset(s_providers.heap[index], 0x30 + index, 80);
        s_providers.heap[index][79] = 0;
        put_u32(s_providers.records[index], 0, (uint32_t)(uintptr_t)s_providers.heap[index]);
    }
    /* A null first name must hide subsequent live names during cleanup. */
    if (initial_count && number & 1) put_u32(s_providers.records[0], 0, 0);
    s_provider_fail_call = (number / 5) % 32;
    provider_snapshot(0, enumerate());
    provider_snapshot(1, clear());
    provider_snapshot(2, callback(&guid, "manual provider", 0x12345678, 0x87654321, (void *)0x1234));
    provider_snapshot(3, clear());
    provider_snapshot(4, enumerate());
    memcpy(results, &s_providers, sizeof(*results));
}
static uint32_t test_providers(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    struct provider_results retail_results, our_results;
    uint32_t number, index, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("providers", "retail", number);
        run_providers(retail, number, &retail_results);
        /* Check known retail outcomes, independently of the port comparison. */
        if ((number == 0 && (retail_results.returns[2] != 1 ||
                retail_results.counts[2] != 1 ||
                get_u32(retail_results.records[0], 0x14) != 0x12345678 ||
                get_u32(retail_results.records[0], 0x18) != 0x87654321)) ||
            (number == 2 && retail_results.live[1] != 1) ||
            (number == 6 && (retail_results.returns[0] != 0 ||
                retail_results.log[0][0] != 5 || retail_results.log[1][0] != 5)) ||
            (number == 11 && retail_results.returns[0] != 0xffffffffu)) {
            fprintf(stderr, "provider fixture missed a required retail boundary\n");
            exit(2);
        }
        multip_test_context("providers", "ours", number);
        run_providers(ours, number, &our_results);
        if (memcmp(&retail_results, &our_results, sizeof(our_results)) != 0) {
            if (bad < 8) {
                const uint32_t *left = (const uint32_t *)&retail_results;
                const uint32_t *right = (const uint32_t *)&our_results;
                printf("providers scenario %lu differs\n", (unsigned long)number);
                for (index = 0; index < sizeof(our_results) / 4; index++)
                    if (left[index] != right[index]) {
                        printf("  word %lu: retail %#lx, ours %#lx\n", (unsigned long)index,
                            (unsigned long)left[index], (unsigned long)right[index]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP provider catalogue: %lu scenarios, %lu differ\n",
        (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
