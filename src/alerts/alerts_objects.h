#ifndef FS98_ALERTS_OBJECTS_H
#define FS98_ALERTS_OBJECTS_H

#include "alerts_internal.h"

#define FS98_ALERTS_STDCALL __attribute__((stdcall))

/* Derived class identities are supplied by the runtime adapter. The object
 * harness pairs these identities with retail; no original code is used by
 * the C constructors or destructors. */
struct fs98_alerts_object_tables {
    void *pModal;
    void *pModeless;
    void *pProgress;
    void *pAdvisory;
};

struct fs98_alerts_object_services {
    void *(*pDialogConstruct)(void *pObject, int32 iTemplate, void *pParent);
    void (*pDialogDestroy)(void *pObject);
    void (*pStringConstruct)(void *pObject);
    void (*pStringDestroy)(void *pObject);
    void (*pTextConstruct)(void *pObject);
    void (*pTextDestroy)(void *pObject);
    void (*pButtonConstruct)(void *pObject);
    void (*pButtonDestroy)(void *pObject);
    void (*pWindowConstruct)(void *pObject);
    void (*pWindowDestroy)(void *pObject);
    void (*pCheckConstruct)(void *pObject);
    void (*pCheckDestroy)(void *pObject);
    void *(*pAllocate)(uint32 uiSize);
    void (*pUiDelete)(void *pObject);
    void (*pMfcDelete)(void *pObject);
    int32 (*pCreate)(void *pObject, uint32 uiTemplate, void *pParent);
    int32 (*pShow)(void *pObject, int32 iShow);
    int32 (*pEnable)(uint32 uiWindow, int32 iEnable);
};

void fs98_alerts_set_object_services(const struct fs98_alerts_object_services *pServices,
                                    const struct fs98_alerts_object_tables *pTables);
void fs98_alerts_set_object_module(uint32 uiModule);
void *FS98_ALERTS_THISCALL fs98_alerts_modal_construct(void *pObject, void *pParent);
void *FS98_ALERTS_THISCALL fs98_alerts_modeless_construct(void *pObject, void *pParent);
void *FS98_ALERTS_THISCALL fs98_alerts_advisory_construct(void *pObject, void *pParent);
void FS98_ALERTS_THISCALL fs98_alerts_modal_destroy(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_modeless_destroy(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_destroy(void *pObject);
void *FS98_ALERTS_THISCALL fs98_alerts_modal_delete(void *pObject, uint32 uiFlags);
void *FS98_ALERTS_THISCALL fs98_alerts_modeless_delete(void *pObject, uint32 uiFlags);
void *FS98_ALERTS_THISCALL fs98_alerts_advisory_delete(void *pObject, uint32 uiFlags);
void *FS98_ALERTS_THISCALL fs98_alerts_progress_delete(void *pObject, uint32 uiFlags);
void FS98_ALERTS_THISCALL fs98_alerts_post_destroy(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_disable(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_enable(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_empty_arg(void *pObject, uint32 uiArgument);
uint32 FS98_ALERTS_STDCALL fs98_alerts_modal_create(uint32 uiParent, void *pDescriptor);
uint32 FS98_ALERTS_STDCALL fs98_alerts_modeless_create(uint32 uiParent, void *pDescriptor);
uint32 FS98_ALERTS_STDCALL fs98_alerts_advisory_create(uint32 uiParent, void *pDescriptor);

#endif
