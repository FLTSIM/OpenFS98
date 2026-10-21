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
    uint32_t uiProbeResult;
    uint32_t uiReadResult;
    uint32_t uiWriteResult;
    uint32_t uiAllocResult;
    uint32_t uiFreeResult;
    uint32_t uiMemsetResult;
    uint32_t uiCalls;
    uint32_t auiLog[16][6];
    uint32_t uiBlocks;
    uint32_t auiLive[4];
    uint8_t abyHeap[4][128];
    uint32_t auiResults[32];
    uint8_t abyNames[8][8];
};
static struct session_metadata_state s_metadata;

static uint32_t *metadata_log(uint32_t uiTag)
{
    uint32_t *puiEntry;
    if (s_metadata.uiCalls >= 16) {
        puts("metadata fixture log exhausted"); exit(2);
    }
    puiEntry = s_metadata.auiLog[s_metadata.uiCalls++];
    puiEntry[0] = uiTag;
    return puiEntry;
}

static int32_t STDCALL metadata_alloc(int32_t iKind, uint32_t uiSize,
                                      void **ppOut, int32_t iZero)
{
    uint32_t *puiEntry = metadata_log(1);
    puiEntry[1] = iKind; puiEntry[2] = uiSize; puiEntry[3] = iZero;
    if (s_metadata.uiAllocResult != 0)
        return (int32_t)s_metadata.uiAllocResult;
    if (uiSize > 128 || s_metadata.uiBlocks >= 4) exit(2);
    *ppOut = s_metadata.abyHeap[s_metadata.uiBlocks];
    s_metadata.auiLive[s_metadata.uiBlocks++] = 1;
    return 0;
}

static int32_t STDCALL metadata_free(void **ppBlock)
{
    uint32_t uiIndex;
    uint32_t *puiEntry = metadata_log(2);
    for (uiIndex = 0; uiIndex < s_metadata.uiBlocks; uiIndex++)
        if (*ppBlock == s_metadata.abyHeap[uiIndex]) break;
    puiEntry[1] = uiIndex;
    if (uiIndex == s_metadata.uiBlocks) exit(2);
    if (s_metadata.uiFreeResult == 0) {
        s_metadata.auiLive[uiIndex] = 0;
        *ppBlock = NULL;
    }
    return (int32_t)s_metadata.uiFreeResult;
}

static int32_t STDCALL metadata_get(void *pObject, void *pDescription, uint32_t *puiSize)
{
    uint32_t *puiEntry = metadata_log(3);
    puiEntry[1] = directplay_identity(pObject);
    puiEntry[2] = pDescription != NULL;
    puiEntry[3] = *puiSize;
    if (pDescription != NULL) {
        puiEntry[4] = get_u32(pDescription, 0);
        memset((uint8_t *)pDescription + 4, 0x65, 76);
    }
    *puiSize = 80;
    return (int32_t)(pDescription != NULL ? s_metadata.uiReadResult : s_metadata.uiProbeResult);
}

static int32_t STDCALL metadata_memset(void *pDest, int32_t iValue, uint32_t uiSize)
{
    uint32_t *puiEntry = metadata_log(5);
    puiEntry[1] = iValue; puiEntry[2] = uiSize;
    memset(pDest, iValue, uiSize);
    return (int32_t)s_metadata.uiMemsetResult;
}

static int32_t STDCALL metadata_get_name(void *pObject, uint32_t uiId,
                                        void *pName, uint32_t *puiSize)
{
    uint32_t *puiEntry = metadata_log(6);
    puiEntry[1] = directplay_identity(pObject);
    puiEntry[2] = uiId; puiEntry[3] = pName != NULL; puiEntry[4] = *puiSize;
    *puiSize = 32;
    if (pName != NULL) memset(pName, 0, 32);
    return (int32_t)(pName != NULL ? s_metadata.uiReadResult : s_metadata.uiProbeResult);
}

static int32_t STDCALL metadata_set_name(void *pObject, uint32_t uiId,
                                        void *pName, uint32_t uiFlags)
{
    uint32_t *puiEntry = metadata_log(7);
    const uint8_t *pbyName = (const uint8_t *)(uintptr_t)get_u32(pName, 12);
    puiEntry[1] = directplay_identity(pObject); puiEntry[2] = uiId;
    puiEntry[3] = get_u32(pName, 0); puiEntry[4] = uiFlags;
    puiEntry[5] = pbyName[0] | (pbyName[1] << 8) | (pbyName[2] << 16);
    return (int32_t)s_metadata.uiWriteResult;
}

static int32_t STDCALL metadata_set(void *pObject, void *pDescription, uint32_t uiFlags)
{
    uint32_t *puiEntry = metadata_log(4);
    puiEntry[1] = directplay_identity(pObject);
    puiEntry[2] = get_u32(pDescription, 0);
    puiEntry[3] = get_u32(pDescription, 0x40);
    puiEntry[4] = uiFlags;
    return (int32_t)s_metadata.uiWriteResult;
}

typedef int32_t (STDCALL *metadata_description_fn)(void **);
typedef int32_t (STDCALL *metadata_capacity_set_fn)(const uint32_t *);
typedef int32_t (STDCALL *metadata_capacity_get_fn)(uint32_t *, const void *);
typedef int32_t (STDCALL *metadata_decode_fn)(const void *, uint32_t *, uint32_t *);
typedef int32_t (STDCALL *metadata_encode_fn)(void *, uint32_t, uint32_t);
typedef int32_t (STDCALL *metadata_collision_fn)(void *);

static void run_metadata(const struct multip_module *pModule, uint32_t uiNumber,
                         struct session_metadata_state *pResults)
{
    static const uint32_t s_auiErrors[] = {0, 1, 0x80004001u, 0x80070057u,
        0x8877001eu, 0xffffffffu};
    uint8_t abyHost[0x2c0] = {0};
    uint8_t abyDescription[80] = {0};
    uint32_t auiCounts[4];
    uint32_t auiName[4] = {16, 0, 0, 0};
    uint8_t abyPlayers[8][0x100];
    uint32_t uiState = uiNumber ^ 0x91283745u;
    uint32_t uiIndex;
    uint32_t uiType;
    uint32_t uiHost;
    void *pFunction;
    void *pDescription = (void *)1;

    s_directplay_module = pModule;
    memset(&s_metadata, 0, sizeof(s_metadata));
    memcpy(pModule->data, pModule->initial_data, 0x1000);
    s_directplay_objects[0].vtable = s_directplay_vtable;
    put_u32(pModule->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(pModule->data, M_CONNECTED, 1);
    put_u32(pModule->data, 0xaa8, 1);
    put_u32(pModule->data, 4, (uint32_t)(uintptr_t)abyHost);
    pFunction = metadata_alloc; memcpy(abyHost + 0x20c, &pFunction, 4);
    pFunction = metadata_free; memcpy(abyHost + 0x214, &pFunction, 4);
    pFunction = metadata_memset; memcpy(abyHost + 0x218, &pFunction, 4);
    s_metadata.uiProbeResult = uiNumber % 7 == 0 ? s_auiErrors[(uiNumber / 7) % 6] : 0x8877001eu;
    s_metadata.uiReadResult = uiNumber % 5 == 0 ? s_auiErrors[(uiNumber / 5) % 6] : 0;
    s_metadata.uiWriteResult = s_auiErrors[uiNumber % 6];
    s_metadata.uiAllocResult = uiNumber % 11 == 0 ? 1 : 0;
    s_metadata.uiFreeResult = uiNumber % 13 == 0 ? 1 : 0;
    s_metadata.uiMemsetResult = uiNumber % 17 == 0 ? 1 : 0;
    s_metadata.auiResults[0] = ((metadata_description_fn)send_internal(pModule, 34))(&pDescription);
    s_metadata.auiResults[1] = pDescription == NULL ? 0 : 1;
    for (uiIndex = 0; uiIndex < 4; uiIndex++) auiCounts[uiIndex] = random_u32(&uiState);
    s_metadata.auiResults[2] = ((metadata_capacity_set_fn)send_internal(pModule, 35))(auiCounts);
    put_u32(abyDescription, 0x40, random_u32(&uiState));
    s_metadata.auiResults[3] = ((metadata_capacity_get_fn)send_internal(pModule, 36))(
        &s_metadata.auiResults[4], abyDescription);
    put_u32(pModule->data, M_CONNECTED, 0);
    pDescription = (void *)1;
    s_metadata.auiResults[8] = ((metadata_description_fn)send_internal(pModule, 34))(&pDescription);
    s_metadata.auiResults[9] = pDescription != NULL;
    put_u32(pModule->data, 0xaa8, 0);
    s_metadata.auiResults[10] = ((metadata_capacity_set_fn)send_internal(pModule, 35))(NULL);
    put_u32(pModule->data, 0x988, 0);
    put_u32(pModule->data, 0xaa8, 1);
    s_metadata.auiResults[11] = ((metadata_capacity_set_fn)send_internal(pModule, 35))(NULL);
    for (uiIndex = 0; uiIndex < 8; uiIndex++) {
        uint8_t *pbyName = s_metadata.abyNames[uiIndex];
        pbyName[0] = (uint8_t)random_u32(&uiState);
        pbyName[1] = (uint8_t)random_u32(&uiState);
        pbyName[2] = 0x73;
        auiName[3] = uiIndex == 7 ? 0 : (uint32_t)(uintptr_t)pbyName;
        uiType = uiIndex == 6 ? 0xffffffffu : uiIndex;
        uiHost = uiNumber % 3 == 0 ? 0 : random_u32(&uiState) | 1;
        pbyName[4] = (uint8_t)((metadata_encode_fn)send_internal(pModule, 38))(auiName, uiType, uiHost);
        /* Alternate encoded values with independently generated valid and invalid names. */
        if (uiNumber & 1) {
            pbyName[0] = "CHx"[(uiNumber / 2) % 3];
            pbyName[1] = (uint8_t)(uiNumber / 6);
        }
        uiType = 0x87654321u; uiHost = 0x12345678u;
        pbyName[5] = (uint8_t)((metadata_decode_fn)send_internal(pModule, 37))(auiName, &uiType, &uiHost);
        pbyName[6] = (uint8_t)uiType; pbyName[7] = (uint8_t)uiHost;
    }
    memset(abyPlayers, 0, sizeof(abyPlayers));
    for (uiIndex = 0; uiIndex < 8; uiIndex++)
        put_u32(abyPlayers[uiIndex], 8, (uiNumber + uiIndex) % 7);
    put_u32(abyPlayers[0], 4, random_u32(&uiState));
    put_u32(pModule->data, 0x988, (uint32_t)(uintptr_t)&s_directplay_objects[0]);
    put_u32(pModule->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)abyPlayers[0]);
    put_u32(pModule->data, 0xaa8, uiNumber & 1);
    s_metadata.auiResults[12] = ((multip_worker_fn)send_internal(pModule, 39))();
    /* The long-name pointer dangles into the callee's stack in both images.
     * The SetPlayerName stub captures its bytes while it is valid. */
    for (uiIndex = 0; uiIndex < s_metadata.uiBlocks; uiIndex++)
        if (s_metadata.abyHeap[uiIndex][0] == 0)
            put_u32(s_metadata.abyHeap[uiIndex], 12, 0);
    put_u32(pModule->data, 0x160, random_u32(&uiState));
    s_metadata.auiResults[13] = ((metadata_collision_fn)send_internal(pModule, 40))(abyPlayers[0]);
    s_metadata.auiResults[14] = get_u32(abyPlayers[0], 0xcc);
    s_metadata.auiResults[15] = ((metadata_collision_fn)send_internal(pModule, 40))(NULL);
    put_u32(pModule->data, M_PLAYER_COUNT, uiNumber % 9);
    put_u32(pModule->data, M_PLAYERS, (uint32_t)(uintptr_t)abyPlayers);
    s_metadata.auiResults[16] = ((multip_worker_fn)send_internal(pModule, 41))();
    s_metadata.auiResults[17] = get_u32(pModule->data, 0x9a0);
    s_metadata.auiResults[18] = get_u32(pModule->data, 0x9a4);
    memcpy(pResults, &s_metadata, sizeof(*pResults));
}

static uint32_t test_metadata(const struct multip_module *pRetail,
                              const struct multip_module *pOurs, uint32_t uiScenarios)
{
    static struct session_metadata_state s_Retail;
    static struct session_metadata_state s_Ours;
    uint32_t uiNumber;
    uint32_t uiBad = 0;
    s_directplay_vtable[0x58 / 4] = (void *)metadata_get;
    s_directplay_vtable[0x7c / 4] = (void *)metadata_set;
    s_directplay_vtable[0x54 / 4] = (void *)metadata_get_name;
    s_directplay_vtable[0x78 / 4] = (void *)metadata_set_name;
    for (uiNumber = 0; uiNumber < uiScenarios; uiNumber++) {
        run_metadata(pRetail, uiNumber, &s_Retail);
        run_metadata(pOurs, uiNumber, &s_Ours);
        if (memcmp(&s_Retail, &s_Ours, sizeof(s_Ours)) != 0) {
            if (uiBad < 4) {
                uint32_t uiByte;
                printf("metadata scenario %lu differs\n", (unsigned long)uiNumber);
                for (uiByte = 0; uiByte < sizeof(s_Ours); uiByte++)
                    if (((uint8_t *)&s_Retail)[uiByte] != ((uint8_t *)&s_Ours)[uiByte]) {
                        printf("  first byte %lu: retail %u ours %u\n", (unsigned long)uiByte,
                            ((uint8_t *)&s_Retail)[uiByte], ((uint8_t *)&s_Ours)[uiByte]); break;
                    }
            }
            uiBad++;
        }
    }
    printf("MULTIP session descriptors/name metadata: %lu scenarios, %lu differ\n",
           (unsigned long)uiScenarios, (unsigned long)uiBad);
    return uiBad;
}
