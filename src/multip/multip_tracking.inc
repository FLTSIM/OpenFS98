/* Selected-player name display, .text+0x96b0/+0x9710/+0x9740/+0x8cb0. */
typedef uint32_t (FS98_STDCALL *multip_chain_callback_fn)(uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *multip_chain_register_fn)(uint32_t, multip_chain_callback_fn);
typedef uint32_t (FS98_STDCALL *multip_draw_name_fn)(uint32_t, uint32_t, uint32_t, const char *, uint32_t);

#define CHAIN_FN(offset, type) (*(type *)(uintptr_t)(M32(0x1c) + (offset)))

uint32_t FS98_STDCALL multip_track_callback(uint32_t code, uint32_t argument)
{
    uint32_t result;

    (void)code;
    if (M32(0xf1c) == 0) {
        result = (uint32_t)CHAIN_FN(0x1c, multip_chain_register_fn)(0x19, multip_track_callback);
        if (result == 0)
            M32(0xf20) = 0;
        return result;
    }
    result = argument;
    if (argument == *(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x119c))
        result = HOST_FN(0x8c, multip_draw_name_fn)(argument, 1, 0x6a,
            (const char *)(uintptr_t)(D + 0xf08), 0);
    return result;
}

int32_t FS98_STDCALL multip_track_show(uint32_t id)
{
    uint8_t *player = multip_find_player(id);

    HOST_FN(0x2a8, multip_strcpy_fn)((char *)(uintptr_t)(D + 0xf08), *(const char **)player);
    M32(0xf1c) = M32(0xa10) * 18;
    if (M32(0xf20) == 0) {
        if (CHAIN_FN(0x18, multip_chain_register_fn)(0x19, multip_track_callback) != 0)
            return -1;
        M32(0xf20) = 1;
    }
    return 0;
}

int32_t multip_track_clear(void)
{
    M32(0xf1c) = 0;
    HOST_FN(0x2a8, multip_strcpy_fn)((char *)(uintptr_t)(D + 0xf08),
                                 (const char *)(uintptr_t)(D + 0xf2c));
    return 0;
}

int32_t multip_selection_reset(void)
{
    M32(M_PLAYER_MODE) = 0;
    M32(M_ACTIVE_PLAYER_ID) = 0;
    *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 0;
    *(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11dc) = 1;
    return multip_track_clear() != 0 ? -1 : 0;
}

/* .text+0xa830 */
void *multip_selection_get(void)
{
    return M32(M_PLAYER_MODE) != 0 ? multip_find_player(M32(M_ACTIVE_PLAYER_ID)) : NULL;
}

/* .text+0xa680 */
uint32_t multip_selection_view_blocked(void)
{
    return *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x5dc) != 0 ||
        *(uint32_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x268) != 0 || M32(0xad4) != 0;
}

/* .text+0x9ea0 -- publish the host aircraft's model references and visibility
 * into the local network record. */
int32_t multip_player_refresh_local(void)
{
    uint8_t *global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    uint8_t *object = (uint8_t *)(uintptr_t)A32(global, 0x11e0);
    uint8_t *local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);

    A32(local, 0x20) = A32(object, 4);
    A32(local, 0x24) = A32(object, 8);
    A32(local, 0x28) = A32(global, 0x11e4);
    A32(local, 0x2c) = A32(object, 0x18);
    A32(local, 0x30) = A32(object, 0xc);
    A32(local, 0x34) = 1;
    A32(local, 0x38) = global[0x609];
    return 0;
}

/* .text+0xa6b0 -- the two view modes copy different state and flags. */
int32_t multip_selection_apply(void)
{
    uint8_t *player;
    void *state;
    uint8_t *global;

    if (M32(M_PLAYER_COUNT) == 0)
        return 0;
    player = multip_selection_get();
    if (player == NULL)
        return multip_selection_reset() != 0 ? -1 : 0;
    state = *(void **)(void *)(player + 0x28);
    global = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    if (M32(0xa58) == 1) {
        *(uint16_t *)(void *)(global + 0x5dc) = 1;
        *(uint16_t *)(void *)(global + 0xdc2) = 0;
        if (state != NULL && HOST_FN(0x21c, multip_copy_fn)(global + 0x560,
            (uint8_t *)state + 4, 36) != 0)
            return -1;
    } else if (M32(0xa58) == 0) {
        *(uint16_t *)(void *)(global + 0xdc2) = 1;
        if (state != NULL && HOST_FN(0x21c, multip_copy_fn)(global + 0x588,
            (uint8_t *)state + 4, 36) != 0)
            return -1;
        if (HOST_FN(0x21c, multip_copy_fn)(global + 0xd74, global + 0x560, 24) != 0)
            return -1;
    }
    return 0;
}
