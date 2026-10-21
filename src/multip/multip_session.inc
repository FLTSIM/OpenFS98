/* DirectPlay session/group primitives. Included by multip.c. */
typedef int32_t (FS98_STDCALL *multip_dp_open_fn)(void *, void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_dp_enum_group_callback)(uint32_t, uint32_t, const void *, uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_dp_enum_groups_fn)(void *, const GUID *, multip_dp_enum_group_callback, void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_dp_create_group_fn)(void *, uint32_t *, void *, void *, uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *multip_dp_group_player_fn)(void *, uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *multip_memset_fn)(void *, int32_t, uint32_t);
typedef int32_t (FS98_STDCALL *multip_strcmp_fn)(const char *, const char *);

#define DP_FN(offset, type) (*(type *)(void *)(*(uint8_t **)(uintptr_t)M32(M_DIRECTPLAY) + (offset)))

/* .text+0x2400 -- only groups (type != 1) supply the two named IDs. */
static int32_t FS98_STDCALL multip_session_group_callback(uint32_t group_id,
    uint32_t type, const void *name, uint32_t flags, void *context)
{
    const char *long_name;

    (void)flags;
    (void)context;
    if (type == 1)
        return 1;
    long_name = *(const char *const *)((const uint8_t *)name + 8);
    if (HOST_FN(0x2bc, multip_strcmp_fn)(long_name,
             (const char *)(uintptr_t)(D + 0x1b0)) == 0)
        M32(0xaa0) = group_id;
    if (HOST_FN(0x2bc, multip_strcmp_fn)(long_name,
             (const char *)(uintptr_t)(D + 0x1c8)) == 0)
        M32(0xaa4) = group_id;
    return 1;
}

/* .text+0x2380 -- any nonzero Open HRESULT is failure. */
int32_t multip_session_open_join(void)
{
    return DP_FN(0x60, multip_dp_open_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
                  (void *)(uintptr_t)(D + 0x9c0), 1) != 0 ? -1 : 0;
}

/* .text+0x23c0 -- several errors are consumed as successful enumeration. */
int32_t multip_session_enum_groups(void)
{
    uint32_t result = (uint32_t)DP_FN(0x2c, multip_dp_enum_groups_fn)(
        (void *)(uintptr_t)M32(M_DIRECTPLAY), NULL,
        multip_session_group_callback, NULL, 0);

    return result == 0x80004001u || result == 0x80070057u ||
           result == 0x88770082u ? -1 : 0;
}

/* .text+0x2340 -- leave previous flags intact on either failure. */
int32_t multip_session_join(void)
{
    if (multip_session_open_join() != 0 || multip_session_enum_groups() != 0)
        return -1;
    M32(0xaa8) = 0;
    M32(M_CONNECTED) = 1;
    return 0;
}

/* .text+0x3940 -- names share one DPNAME descriptor; each call sees its
 * current long-name pointer. A first success is retained if the second fails. */
int32_t multip_session_create_groups(void)
{
    M32(0x140) = D + 0x1b0;
    if (DP_FN(0x14, multip_dp_create_group_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        (uint32_t *)(uintptr_t)(D + 0xaa0), (void *)(uintptr_t)(D + 0x138),
        NULL, 0, 0) != 0)
        return -1;
    M32(0x140) = D + 0x1c8;
    return DP_FN(0x14, multip_dp_create_group_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        (uint32_t *)(uintptr_t)(D + 0xaa4), (void *)(uintptr_t)(D + 0x138),
        NULL, 0, 0) != 0 ? -1 : 0;
}

/* .text+0x3890 -- host session descriptor and group construction. */
int32_t multip_session_host(void)
{
    const uint32_t *application = (const uint32_t *)(uintptr_t)M32(0xa38);
    uint32_t index;

    if (HOST_FN(0x218, multip_memset_fn)((void *)(uintptr_t)(D + 0x9c0), 0, 0x50) != 0)
        return -1;
    M32(0x9c0) = 0x50;
    M32(0x9c4) = M32(0x190);
    for (index = 0; index < 4; index++)
        M32(0x9d8 + index * 4) = application[index];
    M32(0x9e8) = M32(0x998) + M32(0x99c);
    M32(0x9f0) = D + 0xae0;
    if (DP_FN(0x60, multip_dp_open_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        (void *)(uintptr_t)(D + 0x9c0), 2) != 0)
        return -1;
    M32(0xaa8) = 1;
    M32(M_CONNECTED) = 1;
    return multip_session_create_groups() != 0 ? -1 : 0;
}

/* .text+0x97a0/+0x97f0 -- swap the local player's group, ignoring errors. */
int32_t multip_session_set_player(void)
{
    uint8_t *local;

    if (M32(M_CONNECTED) != 0) {
        local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        DP_FN(0x1c, multip_dp_group_player_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
            M32(0xaa4), *(uint32_t *)(void *)(local + 4));
        local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        DP_FN(0x0c, multip_dp_group_player_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
            M32(0xaa0), *(uint32_t *)(void *)(local + 4));
    }
    return 0;
}

int32_t multip_session_set_observer(void)
{
    uint8_t *local;

    if (M32(M_CONNECTED) != 0) {
        local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        DP_FN(0x1c, multip_dp_group_player_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
            M32(0xaa0), *(uint32_t *)(void *)(local + 4));
        local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        DP_FN(0x0c, multip_dp_group_player_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
            M32(0xaa4), *(uint32_t *)(void *)(local + 4));
    }
    return 0;
}

typedef int32_t (FS98_STDCALL *multip_dp_get_description_fn)(void *, void *, uint32_t *);
typedef int32_t (FS98_STDCALL *multip_dp_set_description_fn)(void *, void *, uint32_t);

/* .text+0xa7a0 -- the first query must return BUFFERTOOSMALL exactly.
 * The allocated descriptor remains owned by the caller even on read failure. */
int32_t FS98_STDCALL multip_session_get_description(void **description_out)
{
    uint32_t size = 0;

    *description_out = NULL;
    if (M32(M_CONNECTED) == 0)
        return 0;
    if ((uint32_t)DP_FN(0x58, multip_dp_get_description_fn)(
        (void *)(uintptr_t)M32(M_DIRECTPLAY), NULL, &size) != 0x8877001eu)
        return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, description_out, 0) != 0)
        return -1;
    *(uint32_t *)*description_out = size;
    return DP_FN(0x58, multip_dp_get_description_fn)(
        (void *)(uintptr_t)M32(M_DIRECTPLAY), *description_out, &size) != 0 ? -1 : 0;
}

/* .text+0x9f40 -- the four byte counts use player/observer order on input,
 * and max-player/current-player/max-observer/current-observer on the wire. */
int32_t FS98_STDCALL multip_session_set_capacity(const uint32_t *counts)
{
    void *description;
    uint32_t result;

    if (M32(M_DIRECTPLAY) == 0 || M32(0xaa8) == 0)
        return 0;
    if (multip_session_get_description(&description) != 0)
        return -1;
    *(uint32_t *)((uint8_t *)description + 0x40) =
        (counts[0] << 24) | ((counts[2] & 255) << 16) |
        ((counts[1] & 255) << 8) | (counts[3] & 255);
    result = (uint32_t)DP_FN(0x7c, multip_dp_set_description_fn)(
        (void *)(uintptr_t)M32(M_DIRECTPLAY), description, 0);
    if (result != 0 && result != 0x80004001u)
        return -1;
    return HOST_FN(0x214, multip_free_fn)(&description) != 0 ? -1 : 0;
}

/* .text+0xa020 */
int32_t FS98_STDCALL multip_session_get_capacity(uint32_t *counts,
                                                const void *description)
{
    uint32_t packed = *(const uint32_t *)((const uint8_t *)description + 0x40);

    counts[0] = packed >> 24;
    counts[2] = (packed >> 16) & 255;
    counts[1] = (packed >> 8) & 255;
    counts[3] = packed & 255;
    return 0;
}

/* .text+0xa050 -- a valid host byte is published before validating type. */
int32_t FS98_STDCALL multip_player_decode_name(const void *name,
    uint32_t *type, uint32_t *host)
{
    const char *name_text = *(const char *const *)((const uint8_t *)name + 12);

    if (name_text == NULL)
        return -1;
    if (name_text[0] == 'C')
        *host = 0;
    else if (name_text[0] == 'H')
        *host = 1;
    else
        return -1;
    switch (name_text[1]) {
    case 'N': *type = 0; break;
    case 'P': *type = 1; break;
    case 'O': *type = 2; break;
    case 'U': *type = 4; break;
    default: return -1;
    }
    return 0;
}

/* .text+0xa100 -- invalid type leaves the host byte changed. */
int32_t FS98_STDCALL multip_player_encode_name(void *name,
    uint32_t type, uint32_t host)
{
    char *name_text = *(char **)((uint8_t *)name + 12);
    char type_code;

    if (name_text == NULL)
        return -1;
    name_text[0] = host != 0 ? 'H' : 'C';
    switch (type) {
    case 0: type_code = 'N'; break;
    case 1: type_code = 'P'; break;
    case 2: type_code = 'O'; break;
    case 4: type_code = 'U'; break;
    default: return -1;
    }
    name_text[1] = type_code;
    name_text[2] = 0;
    return 0;
}

typedef int32_t (FS98_STDCALL *multip_dp_get_player_name_fn)(void *, uint32_t, void *, uint32_t *);
typedef int32_t (FS98_STDCALL *multip_dp_set_player_name_fn)(void *, uint32_t, void *, uint32_t);

/* .text+0xa1a0 -- the size-query error is ignored, and the allocated name
 * is never freed, including on successful publication. */
int32_t multip_player_publish_name(void)
{
    char name_text[10];
    void *name = NULL;
    uint32_t size = 0;
    uint8_t *local;

    if (HOST_FN(0x218, multip_memset_fn)(name_text, 0, sizeof(name_text)) != 0)
        return -1;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    DP_FN(0x54, multip_dp_get_player_name_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        *(uint32_t *)(void *)(local + 4), name, &size);
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, &name, 1) != 0)
        return -1;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    if (DP_FN(0x54, multip_dp_get_player_name_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        *(uint32_t *)(void *)(local + 4), name, &size) != 0)
        return -1;
    *(char **)((uint8_t *)name + 12) = name_text;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    if (multip_player_encode_name(name, *(uint32_t *)(void *)(local + 8),
                                 M32(0xaa8)) != 0)
        return -1;
    local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    return DP_FN(0x78, multip_dp_set_player_name_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        *(uint32_t *)(void *)(local + 4), name, 2) != 0 ? -1 : 0;
}

/* .text+0xa590 -- fixed-point countdown, with the high bits discarded. */
int32_t FS98_STDCALL multip_player_reset_collision(void *player)
{
    if (player != NULL)
        *(uint32_t *)((uint8_t *)player + 0xcc) = M32(0x160) << 16;
    return 0;
}

/* .text+0xa8d0 -- count includes the local record; unknown types are skipped. */
int32_t multip_player_count_types(void)
{
    uint32_t count = M32(M_PLAYER_COUNT);
    uint8_t *players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    uint32_t type;

    M32(0x9a0) = 0;
    M32(0x9a4) = 0;
    while (count != 0) {
        count--;
        type = *(uint32_t *)(void *)(players + count * 0x100 + 8);
        if (type == 1)
            M32(0x9a0)++;
        else if (type == 2)
            M32(0x9a4)++;
    }
    return 0;
}
