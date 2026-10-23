/* HG2D services against retail, independently of the PANELS model.
 * Compare eax, host/FE/COM calls, entire .data after each operation, and
 * pixel bytes whenever touched. Deterministic boundary cases precede scripts.
 * In-range unaligned handles are intentionally tested with zero pixels/hw;
 * arbitrary malformed COM pointers and out-of-range accounting indices are
 * outside this suite. Usage: hg2ddiff [scenario_count]. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STDCALL __attribute__((stdcall))
#define DATA_SIZE 0x28cd0u
#define POOL 0x5f60u
#define ADDRESS(pointer) ((uint32_t)(uintptr_t)(pointer))
#define WORD(image, offset) (*(uint32_t *)((image) + (offset)))
#define CHECK(condition) do { if (!(condition)) { printf("fixture failed at %s:%d, scenario %u side %u step %u\n", __FILE__, __LINE__, scenario, side, step); exit(2); } } while (0)

static uint8_t *images[2], initial[2][DATA_SIZE];
#define STATE_CAPACITY 512
static uint8_t states[STATE_CAPACITY][DATA_SIZE], normalized[DATA_SIZE];
static uint32_t results[STATE_CAPACITY], events[STATE_CAPACITY], pixel_addresses[STATE_CAPACITY];
static uint8_t pixel_states[STATE_CAPACITY][0x10000], saved_arena[220][0x10000], saved_supplied[0x10000];
static uint8_t *touched;
static uint8_t arena[220][0x10000], supplied[0x10000];
static uint32_t side, step, scenario, random_state, block_count, alloc_count;
static uint32_t fail_every, load_success, event_hash, operations;
static uint32_t allocations, failures, loads, releases, full_pools, unaligned;
static uint32_t polygon_compare, shaded_map_compare;
static uint8_t normalized_global[0x2000];
static uint8_t global[0x2000], global_states[STATE_CAPACITY][0x2000];
static uint32_t carries[STATE_CAPACITY], check_carry, palette_calls, notifications;
static uint32_t layout_entry[2], util_linkage;
uint32_t returned_flags;
static const char *active_suite;
static uint32_t host[0x300 / 4], fe[0x20 / 4], object[1], vtable[3];

/* eax comes from a scripted caller. Three arguments allow +0xa8 too. */
uint32_t invoke(uint32_t function, uint32_t first, uint32_t second,
                uint32_t third, uint32_t eax, uint32_t count);
__asm__(".text\n_invoke:\n\t"
        "pushl %ebp\n\tmovl %esp,%ebp\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 28(%ebp),%ecx\n\t"
        "cmpl $3,%ecx\n\tjne 1f\n\tpushl 20(%ebp)\n"
        "1:\n\tcmpl $2,%ecx\n\tjb 2f\n\tpushl 16(%ebp)\n"
        "2:\n\ttestl %ecx,%ecx\n\tjz 3f\n\tpushl 12(%ebp)\n"
        "3:\n\tmovl 24(%ebp),%eax\n\t"
        "call *8(%ebp)\n\tpushfl\n\tpopl _returned_flags\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n");

static uint32_t random_word(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void event(uint32_t value)
{
    event_hash = (event_hash ^ value) * 16777619u;
}

static uint32_t STDCALL host_alloc(uint32_t flags, uint32_t size, uint32_t *out, uint32_t tag)
{
    uint32_t index;
    event(1); event(flags); event(size); event(tag);
    event(ADDRESS(out) - ADDRESS(images[side]));
    CHECK(flags == 4 && size == 0x10000 && tag == 0);
    alloc_count++;
    if (fail_every && alloc_count % fail_every == 0) {
        failures++;
        event(0xffffffff);
        return 0xffffffff; /* Leave *out alone, as the host does. */
    }
    CHECK(block_count < 220);
    for (index = 0; index < size; index++)
        arena[block_count][index] = (uint8_t)(index + block_count + scenario);
    touched = arena[block_count];
    *out = ADDRESS(arena[block_count++]);
    allocations++;
    return 0;
}

static uint32_t STDCALL host_free(uint32_t *out)
{
    event(2); event(*out); event(ADDRESS(out) - ADDRESS(images[side]));
    *out = 0;
    return 0x777;
}

static uint32_t STDCALL fe_load(uint8_t *out, const uint32_t *name, uint32_t selector, uint32_t flags)
{
    uint32_t index;
    event(3); event(ADDRESS(out)); event(*name); event(selector); event(flags);
    CHECK(flags == 0);
    loads++;
    for (index = 0; index < 0x10000; index++)
        out[index] = (uint8_t)(index * 7 + selector);
    return load_success;
}

static uint32_t STDCALL com_release(uint32_t instance)
{
    CHECK(instance == ADDRESS(object));
    event(4); releases++;
    return 17;
}

static uint32_t pool_handle(uint32_t offset)
{
    return ADDRESS(images[side] + POOL) + offset;
}

static uint32_t normalize_result(uint32_t result)
{
    uint32_t base = ADDRESS(images[side]);
    if (result >= base && result < base + DATA_SIZE)
        return result - base + 0x20792000;
    return result;
}

static void compare(uint32_t result)
{
    uint32_t index;
    CHECK(step < STATE_CAPACITY);
    memcpy(normalized, images[side], DATA_SIZE);
/* The shaded-span fixture deliberately fills its entire 32-row lookup
     * window. Two normally relocated words in its upper half are then bytes,
     * and must be compared literally rather than normalized as pointers. */
#define HG2D_DATA_POINTER(at, target) if (!(shaded_map_compare && (at) >= 0x4e00 && (at) < 0x6e00)) WORD(normalized, at) -= ADDRESS(images[side]) - 0x20792000u;
#include "../../src/hg2d/hg2d_pointers.inc"
#undef HG2D_DATA_POINTER
    memcpy(normalized_global, global, sizeof global);
    if (polygon_compare) {
        WORD(normalized_global, 0xc14) -= ADDRESS(images[side]) - 0x20792000u;
        if (WORD(normalized, 0x4030))
            WORD(normalized, 0x4030) -= ADDRESS(images[side]) - 0x20792000u;
        if (polygon_compare == 2)
            WORD(normalized, 0x1d88) -= ADDRESS(images[side]) - 0x20792000u;
    }
    /* Service pointers differ by implementation; import links share stubs. */
    memset(normalized + 0x38, 0, 0xc0);
    if (side == 0) {
        memcpy(states[step], normalized, DATA_SIZE);
        memcpy(global_states[step], normalized_global, sizeof global);
        carries[step] = check_carry ? returned_flags & 1 : 0;
        results[step] = result;
        events[step] = event_hash;
        pixel_addresses[step] = ADDRESS(touched);
        if (touched) memcpy(pixel_states[step], touched, 0x10000);
    } else if (results[step] != result || events[step] != event_hash ||
               pixel_addresses[step] != ADDRESS(touched) ||
               (touched && memcmp(pixel_states[step], touched, 0x10000)) ||
               memcmp(states[step], normalized, DATA_SIZE) ||
               memcmp(global_states[step], normalized_global, sizeof global) ||
               carries[step] != (check_carry ? returned_flags & 1 : 0)) {
        printf("%s scenario %u step %u differs: eax %08x/%08x events %08x/%08x\n",
               active_suite, scenario, step, results[step], result, events[step], event_hash);
        for (index = 0; index < DATA_SIZE; index++)
            if (states[step][index] != normalized[index]) {
                printf("first data byte +%x: %02x/%02x\n", index, states[step][index], normalized[index]);
                break;
            }
        if (touched)
            for (index = 0; index < 0x10000; index++)
                if (pixel_states[step][index] != touched[index]) {
                    printf("first pixel byte +%x: %02x/%02x\n", index, pixel_states[step][index], touched[index]);
                    break;
                }
        if (check_carry) printf("carry %u/%u\n", carries[step], returned_flags & 1);
        for (index = 0; index < sizeof global; index++)
            if (global_states[step][index] != normalized_global[index]) {
                printf("first GLOBAL byte +%x: %02x/%02x\n", index, global_states[step][index], normalized_global[index]);
                break;
            }
        exit(1);
    }
    step++;
    operations++;
}

static uint32_t call(uint32_t slot, uint32_t first, uint32_t second, uint32_t third)
{
    uint32_t result, count = 2;
    check_carry = slot == 0x60 || slot == 0x64 || slot == 0x7c;
    if (slot == 0xa8 || slot == 0x70) count = 3;
    if (slot == 0xb4 || slot == 0x6c || slot == 0x7c) count = 1;
    if (slot == 0x5c || slot == 0x60 || slot == 0x64) count = 0;
    touched = NULL;
    if (slot == 0x6c)
        touched = (uint8_t *)(uintptr_t)first;
    else if (slot == 0xb8 && (second & 2) && first >= pool_handle(0) && first < pool_handle(0x28a0))
        touched = (uint8_t *)(uintptr_t)*(uint32_t *)(uintptr_t)first;
    result = invoke(WORD(images[side], 0x38 + slot), first, second, third,
                             0xaabbccdd, count);
    compare(slot == 0xa8 || slot == 0xac || slot == 0x7c ? normalize_result(result) : result);
    return result;
}

static void run_textures(void)
{
    uint32_t index, descriptor[8] = {1, 0x100, 0, 0, 0, 0, 0, 0};
    uint32_t name, handle, choice;
    memcpy(images[side], initial[side], DATA_SIZE);
    WORD(images[side], 4) = ADDRESS(host);
    WORD(images[side], 0x1c) = ADDRESS(fe);
    block_count = alloc_count = step = 0;
    event_hash = 2166136261u;
    random_state = scenario * 2654435761u + 0x12345;
    if (!random_state) random_state = 1;
    memset(images[side] + POOL, 0, 0x28a0);
    for (index = 0; index < sizeof supplied; index++)
        supplied[index] = (uint8_t)index;
    for (index = 0; index < 256; index++)
        *(uint16_t *)(images[side] + 0x34c0 + index * 2) = (uint16_t)(index * 13 + scenario);
    fail_every = 0;
    load_success = 1;
    if (scenario % 16 == 0) {
        descriptor[6] = ADDRESS(supplied);
        for (index = 0; index < 201; index++) {
            handle = call(0xac, ADDRESS(descriptor), 0, 0);
            CHECK(index < 200 ? handle == pool_handle(index * 0x34) : handle == 0);
        }
        full_pools++;
        call(0xa8, ADDRESS(descriptor), 0, 0); /* Full pool before FE/alloc. */
        call(0xb0, pool_handle(199 * 0x34), 1, 0);
        CHECK(call(0xac, ADDRESS(descriptor), 0, 0) == pool_handle(199 * 0x34));
        return;
    }
    /* Range boundaries, empty/double-free, unaligned handle accepted. */
    call(0xb0, pool_handle(0xffffffff), 0, 0);
    call(0xb0, pool_handle(0x28a0), 0, 0);
    call(0xb4, pool_handle(0x28a0), 0, 0);
    call(0xb8, pool_handle(0x28a0), 3, 0);
    call(0xb4, pool_handle(1), 0, 0);
    CHECK(WORD(images[side], POOL + 5) == 0x10000);
    call(0xb0, pool_handle(1), 1, 0);
    unaligned++;
    call(0x6c, ADDRESS(supplied), 0, 0);
    for (index = 0; index < sizeof supplied; index++)
        CHECK(supplied[index] == (uint8_t)((uint8_t)index * 13 + scenario));
    /* Reused free records keep stale metadata, including the flags to clear. */
    for (index = 0; index < 200; index++) {
        uint32_t field;
        for (field = 1; field < 13; field++)
            if (field != 3)
                WORD(images[side], POOL + index * 0x34 + field * 4) = random_word();
    }
    for (index = 0; index < 16; index++)
        WORD(images[side], 0x8800 + index * 4) = random_word();
    WORD(images[side], 0x289ec) = random_word();
    fail_every = 2 + scenario % 5;
    for (index = 0; index < 40; index++) {
        choice = random_word() % 6;
        handle = pool_handle((random_word() % 8) * 0x34);
        if (choice == 0) {
            descriptor[0] = random_word() % 4 ? 1 : 0;
            descriptor[1] = random_word() % 4 ? 0x100 : 0xff;
            descriptor[6] = random_word() % 2 ? ADDRESS(supplied) : 0;
            call(0xac, ADDRESS(descriptor), random_word() % 4 ? 0 : 1, 0);
        } else if (choice == 1) {
            name = random_word() % 2 ? 0x65646973 : 0x74736574;
            load_success = random_word() % 2;
            call(0xa8, ADDRESS(&name), random_word(), random_word() % 2);
        } else if (choice == 2) {
            uint32_t offset = handle - ADDRESS(images[side]);
            /* Exercise hardware release on occupied and empty records. */
            WORD(images[side], offset + 12) = ADDRESS(object);
            WORD(images[side], offset + 28) = random_word() % 9;
            call(0xb0, handle, random_word() & 1, 0);
        } else if (choice == 3) {
            call(0xb4, handle, 0, 0);
        } else if (choice == 4) {
            uint32_t flags = random_word();
            if (!WORD(images[side], handle - ADDRESS(images[side]))) flags &= ~2u;
            call(0xb8, handle, flags, 0);
        } else {
            call(0xb0, handle, random_word() & 1, 0);
        }
    }
    /* Check retained pixels as well as each operation's modified block. */
    if (side == 0) {
        memcpy(saved_arena, arena, block_count * 0x10000);
        memcpy(saved_supplied, supplied, sizeof supplied);
    } else {
        CHECK(!memcmp(saved_arena, arena, block_count * 0x10000));
        CHECK(!memcmp(saved_supplied, supplied, sizeof supplied));
    }
    touched = NULL;
    compare(0);
}

#include "hg2d_palette_suite.inc"
#include "hg2d_draw_suite.inc"
#include "hg2d_polygon_suite.inc"
#include "hg2d_fill_suite.inc"
#include "hg2d_matrix_suite.inc"
#include "hg2d_projective_suite.inc"
#include "hg2d_textured_suite.inc"
#include "hg2d_perspective_suite.inc"
#include "hg2d_affine_suite.inc"
#include "hg2d_perspective_fill_suite.inc"
#include "hg2d_gouraud_suite.inc"
#include "hg2d_blit_suite.inc"
#include "hg2d_disc_suite.inc"

static LONG WINAPI fault(EXCEPTION_POINTERS *exception)
{
    printf("fault scenario %u side %u step %u at %p code %lx\n", scenario, side, step,
           exception->ExceptionRecord->ExceptionAddress, exception->ExceptionRecord->ExceptionCode);
    ExitProcess(2);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 20000;
    HMODULE modules[2], util;
    const char *suite = argc > 2 ? argv[2] : "all";
    CHECK(!strcmp(suite, "all") || !strcmp(suite, "textures") || !strcmp(suite, "palette") || !strcmp(suite, "colours") || !strcmp(suite, "drawing") || !strcmp(suite, "polygons") || !strcmp(suite, "fills") || !strcmp(suite, "textured") || !strcmp(suite, "affine") || !strcmp(suite, "gouraud") || !strcmp(suite, "blits") || !strcmp(suite, "lifecycle") || !strcmp(suite, "optimized") || !strcmp(suite, "perspective") || !strcmp(suite, "discs") || !strcmp(suite, "perspective_fill") || !strcmp(suite, "matrix_vertices") || !strcmp(suite, "matrix_fills") || !strcmp(suite, "projective") || !strcmp(suite, "projective_keyed") || !strcmp(suite, "projective_fog") || !strcmp(suite, "projective_key_fog") || !strcmp(suite, "projective_dither"));
    SetUnhandledExceptionFilter(fault);
    CHECK(AddVectoredExceptionHandler(1, draw_exception));
    {
        DWORD previous_protection;
        uint8_t *allocation = VirtualAlloc(NULL, CANVAS_SIZE + 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        CHECK(allocation);
        canvas = allocation + 0x1000;
        CHECK(VirtualProtect(canvas, CANVAS_SIZE, PAGE_READWRITE, &previous_protection));
    }
    modules[0] = LoadLibraryA("HG2D_retail.dll");
    modules[1] = LoadLibraryA("HG2D_ours.dll");
    CHECK(count > 0 && modules[0] && modules[1] && modules[0] != modules[1]);
    CHECK(ADDRESS(modules[0]) == 0x20730000);

    util = LoadLibraryA("UTIL_retail.dll");
    CHECK(util);
    util_linkage = ADDRESS(GetProcAddress(util, "Linkage"));
    CHECK(util_linkage);
    layout_entry[0] = 0x20731c42;
    layout_entry[1] = ADDRESS(GetProcAddress(modules[1], "hg2d_palette_layout"));
    CHECK(layout_entry[1]);
    for (side = 0; side < 2; side++) {
        images[side] = (uint8_t *)GetProcAddress(modules[side], "ImportTable");
        CHECK(images[side] && (uint8_t *)GetProcAddress(modules[side], "Linkage") == images[side] + 0x38);
        CHECK(WORD(images[side], 0x38) == 13);
        CHECK(WORD(images[side], 0) == 1 && WORD(images[side], 8) == 2 &&
              WORD(images[side], 0x10) == 4 && WORD(images[side], 0x18) == 18 &&
              WORD(images[side], 0x20) == 20 && WORD(images[side], 0x28) == 14 &&
              WORD(images[side], 0x30) == 0);
        memcpy(initial[side], images[side], DATA_SIZE);
    }
    host[0x20c / 4] = ADDRESS(host_alloc);
    host[0x214 / 4] = ADDRESS(host_free);
    fe[0x1c / 4] = ADDRESS(fe_load);
    vtable[2] = ADDRESS(com_release);
    object[0] = ADDRESS(vtable);
    host[0x290 / 4] = ADDRESS(host_realize);
    active_suite = "lifecycle";
    for (side = 0; side < 2; side++) {
        draw_reset();
        CHECK(drawing(0x08,0,0,0,0,0,0) == 0xaabbccdd);
    }
    printf("hg2ddiff lifecycle: register-preserving teardown, 0 differ\n");
    operations = 0;
    if (!strcmp(suite, "all") || !strcmp(suite, "textures")) {
        active_suite = "textures";
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_textures();
        printf("hg2ddiff textures: %u scenarios, %u operations, 0 differ\n", count, operations / 2);
        printf("per side: allocations %u, failures %u, FE loads %u, COM releases %u, full pools %u, unaligned %u\n",
               allocations / 2, failures / 2, loads / 2, releases / 2, full_pools / 2, unaligned / 2);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "palette")) {
        active_suite = "palette";
        operations = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_palette();
        printf("hg2ddiff palette: %u scenarios, %u operations, 0 differ\n", count, operations / 2);
        printf("per side: %u realizations, %u CHAIN notifications\n", palette_calls / 2, notifications / 2);
    }
    if ((!strcmp(suite, "all") || !strcmp(suite, "palette") || !strcmp(suite, "colours")) && (argc < 4 || strcmp(argv[3], "scripts"))) run_colour_vectors();
    if (!strcmp(suite, "all") || !strcmp(suite, "drawing")) {
        active_suite = "drawing";
        operations = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_drawing();
        printf("hg2ddiff drawing: %u scenarios, %u operations, %u matching faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "polygons")) {
        active_suite = "polygons";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_polygons();
        printf("hg2ddiff polygons: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "fills")) {
        active_suite = "fills";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_fills();
        printf("hg2ddiff fills: %u scenarios, %u operations, %u matching faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "matrix_vertices")) {
        active_suite = "matrix_vertices";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_matrix_vertices();
        printf("hg2ddiff matrix_vertices: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "matrix_fills")) {
        active_suite = "matrix_fills";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_matrix_fills();
        printf("hg2ddiff matrix_fills: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "projective")) {
        active_suite = "projective";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_projective();
        printf("hg2ddiff projective: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "projective_keyed")) {
        active_suite = "projective_keyed";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_projective_keyed();
        printf("hg2ddiff projective_keyed: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "projective_fog")) {
        active_suite = "projective_fog";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_projective_fog();
        printf("hg2ddiff projective_fog: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "projective_key_fog")) {
        active_suite = "projective_key_fog";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (uint32_t phase = 0; phase < 2; phase++)
                for (side = 0; side < 2; side++) run_projective_key_fog(phase);
        printf("hg2ddiff projective_key_fog: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "projective_dither")) {
        active_suite = "projective_dither";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (uint32_t phase = 0; phase < 2; phase++)
                for (side = 0; side < 2; side++) run_projective_dither(phase);
        printf("hg2ddiff projective_dither: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "textured")) {
        active_suite = "textured";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_textured();
        printf("hg2ddiff textured: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "affine")) {
        active_suite = "affine";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_affine();
        printf("hg2ddiff affine: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "gouraud")) {
        active_suite = "gouraud";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_gouraud();
        printf("hg2ddiff gouraud: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "blits")) {
        active_suite = "blits";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_blits();
        printf("hg2ddiff blits: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "discs")) {
        active_suite = "discs";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_discs();
        printf("hg2ddiff discs: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "perspective")) {
        active_suite = "perspective";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_perspective();
        printf("hg2ddiff perspective: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "perspective_fill")) {
        active_suite = "perspective_fill";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_perspective_fill();
        printf("hg2ddiff perspective_fill: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
        printf("retail host divide repairs: %u\n", draw_div_repairs);
    }
    if (!strcmp(suite, "all") || !strcmp(suite, "optimized")) {
        DWORD old_protection;
        CHECK(VirtualProtect((uint8_t *)modules[0] + 0x1000, 0x5f266, PAGE_EXECUTE_READWRITE, &old_protection));
        active_suite = "optimized";
        operations = draw_div_repairs = draw_faults = 0;
        for (scenario = 0; scenario < count; scenario++)
            for (side = 0; side < 2; side++) run_optimized();
        printf("hg2ddiff optimized: %u scenarios, %u operations, %u faults, 0 differ\n",
               count, operations / 2, draw_faults / 2);
    }
    return 0;
}
