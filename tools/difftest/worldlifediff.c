/* Retail area-class lifetimes with real x86 exception unwinding. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/world/world_objects.h"
#include "../../src/world/world_tables.inc"
#define TC FS98_WORLD_THISCALL
#define SC FS98_WORLD_STDCALL
#define U32 WORLD_U32
#define PTR WORLD_PTR
#define THROW_CODE 0xe0429812
struct world_life_fixture {
    uint8 abyBefore[32], abyObject[0x1b20], abyAfter[32];
    uint8 abyDescriptor[0x30], abyExchange[32], abyHelp[32];
    uint32 uiModule, uiUiDeletes, uiDialogDeletes;
};
static struct world_life_fixture s_stFixture, s_stInitial, s_stReference;
static HMODULE s_hRetail;
static uint32 s_uiRng, s_uiSeed, s_uiScenario, s_uiOperation, s_uiCalls, s_uiThrowAt;
static uint32 s_uiLength, s_uiSavedSeh, s_uiComparisons, s_uiThrows, s_uiFaults, s_uiChecks, s_uiExits;
static int s_iSide, s_iTyped;
static void (SC *s_pfnThrow)(void *pObject, void *pInfo);
struct world_type_descriptor { void *pVtable, *pSpare; char acName[40]; };
static struct world_type_descriptor s_stBaseType = {NULL, NULL, ".PAVCException@@"};
static struct world_type_descriptor s_stDerivedType = {NULL, NULL, ".PAVCFixtureException@@"};
static struct world_type_descriptor s_stConstType = {NULL, NULL, ".PBVCException@@"};
static struct world_type_descriptor s_stOtherType = {NULL, NULL, ".PAX"};
static uint32 s_aauiCatchable[2][7], s_auiTypes[3], s_auiThrowInfo[4];
static char s_aacLogs[2][8192];
static void *s_apJump[5];
static uint32 world_random(void)
{ s_uiRng ^= s_uiRng << 13; s_uiRng ^= s_uiRng >> 17; s_uiRng ^= s_uiRng << 5; return s_uiRng; }
static uint32 world_offset(const void *pVal)
{ return (uint32)((const uint8 *)pVal - (const uint8 *)&s_stFixture); }
static void world_log(const char *pcFormat, ...)
{
    va_list vaArgs; int iLength;
    va_start(vaArgs, pcFormat);
    iLength = vsnprintf(s_aacLogs[s_iSide] + s_uiLength, 8192 - s_uiLength, pcFormat, vaArgs);
    va_end(vaArgs);
    if (iLength < 0 || (uint32)iLength >= 8192 - s_uiLength) exit(2);
    s_uiLength += (uint32)iLength;
    if (++s_uiCalls == s_uiThrowAt) {
        if (s_iTyped) {
            void *pException = s_uiScenario % 6 == 3 ? NULL : s_stFixture.abyDescriptor;
            s_pfnThrow(&pException, s_auiThrowInfo);
            exit(2);
        }
        RaiseException(THROW_CODE, 0, 0, NULL);
    }
    if (s_uiScenario & 1) {
        s_stFixture.uiModule ^= 0xa137be59;
        if (s_iSide) fs98_world_set_module(s_stFixture.uiModule);
        else U32(s_hRetail, 0x24974) = s_stFixture.uiModule;
    }
}
static void *world_base_construct(void *pObject, uint32 uiTemplate, void *pParent)
{
    world_log("base %x template=%x parent=%x\n", world_offset(pObject), uiTemplate, (uint32)pParent);
    U32(pObject, 0) = 0xba5e0000; U32(pObject, 4) = s_uiSeed;
    return pObject;
}
#define WORLD_BASE_HOOK(uiSlot) \
static void *TC world_hook_##uiSlot(void *pObject, uint32 uiTemplate, void *pParent) \
{ return world_base_construct(pObject, uiTemplate, pParent); }
#define WORLD_CTOR_HOOK(uiSlot) \
static void TC world_hook_##uiSlot(void *pObject) \
{ world_log("construct %x import=%x\n", world_offset(pObject), 0x##uiSlot); \
  U32(pObject, 0) = 0x##uiSlot; U32(pObject, 4) = s_uiSeed; }
#define WORLD_DTOR_HOOK(uiSlot) \
static void TC world_hook_##uiSlot(void *pObject) \
{ world_log("destroy %x import=%x marker=%x\n", world_offset(pObject), 0x##uiSlot, U32(pObject, 0)); \
  U32(pObject, 0) = 0x##uiSlot; }
#include "../../src/world/world_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
static void world_construct(void *pObject, uint32 uiImport)
{
    switch (uiImport) {
#define WORLD_BASE_HOOK(uiSlot)
#define WORLD_CTOR_HOOK(uiSlot) case 0x##uiSlot: world_hook_##uiSlot(pObject); break;
#define WORLD_DTOR_HOOK(uiSlot)
#include "../../src/world/world_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
        default: exit(2);
    }
}
static void world_destroy(void *pObject, uint32 uiImport)
{
    switch (uiImport) {
#define WORLD_BASE_HOOK(uiSlot)
#define WORLD_CTOR_HOOK(uiSlot)
#define WORLD_DTOR_HOOK(uiSlot) case 0x##uiSlot: world_hook_##uiSlot(pObject); break;
#include "../../src/world/world_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
        default: exit(2);
    }
}
static void *world_allocate(uint32 uiBytes)
{
    world_log("allocate %x\n", uiBytes);
    return s_uiScenario % 11 ? s_stFixture.abyObject : NULL;
}
static void world_ui_delete(void *pObject)
{ world_log("ui.delete %x\n", world_offset(pObject)); ++s_stFixture.uiUiDeletes; }
static void SC world_dialog_delete(void *pObject)
{ world_log("dialog.delete %x\n", world_offset(pObject)); ++s_stFixture.uiDialogDeletes; }
static void world_dialog_delete_service(void *pObject) { world_dialog_delete(pObject); }
static int world_create(void *pObject, uint32 uiTemplate, void *pParent)
{
    world_log("create %x template=%x parent=%x descriptor=%x module=%x\n", world_offset(pObject),
        uiTemplate, (uint32)pParent, world_offset(PTR(pObject, 0x10e4)), U32(pObject, 0x80));
    return s_uiScenario & 2 ? 0 : (int)(s_uiSeed | 1);
}
static int TC world_hook_create(void *pObject, uint32 uiTemplate, void *pParent)
{ return world_create(pObject, uiTemplate, pParent); }
static int world_show(void *pObject, int iShow)
{
    world_log("show %x %d\n", world_offset(pObject), iShow);
    U32(pObject, 0x20) = s_uiSeed ^ 0x9f781b27;
    return -1;
}
static int TC world_hook_show(void *pObject, int iShow) { return world_show(pObject, iShow); }
static int SC world_message(const char *pcText, uint32 uiFlags, uint32 uiHelp)
{ world_log("message [%s] flags=%x help=%x\n", pcText, uiFlags, uiHelp); return (int)s_uiSeed; }
static int world_message_service(const char *pcText, uint32 uiFlags, uint32 uiHelp)
{ return world_message(pcText, uiFlags, uiHelp); }
static void SC world_ddx(void *pExchange, uint32 uiId, void *pControl)
{
    world_log("ddx %x id=%x control=%x\n", world_offset(pExchange), uiId, world_offset(pControl));
    U32(pControl, 0x20) = uiId ^ s_uiSeed;
}
static void world_ddx_service(void *pExchange, uint32 uiId, void *pControl)
{ world_ddx(pExchange, uiId, pControl); }
static int SC world_help(uint32 uiWindow, const char *pcFile, uint32 uiCommand, const uint32 *puiMap)
{
    uint32 uiIndex;
    /* One boundary, one injectable call, but include every help-map pair. */
    char acMap[1024]; int iLength = 0;
    for (uiIndex = 0; uiIndex < 64; uiIndex += 2) {
        iLength += snprintf(acMap + iLength, sizeof(acMap) - (uint32)iLength,
            "%x:%x,", puiMap[uiIndex], puiMap[uiIndex + 1]);
        if (!puiMap[uiIndex] && !puiMap[uiIndex + 1]) break;
        if (iLength > 950) exit(2);
    }
    if (uiIndex == 64) exit(2);
    world_log("help %x [%s] command=%x map=[%s]\n", uiWindow, pcFile, uiCommand, acMap);
    return (int)s_uiSeed;
}
static int world_help_service(uint32 uiWindow, const char *pcFile, uint32 uiCommand, const uint32 *puiMap)
{ return world_help(uiWindow, pcFile, uiCommand, puiMap); }
static void world_land(void);
static void world_exit_cleanly(void *pException)
{
    s_uiThrowAt = 0;
    world_log("exit.cleanly %x\n", pException ? world_offset(pException) : 0);
    if (!s_iSide) ++s_uiExits;
    /* SIMSUI displays an error and aborts; terminate this comparison side. */
    world_land();
}
static const struct fs98_world_object_services s_stServices = {
    world_base_construct, world_construct, world_destroy, world_allocate,
    world_ui_delete, world_dialog_delete_service, world_create, world_show,
    world_message_service, world_ddx_service, world_exit_cleanly, world_help_service
};
static const uint32 s_auiEntries[] = {0xbb30, 0xbc90, 0xbc70, 0xbb30, 0x19f0, 0x1920, 0xbdc0, 0xc5d0, 0xc5f0};
static void world_patch(uint32 uiRva, void *pValue)
{
    DWORD dwOld; void **ppSlot = (void **)((uint8 *)s_hRetail + uiRva);
    if (!VirtualProtect(ppSlot, 4, PAGE_READWRITE, &dwOld)) exit(2);
    *ppSlot = pValue;
    if (!VirtualProtect(ppSlot, 4, dwOld, &dwOld)) exit(2);
}
static void world_initialize(void)
{
    uint32 uiByte;
    for (uiByte = 0; uiByte < sizeof(s_stFixture); ++uiByte)
        ((uint8 *)&s_stFixture)[uiByte] = (uint8)world_random();
    s_stFixture.uiModule = s_uiSeed;
    s_stFixture.uiUiDeletes = s_stFixture.uiDialogDeletes = 0;
    memset(s_aauiCatchable, 0, sizeof(s_aauiCatchable));
    memset(s_auiTypes, 0, sizeof(s_auiTypes));
    memset(s_auiThrowInfo, 0, sizeof(s_auiThrowInfo));
    s_aauiCatchable[0][0] = s_aauiCatchable[1][0] = 1;
    s_aauiCatchable[0][3] = s_aauiCatchable[1][3] = 0xffffffff;
    s_aauiCatchable[0][5] = s_aauiCatchable[1][5] = 4;
    s_aauiCatchable[0][1] = (uint32)(s_uiScenario % 6 == 2 ? &s_stOtherType :
        (s_uiScenario % 6 == 1 || s_uiScenario % 6 == 4) ? &s_stDerivedType :
        s_uiScenario % 6 == 5 ? &s_stConstType : &s_stBaseType);
    s_aauiCatchable[1][1] = (uint32)&s_stBaseType;
    s_aauiCatchable[1][2] = 8; /* derived-to-base pointer adjustment */
    if (s_uiScenario % 6 == 4) {
        /* Exercise the full PMD conversion through a virtual-base table. */
        PTR(s_stFixture.abyDescriptor, 4) = s_stFixture.abyHelp;
        U32(s_stFixture.abyHelp, 4) = 12;
        s_aauiCatchable[1][3] = 4;
        s_aauiCatchable[1][4] = 4;
    }
    s_auiTypes[0] = (s_uiScenario % 6 == 1 || s_uiScenario % 6 == 4) ? 2 : 1;
    s_auiTypes[1] = (uint32)s_aauiCatchable[0];
    s_auiTypes[2] = (uint32)s_aauiCatchable[1];
    s_auiThrowInfo[3] = (uint32)s_auiTypes;
    s_stInitial = s_stFixture;
}
static void world_invoke(void)
{
    void *pObject = s_stFixture.abyObject;
    void *pEntry = (uint8 *)s_hRetail + s_auiEntries[s_uiOperation];
    uint32 uiResult = 0, uiExpectedCalls;
    if (s_uiOperation == 0 || s_uiOperation == 3) {
        void *pResult = s_iSide ? fs98_world_area_construct(pObject, (void *)s_uiSeed) :
            ((void *(TC *)(void *, void *))pEntry)(pObject, (void *)s_uiSeed);
        uiResult = world_offset(pResult);
        if (s_uiOperation == 3) {
            if (s_iSide) pResult = fs98_world_area_delete(pObject, s_uiSeed);
            else pResult = ((void *(TC *)(void *, uint32))((uint8 *)s_hRetail + 0xbc70))(pObject, s_uiSeed);
            uiResult = world_offset(pResult);
        }
    } else if (s_uiOperation == 1) {
        if (s_iSide) fs98_world_area_destroy(pObject);
        else ((void (TC *)(void *))pEntry)(pObject);
    } else if (s_uiOperation == 2) {
        void *pResult = s_iSide ? fs98_world_area_delete(pObject, s_uiSeed) :
            ((void *(TC *)(void *, uint32))pEntry)(pObject, s_uiSeed);
        uiResult = world_offset(pResult);
    } else if (s_uiOperation == 4 || s_uiOperation == 5) {
        if (s_iSide) pEntry = s_uiOperation == 4 ? fs98_world_area_create_add : fs98_world_area_create_edit;
        uiResult = ((uint32 (SC *)(uint32, void *))pEntry)(s_uiSeed, s_stFixture.abyDescriptor);
    } else if (s_uiOperation == 6) {
        if (s_iSide) fs98_world_area_exchange(pObject, s_stFixture.abyExchange);
        else ((void (TC *)(void *, void *))pEntry)(pObject, s_stFixture.abyExchange);
    } else if (s_uiOperation == 7) {
        uiResult = s_iSide ? (uint32)fs98_world_area_help(pObject, s_uiSeed, s_stFixture.abyHelp) :
            ((uint32 (TC *)(void *, uint32, void *))pEntry)(pObject, s_uiSeed, s_stFixture.abyHelp);
    } else if (s_uiOperation == 8) {
        uiResult = s_iSide ? (uint32)fs98_world_area_context(pObject, s_uiSeed, s_uiSeed ^ 1) :
            ((uint32 (TC *)(void *, uint32, uint32))pEntry)(pObject, s_uiSeed, s_uiSeed ^ 1);
    }
    if (!s_iSide && !s_uiThrowAt) {
        uiExpectedCalls = s_uiOperation <= 2 ? 16u + (s_uiOperation == 2 && (s_uiSeed & 1)) :
            s_uiOperation == 3 ? 32 + (s_uiSeed & 1) :
            s_uiOperation <= 5 ? 19u + ((s_uiScenario & 2) != 0) : s_uiOperation == 6 ? 14 : 1;
        if (s_uiCalls != uiExpectedCalls ||
            (s_uiOperation == 0 && (U32(pObject, 0x10e0) != 1 || PTR(pObject, 0) != (uint8 *)s_hRetail + WORLD_AREA_VTABLE_RVA)) ||
            (s_uiOperation >= 4 && s_uiOperation <= 5 && uiResult != (s_uiSeed ^ 0x9f781b27)) ||
            (s_uiOperation == 8 && uiResult != 1)) {
            fprintf(stderr, "fixture self-check failed op=%u calls=%u expected=%u\n", s_uiOperation, s_uiCalls, uiExpectedCalls);
            exit(2);
        }
        ++s_uiChecks;
    }
    world_log("return %x\n", uiResult);
}
static void world_land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiSavedSeh) : "memory");
    __builtin_longjmp(s_apJump, 1);
}
static EXCEPTION_DISPOSITION __cdecl world_catch(EXCEPTION_RECORD *pRecord, void *pFrame, CONTEXT *pContext, void *pDispatcher)
{
    (void)pContext; (void)pDispatcher;
    if ((pRecord->ExceptionCode == THROW_CODE || pRecord->ExceptionCode == 0xe06d7363) && !(pRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(pFrame, NULL, pRecord, NULL);
        s_uiThrowAt = 0;
        world_log(pRecord->ExceptionCode == THROW_CODE ? "caught\n" : "caught.cpp\n");
        if (!s_iSide) ++s_uiThrows;
        world_land();
    }
    return ExceptionContinueSearch;
}
static LONG CALLBACK world_fault(EXCEPTION_POINTERS *pException)
{
    if ((s_uiOperation == 4 || s_uiOperation == 5) && s_uiScenario % 11 == 0 &&
        pException->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        pException->ExceptionRecord->ExceptionInformation[0] == 1 &&
        pException->ExceptionRecord->ExceptionInformation[1] == 0x10e4) {
        s_uiThrowAt = 0;
        world_log("allocation.fault write=10e4\n");
        if (!s_iSide) ++s_uiFaults;
        pException->ContextRecord->Eip = (DWORD)world_land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static LONG WINAPI world_fatal(EXCEPTION_POINTERS *pException)
{
    fprintf(stderr, "FAULT %lx at %p side=%d scenario=%u op=%u throw=%u\n", pException->ExceptionRecord->ExceptionCode,
        pException->ExceptionRecord->ExceptionAddress, s_iSide, s_uiScenario, s_uiOperation, s_uiThrowAt);
    ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static __attribute__((noinline)) void world_run(uint32 uiThrowAt)
{
    struct { void *pPrevious, *pHandler; } stCatcher;
    s_stFixture = s_stInitial;
    s_uiCalls = s_uiLength = 0; s_aacLogs[s_iSide][0] = 0; s_uiThrowAt = uiThrowAt;
    U32(s_hRetail, 0x24974) = s_stFixture.uiModule;
    fs98_world_set_module(s_stFixture.uiModule);
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_uiSavedSeh));
    if (__builtin_setjmp(s_apJump) == 0) {
        stCatcher.pPrevious = (void *)s_uiSavedSeh; stCatcher.pHandler = world_catch;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&stCatcher) : "memory");
        world_invoke();
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiSavedSeh) : "memory");
    if (memcmp(s_stFixture.abyBefore, s_stInitial.abyBefore, 32) || memcmp(s_stFixture.abyAfter, s_stInitial.abyAfter, 32)) {
        fprintf(stderr, "fixture guard damaged\n"); exit(2);
    }
}
static int world_compare(uint32 uiThrowAt)
{
    uint32 uiByte;
    s_iSide = 0; world_run(uiThrowAt); s_stReference = s_stFixture;
    s_iSide = 1; world_run(uiThrowAt); ++s_uiComparisons;
    if (!memcmp(&s_stReference, &s_stFixture, sizeof(s_stFixture)) && !strcmp(s_aacLogs[0], s_aacLogs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u operation=%u throw=%u seed=%x\n", s_uiScenario, s_uiOperation, uiThrowAt, s_uiSeed);
    for (uiByte = 0; uiByte < sizeof(s_stFixture); ++uiByte) if (((uint8 *)&s_stReference)[uiByte] != ((uint8 *)&s_stFixture)[uiByte]) {
        fprintf(stderr, "byte=%x retail=%x ours=%x\n", uiByte, ((uint8 *)&s_stReference)[uiByte], ((uint8 *)&s_stFixture)[uiByte]); break;
    }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_aacLogs[0], s_aacLogs[1]); return 1;
}
int main(int iArgc, char **ppcArgv)
{
    uint32 uiCount = iArgc > 1 ? strtoul(ppcArgv[1], NULL, 0) : 2000;
    uint32 uiSeed = iArgc > 2 ? strtoul(ppcArgv[2], NULL, 0) : 0x4a971b63;
    uint32 uiPosition, uiLimit;
    int iUnwind = iArgc > 3 && (!strcmp(ppcArgv[3], "unwind") || !strcmp(ppcArgv[3], "typed"));
    s_iTyped = iArgc > 3 && !strcmp(ppcArgv[3], "typed");
    if (!uiCount || !uiSeed) return 2;
    s_uiRng = uiSeed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, world_fault); SetUnhandledExceptionFilter(world_fatal);
    s_hRetail = LoadLibraryExA("orig/English/World.DLL", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_hRetail || GetProcAddress(s_hRetail, "Linkage") != (FARPROC)((uint8 *)s_hRetail + 0x24120)) return 2;
#define WORLD_BASE_HOOK(uiSlot) world_patch(0x##uiSlot, world_hook_##uiSlot);
#define WORLD_CTOR_HOOK(uiSlot) world_patch(0x##uiSlot, world_hook_##uiSlot);
#define WORLD_DTOR_HOOK(uiSlot) world_patch(0x##uiSlot, world_hook_##uiSlot);
#include "../../src/world/world_hooks.inc"
#undef WORLD_BASE_HOOK
#undef WORLD_CTOR_HOOK
#undef WORLD_DTOR_HOOK
    s_pfnThrow = (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_CxxThrowException");
    if (!s_pfnThrow) return 2;
    world_patch(0x25848, world_exit_cleanly);
    world_patch(0x2584c, world_allocate); world_patch(0x25800, world_ui_delete);
    world_patch(0x25850, world_dialog_delete); world_patch(0x25524, world_hook_create);
    world_patch(0x25500, world_hook_show); world_patch(0x2551c, world_message);
    world_patch(0x255fc, world_ddx); world_patch(0x258b8, world_help);
    world_patch(0x256bc, (void *)GetProcAddress(GetModuleHandleA("msvcrt.dll"), "__CxxFrameHandler"));
    fs98_world_set_object_services(&s_stServices, (uint8 *)s_hRetail + WORLD_AREA_VTABLE_RVA);
    for (s_uiScenario = 0; s_uiScenario < uiCount; ++s_uiScenario) {
        s_uiSeed = world_random();
        for (s_uiOperation = 0; s_uiOperation < 9; ++s_uiOperation) {
            world_initialize(); if (world_compare(0)) return 1;
            uiLimit = s_uiCalls - 1;
            if (iUnwind) for (uiPosition = 1; uiPosition <= uiLimit; ++uiPosition)
                if (world_compare(uiPosition)) return 1;
        }
    }
    printf("World area lifetime: %u scenarios, 9 operations, %u comparisons, 0 differ; seed=%x unwinds=%u allocation_faults=%u self_checks=%u exits=%u\n",
        uiCount, s_uiComparisons, uiSeed, s_uiThrows, s_uiFaults, s_uiChecks, s_uiExits);
    FreeLibrary(s_hRetail); return 0;
}
