/* The main window (FS98MAIN): its procedure, its commands, full screen in a window and back,
 * help, and the callbacks it walks its child and floating windows with. */
#include "host_rt.h"
#include "fs98/arena.h"

#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define CFG ((const char *)VA(0x41c120))
#define MAIN_WINDOW G32(0x41880c)
#define SEND(window, message, w, l) \
    WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l)
#define POST(window, message, w, l) \
    WIN(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l)
#define WRITE_PROFILE(section, key, value) \
    WIN(WritePrivateProfileStringA, int32_t, (const char *, const char *, const char *, const char *))(section, key, value, CFG)
/* the menu (0x418810) unless it is hidden */
#define MENU_UNLESS(hidden) ((hidden) ? 0 : G32(0x418810))

/* ---- help ------------------------------------------------------------------------------ */

/* 0x403de0: WinHelp, full screen first left (command 0x1501). WinHelp's result. */
int32_t SC fs98_host_rt_help(uint32_t window, const char *file, uint32_t command, uint32_t data)
{
    if (G32(0x418854))
        SEND(MAIN_WINDOW, 0x111, 0x1501, 0);
    return WIN(WinHelpA, int32_t, (uint32_t, const char *, uint32_t, uint32_t))(window, file, command, data);
}

/* 0x401a50: a message box's help button: the context popup (HELP_CONTEXTPOPUP) for its
 * HELPINFO's item in the help file 0x417024 names. */
int32_t SC fs98_host_rt_message_help(const uint32_t *info)
{
    return fs98_host_rt_help(info[3], (const char *)G_PTR(0x417024), 8, info[4]);
}

/* ---- the walks' callbacks (EnumChildWindows, EnumThreadWindows): each 1, to go on -------- */

/* 0x403c70: told the main window was resized (0x467) */
int32_t SC fs98_host_rt_tell_resized(uint32_t window, uint32_t unused)
{
    (void)unused;
    SEND(window, 0x467, 0, 0);
    return 1;
}

/* 0x403c90: told a move or resize ended (WM_EXITSIZEMOVE) */
int32_t SC fs98_host_rt_tell_size_move_ended(uint32_t window, uint32_t unused)
{
    (void)unused;
    SEND(window, 0x232, 0, 0);
    return 1;
}

/* 0x403cb0: told it moved (WM_MOVE) */
int32_t SC fs98_host_rt_tell_moved(uint32_t window, uint32_t unused)
{
    (void)unused;
    SEND(window, 3, 0, 0);
    return 1;
}

/* 0x403cd0: a floating window's caption drawn active or not, with the main window's */
int32_t SC fs98_host_rt_activate_floating(uint32_t window, uint32_t active)
{
    char name[0x100];
    WIN(GetClassNameA, int32_t, (uint32_t, char *, int32_t))(window, name, 0xff);
    if (CALL_C(_strcmpi, int32_t, (const char *, const char *))(name, STR(0x417178)) == 0) {
        SEND(window, 0x86, active, 0);
        WIN(RedrawWindow, int32_t, (uint32_t, const void *, uint32_t, uint32_t))(window, 0, 0, 0x501);
    }
    return 1;
}

/* 0x403d40: a visible topmost window that is off the screen now (the display changed) put
 * back in front -- a SetWindowPos with nothing changed */
int32_t SC fs98_host_rt_recheck_topmost(uint32_t window, uint32_t unused)
{
    int32_t rect[4];
    uint32_t dc;
    (void)unused;
    if (!(WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -20) & 0x80) ||
        !WIN(IsWindowVisible, int32_t, (uint32_t))(window))
        return 1;
    dc = WIN(GetDC, uint32_t, (uint32_t))(0);
    WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
    if (!WIN(RectVisible, int32_t, (uint32_t, const void *))(dc, rect))
        WIN(SetWindowPos, int32_t, (uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))(window, 0, 0, 0, 0, 0, 0x15);
    WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(0, dc);
    return 1;
}

/* 0x403db0: WM_COPYDATA from another copy of the game (0x4024c0's block): brought to the
 * front and the game told (0x20014), if the block is one */
void SC fs98_host_rt_copied(uint32_t unused, const uint32_t *data)
{
    (void)unused;
    if (*(const uint32_t *)(uintptr_t)data[2] != 1)
        return;
    WIN(SetForegroundWindow, int32_t, (uint32_t))(MAIN_WINDOW);
    fs98_host_rt_hook(0x20014, fs98_handle(data));
}

/* ---- full screen in a window ------------------------------------------------------------- */

/* ---- the floating windows' DirectDraw clippers ---------------------------------------------- */

/* 0x403e20: a floating window's bitmap (+0xa0) placed: its client rectangle in screen
 * coordinates at +0xc (relative to the main window in full screen), the frame count's
 * value (0x41bb2c) at +4, and a clipper for its window (+0x24) made (+0x8c) if there is
 * DirectDraw and none yet. */
void SC fs98_host_rt_place_bitmap(uint8_t *window)
{
    uint8_t *bitmap = (uint8_t *)PTR(window, 0xa0);
    int32_t client[4], frame[4];
    uint32_t directdraw;
    int32_t (SC *to_screen)(uint32_t, void *) = (int32_t (SC *)(uint32_t, void *))IMPORT(ClientToScreen);
    WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(bitmap, 0x24), client);
    D32(bitmap, 4) = G32(0x41bb2c);
    directdraw = G32(0x41a4e4);
    if (directdraw && D32(bitmap, 0x8c) == 0) {
        void *clipper;
        COM_METHOD((void *)(uintptr_t)directdraw, 0x10, int32_t, (void *, uint32_t, void *, void *))(
            (void *)(uintptr_t)directdraw, 0, AT(bitmap, 0x8c), 0);
        clipper = PTR(bitmap, 0x8c);
        ((int32_t (SC *)(void *, uint32_t, uint32_t))PTR(PTR(clipper, 0), 0x20))(clipper, 0, D32(bitmap, 0x24));
    }
    if (G32(0x418854)) {
        WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(MAIN_WINDOW, frame);
        I32(bitmap, 0xc) = client[0];
        I32(bitmap, 0x10) = client[1];
        I32(bitmap, 0x14) = client[2];
        I32(bitmap, 0x18) = client[3];
        to_screen(D32(bitmap, 0x24), AT(bitmap, 0xc));
        to_screen(D32(bitmap, 0x24), AT(bitmap, 0x14));
        WIN(OffsetRect, int32_t, (void *, int32_t, int32_t))(AT(bitmap, 0xc), (int32_t)-(uint32_t)frame[0],
                                                             (int32_t)-(uint32_t)frame[1]);
        return;
    }
    I32(bitmap, 0xc) = client[0];
    I32(bitmap, 0x10) = client[1];
    I32(bitmap, 0x14) = client[2];
    I32(bitmap, 0x18) = client[3];
    to_screen(D32(bitmap, 0x24), AT(bitmap, 0xc));
    to_screen(D32(bitmap, 0x24), AT(bitmap, 0x14));
}

/* 0x403f30: its clipper released, taken off the primary surface first if it is the one there
 * (0x41a4f0). */
void SC fs98_host_rt_release_clipper(uint8_t *window)
{
    uint8_t *bitmap = (uint8_t *)PTR(window, 0xa0);
    void *clipper = PTR(bitmap, 0x8c), *primary;
    if (clipper == 0)
        return;
    if (G_PTR(0x41a4f0) == clipper) {
        primary = G_PTR(0x41a4f8);
        ((int32_t (SC *)(void *, void *))PTR(PTR(primary, 0), 0x70))(primary, 0);
        G32(0x41a4f0) = 0;
    }
    clipper = PTR(bitmap, 0x8c);
    ((int32_t (SC *)(void *))PTR(PTR(clipper, 0), 8))(clipper);
    D32(bitmap, 0x8c) = 0;
}

/* ---- the commands ------------------------------------------------------------------------- */

/* a web page's command: string id from the language DLL, [Internet] key= in FLTSIM98.CFG
 * over it (+0xc8), opened. 1. */
static int32_t open_page(uint32_t id, uint32_t key)
{
    SERVICE(0x2a0, int32_t, (uint32_t, char *, uint32_t))(id, (char *)VA(0x41c480), 0x104);
    SERVICE(0xc8, int32_t, (const char *, const char *, char *, uint32_t))(STR(0x417a3c), STR(key), (char *)VA(0x41c480), 0x104);
    WIN(ShellExecuteA, uint32_t, (uint32_t, const char *, const char *, const char *, const char *, int32_t))(
        MAIN_WINDOW, 0, STR(0x41c480), 0, 0, 1);
    return 1;
}

/* 0x403390: the main window's own commands (a menu's or an accelerator's: code 0 or 1): help
 * (contents, index, the topics 0xbb8.., the concept pages in "<help file>>Concept"), the web
 * pages, the search page (Internet Explorer's, else the configuration's), quit (the game
 * told, 0x100c4) and, with shift, 0x1103's dump (+0x4c). 1 when handled; 0 for the rest. */
int32_t SC fs98_host_rt_command(uint32_t window, uint32_t id, uint32_t code, uint32_t parameter)
{
    char concept[0x105];
    uint32_t at, key, size;
    (void)window; (void)parameter;
    for (at = 1; at < 0x105; ++at)
        concept[at] = 0;
    concept[0] = (char)G8(0x4187f8);
    for (at = 1; at < 0x105; ++at)
        concept[at] = 0;
    SERVICE(0x2a8, int32_t, (char *, const char *))(concept, STR(0x41c000));
    SERVICE(0x2b0, int32_t, (char *, const char *))(concept, STR(0x417adc));
    if (code > 1)
        return 0;
    switch (id) {
    case 0x1103:
        if (WIN(GetKeyState, uint32_t, (int32_t))(0x10) & 0x8000)
            SERVICE(0x4c, int32_t, (void))();
        return 0;
    case 0x110a:
        fs98_host_rt_hook(0x100c4, 0);
        return 1;
    case 0x1601: fs98_host_rt_help(MAIN_WINDOW, concept, 1, 0xbba); return 1;
    case 0x1608: fs98_host_rt_help(MAIN_WINDOW, concept, 1, 0xbbc); return 1;
    case 0x1609: fs98_host_rt_help(MAIN_WINDOW, concept, 1, 0xbbe); return 1;
    case 0x160a: fs98_host_rt_help(MAIN_WINDOW, concept, 1, 0xbc0); return 1;
    case 0x1602: fs98_host_rt_help(MAIN_WINDOW, STR(0x41c000), 0xf, 0); return 1;
    case 0x1603: fs98_host_rt_help(MAIN_WINDOW, STR(0x41c000), 0x105, fs98_handle(VA(0x4187f8))); return 1;
    case 0x1604: fs98_host_rt_help(MAIN_WINDOW, STR(0x41c000), 1, 0xbb8); return 1;
    case 0x1661: return open_page(0xf600, 0x417ad0);
    case 0x1662: return open_page(0xf601, 0x417ac4);
    case 0x1663: return open_page(0xf602, 0x417ac0);
    case 0x1664: return open_page(0xf603, 0x417ab0);
    case 0x1665: return open_page(0xf604, 0x417aa8);
    case 0x1666: return open_page(0xf605, 0x417a9c);
    case 0x1667: return open_page(0xf606, 0x417a8c);
    case 0x1668:
        /* the key is never closed, as retail's */
        G8(0x41c480) = 0;
        if (WIN(RegOpenKeyExA, int32_t, (uint32_t, const char *, uint32_t, uint32_t, uint32_t *))(
                0x80000001u, STR(0x417a60), 0, 0x20019, &key) == 0) {
            size = 0x104;
            if (WIN(RegQueryValueExA, int32_t, (uint32_t, const char *, uint32_t *, uint32_t *, void *, uint32_t *))(
                    key, STR(0x417a54), 0, 0, VA(0x41c480), &size) != 0)
                G8(0x41c480) = 0;
        }
        if (G8(0x41c480) == 0) {
            SERVICE(0x2a0, int32_t, (uint32_t, char *, uint32_t))(0xf607, (char *)VA(0x41c480), 0x104);
            SERVICE(0xc8, int32_t, (const char *, const char *, char *, uint32_t))(STR(0x417a3c), STR(0x417a48),
                                                                                   (char *)VA(0x41c480), 0x104);
        }
        WIN(ShellExecuteA, uint32_t, (uint32_t, const char *, const char *, const char *, const char *, int32_t))(
            MAIN_WINDOW, 0, STR(0x41c480), 0, 0, 1);
        return 1;
    case 0x1669: return open_page(0xf608, 0x417a30);
    case 0x166a: return open_page(0xf609, 0x417a28);
    default:
        return 0;
    }
}

/* ---- the procedure -------------------------------------------------------------------------- */

/* the window's new client area (WM_SIZE): kept (0x41c340), with the scales from it (0x41bda8:
 * width * 65536 / 8192, 0x41c350: height * 65536 / 6144) and the children told; its centre
 * (0x41c358) from the window's place */
static void sized(uint32_t window)
{
    int32_t rect[4];
    uint32_t shown, has_menu;
    int32_t (SC *metrics)(int32_t) = (int32_t (SC *)(int32_t))IMPORT(GetSystemMetrics);
    fs98_host_rt_save_placement(window);
    if (WIN(IsIconic, int32_t, (uint32_t))(window))
        return;
    if (G32(0x418854))
        shown = G32(0x417198) == 0;
    else
        shown = G32(0x418824) == 0;
    has_menu = WIN(GetMenu, uint32_t, (uint32_t))(window) != 0;
    WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(window, rect);
    if (has_menu != shown) {
        if (has_menu)
            rect[1] = (int32_t)((uint32_t)rect[1] - (uint32_t)metrics(0xf));
        else
            rect[3] = (int32_t)((uint32_t)rect[3] - (uint32_t)metrics(0xf));
    }
    if (G32(0x418854)) {
        rect[2] = (int32_t)((uint32_t)rect[0] + G32(0x4183a8));
        rect[3] = (int32_t)((uint32_t)rect[1] + G32(0x4183ac));
        if (G32(0x417198) == 0)
            rect[3] = (int32_t)((uint32_t)rect[3] - (uint32_t)metrics(0xf));
    }
    if (WIN(EqualRect, int32_t, (const void *, const void *))(rect, VA(0x41c340)))
        return;
    G32(0x41c348) = (uint32_t)rect[2];
    G32(0x41c340) = (uint32_t)rect[0];
    G32(0x41c34c) = (uint32_t)rect[3];
    GI32(0x41bda8) = (int32_t)(((uint32_t)rect[2] - (uint32_t)rect[0]) << 16) / 0x2000;
    G32(0x41c344) = (uint32_t)rect[1];
    GI32(0x41c350) = (int32_t)(((uint32_t)rect[3] - (uint32_t)rect[1]) << 16) / 0x1800;
    WIN(EnumChildWindows, int32_t, (uint32_t, void *, uint32_t))(window, (void *)fs98_host_rt_tell_resized, 0);
    if (G32(0x418864) == 0)
        WIN(GetWindowRect, int32_t, (uint32_t, void *))(window, VA(0x41c590));
    G32(0x41c358) = (uint32_t)((int32_t)(G32(0x41c598) - G32(0x41c590)) / 2) + G32(0x41c590);
    G32(0x41c35c) = (uint32_t)((int32_t)(G32(0x41c59c) - G32(0x41c594)) / 2) + G32(0x41c594);
}

/* the window moved (WM_MOVE): its place kept, unless it is empty or being placed, and the
 * children told */
static void moved(uint32_t window)
{
    int32_t rect[4];
    fs98_host_rt_save_placement(window);
    WIN(GetWindowRect, int32_t, (uint32_t, int32_t *))(window, rect);
    if (rect[0] < rect[2] && rect[1] < rect[3] && G32(0x418864) == 0) {
        GI32(0x41c598) = rect[2];
        GI32(0x41c590) = rect[0];
        GI32(0x41c594) = rect[1];
        G32(0x41c358) = (uint32_t)((int32_t)((uint32_t)rect[2] - (uint32_t)rect[0]) / 2) + (uint32_t)rect[0];
        GI32(0x41c59c) = rect[3];
        G32(0x41c35c) = (uint32_t)((int32_t)((uint32_t)rect[3] - (uint32_t)rect[1]) / 2) + (uint32_t)rect[1];
    }
    WIN(EnumChildWindows, int32_t, (uint32_t, void *, uint32_t))(window, (void *)fs98_host_rt_tell_moved, 0);
}

/* a key down (WM_KEYDOWN, WM_SYSKEYDOWN): alt shows the menu (and tells the game, 0x100f8);
 * then the key, its scan code, the shift, control and alt state and whether it is extended,
 * mapped (+0x208) and given to the game's keys (0x40cc20). 1 when that took it. */
static int32_t key_down(uint32_t window, uint32_t key, uint32_t parameter)
{
    uint32_t scan, modifiers, extended;
    uint32_t (SC *state)(int32_t) = (uint32_t (SC *)(int32_t))IMPORT(GetKeyState);
    if (key == 0x12) {
        fs98_host_rt_hook(0x100f8, 0);
        G32(0x418870) = 1;
        if (WIN(GetMenu, uint32_t, (uint32_t))(window) == 0) {
            G32(0x41886c) = 1;
            WIN(SetMenu, int32_t, (uint32_t, uint32_t))(window, G32(0x418810));
        }
    }
    scan = (parameter >> 16) & 0xff;
    modifiers = 0;
    if (state(0x11) & 0x8000)
        modifiers |= 2;
    if (state(0x10) & 0x8000)
        modifiers |= 1;
    if (state(0x12) & 0x8000)
        modifiers |= 4;
    extended = ((int32_t)parameter >> 24) & 1;
    SERVICE(0x208, int32_t, (uint32_t *, uint32_t *, uint32_t *, uint32_t *))(&key, &scan, &modifiers, &extended);
    return fs98_host_rt_keys(key, scan, modifiers, extended) != 0;
}

/* 0x4027e0: FS98MAIN's procedure. */
int32_t SC fs98_host_rt_main_procedure(uint32_t window, uint32_t message, uint32_t w, uint32_t l)
{
    switch (message) {
    case 1: {           /* WM_CREATE: the palette (the system's twenty kept, the rest PC_RESERVED), the title */
        uint32_t at;
        int32_t (SC *entries)(uint32_t, uint32_t, uint32_t, void *) =
            (int32_t (SC *)(uint32_t, uint32_t, uint32_t, void *))IMPORT(GetPaletteEntries);
        G32(0x41bb60) = WIN(GetStockObject, uint32_t, (int32_t))(0xf);
        entries(G32(0x41bb60), 0, 0xa, VA(0x4171a4));
        entries(G32(0x41bb60), 0xa, 0xa, VA(0x41757c));
        for (at = 0x4171cf; at < 0x41757f; at += 4)
            G8(at) = 1;
        G32(0x41bb60) = WIN(CreatePalette, uint32_t, (const void *))(VA(0x4171a0));
        SERVICE(0x2a0, int32_t, (uint32_t, char *, uint32_t))(2, (char *)VA(0x41c480), 0x100);
        WIN(SetWindowTextA, int32_t, (uint32_t, const char *))(window, STR(0x41c480));
        WIN(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(window, 0);
        SEND(window, 0x282, 1, 0);
        break;
    }
    case 2:             /* WM_DESTROY: help closed, the menu options saved, full screen left, the end */
        fs98_host_rt_help(window, STR(0x41c000), 2, 0);
        WIN(wsprintfA, int32_t, (char *, const char *, ...))((char *)VA(0x41c480), STR(0x417a24), G32(0x418824));
        WRITE_PROFILE(STR(0x417074), STR(0x417048), STR(0x41c480));
        WIN(wsprintfA, int32_t, (char *, const char *, ...))((char *)VA(0x41c480), STR(0x417a24), G32(0x417198));
        WRITE_PROFILE(STR(0x417074), STR(0x417034), STR(0x41c480));
        if (G32(0x418854))
            fs98_host_rt_full_screen_off();
        WIN(DeleteObject, int32_t, (uint32_t))(G32(0x41bb60));
        WIN(PostQuitMessage, void, (int32_t))(0);
        return 0;
    case 3:             /* WM_MOVE */
        if (G32(0x41bb58) == 0)
            moved(window);
        break;
    case 5:             /* WM_SIZE */
        if (G32(0x41bb58) == 0)
            sized(window);
        break;
    case 6:             /* WM_ACTIVATE: a reopening asked for (0x46f) done now; the game told */
        if (w && G32(0x41bb50)) {
            G32(0x41bb50) = 0;
            POST(window, 0x46f, 0, 0);
        }
        if ((uint16_t)w == 0) {
            fs98_host_rt_hook(0x100f8, 0);
            G32(0x418870) = 1;
        } else if (G32(0x418870)) {
            G32(0x418870) = 0;
            fs98_host_rt_hook(0x100f9, 0);
        }
        break;
    case 0xf:           /* WM_PAINT, WM_NCPAINT */
    case 0x85:
        fs98_host_rt_covered();
        break;
    case 0x10:          /* WM_CLOSE: the timer stopped, the callbacks gone, the game told (0x418840) */
        WIN(timeKillEvent, uint32_t, (uint32_t))(G32(0x418818));
        WIN(timeEndPeriod, uint32_t, (uint32_t))(G32(0x418814));
        G32(0x418838) = 0;
        G32(0x418834) = 0;
        G32(0x418830) = 0;
        ((void (*)(void))G_PTR(0x418840))();
        break;
    case 0x15: {        /* WM_SYSCOLORCHANGE: the system's twenty colours again */
        int32_t (SC *entries)(uint32_t, uint32_t, uint32_t, void *) =
            (int32_t (SC *)(uint32_t, uint32_t, uint32_t, void *))IMPORT(GetPaletteEntries);
        int32_t (SC *set)(uint32_t, uint32_t, uint32_t, const void *) =
            (int32_t (SC *)(uint32_t, uint32_t, uint32_t, const void *))IMPORT(SetPaletteEntries);
        entries(WIN(GetStockObject, uint32_t, (int32_t))(0xf), 0, 0xa, VA(0x4171a4));
        entries(WIN(GetStockObject, uint32_t, (int32_t))(0xf), 0xa, 0xa, VA(0x41757c));
        set(G32(0x41bb60), 0, 0xa, VA(0x4171a4));
        set(G32(0x41bb60), 0xf6, 0xa, VA(0x41757c));
        SERVICE(0x290, int32_t, (uint32_t, uint32_t))(0, 0);
        break;
    }
    case 0x1c: {        /* WM_ACTIVATEAPP: full screen left while away and taken again on return */
        uint32_t active;
        if (G32(0x41bb58))
            break;
        active = w != 0;
        G32(0x418858) = active;
        if (active) {
            if (G32(0x418874)) {
                fs98_host_rt_full_screen_on();
                G32(0x418874) = 0;
            }
        } else if (G32(0x41a4e4) && !(G8(0x41a4ec) & 4)) {
            if (G32(0x418854) && G32(0x41886c)) {
                WIN(SetMenu, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, MENU_UNLESS(G32(0x417198)));
                G32(0x41886c) = 0;
            }
        } else if (G32(0x418854)) {
            G32(0x418874) = 1;
            fs98_host_rt_full_screen_off();
        }
        if (G32(0x418848) == 0)
            fs98_host_rt_pause_look(w == 0);
        SERVICE(0x2d0, int32_t, (uint32_t))(w);
        if (G32(0x418858))
            fs98_host_rt_open_directdraw();
        if (G32(0x417a1c))
            --G32(0x417a1c);
        else
            fs98_host_rt_hook(0x20011, w);
        WIN(EnumThreadWindows, int32_t, (uint32_t, void *, uint32_t))(WIN(GetCurrentThreadId, uint32_t, (void))(),
                                                                     (void *)fs98_host_rt_activate_floating, w);
        fs98_host_rt_covered();
        break;
    }
    case 0x20:          /* WM_SETCURSOR: the hourglass while busy (0x41885c) */
        if (G32(0x41885c)) {
            WIN(SetCursor, uint32_t, (uint32_t))(WIN(LoadCursorA, uint32_t, (uint32_t, uint32_t))(0, 0x7f02));
            return 1;
        }
        break;
    case 0x21:          /* WM_MOUSEACTIVATE: not by the left or right button while away */
        if (G32(0x418858) == 0 && (l & 0xffff) != 0 && (l & 0xffff) <= 2)
            return 2;
        break;
    case 0x48:          /* WM_POWER: no suspending while active */
        if (w == 1 && G32(0x418858))
            return -1;
        break;
    case 0x4a:          /* WM_COPYDATA */
        if (*(const uint32_t *)(uintptr_t)l == 0x10000000)
            fs98_host_rt_copied(w, (const uint32_t *)(uintptr_t)l);
        break;
    case 0x7e:          /* WM_DISPLAYCHANGE */
        ++G32(0x41ba2c);
        WIN(EnumThreadWindows, int32_t, (uint32_t, void *, uint32_t))(WIN(GetCurrentThreadId, uint32_t, (void))(),
                                                                     (void *)fs98_host_rt_recheck_topmost, l);
        break;
    case 0x100:         /* WM_KEYDOWN, WM_SYSKEYDOWN */
    case 0x104:
        if (key_down(window, w, l))
            return 0;
        break;
    case 0x111:         /* WM_COMMAND: the window's own, else the game's */
        if (fs98_host_rt_command(window, w & 0xffff, w >> 16, l))
            return 0;
        if (fs98_host_rt_hook(w, l))
            return 0;
        break;
    case 0x112:         /* WM_SYSCOMMAND: close is the game's (0x100c4); no screen saver while active */
        switch (w & 0xfff0) {
        case 0xf060:
            if (G32(0x41885c) == 0)
                fs98_host_rt_hook(0x100c4, 0);
            return 0;
        case 0xf140:
        case 0xf170:
            if (G32(0x418858))
                return 0;
            break;
        }
        break;
    case 0x116:         /* WM_INITMENU */
        if (G32(0x41a4f4))
            WIN(EnableMenuItem, int32_t, (uint32_t, uint32_t, uint32_t))(w, 0x1507, 1);
        break;
    case 0x200: case 0x201: case 0x202: case 0x203: case 0x204:
    case 0x205: case 0x206: case 0x207: case 0x208: case 0x209:
        /* the mouse: with mouse control (0x41881c), its movement from the window's centre to the
         * game (0x418820) and the cursor put back */
        if (G32(0x41881c)) {
            int32_t point[2], dx, dy;
            WIN(GetCursorPos, int32_t, (int32_t *))(point);
            dx = (int32_t)(G32(0x41c358) - (uint32_t)point[0]);
            dy = (int32_t)(G32(0x41c35c) - (uint32_t)point[1]);
            ((void (SC *)(int32_t, int32_t, uint32_t))G_PTR(0x418820))(dx, dy, w);
            if (dx || dy)
                WIN(SetCursorPos, int32_t, (int32_t, int32_t))(GI32(0x41c358), GI32(0x41c35c));
        }
        break;
    case 0x211:         /* WM_ENTERMENULOOP */
        if (G32(0x418848) == 0) {
            fs98_host_rt_full_screen_gdi(0);
            G32(0x418848) = 1;
            fs98_host_rt_hold_menu_bar();
            fs98_host_rt_hook(0x20000, 0);
            fs98_host_rt_draw_menu_bar();
        }
        break;
    case 0x212:         /* WM_EXITMENULOOP */
        if (G32(0x418848) == 1) {
            fs98_host_rt_hold_menu_bar();
            fs98_host_rt_hook(0x20001, 0);
            fs98_host_rt_draw_menu_bar();
            G32(0x418848) = 0;
            G32(0x41884c) = 1;
            if (G32(0x41886c)) {
                G32(0x41886c) = 0;
                WIN(SetMenu, int32_t, (uint32_t, uint32_t))(window, 0);
            }
            if (G32(0x418870)) {
                G32(0x418870) = 0;
                fs98_host_rt_hook(0x100f9, 0);
            }
            fs98_host_rt_full_screen_gdi(1);
        }
        break;
    case 0x231:         /* WM_ENTERSIZEMOVE */
        fs98_host_rt_full_screen_gdi(0);
        G32(0x418860) = 1;
        break;
    case 0x232:         /* WM_EXITSIZEMOVE */
        fs98_host_rt_full_screen_gdi(1);
        G32(0x418860) = 0;
        WIN(EnumChildWindows, int32_t, (uint32_t, void *, uint32_t))(window, (void *)fs98_host_rt_tell_size_move_ended, 0);
        break;
    case 0x30f:         /* WM_QUERYNEWPALETTE: put off while away (asked again, 1, 1), else realised */
        if (G32(0x418858) == 0 && w != 1 && l != 1) {
            POST(window, 0x30f, 1, 1);
            break;
        }
        if (G32(0x41bb60) && G32(0x418864) == 0) {
            uint32_t dc = WIN(GetDC, uint32_t, (uint32_t))(window), old;
            uint32_t (SC *select)(uint32_t, uint32_t, int32_t) =
                (uint32_t (SC *)(uint32_t, uint32_t, int32_t))IMPORT(SelectPalette);
            old = select(dc, G32(0x41bb60), 0);
            WIN(RealizePalette, uint32_t, (uint32_t))(dc);
            select(dc, old, 0);
            WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(window, dc);
        }
        break;
    case 0x311:         /* WM_PALETTECHANGED */
        if (G32(0x41a250))
            WIN(RedrawWindow, int32_t, (uint32_t, const void *, uint32_t, uint32_t))(window, 0, 0, 0x81);
        break;
    case 0x46b:         /* the game's first activation (posted by InitInstance) */
        {
            uint32_t popup = WIN(GetLastActivePopup, uint32_t, (uint32_t))(MAIN_WINDOW);
            if (WIN(GetForegroundWindow, uint32_t, (void))() == popup)
                SERVICE(0x2d0, int32_t, (uint32_t))(1);
        }
        break;
    case 0x46e:         /* a window gone: the main window active again if it was the active one */
        if (w && WIN(GetActiveWindow, uint32_t, (void))() == l)
            WIN(SetActiveWindow, uint32_t, (uint32_t))(MAIN_WINDOW);
        break;
    case 0x46f:         /* DirectDraw to be opened again (full screen again), now or when active */
        if (G32(0x418858) && G32(0x418848) == 0) {
            if (G32(0x418854)) {
                fs98_host_rt_full_screen_off();
                fs98_host_rt_full_screen_on();
            } else {
                fs98_host_rt_release_directdraw();
                fs98_host_rt_open_directdraw();
            }
            G32(0x41bb50) = 0;
        } else
            G32(0x41bb50) = 1;
        break;
    default:
        break;
    }
    return WIN(DefWindowProcA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l);
}
