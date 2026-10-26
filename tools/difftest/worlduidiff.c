/* Partial World native comparison: actual MFC/SIMSUI controls and GDI pixels. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/world/world_internal.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define PTR WORLD_PTR
#define U32 WORLD_U32
struct world_ui_import { uint32 uiId; void *pLink; };
struct world_ui_dialog { uint32 uiId; uint32 (SC *pfnCreate)(uint32, void *); uint32 uiFlags; };
struct world_ui_linkage { uint32 uiId; void (*pfnInit)(void), (*pfnDeinit)(void); void *pReserved; uint32 uiFlags; struct world_ui_dialog *pDialogs; };
static uint8 s_abyHost[0x340], s_abyGlobal[0x1400], s_abyApplication[0x400];
static HMODULE s_hMfc, s_hFll, s_hCurrent, s_hAlerts;
static struct world_ui_linkage *s_pAlertLinkage;
static int s_iAuxiliary, s_iForcePathError;
static uint32 s_uiSide, s_uiCase, s_uiCallbacks, s_uiLength, s_uiLogLength, s_uiChecks;
static uint8 s_abyDescriptor[0x30], s_abyRow[0x40];
static char s_acPath[160], s_acTitle[160], s_aacSnapshots[2][8192], s_aacLogs[2][8192];
static uint8 *s_apbyPixels[2];
static uint32 s_auiPixelLengths[2], s_auiPixelWidths[2], s_auiPixelHeights[2];
static uint32 s_uiRegistrations[2], s_uiReleases[2];
/* Module initialization uses actual Windows unwinding through both modules. */
#define PROBE_THROW 0xe0429814
static int s_iProbe;
static uint32 s_uiProbeCase, s_uiProbeLength, s_uiProbeSeh;
static char s_aacProbeLogs[2][64];
static uint8 s_abyProbeDynamic[0x40];
static void *s_apProbeJump[5];
static void world_probe_put(char cEvent)
{
    if (s_uiProbeLength >= 63) exit(2);
    s_aacProbeLogs[s_uiSide][s_uiProbeLength++] = cEvent;
    s_aacProbeLogs[s_uiSide][s_uiProbeLength] = 0;
}
static void *world_probe_allocate(uint32 uiSize)
{
    if (uiSize != 0x40) exit(2);
    world_probe_put('A');
    if (s_uiProbeCase == 2) RaiseException(PROBE_THROW, 0, 0, NULL);
    return s_uiProbeCase == 1 ? NULL : s_abyProbeDynamic;
}
static void *TC world_probe_construct(void *pObject, void *pExtension, int iSystem)
{
    if (pObject != s_abyProbeDynamic || !pExtension || iSystem) exit(2);
    world_probe_put('C');
    if (s_uiProbeCase == 3) RaiseException(PROBE_THROW, 0, 0, NULL);
    return pObject;
}
static void world_probe_delete(void *pObject)
{ if (pObject != s_abyProbeDynamic) exit(2); world_probe_put('D'); }
static EXCEPTION_DISPOSITION __cdecl world_probe_catch(EXCEPTION_RECORD *pRecord, void *pFrame, CONTEXT *pContext, void *pDispatcher)
{
    (void)pContext; (void)pDispatcher;
    if (pRecord->ExceptionCode == PROBE_THROW && !(pRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(pFrame, NULL, pRecord, NULL); world_probe_put('X');
        __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiProbeSeh) : "memory");
        __builtin_longjmp(s_apProbeJump, 1);
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) void world_probe_run(struct world_ui_linkage *pLinkage)
{
    struct {void *pPrevious, *pHandler;} stCatch;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_uiProbeSeh));
    if (__builtin_setjmp(s_apProbeJump) == 0) {
        stCatch.pPrevious = (void *)s_uiProbeSeh; stCatch.pHandler = world_probe_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&stCatch) : "memory");
        pLinkage->pfnInit(); world_probe_put('N');
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiProbeSeh) : "memory");
    pLinkage->pfnDeinit(); pLinkage->pfnDeinit();
}
static void world_module_probe(struct world_ui_linkage **ppLinkages, HMODULE *phModules)
{
    const uint32 auiOrdinals[] = {823, 342, 825};
    void *apTargets[] = {world_probe_allocate, world_probe_construct, world_probe_delete};
    uint8 *apbyEntries[3], aabySaved[3][5];
    DWORD dwOld; uint32 uiIndex; int iDiffer = 0;
    static const char *s_apcExpected[] = {"RACNL", "RANL", "RAXL", "RACDXL"};
    for (uiIndex = 0; uiIndex < 3; ++uiIndex) {
        apbyEntries[uiIndex] = (void *)GetProcAddress(s_hMfc, (const char *)auiOrdinals[uiIndex]);
        if (!apbyEntries[uiIndex] || !VirtualProtect(apbyEntries[uiIndex], 5, PAGE_EXECUTE_READWRITE, &dwOld)) exit(2);
        memcpy(aabySaved[uiIndex], apbyEntries[uiIndex], 5); apbyEntries[uiIndex][0] = 0xe9;
        *(uint32 *)(apbyEntries[uiIndex] + 1) = (uint32)apTargets[uiIndex] - (uint32)apbyEntries[uiIndex] - 5;
        FlushInstructionCache(GetCurrentProcess(), apbyEntries[uiIndex], 5); VirtualProtect(apbyEntries[uiIndex], 5, dwOld, &dwOld);
    }
    s_iProbe = 1;
    for (s_uiProbeCase = 0; s_uiProbeCase < 4; ++s_uiProbeCase) {
        for (s_uiSide = 0; s_uiSide < 2; ++s_uiSide) {
            s_hCurrent = phModules[s_uiSide]; s_uiProbeLength = 0; s_aacProbeLogs[s_uiSide][0] = 0;
            world_probe_run(ppLinkages[s_uiSide]);
        }
        if (strcmp(s_aacProbeLogs[0], s_aacProbeLogs[1]) || strcmp(s_aacProbeLogs[0], s_apcExpected[s_uiProbeCase])) {
            fprintf(stderr, "WORLD INIT DIFFER case=%u retail=%s ours=%s expected=%s\n", s_uiProbeCase,
                s_aacProbeLogs[0], s_aacProbeLogs[1], s_apcExpected[s_uiProbeCase]); iDiffer = 1; break;
        }
    }
    s_iProbe = 0;
    for (uiIndex = 0; uiIndex < 3; ++uiIndex) {
        VirtualProtect(apbyEntries[uiIndex], 5, PAGE_EXECUTE_READWRITE, &dwOld);
        memcpy(apbyEntries[uiIndex], aabySaved[uiIndex], 5);
        FlushInstructionCache(GetCurrentProcess(), apbyEntries[uiIndex], 5); VirtualProtect(apbyEntries[uiIndex], 5, dwOld, &dwOld);
    }
    if (iDiffer) exit(1);
    printf("World native module init: 4 allocation/throw/double-deinit cases, 0 differ\n");
}

static HMODULE world_load(const char *pcPath)
{
    char acPath[MAX_PATH]; HMODULE hModule;
    GetFullPathNameA(pcPath, MAX_PATH, acPath, NULL); hModule = LoadLibraryA(acPath);
    if (!hModule) { fprintf(stderr, "LoadLibrary %s error=%lu\n", pcPath, GetLastError()); exit(2); }
    return hModule;
}
static LONG WINAPI world_fault(EXCEPTION_POINTERS *pException)
{
    fprintf(stderr, "World UI FAULT side=%u case=%u code=%lx pc=%p\n", s_uiSide, s_uiCase,
        pException->ExceptionRecord->ExceptionCode, pException->ExceptionRecord->ExceptionAddress);
    ExitProcess(3); return EXCEPTION_EXECUTE_HANDLER;
}
static void world_unknown(void)
{ fprintf(stderr, "World unknown host call from %p\n", __builtin_return_address(0)); ExitProcess(4); }
static int SC world_allocate(int iFlags, uint32 uiSize, void **ppOut, int iZero)
{ (void)iFlags; *ppOut = iZero ? calloc(1, uiSize) : malloc(uiSize); return *ppOut ? 0 : -1; }
static int SC world_free(void **ppOut) { free(*ppOut); *ppOut = NULL; return 0; }
static int SC world_reallocate(int iFlags, uint32 uiSize, void **ppOut)
{ (void)iFlags; *ppOut = realloc(*ppOut, uiSize); return *ppOut ? 0 : -1; }
static void SC world_register(void *pLinkage, HMODULE *phInstance)
{
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const char *)pLinkage, phInstance);
    if (!s_iAuxiliary) ++s_uiRegistrations[s_uiSide];
    if (s_iProbe) world_probe_put('R');
}
static void SC world_release(uint32 uiModule)
{ if (uiModule != (uint32)s_hCurrent) exit(2); if (!s_iAuxiliary) ++s_uiReleases[s_uiSide]; if (s_iProbe) world_probe_put('L'); }
static int SC world_string(uint32 uiId, char *pcBuffer, int iCapacity)
{
    int iLength = LoadStringA(s_hFll, uiId, pcBuffer, iCapacity);
    if (!iLength) iLength = LoadStringA(s_hCurrent, uiId, pcBuffer, iCapacity);
    if (!iLength) pcBuffer[0] = 0;
    return iLength;
}
static void *world_object(HWND hWindow)
{ return ((void *(SC *)(HWND))(void *)GetProcAddress(s_hMfc, (const char *)2864))(hWindow); }
static void world_application(void)
{
    HWND hMain = CreateWindowExA(0, "STATIC", "World harness", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);
    ((void *(TC *)(void *, const char *))(void *)GetProcAddress(s_hMfc, (const char *)561))(s_abyApplication, NULL);
    ((int (SC *)(HINSTANCE, HINSTANCE, char *, int))(void *)GetProcAddress(s_hMfc, (const char *)1575))(GetModuleHandleA(NULL), NULL, GetCommandLineA(), 0);
    PTR(s_abyApplication, 0x20) = world_object(hMain);
}
static void world_classes(void)
{
    WNDCLASSA stClass; HMODULE hCommon = LoadLibraryA("comctl32.dll");
    ((void (WINAPI *)(void))(void *)GetProcAddress(hCommon, "InitCommonControls"))();
    memset(&stClass, 0, sizeof(stClass)); stClass.style = CS_GLOBALCLASS;
    stClass.lpfnWndProc = DefWindowProcA; stClass.hInstance = GetModuleHandleA(NULL);
    stClass.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1); stClass.lpszClassName = "FS98DLG3D";
    if (!RegisterClassA(&stClass)) exit(2);
}
static void world_pump(void)
{
    MSG stMessage; uint32 uiCount = 0;
    while (PeekMessageA(&stMessage, NULL, 0, 0, PM_REMOVE) && ++uiCount < 1000) {
        TranslateMessage(&stMessage); DispatchMessageA(&stMessage);
    }
}
static uint32 SC world_check(void *pDescriptor)
{
    void *pRow = PTR(pDescriptor, 0xc); const char *pcPath = PTR(pRow, 4);
    const char *pcTitle = PTR(pRow, 0x18); int iLength;
    ++s_uiCallbacks;
    U32(pRow, 0) = s_iForcePathError || !*pcPath || strstr(pcPath, "invalid") ? 0x1007 : 0;
    U32(pRow, 0x14) = !*pcTitle ? 1 : 0;
    U32(pDescriptor, 0x10) = U32(pDescriptor, 0x14) < 1 ? 5 : U32(pDescriptor, 0x14) > U32(pDescriptor, 0x2c) ? 6 : 0;
    iLength = snprintf(s_aacLogs[s_uiSide] + s_uiLogLength, 8192 - s_uiLogLength,
        "check path=[%s] title=[%s] layer=%u active=%u position=%u statuses=%u/%u/%u\n",
        pcPath, pcTitle, U32(pRow, 0x2c), U32(pRow, 0x34), U32(pDescriptor, 0x14),
        U32(pRow, 0), U32(pRow, 0x14), U32(pDescriptor, 0x10));
    if (iLength < 0 || (uint32)iLength >= 8192 - s_uiLogLength) exit(2);
    s_uiLogLength += (uint32)iLength;
    return U32(pRow, 0) || U32(pRow, 0x14) || U32(pDescriptor, 0x10) ? 0xffffffff : 0;
}
static void world_fixture(void)
{
    memset(s_abyDescriptor, 0, sizeof(s_abyDescriptor)); memset(s_abyRow, 0, sizeof(s_abyRow));
    memset(s_acPath, 0, sizeof(s_acPath)); memset(s_acTitle, 0, sizeof(s_acTitle));
    PTR(s_abyDescriptor, 4) = world_check; PTR(s_abyDescriptor, 8) = "Fixture scenery area";
    PTR(s_abyDescriptor, 0xc) = s_abyRow; U32(s_abyDescriptor, 0x14) = 2; U32(s_abyDescriptor, 0x2c) = 12;
    PTR(s_abyRow, 4) = s_acPath; U32(s_abyRow, 8) = sizeof(s_acPath);
    PTR(s_abyRow, 0x18) = s_acTitle; U32(s_abyRow, 0x1c) = sizeof(s_acTitle);
    U32(s_abyRow, 0x2c) = (s_uiCase % 5 + 1) * 10;
    U32(s_abyRow, 0x34) = s_uiCase & 1; U32(s_abyRow, 0x3c) = !!(s_uiCase & 2);
    if (!(s_uiCase & 4)) strcpy(s_acPath, "C:\\fixture\\scenery\\area.bgl");
    strcpy(s_acTitle, "Fixture area"); s_uiCallbacks = s_uiLogLength = 0; s_aacLogs[s_uiSide][0] = 0;
}
static BOOL CALLBACK world_snapshot(HWND hWindow, LPARAM lParam)
{
    char acClass[80], acText[512]; RECT stRect; int iLength;
    GetClassNameA(hWindow, acClass, sizeof(acClass)); GetWindowTextA(hWindow, acText, sizeof(acText));
    GetWindowRect(hWindow, &stRect); MapWindowPoints(NULL, (HWND)lParam, (POINT *)&stRect, 2);
    iLength = snprintf(s_aacSnapshots[s_uiSide] + s_uiLength, 8192 - s_uiLength,
        "id=%x class=%s text=[%s] xy=%ld/%ld/%ld/%ld enabled=%d visible=%d limit=%ld\n",
        GetDlgCtrlID(hWindow), acClass, acText, stRect.left, stRect.top, stRect.right, stRect.bottom,
        IsWindowEnabled(hWindow), IsWindowVisible(hWindow),
        !strcmp(acClass, "Edit") ? (long)SendMessageA(hWindow, EM_GETLIMITTEXT, 0, 0) : 0L);
    if (iLength < 0 || (uint32)iLength >= 8192 - s_uiLength) exit(2);
    s_uiLength += (uint32)iLength; return TRUE;
}
static void world_pixels(HWND hWindow)
{
    RECT stRect; BITMAPINFO stInfo; HDC hSource = GetDC(hWindow), hMemory = CreateCompatibleDC(hSource);
    HBITMAP hBitmap, hPrevious; void *pBits; uint32 uiLength, uiIndex, uiPainted = 0;
    GetClientRect(hWindow, &stRect); memset(&stInfo, 0, sizeof(stInfo));
    stInfo.bmiHeader.biSize = sizeof(stInfo.bmiHeader); stInfo.bmiHeader.biWidth = stRect.right;
    stInfo.bmiHeader.biHeight = -stRect.bottom; stInfo.bmiHeader.biPlanes = 1;
    stInfo.bmiHeader.biBitCount = 32; stInfo.bmiHeader.biCompression = BI_RGB;
    if (stRect.right <= 0 || stRect.bottom <= 0 || stRect.right > 2048 || stRect.bottom > 2048) exit(2);
    uiLength = (uint32)stRect.right * stRect.bottom * 4;
    hBitmap = CreateDIBSection(hSource, &stInfo, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBitmap || !hMemory) exit(2);
    hPrevious = SelectObject(hMemory, hBitmap); memset(pBits, 0xa5, uiLength);
    BitBlt(hMemory, 0, 0, stRect.right, stRect.bottom, hSource, 0, 0, SRCCOPY); GdiFlush();
    for (uiIndex = 0; uiIndex < uiLength; ++uiIndex) if (((uint8 *)pBits)[uiIndex] != 0xa5) ++uiPainted;
    if (!uiPainted) exit(2);
    s_apbyPixels[s_uiSide] = realloc(s_apbyPixels[s_uiSide], uiLength);
    if (!s_apbyPixels[s_uiSide]) exit(2);
    memcpy(s_apbyPixels[s_uiSide], pBits, uiLength); s_auiPixelLengths[s_uiSide] = uiLength; s_auiPixelWidths[s_uiSide] = (uint32)stRect.right; s_auiPixelHeights[s_uiSide] = (uint32)stRect.bottom;
    SelectObject(hMemory, hPrevious); DeleteObject(hBitmap); DeleteDC(hMemory); ReleaseDC(hWindow, hSource);
}
static HWND s_hErrorOwner;
static UINT_PTR s_uiErrorTimer;
static uint32 s_uiErrorTicks, s_uiErrorSeen;
static char s_aacErrorSnapshots[2][8192];
static uint8 *s_apbyErrorPixels[2];
static uint32 s_auiErrorLengths[2];
static BOOL CALLBACK world_error_window(HWND hWindow, LPARAM lParam)
{
    char acClass[80]; uint32 uiLength;
    (void)lParam;
    GetClassNameA(hWindow, acClass, sizeof(acClass));
    if (hWindow == s_hErrorOwner || !IsWindowVisible(hWindow)) return TRUE;
    KillTimer(NULL, s_uiErrorTimer); s_uiErrorTimer = 0;
    SetForegroundWindow(hWindow); RedrawWindow(hWindow, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    s_uiLength = 0; s_aacSnapshots[s_uiSide][0] = 0;
    EnumChildWindows(hWindow, world_snapshot, (LPARAM)hWindow);
    strcpy(s_aacErrorSnapshots[s_uiSide], s_aacSnapshots[s_uiSide]);
    world_pixels(hWindow); uiLength = s_auiPixelLengths[s_uiSide];
    s_apbyErrorPixels[s_uiSide] = realloc(s_apbyErrorPixels[s_uiSide], uiLength);
    if (!s_apbyErrorPixels[s_uiSide]) exit(2);
    memcpy(s_apbyErrorPixels[s_uiSide], s_apbyPixels[s_uiSide], uiLength); s_auiErrorLengths[s_uiSide] = uiLength;
    ++s_uiErrorSeen; PostMessageA(hWindow, WM_COMMAND, 0x7d3, (LPARAM)GetDlgItem(hWindow, 0x7d3));
    return FALSE;
}
static void CALLBACK world_error_timer(HWND hWindow, UINT uiMessage, UINT_PTR uiTimer, DWORD dwTime)
{
    (void)hWindow; (void)uiMessage; (void)uiTimer; (void)dwTime;
    if (++s_uiErrorTicks > 200) { fprintf(stderr, "World error dialog did not appear\n"); ExitProcess(2); }
    EnumThreadWindows(GetCurrentThreadId(), world_error_window, 0);
}
static uint32 world_error_probe(HWND hWindow, uint32 uiStep)
{
    uint32 uiFocus;
    HWND hPath = GetDlgItem(hWindow, 0x1b05), hTitle = GetDlgItem(hWindow, 0x1b07), hPosition = GetDlgItem(hWindow, 0x1aef);
    SetWindowTextA(hPath, "C:\\fixture\\scenery\\valid.bgl");
    SetWindowTextA(hTitle, uiStep == 5 ? "" : "Error fixture");
    SetWindowTextA(hPosition, uiStep == 4 ? "9999" : "0003");
    s_hErrorOwner = hWindow; s_uiErrorTicks = s_uiErrorSeen = 0;
    s_uiErrorTimer = SetTimer(NULL, 0, 10, world_error_timer);
    /* A path can become invalid after editing (e.g. files removed). This
     * keeps the title enabled until OK, making its focus effect observable. */
    s_iForcePathError = uiStep == 3;
    if (uiStep == 3) {
        void *pObject = world_object(hWindow);
        void **ppVtable = PTR(pObject, 0);
        ((void (TC *)(void *))ppVtable[0xcc / 4])(pObject);
    } else SendMessageA(hWindow, WM_COMMAND, IDOK, 0);
    if (s_uiErrorTimer) KillTimer(NULL, s_uiErrorTimer);
    if (s_uiErrorSeen != 1 || !IsWindowVisible(hWindow) || GetWindowLongA(hWindow, GWL_USERDATA)) {
        fprintf(stderr, "World error/keep-open branch failed step=%u seen=%u\n", uiStep, s_uiErrorSeen); exit(2);
    }
    s_iForcePathError = 0;
    uiFocus = (uint32)GetDlgCtrlID(GetFocus());
    if (!s_uiSide && uiStep == 3 && uiFocus != 0x1b07) {
        fprintf(stderr, "retail focus witness missing: %x\n", uiFocus); exit(2);
    }
    SetWindowTextA(hPath, "C:\\fixture\\scenery\\fixed.bgl"); SetWindowTextA(hTitle, "Fixed title"); SetWindowTextA(hPosition, "0003");
    ++s_uiChecks;
    return uiFocus;
}

static void world_save_pixels(const char *pcKind, uint32 uiCycle, uint32 uiStep)
{
    char acPath[MAX_PATH]; BITMAPFILEHEADER stFile; BITMAPINFOHEADER stInfo; FILE *pFile;
    CreateDirectoryA("bin/world-diff/ui-images", NULL);
    snprintf(acPath, sizeof(acPath), "bin/world-diff/ui-images/%s-cycle%u-case%u-step%u-side%u.bmp", pcKind, uiCycle, s_uiCase, uiStep, s_uiSide);
    memset(&stFile, 0, sizeof(stFile)); memset(&stInfo, 0, sizeof(stInfo));
    stFile.bfType = 0x4d42; stFile.bfOffBits = sizeof(stFile) + sizeof(stInfo); stFile.bfSize = stFile.bfOffBits + s_auiPixelLengths[s_uiSide];
    stInfo.biSize = sizeof(stInfo); stInfo.biWidth = (LONG)s_auiPixelWidths[s_uiSide]; stInfo.biHeight = -(LONG)s_auiPixelHeights[s_uiSide];
    stInfo.biPlanes = 1; stInfo.biBitCount = 32; stInfo.biCompression = BI_RGB;
    pFile = fopen(acPath, "wb"); if (!pFile) exit(2);
    if (fwrite(&stFile, sizeof(stFile), 1, pFile) != 1 || fwrite(&stInfo, sizeof(stInfo), 1, pFile) != 1 ||
        fwrite(s_apbyPixels[s_uiSide], s_auiPixelLengths[s_uiSide], 1, pFile) != 1) exit(2);
    fclose(pFile);
}

static void world_ownership(HWND hWindow, HMODULE hExpected)
{
    void **ppVtable = PTR(world_object(hWindow), 0); HMODULE hOwner;
    const uint32 auiSlots[] = {4, 0x30, 0x8c, 0xc4, 0xcc, 0xd0, 0xd8, 0xdc}; uint32 uiIndex;
    for (uiIndex = 0; uiIndex < sizeof(auiSlots) / sizeof(auiSlots[0]); ++uiIndex) {
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (const char *)ppVtable[auiSlots[uiIndex] / 4], &hOwner) || hOwner != hExpected) {
            fprintf(stderr, "World dispatch ownership failure slot=%x\n", auiSlots[uiIndex]); exit(2);
        }
    }
}
static uint32 (SC *world_factory(struct world_ui_linkage *pLinkage, uint32 uiId))(uint32, void *)
{
    uint32 uiIndex;
    for (uiIndex = 0; pLinkage->pDialogs[uiIndex].uiId; ++uiIndex) {
        if (uiIndex > 64) exit(2);
        if (pLinkage->pDialogs[uiIndex].uiId == uiId) return pLinkage->pDialogs[uiIndex].pfnCreate;
    }
    exit(2);
}
static int SC world_modal(uint32 uiId, void *pDescriptor)
{
    HMODULE hPrevious = s_hCurrent;
    HWND hWindow; DWORD dwStarted;
    if (uiId != 2) { fprintf(stderr, "unexpected nested dialog %x\n", uiId); exit(2); }
    s_hCurrent = s_hAlerts;
    EnableWindow(s_hErrorOwner, FALSE);
    hWindow = (HWND)world_factory(s_pAlertLinkage, 2)(0, pDescriptor);
    dwStarted = GetTickCount();
    while (IsWindow(hWindow) && !GetWindowLongA(hWindow, GWL_USERDATA)) {
        world_pump(); Sleep(1);
        if (GetTickCount() - dwStarted > 5000) { fprintf(stderr, "nested alert timeout\n"); exit(2); }
    }
    DestroyWindow(hWindow); world_pump();
    EnableWindow(s_hErrorOwner, TRUE); SetForegroundWindow(s_hErrorOwner); SetActiveWindow(s_hErrorOwner); SetFocus(s_hErrorOwner);
    s_hCurrent = hPrevious;
    return 0;
}

struct world_ui_result { uint8 abyDescriptor[0x30], abyRow[0x40]; char acPath[160], acTitle[160]; uint32 uiCallbacks, uiValidate, uiClosed, uiFocus; };
int main(int iArgc, char **ppcArgv)
{
    HMODULE hUi, hGlobal, ahModules[2]; struct world_ui_linkage *apLinkages[2];
    char acDirectory[MAX_PATH]; uint32 uiIndex, uiCycle, uiStep, uiComparisons = 0;
    uint32 uiCases = iArgc > 2 ? strtoul(ppcArgv[2], NULL, 0) : 24;
    struct world_ui_result astResults[2];
    if (!uiCases || uiCases > 1000) return 2;
    setvbuf(stdout, NULL, _IONBF, 0); SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(world_fault);
    GetFullPathNameA("orig", MAX_PATH, acDirectory, NULL); SetDllDirectoryA(acDirectory);
    s_hMfc = world_load("cd/mfc42.dll"); world_application(); world_classes();
    hUi = world_load("orig/SIMSUI.DLL"); s_hFll = world_load("orig/ENGLISH.FLL");
    hGlobal = world_load("orig/Modules/GLOBAL.DLL"); memcpy(s_abyGlobal, GetProcAddress(hGlobal, "Linkage"), 4764);
    for (uiIndex = 0; uiIndex < sizeof(s_abyHost); uiIndex += 4) PTR(s_abyHost, uiIndex) = world_unknown;
    PTR(s_abyHost, 0x20c) = world_allocate; PTR(s_abyHost, 0x210) = world_reallocate; PTR(s_abyHost, 0x214) = world_free;
    PTR(s_abyHost, 0x10c) = world_modal; PTR(s_abyHost, 0x298) = world_register; PTR(s_abyHost, 0x29c) = world_release; PTR(s_abyHost, 0x2a0) = world_string;
    {
        struct world_ui_import *pImports = (void *)GetProcAddress(hUi, "ImportTable");
        pImports[0].pLink = s_abyHost; pImports[1].pLink = s_abyGlobal;
    }
    s_iAuxiliary = 1; s_hAlerts = world_load("orig/English/Alerts.DLL"); s_hCurrent = s_hAlerts;
    {
        struct world_ui_import *pImports = (void *)GetProcAddress(s_hAlerts, "ImportTable");
        pImports[0].pLink = s_abyHost; pImports[1].pLink = s_abyGlobal;
        s_pAlertLinkage = (void *)GetProcAddress(s_hAlerts, "Linkage"); s_pAlertLinkage->pfnInit();
    }
    s_iAuxiliary = 0;
    for (uiCycle = 0; uiCycle < 2; ++uiCycle) {
        ahModules[0] = world_load("orig/English/World.DLL");
        ahModules[1] = world_load(iArgc > 1 ? ppcArgv[1] : "bin/world-diff/WORLD_area.dll");
        if (ahModules[0] == ahModules[1]) return 2;
        for (s_uiSide = 0; s_uiSide < 2; ++s_uiSide) {
            struct world_ui_import *pImports = (void *)GetProcAddress(ahModules[s_uiSide], "ImportTable");
            pImports[0].pLink = s_abyHost; pImports[1].pLink = s_abyGlobal;
            apLinkages[s_uiSide] = (void *)GetProcAddress(ahModules[s_uiSide], "Linkage");
            if (apLinkages[s_uiSide]->uiId != 0x80000006) return 2;
        }
        world_module_probe(apLinkages, ahModules);
        for (s_uiSide = 0; s_uiSide < 2; ++s_uiSide) {
            s_hCurrent = ahModules[s_uiSide]; apLinkages[s_uiSide]->pfnInit();
        }
        for (s_uiCase = 0; s_uiCase < uiCases; ++s_uiCase) for (uiStep = 0; uiStep < 6; ++uiStep) {
            for (s_uiSide = 0; s_uiSide < 2; ++s_uiSide) {
                HWND hWindow; void *pObject; void **ppVtable; uint32 uiValidate = 0, uiFocus = 0;
                uint32 uiDialog = s_uiCase & 1 ? 0x13080003 : 0x13080004;
                s_hCurrent = ahModules[s_uiSide]; world_fixture();
                s_aacErrorSnapshots[s_uiSide][0] = 0; s_auiErrorLengths[s_uiSide] = 0;
                hWindow = (HWND)world_factory(apLinkages[s_uiSide], uiDialog)(0, s_abyDescriptor);
                if (!IsWindow(hWindow)) return 2;
                world_pump(); world_ownership(hWindow, ahModules[s_uiSide]);
                pObject = world_object(hWindow); ppVtable = PTR(pObject, 0);
                if (uiStep >= 1) {
                    SetWindowTextA(GetDlgItem(hWindow, 0x1b05), "C:\\fixture\\scenery\\edited.bgl");
                    SetWindowTextA(GetDlgItem(hWindow, 0x1b07), "Edited area");
                    SetWindowTextA(GetDlgItem(hWindow, 0x1aef), "0003");
                    SendMessageA(GetDlgItem(hWindow, 0x1ad8), CB_SETCURSEL, (s_uiCase + 1) % 5, 0);
                    SendMessageA(hWindow, WM_COMMAND, MAKEWPARAM(0x1b05, 0x400), (LPARAM)GetDlgItem(hWindow, 0x1b05));
                    if (uiStep == 2) {
                        SetWindowTextA(GetDlgItem(hWindow, 0x1b07), "");
                        uiValidate = ((int (TC *)(void *, uint32))ppVtable[0xd8 / 4])(pObject, 0);
                        if (uiValidate != 0x1b07) { fprintf(stderr, "World validation branch missing\n"); return 2; }
                        SetWindowTextA(GetDlgItem(hWindow, 0x1b07), "Valid title"); ++s_uiChecks;
                    }
                }
                if (uiStep >= 3) { uiFocus = world_error_probe(hWindow, uiStep); }
                s_uiLength = 0; s_aacSnapshots[s_uiSide][0] = 0;
                EnumChildWindows(hWindow, world_snapshot, (LPARAM)hWindow);
                BringWindowToTop(hWindow); SetForegroundWindow(hWindow); SetFocus(NULL);
                RedrawWindow(hWindow, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                world_pump(); world_pixels(hWindow);
                if (uiCycle == 0 && (s_uiCase == 0 || s_uiCase == 5) && uiStep == 0)
                    world_save_pixels("baseline", uiCycle, uiStep);
                {
                    MSG stKey;
                    memset(&stKey, 0, sizeof(stKey)); stKey.hwnd = hWindow; stKey.message = WM_KEYDOWN;
                    stKey.wParam = uiStep >= 1 && !(s_uiCase & 1) ? VK_RETURN : VK_ESCAPE; stKey.lParam = 1;
                    IsDialogMessageA(hWindow, &stKey);
                }
                if (!GetWindowLongA(hWindow, GWL_USERDATA)) SendMessageA(hWindow, WM_COMMAND, uiStep >= 1 && !(s_uiCase & 1) ? IDOK : IDCANCEL, 0);
                if (!IsWindow(hWindow) || IsWindowVisible(hWindow) || GetWindowLongA(hWindow, GWL_USERDATA) != 1) {
                    fprintf(stderr, "World close state invalid\n"); return 2;
                }
                memset(&astResults[s_uiSide], 0, sizeof(astResults[0]));
                memcpy(astResults[s_uiSide].abyDescriptor, s_abyDescriptor, sizeof(s_abyDescriptor));
                PTR(astResults[s_uiSide].abyDescriptor, 0x1c) = NULL; /* expired error-buffer address */
                memcpy(astResults[s_uiSide].abyRow, s_abyRow, sizeof(s_abyRow));
                memcpy(astResults[s_uiSide].acPath, s_acPath, sizeof(s_acPath)); memcpy(astResults[s_uiSide].acTitle, s_acTitle, sizeof(s_acTitle));
                astResults[s_uiSide].uiCallbacks = s_uiCallbacks; astResults[s_uiSide].uiValidate = uiValidate;
                astResults[s_uiSide].uiClosed = 1; astResults[s_uiSide].uiFocus = uiFocus;
                DestroyWindow(hWindow); world_pump();
                /* Area PostNcDestroy is empty; its caller owns deletion. */
                ((void *(TC *)(void *, uint32))ppVtable[1])(pObject, 1);
            }
            if (memcmp(&astResults[0], &astResults[1], sizeof(astResults[0])) || strcmp(s_aacSnapshots[0], s_aacSnapshots[1]) ||
                strcmp(s_aacLogs[0], s_aacLogs[1]) || s_auiPixelLengths[0] != s_auiPixelLengths[1] ||
                memcmp(s_apbyPixels[0], s_apbyPixels[1], s_auiPixelLengths[0]) ||
                strcmp(s_aacErrorSnapshots[0], s_aacErrorSnapshots[1]) || s_auiErrorLengths[0] != s_auiErrorLengths[1] ||
                (s_auiErrorLengths[0] && memcmp(s_apbyErrorPixels[0], s_apbyErrorPixels[1], s_auiErrorLengths[0]))) {
                for (s_uiSide = 0; s_uiSide < 2; ++s_uiSide) world_save_pixels("difference", uiCycle, uiStep);
                fprintf(stderr, "WORLD UI DIFFER cycle=%u case=%u step=%u state=%d controls=%d callbacks=%d pixels=%u/%u\nRETAIL:\n%s%sOURS:\n%s%s",
                    uiCycle, s_uiCase, uiStep, !!memcmp(&astResults[0], &astResults[1], sizeof(astResults[0])),
                    !!strcmp(s_aacSnapshots[0], s_aacSnapshots[1]), !!strcmp(s_aacLogs[0], s_aacLogs[1]),
                    s_auiPixelLengths[0], s_auiPixelLengths[1], s_aacSnapshots[0], s_aacLogs[0], s_aacSnapshots[1], s_aacLogs[1]);
                return 1;
            }
            ++uiComparisons;
        }
        for (s_uiSide = 0; s_uiSide < 2; ++s_uiSide) {
            s_hCurrent = ahModules[s_uiSide]; apLinkages[s_uiSide]->pfnDeinit(); apLinkages[s_uiSide]->pfnDeinit();
            if (s_uiRegistrations[s_uiSide] != (uiCycle + 1) * 5 || s_uiReleases[s_uiSide] != (uiCycle + 1) * 5) {
                fprintf(stderr, "World module lifetime mismatch\n"); return 1;
            }
            if (!FreeLibrary(ahModules[s_uiSide])) return 2;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (const char *)ahModules[s_uiSide], &s_hCurrent)) return 2;
        }
    }
    s_iAuxiliary = 1; s_hCurrent = s_hAlerts; s_pAlertLinkage->pfnDeinit(); FreeLibrary(s_hAlerts);
    printf("World native area: %u paired pixel/state/callback comparisons, 2 unload cycles, %u validation checks, 0 differ\n", uiComparisons, s_uiChecks);
    return 0;
}
