/* CSimsListBox (0x48 bytes, vtable 0x16438) and CSimsCheckListBox (0x50 bytes, vtable
 * 0x16560): owner-drawn list boxes whose items are drawn in a CSimsFont they recolour,
 * placed from a CSimsText's spec. The two are the same code (but for the frame of a
 * selected item: the check list box frames it); the check list box's fields sit 8 bytes
 * further on, after CCheckListBox's own: the CSimsText +0x40 (+0x48) and the
 * CSimsFont +0x44 (+0x4c). */
#include "simsui_internal.h"

#define VTABLE_LIST 0x16438
#define VTABLE_CHECK_LIST 0x16560
#define LIST 0x40
#define CHECK_LIST 0x48

#define TEXT_OF(box, at) PTR(box, at)
#define FONT_OF(box, at) PTR(box, (at) + 4)

/* 0x2047a4e0 */
void *TC fs98_simsui_list_construct(void *box)
{
    mfc_wnd_construct(box);
    PTR(box, 0) = IMAGE(VTABLE_LIST);
    D32(box, 0x40) = 0;
    D32(box, 0x44) = 0;
    return box;
}
/* 0x2047a550 */
void TC fs98_simsui_list_destroy(void *box)
{
    PTR(box, 0) = IMAGE(VTABLE_LIST);
    D32(box, 0x44) = 0;
    CALL_THIS(MFC_692, void, (void *))(box);
}
/* 0x2047a500 */
void *TC fs98_simsui_list_delete(void *box, uint32_t flags)
{
    return fs98_simsui_delete_object(box, flags, 0x48, fs98_simsui_list_destroy, fs98_simsui_ui_delete);
}
/* 0x2047a570, CSimsButton::_GetBaseMessageMap: CButton's, MFC's */
const void *SC fs98_simsui_button_base_message_map(void)
{
    return fs98_simsui_imports[SIMSUI_IMPORT_MFC_4275];
}
/* 0x2047a580 */
const void *TC fs98_simsui_list_message_map(const void *box) { (void)box; return IMAGE(0x163e8); }
/* 0x2047a590, OnHelpMsg (CSimsButton's and others'): WM_HELP passed to the parent */
uint32_t TC fs98_simsui_help_to_parent(void *window, uint32_t wparam, uint32_t lparam)
{
    void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(window, 0x20)));
    return (uint32_t)CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x53,
                                                                                             wparam, lparam);
}
/* Init(font, CSimsText *): placed by the text's spec "name#l,t,r,b,drop" (the list at the
 * bottom of the rectangle, `drop` tall), else where it is, remapped; the font recoloured
 * and the IME kept out. RETAIL BUG (kept): SUI-8: a four-number spec leaves `drop` unset. */
static void list_init(void *box, uint32_t at, void *font, void *text)
{
    int32_t rect[4] = {0, 0, 0, 0}, drop = 0, remap = 0;
    TEXT_OF(box, at) = text;
    FONT_OF(box, at) = font;
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
    if (remap) fs98_simsui_remap_rect(rect, box);
    D32(FONT_OF(box, at), 8) = 0;
    D32(FONT_OF(box, at), 0xc) = 0xffffff;
    D32(FONT_OF(box, at), 0x14) = 0xffffff;
    D32(FONT_OF(box, at), 0x18) = 0xffffff;
    D32(FONT_OF(box, at), 0x1c) = 0x1000004;
    D32(FONT_OF(box, at), 0x24) = 0x10000f8;
    D32(FONT_OF(box, at), 0x28) = 0x8b8b8b;
    D32(FONT_OF(box, at), 0x2c) = 0xffffff;
    D32(FONT_OF(box, at), 0x34) = 0x8b8b8b;
    D32(FONT_OF(box, at), 0x4c) = 1;
    D32(FONT_OF(box, at), 0x54) = 0;
    D8(FONT_OF(box, at), 0x58) |= 0x20;
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        box, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
    CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(box, 0x20), 0);
}
/* DrawItem: the item, a space before it, in the font: greyed (state 2, unframed) when the
 * box is disabled, framed when selected and focused, in colour 7 when selected, plain */
static void list_draw_item(void *box, uint32_t at, const uint8_t *item)
{
    uint32_t text, copy;
    struct fs98_simsui_rect area;
    void *dc;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    dc = CALL_STD(MFC_2859, void *, (uint32_t))(D32(item, 0x18));
    if (FONT_OF(box, at)) {
        uint32_t saved_text = D32(FONT_OF(box, at), 8), saved_shadow = D32(FONT_OF(box, at), 0x18);
        if (I32(item, 8) != -1) {
            uint32_t spaced;
            CALL_THIS(MFC_3803, void, (void *, int32_t, void *))(box, I32(item, 8), &text);
            CALL_THIS(MFC_858, void *, (void *, const void *))(
                &text, CALL_STD(MFC_925, void *, (void *, uint32_t, const void *))(&spaced, ' ', &text));
            CALL_THIS(MFC_800, void, (void *))(&spaced);
        }
        if (!CALL_THIS(MFC_4123, int32_t, (void *))(box)) {
            D32(FONT_OF(box, at), 0x4c) = 0;
            fs98_simsui_rect_copy(&area, item + 0x1c);
            CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
            fs98_simsui_font_draw_text(FONT_OF(box, at), dc, copy, area, 2);
        } else {
            uint32_t state = D32(item, 0x10);
            if ((state & 1) && (state & 0x10)) {
                D32(FONT_OF(box, at), 0x4c) = 1;
                fs98_simsui_rect_copy(&area, item + 0x1c);
                CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
                fs98_simsui_font_draw_text(FONT_OF(box, at), dc, copy, area, 1);
                D32(FONT_OF(box, at), 0x4c) = 0;
            } else if (state & 1) {
                uint32_t saved_back;
                if (at == CHECK_LIST) D32(FONT_OF(box, at), 0x4c) = 1;     /* the check list box frames it too */
                saved_back = D32(FONT_OF(box, at), 0xc);
                D32(FONT_OF(box, at), 0xc) = 0x1000007;
                fs98_simsui_rect_copy(&area, item + 0x1c);
                CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
                fs98_simsui_font_draw_text(FONT_OF(box, at), dc, copy, area, 0);
                D32(FONT_OF(box, at), 0xc) = saved_back;
                if (at == CHECK_LIST) D32(FONT_OF(box, at), 0x4c) = 0;
            } else {
                fs98_simsui_rect_copy(&area, item + 0x1c);
                CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
                fs98_simsui_font_draw_text(FONT_OF(box, at), dc, copy, area, 0);
            }
        }
        D32(FONT_OF(box, at), 8) = saved_text;
        D32(FONT_OF(box, at), 0x18) = saved_shadow;
    }
    CALL_THIS(MFC_800, void, (void *))(&text);
}
/* CompareItem: -1 with fewer than two items, else the texts by _mbscmp */
static int32_t list_compare(void *box, const uint8_t *compare)
{
    uint32_t first, second;
    int32_t result;
    CALL_THIS(MFC_540, void *, (void *))(&first);
    CALL_THIS(MFC_540, void *, (void *))(&second);
    if (CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(box, 0x20), 0x18b, 0, 0) < 2)
        result = -1;
    else {
        CALL_THIS(MFC_3803, void, (void *, int32_t, void *))(box, I32(compare, 0xc), &first);
        CALL_THIS(MFC_3803, void, (void *, int32_t, void *))(box, I32(compare, 0x14), &second);
        if (CALL_C(_mbscmp, int, (const char *, const char *))((const char *)(uintptr_t)first,
                                                               (const char *)(uintptr_t)second) < 0) result = -1;
        else result = CALL_C(_mbscmp, int, (const char *, const char *))((const char *)(uintptr_t)first,
                                                                         (const char *)(uintptr_t)second) > 0;
    }
    CALL_THIS(MFC_800, void, (void *))(&second);
    CALL_THIS(MFC_800, void, (void *))(&first);
    return result;
}
/* 0x2047a5c0, 0x2047a740, 0x2047a910 */
void TC fs98_simsui_list_init(void *box, void *font, void *text) { list_init(box, LIST, font, text); }
void TC fs98_simsui_list_draw_item(void *box, const uint8_t *item) { list_draw_item(box, LIST, item); }
int32_t TC fs98_simsui_list_compare(void *box, const uint8_t *compare) { return list_compare(box, compare); }

/* 0x2047aa60, OnEraseBkgnd: CWnd::Default's */
int32_t TC fs98_simsui_check_list_erase(void *box, void *dc)
{
    (void)dc;
    return (int32_t)CALL_THIS(MFC_2379, uint32_t, (void *))(box);
}
/* 0x2047aa70 */
void *TC fs98_simsui_check_list_construct(void *box)
{
    mfc_wnd_construct(box);
    PTR(box, 0) = IMAGE(VTABLE_CHECK_LIST);
    D32(box, 0x40) = 0;
    D32(box, 0x44) = 0;
    D32(box, 0x48) = 0;
    D32(box, 0x4c) = 0;
    return box;
}
/* 0x2047aae0 */
void TC fs98_simsui_check_list_destroy(void *box)
{
    PTR(box, 0) = IMAGE(VTABLE_CHECK_LIST);
    D32(box, 0x4c) = 0;
    CALL_THIS(MFC_692, void, (void *))(box);
}
/* 0x2047aa90 */
void *TC fs98_simsui_check_list_delete(void *box, uint32_t flags)
{
    return fs98_simsui_delete_object(box, flags, 0x50, fs98_simsui_check_list_destroy, fs98_simsui_ui_delete);
}
/* 0x2047ab00, _GetBaseMessageMap: CCheckListBox's, MFC's */
const void *SC fs98_simsui_check_list_base_message_map(void)
{
    return fs98_simsui_imports[SIMSUI_IMPORT_MFC_4225];
}
/* 0x2047ab10 */
const void *TC fs98_simsui_check_list_message_map(const void *box) { (void)box; return IMAGE(0x16510); }
/* 0x2047ab20, 0x2047aca0, 0x2047ae80 */
void TC fs98_simsui_check_list_init(void *box, void *font, void *text) { list_init(box, CHECK_LIST, font, text); }
void TC fs98_simsui_check_list_draw_item(void *box, const uint8_t *item) { list_draw_item(box, CHECK_LIST, item); }
int32_t TC fs98_simsui_check_list_compare(void *box, const uint8_t *compare) { return list_compare(box, compare); }
