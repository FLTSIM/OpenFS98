/* Drive the running FS98 window from the keyboard.
 *
 * There is no xdotool here, and screenshots alone cannot tell you whether the
 * sim is actually moving -- a parked aircraft renders a static view quite
 * correctly.  This sends real key events into the game so a test can get into
 * a flight, start it moving, and then look at what the 3D view does.
 *
 * Usage: fsdrive.exe TOKEN...
 *   where each TOKEN is either a key name (ENTER, Y, UP, DOWN, LEFT, RIGHT,
 *   F4, SPACE, ESC, or a single letter/digit) or a plain number meaning
 *   "wait this many milliseconds".  A key name may be suffixed with *N to
 *   repeat it N times.
 *
 * Example: fsdrive.exe 2000 ENTER 8000 Y 500 UP*40
 *   -- wait, dismiss the opening screen, wait for the flight to load, toggle
 *      slew, then nudge forward forty times.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HWND g_found;

static BOOL CALLBACK pick(HWND h, LPARAM lp)
{
    RECT r;
    char cls[64];
    (void)lp;
    if (!IsWindowVisible(h))
        return TRUE;
    if (!GetWindowRect(h, &r))
        return TRUE;
    /* the sim's main window is the large one; dialogs and tool windows are not */
    if (r.right - r.left < 320 || r.bottom - r.top < 240)
        return TRUE;
    GetClassNameA(h, cls, sizeof cls);
    /* FS98's main window announces itself by class; anything else that happens
     * to be large belongs to some other program in the same prefix. */
    if (strcmp(cls, "FS98MAIN") != 0)
        return TRUE;
    printf("  found %p  %ldx%ld  class=%s\n", (void *)h,
           r.right - r.left, r.bottom - r.top, cls);
    g_found = h;
    return FALSE;
}

struct keymap { const char *name; WORD vk; };
static const struct keymap keys[] = {
    { "ENTER", VK_RETURN }, { "ESC", VK_ESCAPE }, { "SPACE", VK_SPACE },
    { "UP", VK_UP }, { "DOWN", VK_DOWN }, { "LEFT", VK_LEFT }, { "RIGHT", VK_RIGHT },
    { "F1", VK_F1 }, { "F2", VK_F2 }, { "F3", VK_F3 }, { "F4", VK_F4 },
    { "TAB", VK_TAB }, { "PGUP", VK_PRIOR }, { "PGDN", VK_NEXT },
    { NULL, 0 }
};

static WORD lookup(const char *name)
{
    int i;
    for (i = 0; keys[i].name; i++)
        if (!_stricmp(name, keys[i].name))
            return keys[i].vk;
    if (strlen(name) == 1) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        return (WORD)c;
    }
    return 0;
}

static void tap(WORD vk)
{
    INPUT in[2];
    memset(in, 0, sizeof in);
    in[0].type = INPUT_KEYBOARD;  in[0].ki.wVk = vk;
    in[1].type = INPUT_KEYBOARD;  in[1].ki.wVk = vk;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
    Sleep(40);
}

int main(int argc, char **argv)
{
    int i;
    EnumWindows(pick, 0);
    if (!g_found) { printf("no FS98 window found\n"); return 2; }
    printf("driving %p\n", (void *)g_found);
    SetForegroundWindow(g_found);
    Sleep(300);

    for (i = 1; i < argc; i++) {
        char tok[64], *star;
        int repeat = 1;
        WORD vk;
        if (argv[i][0] >= '0' && argv[i][0] <= '9' && strlen(argv[i]) > 2) {
            Sleep((DWORD)atoi(argv[i]));
            continue;
        }
        strncpy(tok, argv[i], sizeof tok - 1);
        tok[sizeof tok - 1] = 0;
        star = strchr(tok, '*');
        if (star) { *star = 0; repeat = atoi(star + 1); }
        vk = lookup(tok);
        if (!vk) { printf("unknown key '%s'\n", tok); continue; }
        printf("  %s x%d\n", tok, repeat);
        while (repeat-- > 0) {
            SetForegroundWindow(g_found);
            tap(vk);
        }
    }
    return 0;
}
