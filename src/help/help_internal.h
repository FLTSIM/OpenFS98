#ifndef FS98_HELP_INTERNAL_H
#define FS98_HELP_INTERNAL_H

#include "types.h"
#define FS98_HELP_THISCALL __attribute__((thiscall))
#define FS98_HELP_STDCALL __attribute__((stdcall))

/* Descriptor callback used by About: MAIN 0x207e10f0 ends in ret 4.
 * The unused graph descriptor's callback consumes twelve bytes at its calls;
 * no caller of that dialog id exists in the reference module set. */
typedef int32 (FS98_HELP_STDCALL *fs98_help_about_fn)(void *pDescriptor);
typedef int32 (FS98_HELP_STDCALL *fs98_help_graph_fn)(uint32 uiOperation,
                                                     void *pDescriptor, uint32 uiWindow);
struct fs98_help_rect { int32 iLeft, iTop, iRight, iBottom; };
struct fs98_help_services {
    int32 (*pSendMessage)(uint32 uiWindow, uint32 uiMessage, uint32 uiWParam, uint32 uiLParam);
    int32 (*pWinHelp)(uint32 uiWindow, const char *pFile, uint32 uiCommand, const uint32 *pTable);
    int32 (*pEnableWindow)(uint32 uiWindow, int32 iEnable);
    int32 (*pKillTimer)(uint32 uiWindow, uint32 uiTimer);
    uint32 (*pSetTimer)(uint32 uiWindow, uint32 uiTimer, uint32 uiPeriod, void *pCallback);
    int32 (*pDestroyWindow)(uint32 uiWindow);
    int32 (*pFreeLibrary)(uint32 uiModule);
    void (*pClose)(void *pObject);
    int32 (*pMessageBox)(const char *pText, uint32 uiType, uint32 uiHelp);
    int32 (*pGetWindowLong)(uint32 uiWindow, int32 iIndex);
    int32 (*pSetChecked)(void *pControl, int32 iChecked);
    void (*pSetText)(void *pControl, const char *pText);
    int32 (*pEnableControl)(void *pControl, int32 iEnabled);
    int32 (*pLoadHostString)(uint32 uiId, char *pBuffer, int32 iCapacity);
    int32 (*pSplitPath)(const char *pPath, char *pDrive, char *pDir, char *pName, int32 *pExt);
    int32 (*pJoinPath)(char *pOut, const char *pDrive, const char *pDir, const char *pName);
    int32 (*pExists)(const char *pPath, int32 iExtension);
    void (*pBaseExchange)(void *pObject, void *pExchange);
    void (*pDdxControl)(void *pExchange, uint32 uiId, void *pControl);
    int32 (*pReadFile)(uint32 uiFile, void *pBuffer, uint32 uiCount, uint32 *pRead, void *pOverlapped);
    char *(*pCopyText)(char *pOut, const char *pText);
    int32 (*pOpenFile)(const char *pName, void *pInfo, uint32 uiMode);
    int32 (*pCloseHandle)(uint32 uiHandle);
    int32 (*pBaseInit)(void *pObject);
    int32 (*pDialogInit)(void *pObject);
    int32 (*pInitArt)(void *pControl, int32 iBitmap, void *pDib, void *pPalette, uint32 uiFlags);
    int32 (*pInitButton)(void *pControl, void *pFont, void *pAlternateFont, uint32 uiBitmap,
                         void *pDib, void *pPalette, uint32 uiFlags, uint32 uiColor,
                         uint32 uiModule, int32 iX, int32 iY, struct fs98_help_rect oRect);
    void (*pInitText)(void *pControl, void *pFont);
    void *(*pGetControl)(void *pObject, uint32 uiId);
    void (*pRemapRect)(struct fs98_help_rect *pRect, void *pControl);
    void (*pInitGroup)(void *pControl, void *pFont);
    void *(*pBaseFont)(int32 iId);
    void (*pInitList)(void *pControl, void *pFont, void *pLabel);
    void (*pInitEdit)(void *pControl, void *pFont, uint32 uiForeground, uint32 uiBackground,
                      uint32 uiErrorColor, void *pLabel, uint32 uiFlags);
    int32 (*pCopyRect)(struct fs98_help_rect *pOut, const struct fs98_help_rect *pIn);
    void (*pCommonControls)(void);
    uint32 (*pLoadLibrary)(const char *pName);
    void *(*pModuleState)(void);
    uint32 (*pCreateWindow)(uint32 uiExStyle, const char *pClass, const char *pTitle, uint32 uiStyle,
                            int32 iX, int32 iY, int32 iWidth, int32 iHeight, uint32 uiParent,
                            uint32 uiMenu, uint32 uiInstance, void *pParam);
    void (*pMoveControl)(void *pControl, int32 iX, int32 iY, int32 iWidth, int32 iHeight, int32 iRepaint);
};

void fs98_help_set_resource_module(uint32 uiModule);
int32 FS98_HELP_THISCALL fs98_help_lessons_init(void *pObject);
int32 FS98_HELP_THISCALL fs98_help_credits_init(void *pObject);
int32 FS98_HELP_THISCALL fs98_help_graph_init(void *pObject);
int32 FS98_HELP_THISCALL fs98_help_completion_init(void *pObject);
void fs98_help_set_services(const struct fs98_help_services *pServices);
#define FS98_HELP_EVENT(szName) void FS98_HELP_THISCALL fs98_help_##szName(void *pObject)
FS98_HELP_EVENT(about_cancel);
FS98_HELP_EVENT(about_accept);
FS98_HELP_EVENT(about_credits);
FS98_HELP_EVENT(credits_cancel);
FS98_HELP_EVENT(credits_accept);
FS98_HELP_EVENT(credits_close);
FS98_HELP_EVENT(credits_toggle);
FS98_HELP_EVENT(credits_load);
FS98_HELP_EVENT(lessons_cancel);
FS98_HELP_EVENT(lessons_accept);
FS98_HELP_EVENT(lessons_sync);
FS98_HELP_EVENT(lessons_populate);
FS98_HELP_EVENT(lessons_select);
FS98_HELP_EVENT(lessons_double_click);
FS98_HELP_EVENT(graph_accept);
FS98_HELP_EVENT(graph_notify);
FS98_HELP_EVENT(completion_accept);
FS98_HELP_EVENT(completion_cancel);
FS98_HELP_EVENT(post_destroy);
FS98_HELP_EVENT(disable);
FS98_HELP_EVENT(enable);
FS98_HELP_EVENT(empty);
#undef FS98_HELP_EVENT
int32 FS98_HELP_THISCALL fs98_help_credits_start_timer(void *pObject);
void FS98_HELP_THISCALL fs98_help_credits_timer(void *pObject, uint32 uiTimer);
void FS98_HELP_THISCALL fs98_help_empty_arg(void *pObject, uint32 uiUnused);
int32 FS98_HELP_THISCALL fs98_help_graph_tick(void *pObject, uint32 uiWParam, uint32 uiLParam);
int32 FS98_HELP_THISCALL fs98_help_graph_validate(void *pObject);
int32 FS98_HELP_THISCALL fs98_help_lessons_validate(void *pObject);
int32 FS98_HELP_THISCALL fs98_help_lessons_no_error(void *pObject);
uint32 FS98_HELP_STDCALL fs98_help_credits_read(uint32 uiCookie, void *pBuffer,
                                               int32 iCount, uint32 *pRead);
#define FS98_HELP_CLASS(szName) \
    int32 FS98_HELP_THISCALL fs98_help_##szName##_help(void *pObject, uint32 uiUnused, void *pInfo); \
    int32 FS98_HELP_THISCALL fs98_help_##szName##_context(void *pObject, uint32 uiWindow, uint32 uiUnused); \
    void FS98_HELP_THISCALL fs98_help_##szName##_exchange(void *pObject, void *pExchange)
FS98_HELP_CLASS(about);
FS98_HELP_CLASS(credits);
FS98_HELP_CLASS(lessons);
FS98_HELP_CLASS(graph);
FS98_HELP_CLASS(completion);
#undef FS98_HELP_CLASS
#endif
