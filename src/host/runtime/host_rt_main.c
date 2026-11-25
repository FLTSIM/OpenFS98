/* The application: its startup (the window classes, the paths, the main window), CWinApp's
 * InitInstance, Run and ExitInstance as the game overrides them, the multimedia timer, the
 * logo, the language DLL and the main window's placement. */
#include "host_rt.h"
#include "fs98/arena.h"

/* a Win32 import called through the table, by its arguments */
#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define CFG ((const char *)VA(0x41c120))        /* the configuration file: FLTSIM98.CFG */
#define MAIN_WINDOW G32(0x41880c)

/* the executable's hook for the frame-locked test driver (OpenFS98Host's main_loop_pass, as the
 * lifted build calls it at 0x402171): none in the harness */
void (*fs98_host_rt_loop_pass)(void);

/* ---- the application's overrides (its vtable, 0x416020) -------------------------------- */

/* 0x401110: InitInstance: 0x401a70 under the divide repair. */
int32_t fs98_host_rt_application_init(void)
{
    return fs98_host_rt_guarded(fs98_host_rt_init_instance);
}

/* 0x4010f0: ExitInstance: 0x402050 under the divide repair, then CWinApp's (import #2725). */
int32_t TC fs98_host_rt_application_exit(void *application)
{
    fs98_host_rt_guarded(fs98_host_rt_exit_instance);
    return CALL_THIS(MFC_2725, int32_t, (void *))(application);
}

/* 0x401120: Run: the loop (0x4020b0) under the divide repair, then ExitInstance (vtable +0x70),
 * whose result is Run's. */
int32_t TC fs98_host_rt_application_run(void *application)
{
    fs98_host_rt_guarded(fs98_host_rt_run);
    return VIRTUAL(application, 0x70, int32_t, (void *))(application);
}

/* ---- startup ---------------------------------------------------------------------------- */

/* 0x401940: an error at startup reported: nothing (its message id is not used). */
void SC fs98_host_rt_report(uint32_t message)
{
    (void)message;
}

/* 0x401910: the configuration told the language DLL ([LANGUAGE] DLL= the resource string 5,
 * 0x41bee0), and that it is a new one (0x4187e8). */
void fs98_host_rt_new_configuration(void)
{
    G32(0x4187e8) = 1;
    WIN(WritePrivateProfileStringA, int32_t, (const char *, const char *, const char *, const char *))(
        STR(0x4170a8), STR(0x4170b4), STR(0x41bee0), CFG);
}

/* one of the four window classes registered: 0, after the startup error reported, if not */
static int32_t register_class(uint32_t *wc, uint32_t style, void *procedure, int32_t icon, uint32_t name, uint32_t error)
{
    uint32_t instance = G32(0x418828);
    wc[0] = style;
    wc[1] = fs98_handle(procedure);
    wc[2] = 0;
    wc[3] = 0;
    wc[4] = instance;
    wc[5] = icon ? WIN(LoadIconA, uint32_t, (uint32_t, uint32_t))(instance, 0x65) : 0;
    wc[6] = WIN(LoadCursorA, uint32_t, (uint32_t, uint32_t))(0, 0x7f00);
    wc[7] = WIN(GetStockObject, uint32_t, (int32_t))(4);
    wc[8] = 0;
    wc[9] = fs98_handle(VA(name));
    if ((uint16_t)WIN(RegisterClassA, uint32_t, (const uint32_t *))(wc) != 0)
        return 1;
    fs98_host_rt_report(error);
    return 0;
}

/* 0x401150: the startup proper, with the instance (unused: 0x418828 is read instead). The
 * classes FS98MAIN, FS98CHILD, FS98FLOAT and FS98DLG3D; the directories (the current one kept
 * at 0x418690, the EXE's made current, 0x41bca0); the language DLL's name (string 5,
 * 0x41bee0); the configuration (FLTSIM98.CFG beside the EXE, or /CFG:name -- deleted with
 * shift and control held, or for an empty /CFG:), and the help file (0x41c000); the main
 * window's place ([MAIN] Location=, else three quarters of a screen up to 640 wide, which the
 * configuration is told is maximised); the language DLL loaded (0x4025d0); the main window
 * made and attached to the CWnd at 0x418798, the application's main window; the menu options;
 * and ten strings from the language DLL. 1, or 0 when something could not be done. */
int32_t SC fs98_host_rt_startup(uint32_t instance)
{
    uint32_t wc[10];
    int32_t area[2], horizontal, vertical, width, height;
    char drive[0x40], directory[0x104], value[0x104], extension[0x104], exe_directory[0x104], exe_drive[0x104],
        name[0x104], exe_name[0x104];
    uint32_t dc;
    void (*splitpath)(const char *, char *, char *, char *, char *) =
        (void (*)(const char *, char *, char *, char *, char *))IMPORT(_splitpath);
    void (*makepath)(char *, const char *, const char *, const char *, const char *) =
        (void (*)(char *, const char *, const char *, const char *, const char *))IMPORT(_makepath);
    (void)instance;
    if (!register_class(wc, 3, (void *)fs98_host_rt_main_procedure, 1, 0x417158, 0xf002) ||
        !register_class(wc, 0x4023, (void *)fs98_host_rt_child_procedure, 0, 0x417168, 0xf005) ||
        !register_class(wc, 0x4003, (void *)fs98_host_rt_floating_procedure, 0, 0x417178, 0xf005) ||
        !register_class(wc, 0x4003, (void *)fs98_host_rt_dialog_procedure, 0, 0x417188, 0xf005))
        return 0;
    WIN(GetCurrentDirectoryA, uint32_t, (uint32_t, char *))(0x104, (char *)VA(0x418690));
    WIN(GetModuleFileNameA, uint32_t, (uint32_t, char *, uint32_t))(G32(0x418828), (char *)VA(0x41bca0), 0x104);
    splitpath(STR(0x41bca0), exe_drive, exe_directory, exe_name, 0);
    makepath((char *)VA(0x41bca0), exe_drive, exe_directory, 0, 0);
    WIN(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(G32(0x418828), 5, (char *)VA(0x41bee0), 0x104);
    WIN(SetCurrentDirectoryA, int32_t, (const char *))(STR(0x41bca0));
    makepath((char *)VA(0x41c120), exe_drive, exe_directory, exe_name, STR(0x4170a0));
    makepath((char *)VA(0x41c000), exe_drive, exe_directory, STR(0x41708c), STR(0x417098));
    if ((WIN(GetAsyncKeyState, uint32_t, (int32_t))(0x10) & 0x8000) && (WIN(GetAsyncKeyState, uint32_t, (int32_t))(0x11) & 0x8000))
        WIN(DeleteFileA, int32_t, (const char *))(CFG);
    if (SERVICE(0x98, int32_t, (const char *, char *, uint32_t))(STR(0x417088), value, 0x104) == 0) {
        if (value[0] == 0)
            WIN(DeleteFileA, int32_t, (const char *))(CFG);
        else {
            splitpath(value, drive, directory, name, extension);
            if (drive[0] == 0 && (directory[0] == 0 || directory[0] == '\\'))
                WIN(lstrcpyA, char *, (char *, const char *))(drive, exe_drive);
            if (directory[0] == 0)
                WIN(lstrcpyA, char *, (char *, const char *))(directory, exe_directory);
            if (extension[0] == 0)
                WIN(lstrcpyA, char *, (char *, const char *))(extension, STR(0x4170a0));
            makepath((char *)VA(0x41c120), drive, directory, name, extension);
        }
    }
    if (WIN(GetFileAttributesA, uint32_t, (const char *))(CFG) == 0xffffffffu)
        fs98_host_rt_new_configuration();
    WIN(SetRectEmpty, int32_t, (void *))(VA(0x41c590));
    WIN(GetPrivateProfileStringA, uint32_t, (const char *, const char *, const char *, char *, uint32_t, const char *))(
        STR(0x417074), STR(0x41707c), STR(0x4187f8), (char *)VA(0x41c480), 0x100, CFG);
    CALL_C(sscanf, int32_t, (const char *, const char *, void *, void *, void *, void *))(
        STR(0x41c480), STR(0x417068), VA(0x41c590), VA(0x41c594), VA(0x41c598), VA(0x41c59c));
    WIN(SystemParametersInfoA, int32_t, (uint32_t, uint32_t, void *, uint32_t))(0x30, 0, area, 0);
    WIN(OffsetRect, int32_t, (void *, int32_t, int32_t))(VA(0x41c590), area[0], area[1]);
    if (G32(0x41c598) - G32(0x41c590) == 0x280 && G32(0x41c59c) - G32(0x41c594) == 0x1e0)
        WIN(SetRectEmpty, int32_t, (void *))(VA(0x41c590));
    dc = WIN(GetDC, uint32_t, (uint32_t))(0);
    if (WIN(IsRectEmpty, int32_t, (const void *))(VA(0x41c590)) ||
        !WIN(RectVisible, int32_t, (uint32_t, const void *))(dc, VA(0x41c590))) {
        width = 0x280;
        height = 0x1e0;
        horizontal = WIN(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 8);
        vertical = WIN(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 10);
        if (horizontal <= 0x280) {
            WIN(WritePrivateProfileStringA, int32_t, (const char *, const char *, const char *, const char *))(
                STR(0x417074), STR(0x417058), STR(0x417064), CFG);
            width = (int32_t)((uint32_t)horizontal * 3) / 4;
            height = (int32_t)((uint32_t)vertical * 3) / 4;
        }
        GI32(0x41c590) = (int32_t)(WIN(GetDeviceCaps, uint32_t, (uint32_t, int32_t))(dc, 8) - (uint32_t)width) / 2;
        GI32(0x41c594) = (int32_t)(WIN(GetDeviceCaps, uint32_t, (uint32_t, int32_t))(dc, 10) - (uint32_t)height) / 2;
        G32(0x41c598) = (uint32_t)width + G32(0x41c590);
        G32(0x41c59c) = (uint32_t)height + G32(0x41c594);
        WIN(AdjustWindowRect, int32_t, (void *, uint32_t, int32_t))(VA(0x41c590), 0xcf0000, 1);
    }
    WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(0, dc);
    if (!fs98_host_rt_load_language())
        return 0;
    G32(0x418864) = 1;
    MAIN_WINDOW = WIN(CreateWindowExA, uint32_t, (uint32_t, const char *, const char *, uint32_t, int32_t, int32_t, int32_t,
                                                  int32_t, uint32_t, uint32_t, uint32_t, void *))(
        0, STR(0x417158), STR(0x4187f8), 0x2cf0000, 0x4e20, 0x4e20, (int32_t)(G32(0x41c598) - G32(0x41c590)),
        (int32_t)(G32(0x41c59c) - G32(0x41c594)), 0, 0, G32(0x418828), 0);
    if (MAIN_WINDOW == 0) {
        fs98_host_rt_report(0xf001);
        return 0;
    }
    WIN(ShowWindow, int32_t, (uint32_t, int32_t))(MAIN_WINDOW, 5);
    WIN(UpdateWindow, int32_t, (uint32_t))(MAIN_WINDOW);
    G_PTR(0x4185e8) = VA(0x418798);
    CALL_THIS(MFC_1651, int32_t, (void *, uint32_t))(VA(0x418798), MAIN_WINDOW);
    G32(0x418824) = WIN(GetPrivateProfileIntA, uint32_t, (const char *, const char *, int32_t, const char *))(
        STR(0x417074), STR(0x417048), 0, CFG);
    G32(0x417198) = WIN(GetPrivateProfileIntA, uint32_t, (const char *, const char *, int32_t, const char *))(
        STR(0x417074), STR(0x417034), 1, CFG);
    CALL_STD(_17, void, (void))();
    {
        static const uint32_t strings[10] = {0x41bec0, 0x41bc80, 0x41bb64, 0x41bfe8, 0x41bb68,
                                             0x41bdac, 0x41bda4, 0x41c360, 0x41bc84, 0x41c104};
        uint32_t at;
        for (at = 0; at < 10; ++at)
            SERVICE(0x2a4, int32_t, (uint32_t, void *))(0x9520 + at, VA(strings[at]));
    }
    return 1;
}

/* 0x401850: the end: the ten strings freed, the classes unregistered (FS98FLOAT not), the
 * language DLL freed, and the directory the game started in made current again. */
void fs98_host_rt_shut_down(void)
{
    static const uint32_t strings[10] = {0x41bec0, 0x41bc80, 0x41bb64, 0x41bfe8, 0x41bb68,
                                         0x41bdac, 0x41bda4, 0x41c360, 0x41bc84, 0x41c104};
    uint32_t at;
    for (at = 0; at < 10; ++at)
        SERVICE(0x214, int32_t, (void *))(VA(strings[at]));
    WIN(UnregisterClassA, int32_t, (const char *, uint32_t))(STR(0x417158), G32(0x418828));
    WIN(UnregisterClassA, int32_t, (const char *, uint32_t))(STR(0x417168), G32(0x418828));
    WIN(UnregisterClassA, int32_t, (const char *, uint32_t))(STR(0x417188), G32(0x418828));
    if (G32(0x41882c)) {
        WIN(FreeLibrary, int32_t, (uint32_t))(G32(0x41882c));
        G32(0x41882c) = 0;
    }
    WIN(SetCurrentDirectoryA, int32_t, (const char *))(STR(0x418690));
}

/* 0x4025d0: the language DLL ([LANGUAGE] DLL= in the EXE's directory, else the resource
 * string 5) loaded as 0x41882c, the one before freed (and the handle left), and its string 3
 * read (0x41c380). 1, or 0 after a message box when it does not load. */
int32_t fs98_host_rt_load_language(void)
{
    char *path = (char *)VA(0x41c480);
    uint32_t length = 0, error;
    if (G32(0x41882c))
        WIN(FreeLibrary, int32_t, (uint32_t))(G32(0x41882c));
    do
        path[length] = STR(0x41bca0)[length];
    while (STR(0x41bca0)[length++]);
    for (length = 0; path[length]; ++length)
        ;
    WIN(GetPrivateProfileStringA, uint32_t, (const char *, const char *, const char *, char *, uint32_t, const char *))(
        STR(0x4170a8), STR(0x4170b4), STR(0x41bee0), path + length, 0x104 - length, CFG);
    G32(0x41882c) = WIN(LoadLibraryA, uint32_t, (const char *))(path);
    if (G32(0x41882c) == 0) {
        error = WIN(GetLastError, uint32_t, (void))();
        CALL_C(sprintf, int32_t, (char *, const char *, const char *, uint32_t, uint32_t))(
            (char *)VA(0x41bdc0), STR(0x4179c4), path, error, error);
        WIN(MessageBoxA, int32_t, (uint32_t, const char *, const char *, uint32_t))(0, STR(0x41bdc0), STR(0x4179ac), 0x10);
        return 0;
    }
    SERVICE(0x2a0, int32_t, (uint32_t, char *, uint32_t))(3, (char *)VA(0x41c380), 0x100);
    return 1;
}

/* ---- InitInstance, Run, ExitInstance ------------------------------------------------------ */

/* 0x401f60: the modeless-dialog entry 0x4187d8's handler: nothing, 0. */
int32_t SC fs98_host_rt_401f60(uint32_t unused)
{
    (void)unused;
    return 0;
}

/* 0x401f70: the processor's speed in MHz, measured: 0 unless an x86 Pentium (type 586) with a
 * time-stamp counter. The time-stamp counter's count across a short busy loop over the
 * performance counter's time for it in microseconds, rounded; the process at real-time
 * priority meanwhile. */
int32_t fs98_host_rt_cpu_speed(void)
{
    uint32_t info[9], frequency[2], start[2], stop[2], features, tsc_low, tsc_high, before, process, priority;
    int32_t microseconds;
    uint32_t at;
    for (at = 0; at < 9; ++at)
        info[at] = 0;
    WIN(GetSystemInfo, void, (void *))(info);
    if ((uint16_t)info[0] != 0 || info[6] < 0x24a)
        return 0;
    __asm__ volatile("cpuid" : "=d"(features), "=a"(tsc_low), "=b"(tsc_high), "=c"(before) : "a"(1));
    if (!(features & 0x10))
        return 0;
    process = WIN(GetCurrentProcess, uint32_t, (void))();
    priority = WIN(GetPriorityClass, uint32_t, (uint32_t))(process);
    WIN(SetPriorityClass, int32_t, (uint32_t, uint32_t))(process, 0x100);
    WIN(QueryPerformanceFrequency, int32_t, (void *))(frequency);
    WIN(QueryPerformanceCounter, int32_t, (void *))(start);
    /* rdtsc, the loop, rdtsc: one block, as retail's (the harness answers the two reads) */
    __asm__ volatile("rdtsc\n\t"
                     "movl %%eax, %0\n\t"
                     "movl $100000, %%ecx\n"
                     "1:\n\t"
                     "decl %%ecx\n\t"
                     "jne 1b\n\t"
                     "rdtsc\n\t"
                     "subl %0, %%eax"
                     : "=&r"(before), "=a"(tsc_low), "=d"(tsc_high) : : "ecx");
    WIN(QueryPerformanceCounter, int32_t, (void *))(stop);
    WIN(SetPriorityClass, int32_t, (uint32_t, uint32_t))(process, priority);
    microseconds = WIN(MulDiv, int32_t, (int32_t, int32_t, int32_t))((int32_t)(stop[0] - start[0]), 1000000,
                                                                       (int32_t)frequency[0]);
    return (int32_t)((uint32_t)(microseconds / 2) + tsc_low) / microseconds;
}

/* 0x4024c0: another copy of the game is running (its main window given): the lobby's
 * connection (+0x120) and the command line's /STN: sent to it (WM_COPYDATA, 0x10000000) in a
 * block: +0 1, +4 whether /STN: was given, +8 its value, +0x110 the connection's size and
 * +0x114 the connection. */
void SC fs98_host_rt_tell_running(uint32_t window)
{
    uint8_t *block;
    void *connection;
    uint32_t size, data[3];
    data[0] = 0x10000000;
    data[1] = 0;
    data[2] = 0;
    SERVICE(0x120, int32_t, (void **, uint32_t *, uint32_t))(&connection, &size, 1);
    SERVICE(0x20c, int32_t, (uint32_t, uint32_t, void *, uint32_t))(1, size + 0x114, &block, 1);
    D32(block, 0) = 1;
    D32(block, 0x110) = size;
    if (size)
        SERVICE(0x21c, int32_t, (void *, void *, uint32_t))(block + 0x114, connection, size);
    if (SERVICE(0x98, int32_t, (const char *, char *, uint32_t))(STR(0x417150), (char *)block + 8, 0x104))
        D32(block, 4) = 1;
    data[1] = size + 0x114;
    data[2] = fs98_handle(block);
    WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, void *))(window, 0x4a, 0, data);
    SERVICE(0x214, int32_t, (void *))(&connection);
    SERVICE(0x214, int32_t, (void *))(&block);
}

/* 0x402210: the logo's window procedure: its close command ignored, the rest to MCIWnd's
 * own procedure, kept in its GWL_USERDATA. */
int32_t SC fs98_host_rt_logo_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l)
{
    if (message == 0x112 && (w & 0xfff0) == 0xf060)
        return 0;
    return WIN(CallWindowProcA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))(
        WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -21), window, message, w, l);
}

/* the logo's messages pumped */
static void pump(uint32_t *message)
{
    while (WIN(PeekMessageA, int32_t, (void *, uint32_t, uint32_t, uint32_t, uint32_t))(message, 0, 0, 0, 1)) {
        WIN(TranslateMessage, int32_t, (const void *))(message);
        WIN(DispatchMessageA, int32_t, (const void *))(message);
    }
}

/* 0x402260: the logo (mslogo.avi beside the EXE) played in an MCIWnd, topmost, centred (twice
 * the size on a screen wider than 640), until it stops or escape is held. 0. */
int32_t SC fs98_host_rt_show_logo(uint32_t unused)
{
    char path[0x104];
    int32_t rect[4], width, height;
    uint32_t window, dc, message[7], process;
    (void)unused;
    WIN(lstrcpyA, char *, (char *, const char *))(path, STR(0x41bca0));
    WIN(lstrcatA, char *, (char *, const char *))(path, STR(0x417144));
    if (SERVICE(0x158, int32_t, (const char *, uint32_t))(path, 0) != 0)
        return 0;
    window = CALL_C(MCIWndCreateA, uint32_t, (uint32_t, uint32_t, uint32_t, const char *))(0, G32(0x418828), 0x8088000a, path);
    if (window == 0)
        return 0;
    WIN(GetAsyncKeyState, uint32_t, (int32_t))(0x1b);
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(
        window, -20, WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -20) | 0x80);
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(window, -21, WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -4));
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(window, -4, fs98_handle(fs98_host_rt_logo_procedure));
    if (WIN(GetSystemMetrics, int32_t, (int32_t))(0) > 0x280)
        WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x46c, 0, 0xc8);
    WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
    dc = WIN(GetDC, uint32_t, (uint32_t))(0);
    height = (int32_t)((uint32_t)rect[3] - (uint32_t)rect[1]);
    width = (int32_t)((uint32_t)rect[2] - (uint32_t)rect[0]);
    {
        int32_t y = (int32_t)(WIN(GetDeviceCaps, uint32_t, (uint32_t, int32_t))(dc, 10) + ((uint32_t)rect[1] - (uint32_t)rect[3])) / 2;
        int32_t x = (int32_t)(WIN(GetDeviceCaps, uint32_t, (uint32_t, int32_t))(dc, 8) + ((uint32_t)rect[0] - (uint32_t)rect[2])) / 2;
        WIN(MoveWindow, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t))(window, x, y, width, height, 1);
    }
    WIN(ShowWindow, int32_t, (uint32_t, int32_t))(window, 5);
    WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(0, dc);
    if (WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x806, 0, 0) == 0) {
        if (WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x46a, 0, 0) != 0x20d)
            do {
                pump(message);
                if ((uint16_t)WIN(GetAsyncKeyState, uint32_t, (int32_t))(0x1b))
                    break;
                WIN(Sleep, void, (uint32_t))(0xfa);
            } while (WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x46a, 0, 0) != 0x20d);
        WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x808, 0, 0);
    }
    pump(message);
    WIN(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(WIN(GetForegroundWindow, uint32_t, (void))(), &process);
    WIN(GetCurrentProcessId, uint32_t, (void))();
    WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x10, 0, 0);
    return 0;
}

/* 0x401a70: InitInstance. A second copy (not under /DEBUG) hands its command line to the
 * first and stops; too little memory stops it with a message (string 7), or, short of 32MB,
 * asks (string 8); then the startup (0x401150), the installation's directories from the
 * registry (0x41c240, 0x41bb80), a 256-colour display (else a message from the language DLL,
 * 0xf406, and the end), the logo, DirectDraw, the modules found and started, the modeless
 * dialog list's first entry (0x4187d8), the processor's speed told the game (0x418844) on a
 * new configuration, the game's start (0x41883c), the main window's placement, full screen
 * for [PREFS] FULL_640, and a 50ms multimedia timer (0x401950) -- or the end, if the timer's
 * resolution does not allow one. 1 to run, 0 not to. */
int32_t fs98_host_rt_init_instance(void)
{
    uint32_t status[8], key, size, caps[2], dc, few, show_logo;
    int32_t bits, planes;
    /* SYSTEM_INFO, WINDOWPLACEMENT and MSGBOXPARAMS: one buffer, as retail's */
    uint32_t buffer[11];
    uint8_t *state;
    if (SERVICE(0xa4, int32_t, (const char *))(STR(0x41713c)) == 0) {
        uint32_t running = WIN(FindWindowA, uint32_t, (const char *, const char *))(STR(0x417158), 0);
        if (running) {
            fs98_host_rt_tell_running(running);
            return 0;
        }
    }
    state = CALL_C(MFC_1168, uint8_t *, (void))();
    status[0] = 0x20;
    G32(0x418828) = D32(state, 8);
    WIN(GlobalMemoryStatus, void, (void *))(status);
    if (status[2] <= 0x400000 || status[3] + status[5] < 0x1000000) {
        WIN(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(G32(0x418828), 7, (char *)VA(0x41c480), 0x104);
        WIN(MessageBoxA, int32_t, (uint32_t, const char *, const char *, uint32_t))(0, STR(0x41c480), 0, 0x10);
        return 0;
    }
    if (status[2] < 0x2000000 && status[3] + status[5] < 0x1c00000) {
        WIN(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(G32(0x418828), 8, (char *)VA(0x41c480), 0x104);
        if (WIN(MessageBoxA, int32_t, (uint32_t, const char *, const char *, uint32_t))(0, STR(0x41c480), 0, 0x34) == 7)
            return 0;
    }
    if (!fs98_host_rt_startup(G32(0x418828)))
        return 0;
    if (WIN(RegOpenKeyExA, int32_t, (uint32_t, const char *, uint32_t, uint32_t, uint32_t *))(
            0x80000002u, STR(0x417104), 0, 0xf003f, &key) == 0) {
        int32_t (SC *query)(uint32_t, const char *, uint32_t *, uint32_t *, void *, uint32_t *) =
            (int32_t (SC *)(uint32_t, const char *, uint32_t *, uint32_t *, void *, uint32_t *))
                IMPORT(RegQueryValueExA);
        size = 0x100;
        query(key, STR(0x4170f8), 0, 0, VA(0x41c240), &size);
        size = 0x100;
        query(key, STR(0x4170e8), 0, 0, VA(0x41bb80), &size);
        WIN(RegCloseKey, int32_t, (uint32_t))(key);
    }
    dc = WIN(GetDC, uint32_t, (uint32_t))(0);
    bits = WIN(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 12);
    planes = WIN(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 14);
    few = (int32_t)((uint32_t)bits * (uint32_t)planes) < 8;
    WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(0, dc);
    if (few) {
        WIN(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(G32(0x41882c), 0xf406, (char *)VA(0x41c480), 0x104);
        buffer[0] = 0x28;
        buffer[1] = 0;
        buffer[2] = 0;
        buffer[3] = fs98_handle(VA(0x41c480));
        buffer[4] = 0;
        buffer[5] = 0x14010;
        buffer[6] = 0;
        buffer[7] = 0xbc2;
        buffer[8] = fs98_handle(fs98_host_rt_message_help);
        buffer[9] = 0;
        WIN(MessageBoxIndirectA, int32_t, (const void *))(buffer);
        return 0;
    }
    show_logo = 1;
    SERVICE(0x9c, int32_t, (const char *, uint32_t *))(STR(0x4170dc), &show_logo);
    if (WIN(GetPrivateProfileIntA, uint32_t, (const char *, const char *, int32_t, const char *))(
            STR(0x417074), STR(0x4170dc), 1, CFG) && show_logo)
        fs98_host_rt_show_logo(0);
    G32(0x418864) = 0;
    WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(MAIN_WINDOW, 0x30f, 0, 0);
    G32(0x418864) = 1;
    fs98_host_rt_sound_setup();
    fs98_host_rt_open_directdraw();
    SERVICE(0x50, int32_t, (uint32_t))(1);
    fs98_host_rt_find_modules();
    G32(0x4187d8) = 0;
    G_PTR(0x4187dc) = (void *)fs98_host_rt_401f60;
    G32(0x4187e0) = 1;
    SERVICE(0x110, int32_t, (uint32_t, void *))(1, VA(0x4187d8));
    if (WIN(GetPrivateProfileIntA, uint32_t, (const char *, const char *, int32_t, const char *))(
            STR(0x417074), STR(0x4170cc), -1, CFG) == 0xffffffffu)
        WIN(WritePrivateProfileStringA, int32_t, (const char *, const char *, const char *, const char *))(
            STR(0x417074), STR(0x4170cc), STR(0x417064), CFG);
    fs98_host_rt_start_modules();
    if (G32(0x4187e8)) {
        int32_t setting, speed;
        WIN(GetSystemInfo, void, (void *))(buffer);
        if (buffer[6] == 0x1e6)
            setting = 1;
        else {
            speed = fs98_host_rt_cpu_speed();
            setting = speed == 0 ? 6 : speed < 0x50 ? 3 : speed < 0x6e ? 6 : speed < 0xbe ? 8 : 10;
        }
        ((void (SC *)(int32_t))G_PTR(0x418844))(setting);
    }
    ((void (*)(void))G_PTR(0x41883c))();
    G32(0x4187e0) = 0;
    SERVICE(0x114, int32_t, (void))();
    SERVICE(0x114, int32_t, (void))();
    G32(0x418864) = 0;
    buffer[0] = 0x2c;
    WIN(GetWindowPlacement, int32_t, (uint32_t, void *))(MAIN_WINDOW, buffer);
    if (WIN(GetPrivateProfileIntA, uint32_t, (const char *, const char *, int32_t, const char *))(
            STR(0x417074), STR(0x417058), 0, CFG))
        buffer[2] = 3;
    else
        buffer[2] = G32(0x418640);
    buffer[7] = G32(0x41c590);
    buffer[8] = G32(0x41c594);
    buffer[9] = G32(0x41c598);
    buffer[10] = G32(0x41c59c);
    WIN(SetWindowPlacement, int32_t, (uint32_t, const void *))(MAIN_WINDOW, buffer);
    if (WIN(GetPrivateProfileIntA, uint32_t, (const char *, const char *, int32_t, const char *))(
            STR(0x4170b8), STR(0x4170c0), 0, CFG) && G32(0x418868) == 0)
        fs98_host_rt_full_screen_on();
    WIN(UpdateWindow, int32_t, (uint32_t))(MAIN_WINDOW);
    if (WIN(timeGetDevCaps, uint32_t, (void *, uint32_t))(caps, 8) == 0) {
        uint32_t period = caps[0] > 0x32 ? caps[0] : 0x32;
        if (period >= caps[1])
            period = caps[1];
        G32(0x418814) = period;
        if (period == 0x32) {
            WIN(timeBeginPeriod, uint32_t, (uint32_t))(0x32);
            G32(0x418818) = WIN(timeSetEvent, uint32_t, (uint32_t, uint32_t, void *, uint32_t, uint32_t))(
                G32(0x418814), 0x14, (void *)fs98_host_rt_timer, 0, 1);
            WIN(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(MAIN_WINDOW, 0x46b, 0, 0);
            return 1;
        }
    }
    (void)CALL_THIS(MFC_2463, uint32_t, (void *))(G_PTR(0x4185e8));
    VIRTUAL(G_PTR(0x4185e8), 0x60, int32_t, (void *))(G_PTR(0x4185e8));
    MAIN_WINDOW = 0;
    return 0;
}

/* 0x402050: ExitInstance: the main window detached and destroyed (vtable +0x60), the modules
 * stopped and everything let go -- if there was a main window; then the end (0x401850). 0. */
int32_t fs98_host_rt_exit_instance(void)
{
    if (MAIN_WINDOW) {
        (void)CALL_THIS(MFC_2463, uint32_t, (void *))(G_PTR(0x4185e8));
        VIRTUAL(G_PTR(0x4185e8), 0x60, int32_t, (void *))(G_PTR(0x4185e8));
        MAIN_WINDOW = 0;
        fs98_host_rt_stop_modules();
        fs98_host_rt_409c80();
        fs98_host_rt_40cc10();
        fs98_host_rt_40ee40();
        fs98_host_rt_410400();
        fs98_host_rt_407910();
        fs98_host_rt_release_directdraw();
        fs98_host_rt_sound_end();
        fs98_host_rt_pause_look(0);
    }
    fs98_host_rt_shut_down();
    return 0;
}

/* 0x4020b0: Run's loop. The messages waiting (0x4185fc) handled: WM_QUIT ends it (the cursor
 * let go, +0x278, and freed); Enter pressed (not held) in the foreground main window becomes
 * its command 0x1501; the modeless dialogs (0x409c60) and the accelerators (+0x108) get a
 * look, the rest are dispatched. Between them, the sound ticked (0x40f510) and, unless asked
 * to wait (+0x20 with under 32ms gone: 20ms asleep), the game's frame callback (0x418830),
 * inside a full-screen frame when there is one. 0. */
int32_t fs98_host_rt_run(void)
{
    uint32_t *message = (uint32_t *)VA(0x4185fc), now, gone;
    int32_t (SC *peek)(void *, uint32_t, uint32_t, uint32_t, uint32_t) =
        (int32_t (SC *)(void *, uint32_t, uint32_t, uint32_t, uint32_t))IMPORT(PeekMessageA);
    for (;;) {
        while (peek(message, 0, 0, 0, 1)) {
            if (G32(0x418600) == 0x12) {
                SERVICE(0x278, int32_t, (void))();
                WIN(ClipCursor, int32_t, (const void *))(0);
                return 0;
            }
            if (G32(0x418600) == 0x104 && G32(0x418604) == 0xd && !((G32(0x418608) >> 16) & 0x4000) &&
                WIN(GetForegroundWindow, uint32_t, (void))() == MAIN_WINDOW)
                WIN(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(MAIN_WINDOW, 0x111, 0x1501, 0);
            if (fs98_host_rt_modeless_took(message) == 0 &&
                SERVICE(0x108, int32_t, (uint32_t, void *))(MAIN_WINDOW, message) == 0) {
                WIN(TranslateMessage, int32_t, (const void *))(message);
                WIN(DispatchMessageA, int32_t, (const void *))(message);
            }
        }
        fs98_host_rt_sound_tick();
        if (fs98_host_rt_loop_pass)     /* the executable's test hook, before the clock is read */
            fs98_host_rt_loop_pass();
        now = WIN(timeGetTime, uint32_t, (void))();
        gone = now - G32(0x4187f4);
        G32(0x418850) = gone;
        if (G32(0x41884c)) {
            G32(0x41884c) = 0;
            gone = 0;
            G32(0x418850) = 0;
        }
        if (gone != 0 && SERVICE(0x20, int32_t, (void))() == 0 && G32(0x418850) < 0x20) {
            WIN(Sleep, void, (uint32_t))(0x14);
            continue;
        }
        if (G_PTR(0x418830) == 0)
            continue;
        if (G32(0x41a4f4))
            fs98_host_rt_begin_frame();
        ((void (*)(void))G_PTR(0x418830))();
        if (G32(0x41a4f4))
            fs98_host_rt_end_frame();
        G32(0x4187f4) = now;
    }
}

/* ---- the timer, the main window's placement, the clip ----------------------------------- */

/* 0x401950: the multimedia timer's callback, every 50ms: the tick counted (0x4187f0); unless
 * the game is paused (0x4187ec) or a menu is up (0x418848), the game's timer callback
 * (0x418838) on nine ticks in ten, and the repeating message (0x41c5a0: window, message,
 * wParam, lParam at 0x41c5a4..ac) posted every 0x41c224 ticks after a delay of 0x41bfe4. */
void SC fs98_host_rt_timer(uint32_t id, uint32_t message, uint32_t user, uint32_t one, uint32_t two)
{
    void (*callback)(void);
    (void)id; (void)message; (void)user; (void)one; (void)two;
    ++G32(0x4187f0);
    callback = (void (*)(void))G_PTR(0x418838);
    if (callback == 0 || G32(0x4187ec) || G32(0x418848))
        return;
    if (GI32(0x4187f0) % 10 != 0)
        callback();
    if (G32(0x41c5a0) == 0)
        return;
    if (G32(0x41bfe4)) {
        --G32(0x41bfe4);
        G32(0x417020) = G32(0x41c224);
        return;
    }
    if (G32(0x417020)-- == 0) {
        WIN(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(G32(0x41c5a0), G32(0x41c5a4), G32(0x41c5a8),
                                                                             G32(0x41c5ac));
        G32(0x417020) = G32(0x41c224);
    }
}

/* 0x4026c0: a window's place saved ([MAIN] Location= its normal rectangle, Maximized= whether
 * it restores maximised), when it is visible and the game is neither in full screen
 * (0x418854) nor placing it (0x418864). */
void SC fs98_host_rt_save_placement(uint32_t window)
{
    uint32_t placement[11];
    if (G32(0x418854) || !WIN(IsWindowVisible, int32_t, (uint32_t))(window) || G32(0x418864))
        return;
    placement[0] = 0x2c;
    WIN(GetWindowPlacement, int32_t, (uint32_t, void *))(window, placement);
    WIN(wsprintfA, int32_t, (char *, const char *, ...))((char *)VA(0x41c480), STR(0x417068), placement[7], placement[8],
                                                         placement[9], placement[10]);
    WIN(WritePrivateProfileStringA, int32_t, (const char *, const char *, const char *, const char *))(
        STR(0x417074), STR(0x41707c), STR(0x41c480), CFG);
    WIN(WritePrivateProfileStringA, int32_t, (const char *, const char *, const char *, const char *))(
        STR(0x417074), STR(0x417058), STR(placement[1] & 2 ? 0x417064 : 0x417a20), CFG);
}

/* 0x402790: whether the main window is covered at all (its clip box not one simple
 * rectangle), into 0x41719c (+0x20's answer) and returned. */
uint32_t fs98_host_rt_covered(void)
{
    int32_t box[4];
    uint32_t dc = WIN(GetDCEx, uint32_t, (uint32_t, uint32_t, uint32_t))(MAIN_WINDOW, 0, 0x12);
    int32_t kind = WIN(GetClipBox, int32_t, (uint32_t, int32_t *))(dc, box);
    WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, dc);
    G32(0x41719c) = kind != 1;
    return G32(0x41719c);
}

/* 0x4025b0 and 0x4025c0: nothing (one argument for the second). */
void fs98_host_rt_4025b0(void)
{
}
void SC fs98_host_rt_4025c0(uint32_t unused)
{
    (void)unused;
}
