/* Flights' dialog routines, the original PE32 code against C (src/flights/).
 *
 *   flightseventsdiff.exe [scenarios] [seed] [normal|unwind]
 *
 * Retail Flights.DLL is mapped without resolving its imports, and every import slot the
 * tested routines use is pointed at a logging stub. Each operation then runs twice from
 * the same randomised fixture, retail then ours, and the call logs, return values and
 * every fixture byte must agree. A stub may swap the descriptor, change the list's count
 * and selection, replace the vtable or alter a window handle mid-call, at a position the
 * scenario chooses. In unwind mode each operation is repeated with a C++-style throw at
 * every call position, comparing what the cleanup frames destroy.
 *
 * Covered: the picker family (Adventures 0x1108, Challenges 0x1107) and the helpers every
 * class shares. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/flights/flights_internal.h"
#include "../../src/flights/flights_objects.h"
#include "../../src/flights/flights_objects_tables.inc"

#define TC FS98_FLIGHTS_THISCALL
#define SC FS98_FLIGHTS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define TEST_THROW 0x0e041a22
#define LOG_SIZE 32768
#define BASE 0x22310000
#define RESOURCE_MODULE 0x31415926
#define OBJECT_MODULE 0x27182818

struct fixture {
    uint8_t guard_before[32];
    uint8_t object[0x15e4];
    uint8_t descriptor[2][0x18];
    uint8_t records[5][0xa08];
    uint8_t info[32];
    uint8_t exchange[32];
    uint8_t font[32];
    uint8_t guard_after[32];
};
static struct fixture s_fixture, s_initial, s_retail_result;
static HMODULE s_retail;
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_script;
static uint32_t s_calls, s_log_length, s_throw_at, s_saved_seh;
static uint32_t s_max_calls, s_throws, s_comparisons, s_swaps, s_allocation_faults, s_no_selection;
static int s_side;
static char s_logs[2][LOG_SIZE];
static void *s_vtable[64], *s_other_vtable[64], *s_jump[5];

enum signature {
    SIG_EVENT, SIG_HELP, SIG_CONTEXT, SIG_EXCHANGE, SIG_INT, SIG_EMPTY_ARG,
    SIG_CONSTRUCT, SIG_DESTROY, SIG_DELETE, SIG_CREATE, SIG_ROUNDTRIP, SIG_CONSTRUCT_DELETE
};
struct operation { uint32_t rva; void *ours; enum signature signature; uint32_t kind; const char *name; };
#define EVENT(rva, name) {rva, (void *)fs98_flights_##name, SIG_EVENT, 0, #name}
#define INT(rva, name) {rva, (void *)fs98_flights_##name, SIG_INT, 0, #name}
#define CLASS(help, context, ddx, name) \
    {help, (void *)fs98_flights_##name##_help, SIG_HELP, 0, #name "_help"}, \
    {context, (void *)fs98_flights_##name##_context, SIG_CONTEXT, 0, #name "_context"}, \
    {ddx, (void *)fs98_flights_##name##_exchange, SIG_EXCHANGE, 0, #name "_exchange"}
#define LIFETIME(kind, name) \
    {0, (void *)fs98_flights_##name##_construct, SIG_CONSTRUCT, kind, #name "_construct"}, \
    {0, (void *)fs98_flights_##name##_destroy, SIG_DESTROY, kind, #name "_destroy"}, \
    {0, (void *)fs98_flights_##name##_delete, SIG_DELETE, kind, #name "_delete"}, \
    {0, (void *)fs98_flights_##name##_create, SIG_CREATE, kind, #name "_create"}, \
    {0, (void *)fs98_flights_##name##_construct, SIG_ROUNDTRIP, kind, #name "_roundtrip"}, \
    {0, (void *)fs98_flights_##name##_construct, SIG_CONSTRUCT_DELETE, kind, #name "_construct_delete"}
static const struct operation s_operations[] = {
    EVENT(0x3040, picker_select), EVENT(0x5240, picker_cancel), EVENT(0x54c0, picker_sync),
    EVENT(0x2e50, adventures_accept), EVENT(0x30f0, adventures_populate),
    EVENT(0x3090, adventures_double_click),
    EVENT(0x5260, challenges_accept), EVENT(0x54f0, challenges_populate),
    EVENT(0x5460, challenges_double_click),
    EVENT(0x26a0, post_destroy), EVENT(0x2330, disable), EVENT(0x2340, enable), EVENT(0x2320, empty),
    {0x2310, (void *)fs98_flights_empty_arg, SIG_EMPTY_ARG, 0, "empty_arg"},
    INT(0x5580, picker_no_error), INT(0x5590, picker_validate),
    INT(0x2ec0, adventures_init), INT(0x52e0, challenges_init),
    CLASS(0x30b0, 0x30d0, 0x2db0, adventures),
    CLASS(0x5480, 0x54a0, 0x51a0, challenges),
    LIFETIME(FS98_FLIGHTS_ADVENTURES, adventures),
    LIFETIME(FS98_FLIGHTS_CHALLENGES, challenges),
};
#define OP_COUNT (sizeof(s_operations) / sizeof(s_operations[0]))
#define OP s_operations[s_operation]

static uint32_t next_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t offset(const void *pointer)
{ return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture); }
static void put(const char *format, ...)
{
    va_list args;
    int length;
    if (s_log_length > LOG_SIZE - 2048) exit(2);
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_log_length, LOG_SIZE - s_log_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= LOG_SIZE - s_log_length) exit(2);
    s_log_length += (uint32_t)length;
    ++s_calls;
    if (s_throw_at && s_calls == s_throw_at) RaiseException(TEST_THROW, 0, 0, 0);
}
/* At one call per operation: swap the descriptor for the other, shrink the list to two
 * entries with the second selected, replace the vtable, change the list's window. */
static void mutate(void)
{
    uint8_t *object = s_fixture.object;
    void *descriptor = PTR(object, 0x10dc);
    if (s_calls != 1 + (s_scenario % 4)) return;
    if ((s_script & 1) && (descriptor == s_fixture.descriptor[0] || descriptor == s_fixture.descriptor[1])) {
        PTR(object, 0x10dc) = descriptor == s_fixture.descriptor[0] ? s_fixture.descriptor[1]
                                                                    : s_fixture.descriptor[0];
        if (!s_side) ++s_swaps;
    }
    descriptor = PTR(object, 0x10dc);
    if ((s_script & 2) && (descriptor == s_fixture.descriptor[0] || descriptor == s_fixture.descriptor[1])) {
        D32(descriptor, 8) = 2;
        D32(descriptor, 0xc) = 1;
    }
    if (s_script & 4) PTR(object, 0) = s_other_vtable;
    if (s_script & 8) D32(object, 0x1270) = s_seed ^ 0x12345678;
}

/* ---- the object's own virtuals --------------------------------------------------------- */
static void TC virtual_close(void *object)
{
    put("virtual.close %u state=%u answers=%u/%u\n", offset(object), D32(object, 0x15e0),
        D32(s_fixture.descriptor[0], 0), D32(s_fixture.descriptor[1], 0));
    mutate();
}
static void TC virtual_close_other(void *object) { put("other.close\n"); virtual_close(object); }
static void TC virtual_accept(void *object) { put("virtual.accept %u\n", offset(object)); mutate(); }
static void TC virtual_accept_other(void *object) { put("other.accept\n"); virtual_accept(object); }
static void TC virtual_cancel(void *object) { put("virtual.cancel %u\n", offset(object)); mutate(); }
static void TC virtual_cancel_other(void *object) { put("other.cancel\n"); virtual_cancel(object); }
static void *TC virtual_delete(void *object, uint32_t flags)
{ put("virtual.delete %u flags=%u\n", offset(object), flags); mutate(); return object; }
/* Every other slot: a wrong slot shows up in the log instead of jumping through null */
#define OTHER_SLOT(n) static void TC virtual_slot_##n(void *object) \
{ put("virtual.slot %x %u\n", n * 4, offset(object)); mutate(); }
#define OTHER_SLOTS(n) OTHER_SLOT(n##0) OTHER_SLOT(n##1) OTHER_SLOT(n##2) OTHER_SLOT(n##3) \
    OTHER_SLOT(n##4) OTHER_SLOT(n##5) OTHER_SLOT(n##6) OTHER_SLOT(n##7) \
    OTHER_SLOT(n##8) OTHER_SLOT(n##9) OTHER_SLOT(n##a) OTHER_SLOT(n##b) \
    OTHER_SLOT(n##c) OTHER_SLOT(n##d) OTHER_SLOT(n##e) OTHER_SLOT(n##f)
OTHER_SLOTS(0x0) OTHER_SLOTS(0x1) OTHER_SLOTS(0x2) OTHER_SLOTS(0x3)
#define SLOT_ROW(n) virtual_slot_##n##0, virtual_slot_##n##1, virtual_slot_##n##2, virtual_slot_##n##3, \
    virtual_slot_##n##4, virtual_slot_##n##5, virtual_slot_##n##6, virtual_slot_##n##7, \
    virtual_slot_##n##8, virtual_slot_##n##9, virtual_slot_##n##a, virtual_slot_##n##b, \
    virtual_slot_##n##c, virtual_slot_##n##d, virtual_slot_##n##e, virtual_slot_##n##f
static void (TC *const s_other_slots[64])(void *) = {
    SLOT_ROW(0x0), SLOT_ROW(0x1), SLOT_ROW(0x2), SLOT_ROW(0x3)
};
static int32_t TC virtual_skin(void *object, int32_t resource, uint32_t module, uint32_t flags)
{
    put("skin %u resource=%x module=%x flags=%u\n", offset(object), resource, module, flags);
    mutate(); return s_scenario % 4 ? (int32_t)s_seed | 1 : 0;
}

/* ---- services ------------------------------------------------------------------------- */
static int32_t send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{
    uint32_t result = s_seed;
    if (message == 0x180) put("message %u %x %u text=%s\n", window, message, w_param, (char *)(uintptr_t)l_param);
    else put("message %u %x %u %u\n", window, message, w_param, l_param);
    mutate();
    if (message == 0x188) {
        result = s_scenario % 5 == 0 ? 0xffffffff : (s_scenario + s_calls) % 4;
        if (result == 0xffffffff && !s_side) ++s_no_selection;
    }
    return (int32_t)result;
}
static int32_t win_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{
    uint32_t index = 0;
    char pairs[1024];
    int length = 0;
    do {
        if (index >= 128 || length > 900) exit(2);
        length += snprintf(pairs + length, sizeof(pairs) - length, "%x:%x,", table[index], table[index + 1]);
        index += 2;
    } while (table[index - 2] || table[index - 1]);
    put("help %u %s %u [%s]\n", window, file, command, pairs);
    mutate(); return (int32_t)s_seed;
}
static int32_t enable_window(uint32_t window, int32_t enable)
{ put("enable.window %u %d\n", window, enable); mutate(); return (int32_t)s_seed; }
static void base_exchange(void *object, void *exchange)
{ put("base.exchange %u %u\n", offset(object), offset(exchange)); mutate(); }
static void ddx_control(void *exchange, uint32_t id, void *control)
{ put("ddx %u %x %u\n", offset(exchange), id, offset(control)); mutate(); }
static int32_t base_init(void *object)
{ put("base.init %u\n", offset(object)); mutate(); return (int32_t)s_seed; }
static int32_t init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{
    put("init.art %u bitmap=%x dib=%u pal=%u flags=%u\n", offset(control), bitmap, offset(dib), offset(palette), flags);
    mutate(); return (int32_t)s_seed;
}
static int32_t init_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                          void *dib, void *palette, uint32_t flags, uint32_t color,
                          uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{
    put("init.button %u font=%u/%u bmp=%x dib=%u pal=%u flags=%u color=%x module=%x xy=%d/%d rect=%d/%d/%d/%d\n",
        offset(control), offset(font), offset(alternate_font), bitmap, offset(dib), offset(palette), flags,
        color, module, x, y, rect.left, rect.top, rect.right, rect.bottom);
    mutate(); return (int32_t)s_seed;
}
static void init_text(void *control, void *font)
{ put("init.text %u font=%u\n", offset(control), offset(font)); mutate(); }
static void init_list(void *control, void *font, void *label)
{ put("init.list %u font=%u label=%u\n", offset(control), offset(font), offset(label)); mutate(); }
static void init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags)
{
    put("init.edit %u font=%u fg=%x bg=%x error=%x label=%u flags=%u\n", offset(control),
        offset(font), foreground, background, error_color, offset(label), flags);
    mutate();
}
static void *base_font(int32_t id) { put("base.font %d\n", id); mutate(); return s_fixture.font; }
static void set_window_text(void *window, const char *text)
{ put("set.text %u [%s]\n", offset(window), text); mutate(); }
static const struct fs98_flights_services s_services = {
    send_message, win_help, enable_window, base_exchange, ddx_control, base_init, init_art,
    init_button, init_text, init_list, init_edit, base_font, set_window_text
};

static void *dialog_construct(void *object, int32_t resource, void *parent)
{
    put("dialog.construct %u resource=%x parent=%u\n", offset(object), resource, (uint32_t)(uintptr_t)parent);
    D32(object, 0) = 0xba5e0000; D32(object, 0x20) = s_seed;
    mutate(); return object;
}
static void dialog_destroy(void *object)
{ put("dialog.destroy %u vtable=%x\n", offset(object), D32(object, 0)); D32(object, 0) = 0xdead0000; mutate(); }
static void construct_control(void *control, uint32_t import)
{
    put("control.construct %u import=%x\n", offset(control), import);
    D32(control, 0) = import; D32(control, 4) = s_seed;
    mutate();
}
static void destroy_control(void *control, uint32_t import)
{
    put("control.destroy %u import=%x vtable=%x\n", offset(control), import, D32(control, 0));
    D32(control, 0) = import; mutate();
}
static void *allocate_object(uint32_t size)
{
    put("allocate %x\n", size); mutate();
    return s_scenario % 11 ? s_fixture.object : 0;
}
static void ui_delete(void *object) { put("ui.delete %u\n", offset(object)); mutate(); }
static void dialog_delete(void *object) { put("dialog.delete %u\n", offset(object)); mutate(); }
static int32_t create_dialog(void *object, uint32_t resource, void *parent)
{
    put("dialog.create %u resource=%x parent=%u module=%x descriptor=%u\n", offset(object), resource,
        (uint32_t)(uintptr_t)parent, D32(object, 0x80), offset(PTR(object, 0x10dc)));
    mutate(); return s_scenario & 1 ? 0 : (int32_t)s_seed;
}
static int32_t show_dialog(void *object, int32_t show)
{ put("dialog.show %u %d\n", offset(object), show); mutate(); return s_scenario & 2 ? 0 : (int32_t)s_seed; }
static const struct fs98_flights_object_services s_object_services = {
    dialog_construct, dialog_destroy, construct_control, destroy_control, allocate_object,
    ui_delete, dialog_delete, create_dialog, show_dialog
};

/* ---- the same stubs behind retail's import ABIs ----------------------------------------- */
#define CONTROL_HOOK(name, ctor, dtor) \
static void *TC hook_construct_##name(void *control) { construct_control(control, ctor); return control; } \
static void TC hook_destroy_##name(void *control) { destroy_control(control, dtor); }
CONTROL_HOOK(text, 0x135b8, 0x135d0)
CONTROL_HOOK(list, 0x135bc, 0x135d4)
CONTROL_HOOK(edit, 0x135c0, 0x135d8)
CONTROL_HOOK(button, 0x135b4, 0x135cc)
CONTROL_HOOK(art, 0x135c4, 0x135a8)
#undef CONTROL_HOOK
static void *TC hook_dialog_construct(void *object, int32_t resource, void *parent)
{ return dialog_construct(object, resource, parent); }
static void TC hook_dialog_destroy(void *object) { dialog_destroy(object); }
static void SC hook_dialog_delete(void *object) { dialog_delete(object); }
static void *hook_allocate(uint32_t size) { return allocate_object(size); }
static void hook_ui_delete(void *object) { ui_delete(object); }
static int32_t TC hook_create_dialog(void *object, uint32_t resource, void *parent)
{ return create_dialog(object, resource, parent); }
static int32_t TC hook_show_dialog(void *object, int32_t show) { return show_dialog(object, show); }
static int32_t SC hook_send(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return send_message(a, b, c, d); }
static int32_t SC hook_help(uint32_t a, const char *b, uint32_t c, const uint32_t *d) { return win_help(a, b, c, d); }
static int32_t SC hook_enable(uint32_t a, int32_t b) { return enable_window(a, b); }
static void TC hook_exchange(void *a, void *b) { base_exchange(a, b); }
static void SC hook_ddx(void *a, uint32_t b, void *c) { ddx_control(a, b, c); }
static int32_t TC hook_base_init(void *object) { return base_init(object); }
static int32_t TC hook_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return init_art(control, bitmap, dib, palette, flags); }
static int32_t TC hook_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                             void *dib, void *palette, uint32_t flags, uint32_t color,
                             uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{ return init_button(control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void TC hook_init_text(void *control, void *font) { init_text(control, font); }
static void TC hook_init_list(void *control, void *font, void *label) { init_list(control, font, label); }
static void TC hook_init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                              uint32_t error_color, void *label, uint32_t flags)
{ init_edit(control, font, foreground, background, error_color, label, flags); }
static void *SC hook_base_font(int32_t id) { return base_font(id); }
static void TC hook_set_text(void *window, const char *text) { set_window_text(window, text); }

static void patch(uint32_t rva, void *target)
{
    DWORD old;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) exit(2);
    *slot = target;
    VirtualProtect(slot, 4, old, &old);
}
static void setup_imports(void)
{
    void *vtables[FS98_FLIGHTS_CLASS_COUNT];
    uint32_t index;
    HMODULE crt = LoadLibraryA("msvcrt.dll");
    void *handler = (void *)GetProcAddress(crt, "__CxxFrameHandler");
    if (!handler) exit(2);
    patch(0x134c4, handler);
    patch(0x13628, hook_send); patch(0x1363c, hook_help); patch(0x13638, hook_enable);
    patch(0x133fc, hook_ddx); patch(0x133e0, hook_set_text);
    patch(0x13378, hook_create_dialog); patch(0x134a4, hook_show_dialog);
    patch(0x135a4, hook_exchange); patch(0x13580, hook_base_init); patch(0x13584, hook_art);
    patch(0x13588, hook_button); patch(0x1358c, hook_init_text); patch(0x13594, hook_init_edit);
    patch(0x13590, hook_base_font); patch(0x13598, hook_init_list);
    patch(0x135fc, hook_allocate); patch(0x135ac, hook_ui_delete); patch(0x13600, hook_dialog_delete);
    patch(0x135b0, hook_dialog_construct); patch(0x135c8, hook_dialog_destroy);
#define PATCH_CONTROL(name, ctor, dtor) patch(ctor, hook_construct_##name); patch(dtor, hook_destroy_##name)
    PATCH_CONTROL(text, 0x135b8, 0x135d0); PATCH_CONTROL(list, 0x135bc, 0x135d4);
    PATCH_CONTROL(edit, 0x135c0, 0x135d8); PATCH_CONTROL(button, 0x135b4, 0x135cc);
    PATCH_CONTROL(art, 0x135c4, 0x135a8);
#undef PATCH_CONTROL
    patch(0x12134, (void *)RESOURCE_MODULE); fs98_flights_set_resource_module(RESOURCE_MODULE);
    patch(0x1214c, (void *)OBJECT_MODULE); fs98_flights_set_object_module(OBJECT_MODULE);
    for (index = 0; index < FS98_FLIGHTS_CLASS_COUNT; ++index)
        vtables[index] = (uint8_t *)s_retail + s_classes[index].vtable;
    fs98_flights_set_object_services(&s_object_services, vtables);
    fs98_flights_set_services(&s_services);
}

static void initialize(void)
{
    uint32_t index;
    uint8_t *object = s_fixture.object;
    for (index = 0; index < sizeof(s_fixture); ++index) ((uint8_t *)&s_fixture)[index] = (uint8_t)next_random();
    PTR(object, 0) = s_vtable;
    D32(object, 0x20) = s_seed;
    PTR(object, 0x10dc) = s_fixture.descriptor[0];
    for (index = 0; index < 2; ++index) {
        uint8_t *descriptor = s_fixture.descriptor[index];
        D32(descriptor, 8) = s_scenario % 6;
        D32(descriptor, 0xc) = s_scenario % 7 - 1;
        PTR(descriptor, 0x10) = s_fixture.records;
    }
    for (index = 0; index < 5; ++index) {
        snprintf((char *)s_fixture.records[index], 64, "Flight %u vector %u", index, s_scenario);
        snprintf((char *)s_fixture.records[index] + 0x100, 64, "Description %u seed %x", index, s_seed);
        snprintf((char *)s_fixture.records[index] + 0x900, 64, "C:\\flights\\flight%u.stn", index);
    }
    if (OP.signature == SIG_INT && OP.rva == 0x5590 && s_scenario % 3 == 0) PTR(object, 0x10dc) = 0;
    if ((OP.rva == 0x2ec0 || OP.rva == 0x52e0) && s_scenario % 3 == 0) PTR(object, 0x10dc) = 0;
    s_initial = s_fixture;
}
static LONG WINAPI fatal(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT scenario=%u op=%u (%s) side=%d code=%lx address=%p\n", s_scenario,
        s_operation, OP.name, s_side, exception->ExceptionRecord->ExceptionCode,
        exception->ExceptionRecord->ExceptionAddress);
    fflush(stderr); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static void land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_jump, 1);
}
/* A failed allocation makes the factory store through null on both sides: compare it */
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    if (OP.signature == SIG_CREATE && s_scenario % 11 == 0 &&
        record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        s_throw_at = 0;
        put("allocation.fault access=%u address=%x\n", (uint32_t)record->ExceptionInformation[0],
            (uint32_t)record->ExceptionInformation[1]);
        if (!s_side) ++s_allocation_faults;
        exception->ContextRecord->Eip = (DWORD)land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static EXCEPTION_DISPOSITION __cdecl catch_throw(EXCEPTION_RECORD *record, void *frame,
                                                 CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == TEST_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, 0, record, 0);
        s_throw_at = 0; put("CAUGHT\n"); if (!s_side) ++s_throws;
        land();
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) int32_t run(uint32_t throw_at)
{
    struct { void *previous; void *handler; } catcher;
    const struct fs98_flights_class *class_info = &s_classes[OP.kind];
    void *entry = s_side ? OP.ours : (uint8_t *)s_retail + OP.rva;
    void *object = s_fixture.object;
    volatile int32_t result = 0;
    s_fixture = s_initial;
    s_log_length = s_calls = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at;
    if (OP.signature >= SIG_CONSTRUCT && !s_side) {
        uint32_t rva = OP.signature == SIG_DESTROY ? class_info->destructor :
                       OP.signature == SIG_DELETE ? class_info->delete :
                       OP.signature == SIG_CREATE ? class_info->factory : class_info->constructor;
        entry = (uint8_t *)s_retail + rva;
    }
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_saved_seh; catcher.handler = catch_throw;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        switch (OP.signature) {
        case SIG_EVENT:
            if (OP.rva == 0x26a0 && s_scenario % 2 == 0) object = 0;
            ((void (TC *)(void *))entry)(object);
            break;
        case SIG_HELP:
            result = ((int32_t (TC *)(void *, uint32_t, void *))entry)(object, s_seed, s_fixture.info);
            break;
        case SIG_CONTEXT:
            result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 1);
            break;
        case SIG_EXCHANGE:
            ((void (TC *)(void *, void *))entry)(object, s_fixture.exchange);
            break;
        case SIG_INT:
            result = ((int32_t (TC *)(void *))entry)(object);
            break;
        case SIG_EMPTY_ARG:
            ((void (TC *)(void *, uint32_t))entry)(object, s_seed);
            break;
        default: {
            void *returned = 0;
            if (OP.signature == SIG_CONSTRUCT || OP.signature >= SIG_ROUNDTRIP)
                returned = ((void *(TC *)(void *, void *))entry)(object, (void *)(uintptr_t)s_seed);
            else if (OP.signature == SIG_DESTROY) ((void (TC *)(void *))entry)(object);
            else if (OP.signature == SIG_DELETE) returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
            else result = ((int32_t (SC *)(uint32_t, void *))entry)(s_seed, s_fixture.descriptor[0]);
            if (returned) put("return.object %u\n", offset(returned));
            if (OP.signature >= SIG_ROUNDTRIP) {
                /* this class's construct entry, then destroy and delete after it */
                const struct operation *ours = &OP - (OP.signature - SIG_CONSTRUCT);
                if (OP.signature == SIG_ROUNDTRIP) {
                    entry = s_side ? (ours + 1)->ours : (uint8_t *)s_retail + class_info->destructor;
                    ((void (TC *)(void *))entry)(object);
                } else {
                    entry = s_side ? (ours + 2)->ours : (uint8_t *)s_retail + class_info->delete;
                    returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
                    put("return.object %u\n", offset(returned));
                }
            }
            break;
        }
        }
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    if (s_calls > s_max_calls) s_max_calls = s_calls;
    return result;
}
static int compare(uint32_t throw_at)
{
    int32_t retail, ours;
    uint32_t at;
    s_side = 0; retail = run(throw_at); s_retail_result = s_fixture;
    s_side = 1; ours = run(throw_at);
    ++s_comparisons;
    if (retail == ours && !memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) &&
        !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s) throw=%u seed=%x script=%u returns=%d/%d\n",
        s_scenario, s_operation, OP.name, throw_at, s_seed, s_script, retail, ours);
    for (at = 0; at < sizeof(s_fixture); ++at)
        if (((uint8_t *)&s_retail_result)[at] != ((uint8_t *)&s_fixture)[at]) {
            fprintf(stderr, "byte offset=%x retail=%02x ours=%02x\n", at,
                ((uint8_t *)&s_retail_result)[at], ((uint8_t *)&s_fixture)[at]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], 0, 0) : 20000;
    uint32_t initial_seed = argc > 2 ? strtoul(argv[2], 0, 0) : 0x5f1a6c2d;
    int unwind = argc > 3 && !strcmp(argv[3], "unwind");
    uint32_t throw, limit;
    if (!count || !initial_seed) return 2;
    s_rng = initial_seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal);
    AddVectoredExceptionHandler(1, expected_fault);
    s_retail = LoadLibraryExA("orig/English/Flights.DLL", 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || (uint32_t)(uintptr_t)s_retail != BASE ||
        GetProcAddress(s_retail, "Linkage") != (FARPROC)((uint8_t *)s_retail + 0x120d8)) {
        fprintf(stderr, "orig/English/Flights.DLL not mapped at its base: %p, error %lu\n",
                (void *)s_retail, GetLastError());
        return 2;
    }
    fs98_flights_set_help_file((const char *)s_retail + 0x120fc);
    setup_imports();
    {
        uint32_t slot;
        for (slot = 0; slot < 64; ++slot)
            s_vtable[slot] = s_other_vtable[slot] = (void *)s_other_slots[slot];
    }
    s_vtable[0xec / 4] = virtual_close; s_other_vtable[0xec / 4] = virtual_close_other;
    s_vtable[0xcc / 4] = virtual_accept; s_other_vtable[0xcc / 4] = virtual_accept_other;
    s_vtable[0xd0 / 4] = virtual_cancel; s_other_vtable[0xd0 / 4] = virtual_cancel_other;
    s_vtable[1] = s_other_vtable[1] = virtual_delete;
    s_vtable[0xe4 / 4] = s_other_vtable[0xe4 / 4] = virtual_skin;
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        s_seed = next_random(); s_script = s_scenario & 15;
        for (s_operation = 0; s_operation < OP_COUNT; ++s_operation) {
            initialize();
            if (compare(0)) return 1;
            limit = s_calls;
            if (unwind) for (throw = 1; throw <= limit; ++throw)
                if (compare(throw)) return 1;
        }
    }
    printf("Flights events: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x swaps=%u "
           "no_selection=%u throws=%u max_calls=%u allocation_faults=%u\n",
        count, (uint32_t)OP_COUNT, s_comparisons, initial_seed, s_swaps, s_no_selection, s_throws,
        s_max_calls, s_allocation_faults);
    FreeLibrary(s_retail); return 0;
}
