/* Independent scenery-dynamic fixtures. Only imported controls and the
 * caller-owned validation/close callbacks are patched on the retail side. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_dynamic.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811
struct world_fixture {
    uint8_t before[32], object[0x17e8], descriptors[2][0x30], notify[0x29c], after[32];
};
static struct world_fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60];
static char s_logs[2][4096];
static uint32_t s_rng, s_scenario, s_operation, s_calls, s_length;
static uint32_t s_throw_at, s_comparisons, s_throws, s_self_checks;
static int s_side, s_selection, s_check_result;
static uint32_t s_error_calls;
static uint32_t world_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t world_offset(const void *pointer)
{ return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture); }
static void world_log(const char *format, ...)
{
    va_list args;
    int length;
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_length, 4096 - s_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= 4096 - s_length) exit(2);
    s_length += (uint32_t)length;
    if (++s_calls == s_throw_at) RaiseException(THROW_CODE, 0, 0, NULL);
    /* Change the descriptor and vtable at every possible external boundary.
     * Distinct descriptor payloads expose caching across control calls. */
    if ((s_scenario & 1) && s_calls == 1 + (s_scenario / 8) % 4)
        PTR(s_fixture.object, 0x10e0) = s_fixture.descriptors[1];
    if (s_scenario & 2) PTR(s_fixture.object, 0) = s_other_vtable;
}
static int world_send(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{
    world_log("send %x %x %x %x\n", window, message, first, second);
    return s_selection;
}
static int SC world_hook_send(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ return world_send(window, message, first, second); }
static int world_check_control(void *control, int checked)
{
    world_log("check %x %x\n", world_offset(control), (uint32_t)checked);
    U32(control, 0xbc) = (uint32_t)checked;
    return 0;
}
static int TC world_hook_check(void *control, int checked)
{ return world_check_control(control, checked); }
static int SC world_check_descriptor(void *descriptor)
{
    world_log("callback %x answer=%x mode=%x\n", world_offset(descriptor),
              U32(descriptor, 0), U32(s_fixture.object, 0x17e4));
    /* Exercise mutation of both the passed and newly selected descriptors. */
    U32(descriptor, 8) ^= 0xaeb137;
    return s_check_result;
}
static void TC world_close(void *object)
{
    world_log("close answer=%x mode=%x\n", U32(PTR(object, 0x10e0), 0), U32(object, 0x17e4));
}
static void TC world_other_close(void *object)
{ world_log("other.close\n"); world_close(object); }

static int world_error(void *object, void *control, uint32_t message)
{
    world_log("error %x %x\n", world_offset(control), message);
    (void)object;
    return ++s_error_calls == (s_scenario / 16) % 5 ? (int)0x80000001 : 0;
}
static int TC world_hook_error(void *object, void *control, uint32_t message)
{ return world_error(object, control, message); }
static void world_focus(void *control)
{ world_log("focus %x\n", world_offset(control)); U32(s_fixture.object, 0x8c) ^= s_scenario & 8; }
static void TC world_hook_focus(void *control) { world_focus(control); }
static int world_post(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ world_log("post %x %x %x %x\n", window, message, first, second); return -1; }
static int SC world_hook_post(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ return world_post(window, message, first, second); }
static const struct fs98_world_dynamic_services s_services = {world_send, world_check_control, world_error, world_focus, world_post};
static void world_patch(uint32_t rva, void *value)
{
    DWORD old;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) exit(2);
    *slot = value;
    if (!VirtualProtect(slot, 4, old, &old)) exit(2);
}
static const uint32_t s_entries[] = {0xb910, 0xb970, 0xb850, 0xb870, 0xb360, 0xb370, 0xb9f0};
static void world_initialize(void)
{
    static const uint32_t s_values[] = {0, 1, 2, 0xffffffff, 0x80000000, 0x1234ffff, 65536, 65535};
    uint32_t byte, index;
    for (byte = 0; byte < sizeof(s_fixture); ++byte) ((uint8_t *)&s_fixture)[byte] = (uint8_t)world_random();
    PTR(s_fixture.object, 0) = s_vtable;
    PTR(s_fixture.object, 0x10e0) = s_operation == 4 && (s_scenario & 4) ? NULL : s_fixture.descriptors[0];
    U32(s_fixture.object, 0x1648) = s_values[s_scenario % 8];
    U32(s_fixture.object, 0x1580) = s_values[(s_scenario / 8) % 8];
    U32(s_fixture.object, 0x1710) = s_values[(s_scenario / 64) % 8];
    U32(s_fixture.object, 0x17d8) = s_values[(s_scenario / 512) % 8];
    U32(s_fixture.object, 0x149c) = 0x12345678;
    for (index = 0; index < 2; ++index) {
        PTR(s_fixture.descriptors[index], 4) = world_check_descriptor;
        U32(s_fixture.descriptors[index], 0xc) = s_values[(s_scenario + index) % 8];
        U32(s_fixture.descriptors[index], 0x14) = s_values[(s_scenario / 8 + index) % 8];
        U32(s_fixture.descriptors[index], 0x1c) = s_values[(s_scenario / 64 + index) % 8];
        U32(s_fixture.descriptors[index], 0x24) = s_values[(s_scenario / 512 + index) % 8];
        U32(s_fixture.descriptors[index], 0x2c) = s_values[(s_scenario / 1024 + index) % 8];
        for (uint32_t field = 0; field < 4; ++field)
            U32(s_fixture.descriptors[index], 8 + field * 8) = (s_scenario & (4u << field)) ? 0x1000 + field + index : 0;
    }
    s_selection = (int)s_values[(s_scenario / 4) % 8];
    s_check_result = (int)s_values[(s_scenario / 32) % 8];
    s_initial = s_fixture;
}
static void world_witness(int result)
{
    static const uint32_t s_values[] = {0x1580, 0x1648, 0x1710, 0x17d8};
    static const uint32_t s_fields[] = {0x24, 0x1c, 0x14, 0xc};
    uint32_t index;
    int bad = 0;
    if (s_operation == 4) bad = result != (PTR(s_initial.object, 0x10e0) != NULL);
    if (s_operation == 5) bad = result != s_check_result;
    if (s_operation == 0 && !(s_scenario & 1)) {
        for (index = 0; index < 4; ++index)
            bad |= U32(s_fixture.descriptors[0], s_fields[index]) != U32(s_initial.object, s_values[index]);
        bad |= U32(s_fixture.descriptors[0], 0x2c) != (uint32_t)s_selection;
    }
    if (s_operation == 1 && !(s_scenario & 1))
        for (index = 0; index < 4; ++index)
            bad |= U32(s_fixture.object, s_values[index]) != U32(s_initial.descriptors[0], s_fields[index]);
    if (s_operation == 2) bad |= U32(s_fixture.object, 0x17e4) != 2 || U32(s_fixture.descriptors[0], 0) != 0;
    if (s_operation == 3) bad |= U32(s_fixture.object, 0x17e4) != 1;
    if (s_operation == 6 && !(s_scenario & 1)) {
        uint32_t expected_calls = 0, expected = 0;
        for (index = 0; index < 4; ++index) if (U32(s_initial.descriptors[0], 0x20 - index * 8)) {
            ++expected_calls;
            if ((s_scenario / 16) % 5 == expected_calls) { expected = 0x80000001 ^ (s_scenario & 8); break; }
        }
        bad |= (uint32_t)result != expected || s_error_calls != expected_calls;
    }
    if (bad) { fprintf(stderr, "fixture witness failed scenario=%u operation=%u\n%s", s_scenario, s_operation, s_logs[0]); exit(2); }
    ++s_self_checks;
}
static void world_invoke(void)
{
    void *object = s_fixture.object;
    void *entry = (uint8_t *)s_retail + s_entries[s_operation];
    int result = 0;
    if (!s_side) {
        if (s_operation >= 4) result = ((int (TC *)(void *))entry)(object);
        else ((void (TC *)(void *))entry)(object);
    } else switch (s_operation) {
        case 0: fs98_world_dynamic_collect(object); break;
        case 1: fs98_world_dynamic_populate(object); break;
        case 2: fs98_world_dynamic_cancel(object); break;
        case 3: fs98_world_dynamic_accept(object); break;
        case 4: result = fs98_world_dynamic_has_descriptor(object); break;
        case 5: result = fs98_world_dynamic_check(object); break;
        case 6: result = fs98_world_dynamic_error(object); break;
    }
    if (!s_side && !s_throw_at) world_witness(result);
    world_log("return %x\n", (uint32_t)result);
}
static LONG CALLBACK world_exception(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode == THROW_CODE) {
        s_throw_at = 0;
        if (!s_side) ++s_throws;
        /* The seven routines have no local EH frames or cleanup. This is
         * interruption acceptance, not an MSVC class-lifetime unwind test. */
        __builtin_longjmp(s_jump, 1);
    }
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u operation=%u\n",
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress,
        s_side, s_scenario, s_operation);
    ExitProcess(2);
}
static void world_run(uint32_t throw_at)
{
    s_fixture = s_initial;
    s_calls = s_length = s_error_calls = 0;
    s_logs[s_side][0] = 0;
    s_throw_at = throw_at;
    if (__builtin_setjmp(s_jump) == 0) world_invoke();
    else world_log("caught\n");
    if (memcmp(s_fixture.before, s_initial.before, sizeof(s_fixture.before)) ||
        memcmp(s_fixture.after, s_initial.after, sizeof(s_fixture.after))) exit(2);
}
static int world_compare(uint32_t throw_at)
{
    uint32_t byte;
    s_side = 0; world_run(throw_at); s_reference = s_fixture;
    s_side = 1; world_run(throw_at); ++s_comparisons;
    if (!memcmp(&s_reference, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u operation=%u throw=%u\n", s_scenario, s_operation, throw_at);
    for (byte = 0; byte < sizeof(s_fixture); ++byte)
        if (((uint8_t *)&s_reference)[byte] != ((uint8_t *)&s_fixture)[byte]) {
            fprintf(stderr, "byte=%x retail=%x ours=%x\n", byte, ((uint8_t *)&s_reference)[byte], ((uint8_t *)&s_fixture)[byte]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 2000;
    uint32_t seed = argc > 2 ? strtoul(argv[2], NULL, 0) : 0x4a971b63;
    uint32_t position, limit;
    int interrupt = argc > 3 && !strcmp(argv[3], "interrupt");
    if (!count || !seed || (argc > 3 && strcmp(argv[3], "normal") && !interrupt)) return 2;
    s_rng = seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_exception);
    s_retail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x24120)) return 2;
    world_patch(0x25778, world_hook_error);
    world_patch(0x25590, world_hook_focus);
    world_patch(0x258a0, world_hook_post);
    world_patch(0x2577c, world_hook_check);
    world_patch(0x258b0, world_hook_send);
    s_vtable[0xec / 4] = world_close;
    memcpy(s_other_vtable, s_vtable, sizeof(s_vtable));
    s_other_vtable[0xec / 4] = world_other_close;
    fs98_world_dynamic_set_services(&s_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        for (s_operation = 0; s_operation < 7; ++s_operation) {
            world_initialize();
            if (world_compare(0)) return 1;
            limit = s_calls - 1;
            if (interrupt) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World dynamic core: %u scenarios, 7 operations, %u comparisons, 0 differ; seed=%x interrupts=%u witnesses=%u\n",
        count, s_comparisons, seed, s_throws, s_self_checks);
    FreeLibrary(s_retail);
    return 0;
}
