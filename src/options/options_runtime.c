/* Native Options adapter. Dialog behaviour is rebuilt (src/options/); the shared MFC/SIMSUI
 * controls stay the CD's dependencies during the compatibility port, reached through the same
 * imports retail uses (tools/gen_options_runtime.py lists them), as Flights' runtime does. */
/* MSVCRT's own sprintf, as retail calls it, not mingw's ANSI replacement */
#define __USE_MINGW_ANSI_STDIO 0
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "options_internal.h"
#include "options_objects.h"
#include "options_unwind.h"
#define TC FS98_OPTIONS_THISCALL
#define SC FS98_OPTIONS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
struct options_import_binding { const char *dll, *name; uint16_t ordinal; uint32_t slot; };
struct options_virtual_binding { int table, slot, import_index; void *local; };
struct options_message_entry { uint32_t message, code, first, last, signature; void *handler; };
struct options_message_map { void *(*base)(void); const struct options_message_entry *entries; };
struct options_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
#define MAP_LIMIT 32
static struct options_message_map s_message_maps[MAP_LIMIT], s_book_list_map;
#define MAP_GETTER(index) \
static void *TC options_map_##index(void *object) { (void)object; return &s_message_maps[index]; }
MAP_GETTER(0) MAP_GETTER(1) MAP_GETTER(2) MAP_GETTER(3) MAP_GETTER(4) MAP_GETTER(5) MAP_GETTER(6)
MAP_GETTER(7) MAP_GETTER(8) MAP_GETTER(9) MAP_GETTER(10) MAP_GETTER(11) MAP_GETTER(12) MAP_GETTER(13)
MAP_GETTER(14) MAP_GETTER(15) MAP_GETTER(16) MAP_GETTER(17) MAP_GETTER(18) MAP_GETTER(19) MAP_GETTER(20)
MAP_GETTER(21) MAP_GETTER(22) MAP_GETTER(23) MAP_GETTER(24) MAP_GETTER(25) MAP_GETTER(26) MAP_GETTER(27)
MAP_GETTER(28) MAP_GETTER(29) MAP_GETTER(30) MAP_GETTER(31)
#undef MAP_GETTER
static void *(TC *const options_map_getters[MAP_LIMIT])(void *) = {
    options_map_0, options_map_1, options_map_2, options_map_3, options_map_4, options_map_5, options_map_6,
    options_map_7, options_map_8, options_map_9, options_map_10, options_map_11, options_map_12, options_map_13,
    options_map_14, options_map_15, options_map_16, options_map_17, options_map_18, options_map_19,
    options_map_20, options_map_21, options_map_22, options_map_23, options_map_24, options_map_25,
    options_map_26, options_map_27, options_map_28, options_map_29, options_map_30, options_map_31,
};
static void *TC options_book_list_map(void *object) { (void)object; return &s_book_list_map; }
/* page 0x386's WM_ACTIVATE entry: CSimsTabPage::OnActivate, an import, filled in at bind as
 * retail's static initialiser (0x22392cc0) fills it */
#define OPTIONS_ON_ACTIVATE 0
#include "options_runtime_tables.inc"
#define IMPORT_COUNT (sizeof(s_import_bindings) / sizeof(s_import_bindings[0]))
static void *s_import_functions[IMPORT_COUNT];
static void *s_vtables[CLASS_TOTAL][61], *s_notify_vtable[NOTIFY_SLOTS], *s_book_list_vtable[BOOK_LIST_SLOTS];
static struct options_message_entry s_mutable_messages[CLASS_TOTAL][64];
static uint8_t s_extension[0x14];      /* AFX_EXTENSION_MODULE, 0x223ac348 */
static uint32_t s_module, s_resource_module;
static struct fs98_options_book_state s_book_state;     /* .data 0x223ac368 */
static struct fs98_options_video_state s_video_state;   /* .data 0x223ac450 */
static void (*s_exit_handlers[8])(void);                 /* what retail gives _onexit */
static uint32_t s_exit_count;
struct options_import { uint32_t id; void *link; };
struct options_linkage {
    uint32_t id; void (*init)(void), (*deinit)(void); void *reserved;
    uint32_t flags; const struct options_dialog *dialogs;
};
static void options_module_init(void);
static void options_module_deinit(void);
struct options_import g_options_imports[] = {{1, 0}, {2, 0}, {0, 0}};
struct options_linkage g_options_linkage = {0x80000007, options_module_init, options_module_deinit, 0, 0, s_dialogs};
typedef char options_map_room[CLASS_TOTAL <= MAP_LIMIT ? 1 : -1];

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
static void *rt_host(uint32_t offset) { return PTR(g_options_imports[0].link, offset); }
#define T0(slot, type) ((type (TC *)(void *))rt_slot(slot))
#define T1(slot, type, a) ((type (TC *)(void *, a))rt_slot(slot))
#define T2(slot, type, a, b) ((type (TC *)(void *, a, b))rt_slot(slot))

/* ---- Win32 and the C runtime, called directly ------------------------------------------- */
static int32_t rt_send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return (int32_t)SendMessageA((HWND)window, message, w_param, l_param); }
static int32_t rt_win_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{ return WinHelpA((HWND)window, file, command, (ULONG_PTR)table); }
static int32_t rt_enable_window(uint32_t window, int32_t enable) { return EnableWindow((HWND)window, enable); }
static int32_t rt_is_lead_byte(int32_t character) { return isleadbyte(character); }
static uint32_t rt_get_focus(void) { return (uint32_t)GetFocus(); }
static int32_t rt_compare_text(const char *a, const char *b) { return lstrcmpA(a, b); }
static char *rt_copy_text(char *out, const char *text) { return lstrcpyA(out, text); }
static int32_t rt_post_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return PostMessageA((HWND)window, message, w_param, l_param); }
static int32_t rt_load_string_a(uint32_t instance, uint32_t id, char *buffer, int32_t size)
{ return LoadStringA((HINSTANCE)instance, id, buffer, size); }
static int32_t rt_get_client_rect(uint32_t window, struct fs98_options_rect *rect)
{ return GetClientRect((HWND)window, (RECT *)rect); }
static int32_t rt_text_length(const char *text) { return lstrlenA(text); }
static int32_t rt_format_text(char *out, const char *format, const char *text) { return sprintf(out, format, text); }
static int32_t rt_format_int(char *out, const char *format, int32_t value) { return sprintf(out, format, value); }
static int32_t rt_get_window_rect(uint32_t window, struct fs98_options_rect *rect)
{ return GetWindowRect((HWND)window, (RECT *)rect); }
static int32_t rt_screen_to_client(uint32_t window, void *point) { return ScreenToClient((HWND)window, (POINT *)point); }
static int32_t rt_client_to_screen(uint32_t window, void *point) { return ClientToScreen((HWND)window, (POINT *)point); }
static int32_t rt_to_int(const char *text) { return atoi(text); }
static int32_t rt_to_long(const char *text) { return atol(text); }
static char *rt_long_to_text(int32_t value, char *out, int32_t radix) { return _ltoa(value, out, radix); }
static int32_t rt_get_window_long(uint32_t window, int32_t index) { return GetWindowLongA((HWND)window, index); }
static int16_t rt_async_key_state(int32_t key) { return GetAsyncKeyState(key); }
static int32_t rt_redraw_window(uint32_t window, const void *rect, uint32_t region, uint32_t flags)
{ return RedrawWindow((HWND)window, (const RECT *)rect, (HRGN)region, flags); }
static uint32_t rt_load_library(const char *name) { return (uint32_t)LoadLibraryA(name); }
static void *rt_get_proc_address(uint32_t library, const char *name)
{ return (void *)GetProcAddress((HMODULE)library, name); }
static int32_t rt_free_library(uint32_t library) { return FreeLibrary((HMODULE)library); }
static int32_t rt_set_scroll_pos_window(uint32_t window, int32_t bar, int32_t position, int32_t redraw)
{ return SetScrollPos((HWND)window, bar, position, redraw); }
static int32_t rt_get_scroll_pos_window(uint32_t window, int32_t bar) { return GetScrollPos((HWND)window, bar); }
static int32_t rt_set_scroll_range_window(uint32_t window, int32_t bar, int32_t low, int32_t high, int32_t redraw)
{ return SetScrollRange((HWND)window, bar, low, high, redraw); }
static int32_t rt_get_scroll_range_window(uint32_t window, int32_t bar, int32_t *low, int32_t *high)
{ return GetScrollRange((HWND)window, bar, (int *)low, (int *)high); }
static int32_t rt_enable_scroll_bar(uint32_t window, uint32_t bar, uint32_t arrows)
{ return EnableScrollBar((HWND)window, bar, arrows); }
static int32_t rt_update_window(uint32_t window) { return UpdateWindow((HWND)window); }
static int32_t rt_format_two(char *out, const char *format, const char *a, const char *b)
{ return wsprintfA(out, format, a, b); }
/* atexit through _onexit: the handlers run when the DLL is unloaded */
static void rt_register_exit(void (*handler)(void))
{ if (s_exit_count < sizeof(s_exit_handlers) / sizeof(s_exit_handlers[0])) s_exit_handlers[s_exit_count++] = handler; }

/* ---- HOST, through its import link ------------------------------------------------------ */
static char *rt_host_strcpy(char *out, const char *text) { return ((char *(SC *)(char *, const char *))rt_host(0x2a8))(out, text); }
static int32_t rt_host_strcmpi(const char *a, const char *b) { return ((int32_t (SC *)(const char *, const char *))rt_host(0x2c0))(a, b); }
static uint32_t rt_host_joystick_count(void) { return ((uint32_t (SC *)(void))rt_host(0x1c0))(); }
static void rt_host_key_name(uint32_t key, char *out, uint32_t size)
{ ((void (SC *)(uint32_t, char *, uint32_t))rt_host(0x200))(key, out, size); }
static void rt_host_joystick_name(uint32_t input, char *out, uint32_t size)
{ ((void (SC *)(uint32_t, char *, uint32_t))rt_host(0x1d0))(input, out, size); }
static uint32_t rt_host_dialog(uint32_t id, void *block) { return ((uint32_t (SC *)(uint32_t, void *))rt_host(0x10c))(id, block); }

/* ---- MFC and SIMSUI, through their imports ----------------------------------------------- */
static void rt_ddx_control(void *exchange, uint32_t id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))rt_slot(0x1d56c))(exchange, id, control); }
static int32_t rt_base_init(void *object) { return T0(0x1d6e8, int32_t)(object); }
static int32_t rt_init_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                              void *dib, void *palette, uint32_t flags, uint32_t color,
                              uint32_t module, int32_t x, int32_t y, struct fs98_options_rect rect)
{
    return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t,
                            int32_t, int32_t, struct fs98_options_rect))rt_slot(0x1d750))(
        control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect);
}
static void rt_init_text(void *control, void *font) { T1(0x1d774, void, void *)(control, font); }
static void rt_init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                         uint32_t error_color, void *label, uint32_t flags)
{
    ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))rt_slot(0x1d6bc))(
        control, font, foreground, background, error_color, label, flags);
}
static void *rt_base_font(int32_t id) { return ((void *(SC *)(int32_t))rt_slot(0x1d6f4))(id); }
static void rt_set_window_text(void *window, const char *text) { T1(0x1d538, void, const char *)(window, text); }
static int32_t rt_get_window_text(void *window, char *buffer, int32_t count)
{ return T2(0x1d4c0, int32_t, char *, int32_t)(window, buffer, count); }
static void *rt_set_focus(void *window) { return T0(0x1d488, void *)(window); }
static void rt_base_exchange(void *object, void *exchange) { T1(0x1d718, void, void *)(object, exchange); }
static int32_t rt_init_art(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return ((int32_t (TC *)(void *, uint32_t, void *, void *, uint32_t))rt_slot(0x1d6ec))(control, bitmap, dib, palette, flags); }
static void rt_init_list(void *control, void *font, void *label) { T2(0x1d6a0, void, void *, void *)(control, font, label); }
static void rt_init_combo(void *control, void *font, void *label) { T2(0x1d754, void, void *, void *)(control, font, label); }
static int32_t rt_error_dialog(void *object, void *control, uint32_t code)
{ return T2(0x1d6d4, int32_t, void *, uint32_t)(object, control, code); }
static void *rt_window_from_handle(uint32_t window) { return ((void *(SC *)(uint32_t))rt_slot(0x1d4bc))(window); }
static int32_t rt_enable_control(void *window, int32_t enable) { return T1(0x1d568, int32_t, int32_t)(window, enable); }
static int32_t rt_window_enabled(void *window) { return T0(0x1d4cc, int32_t)(window); }
static int32_t rt_set_checked(void *control, int32_t checked) { return T1(0x1d76c, int32_t, int32_t)(control, checked); }
static void *rt_allocate(uint32_t size) { return ((void *(*)(uint32_t))rt_slot(0x1d7d4))(size); }
static void rt_notify_construct(void *notify, uint32_t a, uint32_t b) { T2(0x1d6f0, void *, uint32_t, uint32_t)(notify, a, b); }
static void rt_notify_free(void *notify) { ((void (SC *)(void *))rt_slot(0x1d64c))(notify); }
static void rt_notify_destroy(void *notify) { T0(0x1d6e4, void)(notify); }
static void rt_ui_delete(void *object) { ((void (*)(void *))rt_slot(0x1d784))(object); }
static void rt_close(void *object) { T0(0x1d7d0, void)(object); }
static void rt_string_construct(void *string) { T0(0x1d530, void *)(string); }
static void rt_string_destroy(void *string) { T0(0x1d53c, void)(string); }
static void rt_string_assign(void *string, const char *text) { T1(0x1d4f8, void *, const char *)(string, text); }
static void rt_ddx_text(void *exchange, uint32_t id, void *string)
{ ((void (SC *)(void *, uint32_t, void *))rt_slot(0x1d4d0))(exchange, id, string); }
static void rt_ddv_max_chars(void *exchange, void *string, int32_t count)
{ ((void (SC *)(void *, void *, int32_t))rt_slot(0x1d4d4))(exchange, string, count); }
static int32_t rt_show_window(void *window, int32_t show) { return T1(0x1d49c, int32_t, int32_t)(window, show); }
static void rt_init_group(void *control, void *font) { T1(0x1d740, void, void *)(control, font); }
static void rt_set_sims_text(void *control, const char *text) { T1(0x1d6b8, void, const char *)(control, text); }
static void rt_get_sims_text(void *control, void *string) { T1(0x1d68c, void, void *)(control, string); }
static void rt_window_text_string(void *window, void *string) { T1(0x1d534, void, void *)(window, string); }
static void rt_font_construct(void *font, void *base_font) { T1(0x1d698, void *, void *)(font, base_font); }
static void rt_font_destroy(void *font) { T0(0x1d694, void)(font); }
static int32_t rt_set_tab_stops(void *font, int32_t count, int32_t *stops)
{ return T2(0x1d69c, int32_t, int32_t, int32_t *)(font, count, stops); }
static int32_t rt_get_sims_text_buffer(void *control, char *buffer, int32_t count)
{ return T2(0x1d6a4, int32_t, char *, int32_t)(control, buffer, count); }
static void rt_string_append(void *string, const char *text) { T1(0x1d4fc, void *, const char *)(string, text); }
static void rt_string_append_char(void *string, char character) { T1(0x1d4c8, void *, char)(string, character); }
static void rt_list_text(void *list, int32_t index, void *string) { T2(0x1d4c4, void, int32_t, void *)(list, index, string); }
static void rt_list_destroy(void *list) { T0(0x1d6ac, void)(list); }
static void *rt_dialog_item(void *object, int32_t id) { return T1(0x1d510, void *, int32_t)(object, id); }
static void rt_remap_rect(struct fs98_options_rect *rect, void *control)
{ ((void (SC *)(struct fs98_options_rect *, void *))rt_slot(0x1d6c0))(rect, control); }
static void rt_move_window(void *window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t))rt_slot(0x1d518))(window, x, y, width, height, repaint); }
static char *rt_get_buffer(void *string, int32_t size) { return T1(0x1d514, char *, int32_t)(string, size); }
static void rt_release_buffer(void *string, int32_t size) { T1(0x1d500, void, int32_t)(string, size); }
static void rt_string_append_string(void *string, const void *other) { T1(0x1d504, void *, const void *)(string, other); }
static int32_t rt_message_box(const char *text, uint32_t type, uint32_t help)
{ return ((int32_t (SC *)(const char *, uint32_t, uint32_t))rt_slot(0x1d4a8))(text, type, help); }
static void rt_paint_construct(void *dc, void *window) { T1(0x1d4f0, void *, void *)(dc, window); }
static void rt_paint_destroy(void *dc) { T0(0x1d4f4, void)(dc); }
static int32_t rt_dc_mode(void *dc, int32_t mode) { return T1(0x1d508, int32_t, int32_t)(dc, mode); }
static uint32_t rt_dc_align(void *dc, uint32_t align) { return T1(0x1d50c, uint32_t, uint32_t)(dc, align); }
static int32_t rt_set_radio(void *control, int32_t checked) { return T1(0x1d6d8, int32_t, int32_t)(control, checked); }
static void rt_ddx_int(void *exchange, uint32_t id, int32_t *value)
{ ((void (SC *)(void *, uint32_t, int32_t *))rt_slot(0x1d51c))(exchange, id, value); }
static void rt_ddv_min_max(void *exchange, int32_t value, int32_t low, int32_t high)
{ ((void (SC *)(void *, int32_t, int32_t, int32_t))rt_slot(0x1d520))(exchange, value, low, high); }
static int32_t rt_notify_send(void *notify, const char *title, const char *format, void *value)
{ return ((int32_t (*)(void *, const char *, const char *, ...))rt_slot(0x1d684))(notify, title, format, value); }
static int32_t rt_tab_init(void *tab, void *dialog, void *font, uint32_t bitmap, void *dib, void *palette,
                           uint32_t a, uint32_t color, int32_t x, int32_t y, int32_t width, int32_t height,
                           uint32_t module, uint32_t b)
{
    return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, int32_t, int32_t,
                            int32_t, int32_t, uint32_t, uint32_t))rt_slot(0x1d6f8))(
        tab, dialog, font, bitmap, dib, palette, a, color, x, y, width, height, module, b);
}
static int32_t rt_page_init(void *page, int32_t value) { return T1(0x1d6fc, int32_t, int32_t)(page, value); }
static void rt_add_page(void *tab, void *page, char *title, int32_t value)
{ ((void (TC *)(void *, void *, char *, int32_t))rt_slot(0x1d700))(tab, page, title, value); }
static void rt_helper_init(void *helper, void *tab) { T1(0x1d704, void, void *)(helper, tab); }
static int32_t rt_set_last_page(void *tab, void *page) { return T1(0x1d708, int32_t, void *)(tab, page); }
static int32_t rt_set_active_tab(void *tab, int32_t index, uint32_t window)
{ return T2(0x1d70c, int32_t, int32_t, uint32_t)(tab, index, window); }
static int32_t rt_get_active_tab(void *tab) { return T0(0x1d688, int32_t)(tab); }
static int32_t rt_slider_init(void *slider, uint32_t bitmap, uint32_t thumb, void *dib, void *palette, uint32_t a,
                              uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f, uint32_t module)
{
    return ((int32_t (TC *)(void *, uint32_t, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t, uint32_t,
                            int32_t, int32_t, uint32_t))rt_slot(0x1d778))(
        slider, bitmap, thumb, dib, palette, a, b, c, d, e, f, module);
}
static void rt_set_scroll_range(void *slider, int32_t low, int32_t high) { T2(0x1d77c, void, int32_t, int32_t)(slider, low, high); }
static int32_t rt_get_scroll_pos(void *slider) { return T0(0x1d770, int32_t)(slider); }
static int32_t rt_set_scroll_pos(void *slider, int32_t position) { return T1(0x1d768, int32_t, int32_t)(slider, position); }
static int32_t rt_string_load(void *string, uint32_t id) { return T1(0x1d4b8, int32_t, uint32_t)(string, id); }
static void rt_string_format(void *string, uint32_t id, uint32_t value)
{ ((void (*)(void *, uint32_t, ...))rt_slot(0x1d4a4))(string, id, value); }
static void rt_check_list_init(void *list, void *font, void *label) { T2(0x1d65c, void, void *, void *)(list, font, label); }
static void rt_check_list_enable(void *list, int32_t index, int32_t enable) { T2(0x1d478, void, int32_t, int32_t)(list, index, enable); }
static void rt_check_list_set_check(void *list, int32_t index, int32_t check) { T2(0x1d47c, void, int32_t, int32_t)(list, index, check); }
static int32_t rt_check_list_get_check(void *list, int32_t index) { return T1(0x1d4ac, int32_t, int32_t)(list, index); }
static int32_t rt_hot_create(uint32_t slot, void *control, uint32_t style, const struct fs98_options_rect *rect,
                             void *parent, uint32_t id)
{
    return ((int32_t (TC *)(void *, uint32_t, const struct fs98_options_rect *, void *, uint32_t))rt_slot(slot))(
        control, style, rect, parent, id);
}
static int32_t rt_hot_key_create(void *control, uint32_t style, const struct fs98_options_rect *rect, void *parent, uint32_t id)
{ return rt_hot_create(0x1d660, control, style, rect, parent, id); }
static int32_t rt_hot_joy_create(void *control, uint32_t style, const struct fs98_options_rect *rect, void *parent, uint32_t id)
{ return rt_hot_create(0x1d7dc, control, style, rect, parent, id); }
static void rt_hot_key_init(void *control, void *font, uint32_t flags) { T2(0x1d7e0, void, void *, uint32_t)(control, font, flags); }
static void rt_hot_joy_init(void *control, void *font, uint32_t flags) { T2(0x1d664, void, void *, uint32_t)(control, font, flags); }
static int32_t rt_hot_key_get(void *control) { return T0(0x1d650, int32_t)(control); }
static uint32_t rt_hot_joy_get(void *control) { return T0(0x1d710, uint32_t)(control); }
static void rt_hot_key_set(void *control, int32_t key) { T1(0x1d658, void, int32_t)(control, key); }
static void rt_hot_joy_set(void *control, uint32_t joystick, uint32_t sense) { T2(0x1d654, void, uint32_t, uint32_t)(control, joystick, sense); }

static const struct fs98_options_services s_services = {
    .send_message = rt_send_message, .win_help = rt_win_help, .enable_window = rt_enable_window,
    .ddx_control = rt_ddx_control, .base_init = rt_base_init, .init_button = rt_init_button,
    .init_text = rt_init_text, .init_edit = rt_init_edit, .base_font = rt_base_font,
    .set_window_text = rt_set_window_text, .get_window_text = rt_get_window_text, .set_focus = rt_set_focus,
    .is_lead_byte = rt_is_lead_byte, .base_exchange = rt_base_exchange, .init_art = rt_init_art,
    .init_list = rt_init_list, .init_combo = rt_init_combo, .error_dialog = rt_error_dialog,
    .get_focus = rt_get_focus, .window_from_handle = rt_window_from_handle, .compare_text = rt_compare_text,
    .copy_text = rt_copy_text, .post_message = rt_post_message, .enable_control = rt_enable_control,
    .window_enabled = rt_window_enabled, .set_checked = rt_set_checked, .host_strcpy = rt_host_strcpy,
    .host_strcmpi = rt_host_strcmpi, .allocate = rt_allocate, .notify_construct = rt_notify_construct,
    .notify_free = rt_notify_free, .notify_destroy = rt_notify_destroy, .ui_delete = rt_ui_delete,
    .close = rt_close, .string_construct = rt_string_construct, .string_destroy = rt_string_destroy,
    .string_assign = rt_string_assign, .ddx_text = rt_ddx_text, .ddv_max_chars = rt_ddv_max_chars,
    .show_window = rt_show_window, .load_string_a = rt_load_string_a, .init_group = rt_init_group,
    .set_sims_text = rt_set_sims_text, .get_sims_text = rt_get_sims_text, .window_text_string = rt_window_text_string,
    .font_construct = rt_font_construct, .font_destroy = rt_font_destroy, .register_exit = rt_register_exit,
    .set_tab_stops = rt_set_tab_stops, .get_client_rect = rt_get_client_rect,
    .get_sims_text_buffer = rt_get_sims_text_buffer, .text_length = rt_text_length,
    .string_append = rt_string_append, .string_append_char = rt_string_append_char, .list_text = rt_list_text,
    .format_text = rt_format_text, .list_destroy = rt_list_destroy, .dialog_item = rt_dialog_item,
    .remap_rect = rt_remap_rect, .move_window = rt_move_window, .get_buffer = rt_get_buffer,
    .release_buffer = rt_release_buffer, .string_append_string = rt_string_append_string,
    .message_box = rt_message_box, .paint_construct = rt_paint_construct, .paint_destroy = rt_paint_destroy,
    .dc_mode = rt_dc_mode, .dc_align = rt_dc_align, .get_window_rect = rt_get_window_rect,
    .screen_to_client = rt_screen_to_client, .to_int = rt_to_int, .format_int = rt_format_int,
    .get_window_long = rt_get_window_long, .set_radio = rt_set_radio, .long_to_text = rt_long_to_text,
    .to_long = rt_to_long, .ddx_int = rt_ddx_int, .ddv_min_max = rt_ddv_min_max, .notify_send = rt_notify_send,
    .async_key_state = rt_async_key_state, .tab_init = rt_tab_init, .page_init = rt_page_init,
    .add_page = rt_add_page, .helper_init = rt_helper_init, .set_last_page = rt_set_last_page,
    .set_active_tab = rt_set_active_tab, .get_active_tab = rt_get_active_tab, .slider_init = rt_slider_init,
    .set_scroll_range = rt_set_scroll_range, .get_scroll_pos = rt_get_scroll_pos, .set_scroll_pos = rt_set_scroll_pos,
    .string_load = rt_string_load, .redraw_window = rt_redraw_window, .load_library = rt_load_library,
    .get_proc_address = rt_get_proc_address, .free_library = rt_free_library,
    .host_joystick_count = rt_host_joystick_count, .set_scroll_pos_window = rt_set_scroll_pos_window,
    .get_scroll_pos_window = rt_get_scroll_pos_window, .set_scroll_range_window = rt_set_scroll_range_window,
    .get_scroll_range_window = rt_get_scroll_range_window, .enable_scroll_bar = rt_enable_scroll_bar,
    .client_to_screen = rt_client_to_screen, .update_window = rt_update_window, .format_two = rt_format_two,
    .string_format = rt_string_format, .check_list_init = rt_check_list_init,
    .check_list_enable = rt_check_list_enable, .check_list_set_check = rt_check_list_set_check,
    .check_list_get_check = rt_check_list_get_check, .hot_key_create = rt_hot_key_create,
    .hot_key_init = rt_hot_key_init, .hot_key_get = rt_hot_key_get, .hot_key_set = rt_hot_key_set,
    .hot_joy_create = rt_hot_joy_create, .hot_joy_init = rt_hot_joy_init, .hot_joy_get = rt_hot_joy_get,
    .hot_joy_set = rt_hot_joy_set, .host_key_name = rt_host_key_name, .host_joystick_name = rt_host_joystick_name,
    .host_dialog = rt_host_dialog,
};

/* ---- the objects' services ---------------------------------------------------------------- */
static void *rt_dialog_construct(void *self, int32_t resource, void *parent)
{ return T2(0x1d71c, void *, int32_t, void *)(self, resource, parent); }
static void rt_dialog_destroy(void *self) { T0(0x1d72c, void)(self); }
static void rt_construct(void *self, uint32_t slot) { T0(slot, void *)(self); }
static void rt_destroy(void *self, uint32_t slot) { T0(slot, void)(self); }
static void rt_dialog_delete(void *self) { ((void (SC *)(void *))rt_slot(0x1d7d8))(self); }
static int32_t rt_create(void *self, uint32_t resource, void *parent)
{ return T2(0x1d4a0, int32_t, uint32_t, void *)(self, resource, parent); }
static void *rt_page_construct(void *self, int32_t resource, void *parent)
{ return T2(0x1d788, void *, int32_t, void *)(self, resource, parent); }
static void rt_page_destroy(void *self) { T0(0x1d798, void)(self); }
static const struct fs98_options_object_services s_object_services = {
    .dialog_construct = rt_dialog_construct, .dialog_destroy = rt_dialog_destroy,
    .construct = rt_construct, .destroy = rt_destroy, .allocate = rt_allocate,
    .ui_delete = rt_ui_delete, .dialog_delete = rt_dialog_delete, .create = rt_create,
    .show = rt_show_window, .page_construct = rt_page_construct, .page_destroy = rt_page_destroy,
    .copy_text = rt_copy_text, .string_assign = rt_string_assign, .message_box = rt_message_box,
};

/* the base message maps, SIMSUI data imports: CSimsDialog's, CSimsTabPage's, CSimsListBox's */
static void *options_dialog_base_map(void) { return rt_slot(0x1d714); }
static void *options_page_base_map(void) { return rt_slot(0x1d780); }
static void *options_list_base_map(void) { return rt_slot(0x1d690); }

static void options_fill(void **vtable, const struct options_virtual_binding *bindings)
{
    for (; bindings->table >= 0; ++bindings)
        vtable[bindings->slot] = bindings->import_index < 0 ? bindings->local : s_import_functions[bindings->import_index];
}
static int options_bind(void)
{
    uint32_t index;
    void *tables[CLASS_TOTAL];
    for (index = 0; index < IMPORT_COUNT; index++) {
        const struct options_import_binding *binding = &s_import_bindings[index];
        HMODULE module = GetModuleHandleA(binding->dll);
        void *function = module ? (void *)GetProcAddress(module, binding->ordinal ?
            (const char *)(uintptr_t)binding->ordinal : binding->name) : 0;
        if (!function) return 0;
        s_import_functions[index] = function;
    }
    for (index = 0; index < sizeof(s_virtual_bindings) / sizeof(s_virtual_bindings[0]); index++) {
        const struct options_virtual_binding *binding = &s_virtual_bindings[index];
        s_vtables[binding->table][binding->slot] = binding->import_index < 0 ?
            binding->local : s_import_functions[binding->import_index];
    }
    options_fill(s_notify_vtable, s_notify_bindings);
    options_fill(s_book_list_vtable, s_book_list_bindings);
    for (index = 0; index < CLASS_TOTAL; index++) {
        uint32_t entry;
        tables[index] = s_vtables[index];
        /* a mutable copy of each map, its WM_ACTIVATE entry given CSimsTabPage::OnActivate */
        for (entry = 0; entry < 64; ++entry) {
            s_mutable_messages[index][entry] = s_messages[index][entry];
            if (s_messages[index][entry].message == 0 && !s_messages[index][entry].handler) break;
            if (!s_mutable_messages[index][entry].handler)
                s_mutable_messages[index][entry].handler = rt_slot(0x1d74c);
        }
        s_message_maps[index].base = s_pages[index] ? options_page_base_map : options_dialog_base_map;
        s_message_maps[index].entries = s_mutable_messages[index];
    }
    s_book_list_map.base = options_list_base_map;
    s_book_list_map.entries = s_book_list_messages;
    fs98_options_set_services(&s_services);
    fs98_options_set_object_services(&s_object_services, tables);
    fs98_options_set_help_file("fs98.hlp");
    fs98_options_set_book_state(&s_book_state);
    fs98_options_set_video_state(&s_video_state);
    fs98_options_set_notify_vtable(s_notify_vtable);
    fs98_options_set_book_list_vtable(s_book_list_vtable);
    fs98_options_set_object_list_vtable(s_book_list_vtable);
    fs98_options_set_spin_vtable(rt_slot(0x1d6c4));
    fs98_options_set_scroll_vtable(rt_slot(0x1d66c));
    return 1;
}

/* 0x22391030: register the dialogs with the host, make the MFC extension's CDynLinkLibrary
 * (freed by MFC's operator delete if its constructor throws) and load the skins */
struct options_dynamic_unwind { struct fs98_options_unwind frame; void *dynamic; volatile uint32_t live; };
static void options_dynamic_cleanup(struct fs98_options_unwind *frame)
{
    struct options_dynamic_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; ((void (*)(void *))rt_mfc(825))(state->dynamic); }
}
static FS98_OPTIONS_NOINLINE void options_module_init(void)
{
    struct options_dynamic_unwind state;
    void *dynamic;
    state.dynamic = 0; state.live = 0;
    fs98_options_unwind_enter(&state.frame, options_dynamic_cleanup);
    ((void (SC *)(void *, uint32_t *))rt_host(0x298))(&g_options_linkage, &s_module);
    fs98_options_set_object_module(s_module);
    dynamic = ((void *(*)(uint32_t))rt_mfc(823))(0x40);
    state.dynamic = dynamic; state.live = 1;
    if (dynamic) ((void *(TC *)(void *, void *, int32_t))rt_mfc(342))(dynamic, s_extension, 0);
    state.live = 0;
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
    fs98_options_set_resource_module(s_resource_module);
    fs98_options_unwind_leave(&state.frame);
}
/* 0x223910b0 */
static void options_module_deinit(void)
{
    if (s_module) ((void (SC *)(uint32_t))rt_host(0x29c))(s_module);
    FreeLibrary((HMODULE)s_resource_module);
    s_module = s_resource_module = 0;
    fs98_options_set_object_module(0);
    fs98_options_set_resource_module(0);
}

__declspec(dllimport) void *fs98_options_simsui_anchor(uint32_t size);
/* 0x22391000 (the module state's raw entry) and 0x223910f0 (DllMain) together */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        void *volatile anchor = (void *)fs98_options_simsui_anchor;
        (void)anchor;
        if (!options_bind()) return FALSE;
        {
            void *state = ((void *(*)(void))rt_mfc(1168))();
            D32(state, 0x1c) = D32(state, 0x20); D32(state, 0x28) = D32(state, 0x2c);
            D32(state, 0x20) = D32(state, 0x2c) = 0;
        }
        ((int32_t (SC *)(void *, HINSTANCE))rt_mfc(1182))(s_extension, instance);
        s_module = (uint32_t)instance;
        fs98_options_set_object_module(s_module);
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
