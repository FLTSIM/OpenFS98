/* Texture owner fixture: real stateless G3D instruction stepping, fixed
 * catalogue banks and a memory filesystem. No game files are read or written. */
struct texture_file {
    char path[260];
    uint32_t exists, size;
    uint8_t bytes[128];
};
struct texture_results {
    struct weather_fixture weather;
    char names[2][16][15], output[260], input[64];
    struct texture_file files[8];
    uint32_t handles[16][2], handle_count, returns[12], outcomes[12], state[12][4];
};
static struct texture_results s_texture;
static const struct multip_module *s_texture_module;
static uint8_t *s_texture_guard, *s_texture_input;
static void *s_texture_next;
static uint32_t texture_fail(void) { return s_weather.calls == s_weather.fail_call; }
static uint32_t *texture_log(uint32_t tag) { return weather_log(tag); }
static uint32_t texture_list_index(const void *list)
{ return (const uint8_t *)list == s_texture_module->data + 0xa0 ? 0 : 1; }
static void *STDCALL texture_list_add(void *list)
{
    uint32_t *entry = texture_log(16), which = texture_list_index(list), count = get_u32(list, 0);
    if (count >= 16) exit(2);
    entry[1] = which; entry[2] = count;
    put_u32(list, 0, count + 1); memset(s_texture.names[which][count], 0, 15);
    return s_texture.names[which][count];
}
static uint32_t STDCALL texture_list_clear(void *list)
{
    uint32_t *entry = texture_log(17); entry[1] = texture_list_index(list); entry[2] = get_u32(list, 0);
    put_u32(list, 0, 0); return (uint32_t)(uintptr_t)list;
}
static int32_t STDCALL texture_compare(const void *a, const void *b, uint32_t size)
{
    uint32_t *entry = texture_log(18); entry[1] = size; entry[2] = draw_hash(a, size); entry[3] = draw_hash(b, size);
    return memcmp(a, b, size);
}
static int32_t STDCALL texture_current(char *out)
{
    texture_log(19); strcpy(out, "C:\\FS98"); return texture_fail() ? -1 : 0;
}
static uint32_t STDCALL texture_resource(uint32_t id, char *out, uint32_t size)
{
    uint32_t *entry = texture_log(20); entry[1] = id; entry[2] = size;
    strcpy(out, "Texture"); return texture_fail() ? 0 : 7;
}
static int32_t STDCALL texture_split(const char *path, char *drive, char *directory, char *name, int32_t *extension)
{
    uint32_t *entry = texture_log(21);
    const char *base = strrchr(path, '\\'), *dot;
    entry[1] = profile_hash(path); entry[2] = drive != NULL; entry[3] = directory != NULL;
    entry[4] = name != NULL; entry[5] = extension != NULL;
    base = base ? base + 1 : path; dot = strrchr(base, '.');
    if (drive) drive[0] = 0;
    if (directory) directory[0] = 0;
    if (name) { size_t length = dot ? (size_t)(dot - base) : strlen(base); memcpy(name, base, length); name[length] = 0; }
    if (extension) *extension = 0;
    return texture_fail() ? -1 : 0;
}
static int32_t STDCALL texture_join(char *out, const char *drive, const char *directory, const char *name)
{
    uint32_t *entry = texture_log(22); entry[1] = profile_hash(drive); entry[2] = profile_hash(directory); entry[3] = profile_hash(name);
    snprintf(out, 260, "%s\\%s\\%s", drive, directory, name); return texture_fail() ? -1 : 0;
}
static int32_t STDCALL texture_concat(char *out, const char *text)
{
    uint32_t *entry = texture_log(23); entry[1] = profile_hash(text); strcat(out, text); return -1;
}
static uint32_t texture_find_file(const char *path)
{
    uint32_t index;
    for (index = 0; index < 8; index++) if (_stricmp(path, s_texture.files[index].path) == 0) return index;
    return 8;
}
static int32_t STDCALL texture_exists(const char *path, uint32_t extension)
{
    uint32_t *entry = texture_log(24), index = texture_find_file(path);
    entry[1] = profile_hash(path); entry[2] = extension;
    return texture_fail() || index == 8 || !s_texture.files[index].exists ? -1 : 0;
}
static int32_t STDCALL texture_unlink(const char *path, uint32_t extension)
{
    uint32_t *entry = texture_log(25), index = texture_find_file(path);
    entry[1] = profile_hash(path); entry[2] = extension;
    if (texture_fail() || index == 8 || !s_texture.files[index].exists) return -1;
    s_texture.files[index].exists = 0; return 0;
}
static int32_t texture_open_impl(uint32_t tag, void **out, const char *path, uint32_t extension, uint32_t mode)
{
    uint32_t *entry = texture_log(tag), index = texture_find_file(path), handle = s_texture.handle_count;
    entry[1] = profile_hash(path); entry[2] = extension; entry[3] = mode;
    if (texture_fail()) { *out = NULL; return -1; }
    if (tag == 26 && (index == 8 || !s_texture.files[index].exists)) { *out = NULL; return -1; }
    if (index == 8) {
        for (index = 0; index < 8; index++) if (!s_texture.files[index].path[0]) break;
        if (index == 8) exit(2);
        strcpy(s_texture.files[index].path, path);
    }
    if (handle >= 16) exit(2);
    if (tag == 27) { s_texture.files[index].exists = 1; s_texture.files[index].size = 0; }
    s_texture.handles[handle][0] = index; s_texture.handles[handle][1] = 1;
    s_texture.handle_count++; *out = (void *)(uintptr_t)(0x7000 + handle); return 0;
}
static int32_t STDCALL texture_open(void **out, const char *path, uint32_t extension, uint32_t mode)
{ return texture_open_impl(26, out, path, extension, mode); }
static int32_t STDCALL texture_create(void **out, const char *path, uint32_t extension)
{ return texture_open_impl(27, out, path, extension, 0); }
static struct texture_file *texture_handle(void *file)
{
    uint32_t handle = (uint32_t)(uintptr_t)file - 0x7000;
    if (handle >= s_texture.handle_count || !s_texture.handles[handle][1]) exit(2);
    return &s_texture.files[s_texture.handles[handle][0]];
}
static int32_t STDCALL texture_file_size(void *file, uint32_t *size)
{
    uint32_t *entry = texture_log(28); entry[1] = (uint32_t)(uintptr_t)file;
    *size = texture_handle(file)->size; return texture_fail() ? -1 : 0;
}
static int32_t STDCALL texture_read(void *file, void *data, uint32_t size, uint32_t *read)
{
    struct texture_file *source = texture_handle(file);
    uint32_t *entry = texture_log(29); entry[1] = (uint32_t)(uintptr_t)file; entry[2] = size;
    *read = size && s_weather.number % 7 == 0 ? size - 1 : size;
    if (*read > source->size) *read = source->size;
    memcpy(data, source->bytes, *read); return texture_fail() ? -1 : 0;
}
static int32_t STDCALL texture_write(void *file, const void *data, uint32_t size, uint32_t *written)
{
    struct texture_file *dest = texture_handle(file);
    uint32_t *entry = texture_log(30); entry[1] = (uint32_t)(uintptr_t)file; entry[2] = size; entry[3] = draw_hash(data, size);
    *written = size && s_weather.number % 5 == 0 ? size - 1 : size;
    if (*written > sizeof(dest->bytes)) exit(2);
    memcpy(dest->bytes, data, *written); dest->size = *written; return texture_fail() ? -1 : 0;
}
static int32_t STDCALL texture_resize(void *file, uint32_t size)
{
    uint32_t *entry = texture_log(31); entry[1] = (uint32_t)(uintptr_t)file; entry[2] = size;
    if (texture_fail()) return -1;
    texture_handle(file)->size = size; return 0;
}
static int32_t STDCALL texture_close(void *file)
{
    uint32_t *entry = texture_log(32), handle = (uint32_t)(uintptr_t)file - 0x7000;
    entry[1] = (uint32_t)(uintptr_t)file; texture_handle(file);
    if (texture_fail()) return -1;
    s_texture.handles[handle][1] = 0; return 0;
}
static void texture_snapshot(uint32_t step, uint32_t result)
{
    s_texture.returns[step] = result; s_texture.outcomes[step] = s_command_outcome;
    s_texture.state[step][0] = get_u32(s_texture_module->data, 0xa0);
    s_texture.state[step][1] = get_u32(s_texture_module->data, 0xc0);
    s_texture.state[step][2] = get_u32(s_texture_module->data, 0x984);
    s_texture.state[step][3] = draw_hash(s_texture.names, sizeof(s_texture.names));
}
static void texture_setup_host(uint8_t *host)
{
    void *function;
#define TEXTURE_HOST(slot, callback) function = callback; memcpy(host + slot, &function, 4)
    TEXTURE_HOST(0x20c, weather_alloc); TEXTURE_HOST(0x214, weather_free); TEXTURE_HOST(0x218, app_players_memset);
    TEXTURE_HOST(0x21c, weather_copy); TEXTURE_HOST(0x228, texture_compare); TEXTURE_HOST(0x2b8, app_players_strlen);
    TEXTURE_HOST(0x2a8, app_players_strcpy); TEXTURE_HOST(0x2b0, texture_concat); TEXTURE_HOST(0x130, texture_current);
    TEXTURE_HOST(0x2a0, texture_resource); TEXTURE_HOST(0x188, texture_split); TEXTURE_HOST(0x184, texture_join);
    TEXTURE_HOST(0x158, texture_exists); TEXTURE_HOST(0x14c, texture_unlink); TEXTURE_HOST(0x17c, texture_open);
    TEXTURE_HOST(0x148, texture_create); TEXTURE_HOST(0x1a4, texture_file_size); TEXTURE_HOST(0x19c, texture_read);
    TEXTURE_HOST(0x1b0, texture_write); TEXTURE_HOST(0x1a8, texture_resize); TEXTURE_HOST(0x13c, texture_close);
    TEXTURE_HOST(0x2f8, texture_list_add); TEXTURE_HOST(0x304, texture_list_clear);
#undef TEXTURE_HOST
}
static void run_textures(const struct multip_module *module, uint32_t number, struct texture_results *results)
{
    uint8_t host[0x320] = {0}, model[512] = {0}, data[64];
    uint8_t *record = model;
    void *g3d[26] = {0}, *loaded = NULL;
    struct send_queue queues[2];
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    uint32_t random_state = number ^ 0x76421938u, index, step, span, loaded_size = 0xf1234567u;
    char write_name[15] = "SAVED.BMP", read_name[15] = "READ.BMP";
    static const char *const s_models[] = {"ONE.BMP", "TWO.BMP", "ONE.BMP", "HAVE.BMP", "THREE.BMP"};
    static const char *const s_strings[] = {"A.BMP    ", "      ", "nodot", ""};
    typedef int32_t (STDCALL *texture_name_fn)(const char *);
    typedef int32_t (STDCALL *texture_path_fn)(char *, const char *);
    typedef int32_t (STDCALL *texture_model_fn)(const void *, uint32_t);
    typedef int32_t (STDCALL *texture_request_fn)(uint32_t, const void *, uint32_t);
    typedef int32_t (STDCALL *texture_read_fn)(const char *, void **, uint32_t *);
    typedef int32_t (STDCALL *texture_write_fn)(const char *, const void *, uint32_t);

    if (!event) exit(2);
    memset(&s_texture, 0, sizeof(s_texture)); memset(&s_weather, 0, sizeof(s_weather)); memset(queues, 0, sizeof(queues));
    s_texture_module = module; s_command_module = module; s_weather.number = number;
    s_weather.fail_call = number % 3 == 0 ? 1 + (number / 3) % 160 : 0;
    s_weather.fail_free = number % 11 == 0 ? 1 : 0;
    memcpy(module->data, module->initial_data, 0x1000);
texture_setup_host(host);
    g3d[0x64 / 4] = s_texture_next;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host); put_u32(module->data, 0x2c, (uint32_t)(uintptr_t)g3d);
    put_u32(module->data, 0xf00, number % 4); put_u32(module->data, 0xe5c, (uint32_t)(uintptr_t)event);
    put_u32(module->data, 0x984, random_u32(&random_state));
    for (index = 0; index < 2; index++) {
        InitializeCriticalSection(&queues[index].lock);
        put_u32(module->data, 0xe60 + index * 4, (uint32_t)(uintptr_t)&queues[index]);
        put_u32(module->data, 0xa0 + index * 0x20, 0);
        put_u32(module->data, 0xb8 + index * 0x20, (uint32_t)(uintptr_t)s_texture.names[index]);
    }
    strcpy(s_texture.files[0].path, "C:\\FS98\\Texture\\READ.BMP"); s_texture.files[0].exists = 1; s_texture.files[0].size = number % 65;
    strcpy(s_texture.files[1].path, "C:\\FS98\\Texture\\HAVE.BMP"); s_texture.files[1].exists = number & 4 ? 1 : 0;
    for (index = 0; index < 128; index++) s_texture.files[0].bytes[index] = random_u32(&random_state);
    for (index = 0; index < sizeof(data); index++) data[index] = random_u32(&random_state);
    write_name[14] = (char)(number | 1);
    for (index = 0; index < 5; index++) {
        *(uint16_t *)(void *)record = 0x18; memcpy(record + 10, s_models[index], strlen(s_models[index]) + 1);
        if (index == 2 && (number & 8)) record[23] = 0x73;
        {
            uint8_t *next = ((uint8_t *(STDCALL *)(const void *))s_texture_next)(record);
            if (next <= record || next > record + 64) exit(2);
            record = next;
        }
    }
    span = (uint32_t)(record - model);
    *(uint16_t *)(void *)record = 0xffff;
    for (step = 0; step < 12; step++) {
        uint32_t result = 0;
        s_command_outcome = 0;
        if (__builtin_setjmp(s_command_jump) == 0) {
            s_command_active = 1;
            if (step < 3) {
                memset(s_texture_input, 0, 64); strcpy((char *)s_texture_input, s_strings[number % 4]);
                memset(s_texture.output, 0x9b, sizeof(s_texture.output));
                if (step == 0) result = ((texture_name_fn)send_internal(module, 141))((char *)s_texture_input);
                else result = ((texture_path_fn)send_internal(module, step == 1 ? 142 : 143))(s_texture.output, (char *)s_texture_input);
                memcpy(s_texture.input, s_texture_input, 64);
            } else if (step == 3) result = ((texture_model_fn)send_internal(module, 146))(model, number & 1 ? span : 1);
            else if (step == 4) result = ((texture_request_fn)send_internal(module, 148))(0xb001, model, span);
            else if (step == 5) result = ((texture_read_fn)send_internal(module, 149))(read_name, &loaded, &loaded_size);
            else if (step == 6) result = ((texture_write_fn)send_internal(module, 150))(write_name, data, number % 65);
            else if (step == 7) result = ((texture_name_fn)send_internal(module, 145))(write_name);
            else if (step == 8) result = ((multip_worker_fn)send_internal(module, 151))();
            else if (step == 9) result = ((multip_worker_fn)send_internal(module, 151))();
            else if (step == 10) result = ((texture_request_fn)send_internal(module, 147))(0xb002, s_texture.names[0], get_u32(module->data, 0xa0));
            else result = ((message_id_fn)send_internal(module, 152))(0xb003);
            s_command_active = 0;
        }
        if (s_command_outcome) memcpy(s_texture.input, s_texture_input, 64);
        texture_snapshot(step, result);
    }
    s_texture.state[11][3] = loaded_size;
    for (index = 0; index < 2; index++) {
        s_weather.queues[index][0] = queues[index].count;
        s_weather.queues[index][1] = weather_address(queues[index].head);
        s_weather.queues[index][2] = weather_address(queues[index].tail);
        s_weather.queues[index][3] = queues[index].lock.RecursionCount;
        while (queues[index].lock.RecursionCount != 0) LeaveCriticalSection(&queues[index].lock);
        DeleteCriticalSection(&queues[index].lock);
    }
    CloseHandle(event);
    memcpy(&s_texture.weather, &s_weather, sizeof(s_weather)); memcpy(results, &s_texture, sizeof(*results));
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t normalized = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (normalized != 0xffffffffu) put_u32(results->weather.heap[index], byte, normalized);
        }
    }
}
static uint32_t test_textures(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct texture_results s_retail, s_ours;
    uint32_t number, bad = 0, faults = 0;
    HMODULE g3d = LoadLibraryA("G3D_retail.dll");
    void *handler;
    if (!g3d) exit(2);
    s_texture_next = *(void **)((uint8_t *)GetProcAddress(g3d, "Linkage") + 0x64);
    s_texture_guard = VirtualAlloc(NULL, 0x2000, MEM_RESERVE, PAGE_NOACCESS);
    s_texture_input = s_texture_guard + 0x1000;
    if (!s_texture_guard || !VirtualAlloc(s_texture_input, 0x1000, MEM_COMMIT, PAGE_READWRITE)) exit(2);
    handler = AddVectoredExceptionHandler(1, command_fault); if (!handler) exit(2);
    for (number = 0; number < scenarios; number++) {
        multip_test_context("textures", "retail", number); run_textures(retail, number, &s_retail);
        multip_test_context("textures", "ours", number); run_textures(ours, number, &s_ours);
        faults += s_retail.outcomes[0] != 0; faults += s_retail.outcomes[1] != 0; faults += s_retail.outcomes[2] != 0;
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("textures scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    RemoveVectoredExceptionHandler(handler); VirtualFree(s_texture_guard, 0, MEM_RELEASE); FreeLibrary(g3d);
    printf("texture path outcomes: %lu pointer faults\n", (unsigned long)faults);
    printf("MULTIP texture catalogue/file services: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
