/* Native Help adapter. Dialog behavior is rebuilt; shared MFC/SIMSUI
 * controls remain the CD dependencies during the compatibility port. */
#include <windows.h>
#include <commctrl.h>
#include "help_objects.h"
#include "help_unwind.h"
#define TC FS98_HELP_THISCALL
#define SC FS98_HELP_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
struct help_import_binding { const char *dll, *name; uint16_t ordinal, slot; };
struct help_virtual_binding { int table, slot, import_index; void *local; };
struct help_message_entry { uint32_t message, code, first, last, signature; void *handler; };
struct help_message_map { void *(*base)(void); const struct help_message_entry *entries; };
struct help_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
static struct help_message_map s_message_maps[5];
#define MAP_GETTER(name, index) \
static void *TC help_map_##name(void *object) { (void)object; return &s_message_maps[index]; }
MAP_GETTER(about, 0)
MAP_GETTER(credits, 1)
MAP_GETTER(lessons, 2)
MAP_GETTER(graph, 3)
MAP_GETTER(completion, 4)
#undef MAP_GETTER
#include "help_runtime_tables.inc"
static void *s_import_functions[sizeof(s_import_bindings) / sizeof(s_import_bindings[0])];
static void *s_vtables[6][60];
static uint8_t s_extension[0x14];
static uint32_t s_module, s_resource_module;
struct help_import { uint32_t id; void *link; };
struct help_linkage {
    uint32_t id; void (*init)(void), (*deinit)(void); void *reserved;
    uint32_t flags; const struct help_dialog *dialogs;
};
static void help_module_init(void);
static void help_module_deinit(void);
struct help_import g_help_imports[] = {{1, 0}, {2, 0}, {0, 0}};
struct help_linkage g_help_linkage = {0x80000008, help_module_init, help_module_deinit, 0, 0, s_dialogs};
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


static int32_t rt_send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ return SendMessageA((HWND)window, message, w_param, l_param); }

static int32_t rt_win_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{ return WinHelpA((HWND)window, file, command, (ULONG_PTR)table); }

static int32_t rt_enable_window(uint32_t window, int32_t enable)
{ return EnableWindow((HWND)window, enable); }

static int32_t rt_kill_timer(uint32_t window, uint32_t timer)
{ return KillTimer((HWND)window, timer); }

static uint32_t rt_set_timer(uint32_t window, uint32_t timer, uint32_t period, void *callback)
{ return SetTimer((HWND)window, timer, period, (TIMERPROC)callback); }

static int32_t rt_destroy_window(uint32_t window)
{ return DestroyWindow((HWND)window); }

static int32_t rt_free_library(uint32_t module)
{ return FreeLibrary((HMODULE)module); }

static void rt_close(void *object)
{ ((void (TC *)(void *))rt_slot(0x6494))(object); }

static int32_t rt_message_box(const char *text, uint32_t type, uint32_t help)
{ return ((int32_t (SC *)(const char *, uint32_t, uint32_t))rt_mfc(1200))(text, type, help); }

static int32_t rt_get_window_long(uint32_t window, int32_t index)
{ return GetWindowLongA((HWND)window, index); }

static int32_t rt_set_checked(void *control, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))rt_slot(0x64e0))(control, checked); }

static void rt_set_text(void *control, const char *text)
{ ((void (TC *)(void *, const char *))rt_mfc(6199))(control, text); }

static int32_t rt_enable_control(void *control, int32_t enabled)
{ return ((int32_t (TC *)(void *, int32_t))rt_mfc(2642))(control, enabled); }

static int32_t rt_load_host_string(uint32_t id, char *buffer, int32_t capacity)
{ return ((int32_t (SC *)(uint32_t, char *, int32_t))PTR(g_help_imports[0].link, 0x2a0))(id, buffer, capacity); }

static int32_t rt_split_path(const char *path, char *drive, char *dir, char *name, int32_t *ext)
{ return ((int32_t (SC *)(const char *, char *, char *, char *, int32_t *))PTR(g_help_imports[0].link, 0x188))(path, drive, dir, name, ext); }

static int32_t rt_join_path(char *out, const char *drive, const char *dir, const char *name)
{ return ((int32_t (SC *)(char *, const char *, const char *, const char *))PTR(g_help_imports[0].link, 0x184))(out, drive, dir, name); }

static int32_t rt_exists(const char *path, int32_t extension)
{ return ((int32_t (SC *)(const char *, int32_t))PTR(g_help_imports[0].link, 0x158))(path, extension); }

static void rt_base_exchange(void *object, void *exchange)
{ ((void (TC *)(void *, void *))rt_slot(0x64ec))(object, exchange); }

static void rt_ddx_control(void *exchange, uint32_t id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))rt_mfc(2302))(exchange, id, control); }

static int32_t rt_read_file(uint32_t file, void *buffer, uint32_t count, uint32_t *read, void *overlapped)
{ return ReadFile((HANDLE)file, buffer, count, (DWORD *)read, overlapped); }

static char *rt_copy_text(char *out, const char *text)
{ return lstrcpyA(out, text); }

static int32_t rt_open_file(const char *name, void *info, uint32_t mode)
{ return OpenFile(name, info, mode); }

static int32_t rt_close_handle(uint32_t handle)
{ return CloseHandle((HANDLE)handle); }

static int32_t rt_base_init(void *object)
{ return ((int32_t (TC *)(void *))rt_slot(0x6474))(object); }

static int32_t rt_dialog_init(void *object)
{ return ((int32_t (TC *)(void *))rt_mfc(4710))(object); }

static int32_t rt_init_art(void *control, int32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return ((int32_t (TC *)(void *, int32_t, void *, void *, uint32_t))rt_slot(0x6480))(control, bitmap, dib, palette, flags); }

static int32_t rt_init_button(void *control, void *font, void *alternate_font, uint32_t bitmap, void *dib, void *palette, uint32_t flags, uint32_t color, uint32_t module, int32_t x, int32_t y, struct fs98_help_rect rect)
{ return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, uint32_t, uint32_t, int32_t, int32_t, struct fs98_help_rect))rt_slot(0x6484))(control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect); }

static void rt_init_text(void *control, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0x6488))(control, font); }

static void *rt_get_control(void *object, uint32_t id)
{ return ((void * (TC *)(void *, uint32_t))rt_mfc(3092))(object, id); }

static void rt_remap_rect(struct fs98_help_rect *rect, void *control)
{ ((void (SC *)(struct fs98_help_rect *, void *))rt_slot(0x646c))(rect, control); }

static void rt_init_group(void *control, void *font)
{ ((void (TC *)(void *, void *))rt_slot(0x648c))(control, font); }

static void *rt_base_font(int32_t id)
{ return ((void * (SC *)(int32_t))rt_slot(0x6478))(id); }

static void rt_init_list(void *control, void *font, void *label)
{ ((void (TC *)(void *, void *, void *))rt_slot(0x6454))(control, font, label); }

static void rt_init_edit(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error_color, void *label, uint32_t flags)
{ ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))rt_slot(0x6450))(control, font, foreground, background, error_color, label, flags); }

static int32_t rt_copy_rect(struct fs98_help_rect *out, const struct fs98_help_rect *in)
{ return CopyRect((RECT *)out, (const RECT *)in); }

static void rt_common_controls(void)
{ InitCommonControls(); }

static uint32_t rt_load_library(const char *name)
{ return (uint32_t)LoadLibraryA(name); }

static void *rt_module_state(void)
{ return ((void *(SC *)(void))rt_mfc(1168))(); }

static uint32_t rt_create_window(uint32_t ex_style, const char *class_info, const char *title, uint32_t style, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t parent, uint32_t menu, uint32_t instance, void *param)
{ return (uint32_t)CreateWindowExA(ex_style, class_info, title, style, x, y, width, height, (HWND)parent, (HMENU)menu, (HINSTANCE)instance, param); }

static void rt_string_construct(void *string)
{ ((void (TC *)(void *))rt_mfc(540))(string); }

static void rt_string_destroy(void *string)
{ ((void (TC *)(void *))rt_mfc(800))(string); }

static int32_t rt_reg_open(uint32_t key, const char *subkey, uint32_t options, uint32_t access, uint32_t *key_out)
{ return RegOpenKeyExA((HKEY)key, subkey, options, access, (HKEY *)key_out); }

static int32_t rt_reg_query(uint32_t key, const char *name, void *reserved, uint32_t *type, void *data, uint32_t *size)
{ return RegQueryValueExA((HKEY)key, name, reserved, (DWORD *)type, data, (DWORD *)size); }

static int32_t rt_load_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{ return LoadStringA((HMODULE)module, id, buffer, capacity); }

static void rt_set_sims_text(void *control, const char *text)
{ ((void (TC *)(void *, const char *))rt_slot(0x6490))(control, text); }

static int32_t rt_format_version(char *out, const char *format, int32_t major, int32_t minor, const char *prefix, int32_t build)
{ return wsprintfA(out, format, major, minor, prefix, build); }

static char *rt_append_text(char *out, const char *text)
{ return lstrcatA(out, text); }

static void rt_font_construct(void *font, void *base_font)
{ ((void (TC *)(void *, void *))rt_slot(0x647c))(font, base_font); }

static void rt_font_destroy(void *font)
{ ((void (TC *)(void *))rt_slot(0x6470))(font); }

static void rt_move_control(void *control, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t))rt_mfc(4299))(control, x, y, width, height, repaint); }

/* Help registers only its cached About font. Match CRT unload lifetime,
 * including absence of a destructor when font construction throws. */
static void (*s_exit_handler)(void);
static void rt_register_exit(void (*handler)(void)) { s_exit_handler = handler; }

static const struct fs98_help_services s_services = {
    .send_message = rt_send_message,
    .win_help = rt_win_help,
    .enable_window = rt_enable_window,
    .kill_timer = rt_kill_timer,
    .set_timer = rt_set_timer,
    .destroy_window = rt_destroy_window,
    .free_library = rt_free_library,
    .close = rt_close,
    .message_box = rt_message_box,
    .get_window_long = rt_get_window_long,
    .set_checked = rt_set_checked,
    .set_text = rt_set_text,
    .enable_control = rt_enable_control,
    .load_host_string = rt_load_host_string,
    .split_path = rt_split_path,
    .join_path = rt_join_path,
    .exists = rt_exists,
    .base_exchange = rt_base_exchange,
    .ddx_control = rt_ddx_control,
    .read_file = rt_read_file,
    .copy_text = rt_copy_text,
    .open_file = rt_open_file,
    .close_handle = rt_close_handle,
    .base_init = rt_base_init,
    .dialog_init = rt_dialog_init,
    .init_art = rt_init_art,
    .init_button = rt_init_button,
    .init_text = rt_init_text,
    .get_control = rt_get_control,
    .remap_rect = rt_remap_rect,
    .init_group = rt_init_group,
    .base_font = rt_base_font,
    .init_list = rt_init_list,
    .init_edit = rt_init_edit,
    .copy_rect = rt_copy_rect,
    .common_controls = rt_common_controls,
    .load_library = rt_load_library,
    .module_state = rt_module_state,
    .create_window = rt_create_window,
    .string_construct = rt_string_construct,
    .string_destroy = rt_string_destroy,
    .reg_open = rt_reg_open,
    .reg_query = rt_reg_query,
    .load_string = rt_load_string,
    .set_sims_text = rt_set_sims_text,
    .format_version = rt_format_version,
    .append_text = rt_append_text,
    .font_construct = rt_font_construct,
    .font_destroy = rt_font_destroy,
    .register_exit = rt_register_exit,
    .move_control = rt_move_control,
};

static void *rt_dialog_construct(void *self, int32_t resource, void *parent)
{ return ((void *(TC *)(void *, int32_t, void *))rt_slot(0x64a0))(self, resource, parent); }
static void rt_dialog_destroy(void *self) { ((void (TC *)(void *))rt_slot(0x64b4))(self); }
static void rt_construct(void *self, uint32_t slot) { ((void *(TC *)(void *))rt_slot(slot))(self); }
static void rt_destroy(void *self, uint32_t slot) { ((void (TC *)(void *))rt_slot(slot))(self); }
static void *rt_allocate(uint32_t size) { return ((void *(*)(uint32_t))rt_slot(0x64e8))(size); }
static void rt_ui_delete(void *self) { ((void (*)(void *))rt_slot(0x649c))(self); }
static void rt_mfc_delete(void *self) { ((void (*)(void *))rt_mfc(825))(self); }
static void rt_dialog_delete(void *self) { ((void (SC *)(void *))rt_slot(0x64f0))(self); }
static int32_t rt_create(void *self, uint32_t resource, void *parent)
{ return ((int32_t (TC *)(void *, uint32_t, void *))rt_mfc(2086))(self, resource, parent); }
static int32_t rt_show(void *self, int32_t show);
static const struct fs98_help_object_services s_object_services = {
    rt_dialog_construct, rt_dialog_destroy, rt_construct, rt_destroy, rt_allocate,
    rt_ui_delete, rt_mfc_delete, rt_dialog_delete, rt_create, rt_show
};
static int32_t rt_show(void *self, int32_t show)
{ return ((int32_t (TC *)(void *, int32_t))rt_mfc(6215))(self, show); }

static void *help_dialog_base_map(void) { return rt_slot(0x64e4); }
static int help_bind(void)
{
    uint32_t index;
    void *tables[5];
    const struct help_message_entry *messages[] = {s_about_messages, s_credits_messages,
        s_lessons_messages, s_graph_messages, s_completion_messages};
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++) {
        const struct help_import_binding *binding = &s_import_bindings[index];
        HMODULE module = GetModuleHandleA(binding->dll);
        void *function = module ? (void *)GetProcAddress(module, binding->ordinal ?
            (const char *)(uint32_t)binding->ordinal : binding->name) : 0;
        if (!function) return 0;
        s_import_functions[index] = function;
    }
    for (index = 0; index < sizeof(s_virtual_bindings) / sizeof(s_virtual_bindings[0]); index++) {
        const struct help_virtual_binding *binding = &s_virtual_bindings[index];
        s_vtables[binding->table][binding->slot] = binding->import_index < 0 ?
            binding->local : s_import_functions[binding->import_index];
    }
    for (index = 0; index < 5; index++) {
        tables[index] = s_vtables[index];
        s_message_maps[index].base = help_dialog_base_map;
        s_message_maps[index].entries = messages[index];
    }
    fs98_help_set_services(&s_services);
    fs98_help_set_object_services(&s_object_services, tables, s_vtables[5]);
    return 1;
}
struct help_dynamic_unwind { struct fs98_help_unwind frame; void *dynamic; volatile uint32_t live; };
static void help_dynamic_cleanup(struct fs98_help_unwind *frame)
{
    struct help_dynamic_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; rt_mfc_delete(state->dynamic); }
}
static FS98_HELP_NOINLINE void help_module_init(void)
{
    struct help_dynamic_unwind state;
    void *dynamic;
    state.dynamic = 0; state.live = 0;
    fs98_help_unwind_enter(&state.frame, help_dynamic_cleanup);
    ((void (SC *)(void *, uint32_t *))PTR(g_help_imports[0].link, 0x298))(&g_help_linkage, &s_module);
    fs98_help_set_object_module(s_module);
    dynamic = ((void *(*)(uint32_t))rt_mfc(823))(0x40);
    state.dynamic = dynamic; state.live = 1;
    if (dynamic) ((void *(TC *)(void *, void *, int32_t))rt_mfc(342))(dynamic, s_extension, 0);
    state.live = 0;
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
    fs98_help_set_resource_module(s_resource_module);
    fs98_help_unwind_leave(&state.frame);
}
static void help_module_deinit(void)
{
    if (s_module) ((void (SC *)(uint32_t))PTR(g_help_imports[0].link, 0x29c))(s_module);
    FreeLibrary((HMODULE)s_resource_module);
    s_module = s_resource_module = 0;
    fs98_help_set_object_module(0);
    fs98_help_set_resource_module(0);
}
__declspec(dllimport) void *fs98_help_simsui_anchor(uint32_t size);
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        void *volatile anchor = (void *)fs98_help_simsui_anchor;
        (void)anchor;
        if (!help_bind()) return FALSE;
        {
            void *state = rt_module_state();
            D32(state, 0x1c) = D32(state, 0x20); D32(state, 0x28) = D32(state, 0x2c);
            D32(state, 0x20) = D32(state, 0x2c) = 0;
        }
        ((int32_t (SC *)(void *, HINSTANCE))rt_mfc(1182))(s_extension, instance);
        s_module = (uint32_t)instance;
        fs98_help_set_object_module(s_module);
    } else if (reason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int32_t))rt_mfc(1253))(s_extension, 0);
        if (s_exit_handler) { void (*handler)(void) = s_exit_handler; s_exit_handler = 0; handler(); }
    }
    return TRUE;
}
