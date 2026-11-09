/* Full screen drawn by the game: the 16-bit palettes, the region to redraw, a frame begun
 * and ended (the floating windows, a frame and the cursor drawn on the back buffer, then a
 * flip), and the floating windows moved or redrawn. */
#include "host_rt.h"

#define DIRECTDRAW G_PTR(0x41a4e4)
#define BACK_BUFFER G_PTR(0x41a4e0)
#define PRIMARY G_PTR(0x41a4f8)
#define COM(object, slot, result, parameters) ((result (SC *) parameters)PTR(PTR(object, 0), slot))
#define FULL_SCREEN_WANTED G32(0x418854)
#define FLIPPING G32(0x41a4f4)          /* a full screen with a back buffer */
#define SHOWN G32(0x418858)             /* 1 drawn by the game, 2 given back to GDI */
#define REDRAW G32(0x41a53c)            /* the region to redraw */
#define DRAWN G32(0x41a984)             /* the region drawn last frame */
#define SCRATCH_REGION G32(0x41a968)
#define CLEARS G32(0x41a520)            /* frames left to clear the whole back buffer */
#define PALETTE_CHANGES G32(0x41ba2c)   /* counts the palette's changes */
#define MODE_WIDTH GI32(0x4183a8)
#define MODE_HEIGHT GI32(0x4183ac)

/* one table of 256 colours (red at +2, green +1, blue +0, a word each) as 16-bit pixels */
static void make_palette(uint16_t *palette, const uint8_t *colours, int32_t depth)
{
    uint32_t at;
    for (at = 0; at < 0x100; ++at, colours += 4)
        if (depth == 15)
            palette[at] = (uint16_t)((((colours[2] & 0xf8u) << 5 | (colours[1] & 0xf8u)) << 2) | colours[0] >> 3);
        else
            palette[at] = (uint16_t)((((colours[2] & 0xf8u) << 5 | (colours[1] & 0xfcu)) << 3) | colours[0] >> 3);
}

/* 0x413d30: the floating windows' palettes for a 15- or 16-bit display: 0x41a768 from the
 * colours at 0x41adc0, 0x41a568 from those at 0x41b620. Other depths leave them. */
void fs98_host_rt_make_palettes(void)
{
    int32_t width, height, depth;
    fs98_host_rt_display_size(&width, &height, &depth);
    if (depth == 15) {
        make_palette((uint16_t *)VA(0x41a768), (const uint8_t *)VA(0x41adc0), 15);
        make_palette((uint16_t *)VA(0x41a568), (const uint8_t *)VA(0x41b620), 15);
    }
    if (depth == 16) {
        make_palette((uint16_t *)VA(0x41a768), (const uint8_t *)VA(0x41adc0), 16);
        make_palette((uint16_t *)VA(0x41a568), (const uint8_t *)VA(0x41b620), 16);
    }
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

/* 0x413f10: a full-screen frame begun: the regions made the first time, the whole back
 * buffer cleared (and redrawn) when the screen's generation (0x41bb2c) has changed and for
 * the frames after, the palettes made again after a change, and the cursor moved. */
void fs98_host_rt_begin_frame(void)
{
    if (FLIPPING == 0 || SHOWN != 1 || BACK_BUFFER == 0)
        return;
    if (REDRAW == 0) {
        uint32_t (SC *create)(int32_t, int32_t, int32_t, int32_t) =
            (uint32_t (SC *)(int32_t, int32_t, int32_t, int32_t))fs98_host_rt_imports[HOST_RT_IMPORT_CreateRectRgn];
        REDRAW = create(0, 0, 0, 0);
        DRAWN = create(0, 0, 0, 0);
        SCRATCH_REGION = create(0, 0, 0, 0);
    }
    if (G32(0x41a544) != G32(0x41bb2c)) {
        G32(0x41a544) = G32(0x41bb2c);
        CLEARS = 3;
    }
    if (G32(0x41a980) != PALETTE_CHANGES) {
        G32(0x41a980) = PALETTE_CHANGES;
        fs98_host_rt_make_palettes();
    }
    if (CLEARS != 0) {
        --CLEARS;
        CALL_STD(SetRectRgn, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(REDRAW, 0, 0, MODE_WIDTH, MODE_HEIGHT);
        fs98_host_rt_fill_clipped(BACK_BUFFER, 0, 0, MODE_WIDTH, MODE_HEIGHT, 0);
    }
    fs98_host_rt_move_cursor();
}

/* 0x414000: a full-screen frame ended: what is to be redrawn now and what was last frame
 * drawn over the back buffer (the floating windows, the frame, the cursor), the regions
 * swapped and the new one emptied, and flipped (the primary surface restored if lost). */
void fs98_host_rt_end_frame(void)
{
    uint32_t drawn;
    void *primary;
    if (FLIPPING == 0 || SHOWN != 1 || BACK_BUFFER == 0)
        return;
    CALL_STD(CombineRgn, int32_t, (uint32_t, uint32_t, uint32_t, int32_t))(SCRATCH_REGION, REDRAW, DRAWN, 2);
    fs98_host_rt_draw_floating(SCRATCH_REGION);
    fs98_host_rt_frame_rect(SCRATCH_REGION);
    fs98_host_rt_draw_cursor(SCRATCH_REGION);
    drawn = DRAWN;
    DRAWN = REDRAW;
    REDRAW = drawn;
    CALL_STD(SetRectRgn, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(drawn, 0, 0, 0, 0);
    primary = PRIMARY;
    if (COM(primary, 0x2c, int32_t, (void *, void *, uint32_t))(primary, 0, 1) == (int32_t)0x887601c2) {
        primary = PRIMARY;
        COM(primary, 0x6c, int32_t, (void *))(primary);
    }
}

/* a rectangle added to the region to redraw, cut to the mode */
static void redraw_rect(uint32_t region, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    if (bottom >= MODE_HEIGHT)
        bottom = MODE_HEIGHT;
    if (right >= MODE_WIDTH)
        right = MODE_WIDTH;
    CALL_STD(SetRectRgn, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(
        SCRATCH_REGION, left <= 0 ? 0 : left, top <= 0 ? 0 : top, right, bottom);
    CALL_STD(CombineRgn, int32_t, (uint32_t, uint32_t, uint32_t, int32_t))(region, REDRAW, SCRATCH_REGION, 2);
}

/* 0x4141e0: the callback the service +0x308 calls for a floating window's changed parts: a
 * rectangle (moved by the point, the window's place) added to the region to redraw. 0. */
int32_t SC fs98_host_rt_redraw_part(const int32_t *rect, const int32_t *point)
{
    uint32_t region = REDRAW;
    if (region != 0)
        redraw_rect(region, rect[0] + point[0], rect[1] + point[1], rect[2] + point[0], rect[3] + point[1]);
    return 0;
}

/* 0x414100: a floating window to be redrawn in full screen: its bitmap told the palette's
 * generation; unless it is hidden (+8 bit 30), its changed parts (with parts, +8 bit 28: the
 * service +0x308 walks them) or all of it added to the region to redraw; with parts, the
 * service +0x304 then clears them. */
void SC fs98_host_rt_redraw_window(uint32_t window, int32_t parts)
{
    uint8_t *data = (uint8_t *)CALL_STD(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -21);
    uint8_t *bitmap = (uint8_t *)PTR(data, 0xa0);
    uint32_t region;
    if (D32(bitmap, 0) != PALETTE_CHANGES)
        D32(bitmap, 0) = PALETTE_CHANGES;
    if (!(D32(data, 8) & 0x40000000u)) {
        if ((D32(data, 8) & 0x10000000u) && parts)
            SERVICE(0x308, int32_t, (void *, void *, void *))(AT(bitmap, 0x54), AT(bitmap, 0xc),
                                                             (void *)fs98_host_rt_redraw_part);
        else if ((region = REDRAW) != 0) {
            int32_t bottom = I32(bitmap, 0x18), right = I32(bitmap, 0x14);
            if (bottom >= MODE_HEIGHT)
                bottom = MODE_HEIGHT;
            if (right >= MODE_WIDTH)
                right = MODE_WIDTH;
            CALL_STD(SetRectRgn, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(SCRATCH_REGION,
                I32(bitmap, 0xc) <= 0 ? 0 : I32(bitmap, 0xc), I32(bitmap, 0x10) <= 0 ? 0 : I32(bitmap, 0x10), right, bottom);
            CALL_STD(CombineRgn, int32_t, (uint32_t, uint32_t, uint32_t, int32_t))(region, REDRAW, SCRATCH_REGION, 2);
        }
    }
    if (D32(data, 8) & 0x10000000u)
        SERVICE(0x304, int32_t, (void *))(AT(bitmap, 0x54));
}

/* 0x414270: a floating window moved or resized in full screen (rect, its new place): moved,
 * its bitmap's rectangle and the window's place (+0x30) follow; resized, it is hidden for a
 * frame and the new rectangle framed (0x41a558) instead. Then a frame drawn there and then:
 * begun, the game's frame callback (0x418830), ended. */
void SC fs98_host_rt_move_window(uint32_t window, const int32_t *rect)
{
    uint8_t *data = (uint8_t *)CALL_STD(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -21);
    /* read before anything is checked, as retail does: a window without data faults here */
    uint8_t *bitmap = *(uint8_t *volatile *)AT(data, 0xa0);
    int32_t resized = 1;
    if (FLIPPING == 0 || SHOWN != 1 || BACK_BUFFER == 0)
        return;
    if (I32(data, 0x60) == rect[2] - rect[0] && I32(data, 0x64) == rect[3] - rect[1])
        resized = 0;
    if (resized) {
        D32(data, 8) &= 0x7fffffffu;
        GI32(0x41a558) = rect[0];
        GI32(0x41a55c) = rect[1];
        GI32(0x41a560) = rect[2];
        GI32(0x41a564) = rect[3];
    } else {
        I32(bitmap, 0xc) = rect[0];
        I32(bitmap, 0x10) = rect[1];
        I32(bitmap, 0x14) = rect[2];
        I32(bitmap, 0x18) = rect[3];
        I32(data, 0x30) = rect[0];
        I32(data, 0x34) = rect[1];
    }
    CLEARS = 1;
    fs98_host_rt_begin_frame();
    ((void (*)(void))G_PTR(0x418830))();
    fs98_host_rt_end_frame();
    if (resized) {
        G32(0x41a560) = 0;
        G32(0x41a558) = 0;
        D32(data, 8) |= 0x80000000u;
    }
}
