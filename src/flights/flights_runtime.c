/* Native Flights adapter. Dialog behaviour is rebuilt; the shared MFC/SIMSUI controls stay
 * the CD's dependencies during the compatibility port, reached through the same imports
 * retail uses (tools/gen_flights_runtime.py lists them). */
/* MSVCRT's own sprintf and atof, as retail calls them, not mingw's ANSI replacements */
#define __USE_MINGW_ANSI_STDIO 0
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "flights_internal.h"
#include "flights_objects.h"
#include "flights_unwind.h"
#define TC FS98_FLIGHTS_THISCALL
#define SC FS98_FLIGHTS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
struct flights_import_binding { const char *dll, *name; uint16_t ordinal; uint32_t slot; };
struct flights_virtual_binding { int table, slot, import_index; void *local; };
struct flights_message_entry { uint32_t message, code, first, last, signature; void *handler; };
struct flights_message_map { void *(*base)(void); const struct flights_message_entry *entries; };
struct flights_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
#define CLASS_TOTAL 16
static struct flights_message_map s_message_maps[CLASS_TOTAL];
#define MAP_GETTER(name, index) \
static void *TC flights_map_##name(void *object) { (void)object; return &s_message_maps[index]; }
MAP_GETTER(adventures, 0)
MAP_GETTER(challenges, 1)
MAP_GETTER(save, 2)
MAP_GETTER(comms, 3)
MAP_GETTER(create, 4)
MAP_GETTER(aircraft, 5)
MAP_GETTER(modeless, 6)
MAP_GETTER(opening, 7)
MAP_GETTER(flight_menu, 8)
MAP_GETTER(learning_menu, 9)
MAP_GETTER(select, 10)
MAP_GETTER(autopilot, 11)
MAP_GETTER(navigation, 12)
MAP_GETTER(nav_efis, 13)
MAP_GETTER(nav_radios, 14)
MAP_GETTER(nav_adf, 15)
#undef MAP_GETTER
#include "flights_runtime_tables.inc"
#define IMPORT_COUNT (sizeof(s_import_bindings) / sizeof(s_import_bindings[0]))
static void *s_import_functions[IMPORT_COUNT];
static void *s_vtables[CLASS_TOTAL][61];
static uint8_t s_extension[0x14];      /* AFX_EXTENSION_MODULE, 0x22322138 */
static uint32_t s_module, s_resource_module;
/* the function-local statics of Create, the welcome dialog and the sub-menus (.bss) */
static struct fs98_flights_create_state s_create_state;
static struct fs98_flights_opening_state s_opening_state;
static struct fs98_flights_menus_state s_menus_state;
struct flights_import { uint32_t id; void *link; };
struct flights_linkage {
    uint32_t id; void (*init)(void), (*deinit)(void); void *reserved;
    uint32_t flags; const struct flights_dialog *dialogs;
};
static void flights_module_init(void);
static void flights_module_deinit(void);
struct flights_import g_flights_imports[] = {{1, 0}, {2, 0}, {0, 0}};
struct flights_linkage g_flights_linkage = {0x80000002, flights_module_init, flights_module_deinit, 0, 0, s_dialogs};

static void *rt_slot(uint32_t slot)
{
    uint32_t index;
    for (index = 0; index < IMPORT_COUNT; index++)
        if (s_import_bindings[index].slot == slot) return s_import_functions[index];
    return 0;
}
static void *rt_mfc(uint32_t ordinal)
{
    uint32_t index;
    for (index = 0; index < IMPORT_COUNT; index++)
        if (s_import_bindings[index].ordinal == ordinal) return s_import_functions[index];
    return 0;
}
#define THIS0(slot, type) ((type (TC *)(void *))rt_slot(slot))

/* ---- the services, each over the import retail calls ------------------------------------ */
static int32_t rt_send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return (int32_t)SendMessageA((HWND)window, message, w_param, l_param); }
static int32_t rt_win_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{ return WinHelpA((HWND)window, file, command, (ULONG_PTR)table); }
static int32_t rt_enable_window(uint32_t window, int32_t enable) { return EnableWindow((HWND)window, enable); }
static void rt_base_exchange(void *object, void *exchange)
{ ((void (TC *)(void *, void *))rt_slot(0x135a4))(object, exchange); }
static void rt_ddx_control(void *exchange, uint32_t id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))rt_slot(0x133fc))(exchange, id, control); }
static int32_t rt_base_init(void *object) { return THIS0(0x13580, int32_t)(object); }
static int32_t rt_init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return ((int32_t (TC *)(void *, int32_t, void *, void *, uint32_t))rt_slot(0x13584))(control, bitmap, dib, palette, flags); }
static int32_t rt_init_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                              void *dib, void *palette, uint32_t flags, uint32_t color,
                              uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{
    return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t,
                            int32_t, int32_t, struct fs98_flights_rect))rt_slot(0x13588))(
        control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect);
}
static void rt_init_text(void *control, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0x1358c))(control, font); }
static void rt_init_list(void *control, void *font, void *label)
{ ((void (TC *)(void *, void *, void *))rt_slot(0x13598))(control, font, label); }
static void rt_init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                         uint32_t error_color, void *label, uint32_t flags)
{
    ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))rt_slot(0x13594))(
        control, font, foreground, background, error_color, label, flags);
}
static void *rt_base_font(int32_t id) { return ((void *(SC *)(int32_t))rt_slot(0x13590))(id); }
static void rt_set_window_text(void *window, const char *text)
{ ((void (TC *)(void *, const char *))rt_slot(0x133e0))(window, text); }
static int32_t rt_get_window_text(void *window, char *buffer, int32_t count)
{ return ((int32_t (TC *)(void *, char *, int32_t))rt_slot(0x133d4))(window, buffer, count); }
static int32_t rt_set_checked(void *control, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x13550))(control, checked); }
static int32_t rt_error_dialog(void *object, void *control, uint32_t code)
{ return ((int32_t (TC *)(void *, void *, uint32_t))rt_slot(0x1354c))(object, control, code); }
static int32_t rt_post_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return PostMessageA((HWND)window, message, w_param, l_param); }
static char *rt_copy_text(char *out, const char *text) { return lstrcpyA(out, text); }
static int32_t rt_compare_text(const char *a, const char *b) { return lstrcmpA(a, b); }
static void rt_close(void *object) { THIS0(0x135f8, void)(object); }
static int32_t rt_get_window_long(uint32_t window, int32_t index) { return GetWindowLongA((HWND)window, index); }
static uint32_t rt_get_focus(void) { return (uint32_t)GetFocus(); }
static int32_t rt_enable_control(void *window, int32_t enable)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x133ec))(window, enable); }
static int32_t rt_set_radio(void *control, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x1351c))(control, checked); }
static int32_t rt_init_slider(void *control, uint32_t bitmap, uint32_t thumb, void *dib, void *palette,
                              uint32_t a, uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f, uint32_t module)
{
    return ((int32_t (TC *)(void *, uint32_t, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t, uint32_t,
                            int32_t, int32_t, uint32_t))rt_slot(0x13528))(
        control, bitmap, thumb, dib, palette, a, b, c, d, e, f, module);
}
static void rt_slider_range(void *control, int32_t low, int32_t high)
{ ((void (TC *)(void *, int32_t, int32_t))rt_slot(0x1352c))(control, low, high); }
static int32_t rt_slider_position(void *control) { return THIS0(0x13524, int32_t)(control); }
static int32_t rt_set_slider_position(void *control, int32_t position)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x13520))(control, position); }
static void rt_init_group(void *control, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0x13530))(control, font); }
static double rt_to_number(const char *text) { return atof(text); }
static int32_t rt_format_number(char *out, const char *format, double value) { return sprintf(out, format, value); }
static int32_t rt_init_picture(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t module, uint16_t color)
{
    return ((int32_t (TC *)(void *, uint32_t, void *, void *, uint32_t, uint32_t))rt_slot(0x13618))(
        control, bitmap, dib, palette, module, color);
}
static void *rt_set_focus(void *window) { return THIS0(0x133f8, void *)(window); }
static void rt_set_sims_text(void *control, const char *text)
{ ((void (TC *)(void *, const char *))rt_slot(0x13610))(control, text); }
static void rt_font_construct(void *font, void *base_font)
{ ((void *(TC *)(void *, void *))rt_slot(0x13604))(font, base_font); }
static void rt_font_destroy(void *font) { THIS0(0x13614, void)(font); }
/* atexit through __dllonexit: run last-in first-out after the extension module ends */
static void (*s_exit_handlers[16])(void);
static uint32_t s_exit_count;
static void rt_register_exit(void (*handler)(void))
{ if (s_exit_count < 16) s_exit_handlers[s_exit_count++] = handler; }
static void rt_default_paint(void *object) { THIS0(0x133d8, void)(object); }
static void rt_string_construct(void *string) { THIS0(0x133dc, void *)(string); }
static void rt_string_destroy(void *string) { THIS0(0x133e8, void)(string); }
static void rt_list_text(void *list, int32_t index, void *string)
{ ((void (TC *)(void *, int32_t, void *))rt_slot(0x133e4))(list, index, string); }
static void *rt_dialog_item(void *object, int32_t id)
{ return ((void *(TC *)(void *, int32_t))rt_slot(0x133f0))(object, id); }
static uint32_t rt_dialog_item_window(uint32_t window, int32_t id) { return (uint32_t)GetDlgItem((HWND)window, id); }
static void rt_remap_rect(struct fs98_flights_rect *rect, void *control)
{ ((void (SC *)(struct fs98_flights_rect *, void *))rt_slot(0x1359c))(rect, control); }
static void rt_move_window(void *window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t))rt_slot(0x133f4))(window, x, y, width, height, repaint); }
static int32_t rt_redraw_window(uint32_t window, void *rect, void *region, uint32_t flags)
{ return RedrawWindow((HWND)window, rect, region, flags); }
static void *rt_window_from_handle(uint32_t window) { return ((void *(SC *)(uint32_t))rt_slot(0x13380))(window); }
static int32_t rt_load_string(void *string, uint32_t id)
{ return ((int32_t (TC *)(void *, uint32_t))rt_slot(0x13384))(string, id); }
static int32_t rt_init_shadow_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                                     void *dib, void *palette, uint32_t flags, uint32_t color,
                                     uint32_t module, int32_t x, int32_t y, struct fs98_flights_rect rect)
{
    return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t,
                            int32_t, int32_t, struct fs98_flights_rect))rt_slot(0x13608))(
        control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect);
}
static int32_t rt_window_enabled(void *window) { return THIS0(0x133d0, int32_t)(window); }
static void rt_init_combo(void *control, void *font, void *label)
{ ((void (TC *)(void *, void *, void *))rt_slot(0x13554))(control, font, label); }
static void rt_ddx_text(void *exchange, uint32_t id, void *string)
{ ((void (SC *)(void *, uint32_t, void *))rt_slot(0x133bc))(exchange, id, string); }
static void rt_ddv_max_chars(void *exchange, void *string, int32_t count)
{ ((void (SC *)(void *, void *, int32_t))rt_slot(0x133c0))(exchange, string, count); }
static int32_t rt_message_box(const char *text, uint32_t type, uint32_t help)
{ return ((int32_t (SC *)(const char *, uint32_t, uint32_t))rt_slot(0x133b8))(text, type, help); }
static int32_t rt_is_kind_of(void *object, const void *runtime_class)
{ return ((int32_t (TC *)(void *, const void *))rt_slot(0x133b4))(object, runtime_class); }
static int32_t rt_show_window(void *window, int32_t show)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x134a4))(window, show); }
static int32_t rt_tab_init(void *tabs, void *dialog, void *font, uint32_t bitmap, void *dib, void *palette,
                           uint32_t a, uint32_t color, int32_t b, int32_t c, int32_t d, int32_t e,
                           uint32_t module, uint32_t flags)
{
    return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, int32_t,
                            int32_t, int32_t, int32_t, uint32_t, uint32_t))rt_slot(0x1356c))(
        tabs, dialog, font, bitmap, dib, palette, a, color, b, c, d, e, module, flags);
}
static void rt_tab_add_page(void *tabs, void *page, char *caption, int32_t flag)
{ ((void (TC *)(void *, void *, char *, int32_t))rt_slot(0x134e4))(tabs, page, caption, flag); }
static void rt_tab_helper_init(void *helper, void *tabs)
{ ((void (TC *)(void *, void *))rt_slot(0x134e0))(helper, tabs); }
static int32_t rt_tab_set_last_page(void *tabs, void *page)
{ return ((int32_t (TC *)(void *, void *))rt_slot(0x134e8))(tabs, page); }
static int32_t rt_tab_set_active(void *tabs, int32_t index, uint32_t window)
{ return ((int32_t (TC *)(void *, int32_t, uint32_t))rt_slot(0x134ec))(tabs, index, window); }
static int32_t rt_page_init(void *page, int32_t value)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x13570))(page, value); }
static int32_t rt_load_string_a(uint32_t instance, uint32_t id, char *buffer, int32_t size)
{ return LoadStringA((HINSTANCE)instance, id, buffer, size); }

static const struct fs98_flights_services s_services = {
    .send_message = rt_send_message, .win_help = rt_win_help, .enable_window = rt_enable_window,
    .base_exchange = rt_base_exchange, .ddx_control = rt_ddx_control, .base_init = rt_base_init,
    .init_art = rt_init_art, .init_button = rt_init_button, .init_text = rt_init_text,
    .init_list = rt_init_list, .init_edit = rt_init_edit, .base_font = rt_base_font,
    .set_window_text = rt_set_window_text, .get_window_text = rt_get_window_text,
    .set_checked = rt_set_checked, .error_dialog = rt_error_dialog, .post_message = rt_post_message,
    .copy_text = rt_copy_text, .compare_text = rt_compare_text, .close = rt_close,
    .get_window_long = rt_get_window_long, .get_focus = rt_get_focus, .enable_control = rt_enable_control,
    .set_radio = rt_set_radio, .init_slider = rt_init_slider, .slider_range = rt_slider_range,
    .slider_position = rt_slider_position, .set_slider_position = rt_set_slider_position,
    .init_group = rt_init_group, .to_number = rt_to_number, .format_number = rt_format_number,
    .init_picture = rt_init_picture, .set_focus = rt_set_focus, .set_sims_text = rt_set_sims_text,
    .font_construct = rt_font_construct, .font_destroy = rt_font_destroy,
    .register_exit = rt_register_exit, .default_paint = rt_default_paint,
    .string_construct = rt_string_construct, .string_destroy = rt_string_destroy,
    .list_text = rt_list_text, .dialog_item = rt_dialog_item, .dialog_item_window = rt_dialog_item_window,
    .remap_rect = rt_remap_rect, .move_window = rt_move_window, .redraw_window = rt_redraw_window,
    .window_from_handle = rt_window_from_handle, .load_string = rt_load_string,
    .init_shadow_button = rt_init_shadow_button, .window_enabled = rt_window_enabled,
    .init_combo = rt_init_combo, .ddx_text = rt_ddx_text, .ddv_max_chars = rt_ddv_max_chars,
    .message_box = rt_message_box, .is_kind_of = rt_is_kind_of, .show_window = rt_show_window,
    .tab_init = rt_tab_init, .tab_add_page = rt_tab_add_page, .tab_helper_init = rt_tab_helper_init,
    .tab_set_last_page = rt_tab_set_last_page, .tab_set_active = rt_tab_set_active,
    .page_init = rt_page_init, .load_string_a = rt_load_string_a,
};

/* ---- the object services ---------------------------------------------------------------- */
static void *rt_dialog_construct(void *self, int32_t resource, void *parent)
{ return ((void *(TC *)(void *, int32_t, void *))rt_slot(0x135b0))(self, resource, parent); }
static void rt_dialog_destroy(void *self) { THIS0(0x135c8, void)(self); }
static void rt_construct(void *self, uint32_t slot) { THIS0(slot, void *)(self); }
static void rt_destroy(void *self, uint32_t slot) { THIS0(slot, void)(self); }
static void *rt_allocate(uint32_t size) { return ((void *(*)(uint32_t))rt_slot(0x135fc))(size); }
static void rt_ui_delete(void *self) { ((void (*)(void *))rt_slot(0x135ac))(self); }
static void rt_dialog_delete(void *self) { ((void (SC *)(void *))rt_slot(0x13600))(self); }
static int32_t rt_create(void *self, uint32_t resource, void *parent)
{ return ((int32_t (TC *)(void *, uint32_t, void *))rt_slot(0x13378))(self, resource, parent); }
static void *rt_page_construct(void *self, int32_t resource, void *parent)
{ return ((void *(TC *)(void *, int32_t, void *))rt_slot(0x13504))(self, resource, parent); }
static void rt_page_destroy(void *self) { THIS0(0x13508, void)(self); }
static void *rt_string_assign(void *string, const char *text)
{ return ((void *(TC *)(void *, const char *))rt_slot(0x133c4))(string, text); }
static const struct fs98_flights_object_services s_object_services = {
    .dialog_construct = rt_dialog_construct, .dialog_destroy = rt_dialog_destroy,
    .construct = rt_construct, .destroy = rt_destroy, .allocate = rt_allocate,
    .ui_delete = rt_ui_delete, .dialog_delete = rt_dialog_delete, .create = rt_create,
    .show = rt_show_window, .copy_text = rt_copy_text, .page_construct = rt_page_construct,
    .page_destroy = rt_page_destroy, .string_assign = rt_string_assign,
};

/* 0x22317fd0 and 0x22318b60: the base message maps, SIMSUI data imports */
static void *flights_dialog_base_map(void) { return rt_slot(0x135a0); }
static void *flights_page_base_map(void) { return rt_slot(0x13500); }

static int flights_bind(void)
{
    uint32_t index;
    void *tables[CLASS_TOTAL];
    for (index = 0; index < IMPORT_COUNT; index++) {
        const struct flights_import_binding *binding = &s_import_bindings[index];
        HMODULE module = GetModuleHandleA(binding->dll);
        void *function = module ? (void *)GetProcAddress(module, binding->ordinal ?
            (const char *)(uintptr_t)binding->ordinal : binding->name) : 0;
        if (!function) return 0;
        s_import_functions[index] = function;
    }
    for (index = 0; index < sizeof(s_virtual_bindings) / sizeof(s_virtual_bindings[0]); index++) {
        const struct flights_virtual_binding *binding = &s_virtual_bindings[index];
        s_vtables[binding->table][binding->slot] = binding->import_index < 0 ?
            binding->local : s_import_functions[binding->import_index];
    }
    for (index = 0; index < CLASS_TOTAL; index++) {
        tables[index] = s_vtables[index];
        s_message_maps[index].base = s_pages[index] ? flights_page_base_map : flights_dialog_base_map;
        s_message_maps[index].entries = s_messages[index];
    }
    fs98_flights_set_services(&s_services);
    fs98_flights_set_object_services(&s_object_services, tables);
    fs98_flights_set_help_file("fs98.hlp");
    fs98_flights_set_number_format("%.3f");
    fs98_flights_set_create_state(&s_create_state);
    fs98_flights_set_opening_state(&s_opening_state);
    fs98_flights_set_menus_state(&s_menus_state);
    fs98_flights_set_static_vtable(rt_slot(0x13568));
    fs98_flights_set_edit_class(rt_slot(0x133b0));
    return 1;
}

/* 0x22311030: register the dialogs with the host, make the MFC extension's CDynLinkLibrary
 * (freed by MFC's operator delete if its constructor throws) and load the skins */
struct flights_dynamic_unwind { struct fs98_flights_unwind frame; void *dynamic; volatile uint32_t live; };
static void flights_dynamic_cleanup(struct fs98_flights_unwind *frame)
{
    struct flights_dynamic_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; ((void (*)(void *))rt_mfc(825))(state->dynamic); }
}
static FS98_FLIGHTS_NOINLINE void flights_module_init(void)
{
    struct flights_dynamic_unwind state;
    void *dynamic;
    state.dynamic = 0; state.live = 0;
    fs98_flights_unwind_enter(&state.frame, flights_dynamic_cleanup);
    ((void (SC *)(void *, uint32_t *))PTR(g_flights_imports[0].link, 0x298))(&g_flights_linkage, &s_module);
    fs98_flights_set_object_module(s_module);
    dynamic = ((void *(*)(uint32_t))rt_mfc(823))(0x40);
    state.dynamic = dynamic; state.live = 1;
    if (dynamic) ((void *(TC *)(void *, void *, int32_t))rt_mfc(342))(dynamic, s_extension, 0);
    state.live = 0;
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
    fs98_flights_set_resource_module(s_resource_module);
    fs98_flights_unwind_leave(&state.frame);
}
/* 0x223110b0 */
static void flights_module_deinit(void)
{
    if (s_module) ((void (SC *)(uint32_t))PTR(g_flights_imports[0].link, 0x29c))(s_module);
    FreeLibrary((HMODULE)s_resource_module);
    s_module = s_resource_module = 0;
    fs98_flights_set_object_module(0);
    fs98_flights_set_resource_module(0);
}

__declspec(dllimport) void *fs98_flights_simsui_anchor(uint32_t size);
/* 0x22311000 (the module state's raw entry) and 0x223110f0 (DllMain) together */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        void *volatile anchor = (void *)fs98_flights_simsui_anchor;
        (void)anchor;
        if (!flights_bind()) return FALSE;
        {
            void *state = ((void *(*)(void))rt_mfc(1168))();
            D32(state, 0x1c) = D32(state, 0x20); D32(state, 0x28) = D32(state, 0x2c);
            D32(state, 0x20) = D32(state, 0x2c) = 0;
        }
        ((int32_t (SC *)(void *, HINSTANCE))rt_mfc(1182))(s_extension, instance);
        s_module = (uint32_t)instance;
        fs98_flights_set_object_module(s_module);
    } else if (reason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int32_t))rt_mfc(1253))(s_extension, 0);
        while (s_exit_count) {
            void (*handler)(void) = s_exit_handlers[--s_exit_count];
            s_exit_handlers[s_exit_count] = 0;
            handler();
        }
    }
    return TRUE;
}
