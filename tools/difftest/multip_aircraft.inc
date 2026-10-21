/* Aircraft ownership fixture: mutable ACONTAIN records and partial failures. */
struct aircraft_fixture {
    uint32_t returns[3];
    uint32_t log[8][5];
    uint32_t call_count;
    uint32_t number;
    uint32_t free_calls;
    uint8_t aircraft[3][0x30];
    uint8_t header[0x24];
    uint8_t model[0x80];
};
static struct aircraft_fixture s_aircraft;

static uint32_t *aircraft_log(uint32_t tag, const void *aircraft)
{
    uint32_t *entry;
    if (s_aircraft.call_count >= 8) exit(2);
    entry = s_aircraft.log[s_aircraft.call_count++];
    entry[0] = tag;
    entry[1] = get_u32(aircraft, 0xc);
    entry[2] = get_u32(aircraft, 0x10);
    entry[3] = get_u32(aircraft, 0x18);
    return entry;
}

static int32_t STDCALL aircraft_select(void *aircraft, const char *title)
{
    uint32_t *entry = aircraft_log(1, aircraft);
    entry[4] = profile_hash(title);
    put_u32(aircraft, 0, (uint32_t)(uintptr_t)s_aircraft.header);
    return s_aircraft.number % 5 == 0 ? 0 : -2;
}

static int32_t STDCALL aircraft_model_load(void *aircraft, uint32_t kind)
{
    uint32_t *entry = aircraft_log(2, aircraft);
    entry[4] = kind;
    put_u32(aircraft, 4, (uint32_t)(uintptr_t)s_aircraft.model);
    return s_aircraft.number % 7 == 0 ? 0 : 3;
}

static int32_t STDCALL aircraft_model_unload(void *aircraft)
{
    aircraft_log(3, aircraft);
    /* Real ACONTAIN can alter the record before reporting failure. */
    put_u32(aircraft, 8, 0x81726354u);
    return s_aircraft.number % 11 == 0 ? 0 : 1;
}

static int32_t STDCALL aircraft_release(void *aircraft)
{
    aircraft_log(4, aircraft);
    put_u32(aircraft, 0, 0);
    return s_aircraft.number % 13 == 0 ? 0 : -1;
}

static int32_t STDCALL aircraft_free(void **block_out)
{
    uint32_t call = s_aircraft.free_calls++;
    uint32_t *entry;
    if (s_aircraft.call_count >= 8) exit(2);
    entry = s_aircraft.log[s_aircraft.call_count++];
    entry[0] = 5; entry[1] = (uint32_t)(uintptr_t)*block_out;
    if ((s_aircraft.number / 3) % 4 == call)
        return 1;
    *block_out = NULL;
    return 0;
}

typedef int32_t (STDCALL *aircraft_select_fn)(const char *, void *);
typedef int32_t (STDCALL *aircraft_release_fn)(void *);

static void run_aircraft(const struct multip_module *module, uint32_t number,
                         struct aircraft_fixture *results)
{
    uint8_t host[0x320] = {0};
    void *aircraft_linkage[16] = {0};
    uint8_t aircraft[0x30];
    uint32_t random_state = number ^ 0x48239176u;
    uint32_t index;
    void *function = aircraft_free;

    memset(&s_aircraft, 0, sizeof(s_aircraft));
    s_aircraft.number = number;
    memcpy(module->data, module->initial_data, 0x1000);
    memcpy(host + 0x214, &function, 4);
    aircraft_linkage[0x20 / 4] = (void *)aircraft_select;
    aircraft_linkage[0x24 / 4] = (void *)aircraft_release;
    aircraft_linkage[0x34 / 4] = (void *)aircraft_model_load;
    aircraft_linkage[0x38 / 4] = (void *)aircraft_model_unload;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x14, (uint32_t)(uintptr_t)aircraft_linkage);
    for (index = 0; index < sizeof(aircraft); index += 4)
        put_u32(aircraft, index, random_u32(&random_state));
    put_u32(aircraft, 0xc, 0);
    for (index = 0; index < sizeof(s_aircraft.header); index += 4)
        put_u32(s_aircraft.header, index, random_u32(&random_state));
    put_u32(s_aircraft.header, 0x18, number & 1);
    for (index = 0; index < sizeof(s_aircraft.model); index += 4)
        put_u32(s_aircraft.model, index, random_u32(&random_state));
    s_aircraft.returns[0] = ((aircraft_select_fn)send_internal(module, 42))(
        "fixture aircraft", aircraft);
    memcpy(s_aircraft.aircraft[0], aircraft, sizeof(aircraft));
    put_u32(aircraft, 0xc, number & 3);
    s_aircraft.returns[1] = ((aircraft_release_fn)send_internal(module, 43))(aircraft);
    memcpy(s_aircraft.aircraft[1], aircraft, sizeof(aircraft));
    /* A separately received model owns its buffers directly. */
    put_u32(aircraft, 0xc, 0);
    put_u32(aircraft, 0x10, 0x76543210u);
    put_u32(aircraft, 0x18, 0x65432109u);
    s_aircraft.returns[2] = ((aircraft_release_fn)send_internal(module, 43))(aircraft);
    memcpy(s_aircraft.aircraft[2], aircraft, sizeof(aircraft));
    memcpy(results, &s_aircraft, sizeof(*results));
}

static uint32_t test_aircraft(const struct multip_module *retail,
                              const struct multip_module *ours, uint32_t scenarios)
{
    static struct aircraft_fixture s_retail;
    static struct aircraft_fixture s_ours;
    uint32_t number;
    uint32_t bad = 0;
    for (number = 0; number < scenarios; number++) {
        run_aircraft(retail, number, &s_retail);
        run_aircraft(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("aircraft scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP aircraft select/release: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}

struct aircraft_copy_fixture {
    uint32_t number;
    uint32_t call_count;
    uint32_t block_count;
    uint32_t results[9];
    uint32_t log[128][5];
    uint32_t sizes[32];
    uint8_t heap[32][160];
    uint8_t source[0x30];
    uint8_t target[0x30];
    uint8_t snapshots[2][0x30];
    uint8_t flight[64];
    uint8_t model[128];
    uint8_t description[80];
    uint8_t session_record[84];
    uint8_t player_record[16];
    uint8_t player_copy[16];
    uint8_t enumerated[4][16];
    uint8_t list_snapshots[2][84];
    uint8_t initial_session[84];
};
static struct aircraft_copy_fixture s_aircraft_copy;
static const char s_aircraft_title[] = "received aircraft";

static uint32_t *aircraft_copy_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_aircraft_copy.call_count >= 128) exit(2);
    entry = s_aircraft_copy.log[s_aircraft_copy.call_count++];
    entry[0] = tag;
    return entry;
}

static int aircraft_copy_fail(void)
{
    return s_aircraft_copy.number % 97 == s_aircraft_copy.call_count;
}

static int32_t STDCALL aircraft_copy_alloc(int32_t kind, uint32_t size,
                                          void **out, int32_t zero)
{
    uint32_t *entry = aircraft_copy_log(1);
    uint32_t block_index = s_aircraft_copy.block_count;
    entry[1] = kind; entry[2] = size; entry[3] = zero;
    if (aircraft_copy_fail()) return 1;
    if (block_index >= 32 || size > 160) exit(2);
    *out = s_aircraft_copy.heap[block_index];
    memset(*out, zero ? 0 : 0x9a, size);
    s_aircraft_copy.sizes[block_index] = size;
    s_aircraft_copy.block_count++;
    return 0;
}

static int32_t STDCALL aircraft_copy_realloc(int32_t kind, uint32_t size, void **out)
{
    uint32_t *entry = aircraft_copy_log(2);
    uint32_t block_index = s_aircraft_copy.block_count;
    entry[1] = kind; entry[2] = size;
    entry[3] = (uint32_t)(uintptr_t)*out;
    if (aircraft_copy_fail()) return 1;
    if (block_index >= 32 || size > 160) exit(2);
    memset(s_aircraft_copy.heap[block_index], 0xa7, size);
    if (*out != NULL) memcpy(s_aircraft_copy.heap[block_index], *out, size);
    *out = s_aircraft_copy.heap[block_index];
    s_aircraft_copy.sizes[block_index] = size;
    s_aircraft_copy.block_count++;
    return 0;
}

static int32_t STDCALL aircraft_copy_memory(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = aircraft_copy_log(3);
    entry[1] = (uint32_t)(uintptr_t)dest;
    entry[2] = (uint32_t)(uintptr_t)source; entry[3] = size;
    memmove(dest, source, size);
    return aircraft_copy_fail();
}

static uint32_t STDCALL aircraft_copy_strlen(const char *text)
{
    uint32_t *entry = aircraft_copy_log(4);
    entry[1] = profile_hash(text);
    return strlen(text);
}

static int32_t STDCALL aircraft_copy_strcpy(char *dest, const char *source)
{
    uint32_t *entry = aircraft_copy_log(5);
    entry[1] = (uint32_t)(uintptr_t)dest;
    entry[2] = profile_hash(source);
    strcpy(dest, source);
    return aircraft_copy_fail();
}

static int32_t STDCALL aircraft_copy_free(void **block_out)
{
    uint32_t *entry = aircraft_copy_log(6);
    entry[1] = (uint32_t)(uintptr_t)*block_out;
    if (aircraft_copy_fail()) return 1;
    *block_out = NULL;
    return 0;
}

typedef int32_t (STDCALL *record_visitor_fn)(void *, void *);

static int32_t STDCALL record_walk(void *list, void *context, record_visitor_fn visit)
{
    uint32_t *entry = aircraft_copy_log(7);
    uint32_t count = get_u32(list, 0);
    uint32_t index;
    uint8_t *data = (uint8_t *)(uintptr_t)get_u32(list, 0x18);
    int32_t result;

    entry[1] = count; entry[2] = context != NULL;
    if (count > 4 || get_u32(list, 0xc) != 16) exit(2);
    for (index = 0; index < count; index++) {
        result = visit(data + index * 16, context);
        if (result != 0) return result;
    }
    return 0;
}

static void *STDCALL record_add(void *list)
{
    uint32_t *entry = aircraft_copy_log(8);
    uint32_t count = get_u32(list, 0);
    void *record;
    entry[1] = count; entry[2] = get_u32(list, 0xc);
    if (count >= 4 || entry[2] != 16) exit(2);
    record = s_aircraft_copy.enumerated[count];
    memset(record, 0, 16);
    put_u32(list, 0, count + 1);
    put_u32(list, 4, 4);
    put_u32(list, 0x18, (uint32_t)(uintptr_t)s_aircraft_copy.enumerated);
    return record;
}

static int32_t STDCALL record_enum(void *object, const GUID *instance,
    session_enum_callback callback, void *context, uint32_t flags)
{
    uint32_t *entry = aircraft_copy_log(9);
    uint32_t name_fields[4] = {16, 0, 0, 0};
    char long_name[4] = {'C', 'N', 0, 0};
    uint32_t index;
    entry[1] = directplay_identity(object);
    entry[2] = instance != NULL;
    entry[3] = flags;
    entry[4] = 2166136261u;
    for (index = 0; index < 16; index++)
        entry[4] = (entry[4] ^ ((const uint8_t *)instance)[index]) * 16777619u;
    name_fields[2] = (uint32_t)(uintptr_t)s_aircraft_title;
    name_fields[3] = (uint32_t)(uintptr_t)long_name;
    for (index = 0; index < 4; index++) {
        uint32_t *callback_entry;
        int32_t keep_going;
        long_name[0] = (s_aircraft_copy.number & 1) ? 'H' : 'C';
        long_name[1] = "POUN"[index];
        if (index == s_aircraft_copy.number % 5) {
            if (s_aircraft_copy.number & 2) long_name[0] = 'x';
            else long_name[1] = 'x';
        }
        keep_going = callback(0x2000 + index, index & 1, name_fields, 0x40, context);
        callback_entry = aircraft_copy_log(10);
        callback_entry[1] = index; callback_entry[2] = keep_going;
        if (keep_going == 0) break;
    }
    return (int32_t)(s_aircraft_copy.number & 4 ? 0x80004005u : 0);
}

typedef int32_t (STDCALL *aircraft_copy_fn)(void *, const void *);
typedef int32_t (STDCALL *session_record_fn)(const void *, void *);
typedef int32_t (STDCALL *player_record_fn)(uint32_t, uint32_t, const void *, void *);
typedef int32_t (STDCALL *player_record_free_fn)(void *, void *);
typedef int32_t (STDCALL *record_enum_fn)(void *, const GUID *, uint32_t);

static void run_aircraft_copy(const struct multip_module *module, uint32_t number,
                              struct aircraft_copy_fixture *results)
{
    uint8_t host[0x320] = {0};
    uint32_t random_state = number ^ 0x19872345u;
    uint32_t index;
    uint32_t name_fields[4] = {16, 0, 0, 0};
    void *function;

    memset(&s_aircraft_copy, 0, sizeof(s_aircraft_copy));
    s_aircraft_copy.number = number;
    memcpy(module->data, module->initial_data, 0x1000);
    function = aircraft_copy_alloc; memcpy(host + 0x20c, &function, 4);
    function = aircraft_copy_realloc; memcpy(host + 0x210, &function, 4);
    function = aircraft_copy_free; memcpy(host + 0x214, &function, 4);
    function = aircraft_copy_memory; memcpy(host + 0x21c, &function, 4);
    function = aircraft_copy_strlen; memcpy(host + 0x2b8, &function, 4);
    function = aircraft_copy_strcpy; memcpy(host + 0x2a8, &function, 4);
    function = record_walk; memcpy(host + 0x308, &function, 4);
    function = record_add; memcpy(host + 0x2f8, &function, 4);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    for (index = 0; index < sizeof(s_aircraft_copy.flight); index++)
        s_aircraft_copy.flight[index] = (uint8_t)random_u32(&random_state);
    for (index = 0; index < sizeof(s_aircraft_copy.model); index++)
        s_aircraft_copy.model[index] = (uint8_t)random_u32(&random_state);
    put_u32(s_aircraft_copy.source, 0x10, (uint32_t)(uintptr_t)s_aircraft_copy.flight);
    put_u32(s_aircraft_copy.source, 0x14, number % 65);
    put_u32(s_aircraft_copy.source, 0x18, (uint32_t)(uintptr_t)s_aircraft_copy.model);
    put_u32(s_aircraft_copy.source, 0x1c, 128);
    put_u32(s_aircraft_copy.source, 0x20, random_u32(&random_state));
    put_u32(s_aircraft_copy.source, 0x28, random_u32(&random_state));
    put_u32(s_aircraft_copy.model, 0x64, (uint32_t)(uintptr_t)s_aircraft_title);
    for (index = 0; index < 2; index++) {
        s_aircraft_copy.results[index] = ((aircraft_copy_fn)send_internal(module, 44))(
            s_aircraft_copy.target, s_aircraft_copy.source);
        memcpy(s_aircraft_copy.snapshots[index], s_aircraft_copy.target, 0x30);
    }
    for (index = 0; index < 80; index += 4)
        put_u32(s_aircraft_copy.description, index, random_u32(&random_state));
    put_u32(s_aircraft_copy.description, 0x30, (uint32_t)(uintptr_t)s_aircraft_title);
    memset(s_aircraft_copy.session_record, 0x97, 84);
    s_aircraft_copy.results[2] = ((session_record_fn)send_internal(module, 45))(
        s_aircraft_copy.description, s_aircraft_copy.session_record);
    memcpy(s_aircraft_copy.initial_session, s_aircraft_copy.session_record, 84);
    name_fields[2] = (uint32_t)(uintptr_t)s_aircraft_title;
    put_u32(s_aircraft_copy.player_record, 4, random_u32(&random_state));
    put_u32(s_aircraft_copy.player_record, 8, random_u32(&random_state));
    put_u32(s_aircraft_copy.player_record, 12, (uint32_t)(uintptr_t)s_aircraft_title);
    s_aircraft_copy.results[3] = ((player_record_fn)send_internal(module, 46))(
        random_u32(&random_state), random_u32(&random_state), name_fields, s_aircraft_copy.player_record);
    s_aircraft_copy.results[4] = ((aircraft_copy_fn)send_internal(module, 47))(
        s_aircraft_copy.player_copy, s_aircraft_copy.player_record);
    s_aircraft_copy.results[5] = ((player_record_free_fn)send_internal(module, 48))(
        s_aircraft_copy.player_copy, (void *)(uintptr_t)random_u32(&random_state));
    memset(s_aircraft_copy.session_record + 0x38, 0, 28);
    put_u32(s_aircraft_copy.session_record, 0x40, 32);
    put_u32(s_aircraft_copy.session_record, 0x44, 16);
    s_directplay_objects[0].vtable = s_directplay_vtable;
    s_directplay_vtable[0x30 / 4] = (void *)record_enum;
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    /* Compare the GUID bytes by hash in the COM call transcript. */
    for (index = 0; index < 2; index++) {
        s_aircraft_copy.results[6 + index] = ((record_enum_fn)send_internal(module, 50))(
            s_aircraft_copy.session_record, (const GUID *)s_aircraft_copy.description,
            random_u32(&random_state));
        memcpy(s_aircraft_copy.list_snapshots[index], s_aircraft_copy.session_record, 84);
    }
    s_aircraft_copy.results[8] = ((record_enum_fn)send_internal(module, 50))(NULL, NULL, 0);
    memcpy(results, &s_aircraft_copy, sizeof(*results));
}

static uint32_t test_aircraft_copy(const struct multip_module *retail,
                                   const struct multip_module *ours, uint32_t scenarios)
{
    static struct aircraft_copy_fixture s_retail;
    static struct aircraft_copy_fixture s_ours;
    uint32_t number;
    uint32_t bad = 0;
    for (number = 0; number < scenarios; number++) {
        run_aircraft_copy(retail, number, &s_retail);
        run_aircraft_copy(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("aircraft copy scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP aircraft copy/session browser records: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
