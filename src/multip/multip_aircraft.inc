/* Aircraft ownership, .text+0x8a20/+0x8aa0. ACONTAIN returns true on success;
 * the host memory services return zero on success. */
typedef int32_t (FS98_STDCALL *multip_aircraft_select_fn)(void *, const char *);
typedef int32_t (FS98_STDCALL *multip_aircraft_model_fn)(void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_aircraft_release_fn)(void *);

#define AC_FN(offset, type) (*(type *)(uintptr_t)(M32(0x14) + (offset)))
#define A32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))

int32_t FS98_STDCALL multip_aircraft_select(const char *pcTitle, void *pAircraft)
{
    uint8_t *pbyHeader;
    uint8_t *pbyModel;

    if (AC_FN(0x20, multip_aircraft_select_fn)(pAircraft, pcTitle) == 0)
        return -1;
    if (AC_FN(0x34, multip_aircraft_model_fn)(pAircraft, 1) == 0)
        return -1;
    pbyModel = (uint8_t *)(uintptr_t)A32(pAircraft, 4);
    pbyHeader = (uint8_t *)(uintptr_t)A32(pAircraft, 0);
    A32(pbyModel, 0x68) = 2;
    if (A32(pbyHeader, 0x18) != 0) {
        A32(pAircraft, 0x18) = ADDR(pbyModel);
        A32(pAircraft, 0x1c) = A32(pbyModel, 0);
        A32(pAircraft, 0x10) = A32(pbyHeader, 4);
        A32(pAircraft, 0x14) = A32(pbyHeader, 8);
        *(uint16_t *)(void *)(pbyModel + 0x60) = 1;
    }
    A32(pAircraft, 0xc) = 1;
    A32(pAircraft, 0x20) = A32(pbyHeader, 0xc);
    return 0;
}

int32_t FS98_STDCALL multip_aircraft_release(void *pAircraft)
{
    uint32_t uiOffset;

    if (A32(pAircraft, 0xc) != 0) {
        if (AC_FN(0x38, multip_aircraft_release_fn)(pAircraft) == 0)
            return -1;
        if (AC_FN(0x24, multip_aircraft_release_fn)(pAircraft) == 0)
            return -1;
    } else {
        if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)pAircraft + 0x10)) != 0)
            return -1;
        if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)pAircraft + 0x18)) != 0)
            return -1;
    }
    for (uiOffset = 0x10; uiOffset <= 0x20; uiOffset += 4)
        A32(pAircraft, uiOffset) = 0;
    A32(pAircraft, 0xc) = 0;
    return 0;
}

typedef uint32_t (FS98_STDCALL *multip_strlen_fn)(const char *);
typedef int32_t (FS98_STDCALL *multip_strcpy_fn)(char *, const char *);

/* .text+0x8b30 -- received model replacement. Each successful stage stays
 * visible when a later allocation/copy fails. The new 0x24-byte header is
 * zero-filled; retail does not populate it from the source header. */
int32_t FS98_STDCALL multip_aircraft_copy(void *pAircraft, const void *pSource)
{
    void **ppBuffer;
    uint32_t uiSize;
    uint8_t *pbyModel;
    const uint8_t *pbySourceModel;
    const char *pcTitle;

    if (A32(pAircraft, 0xc) != 0 && multip_aircraft_release(pAircraft) != 0)
        return -1;
    A32(pAircraft, 0xc) = 0;
    ppBuffer = (void **)((uint8_t *)pAircraft + 0x10);
    if (HOST_FN(0x210, multip_realloc_fn)(1, A32(pSource, 0x14), ppBuffer) != 0)
        return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(*ppBuffer,
        (void *)(uintptr_t)A32(pSource, 0x10), A32(pSource, 0x14)) != 0)
        return -1;
    A32(pAircraft, 0x14) = A32(pSource, 0x14);
    ppBuffer = (void **)((uint8_t *)pAircraft + 0x18);
    if (HOST_FN(0x210, multip_realloc_fn)(1, A32(pSource, 0x1c), ppBuffer) != 0)
        return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(*ppBuffer,
        (void *)(uintptr_t)A32(pSource, 0x18), A32(pSource, 0x1c)) != 0)
        return -1;
    A32(pAircraft, 0x1c) = A32(pSource, 0x1c);
    A32(pAircraft, 0x20) = A32(pSource, 0x20);
    A32(pAircraft, 0x28) = A32(pSource, 0x28);
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 0x24, (void **)pAircraft, 1) != 0)
        return -1;
    pbyModel = *(uint8_t **)((uint8_t *)pAircraft + 0x18);
    pbySourceModel = *(uint8_t *const *)((const uint8_t *)pSource + 0x18);
    pcTitle = *(const char *const *)(void *)(pbySourceModel + 0x64);
    uiSize = HOST_FN(0x2b8, multip_strlen_fn)(pcTitle) + 1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, uiSize, (void **)(void *)(pbyModel + 0x64), 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)(void *)(pbyModel + 0x64), pcTitle);
    A32(pbyModel, 0x68) = 2;
    return 0;
}
