/* Partial native World adapter: only add/edit scenery area are registered. */
#include <windows.h>
#include "world_objects.h"
#include "world_init.h"
#include "world_unwind.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
struct world_import_binding { const char *pcDll, *pcName; uint32 uiOrdinal, uiSlot; };
struct world_virtual_binding { uint32 uiSlot; int iImport; void *pLocal; };
struct world_message_entry { uint32 uiMessage, uiCode, uiFirst, uiLast, uiSignature; void *pHandler; };
struct world_message_map { void *(*pfnBase)(void); const struct world_message_entry *pEntries; };
struct world_dialog { uint32 uiId; uint32 (SC *pfnCreate)(uint32, void *); uint32 uiFlags; };
static struct world_message_map s_stMessageMap;
#include "world_runtime_tables.inc"
static void *s_apFunctions[sizeof(s_astImports) / sizeof(s_astImports[0])], *s_apVtable[60];
static uint8 s_abyExtension[0x14];
static uint32 s_uiModule, s_uiResourceModule;
struct world_import { uint32 uiId; void *pLink; };
struct world_linkage { uint32 uiId; void (*pfnInit)(void), (*pfnDeinit)(void); void *pReserved; uint32 uiFlags; const struct world_dialog *pDialogs; };
static void world_module_init(void);
static void world_module_deinit(void);
struct world_import g_astWorldImports[] = {{1, 0}, {2, 0}, {0, 0}};
struct world_linkage g_stWorldLinkage = {0x80000006, world_module_init, world_module_deinit, 0, 0, s_astDialogs};
static void *world_slot(uint32 uiSlot)
{
    uint32 uiIndex;
    for (uiIndex = 0; uiIndex < sizeof(s_astImports) / sizeof(s_astImports[0]); ++uiIndex)
        if (s_astImports[uiIndex].uiSlot == uiSlot) return s_apFunctions[uiIndex];
    return NULL;
}
static void *world_mfc(uint32 uiOrdinal)
{ return (void *)GetProcAddress(GetModuleHandleA("MFC42.DLL"), (const char *)uiOrdinal); }
static void *world_crt(const char *pcName)
{ return (void *)GetProcAddress(GetModuleHandleA("MSVCRT.DLL"), pcName); }
static void * world_base_construct(void *pObject, uint32 uiTemplate, void *pParent)
{ return ((void * (TC *)(void *, uint32, void *))world_slot(0x25804))(pObject, uiTemplate, pParent); }
static void world_construct(void *pObject, uint32 uiImport)
{ ((void (TC *)(void *))world_slot(uiImport))(pObject); }
static void world_destroy(void *pObject, uint32 uiImport)
{ ((void (TC *)(void *))world_slot(uiImport))(pObject); }
static void * world_allocate(uint32 uiBytes)
{ return ((void * ( *)(uint32))world_slot(0x2584c))(uiBytes); }
static void world_ui_delete(void *pObject)
{ ((void ( *)(void *))world_slot(0x25800))(pObject); }
static void world_dialog_delete(void *pObject)
{ ((void (SC *)(void *))world_slot(0x25850))(pObject); }
static int world_create(void *pObject, uint32 uiTemplate, void *pParent)
{ return ((int (TC *)(void *, uint32, void *))world_slot(0x25524))(pObject, uiTemplate, pParent); }
static int world_show(void *pObject, int iShow)
{ return ((int (TC *)(void *, int))world_slot(0x25500))(pObject, iShow); }
static int world_message(const char *pcText, uint32 uiFlags, uint32 uiHelp)
{ return ((int (SC *)(const char *, uint32, uint32))world_slot(0x2551c))(pcText, uiFlags, uiHelp); }
static void world_ddx(void *pExchange, uint32 uiId, void *pControl)
{ ((void (SC *)(void *, uint32, void *))world_slot(0x255fc))(pExchange, uiId, pControl); }
static void world_exit(void *pException)
{ ((void ( *)(void *))world_slot(0x25848))(pException); }
static int world_get_text(void *pControl, char *pcText, int iCapacity)
{ return ((int (TC *)(void *, char *, int))world_slot(0x25598))(pControl, pcText, iCapacity); }
static void world_set_text(void *pControl, const char *pcText)
{ ((void (TC *)(void *, const char *))world_slot(0x255bc))(pControl, pcText); }
static int world_check(void *pControl, int iChecked)
{ return ((int (TC *)(void *, int))world_slot(0x2577c))(pControl, iChecked); }
static int world_enable(void *pControl, int iEnable)
{ return ((int (TC *)(void *, int))world_slot(0x255c8))(pControl, iEnable); }
static int world_error(void *pObject, void *pControl, void *pStatus)
{ return ((int (TC *)(void *, void *, void *))world_slot(0x25778))(pObject, pControl, pStatus); }
static void world_focus(void *pControl)
{ ((void (TC *)(void *))world_slot(0x25590))(pControl); }
static int world_base_init(void *pObject)
{ return ((int (TC *)(void *))world_slot(0x257c0))(pObject); }
static void world_string_construct(void *pString)
{ ((void (TC *)(void *))world_slot(0x255b4))(pString); }
static char * world_string_buffer(void *pString, int iMinimum)
{ return ((char * (TC *)(void *, int))world_slot(0x25588))(pString, iMinimum); }
static void world_string_release(void *pString, int iLength)
{ ((void (TC *)(void *, int))world_slot(0x2558c))(pString, iLength); }
static void world_string_destroy(void *pString)
{ ((void (TC *)(void *))world_slot(0x255c0))(pString); }
static int world_art(void *pControl, uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiFlags)
{ return ((int (TC *)(void *, uint32, void *, void *, uint32))world_slot(0x257c4))(pControl, uiBitmap, pDib, pPalette, uiFlags); }
static int world_button(void *pControl, void *pFont, void *pAlternate, uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiFlags, int iColor, uint32 uiModule, int iX, int iY, struct fs98_world_rect stRect)
{ return ((int (TC *)(void *, void *, void *, uint32, void *, void *, uint32, int, uint32, int, int, struct fs98_world_rect))world_slot(0x257c8))(pControl, pFont, pAlternate, uiBitmap, pDib, pPalette, uiFlags, iColor, uiModule, iX, iY, stRect); }
static void world_text(void *pControl, void *pFont)
{ ((void (TC *)(void *, void *))world_slot(0x257dc))(pControl, pFont); }
static void world_combo(void *pControl, void *pFont, void *pLabel)
{ ((void (TC *)(void *, void *, void *))world_slot(0x25790))(pControl, pFont, pLabel); }
static void * world_font(int iFont)
{ return ((void * (SC *)(int))world_slot(0x257d0))(iFont); }
static void world_edit(void *pControl, void *pFont, uint32 uiForeground, uint32 uiBackground, uint32 uiError, void *pLabel, uint32 uiFlags)
{ ((void (TC *)(void *, void *, uint32, uint32, uint32, void *, uint32))world_slot(0x25740))(pControl, pFont, uiForeground, uiBackground, uiError, pLabel, uiFlags); }
static void world_title(void *pControl, const char *pcText)
{ ((void (TC *)(void *, const char *))world_slot(0x257e0))(pControl, pcText); }
static int world_help(uint32 uiWindow, const char *pcFile, uint32 uiCommand, const uint32 *puiMap)
{ return WinHelpA((HWND)uiWindow, pcFile, uiCommand, (ULONG_PTR)puiMap); }
static int world_send(uint32 uiWindow, uint32 uiMessage, uint32 uiFirst, uint32 uiSecond)
{ return SendMessageA((HWND)uiWindow, uiMessage, uiFirst, uiSecond); }
static int world_parse(const char *pcText) { return ((int (*)(const char *))world_crt("atoi"))(pcText); }
static int world_format(char *pcText, const char *pcFormat, int iValue)
{ return ((int (*)(char *, const char *, ...))world_crt("sprintf"))(pcText, pcFormat, iValue); }
static int world_load_string(uint32 uiModule, uint32 uiId, char *pcBuffer, int iCapacity)
{ return LoadStringA((HMODULE)uiModule, uiId, pcBuffer, iCapacity); }
static const struct fs98_world_services s_stServices = {
    world_get_text, world_set_text, world_parse, world_format, world_send,
    world_check, world_enable, world_error, world_focus
};
static const struct fs98_world_object_services s_stObjectServices = {
    world_base_construct, world_construct, world_destroy, world_allocate,
    world_ui_delete, world_dialog_delete, world_create, world_show, world_message,
    world_ddx, world_exit, world_help
};
static const struct fs98_world_init_services s_stInitServices = {
    world_base_init, world_string_construct, world_string_buffer, world_string_release,
    world_string_destroy, world_load_string, world_art, world_button, world_text, world_combo,
    world_font, world_edit, world_title
};
static void *world_base_map(void) { return world_slot(0x257f8); }
static int world_bind(void)
{
    uint32 uiIndex;
    for (uiIndex = 0; uiIndex < sizeof(s_astImports) / sizeof(s_astImports[0]); ++uiIndex) {
        const struct world_import_binding *pBinding = &s_astImports[uiIndex];
        HMODULE hModule = GetModuleHandleA(pBinding->pcDll);
        void *pFunction = hModule ? (void *)GetProcAddress(hModule,
            pBinding->uiOrdinal ? (const char *)pBinding->uiOrdinal : pBinding->pcName) : NULL;
        if (!pFunction) return 0;
        s_apFunctions[uiIndex] = pFunction;
    }
    for (uiIndex = 0; uiIndex < sizeof(s_astVirtuals) / sizeof(s_astVirtuals[0]); ++uiIndex) {
        const struct world_virtual_binding *pBinding = &s_astVirtuals[uiIndex];
        s_apVtable[pBinding->uiSlot] = pBinding->iImport < 0 ? pBinding->pLocal : s_apFunctions[pBinding->iImport];
    }
    s_stMessageMap.pfnBase = world_base_map; s_stMessageMap.pEntries = s_astMessages;
    fs98_world_set_services(&s_stServices);
    fs98_world_set_object_services(&s_stObjectServices, s_apVtable);
    fs98_world_set_init_services(&s_stInitServices);
    return 1;
}
struct world_dynamic_unwind { struct fs98_world_unwind stFrame; void *pDynamic; volatile int iLive; };
static void world_dynamic_cleanup(struct fs98_world_unwind *pFrame)
{
    struct world_dynamic_unwind *pState = (void *)pFrame;
    if (pState->iLive) { pState->iLive = 0; ((void (*)(void *))world_mfc(825))(pState->pDynamic); }
}
static WORLD_NOINLINE void world_module_init(void)
{
    struct world_dynamic_unwind stState;
    void *pDynamic;
    stState.pDynamic = NULL; stState.iLive = 0;
    world_unwind_enter(&stState.stFrame, world_dynamic_cleanup);
    ((void (SC *)(void *, uint32 *))WORLD_PTR(g_astWorldImports[0].pLink, 0x298))(&g_stWorldLinkage, &s_uiModule);
    fs98_world_set_module(s_uiModule);
    pDynamic = ((void *(*)(uint32))world_mfc(823))(0x40);
    stState.pDynamic = pDynamic; stState.iLive = 1;
    if (pDynamic) ((void *(TC *)(void *, void *, int))world_mfc(342))(pDynamic, s_abyExtension, 0);
    stState.iLive = 0;
    s_uiResourceModule = (uint32)LoadLibraryA("dialog.dll");
    fs98_world_set_resource_module(s_uiResourceModule);
    world_unwind_leave(&stState.stFrame);
}
static void world_module_deinit(void)
{
    if (s_uiModule) ((void (SC *)(uint32))WORLD_PTR(g_astWorldImports[0].pLink, 0x29c))(s_uiModule);
    FreeLibrary((HMODULE)s_uiResourceModule);
    s_uiModule = s_uiResourceModule = 0;
    fs98_world_set_module(0); fs98_world_set_resource_module(0);
}
__declspec(dllimport) void *fs98_world_simsui_anchor(uint32 uiSize);
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID pReserved)
{
    (void)pReserved;
    if (dwReason == DLL_PROCESS_ATTACH) {
        void *volatile pAnchor = (void *)fs98_world_simsui_anchor;
        (void)pAnchor;
        if (!world_bind()) return FALSE;
        ((int (SC *)(void *, HINSTANCE))world_mfc(1182))(s_abyExtension, hInstance);
        s_uiModule = (uint32)hInstance; fs98_world_set_module(s_uiModule);
    } else if (dwReason == DLL_PROCESS_DETACH) {
        ((void (SC *)(void *, int))world_mfc(1253))(s_abyExtension, 0);
    }
    return TRUE;
}
