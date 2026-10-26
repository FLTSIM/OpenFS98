#ifndef FS98_WORLD_INIT_H
#define FS98_WORLD_INIT_H
#include "world_internal.h"
struct fs98_world_rect { int iLeft, iTop, iRight, iBottom; };
struct fs98_world_init_services {
    int (*pfnBaseInit)(void *pObject);
    void (*pfnStringConstruct)(void *pString);
    char *(*pfnStringBuffer)(void *pString, int iMinimum);
    void (*pfnStringRelease)(void *pString, int iLength);
    void (*pfnStringDestroy)(void *pString);
    int (*pfnLoadString)(uint32 uiModule, uint32 uiId, char *pcBuffer, int iCapacity);
    int (*pfnArt)(void *pControl, uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiFlags);
    int (*pfnButton)(void *pControl, void *pFont, void *pAlternate, uint32 uiBitmap, void *pDib,
        void *pPalette, uint32 uiFlags, int iColor, uint32 uiModule, int iX, int iY, struct fs98_world_rect stRect);
    void (*pfnText)(void *pControl, void *pFont);
    void (*pfnCombo)(void *pControl, void *pFont, void *pLabel);
    void *(*pfnBaseFont)(int iFont);
    void (*pfnEdit)(void *pControl, void *pFont, uint32 uiForeground, uint32 uiBackground, uint32 uiError, void *pLabel, uint32 uiFlags);
    void (*pfnTitle)(void *pControl, const char *pcText);
};
const struct fs98_world_services *fs98_world_get_services(void);
void fs98_world_set_init_services(const struct fs98_world_init_services *pServices);
void fs98_world_set_resource_module(uint32 uiModule);
int fs98_world_area_init(void *pObject);
#endif
