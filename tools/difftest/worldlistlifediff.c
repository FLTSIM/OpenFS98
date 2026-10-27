/* Retail scenery-area list lifetimes with real x86 exception unwinding. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_list.h"
#include "../../src/world/world_list_tables.inc"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429812
struct world_life_fixture {
    uint8_t before[32], object[0x1c8c], after[32];
    uint8_t descriptor[0x30], exchange[32], help[32];
    uint32_t module, ui_deletes, dialog_deletes;
};
static struct world_life_fixture s_fixture, s_initial, s_reference;
static HMODULE s_retail;
static uint32_t s_rng, s_seed, s_scenario, s_operation, s_calls, s_throw_at;
static uint32_t s_length, s_saved_seh, s_comparisons, s_throws, s_faults, s_checks, s_exits;
static int s_side, s_typed;
static void (SC *s_throw)(void *object, void *info);
struct world_type_descriptor { void *vtable, *spare; char name[40]; };
static struct world_type_descriptor s_base_type = {NULL, NULL, ".PAVCException@@"};
static struct world_type_descriptor s_derived_type = {NULL, NULL, ".PAVCFixtureException@@"};
static struct world_type_descriptor s_const_type = {NULL, NULL, ".PBVCException@@"};
static struct world_type_descriptor s_other_type = {NULL, NULL, ".PAX"};
static uint32_t s_catchable[2][7], s_types[3], s_throw_info[4];
static char s_logs[2][8192];
static void *s_jump[5], *s_vtable[60];
static uint32_t world_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t world_offset(const void *ptr)
{ return (uint32_t)((const uint8_t *)ptr - (const uint8_t *)&s_fixture); }
static void world_log(const char *format, ...)
{
    va_list args; int length;
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_length, 8192 - s_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= 8192 - s_length) exit(2);
    s_length += (uint32_t)length;
    if (++s_calls == s_throw_at) {
        if (s_typed) {
            void *exception = s_scenario % 6 == 3 ? NULL : s_fixture.descriptor;
            s_throw(&exception, s_throw_info);
            exit(2);
        }
        RaiseException(THROW_CODE, 0, 0, NULL);
    }
    if (s_scenario & 1) {
        s_fixture.module ^= 0xa137be59;
        if (s_side) fs98_world_list_set_module(s_fixture.module);
        else U32(s_retail, 0x24974) = s_fixture.module;
    }
}
static void *TC world_virtual_delete(void *object, uint32_t flags)
{
    world_log("virtual.delete %x flags=%x\n", world_offset(object), flags);
    U32(object, 4) = flags;
    return object;
}

static void *world_base_construct(void *object, uint32_t template_id, void *parent)
{
    world_log("base %x template=%x parent=%x\n", world_offset(object), template_id, (uint32_t)parent);
    U32(object, 0) = 0xba5e0000; U32(object, 4) = s_seed;
    return object;
}
#define WORLD_BASE_HOOK(slot) \
static void *TC world_hook_##slot(void *object, uint32_t template_id, void *parent) \
{ return world_base_construct(object, template_id, parent); }
#define WORLD_CTOR_HOOK(slot) \
static void TC world_hook_##slot(void *object) \
{ world_log("construct %x import=%x\n", world_offset(object), 0x##slot); \
  U32(object, 0) = 0x##slot; U32(object, 4) = s_seed; }
#define WORLD_DTOR_HOOK(slot) \
static void TC world_hook_##slot(void *object) \
{ world_log("destroy %x import=%x marker=%x\n", world_offset(object), 0x##slot, U32(object, 0)); \
  U32(object, 0) = 0x##slot; }
#include "../../src/world/world_list_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
static void world_construct(void *object, uint32_t import_slot)
{
    switch (import_slot) {
#define WORLD_BASE_HOOK(slot)
#define WORLD_CTOR_HOOK(slot) case 0x##slot: world_hook_##slot(object); break;
#define WORLD_DTOR_HOOK(slot)
#include "../../src/world/world_list_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
        default: exit(2);
    }
}
static void world_destroy(void *object, uint32_t import_slot)
{
    switch (import_slot) {
#define WORLD_BASE_HOOK(slot)
#define WORLD_CTOR_HOOK(slot)
#define WORLD_DTOR_HOOK(slot) case 0x##slot: world_hook_##slot(object); break;
#include "../../src/world/world_list_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
        default: exit(2);
    }
}
static void *world_allocate(uint32_t bytes)
{
    world_log("allocate %x\n", bytes);
    return s_scenario % 11 ? s_fixture.object : NULL;
}
static void world_ui_delete(void *object)
{ world_log("ui.delete %x\n", world_offset(object)); ++s_fixture.ui_deletes; }
static void SC world_dialog_delete(void *object)
{ world_log("dialog.delete %x\n", world_offset(object)); ++s_fixture.dialog_deletes; }
static void world_dialog_delete_service(void *object) { world_dialog_delete(object); }
static int world_create(void *object, uint32_t template_id, void *parent)
{
    world_log("create %x template=%x parent=%x descriptor=%x module=%x\n", world_offset(object),
        template_id, (uint32_t)parent, world_offset(PTR(object, 0x10dc)), U32(object, 0x80));
    return s_scenario & 2 ? 0 : (int)(s_seed | 1);
}
static int TC world_hook_create(void *object, uint32_t template_id, void *parent)
{ return world_create(object, template_id, parent); }
static int world_show(void *object, int show)
{
    world_log("show %x %d\n", world_offset(object), show);
    U32(object, 0x20) = s_seed ^ 0x9f781b27;
    return -1;
}
static int TC world_hook_show(void *object, int show) { return world_show(object, show); }
static int SC world_message(const char *text, uint32_t flags, uint32_t help)
{ world_log("message [%s] flags=%x help=%x\n", text, flags, help); return (int)s_seed; }
static int world_message_service(const char *text, uint32_t flags, uint32_t help)
{ return world_message(text, flags, help); }
static void SC world_ddx(void *exchange, uint32_t id, void *control)
{
    world_log("ddx %x id=%x control=%x\n", world_offset(exchange), id, world_offset(control));
    U32(control, 0x20) = id ^ s_seed;
}
static void world_ddx_service(void *exchange, uint32_t id, void *control)
{ world_ddx(exchange, id, control); }
static int SC world_help(uint32_t window, const char *filename, uint32_t command, const uint32_t *map)
{
    uint32_t index;
    /* One boundary, one injectable call, but include every help-map pair. */
    char map_text[1024]; int length = 0;
    for (index = 0; index < 64; index += 2) {
        length += snprintf(map_text + length, sizeof(map_text) - (uint32_t)length,
            "%x:%x,", map[index], map[index + 1]);
        if (!map[index] && !map[index + 1]) break;
        if (length > 950) exit(2);
    }
    if (index == 64) exit(2);
    world_log("help %x [%s] command=%x map=[%s]\n", window, filename, command, map_text);
    return (int)s_seed;
}
static int world_help_service(uint32_t window, const char *filename, uint32_t command, const uint32_t *map)
{ return world_help(window, filename, command, map); }
static void world_land(void);
static void world_exit_cleanly(void *exception)
{
    s_throw_at = 0;
    world_log("exit.cleanly %x\n", exception ? world_offset(exception) : 0);
    if (!s_side) ++s_exits;
    /* SIMSUI displays an error and aborts; terminate this comparison side. */
    world_land();
}
static const struct fs98_world_object_services s_services = {
    world_base_construct, world_construct, world_destroy, world_allocate,
    world_ui_delete, world_dialog_delete_service, world_create, world_show,
    world_message_service, world_ddx_service, world_exit_cleanly, world_help_service
};
static const uint32_t s_entries[] = {0xa260, 0xa3a0, 0xa380, 0xa260, 0x12e0, 0xa4b0, 0xacd0, 0xacf0, 0x3d30};
static void world_patch(uint32_t rva, void *value)
{
    DWORD old; void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) exit(2);
    *slot = value;
    if (!VirtualProtect(slot, 4, old, &old)) exit(2);
}
static void world_initialize(void)
{
    uint32_t byte;
    for (byte = 0; byte < sizeof(s_fixture); ++byte)
        ((uint8_t *)&s_fixture)[byte] = (uint8_t)world_random();
    s_fixture.module = s_seed;
    s_fixture.ui_deletes = s_fixture.dialog_deletes = 0;
    memset(s_catchable, 0, sizeof(s_catchable));
    memset(s_types, 0, sizeof(s_types));
    memset(s_throw_info, 0, sizeof(s_throw_info));
    s_catchable[0][0] = s_catchable[1][0] = 1;
    s_catchable[0][3] = s_catchable[1][3] = 0xffffffff;
    s_catchable[0][5] = s_catchable[1][5] = 4;
    s_catchable[0][1] = (uint32_t)(s_scenario % 6 == 2 ? &s_other_type :
        (s_scenario % 6 == 1 || s_scenario % 6 == 4) ? &s_derived_type :
        s_scenario % 6 == 5 ? &s_const_type : &s_base_type);
    s_catchable[1][1] = (uint32_t)&s_base_type;
    s_catchable[1][2] = 8; /* derived-to-base pointer adjustment */
    if (s_scenario % 6 == 4) {
        /* Exercise the full PMD conversion through a virtual-base table. */
        PTR(s_fixture.descriptor, 4) = s_fixture.help;
        U32(s_fixture.help, 4) = 12;
        s_catchable[1][3] = 4;
        s_catchable[1][4] = 4;
    }
    s_types[0] = (s_scenario % 6 == 1 || s_scenario % 6 == 4) ? 2 : 1;
    s_types[1] = (uint32_t)s_catchable[0];
    s_types[2] = (uint32_t)s_catchable[1];
    s_throw_info[3] = (uint32_t)s_types;
    if (s_operation == 8) {
        PTR(s_fixture.object, 0) = s_vtable;
        PTR(s_fixture.object, 0x10dc) = s_fixture.descriptor;
    }
    s_initial = s_fixture;
}
static void world_invoke(void)
{
    void *object = s_fixture.object;
    void *entry = (uint8_t *)s_retail + s_entries[s_operation];
    uint32_t result = 0, expected_calls, index;
    if (s_operation == 0 || s_operation == 3) {
        void *returned = s_side ? fs98_world_list_construct(object, (void *)s_seed) :
            ((void *(TC *)(void *, void *))entry)(object, (void *)s_seed);
        result = world_offset(returned);
        if (s_operation == 3) {
            if (s_side) returned = fs98_world_list_scalar_delete(object, s_seed);
            else returned = ((void *(TC *)(void *, uint32_t))((uint8_t *)s_retail + 0xa380))(object, s_seed);
            result = world_offset(returned);
        }
    } else if (s_operation == 1) {
        if (s_side) fs98_world_list_destroy(object);
        else ((void (TC *)(void *))entry)(object);
    } else if (s_operation == 2) {
        void *returned = s_side ? fs98_world_list_scalar_delete(object, s_seed) :
            ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
        result = world_offset(returned);
    } else if (s_operation == 4) {
        if (s_side) entry = fs98_world_list_create;
        result = ((uint32_t (SC *)(uint32_t, void *))entry)(s_seed, s_fixture.descriptor);
    } else if (s_operation == 5) {
        if (s_side) fs98_world_list_exchange(object, s_fixture.exchange);
        else ((void (TC *)(void *, void *))entry)(object, s_fixture.exchange);
    } else if (s_operation == 6) {
        result = s_side ? (uint32_t)fs98_world_list_help(object, s_seed, s_fixture.help) :
            ((uint32_t (TC *)(void *, uint32_t, void *))entry)(object, s_seed, s_fixture.help);
    } else if (s_operation == 7) {
        result = s_side ? (uint32_t)fs98_world_list_context(object, s_seed, s_seed ^ 1) :
            ((uint32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 1);
    }
    if (s_operation == 8) {
        void *target = s_scenario & 1 ? object : NULL;
        if (s_side) fs98_world_list_post_destroy(target);
        else ((void (TC *)(void *))entry)(target);
    }
    if (!s_side && !s_throw_at) {
        expected_calls = s_operation <= 2 ? 15u + (s_operation == 2 && (s_seed & 1)) :
            s_operation == 3 ? 30 + (s_seed & 1) :
            s_operation == 4 ? 18u + ((s_scenario & 2) != 0) : s_operation == 5 ? 14 : s_operation == 8 ? s_scenario & 1 : 1;
        if (s_calls != expected_calls ||
            (s_operation == 0 && (PTR(object, 0) != (uint8_t *)s_retail + WORLD_LIST_VTABLE_RVA)) ||
            (s_operation == 4 && result != (s_seed ^ 0x9f781b27)) ||
            (s_operation == 7 && result != 1) ||
            (s_operation == 8 && (s_scenario & 1) && U32(object, 4) != 1)) {
            fprintf(stderr, "fixture self-check failed op=%u calls=%u expected=%u\n", s_operation, s_calls, expected_calls);
            exit(2);
        }
        /* Independent successful-path witnesses: each embedded service wrote
         * its marker, and deleting destruction released only flag bit zero. */
        if (s_operation <= 4) {
            int constructed = s_operation == 0 || s_operation == 4;
            for (index = 0; index < sizeof(s_list_controls) / sizeof(s_list_controls[0]); ++index) {
                const struct fs98_world_control *control = &s_list_controls[index];
                uint32_t marker = constructed ? control->constructor : control->destructor;
                if (U32(object, control->offset) != marker) {
                    fprintf(stderr, "control witness failed op=%u index=%u\n", s_operation, index);
                    exit(2);
                }
            }
            if ((!constructed && U32(object, 0) != WORLD_LIST_BASE_DTOR) ||
                s_fixture.dialog_deletes != 0 ||
                s_fixture.ui_deletes != (uint32_t)((s_operation == 2 || s_operation == 3) && (s_seed & 1))) {
                fprintf(stderr, "lifetime witness failed op=%u\n", s_operation);
                exit(2);
            }
        }
        ++s_checks;
    }
    world_log("return %x\n", result);
}
static void world_land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_jump, 1);
}
static EXCEPTION_DISPOSITION __cdecl world_catch(EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if ((record->ExceptionCode == THROW_CODE || record->ExceptionCode == 0xe06d7363) && !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, NULL, record, NULL);
        s_throw_at = 0;
        world_log(record->ExceptionCode == THROW_CODE ? "caught\n" : "caught.cpp\n");
        if (!s_side) ++s_throws;
        world_land();
    }
    return ExceptionContinueSearch;
}
static LONG CALLBACK world_fault(EXCEPTION_POINTERS *exception)
{
    if ((s_operation == 4) && s_scenario % 11 == 0 &&
        exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        exception->ExceptionRecord->ExceptionInformation[0] == 1 &&
        exception->ExceptionRecord->ExceptionInformation[1] == 0x10dc) {
        s_throw_at = 0;
        world_log("allocation.fault write=10dc\n");
        if (!s_side) ++s_faults;
        exception->ContextRecord->Eip = (DWORD)world_land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static LONG WINAPI world_fatal(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u op=%u throw=%u\n", exception->ExceptionRecord->ExceptionCode,
        exception->ExceptionRecord->ExceptionAddress, s_side, s_scenario, s_operation, s_throw_at);
    ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static __attribute__((noinline)) void world_run(uint32_t throw_at)
{
    struct { void *previous, *handler; } catcher;
    s_fixture = s_initial;
    s_calls = s_length = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    U32(s_retail, 0x24974) = s_fixture.module;
    fs98_world_list_set_module(s_fixture.module);
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_saved_seh; catcher.handler = world_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        world_invoke();
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    if (memcmp(s_fixture.before, s_initial.before, 32) || memcmp(s_fixture.after, s_initial.after, 32)) {
        fprintf(stderr, "fixture guard damaged\n"); exit(2);
    }
}
static int world_compare(uint32_t throw_at)
{
    uint32_t byte;
    s_side = 0; world_run(throw_at); s_reference = s_fixture;
    s_side = 1; world_run(throw_at); ++s_comparisons;
    if (!memcmp(&s_reference, &s_fixture, sizeof(s_fixture)) && !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u operation=%u throw=%u seed=%x\n", s_scenario, s_operation, throw_at, s_seed);
    for (byte = 0; byte < sizeof(s_fixture); ++byte) if (((uint8_t *)&s_reference)[byte] != ((uint8_t *)&s_fixture)[byte]) {
        fprintf(stderr, "byte=%x retail=%x ours=%x\n", byte, ((uint8_t *)&s_reference)[byte], ((uint8_t *)&s_fixture)[byte]); break;
    }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]); return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], NULL, 0) : 2000;
    uint32_t seed = argc > 2 ? strtoul(argv[2], NULL, 0) : 0x4a971b63;
    uint32_t position, limit;
    int unwind = argc > 3 && (!strcmp(argv[3], "unwind") || !strcmp(argv[3], "typed"));
    s_typed = argc > 3 && !strcmp(argv[3], "typed");
    if (!count || !seed) return 2;
    s_rng = seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_fault); SetUnhandledExceptionFilter(world_fatal);
    s_retail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x24120)) return 2;
#define WORLD_BASE_HOOK(slot) world_patch(0x##slot, world_hook_##slot);
#define WORLD_CTOR_HOOK(slot) world_patch(0x##slot, world_hook_##slot);
#define WORLD_DTOR_HOOK(slot) world_patch(0x##slot, world_hook_##slot);
#include "../../src/world/world_list_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
    s_throw = (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_CxxThrowException");
    if (!s_throw) return 2;
    world_patch(0x25848, world_exit_cleanly);
    world_patch(0x2584c, world_allocate); world_patch(0x25800, world_ui_delete);
    world_patch(0x25850, world_dialog_delete); world_patch(0x25524, world_hook_create);
    world_patch(0x25500, world_hook_show); world_patch(0x2551c, world_message);
    world_patch(0x255fc, world_ddx); world_patch(0x258b8, world_help);
    world_patch(0x256bc, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "__CxxFrameHandler"));
    fs98_world_list_set_object_services(&s_services, (uint8_t *)s_retail + WORLD_LIST_VTABLE_RVA);
    s_vtable[1] = world_virtual_delete;
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        s_seed = world_random();
        for (s_operation = 0; s_operation < 9; ++s_operation) {
            world_initialize(); if (world_compare(0)) return 1;
            limit = s_calls - 1;
            if (unwind) for (position = 1; position <= limit; ++position)
                if (world_compare(position)) return 1;
        }
    }
    printf("World area-list lifetime: %u scenarios, 9 operations, %u comparisons, 0 differ; seed=%x unwinds=%u allocation_faults=%u self_checks=%u exits=%u\n",
        count, s_comparisons, seed, s_throws, s_faults, s_checks, s_exits);
    FreeLibrary(s_retail); return 0;
}
