/* CSimsRadioButton (0xf4 bytes, vtable 0x16a68): an owner-drawn CButton like
 * CSimsCheckButton, five frames built at Init, which keeps its dialog group: each button's
 * user data (GWL_USERDATA) is the group's checked button.
 *
 * The CButton (CWnd) to +0x40; its CDIBSection +0x40 (width +0x48, height +0x4c, CBitmap
 * +0x9c); the CDIBPal +0xb0; a frame's height +0xb4 and width +0xb8; the palette state
 * given to the bits +0xbc; pressed +0xc0; the group's windows +0xc4 (up to ten) and their
 * count +0xf0; the checked button before the last SetChecked +0xec. */
#include "simsui_internal.h"

#define VTABLE_CBUTTON 0x158a0
#define VTABLE_RADIO 0x16a68
#define RADIO_SIZE 0xf4
#define GWL_USERDATA_ (-21)
#define GWL_STYLE_ (-16)

/* 0x2047bbc0 */
void *TC fs98_simsui_radio_construct(void *button)
{
    mfc_wnd_construct(button);
    PTR(button, 0) = IMAGE(VTABLE_CBUTTON);
    fs98_simsui_dib_construct(AT(button, 0x40));
    D32(button, 0xf0) = 0;
    D32(button, 0xec) = 0;
    D32(button, 0xb0) = 0;
    PTR(button, 0) = IMAGE(VTABLE_RADIO);
    return button;
}
/* 0x2047bc90 */
void TC fs98_simsui_radio_destroy(void *button)
{
    PTR(button, 0) = IMAGE(VTABLE_RADIO);
    fs98_simsui_dib_destroy(AT(button, 0x40));
    CALL_THIS(MFC_609, void, (void *))(button);
}
/* 0x2047bc30 */
void *TC fs98_simsui_radio_delete(void *button, uint32_t flags)
{
    return fs98_simsui_delete_object(button, flags, RADIO_SIZE, fs98_simsui_radio_destroy, fs98_simsui_ui_delete);
}
/* 0x2047bce0 */
const void *TC fs98_simsui_radio_message_map(const void *button) { (void)button; return IMAGE(0x16970); }
#define USER_DATA(window) CALL_STD(GetWindowLongA, int32_t, (uint32_t, int32_t))(window, GWL_USERDATA_)
/* 0x2047bcf0, Init(font, strip id, background, CDIBPal *, HINSTANCE): as CSimsCheckButton's
 * (the checked frames framed without the check box's width correction), and the dialog
 * group gathered: from its first button (WS_GROUP), each told the first is checked */
int32_t TC fs98_simsui_radio_init(void *button, void *font, int32_t id, void *background, void *pal, uint32_t instance)
{
    uint8_t strip[0x70], dc[0x10], memory[0x10], pen[8], *info;
    uint32_t text, name, dc_kept, memory_kept, first, window;
    int32_t rect[4], size[2], left, top, frame;
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
        if (frame == 2 || frame == 3)
            CALL_STD(Rectangle, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t))(
                D32(memory, 4), at - 5, frame_top, size[0] + at, bottom);
    }
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        button, left, top, I32(button, 0xb8), I32(button, 0xb4), 1);
    CALL_THIS(MFC_6215, int32_t, (void *, int32_t))(button, 5);
    CALL_THIS(MFC_5788, void *, (void *, void *))(memory, old_pen);
    CALL_THIS(MFC_5678, uint32_t, (void *, uint32_t))(dc, dc_kept);
    CALL_THIS(MFC_5678, uint32_t, (void *, uint32_t))(memory, memory_kept);
    mfc_operator_delete(info);
#define PARENT_WINDOW() D32(CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(button, 0x20))), 0x20)
#define NEXT_IN_GROUP(window, previous) CALL_STD(GetNextDlgGroupItem, uint32_t, (uint32_t, uint32_t, int32_t))( \
        PARENT_WINDOW(), window, previous)
    window = D32(button, 0x20);
    while (!(CALL_STD(GetWindowLongA, int32_t, (uint32_t, int32_t))(window, GWL_STYLE_) & 0x20000))
        window = NEXT_IN_GROUP(window, 1);
    first = window;
    CALL_STD(SetWindowLongA, int32_t, (uint32_t, int32_t, int32_t))(window, GWL_USERDATA_, (int32_t)window);
    D32(button, 0xf0) = 0;
    D32(button, 0xc4) = window;
    for (;;) {
        window = NEXT_IN_GROUP(window, 0);
        if (window == D32(button, 0x20)) {
            int32_t at, found = 0;
            for (at = 0; at < I32(button, 0xf0); ++at)
                if (D32(button, 0xc4 + at * 4) == D32(button, 0x20)) { found = 1; break; }
            if (found) break;
        }
        ++D32(button, 0xf0);
        D32(button, 0xc4 + D32(button, 0xf0) * 4) = window;
        CALL_STD(SetWindowLongA, int32_t, (uint32_t, int32_t, int32_t))(window, GWL_USERDATA_, (int32_t)first);
        if (I32(button, 0xf0) >= 10) break;
    }
#undef NEXT_IN_GROUP
#undef PARENT_WINDOW
    ++D32(button, 0xf0);
    PTR(pen, 0) = IMAGE(0x15030);
    mfc_gdi_delete_object(pen);
    PTR(pen, 0) = IMAGE(0x15018);
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
/* 0x2047c360, DrawItem: the frame for the state (pressed or focused: 2, or 3 checked;
 * disabled 4; else 0, or 1 checked); nothing without a palette */
void TC fs98_simsui_radio_draw_item(void *button, const uint8_t *item)
{
    uint8_t dc[0x10], memory[0x10];
    uint32_t state;
    int32_t row = 0;
    void *old_pal, *old, *pal;
    if (!PTR(button, 0xb0)) return;
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_THIS(MFC_323, void *, (void *))(memory);
    state = D32(item, 0x10);
    if (D32(button, 0x20) == (uint32_t)USER_DATA(D32(button, 0x20))) row = I32(button, 0xb4);
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, D32(item, 0x18));
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory,
            CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(D32(dc, 4)))) {
        void *bitmap = AT(button, 0x9c);
        uint32_t checked;
        old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(button, 0xb0), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        pal = PTR(button, 0xb0);
        if (D32(button, 0xbc) != D32(pal, 0x808)) {
            uint32_t active_process = 0, foreground_process = 0, active, foreground;
            void *table;
            active = CALL_STD(GetActiveWindow, uint32_t, (void))();
            foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
            if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
            if (foreground)
                CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
            if (!D32(pal, 0x808) && (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(D32(memory, 8), 0x26) & 0x100))
                table = AT(pal, 0x408);
            else table = AT(pal, 8);
            CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(memory, 4), 0, 0x100, table);
            D32(button, 0xbc) = D32(PTR(button, 0xb0), 0x808);
        }
        checked = (uint32_t)USER_DATA(D32(button, 0x20));
        if ((state & 1) || (state & 0x10))
            row = D32(button, 0x20) == checked ? I32(button, 0xb4) * 3 : I32(button, 0xb4) * 2;
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
/* the group told `checked` is checked: its user data, and the tab stop on it alone */
static void check_in_group(void *button, uint32_t checked, int32_t repaint_now)
{
    int32_t at;
    for (at = 0; at < I32(button, 0xf0); ++at) {
        uint32_t window = D32(button, 0xc4 + at * 4);
        int32_t style;
        CALL_STD(SetWindowLongA, int32_t, (uint32_t, int32_t, int32_t))(window, GWL_USERDATA_, (int32_t)checked);
        style = CALL_STD(GetWindowLongA, int32_t, (uint32_t, int32_t))(window, GWL_STYLE_);
        window = D32(button, 0xc4 + at * 4);
        if (window == checked) style |= 0x10000;
        else style &= ~0x10000;
        CALL_STD(SetWindowLongA, int32_t, (uint32_t, int32_t, int32_t))(window, GWL_STYLE_, style);
        if (repaint_now)
            CALL_STD(RedrawWindow, int32_t, (uint32_t, const void *, uint32_t, uint32_t))(D32(button, 0xc4 + at * 4), 0, 0, 1);
        else
            CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(button, 0xc4 + at * 4), 0, 0);
    }
}
/* 0x2047c5d0, SetChecked: the checked button kept for an undo; checked (1), the group told */
int32_t TC fs98_simsui_radio_set(void *button, int32_t checked)
{
    D32(button, 0xec) = (uint32_t)USER_DATA(D32(button, 0x20));
    if (checked == 1) check_in_group(button, D32(button, 0x20), 1);
    return checked;
}
/* 0x2047c670, UndoSetChecked: the group's checked button put back */
void TC fs98_simsui_radio_undo(void *button)
{
    if (D32(button, 0xec)) check_in_group(button, D32(button, 0xec), 0);
}
/* 0x2047c6f0, OnLButtonDown: the focus taken, pressed */
void TC fs98_simsui_radio_button_down(void *button, uint32_t keys, int32_t x, int32_t y)
{
    (void)keys; (void)x; (void)y;
    CALL_THIS(MFC_5981, void *, (void *))(button);
    D32(button, 0xc0) = 1;
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x2047c710, OnLButtonUp: released inside while pressed, checked and 0x5f8 posted */
void TC fs98_simsui_radio_button_up(void *button, uint32_t keys, int32_t x, int32_t y)
{
    int32_t rect[4];
    (void)keys;
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(button, 0x20), rect);
    if (CALL_STD(PtInRect, int32_t, (const int32_t *, int32_t, int32_t))(rect, x, y) && D32(button, 0xc0)) {
        fs98_simsui_radio_set(button, 1);
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), 0x5f8, 0, 0);
    }
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x2047c780, OnRdBtnChecked (0x5f8): not pressed any more, the check undone */
uint32_t TC fs98_simsui_radio_checked(void *button, uint32_t wparam, uint32_t lparam)
{
    (void)wparam; (void)lparam;
    if (!D32(button, 0xc0)) fs98_simsui_radio_undo(button);
    D32(button, 0xc0) = 0;
    return 0;
}
/* 0x2047c7b0, OnKeyUp: space checks; an arrow without Ctrl clicks */
void TC fs98_simsui_radio_key_up(void *button, uint32_t key, uint32_t repeat, uint32_t flags)
{
    (void)repeat; (void)flags;
    if (key == 0x20) fs98_simsui_radio_set(button, 1);
    else if (key > 0x24 && key <= 0x28 && CALL_STD(GetKeyState, int16_t, (int32_t))(0x11) >= 0)
        CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), 0xf5, 0, 0);
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x2047c810, OnKillFocus */
void TC fs98_simsui_radio_kill_focus(void *button, void *next)
{
    (void)next;
    D32(button, 0xc0) = 0;
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
