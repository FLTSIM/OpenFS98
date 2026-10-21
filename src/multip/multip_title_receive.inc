/* Aircraft title announcement, .text+0x0f40, and cache lookup +0x250. */
typedef int32_t (FS98_STDCALL *multip_aircraft_cache_exists_fn)(const char *);
int32_t FS98_STDCALL multip_model_cache_exists(const char *title)
{
    char root[264], path[264];
    HOST_FN(0x2a0, multip_string_resource_fn)(0x9010, root, 260);
    if (HOST_FN(0x184, multip_make_path_fn)(path, root, title, NULL) != 0) return 0;
    return UTIL_FN(0xa0, multip_aircraft_cache_exists_fn)(path);
}
int32_t FS98_STDCALL multip_handle_100e(uint32_t id, const void *packet, uint32_t size)
{
    uint8_t *player = multip_find_player(id);
    uint8_t temporary[12], saved[36];
    const char *title = (const char *)packet + 12;
    (void)size;
    if (player == NULL) return -1;
    A32(player, 0x38) = A32(packet, 8);
    if (HOST_FN(0x218, multip_memset_fn)(temporary, 0, 12) != 0) return -1;
    if (AC_FN(0x20, multip_aircraft_select_fn)(temporary, title) != 0) {
        if (HOST_FN(0x21c, multip_copy_fn)(saved,
            (const uint8_t *)(uintptr_t)A32(player, 0x28) + 4, 36) != 0) return -1;
        if (multip_aircraft_release(player + 0x10) != 0) return -1;
        if (multip_aircraft_select(title, player + 0x10) != 0) return -1;
        if (HOST_FN(0x21c, multip_copy_fn)((uint8_t *)(uintptr_t)A32(player, 0x28) + 4,
            saved, 36) != 0) return -1;
        if (AC_FN(0x24, multip_aircraft_release_fn)(temporary) == 0) return -1;
    } else if (multip_model_cache_exists(title) != 0) {
        if (multip_model_cache_select(player + 0x10, title) != 0) return -1;
    } else if (M32(0xad0) != 0) {
        if (multip_message_1010(id) != 0) return -1;
    }
    return 0;
}
