#ifndef FS98_WORLD_UNWIND_H
#define FS98_WORLD_UNWIND_H
#include <windows.h>
#define WORLD_NOINLINE __attribute__((noinline))
/* Cleanup-only SEH. Keep owners out of line so Windows sees their frame. */
struct fs98_world_unwind {
    struct fs98_world_unwind *pPrevious;
    EXCEPTION_DISPOSITION (__cdecl *pfnHandler)(EXCEPTION_RECORD *, void *, CONTEXT *, void *);
    void (*pfnCleanup)(struct fs98_world_unwind *pFrame);
};
static __attribute__((unused)) EXCEPTION_DISPOSITION __cdecl world_unwind_handler(
    EXCEPTION_RECORD *pRecord, void *pEstablisher, CONTEXT *pContext, void *pDispatcher)
{
    struct fs98_world_unwind *pFrame = pEstablisher;
    (void)pContext; (void)pDispatcher;
    if (pRecord->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
        pFrame->pfnCleanup(pFrame);
    return ExceptionContinueSearch;
}
static __attribute__((always_inline)) inline void world_unwind_enter(
    struct fs98_world_unwind *pFrame, void (*pfnCleanup)(struct fs98_world_unwind *))
{
    __asm__ volatile("movl %%fs:0, %0" : "=r"(pFrame->pPrevious));
    pFrame->pfnHandler = world_unwind_handler; pFrame->pfnCleanup = pfnCleanup;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(pFrame) : "memory");
}
static __attribute__((always_inline)) inline void world_unwind_leave(struct fs98_world_unwind *pFrame)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(pFrame->pPrevious) : "memory"); }
#endif
