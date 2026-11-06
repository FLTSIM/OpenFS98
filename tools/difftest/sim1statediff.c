/* Reuse only comparison/ABI utilities; this main exercises new dependency
 * models and private helper exports, independently of the six-service suite.
 */
#define main sim1_service_reference_main
#include "sim1diff.c"
#undef main

static uint8_t global[0x2000], global_initial[0x2000], global_expected[0x2000];
static uint8_t heap[0x300], heap_initial[0x300], heap_expected[0x300];
static uint8_t host_table[0x240], main_table[0x80], chain_table[0x24];
static uint8_t record_header[16];
static uint8_t input_record[256], record_initial[256];
static uint8_t input_expected[256];
static uint8_t written[0xc0], written_expected[0xc0];
#ifndef SIM1_TRANSCRIPT_CAPACITY
#define SIM1_TRANSCRIPT_CAPACITY 16
#endif
static uint32_t calls[SIM1_TRANSCRIPT_CAPACITY][5], calls_expected[SIM1_TRANSCRIPT_CAPACITY][5], call_count, expected_call_count;
static uint32_t callbacks[2][3], helpers[2][8], begin_result, write_result;
static uint32_t operation, operation_counts[15], gate_counts[54], old_allocation;
static uint32_t registers_expected[8];
static uint32_t reader_modes[3], reader_sizes[8], velocity_oracles[2];
static uint32_t find_mode, find_length, restore_paths[5];

uint32_t __stdcall state_begin(uint32_t tag, uint32_t size);
uint32_t __stdcall state_write(const void *data, uint32_t size, uint32_t *written_out);
uint32_t __stdcall state_alloc(uint32_t count, uint32_t size, void **out, uint32_t flags);
uint32_t __stdcall state_free(void **out);
uint32_t __stdcall state_remove(uint32_t list, uintptr_t callback);
uint32_t __stdcall state_find(uint32_t tag, uintptr_t *out, uint32_t *size);

static void legacy_fixture(uint8_t *record)
{
    static const uint32_t edges[] = {0, 1, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xffffffff, 0x80000001};
    static const uint32_t angle_lows[] = {0x32, 0x30, 0x34};
    static const uint16_t rates[] = {0, 1, 0x7fff, 0x8000, 0xffff};
    uint32_t i, value;
    uint16_t word;
    for (i = 0; i < 3; ++i) {
        value = edges[(vector + 3 * i) % 8];
        word = value >> 16; memcpy(record + 2 * i, &word, 2);
        word = value; memcpy(record + angle_lows[i], &word, 2);
        value = edges[(vector + 2 * i + 1) % 8];
        word = value >> 16; memcpy(record + 0xc + 2 * i, &word, 2);
        word = value; memcpy(record + 0x12 + 2 * i, &word, 2);
        word = rates[(vector + i) % 5]; memcpy(record + 6 + 2 * i, &word, 2);
    }
    write32(record + 0x3c, edges[(vector + 5) % 8]);
}

static uint32_t *event(uint32_t kind)
{
    uint32_t *row;
    if (call_count >= SIM1_TRANSCRIPT_CAPACITY) fail("import transcript overflow", call_count);
    row = calls[call_count++];
    row[0] = kind;
    return row;
}

uint32_t __stdcall state_begin_core(uint32_t tag, uint32_t size)
{
    uint32_t *row = event(1);
    row[1] = tag; row[2] = size;
    if (tag != 0x38e || size != 0xc0) fail("MAIN record header", tag);
    return begin_result;
}

uint32_t __stdcall state_write_core(const void *data, uint32_t size, uint32_t *written_out)
{
    uint32_t *row = event(2);
    row[1] = size; row[2] = written_out != NULL;
    if (size != sizeof written || written_out) fail("MAIN write arguments", size);
    memcpy(written, data, sizeof written);
    return write_result;
}

uint32_t __stdcall state_alloc_core(uint32_t count, uint32_t size, void **out, uint32_t flags)
{
    uint32_t *row = event(3);
    row[1] = count; row[2] = size; row[3] = flags;
    if (count != 1 || size != 0xc0 || flags != 1 || (uint8_t *)out != global + 0x11d8)
        fail("HOST allocation arguments", size);
    if (read32(out)) fail("allocation slot not cleared", read32(out));
    *out = heap + 0x100;
    memset(*out, 0, 0xc0);
    return 0;
}

uint32_t __stdcall state_free_core(void **out)
{
    uint32_t *row = event(4);
    if (*out == heap + 0x200 && (uint8_t *)out != global + 0x11d8) {
        row[1] = 0xffff; row[2] = 0x200;
        *out = NULL;
        return 0;
    }
    if ((uint8_t *)out != global + 0x11d8 || (*out != heap + 0x10 && *out != heap + 0x100))
        fail("HOST free ownership", 0);
    row[1] = 0x11d8; row[2] = (uint8_t *)*out - heap;
    *out = NULL;
    return 0;
}

uint32_t __stdcall state_find_core(uint32_t tag, uintptr_t *out, uint32_t *size)
{
    uint32_t *row = event(6);
    row[1] = tag;
    if (tag != 0x38e && tag != 0x5a) fail("MAIN find tag", tag);
    if (!(find_mode & (tag == 0x38e ? 1 : 2))) return 0xffffffff;
    memcpy(heap + 0x200, record_initial + 16 + (vector & 3), 0xc0);
    if (tag == 0x5a && vector < 216) legacy_fixture(heap + 0x200);
    *out = (uintptr_t)(heap + 0x200);
    *size = tag == 0x38e ? find_length : 0x40;
    row[2] = *size;
    return 0;
}

uint32_t __stdcall state_remove_core(uint32_t list, uintptr_t callback)
{
    uint32_t i, *row = event(5);
    for (i = 0; i < 3; ++i) if (callback == callbacks[role][i]) break;
    if (i == 3 || list != (i == 0 ? 1u : i == 1 ? 5u : 0x14u))
        fail("CHAIN callback identity/list", list);
    row[1] = list; row[2] = i;
    return 0x100 + i; /* retail ignores removal failure as well as success */
}

static void state_case(void)
{
    static const uint32_t precision[] = {0, 2, 3};
    uint32_t index, expected_registers[8];
    uint32_t offset = 16 + (vector & 3), layout = vector % 7;
    static const uint32_t sizes[] = {0, 0x54, 0x55, 0x58, 0xbf, 0xc1, 0xffffffff, 0x80000000};
    slot = operation == 4 ? 8 : 0x110a0 + operation;
    call_control = 0x007f | (precision[mode / 4] << 8) | ((mode % 4) << 10);
    call_preload = vector & 1;
    for (index = 0; index < 7; ++index)
        call_inputs[index] = read32(initial_output + 128 + index * 4);
    for (role = 0; role < 2; ++role) {
        uint8_t *image = sides[role].image;
        memcpy(image, sides[role].pristine, DATA_SIZE);
        memcpy(image + HEADER_SIZE, seeded + HEADER_SIZE, DATA_SIZE - HEADER_SIZE);
        memcpy(output, initial_output, sizeof output);
        memcpy(input_record, record_initial, sizeof input_record);
        if ((operation == 9 || operation == 10) && vector < 216)
            legacy_fixture(input_record + offset);
        memcpy(input_expected, input_record, sizeof input_expected);
        memcpy(global, global_initial, sizeof global);
        memcpy(heap, heap_initial, sizeof heap);
        memset(calls, 0, sizeof calls); call_count = 0;
        memset(written, 0x5a, sizeof written);
        call_argc = operation == 0 || operation == 9 ? 1 :
                    operation == 3 || operation == 5 || operation == 6 ||
                    (operation >= 10 && operation <= 12) ? 2 : operation == 13 ? 9 : 0;
        call_target = operation == 4 || operation == 8 ? sides[role].link[2] :
                      helpers[role][operation == 5 || operation == 11 ? 4 :
                                    operation == 6 || operation == 10 || operation == 12 ? 3 :
                                    operation == 7 ? 2 : operation == 9 ? 5 : operation == 13 ? 6 : operation == 14 ? 7 : operation];
        call_args[0] = (uintptr_t)(output + offset);
        if (operation == 0 && layout >= 2)
            call_args[0] = (uintptr_t)(image + 0x1538 + layout - 4);
        if (operation == 0 && layout == 1)
            call_args[0] = (uintptr_t)(global + 0x320); /* overlaps GLOBAL +366 */
        if (operation == 3) {
            write32(record_header, vector & 1 ? 0x5b : 0x38f);
            call_args[0] = vector % 3 == 0 ? 0x23 : vector % 3 == 1 ? 0x22 : 0x99;
            call_args[1] = (uintptr_t)record_header;
        }
        if (operation == 5 || operation == 6 || (operation >= 9 && operation <= 12)) {
            uint8_t header_before[16];
            uintptr_t record = layout >= 2 ? (uintptr_t)(image + 0x1340 + layout - 4) :
                                           (uintptr_t)(input_record + offset);
            uint32_t size = operation == 11 || operation == 12 ? sizes[(vector / 3) % 8] : 0xc0;
            if ((operation == 9 || operation == 10) && layout >= 2)
                record = (uintptr_t)(image + (layout & 1 ? 0x1538 : 0x13e8) + layout - 4);
            if (operation == 5 || operation == 9 || operation == 11) {
                call_args[0] = record; call_args[1] = size;
            } else {
                write32(record_header, operation == 10 ? 0x5a : 0x38e);
                write32(record_header + 8, operation == 10 ? sizes[(vector / 3) % 8] : size);
                write32(record_header + 12, record);
                call_args[0] = 0x22; call_args[1] = (uintptr_t)record_header;
            }
            memcpy(header_before, record_header, sizeof header_before);
            sim1_call();
            compare(header_before, record_header, sizeof header_before, "read header changed");
        } else if (operation == 13) {
            static const double identity[6] = {1, 0, 1, 0, 1, 0};
            static const double input[3] = {2, 3, 5};
            static const double rotated[6] = {0.6, 0.8, -0.8, 0.6, 0, 1};
            static const double rounding[3] = {0x1.0000000000001p+0, -0x1.fffffffffffffp+0, 0x1.0000000000001p-52};
            for (index = 0; index < 6; ++index)
                call_args[index] = read32(input_record + offset + 4 * index);
            for (index = 0; index < 3; ++index)
                call_args[index + 6] = (uintptr_t)(output + offset + 16 * index);
            if (layout == 1) call_args[7] = call_args[8] = call_args[6];
            if (layout == 2) { call_args[7] = call_args[6] + 1; call_args[8] = call_args[6] + 2; }
            if (layout >= 3)
                for (index = 0; index < 3; ++index)
                    call_args[index + 6] = (uintptr_t)(image + 0x1560 +
                        (layout == 4 ? 8 : layout == 6 ? 1 : 0) + 16 * index);
            if (vector < 21) {
                memcpy(image + 0x1560, identity, sizeof identity);
                for (index = 0; index < 6; ++index) call_args[index] = read32((const uint8_t *)input + 4 * index);
            } else if (vector < 144) {
                memcpy(image + 0x1560, rotated, sizeof rotated);
                for (index = 0; index < 6; ++index) call_args[index] = read32((const uint8_t *)rounding + 4 * index);
            }
            sim1_call();
            if (call_registers[0] != call_args[6] || call_registers[2] != call_args[7] || call_registers[3] != call_args[8])
                fail("velocity pointer registers", 0);
            if (vector < 21 && layout == 0) {
                const double expected[3] = {vector % 3 == 1 ? 2 : 3, vector % 3 == 1 ? 3 : -2, 5};
                for (index = 0; index < 3; ++index)
                    compare(expected + index, output + offset + 16 * index, 8, "velocity identity oracle");
                if (role) ++velocity_oracles[vector % 3 == 1];
            }
        } else {
            sim1_call();
            if (operation == 7 || operation == 8) sim1_call();
        }
        /* Nonvolatile register and stack obligations are independent oracles. */
        for (index = 0; index < 7; ++index)
            if ((index == 1 || index >= 4) && call_registers[index] != call_inputs[index])
                fail("state preserved register", index);
        if (call_registers[7]) fail("state ESP", call_registers[7]);
        memcpy(expected_registers, call_registers, sizeof expected_registers);
        /* Only the writer's EAX is known to be an image pointer. Arbitrary
         * numeric state/register values must never be normalized by range.
         */
        if (operation == 0 && layout >= 2)
            expected_registers[0] -= (uintptr_t)image;
        if (operation == 13 && layout >= 3) {
            expected_registers[0] -= (uintptr_t)image;
            expected_registers[2] -= (uintptr_t)image;
            expected_registers[3] -= (uintptr_t)image;
        }
        if (!role) {
            memcpy(expected_output, output, sizeof output);
            memcpy(expected_image, image, DATA_SIZE);
            memcpy(expected_fp, call_fp, sizeof call_fp);
            memcpy(global_expected, global, sizeof global);
            memcpy(heap_expected, heap, sizeof heap);
            memcpy(calls_expected, calls, sizeof calls);
            memcpy(written_expected, written, sizeof written);
            memcpy(registers_expected, expected_registers, sizeof registers_expected);
            expected_call_count = call_count;
        } else {
            compare(registers_expected, expected_registers, sizeof registers_expected, "state integer registers");
            compare(expected_output, output, sizeof output, "state output/guards");
            compare(expected_image + HEADER_SIZE, image + HEADER_SIZE,
                    DATA_SIZE - HEADER_SIZE, "state image");
            compare(global_expected, global, sizeof global, "GLOBAL");
            compare(heap_expected, heap, sizeof heap, "retained heap/guards");
            compare(calls_expected, calls, sizeof calls, "import transcript");
            if (expected_call_count != call_count) fail("import count", call_count);
            compare(written_expected, written, sizeof written, "saved record");
            compare_fp();
        }
        compare(sides[role].pristine, image, HEADER_SIZE, "state import/Linkage prefix");
        compare(input_expected, input_record, sizeof input_record, "input record/guards changed");
    }
    ++operation_counts[operation];
    if (operation == 9) ++reader_modes[vector % 3];
    if (operation == 11) ++reader_sizes[(vector / 3) % 8];
    if (operation == 14)
        ++restore_paths[old_allocation ? 0 : find_mode & 1 ? (find_length == 0xc0 ? 1 : 2) :
                        find_mode & 2 ? 3 : 4];
}

int sim1_state_main(int argc, char **argv)
{
    static const char *names[] = {"Sim1RecordWrite", "Sim1SaveSituation", "Sim1RetainState", "Sim1SituationCallback", "Sim1RecordRead", "Sim1LegacyRead", "Sim1Velocity", "Sim1RestoreState"};
    static const uint32_t helper_rvas[] = {0x110a0, 0x11050, 0x10d50, 0x10da0, 0x11290, 0x10df0, 0x2ac0, 0x10cc0};
    static const char *callback_names[] = {"Sim1TickCallback", "Sim1FrameCallback", "Sim1SituationCallback"};
    static const uint32_t callback_rvas[] = {0x1020, 0x1040, 0x10da0};
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 20000;
    const char *order = argc > 2 ? argv[2] : "retail-first";
    const char *ours = argc > 3 ? argv[3] : "bin/sim1-diff/SIM1_STATE_TEST.DLL";
    const char *paths[2] = {"orig/Sim/SIM1.sim", ours};
    uint32_t first = !strcmp(order, "ours-first"), i, j;
    void *reservation = NULL;
    HMODULE util;
    uint32_t *util_link;
    if (!count || (strcmp(order, "retail-first") && strcmp(order, "ours-first") && strcmp(order, "relocate"))) return 2;
    SetUnhandledExceptionFilter(fault);
    util = LoadLibraryA("orig/Modules/UTIL.DLL");
    if (!util) fail("load retail UTIL", GetLastError());
    util_link = (uint32_t *)GetProcAddress(util, "Linkage");
    if (!util_link || util_link[0] != 20 || !owns_code(util, util_link[0x3c / 4]))
        fail("retail UTIL atan ownership", 0);
    if (!strcmp(order, "relocate")) {
        reservation = VirtualAlloc((void *)0x22000000, 0x40000, MEM_RESERVE, PAGE_NOACCESS);
        if (!reservation) fail("state reserve preferred base", GetLastError());
    }
    write32(host_table + 0x20c, (uintptr_t)state_alloc);
    write32(host_table + 0x214, (uintptr_t)state_free);
    write32(main_table + 0x5c, (uintptr_t)state_begin);
    write32(main_table + 0x60, (uintptr_t)state_write);
    write32(main_table + 0x6c, (uintptr_t)state_find);
    write32(chain_table + 0x1c, (uintptr_t)state_remove);
    for (i = 0; i < 2; ++i) {
        uint32_t side = first ^ i;
        struct side *instance = sides + side;
        instance->module = LoadLibraryA(paths[side]);
        if (!instance->module) fail("state load module", GetLastError());
        instance->image = (uint8_t *)GetProcAddress(instance->module, "ImportTable");
        instance->link = (uint32_t *)GetProcAddress(instance->module, "Linkage");
        if (!instance->image || (uint8_t *)instance->link != instance->image + 0x58 || instance->link[0] != 25)
            fail("state export layout", i);
        if (((uintptr_t)instance->module == 0x22000000) != (!reservation && i == 0))
            fail("state load placement", i);
        write32(instance->image + 4, (uintptr_t)host_table);
        write32(instance->image + 12, (uintptr_t)global);
        write32(instance->image + 28, (uintptr_t)chain_table);
        write32(instance->image + 44, (uintptr_t)main_table);
        write32(instance->image + 68, (uintptr_t)util_link);
        for (j = 0; j < 8; ++j) {
            helpers[side][j] = side ? (uintptr_t)GetProcAddress(instance->module, names[j]) :
                                      (uintptr_t)instance->module + helper_rvas[j];
            if (!owns_code(instance->module, helpers[side][j])) fail("state helper ownership", j);
        }
        for (j = 0; j < 3; ++j) {
            callbacks[side][j] = side ? (uintptr_t)GetProcAddress(instance->module, callback_names[j]) :
                                        (uintptr_t)instance->module + callback_rvas[j];
            if (!owns_code(instance->module, callbacks[side][j])) fail("state callback ownership", j);
        }
        if (!owns_code(instance->module, instance->link[2])) fail("deinit ownership", 0);
        memcpy(instance->pristine, instance->image, DATA_SIZE);
    }
    if (sides[0].module == sides[1].module) fail("state same implementation", 0);
    for (vector = 0; vector < count; ++vector) {
        seed_case();
        fill(record_initial, sizeof record_initial);
        /* Directed first-double cases must reach the writer's x87 store too. */
        if (vector < 144) {
            memcpy(seeded + 0x1538, seeded + 0x640, 8);
            for (i = 0; i < 3; ++i)
                memcpy(record_initial + 16 + (vector & 3) + 8 * i, seeded + 0x13b8 + 8 * i, 8);
        }
        /* The reader's scales are actual mutable module constants. Seed the
         * rest of state freely, but preserve these for useful finite angles.
         */
        {
            static const uint32_t scales[] = {0x678, 0x680, 0x690, 0x6d8, 0x6e0, 0x720};
            for (i = 0; i < 6; ++i)
                memcpy(seeded + scales[i], sides[0].pristine + scales[i], 8);
        }
        write32(seeded + 0x7c0, vector % 3);
        fill(global_initial, sizeof global_initial);
        fill(heap_initial, sizeof heap_initial);
        old_allocation = (vector / 27) % 2;
        write32(global_initial + 0x11d8, old_allocation ? (uintptr_t)(heap + 0x10) : 0);
        write32(global_initial + 0x604, vector % 3);
        global_initial[0x609] = (vector / 3) % 3;
        write32(seeded + 0x1778, (vector / 9) % 3);
        begin_result = vector % 4 == 0 ? 0 : vector % 4 == 1 ? 1 : vector % 4 == 2 ? 0xffffffff : 0x80000000;
        find_mode = (vector / 54) % 4;
        find_length = vector % 9 == 8 ? 0x58 : 0xc0;
        write_result = read32(initial_output + 96);
        write32(record_header, vector & 1 ? 0x5b : 0x38f);
        for (mode = 0; mode < 12; ++mode)
            for (operation = 0; operation < 15; ++operation) state_case();
        ++gate_counts[read32(global_initial + 0x604) + 3 * global_initial[0x609] +
                      9 * read32(seeded + 0x1778) + 27 * old_allocation];
    }
    for (i = 0; i < 15; ++i) printf("SIM1 state operation %u: %u cases, 0 differences\n", i, operation_counts[i]);
    for (i = 0; i < 54; ++i) if (!gate_counts[i]) fail("missing ownership/reset gate", i);
    for (i = 0; i < 3; ++i) if (!reader_modes[i]) fail("missing reader transform convention", i);
    for (i = 0; i < 8; ++i) if (!reader_sizes[i]) fail("missing modern fallback size", i);
    if (!velocity_oracles[0] || !velocity_oracles[1]) fail("missing velocity identity convention", 0);
    for (i = 0; i < 5; ++i) if (!restore_paths[i]) fail("missing restore path", i);
    printf("PASS: state save/retain/deinit, %u scenarios, 12 x87 modes (%s)\n", count, order);
    FreeLibrary(sides[1].module); FreeLibrary(sides[0].module);
    FreeLibrary(util);
    if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    return 0;
}

#ifndef SIM1_STATE_REFERENCE_ONLY
int main(int argc, char **argv) { return sim1_state_main(argc, argv); }
#endif
