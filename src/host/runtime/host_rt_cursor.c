/* The cursor in full screen, drawn by the game on the back buffer (0x41a53c, the region to
 * redraw, is made on entering full screen), and the 8-bit-to-16-bit copies through a palette. */
#include "host_rt.h"

#define BACK_BUFFER G_PTR(0x41a4e0)
#define COM(object, slot, result, parameters) ((result (SC *) parameters)PTR(PTR(object, 0), slot))
#define CURSOR G32(0x41a540)
#define ICON_INFO 0x41a528          /* ICONINFO: fIcon, xHotspot, yHotspot, hbmMask, hbmColor */
#define CURSOR_RECT 0x41a548        /* where the cursor is drawn */
#define LAST_RECT 0x41a970          /* where it was drawn before */
#define REDRAW G32(0x41a53c)        /* the region to redraw */
#define SCRATCH_REGION G32(0x41a968)

/* 0x413700: the cursor moved: its shape (and hotspot) taken again when it changed, the
 * rectangle it covered and the one it will cover added to the region to redraw (cut to the
 * mode's size) and cleared on the back buffer; then the rectangles move on (this one is
 * the next one's last). */
void fs98_host_rt_move_cursor(void)
{
    int32_t point[2], joined[4], now[4];
    int32_t changed = 0, x, y;
    uint32_t cursor = CALL_STD(GetCursor, uint32_t, (void))();
    if (CURSOR != cursor) {
        CURSOR = cursor;
        CALL_STD(GetIconInfo, int32_t, (uint32_t, void *))(cursor, VA(ICON_INFO));
        if (G32(ICON_INFO + 0xc) != 0)
            CALL_STD(DeleteObject, int32_t, (uint32_t))(G32(ICON_INFO + 0xc));
        if (G32(ICON_INFO + 0x10) != 0)
            CALL_STD(DeleteObject, int32_t, (uint32_t))(G32(ICON_INFO + 0x10));
        changed = 1;
    }
    CALL_STD(GetCursorPos, int32_t, (void *))(point);
    x = point[0] - GI32(ICON_INFO + 4);
    y = point[1] - GI32(ICON_INFO + 8);
    now[0] = x;
    now[1] = y;
    now[2] = x + 0x20;
    now[3] = y + 0x20;
    if (x != GI32(LAST_RECT) || y != GI32(LAST_RECT + 4) || changed) {
        uint32_t region;
        CALL_STD(UnionRect, int32_t, (void *, const void *, const void *))(joined, now, VA(LAST_RECT));
        region = REDRAW;
        if (region != 0) {
            int32_t bottom = GI32(0x4183ac), right = GI32(0x4183a8);
            if (joined[3] < bottom)
                bottom = joined[3];
            if (joined[2] < right)
                right = joined[2];
            CALL_STD(SetRectRgn, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(
                SCRATCH_REGION, joined[0] <= 0 ? 0 : joined[0], joined[1] <= 0 ? 0 : joined[1], right, bottom);
            CALL_STD(CombineRgn, int32_t, (uint32_t, uint32_t, uint32_t, int32_t))(region, REDRAW, SCRATCH_REGION, 2);
        }
        fs98_host_rt_fill_clipped(BACK_BUFFER, joined[0], joined[1], joined[2] - joined[0], joined[3] - joined[1], 0);
    }
    G32(LAST_RECT) = G32(CURSOR_RECT);
    G32(LAST_RECT + 4) = G32(CURSOR_RECT + 4);
    G32(LAST_RECT + 8) = G32(CURSOR_RECT + 8);
    G32(LAST_RECT + 0xc) = G32(CURSOR_RECT + 0xc);
    GI32(CURSOR_RECT) = now[0];
    GI32(CURSOR_RECT + 4) = now[1];
    GI32(CURSOR_RECT + 8) = now[2];
    GI32(CURSOR_RECT + 0xc) = now[3];
}

/* 0x4138b0: the cursor drawn on the back buffer, if it is in the region being drawn.
 * Retail's GetDC writes the DC over the region argument's slot: a failed GetDC leaves the
 * region there, and that is what DrawIcon and ReleaseDC are given. */
void SC fs98_host_rt_draw_cursor(uint32_t region)
{
    uint32_t dc = region;
    void *back;
    if (!CALL_STD(RectInRegion, int32_t, (uint32_t, const void *))(region, VA(CURSOR_RECT)))
        return;
    back = BACK_BUFFER;
    COM(back, 0x44, int32_t, (void *, uint32_t *))(back, &dc);
    CALL_STD(DrawIcon, int32_t, (uint32_t, int32_t, int32_t, uint32_t))(dc, GI32(CURSOR_RECT), GI32(CURSOR_RECT + 4), CURSOR);
    back = BACK_BUFFER;
    COM(back, 0x68, int32_t, (void *, uint32_t))(back, dc);
}

/* 0x413910: the rectangle at 0x41a558 framed in the stock white brush on the back buffer, if
 * it is not empty across. The argument is not read. */
void SC fs98_host_rt_frame_rect(uint32_t unused)
{
    uint32_t dc;
    void *back;
    (void)unused;
    if (GI32(0x41a560) <= GI32(0x41a558))
        return;
    back = BACK_BUFFER;
    COM(back, 0x44, int32_t, (void *, uint32_t *))(back, &dc);
    CALL_STD(FrameRect, int32_t, (uint32_t, const void *, uint32_t))(
        dc, VA(0x41a558), CALL_STD(GetStockObject, uint32_t, (int32_t))(0));
    back = BACK_BUFFER;
    COM(back, 0x68, int32_t, (void *, uint32_t))(back, dc);
}

/* 0x413960: rows of 8-bit pixels copied into 16-bit ones through a palette of 256 words.
 * The pitches are in bytes; the destination's is halved (rounding towards 0). */
void SC fs98_host_rt_copy_8_to_16(uint16_t *to, int32_t to_pitch, const uint8_t *from, int32_t from_pitch,
                                  int32_t width, int32_t rows, const uint16_t *palette)
{
    int32_t words = to_pitch / 2, at;
    if (rows <= 0)
        return;
    do {
        for (at = 0; at < width; ++at)
            *to++ = palette[*from++];
        from += from_pitch - width;
        to += words - width;
    } while (--rows != 0);
}

/* 0x4139d0: the same, the pixels of one colour (key) left as they are. */
void SC fs98_host_rt_copy_8_to_16_keyed(uint16_t *to, int32_t to_pitch, const uint8_t *from, int32_t from_pitch,
                                        int32_t width, int32_t rows, uint8_t key, const uint16_t *palette)
{
    int32_t words = to_pitch / 2, at;
    if (rows <= 0)
        return;
    do {
        for (at = 0; at < width; ++at) {
            if (*from != key)
                *to = palette[*from];
            ++to;
            ++from;
        }
        from += from_pitch - width;
        to += words - width;
    } while (--rows != 0);
}

/* A picture as the floating windows keep theirs and as the back buffer is seen when locked:
 * only these fields are read. */
struct picture {
    uint32_t unknown_0;
    int32_t width;          /* +4 */
    int32_t height;         /* +8 */
    int32_t pitch;          /* +0xc, in bytes */
    uint32_t unknown_10[2];
    uint8_t *bits;          /* +0x18 */
};

/* a byte of a picture, as retail works it out: in 32 bits, wrapping */
static uint8_t *picture_at(const struct picture *picture, int32_t byte, int32_t row)
{
    return (uint8_t *)(uintptr_t)((uint32_t)(uintptr_t)picture->bits + (uint32_t)row * (uint32_t)picture->pitch +
                                  (uint32_t)byte);
}

/* 0x413a50: a piece of a floating window's 8-bit picture (from, the rectangle part of it)
 * copied to a 16-bit one (to, at the point), clipped to both, through the palette at
 * 0x41a568 for a window with bit 0x200 at +8 or else 0x41a768; when its bitmap's +0x40 is set,
 * the colour of the picture's first pixel is left out. */
void SC fs98_host_rt_draw_piece(uint8_t *window, struct picture *to, const int32_t *point, struct picture *from,
                                const int32_t *part)
{
    int32_t from_x = part[0], from_y = part[1];
    int32_t width = part[2] - part[0], height = part[3] - part[1];
    int32_t to_x = point[0], to_y = point[1];
    const uint16_t *palette;
    if (from_x + width > from->width)
        width = from->width - from_x;
    if (from_y + height > from->height)
        height = from->height - from_y;
    if (to_x < 0) {
        width += to_x;
        from_x -= to_x;
        to_x = 0;
    }
    if (to_y < 0) {
        height += to_y;
        from_y -= to_y;
        to_y = 0;
    }
    if (to_x + width > to->width)
        width = to->width - to_x;
    if (to_y + height > to->height)
        height = to->height - to_y;
    if (width <= 0 || height <= 0)
        return;
    palette = (const uint16_t *)VA((D8(window, 9) & 2) ? 0x41a568 : 0x41a768);
    if (D32(PTR(window, 0xa0), 0x40) != 0)
        fs98_host_rt_copy_8_to_16_keyed((uint16_t *)picture_at(to, to_x * 2, to_y), to->pitch,
                                        picture_at(from, from_x, from_y), from->pitch, width, height,
                                        from->bits[0], palette);
    else
        fs98_host_rt_copy_8_to_16((uint16_t *)picture_at(to, to_x * 2, to_y), to->pitch,
                                  picture_at(from, from_x, from_y), from->pitch, width, height, palette);
}

/* 0x413bb0: the floating windows drawn on the back buffer inside a region, from the bottom of
 * the main window's children up: each whose data (GWL_USERDATA) is shown (+8 bit 31), not
 * hidden (+0x7c) and has a bitmap (+0xa0, its screen rectangle at +0xc), piece by piece where
 * it meets the region's rectangles. The children are first asked for once for nothing. */
void SC fs98_host_rt_draw_floating(uint32_t region)
{
    struct {
        int32_t meets[4];       /* where the window meets a rectangle; its top left is the point */
        int32_t part[4];        /* the same in the window's picture */
        struct picture back;
        uint32_t header[8];     /* RGNDATAHEADER */
        int32_t rects[(0xca0 - 0x20) / 4];
    } frame;
    uint32_t window;
    uint32_t (SC *get_window)(uint32_t, uint32_t) =
        (uint32_t (SC *)(uint32_t, uint32_t))fs98_host_rt_imports[HOST_RT_IMPORT_GetWindow];
    if (!fs98_host_rt_lock(BACK_BUFFER, (void **)&frame.back.bits, &frame.back.pitch))
        return;
    frame.back.width = GI32(0x4183a8);
    frame.back.height = GI32(0x4183ac);
    CALL_STD(GetRegionData, uint32_t, (uint32_t, uint32_t, void *))(region, 0xca0, frame.header);
    get_window(G32(0x41880c), 5);
    window = get_window(get_window(G32(0x41880c), 5), 1);
    while (window != 0) {
        uint8_t *data = (uint8_t *)CALL_STD(GetWindowLongA, uint32_t, (uint32_t, int32_t))(window, -21);
        uint8_t *bitmap;
        if (data != 0 && (D32(data, 8) & 0x80000000u) && D32(data, 0x7c) == 0 &&
            (bitmap = (uint8_t *)PTR(data, 0xa0)) != 0 && (int32_t)frame.header[2] > 0) {
            int32_t at;
            int32_t *rect = frame.rects;
            for (at = 0; at < (int32_t)frame.header[2]; ++at, rect += 4) {
                if (!CALL_STD(IntersectRect, int32_t, (void *, const void *, const void *))(frame.meets, AT(bitmap, 0xc), rect))
                    continue;
                frame.part[0] = frame.meets[0] - I32(bitmap, 0xc);
                frame.part[1] = frame.meets[1] - I32(bitmap, 0x10);
                frame.part[2] = frame.meets[2] - I32(bitmap, 0xc);
                frame.part[3] = frame.meets[3] - I32(bitmap, 0x10);
                fs98_host_rt_draw_piece(data, &frame.back, frame.meets, (struct picture *)AT(data, 0x80), frame.part);
            }
        }
        window = get_window(window, 3);
    }
    fs98_host_rt_unlock(BACK_BUFFER);
}
