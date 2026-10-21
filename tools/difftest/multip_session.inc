/* Session helpers use the same distinct COM identities as lifetime tests. */
#define SESSION_STEPS 9

struct session_controls {
    uint32_t open_result;
    uint32_t enum_result;
    uint32_t group_result;
    uint32_t membership_result;
    uint32_t memset_result;
    uint32_t group_calls;
    uint32_t number;
};

struct session_results {
    struct directplay_trace trace;
    struct session_controls controls;
    uint32_t returns[SESSION_STEPS];
    uint8_t changes[SESSION_STEPS][0x1000];
};

static struct session_controls s_session_controls;

typedef int32_t (STDCALL *session_enum_callback)(uint32_t, uint32_t, const void *, uint32_t, void *);

static uint32_t session_string_hash(const char *text)
{
    return text != NULL ? profile_hash(text) : 0;
}

static int32_t STDCALL session_memset(void *dest, int32_t value, uint32_t size)
{
    uint32_t *entry = directplay_log(10);

    entry[1] = directplay_data_address(dest);
    entry[2] = value;
    entry[3] = size;
    memset(dest, value, size);
    return (int32_t)s_session_controls.memset_result;
}

static int32_t STDCALL session_strcmp(const char *left, const char *right)
{
    uint32_t *entry = directplay_log(11);
    int32_t result = strcmp(left, right);

    entry[1] = session_string_hash(left);
    entry[2] = session_string_hash(right);
    entry[3] = (uint32_t)(result < 0 ? -1 : result > 0);
    return (int32_t)entry[3];
}

static int32_t STDCALL session_open(void *object, void *descriptor, uint32_t flags)
{
    uint32_t *entry = directplay_log(12);
    const uint8_t *data = descriptor;
    uint32_t index;

    entry[1] = directplay_identity(object);
    entry[2] = directplay_data_address(descriptor);
    entry[3] = flags;
    entry[4] = get_u32(data, 0);
    entry[5] = get_u32(data, 4);
    for (index = 0; index < 4; index++)
        entry[6 + index] = get_u32(data, 0x18 + index * 4);
    entry[10] = get_u32(data, 0x28);
    entry[11] = directplay_data_address((void *)(uintptr_t)get_u32(data, 0x30));
    entry[12] = s_session_controls.open_result;
    return (int32_t)s_session_controls.open_result;
}

static int32_t STDCALL session_enum_groups(void *object, const GUID *instance,
    session_enum_callback callback, void *context, uint32_t flags)
{
    uint32_t *entry = directplay_log(13);
    uint32_t name[4] = {16, 0, 0, 0};
    uint32_t id = 0x2000 + s_session_controls.number;

    entry[1] = directplay_identity(object);
    entry[2] = instance != NULL;
    entry[3] = callback != NULL;
    entry[4] = context != NULL;
    entry[5] = flags;
    /* A player with a matching group name must be skipped before dereference. */
    entry[6] = callback(id, 1, NULL, 0, context);
    name[2] = (uint32_t)(uintptr_t)(s_directplay_module->data + 0x1b0);
    entry[7] = callback(id + 1, 0, name, 0, context);
    name[2] = (uint32_t)(uintptr_t)(s_directplay_module->data + 0x1c8);
    entry[8] = callback(id + 2, 2, name, 0x80, context);
    name[2] = (uint32_t)(uintptr_t)"other group";
    entry[9] = callback(id + 3, 0, name, 0, context);
    if (s_session_controls.number & 1) {
        name[2] = (uint32_t)(uintptr_t)(s_directplay_module->data + 0x1b0);
        entry[10] = callback(id + 4, 0, name, 0, context);
    }
    entry[11] = s_session_controls.enum_result;
    return (int32_t)s_session_controls.enum_result;
}

static int32_t STDCALL session_create_group(void *object, uint32_t *out,
    void *name, void *data, uint32_t size, uint32_t flags)
{
    uint32_t *entry = directplay_log(14);
    uint32_t index = s_session_controls.group_calls++;
    const uint8_t *record = name;
    uint32_t result = (index + s_session_controls.number) % 3 == 0 ?
        s_session_controls.group_result : 0;

    entry[1] = directplay_identity(object);
    entry[2] = directplay_data_address(out);
    entry[3] = directplay_data_address(name);
    entry[4] = get_u32(record, 0);
    entry[5] = get_u32(record, 4);
    entry[6] = session_string_hash((const char *)(uintptr_t)get_u32(record, 8));
    entry[7] = get_u32(record, 12) != 0;
    entry[8] = data != NULL;
    entry[9] = size;
    entry[10] = flags;
    *out = 0x3000 + index;
    entry[11] = result;
    return (int32_t)result;
}

static int32_t STDCALL session_group_add(void *object, uint32_t group, uint32_t player)
{
    uint32_t *entry = directplay_log(15);

    entry[1] = directplay_identity(object);
    entry[2] = group;
    entry[3] = player;
    return (int32_t)s_session_controls.membership_result;
}

static int32_t STDCALL session_group_remove(void *object, uint32_t group, uint32_t player)
{
    uint32_t *entry = directplay_log(16);

    entry[1] = directplay_identity(object);
    entry[2] = group;
    entry[3] = player;
    return (int32_t)s_session_controls.membership_result;
}

static void session_snapshot(const struct multip_module *module,
                              struct session_results *results, uint32_t step, int32_t result)
{
    static const uint32_t s_pointer_offsets[] = {0x140, 0x9f0, 0xa38, 0xa3c};
    uint32_t index;
    uint32_t value;

    results->returns[step] = result;
    for (index = 0xa0; index < 0x1000; index++)
        results->changes[step][index] = module->data[index] ^ module->initial_data[index];
    for (index = 0; index < sizeof(s_pointer_offsets) / 4; index++) {
        value = directplay_data_address((void *)(uintptr_t)get_u32(module->data, s_pointer_offsets[index]));
        put_u32(results->changes[step], s_pointer_offsets[index], value);
    }
}

static void run_session(const struct multip_module *module, uint32_t number,
                         struct session_results *results)
{
    static const uint32_t s_errors[] = {0, 1, 0x80004001u, 0x80004005u,
        0x80070057u, 0x88770082u, 0xffffffffu, 0x887700beu};
    uint8_t host[0x2c0] = {0};
    uint8_t local[0x100] = {0};
    uint32_t application[4];
    uint32_t index;
    uint32_t state = number ^ 0x78653412u;
    void *function;
    directplay_create_fn create = (directplay_create_fn)send_internal(module, 24);

    s_directplay_module = module;
    memset(results, 0, sizeof(*results));
    memset(&s_directplay_trace, 0, sizeof(s_directplay_trace));
    memset(&s_session_controls, 0, sizeof(s_session_controls));
    memcpy(module->data, module->initial_data, sizeof(module->initial_data));
    function = session_memset; memcpy(host + 0x218, &function, 4);
    function = session_strcmp; memcpy(host + 0x2bc, &function, 4);
    function = directplay_unload_exports; memcpy(host + 0x128, &function, 4);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x98c, (uint32_t)(uintptr_t)directplay_factory);
    for (index = 0; index < 4; index++) application[index] = random_u32(&state);
    put_u32(module->data, 0xa38, (uint32_t)(uintptr_t)application);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)local);
    put_u32(local, 4, random_u32(&state));
    put_u32(module->data, 0xaa8, random_u32(&state));
    put_u32(module->data, M_CONNECTED, number & 1);
    put_u32(module->data, 0xaa0, 0x98760001u);
    put_u32(module->data, 0xaa4, 0x98760002u);
    put_u32(module->data, 0x998, random_u32(&state));
    put_u32(module->data, 0x99c, random_u32(&state));
    put_u32(module->data, 0x190, random_u32(&state));
    /* A harmless descriptor for the direct join/Open case. Host overwrites it. */
    put_u32(module->data, 0x9c0, 0x50);
    s_session_controls.number = number;
    s_session_controls.open_result = s_errors[number % 8];
    s_session_controls.enum_result = s_errors[(number / 8) % 8];
    s_session_controls.group_result = s_errors[(number / 3) % 8];
    s_session_controls.membership_result = s_errors[number % 8];
    s_session_controls.memset_result = number % 17 == 0 ? -1 : 0;
    session_snapshot(module, results, 0, create(NULL));
    for (index = 0; index < 7; index++) {
        int32_t result = ((multip_worker_fn)send_internal(module, 27 + index))();
        session_snapshot(module, results, index + 1, result);
    }
    session_snapshot(module, results, 8, ((multip_worker_fn)send_internal(module, 23))());
    memcpy(&results->trace, &s_directplay_trace, sizeof(s_directplay_trace));
    memcpy(&results->controls, &s_session_controls, sizeof(s_session_controls));
}

static uint32_t test_session(const struct multip_module *retail,
                             const struct multip_module *ours, uint32_t scenarios)
{
    static struct session_results s_retail_results;
    static struct session_results s_our_results;
    uint32_t number;
    uint32_t bad = 0;
    uint32_t index;
    uint32_t shown;

    s_directplay_vtable[0x60 / 4] = (void *)session_open;
    s_directplay_vtable[0x2c / 4] = (void *)session_enum_groups;
    s_directplay_vtable[0x14 / 4] = (void *)session_create_group;
    s_directplay_vtable[0x0c / 4] = (void *)session_group_add;
    s_directplay_vtable[0x1c / 4] = (void *)session_group_remove;
    for (number = 0; number < scenarios; number++) {
        run_session(retail, number, &s_retail_results);
        run_session(ours, number, &s_our_results);
        if (memcmp(&s_retail_results, &s_our_results, sizeof(s_our_results)) != 0) {
            if (bad < 4) {
                const uint32_t *retail_words = (const uint32_t *)&s_retail_results;
                const uint32_t *our_words = (const uint32_t *)&s_our_results;
                printf("session scenario %lu differs\n", (unsigned long)number);
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
    printf("MULTIP session/group primitives: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}

/* Descriptor ownership and name metadata, isolated from the group fixture. */
struct session_metadata_state {
    uint32_t probe_result;
    uint32_t read_result;
    uint32_t write_result;
    uint32_t alloc_result;
    uint32_t free_result;
    uint32_t memset_result;
    uint32_t call_count;
    uint32_t log[16][6];
    uint32_t block_count;
    uint32_t live[4];
    uint8_t heap[4][128];
    uint32_t results[32];
    uint8_t names[8][8];
};
static struct session_metadata_state s_metadata;

static uint32_t *metadata_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_metadata.call_count >= 16) {
        puts("metadata fixture log exhausted"); exit(2);
    }
    entry = s_metadata.log[s_metadata.call_count++];
    entry[0] = tag;
    return entry;
}

static int32_t STDCALL metadata_alloc(int32_t kind, uint32_t size,
                                      void **out, int32_t zero)
{
    uint32_t *entry = metadata_log(1);
    entry[1] = kind; entry[2] = size; entry[3] = zero;
    if (s_metadata.alloc_result != 0)
        return (int32_t)s_metadata.alloc_result;
    if (size > 128 || s_metadata.block_count >= 4) exit(2);
    *out = s_metadata.heap[s_metadata.block_count];
    s_metadata.live[s_metadata.block_count++] = 1;
    return 0;
}

static int32_t STDCALL metadata_free(void **block_out)
{
    uint32_t index;
    uint32_t *entry = metadata_log(2);
    for (index = 0; index < s_metadata.block_count; index++)
        if (*block_out == s_metadata.heap[index]) break;
    entry[1] = index;
    if (index == s_metadata.block_count) exit(2);
    if (s_metadata.free_result == 0) {
        s_metadata.live[index] = 0;
        *block_out = NULL;
    }
    return (int32_t)s_metadata.free_result;
}

static int32_t STDCALL metadata_get(void *object, void *description, uint32_t *size)
{
    uint32_t *entry = metadata_log(3);
    entry[1] = directplay_identity(object);
    entry[2] = description != NULL;
    entry[3] = *size;
    if (description != NULL) {
        entry[4] = get_u32(description, 0);
        memset((uint8_t *)description + 4, 0x65, 76);
    }
    *size = 80;
    return (int32_t)(description != NULL ? s_metadata.read_result : s_metadata.probe_result);
}

static int32_t STDCALL metadata_memset(void *dest, int32_t value, uint32_t size)
{
    uint32_t *entry = metadata_log(5);
    entry[1] = value; entry[2] = size;
    memset(dest, value, size);
    return (int32_t)s_metadata.memset_result;
}

static int32_t STDCALL metadata_get_name(void *object, uint32_t id,
                                        void *name, uint32_t *size)
{
    uint32_t *entry = metadata_log(6);
    entry[1] = directplay_identity(object);
    entry[2] = id; entry[3] = name != NULL; entry[4] = *size;
    *size = 32;
    if (name != NULL) memset(name, 0, 32);
    return (int32_t)(name != NULL ? s_metadata.read_result : s_metadata.probe_result);
}

static int32_t STDCALL metadata_set_name(void *object, uint32_t id,
                                        void *name, uint32_t flags)
{
    uint32_t *entry = metadata_log(7);
    const uint8_t *name_text = (const uint8_t *)(uintptr_t)get_u32(name, 12);
    entry[1] = directplay_identity(object); entry[2] = id;
    entry[3] = get_u32(name, 0); entry[4] = flags;
    entry[5] = name_text[0] | (name_text[1] << 8) | (name_text[2] << 16);
    return (int32_t)s_metadata.write_result;
}

static int32_t STDCALL metadata_set(void *object, void *description, uint32_t flags)
{
    uint32_t *entry = metadata_log(4);
    entry[1] = directplay_identity(object);
    entry[2] = get_u32(description, 0);
    entry[3] = get_u32(description, 0x40);
    entry[4] = flags;
    return (int32_t)s_metadata.write_result;
}

typedef int32_t (STDCALL *metadata_description_fn)(void **);
typedef int32_t (STDCALL *metadata_capacity_set_fn)(const uint32_t *);
typedef int32_t (STDCALL *metadata_capacity_get_fn)(uint32_t *, const void *);
typedef int32_t (STDCALL *metadata_decode_fn)(const void *, uint32_t *, uint32_t *);
typedef int32_t (STDCALL *metadata_encode_fn)(void *, uint32_t, uint32_t);
typedef int32_t (STDCALL *metadata_collision_fn)(void *);

static void run_metadata(const struct multip_module *module, uint32_t number,
                         struct session_metadata_state *results)
{
    static const uint32_t s_errors[] = {0, 1, 0x80004001u, 0x80070057u,
        0x8877001eu, 0xffffffffu};
    uint8_t host[0x2c0] = {0};
    uint8_t description[80] = {0};
    uint32_t counts[4];
    uint32_t name_fields[4] = {16, 0, 0, 0};
    uint8_t players[8][0x100];
    uint32_t random_state = number ^ 0x91283745u;
    uint32_t index;
    uint32_t type;
    uint32_t is_host;
    void *function;
    void *description_result = (void *)1;

    s_directplay_module = module;
    memset(&s_metadata, 0, sizeof(s_metadata));
    memcpy(module->data, module->initial_data, 0x1000);
    s_directplay_objects[0].vtable = s_directplay_vtable;
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_CONNECTED, 1);
    put_u32(module->data, 0xaa8, 1);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    function = metadata_alloc; memcpy(host + 0x20c, &function, 4);
    function = metadata_free; memcpy(host + 0x214, &function, 4);
    function = metadata_memset; memcpy(host + 0x218, &function, 4);
    s_metadata.probe_result = number % 7 == 0 ? s_errors[(number / 7) % 6] : 0x8877001eu;
    s_metadata.read_result = number % 5 == 0 ? s_errors[(number / 5) % 6] : 0;
    s_metadata.write_result = s_errors[number % 6];
    s_metadata.alloc_result = number % 11 == 0 ? 1 : 0;
    s_metadata.free_result = number % 13 == 0 ? 1 : 0;
    s_metadata.memset_result = number % 17 == 0 ? 1 : 0;
    s_metadata.results[0] = ((metadata_description_fn)send_internal(module, 34))(&description_result);
    s_metadata.results[1] = description_result == NULL ? 0 : 1;
    for (index = 0; index < 4; index++) counts[index] = random_u32(&random_state);
    s_metadata.results[2] = ((metadata_capacity_set_fn)send_internal(module, 35))(counts);
    put_u32(description, 0x40, random_u32(&random_state));
    s_metadata.results[3] = ((metadata_capacity_get_fn)send_internal(module, 36))(
        &s_metadata.results[4], description);
    put_u32(module->data, M_CONNECTED, 0);
    description_result = (void *)1;
    s_metadata.results[8] = ((metadata_description_fn)send_internal(module, 34))(&description_result);
    s_metadata.results[9] = description_result != NULL;
    put_u32(module->data, 0xaa8, 0);
    s_metadata.results[10] = ((metadata_capacity_set_fn)send_internal(module, 35))(NULL);
    put_u32(module->data, 0x988, 0);
    put_u32(module->data, 0xaa8, 1);
    s_metadata.results[11] = ((metadata_capacity_set_fn)send_internal(module, 35))(NULL);
    for (index = 0; index < 8; index++) {
        uint8_t *name = s_metadata.names[index];
        name[0] = (uint8_t)random_u32(&random_state);
        name[1] = (uint8_t)random_u32(&random_state);
        name[2] = 0x73;
        name_fields[3] = index == 7 ? 0 : (uint32_t)(uintptr_t)name;
        type = index == 6 ? 0xffffffffu : index;
        is_host = number % 3 == 0 ? 0 : random_u32(&random_state) | 1;
        name[4] = (uint8_t)((metadata_encode_fn)send_internal(module, 38))(name_fields, type, is_host);
        /* Alternate encoded values with independently generated valid and invalid names. */
        if (number & 1) {
            name[0] = "CHx"[(number / 2) % 3];
            name[1] = (uint8_t)(number / 6);
        }
        type = 0x87654321u; is_host = 0x12345678u;
        name[5] = (uint8_t)((metadata_decode_fn)send_internal(module, 37))(name_fields, &type, &is_host);
        name[6] = (uint8_t)type; name[7] = (uint8_t)is_host;
    }
    memset(players, 0, sizeof(players));
    for (index = 0; index < 8; index++)
        put_u32(players[index], 8, (number + index) % 7);
    put_u32(players[0], 4, random_u32(&random_state));
    put_u32(module->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)players[0]);
    put_u32(module->data, 0xaa8, number & 1);
    s_metadata.results[12] = ((multip_worker_fn)send_internal(module, 39))();
    /* The long-name pointer dangles into the callee's stack in both images.
     * The SetPlayerName stub captures its bytes while it is valid. */
    for (index = 0; index < s_metadata.block_count; index++)
        if (s_metadata.heap[index][0] == 0)
            put_u32(s_metadata.heap[index], 12, 0);
    put_u32(module->data, 0x160, random_u32(&random_state));
    s_metadata.results[13] = ((metadata_collision_fn)send_internal(module, 40))(players[0]);
    s_metadata.results[14] = get_u32(players[0], 0xcc);
    s_metadata.results[15] = ((metadata_collision_fn)send_internal(module, 40))(NULL);
    put_u32(module->data, M_PLAYER_COUNT, number % 9);
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)players);
    s_metadata.results[16] = ((multip_worker_fn)send_internal(module, 41))();
    s_metadata.results[17] = get_u32(module->data, 0x9a0);
    s_metadata.results[18] = get_u32(module->data, 0x9a4);
    memcpy(results, &s_metadata, sizeof(*results));
}

static uint32_t test_metadata(const struct multip_module *retail,
                              const struct multip_module *ours, uint32_t scenarios)
{
    static struct session_metadata_state s_retail;
    static struct session_metadata_state s_ours;
    uint32_t number;
    uint32_t bad = 0;
    s_directplay_vtable[0x58 / 4] = (void *)metadata_get;
    s_directplay_vtable[0x7c / 4] = (void *)metadata_set;
    s_directplay_vtable[0x54 / 4] = (void *)metadata_get_name;
    s_directplay_vtable[0x78 / 4] = (void *)metadata_set_name;
    for (number = 0; number < scenarios; number++) {
        run_metadata(retail, number, &s_retail);
        run_metadata(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                printf("metadata scenario %lu differs\n", (unsigned long)number);
                for (byte = 0; byte < sizeof(s_ours); byte++)
                    if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)byte,
                            ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                    }
            }
            bad++;
        }
    }
    printf("MULTIP session descriptors/name metadata: %lu scenarios, %lu differ\n",
           (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
