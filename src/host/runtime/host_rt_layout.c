/* A game window's place and picture: its docked place from the main window's scale and back,
 * its alignment, its picture (a DIB section, shared under a key or its own, with a DirectDraw
 * surface beside it), its window region, and the pieces drawn by GDI. The data's fields are
 * host_rt_child.c's; +0x20..+0x2c is the docked place in the main window's units. */
#include "host_rt.h"

#define WIN(ident, result, parameters) CALL_STD(ident, result, parameters)
typedef int32_t (SC *window_handler)(uint32_t kind, uint8_t *data, void *picture, void *result);
#define HANDLER(data) ((window_handler)PTR(data, 0x18))

/* a 16.16 multiply as retail's: the low 32 bits of the product, shifted arithmetically */
static int32_t scaled(uint32_t value, uint32_t scale)
{
    return (int32_t)(value * scale) >> 16;
}
static int32_t magnitude(int32_t value)
{
    return value < 0 ? (int32_t)-(uint32_t)value : value;
}

/* 0x405d20: the docked place from its units at the main window's scale (0x41bda8, 0x41c350
 * from 0x41c340); unless sizing, a fixed size kept where it is near (16 across, 32 down) for
 * a window that snaps (+8 bit 19), and a fixed height snapped to the bottom (bit 26, or within
 * 16 of it); a fixed size otherwise kept and its place put back on the screen. */
void SC fs98_host_rt_dock_place(uint8_t *data)
{
    I32(data, 0x30) = (int32_t)((uint32_t)scaled(D32(data, 0x20), G32(0x41bda8)) + G32(0x41c340));
    I32(data, 0x34) = (int32_t)((uint32_t)scaled(D32(data, 0x24), G32(0x41c350)) + G32(0x41c344));
    I32(data, 0x38) = scaled(D32(data, 0x28), G32(0x41bda8));
    I32(data, 0x3c) = scaled(D32(data, 0x2c), G32(0x41c350));
    if (G32(0x418860) == 0) {
        uint32_t height = D32(data, 0x6c), width = D32(data, 0x68);
        int32_t across = (int32_t)(D32(data, 0x38) - width), down = (int32_t)(D32(data, 0x3c) - height);
        if (width == D32(data, 0x70) && width && (D32(data, 8) & 0x80000) && magnitude(across) <= 0x10 &&
            magnitude(down) <= 0x20)
            D32(data, 0x38) = width;
        if (height == D32(data, 0x74) && height && (D32(data, 8) & 0x80000) && magnitude(across) <= 0x10 &&
            magnitude(down) <= 0x20) {
            uint32_t top = D32(data, 0x34), bottom = top + D32(data, 0x3c), area = G32(0x41c34c);
            D32(data, 0x3c) = height;
            if ((D32(data, 8) & 0x4000000) || magnitude((int32_t)(area - bottom)) <= 0x10)
                D32(data, 0x34) = area - height;
            else if ((int32_t)bottom > (int32_t)(area + 0x10))
                D32(data, 0x34) = (uint32_t)down + top;
        }
    }
    if (D32(data, 0x68) == D32(data, 0x70) && D32(data, 0x68) && !(D32(data, 8) & 0x80000)) {
        D32(data, 0x38) = D32(data, 0x70);
        if (I32(data, 0x30) < 0 || I32(data, 0x30) > GI32(0x41c348))
            D32(data, 0x30) = 0;
    }
    if (D32(data, 0x6c) == D32(data, 0x74) && D32(data, 0x6c) && !(D32(data, 8) & 0x80000)) {
        D32(data, 0x3c) = D32(data, 0x74);
        if (I32(data, 0x34) < 0 || I32(data, 0x34) > GI32(0x41c34c))
            D32(data, 0x34) = 0;
    }
}

/* 0x405ea0: and its units from the docked place, rounded, if it is bigger than a point */
void SC fs98_host_rt_dock_units(uint8_t *data)
{
    int32_t width = I32(data, 0x38), height = I32(data, 0x3c), scale;
    if (width <= 1 || height <= 1)
        return;
    scale = GI32(0x41bda8);
    I32(data, 0x20) = (int32_t)(((D32(data, 0x30) - G32(0x41c340)) << 16) + (uint32_t)(scale / 2)) / scale;
    scale = GI32(0x41c350);
    I32(data, 0x24) = (int32_t)(((D32(data, 0x34) - G32(0x41c344)) << 16) + (uint32_t)(scale / 2)) / scale;
    scale = GI32(0x41bda8);
    I32(data, 0x28) = (int32_t)(((uint32_t)width << 16) + (uint32_t)(scale / 2)) / scale;
    scale = GI32(0x41c350);
    I32(data, 0x2c) = (int32_t)(((uint32_t)height << 16) + (uint32_t)(scale / 2)) / scale;
}

/* 0x406890: the docked place aligned as its flags say: top (bit 25), bottom (26) or centred
 * (21) down, left (23), right (24) or centred (22) across; not above 20, nor left of 0, unless
 * aligned there. */
void SC fs98_host_rt_align(uint8_t *data)
{
    uint32_t flags = D32(data, 8), top = flags & 0x2000000, left = flags & 0x800000;
    if (top)
        D32(data, 0x34) = 0;
    else if (flags & 0x4000000)
        D32(data, 0x34) = G32(0x41c34c) - D32(data, 0x3c);
    else if (flags & 0x200000)
        I32(data, 0x34) = (int32_t)(G32(0x41c34c) - D32(data, 0x3c)) / 2;
    if (left)
        D32(data, 0x30) = 0;
    else if (flags & 0x1000000)
        D32(data, 0x30) = G32(0x41c348) - D32(data, 0x38);
    else if (flags & 0x400000)
        I32(data, 0x30) = (int32_t)(G32(0x41c348) - D32(data, 0x38)) / 2;
    if (!top && I32(data, 0x34) < 0x14)
        D32(data, 0x34) = 0x14;
    if (!left && I32(data, 0x30) < 0)
        D32(data, 0x30) = left;
}

/* 0x405f40: +0x308's callback over a window's pieces (its bitmap's +0x54): one piece (a
 * rectangle, grown by a point for a scaled window) drawn from the memory DC to the window,
 * through 0x40ea20 on the 16-bit GDI path. 0, to go on. */
int32_t SC fs98_host_rt_draw_piece_gdi(int32_t *piece, uint8_t *data)
{
    uint8_t *bitmap = (uint8_t *)PTR(data, 0xa0);
    int32_t to_x, to_y, to_width, to_height, from_x, from_y, from_width, from_height;
    if ((D32(data, 8) & 0x80400) && !(D32(bitmap, 0x44) == 0x10000 && D32(bitmap, 0x48) == 0x10000)) {
        uint32_t across, down;
        if (piece[0])
            --piece[0];
        if (piece[1])
            --piece[1];
        if (piece[2] < (int32_t)(D32(data, 0x84) - 1))
            ++piece[2];
        if (piece[3] < (int32_t)(D32(data, 0x88) - 1))
            ++piece[3];
        across = D32(bitmap, 0x44);
        down = D32(bitmap, 0x48);
        from_x = piece[0];
        from_y = piece[1];
        to_x = scaled((uint32_t)from_x, across);
        to_y = scaled((uint32_t)from_y, down);
        from_width = (int32_t)((uint32_t)piece[2] - (uint32_t)from_x + 1);
        to_width = (int32_t)((uint32_t)from_width * across + 0x8000) >> 16;
        from_height = (int32_t)((uint32_t)piece[3] - (uint32_t)from_y + 1);
        to_height = (int32_t)((uint32_t)from_height * D32(bitmap, 0x48) + 0x8000) >> 16;
    } else {
        to_x = from_x = piece[0];
        to_y = from_y = piece[1];
        to_width = from_width = (int32_t)((uint32_t)piece[2] - (uint32_t)piece[0] + 1);
        to_height = from_height = (int32_t)((uint32_t)piece[3] - (uint32_t)piece[1] + 1);
    }
    if (G32(0x41a250)) {
        fs98_host_rt_gdi_blit(D32(bitmap, 0x34), to_x, to_y, to_width, to_height, D32(bitmap, 0x38), from_x, from_y,
                            from_width, from_height, D32(data, 8));
        return 0;
    }
    WIN(StretchBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t,
                              uint32_t))(D32(bitmap, 0x34), to_x, to_y, to_width, to_height, D32(bitmap, 0x38), from_x,
                                         from_y, from_width, from_height, 0xcc0020);
    return 0;
}

/* 0x406950: +0x308's callback over a shared picture's windows: its size (+8, +0xc) grown to
 * hold the window's picture, but no bigger than the display under DirectDraw. 0. */
int32_t SC fs98_host_rt_grow_shared(uint8_t **entry, uint8_t *shared)
{
    int32_t width, height, depth;
    if (I32(shared, 8) <= I32(*entry, 0x84))
        D32(shared, 8) = D32(*entry, 0x84);
    if (I32(shared, 0xc) <= I32(*entry, 0x88))
        D32(shared, 0xc) = D32(*entry, 0x88);
    if (G32(0x41a4e4)) {
        fs98_host_rt_display_size(&width, &height, &depth);
        if (I32(shared, 8) >= width)
            I32(shared, 8) = width;
        if (I32(shared, 0xc) >= height)
            I32(shared, 0xc) = height;
    }
    return 0;
}

/* 0x4069d0: and each given the shared picture: its bitmap flag, pitch, bits and rows, the
 * bitmap and what GetObject says of its bits and pitch; to be drawn again (bitmap +0 -1). 0. */
int32_t SC fs98_host_rt_take_shared(uint8_t **entry, uint8_t *shared)
{
    uint8_t *bitmap = (uint8_t *)PTR(*entry, 0xa0);
    uint32_t object[6];
    D32(*entry, 0x98) = D32(shared, 0x1c);
    D32(*entry, 0x8c) = D32(shared, 0x10);
    D32(*entry, 0x90) = D32(shared, 0x14);
    D32(*entry, 0x94) = D32(shared, 0x18);
    D32(bitmap, 0x30) = D32(shared, 0x24);
    D32(bitmap, 0) = 0xffffffffu;
    WIN(GetObjectA, int32_t, (uint32_t, int32_t, void *))(D32(shared, 0x24), 0x18, object);
    D32(bitmap, 0x1c) = object[5];
    D32(bitmap, 0x20) = object[3];
    return 0;
}

/* 0x406a50: +0x308's callback over a bitmap's +0x70 list: +0x280 told of the window. 0. */
int32_t SC fs98_host_rt_tell_owner(uint8_t *entry, uint8_t *data)
{
    SERVICE(0x280, int32_t, (void *, uint32_t))(data, D32(entry, 0x10));
    return 0;
}

/* a region added to the window's (its bitmap's +0x40) */
static void add_region(uint8_t *bitmap, uint32_t region)
{
    uint32_t have = D32(bitmap, 0x40);
    if (have) {
        WIN(CombineRgn, int32_t, (uint32_t, uint32_t, uint32_t, int32_t))(have, have, region, 2);
        WIN(DeleteObject, int32_t, (uint32_t))(region);
    } else if (region)
        D32(bitmap, 0x40) = region;
}

/* 0x406590: a fixed-size window's region from its shape (+0x10), scaled as the picture is: an
 * "FS6BOUND" list of rectangles (1) and polygons (2), else a span (two words) per row; none
 * when it is the whole picture. Moved on from anything else (a floating window, +8 bit 11). */
void SC fs98_host_rt_window_region(uint8_t *data)
{
    uint8_t *bitmap, *shape;
    int32_t rect[4];
    int32_t (SC *set_region)(uint32_t, uint32_t, int32_t) =
        (int32_t (SC *)(uint32_t, uint32_t, int32_t))IMPORT(SetWindowRgn);
    uint32_t (SC *rect_region)(const void *) = (uint32_t (SC *)(const void *))IMPORT(CreateRectRgnIndirect);
    ++G32(0x41bb2c);
    bitmap = (uint8_t *)PTR(data, 0xa0);
    set_region(D32(bitmap, 0x24), 0, 1);
    D32(bitmap, 0x40) = 0;
    shape = (uint8_t *)PTR(data, 0x10);
    if (shape == 0 || (D32(data, 8) & 0x40000800) || D32(data, 0x68) != D32(data, 0x70) || D32(data, 0x70) == 0 ||
        D32(data, 0x6c) != D32(data, 0x74) || D32(data, 0x74) == 0)
        return;
    if (SERVICE(0x228, int32_t, (const void *, const char *, uint32_t))(shape, STR(0x417b14), 8) == 0) {
        int32_t at = 0;
        uint8_t *record = shape + 0x10;
        if (I32(shape, 0xc) > 0)
            do {
                uint32_t region = 0, kind = D32(record, 0);
                if (kind == 1) {
                    SERVICE(0x21c, int32_t, (void *, const void *, uint32_t))(rect, record + 4, 0x10);
                    rect[0] = scaled((uint32_t)rect[0], D32(bitmap, 0x44));
                    rect[2] = scaled((uint32_t)rect[2], D32(bitmap, 0x44));
                    rect[1] = scaled((uint32_t)rect[1], D32(bitmap, 0x48));
                    rect[3] = scaled((uint32_t)rect[3], D32(bitmap, 0x48));
                    region = rect_region(rect);
                    record += 0x10;
                } else if (kind == 2) {
                    uint32_t *points, count, i;
                    SERVICE(0x20c, int32_t, (uint32_t, uint32_t, void *, uint32_t))(1, D32(record, 4) << 3, &points, 0);
                    SERVICE(0x21c, int32_t, (void *, const void *, uint32_t))(points, record + 8, D32(record, 4) << 3);
                    for (i = 0; (int32_t)i < (int32_t)(D32(record, 4) << 1); i += 2) {
                        points[i] = (D32(bitmap, 0x44) * points[i]) >> 16;
                        points[i + 1] = (points[i + 1] * D32(bitmap, 0x48)) >> 16;
                    }
                    count = D32(record, 4);
                    region = WIN(CreatePolygonRgn, uint32_t, (const void *, int32_t, int32_t))(points, (int32_t)count, 2);
                    SERVICE(0x214, int32_t, (void *))(&points);
                    record += D32(record, 4) * 8 + 4;
                }
                add_region(bitmap, region);
                record += 4;
            } while (++at < I32(shape, 0xc));
    } else if (I32(data, 0x74) > 0) {
        /* a span per row, until a row whose end is past the width */
        const uint16_t *span = (const uint16_t *)shape;
        int32_t row = 0;
        do {
            uint32_t region;
            if ((int32_t)span[1] > I32(data, 0x70))
                break;
            rect[0] = scaled(span[0], D32(bitmap, 0x44));
            rect[2] = scaled((uint32_t)span[1] + 1, D32(bitmap, 0x44));
            rect[1] = scaled((uint32_t)row, D32(bitmap, 0x48));
            rect[3] = scaled((uint32_t)row + 1, D32(bitmap, 0x48));
            region = rect_region(rect);
            add_region(bitmap, region);
            span += 2;
        } while (++row < I32(data, 0x74));
    }
    if (D32(bitmap, 0x40)) {
        if (WIN(GetRgnBox, int32_t, (uint32_t, int32_t *))(D32(bitmap, 0x40), rect) == 2 && rect[0] == 0 && rect[1] == 0 &&
            rect[2] == I32(data, 0x60) && rect[3] == I32(data, 0x64)) {
            WIN(DeleteObject, int32_t, (uint32_t))(D32(bitmap, 0x40));
            D32(bitmap, 0x40) = 0;
        }
    }
    if (D32(bitmap, 0x40))
        set_region(D32(bitmap, 0x24), D32(bitmap, 0x40), 1);
}

/* 0x4060b0: a window's picture made for its client area (half of it for +8 bit 16) within its
 * tracking sizes (odd for a view, and no bigger than the display under DirectDraw): a DIB
 * section in the picture's format (+0x80: 1 8-bit, 2 16-bit, 3 16-bit 565, 4 24-bit, 5 32-bit;
 * any other made 1) and its row table, made again only when its pitch or height changed; for a
 * shared picture (+0x7c) the size of its largest window, a DirectDraw surface beside it
 * (0x41a4e0) the module is given (5) and kept if it says so, and every window given it; the
 * scales between picture and window (bitmap +0x44..+0x50); the region. The second argument is
 * not used. */
void SC fs98_host_rt_make_picture(uint8_t *data, uint32_t unused)
{
    uint8_t *bitmap = (uint8_t *)PTR(data, 0xa0), *picture, *shared = 0;
    int32_t client[4], width, height, display[3];
    uint32_t old_width, old_height, old_pitch, *bitmap_slot, bytes;
    struct { uint32_t size; int32_t width, height; uint16_t planes, bits; uint32_t compression, image, x, y, used, important;
             uint32_t colours[0x100]; } info;
    (void)unused;
    WIN(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(bitmap, 0x24), client);
    if (client[2] < 0 || client[3] < 0)
        return;
    old_width = D32(data, 0x84);
    old_height = D32(data, 0x88);
    old_pitch = D32(data, 0x8c);
    width = (int32_t)((uint32_t)client[2] - (uint32_t)client[0]);
    height = (int32_t)((uint32_t)client[3] - (uint32_t)client[1]);
    if (D32(data, 8) & 0x10000) {
        width /= 2;
        height /= 2;
    }
    I32(data, 0x84) = width;
    I32(data, 0x88) = height;
    I32(data, 0x60) = width;
    I32(data, 0x64) = height;
    if (D32(data, 0x68) && width < I32(data, 0x68)) {
        D32(data, 0x84) = D32(data, 0x68);
        D32(data, 0x60) = D32(data, 0x68);
    }
    if (D32(data, 0x70) && I32(data, 0x84) > I32(data, 0x70)) {
        D32(data, 0x84) = D32(data, 0x70);
        D32(data, 0x60) = D32(data, 0x70);
    }
    if (D32(data, 0x6c) && height < I32(data, 0x6c)) {
        D32(data, 0x88) = D32(data, 0x6c);
        D32(data, 0x64) = D32(data, 0x6c);
    }
    if (D32(data, 0x74) && I32(data, 0x88) > I32(data, 0x74)) {
        D32(data, 0x88) = D32(data, 0x74);
        D32(data, 0x64) = D32(data, 0x74);
    }
    if (D32(data, 0x1c) && D32(data, 4) != 2) {
        if (!(D32(data, 0x84) & 1))
            ++D32(data, 0x84);
        if (!(D32(data, 0x88) & 1))
            ++D32(data, 0x88);
        if (G32(0x41a4e4)) {
            fs98_host_rt_display_size(&display[0], &display[1], &display[2]);
            if (I32(data, 0x84) > display[0]) {
                I32(data, 0x84) = display[0];
                I32(data, 0x60) = display[0];
            }
            if (I32(data, 0x88) > display[1]) {
                I32(data, 0x88) = display[1];
                I32(data, 0x64) = display[1];
            }
        }
    }
    if (D32(data, 0x7c)) {
        shared = SERVICE(0x310, uint8_t *, (void *, uint32_t, void *, uint32_t, uint32_t))(VA(0x417ae8), 0, AT(data, 0x7c), 4, 0);
        if (shared == 0)
            return;
        old_width = D32(shared, 8);
        old_height = D32(shared, 0xc);
        old_pitch = D32(shared, 0x10);
        D32(shared, 8) = 0;
        D32(shared, 0xc) = 0;
        SERVICE(0x308, int32_t, (void *, void *, void *))(shared + 0x28, shared, (void *)fs98_host_rt_grow_shared);
        bitmap_slot = (uint32_t *)(shared + 0x24);
        picture = shared + 4;
    } else {
        bitmap_slot = (uint32_t *)(bitmap + 0x30);
        picture = data + 0x80;
    }
    (void)old_width;
    info.compression = 0;
    info.image = 0;
    info.used = 0;
    info.important = 0;
    info.size = 0x28;
    info.planes = 1;
    {
        uint32_t at;
        for (at = 0; at < 0x100; ++at)
            info.colours[at] = 0;
    }
    switch (D32(picture, 0)) {
    case 5: info.bits = 0x20; bytes = 4; break;
    case 4: info.bits = 0x18; bytes = 3; break;
    case 3:
        info.bits = 0x10;
        info.compression = 3;
        info.colours[0] = 0xf800;
        info.colours[1] = 0x7e0;
        info.colours[2] = 0x1f;
        bytes = 2;
        break;
    case 2: info.bits = 0x10; bytes = 2; break;
    default:
        D32(picture, 0) = 1;
        info.bits = 8;
        bytes = 1;
        break;
    }
    info.width = (int32_t)((D32(picture, 4) + 3) & ~3u);
    info.height = (int32_t)-D32(picture, 8);
    D32(picture, 0xc) = ((D32(picture, 4) + 3) & ~3u) * bytes;
    if (D32(picture, 0xc) != old_pitch || D32(picture, 8) != old_height) {
        uint32_t row, at;
        D32(picture, 0x14) = D32(picture, 0xc) * D32(picture, 8);
        if (D32(picture, 0x18)) {
            WIN(DeleteObject, int32_t, (uint32_t))(*bitmap_slot);
            D32(picture, 0x18) = 0;
        }
        *bitmap_slot = WIN(CreateDIBSection, uint32_t, (uint32_t, const void *, uint32_t, void *, uint32_t, uint32_t))(
            D32(bitmap, 0x34), &info, 0, AT(picture, 0x18), 0, 0);
        D32(bitmap, 0x1c) = D32(picture, 0x18);
        D32(bitmap, 0x20) = D32(picture, 0xc);
        fs98_host_rt_fill_bytes(PTR(picture, 0x18), 0, D32(picture, 0x14));
        if (D32(picture, 0x10))
            SERVICE(0x214, int32_t, (void *))(AT(picture, 0x10));
        SERVICE(0x20c, int32_t, (uint32_t, uint32_t, void *, uint32_t))(1, D32(picture, 8) << 2, AT(picture, 0x10), 0);
        row = D32(picture, 0x18);
        for (at = 0; (int32_t)at < I32(picture, 8); ++at) {
            ((uint32_t *)PTR(picture, 0x10))[at] = row;
            row += D32(picture, 0xc);
        }
        D32(bitmap, 0) = 0xffffffffu;
    }
    {
        void *surface;
        if (G32(0x41a4e4) && D32(data, 0x18) && D32(data, 0x7c)) {
            if (G32(0x41a4e0) == 0 || D32(picture, 0xc) != old_pitch || D32(picture, 8) != old_height) {
                HANDLER(data)(5, data, 0, 0);
                if (G_PTR(0x41a4e0))
                    COM_METHOD(G_PTR(0x41a4e0), 8, int32_t, (void *))(G_PTR(0x41a4e0));
                G_PTR(0x41a4e0) = fs98_host_rt_render_surface(info.width, (int32_t)-(uint32_t)info.height);
            }
        }
        surface = G_PTR(0x41a4e0);
        if (D32(data, 0x18) && D32(data, 0x7c)) {
            uint32_t keep = 0;
            PTR(picture, 0x1c) = G32(0x41a4e4) && G32(0x41a4dc) && surface ? surface : 0;
            HANDLER(data)(5, data, picture, &keep);
            D32(picture, 0x1c) = 0;
            if (keep == 0 && G_PTR(0x41a4e0)) {
                COM_METHOD(G_PTR(0x41a4e0), 8, int32_t, (void *))(G_PTR(0x41a4e0));
                G32(0x41a4e0) = 0;
            }
        }
    }
    if (D32(data, 0x7c))
        SERVICE(0x308, int32_t, (void *, void *, void *))(shared + 0x28, shared, (void *)fs98_host_rt_take_shared);
    if (!(D32(data, 8) & 0x90400)) {
        D32(bitmap, 0x50) = 0x10000;
        D32(bitmap, 0x4c) = 0x10000;
        D32(bitmap, 0x48) = 0x10000;
        D32(bitmap, 0x44) = 0x10000;
    } else {
        I32(bitmap, 0x44) = (int32_t)(((uint32_t)client[2] << 16) + 0x8000) / I32(data, 0x60);
        I32(bitmap, 0x48) = (int32_t)(((uint32_t)client[3] << 16) + 0x8000) / I32(data, 0x64);
        I32(bitmap, 0x4c) = (int32_t)((D32(data, 0x60) << 16) + 0x8000) / client[2];
        I32(bitmap, 0x50) = (int32_t)((D32(data, 0x64) << 16) + 0x8000) / client[3];
    }
    fs98_host_rt_window_region(data);
    if (D32(bitmap, 0x70))
        SERVICE(0x308, int32_t, (void *, void *, void *))(bitmap + 0x70, data, (void *)fs98_host_rt_tell_owner);
}
