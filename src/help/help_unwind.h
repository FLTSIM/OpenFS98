#ifndef FS98_HELP_UNWIND_H
#define FS98_HELP_UNWIND_H
#include <windows.h>

/* Cleanup-only x86 SEH records. Scope owners must be noinline so their
 * registration record remains inside the physical Windows unwind range. */
#define FS98_HELP_NOINLINE __attribute__((noinline))
struct fs98_help_unwind {
    struct fs98_help_unwind *previous;
    EXCEPTION_DISPOSITION (__cdecl *handler)(EXCEPTION_RECORD *, void *, CONTEXT *, void *);
    void (*cleanup)(struct fs98_help_unwind *frame);
};
static __attribute__((unused)) EXCEPTION_DISPOSITION __cdecl fs98_help_unwind_handler(
    EXCEPTION_RECORD *record, void *establisher, CONTEXT *context, void *dispatcher)
{
    struct fs98_help_unwind *frame = establisher;
    (void)context; (void)dispatcher;
    if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
        frame->cleanup(frame);
    return ExceptionContinueSearch;
}
static __attribute__((always_inline)) inline void fs98_help_unwind_enter(
    struct fs98_help_unwind *frame, void (*cleanup)(struct fs98_help_unwind *))
{
    __asm__ volatile("movl %%fs:0, %0" : "=r"(frame->previous));
    frame->handler = fs98_help_unwind_handler; frame->cleanup = cleanup;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(frame) : "memory");
}
static __attribute__((always_inline)) inline void fs98_help_unwind_leave(struct fs98_help_unwind *frame)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(frame->previous) : "memory"); }
#endif
