#ifndef FS98_ALERTS_UNWIND_H
#define FS98_ALERTS_UNWIND_H

#include <windows.h>

/* C cleanup-only SEH scopes for the PE32 Microsoft C++ dependency boundary.
 * They do not catch exceptions: the surrounding MFC handler performs the
 * search and asks Windows to unwind us. Scope-owning functions must not be
 * inlined, so nested registration records retain their real stack order. */
#define FS98_ALERTS_NOINLINE __attribute__((noinline))
struct fs98_alerts_unwind {
    struct fs98_alerts_unwind *previous;
    EXCEPTION_DISPOSITION (__cdecl *handler)(EXCEPTION_RECORD *, void *, CONTEXT *, void *);
    void (*cleanup)(struct fs98_alerts_unwind *frame);
};

static __attribute__((unused)) EXCEPTION_DISPOSITION __cdecl fs98_alerts_unwind_handler(
    EXCEPTION_RECORD *record, void *establisher, CONTEXT *context, void *dispatcher)
{
    struct fs98_alerts_unwind *frame = establisher;
    (void)context;
    (void)dispatcher;
    if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
        frame->cleanup(frame);
    return ExceptionContinueSearch;
}

static __attribute__((always_inline)) inline void fs98_alerts_unwind_enter(
    struct fs98_alerts_unwind *frame, void (*cleanup)(struct fs98_alerts_unwind *))
{
    __asm__ volatile("movl %%fs:0, %0" : "=r"(frame->previous));
    frame->handler = fs98_alerts_unwind_handler;
    frame->cleanup = cleanup;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(frame) : "memory");
}

static __attribute__((always_inline)) inline void fs98_alerts_unwind_leave(
    struct fs98_alerts_unwind *frame)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(frame->previous) : "memory");
}

#endif
