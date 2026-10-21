/* Aircraft ownership, .text+0x8a20/+0x8aa0. ACONTAIN returns true on success;
 * the host memory services return zero on success. */
typedef int32_t (FS98_STDCALL *multip_aircraft_select_fn)(void *, const char *);
typedef int32_t (FS98_STDCALL *multip_aircraft_model_fn)(void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_aircraft_release_fn)(void *);

#define AC_FN(offset, type) (*(type *)(uintptr_t)(M32(0x14) + (offset)))
#define A32(p, o) (*(uint32_t *)((uint8_t *)(p) + (o)))

int32_t FS98_STDCALL multip_aircraft_select(const char *title, void *aircraft)
{
    uint8_t *header;
    uint8_t *model;

    if (AC_FN(0x20, multip_aircraft_select_fn)(aircraft, title) == 0)
        return -1;
    if (AC_FN(0x34, multip_aircraft_model_fn)(aircraft, 1) == 0)
        return -1;
    model = (uint8_t *)(uintptr_t)A32(aircraft, 4);
    header = (uint8_t *)(uintptr_t)A32(aircraft, 0);
    A32(model, 0x68) = 2;
    if (A32(header, 0x18) != 0) {
        A32(aircraft, 0x18) = ADDR(model);
        A32(aircraft, 0x1c) = A32(model, 0);
        A32(aircraft, 0x10) = A32(header, 4);
        A32(aircraft, 0x14) = A32(header, 8);
        *(uint16_t *)(void *)(model + 0x60) = 1;
    }
    A32(aircraft, 0xc) = 1;
    A32(aircraft, 0x20) = A32(header, 0xc);
    return 0;
}

int32_t FS98_STDCALL multip_aircraft_release(void *aircraft)
{
    uint32_t offset;

    if (A32(aircraft, 0xc) != 0) {
        if (AC_FN(0x38, multip_aircraft_release_fn)(aircraft) == 0)
            return -1;
        if (AC_FN(0x24, multip_aircraft_release_fn)(aircraft) == 0)
            return -1;
    } else {
        if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)aircraft + 0x10)) != 0)
            return -1;
        if (HOST_FN(0x214, multip_free_fn)((void **)((uint8_t *)aircraft + 0x18)) != 0)
            return -1;
    }
    for (offset = 0x10; offset <= 0x20; offset += 4)
        A32(aircraft, offset) = 0;
    A32(aircraft, 0xc) = 0;
    return 0;
}

typedef uint32_t (FS98_STDCALL *multip_strlen_fn)(const char *);
typedef int32_t (FS98_STDCALL *multip_strcpy_fn)(char *, const char *);

/* .text+0x8b30 -- received model replacement. Each successful stage stays
 * visible when a later allocation/copy fails. The new 0x24-byte header is
 * zero-filled; retail does not populate it from the source header. */
int32_t FS98_STDCALL multip_aircraft_copy(void *aircraft, const void *source)
{
    void **buffer_out;
    uint32_t size;
    uint8_t *model;
    const uint8_t *source_model;
    const char *title;

    if (A32(aircraft, 0xc) != 0 && multip_aircraft_release(aircraft) != 0)
        return -1;
    A32(aircraft, 0xc) = 0;
    buffer_out = (void **)((uint8_t *)aircraft + 0x10);
    if (HOST_FN(0x210, multip_realloc_fn)(1, A32(source, 0x14), buffer_out) != 0)
        return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(*buffer_out,
        (void *)(uintptr_t)A32(source, 0x10), A32(source, 0x14)) != 0)
        return -1;
    A32(aircraft, 0x14) = A32(source, 0x14);
    buffer_out = (void **)((uint8_t *)aircraft + 0x18);
    if (HOST_FN(0x210, multip_realloc_fn)(1, A32(source, 0x1c), buffer_out) != 0)
        return -1;
    if (HOST_FN(0x21c, multip_copy_fn)(*buffer_out,
        (void *)(uintptr_t)A32(source, 0x18), A32(source, 0x1c)) != 0)
        return -1;
    A32(aircraft, 0x1c) = A32(source, 0x1c);
    A32(aircraft, 0x20) = A32(source, 0x20);
    A32(aircraft, 0x28) = A32(source, 0x28);
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 0x24, (void **)aircraft, 1) != 0)
        return -1;
    model = *(uint8_t **)((uint8_t *)aircraft + 0x18);
    source_model = *(uint8_t *const *)((const uint8_t *)source + 0x18);
    title = *(const char *const *)(void *)(source_model + 0x64);
    size = HOST_FN(0x2b8, multip_strlen_fn)(title) + 1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, (void **)(void *)(model + 0x64), 0) != 0)
        return -1;
    HOST_FN(0x2a8, multip_strcpy_fn)(*(char **)(void *)(model + 0x64), title);
    A32(model, 0x68) = 2;
    return 0;
}
