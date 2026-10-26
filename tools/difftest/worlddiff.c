/* Partial World area-event comparison. Retail executes its own mapped code;
 * only imported UI/CRT calls and virtual callbacks are replaced by fixtures. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_internal.h"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429811
struct world_fixture {
    uint8 abyBefore[32], abyObject[0x1b20];
    uint8 aabyDescriptors[2][0x30], aabyRows[2][0x40];
    char aacPaths[2][160], aacTitles[2][160];
    uint8 abyAfter[32];
};
static struct world_fixture s_stFixture, s_stInitial, s_stReference;
static HMODULE s_hRetail;
static void *s_apJump[5], *s_apVtable[60], *s_apOtherVtable[60];
static char s_aacLogs[2][8192];
static uint32 s_uiRng, s_uiScenario, s_uiOperation, s_uiCalls, s_uiLength;
static uint32 s_uiThrowAt, s_uiComparisons, s_uiThrows, s_uiSwaps, s_uiSelfChecks;
static int s_iSide, s_iSelection, s_iErrorResult, s_iValidateResult, s_iVirtualError;
static uint32 world_random(void)
{
    s_uiRng ^= s_uiRng << 13; s_uiRng ^= s_uiRng >> 17; s_uiRng ^= s_uiRng << 5;
    return s_uiRng;
}
static uint32 world_offset(const void *pVal)
{
    return (uint32)((const uint8 *)pVal - (const uint8 *)&s_stFixture);
}
static void world_log(const char *pcFormat, ...)
{
    va_list vaArgs;
    int iLength;
    va_start(vaArgs, pcFormat);
    iLength = vsnprintf(s_aacLogs[s_iSide] + s_uiLength, 8192 - s_uiLength, pcFormat, vaArgs);
    va_end(vaArgs);
    if (iLength < 0 || (uint32)iLength >= 8192 - s_uiLength) exit(2);
    s_uiLength += (uint32)iLength;
    if (++s_uiCalls == s_uiThrowAt) RaiseException(THROW_CODE, 0, 0, NULL);
    /* External calls may change the descriptor, row flags or virtual table. */
    if (s_uiCalls == 2 && s_uiScenario & 1) {
        PTR(s_stFixture.abyObject, 0x10e4) = s_stFixture.aabyDescriptors[1];
        if (!s_iSide) ++s_uiSwaps;
    }
    if (s_uiScenario & 2) {
        U32(s_stFixture.aabyRows[0], 0x3c) ^= 1;
        U32(s_stFixture.aabyRows[1], 0x3c) ^= 1;
    }
}
static int world_get_text(void *pControl, char *pcText, int iCapacity)
{
    const char *pcValue;
    static const char *s_apcNumbers[] = {"0", "-1", "2147483647", "-2147483648", "  +42x", "bad", "0000000020"};
    uint32 uiOffset = world_offset(pControl) - world_offset(s_stFixture.abyObject);
    pcValue = uiOffset == 0x19a0 ? s_apcNumbers[s_uiScenario % 7] : uiOffset == 0x1938 ? "C:\\area\\scenery\\test.bgl" : "Area title";
    world_log("get %x capacity=%d [%s]\n", uiOffset, iCapacity, pcValue);
    snprintf(pcText, (size_t)iCapacity, "%s", pcValue);
    return (int)strlen(pcText);
}
static int TC world_hook_get(void *pControl, char *pcText, int iCapacity)
{ return world_get_text(pControl, pcText, iCapacity); }
static void world_set_text(void *pControl, const char *pcText)
{ world_log("set %x [%s]\n", world_offset(pControl), pcText); }
static void TC world_hook_set(void *pControl, const char *pcText)
{ world_set_text(pControl, pcText); }
static int world_parse(const char *pcText)
{ world_log("parse [%s]\n", pcText); return atoi(pcText); }
static int world_format(char *pcText, const char *pcFormat, int iValue)
{ world_log("format [%s] %d\n", pcFormat, iValue); return sprintf(pcText, pcFormat, iValue); }
static int __cdecl world_hook_format(char *pcText, const char *pcFormat, int iValue)
{
    return world_format(pcText, pcFormat, iValue);
}
static int world_send(uint32 uiWindow, uint32 uiMessage, uint32 uiFirst, uint32 uiSecond)
{
    world_log("send %x %x %x %x\n", uiWindow, uiMessage, uiFirst, uiSecond);
    return s_iSelection;
}
static int SC world_hook_send(uint32 uiWindow, uint32 uiMessage, uint32 uiFirst, uint32 uiSecond)
{ return world_send(uiWindow, uiMessage, uiFirst, uiSecond); }
static int world_check_control(void *pControl, int iChecked)
{
    world_log("check %x %x\n", world_offset(pControl), (uint32)iChecked);
    U32(pControl, 0xbc) = (uint32)iChecked;
    return 0;
}
static int TC world_hook_check(void *pControl, int iChecked)
{ return world_check_control(pControl, iChecked); }
static int world_enable(void *pControl, int iEnabled)
{ world_log("enable %x %d\n", world_offset(pControl), iEnabled); return 0; }
static int TC world_hook_enable(void *pControl, int iEnabled)
{ return world_enable(pControl, iEnabled); }
static int world_error(void *pObject, void *pControl, void *pStatus)
{
    uint32 uiOffset = world_offset(pControl) - world_offset(pObject);
    if (uiOffset == 0x19a0) {
        world_log("error %x status=%x text=[%s] max=%u\n", uiOffset, U32(pStatus, 0), (char *)PTR(pStatus, 4), U32(pStatus, 8));
    } else world_log("error %x status=%x\n", uiOffset, (uint32)pStatus);
    return s_iErrorResult;
}
static int TC world_hook_error(void *pObject, void *pControl, void *pStatus)
{ return world_error(pObject, pControl, pStatus); }
static void world_focus_control(void *pControl)
{ world_log("focus.control %x\n", world_offset(pControl)); }
static void TC world_hook_focus_control(void *pControl)
{ world_focus_control(pControl); }
static uint32 SC world_check_descriptor(void *pDescriptor)
{
    world_log("callback %x\n", world_offset(pDescriptor));
    /* The return is ignored. Status records carry the result instead. */
    return 0xfedcba98;
}
static void TC world_close(void *pObject)
{ world_log("close %x answer=%u\n", world_offset(pObject), U32(PTR(pObject, 0x10e4), 0)); }
static void TC world_other_close(void *pObject)
{ world_log("other.close\n"); world_close(pObject); }
static int TC world_virtual_validate(void *pObject, uint32 uiValue)
{
    world_log("validate %x %x\n", world_offset(pObject), uiValue);
    PTR(pObject, 0) = s_apOtherVtable;
    return s_iValidateResult;
}
static int TC world_virtual_error(void *pObject, uint32 uiValue)
{ world_log("virtual.error %x %x\n", world_offset(pObject), uiValue); return s_iVirtualError; }
static int TC world_focus(void *pObject, uint32 uiValue)
{ world_log("focus %x %x\n", world_offset(pObject), uiValue); return -1; }
static const struct fs98_world_services s_stServices = {
    world_get_text, world_set_text, world_parse, world_format, world_send,
    world_check_control, world_enable, world_error, world_focus_control
};
static void world_patch(uint32 uiRva, void *pValue)
{
    DWORD dwOld;
    void **ppSlot = (void **)((uint8 *)s_hRetail + uiRva);
    if (!VirtualProtect(ppSlot, 4, PAGE_READWRITE, &dwOld)) exit(2);
    *ppSlot = pValue;
    if (!VirtualProtect(ppSlot, 4, dwOld, &dwOld)) exit(2);
}
static const uint32 s_auiEntries[] = {0xc360, 0xc440, 0xc2c0, 0xc2e0, 0xc610, 0xc660, 0xc720};
static void world_initialize(void)
{
    uint32 uiIndex, uiByte;
    static const uint32 s_auiLayers[] = {0, 9, 10, 11, 20, 30, 40, 50, 51, 0xffffffff};
    for (uiByte = 0; uiByte < sizeof(s_stFixture); ++uiByte) ((uint8 *)&s_stFixture)[uiByte] = (uint8)world_random();
    PTR(s_stFixture.abyObject, 0) = s_apVtable;
    PTR(s_stFixture.abyObject, 0x10e4) = s_stFixture.aabyDescriptors[0];
    for (uiIndex = 0; uiIndex < 2; ++uiIndex) {
        void *pDescriptor = s_stFixture.aabyDescriptors[uiIndex];
        void *pRow = s_stFixture.aabyRows[uiIndex];
        PTR(pDescriptor, 4) = world_check_descriptor;
        PTR(pDescriptor, 0xc) = pRow;
        U32(pDescriptor, 0x10) = s_uiScenario & 4 ? 6 : 0;
        U32(pDescriptor, 0x14) = world_random();
        PTR(pDescriptor, 0x1c) = NULL;
        U32(pDescriptor, 0x2c) = 23 + uiIndex;
        U32(pRow, 0) = s_uiScenario & 8 ? 0x1007 : 0;
        U32(pRow, 0x14) = s_uiScenario & 16 ? 0x1005 : 0;
        PTR(pRow, 4) = s_stFixture.aacPaths[uiIndex];
        PTR(pRow, 0x18) = s_stFixture.aacTitles[uiIndex];
        strcpy(s_stFixture.aacPaths[uiIndex], "Initial path");
        strcpy(s_stFixture.aacTitles[uiIndex], "Initial title");
        U32(pRow, 0x2c) = s_auiLayers[(s_uiScenario + uiIndex) % 10];
        U32(pRow, 0x3c) = s_uiScenario & 32 ? 1 : 0;
    }
    s_iSelection = (int)(s_uiScenario % 8) - 1;
    s_iErrorResult = s_uiScenario & 64 ? -1 : 0;
    s_iValidateResult = s_uiScenario & 128 ? 0x1b07 : 0;
    s_iVirtualError = s_uiScenario & 256 ? 1 : 0;
    s_stInitial = s_stFixture;
}
static void world_invoke(void)
{
    void *pObject = s_stFixture.abyObject;
    void *pEntry = (uint8 *)s_hRetail + s_auiEntries[s_uiOperation];
    int iResult = 0;
    if (!s_iSide) {
        if (s_uiOperation == 4 || s_uiOperation == 5)
            iResult = ((int (TC *)(void *, uint32))pEntry)(pObject, s_uiScenario);
        else ((void (TC *)(void *))pEntry)(pObject);
    } else switch (s_uiOperation) {
        case 0: fs98_world_area_collect(pObject); break;
        case 1: fs98_world_area_populate(pObject); break;
        case 2: fs98_world_area_cancel(pObject); break;
        case 3: fs98_world_area_accept(pObject); break;
        case 4: iResult = fs98_world_area_validate(pObject, s_uiScenario); break;
        case 5: iResult = fs98_world_area_error(pObject, s_uiScenario); break;
        case 6: fs98_world_area_path_changed(pObject); break;
    }
    if (!s_iSide && !s_uiThrowAt) {
        void *pDescriptor = PTR(pObject, 0x10e4);
        int iExpected = s_uiScenario & 16 ? 0x1b07 : s_uiScenario & 4 ? 0x1aef : 0;
        if ((s_uiOperation == 4 && iResult != iExpected) ||
            (s_uiOperation == 5 && iResult != -1) ||
            (s_uiOperation == 2 && U32(pDescriptor, 0) != 0) ||
            (s_uiOperation == 6 && s_uiCalls != 9) ||
            (s_uiOperation == 0 && s_iSelection >= 0 && s_iSelection <= 4 &&
                U32(s_stFixture.aabyRows[0], 0x2c) != (uint32)(s_iSelection + 1) * 10) ||
            (s_uiOperation == 0 && (s_iSelection < 0 || s_iSelection > 4) &&
                U32(s_stFixture.aabyRows[0], 0x2c) != U32(s_stInitial.aabyRows[0], 0x2c))) {
            fprintf(stderr, "fixture branch self-check failed scenario=%u operation=%u\n", s_uiScenario, s_uiOperation);
            exit(2);
        }
        ++s_uiSelfChecks;
    }
    /* A result is observable only for the two int-returning virtual methods. */
    world_log("return %x\n", (uint32)iResult);
}
static LONG CALLBACK world_exception(EXCEPTION_POINTERS *pException)
{
    if (pException->ExceptionRecord->ExceptionCode == THROW_CODE) {
        s_uiThrowAt = 0;
        if (!s_iSide) ++s_uiThrows;
        /* These seven routines have no local EH frames/owned temporaries. */
        __builtin_longjmp(s_apJump, 1);
    }
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u operation=%u\n",
        pException->ExceptionRecord->ExceptionCode, pException->ExceptionRecord->ExceptionAddress,
        s_iSide, s_uiScenario, s_uiOperation);
    ExitProcess(2);
}
static void world_run(uint32 uiThrowAt)
{
    uint32 uiIndex;
    s_stFixture = s_stInitial;
    s_uiCalls = s_uiLength = 0;
    s_aacLogs[s_iSide][0] = 0;
    s_uiThrowAt = uiThrowAt;
    if (__builtin_setjmp(s_apJump) == 0) world_invoke();
    else world_log("caught\n");
    if (memcmp(s_stFixture.abyBefore, s_stInitial.abyBefore, sizeof(s_stFixture.abyBefore)) ||
        memcmp(s_stFixture.abyAfter, s_stInitial.abyAfter, sizeof(s_stFixture.abyAfter))) {
        fprintf(stderr, "fixture guard changed side=%d scenario=%u operation=%u\n", s_iSide, s_uiScenario, s_uiOperation);
        exit(2);
    }
    /* Retail retains a pointer to its expired stack buffer. Compare the
     * pointed-to text inside Error, then normalize only that address. */
    for (uiIndex = 0; uiIndex < 2; ++uiIndex)
        if (PTR(s_stFixture.aabyDescriptors[uiIndex], 0x1c))
            PTR(s_stFixture.aabyDescriptors[uiIndex], 0x1c) = (void *)0x1234;
}
static int world_compare(uint32 uiThrowAt)
{
    uint32 uiByte;
    s_iSide = 0; world_run(uiThrowAt); s_stReference = s_stFixture;
    s_iSide = 1; world_run(uiThrowAt); ++s_uiComparisons;
    if (!memcmp(&s_stReference, &s_stFixture, sizeof(s_stFixture)) && !strcmp(s_aacLogs[0], s_aacLogs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u operation=%u throw=%u\n", s_uiScenario, s_uiOperation, uiThrowAt);
    for (uiByte = 0; uiByte < sizeof(s_stFixture); ++uiByte)
        if (((uint8 *)&s_stReference)[uiByte] != ((uint8 *)&s_stFixture)[uiByte]) {
            fprintf(stderr, "byte=%x retail=%x ours=%x\n", uiByte, ((uint8 *)&s_stReference)[uiByte], ((uint8 *)&s_stFixture)[uiByte]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_aacLogs[0], s_aacLogs[1]);
    return 1;
}
int main(int iArgc, char **ppcArgv)
{
    uint32 uiCount = iArgc > 1 ? strtoul(ppcArgv[1], NULL, 0) : 2000;
    uint32 uiSeed = iArgc > 2 ? strtoul(ppcArgv[2], NULL, 0) : 0x4a971b63;
    uint32 uiPosition, uiLimit;
    int iInterrupt = iArgc > 3 && !strcmp(ppcArgv[3], "interrupt");
    if (!uiCount || !uiSeed) return 2;
    s_uiRng = uiSeed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_exception);
    s_hRetail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_hRetail || GetProcAddress(s_hRetail, "Linkage") != (FARPROC)((uint8 *)s_hRetail + 0x24120)) {
        fprintf(stderr, "retail ownership failed: module=%p Linkage=%p error=%lu\n", s_hRetail,
            s_hRetail ? GetProcAddress(s_hRetail, "Linkage") : NULL, GetLastError());
        return 2;
    }
    world_patch(0x25598, world_hook_get); world_patch(0x255bc, world_hook_set);
    world_patch(0x255c8, world_hook_enable); world_patch(0x25590, world_hook_focus_control);
    world_patch(0x25778, world_hook_error); world_patch(0x2577c, world_hook_check);
    world_patch(0x258b0, world_hook_send); world_patch(0x256c0, world_parse);
    world_patch(0x256b8, world_hook_format);
    s_apVtable[0xd8 / 4] = world_virtual_validate;
    s_apVtable[0xdc / 4] = world_virtual_error;
    s_apVtable[0xe0 / 4] = world_focus;
    s_apVtable[0xec / 4] = world_close;
    memcpy(s_apOtherVtable, s_apVtable, sizeof(s_apVtable));
    s_apOtherVtable[0xec / 4] = world_other_close;
    fs98_world_set_services(&s_stServices);
    for (s_uiScenario = 0; s_uiScenario < uiCount; ++s_uiScenario) {
        for (s_uiOperation = 0; s_uiOperation < 7; ++s_uiOperation) {
            world_initialize();
            if (world_compare(0)) return 1;
            uiLimit = s_uiCalls - 1; /* Last call is the harness's return log. */
            if (iInterrupt) for (uiPosition = 1; uiPosition <= uiLimit; ++uiPosition)
                if (world_compare(uiPosition)) return 1;
        }
    }
    printf("World area core: %u scenarios, 7 operations, %u comparisons, 0 differ; seed=%x interrupts=%u swaps=%u self_checks=%u\n",
        uiCount, s_uiComparisons, uiSeed, s_uiThrows, s_uiSwaps, s_uiSelfChecks);
    FreeLibrary(s_hRetail);
    return 0;
}
