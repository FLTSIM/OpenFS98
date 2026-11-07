/* CSimsCheckButton (0xc8 bytes, vtable 0x15a48): an owner-drawn CButton painted from five
 * frames built at Init (unchecked, checked, pressed unchecked, pressed checked, disabled)
 * over the dialog's background: the strip's box, and the text in the font beside it.
 *
 * The CButton (CWnd) to +0x40; its CDIBSection +0x40 (width +0x48, height +0x4c, CBitmap
 * +0x9c); the CDIBPal +0xb0; a frame's height +0xb4 and width +0xb8; checked +0xbc; the
 * palette state last given to the bits +0xc0; pressed +0xc4. */
#include "simsui_internal.h"

#define VTABLE_CBUTTON 0x158a0
#define VTABLE_CHECK 0x15a48
#define VTABLE_GDI_OBJECT 0x15030
#define VTABLE_OBJECT 0x15018
#define CHECK_SIZE 0xc8

/* 0x20473af0 */
void *TC fs98_simsui_check_construct(void *button)
{
    mfc_wnd_construct(button);
    PTR(button, 0) = IMAGE(VTABLE_CBUTTON);
    fs98_simsui_dib_construct(AT(button, 0x40));
    PTR(button, 0) = IMAGE(VTABLE_CHECK);
    D32(button, 0xbc) = 1;
    D32(button, 0xb0) = 0;
    D32(button, 0xc4) = 0;
    return button;
}
/* 0x20473bc0 */
void TC fs98_simsui_check_destroy(void *button)
{
    PTR(button, 0) = IMAGE(VTABLE_CHECK);
    fs98_simsui_dib_destroy(AT(button, 0x40));
    CALL_THIS(MFC_609, void, (void *))(button);
}
/* 0x20473b60 */
void *TC fs98_simsui_check_delete(void *button, uint32_t flags)
{
    return fs98_simsui_delete_object(button, flags, CHECK_SIZE, fs98_simsui_check_destroy, fs98_simsui_ui_delete);
}
/* 0x20473c10 */
const void *TC fs98_simsui_check_message_map(const void *button) { (void)button; return IMAGE(0x15968); }
/* 0x20473c20, Init(font, strip id, background, CDIBPal *, HINSTANCE): the window's text is
 * "name#l,t,r,b" (else where it is, remapped). A section the width of that rectangle and
 * five strip frames tall is made, each frame the background with the strip's box over it
 * and the text drawn beside (the checked ones with a frame round it, the disabled one
 * greyed); the window then sized to a frame and shown. */
int32_t TC fs98_simsui_check_init(void *button, void *font, int32_t id, void *background, void *pal, uint32_t instance)
{
    uint8_t strip[0x70], dc[0x10], memory[0x10], pen[8];
    uint32_t text, name, dc_kept, memory_kept;
    int32_t rect[4], size[2], left, top, frame;
    uint8_t *info;
    void *old_pen;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_3874, void, (void *, void *))(button, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_rect(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, rect);
    else {
        fs98_simsui_remap_rect(rect, button);
        CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &text);
    }
    PTR(button, 0xb0) = pal;
    left = rect[0];
    top = rect[1];
    fs98_simsui_dib_construct(strip);
    if (!fs98_simsui_dib_load(strip, id, instance, pal, 0)) goto failed;
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0) = 0x28;
    I32(info, 4) = rect[2] - rect[0];
    I32(info, 8) = -I32(strip, 0xc);
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    D32(info, 0x10) = 0;
    for (frame = 0; frame < 0x100; ++frame) D32(info, 0x28 + frame * 4) = D32(PTR(button, 0xb0), 8 + frame * 4);
    if (!fs98_simsui_dib_create(AT(button, 0x40), info, PTR(button, 0xb0), 0)) goto failed;    /* info is kept */
    mfc_operator_delete(info);
    I32(button, 0xb4) = I32(button, 0x4c) / 5;
    I32(button, 0xb8) = I32(button, 0x48);
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_THIS(MFC_323, void *, (void *))(memory);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    if (!CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0)))
        fs98_simsui_exit_cleanly();
    CALL_STD(SetLastError, void, (uint32_t))(0);
    if (!CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0)))
        fs98_simsui_exit_cleanly();
    dc_kept = CALL_THIS(MFC_5736, uint32_t, (void *))(dc);
    memory_kept = CALL_THIS(MFC_5736, uint32_t, (void *))(dc);
    CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(button, 0xb0), 0);
    CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(memory, PTR(button, 0xb0), 0);
    CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
    CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(memory, 4));
    {
        void *bitmap = AT(background, 0x5c);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), bitmap ? D32(bitmap, 4) : 0);
        bitmap = AT(button, 0x9c);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
    }
    for (frame = 0; frame < 5; ++frame)
        CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(memory, 4), 0, frame * I32(button, 0xb4), I32(button, 0xb8), I32(button, 0xb4), D32(dc, 4), left, top,
            0xcc0020);
    for (frame = 0; frame < 5; ++frame)
        fs98_simsui_dib_sprite(strip, AT(button, 0x40), 0, frame * I32(button, 0xb4), I32(strip, 8), I32(button, 0xb4), 0,
                               frame * I32(button, 0xb4), -1);
    CALL_THIS(MFC_472, void *, (void *, int32_t, int32_t, uint32_t))(pen, 0, 1, D32(font, 8));
    old_pen = CALL_THIS(MFC_5788, void *, (void *, void *))(memory, pen);
    CALL_THIS(MFC_5794, void *, (void *, int32_t))(memory, 5);
    CALL_THIS(MFC_5789, void *, (void *, void *))(memory, PTR(font, 4));
    CALL_STD(GetTextExtentPointA, int32_t, (uint32_t, const char *, int32_t, int32_t *))(
        D32(memory, 4), (const char *)(uintptr_t)name, I32((uintptr_t)name, -8), size);
    for (frame = 0; frame < 5; ++frame) {
        int32_t height = I32(button, 0xb4), at = height + 10, frame_top = frame * height, bottom = (frame + 1) * height;
        uint32_t copy;
        struct fs98_simsui_rect area;
        area.left = at; area.top = frame_top; area.right = I32(button, 0xb8); area.bottom = bottom;
        CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &name);
        fs98_simsui_font_draw_text(font, memory, copy, area, frame == 4 ? 2 : 0);
        if (frame == 2 || frame == 3) {
            int32_t right = size[0] + at + 5;
            if (size[0] + 5 < 100) right += size[0] / -10;
            CALL_STD(Rectangle, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(D32(memory, 4), at - 5, frame_top,
                                                                                        right, bottom);
        }
    }
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        button, left, top, I32(button, 0xb8), I32(button, 0xb4), 1);
    CALL_THIS(MFC_6215, int32_t, (void *, int32_t))(button, 5);
    CALL_THIS(MFC_5788, void *, (void *, void *))(memory, old_pen);
    CALL_THIS(MFC_5678, uint32_t, (void *, uint32_t))(dc, dc_kept);
    CALL_THIS(MFC_5678, uint32_t, (void *, uint32_t))(memory, memory_kept);
    PTR(pen, 0) = IMAGE(VTABLE_GDI_OBJECT);
    mfc_gdi_delete_object(pen);
    PTR(pen, 0) = IMAGE(VTABLE_OBJECT);
    CALL_THIS(MFC_640, void, (void *))(memory);
    CALL_THIS(MFC_640, void, (void *))(dc);
    fs98_simsui_dib_destroy(strip);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return 1;
failed:
    fs98_simsui_dib_destroy(strip);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return 0;
}
/* 0x204741c0, DrawItem(DRAWITEMSTRUCT *): the frame for the state (pressed or focused: 2
 * or 3, disabled 4, else 0 or 1, by checked); nothing without a palette */
void TC fs98_simsui_check_draw_item(void *button, const uint8_t *item)
{
    uint8_t dc[0x10], memory[0x10];
    uint32_t state, active_process, foreground_process;
    int32_t row;
    void *old_pal, *old, *pal;
    if (!PTR(button, 0xb0)) return;
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_THIS(MFC_323, void *, (void *))(memory);
    state = D32(item, 0x10);
    row = D32(button, 0xbc) ? I32(button, 0xb4) : 0;
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, D32(item, 0x18));
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory,
            CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(D32(dc, 4)))) {
        void *bitmap = AT(button, 0x9c);
        old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(button, 0xb0), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        pal = PTR(button, 0xb0);
        if (D32(button, 0xc0) != D32(pal, 0x808)) {
            uint32_t active, foreground;
            void *table;
            active_process = 0;
            foreground_process = 0;
            active = CALL_STD(GetActiveWindow, uint32_t, (void))();
            foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
            if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
            if (foreground)
                CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
            if (!D32(pal, 0x808) && (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(D32(memory, 8), 0x26) & 0x100))
                table = AT(pal, 0x408);
            else table = AT(pal, 8);
            CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(memory, 4), 0, 0x100, table);
            D32(button, 0xc0) = D32(PTR(button, 0xb0), 0x808);
        }
        if (state & 1) row = D32(button, 0xbc) ? I32(button, 0xb4) * 3 : I32(button, 0xb4) * 2;
        else if (state & 0x10) row = D32(button, 0xbc) ? I32(button, 0xb4) * 3 : I32(button, 0xb4) * 2;
        else if (state & 4) row = I32(button, 0xb4) * 4;
        CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(dc, 4), 0, 0, I32(button, 0xb8), I32(button, 0xb4), D32(memory, 4), 0, row, 0xcc0020);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
        if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 0);
    }
    CALL_THIS(MFC_2450, uint32_t, (void *))(dc);
    CALL_THIS(MFC_640, void, (void *))(memory);
    CALL_THIS(MFC_640, void, (void *))(dc);
}
/* 0x20474440, SetChecked: the old state; repainted */
int32_t TC fs98_simsui_check_set(void *button, int32_t checked)
{
    int32_t old = I32(button, 0xbc);
    I32(button, 0xbc) = checked;
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(button, 0x20), 0, 1);
    return old;
}
/* 0x20474470, OnEraseBkgnd (CSimsText's and others'): erased */
int32_t TC fs98_simsui_erase_nothing(void *window, void *dc) { (void)window; (void)dc; return 1; }
/* 0x20474480, OnLButtonUp: released inside while pressed, 0x5f6 posted to itself */
void TC fs98_simsui_check_button_up(void *button, uint32_t keys, int32_t x, int32_t y)
{
    int32_t rect[4];
    (void)keys;
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(button, 0x20), rect);
    if (D32(button, 0xc4)) {
        if (CALL_STD(PtInRect, int32_t, (const int32_t *, int32_t, int32_t))(rect, x, y) && D32(button, 0xc4)) {
            CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), 0x5f6, 0, 0);
            CALL_THIS(MFC_2379, uint32_t, (void *))(button);
            return;
        }
        D32(button, 0xc4) = 0;
    }
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x20474500, OnLButtonDown: pressed when inside */
void TC fs98_simsui_check_button_down(void *button, uint32_t keys, int32_t x, int32_t y)
{
    int32_t rect[4];
    (void)keys;
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(button, 0x20), rect);
    D32(button, 0xc4) = CALL_STD(PtInRect, int32_t, (const int32_t *, int32_t, int32_t))(rect, x, y) ? 1 : 0;
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x20474560, OnLButtonDblClk: a click, down and up */
void TC fs98_simsui_check_double_click(void *button, uint32_t keys, int32_t x, int32_t y)
{
    uint32_t where = ((uint32_t)y & 0xffff) << 16 | ((uint32_t)x & 0xffff);
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), 0x201, keys, where);
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), 0x202, keys, where);
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x204745b0, OnKeyUp: space toggles */
void TC fs98_simsui_check_key_up(void *button, uint32_t key, uint32_t repeat, uint32_t flags)
{
    (void)repeat; (void)flags;
    if (key == 0x20) fs98_simsui_check_set(button, !D32(button, 0xbc));
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x204745e0, OnKillFocus */
void TC fs98_simsui_check_kill_focus(void *button, void *window)
{
    (void)window;
    D32(button, 0xc4) = 0;
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x20474600, OnChBtnChecked (0x5f6): pressed, toggled and its command posted to the parent */
int32_t TC fs98_simsui_check_checked(void *button, uint32_t wparam, uint32_t lparam)
{
    (void)wparam; (void)lparam;
    if (D32(button, 0xc4)) {
        void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(button, 0x20)));
        uint32_t window, id;
        fs98_simsui_check_set(button, !D32(button, 0xbc));
        window = button ? D32(button, 0x20) : 0;
        id = (uint32_t)CALL_THIS(MFC_3089, int32_t, (void *))(button) & 0xffff;
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x111, id, window);
    }
    D32(button, 0xc4) = 0;
    return 0;
}
