/* Open FS98's menu commands one at a time and report what each one shows.
 *
 * fsdrive sends keys; this posts WM_COMMAND straight to the main window, so it
 * needs no focus and works on a screen nobody is looking at.  For each command
 * it lists the dialog's controls with their text, captures the dialog as a BMP
 * and closes it, so two runs (retail MAIN.DLL and ours) can be compared.
 *
 * Usage: fsdialogs.exe OUTDIR BOOT_MS ok|cancel COMMAND...
 *   OUTDIR    a Windows path: drive.log and one cCOMMAND.bmp each go there
 *   BOOT_MS   how long to let the game settle once its window is up
 *   ok|cancel the button to press first; cancel follows until the dialog goes
 *   COMMAND   a menu command, in hex
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

static BOOL CALLBACK list_control(HWND h, LPARAM count)
{
    char name[64], text[64];

    GetClassNameA(h, name, sizeof name);
    GetWindowTextA(h, text, sizeof text);
    fprintf(report, "      [%s] id=%d '%s'\n", name, GetDlgCtrlID(h), text);
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

int main(int argc, char **argv)
{
    char path[300];
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
        unsigned command = strtoul(argv[i], NULL, 16);
        int controls = 0;

        fprintf(report, "== command %#x\n", command);
        PostMessageA(main_window, WM_COMMAND, command, 0);
        Sleep(5000);
        scan_dialogs();
        snprintf(path, sizeof path, "%s\\c%04x.bmp", argv[1], command);
        if (dialog_count == 0) {
            fprintf(report, "    no dialog\n");
            capture(NULL, path);
        } else {
            EnumChildWindows(dialogs[dialog_count - 1], list_control, (LPARAM)&controls);
            fprintf(report, "    %d controls\n", controls);
            capture(dialogs[0], path);
        }
        for (tries = 0; tries < 4 && dialog_count > 0; tries++) {
            int ok = press_ok && tries == 0;

            PostMessageA(dialogs[0], WM_COMMAND, ok ? IDOK : IDCANCEL, 0);
            Sleep(2500);
            scan_dialogs();
            fprintf(report, "    after %s: %d dialogs\n", ok ? "OK" : "cancel", dialog_count);
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
