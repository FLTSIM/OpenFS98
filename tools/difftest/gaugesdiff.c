/* Gauge comparison: both real DLLs, all callbacks, complete data and bitmaps.
 * PANELS stubs deliberately change roots, imports, contexts and return values.
 * Integration through real PANELS is a separate target. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SUCTION_DIFF
#define DATA_SIZE 0x298
#define HEADER 0x18
#define IMPORTS 0x260
#define LINKAGE 0x270
#define ROOT 0x1d0
#define ELEMENT 0x180
#define CODE_COUNT 9
#define SERVICE_COUNT 10
#define PREFERRED_BASE 0x32010000u
#define GAUGE_NAME "CESSSUC"
#define RETAIL_PATH "orig/Gauges/CESSSUC.GAU"
#define OURS_PATH "bin/gauges-diff/CESSNA_182.SUCTION.GAU"
#else
#define DATA_SIZE 0xfc
#define HEADER 0x20
#define IMPORTS 0xc8
#define LINKAGE 0xd8
#define ROOT 0xc0
#define ELEMENT 0x70
#define CODE_COUNT 8
#define SERVICE_COUNT 7
#define PREFERRED_BASE 0x32000000u
#define GAUGE_NAME "737WARNG"
#define RETAIL_PATH "orig/Gauges/737WARNG.GAU"
#define OURS_PATH "bin/gauges-diff/737-400.WARNING_ENUNCIATORS.GAU"
#endif
typedef uint32_t (__stdcall *fn0)(void);
typedef uint32_t (__stdcall *fn1)(uint32_t);

struct side {
    HMODULE module;
    uint32_t *linkage;
    uint8_t *data;
    uint8_t pristine[DATA_SIZE];
    uint32_t code[CODE_COUNT];
};

static struct side sides[2];
static uint32_t panels[2][0xac / 4];
static uint32_t root_word;
static uint32_t expected_root, expected_context, result;
static unsigned calls, called_slot;
static int bad_call;
static uint32_t random_state = 0x7371997;
#ifdef SUCTION_DIFF
static struct side *active_side;
static unsigned active_table, expected_calls;
static unsigned expected_slots[3];
static uint32_t expected_args[3][2], expected_point;
#endif

static uint32_t random_word(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static uint32_t record(unsigned slot, uint32_t root, uint32_t context)
{
#ifdef SUCTION_DIFF
    if (calls >= expected_calls || slot != expected_slots[calls] ||
        root != expected_args[calls][0] || context != expected_args[calls][1])
        bad_call = 1;
    if (slot == 0x64) {
        /* The next two calls must reload PANELS, and +0x80 must use the
         * post-install point address rather than a cached one. */
        uint32_t point = expected_context ^ 0xdeadbeefu;
        memcpy(active_side->data + 0x1bc, &point, 4);
    }
    if (slot == 0x88) {
        uint32_t root_after = expected_root ^ 0x3c3c3c3cu;
        memcpy(active_side->data + ROOT, &root_after, 4);
    }
    if (slot == 0x64 || slot == 0x80 || slot == 0x88) {
        uint32_t import;
        memcpy(panels[active_table ^ 1], panels[active_table], sizeof panels[0]);
        memset(panels[active_table], 0, sizeof panels[0]);
        active_table ^= 1;
        import = (uint32_t)(uintptr_t)panels[active_table];
        memcpy(active_side->data + IMPORTS + 4, &import, 4);
    }
#else
    if (root != expected_root || context != expected_context)
        bad_call = 1;
#endif
    calls++;
    called_slot = slot;
#ifdef SUCTION_DIFF
    return result ^ slot;
#else
    return result;
#endif
}

static uint32_t __stdcall failures(uint32_t root) { return record(0x60, root, 0); }
static uint32_t __stdcall install(uint32_t root, uint32_t context) { return record(0x64, root, context); }
static uint32_t __stdcall unmark(uint32_t root) { return record(0x68, root, 0); }
static uint32_t __stdcall prepare(uint32_t root) { return record(0x6c, root, 0); }
static uint32_t __stdcall update(uint32_t root, uint32_t context) { return record(0x70, root, context); }
static uint32_t __stdcall draw(uint32_t root) { return record(0x74, root, 0); }
static uint32_t __stdcall release_tree(uint32_t root) { return record(0x7c, root, 0); }
#ifdef SUCTION_DIFF
static uint32_t __stdcall mouse_place(uint32_t mouse, uint32_t header, uint32_t point)
{
    if (point != expected_point)
        bad_call = 1;
    return record(0x80, mouse, header);
}
static uint32_t __stdcall mouse_add(uint32_t mouse, uint32_t header) { return record(0x84, mouse, header); }
static uint32_t __stdcall mouse_remove(uint32_t mouse, uint32_t header) { return record(0x88, mouse, header); }
#endif

static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "gauge fault %#lx at %p\n", exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(2);
    return EXCEPTION_EXECUTE_HANDLER;
}

static uint32_t word(const void *data, unsigned offset)
{
    uint32_t value;
    memcpy(&value, (const uint8_t *)data + offset, sizeof value);
    return value;
}

static void set_word(struct side *side, unsigned offset, uint32_t value)
{
    memcpy(side->data + offset, &value, sizeof value);
}

static uint32_t normalize(const struct side *side, uint32_t value)
{
    unsigned index;
    for (index = 0; index < CODE_COUNT; index++)
        if (value == side->code[index])
            return 0xf0000000u + index;
    if (value >= (uint32_t)(uintptr_t)side->data &&
        value < (uint32_t)(uintptr_t)side->data + DATA_SIZE)
        return 0xe0000000u + value - (uint32_t)(uintptr_t)side->data;
    return value;
}

static int compare_data(unsigned scenario, unsigned operation)
{
    unsigned offset;
    for (offset = 0; offset < DATA_SIZE; offset += 4) {
        /* Equal raw values need no pointer interpretation, even when one
         * happens to coincide with a relocated image's address. */
        if (word(sides[0].data, offset) == word(sides[1].data, offset))
            continue;
        uint32_t retail = normalize(&sides[0], word(sides[0].data, offset));
        uint32_t ours = normalize(&sides[1], word(sides[1].data, offset));
        if (retail != ours) {
            fprintf(stderr, "scenario %u op %u .data+%#x: %#x != %#x\n",
                    scenario, operation, offset, retail, ours);
            return 0;
        }
    }
    return 1;
}

static int load_side(struct side *side, const char *path)
{
    unsigned index;
    side->module = LoadLibraryA(path);
    if (!side->module) {
        fprintf(stderr, "cannot load %s: %lu\n", path, GetLastError());
        return 0;
    }
    side->linkage = (uint32_t *)GetProcAddress(side->module, "Linkage");
    if (!side->linkage || side->linkage[0] != 19)
        return 0;
    side->data = (uint8_t *)side->linkage - LINKAGE;
    if (GetProcAddress(side->module, "ImportTable") != (FARPROC)(side->data + IMPORTS) ||
        word(side->data, IMPORTS) != 15 || word(side->data, IMPORTS + 4) != 0 ||
        word(side->data, IMPORTS + 8) != 0 || word(side->data, IMPORTS + 12) != 0)
        return 0;
    for (index = 0; index < 7; index++)
        side->code[index] = word(side->data, HEADER + 0xc + index * 4);
    side->code[7] = side->linkage[1];
#ifdef SUCTION_DIFF
    side->code[8] = word(side->data, 0xf4);
#endif
    if (!side->code[7] || side->linkage[1] != side->linkage[2])
        return 0;
    /* Independent schema assertions: the whole-image comparison handles the rest. */
    if (word(side->data, LINKAGE + 0x18) != (uint32_t)(uintptr_t)(side->data + HEADER) ||
        word(side->data, HEADER + 8) != (uint32_t)(uintptr_t)(side->data + ROOT) ||
        word(side->data, ROOT) != (uint32_t)(uintptr_t)(side->data + ELEMENT))
        return 0;
    memcpy(side->pristine, side->data, DATA_SIZE);
    return 1;
}

static int compare_bitmap(void)
{
    HRSRC resources[2];
    const void *pixels[2];
    DWORD sizes[2];
    unsigned side, index;
#ifdef SUCTION_DIFF
    const DWORD expected_sizes[] = {0x16ac8, 0x1148};
#else
    const DWORD expected_sizes[] = {0x533e};
#endif
    for (index = 0; index < sizeof expected_sizes / sizeof expected_sizes[0]; index++) {
        for (side = 0; side < 2; side++) {
            resources[side] = FindResourceExA(sides[side].module, RT_BITMAP,
                                            MAKEINTRESOURCEA(0x1000 + index * 0x1000), 0x409);
            if (!resources[side])
                return 0;
            sizes[side] = SizeofResource(sides[side].module, resources[side]);
            pixels[side] = LockResource(LoadResource(sides[side].module, resources[side]));
            if (!pixels[side])
                return 0;
        }
        if (sizes[0] != expected_sizes[index] || sizes[0] != sizes[1] ||
            memcmp(pixels[0], pixels[1], sizes[0]) != 0)
            return 0;
    }
    return 1;
}

#ifdef SUCTION_DIFF
#include "suction_numeric.inc"
#endif

int main(int argc, char **argv)
{
    unsigned count = argc > 1 ? (unsigned)strtoul(argv[1], NULL, 0) : 20000;
    unsigned scenario, operation, side, checks = 0;
    const unsigned slots[SERVICE_COUNT] = {0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x7c
#ifdef SUCTION_DIFF
        , 0x80, 0x84, 0x88
#endif
    };
    void *reservation = NULL;
    const uint32_t callbacks[SERVICE_COUNT] = {
        (uint32_t)(uintptr_t)failures, (uint32_t)(uintptr_t)install,
        (uint32_t)(uintptr_t)unmark, (uint32_t)(uintptr_t)prepare,
        (uint32_t)(uintptr_t)update, (uint32_t)(uintptr_t)draw,
        (uint32_t)(uintptr_t)release_tree
#ifdef SUCTION_DIFF
        , (uint32_t)(uintptr_t)mouse_place, (uint32_t)(uintptr_t)mouse_add,
        (uint32_t)(uintptr_t)mouse_remove
#endif
    };
    if (!count || count > 1000000)
        return 2;
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    SetUnhandledExceptionFilter(fault);
    if (getenv("GAUGES_RELOCATE")) {
        reservation = VirtualAlloc((void *)(uintptr_t)PREFERRED_BASE, 0x10000,
                                   MEM_RESERVE, PAGE_NOACCESS);
        if (reservation != (void *)(uintptr_t)PREFERRED_BASE) {
            fprintf(stderr, "cannot reserve gauge preferred base for relocation test\n");
            return 2;
        }
    }
    if (!load_side(&sides[0], RETAIL_PATH) ||
        !load_side(&sides[1], argc > 2 ? argv[2] : OURS_PATH)) {
        fprintf(stderr, "gauge ABI check failed\n");
        return 2;
    }
    if (sides[0].module == sides[1].module ||
        !compare_data(0, 0) || !compare_bitmap()) {
        fprintf(stderr, "initial data/bitmap comparison failed\n");
        return 1;
    }
    if (reservation && (uint32_t)(uintptr_t)sides[1].module == PREFERRED_BASE)
        return 2;
    for (operation = 0; operation < SERVICE_COUNT; operation++)
        panels[0][slots[operation] / 4] = panels[1][slots[operation] / 4] = callbacks[operation];
    for (scenario = 0; scenario < count; scenario++) {
        for (side = 0; side < 2; side++)
            memcpy(sides[side].data, sides[side].pristine, DATA_SIZE);
        /* All seven callbacks and both void Linkage entries in every scenario. */
        for (operation = 0; operation < 9; operation++) {
            uint32_t context = scenario < 4 ? (uint32_t[]){0, 1, 0x80000000, 0xffffffff}[scenario] : random_word();
            uint32_t root = scenario < 4 ? context : random_word();
            unsigned table = (scenario + operation) & 1;
            result = random_word();
            for (side = 0; side < 2; side++) {
                struct side *current = &sides[side];
                uint32_t returned;
                /* Poison the unused import table: a cached PANELS link is observable. */
                unsigned other;
                for (other = 0; other < SERVICE_COUNT; other++)
                    panels[table ^ 1][slots[other] / 4] = 0;
                for (other = 0; other < SERVICE_COUNT; other++)
                    panels[table][slots[other] / 4] = callbacks[other];
                set_word(current, IMPORTS + 4, (uint32_t)(uintptr_t)panels[table]);
                set_word(current, ROOT, root);
                root_word = root ^ 0xa5a5a5a5u;
                set_word(current, HEADER + 8, scenario & 1 ? (uint32_t)(uintptr_t)&root_word :
                         (uint32_t)(uintptr_t)(current->data + ROOT));
                calls = called_slot = 0;
                bad_call = 0;
                expected_root = operation == 0 && (scenario & 1) ? root_word : root;
                expected_context = operation == 1 || operation == 4 ? context : 0;
#ifdef SUCTION_DIFF
                active_side = current;
                active_table = table;
                expected_calls = operation == 1 ? 3 : operation == 6 ? 2 : 1;
                expected_slots[0] = slots[operation < 7 ? operation : 0];
                expected_args[0][0] = expected_root;
                expected_args[0][1] = expected_context;
                if (operation == 1 || operation == 6) {
                    unsigned first_mouse = operation == 1 ? 1 : 0;
                    unsigned last_mouse = operation == 1 ? 2 : 0;
                    unsigned call;
                    for (call = first_mouse; call <= last_mouse; call++) {
                        expected_slots[call] = operation == 6 ? 0x88 : call == 1 ? 0x80 : 0x84;
                        expected_args[call][0] = (uint32_t)(uintptr_t)(current->data + 0x1d8);
                        expected_args[call][1] = (uint32_t)(uintptr_t)(current->data + HEADER);
                    }
                    if (operation == 6) {
                        expected_slots[1] = 0x7c;
                        expected_args[1][0] = root ^ 0x3c3c3c3cu;
                        expected_args[1][1] = 0;
                    }
                }
                expected_point = (context ^ 0xdeadbeefu) + 4;
                set_word(current, 0x1bc, root);
#endif
                if (operation >= 7) {
                    ((void (__stdcall *)(void))(uintptr_t)current->linkage[operation - 6])();
                    if (calls)
                        return 1;
                } else {
                    if (operation == 1 || operation == 4)
                        returned = ((fn1)(uintptr_t)current->code[operation])(context);
                    else
                        returned = ((fn0)(uintptr_t)current->code[operation])();
#ifdef SUCTION_DIFF
                    if (calls != expected_calls || bad_call ||
                        returned != (result ^ expected_slots[expected_calls - 1])) {
#else
                    if (calls != 1 || called_slot != slots[operation] || bad_call || returned != result) {
#endif
                        fprintf(stderr, "scenario %u op %u side %u callback mismatch\n",
                                scenario, operation, side);
                        return 1;
                    }
                }
            }
            if (!compare_data(scenario, operation))
                return 1;
            checks++;
        }
    }
#ifdef SUCTION_DIFF
    if (!test_clamp(count))
        return 1;
#endif
    printf(GAUGE_NAME ": %u scenarios, %u full-data comparisons, all 7 callbacks + init/deinit, "
           "bitmaps identical, 0 differ%s\n", count, checks,
           reservation ? " (forced relocation)" : "");
    FreeLibrary(sides[1].module);
    FreeLibrary(sides[0].module);
    if (reservation)
        VirtualFree(reservation, 0, MEM_RELEASE);
    return 0;
}
