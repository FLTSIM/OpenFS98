/* Session browser records, .text+0xa2a0/+0xa360/+0xa4d0. */
int32_t FS98_STDCALL multip_session_record_init(const void *pDescription, void *pRecord)
{
    const char *pcName = *(const char *const *)((const uint8_t *)pDescription + 0x30);
    uint32_t uiSize = HOST_FN(0x2b8, multip_strlen_fn)(pcName) + 1;

    if (HOST_FN(0x20c, multip_alloc_fn)(1, uiSize, (void **)pRecord, 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)pRecord, pcName);
    if (HOST_FN(0x21c, multip_copy_fn)((uint8_t *)pRecord + 4,
        (const uint8_t *)pDescription + 8, 16) != 0)
        return -1;
    A32(pRecord, 0x14) = A32(pDescription, 4);
    A32(pRecord, 0x18) = 0;
    A32(pRecord, 0x34) = 0;
    A32(pRecord, 0x30) = 0;
    if (multip_session_get_capacity((uint32_t *)((uint8_t *)pRecord + 0x20),
                                   pDescription) != 0)
        return -1;
    A32(pRecord, 0x38) = 0;
    A32(pRecord, 0x3c) = 0;
    A32(pRecord, 0x40) = 32;
    A32(pRecord, 0x44) = 16;
    A32(pRecord, 0x48) = 0;
    A32(pRecord, 0x4c) = 0;
    A32(pRecord, 0x50) = 0;
    return 0;
}

int32_t FS98_STDCALL multip_player_name_record_init(uint32_t uiId,
    uint32_t uiType, const void *pName, void *pRecord)
{
    const char *pcName = *(const char *const *)((const uint8_t *)pName + 8);
    uint32_t uiSize;

    (void)uiType;
    A32(pRecord, 0) = uiId;
    uiSize = HOST_FN(0x2b8, multip_strlen_fn)(pcName) + 1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, uiSize,
        (void **)((uint8_t *)pRecord + 12), 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)((uint8_t *)pRecord + 12), pcName);
    return 0;
}

int32_t FS98_STDCALL multip_player_name_record_copy(void *pDest, const void *pSource)
{
    const char *pcName;
    uint32_t uiSize;

    if (HOST_FN(0x21c, multip_copy_fn)(pDest, pSource, 16) != 0)
        return -1;
    pcName = *(const char *const *)((const uint8_t *)pSource + 12);
    uiSize = HOST_FN(0x2b8, multip_strlen_fn)(pcName) + 1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, uiSize,
        (void **)((uint8_t *)pDest + 12), 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)((uint8_t *)pDest + 12), pcName);
    return 0;
}

/* .text+0xa540 -- host-list visitor, context unused. */
int32_t FS98_STDCALL multip_player_name_record_free(void *pRecord, void *pContext)
{
    (void)pContext;
    return HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)pRecord + 12)) != 0 ? -1 : 0;
}

typedef void *(FS98_STDCALL *multip_list_add_fn)(void *);
typedef int32_t (FS98_STDCALL *multip_list_visitor_fn)(void *, void *);
typedef int32_t (FS98_STDCALL *multip_list_walk_fn)(void *, void *, multip_list_visitor_fn);
typedef int32_t (FS98_STDCALL *multip_dp_enum_players_fn)(void *, const GUID *,
    multip_dp_enum_group_callback, void *, uint32_t);

/* .text+0xa470 -- walker failures are ignored. Freeing a temporary pointer
 * leaves the original header intact when the final free fails. */
int32_t FS98_STDCALL multip_player_names_clear(void *pList)
{
    void *pData;

    HOST_FN(0x308, multip_list_walk_fn)(pList, NULL, multip_player_name_record_free);
    pData = *(void **)((uint8_t *)pList + 0x18);
    if (HOST_FN(0x214, multip_free_fn)(&pData) != 0)
        return -1;
    A32(pList, 0) = 0;
    A32(pList, 4) = 0;
    A32(pList, 0x10) = 0;
    A32(pList, 0x14) = 0;
    A32(pList, 0x18) = 0;
    A32(pList, 8) = 32;
    A32(pList, 0xc) = 16;
    return 0;
}

/* .text+0xa420 -- append before name validation, retaining an invalid last
 * record when the enumeration stops. */
static int32_t FS98_STDCALL multip_player_names_callback(uint32_t uiId,
    uint32_t uiType, const void *pName, uint32_t uiFlags, void *pContext)
{
    uint8_t *pbyRecord;

    (void)uiFlags;
    pbyRecord = HOST_FN(0x2f8, multip_list_add_fn)((uint8_t *)pContext + 0x38);
    if (multip_player_name_record_init(uiId, uiType, pName, pbyRecord) != 0)
        return 0;
    return multip_player_decode_name(pName, (uint32_t *)(void *)(pbyRecord + 4),
                                    (uint32_t *)(void *)(pbyRecord + 8)) == 0;
}

/* .text+0xa3c0 -- EnumPlayers' HRESULT is consumed; partial lists publish. */
int32_t FS98_STDCALL multip_session_enumerate_players(void *pRecord,
    const GUID *pInstance, uint32_t uiFlags)
{
    if (pRecord == NULL)
        return -1;
    if (multip_player_names_clear((uint8_t *)pRecord + 0x38) != 0)
        return -1;
    DP_FN(0x30, multip_dp_enum_players_fn)((void *)(uintptr_t)M32(M_DIRECTPLAY),
        pInstance, multip_player_names_callback, pRecord, uiFlags);
    A32(pRecord, 0x34) = A32(pRecord, 0x50);
    A32(pRecord, 0x30) = A32(pRecord, 0x38);
    A32(pRecord, 0x18) = 1;
    return 0;
}
