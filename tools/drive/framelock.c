/* FRAMELOCK.DLL -- run the sim on a virtual clock and log a checksum of every frame.
 *
 * Every clock the sim reads and every blit it makes go through the host's import
 * table: no module imports a time function or blits in software mode.  This module
 * rewrites those entries from DllMain, before any module's init runs:
 *
 *   timeGetTime       a counter; the main loop's own read (return address 0x402177)
 *                     advances it STEP_MS, so every pass runs exactly one frame
 *   timeSetEvent      the 20 ms multimedia timer (0x401950) is not started; it is
 *                     called on the main thread when the main loop's read passes
 *                     its next due time
 *   GetLocalTime, GetMessageTime   derived from the counter
 *   Sleep             returns at once
 *   BitBlt, StretchBlt, StretchDIBits   logged with a hash of the source bits
 *
 * An access violation is logged with the stack, since wine may swallow it.
 *
 * Two runs that do the same thing then log the same lines, so running retail and a
 * reimplementation is a frame-exact comparison of the whole game.  The harness's
 * first duty is to show that two retail runs agree (tools/hg2d_frames.sh does).
 *
 * The environment, read through wine from the shell:
 *
 *   FRAMELOCK_LOG      the log's path (default framelock.log in the game directory)
 *   FRAMELOCK_PASSES   end the process after this many main-loop passes (default 900)
 *   FRAMELOCK_DUMP     write each source DIB as a BMP beside the log every this many
 *                      passes, and whenever FRAMELOCK_DUMP_AT names the pass
 *   FRAMELOCK_DUMP_AT  one pass to dump
 *   FRAMELOCK_HOLD     every this many passes, create "hold" beside the log and wait
 *                      until it is gone, so a screenshot of that frame can be taken:
 *                      Direct3D frames do not pass through the blits logged here
 *   FRAMELOCK_GLOBAL   a path: GLOBAL's GLOBAL_SIZE bytes appended as each pass begins,
 *                      to find the first field two runs disagree on
 *   FRAMELOCK_SCRIPT   a file of "pass message wparam lparam" lines, hex, ascending,
 *                      posted to the main window as that pass begins
 *
 * It takes module id 12, which the retail set leaves unused, and imports GLOBAL.
 * It is a test tool: nothing installs it into game/.
 */

#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"

#define HOST_MAIN_LOOP_READ 0x402177u
#define HOST_MAIN_WINDOW    0x41880cu
#define STEP_MS             33u
#define TIMER_MS            20u
#define CLOCK_START         1000000u
#define MAX_SCRIPT          4096
#define GLOBAL_SIZE         0x12a8u

struct fs98_import ImportTable[] = {
    { FS98_MOD_GLOBAL, 0 },
    { 0, 0 }
};

static uint32_t clock_ms = CLOCK_START;
static uint32_t passes, pass_limit = 900, dump_every, dump_at, hold_every;
static uint32_t blits;
static uint32_t timer_due;
static LPTIMECALLBACK timer_callback;
static DWORD_PTR timer_user;
static HANDLE log_handle = INVALID_HANDLE_VALUE;
static HANDLE global_handle = INVALID_HANDLE_VALUE;
static char log_dir[MAX_PATH];

static struct { uint32_t pass, message, wparam, lparam; } script[MAX_SCRIPT];
static unsigned script_count, script_next;

static BOOL (WINAPI *real_bitblt)(HDC, int, int, int, int, HDC, int, int, DWORD);
static BOOL (WINAPI *real_stretchblt)(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);
static int (WINAPI *real_stretchdibits)(HDC, int, int, int, int, int, int, int, int,
                                        const void *, const BITMAPINFO *, UINT, DWORD);

/* ---- a log, written without any C runtime --------------------------------- */


static char *put_text(char *at, const char *s)
{
    while (*s)
        *at++ = *s++;
    return at;
}

static char *put_hex(char *at, uint32_t v)
{
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        *at++ = "0123456789abcdef"[(v >> shift) & 15];
    return at;
}

static void log_line(const char *start, const char *end)
{
    DWORD wrote;
    if (log_handle != INVALID_HANDLE_VALUE)
        WriteFile(log_handle, start, (DWORD)(end - start), &wrote, 0);
}

static uint32_t hash_bytes(uint32_t hash, const uint8_t *bytes, uint32_t size)
{
    uint32_t index;
    for (index = 0; index < size; index++)
        hash = (hash ^ bytes[index]) * 0x01000193u;
    return hash;
}

static uint32_t dib_size(const BITMAPINFOHEADER *header)
{
    int32_t height = header->biHeight < 0 ? -header->biHeight : header->biHeight;
    return ((header->biWidth * header->biBitCount + 31) / 32 * 4) * (uint32_t)height;
}

/* ---- frames --------------------------------------------------------------- */

static void dump_bmp(const BITMAPINFOHEADER *header, const RGBQUAD *colours, uint32_t colour_count,
                     const void *bits, uint32_t size)
{
    char path[MAX_PATH + 32], *at;
    BITMAPFILEHEADER file;
    HANDLE out;
    DWORD wrote;
    at = put_text(path, log_dir);
    at = put_text(at, "pass");
    at = put_hex(at, passes);
    *at++ = '-';
    at = put_hex(at, blits);
    at = put_text(at, ".bmp");
    *at = 0;
    out = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (out == INVALID_HANDLE_VALUE)
        return;
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof file + sizeof *header + colour_count * sizeof(RGBQUAD);
    file.bfSize = file.bfOffBits + size;
    file.bfReserved1 = file.bfReserved2 = 0;
    WriteFile(out, &file, sizeof file, &wrote, 0);
    WriteFile(out, header, sizeof *header, &wrote, 0);
    if (colour_count)
        WriteFile(out, colours, colour_count * sizeof(RGBQUAD), &wrote, 0);
    WriteFile(out, bits, size, &wrote, 0);
    CloseHandle(out);
}

static void log_blit(const char *kind, int x, int y, int w, int h,
                     const BITMAPINFOHEADER *header, const RGBQUAD *colours, uint32_t colour_count,
                     const void *bits)
{
    char line[160], *at = line;
    uint32_t hash = 0x811c9dc5u, palette = 0x811c9dc5u, size = 0;
    if (header && bits) {
        size = dib_size(header);
        hash = hash_bytes(hash, bits, size);
        palette = hash_bytes(palette, (const uint8_t *)colours, colour_count * sizeof(RGBQUAD));
    }
    at = put_hex(at, passes);
    *at++ = ' ';
    at = put_hex(at, blits);
    *at++ = ' ';
    at = put_text(at, kind);
    *at++ = ' ';
    at = put_hex(at, (uint32_t)x);
    *at++ = ' ';
    at = put_hex(at, (uint32_t)y);
    *at++ = ' ';
    at = put_hex(at, (uint32_t)w);
    *at++ = ' ';
    at = put_hex(at, (uint32_t)h);
    *at++ = ' ';
    at = put_hex(at, size);
    *at++ = ' ';
    at = put_hex(at, hash);
    *at++ = ' ';
    at = put_hex(at, palette);
    *at++ = '\n';
    log_line(line, at);
    if (header && bits && ((dump_every && passes % dump_every == 0) || (dump_at && passes == dump_at)))
        dump_bmp(header, colours, colour_count, bits, size);
    blits++;
}

/* The source of a blit is a DIB section selected into a memory DC. */
static void log_source(const char *kind, HDC source, int x, int y, int w, int h)
{
    DIBSECTION section;
    RGBQUAD colours[256];
    uint32_t count = 0;
    HGDIOBJ object = source ? GetCurrentObject(source, OBJ_BITMAP) : 0;
    if (object && GetObjectA(object, sizeof section, &section) == sizeof section && section.dsBm.bmBits) {
        if (section.dsBmih.biBitCount <= 8)
            count = GetDIBColorTable(source, 0, 256, colours);
        log_blit(kind, x, y, w, h, &section.dsBmih, colours, count, section.dsBm.bmBits);
    } else {
        log_blit(kind, x, y, w, h, 0, 0, 0, 0);
    }
}

static BOOL WINAPI hook_bitblt(HDC dest, int x, int y, int w, int h, HDC source, int sx, int sy, DWORD rop)
{
    log_source("bitblt", source, x, y, w, h);
    return real_bitblt(dest, x, y, w, h, source, sx, sy, rop);
}

static BOOL WINAPI hook_stretchblt(HDC dest, int x, int y, int w, int h, HDC source,
                                   int sx, int sy, int sw, int sh, DWORD rop)
{
    log_source("stretchblt", source, x, y, w, h);
    return real_stretchblt(dest, x, y, w, h, source, sx, sy, sw, sh, rop);
}

static int WINAPI hook_stretchdibits(HDC dest, int x, int y, int w, int h, int sx, int sy, int sw, int sh,
                                     const void *bits, const BITMAPINFO *info, UINT usage, DWORD rop)
{
    uint32_t count = 0;
    if (info && info->bmiHeader.biBitCount <= 8 && usage == DIB_RGB_COLORS)
        count = info->bmiHeader.biClrUsed ? info->bmiHeader.biClrUsed : 1u << info->bmiHeader.biBitCount;
    log_blit("stretchdibits", x, y, w, h, info ? &info->bmiHeader : 0, info ? info->bmiColors : 0, count, bits);
    return real_stretchdibits(dest, x, y, w, h, sx, sy, sw, sh, bits, info, usage, rop);
}

/* ---- the clock ------------------------------------------------------------ */

static void finish(void)
{
    char line[32], *at = line;
    at = put_text(at, "end ");
    at = put_hex(at, passes);
    *at++ = '\n';
    log_line(line, at);
    if (log_handle != INVALID_HANDLE_VALUE)
        CloseHandle(log_handle);
    ExitProcess(0);
}

static void hold_for_screenshot(void)
{
    char path[MAX_PATH + 8], *at = put_text(path, log_dir);
    HANDLE file;
    at = put_text(at, "hold");
    *at = 0;
    file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return;
    CloseHandle(file);
    while (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
        Sleep(20);
}

static void main_loop_pass(void)
{
    if (global_handle != INVALID_HANDLE_VALUE) {
        DWORD wrote;
        WriteFile(global_handle, (const void *)ImportTable[0].link, GLOBAL_SIZE, &wrote, 0);
    }
    passes++;
    if (passes >= pass_limit)
        finish();
    if (hold_every && passes % hold_every == 0)
        hold_for_screenshot();
    while (script_next < script_count && script[script_next].pass <= passes) {
        HWND window = *(HWND *)HOST_MAIN_WINDOW;
        if (window)
            PostMessageA(window, script[script_next].message, script[script_next].wparam, script[script_next].lparam);
        script_next++;
    }
    clock_ms += STEP_MS;
    while (timer_callback && (int32_t)(clock_ms - timer_due) >= 0) {
        timer_callback(1, 0, timer_user, 0, 0);
        timer_due += TIMER_MS;
    }
}

static DWORD WINAPI hook_timegettime(void)
{
    if ((uint32_t)(uintptr_t)__builtin_return_address(0) == HOST_MAIN_LOOP_READ)
        main_loop_pass();
    return clock_ms;
}

static MMRESULT WINAPI hook_timesetevent(UINT delay, UINT resolution, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags)
{
    (void)delay; (void)resolution; (void)flags;
    timer_callback = callback;
    timer_user = user;
    timer_due = clock_ms + TIMER_MS;
    return 1;
}

static MMRESULT WINAPI hook_timekillevent(UINT id)
{
    (void)id;
    timer_callback = 0;
    return 0;
}

static LONG WINAPI hook_getmessagetime(void)
{
    return (LONG)clock_ms;
}

static void WINAPI hook_sleep(DWORD ms)
{
    (void)ms;
}

/* A fixed day, moved on by the counter. */
static void WINAPI hook_getlocaltime(SYSTEMTIME *time)
{
    uint32_t seconds = (clock_ms - CLOCK_START) / 1000;
    time->wYear = 1998;
    time->wMonth = 6;
    time->wDayOfWeek = 1;
    time->wDay = 15;
    time->wHour = (WORD)(12 + (seconds / 3600) % 12);
    time->wMinute = (WORD)((seconds / 60) % 60);
    time->wSecond = (WORD)(seconds % 60);
    time->wMilliseconds = (WORD)((clock_ms - CLOCK_START) % 1000);
}

/* ---- faults: wine may swallow them, so the log keeps the registers and stack ---- */

static LONG WINAPI log_fault(EXCEPTION_POINTERS *pointers)
{
    const CONTEXT *c = pointers->ContextRecord;
    char line[512], *at = line;
    const uint32_t *stack = (const uint32_t *)c->Esp;
    unsigned index;
    if (pointers->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    at = put_text(at, "fault pass ");
    at = put_hex(at, passes);
    at = put_text(at, " eip ");
    at = put_hex(at, c->Eip);
    at = put_text(at, " stack");
    for (index = 0; index < 40 && !IsBadReadPtr(stack + index, 4); index++) {
        *at++ = ' ';
        at = put_hex(at, stack[index]);
    }
    *at++ = '\n';
    log_line(line, at);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---- the host's import table ---------------------------------------------- */

static int same_name(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static void *patch_import(const char *name, void *hook)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(0);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *library = (IMAGE_IMPORT_DESCRIPTOR *)
        (base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; library->Name; library++) {
        uint32_t *names = (uint32_t *)(base + library->OriginalFirstThunk);
        void **slots = (void **)(base + library->FirstThunk);
        unsigned index;
        if (!library->OriginalFirstThunk)
            continue;
        for (index = 0; names[index]; index++) {
            if (names[index] >> 31)
                continue;
            if (same_name((const char *)(base + names[index] + 2), name)) {
                void *previous = slots[index];
                DWORD protection;
                VirtualProtect(&slots[index], sizeof(void *), PAGE_READWRITE, &protection);
                slots[index] = hook;
                VirtualProtect(&slots[index], sizeof(void *), protection, &protection);
                return previous;
            }
        }
    }
    return 0;
}

static uint32_t parse_hex(const char **text)
{
    const char *at = *text;
    uint32_t value = 0;
    while (*at == ' ' || *at == '\t')
        at++;
    for (;; at++) {
        uint32_t digit;
        if (*at >= '0' && *at <= '9') digit = (uint32_t)(*at - '0');
        else if (*at >= 'a' && *at <= 'f') digit = (uint32_t)(*at - 'a' + 10);
        else if (*at >= 'A' && *at <= 'F') digit = (uint32_t)(*at - 'A' + 10);
        else break;
        value = value * 16 + digit;
    }
    *text = at;
    return value;
}

static uint32_t environment_number(const char *name, uint32_t fallback)
{
    char text[32];
    uint32_t value = 0, index;
    if (!GetEnvironmentVariableA(name, text, sizeof text))
        return fallback;
    for (index = 0; text[index] >= '0' && text[index] <= '9'; index++)
        value = value * 10 + (uint32_t)(text[index] - '0');
    return value;
}

static void read_script(void)
{
    static char text[0x20000];
    char path[MAX_PATH];
    const char *at = text;
    DWORD got = 0;
    HANDLE file;
    if (!GetEnvironmentVariableA("FRAMELOCK_SCRIPT", path, sizeof path))
        return;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE)
        return;
    ReadFile(file, text, sizeof text - 1, &got, 0);
    CloseHandle(file);
    text[got] = 0;
    while (*at && script_count < MAX_SCRIPT) {
        script[script_count].pass = parse_hex(&at);
        script[script_count].message = parse_hex(&at);
        script[script_count].wparam = parse_hex(&at);
        script[script_count].lparam = parse_hex(&at);
        script_count++;
        while (*at && *at != '\n')
            at++;
        while (*at == '\n' || *at == '\r')
            at++;
    }
}

static void framelock_init(void) {}
static void framelock_deinit(void) {}

/* The host reads past init and deinit, so what follows must be zeros: a variable
 * placed here was taken for a pointer. */
struct {
    uint32_t id;
    void (*init)(void);
    void (*deinit)(void);
    uint32_t rest[64];
} Linkage = { 12, framelock_init, framelock_deinit, { 0 } };

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char path[MAX_PATH];
        unsigned index, slash = 0;
        DisableThreadLibraryCalls(inst);
        if (!GetEnvironmentVariableA("FRAMELOCK_LOG", path, sizeof path))
            lstrcpyA(path, "framelock.log");
        for (index = 0; path[index]; index++)
            if (path[index] == '\\' || path[index] == '/')
                slash = index + 1;
        for (index = 0; index < slash; index++)
            log_dir[index] = path[index];
        log_dir[slash] = 0;
        log_handle = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        pass_limit = environment_number("FRAMELOCK_PASSES", pass_limit);
        dump_every = environment_number("FRAMELOCK_DUMP", 0);
        dump_at = environment_number("FRAMELOCK_DUMP_AT", 0);
        hold_every = environment_number("FRAMELOCK_HOLD", 0);
        read_script();
        if (GetEnvironmentVariableA("FRAMELOCK_GLOBAL", path, sizeof path))
            global_handle = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        AddVectoredExceptionHandler(1, log_fault);
        patch_import("timeGetTime", (void *)hook_timegettime);
        patch_import("timeSetEvent", (void *)hook_timesetevent);
        patch_import("timeKillEvent", (void *)hook_timekillevent);
        patch_import("GetMessageTime", (void *)hook_getmessagetime);
        patch_import("GetLocalTime", (void *)hook_getlocaltime);
        patch_import("Sleep", (void *)hook_sleep);
        real_bitblt = patch_import("BitBlt", (void *)hook_bitblt);
        real_stretchblt = patch_import("StretchBlt", (void *)hook_stretchblt);
        real_stretchdibits = patch_import("StretchDIBits", (void *)hook_stretchdibits);
    }
    return TRUE;
}
