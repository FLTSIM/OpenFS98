#ifndef FS98_WORLD_UNWIND_H
#define FS98_WORLD_UNWIND_H
#include <windows.h>
#define WORLD_NOINLINE __attribute__((noinline))
/* Cleanup-only SEH. Keep owners out of line so Windows sees their frame. */
struct fs98_world_unwind {
    struct fs98_world_unwind *previous;
    EXCEPTION_DISPOSITION (__cdecl *handler)(EXCEPTION_RECORD *, void *, CONTEXT *, void *);
    void (*cleanup)(struct fs98_world_unwind *frame);
};
static __attribute__((unused)) EXCEPTION_DISPOSITION __cdecl world_unwind_handler(
    EXCEPTION_RECORD *record, void *establisher, CONTEXT *context, void *dispatcher)
{
    struct fs98_world_unwind *frame = establisher;
    (void)context; (void)dispatcher;
    if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
        frame->cleanup(frame);
    return ExceptionContinueSearch;
}
static __attribute__((always_inline)) inline void world_unwind_enter(
    struct fs98_world_unwind *frame, void (*cleanup)(struct fs98_world_unwind *))
{
    __asm__ volatile("movl %%fs:0, %0" : "=r"(frame->previous));
    frame->handler = world_unwind_handler; frame->cleanup = cleanup;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(frame) : "memory");
}
static __attribute__((always_inline)) inline void world_unwind_leave(struct fs98_world_unwind *frame)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(frame->previous) : "memory"); }
#endif
