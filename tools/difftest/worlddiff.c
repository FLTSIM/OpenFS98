/* Partial World area-event comparison. Retail executes its own mapped code;
 * only imported UI/CRT calls and virtual callbacks are replaced by fixtures. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_internal.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811
struct world_fixture {
    uint8_t before[32], object[0x1b20];
    uint8_t descriptors[2][0x30], rows[2][0x40];
    char paths[2][160], titles[2][160];
    uint8_t after[32];
};
static struct world_fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60];
static char s_logs[2][8192];
static uint32_t s_rng, s_scenario, s_operation, s_calls, s_length;
static uint32_t s_throw_at, s_comparisons, s_throws, s_swaps, s_self_checks;
static int s_side, s_selection, s_error_result, s_validate_result, s_virtual_error;
static uint32_t world_random(void)
{
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static uint32_t world_offset(const void *ptr)
{
    return (uint32_t)((const uint8_t *)ptr - (const uint8_t *)&s_fixture);
}
static void world_log(const char *format, ...)
{
    va_list args;
    int length;
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_length, 8192 - s_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= 8192 - s_length) exit(2);
    s_length += (uint32_t)length;
    if (++s_calls == s_throw_at) RaiseException(THROW_CODE, 0, 0, NULL);
    /* External calls may change the descriptor, row flags or virtual table. */
    if (s_calls == 2 && s_scenario & 1) {
        PTR(s_fixture.object, 0x10e4) = s_fixture.descriptors[1];
        if (!s_side) ++s_swaps;
    }
    if (s_scenario & 2) {
        U32(s_fixture.rows[0], 0x3c) ^= 1;
        U32(s_fixture.rows[1], 0x3c) ^= 1;
    }
}
static int world_get_text(void *control, char *text, int capacity)
{
    const char *value_text;
    static const char *s_numbers[] = {"0", "-1", "2147483647", "-2147483648", "  +42x", "bad", "0000000020"};
    uint32_t offset = world_offset(control) - world_offset(s_fixture.object);
    value_text = offset == 0x19a0 ? s_numbers[s_scenario % 7] : offset == 0x1938 ? "C:\\area\\scenery\\test.bgl" : "Area title";
    world_log("get %x capacity=%d [%s]\n", offset, capacity, value_text);
    snprintf(text, (size_t)capacity, "%s", value_text);
    return (int)strlen(text);
}
static int TC world_hook_get(void *control, char *text, int capacity)
{ return world_get_text(control, text, capacity); }
static void world_set_text(void *control, const char *text)
{ world_log("set %x [%s]\n", world_offset(control), text); }
static void TC world_hook_set(void *control, const char *text)
{ world_set_text(control, text); }
static int world_parse(const char *text)
{ world_log("parse [%s]\n", text); return atoi(text); }
static int world_format(char *text, const char *format, int value)
{ world_log("format [%s] %d\n", format, value); return sprintf(text, format, value); }
static int __cdecl world_hook_format(char *text, const char *format, int value)
{
    return world_format(text, format, value);
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
static int world_enable(void *control, int enabled)
{ world_log("enable %x %d\n", world_offset(control), enabled); return 0; }
static int TC world_hook_enable(void *control, int enabled)
{ return world_enable(control, enabled); }
static int world_error(void *object, void *control, void *status)
{
    uint32_t offset = world_offset(control) - world_offset(object);
    if (offset == 0x19a0) {
        world_log("error %x status=%x text=[%s] max=%u\n", offset, U32(status, 0), (char *)PTR(status, 4), U32(status, 8));
    } else world_log("error %x status=%x\n", offset, (uint32_t)status);
    return s_error_result;
}
static int TC world_hook_error(void *object, void *control, void *status)
{ return world_error(object, control, status); }
static void world_focus_control(void *control)
{ world_log("focus.control %x\n", world_offset(control)); }
static void TC world_hook_focus_control(void *control)
{ world_focus_control(control); }
static uint32_t SC world_check_descriptor(void *descriptor)
{
    world_log("callback %x\n", world_offset(descriptor));
    /* The return is ignored. Status records carry the result instead. */
    return 0xfedcba98;
}
static void TC world_close(void *object)
{ world_log("close %x answer=%u\n", world_offset(object), U32(PTR(object, 0x10e4), 0)); }
static void TC world_other_close(void *object)
{ world_log("other.close\n"); world_close(object); }
static int TC world_virtual_validate(void *object, uint32_t value)
{
    world_log("validate %x %x\n", world_offset(object), value);
    PTR(object, 0) = s_other_vtable;
    return s_validate_result;
}
static int TC world_virtual_error(void *object, uint32_t value)
{ world_log("virtual.error %x %x\n", world_offset(object), value); return s_virtual_error; }
static int TC world_focus(void *object, uint32_t value)
{ world_log("focus %x %x\n", world_offset(object), value); return -1; }
static const struct fs98_world_services s_services = {
    world_get_text, world_set_text, world_parse, world_format, world_send,
    world_check_control, world_enable, world_error, world_focus_control
};
static void world_patch(uint32_t rva, void *value)
{
    DWORD old;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) exit(2);
    *slot = value;
    if (!VirtualProtect(slot, 4, old, &old)) exit(2);
}
static const uint32_t s_entries[] = {0xc360, 0xc440, 0xc2c0, 0xc2e0, 0xc610, 0xc660, 0xc720};
static void world_initialize(void)
{
    uint32_t index, byte;
    static const uint32_t s_layers[] = {0, 9, 10, 11, 20, 30, 40, 50, 51, 0xffffffff};
    for (byte = 0; byte < sizeof(s_fixture); ++byte) ((uint8_t *)&s_fixture)[byte] = (uint8_t)world_random();
    PTR(s_fixture.object, 0) = s_vtable;
    PTR(s_fixture.object, 0x10e4) = s_fixture.descriptors[0];
    for (index = 0; index < 2; ++index) {
        void *descriptor = s_fixture.descriptors[index];
        void *row = s_fixture.rows[index];
        PTR(descriptor, 4) = world_check_descriptor;
        PTR(descriptor, 0xc) = row;
        U32(descriptor, 0x10) = s_scenario & 4 ? 6 : 0;
        U32(descriptor, 0x14) = world_random();
        PTR(descriptor, 0x1c) = NULL;
        U32(descriptor, 0x2c) = 23 + index;
        U32(row, 0) = s_scenario & 8 ? 0x1007 : 0;
        U32(row, 0x14) = s_scenario & 16 ? 0x1005 : 0;
        PTR(row, 4) = s_fixture.paths[index];
        PTR(row, 0x18) = s_fixture.titles[index];
        strcpy(s_fixture.paths[index], "Initial path");
        strcpy(s_fixture.titles[index], "Initial title");
        U32(row, 0x2c) = s_layers[(s_scenario + index) % 10];
        U32(row, 0x3c) = s_scenario & 32 ? 1 : 0;
    }
    s_selection = (int)(s_scenario % 8) - 1;
    s_error_result = s_scenario & 64 ? -1 : 0;
    s_validate_result = s_scenario & 128 ? 0x1b07 : 0;
    s_virtual_error = s_scenario & 256 ? 1 : 0;
    s_initial = s_fixture;
}
static void world_invoke(void)
{
    void *object = s_fixture.object;
    void *entry = (uint8_t *)s_retail + s_entries[s_operation];
    int result = 0;
    if (!s_side) {
        if (s_operation == 4 || s_operation == 5)
            result = ((int (TC *)(void *, uint32_t))entry)(object, s_scenario);
        else ((void (TC *)(void *))entry)(object);
    } else switch (s_operation) {
        case 0: fs98_world_area_collect(object); break;
        case 1: fs98_world_area_populate(object); break;
        case 2: fs98_world_area_cancel(object); break;
        case 3: fs98_world_area_accept(object); break;
        case 4: result = fs98_world_area_validate(object, s_scenario); break;
        case 5: result = fs98_world_area_error(object, s_scenario); break;
        case 6: fs98_world_area_path_changed(object); break;
    }
    if (!s_side && !s_throw_at) {
        void *descriptor = PTR(object, 0x10e4);
        int expected = s_scenario & 16 ? 0x1b07 : s_scenario & 4 ? 0x1aef : 0;
        if ((s_operation == 4 && result != expected) ||
            (s_operation == 5 && result != -1) ||
            (s_operation == 2 && U32(descriptor, 0) != 0) ||
            (s_operation == 6 && s_calls != 9) ||
            (s_operation == 0 && s_selection >= 0 && s_selection <= 4 &&
                U32(s_fixture.rows[0], 0x2c) != (uint32_t)(s_selection + 1) * 10) ||
            (s_operation == 0 && (s_selection < 0 || s_selection > 4) &&
                U32(s_fixture.rows[0], 0x2c) != U32(s_initial.rows[0], 0x2c))) {
            fprintf(stderr, "fixture branch self-check failed scenario=%u operation=%u\n", s_scenario, s_operation);
            exit(2);
        }
        ++s_self_checks;
    }
    /* A result is observable only for the two int-returning virtual methods. */
    world_log("return %x\n", (uint32_t)result);
}
static LONG CALLBACK world_exception(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode == THROW_CODE) {
        s_throw_at = 0;
        if (!s_side) ++s_throws;
        /* These seven routines have no local EH frames/owned temporaries. */
        __builtin_longjmp(s_jump, 1);
    }
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u operation=%u\n",
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress,
        s_side, s_scenario, s_operation);
    ExitProcess(2);
}
static void world_run(uint32_t throw_at)
{
    uint32_t index;
    s_fixture = s_initial;
    s_calls = s_length = 0;
    s_logs[s_side][0] = 0;
    s_throw_at = throw_at;
    if (__builtin_setjmp(s_jump) == 0) world_invoke();
    else world_log("caught\n");
    if (memcmp(s_fixture.before, s_initial.before, sizeof(s_fixture.before)) ||
        memcmp(s_fixture.after, s_initial.after, sizeof(s_fixture.after))) {
        fprintf(stderr, "fixture guard changed side=%d scenario=%u operation=%u\n", s_side, s_scenario, s_operation);
        exit(2);
    }
    /* Retail retains a pointer to its expired stack buffer. Compare the
     * pointed-to text inside Error, then normalize only that address. */
    for (index = 0; index < 2; ++index)
        if (PTR(s_fixture.descriptors[index], 0x1c))
            PTR(s_fixture.descriptors[index], 0x1c) = (void *)0x1234;
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
    if (!count || !seed) return 2;
    s_rng = seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_exception);
    s_retail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x24120)) {
        fprintf(stderr, "retail ownership failed: module=%p Linkage=%p error=%lu\n", s_retail,
            s_retail ? GetProcAddress(s_retail, "Linkage") : NULL, GetLastError());
        return 2;
    }
    world_patch(0x25598, world_hook_get); world_patch(0x255bc, world_hook_set);
    world_patch(0x255c8, world_hook_enable); world_patch(0x25590, world_hook_focus_control);
    world_patch(0x25778, world_hook_error); world_patch(0x2577c, world_hook_check);
    world_patch(0x258b0, world_hook_send); world_patch(0x256c0, world_parse);
    world_patch(0x256b8, world_hook_format);
    s_vtable[0xd8 / 4] = world_virtual_validate;
    s_vtable[0xdc / 4] = world_virtual_error;
    s_vtable[0xe0 / 4] = world_focus;
    s_vtable[0xec / 4] = world_close;
    memcpy(s_other_vtable, s_vtable, sizeof(s_vtable));
    s_other_vtable[0xec / 4] = world_other_close;
    fs98_world_set_services(&s_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        for (s_operation = 0; s_operation < 7; ++s_operation) {
            world_initialize();
            if (world_compare(0)) return 1;
            limit = s_calls - 1; /* Last call is the harness's return log. */
            if (interrupt) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World area core: %u scenarios, 7 operations, %u comparisons, 0 differ; seed=%x interrupts=%u swaps=%u self_checks=%u\n",
        count, s_comparisons, seed, s_throws, s_swaps, s_self_checks);
    FreeLibrary(s_retail);
    return 0;
}
