/* Surfaces drawn on: whether a 3D device is in use, unlocking, clipped fills and blits, and
 * the surface the game renders into. */
#include "host_rt.h"
#define DIRECTDRAW G_PTR(0x41a4e4)     /* the IDirectDraw */
#define DIRECT3D G_PTR(0x41a4dc)       /* the IDirect3D2 */
#define BACK_BUFFER G_PTR(0x41a4e0)
#define PRIMARY G_PTR(0x41a4f8)
#define OVERLAY G_PTR(0x41a4fc)
#define COM(object, slot, result, parameters) ((result (SC *) parameters)PTR(PTR(object, 0), slot))
#define FULL_SCREEN G32(0x418854)       /* the game wants full screen */

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

/* 0x413030: a surface unlocked. Returns whether it was. */
int32_t SC fs98_host_rt_unlock(void *surface)
{
    return COM(surface, 0x80, int32_t, (void *, void *))(surface, 0) == 0;
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
