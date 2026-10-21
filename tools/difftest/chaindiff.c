/* Differential test: retail CHAIN.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven only through their
 * Linkage tables, so what is tested is the packaged module the game loads.
 * Each gets the same stub host: an allocator that numbers heap blocks in
 * allocation order, fills new memory with a chosen byte (a list's dispatch
 * depth is never initialised, so that byte matters), and can be told to fail
 * the Nth allocation; plus a logging memmove.
 *
 * Callbacks are eight stdcall (a, b) functions that log the call and then
 * act back on the module under test -- removing themselves or another
 * callback, adding one, or dispatching a list from inside a dispatch -- so
 * deferred removal, compaction and re-entry are all exercised.
 *
 * A scenario is a random script run once against each module.  Compared: the
 * transcript of every host call, callback and return value, and the bytes of
 * every live list block; then deinit's frees.  Callbacks are asm thunks that
 * log every register they are entered with (retail's ebx points into its own
 * .data, logged as an offset from the ImportTable) and return a scripted edx,
 * which the next callback receives; dispatch's ecx and edx are logged as it
 * returns them.  CHAIN keeps its state in its
 * own .data, which differs between the two builds, so state is compared
 * through what is observable: blocks, calls and returns.
 *
 * Build: make chaindiff     Run: make chaindiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

struct import { uint32_t id; void *link; };
typedef void (STDCALL *cb_fn)(uint32_t, uint32_t);
struct chain_linkage {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t r0c, r10;
    void *dialogs;
    int32_t  (STDCALL *add)(uint32_t, cb_fn);
    int32_t  (STDCALL *remove)(uint32_t, cb_fn);
    uint32_t (STDCALL *dispatch)(uint32_t, uint32_t, uint32_t);
};

/* ---- the transcript -------------------------------------------------------- */

static char tlog[1 << 18];
static unsigned tlen;

static void put(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (tlen < sizeof tlog - 1)
        tlen += (unsigned)vsnprintf(tlog + tlen, sizeof tlog - tlen, fmt, ap);
    va_end(ap);
}

/* ---- the stub host ---------------------------------------------------------- */

#define MAXBLK 256
static unsigned char *blk[MAXBLK];
static uint32_t blksize[MAXBLK];
static unsigned nblk, nalloc_calls;
static unsigned char fill_byte;
static unsigned fail_at;            /* 1-based allocation/realloc call to fail; 0 none */

static int blk_index(const void *p)
{
    unsigned i;
    for (i = 0; i < nblk; i++) if (blk[i] && blk[i] == p) return (int)i;
    return -1;
}

static int32_t STDCALL h_alloc(int32_t fl, uint32_t size, void **out, int32_t tag)
{
    unsigned char *p;
    nalloc_calls++;
    put("alloc(%d,%#x,tag %d)", fl, size, tag);
    if (fail_at && nalloc_calls == fail_at) { put("=FAIL "); return -1; }
    p = malloc(size);
    memset(p, fill_byte, size);
    if (nblk < MAXBLK) { blk[nblk] = p; blksize[nblk] = size; }
    put("=#%u ", nblk++);
    *out = p;
    return 0;
}

static int32_t STDCALL h_realloc(int32_t fl, uint32_t size, void **pp)
{
    int i = blk_index(*pp);
    unsigned char *q;
    nalloc_calls++;
    put("realloc(#%d,%d,%#x)", i, fl, size);
    if (fail_at && nalloc_calls == fail_at) { put("=FAIL "); return -1; }
    q = malloc(size);
    memset(q, fill_byte, size);
    if (i >= 0) {
        memcpy(q, blk[i], blksize[i] < size ? blksize[i] : size);
        free(blk[i]);
        blk[i] = q; blksize[i] = size;
    }
    put(" ");
    *pp = q;
    return 0;
}

static int32_t STDCALL h_free(void **pp)
{
    int i = *pp ? blk_index(*pp) : -2;
    put("free(#%d) ", i);
    if (i >= 0) { free(blk[i]); blk[i] = NULL; }
    *pp = NULL;
    return 0;
}

/* a pointer as "block k + offset", so the two sides compare */
static void put_ptr(const void *p)
{
    unsigned i;
    for (i = 0; i < nblk; i++)
        if (blk[i] && (const unsigned char *)p >= blk[i]
            && (const unsigned char *)p < blk[i] + blksize[i]) {
            put("#%u+%#x", i, (unsigned)((const unsigned char *)p - blk[i]));
            return;
        }
    put("%p", p);
}

static int32_t STDCALL h_memmove(void *d, const void *s, uint32_t n)
{
    put("memmove("); put_ptr(d); put(","); put_ptr(s); put(",%#x) ", n);
    memmove(d, s, n);
    return 0;
}

static unsigned char host_blob[0x320];
static unsigned char global_blob[0x1300];

/* ---- calling with registers seen ---------------------------------------------- */

static struct chain_linkage *cur;   /* the module being driven */
static uint32_t cur_imports;         /* its ImportTable, which its .data starts at */

/* dispatch, its ecx and edx as it returns them */
uint32_t disp_ecx, disp_edx;
uint32_t call_dispatch(void *fn, uint32_t i, uint32_t a, uint32_t b) __asm__("_call_dispatch");
__asm__(".text\n_call_dispatch:\n\t"
        "pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tpushl %ebp\n\t"
        "pushl 32(%esp)\n\tpushl 32(%esp)\n\tpushl 32(%esp)\n\t"
        "call *32(%esp)\n\t"
        "movl %ecx, _disp_ecx\n\tmovl %edx, _disp_edx\n\t"
        "popl %ebp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");

static void put_ptr(const void *p);

/* a register that may point into a block, as a block offset; else raw */
static void put_reg(uint32_t v)
{
    put_ptr((const void *)(uintptr_t)v);
}

static uint32_t dispatch(uint32_t i, uint32_t a, uint32_t b)
{
    uint32_t r = call_dispatch((void *)cur->dispatch, i, a, b);

    put("[ecx ");
    put_reg(disp_ecx);
    put(" edx %#x]", disp_edx);
    return r;
}

/* ---- callbacks that act on the module under test ---------------------------- */

static unsigned depth, max_depth = 3;
/* A callback that adds to the list being dispatched is called by that same
 * dispatch -- retail rereads the count -- so adds can chain without end.
 * Callbacks therefore act at most this many times per run, alike on both
 * sides; after that they only log. */
static unsigned budget;

enum act { A_NONE, A_REMOVE_SELF, A_REMOVE_OTHER, A_ADD_OTHER, A_DISPATCH,
           A_DISPATCH_SELF, A_REMOVE_SELF_TWICE, A_NACTS };
struct beh { int act; unsigned other; uint32_t list; };
static struct beh beh[8];
static cb_fn cbs[8];

static void act_on(unsigned k, uint32_t a, uint32_t b)
{
    const struct beh *x = &beh[k];
    uint32_t r;
    if (budget == 0)
        return;
    budget--;
    switch (x->act) {
    case A_REMOVE_SELF:
        put("[rm self]=%d ", cur->remove(a, cbs[k]));
        break;
    case A_REMOVE_SELF_TWICE:
        r = (uint32_t)cur->remove(a, cbs[k]);
        put("[rm self x2]=%d,%d ", (int)r, cur->remove(a, cbs[k]));
        break;
    case A_REMOVE_OTHER:
        put("[rm %u from %u]=%d ", x->other, x->list, cur->remove(x->list, cbs[x->other]));
        break;
    case A_ADD_OTHER:
        put("[add %u to %u]=%d ", x->other, x->list, cur->add(x->list, cbs[x->other]));
        break;
    case A_DISPATCH:
    case A_DISPATCH_SELF:
        if (depth < max_depth) {
            uint32_t l = x->act == A_DISPATCH ? x->list : a;
            depth++;
            put("[disp %u{ ", l);
            r = dispatch(l, l, b + 1);
            put("}=");
            if (r > 0x10000) put_ptr((void *)(uintptr_t)r); else put("%u", r);
            put("] ");
            depth--;
        }
        break;
    }
    (void)b;
}

/* The callbacks: thunks that keep the registers they were entered with and
 * come back with junk in eax and ecx and the scenario's edx for them. */
uint32_t cap[7], ret_edx[8];
void STDCALL cb_body(uint32_t k, uint32_t a, uint32_t b) __asm__("_cb_body@12");
void STDCALL cb_body(uint32_t k, uint32_t a, uint32_t b)
{
    uint32_t r[7];

    memcpy(r, cap, sizeof r);
    put("cb%u(%u,%u) eax ", k, a, b);
    put_reg(r[0]);
    put(" ecx %#x edx %#x ebx .data+%#x esi %#x edi %#x ebp %#x ", r[1], r[2],
        r[3] - cur_imports, r[4], r[5], r[6]);
    act_on(k, a, b);
}
#define CB(k) void STDCALL cb##k(uint32_t a, uint32_t b) __asm__("_cbt" #k); \
    __asm__(".text\n_cbt" #k ":\n\t"                                           \
            "movl %eax, _cap\n\tmovl %ecx, _cap+4\n\tmovl %edx, _cap+8\n\t"     \
            "movl %ebx, _cap+12\n\tmovl %esi, _cap+16\n\tmovl %edi, _cap+20\n\t" \
            "movl %ebp, _cap+24\n\t"                                              \
            "pushl 8(%esp)\n\tpushl 8(%esp)\n\tpushl $" #k "\n\t"               \
            "call _cb_body@12\n\t"                                                \
            "movl $0xdead000" #k ", %eax\n\tmovl $0xbeef000" #k ", %ecx\n\t"     \
            "movl _ret_edx+4*" #k ", %edx\n\t"                                    \
            "ret $8");
CB(0) CB(1) CB(2) CB(3) CB(4) CB(5) CB(6) CB(7)

/* ---- scripts ---------------------------------------------------------------- */

static uint32_t rs = 0x2545f491u;
static uint32_t rng(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

enum { OP_ADD, OP_REMOVE, OP_DISPATCH };
struct op { int kind; uint32_t list; unsigned cb; uint32_t b; };
#define MAXOPS 80
struct scenario {
    struct op op[MAXOPS];
    unsigned nops;
    uint32_t lists[4];
    unsigned nlists;
    unsigned char fill;
    unsigned fail_at;
    struct beh beh[8];
    uint32_t ret_edx[8];
};

static void make_scenario(struct scenario *s, unsigned v)
{
    unsigned i, k;
    memset(s, 0, sizeof *s);
    s->nlists = 1 + rng() % 3;
    for (i = 0; i < s->nlists; i++)
        s->lists[i] = (v % 7 == 0) ? (v / 7 + i * 11) % 35 : rng() % 35;
    /* The fill is the dispatch depth a new list starts with.  0xff matters
     * most: a depth of 0xffffffff wraps to 0 inside a dispatch, so removals
     * there are immediate while those outside are deferred -- the only way a
     * list can empty with removals still pending. */
    s->fill = (v % 4 == 0) ? 0xab : (v % 4 == 1) ? 0x00 : (v % 4 == 2) ? 0xff
            : (unsigned char)(rng() % 3);
    s->fail_at = (v % 11 == 5) ? 1 + rng() % 6 : 0;
    for (k = 0; k < 8; k++) {
        s->ret_edx[k] = rng();
        s->beh[k].act = (rng() % 3 == 0) ? A_NONE : (int)(rng() % A_NACTS);
        s->beh[k].other = rng() % 8;
        s->beh[k].list = s->lists[rng() % s->nlists];
    }
    /* every list starts with an anchor, so nothing removes from a list that
     * was never allocated -- which faults retail -- unless an allocation
     * failure is being injected, and then nothing removes at all */
    for (i = 0; i < s->nlists; i++) {
        s->op[s->nops].kind = OP_ADD; s->op[s->nops].list = s->lists[i];
        s->op[s->nops].cb = i; s->nops++;
    }
    while (s->nops < 10 + rng() % (MAXOPS - 10)) {
        struct op *o = &s->op[s->nops++];
        unsigned r = rng() % 10;
        o->list = s->lists[rng() % s->nlists];
        o->cb = rng() % 8;
        o->b = rng() % 100;
        o->kind = r < 5 ? OP_ADD : r < 7 ? OP_REMOVE : OP_DISPATCH;
        if (s->fail_at && o->kind == OP_REMOVE) o->kind = OP_ADD;
    }
    if (s->fail_at)
        for (k = 0; k < 8; k++)
            if (s->beh[k].act != A_ADD_OTHER && s->beh[k].act != A_DISPATCH
                && s->beh[k].act != A_DISPATCH_SELF)
                s->beh[k].act = A_NONE;
}

static void reset_host(const struct scenario *s)
{
    unsigned i;
    for (i = 0; i < nblk; i++) if (blk[i]) free(blk[i]);
    memset(blk, 0, sizeof blk);
    nblk = 0; nalloc_calls = 0;
    fill_byte = s->fill;
    fail_at = s->fail_at;
    tlen = 0; tlog[0] = 0;
}

static void run(struct chain_linkage *m, const struct scenario *s)
{
    unsigned i;
    reset_host(s);
    memcpy(beh, s->beh, sizeof beh);
    memcpy(ret_edx, s->ret_edx, sizeof ret_edx);
    cur = m; depth = 0; budget = 40;
    m->init();
    for (i = 0; i < s->nops; i++) {
        const struct op *o = &s->op[i];
        uint32_t r;
        switch (o->kind) {
        case OP_ADD:
            put("\nADD %u cb%u=%d ", o->list, o->cb, m->add(o->list, cbs[o->cb]));
            break;
        case OP_REMOVE:
            put("\nREM %u cb%u=%d ", o->list, o->cb, m->remove(o->list, cbs[o->cb]));
            break;
        case OP_DISPATCH:
            put("\nDISP %u{ ", o->list);
            r = dispatch(o->list, o->list, o->b);
            put("}=");
            if (r > 0x10000) put_ptr((void *)(uintptr_t)r); else put("%u", r);
            break;
        }
    }
    /* every live block, byte for byte */
    put("\nBLOCKS ");
    for (i = 0; i < nblk; i++)
        if (blk[i]) {
            uint32_t k;
            put("#%u:", i);
            for (k = 0; k < blksize[i]; k += 4) {
                uint32_t w = *(uint32_t *)(blk[i] + k);
                unsigned c;
                for (c = 0; c < 8 && (void *)(uintptr_t)w != (void *)cbs[c]; c++) ;
                if (c < 8) put("cb%u,", c); else put("%x,", w);
            }
            put(" ");
        }
    put("\nDEINIT ");
    m->deinit();
}

/* ---- loading ---------------------------------------------------------------- */

static int fault_where_retail;
static unsigned fault_scenario;
static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    printf("\nFAULT %#lx at %p in %s, scenario %u\n", e->ExceptionRecord->ExceptionCode,
           e->ExceptionRecord->ExceptionAddress,
           fault_where_retail ? "retail" : "openfs98", fault_scenario);
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static uint32_t imports_of[2];

static struct chain_linkage *load(const char *path, int which)
{
    HMODULE h = LoadLibraryA(path);
    struct import *im;
    struct chain_linkage *l;
    if (!h) { printf("cannot load %s (%lu)\n", path, GetLastError()); return NULL; }
    im = (struct import *)GetProcAddress(h, "ImportTable");
    l  = (struct chain_linkage *)GetProcAddress(h, "Linkage");
    if (!im || !l || l->id != 4) { printf("%s: not CHAIN\n", path); return NULL; }
    imports_of[which] = (uint32_t)(uintptr_t)im;
    for (; im->id; im++) {
        if (im->id == 1) im->link = host_blob;
        if (im->id == 2) im->link = global_blob;
    }
    return l;
}

int main(int argc, char **argv)
{
    static char rlog[sizeof tlog];
    struct chain_linkage *r, *o;
    static struct scenario s;
    unsigned v, n = argc > 1 ? (unsigned)atoi(argv[1]) : 20000, bad = 0;
    /* what the scripts reached, counted over retail's transcripts, so a
     * passing run says what it covered */
    static const char *seen_what[] = {
        "realloc(", "=FAIL", "memmove(", "[disp ", "[rm self x2]", "=-1 ",
        "[rm self]=0", "[add ", "free(#"
    };
    unsigned long seen[9] = { 0 }, garbage_depth = 0;

    SetUnhandledExceptionFilter(on_fault);
    *(uint32_t *)host_blob = 1;
    *(void **)(host_blob + 0x20c) = (void *)h_alloc;
    *(void **)(host_blob + 0x210) = (void *)h_realloc;
    *(void **)(host_blob + 0x214) = (void *)h_free;
    *(void **)(host_blob + 0x21c) = (void *)h_memmove;
    cbs[0] = cb0; cbs[1] = cb1; cbs[2] = cb2; cbs[3] = cb3;
    cbs[4] = cb4; cbs[5] = cb5; cbs[6] = cb6; cbs[7] = cb7;

    r = load("CHAIN_retail.dll", 0);
    o = load("CHAIN_ours.dll", 1);
    if (!r || !o) return 2;
    if (r->r10 != o->r10 || r->r0c != o->r0c || r->dialogs != o->dialogs) {
        printf("Linkage constants differ\n");
        bad++;
    }

    for (v = 0; v < n; v++) {
        make_scenario(&s, v);
        fault_scenario = v;
        fault_where_retail = 1;
        cur_imports = imports_of[0];
        run(r, &s);
        memcpy(rlog, tlog, tlen + 1);
        {
            unsigned w;
            for (w = 0; w < 9; w++) {
                const char *p = rlog;
                while ((p = strstr(p, seen_what[w])) != NULL) { seen[w]++; p++; }
            }
            garbage_depth += s.fill != 0;
        }
        fault_where_retail = 0;
        cur_imports = imports_of[1];
        run(o, &s);
        if (strcmp(rlog, tlog) && bad++ < 4) {
            unsigned at = 0;
            while (rlog[at] && rlog[at] == tlog[at]) at++;
            printf("scenario %u (fill %#x, fail_at %u) differs at byte %u:\n"
                   "  retail ...%.160s\n  ours   ...%.160s\n", v, s.fill, s.fail_at, at,
                   rlog + (at > 60 ? at - 60 : 0), tlog + (at > 60 ? at - 60 : 0));
        }
    }
    printf("covered: %lu reallocs, %lu injected failures, %lu memmoves (compaction "
           "or immediate removal), %lu nested dispatches, %lu double self-removals, "
           "%lu calls returning -1, %lu self-removals, %lu adds from callbacks, "
           "%lu frees; %lu scenarios with non-zero fill (garbage depth)\n",
           seen[0], seen[1], seen[2], seen[3], seen[4], seen[5], seen[6], seen[7],
           seen[8], garbage_depth);
    printf("%u scenarios, %u differ\n", n, bad);
    if (!bad) printf("IDENTICAL\n");
    return bad != 0;
}
