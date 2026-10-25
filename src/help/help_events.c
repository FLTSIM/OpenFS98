/* Help's dialog handlers; offsets describe the retail PE32 object layouts.
 * The service boundary preserves call order and lets tests replace mutable
 * descriptors and vtables at the same points as external MFC/Win32 calls. */
#include "help_internal.h"
#include "help_tables.inc"
#include "help_unwind.h"

#define D32(pObject, uiOffset) (*(uint32 *)((uint8 *)(pObject) + (uiOffset)))
#define PTR(pObject, uiOffset) (*(void **)((uint8 *)(pObject) + (uiOffset)))
#define AT(pObject, uiOffset) ((uint8 *)(pObject) + (uiOffset))
#define TC FS98_HELP_THISCALL
static const struct fs98_help_services *s_pServices;
static uint32 s_uiResourceModule;

void fs98_help_set_resource_module(uint32 uiModule) { s_uiResourceModule = uiModule; }

void fs98_help_set_services(const struct fs98_help_services *pServices)
{ s_pServices = pServices; }

/* These handlers fetch the close slot before either state write. */
static void fs98_help_answer(void *pObject, uint32 uiDescriptor, uint32 uiState, uint32 uiAnswer)
{
    void *pDescriptor = PTR(pObject, uiDescriptor);
    void *pVtable = PTR(pObject, 0);
    if (uiState) D32(pObject, uiState) = uiAnswer ? 1 : 2;
    D32(pDescriptor, 0) = uiAnswer;
    ((void (TC *)(void *))PTR(pVtable, 0xec))(pObject);
}
void TC fs98_help_about_cancel(void *pObject) { fs98_help_answer(pObject, 0x10dc, 0x1978, 0); }
void TC fs98_help_about_accept(void *pObject) { fs98_help_answer(pObject, 0x10dc, 0x1978, 1); }
void TC fs98_help_credits_cancel(void *pObject) { fs98_help_answer(pObject, 0x1174, 0x1498, 0); }
void TC fs98_help_credits_accept(void *pObject) { fs98_help_answer(pObject, 0x1174, 0x1498, 1); }
void TC fs98_help_lessons_cancel(void *pObject) { fs98_help_answer(pObject, 0x10dc, 0x187c, 0); }
void TC fs98_help_completion_cancel(void *pObject) { fs98_help_answer(pObject, 0x10dc, 0, 0); }
void TC fs98_help_completion_accept(void *pObject) { fs98_help_answer(pObject, 0x10dc, 0, 1); }

void TC fs98_help_about_credits(void *pObject)
{
    void *pDescriptor = PTR(pObject, 0x10dc);
    ((fs98_help_about_fn)PTR(pDescriptor, 0x10))(pDescriptor);
}

#define HELP_CLASS(szName, iBase) \
int32 TC fs98_help_##szName##_help(void *pObject, uint32 uiUnused, void *pInfo) \
{ \
    (void)pObject; (void)uiUnused; \
    return s_pServices->pWinHelp(D32(pInfo, 0xc), "fs98.hlp", 0xc, s_auiHelp_##szName); \
} \
int32 TC fs98_help_##szName##_context(void *pObject, uint32 uiWindow, uint32 uiUnused) \
{ \
    (void)pObject; (void)uiUnused; \
    s_pServices->pWinHelp(uiWindow, "fs98.hlp", 0xa, s_auiHelp_##szName); \
    return 1; \
} \
void TC fs98_help_##szName##_exchange(void *pObject, void *pExchange) \
{ \
    uint32 uiIndex; \
    if (iBase) s_pServices->pBaseExchange(pObject, pExchange); \
    for (uiIndex = 0; uiIndex < sizeof(s_aunDdx_##szName) / sizeof(s_aunDdx_##szName[0]); ++uiIndex) \
        s_pServices->pDdxControl(pExchange, s_aunDdx_##szName[uiIndex][1], \
                                 AT(pObject, s_aunDdx_##szName[uiIndex][0])); \
}
HELP_CLASS(about, 1)
HELP_CLASS(credits, 1)
HELP_CLASS(lessons, 1)
HELP_CLASS(graph, 0)
HELP_CLASS(completion, 0)
#undef HELP_CLASS

void TC fs98_help_disable(void *pObject) { s_pServices->pEnableWindow(D32(pObject, 0x20), 0); }
void TC fs98_help_enable(void *pObject) { s_pServices->pEnableWindow(D32(pObject, 0x20), 1); }
void TC fs98_help_empty(void *pObject) { (void)pObject; }
void TC fs98_help_empty_arg(void *pObject, uint32 uiUnused) { (void)pObject; (void)uiUnused; }
void TC fs98_help_post_destroy(void *pObject)
{
    if (pObject) ((void *(TC *)(void *, uint32))PTR(PTR(pObject, 0), 4))(pObject, 1);
}

/* 0x22331ef0: clear each handle after its dependency returns. */
void TC fs98_help_credits_close(void *pObject)
{
    uint32 uiHandle = D32(pObject, 0x10e8);
    if (uiHandle) s_pServices->pKillTimer(D32(pObject, 0x20), uiHandle);
    uiHandle = D32(pObject, 0x10ec);
    D32(pObject, 0x10e8) = 0;
    if (uiHandle) s_pServices->pDestroyWindow(uiHandle);
    uiHandle = D32(pObject, 0x110c);
    D32(pObject, 0x10ec) = 0;
    if (uiHandle) {
        s_pServices->pFreeLibrary(uiHandle);
        D32(pObject, 0x110c) = 0;
    }
    s_pServices->pClose(pObject);
}

/* 0x22331fe0: a failed ReadFile clears the byte count but STILL returns zero,
 * so RichEdit sees EOF rather than an EDITSTREAM callback error. */
uint32 FS98_HELP_STDCALL fs98_help_credits_read(uint32 uiCookie, void *pBuffer,
                                               int32 iCount, uint32 *pRead)
{
    if (!s_pServices->pReadFile(uiCookie, pBuffer, (uint32)iCount, pRead, 0)) *pRead = 0;
    return 0;
}

/* 0x22332010: the descriptor is deliberately replaced by embedded storage.
 * The synchronous EM_STREAMIN call owns the callback's file lifetime. */
void TC fs98_help_credits_load(void *pObject)
{
    uint8 abyInfo[0x88];
    void *pName = AT(pObject, 0x1124);
    int32 iFile;
    PTR(pObject, 0x1174) = AT(pObject, 0x1110);
    s_pServices->pCopyText(pName, "fltcred.rtf");
    PTR(PTR(pObject, 0x1174), 0xc) = pName;
    iFile = s_pServices->pOpenFile(PTR(PTR(pObject, 0x1174), 0xc), abyInfo, 0);
    if (iFile == -1) {
        s_pServices->pMessageBox("OpenFile failed", 0, 0);
        return;
    }
    D32(pObject, 0x10dc) = (uint32)iFile;
    D32(pObject, 0x10e0) = 0;
    PTR(pObject, 0x10e4) = fs98_help_credits_read;
    s_pServices->pSendMessage(D32(pObject, 0x10ec), 0x435, 0, 0x7d00);
    s_pServices->pSendMessage(D32(pObject, 0x10ec), 0x449, 2, (uint32)AT(pObject, 0x10dc));
    s_pServices->pSendMessage(D32(pObject, 0x10ec), 0xcf, 1, 0);
    s_pServices->pCloseHandle((uint32)iFile);
    D32(pObject, 0x10f0) = (uint32)s_pServices->pSendMessage(D32(pObject, 0x10ec), 0xba, 0, 0) - 6;
    s_pServices->pSendMessage(D32(pObject, 0x10ec), 0x443, 0, 0xffffff);
    fs98_help_credits_start_timer(pObject);
}

/* 0x22332130: timer ids 3..14, 500 ms, preserving every failed attempt. */
int32 TC fs98_help_credits_start_timer(void *pObject)
{
    uint32 uiTimer;
    for (uiTimer = 3; uiTimer < 15; ++uiTimer) {
        D32(pObject, 0x10e8) = s_pServices->pSetTimer(D32(pObject, 0x20), uiTimer, 500, 0);
        if (D32(pObject, 0x10e8)) break;
    }
    if (!D32(pObject, 0x10e8)) s_pServices->pMessageBox("SetTimer failed", 0, 0);
    return (int32)D32(pObject, 0x10e8);
}
void TC fs98_help_credits_timer(void *pObject, uint32 uiTimer)
{
    int32 iLine;
    if (uiTimer != D32(pObject, 0x10e8)) return;
    ++D32(pObject, 0x10f4);
    s_pServices->pSendMessage(D32(pObject, 0x10ec), 0x115, 1, 0);
    iLine = s_pServices->pSendMessage(D32(pObject, 0x10ec), 0xce, 0, 0);
    if (iLine > (int32)D32(pObject, 0x10f0)) {
        s_pServices->pSendMessage(D32(pObject, 0x10ec), 0x115, 6, 0);
        D32(pObject, 0x10f4) = 0;
    }
}
void TC fs98_help_credits_toggle(void *pObject)
{
    if (D32(pObject, 0x1108)) {
        s_pServices->pKillTimer(D32(pObject, 0x20), D32(pObject, 0x10e8));
        D32(pObject, 0x10e8) = 0;
    } else fs98_help_credits_start_timer(pObject);
    D32(pObject, 0x1108) ^= 1;
}

/* 0x22332790: the error helper at 0x2930 always returns zero, so its second
 * call is unreachable. Selection validity is tested before committing. */
void TC fs98_help_lessons_accept(void *pObject)
{
    if (s_pServices->pSendMessage(D32(pObject, 0x151c), 0x188, 0, 0) == -1) {
        ((void (TC *)(void *))PTR(PTR(pObject, 0), 0xd0))(pObject);
        return;
    }
    D32(pObject, 0x187c) = 1;
    fs98_help_lessons_sync(pObject);
    fs98_help_answer(pObject, 0x10dc, 0, 1);
}
void TC fs98_help_lessons_sync(void *pObject)
{
    int32 iSelection = s_pServices->pSendMessage(D32(pObject, 0x151c), 0x188, 0, 0);
    int32 iChecked;
    uint32 uiWindow;
    if (iSelection != -1) D32(PTR(pObject, 0x10dc), 0xc) = (uint32)iSelection;
    iChecked = s_pServices->pGetWindowLong(D32(pObject, 0x17a8), -21);
    uiWindow = D32(pObject, 0x17a8);
    D32(PTR(pObject, 0x10dc), 0x14) = uiWindow == (uint32)iChecked;
}
int32 TC fs98_help_lessons_no_error(void *pObject) { (void)pObject; return 0; }
int32 TC fs98_help_lessons_validate(void *pObject) { return PTR(pObject, 0x10dc) != 0; }

void TC fs98_help_lessons_populate(void *pObject)
{
    void *pDescriptor = PTR(pObject, 0x10dc);
    uint32 uiIndex;
    s_pServices->pSetChecked(AT(pObject, D32(pDescriptor, 0x14) ? 0x1788 : 0x1694), 1);
    s_pServices->pSendMessage(D32(pObject, 0x151c), 0x184, 0, 0);
    uiIndex = 0;
    pDescriptor = PTR(pObject, 0x10dc);
    while (uiIndex < D32(pDescriptor, 8)) {
        s_pServices->pSendMessage(D32(pObject, 0x151c), 0x180, 0,
                                (uint32)AT(PTR(pDescriptor, 0x10), uiIndex * (uint32)0xa08));
        ++uiIndex;
        pDescriptor = PTR(pObject, 0x10dc);
    }
    s_pServices->pSendMessage(D32(pObject, 0x151c), 0x186, 0, 0);
    pDescriptor = PTR(pObject, 0x10dc);
    if (D32(pDescriptor, 0xc) < D32(pDescriptor, 8))
        s_pServices->pSendMessage(D32(pObject, 0x151c), 0x186, D32(pDescriptor, 0xc), 0);
    fs98_help_lessons_select(pObject);
}

/* 0x22332950: the record is saved before the edit setter and host services,
 * so a replacement descriptor must not switch the chosen record mid-path. */
void TC fs98_help_lessons_select(void *pObject)
{
    char acPath[0x104], acRoot[0x100], acName[0x100];
    void *pDescriptor, *pRecord;
    int32 iSelection = s_pServices->pSendMessage(D32(pObject, 0x151c), 0x188, 0, 0);
    int32 iEnable;
    if (iSelection == -1) return;
    pDescriptor = PTR(pObject, 0x10dc);
    D32(pDescriptor, 0xc) = (uint32)iSelection;
    pDescriptor = PTR(pObject, 0x10dc);
    pRecord = AT(PTR(pDescriptor, 0x10), D32(pDescriptor, 0xc) * (uint32)0xa08);
    s_pServices->pSetText(AT(pObject, 0x162c), (char *)AT(pRecord, 0x100));
    s_pServices->pLoadHostString(0x9007, acRoot, 0x100);
    s_pServices->pSplitPath((char *)AT(pRecord, 0x900), 0, acName, acPath, 0);
    s_pServices->pJoinPath(acRoot, 0, acName, acRoot);
    s_pServices->pJoinPath(acPath, 0, acRoot, acPath);
    iEnable = s_pServices->pExists(acPath, 0x1d) == 0;
    s_pServices->pEnableControl(AT(pObject, 0x1788), iEnable);
    if (!iEnable) s_pServices->pSetChecked(AT(pObject, 0x1694), 1);
}
void TC fs98_help_lessons_double_click(void *pObject)
{
    fs98_help_lessons_select(pObject);
    ((void (TC *)(void *))PTR(PTR(pObject, 0), 0xcc))(pObject);
}

/* 0x22332c20/2de0: the graph callback's third argument is a saved HWND,
 * whereas the descriptor itself is reloaded immediately before the call. */
static void fs98_help_graph_call(void *pObject, uint32 uiOperation)
{
    void *pDescriptor = PTR(pObject, 0x10e0);
    uint32 uiWindow;
    D32(pObject, 0x10e4) = uiOperation;
    uiWindow = D32(pObject, 0x10dc);
    ((fs98_help_graph_fn)PTR(pDescriptor, 8))(uiOperation, pDescriptor, uiWindow);
}
int32 TC fs98_help_graph_tick(void *pObject, uint32 uiWParam, uint32 uiLParam)
{
    (void)uiWParam; (void)uiLParam;
    fs98_help_graph_call(pObject, 1);
    return 0;
}
void TC fs98_help_graph_notify(void *pObject) { fs98_help_graph_call(pObject, 0); }
void TC fs98_help_graph_accept(void *pObject)
{
    D32(pObject, 0x12cc) = 1;
    D32(PTR(pObject, 0x10e0), 0) = 1;
    fs98_help_graph_call(pObject, 2);
    ((void (TC *)(void *))PTR(PTR(pObject, 0), 0xec))(pObject);
}
int32 TC fs98_help_graph_validate(void *pObject)
{
    void *pDescriptor = PTR(pObject, 0x10e0);
    if (pDescriptor && (uint32)pDescriptor != 0xcdcdcdcd) return 1;
    s_pServices->pMessageBox("Bad m_pliveInfo\npassed to dialog by core code", 0, 0);
    return 0;
}

/* 0x22332ca0/2fa0: both ignore the base initialiser's return and each
 * control initialiser's return; only the virtual skin setup is an early exit. */
static int32 fs98_help_skin(void *pObject)
{
    return ((int32 (TC *)(void *, int32, uint32, uint32))PTR(PTR(pObject, 0), 0xe4))(
        pObject, 0x14f, s_uiResourceModule, 9);
}
static void fs98_help_button(void *pObject, uint32 uiOffset)
{
    struct fs98_help_rect oRect = {0, 0, 0, 0};
    s_pServices->pInitButton(AT(pObject, uiOffset), AT(pObject, 0x5a0), AT(pObject, 0x610),
                            0x132, AT(pObject, 0xff4), AT(pObject, 0x7e8), 1, 0xffffffff,
                            s_uiResourceModule, 1, 1, oRect);
}
int32 TC fs98_help_graph_init(void *pObject)
{
    void *pControl;
    struct fs98_help_rect oRect;
    s_pServices->pDialogInit(pObject);
    if (!fs98_help_skin(pObject)) return 0;
    fs98_help_button(pObject, 0x11d0);
    s_pServices->pInitArt(AT(pObject, 0x10e8), 0xf831, AT(pObject, 0xff4), AT(pObject, 0x7e8), 2);
    if (!fs98_help_graph_validate(pObject)) return 0;
    pControl = s_pServices->pGetControl(pObject, 0xee2);
    s_pServices->pRemapRect(&oRect, pControl);
    s_pServices->pMoveControl(pControl, oRect.iLeft, oRect.iTop,
                             (int32)((uint32)oRect.iRight - (uint32)oRect.iLeft),
                             (int32)((uint32)oRect.iBottom - (uint32)oRect.iTop), 1);
    fs98_help_graph_notify(pObject);
    return 1;
}
int32 TC fs98_help_completion_init(void *pObject)
{
    s_pServices->pBaseInit(pObject);
    if (!fs98_help_skin(pObject)) return 0;
    fs98_help_button(pObject, 0x1198);
    fs98_help_button(pObject, 0x1294);
    s_pServices->pInitText(AT(pObject, 0x10e0), AT(pObject, 0x220));
    return 1;
}

/* 0x22332570: font colors are changed only after the virtual skin succeeds.
 * The descriptor check is deliberately delayed until every control is ready. */
int32 TC fs98_help_lessons_init(void *pObject)
{
    void *pFont;
    void *pRadio;
    s_pServices->pBaseInit(pObject);
    if (!((int32 (TC *)(void *, int32, uint32, uint32))PTR(PTR(pObject, 0), 0xe4))(
            pObject, 0x14b, s_uiResourceModule, 7)) return 0;
    D32(pObject, 0x228) = 0;
    D32(pObject, 0x4c8) = 0;
    s_pServices->pInitArt(AT(pObject, 0x1544), 0xf831, AT(pObject, 0xff4), AT(pObject, 0x7e8), 2);
    fs98_help_button(pObject, 0x10e0);
    fs98_help_button(pObject, 0x11dc);
    s_pServices->pInitGroup(AT(pObject, 0x1448), AT(pObject, 0x220));
    s_pServices->pInitText(AT(pObject, 0x1390), AT(pObject, 0x370));
    s_pServices->pInitList(AT(pObject, 0x14fc), AT(pObject, 0x450), AT(pObject, 0x1390));
    s_pServices->pInitText(AT(pObject, 0x12d8), AT(pObject, 0x370));
    pFont = s_pServices->pBaseFont(2);
    s_pServices->pInitEdit(AT(pObject, 0x162c), pFont, 0xffffff, 0, 0xff0000, AT(pObject, 0x12d8), 0);
    pRadio = AT(pObject, 0x1694);
    ((int32 (TC *)(void *, void *, uint32, void *, void *, uint32))PTR(PTR(pRadio, 0), 0xc4))(
        pRadio, AT(pObject, 0x4c0), 0x134, AT(pObject, 0xff4), AT(pObject, 0x7e8), s_uiResourceModule);
    pRadio = AT(pObject, 0x1788);
    ((int32 (TC *)(void *, void *, uint32, void *, void *, uint32))PTR(PTR(pRadio, 0), 0xc4))(
        pRadio, AT(pObject, 0x4c0), 0x134, AT(pObject, 0xff4), AT(pObject, 0x7e8), s_uiResourceModule);
    if (!fs98_help_lessons_validate(pObject)) return 0;
    s_pServices->pSendMessage(D32(pObject, 0x164c), 0xc5, 0x3ff, 0);
    fs98_help_lessons_populate(pObject);
    return 1;
}

/* 0x22331d50: replace the template placeholder with a native RichEdit window.
 * LoadLibrary/CreateWindow failures are not checked before the stream load. */
int32 TC fs98_help_credits_init(void *pObject)
{
    struct fs98_help_rect oRect;
    uint32 uiInstance, uiWindow;
    void *pPlaceholder;
    s_pServices->pBaseInit(pObject);
    if (!fs98_help_skin(pObject)) return 0;
    s_pServices->pInitArt(AT(pObject, 0x13b0), 0xf831, AT(pObject, 0xff4), AT(pObject, 0x7e8), 2);
    fs98_help_button(pObject, 0x12b4);
    fs98_help_button(pObject, 0x1178);
    s_pServices->pCopyRect(&oRect, (void *)AT(pObject, 0x10f8));
    pPlaceholder = AT(pObject, 0x1274);
    s_pServices->pRemapRect(&oRect, pPlaceholder);
    *(struct fs98_help_rect *)AT(pObject, 0x10f8) = oRect;
    ((int32 (TC *)(void *))PTR(PTR(pPlaceholder, 0), 0x60))(pPlaceholder);
    s_pServices->pCommonControls();
    D32(pObject, 0x110c) = s_pServices->pLoadLibrary("RICHED32.DLL");
    uiInstance = D32(s_pServices->pModuleState(), 8);
    uiWindow = s_pServices->pCreateWindow(0x200, "RICHEDIT", "", 0x503000c4,
        (int32)D32(pObject, 0x10f8), (int32)D32(pObject, 0x10fc),
        (int32)(D32(pObject, 0x1100) - D32(pObject, 0x10f8)),
        (int32)(D32(pObject, 0x1104) - D32(pObject, 0x10fc)),
        D32(pObject, 0x20), 0xede, uiInstance, 0);
    D32(pObject, 0x10ec) = uiWindow;
    fs98_help_credits_load(pObject);
    return 1;
}

struct fs98_help_about_unwind {
    struct fs98_help_unwind oFrame;
    void *pString;
    volatile uint32 uiLive;
};
static void fs98_help_about_cleanup(struct fs98_help_unwind *pFrame)
{
    struct fs98_help_about_unwind *pState = (void *)pFrame;
    if (pState->uiLive) {
        pState->uiLive = 0;
        s_pServices->pStringDestroy(pState->pString);
    }
}
/* 0x22331950: one unused CString still has observable construction/cleanup.
 * The successful RegOpen handle is never closed in retail. The first value
 * is Name (99 bytes), the second ProductID (49 bytes), despite the screen's
 * field order. The published descriptor pointer expires on return. */
FS98_HELP_NOINLINE void fs98_help_about_populate_body(void *pObject, uint8 *pScratch)
{
    struct fs98_help_about_unwind oState;
    uint32 *pCapacity = (void *)(pScratch + 0xc), *pKey = (void *)(pScratch + 0x10);
    char *pVersion = (char *)pScratch + 0x18, *pBuild = (char *)pScratch + 0x2c;
    char *pProductId = (char *)pScratch + 0x40, *pPrefix = (char *)pScratch + 0x74;
    char *pName = (char *)pScratch + 0xa8, *pCopyright = (char *)pScratch + 0x10c;
    oState.pString = pScratch + 0x14; oState.uiLive = 0;
    fs98_help_unwind_enter(&oState.oFrame, fs98_help_about_cleanup);
    *pCapacity = 0x63;
    s_pServices->pStringConstruct(oState.pString);
    oState.uiLive = 1;
    if (s_pServices->pRegOpen(0x80000002,
            "Software\\Microsoft\\Microsoft Games\\Flight Simulator\\6.1", 0, 0x20019, pKey)) {
        s_pServices->pCopyText(pName, "Registry Error");
        s_pServices->pCopyText(pProductId, "Registry Error");
    } else {
        *pCapacity = 0x63;
        if (s_pServices->pRegQuery(*pKey, "Name", 0, 0, pName, pCapacity))
            s_pServices->pCopyText(pName, "Failed Registry");
        *pCapacity = 0x31;
        if (s_pServices->pRegQuery(*pKey, "ProductID", 0, 0, pProductId, pCapacity))
            s_pServices->pCopyText(pProductId, "Failed Registry");
    }
    PTR(PTR(pObject, 0x10dc), 8) = pName;
    s_pServices->pSetSimsText(AT(pObject, 0x1528), pName);
    s_pServices->pSetSimsText(AT(pObject, 0x15e0), pProductId);
    s_pServices->pLoadString(D32(pObject, 0x80), 0xf70b, pPrefix, 0x1d);
    s_pServices->pLoadString(D32(pObject, 0x80), 0xf6fd, pBuild, 0x13);
    s_pServices->pFormatVersion(pVersion, "%d.%02d      %s%d", 6, 10, pBuild, 0x1a2);
    s_pServices->pAppendText(pPrefix, pVersion);
    s_pServices->pSetSimsText(AT(pObject, 0x1198), pPrefix);
    s_pServices->pLoadString(D32(pObject, 0x80), 0xf70c, pCopyright, 0x1ff);
    s_pServices->pSetSimsText(AT(pObject, 0x10e0), pCopyright);
    fs98_help_about_cleanup(&oState.oFrame);
    fs98_help_unwind_leave(&oState.oFrame);
}

static struct fs98_help_about_state s_oAboutState;
static struct fs98_help_about_state *s_pAboutState = &s_oAboutState;
void fs98_help_set_about_state(struct fs98_help_about_state *pState) { s_pAboutState = pState; }
void fs98_help_about_font_destroy(void) { s_pServices->pFontDestroy(s_pAboutState->abyFont); }
/* 0x22331710: the initialized bit is committed before the base-font and
 * font-constructor calls. A throw does not clear it or schedule a destructor. */
int32 fs98_help_about_init_body(void *pObject, uint8 *pScratch)
{
    s_pServices->pBaseInit(pObject);
    if (!((int32 (TC *)(void *, int32, uint32, uint32))PTR(PTR(pObject, 0), 0xe4))(
            pObject, 0x13a, s_uiResourceModule, 1)) return 0;
    if (!(s_pAboutState->byInitialized & 1)) {
        void *pBaseFont;
        s_pAboutState->byInitialized |= 1;
        pBaseFont = s_pServices->pBaseFont(11);
        s_pServices->pFontConstruct(s_pAboutState->abyFont, pBaseFont);
        s_pServices->pRegisterExit(fs98_help_about_font_destroy);
    }
    D32(s_pAboutState->abyFont, 8) = 0xffffff;
    D32(s_pAboutState->abyFont, 0xc) = 0;
    D32(s_pAboutState->abyFont, 0x50) = 0;
    D32(s_pAboutState->abyFont, 0x54) = 1;
    s_pServices->pInitArt(AT(pObject, 0x1890), 0xf831, AT(pObject, 0xff4), AT(pObject, 0x7e8), 2);
    fs98_help_button(pObject, 0x1698);
    fs98_help_button(pObject, 0x1794);
    s_pServices->pInitText(AT(pObject, 0x10e0), s_pAboutState->abyFont);
    s_pServices->pInitText(AT(pObject, 0x1198), AT(pObject, 0x220));
    s_pServices->pInitText(AT(pObject, 0x1250), AT(pObject, 0x220));
    s_pServices->pInitText(AT(pObject, 0x1308), AT(pObject, 0x220));
    s_pServices->pInitGroup(AT(pObject, 0x13c0), AT(pObject, 0x220));
    s_pServices->pInitGroup(AT(pObject, 0x1474), AT(pObject, 0x220));
    s_pServices->pInitText(AT(pObject, 0x1528), AT(pObject, 0x220));
    s_pServices->pInitText(AT(pObject, 0x15e0), AT(pObject, 0x220));
    fs98_help_about_populate_body(pObject, pScratch);
    return 1;
}
