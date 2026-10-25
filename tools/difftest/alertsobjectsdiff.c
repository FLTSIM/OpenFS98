/* Alerts object/factory slice: original x86 routines against compiled C.
 * MFC/SIMSUI constructors/destructors are controlled logging dependencies.
 * This checks lifecycle, factory ABI and injected dependency unwinding;
 * inherited MFC dispatch has a separate harness. The factory NULL-allocation
 * access violation is compared, including the accessed address. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/alerts/alerts_objects.h"
#include "../../src/alerts/alerts_unwind.h"

#define THISCALL FS98_ALERTS_THISCALL
#define STDCALL FS98_ALERTS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define OBJECT_BYTES 0x1600
#define LOG_BYTES 8192

typedef void *(THISCALL *fs98_construct_fn)(void *object, void *parent);
typedef void (THISCALL *fs98_destroy_fn)(void *object);
typedef void *(THISCALL *fs98_delete_fn)(void *object, uint32_t flags);
typedef uint32_t (STDCALL *fs98_create_fn)(uint32_t parent, void *descriptor);

static const uint32_t s_constructor_rvas[] = {0x12f0, 0x1a50, 0x20d0};
static const uint32_t s_destructor_rvas[] = {0x13d0, 0x1b00, 0x21b0};
static const uint32_t s_deleter_rvas[] = {0x13b0, 0x1ae0, 0x2190, 0x20b0};
static const uint32_t s_factory_rvas[] = {0x1130, 0x11c0, 0x1250};
static const fs98_construct_fn s_constructors[] = {
    fs98_alerts_modal_construct, fs98_alerts_modeless_construct, fs98_alerts_advisory_construct
};
static const fs98_destroy_fn s_destructors[] = {
    fs98_alerts_modal_destroy, fs98_alerts_modeless_destroy, fs98_alerts_advisory_destroy
};
static const fs98_delete_fn s_deleters[] = {
    fs98_alerts_modal_delete, fs98_alerts_modeless_delete,
    fs98_alerts_advisory_delete, fs98_alerts_progress_delete
};
static const fs98_create_fn s_factories[] = {
    fs98_alerts_modal_create, fs98_alerts_modeless_create, fs98_alerts_advisory_create
};
static HMODULE s_retail;
static uint8_t s_storage[OBJECT_BYTES + 32] __attribute__((aligned(16)));
static uint8_t s_initial[sizeof(s_storage)], s_retail_storage[sizeof(s_storage)];
static uint8_t s_descriptor_bytes[64];
static void *s_object;
static void *s_vtable[60];
static uint32_t s_rng, s_seed, s_scenario, s_operation;
static uint32_t s_flags, s_parent;
static int s_side, s_fail_allocate;
static char s_log[2][LOG_BYTES];
static uint32_t s_log_length[2];
static void *s_fault_jump[5];
static uint32_t s_saved_seh;
static volatile int s_running;
static uint32_t s_faults, s_heap_ui_deletes, s_heap_mfc_deletes;
static int s_throw_at, s_calls;
static uint32_t s_throws;
#define TEST_THROW 0x0e041a11
static void fs98_put(const char *format, ...);

static void fs98_maybe_throw(void)
{
    s_calls++;
    if (s_throw_at && s_calls == s_throw_at) {
        fs98_put("THROW at=%d\n", s_throw_at);
        RaiseException(TEST_THROW, 0, 0, 0);
    }
}

static uint32_t fs98_random(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static void fs98_put(const char *format, ...)
{
    va_list args;
    int written;
    uint32_t length = s_log_length[s_side];
    if (length > LOG_BYTES - 1024)
        exit(2);
    va_start(args, format);
    written = vsnprintf(s_log[s_side] + length, LOG_BYTES - length, format, args);
    va_end(args);
    if (written < 0)
        exit(2);
    s_log_length[s_side] += (uint32_t)written;
}

static uint32_t fs98_offset(const void *object)
{
    return (uint32_t)((const uint8_t *)object - (const uint8_t *)s_object);
}

static void fs98_subobject(void *self, const char *name, uint32_t size, uint8_t fill)
{
    fs98_put("%s object=%u size=%u\n", name, fs98_offset(self), size);
    fs98_maybe_throw();
    memset(self, fill, size);
}

static void *fs98_dialog_construct(void *self, int32_t template, void *parent)
{
    fs98_put("dialog.construct template=%d parent=%u object=%u\n",
             template, (uint32_t)parent, fs98_offset(self));
    fs98_maybe_throw();
    memset(self, 0x31, 0x10dc);
    D32(self, 0x20) = 0x10020;
    D32(self, 0x40) = (uint32_t)parent;
    D32(self, 0x44) = (uint32_t)template;
    return self;
}
static void fs98_dialog_destroy(void *self)
{ fs98_subobject(self, "dialog.destroy", 0x10dc, 0xd1); }
static void fs98_string_construct(void *self)
{ fs98_subobject(self, "string.construct", 4, 0x32); }
static void fs98_string_destroy(void *self)
{ fs98_subobject(self, "string.destroy", 4, 0xd2); }
static void fs98_text_construct(void *self)
{ fs98_subobject(self, "text.construct", 0xb8, 0x33); }
static void fs98_text_destroy(void *self)
{ fs98_subobject(self, "text.destroy", 0xb8, 0xd3); }
static void fs98_button_construct(void *self)
{ fs98_subobject(self, "button.construct", 0xfc, 0x34); }
static void fs98_button_destroy(void *self)
{ fs98_subobject(self, "button.destroy", 0xfc, 0xd4); }
static void fs98_window_construct(void *self)
{ fs98_subobject(self, "window.construct", 0x40, 0x35); }
static void fs98_window_destroy(void *self)
{ fs98_subobject(self, "window.destroy", 0x40, 0xd5); }
static void fs98_check_construct(void *self)
{ fs98_subobject(self, "check.construct", 0xc8, 0x36); }
static void fs98_check_destroy(void *self)
{ fs98_subobject(self, "check.destroy", 0xc8, 0xd6); }

static void *fs98_allocate(uint32_t size)
{
    fs98_put("allocate %u fail=%d\n", size, s_fail_allocate);
    fs98_maybe_throw();
    return s_fail_allocate ? 0 : s_object;
}

static void fs98_ui_delete(void *self)
{
    fs98_put("ui.delete %u\n", fs98_offset(self));
    if (!s_side) s_heap_ui_deletes++;
}

static void fs98_mfc_delete(void *self)
{
    fs98_put("mfc.delete %u\n", fs98_offset(self));
    if (!s_side) s_heap_mfc_deletes++;
}
static void fs98_dialog_delete(void *self)
{
    fs98_put("dialog.delete %u\n", fs98_offset(self));
    fs98_ui_delete(self);
}

static int32_t fs98_create(void *self, uint32_t template, void *parent)
{
    fs98_put("create object=%u template=%u parent=%u descriptor=%u module=%u\n",
             fs98_offset(self), template, (uint32_t)parent,
             PTR(self, 0x10dc) == s_descriptor_bytes, D32(self, 0x10e0));
    fs98_maybe_throw();
    return (int32_t)(s_scenario % 3); /* Includes failure, which retail ignores. */
}

static int32_t fs98_show(void *self, int32_t show)
{
    fs98_put("show object=%u flag=%d\n", fs98_offset(self), show);
    fs98_maybe_throw();
    D32(self, 0x20) = 0x20000 + (s_seed & 0xffff);
    return (int32_t)(s_scenario & 1);
}

static int32_t fs98_enable(uint32_t window, int32_t enable)
{
    fs98_put("enable hwnd=%u flag=%d\n", window, enable);
    return (int32_t)(s_seed & 1);
}

static const struct fs98_alerts_object_services s_runtime_services = {
    fs98_dialog_construct, fs98_dialog_destroy, fs98_string_construct, fs98_string_destroy,
    fs98_text_construct, fs98_text_destroy, fs98_button_construct, fs98_button_destroy,
    fs98_window_construct, fs98_window_destroy, fs98_check_construct, fs98_check_destroy,
    fs98_allocate, fs98_ui_delete, fs98_mfc_delete, fs98_create, fs98_show, fs98_enable,
    fs98_dialog_delete
};

/* IAT adapters have the dependency's actual calling convention. */
static void *THISCALL fs98_mfc_dialog(void *self, int32_t template, void *parent)
{ return fs98_dialog_construct(self, template, parent); }
#define DEFINE_CTOR(name, function) \
    static void *THISCALL name(void *self) { function(self); return self; }
#define DEFINE_DTOR(name, function) \
    static void THISCALL name(void *self) { function(self); }
DEFINE_CTOR(fs98_mfc_string, fs98_string_construct)
DEFINE_CTOR(fs98_mfc_text, fs98_text_construct)
DEFINE_CTOR(fs98_mfc_button, fs98_button_construct)
DEFINE_CTOR(fs98_mfc_window, fs98_window_construct)
DEFINE_CTOR(fs98_mfc_check, fs98_check_construct)
DEFINE_DTOR(fs98_mfc_dialog_destroy, fs98_dialog_destroy)
DEFINE_DTOR(fs98_mfc_string_destroy, fs98_string_destroy)
DEFINE_DTOR(fs98_mfc_text_destroy, fs98_text_destroy)
DEFINE_DTOR(fs98_mfc_button_destroy, fs98_button_destroy)
DEFINE_DTOR(fs98_mfc_window_destroy, fs98_window_destroy)
DEFINE_DTOR(fs98_mfc_check_destroy, fs98_check_destroy)
static int32_t THISCALL fs98_mfc_create(void *self, uint32_t template, void *parent)
{ return fs98_create(self, template, parent); }
static int32_t THISCALL fs98_mfc_show(void *self, int32_t show)
{ return fs98_show(self, show); }
static int32_t STDCALL fs98_mfc_enable(uint32_t window, int32_t enable)
{ return fs98_enable(window, enable); }
static void STDCALL fs98_mfc_dialog_delete(void *self)
{ fs98_dialog_delete(self); }
static void *THISCALL fs98_virtual_delete(void *self, uint32_t flags)
{
    fs98_put("virtual.delete object=%u flags=%u\n", fs98_offset(self), flags);
    return self;
}

static void fs98_patch(uint32_t rva, void *function)
{
    DWORD old_protect;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old_protect)) exit(2);
    *slot = function;
    VirtualProtect(slot, 4, old_protect, &old_protect);
}

static void fs98_load(void)
{
    char path[MAX_PATH];
    struct fs98_alerts_object_tables tables;
    GetFullPathNameA("orig\\English\\Alerts.DLL", MAX_PATH, path, 0);
    s_retail = LoadLibraryExA(path, 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || D32(s_retail, 0x4060) != 0x80000009 ||
        D32(s_retail, 0x401c) != (uint32_t)s_retail + 0x1130) {
        fprintf(stderr, "reference Alerts layout/load failed: %lu\n", GetLastError());
        exit(2);
    }
    fs98_patch(0x543c, fs98_mfc_dialog);
    fs98_patch(0x540c, fs98_mfc_dialog_destroy);
    fs98_patch(0x5300, fs98_mfc_string);
    fs98_patch(0x5304, fs98_mfc_string_destroy);
    fs98_patch(0x5444, fs98_mfc_text);
    fs98_patch(0x5410, fs98_mfc_text_destroy);
    fs98_patch(0x5408, fs98_mfc_button);
    fs98_patch(0x5404, fs98_mfc_button_destroy);
    fs98_patch(0x52cc, fs98_mfc_window);
    fs98_patch(0x52d0, fs98_mfc_window_destroy);
    fs98_patch(0x53e4, fs98_mfc_check);
    fs98_patch(0x5434, fs98_mfc_check_destroy);
    fs98_patch(0x5440, fs98_allocate);
    fs98_patch(0x53fc, fs98_ui_delete);
    fs98_patch(0x5448, fs98_mfc_dialog_delete);
    fs98_patch(0x533c, fs98_mfc_delete);
    fs98_patch(0x5338, fs98_mfc_create);
    fs98_patch(0x52ac, fs98_mfc_show);
    fs98_patch(0x5464, fs98_mfc_enable);
    fs98_patch(0x53c8, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "__CxxFrameHandler"));
    fs98_patch(0x53cc, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_except_handler3"));
    D32(s_retail, 0x40bc) = 0x50060;
    tables.modal = (uint8_t *)s_retail + 0x3068;
    tables.modeless = (uint8_t *)s_retail + 0x31a8;
    tables.progress = (uint8_t *)s_retail + 0x3298;
    tables.advisory = (uint8_t *)s_retail + 0x33c0;
    fs98_alerts_set_object_services(&s_runtime_services, &tables);
    fs98_alerts_set_object_module(0x50060);
}

static void fs98_fault_land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_fault_jump, 1);
}

static LONG CALLBACK fs98_exception(EXCEPTION_POINTERS *exception)
{
    if (exception->ExceptionRecord->ExceptionCode == TEST_THROW)
        return EXCEPTION_CONTINUE_SEARCH;
    if (s_running && s_fail_allocate && s_operation >= 14 && s_operation <= 16 &&
        exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        fs98_put("FAULT code=%08lx access=%u address=%u\n",
                 exception->ExceptionRecord->ExceptionCode,
                 (uint32_t)exception->ExceptionRecord->ExceptionInformation[0],
                 (uint32_t)exception->ExceptionRecord->ExceptionInformation[1]);
        if (!s_side) s_faults++;
        exception->ContextRecord->Eip = (DWORD)fs98_fault_land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    fprintf(stderr, "UNEXPECTED FAULT scenario=%u seed=%08x operation=%u side=%d "
            "code=%08lx pc=%p\n", s_scenario, s_seed, s_operation, s_side,
            exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static EXCEPTION_DISPOSITION __cdecl fs98_catch(
    EXCEPTION_RECORD *record, void *establisher, CONTEXT *context, void *dispatcher)
{
    (void)context;
    (void)dispatcher;
    if (record->ExceptionCode == TEST_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(establisher, 0, record, 0);
        fs98_put("CAUGHT\n");
        if (!s_side) s_throws++;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
        __builtin_longjmp(s_fault_jump, 1);
    }
    return ExceptionContinueSearch;
}

static void fs98_operation(void)
{
    void *return_value;
    uint32_t kind;
    if (s_operation < 3) {
        fs98_construct_fn construct = s_side ? s_constructors[s_operation] :
            (fs98_construct_fn)((uint8_t *)s_retail + s_constructor_rvas[s_operation]);
        return_value = construct(s_object, (void *)s_parent);
        fs98_put("return.object %u\n", fs98_offset(return_value));
    } else if (s_operation < 6) {
        kind = s_operation - 3;
        fs98_destroy_fn destroy = s_side ? s_destructors[kind] :
            (fs98_destroy_fn)((uint8_t *)s_retail + s_destructor_rvas[kind]);
        destroy(s_object);
    } else if (s_operation < 10) {
        kind = s_operation - 6;
        fs98_delete_fn delete = s_side ? s_deleters[kind] :
            (fs98_delete_fn)((uint8_t *)s_retail + s_deleter_rvas[kind]);
        return_value = delete(s_object, s_flags);
        fs98_put("return.object %u\n", fs98_offset(return_value));
    } else if (s_operation == 10) {
        fs98_destroy_fn post = s_side ? fs98_alerts_post_destroy :
            (fs98_destroy_fn)((uint8_t *)s_retail + 0x2590);
        post(s_scenario & 1 ? s_object : 0);
    } else if (s_operation < 13) {
        fs98_destroy_fn enable = s_side ?
            (s_operation == 11 ? fs98_alerts_disable : fs98_alerts_enable) :
            (fs98_destroy_fn)((uint8_t *)s_retail + (s_operation == 11 ? 0x1a30 : 0x1a40));
        enable(s_object);
    } else if (s_operation == 13) {
        typedef void (THISCALL *fs98_empty_fn)(void *self, uint32_t argument);
        fs98_empty_fn empty = s_side ? fs98_alerts_empty_arg :
            (fs98_empty_fn)((uint8_t *)s_retail + 0x20a0);
        empty(s_object, s_flags);
    } else if (s_operation < 17) {
        kind = s_operation - 14;
        fs98_create_fn create = s_side ? s_factories[kind] :
            (fs98_create_fn)((uint8_t *)s_retail + s_factory_rvas[kind]);
        uint32_t result = create(s_parent, s_descriptor_bytes);
        fs98_put("return.hwnd %u\n", result);
    } else {
        kind = (s_operation - 17) % 3;
        fs98_construct_fn construct = s_side ? s_constructors[kind] :
            (fs98_construct_fn)((uint8_t *)s_retail + s_constructor_rvas[kind]);
        construct(s_object, (void *)s_parent);
        if (s_operation < 20) {
            fs98_destroy_fn destroy = s_side ? s_destructors[kind] :
                (fs98_destroy_fn)((uint8_t *)s_retail + s_destructor_rvas[kind]);
            destroy(s_object);
        } else {
            fs98_delete_fn delete = s_side ? s_deleters[kind] :
                (fs98_delete_fn)((uint8_t *)s_retail + s_deleter_rvas[kind]);
            return_value = delete(s_object, s_flags);
            fs98_put("return.object %u\n", fs98_offset(return_value));
        }
    }
}

static FS98_ALERTS_NOINLINE void fs98_run(void)
{
    struct fs98_alerts_unwind catch;
    memcpy(s_storage, s_initial, sizeof(s_storage));
    PTR(s_object, 0) = s_vtable;
    s_log_length[s_side] = 0;
    s_log[s_side][0] = 0;
    s_calls = 0;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    s_running = 1;
    if (__builtin_setjmp(s_fault_jump) == 0) {
        catch.previous = (void *)s_saved_seh;
        catch.handler = fs98_catch;
        catch.cleanup = 0;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catch) : "memory");
        fs98_operation();
    }
    s_running = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
}

int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 0) : 20000;
    uint32_t index;
    static const uint32_t s_flag_cases[] = {0, 1, 2, 3, 0x100, 0x101, 0x80000000, 0xffffffff};
    if (!count) return 2;
    s_rng = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 0x587a41e9;
    if (!s_rng) return 2;
    setvbuf(stdout, 0, _IONBF, 0);
    s_object = s_storage + 16;
    s_vtable[1] = fs98_virtual_delete;
    AddVectoredExceptionHandler(1, fs98_exception);
    fs98_load();
    for (s_scenario = 0; s_scenario < count; s_scenario++) {
        s_seed = s_rng;
        for (index = 0; index < sizeof(s_storage); index++)
            s_initial[index] = (uint8_t)fs98_random();
        s_flags = s_scenario % 3 ? s_flag_cases[s_scenario % 8] : fs98_random();
        s_parent = s_scenario % 3 ? fs98_random() : 0;
        s_fail_allocate = s_scenario % 11 == 0;
        s_throw_at = argc > 3 && !strcmp(argv[3], "unwind") ?
            (int)(s_scenario % 11) + 1 : 0;
        for (s_operation = 0; s_operation < 23; s_operation++) {
            s_side = 0;
            fs98_run();
            memcpy(s_retail_storage, s_storage, sizeof(s_storage));
            s_side = 1;
            fs98_run();
            if (strcmp(s_log[0], s_log[1]) ||
                memcmp(s_retail_storage, s_storage, sizeof(s_storage))) {
                fprintf(stderr, "DIFFER scenario=%u seed=%08x operation=%u\nRETAIL:\n%sOURS:\n%s",
                        s_scenario, s_seed, s_operation, s_log[0], s_log[1]);
                return 1;
            }
        }
    }
    printf("Alerts objects: %u scenarios, %u comparisons, 0 differ\n", count, count * 23);
    printf("allocation faults=%u ui.deletes=%u mfc.deletes=%u\n",
           s_faults, s_heap_ui_deletes, s_heap_mfc_deletes);
    printf("caught dependency throws=%u\n", s_throws);
    return 0;
}
