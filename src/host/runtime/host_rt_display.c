/* The display mode: set on a thread of its own and waited for, the main window made the full
 * screen and back, and page flipping's part in a pause.
 *
 * Shared with the services: src/host/host.c calls these too, and its standalone builds compile
 * this file with HOST_RT_STANDALONE (host_rt.h), so there is one copy of each. */
#include "host_rt.h"
#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define MAIN_WINDOW G32(0x41880c)
#define POST(window, message, w, l) \
    WIN(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, message, w, l)
#define MENU_UNLESS(hidden) ((hidden) ? 0 : G32(0x418810))
#define DIRECTDRAW G_PTR(0x41a4e4)
#define COM(object, slot, result, parameters) ((result (SC *) parameters)PTR(PTR(object, 0), slot))
#define FULL_SCREEN_WANTED G32(0x418854)
#define FLIPPING G32(0x41a4f4)          /* a full screen with a back buffer */
#define SHOWN G32(0x418858)             /* 1 drawn by the game, 2 given back to GDI */
#define CLEARS G32(0x41a520)            /* frames left to clear the whole back buffer */
#define PALETTE_CHANGES G32(0x41ba2c)   /* counts the palette's changes */

/* 0x403890: a thread's body: the display mode set (CDS_FULLSCREEN); one that needs a restart
 * tried again with only its width and height. The first answer. */
int32_t SC fs98_host_rt_set_display_mode(uint8_t *mode)
{
    int32_t (SC *change)(void *, uint32_t) = (int32_t (SC *)(void *, uint32_t))IMPORT(ChangeDisplaySettingsA);
    int32_t result = change(mode, 4);
    if (result == 1 && mode != 0) {
        D32(mode, 0x28) = 0x180000;
        change(mode, 4);
    }
    return result;
}

/* 0x4038d0: the display mode set on a thread of its own (0x403890), and its answer; -1 if it
 * could not be had, or if DirectDraw is up without the GDI full screen (0x41a4ec bit 2). With
 * that bit the mode's depth is not asked for. The flags are not used. */
int32_t SC fs98_host_rt_change_display(uint8_t *mode, uint32_t flags)
{
    uint32_t result = 0xffffffffu, thread;
    uint32_t directdraw = G32(0x41a4e4);
    uint8_t options = G8(0x41a4ec);
    (void)flags;
    if (directdraw && !(options & 4))
        return -1;
    if (directdraw && (options & 4) && mode)
        D32(mode, 0x28) &= 0xfffbffffu;
    /* the thread's id is written over the mode's argument, as retail's */
    thread = WIN(CreateThread, uint32_t, (void *, uint32_t, void *, void *, uint32_t, void *))(
        0, 0, (void *)fs98_host_rt_set_display_mode, mode, 0, &mode);
    if (thread) {
        WIN(WaitForSingleObject, uint32_t, (uint32_t, uint32_t))(thread, 0xffffffffu);
        WIN(GetExitCodeThread, int32_t, (uint32_t, uint32_t *))(thread, &result);
        WIN(CloseHandle, int32_t, (uint32_t))(thread);
    }
    return (int32_t)result;
}

/* 0x403950: the main window made the full screen: its place, extended style and style kept
 * (0x41bb40, 0x41bb54, 0x41bb5c) and whether it was maximised (0x41bb3c); no caption or
 * frame, topmost and the screen's size; the menu hidden as HideMenuFullscreen says (always,
 * under DirectDraw's full screen); DirectDraw opened again, or the display mode set
 * (0x4183a8.. width, height and depth); then a new palette and size. 0. */
int32_t fs98_host_rt_full_screen_on(void)
{
    uint8_t mode[0x9c];
    uint32_t style;
    int32_t (SC *position)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) =
        (int32_t (SC *)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))IMPORT(SetWindowPos);
    if (G32(0x418854))
        return 0;
    G32(0x418854) = 1;
    G32(0x41bb58) = 1;
    G32(0x41bb3c) = WIN(IsZoomed, int32_t, (uint32_t))(MAIN_WINDOW);
    if (G32(0x41bb3c))
        WIN(ShowWindow, int32_t, (uint32_t, int32_t))(MAIN_WINDOW, 9);
    WIN(GetWindowRect, int32_t, (uint32_t, void *))(MAIN_WINDOW, VA(0x41bb40));
    G32(0x41bb54) = WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(MAIN_WINDOW, -20);
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(MAIN_WINDOW, -20, 0);
    style = WIN(GetWindowLongA, uint32_t, (uint32_t, int32_t))(MAIN_WINDOW, -16);
    G32(0x41bb5c) = style;
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(MAIN_WINDOW, -16, (style & 0xff38ffffu) | 0x80080000u);
    if (G32(0x41a4f4))
        G32(0x417198) = 1;
    WIN(SetMenu, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, MENU_UNLESS(G32(0x417198)));
    G32(0x41886c) = 0;
    {
        int32_t height = WIN(GetSystemMetrics, int32_t, (int32_t))(1);
        position(MAIN_WINDOW, 0, 0, 0, WIN(GetSystemMetrics, int32_t, (int32_t))(0), height, 0x34);
    }
    fs98_host_rt_release_directdraw();
    fs98_host_rt_open_directdraw();
    if (G32(0x41a4e4) == 0 || (G8(0x41a4ec) & 4)) {
        D32(mode, 0x6c) = G32(0x4183a8);
        D16(mode, 0x24) = 0x9c;
        D32(mode, 0x28) = 0x1c0000;
        D32(mode, 0x70) = G32(0x4183ac);
        D32(mode, 0x68) = G32(0x4183b0);
        fs98_host_rt_change_display(mode, 4);
    }
    {
        int32_t height = WIN(GetSystemMetrics, int32_t, (int32_t))(1);
        position(MAIN_WINDOW, 0, 0, 0, WIN(GetSystemMetrics, int32_t, (int32_t))(0), height, 0x34);
    }
    ++G32(0x41ba2c);
    G32(0x41bb58) = 0;
    POST(MAIN_WINDOW, 0x30f, 0, 0);
    POST(MAIN_WINDOW, 5, 0, 0);
    return 0;
}

/* 0x403b20: and back: the display mode restored (unless DirectDraw's full screen), the style
 * (topmost only while the main window is its own last popup... rather, WS_DISABLED off then;
 * visible either way), the menu as HideMenuNormal says, DirectDraw opened again, the place
 * and maximised state restored, not topmost; then a new palette and size. 0. */
int32_t fs98_host_rt_full_screen_off(void)
{
    uint32_t style;
    int32_t (SC *position)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) =
        (int32_t (SC *)(uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))IMPORT(SetWindowPos);
    if (G32(0x418854) == 0)
        return 0;
    G32(0x41bb58) = 1;
    if (G32(0x41a4e4) == 0 || (G8(0x41a4ec) & 4))
        fs98_host_rt_change_display(0, 4);
    if (WIN(GetLastActivePopup, uint32_t, (uint32_t))(MAIN_WINDOW) == MAIN_WINDOW)
        style = G32(0x41bb5c) & 0xf7ffffffu;
    else
        style = G32(0x41bb5c) | 0x8000000u;
    style |= 0x10000000u;
    G32(0x41bb5c) = style;
    WIN(SetWindowLongA, uint32_t, (uint32_t, int32_t, uint32_t))(MAIN_WINDOW, -16, style);
    WIN(SetMenu, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, MENU_UNLESS(G32(0x418824)));
    G32(0x41886c) = 0;
    G32(0x418854) = 0;
    fs98_host_rt_release_directdraw();
    fs98_host_rt_open_directdraw();
    position(MAIN_WINDOW, 0, GI32(0x41bb40), GI32(0x41bb44), (int32_t)(G32(0x41bb48) - G32(0x41bb40)),
             (int32_t)(G32(0x41bb4c) - G32(0x41bb44)), 0x34);
    position(MAIN_WINDOW, 0xfffffffeu, 0, 0, 0, 0, 0x13);
    if (G32(0x41bb3c))
        WIN(ShowWindow, int32_t, (uint32_t, int32_t))(MAIN_WINDOW, 3);
    ++G32(0x41ba2c);
    G32(0x41bb58) = 0;
    POST(MAIN_WINDOW, 0x30f, 0, 0);
    POST(MAIN_WINDOW, 5, 0, 0);
    return 0;
}

/* 0x413e80: in a flipping full screen, the game's drawing taken back (back, nonzero: shown
 * again, the whole back buffer cleared for three frames) or given to GDI (the palette marked
 * changed, the GDI surface flipped to, the main window redrawn). */
void SC fs98_host_rt_full_screen_gdi(int32_t back)
{
    void *directdraw;
    if (FULL_SCREEN_WANTED == 0 || FLIPPING == 0 || (directdraw = DIRECTDRAW) == 0 || SHOWN == 0)
        return;
    if (back) {
        if ((int32_t)SHOWN > 1)
            SHOWN = 1;
        CLEARS = 3;
        return;
    }
    if (SHOWN != 1)
        return;
    SHOWN = 2;
    ++PALETTE_CHANGES;
    COM(directdraw, 0x28, int32_t, (void *))(directdraw);
    CALL_STD(RedrawWindow, int32_t, (uint32_t, void *, uint32_t, uint32_t))(G32(0x41880c), 0, 0, 0x181);
}
