/* CDIBSection (0x70 bytes, vtable 0x16078): a DIB section and the 8-bit blits SIMSUI draws
 * with. The palette +0x58, the DIBSECTION from +4 (its BITMAP's width +8, height +0xc and
 * bits; its BITMAPINFOHEADER +0x1c), the CBitmap +0x5c (handle +0x60), the bits +0x64.
 * Rows are the width rounded up to a dword, top-down (heights are made negative). */
#include "simsui_internal.h"

#define VTABLE_DIB 0x16078
#define VTABLE_BITMAP 0x16090
#define VTABLE_GDI_OBJECT 0x15030
#define VTABLE_OBJECT 0x15018
#define DIB_SIZE 0x70
#define DIB_LIVE G32(0x213dc)              /* set once any CDIBSection is made */
#define STRIDE(dib) ((D32(dib, 8) + 3) & 0xfffffffc)

/* 0x20477b30 */
void *TC fs98_simsui_dib_construct(void *dib)
{
    D32(dib, 0x60) = 0;
    PTR(dib, 0x5c) = IMAGE(VTABLE_BITMAP);
    PTR(dib, 0) = IMAGE(VTABLE_DIB);
    D32(dib, 0x58) = 0;
    if (!DIB_LIVE) DIB_LIVE = 1;
    return dib;
}
/* 0x20477bb0: the section's handle deleted, then ~CBitmap */
void TC fs98_simsui_dib_destroy(void *dib)
{
    void *bitmap = AT(dib, 0x5c);
    PTR(dib, 0) = IMAGE(VTABLE_DIB);
    if (bitmap ? D32(bitmap, 4) : 0)
        CALL_STD(DeleteObject, int32_t, (uint32_t))(CALL_THIS(MFC_2452, uint32_t, (void *))(bitmap));
    PTR(bitmap, 0) = IMAGE(VTABLE_GDI_OBJECT);
    mfc_gdi_delete_object(bitmap);
    PTR(bitmap, 0) = IMAGE(VTABLE_OBJECT);
    PTR(dib, 0) = IMAGE(VTABLE_OBJECT);
}
/* the vector deleting destructor every SIMSUI class has: an array goes back to MFC's
 * operator delete, a single object to the class's own (ui_delete, or MFC's) */
void *fs98_simsui_delete_object(void *object, uint32_t flags, uint32_t size, void (TC *destroy)(void *),
                                void (*free_one)(void *))
{
    if (flags & 2) {
        fs98_simsui_vector_destroy(object, size, I32(object, -4), destroy);
        mfc_operator_delete((uint8_t *)object - 4);
        return object;
    }
    destroy(object);
    if (flags & 1) free_one(object);
    return object;
}
void fs98_simsui_mfc_free(void *object)
{
    mfc_operator_delete(object);
}
/* 0x20477b60 */
void *TC fs98_simsui_dib_delete(void *dib, uint32_t flags)
{
    return fs98_simsui_delete_object(dib, flags, DIB_SIZE, fs98_simsui_dib_destroy, fs98_simsui_ui_delete);
}
/* 0x204780b0, CreateDIBSection(BITMAPINFO *, CDIBPal *, UINT usage): a new section for the
 * header (made top-down), any old one deleted */
int32_t TC fs98_simsui_dib_create(void *dib, uint8_t *info, void *pal, uint32_t usage)
{
    void *bitmap = AT(dib, 0x5c);
    uint8_t dc[0x10];
    uint32_t handle;
    D32(dib, 0x58) = (uint32_t)(uintptr_t)pal;
    if (bitmap ? D32(bitmap, 4) : 0)
        CALL_STD(DeleteObject, int32_t, (uint32_t))(CALL_THIS(MFC_2452, uint32_t, (void *))(bitmap));
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0));
    if (I32(info, 8) > 0) I32(info, 8) = -I32(info, 8);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    handle = CALL_STD(CreateDIBSection, uint32_t, (uint32_t, void *, uint32_t, void *, uint32_t, uint32_t))(
        D32(dc, 4), info, usage, AT(dib, 0x64), 0, 0);
    if (!handle) fs98_simsui_exit_cleanly();
    if (!handle || !CALL_THIS(MFC_1641, int32_t, (void *, uint32_t))(bitmap, handle)) {
        CALL_THIS(MFC_640, void, (void *))(dc);
        return 0;
    }
    CALL_STD(GetObjectA, int32_t, (uint32_t, int32_t, void *))(handle, 0x54, AT(dib, 4));
    CALL_THIS(MFC_640, void, (void *))(dc);
    return 1;
}
/* 0x20477c40, LoadDib(id, HINSTANCE, CDIBPal *, flags): a bitmap resource into a new
 * section, top-down; then, by flags, the palette made from it (2 or 8), given the static
 * colours (8), and the bits remapped to the palette (4). A resource that will not load
 * ends the program (SIMSUI_ExitCleanly). RETAIL BUG (kept): SUI-3 (docs/retail-bugs.md): a
 * 1-bit bitmap's row is taken as width / 128 bytes, not width / 8. */
int32_t TC fs98_simsui_dib_load(void *dib, int32_t id, uint32_t instance, void *pal, uint32_t flags)
{
    uint8_t header[0x28], map[0x100], quads[0x400], entries[0x400], old_quads[0x400], dc[0x10];
    uint8_t *data, *info = 0, *source;
    uint32_t resource, loaded, table, index;
    int32_t width, image, height, flipped = 0, row;
    D32(dib, 0x58) = (uint32_t)(uintptr_t)pal;
    if (!instance) instance = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    resource = CALL_STD(FindResourceA, uint32_t, (uint32_t, uint32_t, uint32_t))(instance, (uint32_t)id & 0xffff, 2);
    if (!resource) fs98_simsui_exit_cleanly();
    CALL_STD(SetLastError, void, (uint32_t))(0);
    loaded = CALL_STD(LoadResource, uint32_t, (uint32_t, uint32_t))(instance, resource);
    if (!loaded) fs98_simsui_exit_cleanly();
    data = CALL_STD(LockResource, uint8_t *, (uint32_t))(loaded);
    if (!data) goto failed;
    fs98_simsui_copy(header, data, 0x28);
    if (D32(header, 0) != 0x28) goto failed;
    table = (uint32_t)fs98_simsui_info_colour_count(header) * 4;
    width = I32(header, 4);
    switch (D16(header, 0xe)) {
    case 1: width = (width & 7) ? (width >> 7) + 1 : width >> 7; break;
    case 4: width = (width & 1) ? (width >> 1) + 1 : width >> 1; break;
    case 16: width += width; break;
    case 24: width *= 3; break;
    case 32: width <<= 2; break;
    default: break;
    }
    image = (int32_t)(((uint32_t)width + 3) & 0xfffffffc) * I32(header, 8);
    if (image < 0) image = -image;
    info = mfc_operator_new(0x428);
    if (!info) goto failed;
    fs98_simsui_copy(info, header, 0x28);
    height = I32(info, 8);
    if (height < 0) { height = -height; flipped = 1; }
    if (!flipped) I32(info, 8) = -I32(info, 8);
    fs98_simsui_copy(info + 0x28, data + 0x28, table);
    if (!fs98_simsui_dib_create(dib, info, pal, 0)) goto failed;
    row = *(volatile int32_t *)&(volatile int32_t){image / height};    /* divided even when unused: 0 faults */
    source = data + table + 0x28;
    if (!flipped) {
        if (height > 0) {
            int32_t from = row * height, to = 0, count = height;
            do {
                fs98_simsui_copy((uint8_t *)PTR(dib, 0x64) + to, source + from - row, (uint32_t)row);
                to += row;
                from -= row;
            } while (--count);
        }
    } else fs98_simsui_copy(PTR(dib, 0x64), source, (uint32_t)image);
    {
        void *bitmap = AT(dib, 0x5c);
        CALL_STD(GetObjectA, int32_t, (uint32_t, int32_t, void *))(bitmap ? D32(bitmap, 4) : 0, 0x54, AT(dib, 4));
    }
    if (pal && (flags & 0xa)) fs98_simsui_pal_create(pal, info);
    if (!pal) goto done;
    if (flags & 8) fs98_simsui_pal_set_static_colours(pal);
    if (!pal || !(flags & 4)) goto done;
    {
        void *bitmap = AT(dib, 0x5c), *old;
        uint8_t *bits, *end;
        CALL_THIS(MFC_323, void *, (void *))(dc);
        CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0));
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), bitmap ? D32(bitmap, 4) : 0);
        CALL_STD(GetPaletteEntries, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(pal, 4), 0, 0x100, entries);
        for (index = 0; index < 0x400; index += 4) {
            quads[index + 2] = entries[index];
            quads[index + 1] = entries[index + 1];
            quads[index] = entries[index + 2];
            quads[index + 3] = 0;
        }
        CALL_STD(GetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(dc, 4), 0, 0x100, old_quads);
        for (index = 0; index < 0x100; ++index) {
            uint32_t colour = ((uint32_t)old_quads[index * 4] << 16) | ((uint32_t)old_quads[index * 4 + 1] << 8) |
                              old_quads[index * 4 + 2];
            map[index] = (uint8_t)CALL_STD(GetNearestPaletteIndex, uint32_t, (uint32_t, uint32_t))(D32(pal, 4), colour);
        }
        bits = PTR(dib, 0x64);
        end = bits + image;
        for (; bits < end; ++bits) *bits = map[*bits];
        CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(dc, 4), 0, 0x100, quads);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), old ? D32(old, 4) : 0);
        CALL_THIS(MFC_640, void, (void *))(dc);
    }
done:
    mfc_operator_delete(info);
    return 1;
failed:
    if (info) mfc_operator_delete(info);
    return 0;
}
/* 0x20478200, FillColor(CRect, BYTE): the rectangle filled. RETAIL BUG (kept): SUI-4
 * (docs/retail-bugs.md): the rectangle is united with the section's, not intersected, so
 * the whole section is filled, and more when the rectangle reaches outside. */
void TC fs98_simsui_dib_fill_rect(void *dib, struct fs98_simsui_rect rect, uint32_t colour)
{
    struct fs98_simsui_rect whole;
    uint32_t stride, skip, width;
    int32_t rows;
    uint8_t *at;
    whole.left = 0; whole.top = 0;
    whole.right = I32(dib, 8); whole.bottom = I32(dib, 0xc);
    CALL_THIS(MFC_4317, void, (void *))(&rect);
    CALL_STD(UnionRect, int32_t, (void *, const void *, const void *))(&rect, &rect, &whole);
    stride = STRIDE(dib);
    rows = rect.bottom - rect.top;
    at = (uint8_t *)PTR(dib, 0x64) + (uint32_t)rect.top * stride + rect.left;
    width = (uint32_t)(rect.right - rect.left);
    skip = stride - width;
    if (rows) {
        do {
            if (width) fs98_simsui_fill(at, (uint8_t)colour, width);
            at += width + skip;
        } while (--rows);
    }
}
/* 0x204781d0, FillColor(BYTE): all of it */
void TC fs98_simsui_dib_fill(void *dib, uint32_t colour)
{
    struct fs98_simsui_rect rect;
    rect.left = 0; rect.top = 0;
    rect.right = I32(dib, 8); rect.bottom = I32(dib, 0xc);
    fs98_simsui_dib_fill_rect(dib, rect, colour);
}
/* 0x204782d0, Clip(dest, source, &dest x, &dest y, &source x, &source y, &width, &height):
 * a copy's rectangle clipped to both sections; whether anything is left. Values are read
 * and written in retail's order: the references may alias. */
int32_t TC fs98_simsui_dib_clip(void *self, void *dest, void *source, int32_t *dest_x, int32_t *dest_y,
                                int32_t *source_x, int32_t *source_y, int32_t *width, int32_t *height)
{
    int32_t value, size, sum;
    (void)self;
    value = *source_x;
    if (value >= I32(source, 8)) return 0;
    size = *width; sum = size + value;
    if (sum < 0) return 0;
    if (value < 0) { *dest_x = *dest_x + (size - sum); *source_x = 0; *width = sum; }
    value = *source_y;
    if (value >= I32(source, 0xc)) return 0;
    size = *height; sum = size + value;
    if (sum < 0) return 0;
    if (value < 0) { *dest_y += size - sum; *source_y = 0; *height = sum; }
    sum = *source_x; size = *width; value = I32(source, 8); sum += size;
    if (sum >= value) *width = size - sum + value;
    size = *height; sum = *source_y; value = I32(source, 0xc); sum += size;
    if (sum >= value) *height = size - sum + value;
    value = *dest_x;
    if (value >= I32(dest, 8)) return 0;
    size = *width; sum = value + size;
    if (sum < 0) return 0;
    if (value < 0) { *source_x += size - sum; *dest_x = 0; *width = sum; }
    value = *dest_y;
    if (value >= I32(dest, 0xc)) return 0;
    size = *height; sum = value + size;
    if (sum < 0) return 0;
    if (value < 0) { value = *source_y + (size - sum); *source_y = value; *dest_y = 0; *height = sum; }
    value = *width; sum = *dest_x; size = I32(dest, 8); sum += value;
    if (sum >= size) *width = size - sum + value;
    value = *height; size = I32(dest, 0xc); sum = *dest_y + value;
    if (sum >= size) *height = size - sum + value;
    if (*width <= 0) return 0;
    if (*height <= 0) return 0;
    return 1;
}
/* 0x204784e0, SpriteCopy(dest, x, y, width, height, source x, source y, key): this
 * section's pixels into dest's, all but the key colour (0: zero; -1: the first pixel's) */
void TC fs98_simsui_dib_sprite(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                               int32_t source_x, int32_t source_y, int32_t key)
{
    uint32_t source_stride, dest_skip, source_skip;
    uint8_t *from, *to;
    if (!fs98_simsui_dib_clip(dib, dest, dib, &x, &y, &source_x, &source_y, &width, &height)) return;
    if (key == -1) key = *(uint8_t *)PTR(dib, 0x64);
    source_stride = STRIDE(dib);
    source_skip = source_stride - (uint32_t)width;
    dest_skip = STRIDE(dest) - (uint32_t)width;
    from = (uint8_t *)PTR(dib, 0x64) + (uint32_t)source_y * source_stride + source_x;
    to = (uint8_t *)PTR(dest, 0x64) + (uint32_t)y * STRIDE(dest) + x;
    if (!key) {
        while (height--) {
            int32_t count;
            for (count = width; count; --count) {
                uint8_t pixel = *from;
                if (pixel) *to = pixel;
                ++from; ++to;
            }
            from += source_skip;
            to += dest_skip;
        }
    } else {
        while (height--) {
            int32_t count;
            for (count = width; count; --count) {
                uint8_t pixel = *from;
                if (pixel != (uint8_t)key) *to = pixel;
                ++from; ++to;
            }
            from += source_skip;
            to += dest_skip;
        }
    }
}
/* 0x20478430, SpriteCopy(dest, CRect &, key) */
void TC fs98_simsui_dib_sprite_rect(void *dib, void *dest, const int32_t *rect, int32_t key)
{
    fs98_simsui_dib_sprite(dib, dest, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 0, 0, key);
}
/* 0x20478470, SpriteCopy(dest, x, y, width, height, key) */
void TC fs98_simsui_dib_sprite_at(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                                  int32_t key)
{
    fs98_simsui_dib_sprite(dib, dest, x, y, width, height, 0, 0, key);
}
/* 0x204784a0, SpriteCopy(dest, CRect &, CPoint &, key) */
void TC fs98_simsui_dib_sprite_from(void *dib, void *dest, const int32_t *rect, const int32_t *point, int32_t key)
{
    fs98_simsui_dib_sprite(dib, dest, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], point[0], point[1],
                           key);
}
/* 0x20478600, CreateMask(dest, x, y, width, height, source x, source y, BYTE colour, key,
 * all): dest painted `colour` wherever this section is not the key (-1 without all: the
 * first pixel's), or everywhere with all set */
void TC fs98_simsui_dib_mask(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                             int32_t source_x, int32_t source_y, uint32_t colour, int32_t key, int32_t all)
{
    uint32_t source_stride, dest_stride;
    uint8_t *from, *to;
    if (!fs98_simsui_dib_clip(dib, dest, dib, &x, &y, &source_x, &source_y, &width, &height)) return;
    if (key == -1 && !all) key = *(uint8_t *)PTR(dib, 0x64);
    source_stride = STRIDE(dib);
    dest_stride = STRIDE(dest);
    from = (uint8_t *)PTR(dib, 0x64) + (uint32_t)source_y * source_stride + source_x;
    to = (uint8_t *)PTR(dest, 0x64) + (uint32_t)y * dest_stride + x;
    while (height--) {
        int32_t count;
        for (count = width; count; --count) {
            if (*from != (uint8_t)key || all) *to = (uint8_t)colour;
            ++from; ++to;
        }
        from += source_stride - (uint32_t)width;
        to += dest_stride - (uint32_t)width;
    }
}
/* 0x20478790, CopyBits(dest, x, y, width, height, source x, source y) */
void TC fs98_simsui_dib_copy(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height,
                             int32_t source_x, int32_t source_y)
{
    uint32_t source_stride, dest_stride;
    uint8_t *from, *to;
    if (!fs98_simsui_dib_clip(dib, dest, dib, &x, &y, &source_x, &source_y, &width, &height)) return;
    source_stride = STRIDE(dib);
    dest_stride = STRIDE(dest);
    from = (uint8_t *)PTR(dib, 0x64) + (uint32_t)source_y * source_stride + source_x;
    to = (uint8_t *)PTR(dest, 0x64) + (uint32_t)y * dest_stride + x;
    while (height--) {
        int32_t count;
        for (count = width; count; --count) *to++ = *from++;
        from += source_stride - (uint32_t)width;
        to += dest_stride - (uint32_t)width;
    }
}
/* 0x204786f0, CopyBits(dest, CRect &) */
void TC fs98_simsui_dib_copy_rect(void *dib, void *dest, const int32_t *rect)
{
    fs98_simsui_dib_copy(dib, dest, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 0, 0);
}
/* 0x20478720, CopyBits(dest, x, y, width, height) */
void TC fs98_simsui_dib_copy_at(void *dib, void *dest, int32_t x, int32_t y, int32_t width, int32_t height)
{
    fs98_simsui_dib_copy(dib, dest, x, y, width, height, 0, 0);
}
/* 0x20478750, CopyBits(dest, CRect &, CPoint &) */
void TC fs98_simsui_dib_copy_from(void *dib, void *dest, const int32_t *rect, const int32_t *point)
{
    fs98_simsui_dib_copy(dib, dest, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], point[0], point[1]);
}
/* a row of a 9-slice: the left tile's row, the middle one's repeated across, the right's */
static uint8_t *tile_row(uint8_t *to, const uint8_t *left, const uint8_t *middle, const uint8_t *right,
                         int32_t tile_width, uint32_t left_count, int32_t middle_count)
{
    uint32_t count;
    const uint8_t *from = middle;
    int32_t remaining = tile_width;
    for (count = left_count; count; --count) *to++ = *left++;
    if (middle_count) {
        do {
            *to++ = *from++;
            if (!--remaining) { remaining = tile_width; from = middle; }
        } while (--middle_count);
    }
    for (count = (uint32_t)tile_width; count; --count) *to++ = *right++;
    return to;
}
/* 0x20478850, Tile(CDIBSection *dest): dest painted as a 9-slice from this section, whose
 * nine tiles (a third of its width... its own width wide, a ninth of its height tall) are
 * stacked top to bottom: the top row's left, middle, right, then the middle row's, then
 * the bottom row's. The corners keep their size, the edges and centre repeat. */
void TC fs98_simsui_dib_tile(void *dib, void *dest)
{
    int32_t dest_width = I32(dest, 8), width = I32(dib, 8), third, rows, cycle, count;
    uint32_t dest_skip = (((uint32_t)dest_width + 3) & 0xfffffffc) - (uint32_t)dest_width;
    uint32_t stride = ((uint32_t)width + 3) & 0xfffffffc, tile;
    uint8_t *bits = PTR(dib, 0x64), *to = PTR(dest, 0x64);
    const uint8_t *left, *middle, *right;
    third = I32(dib, 0xc) / 9;
    middle = bits + (uint32_t)third * stride;
    right = bits + (uint32_t)third * stride * 2;
    for (count = third; count; --count) {
        to = tile_row(to, middle - (uint32_t)third * stride, middle, right, width, (uint32_t)width,
                      dest_width - width * 2);
        middle += stride;
        right += stride;
        to += dest_skip;
    }
    tile = ((I32(dib, 8) + 3) & 0xfffffffc) * (uint32_t)third;
    rows = I32(dest, 0xc) - third * 2;
    cycle = third;
    left = bits + tile * 3; middle = bits + tile * 4; right = bits + tile * 5;
    for (; rows; --rows) {
        to = tile_row(to, left, middle, right, width, (uint32_t)I32(dib, 8), dest_width - width * 2);
        if (!--cycle) {
            cycle = third;
            left = bits + tile * 3; middle = bits + tile * 4; right = bits + tile * 5;
        } else {
            left += stride; middle += stride; right += stride;
        }
        to += dest_skip;
    }
    tile = ((I32(dib, 8) + 3) & 0xfffffffc) * (uint32_t)third;
    left = bits + tile * 6; middle = bits + tile * 7; right = bits + tile * 8;
    for (count = third; count; --count) {
        to = tile_row(to, left, middle, right, width, (uint32_t)width, dest_width - width * 2);
        left += stride; middle += stride; right += stride;
        to += dest_skip;
    }
}
/* 0x20478bc0, BitBlt(CDC *, x, y, width, height, source x, source y, rop): the section
 * through a memory DC with the colour table that suits the device */
void TC fs98_simsui_dib_blit(void *dib, void *dc, int32_t x, int32_t y, int32_t width, int32_t height,
                             int32_t source_x, int32_t source_y, uint32_t rop)
{
    uint8_t memory[0x10];
    void *bitmap = AT(dib, 0x5c), *old, *pal;
    uint32_t active_process = 0, foreground_process = 0, active, foreground;
    void *table;
    CALL_THIS(MFC_323, void *, (void *))(memory);
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory,
        CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(dc ? D32(dc, 4) : 0));
    old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
    pal = PTR(dib, 0x58);
    active = CALL_STD(GetActiveWindow, uint32_t, (void))();
    foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
    if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
    if (foreground) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
    if (!D32(pal, 0x808) && (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(D32(memory, 8), 0x26) & 0x100))
        table = AT(pal, 0x408);
    else table = AT(pal, 8);
    CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(memory, 4), 0, 0x100, table);
    CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
        D32(dc, 4), x, y, width, height, D32(memory, 4), source_x, source_y, rop);
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
    CALL_THIS(MFC_640, void, (void *))(memory);
}
/* 0x20478b10, BitBlt(CDC *, CRect &, rop) */
void TC fs98_simsui_dib_blit_rect(void *dib, void *dc, const int32_t *rect, uint32_t rop)
{
    fs98_simsui_dib_blit(dib, dc, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 0, 0, rop);
}
/* 0x20478b50, BitBlt(CDC *, x, y, width, height, rop) */
void TC fs98_simsui_dib_blit_at(void *dib, void *dc, int32_t x, int32_t y, int32_t width, int32_t height,
                                uint32_t rop)
{
    fs98_simsui_dib_blit(dib, dc, x, y, width, height, 0, 0, rop);
}
/* 0x20478b80, BitBlt(CDC *, CRect &, CPoint &, rop) */
void TC fs98_simsui_dib_blit_from(void *dib, void *dc, const int32_t *rect, const int32_t *point, uint32_t rop)
{
    fs98_simsui_dib_blit(dib, dc, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], point[0], point[1], rop);
}
/* 0x20478d40, GetColorIdxNotUsed: the first colour from 1 to 254 the bits do not hold, or
 * -1. RETAIL BUG (kept): SUI-5 (docs/retail-bugs.md): the search takes in the byte just
 * past the bits. */
int32_t TC fs98_simsui_dib_unused_colour(void *dib)
{
    uint8_t *bits = PTR(dib, 0x64);
    uint8_t *end = bits + STRIDE(dib) * D32(dib, 0xc);
    int32_t colour = 1;
    do {
        uint8_t *at;
        int32_t unused = 1;
        for (at = bits; at <= end; ++at)
            if (*at == (uint8_t)colour) { unused = 0; break; }
        if (unused) return colour;
    } while (++colour < 0xff);
    return -1;
}
/* 0x20478d90, BlurShadowFromMask(CDIBSection &mask, row, CDIBPal *): from `row` down, each
 * pixel where the mask is clear darkened by .data's shadow table (+0x204f0, six levels per
 * colour) by how near (1 to 5 pixels right, or as many rows down) the mask is set */
int32_t TC fs98_simsui_dib_blur_shadow(void *dib, void *mask, int32_t row, void *pal)
{
    uint32_t stride = STRIDE(dib), mask_stride;
    uint8_t *to = (uint8_t *)PTR(dib, 0x64) + (uint32_t)row * stride;
    uint8_t *end = (uint8_t *)PTR(dib, 0x64) + D32(dib, 0xc) * stride;
    uint8_t *at, *mask_end, *down, *down2, *down4;
    (void)pal;
    mask_stride = STRIDE(mask);
    mask_end = (uint8_t *)PTR(mask, 0x64) + D32(mask, 0xc) * mask_stride;
    at = PTR(mask, 0x64);
    if (at >= mask_end) return 1;
    down2 = at + mask_stride * 2;
    down = at + mask_stride;
    down4 = at + mask_stride * 4;
#define SET(pointer) ((pointer) <= mask_end && *(pointer))
    do {
        if (to >= end) return 1;
        if (!*at) {
            uint32_t level;
            if (SET(at + 1) || SET(down)) level = 0;
            else if (SET(at + 2) || SET(down2)) level = 1;
            else if (SET(at + 3) || SET(down2 + mask_stride)) level = 2;
            else if (SET(at + 4) || SET(down4)) level = 3;
            else if (SET(at + 5) || SET(down4 + mask_stride)) level = 4;
            else level = 5;
            *to = D8(fs98_simsui_image, 0x204f0 + level + (uint32_t)*to * 6);
        }
        ++to; ++at; ++down; ++down2; ++down4;
    } while (at < mask_end);
#undef SET
    return 1;
}
