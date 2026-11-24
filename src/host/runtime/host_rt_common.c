/* The rest of what the services call too: the modeless dialogs a message is offered to, the
 * paused look (the grey palette and the dithered 216-colour cube), sound's start, the frames'
 * stacking, a message to every window, and the game's message hook.
 *
 * Shared with the services: src/host/host.c calls these too, and its standalone builds compile
 * this file with HOST_RT_STANDALONE (host_rt.h), so there is one copy of each. */
#include "host_rt.h"
#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
#define MAIN_WINDOW G32(0x41880c)
typedef int32_t (SC *window_callback)(uint32_t, uint32_t);
struct window_visit { window_callback callback; uint32_t parameter; };

/* 0x409c60: whether a modeless dialog took the message */
int32_t SC fs98_host_rt_modeless_took(void *message)
{
    return SERVICE(0x308, int32_t, (void *, void *, void *))(VA(0x417f00), message, (void *)fs98_host_rt_modeless_message) != 0;
}

/* 0x409ca0: +0x308's callback: a dialog whose window is gone released (its module, the pause
 * looked at again unless the module says not, +0xc bit 3) and taken off; the rest ticked
 * (0x464) and offered the message. The dialog that took it, or 0. */
uint32_t SC fs98_host_rt_modeless_message(uint32_t *entry, void *message)
{
    if (!WIN(IsWindow, int32_t, (uint32_t))(entry[1])) {
        fs98_host_rt_release_id(D32((void *)(uintptr_t)entry[0], 4));
        if (!(D8((void *)(uintptr_t)entry[0], 0xc) & 8))
            fs98_host_rt_pause_look(0);
        SERVICE(0x300, int32_t, (void *, void *))(VA(0x417f00), entry);
        return 0;
    }
    WIN(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(entry[1], 0x464, 0, 0);
    if (WIN(IsDialogMessageA, int32_t, (uint32_t, void *))(entry[1], message))
        return (uint32_t)(uintptr_t)entry;
    return 0;
}

/* 0x40e540: the BGR table at 0x41b620 from the palette's copy (0x4175ac): all grey at half
 * the brightness when paused (0x41a250), else the animated entries (10..105, 118..245) a dim
 * red of their brightness, the rest as they are. */
void fs98_host_rt_tint_palette(void)
{
    uint32_t at, grey = G32(0x41a250);
    for (at = 0; at < 0x400; at += 4) {
        int32_t red = G8(0x4175ac + at), green = G8(0x4175ad + at), blue = G8(0x4175ae + at);
        if (grey) {
            red = green = blue = (299 * red + 587 * green + 114 * blue) / 1000 / 2;
        } else if ((at >= 0x28 && at <= 0x1a4) || (at >= 0x1d8 && at < 0x3d8)) {
            int32_t light = (299 * red + 587 * green + 114 * blue) / 1000;
            red = 210 * light / 255;
            green = 53 * light / 255;
            blue = 0;
        }
        G8(0x41b622 + at) = (uint8_t)red;
        G8(0x41b621 + at) = (uint8_t)green;
        G8(0x41b620 + at) = (uint8_t)blue;
    }
}

/* 0x40e6f0: on, the 216-colour cube's DIB header at 0x41b1e0 (stamped with the display count
 * at 0x41b1c0) and, made once, a 16K table at 0x41ba28 dithering each palette entry (0x41adc0)
 * onto the cube at each of 8x8 places (the pattern at 0x4160c8); off, the table, the dithered
 * picture (0x41b608) and the halftone palette (0x41ba20) let go. */
void SC fs98_host_rt_cube(int32_t on)
{
    uint32_t red, green, blue, x, y, at, place;
    if (!on) {
        if (G32(0x41ba28)) {
            SERVICE(0x214, int32_t, (void *))(VA(0x41ba28));
            G32(0x41ba28) = 0;
        }
        if (G32(0x41b608)) {
            SERVICE(0x214, int32_t, (void *))(VA(0x41b608));
            G32(0x41b608) = 0;
        }
        if (G32(0x41ba20)) {
            WIN(DeleteObject, int32_t, (uint32_t))(G32(0x41ba20));
            G32(0x41ba20) = 0;
        }
        return;
    }
    G32(0x41b1c0) = G32(0x41ba2c);
    for (at = 0; at < 0x10a; ++at)
        G32(0x41b1e0 + at * 4) = 0;
    G32(0x41b1e0) = 0x28;
    G16(0x41b1ec) = 1;
    G16(0x41b1ee) = 8;
    G32(0x41b200) = 0xd8;
    place = 0x41b208;
    for (red = 0; red < 0x5fa; red += 0xff)
        for (green = 0; green < 0x5fa; green += 0xff)
            for (blue = 0; blue < 0x5fa; blue += 0xff, place += 4) {
                G8(place + 2) = (uint8_t)(blue / 5);
                G8(place + 1) = (uint8_t)(green / 5);
                G8(place) = (uint8_t)(red / 5);
            }
    if (G32(0x41ba28) == 0) {
        SERVICE(0x20c, int32_t, (uint32_t, uint32_t, void *, uint32_t))(4, 0x4000, VA(0x41ba28), 0);
        if (G32(0x41ba28) == 0)
            return;
    }
    for (y = 0, at = 0; y < 8; ++y)
        for (x = 0; x < 8; ++x) {
            uint32_t threshold = G8(0x4160c8 + (y & 7) * 8 + (x & 7)), entry;
            for (entry = 0; entry < 0x400; entry += 4) {
                uint32_t b = G8(0x41adc0 + entry), g = G8(0x41adc1 + entry), r = G8(0x41adc2 + entry), value;
                value = (uint8_t)((uint8_t)((b % 51 > threshold) + b / 51) * 6);
                value = (uint8_t)((uint8_t)(value + (g % 51 > threshold) + g / 51) * 6);
                value = (uint8_t)(value + (r % 51 > threshold) + r / 51);
                ((uint8_t *)G_PTR(0x41ba28))[at++] = (uint8_t)value;
            }
        }
}

/* 0x40e450: the paused look on or off: on a palette display, the grey palette (0x41a250), the
 * cube and the tint, the GDI palette realised again on the way out and the window redrawn;
 * elsewhere only the cube off; the game told (0x100f8 on, 0x100f9 off). */
void SC fs98_host_rt_pause_look(int32_t on)
{
    uint32_t dc, caps;
    ++G32(0x41ba2c);
    fs98_host_rt_full_screen_gdi(on == 0);
    dc = WIN(GetDC, uint32_t, (uint32_t))(MAIN_WINDOW);
    caps = WIN(GetDeviceCaps, uint32_t, (uint32_t, int32_t))(dc, 0x26);
    WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, dc);
    if (caps & 0x100) {
        GI32(0x41a250) = on;
        fs98_host_rt_cube(on);
        fs98_host_rt_tint_palette();
        if (!on) {
            dc = WIN(GetDC, uint32_t, (uint32_t))(MAIN_WINDOW);
            WIN(SelectPalette, uint32_t, (uint32_t, uint32_t, int32_t))(dc, G32(0x41bb60), on);
            WIN(RealizePalette, uint32_t, (uint32_t))(dc);
            WIN(ReleaseDC, int32_t, (uint32_t, uint32_t))(MAIN_WINDOW, dc);
        }
        WIN(RedrawWindow, int32_t, (uint32_t, const void *, uint32_t, uint32_t))(MAIN_WINDOW, 0, 0, 0x181);
    } else {
        G32(0x41a250) = 0;
        fs98_host_rt_cube(0);
    }
    fs98_host_rt_hook(on ? 0x100f8 : 0x100f9, 0);
}

/* 0x40f220: DirectSound started from the library at 0x41a3c4: made (0x41a3c8), cooperative
 * (2) with the main window, its primary buffer (0x41a2b8) made, given the format (0x41a264)
 * and played. 0x418308 is 0, or the step that failed (4..7). */
void fs98_host_rt_start_sound(void)
{
    int32_t (SC *create)(void *, void *, void *) =
        (int32_t (SC *)(void *, void *, void *))WIN(GetProcAddress, void *, (uint32_t, const char *))(G32(0x41a3c4), STR(0x41830c));
    uint32_t desc[5];
    void *sound, *buffer;
    if (create == 0) {
        G32(0x418308) = 4;
        return;
    }
    if (create(0, VA(0x41a3c8), 0) != 0) {
        G32(0x418308) = 5;
        return;
    }
    sound = G_PTR(0x41a3c8);
    if (((int32_t (SC *)(void *, uint32_t, uint32_t))PTR(PTR(sound, 0), 0x18))(sound, MAIN_WINDOW, 2) != 0) {
        G32(0x418308) = 6;
        return;
    }
    desc[1] = 0;
    desc[0] = 0x14;
    desc[2] = 0;
    desc[1] = 0x81;
    desc[3] = 0;
    desc[4] = 0;
    sound = G_PTR(0x41a3c8);
    if (((int32_t (SC *)(void *, void *, void *, void *))PTR(PTR(sound, 0), 0xc))(sound, desc, VA(0x41a2b8), 0) != 0) {
        /* RETAIL BUG (kept): HOST-18 -- DirectSound (0x41a3c8) stays set with no primary buffer */
        G32(0x418308) = 7;
        return;
    }
    buffer = G_PTR(0x41a2b8);
    ((int32_t (SC *)(void *, void *))PTR(PTR(buffer, 0), 0x38))(buffer, VA(0x41a264));
    buffer = G_PTR(0x41a2b8);
    ((int32_t (SC *)(void *, void *, uint32_t, void *))PTR(PTR(buffer, 0), 0x14))(buffer, VA(0x41a264), 0x12, 0);
    buffer = G_PTR(0x41a2b8);
    ((int32_t (SC *)(void *, uint32_t, uint32_t, uint32_t))PTR(PTR(buffer, 0), 0x30))(buffer, 0, 0, 1);
    G32(0x418308) = 0;
}

/* 0x411650: the main window's frames restacked, group by group (0x41a3dc), each shown one
 * after the one before; not again while under way (0x41a3e8). */
void fs98_host_rt_restack(void)
{
    uint32_t after = 1, defer, group;
    uint32_t (SC *place)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t) =
        (uint32_t (SC *)(uint32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))
            IMPORT(DeferWindowPos);
    if (G32(0x41a3e8))
        return;
    G32(0x41a3e8) = 1;
    defer = WIN(BeginDeferWindowPos, uint32_t, (int32_t))(1);
    for (group = G32(0x41a3dc); group; group = D32((void *)(uintptr_t)group, 8)) {
        uint8_t *node;
        for (node = (uint8_t *)PTR((void *)(uintptr_t)group, 4); node; node = (uint8_t *)PTR(node, 4)) {
            uint8_t *data = (uint8_t *)PTR(node, 0);
            if (!(D32(data, 8) & 0x80000000))
                continue;
            /* RETAIL BUG (kept): HOST-19 -- the record's host block is read unchecked */
            if (WIN(GetParent, uint32_t, (uint32_t))(D32(PTR(data, 0xa0), 0x24)) != MAIN_WINDOW)
                continue;
            defer = place(defer, D32(PTR(PTR(node, 0), 0xa0), 0x24), after, 0, 0, 0, 0, 0x13);
            after = D32(PTR(PTR(node, 0), 0xa0), 0x24);
        }
    }
    WIN(EndDeferWindowPos, int32_t, (uint32_t))(defer);
    G32(0x41a3e8) = 0;
}

/* 0x4078b0: EnumThreadWindows's callback: a floating window ("FS98FLOAT") is visited. */
int32_t SC fs98_host_rt_visit_floating(uint32_t window, struct window_visit *visit)
{
    char name[0x100];
    CALL_STD(GetClassNameA, int32_t, (uint32_t, char *, int32_t))(window, name, 0xff);
    if (CALL_C(_strcmpi, int32_t, (const char *, const char *))(name, STR(0x417178)) == 0)
        visit->callback(window, visit->parameter);
    return 1;
}

/* 0x407850: call back for every child of a window, then for the thread's floating windows. */
void SC fs98_host_rt_for_each_window(uint32_t window, window_callback callback, uint32_t parameter)
{
    struct window_visit visit;
    if (window == 0 || !CALL_STD(IsWindow, int32_t, (uint32_t))(window))
        return;
    CALL_STD(EnumChildWindows, int32_t, (uint32_t, window_callback, uint32_t))(window, callback, parameter);
    visit.callback = callback;
    visit.parameter = parameter;
    CALL_STD(EnumThreadWindows, int32_t, (uint32_t, void *, void *))(
        CALL_STD(GetCurrentThreadId, uint32_t, (void))(), (void *)fs98_host_rt_visit_floating, &visit);
}

/* 0x401a30: the game's message hook (0x418834, set with the frame callback by 0x4084e0)
 * called, if there is one: its result, else 0. */
int32_t SC fs98_host_rt_hook(uint32_t message, uint32_t parameter)
{
    int32_t (SC *hook)(uint32_t, uint32_t) = (int32_t (SC *)(uint32_t, uint32_t))G_PTR(0x418834);
    if (hook == 0)
        return 0;
    return hook(message, parameter);
}
