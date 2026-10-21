/* Provider catalogue used by configuration dialog 0x110a. */
typedef int32_t (FS98_STDCALL *multip_provider_callback_fn)(const GUID *,
    const char *, uint32_t, uint32_t, void *);
typedef int32_t (FS98_STDCALL *multip_provider_enumerate_fn)(
    multip_provider_callback_fn, void *);

/* .text+0x42a0 -- retail never advances the record pointer. Only the first
 * name is freed; the remaining names leak when the list count is cleared. */
int32_t multip_provider_clear(void)
{
    uint32_t count = M32(0x348);
    void **record = (void **)(uintptr_t)M32(0x360);

    while (count != 0) {
        if (*record != NULL && HOST_FN(0x214, multip_free_fn)(record) != 0)
            return -1;
        count--;
    }
    HOST_FN(0x304, multip_list_clear_fn)((void *)(uintptr_t)(D + 0x348));
    return 0;
}

/* .text+0x4210 -- append before allocating the name. The new record remains
 * on allocation failure; string/GUID copy errors do not stop enumeration. */
static int32_t FS98_STDCALL multip_provider_callback(const GUID *provider,
    const char *description, uint32_t major_version, uint32_t minor_version,
    void *context)
{
    uint8_t *record = HOST_FN(0x2f8, multip_list_add_fn)(
        (void *)(uintptr_t)(D + 0x348));
    uint32_t size = HOST_FN(0x2b8, multip_strlen_fn)(description) + 1;

    (void)context;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, (void **)record, 1) != 0)
        return 0;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)record, description);
    HOST_FN(0x21c, multip_copy_fn)(record + 4, provider, 16);
    A32(record, 0x14) = major_version;
    A32(record, 0x18) = minor_version;
    return 1;
}

/* .text+0x41f0 -- a cleanup failure is consumed as success, while any
 * nonzero DirectPlayEnumerateA result becomes -1. */
int32_t multip_provider_enumerate(void)
{
    if (multip_provider_clear() != 0)
        return 0;
    return ((multip_provider_enumerate_fn)(uintptr_t)M32(M_DIRECTPLAY_ENUMERATE))(
        multip_provider_callback, NULL) != 0 ? -1 : 0;
}
