/* Texture-name catalogue and host file services, .text+0x1d20..+0x2332. */
typedef uint32_t (FS98_STDCALL *multip_list_clear_fn)(void *);
typedef int32_t (FS98_STDCALL *multip_compare_fn)(const void *, const void *, uint32_t);
typedef void *(FS98_STDCALL *multip_g3d_next_fn)(const void *);
typedef int32_t (FS98_STDCALL *multip_current_path_fn)(char *);
typedef uint32_t (FS98_STDCALL *multip_string_resource_fn)(uint32_t, char *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_split_path_fn)(const char *, char *, char *, char *, int32_t *);
typedef int32_t (FS98_STDCALL *multip_make_path_fn)(char *, const char *, const char *, const char *);
typedef int32_t (FS98_STDCALL *multip_file_exists_fn)(const char *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_file_open_fn)(void **, const char *, uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *multip_file_create_fn)(void **, const char *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_file_size_fn)(void *, uint32_t *);
typedef int32_t (FS98_STDCALL *multip_file_resize_fn)(void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_file_read_fn)(void *, void *, uint32_t, uint32_t *);
typedef int32_t (FS98_STDCALL *multip_file_write_fn)(void *, const void *, uint32_t, uint32_t *);
typedef int32_t (FS98_STDCALL *multip_file_close_fn)(void *);

/* .text+0x2250/+0x2280 -- neither retail loop guards an underflow: all-space
 * strings and names without a dot can read before their input buffers. */
int32_t FS98_STDCALL multip_trim_spaces(char *text)
{
    uint32_t size = HOST_FN(0x2b8, multip_strlen_fn)(text);
    if (size != 0) {
        char *end = text + size - 1;
        while (*end == ' ') *end-- = 0;
    }
    return 0;
}
int32_t FS98_STDCALL multip_filename_extension(char *extension, const char *name)
{
    uint32_t size = HOST_FN(0x2b8, multip_strlen_fn)(name);
    const char *dot = name + size;
    while (*dot != '.') dot--;
    HOST_FN(0x2a8, multip_strcpy_fn)(extension, dot);
    return 0;
}

/* .text+0x2120 -- the resource and final strcat results are ignored. */
int32_t FS98_STDCALL multip_texture_path(char *path, const char *name)
{
    char extension[256], basename[256], directory[256], current[260];

    if (HOST_FN(0x130, multip_current_path_fn)(current) != 0) return -1;
    HOST_FN(0x2a0, multip_string_resource_fn)(0x9006, directory, sizeof(directory));
    if (multip_filename_extension(extension, name) != 0) return -1;
    if (multip_trim_spaces(extension) != 0) return -1;
    if (HOST_FN(0x188, multip_split_path_fn)(name, NULL, NULL, basename, NULL) != 0) return -1;
    if (multip_trim_spaces(basename) != 0) return -1;
    if (HOST_FN(0x184, multip_make_path_fn)(path, current, directory, basename) != 0) return -1;
    HOST_FN(0x2b0, multip_strcpy_fn)(path, extension);
    return 0;
}

/* .text+0x1e20/+0x1e80. Exists returns -1 on path failure, which its
 * catalogue caller consumes as a reason to skip that name. */
int32_t FS98_STDCALL multip_texture_list_contains(const void *name)
{
    uint32_t count = M32(0xa0);
    uint8_t *names = (uint8_t *)(uintptr_t)M32(0xb8);
    while (count != 0)
        if (HOST_FN(0x228, multip_compare_fn)(name, names + --count * 15, 14) == 0)
            return 1;
    return 0;
}
int32_t FS98_STDCALL multip_texture_exists(const char *name)
{
    char path[260];
    if (multip_texture_path(path, name) != 0) return -1;
    return HOST_FN(0x158, multip_file_exists_fn)(path, 0) == 0 ? 1 : 0;
}

/* .text+0x1d20 -- only BGL opcode 0x18 contributes a 14-byte name at +0x0a. */
int32_t FS98_STDCALL multip_texture_catalogue(const void *model, uint32_t size)
{
    const uint8_t *record = model;
    uint32_t end = ADDR(model) + size;
    while (record != NULL && ADDR(record) < end) {
        if (*(const uint16_t *)(const void *)record == 0x18) {
            const char *name = (const char *)record + 10;
            if (multip_texture_list_contains(name) == 0 && multip_texture_exists(name) == 0) {
                void *entry = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0xa0));
                if (HOST_FN(0x218, multip_memset_fn)(entry, 0, 15) != 0) return -1;
                if (HOST_FN(0x21c, multip_copy_fn)(entry, name, 14) != 0) return -1;
            }
        }
        record = (*(multip_g3d_next_fn *)(uintptr_t)(M32(0x2c) + 0x64))(record);
    }
    return 0;
}

/* .text+0x66d0 -- catalogue packet includes the 15-byte record terminators. */
int32_t FS98_STDCALL multip_message_1015(uint32_t id, const void *names, uint32_t count)
{
    uint32_t *packet, bytes = count * 15, payload_size = bytes + 4;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, payload_size + 8, (void **)&packet, 1) != 0) return -1;
    packet[0] = 0x1015; packet[1] = payload_size; packet[2] = count;
    if (HOST_FN(0x21c, multip_copy_fn)(packet + 3, names, bytes) != 0) return -1;
    if (multip_send(id, packet, payload_size + 8, 1) != 0) return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&packet) != 0 ? -1 : 0;
}

/* .text+0x1dc0. */
int32_t FS98_STDCALL multip_textures_request(uint32_t id, const void *model, uint32_t size)
{
    HOST_FN(0x304, multip_list_clear_fn)((void *)(uintptr_t)(D + 0xa0));
    if (multip_texture_catalogue(model, size) != 0) return -1;
    if (M32(0xa0) != 0 && multip_message_1015(id, (const void *)(uintptr_t)M32(0xb8), M32(0xa0)) != 0)
        return -1;
    return 0;
}

/* .text+0x1ed0 -- files/allocations remain open/owned on intermediate errors. */
int32_t FS98_STDCALL multip_texture_read(const char *name, void **data, uint32_t *size)
{
    char path[260];
    void *file;
    uint32_t length, read;
    if (multip_texture_path(path, name) != 0) return -1;
    if (HOST_FN(0x17c, multip_file_open_fn)(&file, path, 0, 0) != 0) return -1;
    if (HOST_FN(0x1a4, multip_file_size_fn)(file, &length) != 0) return -1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, length, data, 1) != 0) return -1;
    if (HOST_FN(0x19c, multip_file_read_fn)(file, *data, length, &read) != 0) return -1;
    if (HOST_FN(0x13c, multip_file_close_fn)(file) != 0) return -1;
    if (length != read) return -1;
    *size = length;
    return 0;
}

/* .text+0x2000 -- record the name only after the complete write and close. */
int32_t FS98_STDCALL multip_texture_write(const char *name, const void *data, uint32_t size)
{
    char path[260];
    void *file;
    uint32_t written;
    void *entry;
    if (multip_texture_path(path, name) != 0) return -1;
    if (HOST_FN(0x148, multip_file_create_fn)(&file, path, 0) != 0) return -1;
    if (HOST_FN(0x1b0, multip_file_write_fn)(file, data, size, &written) != 0) return -1;
    if (HOST_FN(0x1a8, multip_file_resize_fn)(file, size) != 0) return -1;
    if (HOST_FN(0x13c, multip_file_close_fn)(file) != 0) return -1;
    if (size != written) return -1;
    entry = HOST_FN(0x2f8, multip_list_add_fn)((void *)(uintptr_t)(D + 0xc0));
    return HOST_FN(0x21c, multip_copy_fn)(entry, name, 15) != 0 ? -1 : 0;
}

/* .text+0x22c0 -- names and list survive the first unlink/path failure. */
int32_t multip_textures_cleanup(void)
{
    uint32_t count = M32(0xc0);
    const char *names = (const char *)(uintptr_t)M32(0xd8);
    char path[260];
    while (count != 0) {
        if (multip_texture_path(path, names + --count * 15) != 0) return -1;
        if (HOST_FN(0x14c, multip_file_exists_fn)(path, 0) != 0) return -1;
    }
    HOST_FN(0x304, multip_list_clear_fn)((void *)(uintptr_t)(D + 0xc0));
    return 0;
}

/* .text+0x6780 -- successful failure-notification enqueue sets pending. */
int32_t FS98_STDCALL multip_message_1016(uint32_t id)
{
    if (multip_message_empty(id, 0x1016) != 0) return -1;
    M32(0x984) = 1;
    return 0;
}
