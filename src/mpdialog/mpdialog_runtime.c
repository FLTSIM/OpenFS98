/* Native ABI and MFC/SIMSUI binding for the isolated MP_DIALOG slice. */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "mpdialog_internal.h"

#define TC FS98_MPDIALOG_THISCALL
#define SC FS98_MPDIALOG_STDCALL

#include "mpdialog_runtime_tables.inc"

static void *s_import_functions[sizeof(s_import_bindings) / sizeof(s_import_bindings[0])];
void *fs98_mpdialog_vtables[62];
void *fs98_mpdialog_details_vtables[64];
void *fs98_mpdialog_flow_vtables[5][62];
struct fs98_mpdialog_message_map fs98_mpdialog_maps[7];
static uint8_t s_extension[0x14];
static uint32_t s_registered_module, s_resource_module;

__declspec(dllexport) struct fs98_mpdialog_import ImportTable[] = {{1, 0}, {2, 0}, {0, 0}};
static void mpdialog_module_init(void);
static void mpdialog_module_deinit(void);
__declspec(dllexport) struct fs98_mpdialog_linkage Linkage = {
    0x8000000a, mpdialog_module_init, mpdialog_module_deinit, 0, 0,
    (struct fs98_mpdialog_dialog *)s_dialogs
};

static void *mpdialog_import(uint32_t index)
{ return index < sizeof(s_import_functions) / sizeof(s_import_functions[0]) ? s_import_functions[index] : 0; }

void *fs98_mpdialog_simsui(const char *name)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); ++index)
        if (!_stricmp(s_import_bindings[index].dll, "SIMSUI.DLL") &&
            !strcmp(s_import_bindings[index].name, name)) return s_import_functions[index];
    return 0;
}

void *fs98_mpdialog_mfc(uint32_t ordinal)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); ++index)
        if (!_stricmp(s_import_bindings[index].dll, "MFC42.DLL") &&
            s_import_bindings[index].ordinal == ordinal) return s_import_functions[index];
    return 0;
}

void *fs98_mpdialog_import_va(uint32_t iat_va)
{
    uint32_t index;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); ++index)
        if (s_import_bindings[index].iat_va == iat_va) return s_import_functions[index];
    return 0;
}

uint32_t fs98_mpdialog_instance(void) { return s_registered_module; }
uint32_t fs98_mpdialog_resource_module(void) { return s_resource_module; }

void *fs98_mpdialog_allocate(uint32_t size)
{ return ((void *(*)(uint32_t))fs98_mpdialog_simsui("?ui_new@@YAPAXI@Z"))(size); }

void fs98_mpdialog_free(void *object)
{ ((void (*)(void *))fs98_mpdialog_simsui("?ui_delete@@YAXPAX@Z"))(object); }

int32_t fs98_mpdialog_create_window(void *object, uint32_t template_id, void *parent)
{ return ((int32_t (TC *)(void *, uint32_t, void *))fs98_mpdialog_mfc(2086))(object, template_id, parent); }

int32_t fs98_mpdialog_show_window(void *object, int32_t show)
{ return ((int32_t (TC *)(void *, int32_t))fs98_mpdialog_mfc(6215))(object, show); }

int32_t fs98_mpdialog_base_init(void *object)
{ return ((int32_t (TC *)(void *))fs98_mpdialog_simsui("?OnInitDialog@CSimsDialog@@MAEHXZ"))(object); }

void fs98_mpdialog_ddx_control(void *exchange, uint32_t id, void *control)
{ ((void (SC *)(void *, uint32_t, void *))fs98_mpdialog_mfc(2302))(exchange, id, control); }

int32_t fs98_mpdialog_check(void *control, int32_t value)
{ return ((int32_t (TC *)(void *, int32_t))fs98_mpdialog_simsui("?SetChecked@CSimsCheckButton@@QAEHH@Z"))(control, value); }

int32_t fs98_mpdialog_enable(void *control, int32_t value)
{ return ((int32_t (TC *)(void *, int32_t))fs98_mpdialog_mfc(2642))(control, value); }

int32_t fs98_mpdialog_show_control(void *control, int32_t value)
{ return ((int32_t (TC *)(void *, int32_t))fs98_mpdialog_mfc(6215))(control, value); }

void fs98_mpdialog_focus_control(void *control)
{ ((void *(TC *)(void *))fs98_mpdialog_mfc(5981))(control); }

static void *mpdialog_base_map(void)
{ return fs98_mpdialog_simsui("?messageMap@CSimsDialog@@1UAFX_MSGMAP@@B"); }

static void mpdialog_bind(void)
{
    uint32_t index;
    HMODULE module;
    for (index = 0; index < sizeof(s_import_bindings) / sizeof(s_import_bindings[0]); ++index) {
        const struct fs98_mpdialog_import_binding *binding = &s_import_bindings[index];
        module = GetModuleHandleA(binding->dll);
        if (!module) return;
        s_import_functions[index] = (void *)GetProcAddress(module, binding->ordinal ?
            (const char *)(uintptr_t)binding->ordinal : binding->name);
        if (!s_import_functions[index]) return;
    }
    for (index = 0; index < sizeof(s_virtual_bindings) / sizeof(s_virtual_bindings[0]); ++index) {
        const struct fs98_mpdialog_virtual_binding *binding = &s_virtual_bindings[index];
        fs98_mpdialog_vtables[binding->slot] = binding->import_index < 0 ?
            binding->local : mpdialog_import((uint32_t)binding->import_index);
    }
    for (index = 0; index < sizeof(s_details_virtual_bindings) / sizeof(s_details_virtual_bindings[0]); ++index) {
        const struct fs98_mpdialog_virtual_binding *binding = &s_details_virtual_bindings[index];
        fs98_mpdialog_details_vtables[binding->slot] = binding->import_index < 0 ?
            binding->local : mpdialog_import((uint32_t)binding->import_index);
    }
    for (index = 0; index < 5; ++index) {
        const struct fs98_mpdialog_virtual_binding *bindings;
        uint32_t slot;
        switch (index) {
        case 0: bindings = s_flow_virtual_bindings_0; break;
        case 1: bindings = s_flow_virtual_bindings_1; break;
        case 2: bindings = s_flow_virtual_bindings_2; break;
        case 3: bindings = s_flow_virtual_bindings_3; break;
        default: bindings = s_flow_virtual_bindings_4; break;
        }
        for (slot = 0; slot < 62; ++slot)
            fs98_mpdialog_flow_vtables[index][bindings[slot].slot] =
                bindings[slot].import_index < 0 ? bindings[slot].local :
                    mpdialog_import((uint32_t)bindings[slot].import_index);
    }
    fs98_mpdialog_maps[0].base = mpdialog_base_map;
    fs98_mpdialog_maps[0].entries = s_messages;
    fs98_mpdialog_maps[1].base = mpdialog_base_map;
    fs98_mpdialog_maps[1].entries = s_details_messages;
    fs98_mpdialog_maps[2].base = mpdialog_base_map;
    fs98_mpdialog_maps[2].entries = s_flow_messages_intro;
    fs98_mpdialog_maps[3].base = mpdialog_base_map;
    fs98_mpdialog_maps[3].entries = s_flow_messages_choice;
    fs98_mpdialog_maps[4].base = mpdialog_base_map;
    fs98_mpdialog_maps[4].entries = s_flow_messages_host;
    fs98_mpdialog_maps[5].base = mpdialog_base_map;
    fs98_mpdialog_maps[5].entries = s_flow_messages_connection;
    fs98_mpdialog_maps[6].base = mpdialog_base_map;
    fs98_mpdialog_maps[6].entries = s_flow_messages_browser;
}

static void mpdialog_module_init(void)
{
    void *dynamic;
    void (SC *register_module)(void *, uint32_t *) =
        (void (SC *)(void *, uint32_t *))(*(void **)((uint8_t *)ImportTable[0].link + 0x298));
    register_module(&Linkage, &s_registered_module);
    dynamic = ((void *(*)(uint32_t))fs98_mpdialog_mfc(823))(0x40);
    if (dynamic)
        ((void *(TC *)(void *, void *, int32_t))fs98_mpdialog_mfc(342))(dynamic, s_extension, 0);
    s_resource_module = (uint32_t)LoadLibraryA("dialog.dll");
}

static void mpdialog_module_deinit(void)
{
    if (s_registered_module) {
        void (SC *release_module)(uint32_t) =
            (void (SC *)(uint32_t))(*(void **)((uint8_t *)ImportTable[0].link + 0x29c));
        release_module(s_registered_module);
    }
    FreeLibrary((HMODULE)s_resource_module);
    s_resource_module = s_registered_module = 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    __declspec(dllimport) void *fs98_mpdialog_simsui_anchor(uint32_t size);
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        void *volatile anchor = (void *)fs98_mpdialog_simsui_anchor;
        (void)anchor;
        mpdialog_bind();
        if (!fs98_mpdialog_mfc(1182) || !fs98_mpdialog_simsui("?ui_new@@YAPAXI@Z")) return FALSE;
        ((int32_t (SC *)(void *, HINSTANCE))fs98_mpdialog_mfc(1182))(s_extension, instance);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (fs98_mpdialog_mfc(1253))
            ((void (SC *)(void *, int32_t))fs98_mpdialog_mfc(1253))(s_extension, 0);
    }
    return TRUE;
}
