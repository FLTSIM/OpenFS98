/* Player-array ownership and notifications. */
#include "multip_nametag.inc"

typedef uint32_t (FS98_STDCALL *multip_chain_dispatch_fn)(uint32_t, uint32_t, void *);
typedef void (FS98_STDCALL *multip_list_remove_fn)(void *, void *);

/* .text+0x8930 -- find checks the old local pointer before the moved array.
 * Preserve that stale-pointer behavior if the allocator moved the storage. */
void *multip_player_append(void)
{
    uint32_t uiLocalId = 0;
    void *pRecord;

    if (M32(M_LOCAL_PLAYER) != 0)
        uiLocalId = A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4);
    pRecord = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0x660));
    if (M32(M_LOCAL_PLAYER) != 0)
        M32(M_LOCAL_PLAYER) = ADDR(multip_find_player(uiLocalId));
    return pRecord;
}

/* .text+0x8980 */
int32_t FS98_STDCALL multip_player_mark_unknown(void *pPlayer)
{
    A32(pPlayer, 8) = 4;
    multip_player_reset_collision(pPlayer);
    return 0;
}

static void multip_player_publish_list(void)
{
    M32(0xef8) = M32(M_PLAYER_COUNT);
    M32(0xefc) = M32(M_PLAYERS);
}

/* .text+0x82c0 -- append before allocations, retaining partial records on
 * failure. Duplicate IDs do not change the existing record. */
int32_t FS98_STDCALL multip_player_add_remote(const void *pName, uint32_t uiId)
{
    uint8_t *pbyPlayer;
    char acTitle[0x104];
    uint8_t *pbyGlobal;

    if (multip_find_player(uiId) != NULL)
        return 0;
    pbyPlayer = multip_player_append();
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 21, (void **)pbyPlayer, 1) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)pbyPlayer,
        *(const char *const *)((const uint8_t *)pName + 8));
    A32(pbyPlayer, 4) = uiId;
    AC_FN(0x30, multip_aircraft_title_fn)((void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11e0),
        1, acTitle, sizeof(acTitle));
    if (multip_aircraft_select(acTitle, pbyPlayer + 0x10) != 0)
        return -1;
    pbyGlobal = (uint8_t *)(uintptr_t)M32(IMPORT_GLOBAL);
    if (HOST_FN(0x21c, multip_copy_fn)((void *)(uintptr_t)(A32(pbyPlayer, 0x28) + 4),
        pbyGlobal + 0x560, 36) != 0)
        return -1;
    if (multip_player_mark_unknown(pbyPlayer) != 0)
        return -1;
    A32(pbyPlayer, 8) = 4;
    A32(pbyPlayer, 0x4c) = 0;
    A32(pbyPlayer, 0x44) = 0;
    A32(pbyPlayer, 0x3c) = 0;
    A32(pbyPlayer, 0x40) = 0;
    multip_player_publish_list();
    A32(pbyPlayer, 0xd4) = A32(pbyPlayer, 0);
    if (HOST_FN(0x20c, multip_alloc_fn)(1, sizeof(s_abyNametag),
        (void **)(void *)(pbyPlayer + 0xd0), 0) != 0)
        return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(*(void **)(void *)(pbyPlayer + 0xd0),
        s_abyNametag, sizeof(s_abyNametag)) != 0)
        return -1;
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 3, (void *)(uintptr_t)(D + 0xef8));
    return 0;
}

/* .text+0xa8a0 -- removing the local list entry leaves its pointer intact. */
int32_t multip_player_remove_local(void)
{
    uint32_t uiId = A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4);
    void *pPlayer = multip_find_player(uiId);

    if (pPlayer != NULL)
        HOST_FN(0x300, multip_list_remove_fn)((void *)(uintptr_t)(D + 0x660), pPlayer);
    return 0;
}

/* .text+0x84b0 */
int32_t FS98_STDCALL multip_player_remove(uint32_t uiId)
{
    uint8_t *pbyPlayer = multip_find_player(uiId);
    uint32_t uiLocalId = 0;
    uint32_t uiType;

    if (pbyPlayer == NULL)
        return -1;
    if (A32(pbyPlayer, 0xc) != 0)
        return multip_player_remove_local() != 0 ? -1 : 0;
    if (M32(M_LOCAL_PLAYER) != 0)
        uiLocalId = A32((void *)(uintptr_t)M32(M_LOCAL_PLAYER), 4);
    if (multip_aircraft_release(pbyPlayer + 0x10) != 0)
        return -1;
    if (A32(pbyPlayer, 0) != 0 && HOST_FN(0x214, multip_free_fn)((void **)pbyPlayer) != 0)
        return -1;
    if (HOST_FN(0x214, multip_free_fn)((void **)(void *)(pbyPlayer + 0xd0)) != 0)
        return -1;
    A32(pbyPlayer, 0xd4) = 0;
    if (M32(0xaa8) != 0) {
        uiType = A32(pbyPlayer, 8);
        if (uiType == 1) M32(0x9a0)--;
        else if (uiType == 2) M32(0x9a4)--;
        if (multip_session_set_capacity((const uint32_t *)(uintptr_t)(D + 0x998)) != 0)
            return -1;
    }
    if (M32(M_PLAYER_MODE) != 0 && A32(pbyPlayer, 4) == M32(M_ACTIVE_PLAYER_ID)) {
        if (multip_selection_reset() != 0)
            return -1;
        *(uint16_t *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x11b0) = 0;
    }
    HOST_FN(0x300, multip_list_remove_fn)((void *)(uintptr_t)(D + 0x660), pbyPlayer);
    multip_player_publish_list();
    if (M32(M_LOCAL_PLAYER) != 0)
        M32(M_LOCAL_PLAYER) = ADDR(multip_find_player(uiLocalId));
    CHAIN_FN(0x20, multip_chain_dispatch_fn)(0x1d, 3, (void *)(uintptr_t)(D + 0xef8));
    return 0;
}

/* .text+0x8f00 */
int32_t multip_player_remove_all(void)
{
    uint32_t uiCount = M32(M_PLAYER_COUNT);

    while (uiCount-- != 0)
        if (multip_player_remove(A32((void *)(uintptr_t)M32(M_PLAYERS), 4)) != 0)
            return -1;
    return 0;
}
