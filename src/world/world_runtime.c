/* Partial native World adapter: only add/edit scenery area are registered. */
#include <windows.h>
#include "world_objects.h"
#include "world_init.h"
#include "world_unwind.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
struct world_import_binding { const char *dll, *name; uint32_t ordinal, slot; };
struct world_virtual_binding { uint32_t slot; int import_index; void *local; };
struct world_message_entry { uint32_t message, code, first, last, signature; void *handler; };
struct world_message_map { void *(*base)(void); const struct world_message_entry *entries; };
struct world_dialog { uint32_t id; uint32_t (SC *create)(uint32_t, void *); uint32_t flags; };
static struct world_message_map s_message_map;
#include "world_runtime_tables.inc"
static void *s_functions[sizeof(s_imports) / sizeof(s_imports[0])], *s_vtable[60];
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
static void *world_base_map(void) { return world_slot(0x257f8); }
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
        s_vtable[binding->slot] = binding->import_index < 0 ? binding->local : s_functions[binding->import_index];
    }
    s_message_map.base = world_base_map; s_message_map.entries = s_messages;
    fs98_world_set_services(&s_services);
    fs98_world_set_object_services(&s_object_services, s_vtable);
    fs98_world_set_init_services(&s_init_services);
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
    fs98_world_set_module(s_module);
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
    fs98_world_set_module(0); fs98_world_set_resource_module(0);
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
        s_module = (uint32_t)instance; fs98_world_set_module(s_module);
    } else if (reason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int))world_mfc(1253))(s_extension, 0);
    }
    return TRUE;
}
