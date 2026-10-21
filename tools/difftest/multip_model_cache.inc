/* Cache files are byte streams with independent cursors and short transfers. */
struct model_cache_results {
    struct texture_results texture;
    uint8_t aircraft[44];
    uint32_t cursors[16], states[6][4];
};
static uint32_t s_cache_cursors[16], s_cache_reads, s_cache_writes;
static int32_t STDCALL cache_resource(uint32_t id, char *out, uint32_t size)
{
    uint32_t *entry = weather_log(20); entry[1] = id; entry[2] = size;
    strcpy(out, id == 0x9010 ? "Cache" : "Texture"); return 0;
}
static int32_t STDCALL cache_join(char *out, const char *drive, const char *directory, const char *name)
{
    char path[260] = {0};
    const char *parts[3] = {drive, directory, name};
    uint32_t index, *entry = weather_log(22);
    entry[1] = drive ? profile_hash(drive) : 0;
    entry[2] = directory ? profile_hash(directory) : 0;
    entry[3] = name ? profile_hash(name) : 0;
    if (texture_fail()) { *out = 0; return -1; }
    for (index = 0; index < 3; index++) if (parts[index] && *parts[index]) {
        if (*path) strcat(path, "\\");
        strcat(path, parts[index]);
    }
    strcpy(out, path); return 0;
}
static int32_t STDCALL cache_mkdir(const char *path)
{
    uint32_t *entry = weather_log(45); entry[1] = profile_hash(path); return texture_fail() ? -1 : 0;
}
static int32_t STDCALL cache_realloc(int32_t flags, uint32_t size, void **out)
{
    uint32_t *entry = weather_log(44), block = s_weather.blocks, old;
    entry[1] = flags; entry[2] = size; entry[3] = weather_address(*out);
    if (texture_fail()) return -1;
    if (block >= 128 || size > 1024) { fprintf(stderr, "cache realloc capacity exceeded\n"); exit(2); }
    s_weather.blocks++; s_weather.sizes[block] = size; s_weather.live[block] = 1;
    memset(s_weather.heap[block], 0xa5, size);
    for (old = 0; old < block; old++) if (*out == s_weather.heap[old]) {
        uint32_t copy = size < s_weather.sizes[old] ? size : s_weather.sizes[old];
        memcpy(s_weather.heap[block], *out, copy); s_weather.live[old] = 0; break;
    }
    *out = s_weather.heap[block]; return 0;
}
static int32_t STDCALL cache_read(void *file, void *data, uint32_t size, uint32_t *read)
{
    struct texture_file *source = texture_handle(file);
    uint32_t handle = (uint32_t)(uintptr_t)file - 0x7000, *entry = weather_log(29);
    entry[1] = (uint32_t)(uintptr_t)file; entry[2] = size; entry[4] = s_cache_cursors[handle];
    *read = size && s_weather.number % 7 == 0 && s_cache_reads % 6 == s_weather.number / 7 % 6 ? size - 1 : size;
    s_cache_reads++;
    if (*read > source->size - s_cache_cursors[handle]) *read = source->size - s_cache_cursors[handle];
    memcpy(data, source->bytes + s_cache_cursors[handle], *read);
    s_cache_cursors[handle] += *read; return texture_fail() ? -1 : 0;
}
static int32_t STDCALL cache_write(void *file, const void *data, uint32_t size, uint32_t *written)
{
    struct texture_file *dest = texture_handle(file);
    uint32_t handle = (uint32_t)(uintptr_t)file - 0x7000, *entry = weather_log(30);
    entry[1] = (uint32_t)(uintptr_t)file; entry[2] = size;
    entry[3] = draw_hash(data, size); entry[4] = s_cache_cursors[handle];
    *written = size && s_weather.number % 5 == 0 && s_cache_writes % 6 == s_weather.number / 5 % 6 ? size - 1 : size;
    s_cache_writes++;
    if (*written + s_cache_cursors[handle] > sizeof(dest->bytes)) { fprintf(stderr, "cache file capacity exceeded\n"); exit(2); }
    memcpy(dest->bytes + s_cache_cursors[handle], data, *written);
    s_cache_cursors[handle] += *written; dest->size = s_cache_cursors[handle];
    return texture_fail() ? -1 : 0;
}
static void cache_setup_host(uint8_t *host)
{
    void *function;
    texture_setup_host(host);
#define CACHE_HOST(slot, callback) function = callback; memcpy(host + slot, &function, 4)
    CACHE_HOST(0x2a0, cache_resource); CACHE_HOST(0x184, cache_join); CACHE_HOST(0x11c, cache_mkdir);
    CACHE_HOST(0x210, cache_realloc); CACHE_HOST(0x19c, cache_read); CACHE_HOST(0x1b0, cache_write);
#undef CACHE_HOST
}
static void run_model_cache(const struct multip_module *module, uint32_t number,
    struct model_cache_results *results)
{
    uint8_t host[0x320] = {0}, aircraft[44] = {0};
    uint32_t index, step, random_state = number ^ 0x3192547u;
    typedef int32_t (STDCALL *cache_fn)(void *, const char *);
    memset(results, 0, sizeof(*results)); memset(&s_weather, 0, sizeof(s_weather));
    memset(&s_texture, 0, sizeof(s_texture)); memset(s_cache_cursors, 0, sizeof(s_cache_cursors));
    s_texture_module = module; s_weather.number = number; s_cache_reads = s_cache_writes = 0;
    s_weather.fail_call = number % 3 == 0 ? 1 + number / 3 % 100 : 0;
    s_weather.fail_free = 0;
    s_weather.blocks = 2; s_weather.sizes[0] = 64; s_weather.sizes[1] = 128;
    s_weather.live[0] = s_weather.live[1] = 1;
    for (index = 0; index < 128; index++) {
        s_weather.heap[0][index] = random_u32(&random_state);
        s_weather.heap[1][index] = random_u32(&random_state);
    }
    put_u32(aircraft, 0x10, (uint32_t)(uintptr_t)s_weather.heap[0]); put_u32(aircraft, 0x14, number % 5);
    put_u32(aircraft, 0x18, (uint32_t)(uintptr_t)s_weather.heap[1]); put_u32(aircraft, 0x1c, 104 + number % 5);
    put_u32(aircraft, 0x20, random_u32(&random_state)); put_u32(aircraft, 0x28, random_u32(&random_state));
    strcpy(s_texture.files[0].path, "Cache\\MODEL\\Cache"); s_texture.files[0].exists = 1;
    s_texture.files[0].size = 128;
    for (index = 0; index < 128; index++) s_texture.files[0].bytes[index] = random_u32(&random_state);
    put_u32(s_texture.files[0].bytes, 0, 0x12345678); put_u32(s_texture.files[0].bytes, 4, 0xabcdef01);
    put_u32(s_texture.files[0].bytes, 8, number % 5); put_u32(s_texture.files[0].bytes, 12, 104 + number % 5);
    memcpy(module->data, module->initial_data, 0x1000); cache_setup_host(host);
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    for (step = 0; step < 6; step++) {
        uint32_t result;
        if (step == 0) result = ((int32_t (STDCALL *)(const char *))send_internal(module, 186))("MODEL");
        else result = ((cache_fn)send_internal(module, step == 1 || step == 3 ? 188 : 187))(aircraft, step == 5 ? "MISSING" : "MODEL");
        results->states[step][0] = result;
        results->states[step][1] = texture_receive_hash();
        results->states[step][2] = s_weather.calls;
        results->states[step][3] = draw_hash(s_texture.files, sizeof(s_texture.files));
    }
    memcpy(results->aircraft, aircraft, sizeof(aircraft)); memcpy(results->cursors, s_cache_cursors, sizeof(s_cache_cursors));
    memcpy(&s_texture.weather, &s_weather, sizeof(s_weather)); memcpy(&results->texture, &s_texture, sizeof(s_texture));
    for (index = 0; index < 44; index += 4) {
        uint32_t address = weather_address((void *)(uintptr_t)get_u32(aircraft, index));
        if (address != 0xffffffffu) put_u32(results->aircraft, index, address);
    }
    for (index = 0; index < s_weather.blocks; index++) {
        uint32_t byte;
        for (byte = 0; byte < s_weather.sizes[index]; byte += 4) {
            uint32_t address = weather_address((void *)(uintptr_t)get_u32(s_weather.heap[index], byte));
            if (address != 0xffffffffu) put_u32(results->texture.weather.heap[index], byte, address);
        }
    }
}
static uint32_t test_model_cache(const struct multip_module *retail,
    const struct multip_module *ours, uint32_t scenarios)
{
    static struct model_cache_results s_retail, s_ours;
    uint32_t number, bad = 0;
    for (number = 0; number < scenarios; number++) {
        multip_test_context("model_cache", "retail", number); run_model_cache(retail, number, &s_retail);
        multip_test_context("model_cache", "ours", number); run_model_cache(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("model_cache scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number,
                        (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    printf("MULTIP aircraft cache files: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
