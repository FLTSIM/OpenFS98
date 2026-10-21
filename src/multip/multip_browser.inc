/* Session browser and its player-list callbacks, .text+0x42f0..+0x47ec. */
typedef void *(FS98_STDCALL *multip_list_find_fn)(void *, uint32_t, const void *, uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_enum_session_callback_fn)(const void *, uint32_t *, uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_enum_sessions_fn)(void *, void *, uint32_t,
    multip_enum_session_callback_fn, void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_dialog_fn)(uint32_t, void *);
typedef void (FS98_STDCALL *multip_exchange_read_fn)(void *, uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_exchange_write_fn)(void *, uint32_t, void *, uint32_t);
typedef uint32_t (FS98_STDCALL *multip_numeric_format_fn)(const void *, uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_numeric_parse_fn)(const void *, uint32_t, void *, void *);
#define NUMERIC_FN(offset, type) (*(type *)(uintptr_t)(M32(0x3c) + (offset)))

/* .text+0x4540 -- a failed name free leaves the nested list untouched. */
static int32_t FS98_STDCALL multip_browser_record_free(void *record, void *context)
{
    (void)context;
    if (HOST_FN(0x214, multip_free_fn)((void **)record) != 0) return -1;
    return multip_player_names_clear((uint8_t *)record + 0x38) != 0 ? -1 : 0;
}
/* .text+0x44c0 -- ignore walker errors and free through a temporary. */
int32_t multip_browser_clear(void)
{
    void *data;
    HOST_FN(0x308, multip_list_walk_fn)((void *)(uintptr_t)(D + 0x368), NULL,
        multip_browser_record_free);
    data = (void *)(uintptr_t)M32(0x380);
    if (HOST_FN(0x214, multip_free_fn)(&data) != 0) return -1;
    M32(0x368) = 0; M32(0x36c) = 0; M32(0x378) = 0; M32(0x37c) = 0; M32(0x380) = 0;
    M32(0x370) = 32; M32(0x374) = 84;
    return 0;
}
static int32_t FS98_STDCALL multip_browser_mark_stale(void *record, void *context)
{
    (void)context;
    A32(record, 0x1c) = 0;
    return 0;
}
static int32_t FS98_STDCALL multip_browser_prune(void *record, void *context)
{
    if (A32(record, 0x1c) == 0) {
        if (multip_browser_record_free(record, NULL) != 0) return -1;
        HOST_FN(0x300, multip_list_remove_fn)(context, record);
    }
    return 0;
}
/* .text+0x4430 -- TIMEOUT stops enumeration without inspecting the descriptor.
 * Existing records are initialized again, leaking their previous names/lists. */
static int32_t FS98_STDCALL multip_browser_session_callback(const void *description,
    uint32_t *timeout, uint32_t flags, void *context)
{
    void *record;
    (void)timeout; (void)context;
    if (flags & 1) return 0;
    record = HOST_FN(0x310, multip_list_find_fn)((void *)(uintptr_t)(D + 0x368),
        4, (const uint8_t *)description + 8, 16, NULL);
    if (record == NULL) {
        record = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0x368));
        if (HOST_FN(0x218, multip_memset_fn)(record, 0, 84) != 0) return -1;
    }
    if (multip_session_record_init(description, record) != 0) return -1;
    A32(record, 0x1c) = 1;
    return 1;
}
/* .text+0x42f0 -- publish only after successful EnumSessions. Stale-record
 * visitor failures are ignored by the caller, as are marker-walk errors. */
int32_t FS98_STDCALL multip_browser_refresh(void *dialog)
{
    uint32_t description[20];
    const uint32_t *application;
    void *progress;
    int32_t result;
    uint32_t index;
    M32(0xc64) = HOST_FN(0x38, multip_ticks_fn)();
    if (HOST_FN(0x218, multip_memset_fn)(description, 0, 80) != 0) return -1;
    HOST_FN(0x308, multip_list_walk_fn)((void *)(uintptr_t)(D + 0x368), NULL,
        multip_browser_mark_stale);
    description[0] = 80;
    application = (const uint32_t *)(uintptr_t)M32(0xa38);
    for (index = 0; index < 4; index++) description[6 + index] = application[index];
    for (index = 0; index < 4; index++) description[2 + index] =
        *(const uint32_t *)(const void *)(s_multip_rdata + 0x1d8 + index * 4);
    UTIL_FN(0xa8, multip_progress_open_fn)(&progress, 1, 0xf53a, 0, 0, 0, 0, 0);
    result = DP_FN(0x34, multip_enum_sessions_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        description, 0, multip_browser_session_callback, NULL, 1);
    UTIL_FN(0xac, multip_progress_close_fn)(&progress);
    if (result != 0) return -1;
    HOST_FN(0x308, multip_list_walk_fn)((void *)(uintptr_t)(D + 0x368),
        (void *)(uintptr_t)(D + 0x368), multip_browser_prune);
    A32(dialog, 0xac) = M32(0x380); A32(dialog, 0xb0) = M32(0x368);
    return 0;
}
/* .text+0x4660 -- selected browser record's player list. */
static int32_t FS98_STDCALL multip_browser_refresh_players(void *dialog, uint32_t index)
{
    uint8_t *record = (uint8_t *)(uintptr_t)A32(dialog, 0xac) + index * 84;
    return multip_session_enumerate_players(record, (const GUID *)(record + 4), 0x80) != 0 ? -1 : 0;
}
/* .text+0x45c0 -- failure and empty-success have deliberately different returns. */
static int32_t FS98_STDCALL multip_browser_refresh_all(void *dialog)
{
    uint32_t index = 0;
    if (multip_browser_refresh(dialog) != 0) return 0;
    while (index < A32(dialog, 0xb0)) {
        if (multip_browser_refresh_players(dialog, index) != 0) return 0;
        index++;
    }
    return 1;
}
/* .text+0x46a0 -- return an allocated deep copy in reverse record order. */
static int32_t FS98_STDCALL multip_browser_player_copy(void **players_out,
    uint32_t *count_out, void *counts_out, void *dialog)
{
    uint8_t *record = (uint8_t *)(uintptr_t)A32(dialog, 0xac) + A32(dialog, 0xb8) * 84;
    uint8_t *players;
    uint32_t count;
    if (multip_session_enumerate_players(record, (const GUID *)(record + 4), 0x80) != 0) return -1;
    if (A32(record, 0x18) == 0) return -1;
    *count_out = A32(record, 0x30);
    if (HOST_FN(0x21c, multip_copy_fn)(counts_out, record + 0x20, 16) != 0) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, A32(record, 0x30) * 16, (void **)&players, 0) != 0) return -1;
    count = A32(record, 0x30);
    while (count != 0) {
        uint32_t offset = --count * 16;
        if (multip_player_name_record_copy(players + offset,
            (const uint8_t *)(uintptr_t)A32(record, 0x34) + offset) != 0) return -1;
    }
    *players_out = players;
    return 0;
}
/* .text+0x47a0 -- decrement before freeing; browser-clear failure is ignored. */
int32_t multip_configuration_clear(void)
{
    while (M32(0x430) != 0) {
        uint8_t *records = (uint8_t *)(uintptr_t)M32(0x42c);
        uint32_t index = M32(0x430) - 1;
        M32(0x430) = index;
        if (HOST_FN(0x214, multip_free_fn)((void **)(records + index * 28)) != 0) return -1;
    }
    multip_browser_clear();
    return 0;
}
