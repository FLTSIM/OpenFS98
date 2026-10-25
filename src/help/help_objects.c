/* Five derived dialogs and their exact embedded control lifetimes. */
#include "help_objects.h"
#include "help_unwind.h"
#include "help_objects_tables.inc"
#define D32(pObject, uiOffset) (*(uint32 *)((uint8 *)(pObject) + (uiOffset)))
#define PTR(pObject, uiOffset) (*(void **)((uint8 *)(pObject) + (uiOffset)))
#define TC FS98_HELP_THISCALL
#define SC FS98_HELP_STDCALL

static const struct fs98_help_object_services *s_pServices;
static void *s_apVtables[5], *s_pPlaceholderVtable;
static uint32 s_uiModule;
struct fs98_help_object_unwind {
    struct fs98_help_unwind oFrame;
    void *pObject;
    const struct fs98_help_class *pClass;
    volatile uint32 uiConstructed, uiBase;
};
static void fs98_help_object_cleanup(struct fs98_help_unwind *pFrame)
{
    struct fs98_help_object_unwind *pState = (void *)pFrame;
    while (pState->uiConstructed) {
        const struct fs98_help_subobject *pControl = &pState->pClass->pControls[--pState->uiConstructed];
        s_pServices->pDestroy((uint8 *)pState->pObject + pControl->unOffset, pControl->unDestructor);
    }
    if (pState->uiBase) {
        pState->uiBase = 0;
        s_pServices->pDialogDestroy(pState->pObject);
    }
}
void fs98_help_set_object_services(const struct fs98_help_object_services *pServices,
                                   void *const *ppVtables, void *pPlaceholderVtable)
{
    uint32 uiIndex;
    s_pServices = pServices;
    for (uiIndex = 0; uiIndex < 5; ++uiIndex) s_apVtables[uiIndex] = ppVtables[uiIndex];
    s_pPlaceholderVtable = pPlaceholderVtable;
}
void fs98_help_set_object_module(uint32 uiModule) { s_uiModule = uiModule; }
static FS98_HELP_NOINLINE void *fs98_help_construct(void *pObject, void *pParent, uint32 uiKind)
{
    struct fs98_help_object_unwind oState;
    oState.pObject = pObject; oState.pClass = &s_aClasses[uiKind];
    oState.uiConstructed = oState.uiBase = 0;
    fs98_help_unwind_enter(&oState.oFrame, fs98_help_object_cleanup);
    s_pServices->pDialogConstruct(pObject, oState.pClass->unTemplate, pParent);
    oState.uiBase = 1;
    while (oState.uiConstructed < oState.pClass->unCount) {
        const struct fs98_help_subobject *pControl = &oState.pClass->pControls[oState.uiConstructed];
        void *pChild = (uint8 *)pObject + pControl->unOffset;
        s_pServices->pConstruct(pChild, pControl->unConstructor);
        ++oState.uiConstructed;
        if (uiKind == 1 && pControl->unOffset == 0x1274) PTR(pChild, 0) = s_pPlaceholderVtable;
    }
    PTR(pObject, 0) = s_apVtables[uiKind];
    if (uiKind == 1) {
        D32(pObject, 0x1108) = 1;
        D32(pObject, 0x10e8) = 0;
        D32(pObject, 0x10f4) = 0;
    }
    fs98_help_unwind_leave(&oState.oFrame);
    return pObject;
}
static FS98_HELP_NOINLINE void fs98_help_destroy(void *pObject, uint32 uiKind)
{
    struct fs98_help_object_unwind oState;
    oState.pObject = pObject; oState.pClass = &s_aClasses[uiKind];
    oState.uiConstructed = oState.pClass->unCount; oState.uiBase = 1;
    fs98_help_unwind_enter(&oState.oFrame, fs98_help_object_cleanup);
    fs98_help_object_cleanup(&oState.oFrame);
    fs98_help_unwind_leave(&oState.oFrame);
}
struct fs98_help_factory_unwind {
    struct fs98_help_unwind oFrame;
    void *pObject;
    volatile uint32 uiLive;
};
static void fs98_help_factory_cleanup(struct fs98_help_unwind *pFrame)
{
    struct fs98_help_factory_unwind *pState = (void *)pFrame;
    if (pState->uiLive) { pState->uiLive = 0; s_pServices->pDialogDelete(pState->pObject); }
}
static FS98_HELP_NOINLINE uint32 fs98_help_create(uint32 uiParent, void *pDescriptor, uint32 uiKind)
{
    struct fs98_help_factory_unwind oState;
    const struct fs98_help_class *pClass = &s_aClasses[uiKind];
    void *pObject;
    (void)uiParent;
    oState.pObject = 0; oState.uiLive = 0;
    fs98_help_unwind_enter(&oState.oFrame, fs98_help_factory_cleanup);
    pObject = s_pServices->pAllocate(pClass->unSize);
    oState.pObject = pObject; oState.uiLive = 1;
    if (pObject) pObject = fs98_help_construct(pObject, 0, uiKind);
    *(void *volatile *)((uint8 *)pObject + pClass->unDescriptor) = pDescriptor;
    oState.uiLive = 0;
    D32(pObject, 0x80) = s_uiModule;
    s_pServices->pCreate(pObject, pClass->unTemplate, 0);
    s_pServices->pShow(pObject, 1);
    fs98_help_unwind_leave(&oState.oFrame);
    return D32(pObject, 0x20);
}
#define OBJECT_CLASS(szName, uiKind) \
void *TC fs98_help_##szName##_construct(void *pObject, void *pParent) \
{ return fs98_help_construct(pObject, pParent, uiKind); } \
void TC fs98_help_##szName##_destroy(void *pObject) \
{ fs98_help_destroy(pObject, uiKind); } \
void *TC fs98_help_##szName##_delete(void *pObject, uint32 uiFlags) \
{ \
    fs98_help_destroy(pObject, uiKind); \
    if (uiFlags & 1) s_pServices->pUiDelete(pObject); \
    return pObject; \
} \
uint32 SC fs98_help_##szName##_create(uint32 uiParent, void *pDescriptor) \
{ return fs98_help_create(uiParent, pDescriptor, uiKind); }
OBJECT_CLASS(about, 0)
OBJECT_CLASS(credits, 1)
OBJECT_CLASS(lessons, 2)
OBJECT_CLASS(graph, 3)
OBJECT_CLASS(completion, 4)
#undef OBJECT_CLASS
void *TC fs98_help_placeholder_delete(void *pObject, uint32 uiFlags)
{
    s_pServices->pDestroy(pObject, 0x634c);
    if (uiFlags & 1) s_pServices->pMfcDelete(pObject);
    return pObject;
}
void SC fs98_help_class_delete(void *pObject) { s_pServices->pMfcDelete(pObject); }
