/* Alerts object/factory slice: original x86 routines against compiled C.
 * MFC/SIMSUI constructors/destructors are controlled logging dependencies.
 * This checks ordinary lifecycle and factory ABI, not throwing dependency
 * unwinding or inherited MFC dispatch. The actual factory NULL-allocation
 * access violation is compared, including the accessed address. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/alerts/alerts_objects.h"

#define THISCALL FS98_ALERTS_THISCALL
#define STDCALL FS98_ALERTS_STDCALL
#define D32(pObject, iOffset) (*(uint32 *)((uint8 *)(pObject) + (iOffset)))
#define PTR(pObject, iOffset) (*(void **)((uint8 *)(pObject) + (iOffset)))
#define OBJECT_BYTES 0x1600
#define LOG_BYTES 8192

typedef void *(THISCALL *fs98_construct_fn)(void *pObject, void *pParent);
typedef void (THISCALL *fs98_destroy_fn)(void *pObject);
typedef void *(THISCALL *fs98_delete_fn)(void *pObject, uint32 uiFlags);
typedef uint32 (STDCALL *fs98_create_fn)(uint32 uiParent, void *pDescriptor);

static const uint32 s_auiConstruct[] = {0x12f0, 0x1a50, 0x20d0};
static const uint32 s_auiDestroy[] = {0x13d0, 0x1b00, 0x21b0};
static const uint32 s_auiDelete[] = {0x13b0, 0x1ae0, 0x2190, 0x20b0};
static const uint32 s_auiCreate[] = {0x1130, 0x11c0, 0x1250};
static const fs98_construct_fn s_apConstruct[] = {
    fs98_alerts_modal_construct, fs98_alerts_modeless_construct, fs98_alerts_advisory_construct
};
static const fs98_destroy_fn s_apDestroy[] = {
    fs98_alerts_modal_destroy, fs98_alerts_modeless_destroy, fs98_alerts_advisory_destroy
};
static const fs98_delete_fn s_apDelete[] = {
    fs98_alerts_modal_delete, fs98_alerts_modeless_delete,
    fs98_alerts_advisory_delete, fs98_alerts_progress_delete
};
static const fs98_create_fn s_apCreate[] = {
    fs98_alerts_modal_create, fs98_alerts_modeless_create, fs98_alerts_advisory_create
};
static HMODULE s_pRetail;
static uint8 s_abyStorage[OBJECT_BYTES + 32] __attribute__((aligned(16)));
static uint8 s_abyInitial[sizeof(s_abyStorage)], s_abyRetail[sizeof(s_abyStorage)];
static uint8 s_abyDescriptor[64];
static void *s_pObject;
static void *s_apVtable[60];
static uint32 s_uiRng, s_uiSeed, s_uiScenario, s_uiOperation;
static uint32 s_uiFlags, s_uiParent;
static int s_iSide, s_bFailAllocate;
static char s_aszLog[2][LOG_BYTES];
static uint32 s_auiLogLength[2];
static void *s_apFaultJump[5];
static uint32 s_uiSavedSeh;
static volatile int s_bRunning;
static uint32 s_uiFaults, s_uiHeapUiDeletes, s_uiHeapMfcDeletes;

static uint32 fs98_random(void)
{
    s_uiRng ^= s_uiRng << 13;
    s_uiRng ^= s_uiRng >> 17;
    s_uiRng ^= s_uiRng << 5;
    return s_uiRng;
}

static void fs98_put(const char *pszFormat, ...)
{
    va_list pArgs;
    int iWritten;
    uint32 uiLength = s_auiLogLength[s_iSide];
    if (uiLength > LOG_BYTES - 1024)
        exit(2);
    va_start(pArgs, pszFormat);
    iWritten = vsnprintf(s_aszLog[s_iSide] + uiLength, LOG_BYTES - uiLength, pszFormat, pArgs);
    va_end(pArgs);
    if (iWritten < 0)
        exit(2);
    s_auiLogLength[s_iSide] += (uint32)iWritten;
}

static uint32 fs98_offset(const void *pObject)
{
    return (uint32)((const uint8 *)pObject - (const uint8 *)s_pObject);
}

static void fs98_subobject(void *pThis, const char *pszName, uint32 uiSize, uint8 byFill)
{
    fs98_put("%s object=%u size=%u\n", pszName, fs98_offset(pThis), uiSize);
    memset(pThis, byFill, uiSize);
}

static void *fs98_dialog_construct(void *pThis, int32 iTemplate, void *pParent)
{
    fs98_put("dialog.construct template=%d parent=%u object=%u\n",
             iTemplate, (uint32)pParent, fs98_offset(pThis));
    memset(pThis, 0x31, 0x10dc);
    D32(pThis, 0x20) = 0x10020;
    D32(pThis, 0x40) = (uint32)pParent;
    D32(pThis, 0x44) = (uint32)iTemplate;
    return pThis;
}
static void fs98_dialog_destroy(void *pThis)
{ fs98_subobject(pThis, "dialog.destroy", 0x10dc, 0xd1); }
static void fs98_string_construct(void *pThis)
{ fs98_subobject(pThis, "string.construct", 4, 0x32); }
static void fs98_string_destroy(void *pThis)
{ fs98_subobject(pThis, "string.destroy", 4, 0xd2); }
static void fs98_text_construct(void *pThis)
{ fs98_subobject(pThis, "text.construct", 0xb8, 0x33); }
static void fs98_text_destroy(void *pThis)
{ fs98_subobject(pThis, "text.destroy", 0xb8, 0xd3); }
static void fs98_button_construct(void *pThis)
{ fs98_subobject(pThis, "button.construct", 0xfc, 0x34); }
static void fs98_button_destroy(void *pThis)
{ fs98_subobject(pThis, "button.destroy", 0xfc, 0xd4); }
static void fs98_window_construct(void *pThis)
{ fs98_subobject(pThis, "window.construct", 0x40, 0x35); }
static void fs98_window_destroy(void *pThis)
{ fs98_subobject(pThis, "window.destroy", 0x40, 0xd5); }
static void fs98_check_construct(void *pThis)
{ fs98_subobject(pThis, "check.construct", 0xc8, 0x36); }
static void fs98_check_destroy(void *pThis)
{ fs98_subobject(pThis, "check.destroy", 0xc8, 0xd6); }

static void *fs98_allocate(uint32 uiSize)
{
    fs98_put("allocate %u fail=%d\n", uiSize, s_bFailAllocate);
    return s_bFailAllocate ? 0 : s_pObject;
}

static void fs98_ui_delete(void *pThis)
{
    fs98_put("ui.delete %u\n", fs98_offset(pThis));
    if (!s_iSide) s_uiHeapUiDeletes++;
}

static void fs98_mfc_delete(void *pThis)
{
    fs98_put("mfc.delete %u\n", fs98_offset(pThis));
    if (!s_iSide) s_uiHeapMfcDeletes++;
}

static int32 fs98_create(void *pThis, uint32 uiTemplate, void *pParent)
{
    fs98_put("create object=%u template=%u parent=%u descriptor=%u module=%u\n",
             fs98_offset(pThis), uiTemplate, (uint32)pParent,
             PTR(pThis, 0x10dc) == s_abyDescriptor, D32(pThis, 0x10e0));
    return (int32)(s_uiScenario % 3); /* Includes failure, which retail ignores. */
}

static int32 fs98_show(void *pThis, int32 iShow)
{
    fs98_put("show object=%u flag=%d\n", fs98_offset(pThis), iShow);
    D32(pThis, 0x20) = 0x20000 + (s_uiSeed & 0xffff);
    return (int32)(s_uiScenario & 1);
}

static int32 fs98_enable(uint32 uiWindow, int32 iEnable)
{
    fs98_put("enable hwnd=%u flag=%d\n", uiWindow, iEnable);
    return (int32)(s_uiSeed & 1);
}

static const struct fs98_alerts_object_services s_stServices = {
    fs98_dialog_construct, fs98_dialog_destroy, fs98_string_construct, fs98_string_destroy,
    fs98_text_construct, fs98_text_destroy, fs98_button_construct, fs98_button_destroy,
    fs98_window_construct, fs98_window_destroy, fs98_check_construct, fs98_check_destroy,
    fs98_allocate, fs98_ui_delete, fs98_mfc_delete, fs98_create, fs98_show, fs98_enable
};

/* IAT adapters have the dependency's actual calling convention. */
static void *THISCALL fs98_mfc_dialog(void *pThis, int32 iTemplate, void *pParent)
{ return fs98_dialog_construct(pThis, iTemplate, pParent); }
#define DEFINE_CTOR(szName, pFunction) \
    static void *THISCALL szName(void *pThis) { pFunction(pThis); return pThis; }
#define DEFINE_DTOR(szName, pFunction) \
    static void THISCALL szName(void *pThis) { pFunction(pThis); }
DEFINE_CTOR(fs98_mfc_string, fs98_string_construct)
DEFINE_CTOR(fs98_mfc_text, fs98_text_construct)
DEFINE_CTOR(fs98_mfc_button, fs98_button_construct)
DEFINE_CTOR(fs98_mfc_window, fs98_window_construct)
DEFINE_CTOR(fs98_mfc_check, fs98_check_construct)
DEFINE_DTOR(fs98_mfc_dialog_destroy, fs98_dialog_destroy)
DEFINE_DTOR(fs98_mfc_string_destroy, fs98_string_destroy)
DEFINE_DTOR(fs98_mfc_text_destroy, fs98_text_destroy)
DEFINE_DTOR(fs98_mfc_button_destroy, fs98_button_destroy)
DEFINE_DTOR(fs98_mfc_window_destroy, fs98_window_destroy)
DEFINE_DTOR(fs98_mfc_check_destroy, fs98_check_destroy)
static int32 THISCALL fs98_mfc_create(void *pThis, uint32 uiTemplate, void *pParent)
{ return fs98_create(pThis, uiTemplate, pParent); }
static int32 THISCALL fs98_mfc_show(void *pThis, int32 iShow)
{ return fs98_show(pThis, iShow); }
static int32 STDCALL fs98_mfc_enable(uint32 uiWindow, int32 iEnable)
{ return fs98_enable(uiWindow, iEnable); }
static void *THISCALL fs98_virtual_delete(void *pThis, uint32 uiFlags)
{
    fs98_put("virtual.delete object=%u flags=%u\n", fs98_offset(pThis), uiFlags);
    return pThis;
}

static void fs98_patch(uint32 uiRva, void *pFunction)
{
    DWORD uiOldProtect;
    void **ppSlot = (void **)((uint8 *)s_pRetail + uiRva);
    if (!VirtualProtect(ppSlot, 4, PAGE_READWRITE, &uiOldProtect)) exit(2);
    *ppSlot = pFunction;
    VirtualProtect(ppSlot, 4, uiOldProtect, &uiOldProtect);
}

static void fs98_load(void)
{
    char szPath[MAX_PATH];
    struct fs98_alerts_object_tables stTables;
    GetFullPathNameA("orig\\English\\Alerts.DLL", MAX_PATH, szPath, 0);
    s_pRetail = LoadLibraryExA(szPath, 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_pRetail || D32(s_pRetail, 0x4060) != 0x80000009 ||
        D32(s_pRetail, 0x401c) != (uint32)s_pRetail + 0x1130) {
        fprintf(stderr, "reference Alerts layout/load failed: %lu\n", GetLastError());
        exit(2);
    }
    fs98_patch(0x543c, fs98_mfc_dialog);
    fs98_patch(0x540c, fs98_mfc_dialog_destroy);
    fs98_patch(0x5300, fs98_mfc_string);
    fs98_patch(0x5304, fs98_mfc_string_destroy);
    fs98_patch(0x5444, fs98_mfc_text);
    fs98_patch(0x5410, fs98_mfc_text_destroy);
    fs98_patch(0x5408, fs98_mfc_button);
    fs98_patch(0x5404, fs98_mfc_button_destroy);
    fs98_patch(0x52cc, fs98_mfc_window);
    fs98_patch(0x52d0, fs98_mfc_window_destroy);
    fs98_patch(0x53e4, fs98_mfc_check);
    fs98_patch(0x5434, fs98_mfc_check_destroy);
    fs98_patch(0x5440, fs98_allocate);
    fs98_patch(0x53fc, fs98_ui_delete);
    fs98_patch(0x533c, fs98_mfc_delete);
    fs98_patch(0x5338, fs98_mfc_create);
    fs98_patch(0x52ac, fs98_mfc_show);
    fs98_patch(0x5464, fs98_mfc_enable);
    D32(s_pRetail, 0x40bc) = 0x50060;
    stTables.pModal = (uint8 *)s_pRetail + 0x3068;
    stTables.pModeless = (uint8 *)s_pRetail + 0x31a8;
    stTables.pProgress = (uint8 *)s_pRetail + 0x3298;
    stTables.pAdvisory = (uint8 *)s_pRetail + 0x33c0;
    fs98_alerts_set_object_services(&s_stServices, &stTables);
    fs98_alerts_set_object_module(0x50060);
}

static void fs98_fault_land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiSavedSeh) : "memory");
    __builtin_longjmp(s_apFaultJump, 1);
}

static LONG CALLBACK fs98_exception(EXCEPTION_POINTERS *pException)
{
    if (s_bRunning && s_bFailAllocate && s_uiOperation >= 14 && s_uiOperation <= 16 &&
        pException->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        fs98_put("FAULT code=%08lx access=%u address=%u\n",
                 pException->ExceptionRecord->ExceptionCode,
                 (uint32)pException->ExceptionRecord->ExceptionInformation[0],
                 (uint32)pException->ExceptionRecord->ExceptionInformation[1]);
        if (!s_iSide) s_uiFaults++;
        pException->ContextRecord->Eip = (DWORD)fs98_fault_land;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    fprintf(stderr, "UNEXPECTED FAULT scenario=%u seed=%08x operation=%u side=%d "
            "code=%08lx pc=%p\n", s_uiScenario, s_uiSeed, s_uiOperation, s_iSide,
            pException->ExceptionRecord->ExceptionCode, pException->ExceptionRecord->ExceptionAddress);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void fs98_operation(void)
{
    void *pReturn;
    uint32 uiKind;
    if (s_uiOperation < 3) {
        fs98_construct_fn pConstruct = s_iSide ? s_apConstruct[s_uiOperation] :
            (fs98_construct_fn)((uint8 *)s_pRetail + s_auiConstruct[s_uiOperation]);
        pReturn = pConstruct(s_pObject, (void *)s_uiParent);
        fs98_put("return.object %u\n", fs98_offset(pReturn));
    } else if (s_uiOperation < 6) {
        uiKind = s_uiOperation - 3;
        fs98_destroy_fn pDestroy = s_iSide ? s_apDestroy[uiKind] :
            (fs98_destroy_fn)((uint8 *)s_pRetail + s_auiDestroy[uiKind]);
        pDestroy(s_pObject);
    } else if (s_uiOperation < 10) {
        uiKind = s_uiOperation - 6;
        fs98_delete_fn pDelete = s_iSide ? s_apDelete[uiKind] :
            (fs98_delete_fn)((uint8 *)s_pRetail + s_auiDelete[uiKind]);
        pReturn = pDelete(s_pObject, s_uiFlags);
        fs98_put("return.object %u\n", fs98_offset(pReturn));
    } else if (s_uiOperation == 10) {
        fs98_destroy_fn pPost = s_iSide ? fs98_alerts_post_destroy :
            (fs98_destroy_fn)((uint8 *)s_pRetail + 0x2590);
        pPost(s_uiScenario & 1 ? s_pObject : 0);
    } else if (s_uiOperation < 13) {
        fs98_destroy_fn pEnable = s_iSide ?
            (s_uiOperation == 11 ? fs98_alerts_disable : fs98_alerts_enable) :
            (fs98_destroy_fn)((uint8 *)s_pRetail + (s_uiOperation == 11 ? 0x1a30 : 0x1a40));
        pEnable(s_pObject);
    } else if (s_uiOperation == 13) {
        typedef void (THISCALL *fs98_empty_fn)(void *pThis, uint32 uiArgument);
        fs98_empty_fn pEmpty = s_iSide ? fs98_alerts_empty_arg :
            (fs98_empty_fn)((uint8 *)s_pRetail + 0x20a0);
        pEmpty(s_pObject, s_uiFlags);
    } else if (s_uiOperation < 17) {
        uiKind = s_uiOperation - 14;
        fs98_create_fn pCreate = s_iSide ? s_apCreate[uiKind] :
            (fs98_create_fn)((uint8 *)s_pRetail + s_auiCreate[uiKind]);
        uint32 uiReturn = pCreate(s_uiParent, s_abyDescriptor);
        fs98_put("return.hwnd %u\n", uiReturn);
    } else {
        uiKind = (s_uiOperation - 17) % 3;
        fs98_construct_fn pConstruct = s_iSide ? s_apConstruct[uiKind] :
            (fs98_construct_fn)((uint8 *)s_pRetail + s_auiConstruct[uiKind]);
        pConstruct(s_pObject, (void *)s_uiParent);
        if (s_uiOperation < 20) {
            fs98_destroy_fn pDestroy = s_iSide ? s_apDestroy[uiKind] :
                (fs98_destroy_fn)((uint8 *)s_pRetail + s_auiDestroy[uiKind]);
            pDestroy(s_pObject);
        } else {
            fs98_delete_fn pDelete = s_iSide ? s_apDelete[uiKind] :
                (fs98_delete_fn)((uint8 *)s_pRetail + s_auiDelete[uiKind]);
            pReturn = pDelete(s_pObject, s_uiFlags);
            fs98_put("return.object %u\n", fs98_offset(pReturn));
        }
    }
}

static void fs98_run(void)
{
    memcpy(s_abyStorage, s_abyInitial, sizeof(s_abyStorage));
    PTR(s_pObject, 0) = s_apVtable;
    s_auiLogLength[s_iSide] = 0;
    s_aszLog[s_iSide][0] = 0;
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_uiSavedSeh));
    s_bRunning = 1;
    if (__builtin_setjmp(s_apFaultJump) == 0)
        fs98_operation();
    s_bRunning = 0;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_uiSavedSeh) : "memory");
}

int main(int iArgc, char **ppszArgv)
{
    uint32 uiCount = iArgc > 1 ? (uint32)strtoul(ppszArgv[1], 0, 0) : 20000;
    uint32 uiIndex;
    static const uint32 s_auiFlags[] = {0, 1, 2, 3, 0x100, 0x101, 0x80000000, 0xffffffff};
    if (!uiCount) return 2;
    s_uiRng = iArgc > 2 ? (uint32)strtoul(ppszArgv[2], 0, 0) : 0x587a41e9;
    if (!s_uiRng) return 2;
    setvbuf(stdout, 0, _IONBF, 0);
    s_pObject = s_abyStorage + 16;
    s_apVtable[1] = fs98_virtual_delete;
    AddVectoredExceptionHandler(1, fs98_exception);
    fs98_load();
    for (s_uiScenario = 0; s_uiScenario < uiCount; s_uiScenario++) {
        s_uiSeed = s_uiRng;
        for (uiIndex = 0; uiIndex < sizeof(s_abyStorage); uiIndex++)
            s_abyInitial[uiIndex] = (uint8)fs98_random();
        s_uiFlags = s_uiScenario % 3 ? s_auiFlags[s_uiScenario % 8] : fs98_random();
        s_uiParent = s_uiScenario % 3 ? fs98_random() : 0;
        s_bFailAllocate = s_uiScenario % 11 == 0;
        for (s_uiOperation = 0; s_uiOperation < 23; s_uiOperation++) {
            s_iSide = 0;
            fs98_run();
            memcpy(s_abyRetail, s_abyStorage, sizeof(s_abyStorage));
            s_iSide = 1;
            fs98_run();
            if (strcmp(s_aszLog[0], s_aszLog[1]) ||
                memcmp(s_abyRetail, s_abyStorage, sizeof(s_abyStorage))) {
                fprintf(stderr, "DIFFER scenario=%u seed=%08x operation=%u\nRETAIL:\n%sOURS:\n%s",
                        s_uiScenario, s_uiSeed, s_uiOperation, s_aszLog[0], s_aszLog[1]);
                return 1;
            }
        }
    }
    printf("Alerts objects: %u scenarios, %u comparisons, 0 differ\n", uiCount, uiCount * 23);
    printf("allocation faults=%u ui.deletes=%u mfc.deletes=%u\n",
           s_uiFaults, s_uiHeapUiDeletes, s_uiHeapMfcDeletes);
    return 0;
}
