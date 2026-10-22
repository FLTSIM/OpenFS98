/* Differential test: retail mpchat.DLL against the OpenFS98 build, whole-module.
 *
 * Both DLLs are loaded into one process and driven, one side after the
 * other over the same script, the way the sim drives MPCHAT: Linkage init
 * and deinit, the CHAIN callbacks it registers (list 0x1d, MULTIP's session
 * news; 0x14, 0xb and 0xc once a session has begun), the host's dialog
 * runner calling its dialog table's create, and the window system calling
 * its window procedure.  What MPCHAT reaches for:
 *
 *   USER32,   a model of the window system, hooked into both DLLs' import
 *   KERNEL32, tables by name (every import must be hooked, or the harness
 *   GDI32     stops).  Windows are records with handles numbered the same
 *             on both sides; the registered class's procedure is called as
 *             Windows would call it: WM_GETMINMAXINFO, WM_NCCREATE,
 *             WM_NCCALCSIZE and WM_CREATE from CreateWindowExA, WM_SIZE
 *             (at the creation or the first show, by scenario),
 *             WM_SHOWWINDOW, WM_ACTIVATE and WM_SETFOCUS from ShowWindow,
 *             WM_DESTROY from DestroyWindow, and SendMessageA to it.  The
 *             controls are models too: a ListBox keeps its strings, an Edit
 *             the user's line, a RichEdit its text and selection, and
 *             GetScrollInfo answers with a scroll state the scenario picks
 *             -- at the bottom, one off it, exactly on either of MPCHAT's
 *             two comparisons, or anywhere.  Everything MPCHAT passes is
 *             logged: strings, the masked fields of the formats, the
 *             geometry.
 *   host      a stub: strings 0x9400..0x9402 as heap copies (sometimes
 *             none), the dialog runner (Linkage +0x14's table read from the
 *             side's own image, then ShowWindow(SW_SHOW) and UpdateWindow,
 *             as the host does), the menu, the module stack, the string
 *             services, PostMessage.
 *   CHAIN     add and remove kept per side; the harness dispatches.
 *   MULTIP    +0x24..+0x2c answer from player lists the harness owns (the
 *             same memory for both sides), changed between ops; +0x20 logs
 *             the payload.
 *   WINMSG    +0x18, logged with its text.
 *
 * Compared after every operation: the transcript of every call and its
 * return, MPCHAT's whole .data image (code pointers paired as first seen,
 * pointers into the image as offsets), the heap and the model's state.
 * A fault is compared as an outcome and ends the scenario.
 *
 * Two places read what nothing wrote, in retail and in ours alike: the
 * RichEdit append's EM_EXGETSEL result, when the RichEdit does not exist
 * (a chat line after the session's window is gone), and GetClientRect's
 * rectangle for a window that does not.  The model writes zeros into both
 * for a bad handle, so the comparison is of the code, not of the stack.
 *
 * Build: make mpchatdiff     Run: make mpchatdiff-run
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STDCALL __attribute__((stdcall))

#define IMAGE_SIZE   0x50cu
#define ARENA_SIZE   (1u << 20)
#define TRANSCRIPT_SIZE (4u << 20)
#define MAX_OPS      60
#define MAX_PAIRS    32

typedef struct { LONG cpMin, cpMax; } CHARRANGE;    /* richedit.h's */
#define MAX_WINDOWS  64
#define RICH_MAX     0x6000u
#define TEXT_MAX     0x800u

struct import { uint32_t id; void *link; };

typedef void (STDCALL *callback_fn)(uint32_t, uint32_t);
typedef uint32_t (STDCALL *create_fn)(uint32_t, uint32_t);

struct reg { uint32_t list; callback_fn fn; };

struct side {
    const char *name;
    HMODULE module;
    struct import *imports;
    uint8_t *link;
    uint8_t *image;
    uint8_t *lo, *hi;
    uint8_t pristine[IMAGE_SIZE];
    struct reg regs[32];
    uint32_t nregs;
    char *transcript;
    uint32_t length;
};

static struct side retail, ours;
static struct side *cur;
static uint8_t host[0x320], global[0x10], chain[0x24], multip[0x40], winmsg[0x28];
static uint8_t arena[ARENA_SIZE] __attribute__((aligned(16)));
static uint32_t arena_used;
static uint32_t rng, scenario_seed;
static uint32_t scenario, op_number;
static int counting;
static uint32_t pair_retail[MAX_PAIRS], pair_ours[MAX_PAIRS], npairs, npairs_ours;
static int size_at_show;

static unsigned long cover_ops[24], cover_sent, cover_received, cover_unknown, cover_bar,
    cover_scroll, cover_restore, cover_long_line,
    cover_empty_line, cover_toggle[2], cover_faults, cover_sessions, cover_stale,
    cover_no_strings, cover_resize_wild, cover_size_at_show, cover_list_rows, cover_null_name,
    cover_av;

#define SET_SLOT(table, off, fn) (*(void **)((table) + (off)) = (void *)(fn))

/* ---- utilities ---------------------------------------------------------------- */

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static uint32_t rnd_n(uint32_t n)
{
    return n ? rnd() % n : 0;
}

static uint32_t digest(const void *p, uint32_t n, uint32_t h)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 0x01000193u;
    return h;
}

static void put(const char *format, ...)
{
    va_list args;
    int n;

    if (cur == NULL || cur->length >= TRANSCRIPT_SIZE - 8192)
        return;
    va_start(args, format);
    n = vsnprintf(cur->transcript + cur->length, TRANSCRIPT_SIZE - cur->length,
                  format, args);
    va_end(args);
    if (n > 0)
        cur->length += (uint32_t)n;
}

static int in_arena(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;
    return b >= arena && b < arena + ARENA_SIZE;
}

static int in_image(uint32_t v)
{
    return cur != NULL && v >= (uint32_t)(uintptr_t)cur->image &&
           v < (uint32_t)(uintptr_t)cur->image + IMAGE_SIZE;
}

static int in_module(uint32_t v)
{
    return cur != NULL && v >= (uint32_t)(uintptr_t)cur->lo &&
           v < (uint32_t)(uintptr_t)cur->hi;
}

static uint32_t pair_code(uint32_t v)
{
    uint32_t *list = cur == &retail ? pair_retail : pair_ours;
    uint32_t *n = cur == &retail ? &npairs : &npairs_ours;
    uint32_t i;

    if (v == 0)
        return 0;
    for (i = 0; i < *n; i++)
        if (list[i] == v)
            return 0xc0de0000u | (i + 1);
    if (*n < MAX_PAIRS) {
        list[*n] = v;
        return 0xc0de0000u | ++*n;
    }
    return 0xc0deffffu;
}

#define HWND_BASE 0x30000u
#define MAIN_HWND 0x10020u

static uint8_t fixtures[0x40000] __attribute__((aligned(16)));

static void put_ptr(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;

    if (v == 0)
        put("null");
    else if (in_arena(v))
        put("arena+%#x", (uint32_t)(b - arena));
    else if (in_image(v))
        put("image+%#x", v - (uint32_t)(uintptr_t)cur->image);
    else if (b >= fixtures && b < fixtures + sizeof fixtures)
        put("fix+%#x", (uint32_t)(b - fixtures));
    else if (in_module(v))
        put("code%u", pair_code(v) & 0xffff);
    else if (v >= HWND_BASE && v < HWND_BASE + 4 * MAX_WINDOWS)
        put("w%u", (v - HWND_BASE) / 4);
    else if (v == 0x400000)
        put("host-instance");
    else if (v == 0xe40000)
        put("mpchat-instance");
    else if (v < 0x10000 || v == MAIN_HWND)
        put("%#x", v);
    else
        put("other");
}

/* A message's parameter: a number, unless it is one of the pointers both
 * sides share the meaning of.  Never read as code -- a WM_SIZE's packed
 * size can land in either module's range. */
static void put_arg(uint32_t v)
{
    const uint8_t *b = (const uint8_t *)(uintptr_t)v;

    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();

    if (v != 0 && (in_arena(v) || in_image(v) || (b >= fixtures && b < fixtures + sizeof fixtures) ||
                   (v >= HWND_BASE && v < HWND_BASE + 4 * MAX_WINDOWS)))
        put_ptr(v);
    else if ((void *)b >= tib->StackLimit && (void *)b < tib->StackBase)
        put("stack");
    else
        put("%#x", v);
}

static void put_s(const char *s)
{
    uint32_t i;

    if (s == NULL) {
        put("null");
        return;
    }
    if ((uintptr_t)s < 0x10000) {
        put("#%#x", (uint32_t)(uintptr_t)s);
        return;
    }
    put("'");
    for (i = 0; s[i] && i < 0x900; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c >= 0x20 && c < 0x7f && c != '\\')
            put("%c", c);
        else
            put("\\%02x", c);
    }
    put("'");
}

static void put_bytes(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;

    put("[");
    for (i = 0; i < n && i < 0x30; i++)
        put("%02x", b[i]);
    if (n > 0x30)
        put("..%08x", digest(b, n, 0x811c9dc5u));
    put("]");
}

/* ---- faults ---------------------------------------------------------------------- */

static void *fault_jb[5];
static volatile int in_mpchat;
static volatile int faulted;

static void fault_land(void)
{
    __builtin_longjmp(fault_jb, 1);
}

static void stub_fault(const char *what)
{
    put(" FAULT(%s)", what);
    if (counting) {
        cover_faults++;
        if (!strcmp(what, "strcat null")) cover_null_name++;
    }
    in_mpchat = 0;
    faulted = 1;
    __builtin_longjmp(fault_jb, 1);
}

static LONG WINAPI on_vectored(EXCEPTION_POINTERS *e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;

    if (code == EXCEPTION_ACCESS_VIOLATION &&
        (e->ContextRecord->Eip & 0xff000000u) == 0xde000000u) {
        printf("scenario %u op %u %s: called module %lu's unstubbed +%#lx\n", scenario,
               op_number, cur ? cur->name : "none", (e->ContextRecord->Eip >> 16) & 0xff,
               e->ContextRecord->Eip & 0xffff);
        fflush(stdout);
        ExitProcess(5);
    }
    if (!in_mpchat || cur == NULL)
        return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_ACCESS_VIOLATION)
        put(" FAULT(av)");
    else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_INT_OVERFLOW)
        put(" FAULT(div)");
    else
        return EXCEPTION_CONTINUE_SEARCH;
    if (getenv("MPCHAT_EXC")) {
        uint8_t *ip = (uint8_t *)(uintptr_t)e->ContextRecord->Eip;
        if (ip >= cur->lo && ip < cur->hi)
            put("@%s.text+%#x", cur->name, (uint32_t)(ip - cur->lo - 0x1000));
    }
    if (counting) {
        cover_faults++;
        cover_av++;
    }
    in_mpchat = 0;
    faulted = 1;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)fault_land;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* ---- the heap ------------------------------------------------------------------- */

#define LIVE  0x0a110cu
#define FREED 0xf4eeu

static void *arena_get(uint32_t size)
{
    uint32_t n = 16 + ((size + 15) & ~15u);
    uint8_t *p;

    if (arena_used + n > ARENA_SIZE) {
        printf("arena exhausted (scenario %u)\n", scenario);
        exit(2);
    }
    p = arena + arena_used;
    arena_used += n;
    memset(p, 0, 16);
    *(uint32_t *)p = size;
    *(uint32_t *)(p + 4) = LIVE;
    return p + 16;
}

static void arena_release(void *p)
{
    uint8_t *b = (uint8_t *)p;

    if (!in_arena((uint32_t)(uintptr_t)b) || ((uintptr_t)b & 15) != 0 ||
        *(uint32_t *)(b - 12) != LIVE)
        stub_fault("heap free");
    memset(b, 0xdd, *(uint32_t *)(b - 16));
    *(uint32_t *)(b - 12) = FREED;
}

/* ---- the window system ------------------------------------------------------------- */

enum { K_MAIN, K_RICH, K_LIST, K_EDIT, K_BUTTON, K_OTHER };

struct win {
    int used, gone, kind, active, sized;
    uint32_t style, exstyle, parent, id;
    int32_t x, y, w, h, cw, ch;
    WNDPROC proc;
    char text[TEXT_MAX];
    /* ListBox */
    uint32_t rows, rows_digest;
    /* RichEdit */
    char rich[RICH_MAX];
    uint32_t rich_len;
    int32_t sel_min, sel_max;
};

static struct win wins[MAX_WINDOWS];
static uint32_t nwins, focus;
static char class_name[32];
static WNDPROC class_proc;
static int class_registered;

static uint32_t handle_of(uint32_t i) { return HWND_BASE + 4 * i; }

static struct win *win_of(uint32_t h)
{
    uint32_t i;

    if (h < HWND_BASE || h >= HWND_BASE + 4 * MAX_WINDOWS || (h & 3) != 0)
        return NULL;
    i = (h - HWND_BASE) / 4;
    if (i >= nwins || !wins[i].used || wins[i].gone)
        return NULL;
    return &wins[i];
}

static LRESULT deliver(uint32_t h, UINT msg, WPARAM wp, LPARAM lp)
{
    struct win *w = win_of(h);
    LRESULT r;

    if (w == NULL || w->proc == NULL)
        return 0;
    put(" {proc(w%u,%#x,%#lx,", (h - HWND_BASE) / 4, msg, (unsigned long)wp);
    put_arg((uint32_t)lp);
    put(")");
    r = w->proc((HWND)(uintptr_t)h, msg, wp, lp);
    put(" =%#lx}", (unsigned long)r);
    return r;
}

static void deliver_size(uint32_t h)
{
    struct win *w = win_of(h);

    if (w == NULL)
        return;
    w->sized = 1;
    deliver(h, WM_SIZE, 0, (LPARAM)(((uint32_t)w->ch << 16) | ((uint32_t)w->cw & 0xffff)));
}

/* A client rectangle for a window of w x h: the frame taken off, now and
 * then anything at all -- narrower than the controls' margins, negative,
 * or wide enough that 0xc000 times it overflows. */
static void pick_client(struct win *w, int wild)
{
    uint32_t k = wild ? rnd_n(10) : 9;

    if (k == 0) {
        w->cw = (int32_t)rnd_n(0x60);
        w->ch = (int32_t)rnd_n(0x40);
    } else if (k == 1) {
        w->cw = (int32_t)rnd();
        w->ch = (int32_t)rnd();
    } else if (k == 2) {
        w->cw = 0x15555 + (int32_t)rnd_n(5) - 2;    /* where right * 0xc000 wraps */
        w->ch = 0x23 + (int32_t)rnd_n(3) - 1;
    } else if (k < 6) {
        w->cw = 0x60 + (int32_t)rnd_n(0x700);
        w->ch = 0x30 + (int32_t)rnd_n(0x500);
    } else {
        w->cw = w->w - 6 - (int32_t)rnd_n(4);
        w->ch = w->h - 0x19 - (int32_t)rnd_n(4);
    }
    if (counting && k < 3) cover_resize_wild++;
}

static HWND STDCALL u_CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR title, DWORD style,
                                      int x, int y, int cx, int cy, HWND parent, HMENU menu,
                                      HINSTANCE inst, LPVOID param)
{
    struct win *w;
    uint32_t h;
    int kind;
    static CREATESTRUCTA cs;
    static MINMAXINFO mmi;
    static NCCALCSIZE_PARAMS ncp;

    put(" CreateWindowEx(%#lx,", ex);
    put_s(cls);
    put(",");
    put_s(title);
    put(",%#lx,%d,%d,%d,%d,", style, x, y, cx, cy);
    put_ptr((uint32_t)(uintptr_t)parent);
    put(",%#x,", (uint32_t)(uintptr_t)menu);
    put_ptr((uint32_t)(uintptr_t)inst);
    put(",");
    put_ptr((uint32_t)(uintptr_t)param);
    put(")");
    if ((uintptr_t)cls < 0x10000)
        kind = K_OTHER;
    else if (class_registered && !_stricmp(cls, class_name))
        kind = K_MAIN;
    else if (!_stricmp(cls, "RichEdit"))
        kind = K_RICH;
    else if (!_stricmp(cls, "ListBox"))
        kind = K_LIST;
    else if (!_stricmp(cls, "Edit"))
        kind = K_EDIT;
    else if (!_stricmp(cls, "Button"))
        kind = K_BUTTON;
    else
        kind = K_OTHER;
    if (kind == K_OTHER || nwins >= MAX_WINDOWS) {
        put("=null");
        return NULL;
    }
    h = handle_of(nwins);
    w = &wins[nwins++];
    memset(w, 0, sizeof *w);
    w->used = 1;
    w->kind = kind;
    w->style = style;
    w->exstyle = ex;
    w->parent = (uint32_t)(uintptr_t)parent;
    w->id = (uint32_t)(uintptr_t)menu;
    w->x = x; w->y = y; w->w = cx; w->h = cy;
    if (title != NULL && (uintptr_t)title >= 0x10000)
        snprintf(w->text, sizeof w->text, "%s", title);
    put("=w%u", (h - HWND_BASE) / 4);
    if (kind != K_MAIN)
        return (HWND)(uintptr_t)h;
    w->proc = class_proc;
    pick_client(w, 0);
    memset(&cs, 0, sizeof cs);
    cs.lpCreateParams = param;
    cs.hInstance = inst;
    cs.hwndParent = parent;
    cs.cx = cx; cs.cy = cy; cs.x = x; cs.y = y;
    cs.style = (LONG)style;
    cs.lpszName = title;
    cs.lpszClass = cls;
    cs.dwExStyle = ex;
    deliver(h, WM_GETMINMAXINFO, 0, (LPARAM)&mmi);
    deliver(h, WM_NCCREATE, 0, (LPARAM)&cs);
    deliver(h, WM_NCCALCSIZE, 0, (LPARAM)&ncp);
    deliver(h, WM_CREATE, 0, (LPARAM)&cs);
    if (!size_at_show && win_of(h) != NULL) {
        deliver_size(h);
        deliver(h, WM_MOVE, 0, 0);
    }
    return (HWND)(uintptr_t)h;
}

static BOOL STDCALL u_ShowWindow(HWND hwnd, int cmd)
{
    uint32_t h = (uint32_t)(uintptr_t)hwnd;
    struct win *w = win_of(h);
    int was;

    put(" ShowWindow(");
    put_ptr(h);
    put(",%d)", cmd);
    if (w == NULL)
        return FALSE;
    was = (w->style & WS_VISIBLE) != 0;
    if (cmd == SW_HIDE)
        w->style &= ~WS_VISIBLE;
    else
        w->style |= WS_VISIBLE;
    if (w->kind == K_MAIN) {
        if (!was && cmd != SW_HIDE) {
            deliver(h, WM_SHOWWINDOW, 1, 0);
            if (!w->sized && win_of(h) != NULL) {
                if (counting) cover_size_at_show++;
                deliver_size(h);
            }
        } else if (was && cmd == SW_HIDE) {
            deliver(h, WM_SHOWWINDOW, 0, 0);
            w->active = 0;
        }
        if ((cmd == SW_SHOW || cmd == SW_SHOWNORMAL) && !w->active && win_of(h) != NULL) {
            w->active = 1;
            deliver(h, WM_ACTIVATE, WA_ACTIVE, 0);
            deliver(h, WM_SETFOCUS, 0, 0);
        }
    }
    put(" =%d", was);
    return was;
}

static BOOL STDCALL u_UpdateWindow(HWND hwnd)
{
    put(" UpdateWindow(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(")");
    return win_of((uint32_t)(uintptr_t)hwnd) != NULL;
}

static BOOL STDCALL u_DestroyWindow(HWND hwnd)
{
    uint32_t h = (uint32_t)(uintptr_t)hwnd, i;
    struct win *w = win_of(h);

    put(" DestroyWindow(");
    put_ptr(h);
    put(")");
    if (w == NULL)
        return FALSE;
    deliver(h, WM_DESTROY, 0, 0);
    for (i = 0; i < nwins; i++)
        if (wins[i].used && wins[i].parent == h)
            wins[i].gone = 1;
    deliver(h, WM_NCDESTROY, 0, 0);
    w->gone = 1;
    if (win_of(focus) == NULL)
        focus = 0;
    return TRUE;
}

static LONG STDCALL u_GetWindowLongA(HWND hwnd, int index)
{
    struct win *w = win_of((uint32_t)(uintptr_t)hwnd);
    LONG r = 0;

    if (w != NULL && index == GWL_STYLE)
        r = (LONG)w->style;
    else if (w != NULL && index == GWL_EXSTYLE)
        r = (LONG)w->exstyle;
    put(" GetWindowLong(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(",%d)=%#lx", index, (unsigned long)r);
    return r;
}

static HWND STDCALL u_SetFocus(HWND hwnd)
{
    uint32_t h = (uint32_t)(uintptr_t)hwnd, was = focus;

    put(" SetFocus(");
    put_ptr(h);
    put(")");
    if (h != 0 && win_of(h) == NULL)
        return NULL;
    focus = h;
    put("=");
    put_ptr(was);
    return (HWND)(uintptr_t)was;
}

static BOOL STDCALL u_GetClientRect(HWND hwnd, RECT *rc)
{
    struct win *w = win_of((uint32_t)(uintptr_t)hwnd);

    put(" GetClientRect(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(")");
    if (w == NULL) {
        memset(rc, 0, sizeof *rc);     /* see the header */
        return FALSE;
    }
    rc->left = 0;
    rc->top = 0;
    rc->right = w->cw;
    rc->bottom = w->ch;
    put("=%d,%d", w->cw, w->ch);
    return TRUE;
}

static BOOL STDCALL u_SetWindowPos(HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT f)
{
    struct win *w = win_of((uint32_t)(uintptr_t)hwnd);

    put(" SetWindowPos(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(",");
    put_ptr((uint32_t)(uintptr_t)after);
    put(",%d,%d,%d,%d,%#x)", x, y, cx, cy, f);
    if (w == NULL)
        return FALSE;
    w->x = x; w->y = y; w->w = cx; w->h = cy;
    return TRUE;
}

static int STDCALL u_GetWindowTextA(HWND hwnd, LPSTR buf, int n)
{
    struct win *w = win_of((uint32_t)(uintptr_t)hwnd);
    int len = 0;

    put(" GetWindowText(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(",%d)", n);
    if (n > 0)
        buf[0] = 0;
    if (w != NULL && n > 0) {
        len = (int)strlen(w->text);
        if (len > n - 1)
            len = n - 1;
        memcpy(buf, w->text, (size_t)len);
        buf[len] = 0;
    }
    put("=%d", len);
    return len;
}

static BOOL STDCALL u_SetWindowTextA(HWND hwnd, LPCSTR s)
{
    struct win *w = win_of((uint32_t)(uintptr_t)hwnd);

    put(" SetWindowText(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(",");
    put_s(s);
    put(")");
    if (w == NULL)
        return FALSE;
    snprintf(w->text, sizeof w->text, "%s", s ? s : "");
    return TRUE;
}

static LRESULT STDCALL u_DefDlgProcA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT r = msg == WM_NCCREATE ? 1 : msg == WM_CREATE ? 0 :
                (LRESULT)((msg * 0x9e37u + (uint32_t)wp * 7u) & 0xffffu);

    put(" DefDlgProc(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(",%#x,%#lx,", msg, (unsigned long)wp);
    put_arg((uint32_t)lp);
    put(")=%#lx", (unsigned long)r);
    return r;
}

/* The scroll state a RichEdit reports: picked to sit on MPCHAT's two
 * comparisons -- nMax against nPage + nPos after, nPos against
 * nMax - nPage before -- as often as anywhere else. */
static BOOL STDCALL u_GetScrollInfo(HWND hwnd, int bar, SCROLLINFO *si)
{
    struct win *w = win_of((uint32_t)(uintptr_t)hwnd);
    uint32_t k;

    put(" GetScrollInfo(");
    put_ptr((uint32_t)(uintptr_t)hwnd);
    put(",%d,%u,%#x)", bar, si->cbSize, si->fMask);
    if (w == NULL || rnd_n(14) == 0) {
        put("=0");
        return FALSE;
    }
    si->nMin = 0;
    si->nPage = rnd_n(5) == 0 ? 0 : rnd_n(12) == 0 ? rnd() : 1 + rnd_n(300);
    si->nMax = rnd_n(12) == 0 ? (int)rnd() : (int)rnd_n(3000);
    k = rnd_n(8);
    if (k < 2)
        si->nPos = (int)((uint32_t)si->nMax - si->nPage);
    else if (k < 4)
        si->nPos = (int)((uint32_t)si->nMax - si->nPage + rnd_n(3) - 1);
    else if (k == 4)
        si->nPos = (int)rnd();
    else
        si->nPos = (int)rnd_n((uint32_t)si->nMax + 1);
    si->nTrackPos = si->nPos;
    put("=%d,%d,%u,%d", si->nMin, si->nMax, si->nPage, si->nPos);
    return TRUE;
}

static void rich_replace(struct win *w, const char *s)
{
    uint32_t n = s ? (uint32_t)strlen(s) : 0, a, b;

    a = (uint32_t)w->sel_min;
    b = (uint32_t)w->sel_max;
    if (a > w->rich_len) a = w->rich_len;
    if (b > w->rich_len) b = w->rich_len;
    if (a > b) { uint32_t t = a; a = b; b = t; }
    if (w->rich_len - (b - a) + n >= RICH_MAX)
        n = RICH_MAX - 1 - (w->rich_len - (b - a));
    memmove(w->rich + a + n, w->rich + b, w->rich_len - b);
    memcpy(w->rich + a, s, n);
    w->rich_len = w->rich_len - (b - a) + n;
    w->sel_min = w->sel_max = (int32_t)(a + n);
}

static LRESULT STDCALL u_SendMessageA(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    uint32_t h = (uint32_t)(uintptr_t)hwnd;
    struct win *w = win_of(h);
    LRESULT r = 0;

    put(" Send(");
    put_ptr(h);
    put(",%#x,%#lx,", msg, (unsigned long)wp);
    switch (msg) {
    case LB_ADDSTRING:
    case EM_REPLACESEL:
        put_s((const char *)lp);
        break;
    case 0x437:                                                   /* EM_EXSETSEL */
        put("{%ld,%ld}", ((CHARRANGE *)lp)->cpMin, ((CHARRANGE *)lp)->cpMax);
        break;
    case 0x447: {                                                 /* EM_SETPARAFORMAT */
        const uint32_t *pf = (const uint32_t *)lp;
        put("pf{%#x,%#x,%d}", pf[0], pf[1], (int32_t)pf[3]);
        break;
    }
    case 0x444: {                                                 /* EM_SETCHARFORMAT */
        const uint32_t *cf = (const uint32_t *)lp;
        put("cf{%#x,%#x,%#x,%#x}", cf[0], cf[1], cf[2], cf[5]);
        break;
    }
    default:
        put_arg((uint32_t)lp);
    }
    put(")");
    if (w == NULL) {
        if (msg == 0x434)                                         /* see the header */
            memset((void *)lp, 0, sizeof(CHARRANGE));
        put("=0");
        return 0;
    }
    if (w->kind == K_MAIN) {
        r = deliver(h, msg, wp, lp);
    } else if (w->kind == K_LIST) {
        if (msg == LB_RESETCONTENT) {
            w->rows = 0;
            w->rows_digest = 0;
        } else if (msg == LB_ADDSTRING) {
            if (lp == 0) {
                r = LB_ERR;
            } else {
                w->rows_digest = digest((const char *)lp, (uint32_t)strlen((const char *)lp),
                                        w->rows_digest ^ w->rows);
                r = (LRESULT)w->rows++;
                if (counting) cover_list_rows++;
            }
        }
    } else if (w->kind == K_RICH) {
        switch (msg) {
        case 0x434:                                               /* EM_EXGETSEL */
            ((CHARRANGE *)lp)->cpMin = w->sel_min;
            ((CHARRANGE *)lp)->cpMax = w->sel_max;
            put("{%d,%d}", w->sel_min, w->sel_max);
            break;
        case WM_GETTEXTLENGTH:
            r = (LRESULT)w->rich_len;
            break;
        case 0x437: {
            int32_t a = ((CHARRANGE *)lp)->cpMin, b = ((CHARRANGE *)lp)->cpMax;
            if (b < 0 || (uint32_t)b > w->rich_len) b = (int32_t)w->rich_len;
            if (a < 0 || (uint32_t)a > w->rich_len) a = b;
            w->sel_min = a;
            w->sel_max = b;
            r = b;
            break;
        }
        case EM_REPLACESEL:
            rich_replace(w, (const char *)lp);
            break;
        case 0x447:
        case 0x444:
            r = 1;
            break;
        case WM_VSCROLL:
            if (counting) cover_scroll++;
            break;
        case EM_SETSEL:
            w->sel_min = w->sel_max = (int32_t)w->rich_len;
            break;
        }
    }
    put("=%#lx", (unsigned long)r);
    return r;
}

static ATOM STDCALL u_RegisterClassA(const WNDCLASSA *wc)
{
    put(" RegisterClass(%#x,", wc->style);
    put_ptr((uint32_t)(uintptr_t)wc->lpfnWndProc);
    put(",%d,%d,", wc->cbClsExtra, wc->cbWndExtra);
    put_ptr((uint32_t)(uintptr_t)wc->hInstance);
    put(",");
    put_ptr((uint32_t)(uintptr_t)wc->hIcon);
    put(",");
    put_ptr((uint32_t)(uintptr_t)wc->hCursor);
    put(",");
    put_ptr((uint32_t)(uintptr_t)wc->hbrBackground);
    put(",");
    put_s(wc->lpszMenuName);
    put(",");
    put_s(wc->lpszClassName);
    put(")");
    if (class_registered)
        return 0;
    snprintf(class_name, sizeof class_name, "%s", wc->lpszClassName);
    class_proc = wc->lpfnWndProc;
    class_registered = 1;
    return 0xc0a1;
}

static BOOL STDCALL u_UnregisterClassA(LPCSTR name, HINSTANCE inst)
{
    put(" UnregisterClass(");
    put_s(name);
    put(",");
    put_ptr((uint32_t)(uintptr_t)inst);
    put(")");
    if (!class_registered || _stricmp(name, class_name))
        return FALSE;
    class_registered = 0;
    return TRUE;
}

static HCURSOR STDCALL u_LoadCursorA(HINSTANCE inst, LPCSTR name)
{
    put(" LoadCursor(");
    put_ptr((uint32_t)(uintptr_t)inst);
    put(",%#x)", (uint32_t)(uintptr_t)name);
    return (HCURSOR)(uintptr_t)0x1234;
}

static HGDIOBJ STDCALL g_GetStockObject(int n)
{
    put(" GetStockObject(%d)", n);
    return (HGDIOBJ)(uintptr_t)(0x900 + n);
}

static HMODULE STDCALL k_LoadLibraryA(LPCSTR name)
{
    put(" LoadLibrary(");
    put_s(name);
    put(")");
    return (HMODULE)(uintptr_t)0x7e00;
}

static BOOL STDCALL k_FreeLibrary(HMODULE m)
{
    put(" FreeLibrary(%#x)", (uint32_t)(uintptr_t)m);
    return TRUE;
}

static BOOL STDCALL k_DisableThreadLibraryCalls(HMODULE m)
{
    (void)m;
    put(" DisableThreadLibraryCalls");
    return TRUE;
}

/* ---- the host -------------------------------------------------------------------- */

static const char *string_text[3];
static char string_buf[3][0x80];

static int32_t STDCALL h_string(uint32_t id, char **out)
{
    const char *s = id >= 0x9400 && id < 0x9403 ? string_text[id - 0x9400] : NULL;
    uint32_t n;

    put(" string(%#x,", id);
    put_ptr((uint32_t)(uintptr_t)out);
    put(")");
    if (s == NULL) {
        *out = NULL;
        put("=none");
        return 0;
    }
    n = (uint32_t)strlen(s);
    *out = (char *)arena_get(n + 1);
    memcpy(*out, s, n + 1);
    put("=");
    put_ptr((uint32_t)(uintptr_t)*out);
    return (int32_t)n;
}

static int32_t STDCALL h_free(void **pp)
{
    put(" free(");
    put_ptr((uint32_t)(uintptr_t)pp);
    put(":");
    put_ptr((uint32_t)(uintptr_t)*pp);
    put(")");
    if (*pp != NULL)
        arena_release(*pp);
    *pp = NULL;
    return 0;
}

static uint32_t STDCALL h_window_instance(uint32_t *wnd, uint32_t *inst)
{
    put(" wndinst(");
    put_ptr((uint32_t)(uintptr_t)wnd);
    put(",");
    put_ptr((uint32_t)(uintptr_t)inst);
    put(")");
    *wnd = MAIN_HWND;
    *inst = 0x400000;
    return 0x400000;
}

static int32_t STDCALL h_module_enter(uint32_t linkage, uint32_t *out)
{
    put(" enter(");
    put_ptr(linkage);
    put(",");
    put_ptr((uint32_t)(uintptr_t)out);
    put(")");
    if (linkage != (uint32_t)(uintptr_t)cur->link) {
        *out = 0;
        return -1;
    }
    *out = 0x00e40000;    /* an HMODULE in neither image */
    return 0;
}

static int32_t STDCALL h_module_leave(uint32_t x)
{
    put(" leave(%#x)", x);
    return 0;
}

/* +0x110: the host's dialog runner, as far as a modeless dialog with flag
 * 8 goes (docs/modules/host.md): the entry found in the module's own table,
 * create(main window, param), then ShowWindow(SW_SHOW) and UpdateWindow;
 * the message pump finds nothing queued. */
static int32_t STDCALL h_dialog(uint32_t id, uint32_t param)
{
    const uint32_t *e = *(const uint32_t **)(cur->link + 0x14);
    uint32_t hwnd;

    put(" dialog(%#x,%#x)", id, param);
    if (!in_image((uint32_t)(uintptr_t)e)) {
        put(" table outside the image");
        return -1;
    }
    put(" table image+%#x", (uint32_t)((const uint8_t *)e - cur->image));
    for (; e[0] != 0; e += 3)
        if (e[0] == id)
            break;
    if (e[0] == 0)
        return -1;
    put(" flags %#x create ", e[2]);
    put_ptr(e[1]);
    hwnd = ((create_fn)(uintptr_t)e[1])(MAIN_HWND, param);
    put(" ->");
    put_ptr(hwnd);
    if (hwnd == 0)
        return -1;
    put(" host:");
    u_ShowWindow((HWND)(uintptr_t)hwnd, SW_SHOW);
    u_UpdateWindow((HWND)(uintptr_t)hwnd);
    return 0;
}

static int32_t STDCALL h_menu_add(uint32_t command, uint32_t id, uint32_t text, uint32_t z)
{
    put(" menuadd(%#x,%#x,", command, id);
    put_ptr(text);
    put(",%#x)", z);
    return 0;
}

static int32_t STDCALL h_menu_remove(uint32_t id)
{
    put(" menuremove(%#x)", id);
    return 0;
}

static int32_t STDCALL h_menu_state(uint32_t id, uint32_t flags)
{
    put(" menustate(%#x,%#x)", id, flags);
    return 0;
}

static int32_t STDCALL h_post(uint32_t a, uint32_t b)
{
    put(" post(%#x,%#x)", a, b);
    return 0;
}

static char *STDCALL h_strcpy(char *d, const char *s)
{
    put(" strcpy(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_s(s);
    put(")");
    if (s == NULL)
        stub_fault("strcpy null");
    return strcpy(d, s);
}

static char *STDCALL h_strcat(char *d, const char *s)
{
    put(" strcat(");
    put_ptr((uint32_t)(uintptr_t)d);
    put(",");
    put_s(s);
    put(")");
    if (s == NULL)
        stub_fault("strcat null");
    return strcat(d, s);
}

static uint32_t STDCALL h_strlen(const char *s)
{
    uint32_t n = s ? (uint32_t)strlen(s) : 0;
    put(" strlen=%u", n);
    return n;
}

/* ---- the other modules -------------------------------------------------------------- */

static uint32_t STDCALL chain_add(uint32_t list, callback_fn cb)
{
    put(" chainadd(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    /* now and then the list cannot grow, as CHAIN's allocation can fail */
    if (rnd_n(60) == 0) {
        put("=-1");
        return 0xffffffffu;
    }
    if (cur->nregs >= 32)
        stub_fault("chain full");
    cur->regs[cur->nregs].list = list;
    cur->regs[cur->nregs].fn = cb;
    cur->nregs++;
    return 0;
}

static uint32_t STDCALL chain_remove(uint32_t list, callback_fn cb)
{
    uint32_t i;

    put(" chainremove(%#x,", list);
    put_ptr((uint32_t)(uintptr_t)cb);
    put(")");
    for (i = 0; i < cur->nregs; i++)
        if (cur->regs[i].list == list && cur->regs[i].fn == cb) {
            memmove(&cur->regs[i], &cur->regs[i + 1], (cur->nregs - i - 1) * sizeof cur->regs[0]);
            cur->nregs--;
            return 0;
        }
    put("=-1");
    return 0xffffffffu;
}

/* MULTIP's player lists: {count, records}, records 0x100 bytes, the name
 * pointer at +0 and the DirectPlay id at +4.  Built per scenario in
 * `fixtures`, which both sides see at the same address. */
#define LISTS 3
static uint32_t *lists[LISTS];
static uint8_t *records[LISTS];
static uint32_t local_record;
static uint32_t fix_used;

static void *fix_get(uint32_t n)
{
    void *p;
    n = (n + 15) & ~15u;
    if (fix_used + n > sizeof fixtures) {
        printf("fixtures exhausted (scenario %u)\n", scenario);
        exit(2);
    }
    p = fixtures + fix_used;
    fix_used += n;
    return p;
}

static char *make_name(void)
{
    uint32_t k = rnd_n(16), n, i;
    char *s;

    n = k == 0 ? 0 : k == 1 ? 0x80 + rnd_n(0x280) : 1 + rnd_n(14);
    s = (char *)fix_get(n + 1);
    for (i = 0; i < n; i++)
        s[i] = rnd_n(30) == 0 ? (char)(0x80 + rnd_n(0x80)) : rnd_n(8) == 0 ? ' '
               : (char)('A' + rnd_n(26) + (rnd_n(2) ? 0x20 : 0));
    s[n] = 0;
    return s;
}

static void make_list(uint32_t which)
{
    uint32_t n = rnd_n(6) == 0 ? 0 : 1 + rnd_n(7), i;

    lists[which] = (uint32_t *)fix_get(8);
    records[which] = (uint8_t *)fix_get(0x100 * (n ? n : 1));
    memset(records[which], 0, 0x100 * (n ? n : 1));
    for (i = 0; i < n; i++) {
        *(char **)(records[which] + 0x100 * i) = make_name();
        *(uint32_t *)(records[which] + 0x100 * i + 4) = 0x1000 + rnd_n(16);
    }
    lists[which][0] = n;
    lists[which][1] = (uint32_t)(uintptr_t)records[which];
}

static uint32_t STDCALL mp_send(uint32_t to, uint32_t payload, uint32_t size, uint32_t flags)
{
    put(" mpsend(%#x,", to);
    put_ptr(payload);
    put(",%#x,%#x)", size, flags);
    put_bytes((const void *)(uintptr_t)payload, size);
    if (counting) cover_sent++;
    return 0;
}

static uint32_t STDCALL mp_list(void)
{
    put(" mplist=");
    put_ptr((uint32_t)(uintptr_t)lists[0]);
    return (uint32_t)(uintptr_t)lists[0];
}

static uint32_t STDCALL mp_player(uint32_t id)
{
    uint32_t i, l;

    put(" mpplayer(%#x)=", id);
    for (l = 0; l < LISTS; l++)
        for (i = 0; i < lists[l][0]; i++)
            if (*(uint32_t *)(records[l] + 0x100 * i + 4) == id) {
                put_ptr((uint32_t)(uintptr_t)(records[l] + 0x100 * i));
                return (uint32_t)(uintptr_t)(records[l] + 0x100 * i);
            }
    if (counting) cover_unknown++;
    put("null");
    return 0;
}

static uint32_t STDCALL mp_local(void)
{
    put(" mplocal=");
    put_ptr(local_record);
    return local_record;
}

static uint32_t STDCALL wm_show(const char *text, uint32_t slot, uint32_t time, uint32_t append)
{
    put(" winmsg(");
    put_s(text);
    put(",%u,%#x,%u)", slot, time, append);
    if (counting) cover_bar++;
    return 0x5150 + append;
}

static void build_stubs(void)
{
    uint32_t i;

#define TRAPS(table, id) \
    for (i = 4; i < sizeof table; i += 4) *(uint32_t *)(table + i) = 0xde000000u | ((id) << 16) | i
    TRAPS(host, 1); TRAPS(chain, 4); TRAPS(multip, 35); TRAPS(winmsg, 22);
#undef TRAPS
    *(uint32_t *)host = 1;
    SET_SLOT(host, 0x03c, h_post);
    SET_SLOT(host, 0x054, h_window_instance);
    SET_SLOT(host, 0x110, h_dialog);
    SET_SLOT(host, 0x214, h_free);
    SET_SLOT(host, 0x244, h_menu_add);
    SET_SLOT(host, 0x254, h_menu_state);
    SET_SLOT(host, 0x25c, h_menu_remove);
    SET_SLOT(host, 0x298, h_module_enter);
    SET_SLOT(host, 0x29c, h_module_leave);
    SET_SLOT(host, 0x2a4, h_string);
    SET_SLOT(host, 0x2a8, h_strcpy);
    SET_SLOT(host, 0x2b0, h_strcat);
    SET_SLOT(host, 0x2b8, h_strlen);
    *(uint32_t *)chain = 4;
    SET_SLOT(chain, 0x18, chain_add);
    SET_SLOT(chain, 0x1c, chain_remove);
    *(uint32_t *)multip = 35;
    SET_SLOT(multip, 0x20, mp_send);
    SET_SLOT(multip, 0x24, mp_list);
    SET_SLOT(multip, 0x28, mp_player);
    SET_SLOT(multip, 0x2c, mp_local);
    *(uint32_t *)winmsg = 22;
    SET_SLOT(winmsg, 0x18, wm_show);
}

/* ---- loading ----------------------------------------------------------------------- */

static const struct { const char *dll, *fn; void *repl; } hooks[] = {
    { "USER32.dll", "CreateWindowExA", (void *)u_CreateWindowExA },
    { "USER32.dll", "ShowWindow", (void *)u_ShowWindow },
    { "USER32.dll", "UpdateWindow", (void *)u_UpdateWindow },
    { "USER32.dll", "DestroyWindow", (void *)u_DestroyWindow },
    { "USER32.dll", "GetWindowLongA", (void *)u_GetWindowLongA },
    { "USER32.dll", "SetFocus", (void *)u_SetFocus },
    { "USER32.dll", "GetClientRect", (void *)u_GetClientRect },
    { "USER32.dll", "SetWindowPos", (void *)u_SetWindowPos },
    { "USER32.dll", "GetWindowTextA", (void *)u_GetWindowTextA },
    { "USER32.dll", "SetWindowTextA", (void *)u_SetWindowTextA },
    { "USER32.dll", "DefDlgProcA", (void *)u_DefDlgProcA },
    { "USER32.dll", "GetScrollInfo", (void *)u_GetScrollInfo },
    { "USER32.dll", "SendMessageA", (void *)u_SendMessageA },
    { "USER32.dll", "RegisterClassA", (void *)u_RegisterClassA },
    { "USER32.dll", "UnregisterClassA", (void *)u_UnregisterClassA },
    { "USER32.dll", "LoadCursorA", (void *)u_LoadCursorA },
    { "GDI32.dll", "GetStockObject", (void *)g_GetStockObject },
    { "KERNEL32.dll", "LoadLibraryA", (void *)k_LoadLibraryA },
    { "KERNEL32.dll", "FreeLibrary", (void *)k_FreeLibrary },
    { "KERNEL32.dll", "DisableThreadLibraryCalls", (void *)k_DisableThreadLibraryCalls },
};

/* Every import of the module replaced by its model; one with no model
 * stops the harness. */
static void hook_imports(struct side *s)
{
    uint8_t *m = (uint8_t *)s->module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(m + ((IMAGE_DOS_HEADER *)m)->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(m + dir->VirtualAddress);
    uint32_t n = 0, i;

    for (; d->Name; d++) {
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(m +
            (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA *iat = (IMAGE_THUNK_DATA *)(m + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            const char *fn;
            DWORD old;

            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) {
                printf("%s: import by ordinal from %s\n", s->name, (char *)m + d->Name);
                exit(2);
            }
            fn = (const char *)((IMAGE_IMPORT_BY_NAME *)(m + names->u1.AddressOfData))->Name;
            for (i = 0; i < sizeof hooks / sizeof hooks[0]; i++)
                if (!_stricmp(hooks[i].dll, (char *)m + d->Name) && !strcmp(hooks[i].fn, fn))
                    break;
            if (i == sizeof hooks / sizeof hooks[0]) {
                printf("%s: no model for %s!%s\n", s->name, (char *)m + d->Name, fn);
                exit(2);
            }
            VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old);
            iat->u1.Function = (DWORD)(uintptr_t)hooks[i].repl;
            VirtualProtect(&iat->u1.Function, 4, old, &old);
            n++;
        }
    }
    if (n != sizeof hooks / sizeof hooks[0]) {
        printf("%s: %u imports hooked, expected %u\n", s->name, n,
               (unsigned)(sizeof hooks / sizeof hooks[0]));
        exit(2);
    }
}

static void load_side(struct side *s, const char *path, const char *name)
{
    struct import *im;
    IMAGE_NT_HEADERS *nt;

    memset(s, 0, sizeof *s);
    s->name = name;
    s->transcript = malloc(TRANSCRIPT_SIZE);
    s->module = LoadLibraryA(path);
    if (s->module == NULL) { printf("cannot load %s (%lu)\n", path, GetLastError()); exit(2); }
    s->imports = (struct import *)GetProcAddress(s->module, "ImportTable");
    s->link = (uint8_t *)GetProcAddress(s->module, "Linkage");
    if (s->imports == NULL || s->link == NULL || *(uint32_t *)s->link != 0xffffffffu) {
        printf("%s: not MPCHAT\n", path);
        exit(2);
    }
    s->image = (uint8_t *)s->imports;
    if (s->link != s->image + 0x48) {
        printf("%s: Linkage is not ImportTable+0x48\n", path);
        exit(2);
    }
    nt = (IMAGE_NT_HEADERS *)((uint8_t *)s->module + ((IMAGE_DOS_HEADER *)s->module)->e_lfanew);
    s->lo = (uint8_t *)s->module;
    s->hi = s->lo + nt->OptionalHeader.SizeOfImage;
    for (im = s->imports; im->id; im++) {
        switch (im->id) {
        case 1:  im->link = host; break;
        case 2:  im->link = global; break;
        case 4:  im->link = chain; break;
        case 22: im->link = winmsg; break;
        case 35: im->link = multip; break;
        default: printf("%s: unexpected import %u\n", name, im->id); exit(2);
        }
    }
    hook_imports(s);
    memcpy(s->pristine, s->image, IMAGE_SIZE);
}

/* ---- driving ---------------------------------------------------------------------- */

typedef uint32_t (STDCALL *s0)(void);
#define SVC(off) (*(void **)(cur->link + (off)))
#define IMG32(o) (*(uint32_t *)(cur->image + (o)))

static uint32_t out_word;
static uint8_t payload[0x1400];
static uint32_t args[3];

static void dispatch(uint32_t list, uint32_t a, uint32_t b)
{
    uint32_t i, n = 0;

    for (i = 0; i < cur->nregs && !faulted; i++) {
        if (cur->regs[i].list != list)
            continue;
        n++;
        put(" ->");
        put_ptr((uint32_t)(uintptr_t)cur->regs[i].fn);
        cur->regs[i].fn(a, b);
    }
    if (n == 0)
        put(" none");
}

static void make_text(char *s, uint32_t max)
{
    uint32_t k = rnd_n(14), n, i;

    n = k == 0 ? 0 : k == 1 ? 0x3fe + rnd_n(3) - 1 : k == 2 ? 0x400 + rnd_n(0x300)
        : 1 + rnd_n(k < 6 ? 8 : 120);
    if (n >= max)
        n = max - 1;
    for (i = 0; i < n; i++)
        s[i] = rnd_n(40) == 0 ? (char)(0x80 + rnd_n(0x80)) : rnd_n(6) == 0 ? ' '
               : rnd_n(30) == 0 ? '\n' : (char)('a' + rnd_n(26));
    s[n] = 0;
}

static void op_session(void)
{
    uint32_t k = rnd_n(100), m, a = 0;

    if (k < 12) {
        m = 1;
        if (counting) cover_sessions++;
    } else if (k < 18) {
        m = 2;
    } else if (k < 30) {
        m = 3;
        if (rnd_n(3) == 0) make_list(0);
        a = rnd_n(8) == 0 ? 0 : (uint32_t)(uintptr_t)lists[0];
    } else if (k < 38) {
        m = 4;
        if (rnd_n(3) == 0) make_list(1);
        a = rnd_n(5) == 0 ? 0 : (uint32_t)(uintptr_t)lists[1];
    } else if (k < 95) {
        uint32_t l = rnd_n(LISTS);
        m = 5;
        *(uint32_t *)payload = rnd_n(8) == 0 ? rnd_n(3) == 0 ? rnd() : 0x1000 + rnd_n(0x40) : 0x1020;
        *(uint32_t *)(payload + 4) = rnd();
        make_text((char *)payload + 8, sizeof payload - 8);
        args[0] = rnd_n(5) == 0 || lists[l][0] == 0 ? 0x2000 + rnd_n(4)
                  : *(uint32_t *)(records[l] + 0x100 * rnd_n(lists[l][0]) + 4);
        args[1] = (uint32_t)(uintptr_t)payload;
        args[2] = 8 + (uint32_t)strlen((char *)payload + 8) + 1;
        a = (uint32_t)(uintptr_t)args;
        if (counting) {
            cover_received++;
            if (win_of(IMG32(0xdc)) == NULL) cover_stale++;
        }
    } else {
        m = rnd_n(2) ? 0 : 6 + rnd_n(0x40);
    }
    put("\n#session(%u,", m);
    put_ptr(a);
    if (m == 5)
        put(" %#x %#x", args[0], *(uint32_t *)payload);
    put(")");
    dispatch(0x1d, m, a);
}

static void op_window_message(void)
{
    uint32_t h = IMG32(0xd8), k = rnd_n(20);
    struct win *w = win_of(h);

    put("\n#message");
    if (w == NULL) {
        put(" no window");
        return;
    }
    if (k < 5) {
        pick_client(w, 1);
        put(" resize %d %d", w->cw, w->ch);
        deliver_size(h);
    } else if (k < 7) {
        deliver(h, WM_SETFOCUS, 0, 0);
    } else if (k < 8) {
        deliver(h, WM_CLOSE, 0, 0);
    } else if (k < 11) {
        /* the transcript takes the focus: EN_SETFOCUS from id 0x100 */
        uint32_t wp = rnd_n(4) ? 0x01000100u : (rnd_n(0x200) << 16) | 0x100;
        deliver(h, WM_COMMAND, wp, (LPARAM)(rnd_n(4) ? IMG32(0xdc) : IMG32(0xe0)));
    } else if (k < 15) {
        /* Send: clicked, or another notification from it */
        uint32_t wp = rnd_n(5) ? 0x103 : (1 + rnd_n(6)) << 16 | 0x103;
        deliver(h, WM_COMMAND, wp, (LPARAM)IMG32(0xec));
    } else if (k < 17) {
        deliver(h, WM_COMMAND, (rnd_n(2) ? 0x101 : 0x102 + rnd_n(0x10)) | (rnd_n(3) << 16), 0);
    } else {
        static const UINT others[] = { 0, 2, 3, 4, 6, 8, 0xf, 0x14, 0x20, 0x84, 0x100, 0x200, 0x401 };
        deliver(h, others[rnd_n(sizeof others / sizeof others[0])], rnd(), 0);
    }
}

static void op(void)
{
    uint32_t k = rnd_n(100), i;
    struct win *w;

    if (counting) cover_ops[k / 5]++;
    if (k < 40) {
        op_session();
    } else if (k < 46) {                                   /* the menu item */
        uint32_t c = rnd_n(5) ? 0x1183 : 0x1180 + rnd_n(8);
        if (counting && c == 0x1183) cover_toggle[0]++;
        put("\n#command(%#x)", c);
        dispatch(0xb, c, rnd());
    } else if (k < 52) {                                   /* the keys */
        uint32_t c = rnd_n(6) == 0 ? 0x10172 - rnd_n(4) : 0x10170 + rnd_n(2);
        if (counting && (c == 0x10170 || c == 0x10171)) cover_toggle[1]++;
        out_word = 0x5a5a5a5a;
        put("\n#key(%#x)", c);
        dispatch(0xc, c, (uint32_t)(uintptr_t)&out_word);
        put(" out=%#x", out_word);
    } else if (k < 56) {
        uint32_t m = rnd_n(4) ? 0x40 : rnd_n(0x60);
        put("\n#situation(%#x)", m);
        dispatch(0x14, m, 0);
    } else if (k < 70) {
        op_window_message();
    } else if (k < 82) {                                   /* typed, and sent */
        w = win_of(IMG32(0xe0));
        put("\n#typed");
        if (w != NULL) {
            make_text(w->text, sizeof w->text);
            if (counting) {
                if (w->text[0] == 0) cover_empty_line++;
                if (strlen(w->text) >= 0x3fe) cover_long_line++;
            }
            put(" ");
            put_s(w->text);
        }
        if (win_of(IMG32(0xd8)) != NULL && rnd_n(4))
            deliver(IMG32(0xd8), WM_COMMAND, 0x103, (LPARAM)IMG32(0xec));
    } else if (k < 86) {                                   /* the user selects in the transcript */
        w = win_of(IMG32(0xdc));
        put("\n#select");
        if (w != NULL) {
            uint32_t j = rnd_n(5);
            if (j == 0) {
                w->sel_min = w->sel_max = (int32_t)w->rich_len;
            } else if (j == 1) {
                w->sel_min = w->sel_max = (int32_t)rnd_n(w->rich_len + 1);
            } else if (j == 2) {
                w->sel_min = 0;
                w->sel_max = -1;
            } else {
                w->sel_min = (int32_t)rnd_n(w->rich_len + 1);
                w->sel_max = (int32_t)rnd_n(w->rich_len + 1);
            }
            if (counting && (w->sel_min != w->sel_max || (uint32_t)w->sel_min != w->rich_len))
                cover_restore++;
            put(" %d,%d", w->sel_min, w->sel_max);
        }
    } else if (k < 89) {                                   /* hidden or shown from outside */
        w = win_of(IMG32(0xd8));
        put("\n#outside");
        if (w != NULL) {
            w->style ^= WS_VISIBLE;
            w->active = 0;
            put(" %u", (w->style & WS_VISIBLE) != 0);
        }
    } else if (k < 95) {                                   /* MULTIP's records change */
        uint32_t j = rnd_n(4), l = rnd_n(LISTS);
        put("\n#players");
        if (j == 0) {
            local_record = rnd_n(4) == 0 || lists[l][0] == 0 ? 0
                           : (uint32_t)(uintptr_t)(records[l] + 0x100 * rnd_n(lists[l][0]));
            put(" local ");
            put_ptr(local_record);
        } else if (j == 1 && lists[l][0] != 0) {
            i = rnd_n(lists[l][0]);
            *(char **)(records[l] + 0x100 * i) = make_name();
            put(" renamed %u.%u", l, i);
        } else if (j == 2 && lists[l][0] != 0) {
            lists[l][0] = rnd_n(lists[l][0] + 1);
            put(" count %u.%u", l, lists[l][0]);
        } else {
            make_list(l);
            put(" new %u", l);
        }
    } else if (k < 97) {                                   /* the strings' next answer */
        i = rnd_n(3);
        string_text[i] = rnd_n(3) == 0 ? NULL : string_buf[i];
        put("\n#strings %u %s", i, string_text[i] ? "some" : "none");
    } else {                                               /* deinit and init again */
        uint32_t r;
        put("\n#deinit:");
        r = ((s0)SVC(8))();
        put(" =%#x", r);
        put("\n#init:");
        r = ((s0)SVC(4))();
        put(" =%#x", r);
    }
}

static void put_state(void)
{
    uint8_t img[IMAGE_SIZE];
    uint32_t i, h = 0x811c9dc5u;

    memcpy(img, cur->image, IMAGE_SIZE);
    *(uint32_t *)(img + 0x34) = pair_code(*(uint32_t *)(img + 0x34));
    *(uint32_t *)(img + 0x4c) = pair_code(*(uint32_t *)(img + 0x4c));
    *(uint32_t *)(img + 0x50) = pair_code(*(uint32_t *)(img + 0x50));
    if (in_image(*(uint32_t *)(img + 0x5c)))
        *(uint32_t *)(img + 0x5c) -= (uint32_t)(uintptr_t)cur->image;
    for (i = 0; i < nwins; i++) {
        struct win *w = &wins[i];
        h = digest(&w->gone, sizeof w->gone, h);
        h = digest(&w->style, 4, h);
        h = digest(&w->x, 16, h);
        h = digest(w->text, (uint32_t)strlen(w->text), h);
        h = digest(&w->rows, 8, h);
        h = digest(w->rich, w->rich_len, h);
        h = digest(&w->sel_min, 8, h);
    }
    put("\n  state img=%08x heap=%#x/%08x windows=%u/%08x focus ",
        digest(img, IMAGE_SIZE, 0x811c9dc5u), arena_used, digest(arena, arena_used, 0x811c9dc5u),
        nwins, h);
    put_ptr(focus);
    put(" chain");
    for (i = 0; i < cur->nregs; i++) {
        put(" %#x:", cur->regs[i].list);
        put_ptr((uint32_t)(uintptr_t)cur->regs[i].fn);
    }
    if (getenv("MPCHAT_IMAGE")) {
        put("\n  whole");
        for (i = 0; i + 4 <= IMAGE_SIZE; i += 4)
            if (*(uint32_t *)(img + i) != 0)
                put(" %x:%08x", i, *(uint32_t *)(img + i));
    }
}

static void run_side(struct side *s)
{
    uint32_t i, nops;

    cur = s;
    s->length = 0;
    s->nregs = 0;
    memcpy(s->image, s->pristine, IMAGE_SIZE);
    arena_used = 0;
    memset(arena, 0, 0x10000);
    nwins = 0;
    focus = 0;
    class_registered = 0;
    class_proc = NULL;
    fix_used = 0;
    rng = scenario_seed;
    size_at_show = (int)rnd_n(2);
    for (i = 0; i < 3; i++) {
        uint32_t n = rnd_n(8) == 0 ? 0x60 + rnd_n(0x1f) : 1 + rnd_n(12), j;
        for (j = 0; j < n; j++)
            string_buf[i][j] = (char)('A' + rnd_n(26));
        string_buf[i][n] = 0;
        string_text[i] = rnd_n(16) == 0 ? NULL : string_buf[i];
        if (counting && string_text[i] == NULL) cover_no_strings++;
    }
    for (i = 0; i < LISTS; i++)
        make_list(i);
    local_record = rnd_n(5) == 0 || lists[0][0] == 0 ? 0 : (uint32_t)(uintptr_t)records[0];
    put("scenario %u size-at-show %d\n", scenario, size_at_show);
    faulted = 0;
    in_mpchat = 1;
    if (__builtin_setjmp(fault_jb) == 0) {
        uint32_t r = ((s0)SVC(4))();
        put(" =%#x", r);
    }
    in_mpchat = 0;
    put_state();
    nops = 4 + rnd_n(MAX_OPS);
    for (i = 0; i < nops && !faulted; i++) {
        op_number = i;
        /* most scenarios begin a session at once: the rest happens in one */
        in_mpchat = 1;
        if (__builtin_setjmp(fault_jb) == 0) {
            if (i == 0 && rnd_n(5)) {
                put("\n#session(1)");
                dispatch(0x1d, 1, 0);
            } else {
                op();
            }
        }
        in_mpchat = 0;
        put_state();
    }
    if (!faulted) {
        put("\n#deinit:");
        in_mpchat = 1;
        if (__builtin_setjmp(fault_jb) == 0) {
            uint32_t r = ((s0)SVC(8))();
            put(" =%#x", r);
        }
        in_mpchat = 0;
        put_state();
    }
    put("\n");
    cur = NULL;
}

static void show_difference(void)
{
    const char *a = retail.transcript, *b = ours.transcript;
    uint32_t n = retail.length < ours.length ? retail.length : ours.length, i, from;
    const char *ls;

    for (i = 0; i < n && a[i] == b[i]; i++)
        ;
    ls = a + i;
    while (ls > a && ls[-1] != '\n')
        ls--;
    printf("scenario %u differs at byte %u\n", scenario, i);
    from = (uint32_t)(ls - a) > 1500 ? (uint32_t)(ls - a) - 1500 : 0;
    printf("--- before:\n%.*s\n", (int)((uint32_t)(ls - a) - from), a + from);
    from = i - (uint32_t)(ls - a) > 400 ? i - 400 : (uint32_t)(ls - a);
    printf("--- retail:\n...%.900s\n", a + from);
    printf("--- ours:\n...%.900s\n", b + from);
}

static LONG WINAPI on_fault(EXCEPTION_POINTERS *e)
{
    uint8_t *at = (uint8_t *)e->ExceptionRecord->ExceptionAddress;
    struct side *s[2] = { &retail, &ours };
    int i;

    for (i = 0; i < 2; i++)
        if (s[i]->module && at >= s[i]->lo && at < s[i]->hi)
            printf("\nFAULT in %s MPCHAT at .text+%#x\n", s[i]->name,
                   (uint32_t)(at - s[i]->lo - 0x1000));
    printf("FAULT %#lx at %p, scenario %u op %u, running %s\n",
           e->ExceptionRecord->ExceptionCode, (void *)at, scenario, op_number,
           cur ? cur->name : "none");
    if (cur != NULL && cur->length > 0) {
        uint32_t from = cur->length > 2500 ? cur->length - 2500 : 0;
        printf("--- transcript tail:\n%.*s\n", (int)(cur->length - from), cur->transcript + from);
    }
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 20000;
    uint32_t first = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0;
    uint32_t i, differ = 0;

    AddVectoredExceptionHandler(1, on_vectored);
    if (getenv("MPCHAT_NOFILTER") == NULL)
        SetUnhandledExceptionFilter(on_fault);
    build_stubs();
    load_side(&retail, "MPCHAT_retail.dll", "retail");
    load_side(&ours, "MPCHAT_ours.dll", "ours");
    for (i = first; i < first + n; i++) {
        scenario = i;
        rng = 0x9e3779b9u ^ (i * 0x85ebca6bu);
        scenario_seed = rnd() | 1;
        counting = 1;
        run_side(&retail);
        counting = 0;
        run_side(&ours);
        if (getenv("MPCHAT_DUMP") != NULL) {
            FILE *f = fopen("mpchat_retail.txt", "wb");
            fwrite(retail.transcript, 1, retail.length, f);
            fclose(f);
            f = fopen("mpchat_ours.txt", "wb");
            fwrite(ours.transcript, 1, ours.length, f);
            fclose(f);
        }
        if (retail.length != ours.length ||
            memcmp(retail.transcript, ours.transcript, retail.length) != 0) {
            if (differ++ < 3)
                show_difference();
        }
        if ((i + 1 - first) % 1000 == 0) {
            printf("%u scenarios, %u differ\n", i + 1 - first, differ);
            fflush(stdout);
        }
    }
    printf("mpchatdiff: %u scenarios, %u differ\n", n, differ);
    printf("  ops:");
    for (i = 0; i < 20; i++)
        printf(" %lu", cover_ops[i]);
    printf("\n  sessions begun %lu; lines sent %lu (empty %lu, at or past 0x3fe %lu), received "
           "%lu (unknown sender %lu, with the transcript gone %lu), on WINMSG's bar %lu\n",
           cover_sessions, cover_sent, cover_empty_line, cover_long_line, cover_received,
           cover_unknown, cover_stale, cover_bar);
    printf("  transcript scrolled %lu, selection restored %lu; list rows %lu; menu toggles %lu, "
           "key toggles %lu\n", cover_scroll, cover_restore, cover_list_rows, cover_toggle[0],
           cover_toggle[1]);
    printf("  wild client rectangles %lu, sized at the first show %lu, strings missing %lu, "
           "faults (alike, the scenario ends) %lu:\n  a null name into strcat (string 0x9402 "
           "missing) %lu, access violations %lu\n", cover_resize_wild, cover_size_at_show,
           cover_no_strings, cover_faults, cover_null_name, cover_av);
    return differ ? 1 : 0;
}
