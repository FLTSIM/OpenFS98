#ifndef FS98_ALERTS_INTERNAL_H
#define FS98_ALERTS_INTERNAL_H

#include "types.h"

#if defined(__i386__)
#define FS98_ALERTS_THISCALL __attribute__((thiscall))
#else
#define FS98_ALERTS_THISCALL
#endif

struct fs98_alerts_point { int32 iX, iY; };
struct fs98_alerts_rect { int32 iLeft, iTop, iRight, iBottom; };
struct fs98_alerts_size { int32 iWidth, iHeight; };

/* Dependency boundary for the progress handler. The retail-facing adapter
 * remains to be supplied with the complete dialog implementation. Locals
 * passed as MFC objects retain their retail storage sizes and field offsets. */
struct fs98_alerts_services {
    void (*pStringConstruct)(void *pString);
    void (*pStringDestroy)(void *pString);
    void (*pDcConstruct)(void *pDc, void *pWindow);
    void (*pDcDestroy)(void *pDc);
    uint32 (*pStockObject)(int32 iObject);
    void *(*pFontFromHandle)(uint32 uiFont);
    void *(*pSelectFont)(void *pDc, void *pFont);
    int32 (*pClientRect)(uint32 uiWindow, struct fs98_alerts_rect *pRect);
    int32 (*pClientToScreen)(uint32 uiWindow, struct fs98_alerts_point *pPoint);
    int32 (*pScreenToClient)(uint32 uiWindow, struct fs98_alerts_point *pPoint);
    void (*pFormat)(void *pString, const char *pszFormat, int32 iValue);
    int32 (*pTextExtent)(uint32 uiDc, const char *pszText, int32 iLength,
                        struct fs98_alerts_size *pSize);
    uint32 (*pTextColor)(void *pDc, uint32 uiColor);
    uint32 (*pBackgroundColor)(void *pDc, uint32 uiColor);
    int32 (*pTextOut)(uint32 uiDc, int32 iX, int32 iY, uint32 uiOptions,
                     const struct fs98_alerts_rect *pRect, const char *pszText,
                     uint32 uiLength, const int32 *pSpacing);
    int32 (*pWinHelp)(uint32 uiWindow, const char *pszFile, uint32 uiCommand,
                     uint32 uiContext);
    void (*pBaseExchange)(void *pObject, void *pExchange);
    void (*pDdxControl)(void *pExchange, uint32 uiControl, void *pControl);
    void (*pDibConstruct)(void *pDib);
    void (*pDibDestroy)(void *pDib);
    int32 (*pBaseInit)(void *pObject);
    int32 (*pLoadString)(uint32 uiId, char *pszBuffer, uint32 uiCapacity);
    void (*pSetText)(void *pControl, const char *pszText);
    void (*pInitText)(void *pControl, void *pFont);
    int32 (*pLoadDib)(void *pDib, int32 iId, uint32 uiModule, void *pPalette, uint32 uiFlags);
    int32 (*pSpriteCopy)(void *pDib, void *pDestination, int32 iX, int32 iY,
                         int32 iWidth, int32 iHeight, int32 iSourceX, int32 iSourceY,
                         uint32 uiRaster);
    int32 (*pShowWindow)(void *pControl, int32 iShow);
    int32 (*pWindowLoadString)(uint32 uiModule, uint32 uiId, char *pszBuffer, int32 iCapacity);
    void (*pButtonText)(void *pControl, const char *pszText);
    int32 (*pButtonInit)(void *pControl, void *pFont, void *pAlternateFont, uint32 uiBitmap,
                         void *pDib, void *pPalette, uint32 uiFlags, uint32 uiColor,
                         uint32 uiModule, int32 iX, int32 iY, struct fs98_alerts_rect stRect);
    int32 (*pSetChecked)(void *pControl, int32 iChecked);
};

void fs98_alerts_set_services(const struct fs98_alerts_services *pServices);
void fs98_alerts_set_resource_module(uint32 uiModule);
void FS98_ALERTS_THISCALL fs98_alerts_modal_first(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_modal_second(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_modal_third(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_accept(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_quit(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_help(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_noop(void *pObject);
void FS98_ALERTS_THISCALL fs98_alerts_modal_exchange(void *pObject, void *pExchange);
void FS98_ALERTS_THISCALL fs98_alerts_modeless_exchange(void *pObject, void *pExchange);
void FS98_ALERTS_THISCALL fs98_alerts_advisory_exchange(void *pObject, void *pExchange);
int32 FS98_ALERTS_THISCALL fs98_alerts_modeless_init(void *pObject);
int32 FS98_ALERTS_THISCALL fs98_alerts_modal_init(void *pObject);
int32 FS98_ALERTS_THISCALL fs98_alerts_advisory_init(void *pObject);
int32 FS98_ALERTS_THISCALL fs98_alerts_progress_tick(void *pObject,
                                                   uint32 uiWParam, uint32 uiLParam);

#endif
