/* Application-message player types and observer transitions. */

static int32_t multip_membership_request(uint32_t id, uint32_t type, uint32_t query)
{
    uint8_t *player = NULL;
    uint32_t count_offset = type == 1 ? 0x9a0 : 0x9a4;
    uint32_t limit_offset = type == 1 ? 0x998 : 0x99c;
    uint32_t already_member = 0;

    if (M32(0xaa8) == 0) return 0;
    if (query != 0) {
        player = multip_find_player(id);
        if (player == NULL) return 0;
        already_member = A32(player, 8) == type;
    }
    if (already_member == 0 && M32(count_offset) == M32(limit_offset))
        return (type == 1 ? multip_message_1005(id) : multip_message_1006(id)) != 0 ? -1 : 0;
    if ((type == 1 ? multip_message_1003(0, id) : multip_message_1004(0, id)) != 0)
        return -1;
    if (already_member != 0) return 0;
    if (query == 0) player = multip_find_player(id);
    if (player != NULL) {
        A32(player, 8) = type;
        M32(count_offset)++;
        if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0)
            return -1;
    }
    return 0;
}

/* .text+0x0b40/+0x0c60 and repeat-query +0x0bc0/+0x0ce0. Retail does not
 * decrement the old type count when a query changes a peer's type. */
#define MULTIP_MEMBERSHIP_REQUEST(tag, type, query) \
    int32_t FS98_STDCALL multip_handle_##tag(uint32_t id, const void *packet, uint32_t size) \
    { (void)packet; (void)size; return multip_membership_request(id, type, query); }
MULTIP_MEMBERSHIP_REQUEST(1001, 1, 0)
MULTIP_MEMBERSHIP_REQUEST(1002, 2, 0)
MULTIP_MEMBERSHIP_REQUEST(1008, 1, 1)
MULTIP_MEMBERSHIP_REQUEST(1009, 2, 1)
#undef MULTIP_MEMBERSHIP_REQUEST

static int32_t multip_membership_granted(uint32_t id, const void *packet, uint32_t type)
{
    uint8_t *player = multip_find_player(A32(packet, 8));
    if (player != NULL) {
        A32(player, 8) = type;
        if (A32(player, 0xc) != 0) M32(0x984) = 0;
        if (multip_player_publish_name() != 0) return -1;
    } else {
        player = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        if (A32(player, 4) == id) {
            A32(player, 8) = type;
            M32(0x984) = 0;
            multip_player_publish_name(); /* This fallback ignores publication failure. */
        }
    }
    return 0;
}

/* .text+0x0d80/+0x0de0. The missing-record fallback compares sender ID,
 * rather than the packet's target ID, to the local player's ID. */
int32_t FS98_STDCALL multip_handle_1003(uint32_t id, const void *packet, uint32_t size)
{ (void)size; return multip_membership_granted(id, packet, 1); }
int32_t FS98_STDCALL multip_handle_1004(uint32_t id, const void *packet, uint32_t size)
{ (void)size; return multip_membership_granted(id, packet, 2); }

/* .text+0x0ef0 -- reply with the local type only for player/observer types. */
int32_t FS98_STDCALL multip_handle_1007(uint32_t id, const void *packet, uint32_t size)
{
    uint8_t *local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    (void)packet; (void)size;
    if (A32(local, 8) == 1) return multip_message_1003(id, A32(local, 4)) != 0 ? -1 : 0;
    if (A32(local, 8) == 2) return multip_message_1004(id, A32(local, 4)) != 0 ? -1 : 0;
    return 0;
}

/* .text+0x5fa0. */
int32_t FS98_STDCALL multip_message_100d(uint32_t id)
{ return multip_message_allocated(id, 0x100d); }

/* .text+0x1100 -- host acknowledges before publishing its own name/counts.
 * The requester record is not changed here. */
int32_t FS98_STDCALL multip_handle_100a(uint32_t id, const void *packet, uint32_t size)
{
    (void)packet; (void)size;
    if (M32(0xaa8) == 0) return 0;
    if (M32(0x9a4) == M32(0x99c)) return multip_message_100d(id) != 0 ? -1 : 0;
    if (multip_message_100c(id) != 0) return -1;
    if (multip_player_publish_name() != 0) return -1;
    M32(0x9a0)--;
    M32(0x9a4)++;
    return multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0 ? -1 : 0;
}

/* .text+0x1180/+0x11f0 -- accept/reject an outstanding observer request. */
int32_t FS98_STDCALL multip_handle_100c(uint32_t id, const void *packet, uint32_t size)
{
    (void)id; (void)packet; (void)size;
    if (M32(0xa64) == 0) return -1;
    A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 8) = 2;
    M32(0xa64) = 0;
    if (multip_message_1004(0, A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4)) != 0)
        return -1;
    UTIL_FN(0xa4, multip_confirm_fn)(2, 1, 0xf534, 0, 0, 0, 0, 0);
    return multip_player_publish_name() != 0 ? -1 : 0;
}
int32_t FS98_STDCALL multip_handle_100d(uint32_t id, const void *packet, uint32_t size)
{
    (void)id; (void)packet; (void)size;
    if (M32(0xa64) == 0) return -1;
    UTIL_FN(0xa4, multip_confirm_fn)(2, 1, 0xf535, 0, 0, 0, 0, 0);
    M32(0xa64) = 0;
    return 0;
}

/* .text+0x10a0/+0x10f0/+0x1230. */
int32_t FS98_STDCALL multip_handle_100f(uint32_t id, const void *packet, uint32_t size)
{ (void)packet; (void)size; return multip_message_100e(id); }
int32_t FS98_STDCALL multip_handle_1011(uint32_t id, const void *packet, uint32_t size)
{
    (void)id; (void)packet; (void)size;
    *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x832) = 0x10;
    return 0;
}
int32_t FS98_STDCALL multip_handle_1012(uint32_t id, const void *packet, uint32_t size)
{ (void)id; (void)packet; (void)size; return 0; }
