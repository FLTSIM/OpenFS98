/* World 0x223fbef0: native control setup for the shared scenery-area class. */
#include "world_init.h"
#include "world_unwind.h"
#define AT(pObject, uiOffset) ((uint8 *)(pObject) + (uiOffset))
static const struct fs98_world_init_services *s_pInitServices;
static uint32 s_uiResourceModule;
void fs98_world_set_init_services(const struct fs98_world_init_services *pServices) { s_pInitServices = pServices; }
void fs98_world_set_resource_module(uint32 uiModule) { s_uiResourceModule = uiModule; }
struct world_string_unwind { struct fs98_world_unwind stFrame; void *pString; volatile int iLive; };
static void world_string_cleanup(struct fs98_world_unwind *pFrame)
{
    struct world_string_unwind *pState = (void *)pFrame;
    if (pState->iLive) { pState->iLive = 0; s_pInitServices->pfnStringDestroy(pState->pString); }
}
WORLD_NOINLINE int fs98_world_area_init(void *pObject)
{
    const struct fs98_world_services *pServices = fs98_world_get_services();
    void *pDescriptor, *pRow, *pFont;
    void **ppVtable;
    uint32 uiIndex, uiString = 0;
    int iEnabled;
    struct fs98_world_rect stRect = {0, 0, 0, 0};
    struct world_string_unwind stState;
    static const uint32 s_auiEnable[] = {0x11a0, 0x1258, 0x1710, 0x1a08, 0x1a50, 0x17c8, 0x19a0};
    s_pInitServices->pfnBaseInit(pObject);
    ppVtable = WORLD_PTR(pObject, 0);
    if (!((int (FS98_WORLD_THISCALL *)(void *, uint32, uint32, uint32))ppVtable[0xe4 / 4])(pObject, 0x14f, s_uiResourceModule, 15)) return 0;
    pDescriptor = WORLD_PTR(pObject, 0x10e4);
    pRow = WORLD_PTR(pDescriptor, 0xc);
    if (!*(char *)WORLD_PTR(pRow, 4)) {
        stState.pString = &uiString; stState.iLive = 0;
        world_unwind_enter(&stState.stFrame, world_string_cleanup);
        s_pInitServices->pfnStringConstruct(&uiString); stState.iLive = 1;
        {
            char *pcBuffer = s_pInitServices->pfnStringBuffer(&uiString, 0x100);
            s_pInitServices->pfnLoadString(WORLD_U32(pObject, 0x80), 0xf8d5, pcBuffer, 0x100);
        }
        s_pInitServices->pfnStringRelease(&uiString, -1);
        pServices->pfnSetText(AT(pObject, 0x12c0), (char *)uiString);
        WORLD_U32(pObject, 0x10e0) = 0;
        stState.iLive = 0; s_pInitServices->pfnStringDestroy(&uiString);
        world_unwind_leave(&stState.stFrame);
    }
    s_pInitServices->pfnArt(AT(pObject, 0x12c0), 0xf831, AT(pObject, 0xff4), AT(pObject, 0x7e8), 2);
    s_pInitServices->pfnButton(AT(pObject, 0x155c), AT(pObject, 0x5a0), AT(pObject, 0x610), 0x132,
        AT(pObject, 0xff4), AT(pObject, 0x7e8), 1, -1, s_uiResourceModule, 0, 0, stRect);
    s_pInitServices->pfnButton(AT(pObject, 0x1460), AT(pObject, 0x5a0), AT(pObject, 0x610), 0x132,
        AT(pObject, 0xff4), AT(pObject, 0x7e8), 1, -1, s_uiResourceModule, 0, 0, stRect);
    s_pInitServices->pfnText(AT(pObject, 0x1710), AT(pObject, 0x370));
    s_pInitServices->pfnCombo(AT(pObject, 0x1a08), AT(pObject, 0x3e0), AT(pObject, 0x1710));
    pServices->pfnSend(WORLD_U32(pObject, 0x1a28), 0x14e, 0, 0);
    s_pInitServices->pfnText(AT(pObject, 0x1658), AT(pObject, 0x220));
    s_pInitServices->pfnText(AT(pObject, 0x10e8), AT(pObject, 0x220));
    ppVtable = WORLD_PTR(AT(pObject, 0x1a50), 0);
    ((int (FS98_WORLD_THISCALL *)(void *, void *, uint32, void *, void *, uint32))ppVtable[0xc4 / 4])(
        AT(pObject, 0x1a50), AT(pObject, 0x4c0), 0x139, AT(pObject, 0xff4), AT(pObject, 0x7e8), s_uiResourceModule);
    s_pInitServices->pfnText(AT(pObject, 0x11a0), AT(pObject, 0x220));
    pFont = s_pInitServices->pfnBaseFont(2);
    s_pInitServices->pfnEdit(AT(pObject, 0x1258), pFont, 0xffffff, 0, 0xff0000, AT(pObject, 0x17c8), 0);
    s_pInitServices->pfnText(AT(pObject, 0x1880), AT(pObject, 0x220));
    pFont = s_pInitServices->pfnBaseFont(2);
    s_pInitServices->pfnEdit(AT(pObject, 0x1938), pFont, 0xffffff, 0, 0xff0000, AT(pObject, 0x1880), 0);
    s_pInitServices->pfnText(AT(pObject, 0x17c8), AT(pObject, 0x220));
    pFont = s_pInitServices->pfnBaseFont(2);
    s_pInitServices->pfnEdit(AT(pObject, 0x19a0), pFont, 0xffffff, 0, 0xff0000, AT(pObject, 0x17c8), 1);
    pDescriptor = WORLD_PTR(pObject, 0x10e4); pRow = WORLD_PTR(pDescriptor, 0xc);
    pServices->pfnSend(WORLD_U32(pObject, 0x19c0), 0xc5, 4, 0);
    pServices->pfnSend(WORLD_U32(pObject, 0x1958), 0xc5, WORLD_U32(pRow, 8) - 1, 0);
    pServices->pfnSend(WORLD_U32(pObject, 0x1278), 0xc5, WORLD_U32(pRow, 0x1c) - 1, 0);
    pDescriptor = WORLD_PTR(pObject, 0x10e4);
    s_pInitServices->pfnTitle(AT(pObject, 0x10e8), WORLD_PTR(pDescriptor, 8));
    fs98_world_area_populate(pObject);
    iEnabled = WORLD_U32(pObject, 0x10e0) != 0;
    for (uiIndex = 0; uiIndex < sizeof(s_auiEnable) / sizeof(s_auiEnable[0]); ++uiIndex)
        pServices->pfnEnable(AT(pObject, s_auiEnable[uiIndex]), iEnabled);
    return 1;
}
