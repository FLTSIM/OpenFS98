/* CHAIN.DLL -- module id 4: 35 numbered callback lists.
 *
 * Modules register stdcall (a, b) callbacks on a list by number (+0x18),
 * unregister them (+0x1c), and fire a whole list (+0x20).  It is the sim's
 * event bus: MAIN dispatches 28 different lists, CONTROLS 16, WINDOW 9.
 *
 * Transliterated from the 966 bytes of retail .text; docs/modules/chain.md
 * walks through it.  The behaviours worth knowing before calling it:
 *
 *   - A list's dispatch depth is never initialised.  The first add allocates
 *     the list block through the host and sets count, capacity and the
 *     pending flag, but not +0x00, so a list starts at whatever depth malloc
 *     left there.  If that is non-zero, every removal from the list is
 *     deferred and the list is never compacted.  Reproduced: the block comes
 *     from the same host allocator.
 *   - Removing from a list that was never added to dereferences null and
 *     crashes retail.  Here it returns -1.
 *   - Removal matches entries already marked removed, so removing the same
 *     callback twice during a dispatch marks the same entry twice rather than
 *     finding a later duplicate.
 *   - Dispatch rereads the count on every step, so a callback added during a
 *     dispatch is called by that same dispatch.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define CHAIN_LISTS 35

struct fs98_import ImportTable[] = {
    { FS98_MOD_HOST,   0 },
    { FS98_MOD_GLOBAL, 0 },         /* imported; never read */
    { 0, 0 }
};

#define HOST ((void *)ImportTable[0].link)

typedef void (FS98_STDCALL *chain_cb)(uint32_t a, uint32_t b);

struct chain_entry {
    chain_cb callback;
    uint32_t removed;
};

struct chain_list {
    uint32_t depth;             /* dispatch nesting; NOT initialised */
    uint32_t count;
    uint32_t capacity;
    uint32_t pending;           /* this list has entries marked removed */
    struct chain_entry entry[]; /* +0x10 */
};

/* .data+0x40 and +0xcc in retail: the lists, then the module-wide flag that
 * some list has a deferred removal.  Init clears the first and not the
 * second. */
static struct {
    struct chain_list *list[CHAIN_LISTS];
    uint32_t pending;
} chain;

/* ---- init / deinit -------------------------------------------------------- */

static void chain_init(void)
{
    unsigned i;
    for (i = 0; i < CHAIN_LISTS; i++)       /* rep stosd, ecx = 0x23 */
        chain.list[i] = 0;
}

static void chain_deinit(void)
{
    unsigned i;
    for (i = 0; i < CHAIN_LISTS; i++)
        if (chain.list[i])
            FS98_HOST_FREE(HOST)((void **)&chain.list[i]);
}

/* ---- add (.text+0x090) ----------------------------------------------------- */

static int32_t chain_add_to(struct chain_list **pp, chain_cb cb)
{
    struct chain_list *l;

    if (!pp)
        return -1;
    if (!*pp) {
        if (FS98_HOST_ALLOC(HOST)(1, 0x60, (void **)pp, 1) != 0)
            return -1;
        (*pp)->count = 0;
        (*pp)->pending = 0;
        (*pp)->capacity = 10;
    }
    l = *pp;
    if (l->count >= l->capacity) {
        if (FS98_HOST_REALLOC(HOST)(1, l->capacity * 8 + 0x60, (void **)pp) != 0)
            return -1;
        l = *pp;
        l->capacity += 10;
    }
    l->entry[l->count].callback = cb;
    l->entry[l->count].removed = 0;
    l->count++;
    return 0;
}

static int32_t FS98_STDCALL chain_add(uint32_t index, chain_cb cb)
{
    return chain_add_to(&chain.list[index], cb);
}

/* ---- remove (.text+0x160) -------------------------------------------------- */

static int32_t chain_remove_from(struct chain_list **pp, chain_cb cb)
{
    struct chain_list *l = *pp;
    uint32_t i, n;

    if (!l)
        return -1;                          /* retail faults here */
    n = l->count;
    for (i = 0; i < n; i++)
        if (l->entry[i].callback == cb)     /* removed or not */
            break;
    if (i == n)
        return -1;

    if (l->depth > 0) {                     /* mid-dispatch: defer */
        l->entry[i].removed = 1;
        l->pending = 1;
        chain.pending = 1;
        return 0;
    }
    if ((n - i - 1) * 8 != 0)
        FS98_HOST_MEMMOVE(HOST)(&l->entry[i], &l->entry[i + 1], (n - i - 1) * 8);
    l->entry[n - 1].callback = 0;
    l->entry[n - 1].removed = 0;
    l->count--;
    return 0;
}

static int32_t FS98_STDCALL chain_remove(uint32_t index, chain_cb cb)
{
    return chain_remove_from(&chain.list[index], cb);
}

/* ---- compaction (.text+0x2a0) ----------------------------------------------
 *
 * Applies deferred removals on every list that has some and is not being
 * dispatched.  Returns 1 if none was left waiting, 0 if a list with pending
 * removals is still mid-dispatch.  A list whose count has reached zero keeps
 * its pending flag: it is skipped before the flag is cleared. */
static int32_t chain_compact(void)
{
    int32_t done = 1;
    unsigned k;

    for (k = 0; k < CHAIN_LISTS; k++) {
        struct chain_list *l = chain.list[k];
        int32_t i, n;

        if (!l || l->count == 0 || l->pending == 0)
            continue;
        if (l->depth > 0) {
            done = 0;
            continue;
        }
        n = (int32_t)l->count;
        for (i = 0; i < n; i++) {
            uint32_t size;
            if (!l->entry[i].removed)
                continue;
            size = (l->count - (uint32_t)i - 1) * 8;
            if (size != 0)
                FS98_HOST_MEMMOVE(HOST)(&l->entry[i], &l->entry[i + 1], size);
            l->entry[l->count - 1].callback = 0;
            l->entry[l->count - 1].removed = 0;
            l->count--;
            n--;
            i--;
        }
        l->pending = 0;
    }
    return done;
}

/* ---- dispatch (.text+0x220) ------------------------------------------------
 *
 * The return value is whatever retail leaves in EAX -- 0, the list pointer
 * for an empty list, or the compaction result -- and every caller in the
 * retail modules overwrites it unread. */
static uint32_t chain_dispatch_list(struct chain_list **pp, uint32_t a, uint32_t b)
{
    uint32_t i, r;

    if (!*pp)
        return 0;
    if ((*pp)->count == 0)
        return (uint32_t)(uintptr_t)*pp;

    (*pp)->depth++;
    for (i = 0; i < (*pp)->count; i++)     /* count and block reread each step */
        if (!(*pp)->entry[i].removed)
            (*pp)->entry[i].callback(a, b);
    (*pp)->depth--;

    if (!chain.pending)
        return 0;
    r = (uint32_t)chain_compact();
    if (r)
        chain.pending = 0;
    return r;
}

static uint32_t FS98_STDCALL chain_dispatch(uint32_t index, uint32_t a, uint32_t b)
{
    return chain_dispatch_list(&chain.list[index], a, b);
}

/* ---- the module ------------------------------------------------------------ */

struct {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;       /* retail's literal 0xe0000000; meaning unknown */
    void    *dialogs;           /* +0x14: no dialog table */
    int32_t  (FS98_STDCALL *add)(uint32_t, chain_cb);
    int32_t  (FS98_STDCALL *remove)(uint32_t, chain_cb);
    uint32_t (FS98_STDCALL *dispatch)(uint32_t, uint32_t, uint32_t);
} Linkage = {
    FS98_MOD_CHAIN, chain_init, chain_deinit, 0, 0xe0000000u, 0,
    chain_add, chain_remove, chain_dispatch
};

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
