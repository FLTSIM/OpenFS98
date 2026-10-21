/* Extrapolation uses real, stateless retail UTIL conversions and longitude
 * arithmetic behind logging wrappers; model records remain fully observable. */
struct motion_fixture {
    uint32_t calls, copy_calls, number, ticks, chain_result, divide_repairs;
    uint32_t log[512][8], returns[9], state[9][7];
    uint8_t players[4][256], models[4][64], global[0x1200];
};
static struct motion_fixture s_motion;
static const struct multip_module *s_motion_module;
static uint8_t *s_motion_util;
static HMODULE s_motion_util_module;
/* The host repairs unsigned DIV overflow to quotient UINT32_MAX/remainder 0.
 * UTIL +0x44 can reach this even for ordinary negative easting values. */
static LONG CALLBACK motion_divide(EXCEPTION_POINTERS *exception)
{
    CONTEXT *context = exception->ContextRecord;
    uintptr_t ip = context->Eip, base = (uintptr_t)s_motion_util_module;
    if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO &&
        ip >= base + 0x1400 && ip < base + 0x1600 &&
        *(const uint16_t *)ip == 0xf3f7) {
        context->Eax = 0xffffffffu; context->Edx = 0; context->Eip += 2;
        s_motion.divide_repairs++;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static uint32_t *motion_log(uint32_t tag)
{
    uint32_t *entry;
    if (s_motion.calls >= 512) exit(2);
    entry = s_motion.log[s_motion.calls++]; entry[0] = tag;
    return entry;
}
static uint32_t STDCALL motion_ticks(void)
{
    motion_log(1);
    if (s_motion.number & 0x100) put_u32(s_motion_module->data, M_PLAYER_COUNT, 0);
    return ++s_motion.ticks;
}
static void motion_to_double(uint32_t slot, double *out, const void *in)
{
    uint32_t *entry = motion_log(slot);
    entry[1] = *(const uint16_t *)((const uint8_t *)in + 2);
    entry[2] = get_u32(in, 4);
    (*(void (STDCALL **)(double *, const void *))(s_motion_util + slot))(out, in);
    memcpy(entry + 3, out, 8);
}
static void STDCALL motion_signed_read(double *out, const void *in) { motion_to_double(0xc8, out, in); }
static void STDCALL motion_unsigned_read(double *out, const void *in) { motion_to_double(0xc0, out, in); }
static void motion_to_fixed(uint32_t slot, const double *in, void *out)
{
    uint32_t *entry = motion_log(slot);
    memcpy(entry + 1, in, 8);
    (*(void (STDCALL **)(const double *, void *))(s_motion_util + slot))(in, out);
    entry[3] = *(uint16_t *)((uint8_t *)out + 2); entry[4] = get_u32(out, 4);
}
static void STDCALL motion_signed_write(const double *in, void *out) { motion_to_fixed(0xc4, in, out); }
static void STDCALL motion_unsigned_write(const double *in, void *out) { motion_to_fixed(0xbc, in, out); }
static void motion_fixed_op(uint32_t slot, void *a, const void *b, void *out)
{
    uint32_t *entry = motion_log(slot);
    entry[1] = *(uint16_t *)((uint8_t *)a + 2); entry[2] = get_u32(a, 4);
    entry[3] = *(const uint16_t *)((const uint8_t *)b + 2); entry[4] = get_u32(b, 4);
    (*(void (STDCALL **)(void *, const void *, void *))(s_motion_util + slot))(a, b, out);
    entry[5] = *(uint16_t *)((uint8_t *)out + 2); entry[6] = get_u32(out, 4);
}
static void STDCALL motion_add(void *a, const void *b, void *out) { motion_fixed_op(0x44, a, b, out); }
static void STDCALL motion_subtract(void *a, const void *b, void *out) { motion_fixed_op(0x48, a, b, out); }
static int32_t STDCALL motion_copy(void *dest, const void *source, uint32_t size)
{
    uint32_t *entry = motion_log(2);
    entry[1] = size;
    entry[2] = draw_hash(source, size);
    memmove(dest, source, size);
    return ++s_motion.copy_calls == s_motion.number % 8;
}
static int32_t STDCALL motion_strcpy(char *dest, const char *source)
{
    uint32_t *entry = motion_log(5);
    entry[1] = profile_hash(source); strcpy(dest, source); return -1;
}
static int32_t motion_registration(uint32_t tag, uint32_t code, track_callback_fn callback)
{
    uint32_t *entry = motion_log(tag);
    entry[1] = code; entry[2] = (void *)callback == send_internal(s_motion_module, 94);
    entry[3] = get_u32(s_motion_module->data, 0x968);
    return (int32_t)s_motion.chain_result;
}
static int32_t STDCALL motion_add_callback(uint32_t code, track_callback_fn callback)
{ return motion_registration(3, code, callback); }
static int32_t STDCALL motion_remove_callback(uint32_t code, track_callback_fn callback)
{ return motion_registration(4, code, callback); }
static void motion_snapshot(uint32_t step, uint32_t result)
{
    static const uint32_t s_offsets[] = {0x968,0x970,0x974,0x978,0xa5c,0xa60,0x660};
    uint32_t index;
    s_motion.returns[step] = result;
    for (index = 0; index < 7; index++) s_motion.state[step][index] = get_u32(s_motion_module->data, s_offsets[index]);
}
static void run_motion(const struct multip_module *module, uint32_t number, struct motion_fixture *results)
{
    uint8_t host[0x2b0] = {0};
    void *util[51] = {0}, *chain[9] = {0}, *function;
    uint32_t random_state = number ^ 0x34876291u, index, byte;
    static const uint32_t s_angles[] = {0,0x3fff,0x4000,0xc000,0xc001,0xffff,0xffffffffu};
    double value;

    memset(&s_motion, 0, sizeof(s_motion)); s_motion.number = number; s_motion_module = module;
    memcpy(module->data, module->initial_data, 0x1000);
    function = motion_ticks; memcpy(host + 0x38, &function, 4);
    function = motion_copy; memcpy(host + 0x21c, &function, 4);
    function = motion_strcpy; memcpy(host + 0x2a8, &function, 4);
    util[0xc8 / 4] = motion_signed_read; util[0xc0 / 4] = motion_unsigned_read;
    util[0xc4 / 4] = motion_signed_write; util[0xbc / 4] = motion_unsigned_write;
    util[0x44 / 4] = motion_add; util[0x48 / 4] = motion_subtract;
    chain[0x18 / 4] = motion_add_callback; chain[0x1c / 4] = motion_remove_callback;
    put_u32(module->data, 4, (uint32_t)(uintptr_t)host);
    put_u32(module->data, 0x44, (uint32_t)(uintptr_t)util);
    put_u32(module->data, 0x1c, (uint32_t)(uintptr_t)chain);
    put_u32(module->data, 0xc, (uint32_t)(uintptr_t)s_motion.global);
    for (index = 0; index < 4; index++) {
        uint8_t *player = s_motion.players[index], *model = s_motion.models[index];
        for (byte = 0; byte < 256; byte++) player[byte] = random_u32(&random_state);
        for (byte = 0; byte < 64; byte++) model[byte] = random_u32(&random_state);
        put_u32(player, 0, (uint32_t)(uintptr_t)"motion peer");
        put_u32(player, 4, 0x5000 + index); put_u32(player, 8, 1);
        put_u32(player, 0xc, 0); put_u32(player, 0x28, (uint32_t)(uintptr_t)model);
        put_u32(player, 0x58, (int32_t)random_u32(&random_state) % 8000000);
        put_u32(player, 0x4c, 1); put_u32(player, 0x98, (number + index) % 3);
        for (byte = 0x7c; byte <= 0xc4; byte += 8) {
            value = (int32_t)random_u32(&random_state) / 1234567.0;
            memcpy(player + byte, &value, 8);
        }
        for (byte = 4; byte <= 0x14; byte += 8) put_u32(model, byte + 4, (number & 1 ? 0u : 0xffff0000u) + index);
        put_u32(model, 0x10, s_angles[(number + index) % 7]);
    }
    put_u32(module->data, M_PLAYERS, (uint32_t)(uintptr_t)s_motion.players);
    put_u32(module->data, M_LOCAL_PLAYER, (uint32_t)(uintptr_t)s_motion.players[0]);
    put_u32(module->data, M_PLAYER_COUNT, number % 5);
    put_u32(module->data, 0x984, (number / 5) % 4 == 0);
    put_u32(module->data, 0x184, (number / 20) % 3 != 0);
    put_u32(module->data, M_PLAYER_MODE, 0);
    put_u32(module->data, 0x970, (number / 60) % 3);
    put_u32(module->data, 0x974, random_u32(&random_state));
    put_u32(module->data, 0xad8, number & 1 ? 0xffffffffu : random_u32(&random_state));
    value = (int32_t)random_u32(&random_state) / 100000000.0; memcpy(module->data + 0xa78, &value, 8);
    value = (int32_t)random_u32(&random_state) / 12345678.0; memcpy(module->data + 0xa80, &value, 8);
    s_motion.ticks = random_u32(&random_state); s_motion.chain_result = number % 3 == 0 ? 1 : 0;
    motion_snapshot(0, ((track_show_fn)send_internal(module, 92))( (uint32_t)(uintptr_t)s_motion.players[0]));
    motion_snapshot(1, ((multip_worker_fn)send_internal(module, 95))());
    /* Independently gate every record on the frame traversal. */
    put_u32(s_motion.players[0], 0xc, number & 2);
    put_u32(s_motion.players[1], 8, number & 4 ? 2 : 1);
    put_u32(s_motion.players[2], 0x28, number & 8 ? 0 : (uint32_t)(uintptr_t)s_motion.models[2]);
    put_u32(s_motion.players[3], 0x4c, number & 16);
    motion_snapshot(2, ((multip_worker_fn)send_internal(module, 93))());
    motion_snapshot(3, ((track_callback_fn)send_internal(module, 94))(0x13, 0));
    motion_snapshot(4, ((multip_worker_fn)send_internal(module, 96))());
    s_motion.chain_result = 0;
    motion_snapshot(5, ((multip_worker_fn)send_internal(module, 96))());
    motion_snapshot(6, ((multip_worker_fn)send_internal(module, 96))());
    motion_snapshot(7, ((multip_worker_fn)send_internal(module, 95))());
    motion_snapshot(8, ((track_callback_fn)send_internal(module, 94))(0xffffffffu, 1));
    memcpy(results, &s_motion, sizeof(*results));
}
static uint32_t test_motion(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    static struct motion_fixture s_retail, s_ours;
    uint32_t number, bad = 0;
    HMODULE util_module = LoadLibraryA("UTIL_retail.dll");
    void *divide_handler;
    if (util_module == NULL) { fprintf(stderr, "motion fixture requires retail UTIL\n"); exit(2); }
    s_motion_util_module = util_module;
    divide_handler = AddVectoredExceptionHandler(1, motion_divide);
    if (divide_handler == NULL) exit(2);
    s_motion_util = (uint8_t *)GetProcAddress(util_module, "Linkage");
    if (s_motion_util == NULL) exit(2);
    for (number = 0; number < scenarios; number++) {
        multip_test_context("motion", "retail", number); run_motion(retail, number, &s_retail);
        multip_test_context("motion", "ours", number); run_motion(ours, number, &s_ours);
        if (memcmp(&s_retail, &s_ours, sizeof(s_ours)) != 0) {
            if (bad < 4) {
                uint32_t byte;
                for (byte = 0; byte < sizeof(s_ours); byte++) if (((uint8_t *)&s_retail)[byte] != ((uint8_t *)&s_ours)[byte]) {
                    printf("motion scenario %lu byte %lu: retail %u ours %u\n", (unsigned long)number, (unsigned long)byte, ((uint8_t *)&s_retail)[byte], ((uint8_t *)&s_ours)[byte]); break;
                }
            }
            bad++;
        }
    }
    RemoveVectoredExceptionHandler(divide_handler);
    FreeLibrary(util_module);
    s_motion_util_module = NULL;
    printf("MULTIP aircraft extrapolation/CHAIN 0x13: %lu scenarios, %lu differ\n", (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
