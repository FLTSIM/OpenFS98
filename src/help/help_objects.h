#ifndef FS98_HELP_OBJECTS_H
#define FS98_HELP_OBJECTS_H
#include "help_internal.h"

struct fs98_help_subobject { uint16 unOffset, unConstructor, unDestructor; };
struct fs98_help_class {
    uint16 unSize, unTemplate, unDescriptor;
    uint16 unConstructor, unDestructor, unDelete, unFactory, unVtable;
    uint16 unCount;
    const struct fs98_help_subobject *pControls;
};
struct fs98_help_object_services {
    void *(*pDialogConstruct)(void *pObject, int32 iResource, void *pParent);
    void (*pDialogDestroy)(void *pObject);
    void (*pConstruct)(void *pControl, uint32 uiImport);
    void (*pDestroy)(void *pControl, uint32 uiImport);
    void *(*pAllocate)(uint32 uiSize);
    void (*pUiDelete)(void *pObject);
    void (*pMfcDelete)(void *pObject);
    void (*pDialogDelete)(void *pObject);
    int32 (*pCreate)(void *pObject, uint32 uiResource, void *pParent);
    int32 (*pShow)(void *pObject, int32 iShow);
};
void fs98_help_set_object_services(const struct fs98_help_object_services *pServices,
                                   void *const *ppVtables, void *pPlaceholderVtable);
void fs98_help_set_object_module(uint32 uiModule);
#define FS98_HELP_OBJECT(szName) \
    void *FS98_HELP_THISCALL fs98_help_##szName##_construct(void *pObject, void *pParent); \
    void FS98_HELP_THISCALL fs98_help_##szName##_destroy(void *pObject); \
    void *FS98_HELP_THISCALL fs98_help_##szName##_delete(void *pObject, uint32 uiFlags); \
    uint32 FS98_HELP_STDCALL fs98_help_##szName##_create(uint32 uiParent, void *pDescriptor)
FS98_HELP_OBJECT(about);
FS98_HELP_OBJECT(credits);
FS98_HELP_OBJECT(lessons);
FS98_HELP_OBJECT(graph);
FS98_HELP_OBJECT(completion);
#undef FS98_HELP_OBJECT
void *FS98_HELP_THISCALL fs98_help_placeholder_delete(void *pObject, uint32 uiFlags);
void FS98_HELP_STDCALL fs98_help_class_delete(void *pObject);
#endif
