/* World drive-search core comparison. Retail executes its own mapped code;
 * only imported UI/CRT calls and virtual callbacks are replaced by fixtures. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_search.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811
struct world_fixture {
    uint8_t before[32], object[0x1e78];
    uint8_t descriptors[2][0x84];
    char paths[2][288], titles[2][288], drives[2][32];
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
    /* Swap at every selectable boundary; count remains bounded even during
     * drive insertion. Refreshes and callback dispatch must reload correctly. */
    if (s_calls == 1 + (s_scenario / 256) % 32 && s_scenario & 1) {
        PTR(s_fixture.object, 0x10dc) = s_fixture.descriptors[1];
        if (!s_side) ++s_swaps;
    }
    if (s_scenario & 2) {
        PTR(s_fixture.object, 0) = s_other_vtable;
        U32(s_fixture.descriptors[0], 0x74) ^= 1;
        U32(s_fixture.descriptors[1], 0x78) ^= 1;
        U32(s_fixture.object, 0x1324) ^= 0x100;
    }
}
static int world_get_text(void *control, char *text, int capacity)
{
    const char *value_text;
    uint32_t offset = world_offset(control) - world_offset(s_fixture.object);
    value_text = offset == 0x1304 ? s_initial.titles[0] : s_initial.paths[0];
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
static int world_send(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{
    world_log("send %x %x %x %x\n", window, message, first, second);
    if (message == 0x143) {
        const char *drive = (const char *)(uintptr_t)second;
        if (strlen(drive) != 3 || drive[1] != ':' || drive[2] != '\\') exit(2);
    }
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
    world_log("error %x control=%x status=%x\n", world_offset(object),
        world_offset(control) - world_offset(object), (uint32_t)(uintptr_t)status);
    return s_error_result;
}
static int TC world_hook_error(void *object, void *control, void *status)
{ return world_error(object, control, status); }
static uint32_t search_callback(void *descriptor, uint32_t offset)
{
    world_log("callback %x slot=%x\n", world_offset(descriptor), offset);
    return 0xfedcba98;
}
#define SEARCH_CALLBACK(name, offset) \
    static uint32_t SC name(void *descriptor) { return search_callback(descriptor, offset); }
SEARCH_CALLBACK(world_check_descriptor, 4)
SEARCH_CALLBACK(world_add, 8)
SEARCH_CALLBACK(world_add_all, 0xc)
SEARCH_CALLBACK(world_scan, 0x10)
SEARCH_CALLBACK(world_next, 0x14)
SEARCH_CALLBACK(world_next_dir, 0x18)
static void TC world_close(void *object)
{ world_log("close %x answer=%u\n", world_offset(object), U32(PTR(object, 0x10dc), 0)); }
static void TC world_other_close(void *object)
{ world_log("other.close\n"); world_close(object); }
static const struct fs98_world_services s_services = {
    world_get_text, world_set_text, NULL, NULL, world_send,
    world_check_control, world_enable, world_error, NULL
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
static int world_post(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{
    world_log("post %x %x %x %x\n", window, message, first, second);
    return -1;
}
static int SC world_hook_post(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ return world_post(window, message, first, second); }
static const struct fs98_world_search_services s_search_services = {&s_services, world_title, world_post};
static const uint32_t s_entries[] = {
    0xe2d0, 0xe560, 0xe270, 0xe210, 0xe320, 0xe370, 0xe3c0,
    0xe430, 0xe470, 0xe460, 0xe4e0, 0xe500, 0xe760
};
static void world_initialize(void)
{
    uint32_t index, byte;
    static const uint32_t s_values[] = {0, 1, 2, 0xffffffff, 0x80000000, 0x1234ffff, 65536, 65535};
    static const uint32_t s_layers[] = {10, 20, 30, 40, 50, 0, 9, 11, 49, 51, 0xffffffff};
    for (byte = 0; byte < sizeof(s_fixture); ++byte) ((uint8_t *)&s_fixture)[byte] = (uint8_t)world_random();
    PTR(s_fixture.object, 0) = s_vtable;
    PTR(s_fixture.object, 0x10dc) = s_fixture.descriptors[0];
    U32(s_fixture.object, 0x1c70) = s_values[s_scenario % 8];
    for (index = 0; index < 2; ++index) {
        void *descriptor = s_fixture.descriptors[index];
        PTR(descriptor, 4) = world_check_descriptor;
        PTR(descriptor, 8) = world_add;
        PTR(descriptor, 0xc) = world_add_all;
        PTR(descriptor, 0x10) = world_scan;
        PTR(descriptor, 0x14) = world_next;
        PTR(descriptor, 0x18) = world_next_dir;
        U32(descriptor, 0x20) = (s_scenario ^ (index ? 28 : 0)) & 4 ? 0x1006 : 0;
        U32(descriptor, 0x34) = (s_scenario ^ (index ? 28 : 0)) & 8 ? 0x1005 : 0;
        U32(descriptor, 0x48) = (s_scenario ^ (index ? 28 : 0)) & 16 ? 0x1009 : 0;
        PTR(descriptor, 0x24) = s_fixture.paths[index];
        PTR(descriptor, 0x38) = s_fixture.titles[index];
        memset(s_fixture.paths[index], 'P' + index, 270);
        s_fixture.paths[index][270] = 0;
        memset(s_fixture.titles[index], 'C' + index, 270);
        s_fixture.titles[index][270] = 0;
        memcpy(s_fixture.drives[index], "C:\\\0D:\\\0E:\\\0F:\\\0G:\\\0H:\\\0", 24);
        PTR(descriptor, 0x70) = s_fixture.drives[index];
        U32(descriptor, 0x68) = (uint32_t)((int)((s_scenario + index) % 8) - 2);
        U32(descriptor, 0x6c) = s_values[(s_scenario / 8 + index) % 8];
        U32(descriptor, 0x4c) = s_layers[(s_scenario / 64 + index) % 11];
        U32(descriptor, 0x54) = s_values[(s_scenario / 16 + index) % 8];
        U32(descriptor, 0x74) = s_values[(s_scenario / 32 + index) % 8];
        U32(descriptor, 0x78) = s_values[(s_scenario / 128 + index) % 8];
        U32(descriptor, 0x7c) = s_values[(s_scenario / 512 + index) % 8];
        U32(descriptor, 0x80) = s_values[(s_scenario / 1024 + index) % 8];
    }
    s_selection = (int)(s_scenario % 8) - 2;
    s_error_result = s_scenario & 32 ? -1 : 0;
    s_initial = s_fixture;
}
static void world_invoke(void)
{
    void *object = s_fixture.object;
    void *entry = (uint8_t *)s_retail + s_entries[s_operation];
    int result = 0;
    if (!s_side) {
        if (s_operation == 12) result = ((int (TC *)(void *))entry)(object);
        else ((void (TC *)(void *))entry)(object);
    } else switch (s_operation) {
        case 0: fs98_world_search_collect(object); break;
        case 1: fs98_world_search_populate(object); break;
        case 2: fs98_world_search_add(object); break;
        case 3: fs98_world_search_add_all(object); break;
        case 4: fs98_world_search_next(object); break;
        case 5: fs98_world_search_next_dir(object); break;
        case 6: fs98_world_search_scan(object); break;
        case 7: fs98_world_search_drive(object); break;
        case 8: case 9: fs98_world_search_layer(object); break;
        case 10: case 11: fs98_world_search_cancel(object); break;
        case 12: result = fs98_world_search_validate(object); break;
    }
    if (!s_side && !s_throw_at) {
        int bad = 0;
        void *descriptor = PTR(object, 0x10dc);
        if (s_operation == 0) bad |= U32(descriptor, 0x54) != U32(object, 0x1c70);
        if (s_operation == 7) bad |= U32(descriptor, 0x6c) != (uint32_t)s_selection;
        if (s_operation == 8 || s_operation == 9) {
            uint32_t expected = (uint32_t)s_selection <= 4 ?
                (uint32_t)(s_selection + 1) * 10 : U32(s_initial.descriptors[0], 0x4c);
            bad |= U32(s_fixture.descriptors[0], 0x4c) != expected;
        }
        if (s_operation == 10 || s_operation == 11) bad |= U32(s_fixture.descriptors[0], 0) != 0;
        if (s_operation == 12) {
            /* Validation reads status after callback #1. Error itself may
             * swap the descriptor at boundary #2, after that decision. */
            descriptor = s_initial.descriptors[(s_scenario & 1) && (s_scenario / 256) % 32 == 0];
            const char *expected = U32(descriptor, 0x20) ? "control=16c0 status=1006" :
                U32(descriptor, 0x34) ? "control=1304 status=1005" : "control=1830 status=1009";
            int has_error = U32(descriptor, 0x20) || U32(descriptor, 0x34) || U32(descriptor, 0x48);
            bad |= result != !has_error;
            bad |= has_error ? strstr(s_logs[0], expected) == NULL : strstr(s_logs[0], "error ") != NULL;
        }
        if (s_operation == 6) bad |= strstr(s_logs[0], "post ") == NULL;
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
        /* These core routines have no local EH frames/owned temporaries. */
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
    world_patch(0x25598, world_hook_get); world_patch(0x25700, world_hook_get);
    world_patch(0x255bc, world_hook_set); world_patch(0x257e0, world_hook_title);
    world_patch(0x255c8, world_hook_enable);
    world_patch(0x25778, world_hook_error); world_patch(0x2577c, world_hook_check);
    world_patch(0x258b0, world_hook_send); world_patch(0x258a0, world_hook_post);
    s_vtable[0xec / 4] = world_close;
    memcpy(s_other_vtable, s_vtable, sizeof(s_vtable));
    s_other_vtable[0xec / 4] = world_other_close;
    fs98_world_search_set_services(&s_search_services);
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        for (s_operation = 0; s_operation < 13; ++s_operation) {
            world_initialize();
            if (world_compare(0)) return 1;
            limit = s_calls - 1; /* Last call is the harness's return log. */
            if (interrupt) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World search core: %u scenarios, 13 operations, %u comparisons, 0 differ; seed=%x interrupts=%u swaps=%u self_checks=%u\n",
        count, s_comparisons, seed, s_throws, s_swaps, s_self_checks);
    FreeLibrary(s_retail);
    return 0;
}
