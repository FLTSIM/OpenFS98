/* DirectPlay system notifications. Sizes and timestamps are ignored by these
 * retail handlers; their dispatch owner is recovered separately. */

/* .text+0x8460 -- a group name can update either or both cached IDs. */
int32_t FS98_STDCALL multip_system_group(const void *name, uint32_t id)
{
    multip_session_group_callback(id, 0, name, 0, NULL);
    return 0;
}

/* .text+0x80a0 -- newly created peers are added before ping and type query. */
int32_t FS98_STDCALL multip_system_created(const void *message, uint32_t size)
{
    uint32_t type = A32(message, 4), id = A32(message, 8);
    (void)size;
    if (type == 1) {
        if (multip_player_add_remote((const uint8_t *)message + 0x18, id) != 0) return -1;
        if (multip_message_1007(id) != 0) return -1;
        if (multip_message_100f(id) != 0) return -1;
    } else if (type == 0) {
        if (multip_system_group((const uint8_t *)message + 0x18, id) != 0) return -1;
    }
    return 0;
}

/* .text+0x8110 -- retail returns failure even after successful removal. */
int32_t FS98_STDCALL multip_system_destroyed(const void *message, uint32_t size)
{
    (void)size;
    if (A32(message, 4) == 1 && A32(message, 8) !=
        A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4))
        multip_player_remove(A32(message, 8));
    return -1;
}

/* .text+0x8060 -- a data update from an unknown nonlocal player is failure. */
int32_t FS98_STDCALL multip_system_player_data(const void *message, uint32_t size, uint32_t timestamp)
{
    (void)size; (void)timestamp;
    if (A32(message, 4) == 1 && A32(message, 8) !=
        A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4) &&
        multip_find_player(A32(message, 8)) == NULL)
        return -1;
    return 0;
}

/* .text+0x8140 -- migration sets host state before its first query. */
int32_t FS98_STDCALL multip_system_host(const void *message, uint32_t size)
{
    void *description;
    (void)message; (void)size;
    M32(0xaa8) = 1;
    if (multip_session_get_description(&description) != 0) return -1;
    if (multip_session_get_capacity((uint32_t *)(uintptr_t)(D + 0x998), description) != 0) return -1;
    if (multip_player_count_types() != 0) return -1;
    if (multip_player_publish_name() != 0) return -1;
    if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0) return -1;
    UTIL_FN(0xa4, multip_confirm_fn)(2, 1, 0xf536, 0xf537, 0, 0, 0, 0);
    return HOST_FN(0x214, multip_free_fn)(&description) != 0 ? -1 : 0;
}
