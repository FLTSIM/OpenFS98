/* Six SIM1 exports only. No lifecycle calls, live-game writes or shared prefix.
 * Each case restores both independent images and compares every byte of their
 * seeded state, output guards, register results and meaningful x87 environment.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_SIZE 0x43f8u
#define HEADER_SIZE 0x94u

uint32_t call_target, call_argc, call_args[3], call_inputs[7], call_registers[8];
uint32_t call_sp, call_preload;
uint16_t call_control;
uint8_t call_fp[108] __attribute__((aligned(16)));
void sim1_call(void);

struct side {
    HMODULE module;
    uint8_t *image;
    uint32_t *link;
    uint8_t pristine[DATA_SIZE];
};

static struct side sides[2];
static uint8_t seeded[DATA_SIZE], initial_output[256], output[256], expected_output[256];
static uint8_t expected_image[DATA_SIZE], expected_fp[108];
static uint32_t vector, slot, mode, role, seed = 0x53494d31;
static uint32_t coverage[6], aliases[6];
static const uint32_t slots[6] = {0x18, 0x28, 0x2c, 0x30, 0x34, 0x38};

static uint32_t read32(const void *p)
{ uint32_t value; memcpy(&value, p, 4); return value; }
static uint16_t read16(const void *p)
{ uint16_t value; memcpy(&value, p, 2); return value; }
static void write32(void *p, uint32_t value)
{ memcpy(p, &value, 4); }
static uint32_t rng(void)
{ seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static void fill(uint8_t *p, uint32_t count)
{
    uint32_t i;
    for (i = 0; i + 4 <= count; i += 4) write32(p + i, rng());
    while (i < count) p[i++] = (uint8_t)rng();
}
static void fail(const char *what, uint32_t offset)
{
    fprintf(stderr, "SIM1 vector=%u slot=%02x fp-mode=%u side=%u seed=%08x: %s +%x\n",
            vector, slot, mode, role, seed, what, offset);
    ExitProcess(1);
}
static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "SIM1 exception=%08lx address=%p\n",
            exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    fail("unexpected exception", 0);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void compare(const void *a, const void *b, uint32_t count, const char *what)
{
    uint32_t i;
    const uint8_t *x = a, *y = b;
    if (!memcmp(a, b, count)) return;
    for (i = 0; i < count; ++i) if (x[i] != y[i]) {
        fprintf(stderr, "expected=%02x actual=%02x\n", x[i], y[i]);
        fail(what, i);
    }
}
static int owns_code(HMODULE module, uint32_t address)
{
    uint8_t *base = (uint8_t *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
    uint32_t i, rva = address - (uint32_t)(uintptr_t)base;
    for (i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) &&
            rva >= section->VirtualAddress && rva < section->VirtualAddress + section->Misc.VirtualSize)
            return 1;
    return 0;
}

/* Compare control/status/tag words and only live logical x87 registers.
 * Saved instruction/data addresses identify different code and are not results;
 * empty register payloads are historical contents, not published values.
 */
static void compare_fp(void)
{
    uint32_t i, top = (read16(expected_fp + 4) >> 11) & 7;
    uint16_t tags = read16(expected_fp + 8);
    compare(expected_fp, call_fp, 2, "x87 control");
    compare(expected_fp + 4, call_fp + 4, 2, "x87 status");
    compare(expected_fp + 8, call_fp + 8, 2, "x87 tags");
    for (i = 0; i < 8; ++i)
        if (((tags >> (2 * ((top + i) & 7))) & 3) != 3)
            compare(expected_fp + 28 + 10 * i, call_fp + 28 + 10 * i, 10, "x87 live value");
}

static uint32_t scalar(void)
{
    static const uint32_t edges[] = {0, 1, 0x7fffffff, 0x80000000, 0xffffffff,
                                    0x00ffffff, 0x01000001, 0xffff0001};
    return vector < sizeof edges / sizeof edges[0] ? edges[vector] : read32(seeded + 0x17a4);
}

static void run_case(uint32_t index)
{
    static const uint32_t precision[] = {0, 2, 3};
    uint32_t expected_registers[8], i, j;
    uint32_t offset = 32 + (vector & 3), alias = vector % 8;
    slot = slots[index];
    call_control = 0x007f | (precision[mode / 4] << 8) | ((mode % 4) << 10);
    call_preload = vector & 1;
    for (i = 0; i < 7; ++i) call_inputs[i] = read32(initial_output + 128 + i * 4);
    for (role = 0; role < 2; ++role) {
        struct side *side = sides + role;
        memcpy(side->image, side->pristine, DATA_SIZE);
        memcpy(side->image + HEADER_SIZE, seeded + HEADER_SIZE, DATA_SIZE - HEADER_SIZE);
        memcpy(output, initial_output, sizeof output);
        for (i = 0; i < 3; ++i)
            call_args[i] = (uint32_t)(uintptr_t)(output + offset + 16 * i);
        call_argc = slot == 0x38 ? 0 : slot == 0x30 ? 3 : 1;
        if (alias == 1) call_args[1] = call_args[2] = call_args[0];
        if (alias == 2) call_args[1] = call_args[0] + 1;
        if (alias >= 3 && slot != 0x34 && slot != 0x38) {
            uint32_t source = slot == 0x18 ? 0x1730 : slot == 0x28 ? 0x1518 :
                              slot == 0x2c ? 0x17a4 : 0x13b8;
            call_args[0] = (uint32_t)(uintptr_t)(side->image + source + alias - 5);
            if (slot == 0x30) {
                call_args[1] = (uint32_t)(uintptr_t)(side->image + 0x13c0);
                call_args[2] = (uint32_t)(uintptr_t)(side->image + 0x13c8);
            }
        }
        if (slot == 0x34) call_args[0] = scalar();
        memcpy(expected_registers, call_inputs, sizeof call_inputs);
        expected_registers[7] = 0;
        if (slot == 0x28)
            expected_registers[0] = (call_inputs[0] & 0xffff0000) | read16(seeded + 0x1518);
        if (slot == 0x2c || slot == 0x30) {
            expected_registers[0] = 0;
            expected_registers[2] = call_args[slot == 0x30 ? 1 : 0];
            if (slot == 0x30) expected_registers[3] = call_args[2];
        }
        if (slot == 0x34) expected_registers[0] = scalar();
        call_target = side->link[slot / 4];
        sim1_call();
        compare(expected_registers, call_registers, sizeof call_registers, "integer registers/ESP");
        compare(side->pristine, side->image, HEADER_SIZE, "ImportTable/Linkage changed");
        if (!role) {
            memcpy(expected_output, output, sizeof output);
            memcpy(expected_image, side->image, DATA_SIZE);
            memcpy(expected_fp, call_fp, sizeof call_fp);
        } else {
            compare(expected_output, output, sizeof output, "output/guards");
            compare(expected_image + HEADER_SIZE, side->image + HEADER_SIZE,
                    DATA_SIZE - HEADER_SIZE, "complete seeded state");
            compare_fp();
        }
    }
    /* Assert the ordinary byte-copy output independently of the two modules. */
    if (alias < 3 && (slot == 0x18 || slot == 0x28)) {
        j = slot == 0x18 ? 0x2c : 2;
        compare(seeded + (slot == 0x18 ? 0x1730 : 0x1518), output + offset, j, "copy oracle");
        compare(initial_output, output, offset, "leading guard");
        compare(initial_output + offset + j, output + offset + j,
                sizeof output - offset - j, "trailing guard");
    }
    ++coverage[index];
    if ((alias >= 3 && slot != 0x34 && slot != 0x38) ||
        (slot == 0x30 && (alias == 1 || alias == 2))) ++aliases[index];
}

static void seed_case(void)
{
    static const uint64_t fp_edges[] = {
        0, UINT64_C(0x8000000000000000), UINT64_C(0x3ff0000000000000),
        UINT64_C(0xbff0000000000000), 1, UINT64_C(0x000fffffffffffff),
        UINT64_C(0x0010000000000000), UINT64_C(0x7fefffffffffffff),
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000001234), UINT64_C(0x7ff0000000001234)};
    static const uint32_t offsets[] = {0x640, 0x740, 0x13b8, 0x13c0, 0x13c8};
    uint32_t i;
    fill(seeded, sizeof seeded);
    fill(initial_output, sizeof initial_output);
    write32(seeded + 0x17a4, scalar());
    if (vector < 144)
        for (i = 0; i < sizeof offsets / sizeof offsets[0]; ++i)
            memcpy(seeded + offsets[i], fp_edges + ((vector + 3 * i) % 12), 8);
}

int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 20000;
    const char *order = argc > 2 ? argv[2] : "retail-first";
    const char *ours = argc > 3 ? argv[3] : "bin/sim1-diff/SIM1_PARTIAL.DLL";
    const char *paths[2] = {"orig/Sim/SIM1.sim", ours};
    uint32_t first = !strcmp(order, "ours-first"), i;
    void *reservation = NULL;
    if (!count || (strcmp(order, "retail-first") && strcmp(order, "ours-first") && strcmp(order, "relocate")))
        return 2;
    SetUnhandledExceptionFilter(fault);
    if (!strcmp(order, "relocate")) {
        reservation = VirtualAlloc((void *)0x22000000, 0x40000, MEM_RESERVE, PAGE_NOACCESS);
        if (!reservation) fail("reserve preferred image", GetLastError());
    }
    for (i = 0; i < 2; ++i) {
        struct side *side = sides + (first ^ i);
        side->module = LoadLibraryA(paths[first ^ i]);
        if (!side->module) fail("load module", GetLastError());
        side->image = (uint8_t *)GetProcAddress(side->module, "ImportTable");
        side->link = (uint32_t *)GetProcAddress(side->module, "Linkage");
        if (!side->image || (uint8_t *)side->link != side->image + 0x58 || side->link[0] != 25)
            fail("export layout/identity", i);
        if (((uintptr_t)side->module == 0x22000000) != (!reservation && i == 0))
            fail("preferred/relocated placement", i);
        for (slot = 0; slot < 6; ++slot)
            if (!owns_code(side->module, side->link[slots[slot] / 4]))
                fail("service outside owned executable section", slot);
        memcpy(side->pristine, side->image, DATA_SIZE);
    }
    if (sides[0].module == sides[1].module || sides[0].image == sides[1].image)
        fail("retail and ours are the same implementation", 0);
    for (vector = 0; vector < count; ++vector) {
        seed_case();
        for (mode = 0; mode < 12; ++mode)
            for (i = 0; i < 6; ++i) run_case(i);
    }
    /* Exhaustive 16-bit copy domain; FP modes were already covered above. */
    seed_case();
    mode = 0;
    for (vector = 0; vector < 65536; ++vector) {
        uint16_t word = (uint16_t)vector;
        memcpy(seeded + 0x1518, &word, 2);
        run_case(1);
    }
    for (i = 0; i < 6; ++i)
        printf("SIM1 +%02x: %u cases (%u alias layouts), 0 differences\n", slots[i], coverage[i], aliases[i]);
    printf("PASS: %u seeded scenarios, 12 x87 modes, exhaustive words (%s)\n", count, order);
    FreeLibrary(sides[1].module);
    FreeLibrary(sides[0].module);
    if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    return 0;
}
