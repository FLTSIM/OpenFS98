/* Alerts' command handlers and modeless tick, transliterated from the retail
 * 1997 reference. This is an implementation slice, not yet a drop-in DLL.
 * The external harness calls the original routines and these over identical
 * objects, descriptors and dependency scripts. See docs/modules/alerts.md. */
#include "alerts_internal.h"
#include "alerts_tables.inc"

static const struct fs98_alerts_services *s_pServices;
static uint32 s_uiResourceModule;

#define D32(pObject, iOffset) (*(uint32 *)((uint8 *)(pObject) + (iOffset)))
#define PTR(pObject, iOffset) (*(void **)((uint8 *)(pObject) + (iOffset)))

typedef void (FS98_ALERTS_THISCALL *fs98_alerts_virtual_fn)(void *pObject);

static void fs98_alerts_virtual(void *pObject, uint32 uiOffset)
{
    fs98_alerts_virtual_fn *ppFunctions = PTR(pObject, 0);
    ppFunctions[uiOffset / 4](pObject);
}

void fs98_alerts_set_services(const struct fs98_alerts_services *pServices)
{
    s_pServices = pServices;
}

void fs98_alerts_set_resource_module(uint32 uiModule)
{
    s_uiResourceModule = uiModule;
}

/* 0x223019c0 / 0x223019e0 / 0x22301a00: the button returns its stored bit,
 * not its control id. In particular an invisible button can carry zero. */
static void fs98_alerts_modal_answer(void *pObject, uint32 uiOffset)
{
    void *pDescriptor = PTR(pObject, 0x10dc);
    D32(pDescriptor, 0x0c) = D32(pObject, uiOffset);
    fs98_alerts_virtual(pObject, 0xec);
}

void FS98_ALERTS_THISCALL fs98_alerts_modal_first(void *pObject)
{
    fs98_alerts_modal_answer(pObject, 0x10e4);
}

void FS98_ALERTS_THISCALL fs98_alerts_modal_second(void *pObject)
{
    fs98_alerts_modal_answer(pObject, 0x10e8);
}

void FS98_ALERTS_THISCALL fs98_alerts_modal_third(void *pObject)
{
    fs98_alerts_modal_answer(pObject, 0x10ec);
}

/* 0x22302560: retain both stores, even though the second overwrites the first.
 * Descriptor pointers are reloaded as retail does before virtual Close. */
void FS98_ALERTS_THISCALL fs98_alerts_advisory_accept(void *pObject)
{
    void *pDescriptor = PTR(pObject, 0x10dc);
    D32(pDescriptor, 0x10) = D32(pObject, 0x1550);
    pDescriptor = PTR(pObject, 0x10dc);
    *(volatile uint32 *)pDescriptor = 1;
    pDescriptor = PTR(pObject, 0x10dc);
    *(volatile uint32 *)pDescriptor = 0;
    fs98_alerts_virtual(pObject, 0xec);
}

/* 0x223025d0. */
void FS98_ALERTS_THISCALL fs98_alerts_advisory_quit(void *pObject)
{
    void *pDescriptor = PTR(pObject, 0x10dc);
    D32(pDescriptor, 0x10) = D32(pObject, 0x1550);
    pDescriptor = PTR(pObject, 0x10dc);
    D32(pDescriptor, 0) = 0;
    pDescriptor = PTR(pObject, 0x10dc);
    D32(pDescriptor, 0x0c) = 1;
    fs98_alerts_virtual(pObject, 0xec);
}

/* 0x223025b0 / 0x223025a0. */
void FS98_ALERTS_THISCALL fs98_alerts_advisory_help(void *pObject)
{
    s_pServices->pWinHelp(D32(pObject, 0x20), "fs98.hlp", 8, 0xbc2);
}

void FS98_ALERTS_THISCALL fs98_alerts_noop(void *pObject)
{
    (void)pObject;
}

/* 0x22301450 / 0x22301b70 / 0x22302260. The control binding order is
 * observable through MFC and is deliberately not sorted by control id. */
void FS98_ALERTS_THISCALL fs98_alerts_modal_exchange(void *pObject, void *pExchange)
{
    s_pServices->pBaseExchange(pObject, pExchange);
    s_pServices->pDdxControl(pExchange, 0x7d1, (uint8 *)pObject + 0x10fc);
    s_pServices->pDdxControl(pExchange, 0x7d5, (uint8 *)pObject + 0x13ac);
    s_pServices->pDdxControl(pExchange, 0x7d4, (uint8 *)pObject + 0x12b0);
    s_pServices->pDdxControl(pExchange, 0x7d3, (uint8 *)pObject + 0x11b4);
}

void FS98_ALERTS_THISCALL fs98_alerts_modeless_exchange(void *pObject, void *pExchange)
{
    s_pServices->pBaseExchange(pObject, pExchange);
    s_pServices->pDdxControl(pExchange, 0x7d6, (uint8 *)pObject + 0x10e4);
    s_pServices->pDdxControl(pExchange, 0x7d2, (uint8 *)pObject + 0x1124);
}

void FS98_ALERTS_THISCALL fs98_alerts_advisory_exchange(void *pObject, void *pExchange)
{
    s_pServices->pBaseExchange(pObject, pExchange);
    s_pServices->pDdxControl(pExchange, 0x7da, (uint8 *)pObject + 0x10e8);
    s_pServices->pDdxControl(pExchange, 0x7d9, (uint8 *)pObject + 0x11e4);
    s_pServices->pDdxControl(pExchange, 0x7d1, (uint8 *)pObject + 0x12e0);
    s_pServices->pDdxControl(pExchange, 0x7d8, (uint8 *)pObject + 0x1398);
    s_pServices->pDdxControl(pExchange, 0x7d7, (uint8 *)pObject + 0x1494);
}

/* 0x22301bc0: initialise the modeless text/icon and progress visibility.
 * CDIBSection's width/height are the dwords at +8/+0xc of its 0x70 bytes. */
int32 FS98_ALERTS_THISCALL fs98_alerts_modeless_init(void *pObject)
{
    typedef int32 (FS98_ALERTS_THISCALL *fs98_init_fn)(void *pThis, int32 iId,
                                                     uint32 uiModule, uint32 uiFlags);
    uint8 abyDib[0x70];
    char szBuffer[0x200];
    void *pDescriptor;
    const char *pszText;
    fs98_init_fn *ppVtable;
    uint32 uiText, uiTag;
    int32 iIcon;
    s_pServices->pDibConstruct(abyDib);
    s_pServices->pBaseInit(pObject);
    ppVtable = PTR(pObject, 0);
    if (!ppVtable[0xe4 / 4](pObject, 0x14f, s_uiResourceModule, 9)) {
        s_pServices->pDibDestroy(abyDib);
        return 0;
    }
    pDescriptor = PTR(pObject, 0x10dc);
    uiText = D32(pDescriptor, 0x24);
    pszText = (const char *)uiText;
    if ((int32)uiText <= 0xffff) {
        s_pServices->pLoadString(uiText, szBuffer, 0x200);
        pszText = szBuffer;
    }
    s_pServices->pSetText((uint8 *)pObject + 0x1124, pszText);
    s_pServices->pInitText((uint8 *)pObject + 0x1124, (uint8 *)pObject + 0x220);
    pDescriptor = PTR(pObject, 0x10dc);
    uiTag = D32(pDescriptor, 8);
    iIcon = uiTag == 1 ? 0x86 : uiTag == 2 ? 0x87 : uiTag == 4 ? 0x89 : 0x88;
    if (uiTag)
        s_pServices->pLoadDib(abyDib, iIcon, D32(pObject, 0x10e0),
                             (uint8 *)pObject + 0x7e8, 0);
    pDescriptor = PTR(pObject, 0x10dc);
    if (D32(pDescriptor, 8))
        s_pServices->pSpriteCopy(abyDib, (uint8 *)pObject + 0xff4, 0x1e, 0x22,
                                (int32)D32(abyDib, 8), (int32)D32(abyDib, 0x0c),
                                0, 0, 0xffffffff);
    pDescriptor = PTR(pObject, 0x10dc);
    if (!D32(pDescriptor, 0x10))
        s_pServices->pShowWindow((uint8 *)pObject + 0x10e4, 0);
    s_pServices->pDibDestroy(abyDib);
    return 1;
}

/* 0x223014c0: modal text, low-eight-bit button selection, button surfaces,
 * and optional icon. The label ids come from the original jump tables. */
int32 FS98_ALERTS_THISCALL fs98_alerts_modal_init(void *pObject)
{
    typedef int32 (FS98_ALERTS_THISCALL *fs98_init_fn)(void *pThis, int32 iId,
                                                     uint32 uiModule, uint32 uiFlags);
    uint8 abyDib[0x70];
    char szText[0x200], szLabel[0x100];
    struct fs98_alerts_rect stZero = {0, 0, 0, 0};
    fs98_init_fn *ppVtable;
    void *pDescriptor;
    const char *pszText;
    uint32 uiText, uiMask, uiCount = 0, uiIndex, uiBit, uiValue, uiTag;
    int32 iIcon;
    s_pServices->pDibConstruct(abyDib);
    s_pServices->pBaseInit(pObject);
    ppVtable = PTR(pObject, 0);
    if (!ppVtable[0xe4 / 4](pObject, 0x14f, s_uiResourceModule, 9)) {
        s_pServices->pDibDestroy(abyDib);
        return 0;
    }
    pDescriptor = PTR(pObject, 0x10dc);
    uiText = D32(pDescriptor, 0x14);
    pszText = (const char *)uiText;
    if ((int32)uiText <= 0xffff) {
        s_pServices->pLoadString(uiText, szText, 0x200);
        pszText = szText;
    }
    s_pServices->pSetText((uint8 *)pObject + 0x10fc, pszText);
    s_pServices->pInitText((uint8 *)pObject + 0x10fc, (uint8 *)pObject + 0x220);
    for (uiMask = 1; uiMask < 0x100; uiMask <<= 1) {
        pDescriptor = PTR(pObject, 0x10dc);
        if ((D32(pDescriptor, 8) & uiMask) && uiCount < 3) {
            D32(pObject, 0x10e4 + uiCount * 4) = uiMask;
            uiCount++;
        }
    }
    if (D32(pObject, 0x10e8) == 0) {
        D32(pObject, 0x10e8) = D32(pObject, 0x10e4);
        D32(pObject, 0x10e4) = 0;
    }
    if (D32(pObject, 0x10ec) == 0) {
        D32(pObject, 0x10ec) = D32(pObject, 0x10e8);
        D32(pObject, 0x10e8) = D32(pObject, 0x10e4);
        D32(pObject, 0x10e4) = 0;
    }
    /* The listing's focus/default branch is dead: its stack variable starts
     * at -1 and can only be changed behind a test that it is already != -1.
     * No call can observe the omitted test. The two answer indices below
     * are separate live object fields and must still be written. */
    for (uiIndex = 0; uiIndex < 3; uiIndex++) {
        void *pButton = (uint8 *)pObject + 0x11b4 + uiIndex * 0xfc;
        uiValue = D32(pObject, 0x10e4 + uiIndex * 4);
        for (uiBit = 0; uiBit < 8; uiBit++)
            if (uiValue == (1u << uiBit))
                break;
        if (uiBit == 8) {
            s_pServices->pShowWindow(pButton, 0);
        } else {
            s_pServices->pWindowLoadString(D32(pObject, 0x10e0), s_auiButtonText[uiBit],
                                          szLabel, 0x100);
            s_pServices->pButtonText(pButton, szLabel);
            if (uiValue == 1 || uiValue == 2 || uiValue == 0x40)
                D32(pObject, 0x10f4) = uiIndex;
            else if (uiValue == 4 || uiValue == 0x80)
                D32(pObject, 0x10f8) = uiIndex;
        }
    }
    for (uiIndex = 0; uiIndex < 3; uiIndex++)
        s_pServices->pButtonInit((uint8 *)pObject + 0x11b4 + uiIndex * 0xfc,
                                 (uint8 *)pObject + 0x5a0, (uint8 *)pObject + 0x610,
                                 0x132, (uint8 *)pObject + 0xff4,
                                 (uint8 *)pObject + 0x7e8, 1, 0xffffffff,
                                 s_uiResourceModule, 1, 1, stZero);
    pDescriptor = PTR(pObject, 0x10dc);
    uiTag = D32(pDescriptor, 0x10);
    iIcon = uiTag == 1 ? 0x86 : uiTag == 2 ? 0x87 : uiTag == 4 ? 0x89 : 0x88;
    if (uiTag)
        s_pServices->pLoadDib(abyDib, iIcon, D32(pObject, 0x10e0),
                             (uint8 *)pObject + 0x7e8, 0);
    pDescriptor = PTR(pObject, 0x10dc);
    if (D32(pDescriptor, 0x10))
        s_pServices->pSpriteCopy(abyDib, (uint8 *)pObject + 0xff4, 0x1e, 0x22,
                                (int32)D32(abyDib, 8), (int32)D32(abyDib, 0xc),
                                0, 0, 0xffffffff);
    s_pServices->pDibDestroy(abyDib);
    return 1;
}

/* 0x223022f0: the advisory's fixed message, three buttons and suppression
 * checkbox. SIMSUI SetChecked writes checkbox +0xbc, i.e. object +0x1550. */
int32 FS98_ALERTS_THISCALL fs98_alerts_advisory_init(void *pObject)
{
    typedef int32 (FS98_ALERTS_THISCALL *fs98_init_fn)(void *pThis, int32 iId,
                                                     uint32 uiModule, uint32 uiFlags);
    typedef int32 (FS98_ALERTS_THISCALL *fs98_check_init_fn)(void *pThis, void *pFont,
                uint32 uiBitmap, void *pDib, void *pPalette, uint32 uiModule);
    uint8 abyString[4], abyDib[0x70];
    char szBuffer[0x200];
    struct fs98_alerts_rect stZero = {0, 0, 0, 0};
    fs98_init_fn *ppVtable;
    fs98_check_init_fn *ppCheckVtable;
    void *pCheck = (uint8 *)pObject + 0x1494;
    void *pDescriptor;
    uint32 uiIndex;
    s_pServices->pStringConstruct(abyString);
    s_pServices->pDibConstruct(abyDib);
    s_pServices->pBaseInit(pObject);
    ppVtable = PTR(pObject, 0);
    if (!ppVtable[0xe4 / 4](pObject, 0x14f, s_uiResourceModule, 9)) {
        s_pServices->pDibDestroy(abyDib);
        s_pServices->pStringDestroy(abyString);
        return 0;
    }
    s_pServices->pLoadString(0xf6f0, szBuffer, 0x200);
    s_pServices->pSetText((uint8 *)pObject + 0x12e0, szBuffer);
    s_pServices->pInitText((uint8 *)pObject + 0x12e0, (uint8 *)pObject + 0x220);
    /* The listing initializes OK, Help, Quit in descending object order. */
    for (uiIndex = 0; uiIndex < 3; uiIndex++) {
        uint32 uiOffset = uiIndex == 0 ? 0x1398 : uiIndex == 1 ? 0x11e4 : 0x10e8;
        s_pServices->pButtonInit((uint8 *)pObject + uiOffset,
                                 (uint8 *)pObject + 0x5a0, (uint8 *)pObject + 0x610,
                                 0x132, (uint8 *)pObject + 0xff4,
                                 (uint8 *)pObject + 0x7e8, 1, 0xffffffff,
                                 s_uiResourceModule, 1, 1, stZero);
    }
    ppCheckVtable = PTR(pCheck, 0);
    ppCheckVtable[0xc4 / 4](pCheck, (uint8 *)pObject + 0x4c0, 0x139,
                           (uint8 *)pObject + 0xff4, (uint8 *)pObject + 0x7e8,
                           s_uiResourceModule);
    s_pServices->pLoadDib(abyDib, 0x87, D32(pObject, 0x10e0),
                         (uint8 *)pObject + 0x7e8, 0);
    s_pServices->pSpriteCopy(abyDib, (uint8 *)pObject + 0xff4, 0x1e, 0x22,
                            (int32)D32(abyDib, 8), (int32)D32(abyDib, 0xc),
                            0, 0, 0xffffffff);
    pDescriptor = PTR(pObject, 0x10dc);
    s_pServices->pSetChecked(pCheck, (int32)D32(pDescriptor, 0x10));
    s_pServices->pDibDestroy(abyDib);
    s_pServices->pStringDestroy(abyString);
    return 1;
}

/* Use modulo-2^32 additions/subtractions before interpreting signed results.
 * Retail's drawing arithmetic is ordinary wrapping x86, not C signed UB. */
static int32 fs98_alerts_add(int32 iLeft, int32 iRight)
{
    return (int32)((uint32)iLeft + (uint32)iRight);
}

static int32 fs98_alerts_sub(int32 iLeft, int32 iRight)
{
    return (int32)((uint32)iLeft - (uint32)iRight);
}

/* 0x22301db0..0x22302090. Current/total comparisons are signed, but the
 * progress product wraps at 32 bits and its division is unsigned. The split
 * uses a separate signed product and truncating divide by 100. */
int32 FS98_ALERTS_THISCALL fs98_alerts_progress_tick(void *pObject,
                                                   uint32 uiWParam, uint32 uiLParam)
{
    uint8 abyString[4];
    uint8 abyDc[0x14];
    struct fs98_alerts_rect stRemaining, stFilled;
    struct fs98_alerts_size stText;
    void *pDescriptor = PTR(pObject, 0x10dc);
    void *pFont;
    const char *pszText;
    uint32 uiChild, uiPercent, uiStart, uiLimit, uiLength;
    int32 iCurrent, iTotal, iWidth, iHeight, iSplit, iX, iY;
    (void)uiWParam;
    (void)uiLParam;

    if (D32(pDescriptor, 0x0c) == 0) {
        /* Retail keeps the original vtable in edi across the first call. */
        fs98_alerts_virtual_fn *ppFunctions = PTR(pObject, 0);
        ppFunctions[0xec / 4](pObject);
        ppFunctions[0x60 / 4](pObject);
        return 0;
    }
    if (D32(pDescriptor, 0x10) == 0)
        return 0;

    s_pServices->pStringConstruct(abyString);
    pDescriptor = PTR(pObject, 0x10dc);
    iCurrent = (int32)D32(pDescriptor, 0x14);
    iTotal = (int32)D32(pDescriptor, 0x18);
    s_pServices->pDcConstruct(abyDc, pObject);
    pFont = s_pServices->pFontFromHandle(s_pServices->pStockObject(13));
    s_pServices->pSelectFont(abyDc, pFont);

    uiChild = D32(pObject, 0x1104);
    s_pServices->pClientRect(uiChild, &stRemaining);
    iWidth = stRemaining.iRight;
    iHeight = stRemaining.iBottom;
    s_pServices->pClientToScreen(D32(pObject, 0x1104),
                                (struct fs98_alerts_point *)&stRemaining.iLeft);
    s_pServices->pClientToScreen(D32(pObject, 0x1104),
                                (struct fs98_alerts_point *)&stRemaining.iRight);
    s_pServices->pScreenToClient(D32(pObject, 0x20),
                                (struct fs98_alerts_point *)&stRemaining.iLeft);
    s_pServices->pScreenToClient(D32(pObject, 0x20),
                                (struct fs98_alerts_point *)&stRemaining.iRight);
    stRemaining.iRight = fs98_alerts_add(stRemaining.iLeft, fs98_alerts_sub(iWidth, 1));
    stRemaining.iBottom = fs98_alerts_add(stRemaining.iTop, fs98_alerts_sub(iHeight, 1));

    if (iTotal <= 0)
        iTotal = 1;
    if (iCurrent > iTotal)
        iCurrent = iTotal;
    pDescriptor = PTR(pObject, 0x10dc);
    if (D32(pDescriptor, 0x20) > 100)
        D32(pDescriptor, 0x20) = 100;
    pDescriptor = PTR(pObject, 0x10dc);
    if (D32(pDescriptor, 0x20) == 0)
        D32(pDescriptor, 0x20) = 1;
    pDescriptor = PTR(pObject, 0x10dc);
    if (D32(pDescriptor, 0x1c) >= D32(pDescriptor, 0x20))
        D32(pDescriptor, 0x1c) = D32(pDescriptor, 0x20) - 1;
    stFilled = stRemaining;
    pDescriptor = PTR(pObject, 0x10dc);
    uiStart = D32(pDescriptor, 0x1c);
    uiLimit = D32(pDescriptor, 0x20);
    uiPercent = (uint32)((uiLimit - uiStart) * (uint32)iCurrent) / (uint32)iTotal + uiStart;
    iSplit = (int32)((uint32)fs98_alerts_sub(stRemaining.iRight, stRemaining.iLeft) * uiPercent);
    iSplit = fs98_alerts_add(stRemaining.iLeft, iSplit / 100);
    stRemaining.iLeft = iSplit;
    stFilled.iRight = iSplit;
    s_pServices->pFormat(abyString, "%3d%%", (int32)uiPercent);
    pszText = PTR(abyString, 0);
    uiLength = D32(pszText, -8);
    s_pServices->pTextExtent(D32(abyDc, 8), pszText, (int32)uiLength, &stText);

    s_pServices->pTextColor(abyDc, 0xffffff);
    s_pServices->pBackgroundColor(abyDc, 0x7f0000);
    pszText = PTR(abyString, 0);
    uiLength = D32(pszText, -8);
    iY = fs98_alerts_add(stFilled.iTop,
                       fs98_alerts_sub(fs98_alerts_sub(stFilled.iBottom, stText.iHeight), stFilled.iTop) / 2);
    iX = fs98_alerts_add(stFilled.iLeft,
                       fs98_alerts_sub(fs98_alerts_sub(stRemaining.iRight, stText.iWidth), stFilled.iLeft) / 2);
    s_pServices->pTextOut(D32(abyDc, 4), iX, iY, 6, &stFilled, pszText, uiLength, 0);
    s_pServices->pBackgroundColor(abyDc, 0xffffff);
    s_pServices->pTextColor(abyDc, 0x7f0000);
    pszText = PTR(abyString, 0);
    uiLength = D32(pszText, -8);
    /* The second pass uses the same uncut overall rectangle for centering. */
    iY = fs98_alerts_add(stFilled.iTop,
                       fs98_alerts_sub(fs98_alerts_sub(stFilled.iBottom, stText.iHeight), stFilled.iTop) / 2);
    iX = fs98_alerts_add(stFilled.iLeft,
                       fs98_alerts_sub(fs98_alerts_sub(stRemaining.iRight, stText.iWidth), stFilled.iLeft) / 2);
    s_pServices->pTextOut(D32(abyDc, 4), iX, iY, 6, &stRemaining, pszText, uiLength, 0);
    s_pServices->pDcDestroy(abyDc);
    s_pServices->pStringDestroy(abyString);
    return 0;
}
