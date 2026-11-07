/* Executable shell for the recovered host runtime. The retail CRT startup is
 * replaced by mingw's CRT; the game's three global constructors still run in
 * their original order before MFC's AfxWinMain.
 */
#include "host_runtime.h"
#include <stdio.h>

extern void host_entry_1000(void);
extern void host_entry_10b0(void);
extern void host_entry_1504a(void);

void host_runtime_fail(uint32_t address)
{
    fprintf(stderr, "host runtime: unsupported state at %08lx\n", (unsigned long)address);
    fflush(stderr);
    ExitProcess(3);
}

void host_runtime_import_error(const char *library, const char *name)
{
    fprintf(stderr, "host runtime: cannot resolve %s!%s (error %lu)\n",
            library, name, (unsigned long)GetLastError());
}

static LONG WINAPI host_runtime_exception(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "host runtime: exception %08lx at %p\n",
            (unsigned long)exception->ExceptionRecord->ExceptionCode,
            exception->ExceptionRecord->ExceptionAddress);
    fflush(stderr);
    ExitProcess(4);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* Deliver the host's divide repair before Wine catches window-procedure
 * exceptions. Only the recovered host's active protected frame authorizes it.
 */
static LONG CALLBACK host_runtime_divide(EXCEPTION_POINTERS *exception)
{
    typedef LONG (__stdcall *filter_fn)(EXCEPTION_POINTERS *);
    NT_TIB *tib = (NT_TIB *)(uintptr_t)host_fs_address(0);
    uint32_t *record = (uint32_t *)tib->ExceptionList;
    unsigned depth;
    DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_INT_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;
    for (depth = 0; depth < 64 && (uintptr_t)record >= (uintptr_t)tib->StackLimit &&
         (uintptr_t)record + 16 <= (uintptr_t)tib->StackBase; ++depth) {
        if (record[2] == host_runtime_address(NULL,0x16110) && record[3] == 0)
            return ((filter_fn)(uintptr_t)host_runtime_address(NULL,0x119a0))(exception);
        record = (uint32_t *)(uintptr_t)record[0];
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    typedef int (WINAPI *afx_win_main_fn)(HINSTANCE, HINSTANCE, LPSTR, int);
    afx_win_main_fn afx_win_main;
    /* MSVCRT initializes VC5 applications at 53-bit precision. MinGW's
     * executable startup uses FNINIT (64-bit), which changes SIM1 trajectories.
     */
    { const uint16_t control=0x027f; __asm__ volatile("fldcw %0" : : "m"(control)); }
    SetUnhandledExceptionFilter(host_runtime_exception);
    if (!host_runtime_init())
        return 2;
    AddVectoredExceptionHandler(1,host_runtime_divide);
    /* The VC5 onexit wrapper uses these sentinels to delegate to MSVCRT. */
    mdl_write(NULL,host_runtime_address(NULL,0x1c5c4),UINT32_MAX,32);
    mdl_write(NULL,host_runtime_address(NULL,0x1c5c8),UINT32_MAX,32);
    fprintf(stderr,"host: global constructors\n"); fflush(stderr);
    host_entry_1504a();
    fprintf(stderr,"host: CWinApp constructor\n"); fflush(stderr);
    host_entry_1000();
    fprintf(stderr,"host: application constructor\n"); fflush(stderr);
    host_entry_10b0();
    afx_win_main = (afx_win_main_fn)(uintptr_t)mdl_read(NULL,host_runtime_address(NULL,0x1d8a0),32);
    fprintf(stderr,"host: AfxWinMain\n"); fflush(stderr);
    return afx_win_main(instance,previous,command,show);
}
