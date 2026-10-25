#ifndef FS98_HELP_UNWIND_H
#define FS98_HELP_UNWIND_H
#include <windows.h>

/* Cleanup-only x86 SEH records. Scope owners must be noinline so their
 * registration record remains inside the physical Windows unwind range. */
#define FS98_HELP_NOINLINE __attribute__((noinline))
struct fs98_help_unwind {
    struct fs98_help_unwind *pPrevious;
    EXCEPTION_DISPOSITION (__cdecl *pHandler)(EXCEPTION_RECORD *, void *, CONTEXT *, void *);
    void (*pCleanup)(struct fs98_help_unwind *pFrame);
};
static __attribute__((unused)) EXCEPTION_DISPOSITION __cdecl fs98_help_unwind_handler(
    EXCEPTION_RECORD *pRecord, void *pEstablisher, CONTEXT *pContext, void *pDispatcher)
{
    struct fs98_help_unwind *pFrame = pEstablisher;
    (void)pContext; (void)pDispatcher;
    if (pRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
        pFrame->pCleanup(pFrame);
    return ExceptionContinueSearch;
}
static __attribute__((always_inline)) inline void fs98_help_unwind_enter(
    struct fs98_help_unwind *pFrame, void (*pCleanup)(struct fs98_help_unwind *))
{
    __asm__ volatile("movl %%fs:0, %0" : "=r"(pFrame->pPrevious));
    pFrame->pHandler = fs98_help_unwind_handler; pFrame->pCleanup = pCleanup;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(pFrame) : "memory");
}
static __attribute__((always_inline)) inline void fs98_help_unwind_leave(struct fs98_help_unwind *pFrame)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(pFrame->pPrevious) : "memory"); }
#endif
