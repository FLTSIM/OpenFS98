/* CSimsHotKey (0x64 bytes, vtable 0x17660) and CSimsHotJoy (0x60 bytes, vtable 0x17788):
 * CWnds that show a key or a joystick control in a CSimsFont, for Options' key and joystick
 * assignment. Both keep the font +0x4c and an unused CBrush +0x50 (its handle +0x54).
 *
 * CSimsHotKey: the key +0x58, its modifiers +0x5c (1 shift, 2 ctrl, 4 alt; 8 once a key has
 * been pressed), flags +0x60. CSimsHotJoy: the joystick control +0x58, flags +0x5c. Each tells
 * its parent of a change with WM_COMMAND, notification 0x300. */
#include "simsui_internal.h"

#define VTABLE_HOT_KEY 0x17660
#define VTABLE_HOT_JOY 0x17788
#define VTABLE_BRUSH 0x16200
#define HOT_KEY_SIZE 0x64
#define HOT_JOY_SIZE 0x60
#define NO_TEXT 0x213d4                      /* "" */
#define ONE_SPACE 0x20b80                    /* " " */
#define HOST_TABLE G_PTR(0x2002c)

/* the shared construction: a CWnd, the brush with no handle, no font */
static void *hot_construct(void *window, uint32_t vtable)
{
    mfc_wnd_construct(window);
    PTR(window, 0x50) = IMAGE(VTABLE_BRUSH);
    D32(window, 0x54) = 0;
    D32(window, 0x4c) = 0;
    PTR(window, 0) = IMAGE(vtable);
    return window;
}
static void hot_destroy(void *window, uint32_t vtable)
{
    PTR(window, 0) = IMAGE(vtable);
    D32(window, 0x4c) = 0;
    PTR(window, 0x50) = IMAGE(0x15030);
    mfc_gdi_delete_object(AT(window, 0x50));
    PTR(window, 0x50) = IMAGE(0x15018);
    CALL_THIS(MFC_818, void, (void *))(window);
}
/* the shared Init(CSimsFont *, flags) tail: the font's CFont given to the window, which is
 * placed where it is, remapped */
static void hot_place(void *window, void *font)
{
    int32_t rect[4];
    if (font && D32(font, 4)) {
        void *cfont = PTR(font, 4);
        CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, int32_t))(D32(window, 0x20), 0x30,
                                                                                cfont ? D32(cfont, 4) : 0, 1);
    }
    fs98_simsui_remap_rect(rect, window);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        window, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
}
/* the parent told: WM_COMMAND (its id, notification 0x300) */
static void hot_notify(void *window)
{
    void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(window, 0x20)));
    uint32_t handle = D32(window, 0x20);
    uint32_t id = CALL_THIS(MFC_3089, uint32_t, (void *))(window);
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x111,
                                                                            (id & 0xffff) | 0x3000000, handle);
}
/* the shared OnPaint: `text` in the font (selected, its pointer read before it is checked)
 * over the client rectangle */
static void hot_paint_text(void *window, void *dc, uint32_t text, const int32_t *rect)
{
    struct fs98_simsui_rect area;
    area.left = rect[0]; area.top = rect[1]; area.right = rect[2]; area.bottom = rect[3];
    fs98_simsui_font_draw_text(PTR(window, 0x4c), dc, text, area, 0);
}

/* 0x20482590 */
void *TC fs98_simsui_hot_key_construct(void *key) { return hot_construct(key, VTABLE_HOT_KEY); }
/* 0x20482610 */
void TC fs98_simsui_hot_key_destroy(void *key) { hot_destroy(key, VTABLE_HOT_KEY); }
/* 0x204825c0 */
void *TC fs98_simsui_hot_key_delete(void *key, uint32_t flags)
{
    return fs98_simsui_delete_object(key, flags, HOT_KEY_SIZE, fs98_simsui_hot_key_destroy, fs98_simsui_ui_delete);
}
/* 0x20482680 */
const void *TC fs98_simsui_hot_key_message_map(const void *key) { (void)key; return IMAGE(0x17580); }
/* 0x20482690, OnHelpMsg: 0, so no help */
int32_t TC fs98_simsui_hot_key_on_help(void *key, uint32_t wparam, int32_t lparam)
{
    (void)key; (void)wparam; (void)lparam;
    return 0;
}
/* 0x204826a0, Init(CSimsFont *, flags): no key yet */
void TC fs98_simsui_hot_key_init(void *key, void *font, uint32_t flags)
{
    PTR(key, 0x4c) = font;
    D32(key, 0x58) = 0;
    D32(key, 0x5c) = 0;
    D32(key, 0x60) = flags;
    hot_place(key, font);
}
/* 0x20482720, SetHotKey(code): the key the low byte, the modifiers the rest; repainted */
void TC fs98_simsui_hot_key_set(void *key, int32_t code)
{
    I32(key, 0x5c) = code >> 8;
    D32(key, 0x58) = (uint32_t)code & 0xff;
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(key, 0x20), 0, 1);
}
/* 0x20482750, GetHotKey */
int32_t TC fs98_simsui_hot_key_get(void *key)
{
    return I32(key, 0x58) + (I32(key, 0x5c) << 8);
}
/* 0x20482760, OnPaint: the key's name, as the host gives it */
void TC fs98_simsui_hot_key_paint(void *key)
{
    uint8_t dc[0x54];
    int32_t rect[4];
    char name[0x100];
    uint32_t text;
    CALL_THIS(MFC_470, void *, (void *, void *))(dc, key);
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(key, 0x20), rect);
    CALL_THIS(MFC_5736, int32_t, (void *))(dc);
    CALL_THIS(MFC_5789, void *, (void *, void *))(dc, PTR(PTR(key, 0x4c), 4));
    if (PTR(key, 0x4c)) {
        ((int32_t (SC *)(uint32_t, char *, uint32_t))PTR(HOST_TABLE, 0x200))(
            (uint32_t)fs98_simsui_hot_key_get(key), name, 0xff);
        CALL_THIS(MFC_537, void *, (void *, const char *))(&text, name);
        hot_paint_text(key, dc, text, rect);
    }
    CALL_THIS(MFC_5678, int32_t, (void *, int32_t))(dc, -1);
    CALL_THIS(MFC_755, void, (void *))(dc);
}
/* 0x20482860, OnKeyDown(key, repeat, flags): shift and ctrl held as modifiers (alt only
 * repaints); any other key, the keypad folded by the host, taken with the modifiers held and
 * the parent told */
void TC fs98_simsui_hot_key_key_down(void *key, uint32_t code, uint32_t repeat, uint32_t flags)
{
    (void)repeat;
    switch (code) {
    case 0x10: D32(key, 0x5c) |= 1; break;
    case 0x11: D32(key, 0x5c) |= 2; break;
    case 0x12: break;
    default: {
        uint32_t modifiers = D32(key, 0x5c), unused = 0, extended = (flags >> 8) & 1;
        ((uint32_t (SC *)(uint32_t *, uint32_t *, uint32_t *, uint32_t *))PTR(HOST_TABLE, 0x208))(
            &code, &unused, &modifiers, &extended);
        D32(key, 0x58) = code;
        D32(key, 0x5c) = modifiers | 8;
        hot_notify(key);
        break;
    }
    }
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(key, 0x20), 0, 1);
}
/* 0x20482940, OnSysKeyDown: as OnKeyDown */
void TC fs98_simsui_hot_key_sys_key_down(void *key, uint32_t code, uint32_t repeat, uint32_t flags)
{
    fs98_simsui_hot_key_key_down(key, code, repeat, flags);
}
/* 0x20482960, OnKeyUp: shift or ctrl let go; redrawn at once */
void TC fs98_simsui_hot_key_key_up(void *key, uint32_t code, uint32_t repeat, uint32_t flags)
{
    (void)repeat; (void)flags;
    if (code == 0x10) D32(key, 0x5c) &= ~1u;
    else if (code == 0x11) D32(key, 0x5c) &= ~2u;
    CALL_STD(RedrawWindow, int32_t, (uint32_t, const void *, uint32_t, uint32_t))(D32(key, 0x20), 0, 0, 0x105);
}
/* 0x204829a0, OnSysKeyUp: as OnKeyUp */
void TC fs98_simsui_hot_key_sys_key_up(void *key, uint32_t code, uint32_t repeat, uint32_t flags)
{
    fs98_simsui_hot_key_key_up(key, code, repeat, flags);
}
/* 0x204829c0, OnGetDlgCode: DLGC_WANTALLKEYS */
uint32_t TC fs98_simsui_hot_key_get_dlg_code(void *key) { (void)key; return 4; }

/* 0x204829d0, CSimsHotJoy::OnKillFocus (CSimsHotKey's map too): CWnd's with no window
 * (#6215 called with 0) */
void TC fs98_simsui_hot_joy_kill_focus(void *joy, void *next)
{
    (void)next;
    CALL_THIS(MFC_6215, void, (void *, void *))(joy, 0);
}
/* 0x204829e0 */
void *TC fs98_simsui_hot_joy_construct(void *joy) { return hot_construct(joy, VTABLE_HOT_JOY); }
/* 0x20482a60 */
void TC fs98_simsui_hot_joy_destroy(void *joy) { hot_destroy(joy, VTABLE_HOT_JOY); }
/* 0x20482a10 */
void *TC fs98_simsui_hot_joy_delete(void *joy, uint32_t flags)
{
    return fs98_simsui_delete_object(joy, flags, HOT_JOY_SIZE, fs98_simsui_hot_joy_destroy, fs98_simsui_ui_delete);
}
/* 0x20482ad0 */
const void *TC fs98_simsui_hot_joy_message_map(const void *joy) { (void)joy; return IMAGE(0x17720); }
/* 0x20482ae0, Create(style, rect, parent, id): CWnd::Create with no class and "" for a name */
int32_t TC fs98_simsui_hot_joy_create(void *joy, uint32_t style, const void *rect, void *parent, uint32_t id)
{
    return CALL_THIS(MFC_2124, int32_t, (void *, const char *, const char *, uint32_t, const void *, void *, uint32_t, void *))(
        joy, 0, (const char *)IMAGE(NO_TEXT), style, rect, parent, id, 0);
}
/* 0x20482b10, Init(CSimsFont *, flags): no control yet */
void TC fs98_simsui_hot_joy_init(void *joy, void *font, uint32_t flags)
{
    PTR(joy, 0x4c) = font;
    D32(joy, 0x58) = 0;
    D32(joy, 0x5c) = flags;
    hot_place(joy, font);
}
/* 0x20482b90, SetHotJoy(joystick, sense): the host waits for a control on it to move; the
 * parent told and the window repainted */
void TC fs98_simsui_hot_joy_set(void *joy, uint32_t joystick, uint32_t sense)
{
    ((uint32_t (SC *)(uint32_t, uint32_t, void *))PTR(HOST_TABLE, 0x1cc))(joystick, sense, AT(joy, 0x58));
    hot_notify(joy);
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(joy, 0x20), 0, 1);
}
/* 0x20482c00, GetHotJoy */
uint32_t TC fs98_simsui_hot_joy_get(void *joy) { return D32(joy, 0x58); }
/* 0x20482c10, OnPaint: a single space, whatever the control */
void TC fs98_simsui_hot_joy_paint(void *joy)
{
    uint8_t dc[0x54];
    int32_t rect[4];
    uint32_t text, copy;
    CALL_THIS(MFC_470, void *, (void *, void *))(dc, joy);
    CALL_THIS(MFC_537, void *, (void *, const char *))(&text, (const char *)IMAGE(NO_TEXT));
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(joy, 0x20), rect);
    CALL_THIS(MFC_5736, int32_t, (void *))(dc);
    CALL_THIS(MFC_5789, void *, (void *, void *))(dc, PTR(PTR(joy, 0x4c), 4));
    if (PTR(joy, 0x4c)) {
        CALL_THIS(MFC_860, void *, (void *, const char *))(&text, (const char *)IMAGE(ONE_SPACE));
        CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &text);
        hot_paint_text(joy, dc, copy, rect);
    }
    CALL_THIS(MFC_5678, int32_t, (void *, int32_t))(dc, -1);
    CALL_THIS(MFC_800, void, (void *))(&text);
    CALL_THIS(MFC_755, void, (void *))(dc);
}
