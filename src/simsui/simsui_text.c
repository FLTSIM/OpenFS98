/* CSimsText (0xb8 bytes, vtable 0x173c0): a CStatic that keeps its own text (+0xb4) and
 * draws it in a copy of a CSimsFont (+0x40; the font to use +0xb0, none until Init), aligned
 * as its static style says. Its GetWindowText(CString &) is in simsui_combo.c. */
#include "simsui_internal.h"

#define VTABLE_STATIC_BASE 0x16328
#define VTABLE_TEXT 0x173c0

/* 0x20481620, CSimsStatic::GetMessageMap; 0x20481630, CSimsSpinButtonCtrl::GetMessageMap */
const void *TC fs98_simsui_static_message_map(const void *window) { (void)window; return IMAGE(0x17300); }
const void *TC fs98_simsui_spin_message_map(const void *window) { (void)window; return IMAGE(0x17320); }
/* 0x20481640 */
void *TC fs98_simsui_text_construct(void *text)
{
    mfc_wnd_construct(text);
    PTR(text, 0) = IMAGE(VTABLE_STATIC_BASE);
    fs98_simsui_font_construct(AT(text, 0x40));
    CALL_THIS(MFC_540, void *, (void *))(AT(text, 0xb4));
    PTR(text, 0) = IMAGE(VTABLE_TEXT);
    D32(text, 0xb0) = 0;
    return text;
}
/* 0x20481710 */
void TC fs98_simsui_text_destroy(void *text)
{
    PTR(text, 0) = IMAGE(VTABLE_TEXT);
    D32(text, 0xb0) = 0;
    CALL_THIS(MFC_800, void, (void *))(AT(text, 0xb4));
    fs98_simsui_font_destroy(AT(text, 0x40));
    CALL_THIS(MFC_795, void, (void *))(text);
}
/* 0x204816b0 */
void *TC fs98_simsui_text_delete(void *text, uint32_t flags)
{
    return fs98_simsui_delete_object(text, flags, 0xb8, fs98_simsui_text_destroy, fs98_simsui_ui_delete);
}
/* 0x20481780 */
const void *TC fs98_simsui_text_message_map(const void *text) { (void)text; return IMAGE(0x17340); }
/* 0x20481790, Init(CSimsFont *): the font copied; the text and place from its spec
 * "name#l,t,r,b" (else its text, where it is, remapped); the IME kept out */
void TC fs98_simsui_text_init(void *text, void *font)
{
    uint32_t spec, name;
    int32_t rect[4];
    D32(text, 0x44) = D32(font, 4);
    D32(text, 0x48) = D32(font, 8);
    D32(text, 0x4c) = D32(font, 0xc);
    D32(text, 0x90) = D32(font, 0x50);
    D32(text, 0x94) = D32(font, 0x54);
    D16(text, 0x98) = D16(font, 0x58);
    PTR(text, 0xb0) = AT(text, 0x40);
    CALL_THIS(MFC_540, void *, (void *))(&spec);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_3874, void, (void *, void *))(text, &spec);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&spec, '#') != -1)
        fs98_simsui_parse_rect(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&spec, 0), AT(text, 0xb4), rect);
    else {
        fs98_simsui_remap_rect(rect, text);
        CALL_THIS(MFC_858, void *, (void *, const void *))(AT(text, 0xb4), &spec);
    }
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        text, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
    CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(text, 0x20), 0);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&spec);
}
/* 0x204818c0, SetWindowText: the window's text set while it is no wide (so nothing shows
 * through), its own kept, and repainted */
void TC fs98_simsui_text_set_window_text(void *text, const char *string)
{
    int32_t rect[4];
    void *parent;
    if (!D32(text, 0xb0)) {
        CALL_THIS(MFC_6199, void, (void *, const char *))(text, string);
        return;
    }
    CALL_STD(GetWindowRect, int32_t, (uint32_t, int32_t *))(D32(text, 0x20), rect);
    parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(text, 0x20)));
    CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect);
    CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect + 2);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        text, rect[0], rect[1], 0, rect[3] - rect[1], 1);
    CALL_THIS(MFC_6199, void, (void *, const char *))(text, string);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        text, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
    CALL_THIS(MFC_860, void *, (void *, const char *))(AT(text, 0xb4), string);
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(text, 0x20), 0, 1);
}
/* 0x204819a0, GetWindowText(buffer, room): its own text copied whole (the room not
 * checked: RETAIL BUG (kept): SUI-11, docs/retail-bugs.md); its length */
int32_t TC fs98_simsui_text_get_window_text(void *text, char *buffer, int32_t room)
{
    const char *own;
    if (!D32(text, 0xb0)) return CALL_THIS(MFC_3873, int32_t, (void *, char *, int32_t))(text, buffer, room);
    own = (const char *)(uintptr_t)D32(text, 0xb4);
    {
        uint32_t length = 0;
        while (own[length]) ++length;
        fs98_simsui_copy(buffer, own, length + 1);
    }
    return I32((uintptr_t)D32(text, 0xb4), -8);
}
/* 0x20481a30, OnPaint: its text drawn in the font, aligned by the static style (left,
 * centre, right; SS_LEFTNOWORDWRAP on one line), with prefixes or not (SS_NOPREFIX), greyed
 * when disabled */
void TC fs98_simsui_text_paint(void *text)
{
    uint8_t dc[0x54];
    int32_t rect[4];
    uint32_t saved_colour, style, copy;
    uint16_t saved_format;
    struct fs98_simsui_rect area;
    CALL_THIS(MFC_470, void *, (void *, void *))(dc, text);
    if (!PTR(text, 0xb0)) {
        CALL_THIS(MFC_755, void, (void *))(dc);
        return;
    }
    saved_colour = D32(PTR(text, 0xb0), 8);
    saved_format = D16(PTR(text, 0xb0), 0x58);
    if (!CALL_THIS(MFC_4123, int32_t, (void *))(text)) D32(PTR(text, 0xb0), 8) = 0x8b8b8b;
    D16(PTR(text, 0xb0), 0x58) &= 0xf8df;
    style = CALL_THIS(MFC_3797, uint32_t, (void *))(text) & 0x1f;
    D8(PTR(text, 0xb0), 0x58) |= 0x10;
    switch (style) {
    case 1: D8(PTR(text, 0xb0), 0x59) |= 4; break;
    case 2: D8(PTR(text, 0xb0), 0x59) |= 2; break;
    case 0xc: D16(PTR(text, 0xb0), 0x58) &= 0xffef;      /* fall through */
    case 0: D8(PTR(text, 0xb0), 0x59) |= 1; break;
    default: break;
    }
    if (CALL_THIS(MFC_3797, uint32_t, (void *))(text) & 0x80) D8(PTR(text, 0xb0), 0x58) |= 0x20;
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(text, 0x20), rect);
    area.left = rect[0]; area.top = rect[1]; area.right = rect[2]; area.bottom = rect[3];
    CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, AT(text, 0xb4));
    fs98_simsui_font_draw_text(PTR(text, 0xb0), dc, copy, area, 0);
    D32(PTR(text, 0xb0), 8) = saved_colour;
    D16(PTR(text, 0xb0), 0x58) = saved_format;
    CALL_THIS(MFC_755, void, (void *))(dc);
}
