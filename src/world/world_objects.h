#ifndef FS98_WORLD_OBJECTS_H
#define FS98_WORLD_OBJECTS_H
#include "world_internal.h"
struct fs98_world_control { uint32 uiOffset, uiConstructor, uiDestructor; };
struct fs98_world_object_services {
    void *(*pfnBaseConstruct)(void *pObject, uint32 uiTemplate, void *pParent);
    void (*pfnConstruct)(void *pObject, uint32 uiImport);
    void (*pfnDestroy)(void *pObject, uint32 uiImport);
    void *(*pfnAllocate)(uint32 uiBytes);
    void (*pfnUiDelete)(void *pObject);
    void (*pfnDialogDelete)(void *pObject);
    int (*pfnCreate)(void *pObject, uint32 uiTemplate, void *pParent);
    int (*pfnShow)(void *pObject, int iShow);
    int (*pfnMessage)(const char *pcText, uint32 uiFlags, uint32 uiHelp);
    void (*pfnDdx)(void *pExchange, uint32 uiId, void *pControl);
    void (*pfnExitCleanly)(void *pException);
    int (*pfnHelp)(uint32 uiWindow, const char *pcFile, uint32 uiCommand, const uint32 *puiMap);
};
void fs98_world_set_object_services(const struct fs98_world_object_services *pServices, void *pVtable);
void fs98_world_set_module(uint32 uiModule);
void *fs98_world_area_construct(void *pObject, void *pParent);
void fs98_world_area_destroy(void *pObject);
void *fs98_world_area_delete(void *pObject, uint32 uiFlags);
void fs98_world_area_exchange(void *pObject, void *pExchange);
int fs98_world_area_help(void *pObject, uint32 uiUnused, void *pHelp);
int fs98_world_area_context(void *pObject, uint32 uiWindow, uint32 uiUnused);
uint32 FS98_WORLD_STDCALL fs98_world_area_create_add(uint32 uiParent, void *pDescriptor);
uint32 FS98_WORLD_STDCALL fs98_world_area_create_edit(uint32 uiParent, void *pDescriptor);
#endif
