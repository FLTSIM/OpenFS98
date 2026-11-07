/* CSimsFont (0x70 bytes, vtable 0x16218): a CFont and how SIMSUI draws text with it.
 *
 * The CFont +4; three colour sets for the states (normal +8, 1 +0x18, 2 +0x28), each the
 * text, the background, the shadow and the frame; the shadow's offset +0x38/+0x3c; the
 * rounded corners' size +0x40/+0x44; flags: a shadow +0x48, a frame +0x4c, a pill +0x50,
 * no background +0x54, and the format word +0x58 (0x100 left, 0x200 right-centred, 0x400
 * right... of DrawText's; 0x10 not single-line; 0x20 ellipsis; 1, 2, 4, 8 a rounded
 * corner's square); and the tab stops, a CDWordArray +0x5c (its data +0x60, size +0x64). */
#include "simsui_internal.h"

#define VTABLE_FONT 0x16218
#define VTABLE_GDI_OBJECT 0x15030
#define VTABLE_OBJECT 0x15018
#define FONT_SIZE 0x70
/* a CDC's virtual members */
#define DC_SAVE 0x24
#define DC_RESTORE 0x28
#define DC_STOCK 0x2c
#define DC_FONT 0x30
#define DC_BACK_COLOUR 0x34
#define DC_TEXT_COLOUR 0x38
#define DC_DRAW_TEXT 0x70

/* 0x20479580, Construct: the defaults */
void TC fs98_simsui_font_defaults(void *font)
{
    D32(font, 0xc) = 0xffffff; D32(font, 0x18) = 0xffffff; D32(font, 0x2c) = 0xffffff; D32(font, 0x34) = 0xffffff;
    D32(font, 0x10) = 0x1000000; D32(font, 0x30) = 0x1000000;
    D32(font, 0x38) = 2; D32(font, 0x3c) = 2;
    D32(font, 0x40) = 0xf; D32(font, 0x44) = 0xf;
    D32(font, 4) = 0; D32(font, 8) = 0; D32(font, 0x14) = 0;
    D32(font, 0x1c) = 0x1000004;
    D32(font, 0x20) = 0; D32(font, 0x24) = 0;
    D32(font, 0x28) = 0x8b8b8b;
    D32(font, 0x48) = 0; D32(font, 0x4c) = 0; D32(font, 0x50) = 0;
    D32(font, 0x54) = 1;
    D16(font, 0x58) = 0;
}
/* 0x204795f0 */
void *TC fs98_simsui_font_construct(void *font)
{
    PTR(font, 0) = IMAGE(VTABLE_OBJECT);
    CALL_THIS(MFC_559, void *, (void *))(AT(font, 0x5c));
    PTR(font, 0) = IMAGE(VTABLE_FONT);
    fs98_simsui_font_defaults(font);
    return font;
}
/* 0x204796a0, CSimsFont(CFont *) */
void *TC fs98_simsui_font_construct_with(void *font, void *base)
{
    PTR(font, 0) = IMAGE(VTABLE_OBJECT);
    CALL_THIS(MFC_559, void *, (void *))(AT(font, 0x5c));
    PTR(font, 0) = IMAGE(VTABLE_FONT);
    fs98_simsui_font_defaults(font);
    PTR(font, 4) = base;
    return font;
}
/* 0x20479700 */
void TC fs98_simsui_font_destroy(void *font)
{
    PTR(font, 0) = IMAGE(VTABLE_FONT);
    D32(font, 4) = 0;
    if (D32(font, 0x64)) CALL_THIS(MFC_6144, void, (void *, int32_t, int32_t))(AT(font, 0x5c), 0, -1);
    CALL_THIS(MFC_812, void, (void *))(AT(font, 0x5c));
    PTR(font, 0) = IMAGE(VTABLE_OBJECT);
}
/* 0x20479650 */
void *TC fs98_simsui_sims_font_delete(void *font, uint32_t flags)
{
    return fs98_simsui_delete_object(font, flags, FONT_SIZE, fs98_simsui_font_destroy, fs98_simsui_ui_delete);
}
/* 0x20479db0, SetTabStops(count, stops) */
int32_t TC fs98_simsui_font_tab_stops(void *font, int32_t count, const uint32_t *stops)
{
    void *array = AT(font, 0x5c);
    if (count < 0) return 0;
    CALL_THIS(MFC_6144, void, (void *, int32_t, int32_t))(array, 0, -1);
    if (count > 0) {
        do {
            CALL_THIS(MFC_5862, void, (void *, int32_t, uint32_t))(array, I32(array, 8), *stops);
            ++stops;
        } while (--count);
    }
    return 1;
}
/* a CPen or a CBrush on the stack torn down: ~CGdiObject inline, as retail leaves it */
static void gdi_local_destroy(void *object)
{
    PTR(object, 0) = IMAGE(VTABLE_GDI_OBJECT);
    mfc_gdi_delete_object(object);
}
/* 0x20479770, DrawSimsText(CDC *, CString, CRect, state): the text in its state's colours,
 * over its background (a rectangle, a pill or a rounded box) and frame, with a shadow, and
 * split at tabs by the tab stops. RETAIL BUG (kept): SUI-6 (docs/retail-bugs.md): a text
 * with more tabs than stops reads the stop past the last. RETAIL BUG (kept): SUI-7: a state
 * other than 0, 1 or 2 draws with colours never set (the state itself as the background). */
FS98_SIMSUI_NOINLINE void TC fs98_simsui_font_draw_text(void *font, void *dc, uint32_t text,
                                                         struct fs98_simsui_rect rect, int32_t state)
{
    void **vtable;
    void (TC *save)(void *);
    uint8_t brush[8];
    volatile uint32_t colour_text, colour_shadow, colour_frame, colour_back;
    uint32_t format;
    uint16_t flags;
    colour_back = (uint32_t)state;
    if (!PTR(font, 4) || !D32(PTR(font, 4), 4)) goto finished;
    vtable = PTR(dc, 0);
    save = (void (TC *)(void *))vtable[DC_SAVE / 4];
    save(dc);
    CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(dc, 1);
    flags = D16(font, 0x58);
    format = 0x50;
    if (flags & 0x200) format = 0x12;
    if (flags & 0x400) format = 0x11;
    if (flags & 0x20) format |= 0x800;
    if (!(flags & 0x10)) format |= 0x24;
    if (state == 0 || state == 1 || state == 2) {
        uint32_t at = state == 0 ? 8 : state == 1 ? 0x18 : 0x28;
        colour_text = D32(font, at);
        colour_back = D32(font, at + 4);
        colour_shadow = D32(font, at + 8);
        colour_frame = D32(font, at + 0xc);
    }
    CALL_THIS(MFC_283, void *, (void *, uint32_t))(brush, colour_back);
    VIRTUAL(dc, DC_FONT, void *, (void *, void *))(dc, PTR(font, 4));
    if (!D32(font, 0x54)) {
        int32_t height, half, corner;
        VIRTUAL(dc, DC_STOCK, void *, (void *, int32_t))(dc, 8);
        CALL_THIS(MFC_5787, void *, (void *, void *))(dc, brush);
        height = rect.bottom - rect.top;     /* a pill's ends are round: the height is their size */
        half = height / 2;
        if (D32(font, 0x50)) {
            uint16_t shape = D16(font, 0x58);
            corner = 0;
            I32(font, 0x40) = height;
            if ((shape & 5) && !(shape & 0xa)) corner = height;
            if (corner) {
#define RECTANGLE(a, b, c, d) CALL_STD(Rectangle, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(D32(dc, 4), a, b, c, d)
#define PIE(a, b, c, d, e, f, g, h) CALL_STD(Pie, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, \
                                                            int32_t, int32_t))(D32(dc, 4), a, b, c, d, e, f, g, h)
                RECTANGLE(corner + rect.left - 1, rect.top, rect.right - corner + 1, rect.bottom);
                if (D8(font, 0x58) & 1)
                    PIE(rect.left, rect.top, rect.left + corner * 2, rect.bottom + corner, corner + rect.left, rect.top,
                        rect.left, rect.bottom);
                else RECTANGLE(rect.left, rect.top, corner + rect.left, rect.bottom);
                if (D8(font, 0x58) & 4)
                    PIE(rect.right - corner * 2, rect.top, rect.right, rect.bottom + corner, rect.right, rect.bottom,
                        rect.right - corner, rect.top);
                else RECTANGLE(rect.right - corner, rect.top, rect.right, rect.bottom);
            } else {
                CALL_STD(RoundRect, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t))(
                    D32(dc, 4), rect.left, rect.top, rect.right, rect.bottom, I32(font, 0x40), I32(font, 0x44));
                if (!(D8(font, 0x58) & 1)) RECTANGLE(rect.left, rect.top, rect.left + I32(font, 0x40), half + rect.top);
                if (!(D8(font, 0x58) & 2)) RECTANGLE(rect.left, rect.bottom - half, rect.left + I32(font, 0x40), rect.bottom);
                if (!(D8(font, 0x58) & 4)) RECTANGLE(rect.right - I32(font, 0x40), rect.top, rect.right, rect.top + half);
                if (!(D8(font, 0x58) & 8)) RECTANGLE(rect.right - I32(font, 0x40), rect.bottom - half, rect.right, rect.bottom);
            }
        } else {
            uint8_t pen[8];
            void *old;
            save(dc);
            CALL_THIS(MFC_472, void *, (void *, int32_t, int32_t, uint32_t))(pen, 0, 1, colour_back);
            old = CALL_THIS(MFC_5788, void *, (void *, void *))(dc, pen);
            RECTANGLE(rect.left, rect.top, rect.right, rect.bottom);
            CALL_THIS(MFC_5788, void *, (void *, void *))(dc, old);
            VIRTUAL(dc, DC_RESTORE, int32_t, (void *, int32_t))(dc, -1);
            gdi_local_destroy(pen);
        }
    }
    if (D32(font, 0x4c) && !D32(font, 0x50)) {
        uint8_t pen[8];
        save(dc);
        CALL_THIS(MFC_472, void *, (void *, int32_t, int32_t, uint32_t))(pen, 0, 1, colour_frame);
        CALL_THIS(MFC_5788, void *, (void *, void *))(dc, pen);
        VIRTUAL(dc, DC_STOCK, void *, (void *, int32_t))(dc, 5);
        RECTANGLE(rect.left, rect.top, rect.right, rect.bottom);
        VIRTUAL(dc, DC_RESTORE, int32_t, (void *, int32_t))(dc, -1);
        gdi_local_destroy(pen);
    }
#undef RECTANGLE
#undef PIE
#define DRAW(string, area) VIRTUAL(dc, DC_DRAW_TEXT, int32_t, (void *, uint32_t, int32_t, void *, uint32_t))( \
        dc, string, I32((uintptr_t)(string), -8), area, format)
    if (D32(font, 0x48)) {
        int32_t back_x, back_y;
        CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(dc, 1);
        VIRTUAL(dc, DC_TEXT_COLOUR, uint32_t, (void *, uint32_t))(dc, colour_shadow);
        back_x = -I32(font, 0x38);
        back_y = -I32(font, 0x3c);
        CALL_STD(OffsetRect, int32_t, (void *, int32_t, int32_t))(&rect, I32(font, 0x38), I32(font, 0x3c));
        DRAW(text, &rect);
        CALL_STD(OffsetRect, int32_t, (void *, int32_t, int32_t))(&rect, back_x, back_y);
    }
    VIRTUAL(dc, DC_TEXT_COLOUR, uint32_t, (void *, uint32_t))(dc, colour_text);
    VIRTUAL(dc, DC_BACK_COLOUR, uint32_t, (void *, uint32_t))(dc, colour_back);
    {
        int32_t tabs = I32(font, 0x64);
        if (!tabs) DRAW(text, &rect);
        else {
            uint32_t rest, piece;
            struct fs98_simsui_rect area;
            int32_t stop = 0;
            CALL_THIS(MFC_535, void *, (void *, const void *))(&rest, &text);
            CALL_THIS(MFC_540, void *, (void *))(&piece);
            area = rect;
            if (tabs >= 0) {
                do {
                    uint32_t part;
                    int32_t at = CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&rest, 9);
                    if (at == -1) {
                        area.right = rect.right;
                        DRAW(rest, &area);
                        break;
                    }
                    CALL_THIS(MFC_4278, void *, (void *, void *, int32_t, int32_t))(&rest, &part, 0, at);
                    CALL_THIS(MFC_858, void *, (void *, const void *))(&piece, &part);
                    CALL_THIS(MFC_800, void, (void *))(&part);
                    area.right = ((int32_t *)PTR(font, 0x60))[stop] + rect.left;
                    DRAW(piece, &area);
                    CALL_THIS(MFC_4277, void *, (void *, void *, int32_t))(&rest, &part, at + 1);
                    CALL_THIS(MFC_858, void *, (void *, const void *))(&rest, &part);
                    CALL_THIS(MFC_800, void, (void *))(&part);
                    area.left = area.right + 1;
                    ++stop;
                } while (stop <= tabs);
            }
            CALL_THIS(MFC_800, void, (void *))(&piece);
            CALL_THIS(MFC_800, void, (void *))(&rest);
        }
    }
#undef DRAW
    VIRTUAL(dc, DC_RESTORE, int32_t, (void *, int32_t))(dc, -1);
    gdi_local_destroy(brush);
    PTR(brush, 0) = IMAGE(VTABLE_OBJECT);
finished:
    CALL_THIS(MFC_800, void, (void *))(&text);
}
