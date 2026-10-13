/* OFSTEST.DLL -- a test module that runs inside the sim.
 *
 * The host cannot be loaded beside an external test program: it has no .reloc,
 * so it only works at 0x400000, and that address is already committed in any
 * wine process.  So services that call the host have been unverifiable.
 *
 * The way round it is the mechanism the sim already provides.  Every DLL in
 * Modules\ is loaded by the host, assigned its module id, and handed pointers
 * to the modules it imports -- including the host itself.  A module whose only
 * job is to run assertions therefore gets exactly the access a test needs, from
 * the real loader, inside the real process.
 *
 * It takes module id 5, which the retail set leaves unused, imports the host,
 * GLOBAL and UTIL, and writes its results to a file because there is nowhere
 * else for output to go.  Everything here is deliberately defensive: this code
 * runs during the sim's startup, and a fault in it means the sim does not
 * start.
 */

#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"

#define OFSTEST_LOG "C:\\openfs98-test.log"

/* ---- the services under test, from the reimplementation ------------------- */
#define FS98_STDCALL __attribute__((stdcall))
const char * FS98_STDCALL fs98_util_longer_string(const char *, const char *);
void                      fs98_util_set_host(void *);
void                      fs98_util_rand_seed(uint32_t);
uint32_t                  fs98_util_rand_get(void);
void                      fs98_util_frand_seed(double);
double                    fs98_util_frand_get(void);
void     FS98_STDCALL     fs98_util_init(void);

/* ---- the host services this module reaches for ---------------------------- */
typedef uint32_t (FS98_STDCALL *host_ticks_fn)(void);
typedef uint32_t (FS98_STDCALL *host_strlen_fn)(const char *);

struct fs98_import ImportTable[] = {
    { FS98_MOD_HOST,   0 },
    { FS98_MOD_GLOBAL, 0 },
    { FS98_MOD_UTIL,   0 },
    { 0, 0 }
};

/* ---- a log, written without any C runtime --------------------------------- */

static HANDLE log_handle = INVALID_HANDLE_VALUE;
static unsigned pass_count, fail_count;

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
    char buf[12];
    int i = 11;
    buf[11] = 0;
    if (!v) { emit("0"); return; }
    while (v && i > 0) { buf[--i] = (char)('0' + (v % 10)); v /= 10; }
    emit(buf + i);
}

static void emit_x(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    int i;
    buf[0] = '0'; buf[1] = 'x';
    for (i = 0; i < 8; i++)
        buf[2 + i] = hex[(v >> (28 - i * 4)) & 0xf];
    buf[10] = 0;
    emit(buf);
}

static void check(int ok, const char *what)
{
    emit(ok ? "  pass  " : "  FAIL  ");
    emit(what);
    emit("\r\n");
    if (ok) pass_count++; else fail_count++;
}

/* ---- the tests ------------------------------------------------------------ */

static void run_tests(void)
{
    void *host   = ImportTable[0].link;
    void *global = ImportTable[1].link;
    void *util   = ImportTable[2].link;

    emit("OpenFS98 in-process test module\r\n");

    emit("host pointer   "); emit_x((uint32_t)(uintptr_t)host);   emit("\r\n");
    emit("GLOBAL pointer "); emit_x((uint32_t)(uintptr_t)global); emit("\r\n");
    emit("UTIL pointer   "); emit_x((uint32_t)(uintptr_t)util);   emit("\r\n");

    check(host   != 0, "the loader bound the host");
    check(global != 0, "the loader bound GLOBAL");
    check(util   != 0, "the loader bound UTIL");
    if (!host || !global || !util)
        return;

    check(*(uint32_t *)global == FS98_MOD_GLOBAL, "GLOBAL reports module id 2");
    check(*(uint32_t *)util   == FS98_MOD_UTIL,   "UTIL reports module id 20");

    /* host +0x2b8: the null-safe strlen UTIL measures strings through */
    {
        host_strlen_fn hstrlen = *(host_strlen_fn *)((unsigned char *)host + 0x2b8);
        check(hstrlen("") == 0,        "host strlen of an empty string is 0");
        check(hstrlen("abcd") == 4,    "host strlen of \"abcd\" is 4");
        check(hstrlen(0) == 0,         "host strlen tolerates a null pointer");
    }

    /* host +0x38: the millisecond clock */
    {
        host_ticks_fn ticks = *(host_ticks_fn *)((unsigned char *)host + 0x38);
        uint32_t a = ticks(), bq = ticks();
        emit("  ticks "); emit_u(a); emit(" then "); emit_u(bq); emit("\r\n");
        check(bq >= a, "host clock does not run backwards");
    }

    /* UTIL +0x9c against the reimplementation, both measuring through the
     * real host rather than a stub */
    {
        typedef const char * (FS98_STDCALL *util_longer_fn)(const char *, const char *);
        util_longer_fn rlonger = *(util_longer_fn *)((unsigned char *)util + 0x9c);
        static const char *s[] = { "", "a", "ab", "abc", "hello world" };
        unsigned i, j, bad = 0;
        fs98_util_set_host(host);
        for (i = 0; i < 5; i++)
            for (j = 0; j < 5; j++)
                if (rlonger(s[i], s[j]) != fs98_util_longer_string(s[i], s[j]))
                    bad++;
        check(bad == 0, "UTIL +0x9c matches retail across 25 string pairs");
    }

    emit("\r\n");
    emit("passed "); emit_u(pass_count);
    emit(", failed "); emit_u(fail_count);
    emit("\r\n");
}

/* ---- module plumbing ------------------------------------------------------ */

static void ofstest_init(void)
{
    log_handle = CreateFileA(OFSTEST_LOG, GENERIC_WRITE, FILE_SHARE_READ, 0,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    run_tests();
    if (log_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(log_handle);
        log_handle = INVALID_HANDLE_VALUE;
    }
}

static void ofstest_deinit(void) { }

struct {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
} Linkage = { 5, ofstest_init, ofstest_deinit };

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(inst);
    return TRUE;
}
