/* Shared World booster-event comparison. Retail executes its own mapped code;
 * only imported UI/CRT calls and virtual callbacks are replaced by fixtures. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_booster.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811
struct world_fixture {
    uint8_t before[32], object[0x1f78];
    uint8_t descriptors[2][0x30], rows[2][0x80];
    char paths[2][288], titles[2][288], names[2][64], descriptions[2][96], bounds[2][32];
    uint8_t after[32];
};
static struct world_fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static void *s_jump[5], *s_vtable[60], *s_other_vtable[60];
static char s_logs[2][8192];
static uint32_t s_rng, s_scenario, s_operation, s_calls, s_length;
static uint32_t s_throw_at, s_comparisons, s_throws, s_swaps, s_self_checks;
static int s_side, s_selection, s_error_result;
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
    if (s_calls == 1 + (s_scenario / 1024) % 16 && s_scenario & 1) {
        PTR(s_fixture.object, 0x10dc) = s_fixture.descriptors[1];
        if (!s_side) ++s_swaps;
    }
    if (s_scenario & 2) {
        U32(s_fixture.rows[0], 0) ^= 0x1007;
        U32(s_fixture.rows[1], 0) ^= 0x1007;
        PTR(s_fixture.object, 0) = s_other_vtable;
    }
}
static int world_get_text(void *control, char *text, int capacity)
{
    const char *value_text;
    static const char *s_numbers[] = {"0", "-1", "2147483647", "-2147483648", "  +42x", "bad", "0000000020"};
    uint32_t offset = world_offset(control) - world_offset(s_fixture.object);
    value_text = offset == 0x1b90 ? s_numbers[s_scenario % 7] : offset == 0x1bf8 ? s_initial.paths[0] : s_initial.titles[0];
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
    if (offset == 0x1b90) {
        world_log("error %x status=%x text=[%s] max=%d\n", offset, U32(status, 0), (char *)PTR(status, 4), (int)U32(status, 8));
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
{ world_log("close %x answer=%u\n", world_offset(object), U32(PTR(object, 0x10dc), 0)); }
static void TC world_other_close(void *object)
{ world_log("other.close\n"); world_close(object); }
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
static void world_title(void *control, const char *text)
{ world_log("title %x [%s]\n", world_offset(control), text); }
static void TC world_hook_title(void *control, const char *text)
{ world_title(control, text); }
static const struct fs98_world_booster_services s_booster_services = {&s_services, world_title};
static const uint32_t s_entries[] = {0x6dd0, 0x6ea0, 0x6740, 0x6fd0, 0x7160, 0x71a0};
static void world_initialize(void)
{
    uint32_t index, byte;
    static const uint32_t s_values[] = {0, 1, 2, 0xffffffff, 0x80000000, 0x1234ffff, 65536, 65535};
    for (byte = 0; byte < sizeof(s_fixture); ++byte) ((uint8_t *)&s_fixture)[byte] = (uint8_t)world_random();
    PTR(s_fixture.object, 0) = s_vtable;
    PTR(s_fixture.object, 0x10dc) = s_fixture.descriptors[0];
    U32(s_fixture.object, 0x1d64) = s_values[s_scenario % 8];
    for (index = 0; index < 2; ++index) {
        void *descriptor = s_fixture.descriptors[index];
        void *row = s_fixture.rows[index];
        PTR(descriptor, 4) = world_check_descriptor;
        PTR(descriptor, 0xc) = row;
        U32(row, 0) = s_scenario & 4 ? 0x1007 : 0;
        U32(row, 0x14) = s_scenario & 8 ? 0x1005 : 0;
        U32(row, 0x78) = s_scenario & 16 ? 0x1009 : 0;
        U32(row, 0x50) = s_scenario & 32 ? 0x100a : 0;
        U32(row, 0x58) = s_scenario & 64 ? 0x100b : 0;
        PTR(row, 4) = s_fixture.paths[index];
        PTR(row, 0x18) = s_fixture.titles[index];
        PTR(row, 0x2c) = s_fixture.names[index];
        PTR(row, 0x40) = s_fixture.descriptions[index];
        PTR(row, 0x70) = s_fixture.bounds[index];
        PTR(row, 0x64) = NULL;
        memset(s_fixture.paths[index], 'P' + index, 270);
        s_fixture.paths[index][270] = 0;
        memset(s_fixture.titles[index], 'C' + index, 270);
        s_fixture.titles[index][270] = 0;
        snprintf(s_fixture.names[index], 64, "Booster name %u", index);
        snprintf(s_fixture.descriptions[index], 96, "Booster description %u", index);
        snprintf(s_fixture.bounds[index], 32, "%d", index ? -27 : 1234);
        U32(row, 0x5c) = world_random();
        U32(row, 0x74) = s_values[(s_scenario / 8 + index) % 8];
        U32(row, 0x7c) = s_values[(s_scenario / 64 + index) % 8];
        U32(row, 0x54) = s_values[(s_scenario / 512 + index) % 8];
    }
    s_selection = (int)(s_scenario % 5) - 1;
    s_error_result = s_scenario & 128 ? -1 : 0;
    s_initial = s_fixture;
}
static void world_invoke(void)
{
    void *object = s_fixture.object;
    void *entry = (uint8_t *)s_retail + s_entries[s_operation];
    int result = 0;
    if (!s_side) {
        if (s_operation >= 4)
            result = ((int (TC *)(void *, uint32_t))entry)(object, s_scenario);
        else ((void (TC *)(void *))entry)(object);
    } else switch (s_operation) {
        case 0: fs98_world_booster_collect(object); break;
        case 1: fs98_world_booster_populate(object); break;
        case 2: fs98_world_booster_accept(object); break;
        case 3: fs98_world_booster_path_changed(object); break;
        case 4: result = fs98_world_booster_validate(object, s_scenario); break;
        case 5: result = fs98_world_booster_error(object, s_scenario); break;
    }
    if (!s_side && !s_throw_at) {
        void *row = PTR(PTR(object, 0x10dc), 0xc);
        int bad = (s_operation == 4 && result != (s_scenario & 64 ? 0x1ae9 : 0)) ||
            (s_operation == 5 && result != -1) || (s_operation == 3 && s_calls != 15);
        if (s_operation == 0) bad |= U32(row, 0x7c) != (uint32_t)(s_selection == 1) ||
            U32(row, 0x54) != (uint32_t)(U32(object, 0x1d64) != 0);
        /* Stable scenarios independently witness all five error priorities and
         * the zero-error-return accept path, including errors in later slots. */
        if (s_operation == 2 && !(s_scenario & 3)) {
            const uint32_t bits[] = {4, 8, 16, 32, 64};
            const uint32_t controls[] = {0x1bf8, 0x1b28, 0x1c60, 0x1ca8, 0x1b90};
            uint32_t index;
            char expected[64];
            for (index = 0; index < 5; ++index) if (s_scenario & bits[index]) break;
            if (index < 5) {
                snprintf(expected, sizeof(expected), "error %x ", controls[index]);
                bad |= strstr(s_logs[0], expected) == NULL;
            } else bad |= strstr(s_logs[0], "error ") != NULL;
            bad |= (strstr(s_logs[0], "close ") != NULL) != (index == 5 || !s_error_result);
            if (index == 5 || !s_error_result) bad |= U32(PTR(object, 0x10dc), 0) != 1;
        }
        if (bad) {
            fprintf(stderr, "fixture branch self-check failed scenario=%u operation=%u\n%s", s_scenario, s_operation, s_logs[0]);
            exit(2);
        }
        ++s_self_checks;
    }
    world_log("return %x\n", (uint32_t)result);
}
static LONG CALLBACK world_exception(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode == THROW_CODE) {
        s_throw_at = 0;
        if (!s_side) ++s_throws;
        /* These six routines have no local EH frames/owned temporaries. */
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
        if (PTR(s_fixture.rows[index], 0x64))
            PTR(s_fixture.rows[index], 0x64) = (void *)0x1234;
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
    s_vtable[0xe0 / 4] = world_focus;
    s_vtable[0xec / 4] = world_close;
    memcpy(s_other_vtable, s_vtable, sizeof(s_vtable));
    s_other_vtable[0xec / 4] = world_other_close;
    world_patch(0x257e0, world_hook_title);
    fs98_world_booster_set_services(&s_booster_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        for (s_operation = 0; s_operation < 6; ++s_operation) {
            world_initialize();
            if (world_compare(0)) return 1;
            limit = s_calls - 1; /* Last call is the harness's return log. */
            if (interrupt) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World booster core: %u scenarios, 6 operations, %u comparisons, 0 differ; seed=%x interrupts=%u swaps=%u self_checks=%u\n",
        count, s_comparisons, seed, s_throws, s_swaps, s_self_checks);
    FreeLibrary(s_retail);
    return 0;
}
