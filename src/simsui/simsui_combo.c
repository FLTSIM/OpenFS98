/* CSimsComboBox (0x48 bytes, vtable 0x15b60): an owner-drawn CComboBox whose items are
 * drawn in a CSimsFont it recolours for the list, placed from a CSimsText's spec.
 *
 * The CComboBox (CWnd) to +0x40; the CSimsText it takes its spec from +0x40; the
 * CSimsFont +0x44. */
#include "simsui_internal.h"

#define VTABLE_COMBO 0x15b60
#define VTABLE_GDI_OBJECT 0x15030
#define VTABLE_OBJECT 0x15018
#define COMBO_SIZE 0x48

/* 0x20481a00, CSimsText::GetWindowText(CString &): its own text when it keeps one (+0xb0,
 * the text +0xb4), else the window's */
void TC fs98_simsui_text_window_text(void *text, void *out)
{
    if (!D32(text, 0xb0)) CALL_THIS(MFC_3874, void, (void *, void *))(text, out);
    else CALL_THIS(MFC_858, void *, (void *, const void *))(out, AT(text, 0xb4));
}
/* 0x20474cd0, CRect(const RECT *) */
void *TC fs98_simsui_rect_copy(void *rect, const void *from)
{
    CALL_STD(CopyRect, int32_t, (void *, const void *))(rect, from);
    return rect;
}
/* 0x20474cf0, a CGdiObject's complete destructor (a CBrush's or CPen's) */
void TC fs98_simsui_gdi_object_destroy(void *object)
{
    PTR(object, 0) = IMAGE(VTABLE_GDI_OBJECT);
    mfc_gdi_delete_object(object);
    PTR(object, 0) = IMAGE(VTABLE_OBJECT);
}
/* 0x204746c0 */
void *TC fs98_simsui_combo_construct(void *combo)
{
    mfc_wnd_construct(combo);
    PTR(combo, 0) = IMAGE(VTABLE_COMBO);
    D32(combo, 0x40) = 0;
    D32(combo, 0x44) = 0;
    return combo;
}
/* 0x20474730 */
void TC fs98_simsui_combo_destroy(void *combo)
{
    PTR(combo, 0) = IMAGE(VTABLE_COMBO);
    D32(combo, 0x44) = 0;
    CALL_THIS(MFC_616, void, (void *))(combo);
}
/* 0x204746e0 */
void *TC fs98_simsui_combo_delete(void *combo, uint32_t flags)
{
    return fs98_simsui_delete_object(combo, flags, COMBO_SIZE, fs98_simsui_combo_destroy, fs98_simsui_ui_delete);
}
/* 0x20474750, Init(font, CSimsText *): placed by the text's spec "name#l,t,r,b,drop" (the
 * edit at the bottom, the list `drop` tall below), else where it is, remapped; the box
 * six times its height; the font set, recoloured, and the IME kept out. RETAIL BUG (kept):
 * SUI-8 (docs/retail-bugs.md): a spec with four numbers leaves `drop` unset (a local; 0 here) */
void TC fs98_simsui_combo_init(void *combo, void *font, void *text)
{
    int32_t rect[4] = {0, 0, 0, 0}, drop = 0, remap = 0;
    PTR(combo, 0x40) = text;
    PTR(combo, 0x44) = font;
    if (text) {
        uint32_t spec, name;
        CALL_THIS(MFC_540, void *, (void *))(&spec);
        CALL_THIS(MFC_540, void *, (void *))(&name);
        fs98_simsui_text_window_text(text, &spec);
        if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&spec, '#') != -1)
            fs98_simsui_parse_rect_and(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&spec, 0), &name, rect, &drop);
        else remap = 1;
        rect[1] = rect[3];
        rect[3] += drop;
        CALL_THIS(MFC_800, void, (void *))(&name);
        CALL_THIS(MFC_800, void, (void *))(&spec);
    } else remap = 1;
    if (remap) fs98_simsui_remap_rect(rect, combo);
    rect[3] = rect[3] * 6 - rect[1] * 5;
    {
        void *base = PTR(PTR(combo, 0x44), 4);
        CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(combo, 0x20), 0x30,
                                                                                 base ? D32(base, 4) : 0, 1);
    }
    D32(PTR(combo, 0x44), 8) = 0;
    D32(PTR(combo, 0x44), 0xc) = 0xffffff;
    D32(PTR(combo, 0x44), 0x14) = 0xffffff;
    D32(PTR(combo, 0x44), 0x18) = 0xffffff;
    D32(PTR(combo, 0x44), 0x1c) = 0x1000004;
    D32(PTR(combo, 0x44), 0x24) = 0x10000f8;
    D32(PTR(combo, 0x44), 0x28) = 0x8b8b8b;
    D32(PTR(combo, 0x44), 0x2c) = 0xffffff;
    D32(PTR(combo, 0x44), 0x34) = 0x8b8b8b;
    D32(PTR(combo, 0x44), 0x4c) = 1;
    D32(PTR(combo, 0x44), 0x54) = 0;
    D8(PTR(combo, 0x44), 0x58) |= 0x20;
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        combo, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
    CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(combo, 0x20), 0);
}
/* 0x20474910, DrawItem(DRAWITEMSTRUCT *): the item, a space before it, in the font: framed
 * when selected and focused, in colour 7 when selected, plain otherwise; nothing drawn when
 * the box is disabled */
void TC fs98_simsui_combo_draw_item(void *combo, const uint8_t *item)
{
    uint32_t text;
    void *dc;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    dc = CALL_STD(MFC_2859, void *, (uint32_t))(D32(item, 0x18));
    if (PTR(combo, 0x44)) {
        uint32_t saved_text = D32(PTR(combo, 0x44), 8), saved_shadow = D32(PTR(combo, 0x44), 0x18);
        if (I32(item, 8) != -1) {
            uint32_t spaced;
            CALL_THIS(MFC_3317, void, (void *, int32_t, void *))(combo, I32(item, 8), &text);
            CALL_THIS(MFC_858, void *, (void *, const void *))(
                &text, CALL_STD(MFC_925, void *, (void *, uint32_t, const void *))(&spaced, ' ', &text));
            CALL_THIS(MFC_800, void, (void *))(&spaced);
        }
        if (CALL_THIS(MFC_4123, int32_t, (void *))(combo)) {
            uint32_t state = D32(item, 0x10), copy;
            struct fs98_simsui_rect area;
            if ((state & 1) && (state & 0x10)) {
                D32(PTR(combo, 0x44), 0x4c) = 1;
                fs98_simsui_rect_copy(&area, item + 0x1c);
                CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
                fs98_simsui_font_draw_text(PTR(combo, 0x44), dc, copy, area, 1);
                D32(PTR(combo, 0x44), 0x4c) = 0;
            } else if (state & 1) {
                uint32_t saved_back;
                D32(PTR(combo, 0x44), 0x4c) = 1;
                saved_back = D32(PTR(combo, 0x44), 0xc);
                D32(PTR(combo, 0x44), 0xc) = 0x1000007;
                fs98_simsui_rect_copy(&area, item + 0x1c);
                CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
                fs98_simsui_font_draw_text(PTR(combo, 0x44), dc, copy, area, 0);
                D32(PTR(combo, 0x44), 0xc) = saved_back;
                D32(PTR(combo, 0x44), 0x4c) = 0;
            } else {
                fs98_simsui_rect_copy(&area, item + 0x1c);
                CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
                fs98_simsui_font_draw_text(PTR(combo, 0x44), dc, copy, area, 0);
            }
        }
        D32(PTR(combo, 0x44), 8) = saved_text;
        D32(PTR(combo, 0x44), 0x18) = saved_shadow;
    }
    CALL_THIS(MFC_800, void, (void *))(&text);
}
/* 0x20474ad0, CompareItem(COMPAREITEMSTRUCT *): the two items' texts, by _mbscmp */
int32_t TC fs98_simsui_combo_compare(void *combo, const uint8_t *compare)
{
    uint32_t first, second;
    int32_t result;
    CALL_THIS(MFC_540, void *, (void *))(&first);
    CALL_THIS(MFC_540, void *, (void *))(&second);
    CALL_THIS(MFC_3317, void, (void *, int32_t, void *))(combo, I32(compare, 0xc), &first);
    CALL_THIS(MFC_3317, void, (void *, int32_t, void *))(combo, I32(compare, 0x14), &second);
    if (CALL_C(_mbscmp, int, (const char *, const char *))((const char *)(uintptr_t)first,
                                                           (const char *)(uintptr_t)second) < 0) result = -1;
    else result = CALL_C(_mbscmp, int, (const char *, const char *))((const char *)(uintptr_t)first,
                                                                     (const char *)(uintptr_t)second) > 0;
    CALL_THIS(MFC_800, void, (void *))(&second);
    CALL_THIS(MFC_800, void, (void *))(&first);
    return result;
}
/* 0x20474c00 */
const void *TC fs98_simsui_combo_message_map(const void *combo) { (void)combo; return IMAGE(0x15b10); }
/* 0x20474c10, OnEraseBkgnd: the client area filled with the font's background colour */
int32_t TC fs98_simsui_combo_erase(void *combo, void *dc)
{
    uint8_t brush[8];
    int32_t rect[4];
    void *old;
    if (!PTR(combo, 0x44)) return 1;
    CALL_THIS(MFC_283, void *, (void *, uint32_t))(brush, D32(PTR(combo, 0x44), 0xc));
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(combo, 0x20), rect);
    old = CALL_THIS(MFC_5787, void *, (void *, void *))(dc, brush);
    CALL_STD(Rectangle, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(D32(dc, 4), rect[0], rect[1], rect[2],
                                                                                rect[3]);
    CALL_THIS(MFC_5787, void *, (void *, void *))(dc, old);
    PTR(brush, 0) = IMAGE(VTABLE_GDI_OBJECT);
    mfc_gdi_delete_object(brush);
    return 1;
}
/* 0x20474cc0, MeasureItem: 14 pixels */
void TC fs98_simsui_combo_measure(void *combo, uint8_t *measure)
{
    (void)combo;
    D32(measure, 0x10) = 0xe;
}
