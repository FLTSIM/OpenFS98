/* SEHFIX.DLL -- give the host's divide repair back its exceptions under wine.
 *
 * The sim runs its main loop inside a __try (0x412140) whose filter, 0x4119a0,
 * repairs integer divide faults: it decodes the faulting idiv/div, writes a
 * saturated quotient into the context and resumes after the instruction.
 * Retail code divides into overflow and relies on it -- WINDOW's eye, and,
 * under wine, two that show up in every flight:
 *
 *   0x406525  host WM_SIZE, idiv [record+0x60] while a view is still 0x0
 *   0x207376e2  HG2D .text+0x66e2, a gradient over a zero-length edge,
 *               idiv ebp (and its two siblings at 0x207376fc, 0x20737716)
 *
 * On x86 Windows an exception raised in a window procedure unwinds out
 * through DispatchMessage and reaches that __try.  Wine calls window
 * procedures from KiUserCallbackDispatcher, which wraps the call in a
 * catch-all ("err:seh:KiUserCallbackDispatcher ignoring exception"), so the
 * host's filter never runs: the rest of the window procedure is abandoned,
 * along with whatever frame or layout it was in the middle of.
 *
 * This module puts that back.  A vectored handler runs before any frame-based
 * one, so on an integer divide fault it walks the SEH chain as x86 Windows
 * would see it -- skipping frames whose handlers live outside the game's
 * directory, which is wine's own -- and if the first frame that belongs to
 * the game is the host's main-loop __try, it calls the host's own filter and
 * returns its verdict.  Anything else is left to wine exactly as before.  No
 * saturation logic of our own: the result is what retail computes on Windows.
 *
 * It takes module id 8, which the retail set leaves unused, and imports
 * nothing.  C:\openfs98-sehfix.log gets the first MAX_LOGGED divide faults
 * with the chain each one walked, so the model above can be checked against
 * the running sim, and at exit a count per faulting address -- a flight
 * takes tens of thousands of them in HG2D alone.  AddVectoredExceptionHandler is looked up at run time, so on a
 * system without it the module loads and does nothing.
 */

#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"

#define SEHFIX_LOG "C:\\openfs98-sehfix.log"

/* The host's main-loop __try frame, as 0x412140 builds it for
 * _except_handler3: { prev, handler, scope table, try level }. */
#define HOST_FILTER       0x4119a0u
#define HOST_EH_HANDLER   0x414cd6u
#define HOST_EH_SCOPES    0x416110u

#define MAX_FRAMES  64
#define MAX_LOGGED  32
#define MAX_TALLIED 32

struct seh_frame {
    struct seh_frame *prev;
    uint32_t          handler;
    uint32_t          scopes;
    int32_t           try_level;
};

typedef LONG (WINAPI *host_filter_fn)(EXCEPTION_POINTERS *);
typedef PVOID (WINAPI *add_veh_fn)(ULONG, PVECTORED_EXCEPTION_HANDLER);

struct fs98_import ImportTable[] = {
    { 0, 0 }
};

static char     game_dir[MAX_PATH];
static unsigned game_dir_len;
static unsigned logged;

/* Faults per address and verdict, written out at exit. */
static struct {
    uint32_t eip;
    uint32_t repaired;
    uint32_t declined;    /* the host filter said no */
    uint32_t left;        /* not under the host's frame: left to wine */
} tally[MAX_TALLIED];
static unsigned tallied;
static uint32_t untallied;
static HANDLE   log_handle = INVALID_HANDLE_VALUE;

/* ---- a log, written without any C runtime --------------------------------- */

static void emit(const char *s)
{
    DWORD wrote;
    unsigned n = 0;
    if (log_handle == INVALID_HANDLE_VALUE)
        return;
    while (s[n])
        n++;
    WriteFile(log_handle, s, n, &wrote, 0);
}

static void emit_u(uint32_t v)
{
    char buf[11];
    int i = 10;
    buf[10] = 0;
    if (!v) { emit("0"); return; }
    while (v && i > 0) { buf[--i] = (char)('0' + v % 10); v /= 10; }
    emit(buf + i);
}

static void emit_x(uint32_t v)
{
    char buf[11];
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; i++)
        buf[2 + i] = "0123456789abcdef"[(v >> (28 - 4 * i)) & 0xf];
    buf[10] = 0;
    emit(buf);
}

/* ---- who owns an address -------------------------------------------------- */

static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* The module's file name into `path` (empty when no module owns `addr`), and
 * whether it lies under the game's directory. */
static int owner_of(uint32_t addr, char *path, unsigned size)
{
    HMODULE m = 0;
    unsigned i;
    path[0] = 0;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)addr, &m))
        return 0;
    if (!GetModuleFileNameA(m, path, size))
        return 0;
    if (!game_dir_len)
        return 0;
    for (i = 0; i < game_dir_len; i++)
        if (lower(path[i]) != lower(game_dir[i]))
            return 0;
    return 1;
}

static const char *base_name(const char *path)
{
    const char *b = path;
    for (; *path; path++)
        if (*path == '\\' || *path == '/')
            b = path + 1;
    return *b ? b : "?";
}

/* ---- the handler ---------------------------------------------------------- */

static void count(uint32_t eip, int found, LONG verdict)
{
    unsigned i;
    for (i = 0; i < tallied && tally[i].eip != eip; i++)
        ;
    if (i == tallied) {
        if (tallied == MAX_TALLIED) {
            untallied++;
            return;
        }
        tally[tallied++].eip = eip;
    }
    if (!found)
        tally[i].left++;
    else if (verdict == EXCEPTION_CONTINUE_EXECUTION)
        tally[i].repaired++;
    else
        tally[i].declined++;
}

static void write_tally(void)
{
    char path[MAX_PATH];
    unsigned i;
    emit("\nfaults by address: repaired / declined / left to wine\n");
    for (i = 0; i < tallied; i++) {
        owner_of(tally[i].eip, path, sizeof path);
        emit("  ");
        emit_x(tally[i].eip);
        emit(" ");
        emit(base_name(path));
        emit("  ");
        emit_u(tally[i].repaired);
        emit(" / ");
        emit_u(tally[i].declined);
        emit(" / ");
        emit_u(tally[i].left);
        emit("\n");
    }
    if (untallied) {
        emit("  and ");
        emit_u(untallied);
        emit(" at further addresses\n");
    }
}

static int is_host_frame(const struct seh_frame *f)
{
    return f->handler == HOST_EH_HANDLER && f->scopes == HOST_EH_SCOPES &&
           f->try_level == 0;
}

static LONG WINAPI divide_handler(EXCEPTION_POINTERS *ep)
{
    NT_TIB *tib;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    uint32_t eip = (uint32_t)ep->ContextRecord->Eip;
    struct seh_frame *f;
    char path[MAX_PATH];
    int log = logged < MAX_LOGGED;
    int found = 0, n;
    LONG verdict = EXCEPTION_CONTINUE_SEARCH;

    if (code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_INT_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;
    __asm__("movl %%fs:0x18, %0" : "=r"(tib));

    if (log) {
        logged++;
        owner_of(eip, path, sizeof path);
        emit("fault ");
        emit_x(code);
        emit(" at ");
        emit_x(eip);
        emit(" ");
        emit(base_name(path));
        emit(" thread ");
        emit_x(GetCurrentThreadId());
        emit("\n");
    }

    /* Walk the chain from the innermost frame.  Wine's own frames are
     * skipped -- x86 Windows has no frame between a window procedure and its
     * caller -- and the first frame the game owns decides. */
    f = (struct seh_frame *)tib->ExceptionList;
    for (n = 0; n < MAX_FRAMES && f != (struct seh_frame *)~0u; n++) {
        int ours;
        if ((void *)f < tib->StackLimit || (void *)(f + 1) > tib->StackBase ||
            ((uint32_t)f & 3))
            break;
        ours = owner_of(f->handler, path, sizeof path);
        if (log) {
            emit("  frame ");
            emit_x((uint32_t)f);
            emit(" handler ");
            emit_x(f->handler);
            emit(" ");
            emit(base_name(path));
            emit(ours ? "" : " (skipped)");
            emit("\n");
        }
        if (ours) {
            found = is_host_frame(f);
            break;
        }
        f = f->prev;
    }

    if (found)
        verdict = ((host_filter_fn)HOST_FILTER)(ep);
    count(eip, found, verdict);

    if (log) {
        emit(found ? "  host filter: " : "  left to wine\n");
        if (found) {
            emit(verdict == EXCEPTION_CONTINUE_EXECUTION ? "repaired, eip " :
                                                           "declined, eip ");
            emit_x((uint32_t)ep->ContextRecord->Eip);
            emit(" eax ");
            emit_x((uint32_t)ep->ContextRecord->Eax);
            emit(" edx ");
            emit_x((uint32_t)ep->ContextRecord->Edx);
            emit("\n");
        }
    }
    return verdict;
}

/* ---- module plumbing ------------------------------------------------------ */

static void sehfix_init(void) { }

/* Called from the host's deinit and again at process detach, whichever comes
 * first; the second finds the log closed. */
static void sehfix_deinit(void)
{
    if (log_handle != INVALID_HANDLE_VALUE) {
        write_tally();
        CloseHandle(log_handle);
        log_handle = INVALID_HANDLE_VALUE;
    }
}

struct {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
} Linkage = { 8, sehfix_init, sehfix_deinit };

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        add_veh_fn add_veh;
        unsigned i, slash = 0;

        DisableThreadLibraryCalls(inst);

        /* The game's directory is the host's: <dir>\FLTSIM98.EXE. */
        if (GetModuleFileNameA(0, game_dir, sizeof game_dir)) {
            for (i = 0; game_dir[i]; i++)
                if (game_dir[i] == '\\' || game_dir[i] == '/')
                    slash = i + 1;
            game_dir[slash] = 0;
            game_dir_len = slash;
        }

        log_handle = CreateFileA(SEHFIX_LOG, GENERIC_WRITE, FILE_SHARE_READ, 0,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        add_veh = (add_veh_fn)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),
                                             "AddVectoredExceptionHandler");
        emit("SEHFIX loaded, game directory ");
        emit(game_dir_len ? game_dir : "(unknown)");
        emit(add_veh ? "\n" : "\nAddVectoredExceptionHandler missing: inactive\n");
        if (add_veh)
            add_veh(1, divide_handler);
    } else if (reason == DLL_PROCESS_DETACH) {
        sehfix_deinit();
    }
    return TRUE;
}
