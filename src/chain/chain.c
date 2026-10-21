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
 *   - A callback is entered with retail's registers: eax the list's block, ecx
 *     the dispatch's first argument, edx the list's depth (for the first) or
 *     what the callback before it left in edx, ebx the list's slot in .data,
 *     esi 0x10 + 8 * its index, edi its index, ebp the second argument.  And
 *     dispatch returns with retail's ecx and edx (see chain_dispatch_body).
 *     Asm callbacks read them; so the module's data is laid out as retail's
 *     .data, which ebx points into.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define CHAIN_LISTS 35

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

struct chain_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;       /* retail's literal 0xe0000000; meaning unknown */
    void    *dialogs;           /* +0x14: no dialog table */
    int32_t  (FS98_STDCALL *add)(uint32_t, chain_cb);
    int32_t  (FS98_STDCALL *remove)(uint32_t, chain_cb);
    void    *dispatch;          /* chain_dispatch, asm */
};

static void chain_init(void);
static void chain_deinit(void);
static int32_t FS98_STDCALL chain_add(uint32_t index, chain_cb cb);
static int32_t FS98_STDCALL chain_remove(uint32_t index, chain_cb cb);
void chain_dispatch(void) __asm__("_chain_dispatch");

/* Retail's .data, as it lies: the ImportTable (+0x00), the Linkage (+0x18),
 * a zero word, the lists (+0x40), then the module-wide flag that some list
 * has a deferred removal (+0xcc).  Init clears the lists and not the flag.
 * ImportTable and Linkage are exported as names for its first two parts. */
struct chain_data {
    struct fs98_import imports[3];
    struct chain_linkage linkage;
    uint32_t zero_3c;
    struct chain_list *list[CHAIN_LISTS];
    uint32_t pending;
};

struct chain_data chain = {
    { { FS98_MOD_HOST, 0 }, { FS98_MOD_GLOBAL, 0 } /* imported; never read */, { 0, 0 } },
    { FS98_MOD_CHAIN, chain_init, chain_deinit, 0, 0xe0000000u, 0,
      chain_add, chain_remove, (void *)chain_dispatch },
    0, { 0 }, 0
};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _chain + 0x0\n\t"
        ".globl _Linkage\n\t.set _Linkage, _chain + 0x18");

#define HOST ((void *)chain.imports[0].link)

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
uint32_t chain_ecx, chain_edx;            /* what dispatch returns in them */

static int32_t chain_compact(void)
{
    int32_t done = 1;
    unsigned k;

    for (k = 0; k < CHAIN_LISTS; k++) {
        struct chain_list *l = chain.list[k];
        int32_t i, n, walked;

        if (!l)
            continue;
        chain_edx = l->count;               /* every allocated list's count */
        if (l->count == 0 || l->pending == 0)
            continue;
        if (l->depth > 0) {
            done = 0;
            continue;
        }
        n = (int32_t)l->count;
        walked = n > 0;
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
        if (walked) {
            /* the walk's pointer to the flag one past the last entry left */
            chain_ecx = (uint32_t)(uintptr_t)&l->entry[l->count].removed;
            chain_edx = l->count;
        }
        l->pending = 0;
    }
    return done;
}

/* ---- dispatch (.text+0x200, +0x220) -----------------------------------------
 *
 * The return value is whatever retail leaves in EAX -- 0, the list pointer
 * for an empty list, or the compaction result -- and every caller in the
 * retail modules overwrites it unread.  ecx and edx come back as retail
 * leaves them: b and a with no list (the +0x20 wrapper loads them); the
 * count, 0, and a for an empty one; else the depth after the decrement and
 * the last callback's edx (the depth before any), which a compaction then
 * overwrites with its walk's pointer and every allocated list's count. */

/* The callback with retail's registers (r: eax, edx, ebx, esi, edi), ecx a
 * and ebp b; its edx back. */
uint32_t chain_call(chain_cb cb, uint32_t a, uint32_t b, const uint32_t *r) __asm__("_chain_call");
__asm__(".text\n\t.p2align 4\n"
        "_chain_call:\n\t"
        "pushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 32(%esp), %eax\n\t"
        "movl 8(%eax), %ebx\n\tmovl 12(%eax), %esi\n\tmovl 16(%eax), %edi\n\t"
        "movl 4(%eax), %edx\n\tmovl (%eax), %eax\n\t"
        "movl 28(%esp), %ebp\n\tmovl 24(%esp), %ecx\n\t"
        "pushl %ebp\n\tpushl %ecx\n\t"
        "call *28(%esp)\n\t"
        "movl %edx, %eax\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret");

uint32_t chain_dispatch_body(uint32_t index, uint32_t a, uint32_t b) __asm__("_chain_dispatch_body");
uint32_t chain_dispatch_body(uint32_t index, uint32_t a, uint32_t b)
{
    struct chain_list **pp = &chain.list[index];
    uint32_t i, r, edx;

    if (!*pp) {
        chain_ecx = b;
        chain_edx = a;
        return 0;
    }
    if ((*pp)->count == 0) {
        chain_ecx = 0;
        chain_edx = a;
        return (uint32_t)(uintptr_t)*pp;
    }

    edx = ++(*pp)->depth;
    for (i = 0; i < (*pp)->count; i++)     /* count and block reread each step */
        if (!(*pp)->entry[i].removed) {
            uint32_t regs[5];

            regs[0] = (uint32_t)(uintptr_t)*pp;
            regs[1] = edx;
            regs[2] = (uint32_t)(uintptr_t)pp;
            regs[3] = 0x10 + 8 * i;
            regs[4] = i;
            edx = chain_call((*pp)->entry[i].callback, a, b, regs);
        }
    chain_ecx = --(*pp)->depth;
    chain_edx = edx;

    if (!chain.pending)
        return 0;
    r = (uint32_t)chain_compact();
    if (r)
        chain.pending = 0;
    return r;
}

/* +0x20 (index, a, b), stdcall: the body, then ecx and edx as retail's. */
__asm__(".text\n\t.p2align 4\n"
        "_chain_dispatch:\n\t"
        "pushl 12(%esp)\n\tpushl 12(%esp)\n\tpushl 12(%esp)\n\t"
        "call _chain_dispatch_body\n\t"
        "addl $12, %esp\n\t"
        "movl _chain_ecx, %ecx\n\tmovl _chain_edx, %edx\n\t"
        "ret $12");

/* ---- the module ------------------------------------------------------------ */

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
