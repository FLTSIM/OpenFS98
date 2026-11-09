/* The DirectDraw device list: naming the devices DirectDrawEnumerate reports, and picking the
 * one the configuration names; then opening DirectDraw and its surfaces.
 *
 * Written twice until the runtime is linked with the services: src/host/host.c has 0x4121a0,
 * 0x412210, 0x412250, 0x412340 and 0x412420 too, for the display-settings service +0x31c
 * (docs/remaining-work.md, "Routines written twice"). A change to one must go to both. */
#include "host_rt.h"

/* 0x4121a0: a device's name for the list. A driver ("\\.\DISPLAY2") is shown as
 * "description (driver)", the primary display by its description; then " " and the 3D
 * device's name, if there is one. */
void SC fs98_host_rt_device_name(char *out, const char *description, const char *driver, const char *name)
{
    if (driver[0] == '\\' && driver[1] == '\\' && driver[2] == '.')
        CALL_C(wsprintfA, int32_t, (char *, const char *, ...))(out, STR(0x4183bc), description, driver);
    else
        CALL_STD(lstrcpyA, char *, (char *, const char *))(out, description);
    if (name != 0 && name[0] != 0) {
        CALL_STD(lstrcatA, char *, (char *, const char *))(out, STR(0x4183b8));
        CALL_STD(lstrcatA, char *, (char *, const char *))(out, name);
    }
}

/* 0x412210: whether a device is the display itself: no GUID, a driver named "display", or a
 * "\\." device name. The second argument is not read. */
int32_t SC fs98_host_rt_device_is(const void *guid, const char *unused, const char *driver)
{
    (void)unused;
    if (guid == 0)
        return 1;
    if (CALL_STD(lstrcmpiA, int32_t, (const char *, const char *))(driver, STR(0x4183c4)) == 0)
        return 1;
    return driver[0] == '\\' && driver[1] == '\\' && driver[2] == '.';
}

/* The device a search through DirectDrawEnumerate looks for (0x412420's frame). */
struct device_search {
    int32_t result;         /* DirectDrawCreate's, 0x80004005 until one is tried */
    char name[0x80];        /* the configured device, its 3D suffix cut off */
    void **object;          /* where the IDirectDraw goes */
    uint32_t found;         /* 1 the display, 4 another device, 3 the primary (no GUID) */
};

/* 0x412340: the 3D suffix a device name ends with -- "(Direct3D)", "(Direct3D MMX)",
 * "(Direct3D RGB)" or "(Direct3D RAMP)" -- copied to out if there is one to copy to, and with
 * cut the name ended before it (the character before the suffix, its space). Returns the
 * suffix in the name, or an empty string. */
char *SC fs98_host_rt_device_suffix(char *name, char *out, int32_t cut)
{
    static const uint32_t suffixes[] = {0x4183fc, 0x4183ec, 0x4183dc, 0x4183cc};
    char *found = 0;
    uint32_t at;
    for (at = 0; at < 4 && found == 0; ++at) {
        int32_t length = CALL_STD(lstrlenA, int32_t, (const char *))(name);
        int32_t suffix = CALL_STD(lstrlenA, int32_t, (const char *))(STR(suffixes[at]));
        if (length > suffix) {
            char *tail = name + length - suffix;
            if (CALL_STD(lstrcmpA, int32_t, (const char *, const char *))(tail, STR(suffixes[at])) == 0)
                found = tail;
        }
    }
    if (found == 0)
        found = (char *)VA(0x4187f8);
    else if (cut)
        found[-1] = 0;
    if (out != 0)
        CALL_STD(lstrcpyA, char *, (char *, const char *))(out, found);
    return found;
}

/* 0x412250: DirectDrawEnumerate's callback. A device whose driver or listed name is the
 * one searched for is created; enumeration goes on (1) until one is. */
int32_t SC fs98_host_rt_device_found(void *guid, char *description, char *driver, struct device_search *search)
{
    char shown[0x80];
    fs98_host_rt_device_name(shown, description, driver, 0);
    if (CALL_STD(lstrcmpiA, int32_t, (const char *, const char *))(search->name, driver) != 0 &&
        CALL_STD(lstrcmpiA, int32_t, (const char *, const char *))(search->name, shown) != 0)
        return 1;
    search->result = CALL_STD(DirectDrawCreate, int32_t, (void *, void **, void *))(guid, search->object, 0);
    if (search->result != 0)
        return 1;
    if (guid == 0)
        search->found |= 3;
    else if (fs98_host_rt_device_is(guid, description, driver))
        search->found |= 1;
    else
        search->found |= 4;
    return 0;
}

/* 0x412420: create the IDirectDraw for the named device (by its driver or listed name), or
 * for the primary display if none is named. Returns what was found: 3 the primary, 1 the
 * display, 4 another device, 0 none. */
uint32_t SC fs98_host_rt_device_create(const char *name, void **object)
{
    struct device_search search;
    if (object == 0)
        return 0;
    *object = 0;
    if (name == 0 || name[0] == 0)
        return CALL_STD(DirectDrawCreate, int32_t, (void *, void **, void *))(0, object, 0) == 0 ? 3 : 0;
    search.object = object;
    search.found = 0;
    search.result = (int32_t)0x80004005;
    CALL_STD(lstrcpyA, char *, (char *, const char *))(search.name, name);
    fs98_host_rt_device_suffix(search.name, 0, 1);
    CALL_STD(DirectDrawEnumerateA, int32_t, (void *, void *))((void *)fs98_host_rt_device_found, &search);
    return search.found;
}

#define DIRECTDRAW G_PTR(0x41a4e4)     /* the IDirectDraw */
#define DIRECT3D G_PTR(0x41a4dc)       /* the IDirect3D2 */
#define BACK_BUFFER G_PTR(0x41a4e0)
#define PRIMARY G_PTR(0x41a4f8)
#define OVERLAY G_PTR(0x41a4fc)
#define PALETTE G_PTR(0x41a4e8)
/* a COM method, by its byte offset in the vtable; this is the first argument */
#define COM(object, slot, result, parameters) ((result (SC *) parameters)PTR(PTR(object, 0), slot))

/* 0x412bf0: the display's width, height and colour depth: the primary surface's (15 for a
 * 16-bit 5-5-5 format), or without one the screen DC's. */
void SC fs98_host_rt_display_size(int32_t *width, int32_t *height, int32_t *depth)
{
    if (DIRECTDRAW != 0 && PRIMARY != 0) {
        uint32_t desc[0x1b] = {0};
        void *primary = PRIMARY;
        desc[0] = 0x6c;
        COM(primary, 0x58, int32_t, (void *, void *))(primary, desc);
        *width = (int32_t)desc[3];
        *height = (int32_t)desc[2];
        *depth = (int32_t)desc[0x15];
        if (desc[0x15] == 0x10 && desc[0x16] == 0x7c00 && desc[0x17] == 0x3e0 && desc[0x18] == 0x1f)
            *depth = 15;
        return;
    }
    {
        uint32_t dc = CALL_STD(GetDC, uint32_t, (uint32_t))(0);
        int32_t bits;
        *width = CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 8);
        *height = CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 10);
        bits = CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 12);
        *depth = bits * CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc, 14);
        CALL_STD(ReleaseDC, int32_t, (uint32_t, uint32_t))(0, dc);
    }
}

/* 0x4124d0: whether the DirectDraw device can do 3D in hardware: its caps have DDCAPS_3D,
 * not the bank-switched flag (ignored past 8 bits a pixel when there is an IDirect3D2) nor
 * no-2D-during-3D, video memory of none or at least 0x25800, and DDCAPS_BLTSTRETCH. */
int32_t fs98_host_rt_device_3d(void)
{
    uint32_t caps[0x5b] = {0}, flags;
    void *directdraw = DIRECTDRAW;
    caps[0] = 0x16c;
    COM(directdraw, 0x2c, int32_t, (void *, void *, void *))(directdraw, caps, 0);
    flags = caps[1];
    if (flags & 1) {
        if (!(flags & 0x8000000))
            goto checked;
        {
            int32_t width, height, depth;
            fs98_host_rt_display_size(&width, &height, &depth);
            flags = caps[1];
            if (DIRECT3D != 0 && depth > 8)
                flags &= ~0x8000000u;
        }
    }
    if (flags & 0x8000000)
        return 0;
checked:
    if (flags & 0x2000000)
        return 0;
    if (caps[0x10] != 0 && caps[0x10] < 0x25800)
        return 0;
    return (int32_t)((flags & 0xff) >> 6 & 1);
}

/* 0x412a20: tell a window the DirectDraw object is going (0x46d, sent) or about some other
 * change (posted), with the object. */
int32_t SC fs98_host_rt_tell_window(uint32_t window, uint32_t message)
{
    if (message == 0x46d)
        CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, void *))(window, 0x46d, 0, DIRECTDRAW);
    else
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, void *))(window, message, 0, DIRECTDRAW);
    return 1;
}

/* 0x4140b0: back to the GDI surface, and the GDI objects drawn with in full screen deleted. */
void fs98_host_rt_leave_full_screen(void)
{
    void *directdraw = DIRECTDRAW;
    COM(directdraw, 0x28, int32_t, (void *))(directdraw);
    if (G32(0x41a53c) == 0)
        return;
    CALL_STD(DeleteObject, int32_t, (uint32_t))(G32(0x41a53c));
    G32(0x41a53c) = 0;
    CALL_STD(DeleteObject, int32_t, (uint32_t))(G32(0x41a984));
    G32(0x41a984) = 0;
    CALL_STD(DeleteObject, int32_t, (uint32_t))(G32(0x41a968));
    G32(0x41a968) = 0;
    G32(0x41a540) = 0;
}

static void release(uint32_t va)
{
    void *object = G_PTR(va);
    if (object != 0) {
        COM(object, 8, uint32_t, (void *))(object);
        G_PTR(va) = 0;
    }
}

/* 0x412a70: the surfaces and palette released, the windows told first. */
void fs98_host_rt_release_surfaces(void)
{
    if (PRIMARY == 0)
        return;
    fs98_host_rt_for_each_window(G32(0x41880c), (void *)fs98_host_rt_tell_window, 0x46d);
    if (G32(0x41a4f4) != 0) {
        fs98_host_rt_leave_full_screen();
        G32(0x41a4f4) = 0;
    }
    G32(0x41a4f0) = 0;
    release(0x41a4fc);
    release(0x41a4e0);
    release(0x41a4f8);
    release(0x41a4e8);
}

/* 0x412b10: everything DirectDraw released. */
void fs98_host_rt_release_directdraw(void)
{
    fs98_host_rt_release_surfaces();
    release(0x41a4dc);
    release(0x41a4e4);
}

/* 0x412590: IDirect3D2::EnumDevices's callback: the first hardware device's description
 * (a hardware one), or a software one that has the flag 2 in its caps when no hardware one
 * was found, is kept. */
int32_t SC fs98_host_rt_d3d_device_found(void *guid, char *description, char *name, uint32_t *hardware,
                                          uint32_t *software, uint32_t *kept)
{
    (void)description; (void)name;
    if (guid == 0)
        return 1;
    if (hardware != 0 && hardware[1] != 0)
        fs98_host_rt_copy_words(kept, hardware, 0x33);
    if (software != 0 && software[1] != 0 && (software[2] & 2) && kept[1] == 0)
        fs98_host_rt_copy_words(kept, software, 0x33);
    return 1;
}

/* 0x4125f0: whether there is a usable Direct3D hardware device: the options allow it
 * (0x41a4c0 bit 2) and 3D is on, a hardware device with the colour-model bit, and the
 * DirectDraw device's video memory none or at least 0x14b000. */
int32_t fs98_host_rt_d3d_hardware(void)
{
    uint32_t device[0x33] = {0};
    if (!(G8(0x41a4c0) & 4) || G8(0x41a470) == 0)
        return 0;
    {
        void *direct3d = DIRECT3D;
        COM(direct3d, 0xc, int32_t, (void *, void *, void *))(direct3d, (void *)fs98_host_rt_d3d_device_found, device);
    }
    if (device[1] == 0 || !(((uint8_t *)device)[0x84] & 1))
        return 0;
    {
        uint32_t caps[0x5b];
        void *directdraw = DIRECTDRAW;
        caps[0] = 0x16c;
        COM(directdraw, 0x2c, int32_t, (void *, void *, void *))(directdraw, caps, 0);
        return caps[0xf] == 0 || caps[0xf] >= 0x14b000;
    }
}

/* 0x412b50: the 8-bit palette set from entries, when the primary surface is a palettised
 * full screen (0x418854) and the palette is the game's to set (0x41a4ec bit 1 without bit 2):
 * created the first time, its entries set after; then given to the primary surface. */
void SC fs98_host_rt_set_palette(void *entries)
{
    int32_t width, height, depth;
    void *directdraw;
    fs98_host_rt_display_size(&width, &height, &depth);
    directdraw = DIRECTDRAW;
    if (directdraw == 0 || PRIMARY == 0 || G32(0x418854) == 0 || depth != 8)
        return;
    if (!(G8(0x41a4ec) & 1) || (G8(0x41a4ec) & 2))
        return;
    if (PALETTE == 0)
        COM(directdraw, 0x14, int32_t, (void *, uint32_t, void *, void *, void *))(directdraw, 4, entries, VA(0x41a4e8), 0);
    else
        COM(PALETTE, 0x18, int32_t, (void *, uint32_t, uint32_t, uint32_t, void *))(PALETTE, 0, 0, 0x100, entries);
    {
        void *primary = PRIMARY;
        COM(primary, 0x7c, int32_t, (void *, void *))(primary, PALETTE);
    }
}

/* 0x412fb0: a surface locked (with the lock flags at 0x41a4d8), restored and locked again if
 * the first lock fails; its memory and pitch given back. Returns whether it is locked. */
int32_t SC fs98_host_rt_lock(void *surface, void **memory, int32_t *pitch)
{
    uint32_t desc[0x1b];
    int32_t (SC *lock)(void *, void *, void *, uint32_t, void *) =
        (int32_t (SC *)(void *, void *, void *, uint32_t, void *))PTR(PTR(surface, 0), 0x64);
    int32_t result;
    desc[0] = 0x6c;
    result = lock(surface, 0, desc, G32(0x41a4d8), 0);
    if (result != 0) {
        result = COM(surface, 0x6c, int32_t, (void *))(surface);
        if (result != 0)
            return 0;
        result = lock(surface, 0, desc, G32(0x41a4d8), 0);
        if (result != 0)
            return 0;
    }
    *memory = (void *)desc[9];
    *pitch = (int32_t)desc[4];
    return 1;
}

/* 0x413030: a surface unlocked. Returns whether it was. */
int32_t SC fs98_host_rt_unlock(void *surface)
{
    return COM(surface, 0x80, int32_t, (void *, void *))(surface, 0) == 0;
}

/* 0x413050: a rectangle of a surface filled with a colour (DDBLT_COLORFILL, waiting). Only the
 * DDBLTFX's size and fill colour are set; the rest is the stack's. */
int32_t SC fs98_host_rt_fill(void *surface, void *rect, uint32_t colour)
{
    uint32_t fx[0x19];
    fx[0x14] = colour;
    fx[0] = 0x64;
    return COM(surface, 0x14, int32_t, (void *, void *, void *, void *, uint32_t, void *))(
               surface, rect, 0, 0, 0x1000400, fx) == 0;
}

/* 0x413090: a rectangle (x, y, width, height) filled, clipped to the surface first. */
int32_t SC fs98_host_rt_fill_clipped(void *surface, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t colour)
{
    int32_t rect[4];
    uint32_t desc[0x1b];
    desc[0] = 0x6c;
    COM(surface, 0x58, int32_t, (void *, void *))(surface, desc);
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (width + x > (int32_t)desc[3])
        width = (int32_t)desc[3] - x;
    if (height + y > (int32_t)desc[2])
        height = (int32_t)desc[2] - y;
    rect[1] = y;
    rect[0] = x;
    rect[2] = width + x;
    rect[3] = height + y;
    return fs98_host_rt_fill(surface, rect, colour);
}

/* 0x413130: a rectangle of one surface copied to another, with the source's colour key if
 * keyed. The back buffer to the primary surface with an overlay goes by the overlay instead:
 * shown at the destination rectangle, then flipped. A lost surface is restored, both of them.
 * Returns whether it worked (with a lost surface: whether the source was restored). */
int32_t SC fs98_host_rt_blit(void *to, int32_t to_x, int32_t to_y, int32_t to_width, int32_t to_height, void *from,
                             int32_t from_x, int32_t from_y, int32_t from_width, int32_t from_height, int32_t keyed)
{
    int32_t from_rect[4], to_rect[4], result;
    uint32_t flags = (keyed ? 0x8000u : 0) | 0x1000000u;
    from_rect[0] = from_x;
    from_rect[1] = from_y;
    from_rect[2] = from_x + from_width;
    from_rect[3] = from_y + from_height;
    to_rect[0] = to_x;
    to_rect[1] = to_y;
    to_rect[2] = to_x + to_width;
    to_rect[3] = to_y + to_height;
    if (OVERLAY != 0 && to == PRIMARY && from == BACK_BUFFER) {
        void *overlay = OVERLAY;
        COM(overlay, 0x84, int32_t, (void *, void *, void *, void *, uint32_t, void *))(
            overlay, from_rect, PRIMARY, to_rect, 0x4000, 0);
        overlay = OVERLAY;
        result = COM(overlay, 0x2c, int32_t, (void *, void *, uint32_t))(overlay, 0, 1);
    } else
        result = COM(to, 0x14, int32_t, (void *, void *, void *, void *, uint32_t, void *))(
            to, to_rect, from, from_rect, flags, 0);
    if (result == (int32_t)0x887601c2) {
        COM(to, 0x6c, int32_t, (void *))(to);
        result = COM(from, 0x6c, int32_t, (void *))(from);
    }
    return result == 0;
}

#define FULL_SCREEN G32(0x418854)       /* the game wants full screen */
#define MAIN_WINDOW G32(0x41880c)
#define DISPLAY_SETTINGS ((uint8_t *)VA(0x41a3f0))
#define MODE_WIDTH G32(0x4183a8)
#define MODE_HEIGHT G32(0x4183ac)
#define MODE_DEPTH G32(0x4183b0)

/* 0x412690: DirectDraw opened, or reopened: with the surfaces there, restoring them is
 * enough (and in a window without 0x418858 nothing is done); else the settings read, the
 * configured device (or the primary display) created, its cooperative level set for full
 * screen or a window, the IDirect3D2 asked for, the mode set in full screen (640x480 at 16
 * bits, or 8 without Direct3D, when the configured one fails), Direct3D dropped without
 * hardware for it, and the primary surface made -- with a back buffer when flipping
 * (0x41a4c0 bit 0x40, set by a device that is not the display) in full screen. Then the
 * palette set and the windows told (0x46c). 1 when open; on failure everything released, 0.
 * 0x41a500 counts the calls under way: a call during SetCooperativeLevel does nothing. */
int32_t fs98_host_rt_open_directdraw(void)
{
    uint32_t desc[0x1b];
    int32_t result;
    uint32_t at;
    if (G32(0x41a500) != 0)
        return 1;
    if (G32(0x4183b4) != 0)
        G32(0x4183b4) = (uint32_t)CALL_STD(GetSystemMetrics, int32_t, (int32_t))(0x50);
    if (DIRECTDRAW != 0 && PRIMARY != 0) {
        void *primary = PRIMARY;
        if (G32(0x418858) == 0)
            return 0;
        if (COM(primary, 0x6c, int32_t, (void *))(primary) == 0) {
            void *back = BACK_BUFFER;
            if (back == 0 || COM(back, 0x6c, int32_t, (void *))(back) == 0) {
                result = 1;
                goto restored;
            }
        }
    }
    fs98_host_rt_read_display(DISPLAY_SETTINGS, 0);
    if (G8(0x41a4c0) & 1) {
        fs98_host_rt_release_directdraw();
        return 0;
    }
    MODE_HEIGHT = G32(0x41a4c8);
    MODE_WIDTH = G32(0x41a4c4);
    MODE_DEPTH = G32(0x41a4cc);
    if (DIRECTDRAW == 0) {
        void *directdraw;
        int32_t failed;
        if (FULL_SCREEN != 0) {
            G32(0x41a4ec) = fs98_host_rt_device_create((const char *)DISPLAY_SETTINGS, (void **)VA(0x41a4e4));
            if (DIRECTDRAW != 0 && (G8(0x41a4ec) & 4))
                G8(0x41a4c0) |= 0x40;
        }
        if (DIRECTDRAW == 0) {
            G32(0x41a4ec) = fs98_host_rt_device_create(0, (void **)VA(0x41a4e4));
            if (DIRECTDRAW == 0)
                return 0;
        }
        directdraw = DIRECTDRAW;
        ++G32(0x41a500);
        failed = COM(directdraw, 0x50, int32_t, (void *, uint32_t, uint32_t))(directdraw, MAIN_WINDOW,
                                                                             FULL_SCREEN != 0 ? 0x11 : 8);
        --G32(0x41a500);
        if (failed) {
            fs98_host_rt_release_directdraw();
            return 0;
        }
        if (!(G8(0x41a4c0) & 0x40) || FULL_SCREEN == 0) {
            uint32_t (SC *set_error_mode)(uint32_t) =
                (uint32_t (SC *)(uint32_t))fs98_host_rt_imports[HOST_RT_IMPORT_SetErrorMode];
            set_error_mode(set_error_mode(0) & ~2u);
        }
        directdraw = DIRECTDRAW;
        COM(directdraw, 0, int32_t, (void *, void *, void *))(directdraw, VA(0x416130), VA(0x41a4dc));
        if (DIRECT3D == 0) {
            directdraw = DIRECTDRAW;
            if (directdraw == 0)
                return 0;
            COM(directdraw, 8, uint32_t, (void *))(directdraw);
            G_PTR(0x41a4e4) = 0;
            return 0;
        }
        G32(0x41a4d8) = 0x801;
        if (FULL_SCREEN != 0) {
            int32_t (SC *set_mode)(void *, uint32_t, uint32_t, uint32_t);
            if ((G8(0x41a4ec) & 4) && (MODE_WIDTH != 0x280 || MODE_HEIGHT != 0x1e0)) {
                directdraw = DIRECTDRAW;
                set_mode = (int32_t (SC *)(void *, uint32_t, uint32_t, uint32_t))PTR(PTR(directdraw, 0), 0x54);
                set_mode(directdraw, 0x280, 0x1e0, MODE_DEPTH);
            }
            directdraw = DIRECTDRAW;
            set_mode = (int32_t (SC *)(void *, uint32_t, uint32_t, uint32_t))PTR(PTR(directdraw, 0), 0x54);
            if (set_mode(directdraw, MODE_WIDTH, MODE_HEIGHT, MODE_DEPTH) != 0) {
                uint32_t depth = DIRECT3D != 0 ? 0x10 : 8;
                directdraw = DIRECTDRAW;
                set_mode = (int32_t (SC *)(void *, uint32_t, uint32_t, uint32_t))PTR(PTR(directdraw, 0), 0x54);
                MODE_WIDTH = 0x280;
                MODE_HEIGHT = 0x1e0;
                MODE_DEPTH = depth;
                if (set_mode(directdraw, 0x280, 0x1e0, depth) != 0) {
                    fs98_host_rt_release_directdraw();
                    return 0;
                }
            }
        }
        if (!fs98_host_rt_d3d_hardware() && DIRECT3D != 0) {
            void *direct3d = DIRECT3D;
            COM(direct3d, 8, uint32_t, (void *))(direct3d);
            G_PTR(0x41a4dc) = 0;
        }
    }
    fs98_host_rt_release_surfaces();
    for (at = 0; at < 0x1b; ++at)
        desc[at] = 0;
    result = 1;
    desc[0] = 0x6c;
    desc[1] = 1;                    /* DDSD_CAPS */
    desc[0x1a] = 0x200;             /* DDSCAPS_PRIMARYSURFACE */
    if (FULL_SCREEN != 0 && (G8(0x41a4c0) & 0x40)) {
        desc[1] = 0x21;             /* and DDSD_BACKBUFFERCOUNT */
        desc[0x1a] = 0x6218;        /* and FLIP, COMPLEX, 3DDEVICE, VIDEOMEMORY */
        desc[5] = 1;
        G32(0x41a4f4) = 1;
    } else
        G32(0x41a4f4) = 0;
    {
        void *directdraw = DIRECTDRAW;
        if (COM(directdraw, 0x18, int32_t, (void *, void *, void *, void *))(directdraw, desc, VA(0x41a4f8), 0) != 0) {
            fs98_host_rt_release_directdraw();
            return 0;
        }
    }
    if (FULL_SCREEN != 0 && G32(0x41a4f4) != 0)
        fs98_host_rt_fill(PRIMARY, 0, 0);
restored:
    fs98_host_rt_set_palette(VA(0x4171a4));
    fs98_host_rt_for_each_window(MAIN_WINDOW, (void *)fs98_host_rt_tell_window, 0x46c);
    return result;
}

/* 0x412ce0: the surface the 3D view draws in, width by height, cleared to 0, or 0 if there
 * is none: with DirectDraw open, a display of at most 16 bits that is 8 only without
 * Direct3D or with 0x41a4c0 bit 2, and a device for 3D. Flipping in full screen it is the
 * back buffer; else an offscreen surface (3D in video memory with Direct3D) -- or, on an
 * 8-bit display with Direct3D and 0x41a4c0 bit 0x200, a 16-bit 5-5-5 overlay with a back
 * buffer, when the device has overlays and at most 1000 of something (its caps' +0x88). */
void *SC fs98_host_rt_render_surface(int32_t width, int32_t height)
{
    void *surface = 0;
    uint32_t caps_wanted[1];
    int32_t display_width, display_height, depth;
    uint32_t desc[0x1b], caps[0x5b];
    int32_t result = (int32_t)0x80004005;
    uint32_t at;
    if (DIRECTDRAW == 0 || PRIMARY == 0)
        return 0;
    fs98_host_rt_display_size(&display_width, &display_height, &depth);
    if (depth == 8 && !(G8(0x41a4c0) & 2))
        return 0;
    if (DIRECT3D == 0) {
        if (depth != 8)
            return 0;
    } else if (depth > 16)
        return 0;
    if (!fs98_host_rt_device_3d())
        return 0;
    if (FULL_SCREEN != 0 && G32(0x41a4f4) != 0) {
        void *primary = PRIMARY;
        caps_wanted[0] = 4;     /* DDSCAPS_BACKBUFFER */
        COM(primary, 0x30, int32_t, (void *, void *, void *))(primary, caps_wanted, &surface);
        goto made;
    }
    for (at = 0; at < 0x1b; ++at)
        desc[at] = 0;
    desc[0] = 0x6c;
    desc[1] = 7;                /* DDSD_CAPS, HEIGHT, WIDTH */
    desc[0x1a] = DIRECT3D != 0 ? 0x6040 : 0x4040;
    desc[2] = (uint32_t)height;
    desc[3] = (uint32_t)width;
    if (OVERLAY != 0) {
        void *overlay = OVERLAY;
        COM(overlay, 8, uint32_t, (void *))(overlay);
        G_PTR(0x41a4fc) = 0;
    }
    if (DIRECT3D != 0 && depth == 8 && (G32(0x41a4c0) & 0x200)) {
        void *directdraw = DIRECTDRAW;
        caps[0] = 0x16c;
        COM(directdraw, 0x2c, int32_t, (void *, void *, void *))(directdraw, caps, 0);
        if ((caps[1] & 0x800) && caps[0x22] <= 1000) {
            uint32_t flags = desc[1];
            desc[0x1a] = (desc[0x1a] & ~0x40u) | 0x98;
            directdraw = DIRECTDRAW;
            desc[0x12] = 0x20;              /* the pixel format: RGB, 16 bits, 5-5-5 and a 1-bit alpha */
            desc[0x13] = 0x41;
            desc[0x14] = 0;
            desc[0x15] = 0x10;
            desc[0x16] = 0x7c00;
            desc[0x17] = 0x3e0;
            desc[0x18] = 0x1f;
            desc[0x19] = 0x8000;
            desc[1] = flags | 0x1020;
            desc[5] = 1;
            result = COM(directdraw, 0x18, int32_t, (void *, void *, void *, void *))(directdraw, desc, VA(0x41a4fc), 0);
            if (result == 0) {
                void *overlay = OVERLAY;
                caps_wanted[0] = 4;
                result = COM(overlay, 0x30, int32_t, (void *, void *, void *))(overlay, caps_wanted, &surface);
                if (result == 0)
                    goto made;
            }
            if (OVERLAY != 0) {
                void *overlay = OVERLAY;
                COM(overlay, 8, uint32_t, (void *))(overlay);
                G_PTR(0x41a4fc) = 0;
            }
            desc[0x1a] = (desc[0x1a] & 0xffffff67u) | 0x40;
            desc[1] &= 0xffffefdfu;
        }
    }
    if (result != 0) {
        void *directdraw = DIRECTDRAW;
        COM(directdraw, 0x18, int32_t, (void *, void *, void *, void *))(directdraw, desc, &surface, 0);
    }
made:
    if (surface == 0)
        return 0;
    fs98_host_rt_fill_clipped(surface, 0, 0, width, height, 0);
    return surface;
}
