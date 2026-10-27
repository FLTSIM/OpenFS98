/* Partial native World adapter: scenery list/editors, priority, boosters and drive search. */
#include <windows.h>
#include "world_objects.h"
#include "world_init.h"
#include "world_priority.h"
#include "world_booster.h"
#include "world_search.h"
#include "world_list.h"
#include "world_cache.h"
#include "world_files.h"
#include "world_position.h"
#include "world_facilities.h"
#include "world_airport.h"
#include "world_complexity.h"
#include "world_dynamic.h"
#include "world_copy.h"
#include "world_unwind.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
struct world_import_binding { const char *dll, *name; uint32_t ordinal, slot; };
struct world_virtual_binding { uint32_t table, slot; int import_index; void *local; };
struct world_message_entry { uint32_t message, code, first, last, signature; void *handler; };
struct world_message_map { void *(*base)(void); const struct world_message_entry *entries; };
struct world_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
static struct world_message_map s_message_map, s_priority_message_map, s_booster_message_map, s_search_message_map, s_list_message_map, s_cache_message_map, s_files_message_map, s_position_message_map, s_facilities_message_map, s_airport_message_map, s_complexity_message_map, s_dynamic_message_map, s_copy_message_map;
static void *world_slot(uint32_t slot);
#include "world_runtime_tables.inc"
static void *s_functions[sizeof(s_imports) / sizeof(s_imports[0])], *s_vtables[16][62];
static void (*s_font_exit)(void), (*s_font_exits[2])(void);
static uint32_t s_font_exit_count;
static uint8_t s_extension[0x14];
static uint32_t s_module, s_resource_module;
struct world_import { uint32_t id; void *link; };
struct world_linkage { uint32_t id; void (*init)(void), (*deinit)(void); void *reserved; uint32_t flags; const struct world_dialog *dialogs; };
static void world_module_init(void);
static void world_module_deinit(void);
struct world_import g_world_imports[] = {{1, 0}, {2, 0}, {0, 0}};
struct world_linkage g_world_linkage = {0x80000006, world_module_init, world_module_deinit, 0, 0, s_dialogs};
static void *world_slot(uint32_t slot)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_imports) / sizeof(s_imports[0]); ++index)
        if (s_imports[index].slot == slot) return s_functions[index];
    return NULL;
}
static void *world_mfc(uint32_t ordinal)
{ return (void *)GetProcAddress(GetModuleHandleA("MFC42.DLL"), (const char *)ordinal); }
static void *world_crt(const char *name)
{ return (void *)GetProcAddress(GetModuleHandleA("MSVCRT.DLL"), name); }
static void * world_base_construct(void *object, uint32_t template_id, void *parent)
{ return ((void * (TC *)(void *, uint32_t, void *))world_slot(0x25804))(object, template_id, parent); }
static void world_construct(void *object, uint32_t import_slot)
{ ((void (TC *)(void *))world_slot(import_slot))(object); }
static void world_destroy(void *object, uint32_t import_slot)
{ ((void (TC *)(void *))world_slot(import_slot))(object); }
static void * world_allocate(uint32_t bytes)
{ return ((void * ( *)(uint32_t))world_slot(0x2584c))(bytes); }
static void world_ui_delete(void *object)
{ ((void ( *)(void *))world_slot(0x25800))(object); }
static void world_dialog_delete(void *object)
{ ((void (SC *)(void *))world_slot(0x25850))(object); }
static int world_create(void *object, uint32_t template_id, void *parent)
{ return ((int (TC *)(void *, uint32_t, void *))world_slot(0x25524))(object, template_id, parent); }
static int world_show(void *object, int show)
{ return ((int (TC *)(void *, int))world_slot(0x25500))(object, show); }
static int world_message(const char *text, uint32_t flags, uint32_t help)
{ return ((int (SC *)(const char *, uint32_t, uint32_t))world_slot(0x2551c))(text, flags, help); }
static void world_ddx(void *exchange, uint32_t id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))world_slot(0x255fc))(exchange, id, control); }
static void world_exit(void *exception)
{ ((void ( *)(void *))world_slot(0x25848))(exception); }
static int world_get_text(void *control, char *text, int capacity)
{ return ((int (TC *)(void *, char *, int))world_slot(0x25598))(control, text, capacity); }
static void world_set_text(void *control, const char *text)
{ ((void (TC *)(void *, const char *))world_slot(0x255bc))(control, text); }
static int world_check(void *control, int checked)
{ return ((int (TC *)(void *, int))world_slot(0x2577c))(control, checked); }
static int world_enable(void *control, int enable)
{ return ((int (TC *)(void *, int))world_slot(0x255c8))(control, enable); }
static int world_error(void *object, void *control, void *status)
{ return ((int (TC *)(void *, void *, void *))world_slot(0x25778))(object, control, status); }
static void world_focus(void *control)
{ ((void (TC *)(void *))world_slot(0x25590))(control); }
static int world_base_init(void *object)
{ return ((int (TC *)(void *))world_slot(0x257c0))(object); }
static void world_string_construct(void *string)
{ ((void (TC *)(void *))world_slot(0x255b4))(string); }
static char * world_string_buffer(void *string, int minimum)
{ return ((char * (TC *)(void *, int))world_slot(0x25588))(string, minimum); }
static void world_string_release(void *string, int length)
{ ((void (TC *)(void *, int))world_slot(0x2558c))(string, length); }
static void world_string_destroy(void *string)
{ ((void (TC *)(void *))world_slot(0x255c0))(string); }
static int world_art(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return ((int (TC *)(void *, uint32_t, void *, void *, uint32_t))world_slot(0x257c4))(control, bitmap, dib, palette, flags); }
static int world_button(void *control, void *font, void *alternate, uint32_t bitmap, void *dib, void *palette, uint32_t flags, int color, uint32_t module, int x, int y, struct fs98_world_rect rect)
{ return ((int (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t, int, uint32_t, int, int, struct fs98_world_rect))world_slot(0x257c8))(control, font, alternate, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void world_text(void *control, void *font)
{ ((void (TC *)(void *, void *))world_slot(0x257dc))(control, font); }
static void world_combo(void *control, void *font, void *label)
{ ((void (TC *)(void *, void *, void *))world_slot(0x25790))(control, font, label); }
static void * world_font(int font)
{ return ((void * (SC *)(int))world_slot(0x257d0))(font); }
static void world_edit(void *control, void *font, uint32_t foreground, uint32_t background, uint32_t error, void *label, uint32_t flags)
{ ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))world_slot(0x25740))(control, font, foreground, background, error, label, flags); }
static void world_title(void *control, const char *text)
{ ((void (TC *)(void *, const char *))world_slot(0x257e0))(control, text); }
static int world_help(uint32_t window, const char *filename, uint32_t command, const uint32_t *map)
{ return WinHelpA((HWND)window, filename, command, (ULONG_PTR)map); }
static int world_send(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ return SendMessageA((HWND)window, message, first, second); }
static int world_parse(const char *text) { return ((int (*)(const char *))world_crt("atoi"))(text); }
static int world_format(char *text, const char *format, int value)
{ return ((int (*)(char *, const char *, ...))world_crt("sprintf"))(text, format, value); }
static int world_load_string(uint32_t module, uint32_t id, char *buffer, int capacity)
{ return LoadStringA((HMODULE)module, id, buffer, capacity); }
static void world_string_assign(void *string, const char *text)
{ ((void *(TC *)(void *, const char *))world_slot(0x25578))(string, text); }
static void world_string_append(void *string, const char *text)
{ ((void *(TC *)(void *, const char *))world_slot(0x25580))(string, text); }
static int world_font_tabs(void *font, int count, const int *tabs)
{ return ((int (TC *)(void *, int, const int *))world_slot(0x25714))(font, count, tabs); }
static void world_list_init(void *control, void *font, void *label)
{ ((void (TC *)(void *, void *, void *))world_slot(0x25718))(control, font, label); }
static void world_font_construct(void *font, void *base_font)
{ ((void *(TC *)(void *, void *))world_slot(0x2578c))(font, base_font); }
static void world_font_destroy(void *font)
{ ((void (TC *)(void *))world_slot(0x2579c))(font); }
/* Own both lazily constructed fonts. Preserve reverse registration order
 * without putting unloadable callbacks in the process-wide CRT exit table. */
static int world_font_at_exit(void (*callback)(void))
{
    if (s_font_exit_count == 2) return -1;
    s_font_exits[s_font_exit_count++] = callback;
    return 0;
}
static void world_group(void *control, void *font)
{ ((void (TC *)(void *, void *))world_slot(0x25730))(control, font); }
static const struct fs98_world_priority_services s_priority_services = {
    world_string_assign, world_string_append, world_font_tabs, world_list_init
};
static const struct fs98_world_services s_services = {
    world_get_text, world_set_text, world_parse, world_format, world_send,
    world_check, world_enable, world_error, world_focus
};
static const struct fs98_world_object_services s_object_services = {
    world_base_construct, world_construct, world_destroy, world_allocate,
    world_ui_delete, world_dialog_delete, world_create, world_show, world_message,
    world_ddx, world_exit, world_help
};
static const struct fs98_world_init_services s_init_services = {
    world_base_init, world_string_construct, world_string_buffer, world_string_release,
    world_string_destroy, world_load_string, world_art, world_button, world_text, world_combo,
    world_font, world_edit, world_title
};
static const struct fs98_world_list_services s_list_services = {
    &s_services, &s_init_services, world_string_assign, world_string_append, world_font_tabs, world_list_init
};
static const struct fs98_world_cache_services s_cache_services = {
    &s_services, &s_init_services, world_string_assign, world_string_append, world_font_tabs, world_list_init, world_group
};
static int world_search_get_path(void *control, char *text, int capacity)
{ return ((int (TC *)(void *, char *, int))world_slot(0x25700))(control, text, capacity); }
static int world_post(uint32_t window, uint32_t message, uint32_t first, uint32_t second)
{ return PostMessageA((HWND)window, message, first, second); }
static int world_erase(void *object, void *dc)
{ return ((int (TC *)(void *, void *))world_slot(0x25704))(object, dc); }
static void *world_spin_vtable(void) { return world_slot(0x256ec); }
static void world_base_exchange(void *object, void *exchange)
{ ((void (TC *)(void *, void *))world_slot(0x257fc))(object, exchange); }
static void world_remap(struct fs98_world_rect *rect, void *control)
{ ((void (SC *)(struct fs98_world_rect *, void *))world_slot(0x25744))(rect, control); }
static void world_move(void *control, int x, int y, int width, int height, int repaint)
{ ((void (TC *)(void *, int, int, int, int, int))world_slot(0x2559c))(control, x, y, width, height, repaint); }
static const struct fs98_world_position_services s_position_services = {
    &s_services, &s_init_services, world_group, world_remap, world_move, world_message
};
static const struct fs98_world_files_services s_files_services = {
    &s_services, &s_init_services, world_list_init, world_group, world_post, world_erase
};
static const struct fs98_world_search_services s_search_services = {
    &s_services, world_title, world_post, world_search_get_path
};
static const struct fs98_world_search_init_services s_search_init_services = {
    &s_init_services, &s_services, world_group
};
static const struct fs98_world_booster_services s_booster_services = {&s_services, world_title};
static const struct fs98_world_booster_init_services s_booster_init_services = {
    &s_init_services, &s_services, world_font_construct, world_font_destroy, world_font_at_exit, world_group
};
static void *world_base_map(void) { return world_slot(0x257f8); }
static void *world_facilities_host(void) { return g_world_imports[0].link; }
static uint32_t world_module(void) { return s_module; }
static uint32_t world_resources(void) { return s_resource_module; }
static struct fs98_world_facilities_services s_facilities_services = {
    world_slot, world_facilities_host, world_module, world_resources, NULL, world_font_at_exit
};
static const struct fs98_world_airport_services s_airport_services = {world_slot, world_facilities_host, world_resources};
static const struct fs98_world_complexity_services s_complexity_services = {world_send, world_check};
static int world_dynamic_error(void *object, void *control, uint32_t message)
{ return ((int (TC *)(void *, void *, uint32_t))world_slot(0x25778))(object, control, message); }
static const struct fs98_world_dynamic_services s_dynamic_services = {world_send, world_check, world_dynamic_error, world_focus, world_post};
static struct fs98_world_dynamic_init_services s_dynamic_init_services = {&s_init_services, world_slot, world_resources, NULL};
static struct fs98_world_complexity_init_services s_complexity_init_services = {&s_init_services, world_slot, world_resources, NULL};
static int world_cursor_position(int32_t *point) { return GetCursorPos((POINT *)point); }
static int world_window_rect(uint32_t window, struct fs98_world_rect *rect) { return GetWindowRect((HWND)window, (RECT *)rect); }
static int world_contains(const struct fs98_world_rect *rect, int32_t x, int32_t y) { POINT point = {x,y}; return PtInRect((const RECT *)rect,point); }
static const struct fs98_world_facilities_help_services s_facilities_help = {world_cursor_position, world_window_rect, world_contains};
static const struct fs98_world_copy_services s_copy_services = {world_send, world_check, world_dynamic_error, world_enable, world_post};
static const struct fs98_world_copy_init_services s_copy_init_services = {&s_init_services, world_resources};
static int world_bind(void)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_imports) / sizeof(s_imports[0]); ++index) {
        const struct world_import_binding *binding = &s_imports[index];
        HMODULE module = GetModuleHandleA(binding->dll);
        void *function = module ? (void *)GetProcAddress(module,
            binding->ordinal ? (const char *)binding->ordinal : binding->name) : NULL;
        if (!function) return 0;
        s_functions[index] = function;
    }
    for (index = 0; index < sizeof(s_virtuals) / sizeof(s_virtuals[0]); ++index) {
        const struct world_virtual_binding *binding = &s_virtuals[index];
        s_vtables[binding->table][binding->slot] = binding->import_index < 0 ? binding->local : s_functions[binding->import_index];
    }
    s_message_map.base = world_base_map; s_message_map.entries = s_messages;
    fs98_world_set_services(&s_services);
    fs98_world_set_object_services(&s_object_services, s_vtables[0]);
    fs98_world_set_init_services(&s_init_services);
    s_priority_message_map.base = world_base_map; s_priority_message_map.entries = s_priority_messages;
    fs98_world_priority_set_object_services(&s_object_services, s_vtables[1]);
    fs98_world_priority_set_services(&s_priority_services);
    s_booster_message_map.base = world_base_map; s_booster_message_map.entries = s_booster_messages;
    fs98_world_booster_set_object_services(&s_object_services, s_vtables[2]);
    fs98_world_booster_set_services(&s_booster_services);
    fs98_world_booster_set_init_services(&s_booster_init_services);
    s_search_message_map.base = world_base_map; s_search_message_map.entries = s_search_messages;
    fs98_world_search_set_object_services(&s_object_services, s_vtables[3]);
    fs98_world_search_set_services(&s_search_services);
    fs98_world_search_set_init_services(&s_search_init_services);
    s_list_message_map.base = world_base_map; s_list_message_map.entries = s_list_messages;
    fs98_world_list_set_object_services(&s_object_services, s_vtables[4]);
    fs98_world_list_set_services(&s_list_services);
    s_cache_message_map.base = world_base_map; s_cache_message_map.entries = s_cache_messages;
    fs98_world_cache_set_object_services(&s_object_services, s_vtables[5]);
    fs98_world_cache_set_services(&s_cache_services);
    s_files_message_map.base = world_base_map; s_files_message_map.entries = s_files_messages;
    fs98_world_files_set_object_services(&s_object_services, s_vtables[6]);
    fs98_world_files_set_services(&s_files_services);
    s_position_message_map.base = world_base_map; s_position_message_map.entries = s_position_messages;
    fs98_world_position_set_object_services(&s_object_services, s_vtables[7], world_spin_vtable, world_base_exchange);
    fs98_world_position_set_services(&s_position_services);
    s_airport_message_map.base = world_base_map; s_airport_message_map.entries = s_airport_messages;
    fs98_world_airport_set_object_services(&s_object_services, s_vtables[11], world_base_exchange);
    fs98_world_airport_set_services(&s_airport_services);
    s_copy_message_map.base = world_base_map; s_copy_message_map.entries = s_copy_messages;
    fs98_world_copy_set_object_services(&s_object_services, s_vtables[15]);
    fs98_world_copy_set_services(&s_copy_services);
    fs98_world_copy_set_init_services(&s_copy_init_services);
    s_dynamic_message_map.base = world_base_map; s_dynamic_message_map.entries = s_dynamic_messages;
    fs98_world_dynamic_set_object_services(&s_object_services, s_vtables[14]);
    fs98_world_dynamic_set_services(&s_dynamic_services);
    s_dynamic_init_services.notify_vtable = s_vtables[13];
    fs98_world_dynamic_set_init_services(&s_dynamic_init_services);
    s_complexity_message_map.base = world_base_map; s_complexity_message_map.entries = s_complexity_messages;
    fs98_world_complexity_set_object_services(&s_object_services, s_vtables[12]);
    fs98_world_complexity_set_services(&s_complexity_services);
    s_complexity_init_services.notify_vtable = s_vtables[13];
    fs98_world_complexity_set_init_services(&s_complexity_init_services);
    s_facilities_message_map.base = world_base_map; s_facilities_message_map.entries = s_facilities_messages;
    fs98_world_facilities_set_object_services(&s_object_services, s_vtables[8], s_vtables[9], world_base_exchange);
    s_facilities_services.output_vtable = s_vtables[10];
    fs98_world_facilities_set_services(&s_facilities_services);
    fs98_world_facilities_set_help_services(&s_facilities_help);
    return 1;
}
struct world_dynamic_unwind { struct fs98_world_unwind frame; void *dynamic; volatile int live; };
static void world_dynamic_cleanup(struct fs98_world_unwind *frame)
{
    struct world_dynamic_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; ((void (*)(void *))world_mfc(825))(state->dynamic); }
}
static WORLD_NOINLINE void world_module_init(void)
{
    struct world_dynamic_unwind state;
    void *dynamic;
    state.dynamic = NULL; state.live = 0;
    world_unwind_enter(&state.frame, world_dynamic_cleanup);
    ((void (SC *)(void *, uint32_t *))WORLD_PTR(g_world_imports[0].link, 0x298))(&g_world_linkage, &s_module);
    fs98_world_set_module(s_module); fs98_world_priority_set_module(s_module);
    fs98_world_booster_set_module(s_module); fs98_world_search_set_module(s_module); fs98_world_list_set_module(s_module); fs98_world_cache_set_module(s_module); fs98_world_files_set_module(s_module); fs98_world_position_set_module(s_module); fs98_world_facilities_set_module(s_module); fs98_world_airport_set_module(s_module); fs98_world_complexity_set_module(s_module); fs98_world_dynamic_set_module(s_module); fs98_world_copy_set_module(s_module);
    dynamic = ((void *(*)(uint32_t))world_mfc(823))(0x40);
    state.dynamic = dynamic; state.live = 1;
    if (dynamic) ((void *(TC *)(void *, void *, int))world_mfc(342))(dynamic, s_extension, 0);
    state.live = 0;
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
    fs98_world_set_resource_module(s_resource_module);
    world_unwind_leave(&state.frame);
}
static void world_module_deinit(void)
{
    if (s_module) ((void (SC *)(uint32_t))WORLD_PTR(g_world_imports[0].link, 0x29c))(s_module);
    FreeLibrary((HMODULE)s_resource_module);
    s_module = s_resource_module = 0;
    fs98_world_set_module(0); fs98_world_priority_set_module(0); fs98_world_set_resource_module(0);
    fs98_world_booster_set_module(0); fs98_world_search_set_module(0); fs98_world_list_set_module(0); fs98_world_cache_set_module(0); fs98_world_files_set_module(0); fs98_world_position_set_module(0); fs98_world_facilities_set_module(0); fs98_world_airport_set_module(0); fs98_world_complexity_set_module(0); fs98_world_dynamic_set_module(0); fs98_world_copy_set_module(0);
}
__declspec(dllimport) void *fs98_world_simsui_anchor(uint32_t size);
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        void *volatile anchor = (void *)fs98_world_simsui_anchor;
        (void)anchor;
        if (!world_bind()) return FALSE;
        ((int (SC *)(void *, HINSTANCE))world_mfc(1182))(s_extension, instance);
        s_module = (uint32_t)instance; fs98_world_set_module(s_module); fs98_world_priority_set_module(s_module);
        fs98_world_booster_set_module(s_module); fs98_world_search_set_module(s_module); fs98_world_list_set_module(s_module); fs98_world_cache_set_module(s_module); fs98_world_files_set_module(s_module); fs98_world_position_set_module(s_module); fs98_world_facilities_set_module(s_module); fs98_world_airport_set_module(s_module); fs98_world_complexity_set_module(s_module); fs98_world_dynamic_set_module(s_module); fs98_world_copy_set_module(s_module);
    } else if (reason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int))world_mfc(1253))(s_extension, 0);
        while (s_font_exit_count) {
            void (*callback)(void);
            s_font_exit = s_font_exits[--s_font_exit_count];
            callback = s_font_exit;
            s_font_exit = NULL; callback();
        }
    }
    return TRUE;
}
