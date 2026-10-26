/* Add/edit scenery-area class lifetimes, exchange and help dispatch. */
#include "world_objects.h"
#include "world_unwind.h"
#include "world_exceptions.h"
#include "world_tables.inc"
static const struct fs98_world_object_services *s_pObjectServices;
static void *s_pAreaVtable;
static uint32 s_uiModule;
void fs98_world_set_object_services(const struct fs98_world_object_services *pServices, void *pVtable)
{ s_pObjectServices = pServices; s_pAreaVtable = pVtable; }
void fs98_world_set_module(uint32 uiModule) { s_uiModule = uiModule; }
struct world_object_unwind {
    struct fs98_world_unwind stFrame;
    void *pObject;
    volatile uint32 uiConstructed, uiBase;
};
static void world_destroy_remaining(struct world_object_unwind *pState)
{
    while (pState->uiConstructed) {
        const struct fs98_world_control *pControl = &s_astAreaControls[--pState->uiConstructed];
        s_pObjectServices->pfnDestroy((uint8 *)pState->pObject + pControl->uiOffset, pControl->uiDestructor);
    }
    if (pState->uiBase) {
        pState->uiBase = 0;
        s_pObjectServices->pfnDestroy(pState->pObject, WORLD_AREA_BASE_DTOR);
    }
}
static void world_object_cleanup(struct fs98_world_unwind *pFrame)
{ world_destroy_remaining((struct world_object_unwind *)pFrame); }
/* 0x223fbb30 */
WORLD_NOINLINE void *fs98_world_area_construct(void *pObject, void *pParent)
{
    struct world_object_unwind stState;
    stState.pObject = pObject; stState.uiConstructed = stState.uiBase = 0;
    world_unwind_enter(&stState.stFrame, world_object_cleanup);
    s_pObjectServices->pfnBaseConstruct(pObject, 0x6b3, pParent);
    stState.uiBase = 1;
    while (stState.uiConstructed < sizeof(s_astAreaControls) / sizeof(s_astAreaControls[0])) {
        const struct fs98_world_control *pControl = &s_astAreaControls[stState.uiConstructed];
        s_pObjectServices->pfnConstruct((uint8 *)pObject + pControl->uiOffset, pControl->uiConstructor);
        ++stState.uiConstructed;
    }
    WORLD_PTR(pObject, 0) = s_pAreaVtable;
    WORLD_U32(pObject, 0x10e0) = 1;
    world_unwind_leave(&stState.stFrame);
    return pObject;
}
/* 0x223fbc90 does not reset its vtable on entry. */
WORLD_NOINLINE void fs98_world_area_destroy(void *pObject)
{
    struct world_object_unwind stState;
    stState.pObject = pObject; stState.uiBase = 1;
    stState.uiConstructed = sizeof(s_astAreaControls) / sizeof(s_astAreaControls[0]);
    world_unwind_enter(&stState.stFrame, world_object_cleanup);
    world_destroy_remaining(&stState);
    world_unwind_leave(&stState.stFrame);
}
/* 0x223fbc70 */
void *fs98_world_area_delete(void *pObject, uint32 uiFlags)
{
    fs98_world_area_destroy(pObject);
    if (uiFlags & 1) s_pObjectServices->pfnUiDelete(pObject);
    return pObject;
}
struct world_factory_unwind {
    struct fs98_world_unwind stFrame;
    void *pObject;
    volatile uint32 uiLive;
};
static void world_factory_cleanup(struct fs98_world_unwind *pFrame)
{
    struct world_factory_unwind *pState = (void *)pFrame;
    if (pState->uiLive) {
        pState->uiLive = 0;
        s_pObjectServices->pfnDialogDelete(pState->pObject);
    }
}
/* Retail catches CException* and calls SIMSUI_ExitCleanlyE, which aborts.
 * Keep this distinct from unrelated exceptions, which propagate after cleanup. */
static EXCEPTION_DISPOSITION __cdecl world_factory_handler(
    EXCEPTION_RECORD *pRecord, void *pEstablisher, CONTEXT *pContext, void *pDispatcher)
{
    struct world_factory_unwind *pState = pEstablisher;
    void *pException;
    if (!(pRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) &&
        world_exception_pointer(pRecord, &pException)) {
        RtlUnwind(pEstablisher, NULL, pRecord, NULL);
        world_factory_cleanup(&pState->stFrame);
        s_pObjectServices->pfnExitCleanly(pException);
        /* SIMSUI's real implementation cannot return. */
        __builtin_trap();
    }
    return world_unwind_handler(pRecord, pEstablisher, pContext, pDispatcher);
}
/* 0x223f1920 / 0x223f19f0. The parent argument is ignored. */
static WORLD_NOINLINE uint32 world_area_create(uint32 uiParent, void *pDescriptor)
{
    struct world_factory_unwind stState;
    void *pObject;
    (void)uiParent;
    stState.pObject = NULL; stState.uiLive = 0;
    world_unwind_enter(&stState.stFrame, world_factory_cleanup);
    stState.stFrame.pfnHandler = world_factory_handler;
    pObject = s_pObjectServices->pfnAllocate(0x1b20);
    stState.pObject = pObject; stState.uiLive = 1;
    if (pObject) pObject = fs98_world_area_construct(pObject, NULL);
    /* A null allocation faults here in retail, before Create. */
    *(void *volatile *)((uint8 *)pObject + 0x10e4) = pDescriptor;
    WORLD_U32(pObject, 0x80) = s_uiModule;
    stState.uiLive = 0;
    if (!s_pObjectServices->pfnCreate(pObject, 0x6b3, NULL))
        s_pObjectServices->pfnMessage("CSceneryEdit::Create() failed\n", 0, 0);
    s_pObjectServices->pfnShow(pObject, 1);
    world_unwind_leave(&stState.stFrame);
    return WORLD_U32(pObject, 0x20);
}
uint32 FS98_WORLD_STDCALL fs98_world_area_create_add(uint32 uiParent, void *pDescriptor)
{ return world_area_create(uiParent, pDescriptor); }
uint32 FS98_WORLD_STDCALL fs98_world_area_create_edit(uint32 uiParent, void *pDescriptor)
{ return world_area_create(uiParent, pDescriptor); }
/* 0x223fbdc0 */
void fs98_world_area_exchange(void *pObject, void *pExchange)
{
    uint32 uiIndex;
    for (uiIndex = 0; uiIndex < sizeof(s_aaunAreaDdx) / sizeof(s_aaunAreaDdx[0]); ++uiIndex)
        s_pObjectServices->pfnDdx(pExchange, s_aaunAreaDdx[uiIndex][1], (uint8 *)pObject + s_aaunAreaDdx[uiIndex][0]);
}
/* 0x223fc5d0 leaves the WinHelp result in eax. */
int fs98_world_area_help(void *pObject, uint32 uiUnused, void *pHelp)
{
    (void)pObject; (void)uiUnused;
    return s_pObjectServices->pfnHelp(WORLD_U32(pHelp, 0xc), "fs98.hlp", 12, s_auiAreaHelp);
}
/* 0x223fc5f0 */
int fs98_world_area_context(void *pObject, uint32 uiWindow, uint32 uiUnused)
{
    (void)pObject; (void)uiUnused;
    s_pObjectServices->pfnHelp(uiWindow, "fs98.hlp", 10, s_auiAreaHelp);
    return 1;
}
