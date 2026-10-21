/* Session browser records, .text+0xa2a0/+0xa360/+0xa4d0. */
int32_t FS98_STDCALL multip_session_record_init(const void *description, void *record)
{
    const char *name_text = *(const char *const *)((const uint8_t *)description + 0x30);
    uint32_t size = HOST_FN(0x2b8, multip_strlen_fn)(name_text) + 1;

    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, (void **)record, 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)record, name_text);
    if (HOST_FN(0x21c, multip_copy_fn)((uint8_t *)record + 4,
        (const uint8_t *)description + 8, 16) != 0)
        return -1;
    A32(record, 0x14) = A32(description, 4);
    A32(record, 0x18) = 0;
    A32(record, 0x34) = 0;
    A32(record, 0x30) = 0;
    if (multip_session_get_capacity((uint32_t *)((uint8_t *)record + 0x20),
                                   description) != 0)
        return -1;
    A32(record, 0x38) = 0;
    A32(record, 0x3c) = 0;
    A32(record, 0x40) = 32;
    A32(record, 0x44) = 16;
    A32(record, 0x48) = 0;
    A32(record, 0x4c) = 0;
    A32(record, 0x50) = 0;
    return 0;
}

int32_t FS98_STDCALL multip_player_name_record_init(uint32_t id,
    uint32_t type, const void *name, void *record)
{
    const char *name_text = *(const char *const *)((const uint8_t *)name + 8);
    uint32_t size;

    (void)type;
    A32(record, 0) = id;
    size = HOST_FN(0x2b8, multip_strlen_fn)(name_text) + 1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size,
        (void **)((uint8_t *)record + 12), 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)((uint8_t *)record + 12), name_text);
    return 0;
}

int32_t FS98_STDCALL multip_player_name_record_copy(void *dest, const void *source)
{
    const char *name_text;
    uint32_t size;

    if (HOST_FN(0x21c, multip_copy_fn)(dest, source, 16) != 0)
        return -1;
    name_text = *(const char *const *)((const uint8_t *)source + 12);
    size = HOST_FN(0x2b8, multip_strlen_fn)(name_text) + 1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size,
        (void **)((uint8_t *)dest + 12), 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)((uint8_t *)dest + 12), name_text);
    return 0;
}

/* .text+0xa540 -- host-list visitor, context unused. */
int32_t FS98_STDCALL multip_player_name_record_free(void *record, void *context)
{
    (void)context;
    return HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)record + 12)) != 0 ? -1 : 0;
}

typedef void *(FS98_STDCALL *multip_list_add_fn)(void *);
typedef int32_t (FS98_STDCALL *multip_list_visitor_fn)(void *, void *);
typedef int32_t (FS98_STDCALL *multip_list_walk_fn)(void *, void *, multip_list_visitor_fn);
typedef int32_t (FS98_STDCALL *multip_dp_enum_players_fn)(void *, const GUID *,
    multip_dp_enum_group_callback, void *, uint32_t);

/* .text+0xa470 -- walker failures are ignored. Freeing a temporary pointer
 * leaves the original header intact when the final free fails. */
int32_t FS98_STDCALL multip_player_names_clear(void *list)
{
    void *data;

    HOST_FN(0x308, multip_list_walk_fn)(list, NULL, multip_player_name_record_free);
    data = *(void **)((uint8_t *)list + 0x18);
    if (HOST_FN(0x214, multip_free_fn)(&data) != 0)
        return -1;
    A32(list, 0) = 0;
    A32(list, 4) = 0;
    A32(list, 0x10) = 0;
    A32(list, 0x14) = 0;
    A32(list, 0x18) = 0;
    A32(list, 8) = 32;
    A32(list, 0xc) = 16;
    return 0;
}

/* .text+0xa420 -- append before name validation, retaining an invalid last
 * record when the enumeration stops. */
static int32_t FS98_STDCALL multip_player_names_callback(uint32_t id,
    uint32_t type, const void *name, uint32_t flags, void *context)
{
    uint8_t *record;

    (void)flags;
    record = HOST_FN(0x2f8, multip_list_add_fn)((uint8_t *)context + 0x38);
    if (multip_player_name_record_init(id, type, name, record) != 0)
        return 0;
    return multip_player_decode_name(name, (uint32_t *)(void *)(record + 4),
                                    (uint32_t *)(void *)(record + 8)) == 0;
}

/* .text+0xa3c0 -- EnumPlayers' HRESULT is consumed; partial lists publish. */
int32_t FS98_STDCALL multip_session_enumerate_players(void *record,
    const GUID *instance, uint32_t flags)
{
    if (record == NULL)
        return -1;
    if (multip_player_names_clear((uint8_t *)record + 0x38) != 0)
        return -1;
    DP_FN(0x30, multip_dp_enum_players_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        instance, multip_player_names_callback, record, flags);
    A32(record, 0x34) = A32(record, 0x50);
    A32(record, 0x30) = A32(record, 0x38);
    A32(record, 0x18) = 1;
    return 0;
}
