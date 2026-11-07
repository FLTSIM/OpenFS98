/* CDIBPal (a CPalette with two 256-entry colour tables, 0x80c bytes) and CDIBSection's
 * accessors.
 *
 * CDIBPal: vtable 0x15000, the palette handle +4 (CGdiObject's m_hObject), the plain colour
 * table +8 and the one for a palette device +0x408 (each 0x100 RGBQUADs), and +0x808, set
 * when the plain one is always to be used.
 *
 * CDIBSection: vtable 0x16078, width +8, height +0xc, the BITMAPINFOHEADER +0x1c (its
 * height +0x24, colours used +0x3c), the CBitmap +0x5c (handle +0x60), the bits +0x64. */
#include "simsui_internal.h"

#define VTABLE_PAL 0x15000
#define VTABLE_GDI_OBJECT 0x15030       /* CGdiObject's, stored while it is torn down */
#define VTABLE_OBJECT 0x15018           /* CObject's */
#define PAL_SIZE 0x80c
#define RC_PALETTE_ 0x100
#define RASTERCAPS_ 0x26

/* 0x20471000, SetColorTable(CDC *): the table that suits the DC into its selected DIB. The
 * foreground and active windows' processes are asked for and never used. */
void TC fs98_simsui_pal_set_color_table(void *pal, void *dc)
{
    uint32_t active_process = 0, foreground_process = 0;
    uint32_t active = CALL_STD(GetActiveWindow, uint32_t, (void))();
    uint32_t foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
    uint32_t handle;
    void *table;
    if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
    if (foreground) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
    if (!D32(pal, 0x808) &&
        (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(D32(dc, 8), RASTERCAPS_) & RC_PALETTE_)) {
        handle = dc ? D32(dc, 4) : 0;
        table = AT(pal, 0x408);
    } else {
        handle = dc ? D32(dc, 4) : 0;
        table = AT(pal, 8);
    }
    CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(handle, 0, 0x100, table);
}
/* 0x204710a0 */
void *TC fs98_simsui_pal_construct(void *pal)
{
    D32(pal, 4) = 0;
    PTR(pal, 0) = IMAGE(VTABLE_PAL);
    return pal;
}
/* 0x204710b0: an empty member (several vtables' slots) */
void TC fs98_simsui_nothing(void *object)
{
    (void)object;
}
/* 0x204710c0 (and 0x20471150, the same code): ~CGdiObject inline */
void TC fs98_simsui_pal_destroy(void *pal)
{
    PTR(pal, 0) = IMAGE(VTABLE_GDI_OBJECT);
    mfc_gdi_delete_object(pal);
    PTR(pal, 0) = IMAGE(VTABLE_OBJECT);
}
/* 0x20471110: CObject's deleting destructor */
void *TC fs98_simsui_object_delete(void *object, uint32_t flags)
{
    PTR(object, 0) = IMAGE(VTABLE_OBJECT);
    if (flags & 1) mfc_operator_delete(object);
    return object;
}
/* 0x20471130: CGdiObject's deleting destructor */
void *TC fs98_simsui_gdi_object_delete(void *object, uint32_t flags)
{
    fs98_simsui_pal_destroy(object);
    if (flags & 1) mfc_operator_delete(object);
    return object;
}
/* 0x204711a0: CDIBPal's deleting destructor. A single one goes back to ui_delete (CDIBPal's
 * operator delete), an array to MFC's. */
void *TC fs98_simsui_pal_delete(void *pal, uint32_t flags)
{
    if (flags & 2) {
        fs98_simsui_vector_destroy(pal, PAL_SIZE, I32(pal, -4), fs98_simsui_pal_destroy);
        mfc_operator_delete((uint8_t *)pal - 4);
        return pal;
    }
    fs98_simsui_pal_destroy(pal);
    if (flags & 1) fs98_simsui_ui_delete(pal);
    return pal;
}

/* 0x20471200, operator CBitmap * (and GetBitmap) */
void *TC fs98_simsui_dib_bitmap(void *dib)
{
    return AT(dib, 0x5c);
}
/* 0x20471210, operator HBITMAP */
uint32_t TC fs98_simsui_dib_handle(void *dib)
{
    void *bitmap = AT(dib, 0x5c);
    return bitmap ? D32(bitmap, 4) : 0;
}
/* 0x20471220 */
void *TC fs98_simsui_dib_bits(void *dib)
{
    return PTR(dib, 0x64);
}
/* 0x20471230 */
int32_t TC fs98_simsui_dib_height(void *dib)
{
    return I32(dib, 0xc);
}
/* 0x20471240 */
int32_t TC fs98_simsui_dib_bmi_height(void *dib)
{
    return I32(dib, 0x24);
}
/* 0x20471250 (and StorageWidth) */
int32_t TC fs98_simsui_dib_width(void *dib)
{
    return I32(dib, 8);
}
/* 0x20471260: a row's bytes, to a dword */
int32_t TC fs98_simsui_dib_width_bytes(void *dib)
{
    return (int32_t)((D32(dib, 8) + 3) & 0xfffffffc);
}
/* 0x20471270 */
int32_t TC fs98_simsui_dib_colour_count(void *dib)
{
    return fs98_simsui_info_colour_count(AT(dib, 0x1c));
}
/* 0x20471280 */
void *TC fs98_simsui_dib_info(void *dib)
{
    return AT(dib, 0x1c);
}
/* 0x20471290: within the bits? A row is taken as the width rounded up to a dword. */
int32_t TC fs98_simsui_dib_is_pointer_valid(void *dib, const uint8_t *pointer)
{
    uint32_t bits = D32(dib, 0x64);
    if ((uint32_t)(uintptr_t)pointer < bits) return 0;
    if ((uint32_t)(uintptr_t)pointer > ((D32(dib, 8) + 3) & 0xfffffffc) * D32(dib, 0xc) + bits - 1) return 0;
    return 1;
}
/* 0x20477ac0: a BITMAPINFOHEADER rather than a BITMAPCOREHEADER? */
int32_t SC fs98_simsui_info_is_header(const void *info)
{
    return D32(info, 0) != 0xc;
}
/* 0x20477ad0: the colour table's entries */
int32_t SC fs98_simsui_info_colour_count(const void *info)
{
    uint32_t bits = fs98_simsui_info_is_header(info) ? D16(info, 0xe) : D16(info, 0xa);
    int32_t count;
    switch (bits) {
    case 1: count = 2; break;
    case 4: count = 0x10; break;
    case 8: count = 0x100; break;
    default: count = 0; break;
    }
    if (fs98_simsui_info_is_header(info) && D32(info, 0x20)) count = I32(info, 0x20);
    return count;
}

/* 0x204774c0, Create(BITMAPINFO *): a palette from a DIB's colour table, which is also
 * kept as the plain table */
int32_t TC fs98_simsui_pal_create(void *pal, const void *info)
{
    uint32_t count = (uint32_t)fs98_simsui_info_colour_count(info);
    const uint8_t *colour = (const uint8_t *)info + 0x28;
    uint8_t *logical, *entry;
    uint32_t *plain;
    int32_t result;
    if (!count) return 0;
    logical = CALL_C(malloc, void *, (uint32_t))(count * 4 + 8);
    if (!logical) return 0;
    D16(logical, 0) = 0x300;
    D16(logical, 2) = (uint16_t)count;
    entry = logical + 4;
    plain = AT(pal, 8);
    do {
        entry[0] = colour[2];
        entry[1] = colour[1];
        entry[2] = colour[0];
        entry[3] = 0;
        *plain++ = D32(colour, 0);
        colour += 4;
        entry += 4;
    } while (--count);
    fs98_simsui_pal_init_tables(pal, logical + 4);
    result = CALL_THIS(MFC_1641, int32_t, (void *, uint32_t))(pal, CALL_STD(CreatePalette, uint32_t, (void *))(logical));
    CALL_C(free, void, (void *))(logical);
    return result;
}
/* 0x20477580, GetNumColors */
int32_t TC fs98_simsui_pal_colour_count(void *pal)
{
    volatile uint32_t count = 0;
    int32_t read = CALL_STD(GetObjectA, int32_t, (uint32_t, int32_t, volatile uint32_t *))(D32(pal, 4), 4, &count);
    return read ? (int32_t)count : 0;
}
/* 0x204775b0, Draw(CDC *, CRect *, BOOL background): the palette as a 16 x 16 grid of
 * swatches, as many as it has colours */
void TC fs98_simsui_pal_draw(void *pal, void *dc, const int32_t *rect, int32_t background)
{
    int32_t colours = fs98_simsui_pal_colour_count(pal);
    void *old = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, pal, background);
    int32_t row = 0, top = 0, bottom, column, left, right;
    CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
    while (colours) {
        bottom = rect[3] * (row + 1) / 16 + 1;
        column = 0;
        left = 0;
        while (colours) {
            uint8_t brush[8];
            void *old_brush;
            right = rect[2] * (column + 1) / 16 + 1;
            CALL_THIS(MFC_283, void *, (void *, uint32_t))(brush, ((((uint32_t)row << 4) + column) & 0xffff) | 0x1000000);
            old_brush = CALL_THIS(MFC_5787, void *, (void *, void *))(dc, brush);
            CALL_STD(Rectangle, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(D32(dc, 4), left - 1, top - 1,
                                                                                           right, bottom);
            CALL_THIS(MFC_5787, void *, (void *, void *))(dc, old_brush);
            --colours;
            fs98_simsui_pal_destroy(brush);
            ++column;
            left = right;
            if (column >= 16) break;
        }
        top = bottom;
        if (++row >= 16) break;
    }
    if (old) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old, 0);
}
/* 0x20477730, SetStaticColors: the system's twenty static colours at the ends, the rest
 * PC_NOCOLLAPSE */
int32_t TC fs98_simsui_pal_set_static_colours(void *pal)
{
    uint8_t entries[0x400], first[0x28], last[0x28];
    uint32_t window = CALL_STD(GetActiveWindow, uint32_t, (void))();
    uint32_t screen = CALL_STD(GetDC, uint32_t, (uint32_t))(window);
    uint32_t index;
    CALL_STD(GetPaletteEntries, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(
        CALL_STD(GetStockObject, uint32_t, (int32_t))(15), 0, 10, first);
    CALL_STD(GetPaletteEntries, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(
        CALL_STD(GetStockObject, uint32_t, (int32_t))(15), 10, 10, last);
    CALL_STD(GetPaletteEntries, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(pal, 4), 0, 0x100, entries);
    fs98_simsui_copy(entries, first, 0x28);
    for (index = 0; index < 0xec; ++index) entries[0x2b + index * 4] = 4;
    fs98_simsui_copy(entries + 0x3d8, last, 0x28);
    CALL_STD(SetPaletteEntries, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(pal, 4), 0, 0x100, entries);
    fs98_simsui_pal_init_tables(pal, entries);
    CALL_STD(ReleaseDC, int32_t, (uint32_t, uint32_t))(window, screen);
    return 1;
}
/* memcpy as MSVC inlines it: rep movsd, then rep movsb for the rest */
void fs98_simsui_copy(void *to, const void *from, uint32_t size)
{
    volatile uint32_t *out = to;
    const volatile uint32_t *in = from;
    volatile uint8_t *out_byte;
    const volatile uint8_t *in_byte;
    uint32_t count;
    for (count = size >> 2; count; --count) *out++ = *in++;
    out_byte = (volatile uint8_t *)out;
    in_byte = (const volatile uint8_t *)in;
    for (count = size & 3; count; --count) *out_byte++ = *in_byte++;
}
/* memset as MSVC inlines it: rep stosd, then rep stosb */
void fs98_simsui_fill(void *to, uint8_t value, uint32_t size)
{
    volatile uint32_t *out = to;
    volatile uint8_t *out_byte;
    uint32_t word = value * 0x01010101u, count;
    for (count = size >> 2; count; --count) *out++ = word;
    out_byte = (volatile uint8_t *)out;
    for (count = size & 3; count; --count) *out_byte++ = value;
}
/* 0x20477810, InitColorTables(PALETTEENTRY *): the plain table from the entries, and the
 * palette device's: each colour by its lightness ((max + min) / 2) to one of five of the
 * plain table's (0 below 64; 248 above 65 and below 144; 247 above 145 and below 176; 7
 * above 177 and below 216; 255 otherwise). RETAIL BUG (kept): SUI-2 (docs/retail-bugs.md):
 * the minimum is compared with green again rather than blue, so blue never lowers it. */
void TC fs98_simsui_pal_init_tables(void *pal, const uint8_t *entries)
{
    static const double half = 0.5, k64 = 64.0, k65 = 65.0, k144 = 144.0, k145 = 145.0, k176 = 176.0,
                        k177 = 177.0, k216 = 216.0;
    uint8_t *rgb = AT(pal, 8);
    uint32_t index, colour_248, colour_247, colour_0, colour_7, colour_255;
    for (index = 0; index < 0x100; ++index) {
        rgb[index * 4 + 2] = entries[index * 4];
        rgb[index * 4 + 1] = entries[index * 4 + 1];
        rgb[index * 4] = entries[index * 4 + 2];
    }
    colour_248 = D32(pal, 0x3e8);
    colour_247 = D32(pal, 0x3e4);
    colour_0 = D32(pal, 8);
    colour_7 = D32(pal, 0x24);
    colour_255 = D32(pal, 0x404);
    for (index = 0; index < 0x100; ++index) {
        uint8_t red = rgb[index * 4 + 2], green = rgb[index * 4 + 1], blue = rgb[index * 4];
        double high = red > green ? red : green, low = red < green ? red : green, lightness;
        uint32_t chosen;
        if (!(high > (double)blue)) high = blue;
        if (!(low < (double)green)) low = green;
        lightness = (high + low) * half;
        if (lightness < k64) chosen = colour_0;
        else if (lightness > k65 && lightness < k144) chosen = colour_248;
        else if (lightness > k145 && lightness < k176) chosen = colour_247;
        else if (lightness > k177 && lightness < k216) chosen = colour_7;
        else chosen = colour_255;
        D32(pal, 0x408 + index * 4) = chosen;
    }
}
/* 0x204779a0, SimsGetNearestPaletteIndex(COLORREF, PALETTEENTRY *): the closest of entries
 * 10..245 (the given ones, or the plain table's), by squared distance from .data's table
 * of squares (+0x202d6 is a difference of 0) */
int32_t TC fs98_simsui_pal_nearest(void *pal, uint32_t colour, const uint8_t *entries)
{
    int32_t red = colour & 0xff, green = (colour >> 8) & 0xff, blue = (colour >> 16) & 0xff;
    int32_t best = 0x8000001, chosen = 0, index;
#define SQUARE(difference) ((int32_t)D16(fs98_simsui_image, 0x202d6 + (difference) * 2))
    if (entries) {
        for (index = 10; index < 0xf6; ++index) {
            const uint8_t *entry = entries + index * 4;
            int32_t distance = SQUARE(red - entry[0]) + SQUARE(green - entry[1]) + SQUARE(blue - entry[2]);
            if (distance < best) { best = distance; chosen = index; }
        }
    } else {
        for (index = 10; index < 0xf6; ++index) {
            const uint8_t *entry = (const uint8_t *)pal + 8 + index * 4;
            int32_t distance = SQUARE(green - entry[1]) + SQUARE(red - entry[2]) + SQUARE(blue - entry[0]);
            if (distance < best) { best = distance; chosen = index; }
        }
    }
#undef SQUARE
    return chosen;
}
