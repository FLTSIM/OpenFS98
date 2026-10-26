#ifndef FS98_WORLD_INTERNAL_H
#define FS98_WORLD_INTERNAL_H
#include "types.h"
#define FS98_WORLD_THISCALL __attribute__((thiscall))
#define FS98_WORLD_STDCALL __attribute__((stdcall))
#define WORLD_U32(pVal, uiOffset) (*(uint32 *)((uint8 *)(pVal) + (uiOffset)))
#define WORLD_PTR(pVal, uiOffset) (*(void **)((uint8 *)(pVal) + (uiOffset)))
/* Area-dialog core; the native adapter registers only area add/edit. */
struct fs98_world_services {
    int (*pfnGetText)(void *pControl, char *pcText, int iCapacity);
    void (*pfnSetText)(void *pControl, const char *pcText);
    int (*pfnParseInt)(const char *pcText);
    int (*pfnFormatInt)(char *pcText, const char *pcFormat, int iValue);
    int (*pfnSend)(uint32 uiWindow, uint32 uiMessage, uint32 uiFirst, uint32 uiSecond);
    int (*pfnCheck)(void *pControl, int iChecked);
    int (*pfnEnable)(void *pControl, int iEnabled);
    int (*pfnError)(void *pObject, void *pControl, void *pStatus);
    void (*pfnFocusControl)(void *pControl);
};
void fs98_world_set_services(const struct fs98_world_services *pServices);
void fs98_world_area_collect(void *pObject);
void fs98_world_area_populate(void *pObject);
void fs98_world_area_accept(void *pObject);
void fs98_world_area_cancel(void *pObject);
int fs98_world_area_validate(void *pObject, uint32 uiUnused);
int fs98_world_area_error(void *pObject, uint32 uiUnused);
void fs98_world_area_path_changed(void *pObject);
#endif
