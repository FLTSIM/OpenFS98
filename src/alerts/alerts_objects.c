/* Retail Alerts' ordinary C++ object lifecycle and three factory functions.
 * Exception unwinding from a throwing dependency still needs the real runtime
 * adapter; these functions preserve the normal-path subobject call order. */
#include "alerts_objects.h"

#define D32(pObject, iOffset) (*(uint32 *)((uint8 *)(pObject) + (iOffset)))
#define PTR(pObject, iOffset) (*(void **)((uint8 *)(pObject) + (iOffset)))

static const struct fs98_alerts_object_services *s_pServices;
static struct fs98_alerts_object_tables s_stTables;
static uint32 s_uiModule;

void fs98_alerts_set_object_services(const struct fs98_alerts_object_services *pServices,
                                    const struct fs98_alerts_object_tables *pTables)
{
    s_pServices = pServices;
    s_stTables = *pTables;
}

void fs98_alerts_set_object_module(uint32 uiModule)
{
    s_uiModule = uiModule;
}

/* 0x223012f0: the three buttons are constructed in ascending order. */
void *FS98_ALERTS_THISCALL fs98_alerts_modal_construct(void *pObject, void *pParent)
{
    uint32 uiIndex;
    s_pServices->pDialogConstruct(pObject, 0x8a, pParent);
    s_pServices->pStringConstruct((uint8 *)pObject + 0x10f0);
    s_pServices->pTextConstruct((uint8 *)pObject + 0x10fc);
    for (uiIndex = 0; uiIndex < 3; uiIndex++)
        s_pServices->pButtonConstruct((uint8 *)pObject + 0x11b4 + uiIndex * 0xfc);
    D32(pObject, 0x10dc) = 0;
    D32(pObject, 0x10e0) = 0;
    D32(pObject, 0x10e4) = 0;
    D32(pObject, 0x10e8) = 0;
    D32(pObject, 0x10ec) = 0;
    D32(pObject, 0x10f8) = 0xffffffff;
    D32(pObject, 0x10f4) = 0xffffffff;
    PTR(pObject, 0) = s_stTables.pModal;
    return pObject;
}

/* 0x22301a50: the embedded CWnd gets a derived progress vtable before text. */
void *FS98_ALERTS_THISCALL fs98_alerts_modeless_construct(void *pObject, void *pParent)
{
    s_pServices->pDialogConstruct(pObject, 0x8b, pParent);
    s_pServices->pWindowConstruct((uint8 *)pObject + 0x10e4);
    PTR(pObject, 0x10e4) = s_stTables.pProgress;
    s_pServices->pTextConstruct((uint8 *)pObject + 0x1124);
    PTR(pObject, 0) = s_stTables.pModeless;
    D32(pObject, 0x10dc) = 0;
    D32(pObject, 0x10e0) = 0;
    return pObject;
}

/* 0x223020d0: a CString, Quit, Help, message, OK, then checkbox. */
void *FS98_ALERTS_THISCALL fs98_alerts_advisory_construct(void *pObject, void *pParent)
{
    s_pServices->pDialogConstruct(pObject, 0x8c, pParent);
    s_pServices->pStringConstruct((uint8 *)pObject + 0x10e4);
    s_pServices->pButtonConstruct((uint8 *)pObject + 0x10e8);
    s_pServices->pButtonConstruct((uint8 *)pObject + 0x11e4);
    s_pServices->pTextConstruct((uint8 *)pObject + 0x12e0);
    s_pServices->pButtonConstruct((uint8 *)pObject + 0x1398);
    s_pServices->pCheckConstruct((uint8 *)pObject + 0x1494);
    PTR(pObject, 0) = s_stTables.pAdvisory;
    D32(pObject, 0x10dc) = 0;
    D32(pObject, 0x10e0) = 0;
    return pObject;
}

/* 0x223013d0 / 0x22301b00 / 0x223021b0: the derived vtable is not reset on
 * entry. This can matter to callbacks made by the dependency destructors. */
void FS98_ALERTS_THISCALL fs98_alerts_modal_destroy(void *pObject)
{
    uint32 uiIndex = 3;
    while (uiIndex--)
        s_pServices->pButtonDestroy((uint8 *)pObject + 0x11b4 + uiIndex * 0xfc);
    s_pServices->pTextDestroy((uint8 *)pObject + 0x10fc);
    s_pServices->pStringDestroy((uint8 *)pObject + 0x10f0);
    s_pServices->pDialogDestroy(pObject);
}

void FS98_ALERTS_THISCALL fs98_alerts_modeless_destroy(void *pObject)
{
    s_pServices->pTextDestroy((uint8 *)pObject + 0x1124);
    s_pServices->pWindowDestroy((uint8 *)pObject + 0x10e4);
    s_pServices->pDialogDestroy(pObject);
}

void FS98_ALERTS_THISCALL fs98_alerts_advisory_destroy(void *pObject)
{
    s_pServices->pCheckDestroy((uint8 *)pObject + 0x1494);
    s_pServices->pButtonDestroy((uint8 *)pObject + 0x1398);
    s_pServices->pTextDestroy((uint8 *)pObject + 0x12e0);
    s_pServices->pButtonDestroy((uint8 *)pObject + 0x11e4);
    s_pServices->pButtonDestroy((uint8 *)pObject + 0x10e8);
    s_pServices->pStringDestroy((uint8 *)pObject + 0x10e4);
    s_pServices->pDialogDestroy(pObject);
}

/* Scalar deleting destructors: only low-byte bit 0 controls deallocation. */
void *FS98_ALERTS_THISCALL fs98_alerts_modal_delete(void *pObject, uint32 uiFlags)
{
    fs98_alerts_modal_destroy(pObject);
    if (uiFlags & 1)
        s_pServices->pUiDelete(pObject);
    return pObject;
}

void *FS98_ALERTS_THISCALL fs98_alerts_modeless_delete(void *pObject, uint32 uiFlags)
{
    fs98_alerts_modeless_destroy(pObject);
    if (uiFlags & 1)
        s_pServices->pUiDelete(pObject);
    return pObject;
}

void *FS98_ALERTS_THISCALL fs98_alerts_advisory_delete(void *pObject, uint32 uiFlags)
{
    fs98_alerts_advisory_destroy(pObject);
    if (uiFlags & 1)
        s_pServices->pUiDelete(pObject);
    return pObject;
}

void *FS98_ALERTS_THISCALL fs98_alerts_progress_delete(void *pObject, uint32 uiFlags)
{
    s_pServices->pWindowDestroy(pObject);
    if (uiFlags & 1)
        s_pServices->pMfcDelete(pObject);
    return pObject;
}

void FS98_ALERTS_THISCALL fs98_alerts_post_destroy(void *pObject)
{
    typedef void *(FS98_ALERTS_THISCALL *fs98_delete_fn)(void *pThis, uint32 uiFlags);
    if (pObject) {
        fs98_delete_fn *ppVtable = PTR(pObject, 0);
        ppVtable[1](pObject, 1);
    }
}

void FS98_ALERTS_THISCALL fs98_alerts_disable(void *pObject)
{
    s_pServices->pEnable(D32(pObject, 0x20), 0);
}

void FS98_ALERTS_THISCALL fs98_alerts_enable(void *pObject)
{
    s_pServices->pEnable(D32(pObject, 0x20), 1);
}

void FS98_ALERTS_THISCALL fs98_alerts_empty_arg(void *pObject, uint32 uiArgument)
{
    (void)pObject;
    (void)uiArgument;
}

typedef void *(FS98_ALERTS_THISCALL *fs98_construct_fn)(void *pObject, void *pParent);

static uint32 fs98_alerts_create(void *pDescriptor, uint32 uiSize,
                                 uint32 uiTemplate, fs98_construct_fn pConstruct)
{
    void *pObject = s_pServices->pAllocate(uiSize);
    if (pObject)
        pObject = pConstruct(pObject, 0);
    /* Retail deliberately continues through NULL on allocation failure. */
    *(void *volatile *)((uint8 *)pObject + 0x10dc) = pDescriptor;
    D32(pObject, 0x10e0) = s_uiModule;
    s_pServices->pCreate(pObject, uiTemplate, 0);
    s_pServices->pShow(pObject, 1);
    return D32(pObject, 0x20);
}

uint32 FS98_ALERTS_STDCALL fs98_alerts_modal_create(uint32 uiParent, void *pDescriptor)
{
    (void)uiParent;
    return fs98_alerts_create(pDescriptor, 0x14a8, 0x8a, fs98_alerts_modal_construct);
}

uint32 FS98_ALERTS_STDCALL fs98_alerts_modeless_create(uint32 uiParent, void *pDescriptor)
{
    (void)uiParent;
    return fs98_alerts_create(pDescriptor, 0x11dc, 0x8b, fs98_alerts_modeless_construct);
}

uint32 FS98_ALERTS_STDCALL fs98_alerts_advisory_create(uint32 uiParent, void *pDescriptor)
{
    (void)uiParent;
    return fs98_alerts_create(pDescriptor, 0x155c, 0x8c, fs98_alerts_advisory_construct);
}
