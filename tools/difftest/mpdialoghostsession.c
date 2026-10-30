/* Bounded actual HOST/MULTIP local-session attempt for private Wine copies. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DWORD g_target_pid;
static DWORD g_game_pid;
static HWND g_main;

struct find_window_context {
    UINT control_id;
    BOOL visible_only;
    HWND found;
};

static BOOL CALLBACK find_game_main(HWND window, LPARAM parameter)
{
    HWND *result = (HWND *)parameter;
    char class_name[64] = {0};
    char title[128] = {0};
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != g_target_pid) return TRUE;
    GetClassNameA(window, class_name, sizeof(class_name));
    GetWindowTextA(window, title, sizeof(title));
    if (!lstrcmpiA(class_name, "FS98MAIN") &&
        !lstrcmpiA(title, "Microsoft Flight Simulator 98")) {
        *result = window;
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK find_dialog_callback(HWND window, LPARAM parameter)
{
    struct find_window_context *context = (struct find_window_context *)parameter;
    char class_name[64] = {0};
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != g_game_pid) return TRUE;
    GetClassNameA(window, class_name, sizeof(class_name));
    if (lstrcmpiA(class_name, "#32770") != 0) return TRUE;
    if (context->visible_only && !IsWindowVisible(window)) return TRUE;
    if (context->control_id && !GetDlgItem(window, context->control_id)) return TRUE;
    context->found = window;
    return FALSE;
}

static HWND find_dialog(UINT control_id, BOOL visible_only)
{
    struct find_window_context context;
    context.control_id = control_id;
    context.visible_only = visible_only;
    context.found = NULL;
    EnumWindows(find_dialog_callback, (LPARAM)&context);
    return context.found;
}

static HWND wait_dialog(UINT control_id, DWORD timeout_ms)
{
    DWORD start = GetTickCount();
    HWND window = NULL;
    while (!window && GetTickCount() - start < timeout_ms) {
        window = find_dialog(control_id, TRUE);
        if (!window) Sleep(50);
    }
    return window;
}

static BOOL CALLBACK dump_child(HWND window, LPARAM parameter);

static BOOL click_control(HWND dialog, UINT control_id)
{
    HWND control = GetDlgItem(dialog, control_id);
    DWORD_PTR result = 0;
    if (!control) {
        printf("CONTROL missing dialog=%p id=%u\n", dialog, control_id);
        return FALSE;
    }
    printf("CLICK dialog=%p id=%u hwnd=%p enabled=%u visible=%u\n",
        dialog, control_id, control, (unsigned)IsWindowEnabled(control),
        (unsigned)IsWindowVisible(control));
    if (!IsWindowEnabled(control)) return FALSE;
    SetLastError(ERROR_SUCCESS);
    if (!SendMessageTimeoutA(control, BM_CLICK, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result)) {
        printf("CLICK error dialog=%p id=%u winerr=%lu\n", dialog,
            control_id, (unsigned long)GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL set_control_text(HWND dialog, UINT control_id, const char *text)
{
    HWND control = GetDlgItem(dialog, control_id);
    DWORD_PTR result = 0;
    if (!control) return FALSE;
    SetLastError(ERROR_SUCCESS);
    if (!SendMessageTimeoutA(control, WM_SETTEXT, 0, (LPARAM)text,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result)) {
        printf("SETTEXT error dialog=%p id=%u winerr=%lu\n", dialog,
            control_id, (unsigned long)GetLastError());
        return FALSE;
    }
    printf("SETTEXT dialog=%p id=%u value=%s result=%lu\n", dialog,
        control_id, text, (unsigned long)result);
    return TRUE;
}

static void dump_dialog(HWND dialog, const char *stage)
{
    char title[128] = {0};
    char class_name[64] = {0};
    GetWindowTextA(dialog, title, sizeof(title));
    GetClassNameA(dialog, class_name, sizeof(class_name));
    printf("DIALOG stage=%s hwnd=%p class=%s title=%s style=%08lx visible=%u enabled=%u owner=%p\n",
        stage, dialog, class_name, title,
        (unsigned long)GetWindowLongA(dialog, GWL_STYLE),
        (unsigned)IsWindowVisible(dialog), (unsigned)IsWindowEnabled(dialog),
        GetWindow(dialog, GW_OWNER));
    EnumChildWindows(dialog, dump_child, (LPARAM)stage);
}

static BOOL CALLBACK dump_child(HWND window, LPARAM parameter)
{
    char title[256] = {0};
    char class_name[96] = {0};
    const char *stage = (const char *)parameter;
    GetWindowTextA(window, title, sizeof(title));
    GetClassNameA(window, class_name, sizeof(class_name));
    printf("CHILD stage=%s hwnd=%p id=%u class=%s text=%s enabled=%u visible=%u style=%08lx\n",
        stage ? stage : "?", window,
        (unsigned)(UINT_PTR)GetDlgCtrlID(window), class_name, title,
        (unsigned)IsWindowEnabled(window), (unsigned)IsWindowVisible(window),
        (unsigned long)GetWindowLongA(window, GWL_STYLE));
    return TRUE;
}

static int run_session_attempt(DWORD timeout_seconds, DWORD target_pid)
{
    typedef LRESULT (CALLBACK *hook_proc_fn)(int, WPARAM, LPARAM);
    HMODULE hook_module = NULL;
    hook_proc_fn cbt_proc, callwnd_proc, getmsg_proc;
    HHOOK cbt_hook = NULL, callwnd_hook = NULL, getmsg_hook = NULL;
    HWND found = NULL, intro = NULL, choice = NULL, host = NULL, connection = NULL;
    HWND settings = NULL, details = NULL;
    DWORD thread_id, start, modal_start = 0;
    int stage = 0;
    BOOL saw_modal = FALSE, flow_returned = FALSE, reached_details = FALSE;
    BOOL returned_to_game = FALSE;
    int result = 1;

    (void)freopen("Z:\\tmp\\mpdialog-host-session.log", "w", stdout);
    g_target_pid = target_pid;
    start = GetTickCount();
    while (!found && GetTickCount() - start < timeout_seconds * 1000u) {
        EnumWindows(find_game_main, (LPARAM)&found);
        if (!found) Sleep(100);
    }
    if (!found) {
        printf("ERROR isolated game main window not found pid=%lu timeout=%lu\n",
            (unsigned long)target_pid, (unsigned long)timeout_seconds);
        return 1;
    }
    g_main = found;
    thread_id = GetWindowThreadProcessId(g_main, &g_game_pid);
    printf("LOCAL_HOST_BEGIN main=%p pid=%lu tid=%lu timeout=%lu\n",
        g_main, (unsigned long)g_game_pid, (unsigned long)thread_id,
        (unsigned long)timeout_seconds);

    {
        HANDLE trace = CreateFileA("Z:\\tmp\\mpdialog-host-integration.log",
            GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (trace == INVALID_HANDLE_VALUE) {
            printf("ERROR truncate hook trace winerr=%lu\n",
                (unsigned long)GetLastError());
            return 2;
        }
        CloseHandle(trace);
    }
    hook_module = LoadLibraryA("Z:\\tmp\\mpdialoghosthook.dll");
    if (!hook_module) {
        printf("ERROR load hook DLL winerr=%lu\n", (unsigned long)GetLastError());
        return 2;
    }
    cbt_proc = (hook_proc_fn)GetProcAddress(hook_module, "cbt_hook");
    callwnd_proc = (hook_proc_fn)GetProcAddress(hook_module, "callwnd_hook");
    getmsg_proc = (hook_proc_fn)GetProcAddress(hook_module, "getmsg_hook");
    if (!cbt_proc || !callwnd_proc || !getmsg_proc) {
        printf("ERROR hook exports missing\n");
        goto cleanup;
    }
    cbt_hook = SetWindowsHookExA(WH_CBT, cbt_proc, hook_module, thread_id);
    callwnd_hook = SetWindowsHookExA(WH_CALLWNDPROC, callwnd_proc,
        hook_module, thread_id);
    getmsg_hook = SetWindowsHookExA(WH_GETMESSAGE, getmsg_proc,
        hook_module, thread_id);
    printf("HOOKS cbt=%p callwnd=%p getmsg=%p error=%lu\n",
        cbt_hook, callwnd_hook, getmsg_hook, (unsigned long)GetLastError());
    if (!cbt_hook || !callwnd_hook || !getmsg_hook) goto cleanup;

    if (!PostMessageA(g_main, WM_COMMAND, MAKEWPARAM(0x1180, 0), 0)) {
        printf("ERROR post Connect winerr=%lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }
    printf("POST Connect id=0x1180\n");
    while (GetTickCount() - start < timeout_seconds * 1000u) {
        DWORD elapsed = GetTickCount() - start;
        if (!saw_modal && !IsWindowEnabled(g_main)) {
            saw_modal = TRUE;
            modal_start = elapsed;
            printf("HOST_MODAL_OPEN elapsed_ms=%lu\n", (unsigned long)elapsed);
        }
        if (saw_modal && stage == 0 && elapsed - modal_start > 500) {
            intro = wait_dialog(1000, 1000);
            if (!intro || !IsWindowVisible(intro)) {
                printf("BLOCKED intro Next page not visible\n");
                break;
            }
            dump_dialog(intro, "intro");
            if (!click_control(intro, 1000)) break;
            stage = 1;
            printf("FLOW_ACTION Intro.Next\n");
        } else if (stage == 1) {
            choice = wait_dialog(1049, 500);
            if (choice && IsWindowVisible(choice)) {
                if (!set_control_text(choice, 1049, "OpenFS Local Pilot")) break;
                if (!click_control(choice, 1054)) break;
                Sleep(200);
                {
                    HWND host_radio = GetDlgItem(choice, 1054);
                    DWORD_PTR check = 0;
                    if (host_radio) SendMessageTimeoutA(host_radio, BM_GETCHECK,
                        0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &check);
                    printf("CHOICE host_radio_check=%lu\n", (unsigned long)check);
                }
                if (!click_control(choice, 1000)) break;
                stage = 2;
                printf("FLOW_ACTION Choice.Host.Next\n");
            }
        } else if (stage == 2) {
            host = wait_dialog(1050, 500);
            if (host && IsWindowVisible(host)) {
                char current[128] = {0};
                HWND name = GetDlgItem(host, 1050);
                if (name) GetWindowTextA(name, current, sizeof(current));
                printf("HOST_OPTIONS initial_name=%s\n", current);
                if (!current[0]) set_control_text(host, 1050, "OpenFS Local Session");
                dump_dialog(host, "host-options");
                if (!click_control(host, 1000)) break;
                stage = 3;
                printf("FLOW_ACTION HostOptions.Next\n");
            }
        } else if (stage == 3) {
            connection = wait_dialog(1051, 500);
            if (connection && IsWindowVisible(connection)) {
                HWND list = GetDlgItem(connection, 1051);
                HWND host_button = GetDlgItem(connection, 1075);
                DWORD_PTR count = 0, selection = 0;
                char class_name[64] = {0};
                if (list) {
                    GetClassNameA(list, class_name, sizeof(class_name));
                    SendMessageTimeoutA(list, LB_GETCOUNT, 0, 0,
                        SMTO_ABORTIFHUNG | SMTO_BLOCK, 1500, &count);
                    SendMessageTimeoutA(list, LB_GETCURSEL, 0, 0,
                        SMTO_ABORTIFHUNG | SMTO_BLOCK, 1500, &selection);
                }
                printf("CONNECTION provider_control=%p class=%s count=%ld selected=%ld Host_enabled=%u Host_visible=%u\n",
                    list, class_name, (long)count, (long)selection,
                    host_button ? (unsigned)IsWindowEnabled(host_button) : 0,
                    host_button ? (unsigned)IsWindowVisible(host_button) : 0);
                dump_dialog(connection, "connection-type");
                if (list && count > 0) {
                    DWORD_PTR current_index;
                    for (current_index = 0; current_index < count; ++current_index) {
                        SendMessageTimeoutA(list, LB_SETCURSEL, current_index, 0,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 1500, &selection);
                        printf("PROVIDER selected_index=%ld\n", (long)current_index);
                        Sleep(250);
                        if (!IsWindowVisible(connection)) break;
                        host_button = GetDlgItem(connection, 1075);
                        if (host_button && IsWindowEnabled(host_button)) {
                            if (click_control(connection, 1075)) {
                                printf("FLOW_ACTION Connection.Host index=%ld\n",
                                    (long)current_index);
                                stage = 4;
                                break;
                            }
                        }
                    }
                }
                if (stage == 3) {
                    printf("LOCAL_HOST_BLOCKED usable provider Host action unavailable; canceling test flow\n");
                    click_control(connection, IDCANCEL);
                    stage = 5;
                }
            }
        } else if (stage == 4) {
            if (IsWindowEnabled(g_main)) {
                flow_returned = TRUE;
                printf("HOST_FLOW_RETURNED main_enabled=1\n");
                PostMessageA(g_main, WM_COMMAND, MAKEWPARAM(0x1181, 0), 0);
                printf("POST Multiplayer.Settings id=0x1181\n");
                stage = 6;
                Sleep(500);
            }
        } else if (stage == 5) {
            if (IsWindowEnabled(g_main)) {
                flow_returned = TRUE;
                returned_to_game = TRUE;
                printf("HOST_FLOW_RETURNED after-cancel main_enabled=1\n");
                break;
            }
        } else if (stage == 6) {
            settings = wait_dialog(1027, 500);
            if (settings) {
                dump_dialog(settings, "session-settings");
                HWND host_settings = GetDlgItem(settings, 1027);
                if (!host_settings || !IsWindowEnabled(host_settings)) {
                    printf("SESSION_SETTINGS Host Settings unavailable enabled=%u\n",
                        host_settings ? (unsigned)IsWindowEnabled(host_settings) : 0);
                    click_control(settings, IDCANCEL);
                    stage = 9;
                } else if (click_control(settings, 1027)) {
                    printf("SETTINGS_ACTION HostSettings\n");
                    stage = 7;
                }
            }
        } else if (stage == 7) {
            details = wait_dialog(1030, 500);
            if (details) {
                reached_details = TRUE;
                dump_dialog(details, "session-details");
                {
                    HWND players = GetDlgItem(details, 1030);
                    HWND remove = GetDlgItem(details, 1007);
                    DWORD_PTR count = 0;
                    if (players)
                        SendMessageTimeoutA(players, LB_GETCOUNT, 0, 0,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 1500, &count);
                    printf("SESSION_DETAILS players=%ld Remove_enabled=%u\n",
                        (long)count, remove ? (unsigned)IsWindowEnabled(remove) : 0);
                    if (remove && IsWindowEnabled(remove)) {
                        click_control(details, 1007);
                        Sleep(500);
                        {
                            HWND confirmation = find_dialog(IDYES, TRUE);
                            if (confirmation) {
                                dump_dialog(confirmation, "remove-confirmation");
                                printf("REMOVE_CONFIRMATION reached IDYES\n");
                                click_control(confirmation, IDYES);
                                printf("REMOVE_CONFIRMATION accepted for isolated local session\n");
                            } else printf("REMOVE_CONFIRMATION no visible IDYES dialog\n");
                        }
                    } else {
                        printf("REMOVE_CONFIRMATION blocked: Remove unavailable for current player list\n");
                    }
                }
                if (GetDlgItem(details, IDCANCEL)) click_control(details, IDCANCEL);
                else if (GetDlgItem(details, 1002)) click_control(details, 1002);
                stage = 8;
            }
        } else if (stage == 8) {
            if (settings && IsWindowVisible(settings)) click_control(settings, IDCANCEL);
            stage = 9;
        } else if (stage == 9) {
            if (IsWindowEnabled(g_main)) {
                returned_to_game = TRUE;
                printf("WORKFLOW_RETURNED_TO_GAME main_enabled=1\n");
                break;
            }
        }
        if (saw_modal && elapsed - modal_start > 15000 && stage < 4) {
            printf("LOCAL_HOST_TIMEOUT before Host flow completion stage=%d\n", stage);
            if (connection && IsWindowVisible(connection)) click_control(connection, IDCANCEL);
            stage = 5;
        }
        if (saw_modal && elapsed - modal_start > 25000 && stage >= 6 && stage <= 7) {
            printf("SETTINGS_TIMEOUT stage=%d; cancel available dialog\n", stage);
            if (details && IsWindowVisible(details)) click_control(details, IDCANCEL);
            if (settings && IsWindowVisible(settings)) click_control(settings, IDCANCEL);
            stage = 9;
        }
        Sleep(50);
    }
    if (!saw_modal) printf("ERROR Connect did not enter HOST modal\n");
    else if (!returned_to_game && !flow_returned)
        printf("TIMEOUT modal failed to return stage=%d main_enabled=%u\n",
            stage, (unsigned)IsWindowEnabled(g_main));
    if (flow_returned && !reached_details) {
        /* The actual host state either opened settings or was unavailable. */
        if (IsWindowEnabled(g_main)) returned_to_game = TRUE;
    }
    if (returned_to_game) {
        PostMessageA(g_main, WM_APP + 0x384, 0, 0);
        printf("POST HOOK_QUIESCE\n");
        Sleep(750);
        result = reached_details ? 0 : (flow_returned ? 0 : 1);
        PostMessageA(g_main, WM_CLOSE, 0, 0);
        printf("CLOSE only launched private game pid=%lu\n", (unsigned long)g_game_pid);
        Sleep(500);
    }
cleanup:
    if (getmsg_hook) UnhookWindowsHookEx(getmsg_hook);
    if (callwnd_hook) UnhookWindowsHookEx(callwnd_hook);
    if (cbt_hook) UnhookWindowsHookEx(cbt_hook);
    if (hook_module) FreeLibrary(hook_module);
    printf("LOCAL_HOST_END result=%d flow_returned=%u details=%u returned_to_game=%u elapsed_ms=%lu\n",
        result, (unsigned)flow_returned, (unsigned)reached_details,
        (unsigned)returned_to_game, (unsigned long)(GetTickCount() - start));
    fflush(stdout);
    return result;
}

int main(int argc, char **argv)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    BOOL created;
    DWORD timeout_seconds = argc > 1 ? (DWORD)strtoul(argv[1], NULL, 0) : 90;
    int result;
    if (timeout_seconds < 30 || timeout_seconds > 180) {
        printf("ERROR timeout bounds are 30..180 seconds\n");
        return 2;
    }
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNORMAL;
    created = CreateProcessA("C:\\FS98\\FLTSIM98.EXE", NULL, NULL, NULL,
        FALSE, 0, NULL, "C:\\FS98", &startup, &process);
    if (!created) {
        (void)freopen("Z:\\tmp\\mpdialog-host-session.log", "w", stdout);
        printf("ERROR CreateProcess isolated game winerr=%lu\n",
            (unsigned long)GetLastError());
        return 2;
    }
    (void)freopen("Z:\\tmp\\mpdialog-host-session.log", "w", stdout);
    printf("LAUNCHED private game pid=%lu tid=%lu\n",
        (unsigned long)process.dwProcessId, (unsigned long)process.dwThreadId);
    fflush(stdout);
    result = run_session_attempt(timeout_seconds, process.dwProcessId);
    if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
        if (result != 0) {
            printf("CLEANUP terminating only failed private test PID=%lu\n",
                (unsigned long)process.dwProcessId);
            TerminateProcess(process.hProcess, (UINT)result);
        } else {
            PostThreadMessageA(process.dwThreadId, WM_QUIT, 0, 0);
        }
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}
