/* Alerts slice 1: real command handlers and modeless progress tick against C.
 * Maps retail without resolving imports or running its CRT/DllMain. Only the
 * routines named in s_auiRetailEntries are invoked. Dependency calls reached
 * by those routines are patched to a deterministic MFC/Win32 model. This
 * tests event logic, descriptor mutations, virtual-call order and drawing
 * transcripts; it does not test construction, MFC dispatch or real pixels. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../src/alerts/alerts_internal.h"

#define STDCALL __attribute__((stdcall))
#define THISCALL FS98_ALERTS_THISCALL
#define D32(pVal, iOffset) (*(uint32 *)((uint8 *)(pVal) + (iOffset)))
#define PTR(pVal, iOffset) (*(void **)((uint8 *)(pVal) + (iOffset)))
#define OBJECT_SIZE 0x1560
#define DESCRIPTOR_SIZE 0x40
#define LOG_SIZE 8192

typedef void (THISCALL *fs98_event_fn)(void *pObject);
typedef int32 (THISCALL *fs98_tick_fn)(void *pObject, uint32 uiWParam, uint32 uiLParam);
typedef void (THISCALL *fs98_exchange_fn)(void *pObject, void *pExchange);
typedef int32 (THISCALL *fs98_init_event_fn)(void *pObject);

static uint8 s_abyObject[OBJECT_SIZE] __attribute__((aligned(16)));
static uint8 s_abyDescriptor[DESCRIPTOR_SIZE] __attribute__((aligned(16)));
static uint8 s_abyInitialObject[OBJECT_SIZE], s_abyInitialDescriptor[DESCRIPTOR_SIZE];
static uint8 s_abyRetailObject[OBJECT_SIZE], s_abyRetailDescriptor[DESCRIPTOR_SIZE];
static void *s_apVtable[60], *s_apOtherVtable[60];
static void *s_apCheckVtable[60];
static char s_aszLog[2][LOG_SIZE];
static uint32 s_auiLogLength[2];
static uint32 s_uiRng, s_uiScenario, s_uiSeed;
static int s_iSide, s_iEntry, s_bReplaceVtable;
static int s_bRunning;
static struct fs98_alerts_rect s_stRect;
static struct fs98_alerts_size s_stSize;
static int32 s_iChildX, s_iChildY, s_iParentX, s_iParentY;
static uint32 s_uiForeground, s_uiBackground;
static struct {
    uint32 uiReferences, uiLength, uiCapacity;
    char szText[128];
} s_stString;
static uint32 s_uiEnabled, s_uiDisabled, s_uiClosed, s_uiWrapped;
static HMODULE s_pRetail;
static uint32 s_auiExchange[4];
static uint32 s_uiProductWitness;
static uint8 s_abyHost[0x300];
static const char s_szMessage[] = "Alerts fixture: line one\nline two";
static int s_bInitSuccess;
static uint32 s_uiInitSuccess, s_uiInitFailure;
static uint32 s_uiTextResources, s_uiDirectText;
static uint32 s_auiButtonFlags[256];

static uint32 fs98_rnd(void)
{
    s_uiRng ^= s_uiRng << 13;
    s_uiRng ^= s_uiRng >> 17;
    s_uiRng ^= s_uiRng << 5;
    return s_uiRng;
}

static int32 fs98_add(int32 iLeft, int32 iRight)
{
    return (int32)((uint32)iLeft + (uint32)iRight);
}

static int32 fs98_sub(int32 iLeft, int32 iRight)
{
    return (int32)((uint32)iLeft - (uint32)iRight);
}

static void fs98_put(const char *pszFormat, ...)
{
    va_list pArgs;
    int iWritten;
    uint32 uiLength = s_auiLogLength[s_iSide];
    if (uiLength > LOG_SIZE - 1024) {
        fprintf(stderr, "transcript overflow\n");
        exit(2);
    }
    va_start(pArgs, pszFormat);
    iWritten = vsnprintf(s_aszLog[s_iSide] + uiLength, LOG_SIZE - uiLength, pszFormat, pArgs);
    va_end(pArgs);
    if (iWritten < 0)
        exit(2);
    s_auiLogLength[s_iSide] += (uint32)iWritten;
}

static void fs98_string_construct(void *pString)
{
    fs98_put("string.construct\n");
    PTR(pString, 0) = s_stString.szText;
    s_stString.szText[0] = 0;
    s_stString.uiLength = 0;
}

static void fs98_string_destroy(void *pString)
{
    fs98_put("string.destroy %s\n", (char *)PTR(pString, 0));
}

static void fs98_dc_construct(void *pDc, void *pWindow)
{
    fs98_put("dc.construct %u\n", D32(pWindow, 0x20));
    memset(pDc, 0, 0x14);
    D32(pDc, 4) = 0xd001;
    D32(pDc, 8) = 0xd002;
}

static void fs98_dc_destroy(void *pDc)
{
    fs98_put("dc.destroy %u\n", D32(pDc, 4));
}

static uint32 fs98_stock(int32 iObject)
{
    fs98_put("stock %d\n", iObject);
    return 0xf001;
}

static void *fs98_font(uint32 uiFont)
{
    fs98_put("font %u\n", uiFont);
    return (void *)0xf002;
}

static void *fs98_select(void *pDc, void *pFont)
{
    fs98_put("select %u %u\n", D32(pDc, 4), (uint32)pFont);
    return (void *)0xf003;
}

static int32 STDCALL fs98_client_rect(uint32 uiWindow, struct fs98_alerts_rect *pRect)
{
    *pRect = s_stRect;
    fs98_put("rect %u %d %d %d %d\n", uiWindow, pRect->iLeft, pRect->iTop,
             pRect->iRight, pRect->iBottom);
    return 1;
}

static int32 STDCALL fs98_client_screen(uint32 uiWindow, struct fs98_alerts_point *pPoint)
{
    fs98_put("to.screen %u %d %d\n", uiWindow, pPoint->iX, pPoint->iY);
    pPoint->iX = fs98_add(pPoint->iX, s_iChildX);
    pPoint->iY = fs98_add(pPoint->iY, s_iChildY);
    return 1;
}

static int32 STDCALL fs98_screen_client(uint32 uiWindow, struct fs98_alerts_point *pPoint)
{
    fs98_put("to.client %u %d %d\n", uiWindow, pPoint->iX, pPoint->iY);
    pPoint->iX = fs98_sub(pPoint->iX, s_iParentX);
    pPoint->iY = fs98_sub(pPoint->iY, s_iParentY);
    return 1;
}

static void fs98_format(void *pString, const char *pszFormat, int32 iValue)
{
    fs98_put("format %s %d\n", pszFormat, iValue);
    snprintf(s_stString.szText, sizeof(s_stString.szText), pszFormat, iValue);
    s_stString.uiLength = (uint32)strlen(s_stString.szText);
    PTR(pString, 0) = s_stString.szText;
}

static int32 STDCALL fs98_extent(uint32 uiDc, const char *pszText, int32 iLength,
                                struct fs98_alerts_size *pSize)
{
    fs98_put("extent %u %d %s\n", uiDc, iLength, pszText);
    *pSize = s_stSize;
    return 1;
}

static uint32 fs98_text_color(void *pDc, uint32 uiColor)
{
    uint32 uiPrevious = s_uiForeground;
    fs98_put("foreground %u %u\n", D32(pDc, 4), uiColor);
    s_uiForeground = uiColor;
    return uiPrevious;
}

static uint32 fs98_background(void *pDc, uint32 uiColor)
{
    uint32 uiPrevious = s_uiBackground;
    fs98_put("background %u %u\n", D32(pDc, 4), uiColor);
    s_uiBackground = uiColor;
    return uiPrevious;
}

static int32 STDCALL fs98_text_out(uint32 uiDc, int32 iX, int32 iY, uint32 uiOptions,
                                  const struct fs98_alerts_rect *pRect, const char *pszText,
                                  uint32 uiLength, const int32 *pSpacing)
{
    fs98_put("text %u %d %d %u rect=%d,%d,%d,%d len=%u %s spacing=%u colors=%u,%u\n",
             uiDc, iX, iY, uiOptions, pRect->iLeft, pRect->iTop, pRect->iRight,
             pRect->iBottom, uiLength, pszText, (uint32)pSpacing, s_uiForeground, s_uiBackground);
    return 1;
}

static int32 STDCALL fs98_help(uint32 uiWindow, const char *pszFile,
                              uint32 uiCommand, uint32 uiContext)
{
    fs98_put("help %u %s %u %u\n", uiWindow, pszFile, uiCommand, uiContext);
    return (int32)(s_uiSeed & 1);
}

/* Regular-C service adapters for WINAPI imports. */
static int32 fs98_core_rect(uint32 uiWindow, struct fs98_alerts_rect *pRect)
{ return fs98_client_rect(uiWindow, pRect); }
static int32 fs98_core_screen(uint32 uiWindow, struct fs98_alerts_point *pPoint)
{ return fs98_client_screen(uiWindow, pPoint); }
static int32 fs98_core_client(uint32 uiWindow, struct fs98_alerts_point *pPoint)
{ return fs98_screen_client(uiWindow, pPoint); }
static int32 fs98_core_extent(uint32 uiDc, const char *pszText, int32 iLength,
                              struct fs98_alerts_size *pSize)
{ return fs98_extent(uiDc, pszText, iLength, pSize); }
static int32 fs98_core_text(uint32 uiDc, int32 iX, int32 iY, uint32 uiOptions,
                            const struct fs98_alerts_rect *pRect, const char *pszText,
                            uint32 uiLength, const int32 *pSpacing)
{ return fs98_text_out(uiDc, iX, iY, uiOptions, pRect, pszText, uiLength, pSpacing); }
static int32 fs98_core_help(uint32 uiWindow, const char *pszFile, uint32 uiCommand, uint32 uiContext)
{ return fs98_help(uiWindow, pszFile, uiCommand, uiContext); }

static void fs98_base_exchange(void *pObject, void *pExchange)
{
    fs98_put("base.exchange object=%u exchange=%u\n",
             (uint32)((uint8 *)pObject - s_abyObject), D32(pExchange, 0));
}

static void fs98_ddx_control(void *pExchange, uint32 uiControl, void *pControl)
{
    fs98_put("ddx.control exchange=%u id=%u object=%u\n", D32(pExchange, 0),
             uiControl, (uint32)((uint8 *)pControl - s_abyObject));
    D32(pControl, 0x20) = 0x10000 + uiControl;
}

static void fs98_dib_construct(void *pDib)
{
    fs98_put("dib.construct\n");
    memset(pDib, 0, 0x70);
}

static void fs98_dib_destroy(void *pDib)
{
    fs98_put("dib.destroy %d %d\n", (int32)D32(pDib, 8), (int32)D32(pDib, 0xc));
}

static int32 fs98_base_init(void *pObject)
{
    fs98_put("base.init %u\n", D32(pObject, 0x20));
    return (int32)s_uiSeed;
}

static int32 STDCALL fs98_load_string(uint32 uiId, char *pszBuffer, uint32 uiCapacity)
{
    int iLength;
    fs98_put("load.string %u %u\n", uiId, uiCapacity);
    iLength = snprintf(pszBuffer, uiCapacity, "fixture resource:%08x", uiId);
    return iLength;
}

static void fs98_set_text(void *pControl, const char *pszText)
{
    fs98_put("set.text %u %s\n", (uint32)((uint8 *)pControl - s_abyObject), pszText);
    D32(pControl, 0x24) = (uint32)strlen(pszText);
}

static void fs98_init_text(void *pControl, void *pFont)
{
    fs98_put("init.text %u font=%u\n", (uint32)((uint8 *)pControl - s_abyObject),
             (uint32)((uint8 *)pFont - s_abyObject));
}

static int32 fs98_load_dib(void *pDib, int32 iId, uint32 uiModule, void *pPalette, uint32 uiFlags)
{
    fs98_put("load.dib %d module=%u palette=%u flags=%u\n", iId, uiModule,
             (uint32)((uint8 *)pPalette - s_abyObject), uiFlags);
    D32(pDib, 8) = s_uiSeed % 129;
    D32(pDib, 0xc) = (s_uiSeed >> 8) % 97;
    return (int32)(s_uiSeed & 1);
}

static int32 fs98_sprite(void *pDib, void *pDestination, int32 iX, int32 iY,
                         int32 iWidth, int32 iHeight, int32 iSourceX, int32 iSourceY,
                         uint32 uiRaster)
{
    fs98_put("sprite dst=%u xy=%d,%d wh=%d,%d src=%d,%d raster=%u dib=%u,%u\n",
             (uint32)((uint8 *)pDestination - s_abyObject), iX, iY, iWidth, iHeight,
             iSourceX, iSourceY, uiRaster, D32(pDib, 8), D32(pDib, 0xc));
    return 1;
}

static int32 fs98_show(void *pControl, int32 iShow)
{
    fs98_put("show object=%u flag=%d\n", (uint32)((uint8 *)pControl - s_abyObject), iShow);
    return 1;
}

static int32 fs98_core_load_string(uint32 uiId, char *pszBuffer, uint32 uiCapacity)
{ return fs98_load_string(uiId, pszBuffer, uiCapacity); }

static int32 fs98_window_string(uint32 uiModule, uint32 uiId, char *pszBuffer, int32 iCapacity)
{
    fs98_put("window.string module=%u id=%u capacity=%d\n", uiModule, uiId, iCapacity);
    return snprintf(pszBuffer, (uint32)iCapacity, "window resource:%08x", uiId);
}

static void fs98_button_text(void *pControl, const char *pszText)
{
    fs98_put("button.text %u %s\n", (uint32)((uint8 *)pControl - s_abyObject), pszText);
    D32(pControl, 0x24) = (uint32)strlen(pszText);
}

static int32 fs98_button_init(void *pControl, void *pFont, void *pAlternateFont, uint32 uiBitmap,
                              void *pDib, void *pPalette, uint32 uiFlags, uint32 uiColor,
                              uint32 uiModule, int32 iX, int32 iY, struct fs98_alerts_rect stRect)
{
    fs98_put("button.init obj=%u fonts=%u,%u bitmap=%u dib=%u pal=%u flags=%u color=%u "
             "module=%u xy=%d,%d rect=%d,%d,%d,%d\n",
             (uint32)((uint8 *)pControl - s_abyObject), (uint32)((uint8 *)pFont - s_abyObject),
             (uint32)((uint8 *)pAlternateFont - s_abyObject), uiBitmap,
             (uint32)((uint8 *)pDib - s_abyObject), (uint32)((uint8 *)pPalette - s_abyObject),
             uiFlags, uiColor, uiModule, iX, iY,
             stRect.iLeft, stRect.iTop, stRect.iRight, stRect.iBottom);
    return (int32)s_uiSeed;
}

static int32 fs98_set_checked(void *pControl, int32 iChecked)
{
    int32 iPrevious = (int32)D32(pControl, 0xbc);
    fs98_put("set.checked object=%u value=%d\n", (uint32)((uint8 *)pControl - s_abyObject), iChecked);
    D32(pControl, 0xbc) = (uint32)iChecked;
    return iPrevious;
}

static int32 THISCALL fs98_check_init(void *pThis, void *pFont, uint32 uiBitmap,
                                     void *pDib, void *pPalette, uint32 uiModule)
{
    fs98_put("check.init object=%u font=%u bitmap=%u dib=%u palette=%u module=%u\n",
             (uint32)((uint8 *)pThis - s_abyObject), (uint32)((uint8 *)pFont - s_abyObject),
             uiBitmap, (uint32)((uint8 *)pDib - s_abyObject),
             (uint32)((uint8 *)pPalette - s_abyObject), uiModule);
    return (int32)s_uiSeed;
}

static const struct fs98_alerts_services s_stServices = {
    fs98_string_construct, fs98_string_destroy, fs98_dc_construct, fs98_dc_destroy,
    fs98_stock, fs98_font, fs98_select, fs98_core_rect, fs98_core_screen,
    fs98_core_client, fs98_format, fs98_core_extent, fs98_text_color,
    fs98_background, fs98_core_text, fs98_core_help,
    fs98_base_exchange, fs98_ddx_control, fs98_dib_construct, fs98_dib_destroy,
    fs98_base_init, fs98_core_load_string, fs98_set_text, fs98_init_text,
    fs98_load_dib, fs98_sprite, fs98_show, fs98_window_string,
    fs98_button_text, fs98_button_init, fs98_set_checked
};

/* The retail imports use thiscall/stdcall; keep those adapters at the actual
 * PE IAT boundary so the dependency transcript is shared with the C slice. */
static void *THISCALL fs98_mfc_string(void *pThis)
{ fs98_string_construct(pThis); return pThis; }
static void THISCALL fs98_mfc_string_destroy(void *pThis)
{ fs98_string_destroy(pThis); }
static void *THISCALL fs98_mfc_dc(void *pThis, void *pWindow)
{ fs98_dc_construct(pThis, pWindow); return pThis; }
static void THISCALL fs98_mfc_dc_destroy(void *pThis)
{ fs98_dc_destroy(pThis); }
static uint32 STDCALL fs98_mfc_stock(int32 iObject)
{ return fs98_stock(iObject); }
static void *STDCALL fs98_mfc_font(uint32 uiFont)
{ return fs98_font(uiFont); }
static void *THISCALL fs98_mfc_select(void *pThis, void *pFont)
{ return fs98_select(pThis, pFont); }
static uint32 THISCALL fs98_mfc_text_color(void *pThis, uint32 uiColor)
{ return fs98_text_color(pThis, uiColor); }
static uint32 THISCALL fs98_mfc_background(void *pThis, uint32 uiColor)
{ return fs98_background(pThis, uiColor); }
static void THISCALL fs98_mfc_base_exchange(void *pThis, void *pExchange)
{ fs98_base_exchange(pThis, pExchange); }
static void STDCALL fs98_mfc_ddx(void *pExchange, uint32 uiControl, void *pControl)
{ fs98_ddx_control(pExchange, uiControl, pControl); }
static void *THISCALL fs98_mfc_dib_construct(void *pThis)
{ fs98_dib_construct(pThis); return pThis; }
static void THISCALL fs98_mfc_dib_destroy(void *pThis)
{ fs98_dib_destroy(pThis); }
static int32 THISCALL fs98_mfc_base_init(void *pThis)
{ return fs98_base_init(pThis); }
static void THISCALL fs98_mfc_set_text(void *pThis, const char *pszText)
{ fs98_set_text(pThis, pszText); }
static void THISCALL fs98_mfc_init_text(void *pThis, void *pFont)
{ fs98_init_text(pThis, pFont); }
static int32 THISCALL fs98_mfc_load_dib(void *pThis, int32 iId, uint32 uiModule,
                                      void *pPalette, uint32 uiFlags)
{ return fs98_load_dib(pThis, iId, uiModule, pPalette, uiFlags); }
static int32 THISCALL fs98_mfc_sprite(void *pThis, void *pDestination, int32 iX, int32 iY,
                                    int32 iWidth, int32 iHeight, int32 iSourceX,
                                    int32 iSourceY, uint32 uiRaster)
{ return fs98_sprite(pThis, pDestination, iX, iY, iWidth, iHeight, iSourceX, iSourceY, uiRaster); }
static int32 THISCALL fs98_mfc_show(void *pThis, int32 iShow)
{ return fs98_show(pThis, iShow); }
static int32 STDCALL fs98_mfc_window_string(uint32 uiModule, uint32 uiId,
                                           char *pszBuffer, int32 iCapacity)
{ return fs98_window_string(uiModule, uiId, pszBuffer, iCapacity); }
static void THISCALL fs98_mfc_button_text(void *pThis, const char *pszText)
{ fs98_button_text(pThis, pszText); }
static int32 THISCALL fs98_mfc_button_init(void *pThis, void *pFont, void *pAlternateFont,
                                         uint32 uiBitmap, void *pDib, void *pPalette,
                                         uint32 uiFlags, uint32 uiColor, uint32 uiModule,
                                         int32 iX, int32 iY, struct fs98_alerts_rect stRect)
{ return fs98_button_init(pThis, pFont, pAlternateFont, uiBitmap, pDib, pPalette,
                          uiFlags, uiColor, uiModule, iX, iY, stRect); }
static int32 THISCALL fs98_mfc_set_checked(void *pThis, int32 iChecked)
{ return fs98_set_checked(pThis, iChecked); }

static int32 THISCALL fs98_virtual_init(void *pThis, int32 iId, uint32 uiModule, uint32 uiFlags)
{
    fs98_put("virtual.init id=%d module=%u flags=%u object=%u\n", iId, uiModule,
             uiFlags, (uint32)((uint8 *)pThis - s_abyObject));
    return s_bInitSuccess;
}

static void THISCALL fs98_close(void *pThis)
{
    void *pDescriptor = PTR(pThis, 0x10dc);
    fs98_put("close answer=%u live=%u flag=%u\n", D32(pDescriptor, 0),
             D32(pDescriptor, 0x0c), D32(pDescriptor, 0x10));
    if (s_bReplaceVtable)
        PTR(pThis, 0) = s_apOtherVtable;
}

static void THISCALL fs98_destroy(void *pThis)
{
    fs98_put("destroy %u\n", D32(pThis, 0x20));
}

static void THISCALL fs98_other_destroy(void *pThis)
{
    fs98_put("other.destroy %u\n", D32(pThis, 0x20));
}

static const uint32 s_auiRetailEntries[] = {
    0x19c0, 0x19e0, 0x1a00, 0x2560, 0x25d0, 0x25b0, 0x25a0, 0x1db0,
    0x1450, 0x1b70, 0x2260, 0x1bc0, 0x14c0, 0x22f0
};
static const fs98_exchange_fn s_apOwnExchange[] = {
    fs98_alerts_modal_exchange, fs98_alerts_modeless_exchange, fs98_alerts_advisory_exchange
};
static const fs98_event_fn s_apOwnEntries[] = {
    fs98_alerts_modal_first, fs98_alerts_modal_second, fs98_alerts_modal_third,
    fs98_alerts_advisory_accept, fs98_alerts_advisory_quit,
    fs98_alerts_advisory_help, fs98_alerts_noop
};

static void fs98_patch(uint32 uiRva, void *pFunction)
{
    DWORD uiOldProtect;
    void **ppSlot = (void **)((uint8 *)s_pRetail + uiRva);
    if (!VirtualProtect(ppSlot, 4, PAGE_READWRITE, &uiOldProtect))
        exit(2);
    *ppSlot = pFunction;
    VirtualProtect(ppSlot, 4, uiOldProtect, &uiOldProtect);
}

static void fs98_load(void)
{
    char szPath[MAX_PATH];
    GetFullPathNameA("orig\\English\\Alerts.DLL", MAX_PATH, szPath, 0);
    s_pRetail = LoadLibraryExA(szPath, 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_pRetail) {
        fprintf(stderr, "retail Alerts load failed: %lu path=%s\n", GetLastError(), szPath);
        exit(2);
    }
    /* Validate reference layout before patching or executing its instructions. */
    if (D32(s_pRetail, 0x4060) != 0x80000009 ||
        D32(s_pRetail, 0x4018) != 2 || D32(s_pRetail, 0x4024) != 3 ||
        D32(s_pRetail, 0x4030) != 4 ||
        D32(s_pRetail, 0x401c) != (uint32)s_pRetail + 0x1130) {
        fprintf(stderr, "retail Alerts reference layout changed\n");
        exit(2);
    }
    fs98_patch(0x5300, fs98_mfc_string);
    fs98_patch(0x5304, fs98_mfc_string_destroy);
    fs98_patch(0x52b0, fs98_mfc_dc);
    fs98_patch(0x52b4, fs98_mfc_font);
    fs98_patch(0x52b8, fs98_mfc_select);
    fs98_patch(0x52bc, fs98_format);
    fs98_patch(0x52c0, fs98_mfc_text_color);
    fs98_patch(0x52c4, fs98_mfc_background);
    fs98_patch(0x52c8, fs98_mfc_dc_destroy);
    fs98_patch(0x5280, fs98_mfc_stock);
    fs98_patch(0x527c, fs98_extent);
    fs98_patch(0x5284, fs98_text_out);
    fs98_patch(0x5458, fs98_client_rect);
    fs98_patch(0x545c, fs98_client_screen);
    fs98_patch(0x5460, fs98_screen_client);
    fs98_patch(0x5454, fs98_help);
    fs98_patch(0x53f8, fs98_mfc_base_exchange);
    fs98_patch(0x52fc, fs98_mfc_ddx);
    fs98_patch(0x53e0, fs98_mfc_dib_construct);
    fs98_patch(0x53d8, fs98_mfc_dib_destroy);
    fs98_patch(0x53d4, fs98_mfc_base_init);
    fs98_patch(0x53dc, fs98_mfc_set_text);
    fs98_patch(0x53e8, fs98_mfc_init_text);
    fs98_patch(0x53ec, fs98_mfc_load_dib);
    fs98_patch(0x53f0, fs98_mfc_sprite);
    fs98_patch(0x52ac, fs98_mfc_show);
    fs98_patch(0x5450, fs98_mfc_window_string);
    fs98_patch(0x52f8, fs98_mfc_button_text);
    fs98_patch(0x5400, fs98_mfc_button_init);
    fs98_patch(0x5438, fs98_mfc_set_checked);
    PTR(s_abyHost, 0x2a0) = fs98_load_string;
    PTR(s_pRetail, 0x404c) = s_abyHost;
    D32(s_pRetail, 0x40a4) = 0x50050;
}

static LONG WINAPI fs98_fault(EXCEPTION_POINTERS *pException)
{
    fprintf(stderr, "FAULT scenario=%u seed=%08x side=%d entry=%d code=%08lx at=%p running=%d\n",
            s_uiScenario, s_uiSeed, s_iSide, s_iEntry,
            pException->ExceptionRecord->ExceptionCode,
            pException->ExceptionRecord->ExceptionAddress, s_bRunning);
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void fs98_prepare(void)
{
    uint32 uiIndex;
    static const uint32 s_auiEdges[] = {
        0, 1, 2, 99, 100, 101, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff
    };
    for (uiIndex = 0; uiIndex < OBJECT_SIZE; uiIndex++)
        s_abyObject[uiIndex] = (uint8)fs98_rnd();
    for (uiIndex = 0; uiIndex < DESCRIPTOR_SIZE; uiIndex++)
        s_abyDescriptor[uiIndex] = (uint8)fs98_rnd();
    PTR(s_abyObject, 0) = s_apVtable;
    PTR(s_abyObject, 0x10dc) = s_abyDescriptor;
    PTR(s_abyObject, 0x1494) = s_apCheckVtable;
    D32(s_abyObject, 0x20) = 0x10020;
    D32(s_abyObject, 0x1104) = 0x10030;
    D32(s_abyObject, 0x10e0) = 0x50060;
    D32(s_abyDescriptor, 8) = s_uiScenario % 7;
    if (s_uiScenario % 7 == 6)
        D32(s_abyDescriptor, 8) = 0xffffffff;
    switch (s_uiScenario % 6) {
    case 0: D32(s_abyDescriptor, 0x24) = 0; break;
    case 1: D32(s_abyDescriptor, 0x24) = 1; break;
    case 2: D32(s_abyDescriptor, 0x24) = 0xffff; break;
    case 3: D32(s_abyDescriptor, 0x24) = 0x80000000; break;
    case 4: D32(s_abyDescriptor, 0x24) = 0xffffffff; break;
    default: PTR(s_abyDescriptor, 0x24) = (void *)s_szMessage; break;
    }
    s_bInitSuccess = s_uiScenario % 9 != 0;
    D32(s_abyDescriptor, 0x0c) = s_uiScenario % 7 ? fs98_rnd() | 1 : 0;
    D32(s_abyDescriptor, 0x10) = s_uiScenario % 5 ? fs98_rnd() | 1 : 0;
    D32(s_abyDescriptor, 0x14) = s_auiEdges[s_uiScenario % 10];
    D32(s_abyDescriptor, 0x18) = s_auiEdges[(s_uiScenario / 10) % 10];
    D32(s_abyDescriptor, 0x1c) = s_auiEdges[(s_uiScenario / 100) % 10];
    D32(s_abyDescriptor, 0x20) = s_auiEdges[(s_uiScenario / 1000) % 10];
    if (s_uiScenario % 3 == 2) {
        D32(s_abyDescriptor, 0x14) = fs98_rnd();
        D32(s_abyDescriptor, 0x18) = fs98_rnd();
        D32(s_abyDescriptor, 0x1c) = fs98_rnd();
        D32(s_abyDescriptor, 0x20) = fs98_rnd();
    }
    /* A self-checking product-wrap witness: 100*INT_MAX wraps to 0xffffff9c,
     * then unsigned division by INT_MAX yields 1. A widened product gives
     * 100 instead. Broad random ranges usually clamp start to limit-1,
     * leaving a multiplier of one and never exercising this distinction. */
    if (s_uiScenario % 17 == 16) {
        D32(s_abyDescriptor, 0x0c) = 1;
        D32(s_abyDescriptor, 0x10) = 1;
        D32(s_abyDescriptor, 0x14) = 0x7fffffff;
        D32(s_abyDescriptor, 0x18) = 0x7fffffff;
        D32(s_abyDescriptor, 0x1c) = 0;
        D32(s_abyDescriptor, 0x20) = 100;
    }
    s_bReplaceVtable = s_uiScenario & 1;
    s_stRect.iLeft = (int32)(fs98_rnd() % 31) - 15;
    s_stRect.iTop = (int32)(fs98_rnd() % 31) - 15;
    s_stRect.iRight = (int32)(fs98_rnd() % 2049);
    s_stRect.iBottom = (int32)(fs98_rnd() % 129);
    s_stSize.iWidth = (int32)(fs98_rnd() % 251);
    s_stSize.iHeight = (int32)(fs98_rnd() % 101);
    s_iChildX = (int32)(fs98_rnd() % 1001) - 500;
    s_iChildY = (int32)(fs98_rnd() % 1001) - 500;
    s_iParentX = (int32)(fs98_rnd() % 1001) - 500;
    s_iParentY = (int32)(fs98_rnd() % 1001) - 500;
    memcpy(s_abyInitialObject, s_abyObject, OBJECT_SIZE);
    memcpy(s_abyInitialDescriptor, s_abyDescriptor, DESCRIPTOR_SIZE);
}

static void fs98_run(void)
{
    int32 iReturn;
    s_auiLogLength[s_iSide] = 0;
    s_aszLog[s_iSide][0] = 0;
    memcpy(s_abyObject, s_abyInitialObject, OBJECT_SIZE);
    memcpy(s_abyDescriptor, s_abyInitialDescriptor, DESCRIPTOR_SIZE);
    if (s_iEntry == 12 || s_iEntry == 14) {
        D32(s_abyObject, 0x10e4) = 0;
        D32(s_abyObject, 0x10e8) = 0;
        D32(s_abyObject, 0x10ec) = 0;
        D32(s_abyObject, 0x10f4) = 0xffffffff;
        D32(s_abyObject, 0x10f8) = 0xffffffff;
        D32(s_abyDescriptor, 8) = (s_uiSeed & 0xffffff00) | (s_uiScenario & 255);
        D32(s_abyDescriptor, 0x14) = D32(s_abyDescriptor, 0x24);
        D32(s_abyDescriptor, 0x10) = s_uiScenario % 7;
    }
    memset(&s_stString, 0xa5, sizeof(s_stString));
    s_uiForeground = 0x556677;
    s_uiBackground = 0x112233;
    s_bRunning = 1;
    if (s_iEntry >= 14) {
        int iInitEntry = s_iEntry == 14 ? 12 : s_iEntry == 15 ? 13 : 11;
        fs98_init_event_fn pInit = s_iSide ?
            (iInitEntry == 11 ? fs98_alerts_modeless_init : iInitEntry == 12 ?
             fs98_alerts_modal_init : fs98_alerts_advisory_init) :
            (fs98_init_event_fn)((uint8 *)s_pRetail + s_auiRetailEntries[iInitEntry]);
        int32 iReady = pInit(s_abyObject);
        fs98_put("sequence.init %d\n", iReady);
        if (iReady && s_iEntry == 14) {
            int iButton = (int)(s_uiScenario % 3);
            fs98_event_fn pEvent = s_iSide ? s_apOwnEntries[iButton] :
                (fs98_event_fn)((uint8 *)s_pRetail + s_auiRetailEntries[iButton]);
            pEvent(s_abyObject);
        } else if (iReady && s_iEntry == 15) {
            int iButton = s_uiScenario & 1 ? 3 : 4;
            fs98_event_fn pEvent = s_iSide ? s_apOwnEntries[iButton] :
                (fs98_event_fn)((uint8 *)s_pRetail + s_auiRetailEntries[iButton]);
            /* User edits the checkbox after init; the real SIMSUI setter's
             * +0xbc write is independently pinned in its retail callee. */
            fs98_set_checked(s_abyObject + 0x1494, (int32)(s_uiSeed ^ 0x5a5a5a5a));
            pEvent(s_abyObject);
        } else if (iReady) {
            fs98_tick_fn pTick = s_iSide ? fs98_alerts_progress_tick :
                (fs98_tick_fn)((uint8 *)s_pRetail + s_auiRetailEntries[7]);
            int bWasLive = D32(s_abyDescriptor, 0xc) != 0;
            fs98_put("sequence.tick %d\n", pTick(s_abyObject, s_uiSeed, ~s_uiSeed));
            if (bWasLive) {
                D32(s_abyDescriptor, 0xc) = 0;
                fs98_put("sequence.close %d\n", pTick(s_abyObject, 0, 0));
            }
        }
    } else if (s_iEntry >= 11) {
        fs98_init_event_fn pInit = s_iSide ?
            (s_iEntry == 11 ? fs98_alerts_modeless_init : s_iEntry == 12 ?
             fs98_alerts_modal_init : fs98_alerts_advisory_init) :
            (fs98_init_event_fn)((uint8 *)s_pRetail + s_auiRetailEntries[s_iEntry]);
        iReturn = pInit(s_abyObject);
        fs98_put("return %d\n", iReturn);
        if (s_iEntry == 12 && s_iSide == 0 && iReturn)
            s_auiButtonFlags[s_uiScenario & 255]++;
    } else if (s_iEntry >= 8) {
        fs98_exchange_fn pExchange = s_iSide ? s_apOwnExchange[s_iEntry - 8] :
            (fs98_exchange_fn)((uint8 *)s_pRetail + s_auiRetailEntries[s_iEntry]);
        s_auiExchange[0] = s_uiSeed;
        pExchange(s_abyObject, s_auiExchange);
    } else if (s_iEntry == 7) {
        fs98_tick_fn pTick = s_iSide ? fs98_alerts_progress_tick :
                            (fs98_tick_fn)((uint8 *)s_pRetail + s_auiRetailEntries[7]);
        iReturn = pTick(s_abyObject, s_uiSeed, ~s_uiSeed);
        fs98_put("return %d\n", iReturn);
        if (s_uiScenario % 17 == 16 && s_iSide == 0) {
            if (!strstr(s_aszLog[0], "format %3d%% 1\n")) {
                fprintf(stderr, "product-wrap witness not reached\n");
                exit(2);
            }
            s_uiProductWitness++;
        }
    } else {
        fs98_event_fn pEvent = s_iSide ? s_apOwnEntries[s_iEntry] :
                              (fs98_event_fn)((uint8 *)s_pRetail + s_auiRetailEntries[s_iEntry]);
        pEvent(s_abyObject);
    }
    s_bRunning = 0;
}

int main(int iArgc, char **ppszArgv)
{
    uint32 uiCount = iArgc > 1 ? (uint32)strtoul(ppszArgv[1], 0, 0) : 20000;
    uint32 uiInitialSeed = iArgc > 2 ? (uint32)strtoul(ppszArgv[2], 0, 0) : 0x41e2753b;
    uint32 uiMismatch = 0;
    if (uiCount == 0 || uiInitialSeed == 0)
        return 2;
    setvbuf(stdout, 0, _IONBF, 0);
    SetUnhandledExceptionFilter(fs98_fault);
    s_apVtable[0xec / 4] = fs98_close;
    s_apVtable[0x60 / 4] = fs98_destroy;
    s_apVtable[0xe4 / 4] = fs98_virtual_init;
    s_apCheckVtable[0xc4 / 4] = fs98_check_init;
    memcpy(s_apOtherVtable, s_apVtable, sizeof(s_apVtable));
    s_apOtherVtable[0x60 / 4] = fs98_other_destroy;
    fs98_alerts_set_services(&s_stServices);
    fs98_alerts_set_resource_module(0x50050);
    fs98_load();
    s_uiRng = uiInitialSeed;
    for (s_uiScenario = 0; s_uiScenario < uiCount; s_uiScenario++) {
        s_uiSeed = s_uiRng;
        fs98_prepare();
        if (!D32(s_abyDescriptor, 0xc)) s_uiClosed++;
        else if (!D32(s_abyDescriptor, 0x10)) s_uiDisabled++;
        else s_uiEnabled++;
        if ((int32)D32(s_abyDescriptor, 0x14) < 0) s_uiWrapped++;
        if (s_bInitSuccess) s_uiInitSuccess++; else s_uiInitFailure++;
        if (s_uiScenario % 6 == 5) s_uiDirectText++; else s_uiTextResources++;
        for (s_iEntry = 0; s_iEntry < 17; s_iEntry++) {
            s_iSide = 0;
            fs98_run();
            memcpy(s_abyRetailObject, s_abyObject, OBJECT_SIZE);
            memcpy(s_abyRetailDescriptor, s_abyDescriptor, DESCRIPTOR_SIZE);
            s_iSide = 1;
            fs98_run();
            if (strcmp(s_aszLog[0], s_aszLog[1]) ||
                memcmp(s_abyRetailObject, s_abyObject, OBJECT_SIZE) ||
                memcmp(s_abyRetailDescriptor, s_abyDescriptor, DESCRIPTOR_SIZE)) {
                fprintf(stderr, "DIFFER scenario=%u seed=%08x entry=%d\nRETAIL:\n%sOURS:\n%s",
                        s_uiScenario, s_uiSeed, s_iEntry, s_aszLog[0], s_aszLog[1]);
                uiMismatch++;
                goto done;
            }
        }
    }
done:
    printf("Alerts events: %u scenarios, %u event comparisons, %u differ; seed=%08x\n",
           s_uiScenario, s_uiScenario * 17, uiMismatch, uiInitialSeed);
    printf("progress enabled=%u disabled=%u closed=%u negative-current=%u\n",
           s_uiEnabled, s_uiDisabled, s_uiClosed, s_uiWrapped);
    printf("self-checked product-wrap witnesses=%u\n", s_uiProductWitness);
    printf("init success=%u failure=%u; text resource=%u direct=%u\n",
           s_uiInitSuccess, s_uiInitFailure, s_uiTextResources, s_uiDirectText);
    {
        uint32 uiFlags, uiSeen = 0;
        for (uiFlags = 0; uiFlags < 256; uiFlags++)
            if (s_auiButtonFlags[uiFlags]) uiSeen++;
        printf("modal low-byte flag combinations reached=%u/256\n", uiSeen);
    }
    return uiMismatch ? 1 : 0;
}
