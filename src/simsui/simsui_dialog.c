/* CSimsDialog (0x10dc bytes, vtable 0x15ef0): SIMSUI's CDialog, painted from a bitmap
 * background with a palette of its own, with optional Help and Exit buttons, thirteen
 * CSimsFonts for its controls, a tab control, and the validation of edit fields.
 *
 * The CDialog to +0x60; +0x60; the tab control +0x7c; the resource handle to use while
 * the dialog initialises +0x80; the combo boxes' item height +0x84 (14); +0x88 (9; -1
 * makes the dialog not draggable by its body); the error box's flags +0x90 (2) and type
 * +0x94 (4); the Help button +0x98 and the Exit button +0x15c; the fonts +0x220 (0x70
 * each); the edit being validated +0x7d0; the focus while inactive +0x7d4; the selection
 * to restore +0x7d8; validating +0x7dc; OK or Cancel clicked +0x7e0; the edit to restore
 * it in +0x7e4; the CDIBPal +0x7e8; the background +0xff4 and the tab strip +0x1064
 * (CDIBSections); the window disabled while this one is up +0x10d4 and whether it was
 * enabled +0x10d8. */
#include "simsui_internal.h"

#define VTABLE_DIALOG 0x15ef0
#define VTABLE_PAL 0x15000
#define VTABLE_GDI_OBJECT 0x15030
#define VTABLE_OBJECT 0x15018
#define DIALOG_SIZE 0x10dc
#define FONT_SIZE 0x70
#define FONTS_AT 0x220
#define FONT_COUNT 13
#define RUNTIME_3PIECE 0x15398
#define RUNTIME_TAB_PAGE 0x16f80
#define BUTTON_TEXT 0x213d4                  /* "" */
#define NOTIFY_IDS 0x20098                   /* three control ids, to 0x200a4 */
#define NOTIFY_IDS_END 0x200a4
#define LINE_BREAK 0x20058
#define ERROR_CODES 0x15db8                  /* (code, string) pairs, ended by code -2 */
#define ERROR_LIBRARY 0x200ac
#define ERROR_MARK 0x200a8
#define ERROR_TAIL 0x200a4
#define HOST_TABLE G_PTR(0x2002c)

/* 0x20474d40, _GetBaseClass: CDialog's runtime class, MFC's */
void *SC fs98_simsui_dialog_base_class(void)
{
    return fs98_simsui_imports[SIMSUI_IMPORT_MFC_1816];
}
/* 0x20474d50, GetRuntimeClass */
const void *TC fs98_simsui_dialog_runtime_class(const void *dialog) { (void)dialog; return IMAGE(0x15c30); }
/* 0x20474d60, CSimsDialog(template id, parent) */
void *TC fs98_simsui_dialog_construct(void *dialog, uint32_t id, void *parent)
{
    int32_t font;
    CALL_THIS(MFC_324, void *, (void *, uint32_t, void *))(dialog, id, parent);
    fs98_simsui_help_construct(AT(dialog, 0x98));
    fs98_simsui_exit_construct(AT(dialog, 0x15c));
    for (font = 0; font < FONT_COUNT; ++font) fs98_simsui_font_construct(AT(dialog, FONTS_AT + font * FONT_SIZE));
    D32(dialog, 0x7ec) = 0;
    PTR(dialog, 0x7e8) = IMAGE(VTABLE_PAL);
    fs98_simsui_dib_construct(AT(dialog, 0xff4));
    fs98_simsui_dib_construct(AT(dialog, 0x1064));
    D32(dialog, 0x80) = 0;
    D32(dialog, 0x60) = 0;
    D32(dialog, 0x10d4) = 0;
    D32(dialog, 0x7e0) = 0;
    D32(dialog, 0x7d0) = 0;
    D32(dialog, 0x7e4) = 0;
    D32(dialog, 0x7dc) = 0;
    D32(dialog, 0x7c) = 0;
    D32(dialog, 0x7d4) = 0;
    PTR(dialog, 0) = IMAGE(VTABLE_DIALOG);
    D32(dialog, 0x84) = 0xe;
    D32(dialog, 0x88) = 9;
    D32(dialog, 0x10d8) = 1;
    D32(dialog, 0x90) = 2;
    D32(dialog, 0x94) = 4;
    return dialog;
}
/* 0x20474f90 */
void TC fs98_simsui_dialog_destroy(void *dialog)
{
    int32_t font;
    PTR(dialog, 0) = IMAGE(VTABLE_DIALOG);
    fs98_simsui_dib_destroy(AT(dialog, 0x1064));
    fs98_simsui_dib_destroy(AT(dialog, 0xff4));
    PTR(dialog, 0x7e8) = IMAGE(VTABLE_GDI_OBJECT);
    mfc_gdi_delete_object(AT(dialog, 0x7e8));
    PTR(dialog, 0x7e8) = IMAGE(VTABLE_OBJECT);
    for (font = FONT_COUNT - 1; font >= 0; --font) fs98_simsui_font_destroy(AT(dialog, FONTS_AT + font * FONT_SIZE));
    fs98_simsui_exit_destroy(AT(dialog, 0x15c));
    fs98_simsui_help_destroy(AT(dialog, 0x98));
    CALL_THIS(MFC_641, void, (void *))(dialog);
}
/* 0x20474f30 */
void *TC fs98_simsui_dialog_delete(void *dialog, uint32_t flags)
{
    return fs98_simsui_delete_object(dialog, flags, DIALOG_SIZE, fs98_simsui_dialog_destroy, fs98_simsui_ui_delete);
}
/* 0x20475110, DoDataExchange: none */
void TC fs98_simsui_dialog_exchange(void *dialog, void *exchange) { (void)dialog; (void)exchange; }
/* 0x20475120, _GetBaseMessageMap: CDialog's, MFC's */
const void *SC fs98_simsui_dialog_base_message_map(void)
{
    return fs98_simsui_imports[SIMSUI_IMPORT_MFC_4234];
}
/* 0x20475130, GetMessageMap */
const void *TC fs98_simsui_dialog_message_map(const void *dialog) { (void)dialog; return IMAGE(0x15c48); }
/* 0x20475140, OnGetDefID (DM_GETDEFID): the focused 3-piece button, else IDOK */
uint32_t TC fs98_simsui_dialog_default_id(void *dialog, uint32_t wparam, uint32_t lparam)
{
    void *focus = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetFocus, uint32_t, (void))());
    (void)dialog; (void)wparam; (void)lparam;
    if (focus && CALL_THIS(MFC_4083, int32_t, (void *, const void *))(focus, IMAGE(RUNTIME_3PIECE)))
        return ((uint32_t)CALL_THIS(MFC_3089, int32_t, (void *))(focus) & 0xffff) | 0x534b0000;
    return 0x534b0001;
}
/* 0x20475190, OnSafeSetFocus (0x5fa, CSimsRadioButton's and others'): the focus to wparam */
uint32_t TC fs98_simsui_safe_focus(void *window, uint32_t wparam, uint32_t lparam)
{
    (void)window; (void)lparam;
    CALL_STD(SetFocus, uint32_t, (uint32_t))(wparam);
    return 0;
}
/* 0x204751a0, OnLDialogParent: the window lparam disabled while this dialog is up */
uint32_t TC fs98_simsui_dialog_parent(void *dialog, uint32_t wparam, uint32_t window)
{
    (void)wparam;
    if (CALL_STD(IsWindow, int32_t, (uint32_t))(window)) {
        D32(dialog, 0x10d4) = window;
        D32(dialog, 0x10d8) = (uint32_t)CALL_STD(IsWindowEnabled, int32_t, (uint32_t))(window);
        CALL_STD(EnableWindow, int32_t, (uint32_t, int32_t))(window, 0);
    }
    return 0;
}
/* 0x204751e0, Close: marked closed (its user data 1), the parent given back, hidden */
void TC fs98_simsui_dialog_close(void *dialog)
{
    CALL_STD(SetWindowLongA, int32_t, (uint32_t, int32_t, int32_t))(D32(dialog, 0x20), -21, 1);
    if (D32(dialog, 0x10d4) && CALL_STD(IsWindow, int32_t, (uint32_t))(D32(dialog, 0x10d4))) {
        CALL_STD(EnableWindow, int32_t, (uint32_t, int32_t))(D32(dialog, 0x10d4), 1);
        CALL_STD(SetFocus, uint32_t, (uint32_t))(D32(dialog, 0x10d4));
    }
    CALL_THIS(MFC_6215, int32_t, (void *, int32_t))(dialog, 0);
}
/* 0x20475230, DrawPalette: the palette's colours over the top-left ninth of the window */
void TC fs98_simsui_dialog_draw_palette(void *dialog)
{
    uint8_t dc[0x14];
    int32_t rect[4];
    CALL_THIS(MFC_289, void *, (void *, void *))(dc, dialog);
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(dialog, 0x20), rect);
    rect[2] /= 3;
    rect[3] /= 3;
    fs98_simsui_pal_draw(AT(dialog, 0x7e8), dc, rect, 0);
    CALL_THIS(MFC_613, void, (void *))(dc);
}
/* 0x204752d0, InitTab(strip id, x, y, width, height, HINSTANCE): the tab strip, `width` by
 * `height`, tiled from the strip, and copied into the background at (x, y) */
int32_t TC fs98_simsui_dialog_init_tab(void *dialog, int32_t id, int32_t x, int32_t y, int32_t width, int32_t height,
                                       uint32_t instance)
{
    uint8_t strip[0x70];
    int32_t rect[4], at;
    uint8_t *info;
    fs98_simsui_dib_construct(strip);
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(dialog, 0x20), rect);
    if (!fs98_simsui_dib_load(strip, id, instance, AT(dialog, 0x7e8), 0)) {
        fs98_simsui_dib_destroy(strip);
        return 0;
    }
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0x10) = 0;
    I32(info, 4) = width;
    I32(info, 8) = -height;
    D32(info, 0) = 0x28;
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(dialog, 0x7f0 + at * 4);
    at = fs98_simsui_dib_create(AT(dialog, 0x1064), info, AT(dialog, 0x7e8), 0);
    mfc_operator_delete(info);
    if (!at) {
        fs98_simsui_dib_destroy(strip);
        return 0;
    }
    fs98_simsui_dib_tile(strip, AT(dialog, 0x1064));
    fs98_simsui_dib_copy(AT(dialog, 0x1064), AT(dialog, 0xff4), x, y, width, height, 0, 0);
    fs98_simsui_dib_destroy(strip);
    return 1;
}
/* 0x20475460, Init(background id, HINSTANCE, flags): the background (flags & 8: tiled from
 * the strip to the remapped client size; else the bitmap itself, its palette made with
 * the static colours), the window sized to it (flags & 1: centred on the parent when the
 * screen is wider than 640 and the parent is shown, else on the screen), and the Exit
 * (flags & 4) and Help (flags & 2) buttons at the top right. A background that will not
 * load fails, the screen's DC kept. */
int32_t TC fs98_simsui_dialog_init(void *dialog, int32_t id, uint32_t instance, uint32_t flags)
{
    uint32_t screen;
    int32_t rect[4], width, height, x, y;
    if (!instance) instance = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    screen = CALL_STD(GetDC, uint32_t, (uint32_t))(0);
    if (!screen) fs98_simsui_exit_cleanly();
    if (flags & 8) {
        uint8_t strip[0x70], *info;
        int32_t at, made;
        fs98_simsui_dib_construct(strip);
        CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(dialog, 0x20), rect);
        fs98_simsui_remap_rect(rect, dialog);
        if (!fs98_simsui_dib_load(strip, id, instance, AT(dialog, 0x7e8), 0xa)) {
            fs98_simsui_dib_destroy(strip);
            return 0;
        }
        info = mfc_operator_new(0x428);
        fs98_simsui_fill(info, 0, 0x428);
        D32(info, 0) = 0x28;
        D16(info, 0xc) = 1;
        D16(info, 0xe) = 8;
        D32(info, 0x10) = 0;
        I32(info, 4) = rect[2] - rect[0];
        I32(info, 8) = rect[3] - rect[1];
        for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(dialog, 0x7f0 + at * 4);
        made = fs98_simsui_dib_create(AT(dialog, 0xff4), info, AT(dialog, 0x7e8), 0);
        mfc_operator_delete(info);
        if (!made) {
            fs98_simsui_dib_destroy(strip);
            return 0;
        }
        fs98_simsui_dib_tile(strip, AT(dialog, 0xff4));
        fs98_simsui_dib_destroy(strip);
    } else if (!fs98_simsui_dib_load(AT(dialog, 0xff4), id, instance, AT(dialog, 0x7e8), 0xa))
        return 0;
    height = I32(dialog, 0x1000);
    width = I32(dialog, 0xffc);
#define SET_POS(left, top, flag) CALL_THIS(MFC_6197, int32_t, (void *, const void *, int32_t, int32_t, int32_t, int32_t, \
                                                                 uint32_t))(dialog, 0, left, top, width, height, flag)
    if (flags & 1) {
        uint32_t parent = CALL_STD(GetParent, uint32_t, (uint32_t))(D32(dialog, 0x20));
        if (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(screen, 8) > 0x280 && parent &&
            !CALL_STD(IsIconic, int32_t, (uint32_t))(parent)) {
            CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(parent, rect);
            CALL_STD(ClientToScreen, int32_t, (uint32_t, int32_t *))(parent, rect);
            CALL_STD(ClientToScreen, int32_t, (uint32_t, int32_t *))(parent, rect + 2);
            if (rect[0] <= 10000 && rect[1] <= 10000) {
                x = (rect[2] - rect[0] - width) / 2 + rect[0];
                y = (rect[3] - rect[1] - height) / 2 + rect[1];
                SET_POS(x, y, 4);
                goto placed;
            }
        }
        x = (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(screen, 8) - width) / 2;
        y = (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(screen, 0xa) - height) / 2;
        SET_POS(x, y, 4);
    } else SET_POS(0, 0, 6);
#undef SET_POS
placed:
    if (flags & 4) {
        int32_t area[4] = {0, 0, 10, 10};
        void *button = AT(dialog, 0x15c);
        CALL_THIS(MFC_2078, int32_t, (void *, const char *, uint32_t, const int32_t *, void *, uint32_t))(
            button, IMAGE(BUTTON_TEXT), 0x5002000b, area, dialog, 0x2711);
        fs98_simsui_exit_init(button, AT(dialog, 0x7e8), 0x1f1, 10);
        CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
            button, width - I32(dialog, 0x214) - 10, 10, I32(dialog, 0x214), I32(dialog, 0x210), 1);
    }
    if (flags & 2) {
        int32_t area[4] = {0, 0, 10, 10};
        void *button = AT(dialog, 0x98);
        CALL_THIS(MFC_2078, int32_t, (void *, const char *, uint32_t, const int32_t *, void *, uint32_t))(
            button, IMAGE(BUTTON_TEXT), 0x5002000b, area, dialog, 0x2710);
        fs98_simsui_help_init(button, AT(dialog, 0x7e8), 0x1db, 10);
        x = (flags & 4) ? width - (I32(dialog, 0x150) * 2 + 10) : width - I32(dialog, 0x150) - 10;
        CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
            button, x, 10, I32(dialog, 0x150), I32(dialog, 0x14c), 1);
    }
    CALL_STD(ReleaseDC, int32_t, (uint32_t, uint32_t))(0, screen);
    return 1;
}
/* a CSimsFont of the dialog's */
#define FONT(index) AT(dialog, FONTS_AT + (index) * FONT_SIZE)
/* 0x20475870, OnInitDialog: CDialog's (with the dialog's resources current, when it has
 * them), the capture let go, three controls' styles changed, and the fonts set */
int32_t TC fs98_simsui_dialog_on_init(void *dialog)
{
    uint32_t saved = 0, at;
    if (D32(dialog, 0x80)) {
        saved = D32(CALL_STD(MFC_1168, void *, (void))(), 0xc);
        D32(CALL_STD(MFC_1168, void *, (void))(), 0xc) = D32(dialog, 0x80);
    }
    CALL_THIS(MFC_4710, int32_t, (void *))(dialog);
    if (saved) D32(CALL_STD(MFC_1168, void *, (void))(), 0xc) = saved;
    CALL_STD(ReleaseCapture, int32_t, (void))();
    for (at = NOTIFY_IDS; at < NOTIFY_IDS_END; at += 4) {
        void *control = CALL_THIS(MFC_3092, void *, (void *, int32_t))(dialog, I32(fs98_simsui_image, at));
        if (control) {
            CALL_THIS(MFC_4287, int32_t, (void *, uint32_t, uint32_t, uint32_t))(control, 4, 0, 0);
            CALL_THIS(MFC_4284, int32_t, (void *, uint32_t, uint32_t, uint32_t))(control, 0, 0x4000, 0);
        }
    }
    PTR(FONT(0), 4) = fs98_simsui_base_font(8);
    D32(FONT(0), 8) = 0xffffff; D32(FONT(0), 0xc) = 0; D32(FONT(0), 0x50) = 0; D32(FONT(0), 0x54) = 1;
    PTR(FONT(1), 4) = fs98_simsui_base_font(8);
    D32(FONT(1), 8) = 0xffffff; D32(FONT(1), 0xc) = 0; D32(FONT(1), 0x50) = 0; D32(FONT(1), 0x54) = 1;
    D16(FONT(1), 0x58) = 0x100;
    PTR(FONT(2), 4) = fs98_simsui_base_font(8);
    D32(FONT(2), 8) = 0xffffff; D32(FONT(2), 0xc) = 0; D32(FONT(2), 0x50) = 0; D32(FONT(2), 0x54) = 1;
    D16(FONT(2), 0x58) = 0x200;
    PTR(FONT(3), 4) = fs98_simsui_base_font(1);
    D32(FONT(3), 8) = 0xffffff; D32(FONT(3), 0xc) = 0; D32(FONT(3), 0x50) = 1; D32(FONT(3), 0x54) = 0;
    D16(FONT(3), 0x58) = 4;
    for (at = 4; at <= 5; ++at) {
        PTR(FONT(at), 4) = fs98_simsui_base_font(3);
        D32(FONT(at), 8) = 0; D32(FONT(at), 0xc) = 0xffffff; D32(FONT(at), 0x14) = 0xffffff;
        D32(FONT(at), 0x18) = 0xffffff; D32(FONT(at), 0x1c) = 0; D32(FONT(at), 0x24) = 0xff0000;
        D32(FONT(at), 0x4c) = 1; D32(FONT(at), 0x54) = 0;
    }
    PTR(FONT(6), 4) = fs98_simsui_base_font(4);
    D32(FONT(6), 8) = 0xffffff;
    PTR(FONT(7), 4) = fs98_simsui_base_font(5);
    D32(FONT(7), 8) = 0xffffff;
    PTR(FONT(8), 4) = fs98_simsui_base_font(0);
    D32(FONT(8), 8) = 0xffffff; D32(FONT(8), 0x18) = 0xffffff; D16(FONT(8), 0x58) = 0x400;
    PTR(FONT(9), 4) = fs98_simsui_base_font(0);
    D32(FONT(9), 8) = 0xc8c8c8; D16(FONT(9), 0x58) = 0x400;
    PTR(FONT(10), 4) = fs98_simsui_base_font(0);
    D32(FONT(10), 8) = 0xc8c8c8; D32(FONT(10), 0x18) = 0xc8c8c8; D16(FONT(10), 0x58) = 0x400;
    PTR(FONT(11), 4) = fs98_simsui_base_font(0);
    D32(FONT(11), 8) = 0xffffff; D16(FONT(11), 0x58) = 0x400;
    PTR(FONT(12), 4) = fs98_simsui_base_font(6);
    D32(FONT(12), 0xc) = 0xffffff; D32(FONT(12), 8) = 0; D32(FONT(12), 0x50) = 0; D32(FONT(12), 0x54) = 1;
    return 1;
}
#undef FONT
/* the colour table the palette gives a section in a DC: the identity one on a palette
 * device before the static colours are set, else its entries */
static void *colour_table(void *pal, uint32_t dc_attrib)
{
    if (!D32(pal, 0x808) && (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(dc_attrib, 0x26) & 0x100))
        return AT(pal, 0x408);
    return AT(pal, 8);
}
/* the active and foreground windows' processes asked for, as retail asks and ignores */
static void ask_processes(void)
{
    uint32_t active_process = 0, foreground_process = 0;
    uint32_t active = CALL_STD(GetActiveWindow, uint32_t, (void))();
    uint32_t foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
    if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
    if (foreground) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
}
/* 0x20475b00, OnEraseBkgnd: the background copied in through the palette */
int32_t TC fs98_simsui_dialog_erase(void *dialog, void *dc)
{
    uint8_t memory[0x10];
    int32_t rect[4];
    void *old_pal, *old, *bitmap;
    CALL_THIS(MFC_323, void *, (void *))(memory);
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(dc ? D32(dc, 4) : 0));
    old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, AT(dialog, 0x7e8), 0);
    CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(dialog, 0x20), rect);
    bitmap = AT(dialog, 0x1050);
    old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
    ask_processes();
    CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(
        D32(memory, 4), 0, 0x100, colour_table(AT(dialog, 0x7e8), D32(memory, 8)));
    CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
        D32(dc, 4), rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], D32(memory, 4), 0, 0, 0xcc0020);
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
    if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 1);
    CALL_THIS(MFC_640, void, (void *))(memory);
    return 1;
}
/* 0x20475cb0, OnMeasureItem: a combo box's items the dialog's height */
void TC fs98_simsui_dialog_measure(void *dialog, int32_t id, uint8_t *measure)
{
    if (D32(measure, 0) == 3) D32(measure, 0x10) = D32(dialog, 0x84);
    CALL_THIS(MFC_4774, void, (void *, int32_t, void *))(dialog, id, measure);
}
/* 0x20475cd0, OnSysExit: IDCANCEL posted */
void TC fs98_simsui_dialog_sys_exit(void *dialog)
{
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(dialog, 0x20), 0x111, 2, 0);
}
/* 0x20475cf0, OnSysHelp: context help at the cursor */
void TC fs98_simsui_dialog_sys_help(void *dialog)
{
    int32_t point[2];
    CALL_STD(GetCursorPos, int32_t, (int32_t *))(point);
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(
        D32(dialog, 0x20), 0x112, 0xf180, ((uint32_t)point[1] & 0xffff) << 16 | ((uint32_t)point[0] & 0xffff));
}
/* 0x20475d40, OnQueryNewPalette: the palette realised in a client DC, the window repainted */
int32_t TC fs98_simsui_dialog_query_palette(void *dialog)
{
    uint8_t dc[0x14];
    void *old;
    int32_t realised;
    CALL_THIS(MFC_289, void *, (void *, void *))(dc, dialog);
    old = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, AT(dialog, 0x7e8), 0);
    realised = (int32_t)CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(dialog, 0x20), 0, 1);
    if (old) {
        CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old, 1);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
    }
    CALL_THIS(MFC_613, void, (void *))(dc);
    return realised;
}
/* 0x20475e00, OnPaletteChanged (CSimsTabPage's and others'): repainted */
void TC fs98_simsui_palette_changed(void *window, void *focus)
{
    (void)focus;
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(window, 0x20), 0, 1);
}
/* 0x20475e20, OnNcHitTest: the body drags the dialog, unless +0x88 is -1 */
uint32_t TC fs98_simsui_dialog_hit_test(void *dialog, int32_t x, int32_t y)
{
    (void)x; (void)y;
    if (I32(dialog, 0x88) == -1) return CALL_THIS(MFC_2379, uint32_t, (void *))(dialog);
    return 2;
}
/* 0x20475e40, DoErrorDialog(control, code): the code's message (from the error table, its
 * strings in the main DLL), with the control's name (its text with the part after the
 * mark cut out), or for an exception record (a code above 0x2000) its description, shown
 * by HOST's message box with the dialog's flags and type; those set back to 2 and 4.
 * Whether the answer was 2 or 0x40, or the flags hold neither 4 nor 0x80. */
int32_t TC fs98_simsui_dialog_error(void *dialog, void *control, uint32_t code)
{
    struct { uint32_t zero, also_zero, flags; volatile uint32_t answer; uint32_t type; char *text; } box;
    char message[0x104];
    uint32_t name, joined, unused_c, unused_d, text, piece, string, library;
    const uint32_t *record = 0;
    int32_t is_record = 0, index, result;
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_540, void *, (void *))(&joined);
    CALL_THIS(MFC_540, void *, (void *))(&unused_c);
    CALL_THIS(MFC_540, void *, (void *))(&unused_d);
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&piece);
    box.zero = 0;
    box.also_zero = 0;
    box.flags = D32(dialog, 0x90);
    box.type = D32(dialog, 0x94);
    if (code == 0xd) box.flags |= 4;
    if (code > 0x2000 && code != 0xffffffffu) {
        record = (const uint32_t *)(uintptr_t)code;
        if (CALL_STD(MFC_1187, int32_t, (const void *, uint32_t, int32_t))((const void *)(uintptr_t)record[3], 4, 1) &&
            CALL_STD(MFC_1187, int32_t, (const void *, uint32_t, int32_t))((const void *)(uintptr_t)record[4], 4, 1))
            is_record = 1;
        code = record[0];
    }
    for (index = 0; I32(fs98_simsui_image, ERROR_CODES + index * 8) != -2 &&
                    D32(fs98_simsui_image, ERROR_CODES + index * 8) != code; ++index) {}
    string = D32(fs98_simsui_image, ERROR_CODES + index * 8 + 4);
    library = CALL_STD(LoadLibraryA, uint32_t, (const char *))(IMAGE(ERROR_LIBRARY));
    CALL_STD(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(library, string, message, 0xff);
    if (control) {
        int32_t mark;
        if (CALL_STD(MFC_1187, int32_t, (const void *, uint32_t, int32_t))(PTR(control, 0x40), 4, 1) && PTR(control, 0x40))
            fs98_simsui_text_window_text(PTR(control, 0x40), &name);
        else CALL_THIS(MFC_3874, void, (void *, void *))(control, &name);
        mark = CALL_THIS(MFC_2784, int32_t, (void *, const char *))(&name, IMAGE(ERROR_MARK));
        if (mark != -1) {
            uint32_t part;
            CALL_THIS(MFC_858, void *, (void *, const void *))(
                &joined, CALL_THIS(MFC_4129, void *, (void *, void *, int32_t))(&name, &part, mark));
            CALL_THIS(MFC_800, void, (void *))(&part);
            CALL_THIS(MFC_939, void *, (void *, const void *))(
                &joined, CALL_THIS(MFC_5710, void *, (void *, void *, int32_t))(
                             &name, &part, I32((uintptr_t)name, -8) - mark - 1));
            CALL_THIS(MFC_800, void, (void *))(&part);
            CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &joined);
        }
        CALL_STD(lstrcatA, char *, (char *, const char *))(message, IMAGE(LINE_BREAK));
        if (!is_record) {
            CALL_STD(lstrcatA, char *, (char *, const char *))(message, IMAGE(LINE_BREAK));
            CALL_STD(lstrcatA, char *, (char *, const char *))(message, CALL_THIS(MFC_2915, char *, (void *, int32_t))(&name, 0));
        } else {
            static const uint32_t strings[3] = {0xf83d, 0xf841, 0xf80c};
            CALL_THIS(MFC_860, void *, (void *, const char *))(&text, (const char *)(uintptr_t)record[1]);
            for (index = 0; index < 3; ++index) {
                CALL_STD(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(
                    library, strings[index], CALL_THIS(MFC_2915, char *, (void *, int32_t))(&piece, 0x100), 0x100);
                CALL_THIS(MFC_5572, void, (void *, int32_t))(&piece, -1);
                CALL_THIS(MFC_939, void *, (void *, const void *))(&text, &piece);
                if (index == 1) CALL_THIS(MFC_941, void *, (void *, const char *))(&text, (const char *)(uintptr_t)record[3]);
            }
            CALL_THIS(MFC_941, void *, (void *, const char *))(&text, (const char *)(uintptr_t)record[4]);
            CALL_THIS(MFC_941, void *, (void *, const char *))(&text, IMAGE(ERROR_TAIL));
            CALL_THIS(MFC_939, void *, (void *, const void *))(&text, &name);
            CALL_THIS(MFC_941, void *, (void *, const char *))(&text, IMAGE(ERROR_TAIL));
            CALL_STD(lstrcpyA, char *, (char *, const char *))(message, CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0));
        }
    }
    CALL_STD(FreeLibrary, int32_t, (uint32_t))(library);
    box.text = message;
    ((void (SC *)(uint32_t, void *))PTR(HOST_TABLE, 0x10c))(2, &box);
    if (box.answer == 2 || box.answer == 0x40) result = 1;
    else {
        CALL_STD(GetAsyncKeyState, int16_t, (int32_t))(0x1b);
        CALL_STD(GetAsyncKeyState, int16_t, (int32_t))(0x73);
        result = (D8(dialog, 0x90) & 0x84) ? 0 : 1;
    }
    D32(dialog, 0x90) = 2;
    D32(dialog, 0x94) = 4;
    CALL_THIS(MFC_800, void, (void *))(&piece);
    CALL_THIS(MFC_800, void, (void *))(&text);
    CALL_THIS(MFC_800, void, (void *))(&unused_d);
    CALL_THIS(MFC_800, void, (void *))(&unused_c);
    CALL_THIS(MFC_800, void, (void *))(&joined);
    CALL_THIS(MFC_800, void, (void *))(&name);
    return result;
}
/* 0x20476790, OnEditSetFocus (0x5f?): the focus to control wparam */
uint32_t TC fs98_simsui_dialog_edit_set_focus(void *dialog, uint32_t id, uint32_t lparam)
{
    (void)lparam;
    if (id) {
        void *control = CALL_THIS(MFC_3092, void *, (void *, int32_t))(dialog, (int32_t)id);
        if (control) CALL_THIS(MFC_5981, void *, (void *))(control);
    }
    return 0;
}
/* 0x204767b0, OnEditKillFocus: while validating, the edit validated (virtual +0xdc); one
 * that fails has its selection put back */
uint32_t TC fs98_simsui_dialog_edit_kill_focus(void *dialog, uint32_t id, uint32_t lparam)
{
    (void)lparam;
    if (D32(dialog, 0x7dc)) {
        int32_t failed = VIRTUAL(dialog, 0xdc, int32_t, (void *, uint32_t))(dialog, id);
        fs98_simsui_dialog_set_edit(dialog, failed);
        if (failed && D32(dialog, 0x7e4)) {
            CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(dialog, 0x7e4), 0x186,
                                                                                     D32(dialog, 0x7d8), 0);
            D32(dialog, 0x7d8) = 0;
            D32(dialog, 0x7e4) = 0;
        }
    }
    return 0;
}
/* 0x20476820, Validate(id): a failing id has 0x5f5 posted for it */
int32_t TC fs98_simsui_dialog_validate(void *dialog, int32_t id)
{
    if (id) CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(dialog, 0x20), 0x5f5, (uint32_t)id, 0);
    return id;
}
/* 0x20476850, OnParentNotify: CWnd's; a click on one of three controls stops validation */
void TC fs98_simsui_dialog_parent_notify(void *dialog, uint32_t message, uint32_t lparam)
{
    int32_t rect[4] = {0, 0, 0, 0}, x, y;
    uint32_t at;
    CALL_THIS(MFC_4881, void, (void *, uint32_t, uint32_t))(dialog, message, lparam);
    if (message != 0x201) return;
    x = (int32_t)(lparam & 0xffff);
    y = (int32_t)(lparam >> 16);
    for (at = NOTIFY_IDS; at < NOTIFY_IDS_END; at += 4) {
        void *control = CALL_THIS(MFC_3092, void *, (void *, int32_t))(dialog, I32(fs98_simsui_image, at));
        if (!control) continue;
        CALL_STD(GetWindowRect, int32_t, (uint32_t, int32_t *))(D32(control, 0x20), rect);
        CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(dialog, 0x20), rect);
        CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(dialog, 0x20), rect + 2);
        if (CALL_STD(PtInRect, int32_t, (const int32_t *, int32_t, int32_t))(rect, x, y)) {
            fs98_simsui_dialog_set_validate(dialog, 0);
            return;
        }
    }
}
/* a dialog field set here, in the active tab page, and in the page's owner when this is a
 * tab page (+0x10f4) */
static void set_everywhere(void *dialog, uint32_t offset, uint32_t value)
{
    if (PTR(dialog, 0x7c)) D32(fs98_simsui_tabs_active_page(PTR(dialog, 0x7c)), offset) = value;
    if (CALL_THIS(MFC_4083, int32_t, (void *, const void *))(dialog, IMAGE(RUNTIME_TAB_PAGE)))
        D32(PTR(dialog, 0x10f4), offset) = value;
    D32(dialog, offset) = value;
}
/* 0x20476910, SetValidate */
void TC fs98_simsui_dialog_set_validate(void *dialog, uint32_t value) { set_everywhere(dialog, 0x7dc, value); }
/* 0x20476960, SetEditValidateID */
void TC fs98_simsui_dialog_set_edit(void *dialog, uint32_t value) { set_everywhere(dialog, 0x7d0, value); }
/* 0x204769b0, SetOkOrCancelClicked */
void TC fs98_simsui_dialog_set_clicked(void *dialog, uint32_t value) { set_everywhere(dialog, 0x7e0, value); }
/* 0x20476a00, PostMessageSimsEditKillFocus: 0x5f4 to the active page, or to the dialog */
void TC fs98_simsui_dialog_post_kill_focus(void *dialog, uint32_t id)
{
    uint32_t window = PTR(dialog, 0x7c) ? D32(fs98_simsui_tabs_active_page(PTR(dialog, 0x7c)), 0x20) : D32(dialog, 0x20);
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(window, 0x5f4, id, 0);
}
