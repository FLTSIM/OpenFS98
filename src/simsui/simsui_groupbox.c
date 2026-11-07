/* CSimsGroupbox (0xb4 bytes, vtable 0x16268): a CStatic drawn as an etched frame with its
 * title in a copy of a CSimsFont.
 *
 * The CStatic (CWnd) to +0x40; its CSimsFont +0x40 (a copy of the given font's CFont,
 * colours and flags); the font to draw with +0xb0 (+0x40, or none). */
#include "simsui_internal.h"

#define VTABLE_STATIC_BASE 0x16328
#define VTABLE_GROUPBOX 0x16268
#define GROUPBOX_SIZE 0xb4

/* 0x20479e00 */
void *TC fs98_simsui_groupbox_construct(void *box)
{
    mfc_wnd_construct(box);
    PTR(box, 0) = IMAGE(VTABLE_STATIC_BASE);
    fs98_simsui_font_construct(AT(box, 0x40));
    PTR(box, 0) = IMAGE(VTABLE_GROUPBOX);
    D32(box, 0xb0) = 0;
    return box;
}
/* 0x20479ec0 */
void TC fs98_simsui_groupbox_destroy(void *box)
{
    PTR(box, 0) = IMAGE(VTABLE_GROUPBOX);
    D32(box, 0xb0) = 0;
    fs98_simsui_font_destroy(AT(box, 0x40));
    CALL_THIS(MFC_795, void, (void *))(box);
}
/* 0x20479e60 */
void *TC fs98_simsui_groupbox_delete(void *box, uint32_t flags)
{
    return fs98_simsui_delete_object(box, flags, GROUPBOX_SIZE, fs98_simsui_groupbox_destroy, fs98_simsui_mfc_free);
}
/* 0x2047a4c0: the CStatic's deleting destructor */
void *TC fs98_simsui_cstatic_delete(void *window, uint32_t flags)
{
    CALL_THIS(MFC_795, void, (void *))(window);
    if (flags & 1) mfc_operator_delete(window);
    return window;
}
/* 0x20479f20 */
const void *TC fs98_simsui_groupbox_message_map(const void *box) { (void)box; return IMAGE(0x16230); }
/* 0x20479f30, Init(CSimsFont *): the font copied (or none), the window remapped */
void TC fs98_simsui_groupbox_init(void *box, void *font)
{
    int32_t rect[4];
    if (font) {
        D32(box, 0x44) = D32(font, 4);
        D32(box, 0x48) = D32(font, 8);
        D32(box, 0x4c) = D32(font, 0xc);
        D32(box, 0x90) = D32(font, 0x50);
        D32(box, 0x94) = D32(font, 0x54);
        D16(box, 0x98) = D16(font, 0x58);
        PTR(box, 0xb0) = AT(box, 0x40);
    } else D32(box, 0xb0) = 0;
    fs98_simsui_remap_rect(rect, box);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        box, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
}
/* a pen on the stack torn down: ~CGdiObject inline, and CObject's vtable */
static void pen_destroy(void *pen)
{
    PTR(pen, 0) = IMAGE(0x15030);
    mfc_gdi_delete_object(pen);
    PTR(pen, 0) = IMAGE(0x15018);
}
#define MOVE_TO(x, y) CALL_THIS(MFC_4297, void *, (void *, void *, int32_t, int32_t))(dc, point, x, y)
#define LINE_TO(x, y) CALL_THIS(MFC_4133, int32_t, (void *, int32_t, int32_t))(dc, x, y)
#define SELECT_PEN(pen) CALL_THIS(MFC_5788, void *, (void *, void *))(dc, pen)
/* 0x20479fc0, OnPaint: an etched frame (dark and white, black and light) round the client
 * area, its top through the middle of the title, which is drawn in the font (in grey when
 * the box is disabled) after a gap at the left */
void TC fs98_simsui_groupbox_paint(void *box)
{
    uint8_t dc[0x60], white[8], light[8], dark[8], black[8];
    int32_t rect[4], size[2] = {0, 0}, point[2];
    uint32_t text;
    void *old;
    CALL_THIS(MFC_470, void *, (void *, void *))(dc, box);
    CALL_THIS(MFC_472, void *, (void *, int32_t, int32_t, uint32_t))(white, 0, 0, 0xffffff);
    CALL_THIS(MFC_472, void *, (void *, int32_t, int32_t, uint32_t))(light, 0, 0, 0xaaaaaa);
    CALL_THIS(MFC_472, void *, (void *, int32_t, int32_t, uint32_t))(dark, 0, 0, 0x555555);
    CALL_THIS(MFC_472, void *, (void *, int32_t, int32_t, uint32_t))(black, 0, 0, 0);
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(box, 0x20), rect);
    CALL_THIS(MFC_5736, uint32_t, (void *))(dc);
    CALL_THIS(MFC_3874, void, (void *, void *))(box, &text);
    if (I32((uintptr_t)text, -8) && PTR(box, 0xb0)) {
        CALL_THIS(MFC_5789, void *, (void *, void *))(dc, PTR(PTR(box, 0xb0), 4));
        CALL_STD(GetTextExtentPointA, int32_t, (uint32_t, const char *, int32_t, int32_t *))(
            D32(dc, 8), (const char *)(uintptr_t)text, I32((uintptr_t)text, -8), size);
        rect[0] = rect[0] + size[0] + 7;
        rect[1] = size[1] / 2 + 1;
    }
    old = SELECT_PEN(black);
    MOVE_TO(1, rect[1] + 1); LINE_TO(1, rect[3] - 2);
    MOVE_TO(rect[0], rect[1] + 1); LINE_TO(rect[2] - 2, rect[1] + 1);
    SELECT_PEN(dark);
    MOVE_TO(0, rect[1]); LINE_TO(0, rect[3] - 1);
    MOVE_TO(rect[0], rect[1]); LINE_TO(rect[2] - 1, rect[1]);
    SELECT_PEN(light);
    MOVE_TO(1, rect[3] - 1); LINE_TO(rect[2] - 1, rect[3] - 1); LINE_TO(rect[2] - 1, rect[1] + 1);
    SELECT_PEN(white);
    MOVE_TO(0, rect[3]); LINE_TO(rect[2], rect[3]); LINE_TO(rect[2], rect[1]);
    SELECT_PEN(old);
    if (PTR(box, 0xb0) && I32((uintptr_t)text, -8)) {
        uint32_t saved = D32(PTR(box, 0xb0), 8), copy;
        struct fs98_simsui_rect area;
        area.left = 5; area.top = 0; area.right = rect[2]; area.bottom = size[1] + 2;
        if (!CALL_THIS(MFC_4123, int32_t, (void *))(box)) D32(PTR(box, 0xb0), 8) = 0x8b8b8b;
        CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
        fs98_simsui_font_draw_text(PTR(box, 0xb0), dc, copy, area, 0);
        D32(PTR(box, 0xb0), 8) = saved;
    }
    CALL_THIS(MFC_5678, uint32_t, (void *, int32_t))(dc, -1);
    CALL_THIS(MFC_800, void, (void *))(&text);
    pen_destroy(black);
    pen_destroy(dark);
    pen_destroy(light);
    pen_destroy(white);
    CALL_THIS(MFC_755, void, (void *))(dc);
}
#undef MOVE_TO
#undef LINE_TO
#undef SELECT_PEN
