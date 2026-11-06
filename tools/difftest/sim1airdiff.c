/* Caller-side AIR ownership/init comparison. The one retail call to its
 * pending decoder is replaced, in private process memory only, by the same
 * explicit decoder fixture bound through the test DLL. This does not accept
 * the .AIR decoder. Publication and existing state restoration remain real.
 */
#define SIM1_STATE_REFERENCE_ONLY
#define SIM1_TRANSCRIPT_CAPACITY 64
#include "sim1statediff.c"

static uint8_t acontain_table[0x40];
static uint8_t air_data[512], air_buffer[544], air_buffer_initial[544], air_buffer_expected[544];
static uint32_t air_file_token, air_failure, air_size_value, air_result, air_operation;
static uint32_t air_opened, air_allocated, air_parse_count, air_registration[3];
static uint32_t air_model_expected[6], air_operations[4], air_cases[5][11];
static uint32_t air_entries[2][2];
static uint8_t retail_call[5];
static char air_path[256];

uint32_t __stdcall air_query(void *aircraft, uint32_t key, char *out, uint32_t capacity);
uint32_t __stdcall air_open(uintptr_t *out, const char *path, uint32_t extension, uint32_t flags);
uint32_t __stdcall air_size(uintptr_t file, uint32_t *out);
uint32_t __stdcall air_read(uintptr_t file, void *out, uint32_t size, uint32_t *got);
uint32_t __stdcall air_alloc(uint32_t count, uint32_t size, void **out, uint32_t flags);
uint32_t __stdcall air_free(void **out);
uint32_t __stdcall air_close(uintptr_t file);
uint32_t __stdcall air_parser(const uint8_t *data, uint32_t size);
uint32_t __stdcall air_move(void *out, const void *source, uint32_t size);
uint32_t __stdcall air_add(uint32_t list, uintptr_t callback);
uint32_t __stdcall air_remove(uint32_t list, uintptr_t callback);

uint32_t __stdcall air_query_core(void *aircraft, uint32_t key, char *out, uint32_t capacity)
{
    uint32_t *row = event(10);
    row[1] = key; row[2] = capacity;
    if (aircraft != global + 0x11e0 || key != 2 || capacity != 0x100)
        fail("AIR aircraft query arguments", key);
    memcpy(out, air_path, strlen(air_path) + 1);
    return 1;
}

uint32_t __stdcall air_open_core(uintptr_t *out, const char *path, uint32_t extension, uint32_t flags)
{
    uint32_t *row = event(11);
    row[1] = extension; row[2] = flags;
    if (strcmp(path, air_path) || extension != 0xf || flags != 0 || air_opened)
        fail("AIR open arguments", extension);
    *out = 0;
    if (air_failure == 1) return 0xffffffff;
    *out = (uintptr_t)&air_file_token;
    air_opened = 1;
    return 0;
}

uint32_t __stdcall air_size_core(uintptr_t file, uint32_t *out)
{
    event(12);
    if (!air_opened || file != (uintptr_t)&air_file_token) fail("AIR size ownership", 0);
    if (air_failure == 2) return 0xffffffff;
    *out = air_size_value;
    return 0;
}

uint32_t __stdcall air_alloc_core(uint32_t count, uint32_t size, void **out, uint32_t flags)
{
    uint32_t *row;
    if (flags == 1) return state_alloc_core(count, size, out, flags);
    row = event(13);
    row[1] = count; row[2] = size; row[3] = flags;
    if (count != 1 || size != air_size_value || flags != 0 || air_allocated)
        fail("AIR allocation arguments", size);
    *out = air_buffer + 16;
    air_allocated = 1;
    return 0;
}

uint32_t __stdcall air_read_core(uintptr_t file, void *out, uint32_t size, uint32_t *got)
{
    uint32_t *row = event(14);
    row[1] = size;
    if (!air_opened || file != (uintptr_t)&air_file_token || out != air_buffer + 16 ||
        !air_allocated || size != air_size_value || !got)
        fail("AIR read arguments/ownership", size);
    *got = 0;
    if (air_failure == 3) return 0xffffffff;
    *got = air_failure == 4 && size ? size - 1 : size;
    memcpy(out, air_data, *got);
    row[2] = *got;
    return 0;
}

uint32_t __stdcall air_parser_core(const uint8_t *data, uint32_t size)
{
    uint32_t *row = event(15);
    row[1] = size; row[2] = air_result;
    if (!air_allocated || data != air_buffer + 16 || size != air_size_value)
        fail("AIR decoder boundary arguments", size);
    compare(air_data, data, size, "AIR decoder input bytes");
    ++air_parse_count;
    /* Controlled decoder effects make publication-before-load observable.
     * This is a fixture, not an implementation of the retail record format. */
    write32(sides[role].image + 0x1d10, air_result);
    if (size >= 32) {
        memcpy(sides[role].image + 0x13b8, data, 24);
        memcpy(sides[role].image + 0x13b0, data + 24, 8);
    }
    return air_result;
}

uint32_t __stdcall air_free_core(void **out)
{
    if (*out == air_buffer + 16) {
        event(16);
        if (!air_allocated) fail("AIR double free", 0);
        air_allocated = 0;
        *out = NULL;
        return 0;
    }
    return state_free_core(out);
}

uint32_t __stdcall air_close_core(uintptr_t file)
{
    event(17);
    if (!air_opened || file != (uintptr_t)&air_file_token || air_allocated)
        fail("AIR close ownership/order", 0);
    air_opened = 0;
    return 0;
}

uint32_t __stdcall air_move_core(void *out, const void *source, uint32_t size)
{
    uint32_t *row = event(18);
    row[1] = size;
    if (out != sides[role].image + 0x1730 || source != global + 0x560 || size != 0x24)
        fail("descriptor copy arguments", size);
    memmove(out, source, size);
    return 0;
}

uint32_t __stdcall air_add_core(uint32_t list, uintptr_t callback)
{
    uint32_t i, *row = event(19);
    for (i = 0; i < 3; ++i) if (callback == callbacks[role][i]) break;
    if (i == 3 || list != (i == 0 ? 1u : i == 1 ? 5u : 0x14u))
        fail("AIR CHAIN add identity", list);
    row[1] = list; row[2] = i;
    ++air_registration[i];
    return 0x100 + i;
}

uint32_t __stdcall air_remove_core(uint32_t list, uintptr_t callback)
{
    uint32_t i, result = state_remove_core(list, callback);
    for (i = 0; i < 3; ++i) if (callback == callbacks[role][i]) break;
    if (i >= 3 || air_registration[i] != 1) fail("AIR CHAIN remove ownership", list);
    --air_registration[i];
    return result;
}

static void patch_decoder(int install)
{
    uint8_t *site = (uint8_t *)sides[0].module + 0xaf48;
    DWORD old_protection, ignored;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old_protection)) fail("AIR patch protection", GetLastError());
    if (install) {
        if (site[0] != 0xe8 || read32(site + 1) != 0x43) fail("AIR decoder call site changed", 0);
        memcpy(retail_call, site, sizeof retail_call);
        write32(site + 1, (uintptr_t)air_parser - (uintptr_t)(site + 5));
    } else {
        memcpy(site, retail_call, sizeof retail_call);
    }
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    if (!VirtualProtect(site, 5, old_protection, &ignored)) fail("AIR restore protection", GetLastError());
}

static void air_case(void)
{
    static const uint32_t precision[] = {0, 2, 3};
    uint32_t i, model[6], registers[8];
    slot = air_operation == 0 ? 0xae90 : air_operation == 1 ? 0x10c40 : 4;
    call_control = 0x007f | (precision[mode / 4] << 8) | ((mode % 4) << 10);
    call_preload = vector & 1;
    for (i = 0; i < 7; ++i) call_inputs[i] = read32(initial_output + 128 + i * 4);
    call_argc = 0;
    for (role = 0; role < 2; ++role) {
        uint8_t *image = sides[role].image;
        memcpy(image, sides[role].pristine, DATA_SIZE);
        memcpy(image + HEADER_SIZE, seeded + HEADER_SIZE, DATA_SIZE - HEADER_SIZE);
        memcpy(global, global_initial, sizeof global);
        memcpy(heap, heap_initial, sizeof heap);
        memcpy(air_buffer, air_buffer_initial, sizeof air_buffer);
        memset(calls, 0, sizeof calls); call_count = 0;
        air_opened = air_allocated = air_parse_count = 0;
        memset(air_registration, 0, sizeof air_registration);
        call_target = air_operation < 2 ? air_entries[role][air_operation] : sides[role].link[1];
        sim1_call();
        if (air_operation == 3) {
            call_target = sides[role].link[2];
            sim1_call();
        }
        for (i = 0; i < 7; ++i)
            if ((i == 1 || i >= 4) && call_registers[i] != call_inputs[i]) fail("AIR nonvolatile register", i);
        if (call_registers[7]) fail("AIR ESP", call_registers[7]);
        memcpy(registers, call_registers, sizeof registers);
        /* Only deinit's EAX is a known pointer; GLOBAL is a shared reset
         * fixture so its exact value already compares without normalization. */
        model[0] = air_opened; model[1] = air_allocated; model[2] = air_parse_count;
        memcpy(model + 3, air_registration, sizeof air_registration);
        if (!role) {
            memcpy(registers_expected, registers, sizeof registers);
            memcpy(expected_image, image, DATA_SIZE);
            memcpy(expected_fp, call_fp, sizeof call_fp);
            memcpy(global_expected, global, sizeof global);
            memcpy(heap_expected, heap, sizeof heap);
            memcpy(air_buffer_expected, air_buffer, sizeof air_buffer);
            memcpy(calls_expected, calls, sizeof calls);
            memcpy(air_model_expected, model, sizeof model);
            expected_call_count = call_count;
        } else {
            compare(registers_expected, registers, sizeof registers, "AIR integer registers");
            compare(expected_image + HEADER_SIZE, image + HEADER_SIZE, DATA_SIZE - HEADER_SIZE, "AIR state image");
            compare(global_expected, global, sizeof global, "AIR GLOBAL");
            compare(heap_expected, heap, sizeof heap, "AIR retained heap");
            compare(air_buffer_expected, air_buffer, sizeof air_buffer, "AIR read buffer/guards");
            compare(calls_expected, calls, sizeof calls, "AIR import transcript");
            compare(air_model_expected, model, sizeof model, "AIR ownership/decoder model");
            if (expected_call_count != call_count) fail("AIR import count", call_count);
            compare_fp();
        }
        compare(sides[role].pristine, image, HEADER_SIZE, "AIR import/Linkage prefix");
        if (air_allocated) fail("AIR buffer leaked", 0);
        if (air_opened != (air_operation != 1 && air_failure == 2)) fail("AIR file ownership oracle", 0);
        for (i = 0; i < 3; ++i)
            if (air_registration[i] != (air_operation == 2 ? 1u : 0u)) fail("AIR registration oracle", i);
        if (air_parse_count != (air_operation != 1 && (air_failure == 0 || (air_failure == 4 && !air_size_value))))
            fail("AIR decoder reachability oracle", 0);
    }
    ++air_operations[air_operation];
}

int main(int argc, char **argv)
{
    static const uint32_t lengths[] = {0, 1, 7, 8, 31, 32, 64, 255, 256, 511, 512};
    static const uint32_t paths[] = {1, 15, 254, 255};
    static const char *callback_names[] = {"Sim1TickCallback", "Sim1FrameCallback", "Sim1SituationCallback"};
    static const uint32_t callback_rvas[] = {0x1020, 0x1040, 0x10da0};
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 20000;
    const char *order = argc > 2 ? argv[2] : "retail-first";
    const char *ours = argc > 3 ? argv[3] : "bin/sim1-diff/SIM1_AIR_TEST.DLL";
    const char *filenames[2] = {"orig/Sim/SIM1.sim", ours};
    uint32_t first = !strcmp(order, "ours-first"), i, j;
    HMODULE util;
    uintptr_t *parser_fixture;
    uint32_t *util_link;
    void *reservation = NULL;
    if (!count || (strcmp(order, "retail-first") && strcmp(order, "ours-first") && strcmp(order, "relocate"))) return 2;
    SetUnhandledExceptionFilter(fault);
    util = LoadLibraryA("orig/Modules/UTIL.DLL");
    if (!util) fail("AIR load retail UTIL", GetLastError());
    util_link = (uint32_t *)GetProcAddress(util, "Linkage");
    if (!util_link || util_link[0] != 20 || !owns_code(util, util_link[0x3c / 4])) fail("AIR UTIL ownership", 0);
    if (!strcmp(order, "relocate")) {
        reservation = VirtualAlloc((void *)0x22000000, 0x40000, MEM_RESERVE, PAGE_NOACCESS);
        if (!reservation) fail("AIR reserve image", GetLastError());
    }
    write32(host_table + 0x17c, (uintptr_t)air_open);
    write32(host_table + 0x1a4, (uintptr_t)air_size);
    write32(host_table + 0x19c, (uintptr_t)air_read);
    write32(host_table + 0x20c, (uintptr_t)air_alloc);
    write32(host_table + 0x214, (uintptr_t)air_free);
    write32(host_table + 0x13c, (uintptr_t)air_close);
    write32(host_table + 0x21c, (uintptr_t)air_move);
    write32(acontain_table + 0x30, (uintptr_t)air_query);
    write32(chain_table + 0x18, (uintptr_t)air_add);
    write32(chain_table + 0x1c, (uintptr_t)air_remove);
    write32(main_table + 0x6c, (uintptr_t)state_find);
    for (i = 0; i < 2; ++i) {
        uint32_t side = first ^ i;
        struct side *instance = sides + side;
        instance->module = LoadLibraryA(filenames[side]);
        if (!instance->module) fail("AIR load SIM1", GetLastError());
        instance->image = (uint8_t *)GetProcAddress(instance->module, "ImportTable");
        instance->link = (uint32_t *)GetProcAddress(instance->module, "Linkage");
        if (!instance->image || (uint8_t *)instance->link != instance->image + 0x58 || instance->link[0] != 25)
            fail("AIR export layout", i);
        if (((uintptr_t)instance->module == 0x22000000) != (!reservation && i == 0)) fail("AIR load placement", i);
        write32(instance->image + 4, (uintptr_t)host_table);
        write32(instance->image + 12, (uintptr_t)global);
        write32(instance->image + 20, (uintptr_t)acontain_table);
        write32(instance->image + 28, (uintptr_t)chain_table);
        write32(instance->image + 44, (uintptr_t)main_table);
        write32(instance->image + 68, (uintptr_t)util_link);
        air_entries[side][0] = side ? (uintptr_t)GetProcAddress(instance->module, "Sim1LoadAir") : (uintptr_t)instance->module + 0xae90;
        air_entries[side][1] = side ? (uintptr_t)GetProcAddress(instance->module, "Sim1Publish") : (uintptr_t)instance->module + 0x10c40;
        for (j = 0; j < 2; ++j) if (!owns_code(instance->module, air_entries[side][j])) fail("AIR helper ownership", j);
        for (j = 0; j < 3; ++j) {
            callbacks[side][j] = side ? (uintptr_t)GetProcAddress(instance->module, callback_names[j]) : (uintptr_t)instance->module + callback_rvas[j];
            if (!owns_code(instance->module, callbacks[side][j])) fail("AIR callback ownership", j);
        }
        if (!owns_code(instance->module, instance->link[1]) || !owns_code(instance->module, instance->link[2]))
            fail("AIR lifecycle ownership", 0);
        memcpy(instance->pristine, instance->image, DATA_SIZE);
    }
    if (sides[0].module == sides[1].module) fail("AIR same implementation", 0);
    parser_fixture = (uintptr_t *)GetProcAddress(sides[1].module, "Sim1AirParserFixture");
    if (!parser_fixture) fail("AIR decoder test fixture missing", 0);
    *parser_fixture = (uintptr_t)air_parser;
    patch_decoder(1);
    for (vector = 0; vector < count; ++vector) {
        seed_case();
        fill(global_initial, sizeof global_initial);
        fill(heap_initial, sizeof heap_initial);
        fill(record_initial, sizeof record_initial);
        fill(air_data, sizeof air_data);
        fill(air_buffer_initial, sizeof air_buffer_initial);
        {
            static const uint32_t scales[] = {0x678, 0x680, 0x690, 0x6d8, 0x6e0, 0x720, 0x728};
            for (i = 0; i < 7; ++i) memcpy(seeded + scales[i], sides[0].pristine + scales[i], 8);
        }
        if (vector < 144) {
            memcpy(seeded + 0x13b0, seeded + 0x640, 8);
            /* Decoder outputs must differ from pre-load state, while keeping
             * the directed FP encodings. Equal input/output would hide early
             * descriptor publication in the initializer. */
            memcpy(air_data, seeded + 0x13c0, 16);
            memcpy(air_data + 16, seeded + 0x13b0, 16);
        }
        old_allocation = (vector / 27) % 2;
        write32(global_initial + 0x11d8, old_allocation ? (uintptr_t)(heap + 0x10) : 0);
        write32(global_initial + 0x604, vector % 3);
        global_initial[0x609] = (vector / 3) % 3;
        write32(seeded + 0x1778, (vector / 9) % 3);
        find_mode = (vector / 54) % 4;
        find_length = vector % 9 == 8 ? 0x58 : 0xc0;
        air_failure = vector % 5;
        air_size_value = lengths[(vector / 5) % 11];
        air_result = vector & 1;
        memset(air_path, 'A' + vector % 26, paths[(vector / 55) % 4]);
        air_path[paths[(vector / 55) % 4]] = 0;
        for (mode = 0; mode < 12; ++mode)
            for (air_operation = 0; air_operation < 4; ++air_operation) air_case();
        ++air_cases[air_failure][(vector / 5) % 11];
    }
    for (i = 0; i < 4; ++i) printf("SIM1 AIR boundary %u: %u cases, 0 differences\n", i, air_operations[i]);
    for (i = 0; i < 5; ++i) for (j = 0; j < 11; ++j)
        if (!air_cases[i][j]) fail("missing AIR I/O outcome/size", i * 11 + j);
    puts("PASS: AIR file wrapper/init/descriptor; decoder explicitly modeled");
    patch_decoder(0);
    FreeLibrary(sides[1].module); FreeLibrary(sides[0].module); FreeLibrary(util);
    if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    return 0;
}
