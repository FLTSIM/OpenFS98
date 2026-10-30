/* Read-only/control probe for the private MP_DIALOG owner-integration Wine run. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DWORD g_process_id;
static HWND g_main_window;
static HWND g_cancel_candidate;
static DWORD g_cancel_candidate_rank;
static unsigned g_visible_page_count;
static DWORD g_target_process_id;

static BOOL CALLBACK dump_control(HWND window, LPARAM parameter)
{
    char title[256] = {0};
    char class_name[128] = {0};
    RECT rect;
    (void)parameter;
    GetWindowTextA(window, title, sizeof(title));
    GetClassNameA(window, class_name, sizeof(class_name));
    GetWindowRect(window, &rect);
    printf("CHILD hwnd=%p parent=%p id=%u class=%s title=%s style=%08lx ex=%08lx rect=%ld,%ld,%ld,%ld visible=%u enabled=%u\n",
        window, GetParent(window), (unsigned)(UINT_PTR)GetDlgCtrlID(window),
        class_name, title, (unsigned long)GetWindowLongA(window, GWL_STYLE),
        (unsigned long)GetWindowLongA(window, GWL_EXSTYLE),
        rect.left, rect.top, rect.right, rect.bottom,
        (unsigned)IsWindowVisible(window), (unsigned)IsWindowEnabled(window));
    return TRUE;
}

static BOOL CALLBACK dump_top_level(HWND window, LPARAM parameter)
{
    char title[256] = {0};
    char class_name[128] = {0};
    DWORD process_id = 0;
    RECT rect;
    (void)parameter;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != g_process_id) return TRUE;
    GetWindowTextA(window, title, sizeof(title));
    GetClassNameA(window, class_name, sizeof(class_name));
    GetWindowRect(window, &rect);
    printf("TOP hwnd=%p owner=%p id=%u class=%s title=%s style=%08lx ex=%08lx rect=%ld,%ld,%ld,%ld visible=%u enabled=%u active=%u foreground=%u\n",
        window, GetWindow(window, GW_OWNER), (unsigned)(UINT_PTR)GetDlgCtrlID(window),
        class_name, title, (unsigned long)GetWindowLongA(window, GWL_STYLE),
        (unsigned long)GetWindowLongA(window, GWL_EXSTYLE),
        rect.left, rect.top, rect.right, rect.bottom,
        (unsigned)IsWindowVisible(window), (unsigned)IsWindowEnabled(window),
        (unsigned)(GetActiveWindow() == window),
        (unsigned)(GetForegroundWindow() == window));
    EnumChildWindows(window, dump_control, 0);
    return TRUE;
}

static BOOL CALLBACK find_target_main(HWND window, LPARAM parameter)
{
    char title[256] = {0};
    char class_name[128] = {0};
    DWORD process_id = 0;
    HWND *result = (HWND *)parameter;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != g_target_process_id) return TRUE;
    GetWindowTextA(window, title, sizeof(title));
    GetClassNameA(window, class_name, sizeof(class_name));
    if (!lstrcmpiA(class_name, "FS98MAIN") &&
        !lstrcmpiA(title, "Microsoft Flight Simulator 98")) {
        *result = window;
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK find_visible_page(HWND window, LPARAM parameter)
{
    char class_name[128] = {0};
    DWORD process_id = 0;
    DWORD style = (DWORD)GetWindowLongA(window, GWL_STYLE);
    (void)parameter;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != g_process_id || GetWindow(window, GW_OWNER) != g_main_window)
        return TRUE;
    GetClassNameA(window, class_name, sizeof(class_name));
    if (lstrcmpiA(class_name, "#32770") == 0 && IsWindowVisible(window)) {
        DWORD rank = IsWindowEnabled(window) ? 2 : 1;
        if (GetParent(window) == g_main_window) ++rank;
        printf("VISIBLE_PAGE hwnd=%p parent=%p owner=%p style=%08lx enabled=%u active=%u\n",
            window, GetParent(window), GetWindow(window, GW_OWNER),
            (unsigned long)style, (unsigned)IsWindowEnabled(window),
            (unsigned)(GetActiveWindow() == window));
        ++g_visible_page_count;
        if (!g_cancel_candidate || rank >= g_cancel_candidate_rank) {
            g_cancel_candidate = window;
            g_cancel_candidate_rank = rank;
        }
    }
    return TRUE;
}

static unsigned count_visible_pages(void)
{
    HWND prior = g_cancel_candidate;
    DWORD prior_rank = g_cancel_candidate_rank;
    g_cancel_candidate = NULL;
    g_cancel_candidate_rank = 0;
    g_visible_page_count = 0;
    EnumWindows(find_visible_page, 0);
    {
        unsigned count = g_visible_page_count;
        g_visible_page_count = 0;
        g_cancel_candidate = prior;
        g_cancel_candidate_rank = prior_rank;
        return count;
    }
}

static HWND choose_cancel_target(void)
{
    HWND popup = GetLastActivePopup(g_main_window);
    DWORD process_id = 0;
    char class_name[128] = {0};
    g_cancel_candidate = NULL;
    g_cancel_candidate_rank = 0;
    g_visible_page_count = 0;
    EnumWindows(find_visible_page, 0);
    if (popup && popup != g_main_window) {
        GetWindowThreadProcessId(popup, &process_id);
        GetClassNameA(popup, class_name, sizeof(class_name));
        printf("LAST_ACTIVE_POPUP hwnd=%p pid=%lu class=%s visible=%u enabled=%u\n",
            popup, (unsigned long)process_id, class_name,
            (unsigned)IsWindowVisible(popup), (unsigned)IsWindowEnabled(popup));
        if (process_id == g_process_id && IsWindowVisible(popup) &&
            lstrcmpiA(class_name, "#32770") == 0) return popup;
    }
    return g_cancel_candidate;
}

static int run_connect_cancel(unsigned timeout_seconds, unsigned delay_ms,
                              DWORD target_process_id)
{
    typedef LRESULT (CALLBACK *hook_proc_fn)(int, WPARAM, LPARAM);
    HMODULE hook_module;
    hook_proc_fn cbt_proc, callwnd_proc, getmsg_proc;
    HHOOK cbt_hook = NULL, callwnd_hook = NULL, getmsg_hook = NULL;
    DWORD thread_id = 0, elapsed_ms = 0, modal_elapsed = 0;
    DWORD no_page_since = 0;
    BOOL saw_modal = FALSE, canceled = FALSE, returned = FALSE;
    int result = 1;
    HWND main_window = NULL;
    HWND found_window = NULL;
    DWORD start;
    const UINT quiesce_message = WM_APP + 0x384;

    (void)freopen("Z:\\tmp\\mpdialog-host-probe.log", "w", stdout);
    g_target_process_id = target_process_id;
    start = GetTickCount();
    while (!found_window && GetTickCount() - start < timeout_seconds * 1000u) {
        found_window = NULL;
        EnumWindows(find_target_main, (LPARAM)&found_window);
        if (!found_window) Sleep(100);
    }
    main_window = found_window;
    if (!main_window) {
        printf("ERROR target game HWND not found pid=%lu before %u second deadline\n",
            (unsigned long)target_process_id, timeout_seconds);
        return 1;
    }
    g_main_window = main_window;
    thread_id = GetWindowThreadProcessId(main_window, &g_process_id);
    printf("CONNECT_CANCEL BEGIN main=%p pid=%lu tid=%lu timeout=%u delay_ms=%u\n",
        main_window, (unsigned long)g_process_id, (unsigned long)thread_id,
        timeout_seconds, delay_ms);

    {
        HANDLE trace = CreateFileA("Z:\\tmp\\mpdialog-host-integration.log",
            GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (trace == INVALID_HANDLE_VALUE) {
            printf("ERROR truncating integration trace error=%lu\n",
                (unsigned long)GetLastError());
            return 2;
        }
        CloseHandle(trace);
    }

    hook_module = LoadLibraryA("Z:\\tmp\\mpdialoghosthook.dll");
    if (!hook_module) {
        printf("ERROR loading hook DLL error=%lu\n", (unsigned long)GetLastError());
        return 2;
    }
    cbt_proc = (hook_proc_fn)GetProcAddress(hook_module, "cbt_hook");
    callwnd_proc = (hook_proc_fn)GetProcAddress(hook_module, "callwnd_hook");
    getmsg_proc = (hook_proc_fn)GetProcAddress(hook_module, "getmsg_hook");
    if (!cbt_proc || !callwnd_proc || !getmsg_proc) {
        printf("ERROR hook exports missing cbt=%p callwnd=%p getmsg=%p\n",
            (void *)cbt_proc, (void *)callwnd_proc, (void *)getmsg_proc);
        goto cleanup;
    }
    cbt_hook = SetWindowsHookExA(WH_CBT, cbt_proc, hook_module, thread_id);
    callwnd_hook = SetWindowsHookExA(WH_CALLWNDPROC, callwnd_proc,
        hook_module, thread_id);
    getmsg_hook = SetWindowsHookExA(WH_GETMESSAGE, getmsg_proc,
        hook_module, thread_id);
    printf("HOOKS cbt=%p callwnd=%p getmsg=%p last_error=%lu\n",
        cbt_hook, callwnd_hook, getmsg_hook, (unsigned long)GetLastError());
    if (!cbt_hook || !callwnd_hook || !getmsg_hook) goto cleanup;

    SetLastError(ERROR_SUCCESS);
    if (!PostMessageA(main_window, WM_COMMAND, MAKEWPARAM(0x1180, 0), 0)) {
        printf("ERROR posting Connect error=%lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }
    printf("POST CONNECT hwnd=%p msg=WM_COMMAND id=1180\n", main_window);
    start = GetTickCount();
    while (GetTickCount() - start < timeout_seconds * 1000u) {
        DWORD now = GetTickCount();
        BOOL modal_now = !IsWindowEnabled(main_window);
        elapsed_ms = now - start;
        if (modal_now && !saw_modal) {
            saw_modal = TRUE;
            modal_elapsed = elapsed_ms;
            printf("HOST MODAL OPEN elapsed_ms=%lu main_enabled=0\n",
                (unsigned long)elapsed_ms);
            EnumWindows(dump_top_level, 0);
        }
        if (saw_modal && !canceled && elapsed_ms - modal_elapsed >= delay_ms) {
            HWND target = choose_cancel_target();
            if (!target) {
                if (!no_page_since) no_page_since = elapsed_ms;
                if (elapsed_ms - no_page_since >= 5000) {
                    printf("ERROR no visible owned page within 5000 ms of modal entry\n");
                    break;
                }
            } else {
                printf("POST CANCEL target=%p message=WM_COMMAND id=0002 elapsed_ms=%lu\n",
                    target, (unsigned long)elapsed_ms);
                SetLastError(ERROR_SUCCESS);
                if (!PostMessageA(target, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0))
                    printf("ERROR posting cancel error=%lu\n", (unsigned long)GetLastError());
                else
                    canceled = TRUE;
            }
        }
        if (canceled && saw_modal && IsWindowEnabled(main_window) &&
            count_visible_pages() == 0) {
            Sleep(500);
            if (IsWindowEnabled(main_window) && count_visible_pages() == 0) {
                printf("HOST MODAL RETURN elapsed_ms=%lu main_enabled=1 visible_pages=0\n",
                    (unsigned long)(GetTickCount() - start));
                returned = TRUE;
                result = 0;
                break;
            }
        }
        Sleep(20);
    }
    if (!saw_modal)
        printf("TIMEOUT Connect did not enter disabled-owner modal; elapsed_ms=%lu\n",
            (unsigned long)(GetTickCount() - start));
    else if (!returned)
        printf("TIMEOUT modal did not return after cancel=%u elapsed_ms=%lu main_enabled=%u visible_pages=%u\n",
            (unsigned)canceled, (unsigned long)(GetTickCount() - start),
            (unsigned)IsWindowEnabled(main_window), count_visible_pages());
    if (returned) {
        PostMessageA(main_window, quiesce_message, 0, 0);
        printf("POST HOOK_QUIESCE hwnd=%p msg=%04x\n", main_window,
            quiesce_message);
        Sleep(750);
    }
cleanup:
    if (getmsg_hook) UnhookWindowsHookEx(getmsg_hook);
    if (callwnd_hook) UnhookWindowsHookEx(callwnd_hook);
    if (cbt_hook) UnhookWindowsHookEx(cbt_hook);
    if (hook_module) FreeLibrary(hook_module);
    printf("CONNECT_CANCEL END result=%d returned=%u elapsed_ms=%lu\n",
        result, (unsigned)returned, (unsigned long)(GetTickCount() - start));
    fflush(stdout);
    return result;
}

static int launch_connect_cancel(unsigned timeout_seconds, unsigned delay_ms)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    BOOL created;
    int result;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNORMAL;
    created = CreateProcessA("C:\\FS98\\FLTSIM98.EXE", NULL, NULL, NULL,
        FALSE, 0, NULL, "C:\\FS98", &startup, &process);
    if (!created) {
        (void)freopen("Z:\\tmp\\mpdialog-host-probe.log", "w", stdout);
        printf("ERROR CreateProcess FLTSIM98.EXE error=%lu\n",
            (unsigned long)GetLastError());
        return 2;
    }
    (void)freopen("Z:\\tmp\\mpdialog-host-probe.log", "w", stdout);
    printf("LAUNCHED isolated game pid=%lu tid=%lu\n",
        (unsigned long)process.dwProcessId, (unsigned long)process.dwThreadId);
    fflush(stdout);
    result = run_connect_cancel(timeout_seconds, delay_ms,
        process.dwProcessId);
    if (result != 0 && WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
        printf("CLEANUP terminating only launched test PID=%lu after failed run\n",
            (unsigned long)process.dwProcessId);
        TerminateProcess(process.hProcess, (UINT)result);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

int main(int argc, char **argv)
{
    HWND main_window = FindWindowA(NULL, "Microsoft Flight Simulator 98");
    HWND active_popup;
    DWORD thread_id;
    if (argc > 1 && !strcmp(argv[1], "launch-connect-cancel")) {
        unsigned timeout_seconds = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 0) : 45;
        unsigned delay_ms = argc > 3 ? (unsigned)strtoul(argv[3], NULL, 0) : 1500;
        if (timeout_seconds < 5 || timeout_seconds > 180 || delay_ms > 30000) {
            (void)freopen("Z:\\tmp\\mpdialog-host-probe.log", "w", stdout);
            printf("ERROR launch-connect-cancel bounds: timeout 5..180, delay 0..30000\n");
            return 2;
        }
        return launch_connect_cancel(timeout_seconds, delay_ms);
    }
    (void)freopen("Z:\\tmp\\mpdialog-host-probe.log", "w", stdout);
    if (!main_window) {
        printf("ERROR game window not found\n");
        return 1;
    }
    thread_id = GetWindowThreadProcessId(main_window, &g_process_id);
    active_popup = GetLastActivePopup(main_window);
    printf("GAME hwnd=%p pid=%lu tid=%lu active=%p foreground=%p last_popup=%p\n",
        main_window, (unsigned long)g_process_id, (unsigned long)thread_id,
        GetActiveWindow(), GetForegroundWindow(), active_popup);
    if (argc > 1 && !strcmp(argv[1], "dump")) {
        EnumWindows(dump_top_level, 0);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "main")) {
        UINT command = (UINT)strtoul(argv[2], NULL, 0);
        printf("POST main WM_COMMAND id=%04x\n", command);
        PostMessageA(main_window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[1], "post")) {
        HWND target = (HWND)(UINT_PTR)strtoul(argv[2], NULL, 0);
        UINT command = (UINT)strtoul(argv[3], NULL, 0);
        BOOL posted;
        DWORD error;
        SetLastError(ERROR_SUCCESS);
        posted = PostMessageA(target, WM_COMMAND, MAKEWPARAM(command, 0), 0);
        error = GetLastError();
        printf("POST target=%p WM_COMMAND id=%04x result=%u error=%lu\n",
            target, command, (unsigned)posted, (unsigned long)error);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[1], "click")) {
        HWND target = (HWND)(UINT_PTR)strtoul(argv[2], NULL, 0);
        UINT control_id = (UINT)strtoul(argv[3], NULL, 0);
        HWND control = GetDlgItem(target, (int)control_id);
        LRESULT result = 0;
        DWORD error;
        SetLastError(ERROR_SUCCESS);
        if (control) result = SendMessageA(control, BM_CLICK, 0, 0);
        error = GetLastError();
        printf("CLICK target=%p control=%p id=%04x result=%ld error=%lu\n",
            target, control, control_id, (long)result, (unsigned long)error);
        return control ? 0 : 2;
    }
    if (argc > 1 && !strcmp(argv[1], "cancel")) {
        BOOL posted;
        DWORD error;
        SetLastError(ERROR_SUCCESS);
        posted = PostMessageA(active_popup, WM_COMMAND,
                              MAKEWPARAM(IDCANCEL, 0), 0);
        error = GetLastError();
        printf("POST last_popup=%p WM_COMMAND IDCANCEL result=%u error=%lu\n",
            active_popup, (unsigned)posted, (unsigned long)error);
        return 0;
    }
    printf("USAGE dump | main ID | post HWND ID | click HWND CONTROL_ID | cancel | launch-connect-cancel TIMEOUT_SECONDS DELAY_MS\n");
    return 2;
}
