/* Win32 adapter for Views. The dialog code is rebuilt; MFC42 and SIMSUI
 * remain the original shared dependencies during the compatibility port. */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "views_objects.h"
#include "views_unwind.h"

#define TC FS98_VIEWS_THISCALL
#define SC FS98_VIEWS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))

struct fs98_views_binding { const char *dll, *name; uint16_t ordinal, slot; };
struct fs98_views_virtual_binding { int table, slot, import_index; void *local; };
struct fs98_views_message_entry { uint32_t message, code, first, last, signature; void *handler; };
struct fs98_views_message_map { void *(*base)(void); const struct fs98_views_message_entry *entries; };
struct fs98_views_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
static void *TC views_message_map(void *object);
static void TC views_empty(void *object) { (void)object; }
#include "views_runtime_tables.inc"

static void *s_import_functions[sizeof(s_import_bindings) / sizeof(s_import_bindings[0])];
static void *s_vtable[60];
static uint8_t s_extension[0x14];
static uint32_t s_module, s_resource_module;
static void *s_ftol, *s_scan, *s_compare;
struct fs98_views_import { uint32_t id; void *link; };
struct fs98_views_linkage {
    uint32_t id; void (*init)(void), (*deinit)(void); void *reserved;
    uint32_t flags; const struct fs98_views_dialog *dialogs;
};
static void views_module_init(void);
static void views_module_deinit(void);
struct fs98_views_import g_views_imports[] = {{1, 0}, {2, 0}, {0, 0}};
struct fs98_views_linkage g_views_linkage = {
    0x80000005, views_module_init, views_module_deinit, 0, 0, s_dialogs
};

static void *rt_slot(uint32_t slot)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++)
        if (s_import_bindings[index].slot == slot) return s_import_functions[index];
    return 0;
}
static void *rt_mfc(uint32_t ordinal)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++)
        if (s_import_bindings[index].ordinal == ordinal) return s_import_functions[index];
    return 0;
}
#define M0(ordinal, self) ((void (TC *)(void *))rt_mfc(ordinal))(self)
#define M1(ordinal, self, argument) ((void (TC *)(void *, void *))rt_mfc(ordinal))(self, argument)

static int32_t rt_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return (int32_t)SendMessageA((HWND)window, message, w_param, l_param); }
static int32_t rt_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{ return WinHelpA((HWND)window, file, command, (ULONG_PTR)table); }
static void rt_close(void *self) { ((void (TC *)(void *))rt_slot(0x7450))(self); }
static int32_t rt_get_text(void *self, char *buffer, int32_t capacity)
{ return ((int32_t (TC *)(void *, char *, int32_t))rt_mfc(3873))(self, buffer, capacity); }
static int32_t rt_error(void *self, void *control, void *descriptor)
{ return ((int32_t (TC *)(void *, void *, uint32_t))rt_slot(0x74a0))(self, control, (uint32_t)descriptor); }
static int32_t rt_enable_window(uint32_t window, int32_t enabled) { return EnableWindow((HWND)window, enabled); }
static void *rt_control(void *self, uint32_t id)
{ return ((void *(TC *)(void *, uint32_t))rt_mfc(3092))(self, id); }
static int32_t rt_enable(void *self, int32_t enabled)
{ return ((int32_t (TC *)(void *, int32_t))rt_mfc(2642))(self, enabled); }
static int32_t rt_show(void *self, int32_t show)
{ return ((int32_t (TC *)(void *, int32_t))rt_mfc(6215))(self, show); }
static void rt_set_text(void *self, const char *text)
{ ((void (TC *)(void *, const char *))rt_mfc(6199))(self, text); }
static int32_t rt_check(void *self, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x7498))(self, checked); }
static int32_t rt_radio(void *self, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x749c))(self, checked); }
static void rt_ddx(void *exchange, uint32_t id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))rt_mfc(2302))(exchange, id, control); }
static void rt_string_construct(void *self) { M0(540, self); }
static void rt_string_destroy(void *self) { M0(800, self); }
static char *rt_string_buffer(void *self, int32_t capacity)
{ return ((char *(TC *)(void *, int32_t))rt_mfc(2915))(self, capacity); }
static void rt_string_release(void *self, int32_t length)
{ ((void (TC *)(void *, int32_t))rt_mfc(5572))(self, length); }
static int32_t rt_load_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{ return LoadStringA((HINSTANCE)module, id, buffer, capacity); }
static int32_t rt_find_char(void *self, uint32_t character)
{ return ((int32_t (TC *)(void *, uint32_t))rt_mfc(2763))(self, character); }
static int32_t rt_find_text(void *self, const char *text)
{ return ((int32_t (TC *)(void *, const char *))rt_mfc(2764))(self, text); }
static void *rt_mid(void *self, void *result, int32_t start, int32_t count)
{ return ((void *(TC *)(void *, void *, int32_t, int32_t))rt_mfc(4278))(self, result, start, count); }
static void *rt_tail(void *self, void *result, int32_t start)
{ return ((void *(TC *)(void *, void *, int32_t))rt_mfc(4277))(self, result, start); }
static void rt_assign(void *self, const void *source)
{ ((void *(TC *)(void *, const void *))rt_mfc(858))(self, source); }
static void rt_append(void *self, const void *source)
{ ((void *(TC *)(void *, const void *))rt_mfc(939))(self, source); }
static void rt_append_text(void *self, const char *text)
{ ((void *(TC *)(void *, const char *))rt_mfc(941))(self, text); }
static void rt_assign_text(void *self, const char *text)
{ ((void *(TC *)(void *, const char *))rt_mfc(860))(self, text); }
static void rt_get_text_string(void *self, void *string) { M1(3874, self, string); }
static void rt_format(void *self, const char *format, double value)
{ ((void (*)(void *, const char *, ...))rt_mfc(2818))(self, format, value); }
static int32_t rt_scan_float(const char *text, float *value)
{ return ((int32_t (*)(const char *, const char *, ...))s_scan)(text, "%f", value); }
static int64_t rt_float_to_integer(long double value)
{
    uint32_t low, high;
    __asm__ volatile("fldt %3\n\tcall *%2" : "=a"(low), "=d"(high) : "r"(s_ftol), "m"(value) : "ecx", "cc", "memory", "st");
    return (int64_t)(((uint64_t)high << 32) | low);
}
static int32_t rt_compare(const char *left, const char *right)
{ return ((int32_t (*)(const char *, const char *))s_compare)(left, right); }
static int32_t rt_base_init(void *self)
{ return ((int32_t (TC *)(void *))rt_slot(0x7454))(self); }
static int32_t rt_init_art(void *self, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return ((int32_t (TC *)(void *, int32_t, void *, void *, uint32_t))rt_slot(0x7428))(self, bitmap, dib, palette, flags); }
static int32_t rt_init_button(void *self, void *font, void *alternate, uint32_t bitmap,
                              void *dib, void *palette, uint32_t flags, uint32_t color,
                              uint32_t module, int32_t x, int32_t y, struct fs98_views_rect rect)
{
    return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t,
                            uint32_t, uint32_t, int32_t, int32_t, struct fs98_views_rect))rt_slot(0x7410))(
                            self, font, alternate, bitmap, dib, palette, flags, color, module, x, y, rect);
}
static void rt_init_text(void *self, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0x7414))(self, font); }
static void rt_init_combo(void *self, void *font, void *label)
{ ((void (TC *)(void *, void *, void *))rt_slot(0x7418))(self, font, label); }
static void rt_init_group(void *self, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0x741c))(self, font); }
static void rt_remap(struct fs98_views_rect *rect, void *control)
{ ((void (SC *)(struct fs98_views_rect *, void *))rt_slot(0x7420))(rect, control); }
static void rt_move(void *self, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t))rt_mfc(4299))(self, x, y, width, height, repaint); }
static uint32_t rt_dlg_item(uint32_t window, uint32_t id) { return (uint32_t)GetDlgItem((HWND)window, id); }
static void *rt_font(int32_t id)
{ return ((void *(SC *)(int32_t))rt_slot(0x7424))(id); }
static void rt_init_edit(void *self, void *font, uint32_t foreground, uint32_t background,
                         uint32_t error_color, void *label, uint32_t flags)
{
    ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))rt_slot(0x742c))(
        self, font, foreground, background, error_color, label, flags);
}
static const struct fs98_views_services s_services = {
    .send_message = rt_message, .win_help = rt_help, .close = rt_close, .get_text = rt_get_text,
    .error_dialog = rt_error, .enable_window = rt_enable_window, .get_control = rt_control,
    .enable_control = rt_enable, .show_control = rt_show, .set_text = rt_set_text,
    .set_checked = rt_check, .ddx_control = rt_ddx, .string_construct = rt_string_construct,
    .string_destroy = rt_string_destroy, .string_buffer = rt_string_buffer, .string_release = rt_string_release,
    .load_string = rt_load_string, .string_find_char = rt_find_char, .string_find_text = rt_find_text,
    .string_mid = rt_mid, .string_tail = rt_tail, .string_assign = rt_assign, .string_append = rt_append,
    .string_append_text = rt_append_text, .get_text_string = rt_get_text_string, .string_format = rt_format,
    .string_assign_text = rt_assign_text, .scan_float = rt_scan_float, .float_to_integer = rt_float_to_integer,
    .set_radio = rt_radio, .compare_text = rt_compare, .base_init = rt_base_init, .init_art = rt_init_art, .init_button = rt_init_button,
    .init_text = rt_init_text, .init_combo = rt_init_combo, .init_group = rt_init_group, .remap_rect = rt_remap,
    .move_control = rt_move, .get_dlg_item = rt_dlg_item, .base_font = rt_font, .init_edit = rt_init_edit
};

static void *rt_dialog_construct(void *self, int32_t resource, void *parent)
{ return ((void *(TC *)(void *, int32_t, void *))rt_slot(0x7438))(self, resource, parent); }
static void rt_dialog_destroy(void *self) { ((void (TC *)(void *))rt_slot(0x7460))(self); }
static void rt_construct(void *self, uint32_t slot) { ((void *(TC *)(void *))rt_slot(slot))(self); }
static void rt_destroy(void *self, uint32_t slot) { ((void (TC *)(void *))rt_slot(slot))(self); }
static void *rt_allocate(uint32_t size) { return ((void *(*)(uint32_t))rt_slot(0x744c))(size); }
static void rt_ui_delete(void *self) { ((void (*)(void *))rt_slot(0x7434))(self); }
static void rt_mfc_delete(void *self) { ((void (*)(void *))rt_mfc(825))(self); }
static void rt_dialog_delete(void *self) { ((void (SC *)(void *))rt_slot(0x74a4))(self); }
static int32_t rt_create(void *self, uint32_t resource, void *parent)
{ return ((int32_t (TC *)(void *, uint32_t, void *))rt_mfc(2086))(self, resource, parent); }
static const struct fs98_views_object_services s_object_services = {
    rt_dialog_construct, rt_dialog_destroy, rt_construct, rt_destroy, rt_allocate,
    rt_ui_delete, rt_mfc_delete, rt_dialog_delete, rt_create, rt_show
};
static void *views_base_map(void) { return rt_slot(0x7430); }
static struct fs98_views_message_map s_map = {views_base_map, s_views_messages};
static void *TC views_message_map(void *object) { (void)object; return &s_map; }

static int views_bind(void)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++) {
        const struct fs98_views_binding *binding = &s_import_bindings[index];
        HMODULE module = GetModuleHandleA(binding->dll);
        void *function = module ? (void *)GetProcAddress(module, binding->ordinal ?
            (const char *)(uint32_t)binding->ordinal : binding->name) : 0;
        if (!function) return 0;
        s_import_functions[index] = function;
    }
    for (index = 0; index < sizeof(s_virtual_bindings) / sizeof(s_virtual_bindings[0]); index++) {
        const struct fs98_views_virtual_binding *binding = &s_virtual_bindings[index];
        s_vtable[binding->slot] = binding->import_index < 0 ? binding->local : s_import_functions[binding->import_index];
    }
    s_ftol = (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_ftol");
    s_scan = (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "sscanf");
    s_compare = (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_mbscmp");
    if (!s_ftol || !s_scan || !s_compare) return 0;
    fs98_views_set_services(&s_services);
    fs98_views_set_object_services(&s_object_services, s_vtable, rt_slot(0x745c));
    return 1;
}
struct views_dynamic_unwind { struct fs98_views_unwind frame; void *dynamic; volatile uint32_t live; };
static void views_dynamic_cleanup(struct fs98_views_unwind *frame)
{
    struct views_dynamic_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; rt_mfc_delete(state->dynamic); }
}
static FS98_VIEWS_NOINLINE void views_module_init(void)
{
    struct views_dynamic_unwind state;
    void *dynamic;
    state.dynamic = 0; state.live = 0;
    fs98_views_unwind_enter(&state.frame, views_dynamic_cleanup);
    ((void (SC *)(void *, uint32_t *))PTR(g_views_imports[0].link, 0x298))(&g_views_linkage, &s_module);
    fs98_views_set_object_module(s_module);
    dynamic = ((void *(*)(uint32_t))rt_mfc(823))(0x40);
    state.dynamic = dynamic; state.live = 1;
    if (dynamic) ((void *(TC *)(void *, void *, int32_t))rt_mfc(342))(dynamic, s_extension, 0);
    state.live = 0;
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
    fs98_views_set_resource_module(s_resource_module);
    fs98_views_unwind_leave(&state.frame);
}
static void views_module_deinit(void)
{
    if (s_module) ((void (SC *)(uint32_t))PTR(g_views_imports[0].link, 0x29c))(s_module);
    FreeLibrary((HMODULE)s_resource_module);
    s_module = s_resource_module = 0;
    fs98_views_set_object_module(0);
    fs98_views_set_resource_module(0);
}
__declspec(dllimport) void *fs98_views_simsui_anchor(uint32_t size);
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        void *volatile anchor = (void *)fs98_views_simsui_anchor;
        void *state;
        (void)anchor;
        if (!views_bind()) return FALSE;
        state = ((void *(*)(void))rt_mfc(1168))();
        D32(state, 0x1c) = D32(state, 0x20); D32(state, 0x28) = D32(state, 0x2c);
        D32(state, 0x20) = D32(state, 0x2c) = 0;
        ((int32_t (SC *)(void *, HINSTANCE))rt_mfc(1182))(s_extension, instance);
        s_module = (uint32_t)instance;
        fs98_views_set_object_module(s_module);
    } else if (reason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int32_t))rt_mfc(1253))(s_extension, 0);
    }
    return TRUE;
}
