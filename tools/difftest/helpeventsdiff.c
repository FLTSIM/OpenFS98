/* Help's event boundary, original PE32 routines versus C. Every operation
 * starts from the same guarded fixture, including side-effecting imports. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/help/help_internal.h"
#include "../../src/help/help_objects.h"
#include "../../src/help/help_objects_tables.inc"

#define TC FS98_HELP_THISCALL
#define SC FS98_HELP_STDCALL
#define D32(pObject, uiOffset) (*(uint32 *)((uint8 *)(pObject) + (uiOffset)))
#define PTR(pObject, uiOffset) (*(void **)((uint8 *)(pObject) + (uiOffset)))
#define TEST_THROW 0x0e041a22
#define LOG_SIZE 32768
struct fixture {
    uint8 abyGuardBefore[32];
    uint8 abyObject[0x197c];
    uint8 aabyDescriptor[2][0x30];
    uint8 aabyRecords[4][0xa08];
    uint8 abyHelp[32];
    uint8 abyExchange[32];
    uint8 abyBuffer[64];
    uint32 uiRead;
    struct fs98_help_about_state oAboutState;
    uint8 abyGuardAfter[32];
};
static struct fixture s_oFixture, s_oInitial, s_oRetailResult;
static HMODULE s_hRetail;
static uint32 s_uiSeed, s_uiRng, s_uiScenario, s_uiOperation, s_uiScript;
static uint32 s_uiCalls, s_uiLogLength, s_uiThrowAt, s_uiSavedSeh;
static uint32 s_uiMaxCalls, s_uiThrows, s_uiComparisons, s_uiSwaps, s_uiReads, s_uiResets, s_uiAllocationFaults;
static int s_iSide;
static char s_aacLogs[2][LOG_SIZE];
static void *s_apVtable[60], *s_apOtherVtable[60], *s_apControlVtable[60], *s_apJump[5];

typedef void (TC *event_fn)(void *);
struct operation { uint32 uiRva; event_fn pEvent; uint32 uiSignature; const char *pName; };
#define EVENT(uiRva, szName) {uiRva, fs98_help_##szName, 0, #szName}
#define SPECIAL(uiRva, uiSig, szName) {uiRva, (event_fn)0, uiSig, #szName}
#define CLASS(uiHelp, uiContext, uiDdx, szName) \
    SPECIAL(uiHelp, 1, szName##_help), SPECIAL(uiContext, 2, szName##_context), \
    SPECIAL(uiDdx, 3, szName##_exchange)
static const struct operation s_aOperations[] = {
    EVENT(0x18c0, about_cancel), EVENT(0x18e0, about_accept), EVENT(0x1900, about_credits),
    EVENT(0x1ef0, credits_close), EVENT(0x1f60, credits_cancel), EVENT(0x1f80, credits_accept),
    EVENT(0x2010, credits_load), SPECIAL(0x2130, 4, credits_start_timer),
    SPECIAL(0x2180, 5, credits_timer), EVENT(0x21f0, credits_toggle),
    EVENT(0x2770, lessons_cancel), EVENT(0x2790, lessons_accept), EVENT(0x2800, lessons_sync),
    EVENT(0x2870, lessons_populate), SPECIAL(0x2930, 4, lessons_no_error),
    SPECIAL(0x2940, 4, lessons_validate), EVENT(0x2950, lessons_select),
    EVENT(0x2a80, lessons_double_click), SPECIAL(0x2c20, 6, graph_tick),
    EVENT(0x2c50, graph_accept), EVENT(0x2de0, graph_notify), SPECIAL(0x2e00, 4, graph_validate),
    EVENT(0x3090, completion_accept), EVENT(0x30b0, completion_cancel),
    EVENT(0x2760, post_destroy), EVENT(0x1b50, disable), EVENT(0x1b60, enable),
    EVENT(0x1b40, empty), SPECIAL(0x2250, 5, empty_arg), SPECIAL(0x1fe0, 7, credits_read),
    CLASS(0x1910, 0x1930, 0x1620, about),
    CLASS(0x1fa0, 0x1fc0, 0x1ce0, credits),
    CLASS(0x2aa0, 0x2ac0, 0x2480, lessons),
    CLASS(0x2da0, 0x2dc0, 0x2be0, graph),
    CLASS(0x30d0, 0x30f0, 0x2f50, completion),
    SPECIAL(0x2ca0, 4, graph_init), SPECIAL(0x2fa0, 4, completion_init),
    SPECIAL(0x2570, 4, lessons_init), SPECIAL(0x1d50, 4, credits_init),
#define LIFETIME(uiCtor, uiDtor, uiDelete, uiFactory, szName) \
    SPECIAL(uiCtor, 8, szName##_construct), SPECIAL(uiDtor, 9, szName##_destroy), \
    SPECIAL(uiDelete, 10, szName##_delete), SPECIAL(uiFactory, 11, szName##_create), \
    SPECIAL(uiCtor, 12, szName##_roundtrip), SPECIAL(uiCtor, 13, szName##_construct_delete)
    LIFETIME(0x1410, 0x1530, 0x1510, 0x11c0, about),
    LIFETIME(0x1b70, 0x1c50, 0x1c30, 0x1250, credits),
    LIFETIME(0x2280, 0x2390, 0x2370, 0x1130, lessons),
    LIFETIME(0x2ae0, 0x2b70, 0x2b50, 0x12e0, graph),
    LIFETIME(0x2e30, 0x2ed0, 0x2eb0, 0x1370, completion),
    SPECIAL(0x2260, 14, placeholder_delete), SPECIAL(0x1400, 15, class_delete),
    SPECIAL(0x1950, 16, about_populate), SPECIAL(0x1710, 18, about_init),
    SPECIAL(0x18b0, 17, about_font_destroy), SPECIAL(0x1710, 19, about_init_twice)
#undef LIFETIME
};
#define OP_COUNT (sizeof(s_aOperations) / sizeof(s_aOperations[0]))
static uint32 next_random(void)
{ s_uiRng ^= s_uiRng << 13; s_uiRng ^= s_uiRng >> 17; s_uiRng ^= s_uiRng << 5; return s_uiRng; }
static uint32 offset(const void *pPointer)
{
    if (pPointer == (uint8 *)s_hRetail + 0x51c8) pPointer = s_oFixture.oAboutState.abyFont;
    return (uint32)((const uint8 *)pPointer - (const uint8 *)&s_oFixture); }
static void put(const char *pFormat, ...)
{
    va_list pArgs;
    int iLength;
    if (s_uiLogLength > LOG_SIZE - 2048) exit(2);
    va_start(pArgs, pFormat);
    iLength = vsnprintf(s_aacLogs[s_iSide] + s_uiLogLength, LOG_SIZE - s_uiLogLength, pFormat, pArgs);
    va_end(pArgs);
    if (iLength < 0 || (uint32)iLength >= LOG_SIZE - s_uiLogLength) exit(2);
    s_uiLogLength += (uint32)iLength;
    ++s_uiCalls;
    if (s_uiThrowAt && s_uiCalls == s_uiThrowAt) RaiseException(TEST_THROW, 0, 0, 0);
}
/* One boundary per operation can replace the descriptor/vtable, change list
 * count/selection and alter a handle. All choices remain valid fixtures. */
static void mutate(void)
{
    uint8 *pObject = s_oFixture.abyObject;
    uint32 uiDescriptorOffset;
    void *pDescriptor;
    if (s_uiCalls != 1 + (s_uiScenario % 4)) return;
    uiDescriptorOffset = s_uiOperation == 4 || s_uiOperation == 5 ? 0x1174 :
                         ((s_uiOperation >= 18 && s_uiOperation <= 21) || s_uiOperation == 45) ? 0x10e0 : 0x10dc;
    if (s_uiOperation == 3 || s_uiOperation == 6 || s_uiOperation == 7 ||
        s_uiOperation == 8 || s_uiOperation == 9 || s_uiOperation == 29 || s_uiOperation == 48) {
        if (s_uiScript & 1) D32(pObject, 0x1108) ^= 1;
        if (s_uiScript & 2) D32(pObject, 0x10ec) = s_uiSeed;
    } else {
        pDescriptor = PTR(pObject, uiDescriptorOffset);
        if ((s_uiScript & 1) && (pDescriptor == s_oFixture.aabyDescriptor[0] ||
                                pDescriptor == s_oFixture.aabyDescriptor[1])) {
            PTR(pObject, uiDescriptorOffset) = pDescriptor == s_oFixture.aabyDescriptor[0] ?
                s_oFixture.aabyDescriptor[1] : s_oFixture.aabyDescriptor[0];
            if (!s_iSide) ++s_uiSwaps;
        }
        if ((s_uiScript & 2) && ((s_uiOperation >= 10 && s_uiOperation <= 17) || s_uiOperation == 47)) {
            pDescriptor = PTR(pObject, 0x10dc);
            if (pDescriptor) {
                D32(pDescriptor, 8) = 2;
                D32(pDescriptor, 0xc) = 1;
            }
        }
    }
    if (s_uiScript & 4) PTR(pObject, 0) = s_apOtherVtable;
    if (s_uiScript & 8) D32(pObject, 0x17a8) = s_uiSeed ^ 0x12345678;
}
static void TC virtual_close(void *pObject)
{
    put("virtual.close object=%u state=%u/%u/%u/%u desc=%u/%u\n", offset(pObject),
        D32(pObject, 0x1978), D32(pObject, 0x1498), D32(pObject, 0x187c), D32(pObject, 0x12cc),
        D32(s_oFixture.aabyDescriptor[0], 0), D32(s_oFixture.aabyDescriptor[1], 0));
    mutate();
}
static void TC virtual_close_other(void *pObject) { put("other.close\n"); virtual_close(pObject); }
static void TC virtual_accept(void *pObject) { put("virtual.accept %u\n", offset(pObject)); mutate(); }
static void TC virtual_accept_other(void *pObject) { put("other.accept\n"); virtual_accept(pObject); }
static void TC virtual_cancel(void *pObject) { put("virtual.cancel %u\n", offset(pObject)); mutate(); }
static void TC virtual_cancel_other(void *pObject) { put("other.cancel\n"); virtual_cancel(pObject); }
static void *TC virtual_delete(void *pObject, uint32 uiFlags)
{ put("virtual.delete %u flags=%u\n", offset(pObject), uiFlags); mutate(); return pObject; }
static int32 SC about_callback(void *pDescriptor)
{ put("about.callback %u\n", offset(pDescriptor)); mutate(); return (int32)s_uiSeed; }
static int32 SC graph_callback(uint32 uiOperation, void *pDescriptor, uint32 uiWindow)
{
    put("graph.callback op=%u desc=%u hwnd=%u state=%u answer=%u\n", uiOperation,
        offset(pDescriptor), uiWindow, D32(s_oFixture.abyObject, 0x10e4), D32(pDescriptor, 0));
    mutate(); return (int32)s_uiSeed;
}
static int32 send_message(uint32 uiWindow, uint32 uiMessage, uint32 uiWParam, uint32 uiLParam)
{
    uint32 uiResult = s_uiSeed;
    if (uiMessage == 0x180) put("message %u %x %u text=%s\n", uiWindow, uiMessage, uiWParam, (char *)uiLParam);
    else if (uiMessage == 0x449) {
        uint32 uiRead = 0xdeadbeef;
        uint8 abyBuffer[32];
        put("stream %u %x %u ptr=%u file=%u error=%u callback=%u\n", uiWindow, uiMessage,
            uiWParam, offset((void *)uiLParam), D32((void *)uiLParam, 0), D32((void *)uiLParam, 4),
            PTR((void *)uiLParam, 8) == (s_iSide ? (void *)fs98_help_credits_read : (uint8 *)s_hRetail + 0x1fe0));
        mutate();
        memset(abyBuffer, 0xcd, sizeof(abyBuffer));
        uiResult = ((uint32 (SC *)(uint32, void *, int32, uint32 *))PTR((void *)uiLParam, 8))(
            D32((void *)uiLParam, 0), abyBuffer, sizeof(abyBuffer), &uiRead);
        put("stream.result %u read=%u byte=%u/%u\n", uiResult, uiRead, abyBuffer[0], abyBuffer[31]);
        return (int32)uiResult;
    } else put("message %u %x %u %u\n", uiWindow, uiMessage, uiWParam, uiLParam);
    mutate();
    if (uiMessage == 0x188) uiResult = s_uiScenario % 5 == 0 ? 0xffffffff : (s_uiScenario + s_uiCalls) % 4;
    if (uiMessage == 0xce) {
        static const uint32 auiLines[] = {0x80000000, 0xffffffff, 0, 1, 6, 0x7fffffff};
        uiResult = auiLines[s_uiScenario % 6];
    }
    if (uiMessage == 0x115 && uiWParam == 6 && !s_iSide) ++s_uiResets;
    return (int32)uiResult;
}
static int32 win_help(uint32 uiWindow, const char *pFile, uint32 uiCommand, const uint32 *pTable)
{
    uint32 uiIndex = 0;
    char acPairs[1024];
    int iLength = 0;
    do {
        if (uiIndex >= 128 || iLength > 900) exit(2);
        iLength += snprintf(acPairs + iLength, sizeof(acPairs) - iLength, "%x:%x,", pTable[uiIndex], pTable[uiIndex + 1]);
        uiIndex += 2;
    } while (pTable[uiIndex - 2] || pTable[uiIndex - 1]);
    put("help %u %s %u [%s]\n", uiWindow, pFile, uiCommand, acPairs);
    mutate(); return (int32)s_uiSeed;
}
static int32 enable_window(uint32 uiWindow, int32 iEnable)
{ put("enable.window %u %d\n", uiWindow, iEnable); mutate(); return (int32)s_uiSeed; }
static int32 kill_timer(uint32 uiWindow, uint32 uiTimer)
{ put("kill.timer %u %u state=%u\n", uiWindow, uiTimer, D32(s_oFixture.abyObject, 0x10e8)); mutate(); return (int32)s_uiSeed; }
static uint32 set_timer(uint32 uiWindow, uint32 uiTimer, uint32 uiPeriod, void *pCallback)
{
    put("set.timer %u %u %u callback=%u old=%u\n", uiWindow, uiTimer, uiPeriod,
        (uint32)pCallback, D32(s_oFixture.abyObject, 0x10e8));
    mutate();
    return uiTimer >= 3 + s_uiScenario % 14 ? s_uiSeed | 1 : 0;
}
static int32 destroy_window(uint32 uiWindow)
{ put("destroy.window %u timer=%u\n", uiWindow, D32(s_oFixture.abyObject, 0x10e8)); mutate(); return (int32)s_uiSeed; }
static int32 free_library(uint32 uiModule)
{ put("free.library %u rich=%u\n", uiModule, D32(s_oFixture.abyObject, 0x10ec)); mutate(); return (int32)s_uiSeed; }
static void close_dialog(void *pObject) { put("base.close %u\n", offset(pObject)); mutate(); }
static int32 message_box(const char *pText, uint32 uiType, uint32 uiHelp)
{ put("message.box [%s] %u %u\n", pText, uiType, uiHelp); mutate(); return (int32)s_uiSeed; }
static int32 get_window_long(uint32 uiWindow, int32 iIndex)
{ put("get.window.long %u %d\n", uiWindow, iIndex); mutate(); return s_uiScenario & 1 ? (int32)uiWindow : (int32)(uiWindow ^ 1); }
static int32 set_checked(void *pControl, int32 iChecked)
{ put("set.checked %u %d\n", offset(pControl), iChecked); mutate(); return (int32)s_uiSeed; }
static void set_text(void *pControl, const char *pText)
{ put("set.text %u [%s]\n", offset(pControl), pText); mutate(); }
static int32 enable_control(void *pControl, int32 iEnabled)
{ put("enable.control %u %d\n", offset(pControl), iEnabled); mutate(); return (int32)s_uiSeed; }
static int32 load_string(uint32 uiId, char *pBuffer, int32 iCapacity)
{ put("load.string %x %d\n", uiId, iCapacity); strcpy(pBuffer, "lessons"); mutate(); return 7; }
static int32 split_path(const char *pPath, char *pDrive, char *pDir, char *pName, int32 *pExt)
{
    put("split.path [%s] drive=%u dir=%u name=%u ext=%u\n", pPath, !!pDrive, !!pDir, !!pName, !!pExt);
    if (pDrive) strcpy(pDrive, "C");
    if (pDir) strcpy(pDir, "training");
    if (pName) snprintf(pName, 32, "lesson%u", s_uiScenario % 9);
    if (pExt) *pExt = 0x1d;
    mutate(); return (int32)s_uiSeed;
}
static int32 join_path(char *pOut, const char *pDrive, const char *pDir, const char *pName)
{
    char acResult[256];
    put("join.path drive=%s dir=%s name=%s alias=%u/%u\n", pDrive ? pDrive : "NULL",
        pDir ? pDir : "NULL", pName ? pName : "NULL", pOut == pDir, pOut == pName);
    snprintf(acResult, sizeof(acResult), "%s/%s/%s", pDrive ? pDrive : "", pDir ? pDir : "", pName ? pName : "");
    strcpy(pOut, acResult); mutate(); return (int32)s_uiSeed;
}
static int32 exists(const char *pPath, int32 iExtension)
{ put("exists [%s] %d\n", pPath, iExtension); mutate(); return s_uiScenario % 3 ? 0 : -1; }
static void base_exchange(void *pObject, void *pExchange)
{ put("base.exchange %u %u\n", offset(pObject), offset(pExchange)); mutate(); }
static void ddx_control(void *pExchange, uint32 uiId, void *pControl)
{ put("ddx %u %u %u\n", offset(pExchange), uiId, offset(pControl)); mutate(); }
static int32 read_file(uint32 uiFile, void *pBuffer, uint32 uiCount, uint32 *pRead, void *pOverlapped)
{
    put("read.file %u count=%u overlapped=%u\n", uiFile, uiCount, (uint32)pOverlapped);
    memset(pBuffer, s_uiSeed & 255, uiCount > 64 ? 64 : uiCount);
    *pRead = s_uiScenario % 2 ? uiCount : 0xbad;
    mutate(); if (!s_iSide) ++s_uiReads;
    return s_uiScenario % 2;
}
static char *copy_text(char *pOut, const char *pText)
{ put("copy.text %u [%s] descriptor=%u\n", offset(pOut), pText, offset(PTR(s_oFixture.abyObject, 0x1174))); strcpy(pOut, pText); mutate(); return pOut; }
static int32 open_file(const char *pName, void *pInfo, uint32 uiMode)
{
    put("open.file [%s] mode=%u descriptor=%u name=%u\n", pName, uiMode,
        offset(PTR(s_oFixture.abyObject, 0x1174)), offset(PTR(PTR(s_oFixture.abyObject, 0x1174), 0xc)));
    memset(pInfo, 0, 0x88); mutate(); return s_uiScenario % 3 ? (int32)(s_uiSeed & 0xffff) : -1;
}
static int32 close_handle(uint32 uiHandle)
{ put("close.handle %u\n", uiHandle); mutate(); return (int32)s_uiSeed; }
static int32 TC virtual_skin(void *pObject, int32 iResource, uint32 uiModule, uint32 uiFlags)
{
    put("skin %u resource=%x module=%u flags=%u\n", offset(pObject), iResource, uiModule, uiFlags);
    mutate(); return s_uiScenario % 4 ? (int32)s_uiSeed | 1 : 0;
}
static int32 base_init(void *pObject)
{ put("base.init %u\n", offset(pObject)); mutate(); return (int32)s_uiSeed; }
static int32 dialog_init(void *pObject)
{ put("dialog.init %u\n", offset(pObject)); mutate(); return (int32)s_uiSeed; }
static int32 init_art(void *pControl, int32 iBitmap, void *pDib, void *pPalette, uint32 uiFlags)
{
    put("init.art %u bitmap=%x dib=%u pal=%u flags=%u\n", offset(pControl), iBitmap, offset(pDib), offset(pPalette), uiFlags);
    mutate(); return (int32)s_uiSeed;
}
static int32 init_button(void *pControl, void *pFont, void *pAlternateFont, uint32 uiBitmap,
                          void *pDib, void *pPalette, uint32 uiFlags, uint32 uiColor,
                          uint32 uiModule, int32 iX, int32 iY, struct fs98_help_rect oRect)
{
    put("init.button %u font=%u/%u bmp=%x dib=%u pal=%u flags=%u color=%x module=%u xy=%d/%d rect=%d/%d/%d/%d\n",
        offset(pControl), offset(pFont), offset(pAlternateFont), uiBitmap, offset(pDib), offset(pPalette), uiFlags,
        uiColor, uiModule, iX, iY, oRect.iLeft, oRect.iTop, oRect.iRight, oRect.iBottom);
    mutate(); return (int32)s_uiSeed;
}
static void init_text(void *pControl, void *pFont)
{ put("init.text %u font=%u\n", offset(pControl), offset(pFont)); mutate(); }
static void *get_control(void *pObject, uint32 uiId)
{ put("get.control %u id=%x\n", offset(pObject), uiId); mutate(); return s_oFixture.abyExchange; }
static void remap_rect(struct fs98_help_rect *pRect, void *pControl)
{
    put("remap.rect %u\n", offset(pControl));
    pRect->iLeft = (int32)s_uiSeed; pRect->iTop = (int32)(s_uiSeed ^ 0xdeadbeef);
    pRect->iRight = (int32)(s_uiSeed + 0x87654321); pRect->iBottom = (int32)(s_uiSeed ^ 0x12345678);
    mutate();
}
static void move_control(void *pControl, int32 iX, int32 iY, int32 iWidth, int32 iHeight, int32 iRepaint)
{ put("move.control %u %d/%d %d/%d repaint=%d\n", offset(pControl), iX, iY, iWidth, iHeight, iRepaint); mutate(); }
static void init_group(void *pControl, void *pFont)
{ put("init.group %u font=%u\n", offset(pControl), offset(pFont)); mutate(); }
static void *base_font(int32 iId)
{ put("base.font %d\n", iId); mutate(); return s_oFixture.abyHelp; }
static void init_list(void *pControl, void *pFont, void *pLabel)
{ put("init.list %u font=%u label=%u\n", offset(pControl), offset(pFont), offset(pLabel)); mutate(); }
static void init_edit(void *pControl, void *pFont, uint32 uiForeground, uint32 uiBackground,
                      uint32 uiErrorColor, void *pLabel, uint32 uiFlags)
{
    put("init.edit %u font=%u fg=%x bg=%x error=%x label=%u flags=%u\n", offset(pControl),
        offset(pFont), uiForeground, uiBackground, uiErrorColor, offset(pLabel), uiFlags);
    mutate();
}
static int32 TC virtual_radio(void *pControl, void *pFont, uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiModule)
{
    put("init.radio %u font=%u bmp=%x dib=%u pal=%u module=%u\n", offset(pControl), offset(pFont),
        uiBitmap, offset(pDib), offset(pPalette), uiModule);
    mutate(); return (int32)s_uiSeed;
}
static int32 copy_rect(struct fs98_help_rect *pOut, const struct fs98_help_rect *pIn)
{ put("copy.rect input=%u\n", offset(pIn)); *pOut = *pIn; mutate(); return (int32)s_uiSeed; }
static int32 TC placeholder_destroy(void *pControl)
{ put("placeholder.destroy %u\n", offset(pControl)); mutate(); return (int32)s_uiSeed; }
static void common_controls(void) { put("common.controls\n"); mutate(); }
static uint32 load_library(const char *pName)
{ put("load.library [%s]\n", pName); mutate(); return s_uiScenario % 3 ? s_uiSeed : 0; }
static void *module_state(void)
{ put("module.state\n"); mutate(); return s_oFixture.abyHelp; }
static uint32 create_window(uint32 uiExStyle, const char *pClass, const char *pTitle, uint32 uiStyle,
                             int32 iX, int32 iY, int32 iWidth, int32 iHeight, uint32 uiParent,
                             uint32 uiMenu, uint32 uiInstance, void *pParam)
{
    put("create.window ex=%x class=[%s] title=[%s] style=%x xy=%d/%d size=%d/%d parent=%u menu=%u instance=%u param=%u\n",
        uiExStyle, pClass, pTitle, uiStyle, iX, iY, iWidth, iHeight, uiParent, uiMenu, uiInstance, (uint32)pParam);
    mutate(); return s_uiScenario % 5 ? s_uiSeed ^ 0x123 : 0;
}
static void string_construct(void *pString)
{ put("string.construct %u\n", offset(pString)); D32(pString, 0) = 0x1234; mutate(); }
static void string_destroy(void *pString)
{ put("string.destroy %u value=%u\n", offset(pString), D32(pString, 0)); mutate(); }
static int32 reg_open(uint32 uiKey, const char *pSubkey, uint32 uiOptions, uint32 uiAccess, uint32 *pKey)
{
    put("reg.open key=%x subkey=[%s] options=%u access=%x\n", uiKey, pSubkey, uiOptions, uiAccess);
    *pKey = s_uiSeed; mutate(); return s_uiScenario % 4 == 0 ? 2 : 0;
}
static int32 reg_query(uint32 uiKey, const char *pName, void *pReserved, uint32 *pType, void *pData, uint32 *pSize)
{
    uint32 uiCapacity = *pSize;
    uint32 uiLength = s_uiScenario % 5 == 0 ? uiCapacity - 1 : 6;
    int iFail = s_uiScenario & (!strcmp(pName, "Name") ? 1 : 2);
    put("reg.query key=%u name=[%s] reserved=%u type=%u capacity=%u\n", uiKey, pName, (uint32)pReserved, (uint32)pType, uiCapacity);
    memset(pData, !strcmp(pName, "Name") ? 'N' : 'P', uiLength);
    ((char *)pData)[uiLength] = 0; *pSize = uiLength + 1;
    mutate(); return iFail ? 234 : 0;
}
static int32 load_ui_string(uint32 uiModule, uint32 uiId, char *pBuffer, int32 iCapacity)
{
    const char *pText = uiId == 0xf70b ? "Version " : uiId == 0xf6fd ? "Build " : "Copyright fixture";
    put("load.ui.string module=%u id=%x capacity=%d\n", uiModule, uiId, iCapacity);
    snprintf(pBuffer, iCapacity, "%s", s_uiScenario % 7 ? pText : "");
    mutate(); return strlen(pBuffer);
}
static void set_sims_text(void *pControl, const char *pText)
{
    put("set.sims.text %u [%s]\n", offset(pControl), pText);
    mutate();
}
static int32 format_version(char *pOut, const char *pFormat, int32 iMajor, int32 iMinor,
                            const char *pPrefix, int32 iBuild)
{
    put("format.version [%s] major=%d minor=%d prefix=[%s] build=%d\n", pFormat, iMajor, iMinor, pPrefix, iBuild);
    mutate(); return sprintf(pOut, pFormat, iMajor, iMinor, pPrefix, iBuild);
}
static char *append_text(char *pOut, const char *pText)
{ put("append.text [%s] [%s]\n", pOut, pText); mutate(); return strcat(pOut, pText); }
static void font_construct(void *pFont, void *pBaseFont)
{
    put("font.construct %u base=%u\n", offset(pFont), offset(pBaseFont));
    memset(pFont, s_uiSeed & 255, 0x70); mutate();
}
static void font_destroy(void *pFont)
{ put("font.destroy %u value=%x\n", offset(pFont), D32(pFont, 0)); mutate(); }
static void register_exit(void (*pHandler)(void))
{
    put("register.exit owns=%u\n", pHandler == (s_iSide ? fs98_help_about_font_destroy : (void (*)(void))((uint8 *)s_hRetail + 0x18b0)));
    mutate();
}
static const struct fs98_help_services s_oServices = {
    send_message, win_help, enable_window, kill_timer, set_timer, destroy_window, free_library,
    close_dialog, message_box, get_window_long, set_checked, set_text, enable_control, load_string,
    split_path, join_path, exists, base_exchange, ddx_control, read_file, copy_text, open_file, close_handle,
    base_init, dialog_init, init_art, init_button, init_text, get_control, remap_rect,
    init_group, base_font, init_list, init_edit, copy_rect, common_controls, load_library, module_state, create_window,
    string_construct, string_destroy, reg_open, reg_query, load_ui_string, set_sims_text, format_version, append_text, font_construct, font_destroy, register_exit, move_control
};

static void *dialog_construct(void *pObject, int32 iResource, void *pParent)
{
    put("dialog.construct %u resource=%x parent=%u\n", offset(pObject), iResource, (uint32)pParent);
    D32(pObject, 0) = 0xba5e0000; D32(pObject, 0x20) = s_uiSeed;
    mutate(); return pObject;
}
static void dialog_destroy(void *pObject)
{ put("dialog.destroy %u vtable=%x\n", offset(pObject), D32(pObject, 0)); D32(pObject, 0) = 0xdead0000; mutate(); }
static void construct_control(void *pControl, uint32 uiImport)
{
    put("control.construct %u import=%x\n", offset(pControl), uiImport);
    D32(pControl, 0) = uiImport; D32(pControl, 4) = s_uiSeed;
    mutate();
}
static void destroy_control(void *pControl, uint32 uiImport)
{
    put("control.destroy %u import=%x vtable=%x\n", offset(pControl), uiImport, D32(pControl, 0));
    D32(pControl, 0) = uiImport; mutate();
}
static void *allocate_object(uint32 uiSize)
{
    put("allocate %u\n", uiSize); mutate();
    return s_uiScenario % 11 ? s_oFixture.abyObject : 0;
}
static void ui_delete(void *pObject) { put("ui.delete %u\n", offset(pObject)); mutate(); }
static void mfc_delete(void *pObject) { put("mfc.delete %u\n", offset(pObject)); mutate(); }
static void dialog_delete(void *pObject) { put("dialog.delete %u\n", offset(pObject)); mutate(); }
static int32 create_dialog(void *pObject, uint32 uiResource, void *pParent)
{
    put("dialog.create %u resource=%x parent=%u module=%u\n", offset(pObject), uiResource, (uint32)pParent, D32(pObject, 0x80));
    mutate(); return s_uiScenario & 1 ? 0 : (int32)s_uiSeed;
}
static int32 show_dialog(void *pObject, int32 iShow)
{ put("dialog.show %u %d\n", offset(pObject), iShow); mutate(); return s_uiScenario & 2 ? 0 : (int32)s_uiSeed; }
static const struct fs98_help_object_services s_oObjectServices = {
    dialog_construct, dialog_destroy, construct_control, destroy_control, allocate_object,
    ui_delete, mfc_delete, dialog_delete, create_dialog, show_dialog
};
#define CONTROL_HOOK(szName, uiCtor, uiDtor) \
static void *TC hook_construct_##szName(void *pControl) { construct_control(pControl, uiCtor); return pControl; } \
static void TC hook_destroy_##szName(void *pControl) { destroy_control(pControl, uiDtor); }
CONTROL_HOOK(text, 0x64a4, 0x64b8)
CONTROL_HOOK(group, 0x64a8, 0x64bc)
CONTROL_HOOK(button, 0x64ac, 0x64c0)
CONTROL_HOOK(art, 0x64b0, 0x6498)
CONTROL_HOOK(list, 0x644c, 0x645c)
CONTROL_HOOK(edit, 0x6458, 0x6460)
CONTROL_HOOK(radio, 0x6468, 0x6464)
CONTROL_HOOK(window, 0x6348, 0x634c)
#undef CONTROL_HOOK
static void *TC hook_dialog_construct(void *pObject, int32 iResource, void *pParent)
{ return dialog_construct(pObject, iResource, pParent); }
static void TC hook_dialog_destroy(void *pObject) { dialog_destroy(pObject); }
static void SC hook_dialog_delete(void *pObject) { dialog_delete(pObject); }
static int32 TC hook_create_dialog(void *pObject, uint32 uiResource, void *pParent)
{ return create_dialog(pObject, uiResource, pParent); }
static int32 TC hook_show_dialog(void *pObject, int32 iShow) { return show_dialog(pObject, iShow); }

/* Original Win32/MFC import ABIs, separate from the plain-C service boundary. */
static int32 SC hook_send(uint32 uiA, uint32 uiB, uint32 uiC, uint32 uiD) { return send_message(uiA,uiB,uiC,uiD); }
static int32 SC hook_help(uint32 uiA, const char *pB, uint32 uiC, const uint32 *pD) { return win_help(uiA,pB,uiC,pD); }
static int32 SC hook_enable(uint32 uiA, int32 iB) { return enable_window(uiA,iB); }
static int32 SC hook_kill(uint32 uiA, uint32 uiB) { return kill_timer(uiA,uiB); }
static uint32 SC hook_timer(uint32 uiA, uint32 uiB, uint32 uiC, void *pD) { return set_timer(uiA,uiB,uiC,pD); }
static int32 SC hook_destroy(uint32 uiA) { return destroy_window(uiA); }
static int32 SC hook_free(uint32 uiA) { return free_library(uiA); }
static void TC hook_close(void *pObject) { close_dialog(pObject); }
static int32 SC hook_box(const char *pA, uint32 uiB, uint32 uiC) { return message_box(pA,uiB,uiC); }
static int32 SC hook_long(uint32 uiA, int32 iB) { return get_window_long(uiA,iB); }
static int32 TC hook_checked(void *pA, int32 iB) { return set_checked(pA,iB); }
static void TC hook_text(void *pA, const char *pB) { set_text(pA,pB); }
static int32 TC hook_control(void *pA, int32 iB) { return enable_control(pA,iB); }
static int32 SC hook_load(uint32 uiA, char *pB, int32 iC) { return load_string(uiA,pB,iC); }
static int32 SC hook_split(const char *pA, char *pB, char *pC, char *pD, int32 *pE) { return split_path(pA,pB,pC,pD,pE); }
static int32 SC hook_join(char *pA, const char *pB, const char *pC, const char *pD) { return join_path(pA,pB,pC,pD); }
static int32 SC hook_exists(const char *pA, int32 iB) { return exists(pA,iB); }
static void TC hook_exchange(void *pA, void *pB) { base_exchange(pA,pB); }
static void SC hook_ddx(void *pA, uint32 uiB, void *pC) { ddx_control(pA,uiB,pC); }
static int32 SC hook_read(uint32 uiA, void *pB, uint32 uiC, uint32 *pD, void *pE) { return read_file(uiA,pB,uiC,pD,pE); }
static char *SC hook_copy(char *pA, const char *pB) { return copy_text(pA,pB); }
static int32 SC hook_open(const char *pA, void *pB, uint32 uiC) { return open_file(pA,pB,uiC); }
static int32 SC hook_close_handle(uint32 uiA) { return close_handle(uiA); }
static int32 TC hook_base_init(void *pObject) { return base_init(pObject); }
static int32 TC hook_dialog_init(void *pObject) { return dialog_init(pObject); }
static int32 TC hook_art(void *pControl, int32 iBitmap, void *pDib, void *pPalette, uint32 uiFlags)
{ return init_art(pControl, iBitmap, pDib, pPalette, uiFlags); }
static int32 TC hook_button(void *pControl, void *pFont, void *pAlternateFont, uint32 uiBitmap,
                            void *pDib, void *pPalette, uint32 uiFlags, uint32 uiColor,
                            uint32 uiModule, int32 iX, int32 iY, struct fs98_help_rect oRect)
{ return init_button(pControl, pFont, pAlternateFont, uiBitmap, pDib, pPalette, uiFlags, uiColor, uiModule, iX, iY, oRect); }
static void TC hook_init_text(void *pControl, void *pFont) { init_text(pControl, pFont); }
static void *TC hook_get_control(void *pObject, uint32 uiId) { return get_control(pObject, uiId); }
static void SC hook_remap(struct fs98_help_rect *pRect, void *pControl) { remap_rect(pRect, pControl); }
static void TC hook_move(void *pControl, int32 iX, int32 iY, int32 iWidth, int32 iHeight, int32 iRepaint)
{ move_control(pControl, iX, iY, iWidth, iHeight, iRepaint); }
static void TC hook_init_group(void *pControl, void *pFont) { init_group(pControl, pFont); }
static void *SC hook_base_font(int32 iId) { return base_font(iId); }
static void TC hook_init_list(void *pControl, void *pFont, void *pLabel) { init_list(pControl, pFont, pLabel); }
static void TC hook_init_edit(void *pControl, void *pFont, uint32 uiForeground, uint32 uiBackground,
                              uint32 uiErrorColor, void *pLabel, uint32 uiFlags)
{ init_edit(pControl, pFont, uiForeground, uiBackground, uiErrorColor, pLabel, uiFlags); }
static int32 SC hook_copy_rect(struct fs98_help_rect *pOut, const struct fs98_help_rect *pIn) { return copy_rect(pOut, pIn); }
static void SC hook_common_controls(void) { common_controls(); }
static uint32 SC hook_load_library(const char *pName) { return load_library(pName); }
static void *SC hook_module_state(void) { return module_state(); }
static uint32 SC hook_create_window(uint32 uiExStyle, const char *pClass, const char *pTitle, uint32 uiStyle,
                                    int32 iX, int32 iY, int32 iWidth, int32 iHeight, uint32 uiParent,
                                    uint32 uiMenu, uint32 uiInstance, void *pParam)
{ return create_window(uiExStyle, pClass, pTitle, uiStyle, iX, iY, iWidth, iHeight, uiParent, uiMenu, uiInstance, pParam); }
static void TC hook_string_construct(void *pString) { string_construct(pString); }
static void TC hook_string_destroy(void *pString) { string_destroy(pString); }
static int32 SC hook_reg_open(uint32 uiKey, const char *pSubkey, uint32 uiOptions, uint32 uiAccess, uint32 *pKey)
{ return reg_open(uiKey, pSubkey, uiOptions, uiAccess, pKey); }
static int32 SC hook_reg_query(uint32 uiKey, const char *pName, void *pReserved, uint32 *pType, void *pData, uint32 *pSize)
{ return reg_query(uiKey, pName, pReserved, pType, pData, pSize); }
static int32 SC hook_load_ui_string(uint32 uiModule, uint32 uiId, char *pBuffer, int32 iCapacity)
{ return load_ui_string(uiModule, uiId, pBuffer, iCapacity); }
static void TC hook_set_sims_text(void *pControl, const char *pText) { set_sims_text(pControl, pText); }
static char *SC hook_append_text(char *pOut, const char *pText) { return append_text(pOut, pText); }
static void TC hook_font_construct(void *pFont, void *pBaseFont) { font_construct(pFont, pBaseFont); }
static void TC hook_font_destroy(void *pFont) { font_destroy(pFont); }
static void *hook_dll_onexit(void (*pHandler)(void), void **ppBegin, void **ppEnd)
{
    if (ppBegin != (void **)((uint8 *)s_hRetail + 0x5250) || ppEnd != (void **)((uint8 *)s_hRetail + 0x524c)) exit(2);
    register_exit(pHandler); return (void *)pHandler;
}
static void patch(uint32 uiRva, void *pTarget)
{
    DWORD uiOld;
    void **pSlot = (void **)((uint8 *)s_hRetail + uiRva);
    if (!VirtualProtect(pSlot, 4, PAGE_READWRITE, &uiOld)) exit(2);
    *pSlot = pTarget;
    VirtualProtect(pSlot, 4, uiOld, &uiOld);
}
static void setup_imports(void)
{
    static void *apHost[0x2a4 / 4];
    patch(0x6508, hook_send); patch(0x64fc, hook_help); patch(0x6518, hook_enable);
    patch(0x650c, hook_kill); patch(0x6504, hook_timer); patch(0x64f8, hook_destroy);
    patch(0x6300, hook_free); patch(0x6494, hook_close); patch(0x6344, hook_box);
    patch(0x6500, hook_long); patch(0x64e0, hook_checked); patch(0x633c, hook_text);
    patch(0x6340, hook_control); patch(0x64ec, hook_exchange); patch(0x6378, hook_ddx);
    patch(0x6310, hook_read); patch(0x630c, hook_copy); patch(0x6308, hook_open);
    patch(0x62f8, hook_close_handle);
    apHost[0x2a0 / 4] = hook_load; apHost[0x188 / 4] = hook_split;
    apHost[0x184 / 4] = hook_join; apHost[0x158 / 4] = hook_exists;
    patch(0x5064, apHost);
    patch(0x6474, hook_base_init); patch(0x632c, hook_dialog_init); patch(0x6480, hook_art);
    patch(0x6484, hook_button); patch(0x6488, hook_init_text); patch(0x6330, hook_get_control);
    patch(0x646c, hook_remap); patch(0x6334, hook_move); patch(0x648c, hook_init_group);
    patch(0x6478, hook_base_font); patch(0x6454, hook_init_list); patch(0x6450, hook_init_edit);
    patch(0x6510, hook_copy_rect); patch(0x62f0, hook_common_controls); patch(0x6304, hook_load_library);
    patch(0x63c8, hook_module_state); patch(0x6514, hook_create_window);
    patch(0x6370, hook_string_construct); patch(0x6374, hook_string_destroy);
    patch(0x62e4, hook_reg_open); patch(0x62e8, hook_reg_query); patch(0x651c, hook_load_ui_string);
    patch(0x6490, hook_set_sims_text); patch(0x6520, format_version); patch(0x62fc, hook_append_text);
    patch(0x647c, hook_font_construct); patch(0x6470, hook_font_destroy); patch(0x6444, hook_dll_onexit);
    fs98_help_set_about_state(&s_oFixture.oAboutState);
    patch(0x51a4, (void *)0x31415926); fs98_help_set_resource_module(0x31415926);
    patch(0x51bc, (void *)0x27182818); fs98_help_set_object_module(0x27182818);
    patch(0x64a0, hook_dialog_construct); patch(0x64b4, hook_dialog_destroy);
#define PATCH_CONTROL(szName, uiCtor, uiDtor) patch(uiCtor, hook_construct_##szName); patch(uiDtor, hook_destroy_##szName)
    PATCH_CONTROL(text, 0x64a4, 0x64b8); PATCH_CONTROL(group, 0x64a8, 0x64bc);
    PATCH_CONTROL(button, 0x64ac, 0x64c0); PATCH_CONTROL(art, 0x64b0, 0x6498);
    PATCH_CONTROL(list, 0x644c, 0x645c); PATCH_CONTROL(edit, 0x6458, 0x6460);
    PATCH_CONTROL(radio, 0x6468, 0x6464); PATCH_CONTROL(window, 0x6348, 0x634c);
#undef PATCH_CONTROL
    patch(0x64e8, allocate_object); patch(0x649c, ui_delete); patch(0x63e4, mfc_delete);
    patch(0x64f0, hook_dialog_delete); patch(0x63e0, hook_create_dialog); patch(0x63dc, hook_show_dialog);
    {
        void *apVtables[5]; uint32 uiIndex;
        HMODULE hCrt = LoadLibraryA("msvcrt.dll");
        void *pHandler = (void *)GetProcAddress(hCrt, "__CxxFrameHandler");
        if (!pHandler) exit(2);
        patch(0x6440, pHandler);
        for (uiIndex = 0; uiIndex < 5; ++uiIndex) apVtables[uiIndex] = (uint8 *)s_hRetail + s_aClasses[uiIndex].unVtable;
        fs98_help_set_object_services(&s_oObjectServices, apVtables, (uint8 *)s_hRetail + 0x4350);
    }
}
static void initialize(void)
{
    uint32 uiIndex;
    uint8 *pObject = s_oFixture.abyObject;
    uint32 uiDescriptorOffset = s_uiOperation == 4 || s_uiOperation == 5 ? 0x1174 :
        ((s_uiOperation >= 18 && s_uiOperation <= 21) || s_uiOperation == 45) ? 0x10e0 : 0x10dc;
    for (uiIndex = 0; uiIndex < sizeof(s_oFixture); ++uiIndex) ((uint8 *)&s_oFixture)[uiIndex] = (uint8)next_random();
    PTR(pObject, 0) = s_apVtable;
    PTR(pObject, 0x1694) = PTR(pObject, 0x1788) = PTR(pObject, 0x1274) = s_apControlVtable;
    D32(pObject, 0x20) = s_uiSeed;
    PTR(pObject, uiDescriptorOffset) = s_oFixture.aabyDescriptor[0];
    for (uiIndex = 0; uiIndex < 2; ++uiIndex) {
        uint8 *pDescriptor = s_oFixture.aabyDescriptor[uiIndex];
        D32(pDescriptor, 8) = s_uiScenario % 5;
        D32(pDescriptor, 0xc) = s_uiScenario % 6 - 1;
        PTR(pDescriptor, 0x10) = s_oFixture.aabyRecords;
        D32(pDescriptor, 0x14) = s_uiScenario % 3;
        if (s_uiOperation == 2) PTR(pDescriptor, 0x10) = about_callback;
        if ((s_uiOperation >= 18 && s_uiOperation <= 21) || s_uiOperation == 45) PTR(pDescriptor, 8) = graph_callback;
    }
    for (uiIndex = 0; uiIndex < 4; ++uiIndex) {
        snprintf((char *)s_oFixture.aabyRecords[uiIndex], 64, "Lesson %u vector %u", uiIndex, s_uiScenario);
        snprintf((char *)s_oFixture.aabyRecords[uiIndex] + 0x100, 64, "Description %u seed %x", uiIndex, s_uiSeed);
        snprintf((char *)s_oFixture.aabyRecords[uiIndex] + 0x900, 64, "C:\\lessons\\lesson%u.adv", uiIndex);
    }
    if (s_uiOperation == 8) {
        static const uint32 auiLimits[] = {0x80000000, 0xffffffff, 0, 1, 5, 0x7fffffff};
        D32(pObject, 0x10f0) = auiLimits[(s_uiScenario / 6) % 6];
        D32(pObject, 0x10f4) = s_uiScenario & 1 ? 0xffffffff : s_uiSeed;
    }
    if (s_uiOperation == 3) {
        if (!(s_uiScenario & 1)) D32(pObject, 0x10e8) = 0;
        if (!(s_uiScenario & 2)) D32(pObject, 0x10ec) = 0;
        if (!(s_uiScenario & 4)) D32(pObject, 0x110c) = 0;
    }
    if (s_uiOperation == 9) D32(pObject, 0x1108) = s_uiScenario % 4;
    if ((s_uiOperation == 15 || s_uiOperation == 47) && s_uiScenario % 3 == 0) PTR(pObject, 0x10dc) = 0;
    if ((s_uiOperation == 21 || s_uiOperation == 45) && s_uiScenario % 3 != 0)
        PTR(pObject, 0x10e0) = s_uiScenario % 3 == 1 ? 0 : (void *)0xcdcdcdcd;
    s_oInitial = s_oFixture;
}
static LONG WINAPI fatal(EXCEPTION_POINTERS *pException)
{
    fprintf(stderr, "FAULT scenario=%u op=%u (%s) side=%d code=%lx address=%p\n", s_uiScenario,
        s_uiOperation, s_aOperations[s_uiOperation].pName, s_iSide,
        pException->ExceptionRecord->ExceptionCode, pException->ExceptionRecord->ExceptionAddress);
    fflush(stderr); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static void land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiSavedSeh) : "memory");
    __builtin_longjmp(s_apJump, 1);
}
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *pException)
{
    EXCEPTION_RECORD *pRecord = pException->ExceptionRecord;
    if (s_aOperations[s_uiOperation].uiSignature == 11 && s_uiScenario % 11 == 0 &&
        pRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        s_uiThrowAt = 0;
        put("allocation.fault access=%u address=%u\n", (uint32)pRecord->ExceptionInformation[0],
            (uint32)pRecord->ExceptionInformation[1]);
        if (!s_iSide) ++s_uiAllocationFaults;
        pException->ContextRecord->Eip = (DWORD)land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static EXCEPTION_DISPOSITION __cdecl catch_throw(EXCEPTION_RECORD *pRecord, void *pFrame,
                                                 CONTEXT *pContext, void *pDispatcher)
{
    (void)pContext; (void)pDispatcher;
    if (pRecord->ExceptionCode == TEST_THROW &&
        !(pRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(pFrame, 0, pRecord, 0);
        s_uiThrowAt = 0; put("CAUGHT\n"); if (!s_iSide) ++s_uiThrows;
        land();
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) int32 run(uint32 uiThrowAt)
{
    struct { void *pPrevious; void *pHandler; } oCatcher;
    void *pEntry = (uint8 *)s_hRetail + s_aOperations[s_uiOperation].uiRva;
    void *pObject = s_oFixture.abyObject;
    volatile int32 iResult = 0;
    uint32 uiArgument;
    s_oFixture = s_oInitial;
    memcpy((uint8 *)s_hRetail + 0x51c0, &s_oFixture.oAboutState, sizeof(s_oFixture.oAboutState));
    s_uiLogLength = s_uiCalls = 0; s_aacLogs[s_iSide][0] = 0; s_uiThrowAt = uiThrowAt;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_uiSavedSeh));
    if (__builtin_setjmp(s_apJump) == 0) {
        oCatcher.pPrevious = (void *)s_uiSavedSeh; oCatcher.pHandler = catch_throw;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&oCatcher) : "memory");
        switch (s_aOperations[s_uiOperation].uiSignature) {
        case 0:
            if (s_uiOperation == 24 && s_uiScenario % 2 == 0) pObject = 0;
            (s_iSide ? s_aOperations[s_uiOperation].pEvent : (event_fn)pEntry)(pObject);
            break;
        case 1: case 2: case 3: {
            uint32 uiKind = (s_uiOperation - 30) / 3;
            uint32 uiWhich = (s_uiOperation - 30) % 3;
            static void *aapOurs[5][3] = {
#define OUR_CLASS(szName) {fs98_help_##szName##_help, fs98_help_##szName##_context, fs98_help_##szName##_exchange}
                OUR_CLASS(about), OUR_CLASS(credits), OUR_CLASS(lessons), OUR_CLASS(graph), OUR_CLASS(completion)
#undef OUR_CLASS
            };
            if (s_iSide) pEntry = aapOurs[uiKind][uiWhich];
            if (uiWhich == 0)
                iResult = ((int32 (TC *)(void *, uint32, void *))pEntry)(pObject, s_uiSeed, s_oFixture.abyHelp);
            else if (uiWhich == 1)
                iResult = ((int32 (TC *)(void *, uint32, uint32))pEntry)(pObject, s_uiSeed, s_uiSeed ^ 1);
            else ((void (TC *)(void *, void *))pEntry)(pObject, s_oFixture.abyExchange);
            break;
        }
        case 4:
            if (s_iSide) {
                if (s_uiOperation == 7) pEntry = fs98_help_credits_start_timer;
                if (s_uiOperation == 14) pEntry = fs98_help_lessons_no_error;
                if (s_uiOperation == 15) pEntry = fs98_help_lessons_validate;
                if (s_uiOperation == 21) pEntry = fs98_help_graph_validate;
                if (s_uiOperation == 45) pEntry = fs98_help_graph_init;
                if (s_uiOperation == 46) pEntry = fs98_help_completion_init;
                if (s_uiOperation == 47) pEntry = fs98_help_lessons_init;
                if (s_uiOperation == 48) pEntry = fs98_help_credits_init;
            }
            iResult = ((int32 (TC *)(void *))pEntry)(pObject); break;
        case 5:
            uiArgument = s_uiScenario & 1 ? D32(pObject, 0x10e8) : D32(pObject, 0x10e8) ^ 1;
            if (s_iSide) pEntry = s_uiOperation == 8 ? (void *)fs98_help_credits_timer : (void *)fs98_help_empty_arg;
            ((void (TC *)(void *, uint32))pEntry)(pObject, uiArgument); break;
        case 6:
            if (s_iSide) pEntry = fs98_help_graph_tick;
            iResult = ((int32 (TC *)(void *, uint32, uint32))pEntry)(pObject, s_uiSeed, s_uiSeed ^ 1); break;
        case 7:
            if (s_iSide) pEntry = fs98_help_credits_read;
            iResult = ((int32 (SC *)(uint32, void *, int32, uint32 *))pEntry)(s_uiSeed,
                s_oFixture.abyBuffer, s_uiScenario % 65, &s_oFixture.uiRead); break;
        case 8: case 9: case 10: case 11: case 12: case 13: {
            uint32 uiKind = (s_uiOperation - 49) / 6;
            uint32 uiWhich = (s_uiOperation - 49) % 6;
            void *pReturned = 0;
            static void *aapOurs[5][4] = {
#define OUR_OBJECT(szName) {fs98_help_##szName##_construct, fs98_help_##szName##_destroy, fs98_help_##szName##_delete, fs98_help_##szName##_create}
                OUR_OBJECT(about), OUR_OBJECT(credits), OUR_OBJECT(lessons), OUR_OBJECT(graph), OUR_OBJECT(completion)
#undef OUR_OBJECT
            };
            const struct fs98_help_class *pClass = &s_aClasses[uiKind];
            if (s_iSide) pEntry = aapOurs[uiKind][uiWhich < 4 ? uiWhich : 0];
            if (uiWhich == 0 || uiWhich >= 4)
                pReturned = ((void *(TC *)(void *, void *))pEntry)(pObject, (void *)s_uiSeed);
            else if (uiWhich == 1) ((void (TC *)(void *))pEntry)(pObject);
            else if (uiWhich == 2) pReturned = ((void *(TC *)(void *, uint32))pEntry)(pObject, s_uiSeed);
            else iResult = ((int32 (SC *)(uint32, void *))pEntry)(s_uiSeed, s_oFixture.aabyDescriptor[0]);
            if (uiWhich >= 4) {
                pEntry = s_iSide ? aapOurs[uiKind][uiWhich == 4 ? 1 : 2] :
                    (uint8 *)s_hRetail + (uiWhich == 4 ? pClass->unDestructor : pClass->unDelete);
                if (uiWhich == 4) ((void (TC *)(void *))pEntry)(pObject);
                else pReturned = ((void *(TC *)(void *, uint32))pEntry)(pObject, s_uiSeed);
            }
            if (pReturned) put("return.object %u\n", offset(pReturned));
            break;
        }
        case 14:
            if (s_iSide) pEntry = fs98_help_placeholder_delete;
            put("return.object %u\n", offset(((void *(TC *)(void *, uint32))pEntry)(
                s_oFixture.abyObject + 0x1274, s_uiSeed)));
            break;
        case 15:
            if (s_iSide) pEntry = fs98_help_class_delete;
            ((void (SC *)(void *))pEntry)(pObject); break;
        case 16:
            if (s_iSide) pEntry = fs98_help_about_populate;
            ((event_fn)pEntry)(pObject); break;
        case 17:
            if (s_iSide) pEntry = fs98_help_about_font_destroy;
            ((void (*)(void))pEntry)(); break;
        case 18: case 19:
            if (s_iSide) pEntry = fs98_help_about_init;
            iResult = ((int32 (TC *)(void *))pEntry)(pObject);
            if (s_aOperations[s_uiOperation].uiSignature == 19)
                iResult = ((int32 (TC *)(void *))pEntry)(pObject);
            break;
        }
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiSavedSeh) : "memory");
    if (!s_iSide) memcpy(&s_oFixture.oAboutState, (uint8 *)s_hRetail + 0x51c0, sizeof(s_oFixture.oAboutState));
    /* A persistent function pointer is compared by ownership, then normalized.
     * All other bytes, including the embedded descriptor/name, stay exact. */
    if ((s_uiOperation == 6 || s_uiOperation == 48) && PTR(s_oFixture.abyObject, 0x10e4) ==
        (s_iSide ? (void *)fs98_help_credits_read : (uint8 *)s_hRetail + 0x1fe0))
        PTR(s_oFixture.abyObject, 0x10e4) = (void *)0xfeed1234;
    if (s_uiCalls > s_uiMaxCalls) s_uiMaxCalls = s_uiCalls;
    return iResult;
}
static int compare(uint32 uiThrowAt)
{
    int32 iRetail, iOurs;
    uint32 uiOffset;
    s_iSide = 0; iRetail = run(uiThrowAt); s_oRetailResult = s_oFixture;
    s_iSide = 1; iOurs = run(uiThrowAt);
    ++s_uiComparisons;
    if (iRetail == iOurs && !memcmp(&s_oRetailResult, &s_oFixture, sizeof(s_oFixture)) &&
        !strcmp(s_aacLogs[0], s_aacLogs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s) throw=%u seed=%x script=%u returns=%d/%d\n",
        s_uiScenario, s_uiOperation, s_aOperations[s_uiOperation].pName, uiThrowAt, s_uiSeed, s_uiScript, iRetail, iOurs);
    for (uiOffset = 0; uiOffset < sizeof(s_oFixture); ++uiOffset)
        if (((uint8 *)&s_oRetailResult)[uiOffset] != ((uint8 *)&s_oFixture)[uiOffset]) {
            fprintf(stderr, "byte offset=%x retail=%02x ours=%02x\n", uiOffset,
                ((uint8 *)&s_oRetailResult)[uiOffset], ((uint8 *)&s_oFixture)[uiOffset]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_aacLogs[0], s_aacLogs[1]);
    return 1;
}
int main(int iArgc, char **ppArgv)
{
    uint32 uiCount = iArgc > 1 ? strtoul(ppArgv[1], 0, 0) : 20000;
    uint32 uiInitialSeed = iArgc > 2 ? strtoul(ppArgv[2], 0, 0) : 0x19a847e3;
    int iUnwind = iArgc > 3 && !strcmp(ppArgv[3], "unwind");
    uint32 uiThrow, uiLimit;
    if (!uiCount || !uiInitialSeed) return 2;
    s_uiRng = uiInitialSeed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal);
    AddVectoredExceptionHandler(1, expected_fault);
    s_hRetail = LoadLibraryExA("orig/English/Help.DLL", 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_hRetail || GetProcAddress(s_hRetail, "Linkage") != (FARPROC)((uint8 *)s_hRetail + 0x5078)) return 2;
    setup_imports(); fs98_help_set_services(&s_oServices);
    s_apVtable[0xec / 4] = virtual_close; s_apOtherVtable[0xec / 4] = virtual_close_other;
    s_apVtable[0xcc / 4] = virtual_accept; s_apOtherVtable[0xcc / 4] = virtual_accept_other;
    s_apVtable[0xd0 / 4] = virtual_cancel; s_apOtherVtable[0xd0 / 4] = virtual_cancel_other;
    s_apVtable[1] = s_apOtherVtable[1] = virtual_delete;
    s_apVtable[0xe4 / 4] = s_apOtherVtable[0xe4 / 4] = virtual_skin;
    s_apControlVtable[0xc4 / 4] = virtual_radio; s_apControlVtable[0x60 / 4] = placeholder_destroy;
    for (s_uiScenario = 0; s_uiScenario < uiCount; ++s_uiScenario) {
        s_uiSeed = next_random(); s_uiScript = s_uiScenario & 15;
        for (s_uiOperation = 0; s_uiOperation < OP_COUNT; ++s_uiOperation) {
            initialize();
            if (compare(0)) return 1;
            uiLimit = s_uiCalls;
            if (iUnwind) for (uiThrow = 1; uiThrow <= uiLimit; ++uiThrow)
                if (compare(uiThrow)) return 1;
        }
    }
    printf("Help events: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x swaps=%u reads=%u resets=%u throws=%u max_calls=%u allocation_faults=%u\n",
        uiCount, (uint32)OP_COUNT, s_uiComparisons, uiInitialSeed, s_uiSwaps, s_uiReads, s_uiResets, s_uiThrows, s_uiMaxCalls, s_uiAllocationFaults);
    FreeLibrary(s_hRetail); return 0;
}
