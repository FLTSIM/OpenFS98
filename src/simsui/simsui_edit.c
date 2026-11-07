/* CSimsEdit (0x68 bytes, vtable 0x16140): a CEdit in SIMSUI's colours, placed from a
 * CSimsText's spec, with an IME context and a filter on the characters typed.
 *
 * The CEdit (CWnd) to +0x40; the CSimsText +0x40; the background colour +0x44 and text
 * colour +0x48; +0x4c; the CFont +0x50; the background CBrush +0x54 (its handle +0x58);
 * the IME context +0x5c; flags +0x60 (1 a number: 2 signed, 4 with a point; 8 read only;
 * 0x10 limited to +0x64 characters; 0x20, 0x40, 0x80 no IME); the limit +0x64. */
#include "simsui_internal.h"

#define VTABLE_EDIT 0x16140
#define VTABLE_BRUSH 0x16200
#define EDIT_SIZE 0x68
#define MINUS 0x20af0                        /* "-" */
#define DC_FONT 0x30
#define DC_BACK_COLOUR 0x34
#define DC_TEXT_COLOUR 0x38

/* 0x20478f60: a CGdiObject's complete destructor (another copy of 0x20474cf0) */
void TC fs98_simsui_gdi_object_destroy_too(void *object)
{
    fs98_simsui_gdi_object_destroy(object);
}
/* 0x20479560: a CBrush's deleting destructor */
void *TC fs98_simsui_brush_delete(void *brush, uint32_t flags)
{
    fs98_simsui_gdi_object_destroy(brush);
    if (flags & 1) mfc_operator_delete(brush);
    return brush;
}
/* 0x20478fb0 */
void *TC fs98_simsui_edit_construct(void *edit)
{
    mfc_wnd_construct(edit);
    PTR(edit, 0x54) = IMAGE(VTABLE_BRUSH);
    D32(edit, 0x58) = 0;
    D32(edit, 0x40) = 0;
    D32(edit, 0x50) = 0;
    D32(edit, 0x5c) = 0;
    PTR(edit, 0) = IMAGE(VTABLE_EDIT);
    return edit;
}
/* 0x20479030 */
void TC fs98_simsui_edit_destroy(void *edit)
{
    PTR(edit, 0) = IMAGE(VTABLE_EDIT);
    D32(edit, 0x50) = 0;
    if (D32(edit, 0x5c)) {
        CALL_STD(ImmDestroyContext, int32_t, (uint32_t))(D32(edit, 0x5c));
        D32(edit, 0x5c) = 0;
    }
    PTR(edit, 0x54) = IMAGE(0x15030);
    mfc_gdi_delete_object(AT(edit, 0x54));
    PTR(edit, 0x54) = IMAGE(0x15018);
    CALL_THIS(MFC_656, void, (void *))(edit);
}
/* 0x20478fe0 */
void *TC fs98_simsui_edit_delete(void *edit, uint32_t flags)
{
    return fs98_simsui_delete_object(edit, flags, EDIT_SIZE, fs98_simsui_edit_destroy, fs98_simsui_ui_delete);
}
/* 0x204790b0 */
const void *TC fs98_simsui_edit_message_map(const void *edit) { (void)edit; return IMAGE(0x160a8); }
/* whether the edit takes the IME: no flag against it, and not ES_NUMBER */
static int32_t takes_ime(void *edit)
{
    return !(D8(edit, 0x60) & 0xe1) && !(CALL_THIS(MFC_3797, uint32_t, (void *))(edit) & 0x2000);
}
/* 0x204790c0, Init(CFont *, background, text colour, +0x4c, CSimsText *, flags): the font
 * (and the IME's), the brush, placed by the text's spec "name#l,t,r,b,drop" (the edit at the
 * bottom of the rectangle, `drop` tall), else where it is, remapped. RETAIL BUG (kept):
 * SUI-8 (docs/retail-bugs.md): a spec with only four numbers leaves `drop` unset, which in
 * retail's frame is the stack slot of the fourth argument. */
void TC fs98_simsui_edit_init(void *edit, void *font, uint32_t back, uint32_t text_colour, uint32_t fourth, void *text,
                              uint32_t flags)
{
    int32_t rect[4] = {0, 0, 0, 0}, drop = (int32_t)fourth, remap = 0;
    D32(edit, 0x48) = text_colour;
    D32(edit, 0x44) = back;
    D32(edit, 0x4c) = fourth;
    PTR(edit, 0x40) = text;
    PTR(edit, 0x50) = font;
    D32(edit, 0x60) = flags;
    D32(edit, 0x5c) = CALL_STD(ImmCreateContext, uint32_t, (void))();
    if (PTR(edit, 0x50)) {
        uint8_t logfont[0x3c];
        CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(edit, 0x20), 0x30,
                                                                                 D32(PTR(edit, 0x50), 4), 1);
        CALL_STD(GetObjectA, int32_t, (uint32_t, int32_t, void *))(D32(PTR(edit, 0x50), 4), 0x3c, logfont);
        CALL_STD(ImmSetCompositionFontA, int32_t, (uint32_t, void *))(D32(edit, 0x5c), logfont);
    }
    CALL_THIS(MFC_1641, int32_t, (void *, uint32_t))(AT(edit, 0x54), CALL_STD(CreateSolidBrush, uint32_t, (uint32_t))(back));
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
    if (remap) fs98_simsui_remap_rect(rect, edit);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        edit, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
    if (takes_ime(edit)) {
        CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(edit, 0x20), D32(edit, 0x5c));
        CALL_STD(ImmSetOpenStatus, int32_t, (uint32_t, int32_t))(D32(edit, 0x5c), 1);
    } else CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(edit, 0x20), 0);
}
/* 0x20479290, CtlColor: opaque, its colours and font; its brush */
uint32_t TC fs98_simsui_edit_colour(void *edit, void *dc, uint32_t type)
{
    (void)type;
    CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(dc, 2);
    VIRTUAL(dc, DC_TEXT_COLOUR, uint32_t, (void *, uint32_t))(dc, D32(edit, 0x48));
    VIRTUAL(dc, DC_BACK_COLOUR, uint32_t, (void *, uint32_t))(dc, D32(edit, 0x44));
    VIRTUAL(dc, DC_FONT, void *, (void *, void *))(dc, PTR(edit, 0x50));
    return D32(edit, 0x58);
}
/* 0x204792e0, OnChar: read only, a beep; a full limited edit, a beep; a number edit takes
 * digits, a sign at the start (signed), one point (with a point), and editing keys */
void TC fs98_simsui_edit_char(void *edit, uint32_t key, uint32_t repeat, uint32_t flags)
{
    uint32_t text, start = 0, end = 0;
    int32_t point[2], at, accept;
    uint32_t character;
    (void)repeat; (void)flags;
    if (D8(edit, 0x60) & 8) {
        CALL_STD(MessageBeep, int32_t, (uint32_t))(0);
        return;
    }
    if (key == 0xc || key == 8 || key == 0x1a) {
        CALL_THIS(MFC_2379, uint32_t, (void *))(edit);
        return;
    }
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(
        D32(edit, 0x20), 0xb0, (uint32_t)(uintptr_t)&start, (uint32_t)(uintptr_t)&end);
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_3874, void, (void *, void *))(edit, &text);
    CALL_STD(GetCaretPos, int32_t, (int32_t *))(point);
    character = (uint32_t)CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(
        D32(edit, 0x20), 0xd7, 0, ((uint32_t)point[1] & 0xffff) << 16 | ((uint32_t)point[0] & 0xffff));
    if ((D8(edit, 0x60) & 0x10) && start == end && I32((uintptr_t)text, -8) >= I32(edit, 0x64)) goto beep;
    if (!(D8(edit, 0x60) & 1)) goto pass;
    accept = 0;
    if ((D8(edit, 0x60) & 2) && (key == '+' || key == '-')) {
        if (!I32((uintptr_t)text, -8)) accept = 1;
        else if (CALL_THIS(MFC_2784, int32_t, (void *, const char *))(&text, IMAGE(MINUS)) == -1 && !character) accept = 1;
        else if (!start && (int32_t)end >= 1) accept = 1;
    }
    if (D8(edit, 0x60) & 4) {
        at = CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '.');
        if (at > -1 && at >= (int32_t)start && at < (int32_t)end) accept = 1;
        if (key == '.' && CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, (int32_t)key) == -1) accept = 1;
    }
    if (accept) goto pass;
    if (key < '0' || key > '9') goto beep;
    if (CALL_THIS(MFC_2784, int32_t, (void *, const char *))(&text, IMAGE(MINUS)) == -1 || character || end) goto pass;
beep:
    CALL_STD(MessageBeep, int32_t, (uint32_t))(0);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return;
pass:
    CALL_THIS(MFC_800, void, (void *))(&text);
    CALL_THIS(MFC_2379, uint32_t, (void *))(edit);
}
/* 0x204794c0, OnSetFocus: the IME on or off; all selected */
void TC fs98_simsui_edit_set_focus(void *edit, void *previous)
{
    (void)previous;
    CALL_THIS(MFC_2379, uint32_t, (void *))(edit);
    if (takes_ime(edit)) {
        CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(edit, 0x20), D32(edit, 0x5c));
        CALL_STD(ImmSetOpenStatus, int32_t, (uint32_t, int32_t))(D32(edit, 0x5c), 1);
    } else CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(edit, 0x20), 0);
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(edit, 0x20), 0xb1, 0, 0xffffffffu);
}
/* 0x20479520, OnKillFocus: the IME closed */
void TC fs98_simsui_edit_kill_focus(void *edit, void *next)
{
    (void)next;
    CALL_THIS(MFC_2379, uint32_t, (void *))(edit);
    if (takes_ime(edit)) {
        CALL_STD(ImmSetOpenStatus, int32_t, (uint32_t, int32_t))(D32(edit, 0x5c), 0);
        CALL_STD(ImmAssociateContext, uint32_t, (uint32_t, uint32_t))(D32(edit, 0x20), 0);
    }
}
