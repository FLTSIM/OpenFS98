/* Received aircraft cache files, .text+0x410..+0xab1. Paths and partial
 * allocations/handles intentionally survive the same retail error edges. */
typedef int32_t (FS98_STDCALL *multip_directory_create_fn)(const char *);
static const uint8_t s_cache_field_offsets[4] = {0x20, 0x28, 0x14, 0x1c};

int32_t FS98_STDCALL multip_model_cache_prepare(const char *title)
{
    char root[264], path[264], folder[264];
    HOST_FN(0x2a0, multip_string_resource_fn)(0x9010, root, 260);
    if (HOST_FN(0x184, multip_make_path_fn)(path, root, title, NULL) != 0) return 0;
    if (HOST_FN(0x11c, multip_directory_create_fn)(path) != 0) return -1;
    HOST_FN(0x2a0, multip_string_resource_fn)(0x9006, folder, 260);
    if (HOST_FN(0x184, multip_make_path_fn)(path, NULL, path, folder) != 0) return -1;
    return HOST_FN(0x11c, multip_directory_create_fn)(path) != 0 ? -1 : 0;
}
/* .text+0x750 -- four scalar fields followed by header bytes and model bytes.
 * A path-join error returns success without attempting a file. */
int32_t FS98_STDCALL multip_model_cache_write(const void *aircraft, const char *title)
{
    char root[264], path[264];
    void *file;
    uint32_t written, index;
    HOST_FN(0x2a0, multip_string_resource_fn)(0x9010, root, 260);
    if (HOST_FN(0x184, multip_make_path_fn)(path, root, title, root) != 0) return 0;
    if (multip_model_cache_prepare(title) != 0) return -1;
    if (HOST_FN(0x148, multip_file_create_fn)(&file, path, 0) != 0) return -1;
    for (index = 0; index < 4; index++) {
        if (HOST_FN(0x1b0, multip_file_write_fn)(file,
            (const uint8_t *)aircraft + s_cache_field_offsets[index], 4, &written) != 0) return -1;
        if (written != 4) return -1;
    }
    if (HOST_FN(0x1b0, multip_file_write_fn)(file, (const void *)(uintptr_t)A32(aircraft, 0x10),
        A32(aircraft, 0x14), &written) != 0) return -1;
    if (written != A32(aircraft, 0x14)) return -1;
    if (HOST_FN(0x1b0, multip_file_write_fn)(file, (const void *)(uintptr_t)A32(aircraft, 0x18),
        A32(aircraft, 0x1c), &written) != 0) return -1;
    if (written != A32(aircraft, 0x1c)) return -1;
    return HOST_FN(0x13c, multip_file_close_fn)(file) != 0 ? -1 : 0;
}
/* .text+0x410 -- reallocate both byte buffers, then install a separately
 * allocated path at model+0x64 after closing the cache file. */
int32_t FS98_STDCALL multip_model_cache_read(void *aircraft, const char *title)
{
    char root[264], path[264], folder[264];
    void *file;
    uint32_t read, index;
    HOST_FN(0x2a0, multip_string_resource_fn)(0x9010, root, 260);
    if (HOST_FN(0x184, multip_make_path_fn)(path, root, title, root) != 0) return 0;
    if (HOST_FN(0x17c, multip_file_open_fn)(&file, path, 0, 0) != 0) return -1;
    for (index = 0; index < 4; index++) {
        if (HOST_FN(0x19c, multip_file_read_fn)(file,
            (uint8_t *)aircraft + s_cache_field_offsets[index], 4, &read) != 0) return -1;
        if (read != 4) return -1;
    }
    if (HOST_FN(0x210, multip_realloc_fn)(1, A32(aircraft, 0x14),
        (void **)((uint8_t *)aircraft + 0x10)) != 0) return -1;
    if (HOST_FN(0x19c, multip_file_read_fn)(file, (void *)(uintptr_t)A32(aircraft, 0x10),
        A32(aircraft, 0x14), &read) != 0) return -1;
    if (read != A32(aircraft, 0x14)) return -1;
    if (HOST_FN(0x210, multip_realloc_fn)(1, A32(aircraft, 0x1c),
        (void **)((uint8_t *)aircraft + 0x18)) != 0) return -1;
    if (HOST_FN(0x19c, multip_file_read_fn)(file, (void *)(uintptr_t)A32(aircraft, 0x18),
        A32(aircraft, 0x1c), &read) != 0) return -1;
    if (read != A32(aircraft, 0x1c)) return -1;
    if (HOST_FN(0x13c, multip_file_close_fn)(file) != 0) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, 261,
        (void **)(uintptr_t)(A32(aircraft, 0x18) + 0x64), 0) != 0) return -1;
    HOST_FN(0x2a0, multip_string_resource_fn)(0x9006, folder, 260);
    return HOST_FN(0x184, multip_make_path_fn)((char *)(uintptr_t)A32((void *)(uintptr_t)A32(aircraft, 0x18), 0x64),
        NULL, path, folder) != 0 ? -1 : 0;
}

/* .text+0x2c0 -- replace through a temporary cache-owned aircraft while
 * retaining the old model's position. Failures keep every completed stage. */
int32_t FS98_STDCALL multip_model_cache_select(void *aircraft, const char *title)
{
    uint8_t temporary[44], position[36], saved[36];
    if (HOST_FN(0x218, multip_memset_fn)(temporary, 0, sizeof(temporary)) != 0) return -1;
    if (HOST_FN(0x218, multip_memset_fn)(position, 0, sizeof(position)) != 0) return -1;
    A32(temporary, 0) = ADDR(position);
    if (HOST_FN(0x21c, multip_copy_fn)(saved,
        (const uint8_t *)(uintptr_t)A32(aircraft, 0x18) + 4, 36) != 0) return -1;
    if (multip_aircraft_release(aircraft) != 0) return -1;
    if (multip_model_cache_read(temporary, title) != 0) return -1;
    if (multip_aircraft_copy(aircraft, temporary) != 0) return -1;
    if (HOST_FN(0x21c, multip_copy_fn)((uint8_t *)(uintptr_t)A32(aircraft, 0x18) + 4,
        saved, 36) != 0) return -1;
    if (HOST_FN(0x214, multip_free_fn)((void **)(uintptr_t)(A32(temporary, 0x18) + 0x64)) != 0) return -1;
    if (HOST_FN(0x214, multip_free_fn)((void **)(temporary + 0x18)) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)(temporary + 0x10)) != 0 ? -1 : 0;
}
