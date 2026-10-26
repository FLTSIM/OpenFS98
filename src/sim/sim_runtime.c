/* Native Sim adapter. Dialog behavior is rebuilt; shared MFC/SIMSUI
 * controls remain the CD dependencies during the compatibility port. */
#include <windows.h>
#include <stdlib.h>
#include "sim_internal.h"
#include "sim_unwind.h"
#define TC FS98_SIM_THISCALL
#define SC FS98_SIM_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
struct sim_import_binding { const char *dll, *name; uint16_t ordinal, slot; };
struct sim_virtual_binding { int table, slot, import_index; void *local; };
struct sim_message_entry { uint32_t message, code, first, last, signature; void *handler; };
struct sim_message_map { void *(*base)(void); const struct sim_message_entry *entries; };
struct sim_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
static struct sim_message_map s_message_maps[8];
#include "sim_runtime_tables.inc"
static void *s_import_functions[sizeof(s_import_bindings) / sizeof(s_import_bindings[0])];
static void *s_vtables[10][61];
static uint8_t s_extension[0x14];
static uint32_t s_module, s_resource_module;
struct sim_import { uint32_t id; void *link; };
struct sim_linkage {
    uint32_t id; void (*init)(void), (*deinit)(void); void *reserved;
    uint32_t flags; const struct sim_dialog *dialogs;
};
static void sim_module_init(void);
static void sim_module_deinit(void);
struct sim_import g_sim_imports[] = {{1, 0}, {2, 0}, {0, 0}};
struct sim_linkage g_sim_linkage = {0x80000004, sim_module_init, sim_module_deinit, 0, 0, s_dialogs};
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

static void *rt_base_construct(void *object, uint32_t import, int32_t template, void *parent)
{ return ((void *(TC *)(void *, int32_t, void *))rt_slot(import))(object, template, parent); }

static void rt_destroy(void *object, uint32_t import)
{ ((void (TC *)(void *))rt_slot(import))(object); }

static void rt_construct(void *object, uint32_t import)
{ ((void (TC *)(void *))rt_slot(import))(object); }

static void *rt_allocate(uint32_t bytes)
{ return ((void *(*)(uint32_t))rt_slot(0xd5b4))(bytes); }

static void rt_ui_delete(void *object)
{ ((void (*)(void *))rt_slot(0xd574))(object); }

static void rt_mfc_delete(void *object)
{ ((void (*)(void *))rt_mfc(825))(object); }

static void rt_dialog_delete(void *object)
{ ((void (SC *)(void *))rt_slot(0xd5b8))(object); }

static int32_t rt_create(void *object, uint32_t template, void *parent)
{ return ((int32_t (TC *)(void *, uint32_t, void *))rt_mfc(2086))(object, template, parent); }

static int32_t rt_show(void *object, int32_t show)
{ return ((int32_t (TC *)(void *, int32_t))rt_mfc(6215))(object, show); }

static void rt_ddx(void *exchange, uint32_t id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))rt_mfc(2302))(exchange, id, control); }

static int32_t rt_help(uint32_t window, const char *file, uint32_t command, const uint32_t *map)
{ return WinHelpA((HWND)window, file, command, (ULONG_PTR)map); }

static int32_t rt_enable_window(uint32_t window, int32_t enable)
{ return EnableWindow((HWND)window, enable); }

static int32_t rt_enable_control(void *control, int32_t enable)
{ return ((int32_t (TC *)(void *, int32_t))rt_mfc(2642))(control, enable); }

static int32_t rt_window_long(uint32_t window, int32_t index)
{ return GetWindowLongA((HWND)window, index); }

static int32_t rt_check(void *control, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0xd504))(control, checked); }

static int32_t rt_slider_get(void *control)
{ return ((int32_t (TC *)(void *))rt_slot(0xd4c8))(control); }

static void rt_slider_range(void *control, int32_t min, int32_t max)
{ ((void (TC *)(void *, int32_t, int32_t))rt_slot(0xd4c0))(control, min, max); }

static int32_t rt_slider_set(void *control, int32_t position)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0xd4c4))(control, position); }

static int32_t rt_send(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return SendMessageA((HWND)window, message, w_param, l_param); }

static void rt_text(void *control, const char *text)
{ ((void (TC *)(void *, const char *))rt_slot(0xd548))(control, text); }

static void rt_close(void *object)
{ ((void (TC *)(void *))rt_slot(0xd56c))(object); }

static int32_t rt_radio_check(void *control, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0xd508))(control, checked); }

static void *rt_active_page(void *tabs)
{ return ((void *(TC *)(void *))rt_slot(0xd4ac))(tabs); }

static uint32_t rt_focus(void)
{ return (uint32_t)GetFocus(); }

static void *rt_from_handle(uint32_t window)
{ return ((void *(SC *)(uint32_t))rt_mfc(2864))(window); }

static int32_t rt_is_edit(void *control)
{ return ((int32_t (TC *)(void *, void *))rt_mfc(4083))(control, rt_mfc(1829)); }

static int32_t rt_control_id(void *control)
{ return ((int32_t (TC *)(void *))rt_mfc(3089))(control); }

static int32_t rt_base_init(void *object)
{ return ((int32_t (TC *)(void *))rt_slot(0xd550))(object); }

static int32_t rt_init_button(void *control, void *font, void *alternate_font, uint32_t bitmap, void *dib, void *palette, uint32_t flags, uint32_t color, uint32_t module, int32_t x, int32_t y, struct fs98_sim_rect rect)
{ return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t, int32_t, int32_t, struct fs98_sim_rect))rt_slot(0xd554))(control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect); }

static int32_t rt_init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return ((int32_t (TC *)(void *, int32_t, void *, void *, uint32_t))rt_slot(0xd558))(control, bitmap, dib, palette, flags); }

static void rt_init_text(void *control, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0xd55c))(control, font); }

static void rt_notify_construct(void *notify, uint32_t flags, uint32_t mode)
{ ((void (TC *)(void *, uint32_t, uint32_t))rt_slot(0xd560))(notify, flags, mode); }

static void rt_notify_delete(void *notify)
{ ((void (SC *)(void *))rt_slot(0xd568))(notify); }

/* Variadic CNotify::Send passes this on the cdecl stack, not in ECX. */
static int32_t rt_notify_send(void *notify, const char *format, const char *title, void *argument)
{ return ((int32_t (*)(void *, const char *, const char *, void *))rt_slot(0xd544))(notify, format, title, argument); }

static void *rt_get_control(void *object, uint32_t id)
{ return ((void *(TC *)(void *, uint32_t))rt_mfc(3092))(object, id); }

static void rt_remap_rect(struct fs98_sim_rect *rect, void *control)
{ ((void (SC *)(struct fs98_sim_rect *, void *))rt_slot(0xd564))(rect, control); }

static void rt_move_control(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t))rt_mfc(4299))(control, x, y, width, height, repaint); }

static void rt_paint_construct(void *dc, void *object)
{ ((void (TC *)(void *, void *))rt_mfc(470))(dc, object); }

static void rt_paint_destroy(void *dc)
{ ((void (TC *)(void *))rt_mfc(755))(dc); }

static int32_t rt_load_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{ return LoadStringA((HMODULE)module, id, buffer, capacity); }

static int32_t rt_invalidate(uint32_t window, const void *rect, int32_t erase)
{ return InvalidateRect((HWND)window, rect, erase); }

static int32_t rt_set_rect(struct fs98_sim_rect *rect, int32_t left, int32_t top, int32_t right, int32_t bottom)
{ return SetRect((RECT *)rect, left, top, right, bottom); }

static void rt_string_construct_text(void *string, const char *text)
{ ((void (TC *)(void *, const char *))rt_mfc(537))(string, text); }

static void rt_draw_text(void *font, void *dc, uint32_t string, struct fs98_sim_rect rect, int32_t flags)
{ ((void (TC *)(void *, void *, uint32_t, struct fs98_sim_rect, int32_t))rt_slot(0xd540))(font, dc, string, rect, flags); }

static void rt_init_group(void *control, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0xd4ec))(control, font); }

static void rt_init_combo(void *control, void *font, void *label)
{ ((void (TC *)(void *, void *, void *))rt_slot(0xd4f0))(control, font, label); }

static void rt_control_text(void *control, const char *text)
{ ((void (TC *)(void *, const char *))rt_mfc(6199))(control, text); }

static int32_t rt_get_text(void *control, char *buffer, int32_t capacity)
{ return ((int32_t (TC *)(void *, char *, int32_t))rt_mfc(3873))(control, buffer, capacity); }

static int32_t rt_error_dialog(void *object, void *control, void *descriptor)
{ return ((int32_t (TC *)(void *, void *, void *))rt_slot(0xd4dc))(object, control, descriptor); }

static void *rt_base_font(int32_t id)
{ return ((void *(SC *)(int32_t))rt_slot(0xd524))(id); }

static void rt_init_edit(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error_color, void *label, uint32_t flags)
{ ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))rt_slot(0xd4e0))(control, font, foreground, background, error_color, label, flags); }

static int32_t rt_init_slider(void *control, uint32_t first_bitmap, uint32_t second_bitmap, void *dib, void *palette, uint32_t first, uint32_t second, uint32_t third, uint32_t fourth, int32_t x, int32_t y, uint32_t module)
{ return ((int32_t (TC *)(void *, uint32_t, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t, uint32_t))rt_slot(0xd4cc))(control, first_bitmap, second_bitmap, dib, palette, first, second, third, fourth, x, y, module); }

static int rt_is_enabled(void *control)
{ return ((int (TC *)(void *))rt_mfc(4123))(control); }

static int rt_window_control_id(uint32_t window)
{ return GetDlgCtrlID((HWND)window); }

static uint32_t rt_host_string(uint32_t id, char **text_out)
{ return ((uint32_t (SC *)(uint32_t, char **))PTR(g_sim_imports[0].link, 0x2a4))(id, text_out); }

static uint32_t rt_host_length(const char *text)
{ return ((uint32_t (SC *)(const char *))PTR(g_sim_imports[0].link, 0x2b8))(text); }

static int rt_host_alloc(int flags, uint32_t size, void **data_out, int zero)
{ return ((int (SC *)(int, uint32_t, void **, int))PTR(g_sim_imports[0].link, 0x20c))(flags, size, data_out, zero); }

static int rt_host_realloc(int flags, uint32_t size, void **data_out)
{ return ((int (SC *)(int, uint32_t, void **))PTR(g_sim_imports[0].link, 0x210))(flags, size, data_out); }

static int rt_host_free(void **data_out)
{ return ((int (SC *)(void **))PTR(g_sim_imports[0].link, 0x214))(data_out); }

static char *rt_string_copy(char *destination, const char *source)
{ return lstrcpyA(destination, source); }

static char *rt_string_cat(char *destination, const char *source)
{ return lstrcatA(destination, source); }

static int rt_string_length(const char *text)
{ return lstrlenA(text); }

static int rt_parse_long(const char *text)
{ return atol(text); }

static void rt_init_tabs(void *tabs, void *parent, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t flags, uint32_t color, int inset, int height, int width, int selection, uint32_t module, int last)
{ ((void (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, int, int, int, int, uint32_t, int))rt_slot(0xd528))(tabs, parent, font, bitmap, dib, palette, flags, color, inset, height, width, selection, module, last); }

static void rt_init_page(void *page, uint32_t flags)
{ ((void (TC *)(void *, uint32_t))rt_slot(0xd534))(page, flags); }

static void rt_add_page(void *tabs, void *page, const char *label, uint32_t available)
{ ((void (TC *)(void *, void *, const char *, uint32_t))rt_slot(0xd5bc))(tabs, page, label, available); }

static void rt_init_tab_helper(void *helper, void *tabs)
{ ((void (TC *)(void *, void *))rt_slot(0xd4b4))(helper, tabs); }

static int rt_modify_style(void *control, uint32_t remove, uint32_t add, uint32_t flags)
{ return ((int (TC *)(void *, uint32_t, uint32_t, uint32_t))rt_mfc(4287))(control, remove, add, flags); }

static int rt_window_rect(uint32_t window, struct fs98_sim_rect *rect)
{ return GetWindowRect((HWND)window, (RECT *)rect); }

static int rt_screen_to_client(uint32_t window, int *point)
{ return ScreenToClient((HWND)window, (POINT *)point); }

static void rt_last_page(void *tabs, void *page)
{ ((void (TC *)(void *, void *))rt_slot(0xd4bc))(tabs, page); }

static void rt_active_tab(void *tabs, uint32_t index, int focus)
{ ((void (TC *)(void *, uint32_t, int))rt_slot(0xd4a4))(tabs, index, focus); }

static int rt_message_box(void *object, const char *text, const char *caption, uint32_t flags)
{ return ((int (TC *)(void *, const char *, const char *, uint32_t))rt_mfc(1200))(object, text, caption, flags); }

static const struct fs98_sim_services s_services = {
    .base_construct = rt_base_construct,
    .destroy = rt_destroy,
    .construct = rt_construct,
    .allocate = rt_allocate,
    .ui_delete = rt_ui_delete,
    .mfc_delete = rt_mfc_delete,
    .dialog_delete = rt_dialog_delete,
    .create = rt_create,
    .show = rt_show,
    .ddx = rt_ddx,
    .help = rt_help,
    .enable_window = rt_enable_window,
    .enable_control = rt_enable_control,
    .window_long = rt_window_long,
    .check = rt_check,
    .slider_get = rt_slider_get,
    .slider_range = rt_slider_range,
    .slider_set = rt_slider_set,
    .send = rt_send,
    .text = rt_text,
    .close = rt_close,
    .radio_check = rt_radio_check,
    .active_page = rt_active_page,
    .focus = rt_focus,
    .from_handle = rt_from_handle,
    .is_edit = rt_is_edit,
    .control_id = rt_control_id,
    .base_init = rt_base_init,
    .init_button = rt_init_button,
    .init_art = rt_init_art,
    .init_text = rt_init_text,
    .notify_construct = rt_notify_construct,
    .notify_delete = rt_notify_delete,
    .notify_send = rt_notify_send,
    .get_control = rt_get_control,
    .remap_rect = rt_remap_rect,
    .move_control = rt_move_control,
    .paint_construct = rt_paint_construct,
    .paint_destroy = rt_paint_destroy,
    .load_string = rt_load_string,
    .invalidate = rt_invalidate,
    .set_rect = rt_set_rect,
    .string_construct_text = rt_string_construct_text,
    .draw_text = rt_draw_text,
    .init_group = rt_init_group,
    .init_combo = rt_init_combo,
    .control_text = rt_control_text,
    .get_text = rt_get_text,
    .error_dialog = rt_error_dialog,
    .base_font = rt_base_font,
    .init_edit = rt_init_edit,
    .init_slider = rt_init_slider,
    .is_enabled = rt_is_enabled,
    .window_control_id = rt_window_control_id,
    .host_string = rt_host_string,
    .host_length = rt_host_length,
    .host_alloc = rt_host_alloc,
    .host_realloc = rt_host_realloc,
    .host_free = rt_host_free,
    .string_copy = rt_string_copy,
    .string_cat = rt_string_cat,
    .string_length = rt_string_length,
    .parse_long = rt_parse_long,
    .init_tabs = rt_init_tabs,
    .init_page = rt_init_page,
    .add_page = rt_add_page,
    .init_tab_helper = rt_init_tab_helper,
    .modify_style = rt_modify_style,
    .window_rect = rt_window_rect,
    .screen_to_client = rt_screen_to_client,
    .last_page = rt_last_page,
    .active_tab = rt_active_tab,
    .message_box = rt_message_box,
};

static void *sim_dialog_base_map(void) { return rt_slot(0xd570); }
static void *sim_page_base_map(void) { return rt_slot(0xd50c); }
static int sim_bind(void)
{
    uint32_t index;
    void *tables[8];
    const struct sim_message_entry *messages[] = {s_messages0, s_messages1, s_messages2,
        s_messages3, s_messages4, s_messages5, s_messages6, s_messages7};
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++) {
        const struct sim_import_binding *binding = &s_import_bindings[index];
        HMODULE module = GetModuleHandleA(binding->dll);
        void *function = module ? (void *)GetProcAddress(module, binding->ordinal ?
            (const char *)(uint32_t)binding->ordinal : binding->name) : 0;
        if (!function) return 0;
        s_import_functions[index] = function;
    }
    for (index = 0; index < sizeof(s_virtual_bindings) / sizeof(s_virtual_bindings[0]); index++) {
        const struct sim_virtual_binding *binding = &s_virtual_bindings[index];
        s_vtables[binding->table][binding->slot] = binding->import_index < 0 ?
            binding->local : s_import_functions[binding->import_index];
    }
    for (index = 0; index < 8; index++) {
        tables[index] = s_vtables[index];
        s_message_maps[index].base = index == 0 || index == 7 ? sim_dialog_base_map : sim_page_base_map;
        s_message_maps[index].entries = messages[index];
    }
    fs98_sim_set_services(&s_services, tables, s_vtables[9], rt_slot(0xd514));
    fs98_sim_set_crash_tables(s_vtables[8]);
    return 1;
}
struct sim_dynamic_unwind { struct fs98_sim_unwind frame; void *dynamic; volatile uint32_t live; };
static void sim_dynamic_cleanup(struct fs98_sim_unwind *frame)
{
    struct sim_dynamic_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; rt_mfc_delete(state->dynamic); }
}
static FS98_SIM_NOINLINE void sim_module_init(void)
{
    struct sim_dynamic_unwind state;
    void *dynamic;
    state.dynamic = 0; state.live = 0;
    fs98_sim_unwind_enter(&state.frame, sim_dynamic_cleanup);
    ((void (SC *)(void *, uint32_t *))PTR(g_sim_imports[0].link, 0x298))(&g_sim_linkage, &s_module);
    fs98_sim_set_module(s_module);
    dynamic = ((void *(*)(uint32_t))rt_mfc(823))(0x40);
    state.dynamic = dynamic; state.live = 1;
    if (dynamic) ((void *(TC *)(void *, void *, int32_t))rt_mfc(342))(dynamic, s_extension, 0);
    state.live = 0;
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
    fs98_sim_set_resource_module(s_resource_module);
    fs98_sim_unwind_leave(&state.frame);
}
static void sim_module_deinit(void)
{
    if (s_module) ((void (SC *)(uint32_t))PTR(g_sim_imports[0].link, 0x29c))(s_module);
    FreeLibrary((HMODULE)s_resource_module);
    s_module = s_resource_module = 0;
    fs98_sim_set_module(0);
    fs98_sim_set_resource_module(0);
}
__declspec(dllimport) void *fs98_sim_simsui_anchor(uint32_t size);
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        void *volatile anchor = (void *)fs98_sim_simsui_anchor;
        (void)anchor;
        if (!sim_bind()) return FALSE;
        ((int32_t (SC *)(void *, HINSTANCE))rt_mfc(1182))(s_extension, instance);
        s_module = (uint32_t)instance;
        fs98_sim_set_module(s_module);
    } else if (reason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int32_t))rt_mfc(1253))(s_extension, 0);
    }
    return TRUE;
}
