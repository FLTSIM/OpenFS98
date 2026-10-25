/* Win32/MFC42 adapter for the retail Alerts ABI. SIMSUI is an original shared
 * dependency, just as for the shipping English DLLs; Alerts code is rebuilt. */
#include <windows.h>
#include <string.h>
#include "alerts_objects.h"
#include "alerts_unwind.h"

#define TC FS98_ALERTS_THISCALL
#define SC FS98_ALERTS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))

struct fs98_alerts_binding { const char *dll, *name; uint16_t ordinal; };
struct fs98_alerts_virtual_binding { int table, slot, import_index; void *local; };
struct fs98_alerts_message_entry {
    uint32_t message, code, first_id, last_id, signature;
    void *handler;
};
struct fs98_alerts_message_map {
    void *(*base)(void);
    const struct fs98_alerts_message_entry *entries;
};
struct fs98_alerts_dialog {
    uint32_t id;
    uint32_t (SC *create)(uint32_t parent, void *descriptor);
    uint32_t flags;
};

static void *TC fs98_alerts_modal_map(void *object);
static void *TC fs98_alerts_modeless_map(void *object);
static void *TC fs98_alerts_advisory_map(void *object);
#include "alerts_runtime_tables.inc"

static void *s_import_functions[sizeof(s_import_bindings) / sizeof(s_import_bindings[0])];
static void *s_vtables[4][60];
static uint8_t s_extension[0x14];
static uint32_t s_module, s_resource_module;

struct fs98_alerts_import { uint32_t id; void *link; };
struct fs98_alerts_linkage {
    uint32_t id;
    void (*init)(void), (*deinit)(void);
    void *reserved;
    uint32_t flags;
    const struct fs98_alerts_dialog *dialogs;
};

static void fs98_alerts_init(void);
static void fs98_alerts_deinit(void);
struct fs98_alerts_import g_alerts_imports[] = {{1, 0}, {2, 0}, {0, 0}};
struct fs98_alerts_linkage g_alerts_linkage = {
    0x80000009, fs98_alerts_init, fs98_alerts_deinit, 0, 0, s_dialogs
};

static void *fs98_alerts_mfc(uint32_t ordinal)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++)
        if (s_import_bindings[index].ordinal == ordinal)
            return s_import_functions[index];
    return 0;
}

static void *fs98_alerts_ui(const char *name)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++)
        if (!s_import_bindings[index].ordinal && !strcmp(s_import_bindings[index].name, name))
            return s_import_functions[index];
    return 0;
}

#define M0(ord, self) ((void (TC *)(void *))fs98_alerts_mfc(ord))(self)
#define U0(name, self) ((void (TC *)(void *))fs98_alerts_ui(name))(self)

static void fs98_string_construct(void *self) { M0(540, self); }
static void fs98_string_destroy(void *self) { M0(800, self); }
static void fs98_dc_construct(void *self, void *window)
{ ((void *(TC *)(void *, void *))fs98_alerts_mfc(289))(self, window); }
static void fs98_dc_destroy(void *self) { M0(613, self); }
static uint32_t fs98_stock(int32_t id) { return (uint32_t)GetStockObject(id); }
static void *fs98_font(uint32_t handle)
{ return ((void *(SC *)(uint32_t))fs98_alerts_mfc(2860))(handle); }
static void *fs98_select(void *self, void *font)
{ return ((void *(TC *)(void *, void *))fs98_alerts_mfc(5789))(self, font); }
static int32_t fs98_rect(uint32_t window, struct fs98_alerts_rect *rect)
{ return GetClientRect((HWND)window, (RECT *)rect); }
static int32_t fs98_to_screen(uint32_t window, struct fs98_alerts_point *point)
{ return ClientToScreen((HWND)window, (POINT *)point); }
static int32_t fs98_to_client(uint32_t window, struct fs98_alerts_point *point)
{ return ScreenToClient((HWND)window, (POINT *)point); }
static void fs98_format(void *self, const char *format, int32_t value)
{ ((void (*)(void *, const char *, ...))fs98_alerts_mfc(2818))(self, format, value); }
static int32_t fs98_extent(uint32_t dc, const char *text, int32_t length,
                         struct fs98_alerts_size *size_out)
{ return GetTextExtentPointA((HDC)dc, text, length, (SIZE *)size_out); }
static uint32_t fs98_text_color(void *self, uint32_t color)
{ return ((uint32_t (TC *)(void *, uint32_t))fs98_alerts_mfc(6172))(self, color); }
static uint32_t fs98_background(void *self, uint32_t color)
{ return ((uint32_t (TC *)(void *, uint32_t))fs98_alerts_mfc(5873))(self, color); }
static int32_t fs98_text_out(uint32_t dc, int32_t x, int32_t y, uint32_t options,
                           const struct fs98_alerts_rect *rect, const char *text,
                           uint32_t length, const int32_t *spacing)
{ return ExtTextOutA((HDC)dc, x, y, options, (const RECT *)rect,
                     text, length, spacing); }
static int32_t fs98_help(uint32_t window, const char *file, uint32_t command, uint32_t context)
{ return WinHelpA((HWND)window, file, command, context); }
static void fs98_base_exchange(void *self, void *exchange)
{ ((void (TC *)(void *, void *))fs98_alerts_ui("?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z"))(self, exchange); }
static void fs98_ddx(void *exchange, uint32_t control_id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))fs98_alerts_mfc(2302))(exchange, control_id, control); }
static void fs98_dib_construct(void *self) { U0("??0CDIBSection@@QAE@XZ", self); }
static void fs98_dib_destroy(void *self) { U0("??1CDIBSection@@UAE@XZ", self); }
static int32_t fs98_base_init(void *self)
{ return ((int32_t (TC *)(void *))fs98_alerts_ui("?OnInitDialog@CSimsDialog@@MAEHXZ"))(self); }
static int32_t fs98_load_string(uint32_t id, char *buffer, uint32_t capacity)
{ return ((int32_t (SC *)(uint32_t, char *, uint32_t))PTR(g_alerts_imports[0].link, 0x2a0))(id, buffer, capacity); }
static void fs98_set_text(void *self, const char *text)
{ ((void (TC *)(void *, const char *))fs98_alerts_ui("?SetWindowTextA@CSimsText@@QAEXPBD@Z"))(self, text); }
static void fs98_init_text(void *self, void *font)
{ ((void (TC *)(void *, void *))fs98_alerts_ui("?Init@CSimsText@@QAEXPAVCSimsFont@@@Z"))(self, font); }
static int32_t fs98_load_dib(void *self, int32_t id, uint32_t module, void *palette, uint32_t flags)
{ return ((int32_t (TC *)(void *, int32_t, uint32_t, void *, uint32_t))fs98_alerts_ui(
    "?LoadDib@CDIBSection@@QAEHHPAUHINSTANCE__@@PAVCDIBPal@@K@Z"))(self, id, module, palette, flags); }
static int32_t fs98_sprite(void *self, void *destination, int32_t x, int32_t y,
                         int32_t width, int32_t height, int32_t source_x, int32_t source_y, uint32_t raster)
{
    ((void (TC *)(void *, void *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))fs98_alerts_ui(
        "?SpriteCopy@CDIBSection@@QAEXPAV1@HHHHHHH@Z"))(self, destination,
        x, y, width, height, source_x, source_y, raster);
    return 0;
}
static int32_t fs98_show(void *self, int32_t show)
{ return ((int32_t (TC *)(void *, int32_t))fs98_alerts_mfc(6215))(self, show); }
static int32_t fs98_window_string(uint32_t module, uint32_t id, char *buffer, int32_t capacity)
{ return LoadStringA((HINSTANCE)module, id, buffer, capacity); }
static void fs98_button_text(void *self, const char *text)
{ ((void (TC *)(void *, const char *))fs98_alerts_mfc(6199))(self, text); }
static int32_t fs98_button_init(void *self, void *font, void *alternate_font, uint32_t bitmap,
                              void *dib, void *palette, uint32_t flags, uint32_t color,
                              uint32_t module, int32_t x, int32_t y, struct fs98_alerts_rect rect)
{
    return ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t,
                          uint32_t, uint32_t, int32_t, int32_t, struct fs98_alerts_rect))fs98_alerts_ui(
        "?Init@CSims3PieceButton@@QAEHPAVCSimsFont@@0IPAVCDIBSection@@PAVCDIBPal@@KHPAUHINSTANCE__@@HHVCRect@@@Z"))(
        self, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect);
}
static int32_t fs98_set_checked(void *self, int32_t checked)
{ return ((int32_t (TC *)(void *, int32_t))fs98_alerts_ui("?SetChecked@CSimsCheckButton@@QAEHH@Z"))(self, checked); }

static const struct fs98_alerts_services s_runtime_services = {
    fs98_string_construct, fs98_string_destroy, fs98_dc_construct, fs98_dc_destroy,
    fs98_stock, fs98_font, fs98_select, fs98_rect, fs98_to_screen, fs98_to_client,
    fs98_format, fs98_extent, fs98_text_color, fs98_background, fs98_text_out,
    fs98_help, fs98_base_exchange, fs98_ddx, fs98_dib_construct, fs98_dib_destroy,
    fs98_base_init, fs98_load_string, fs98_set_text, fs98_init_text, fs98_load_dib,
    fs98_sprite, fs98_show, fs98_window_string, fs98_button_text, fs98_button_init, fs98_set_checked
};

static void *fs98_dialog_construct(void *self, int32_t template, void *parent)
{ return ((void *(TC *)(void *, int32_t, void *))fs98_alerts_ui("??0CSimsDialog@@QAE@HPAVCWnd@@@Z"))(self, template, parent); }
static void fs98_dialog_destroy(void *self) { U0("??1CSimsDialog@@UAE@XZ", self); }
static void fs98_text_construct(void *self) { U0("??0CSimsText@@QAE@XZ", self); }
static void fs98_text_destroy(void *self) { U0("??1CSimsText@@UAE@XZ", self); }
static void fs98_button_construct(void *self) { U0("??0CSims3PieceButton@@QAE@XZ", self); }
static void fs98_button_destroy(void *self) { U0("??1CSims3PieceButton@@UAE@XZ", self); }
static void fs98_window_construct(void *self) { M0(567, self); }
static void fs98_window_destroy(void *self) { M0(795, self); }
static void fs98_check_construct(void *self) { U0("??0CSimsCheckButton@@QAE@XZ", self); }
static void fs98_check_destroy(void *self) { U0("??1CSimsCheckButton@@UAE@XZ", self); }
static void *fs98_allocate(uint32_t size)
{ return ((void *(*)(uint32_t))fs98_alerts_ui("?ui_new@@YAPAXI@Z"))(size); }
static void fs98_ui_delete(void *self)
{ ((void (*)(void *))fs98_alerts_ui("?ui_delete@@YAXPAX@Z"))(self); }
static void fs98_mfc_delete(void *self)
{ ((void (*)(void *))fs98_alerts_mfc(825))(self); }
static int32_t fs98_create(void *self, uint32_t template, void *parent)
{ return ((int32_t (TC *)(void *, uint32_t, void *))fs98_alerts_mfc(2086))(self, template, parent); }
static int32_t fs98_enable(uint32_t window, int32_t enable)
{ return EnableWindow((HWND)window, enable); }
static void fs98_dialog_delete(void *self)
{ ((void (SC *)(void *))fs98_alerts_ui("??3CSimsDialog@@SGXPAX@Z"))(self); }

static const struct fs98_alerts_object_services s_object_services = {
    fs98_dialog_construct, fs98_dialog_destroy, fs98_string_construct, fs98_string_destroy,
    fs98_text_construct, fs98_text_destroy, fs98_button_construct, fs98_button_destroy,
    fs98_window_construct, fs98_window_destroy, fs98_check_construct, fs98_check_destroy,
    fs98_allocate, fs98_ui_delete, fs98_mfc_delete, fs98_create, fs98_show, fs98_enable,
    fs98_dialog_delete
};

static void *fs98_alerts_base_map(void)
{ return fs98_alerts_ui("?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B"); }
static struct fs98_alerts_message_map s_maps[] = {
    {fs98_alerts_base_map, s_modal_messages},
    {fs98_alerts_base_map, s_modeless_messages},
    {fs98_alerts_base_map, s_advisory_messages}
};
static void *TC fs98_alerts_modal_map(void *object) { (void)object; return &s_maps[0]; }
static void *TC fs98_alerts_modeless_map(void *object) { (void)object; return &s_maps[1]; }
static void *TC fs98_alerts_advisory_map(void *object) { (void)object; return &s_maps[2]; }

static int fs98_alerts_bind(void)
{
    uint32_t index;
    struct fs98_alerts_object_tables tables;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); index++) {
        const struct fs98_alerts_binding *binding = &s_import_bindings[index];
        HMODULE module = GetModuleHandleA(binding->dll);
        FARPROC function = module ? GetProcAddress(module, binding->ordinal ?
            (const char *)(uint32_t)binding->ordinal : binding->name) : 0;
        if (!function) return 0;
        s_import_functions[index] = (void *)function;
    }
    for (index = 0; index < sizeof(s_virtual_bindings) / sizeof(s_virtual_bindings[0]); index++) {
        const struct fs98_alerts_virtual_binding *binding = &s_virtual_bindings[index];
        s_vtables[binding->table][binding->slot] = binding->import_index < 0 ?
            binding->local : s_import_functions[binding->import_index];
    }
    tables.modal = s_vtables[0]; tables.modeless = s_vtables[1];
    tables.progress = s_vtables[2]; tables.advisory = s_vtables[3];
    fs98_alerts_set_services(&s_runtime_services);
    fs98_alerts_set_object_services(&s_object_services, &tables);
    return 1;
}

struct fs98_alerts_dynamic_unwind {
    struct fs98_alerts_unwind frame;
    void *dynamic;
    volatile int live;
};
static void fs98_alerts_dynamic_cleanup(struct fs98_alerts_unwind *frame)
{
    struct fs98_alerts_dynamic_unwind *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        fs98_mfc_delete(state->dynamic);
    }
}

static FS98_ALERTS_NOINLINE void fs98_alerts_init(void)
{
    void *dynamic;
    struct fs98_alerts_dynamic_unwind state;
    state.dynamic = 0;
    state.live = 0;
    fs98_alerts_unwind_enter(&state.frame, fs98_alerts_dynamic_cleanup);
    ((void (SC *)(void *, uint32_t *))PTR(g_alerts_imports[0].link, 0x298))(
        &g_alerts_linkage, &s_module);
    fs98_alerts_set_object_module(s_module);
    dynamic = ((void *(*)(uint32_t))fs98_alerts_mfc(823))(0x40);
    state.dynamic = dynamic;
    state.live = 1;
    if (dynamic)
        ((void *(TC *)(void *, void *, int32_t))fs98_alerts_mfc(342))(dynamic, s_extension, 0);
    state.live = 0;
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
    fs98_alerts_set_resource_module(s_resource_module);
    fs98_alerts_unwind_leave(&state.frame);
}

static void fs98_alerts_deinit(void)
{
    if (s_module)
        ((void (SC *)(uint32_t))PTR(g_alerts_imports[0].link, 0x29c))(s_module);
    FreeLibrary((HMODULE)s_resource_module);
    s_resource_module = s_module = 0;
    fs98_alerts_set_object_module(0);
    fs98_alerts_set_resource_module(0);
}

__declspec(dllimport) void *fs98_alerts_simsui_anchor(uint32_t size);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        /* The ordinal import forces SIMSUI/MFC initialization before us. */
        void *volatile anchor = (void *)fs98_alerts_simsui_anchor;
        void *state;
        (void)anchor;
        if (!fs98_alerts_bind()) return FALSE;
        state = ((void *(*)(void))fs98_alerts_mfc(1168))();
        D32(state, 0x1c) = D32(state, 0x20);
        D32(state, 0x28) = D32(state, 0x2c);
        D32(state, 0x20) = D32(state, 0x2c) = 0;
        ((int32_t (SC *)(void *, HINSTANCE))fs98_alerts_mfc(1182))(s_extension, instance);
        s_module = (uint32_t)instance;
        fs98_alerts_set_object_module(s_module);
    } else if (reason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int32_t))fs98_alerts_mfc(1253))(s_extension, 0);
    }
    return TRUE;
}
