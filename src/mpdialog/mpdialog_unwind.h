#ifndef FS98_MPDIALOG_UNWIND_H
#define FS98_MPDIALOG_UNWIND_H

#include <windows.h>
#include "mpdialog_internal.h"

/* Cleanup-only PE32 SEH scopes. The enclosing MFC/owner handler chooses a
 * catch; Windows calls these scopes during the actual unwind. Keep their
 * owning functions out of line so registration records follow stack order. */
#define MPD_NOINLINE __attribute__((noinline))
struct fs98_mpdialog_unwind {
    struct fs98_mpdialog_unwind *previous;
    EXCEPTION_DISPOSITION (__cdecl *handler)(EXCEPTION_RECORD *, void *, CONTEXT *, void *);
    void (*cleanup)(struct fs98_mpdialog_unwind *);
};

static EXCEPTION_DISPOSITION __cdecl fs98_mpdialog_unwind_handler(
    EXCEPTION_RECORD *record, void *frame, CONTEXT *context, void *dispatcher)
{
    struct fs98_mpdialog_unwind *scope = frame;
    (void)context; (void)dispatcher;
    if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND)) scope->cleanup(scope);
    return ExceptionContinueSearch;
}

static __attribute__((always_inline)) inline void fs98_mpdialog_unwind_enter(
    struct fs98_mpdialog_unwind *scope, void (*cleanup)(struct fs98_mpdialog_unwind *))
{
    __asm__ volatile("movl %%fs:0, %0" : "=r"(scope->previous));
    scope->handler = fs98_mpdialog_unwind_handler;
    scope->cleanup = cleanup;
    __asm__ volatile("movl %0, %%fs:0" : : "r"(scope) : "memory");
}

static __attribute__((always_inline)) inline void fs98_mpdialog_unwind_leave(struct fs98_mpdialog_unwind *scope)
{ __asm__ volatile("movl %0, %%fs:0" : : "r"(scope->previous) : "memory"); }

extern void *fs98_mpdialog_simsui(const char *name);
struct fs98_mpdialog_object_scope {
    struct fs98_mpdialog_unwind frame;
    void *object;
    void (*destroy_control)(void *, uint32_t, uint32_t);
    uint32_t page;
    volatile uint32_t completed, base;
};

static __attribute__((unused)) void fs98_mpdialog_object_cleanup(struct fs98_mpdialog_unwind *frame)
{
    struct fs98_mpdialog_object_scope *state = (void *)frame;
    while (state->completed) state->destroy_control(state->object, --state->completed, state->page);
    if (state->base) {
        state->base = 0;
        ((void (FS98_MPDIALOG_THISCALL *)(void *))
            fs98_mpdialog_simsui("??1CSimsDialog@@UAE@XZ"))(state->object);
    }
}

struct fs98_mpdialog_factory_scope {
    struct fs98_mpdialog_unwind frame;
    void *object;
    volatile uint32_t live;
};
static __attribute__((unused)) void fs98_mpdialog_factory_cleanup(struct fs98_mpdialog_unwind *frame)
{
    struct fs98_mpdialog_factory_scope *state = (void *)frame;
    if (state->live) {
        state->live = 0;
        ((void (FS98_MPDIALOG_STDCALL *)(void *))
            fs98_mpdialog_simsui("??3CSimsDialog@@SGXPAX@Z"))(state->object);
    }
}
#endif
