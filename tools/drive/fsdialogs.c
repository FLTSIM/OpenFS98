/* Open FS98's menu commands one at a time and report what each one shows.
 *
 * fsdrive sends keys; this posts WM_COMMAND straight to the main window, so it
 * needs no focus and works on a screen nobody is looking at.  For each command
 * it lists the dialog's controls with their text, captures the dialog as a BMP
 * and closes it, so two runs (retail MAIN.DLL and ours) can be compared.
 *
 * Usage: fsdialogs.exe OUTDIR BOOT_MS ok|cancel STEP...
 *   OUTDIR    a Windows path: drive.log and one cSTEP.bmp each go there
 *   BOOT_MS   how long to let the game settle once its window is up
 *   ok|cancel the button to press first; cancel follows until the dialog goes
 *   STEP      COMMAND       a menu command, in hex
 *             COMMAND/CTRL...  that command, then click each control CTRL (decimal) in
 *                           the dialog it opens, and look at what is on top: c1607-3805.bmp;
 *                           a CTRL of @X,Y clicks the mouse there in the dialog instead
 *             set:COMMAND/CTRL...  click the controls and press OK, to change a setting
 *             crash         full throttle and the elevator down until a dialog comes up
 *             exit          the Exit command; its confirmation is always answered No
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DIALOGS 32

static HWND main_window;
static DWORD game_pid;
static FILE *report;
static HWND dialogs[MAX_DIALOGS];
static int dialog_count;
static int quiet;                           /* scan without listing the windows */

static BOOL CALLBACK find_main(HWND h, LPARAM unused)
{
    char name[64];

    (void)unused;
    GetClassNameA(h, name, sizeof name);
    if (strcmp(name, "FS98MAIN") == 0 && IsWindowVisible(h)) {
        main_window = h;
        return FALSE;
    }
    return TRUE;
}

/* A list box item's text.  Flights' lists are owner drawn, with a placeholder string
   ('A', 'B'): what they paint is the record their item data points at in the game, which
   starts with its title */
static void item_text(HWND h, long n, char *item, size_t size)
{
    LRESULT data;

    item[0] = 0;
    if (n < 0)
        return;
    data = SendMessageA(h, LB_GETITEMDATA, n, 0);
    if ((GetWindowLongA(h, GWL_STYLE) & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) && data != 0
        && data != LB_ERR) {
        HANDLE process = OpenProcess(PROCESS_VM_READ, FALSE, game_pid);
        SIZE_T got = 0;
        size_t i;

        if (process == NULL || !ReadProcessMemory(process, (LPCVOID)data, item, size - 1, &got))
            got = 0;
        if (process != NULL)
            CloseHandle(process);
        item[got] = 0;
        for (i = 0; item[i] != 0; i++)
            if ((unsigned char)item[i] < 0x20 || (unsigned char)item[i] > 0x7e) {
                item[i] = 0;
                break;
            }
        if (item[0] != 0)
            return;
    }
    /* the pickers keep no item data: their placeholder string, and the description shows
       which entry it is */
    if (SendMessageA(h, LB_GETTEXTLEN, n, 0) < (LRESULT)size)
        SendMessageA(h, LB_GETTEXT, n, (LPARAM)item);
}

static BOOL CALLBACK list_control(HWND h, LPARAM count)
{
    char name[64], text[64];

    GetClassNameA(h, name, sizeof name);
    GetWindowTextA(h, text, sizeof text);
    /* an edit's text is its own, which GetWindowText does not ask another process for */
    if (strcmp(name, "Edit") == 0 && !SendMessageTimeoutA(h, WM_GETTEXT, sizeof text, (LPARAM)text,
                                                         SMTO_ABORTIFHUNG, 2000, NULL))
        text[0] = 0;
    fprintf(report, "      [%s] id=%d '%s'\n", name, GetDlgCtrlID(h), text);
    if (strcmp(name, "ListBox") == 0) {     /* what a picker offers, and what it has chosen */
        char item[256];
        long n = (long)SendMessageA(h, LB_GETCOUNT, 0, 0), sel = (long)SendMessageA(h, LB_GETCURSEL, 0, 0);

        item_text(h, sel, item, sizeof item);
        fprintf(report, "        %ld items, selected %ld '%s'\n", n, sel, item);
    }
    if (strcmp(name, "ComboBox") == 0) {
        char item[128] = "";
        long sel = (long)SendMessageA(h, CB_GETCURSEL, 0, 0);

        if (sel >= 0 && SendMessageA(h, CB_GETLBTEXTLEN, sel, 0) < (LRESULT)sizeof item)
            SendMessageA(h, CB_GETLBTEXT, sel, (LPARAM)item);
        fprintf(report, "        %ld entries, chosen %ld '%s'\n", (long)SendMessageA(h, CB_GETCOUNT, 0, 0), sel, item);
    }
    if (strcmp(name, "Button") == 0 && (((GetWindowLongA(h, GWL_STYLE) & BS_TYPEMASK) >= BS_CHECKBOX
        && (GetWindowLongA(h, GWL_STYLE) & BS_TYPEMASK) <= BS_AUTORADIOBUTTON)
        || SendMessageA(h, BM_GETCHECK, 0, 0) != 0))
        fprintf(report, "        checked %ld\n", (long)SendMessageA(h, BM_GETCHECK, 0, 0));
    ++*(int *)count;
    return TRUE;
}

static BOOL CALLBACK find_dialogs(HWND h, LPARAM unused)
{
    DWORD pid;
    char name[64], text[128];
    RECT r;

    (void)unused;
    GetWindowThreadProcessId(h, &pid);
    if (pid != game_pid || h == main_window || !IsWindowVisible(h))
        return TRUE;
    GetClassNameA(h, name, sizeof name);
    GetWindowTextA(h, text, sizeof text);
    GetWindowRect(h, &r);
    if (!quiet)
        fprintf(report, "    window [%s] '%s' %ldx%ld\n", name, text, r.right - r.left, r.bottom - r.top);
    if (strcmp(name, "#32770") == 0 && dialog_count < MAX_DIALOGS)
        dialogs[dialog_count++] = h;
    return TRUE;
}

static void scan_dialogs(void)
{
    dialog_count = 0;
    EnumWindows(find_dialogs, 0);
}

/* A window, or the whole screen for NULL, as a 24-bit BMP */
static void capture(HWND h, const char *path)
{
    RECT r;
    HDC screen, memory;
    HBITMAP bitmap;
    BITMAPINFO info;
    BITMAPFILEHEADER header;
    void *bits;
    FILE *f;
    int width, height, size;

    if (h != NULL) {
        GetWindowRect(h, &r);
    } else {
        r.left = r.top = 0;
        r.right = GetSystemMetrics(SM_CXSCREEN);
        r.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    width = r.right - r.left;
    height = r.bottom - r.top;
    if (width <= 0 || height <= 0)
        return;
    screen = GetDC(NULL);
    memory = CreateCompatibleDC(screen);
    memset(&info, 0, sizeof info);
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(memory, bitmap);
    BitBlt(memory, 0, 0, width, height, screen, r.left, r.top, SRCCOPY);
    size = ((width * 3 + 3) & ~3) * height;
    f = fopen(path, "wb");
    if (f != NULL) {
        memset(&header, 0, sizeof header);
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof header + sizeof info.bmiHeader;
        header.bfSize = header.bfOffBits + size;
        fwrite(&header, sizeof header, 1, f);
        fwrite(&info.bmiHeader, sizeof info.bmiHeader, 1, f);
        fwrite(bits, size, 1, f);
        fclose(f);
    }
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(NULL, screen);
}

struct find_control { int id; HWND found; };

static BOOL CALLBACK find_control_in(HWND h, LPARAM data)
{
    struct find_control *f = (struct find_control *)data;

    if (GetDlgCtrlID(h) == f->id && f->found == NULL)
        f->found = h;
    return f->found == NULL;
}

/* Click a control anywhere in a dialog, pages included, as its parent sees a click */
static int click(HWND dialog, int id)
{
    struct find_control f = { id, NULL };

    EnumChildWindows(dialog, find_control_in, (LPARAM)&f);
    if (f.found == NULL) {
        fprintf(report, "    no control %d\n", id);
        return 0;
    }
    /* what a click sends: BM_CLICK does nothing for the owner-drawn buttons under wine */
    PostMessageA(GetParent(f.found), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)f.found);
    Sleep(3000);
    fprintf(report, "    clicked %d, checked %ld, style %#lx\n", id, (long)SendMessageA(f.found, BM_GETCHECK, 0, 0),
            (unsigned long)GetWindowLongA(f.found, GWL_STYLE));
    return 1;
}

/* A mouse click at X,Y in the dialog's window, on whatever child is there */
static int click_at(HWND dialog, int x, int y)
{
    RECT r;
    POINT p;
    HWND h = dialog, child;

    GetWindowRect(dialog, &r);
    p.x = r.left + x;
    p.y = r.top + y;
    for (;;) {
        POINT c = p;
        ScreenToClient(h, &c);
        child = ChildWindowFromPointEx(h, c, CWP_SKIPINVISIBLE);
        if (child == NULL || child == h)
            break;
        h = child;
    }
    ScreenToClient(h, &p);
    PostMessageA(h, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(p.x, p.y));
    PostMessageA(h, WM_LBUTTONUP, 0, MAKELPARAM(p.x, p.y));
    Sleep(3000);
    fprintf(report, "    clicked at %d,%d on control %d\n", x, y, GetDlgCtrlID(h));
    return 1;
}

/* The first visible button in a dialog, for one that ignores IDCANCEL */
static BOOL CALLBACK find_button(HWND h, LPARAM found)
{
    char name[64];

    GetClassNameA(h, name, sizeof name);
    if (strcmp(name, "Button") == 0 && IsWindowVisible(h) && GetDlgCtrlID(h) > 0) {
        *(int *)found = GetDlgCtrlID(h);
        return FALSE;
    }
    return TRUE;
}

/* Find a control anywhere in a dialog, pages included */
static HWND control(HWND dialog, int id)
{
    struct find_control f = { id, NULL };

    EnumChildWindows(dialog, find_control_in, (LPARAM)&f);
    if (f.found == NULL)
        fprintf(report, "    no control %d\n", id);
    return f.found;
}

/* Type TEXT into an edit, as the user's typing tells its parent */
static int set_text(HWND dialog, int id, const char *text)
{
    HWND h = control(dialog, id);

    if (h == NULL)
        return 0;
    SendMessageA(h, WM_SETTEXT, 0, (LPARAM)text);
    SendMessageA(h, EM_SETMODIFY, TRUE, 0);
    PostMessageA(GetParent(h), WM_COMMAND, MAKEWPARAM(id, EN_CHANGE), (LPARAM)h);
    Sleep(1000);
    fprintf(report, "    typed '%s' into %d\n", text, id);
    return 1;
}

/* Choose item WHICH of a list box, a number or a title with _ for each space, as a click
   on it tells its parent */
static int select_item(HWND dialog, int id, const char *which)
{
    char item[256], title[128];
    HWND h = control(dialog, id);
    long n, count;
    size_t i;

    if (h == NULL)
        return 0;
    for (i = 0; which[i] != 0 && which[i] != '/' && i < sizeof title - 1; i++)
        title[i] = which[i] == '_' ? ' ' : which[i];
    title[i] = 0;
    n = strtol(title, NULL, 10);
    if (title[0] < '0' || title[0] > '9') {
        count = (long)SendMessageA(h, LB_GETCOUNT, 0, 0);
        for (n = 0; n < count; n++) {
            item_text(h, n, item, sizeof item);
            if (strcmp(item, title) == 0)
                break;
        }
        if (n == count) {
            fprintf(report, "    no control %d item '%s'\n", id, title);
            return 0;
        }
    }
    SendMessageA(h, LB_SETCURSEL, n, 0);
    PostMessageA(GetParent(h), WM_COMMAND, MAKEWPARAM(id, LBN_SELCHANGE), (LPARAM)h);
    Sleep(1500);
    item_text(h, n, item, sizeof item);
    fprintf(report, "    selected %ld '%s' in %d\n", n, item, id);
    return 1;
}

/* Tick a check box or choose a radio button, as a click on it does: wine's buttons in
   another process toggle on BM_CLICK, but SIMSUI's owner-drawn ones do not, so set the
   state and tell the parent */
static int check(HWND dialog, int id)
{
    HWND h = control(dialog, id);
    LONG type;
    LRESULT was;

    if (h == NULL)
        return 0;
    type = GetWindowLongA(h, GWL_STYLE) & BS_TYPEMASK;
    was = SendMessageA(h, BM_GETCHECK, 0, 0);
    if (type == BS_OWNERDRAW) {             /* SIMSUI's keep their own state: a mouse click */
        RECT r;

        GetClientRect(h, &r);
        PostMessageA(h, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(r.right / 2, r.bottom / 2));
        PostMessageA(h, WM_LBUTTONUP, 0, MAKELPARAM(r.right / 2, r.bottom / 2));
        Sleep(1500);
        fprintf(report, "    ticked %d by a click, checked %ld -> %ld\n", id, (long)was,
                (long)SendMessageA(h, BM_GETCHECK, 0, 0));
        return 1;
    }
    if (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON)
        SendMessageA(h, BM_SETCHECK, BST_CHECKED, 0);
    else
        SendMessageA(h, BM_SETCHECK, was == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
    PostMessageA(GetParent(h), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)h);
    Sleep(1500);
    fprintf(report, "    ticked %d, checked %ld -> %ld\n", id, (long)was, (long)SendMessageA(h, BM_GETCHECK, 0, 0));
    return 1;
}

/* Choose entry N of a combo box, as the user's choice tells its parent */
static int choose(HWND dialog, int id, int n)
{
    char item[128] = "";
    HWND h = control(dialog, id);

    if (h == NULL)
        return 0;
    SendMessageA(h, CB_SETCURSEL, n, 0);
    PostMessageA(GetParent(h), WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), (LPARAM)h);
    Sleep(1500);
    if (SendMessageA(h, CB_GETLBTEXTLEN, n, 0) < (LRESULT)sizeof item)
        SendMessageA(h, CB_GETLBTEXT, n, (LPARAM)item);
    fprintf(report, "    chose %d '%s' in %d\n", n, item, id);
    return 1;
}

/* Whether a dialog has a visible button: the game's progress messages ("Loading ...",
   "Ending ...") have none, and close by themselves */
static BOOL CALLBACK find_visible_button(HWND h, LPARAM found)
{
    char name[64];

    GetClassNameA(h, name, sizeof name);
    if (strcmp(name, "Button") == 0 && IsWindowVisible(h)) {
        *(int *)found = 1;
        return FALSE;
    }
    return TRUE;
}

/* Wait, without logging, for a progress message on top to go: how long it stays up is
   wine's timing, not the dialogs' */
static void wait_progress(void)
{
    int waited, buttons = 0;

    quiet = 1;
    for (waited = 0; waited < 240; waited++) {
        scan_dialogs();
        buttons = 0;
        if (dialog_count > 0)
            EnumChildWindows(dialogs[0], find_visible_button, (LPARAM)&buttons);
        if (dialog_count == 0 || buttons)
            break;
        Sleep(500);
    }
    quiet = 0;
}

static void post(unsigned command)
{
    PostMessageA(main_window, WM_COMMAND, command, 0);
}

int main(int argc, char **argv)
{
    char path[300], name[64];
    int i, tries, press_ok;

    if (argc < 4)
        return 2;
    press_ok = strcmp(argv[3], "ok") == 0;
    snprintf(path, sizeof path, "%s\\drive.log", argv[1]);
    report = fopen(path, "w");
    if (report == NULL)
        return 2;
    setvbuf(report, NULL, _IONBF, 0);
    for (tries = 0; tries < 600 && main_window == NULL; tries++) {
        EnumWindows(find_main, 0);
        Sleep(500);
    }
    if (main_window == NULL) {
        fprintf(report, "no window\n");
        return 2;
    }
    GetWindowThreadProcessId(main_window, &game_pid);
    Sleep(atoi(argv[2]));
    for (scan_dialogs(); dialog_count > 0; scan_dialogs()) {
        fprintf(report, "closing a startup dialog\n");
        PostMessageA(dialogs[0], WM_COMMAND, IDCANCEL, 0);
        Sleep(3000);
    }
    for (i = 4; i < argc; i++) {
        const char *step = argv[i], *slash;
        unsigned command = 0;
        int controls = 0, control = 0, setting = 0, cancel_only = 0, waited, top = 0;

        if (strncmp(step, "set:", 4) == 0) {
            setting = 1;
            step += 4;
        }
        if (strncmp(step, "sleep:", 6) == 0) {     /* a flight loading: Save opens nothing meanwhile */
            fprintf(report, "== sleep %s\n", step + 6);
            Sleep(atoi(step + 6) * 1000);
            continue;
        }
        if (strcmp(step, "quit") == 0) {           /* Exit, answered Yes: the game saves its settings */
            fprintf(report, "== quit\n");
            post(0x100c4);                          /* EXIT */
            Sleep(4000);
            scan_dialogs();
            if (dialog_count == 0 || !click(dialogs[0], 2004)) {
                fprintf(report, "    STUCK\n");
                return 3;
            }
            for (waited = 0; waited < 60 && IsWindow(main_window); waited++)
                Sleep(1000);
            fprintf(report, "    %s\n", IsWindow(main_window) ? "STUCK, still running" : "the game has gone");
            break;
        }
        if (strcmp(step, "pause") == 0) {
            fprintf(report, "== pause\n");
            post(0x10102);                          /* PAUSE_ON */
            Sleep(2000);
            continue;
        }
        if (strcmp(step, "crash") == 0) {
            fprintf(report, "== crash\n");
            snprintf(name, sizeof name, "crash");
            post(0x1003c);                          /* THROTTLE_FULL */
            for (waited = 0, dialog_count = 0; waited < 240 && dialog_count == 0; waited++) {
                post(0x10048);                      /* NUMPAD8, the elevator down */
                Sleep(500);
                scan_dialogs();
            }
            fprintf(report, "    %s\n", dialog_count ? "a dialog came up" : "nothing came up in 120 s");
            Sleep(3000);
            scan_dialogs();
        } else {
            if (strcmp(step, "exit") == 0) {
                command = 0x100c4;                  /* EXIT */
                cancel_only = 2005;                 /* Alerts' No: it ignores IDCANCEL */
                snprintf(name, sizeof name, "exit");
            } else {
                command = strtoul(step, NULL, 16);
                slash = strchr(step, '/');
                control = slash != NULL;
                if (control)
                    snprintf(name, sizeof name, "%04x-%s", command, strrchr(step, '/') + 1);
                else
                    snprintf(name, sizeof name, "%04x", command);
            }
            fprintf(report, "== %scommand %#x\n", setting ? "set: " : "", command);
            post(command);
            Sleep(5000);
            scan_dialogs();
            for (slash = strchr(step, '/'); slash != NULL && dialog_count > 0; slash = strchr(slash + 1, '/')) {
                if (slash[1] == '@') {
                    fprintf(report, "   then the point %s\n", slash + 2);
                    click_at(dialogs[0], atoi(slash + 2), atoi(strchr(slash, ',') + 1));
                    scan_dialogs();
                } else if (slash[1] == '!') {      /* /!ID: tick a box or a radio button */
                    check(dialogs[0], atoi(slash + 2));
                    scan_dialogs();
                    continue;
                } else if (slash[1] == '%') {      /* /%ID=N: combo box entry N */
                    choose(dialogs[0], atoi(slash + 2), atoi(strchr(slash, '=') + 1));
                    scan_dialogs();
                    continue;
                } else if (slash[1] == '#') {      /* /#ID=N: list item N */
                    select_item(dialogs[0], atoi(slash + 2), strchr(slash, '=') + 1);
                    scan_dialogs();
                    continue;
                } else if (strchr(slash, '=') != NULL
                           && (strchr(slash + 1, '/') == NULL || strchr(slash, '=') < strchr(slash + 1, '/'))) {
                    char text[128];                 /* /ID=TEXT: typed into an edit */
                    const char *from = strchr(slash, '=') + 1, *end = strchr(from, '/');
                    size_t n = end != NULL ? (size_t)(end - from) : strlen(from);

                    if (n >= sizeof text)
                        n = sizeof text - 1;
                    memcpy(text, from, n);
                    text[n] = 0;
                    set_text(dialogs[0], atoi(slash + 1), text);
                    scan_dialogs();
                    continue;
                } else {
                    fprintf(report, "   then control %d\n", atoi(slash + 1));
                    if (click(dialogs[0], atoi(slash + 1)))
                        scan_dialogs();
                }
                top = 1;
            }
            if (setting) {
                snprintf(path, sizeof path, "%s\\s%s.bmp", argv[1], name);
                if (dialog_count > 0)
                    capture(dialogs[0], path);
                if (dialog_count > 0)
                    PostMessageA(dialogs[dialog_count - 1], WM_COMMAND, IDOK, 0);
                Sleep(3000);
                scan_dialogs();
                fprintf(report, "    after OK: %d dialogs\n", dialog_count);
                if (dialog_count > 0)
                    fprintf(report, "    STUCK\n");
                continue;
            }
        }
        snprintf(path, sizeof path, "%s\\c%s.bmp", argv[1], name);
        if (dialog_count == 0) {
            fprintf(report, "    no dialog\n");
            capture(NULL, path);
        } else {
            EnumChildWindows(dialogs[top ? 0 : dialog_count - 1], list_control, (LPARAM)&controls);
            fprintf(report, "    %d controls\n", controls);
            capture(dialogs[0], path);
        }
        for (tries = 0; tries < 4 && dialog_count > 0; tries++) {
            int ok = press_ok && tries == 0 && !cancel_only, before = dialog_count;

            PostMessageA(dialogs[0], WM_COMMAND, ok ? IDOK : cancel_only ? cancel_only : IDCANCEL, 0);
            Sleep(2500);
            wait_progress();
            scan_dialogs();
            fprintf(report, "    after %s: %d dialogs\n", ok ? "OK" : cancel_only ? "No" : "cancel", dialog_count);
            /* A dialog that ignores IDCANCEL, such as an alert that OK raised: press its first
               button instead, after listing what it is */
            if (!ok && !cancel_only && dialog_count > 0 && dialog_count >= before) {
                int count = 0, button = 0;

                EnumChildWindows(dialogs[0], list_control, (LPARAM)&count);
                EnumChildWindows(dialogs[0], find_button, (LPARAM)&button);
                if (button != 0 && click(dialogs[0], button)) {
                    scan_dialogs();
                    fprintf(report, "    after button %d: %d dialogs\n", button, dialog_count);
                }
            }
        }
        if (dialog_count > 0)
            fprintf(report, "    STUCK\n");
        if (!IsWindow(main_window)) {
            fprintf(report, "GAME GONE\n");
            return 3;
        }
    }
    fprintf(report, "done\n");
    return 0;
}
