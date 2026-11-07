/* CSimsButton (0xc0 bytes, vtable 0x157d8): an owner-drawn CButton painted from a bitmap
 * strip of four equal frames stacked top to bottom (normal, pressed, focused, disabled).
 *
 * The CButton (CWnd) to +0x40; its CDIBSection +0x40 (so the bitmap's BITMAP +0x44, width
 * +0x48, height +0x4c, CBitmap +0x9c); the CDIBPal +0xb0; a frame's height (a quarter of
 * the strip) +0xb4 and width +0xb8; and the palette state last given to the bits +0xbc.
 * ms_ObjectCount (.data +0x213cc) counts the live buttons. */
#include "simsui_internal.h"

#define VTABLE_CBUTTON 0x158a0
#define VTABLE_BUTTON 0x157d8
#define BUTTON_SIZE 0xc0
#define OBJECT_COUNT I32(fs98_simsui_image, 0x213cc)
#define MESSAGE_MAP_SLIDER_BASE 0x15610

/* 0x20473600 */
void *TC fs98_simsui_button_construct(void *button)
{
    mfc_wnd_construct(button);
    PTR(button, 0) = IMAGE(VTABLE_CBUTTON);
    fs98_simsui_dib_construct(AT(button, 0x40));
    PTR(button, 0) = IMAGE(VTABLE_BUTTON);
    ++OBJECT_COUNT;
    return button;
}
/* 0x204736c0 */
void TC fs98_simsui_button_destroy(void *button)
{
    PTR(button, 0) = IMAGE(VTABLE_BUTTON);
    --OBJECT_COUNT;
    fs98_simsui_dib_destroy(AT(button, 0x40));
    CALL_THIS(MFC_609, void, (void *))(button);
}
/* 0x20473660 */
void *TC fs98_simsui_button_delete(void *button, uint32_t flags)
{
    return fs98_simsui_delete_object(button, flags, BUTTON_SIZE, fs98_simsui_button_destroy, fs98_simsui_ui_delete);
}
/* 0x20473ad0: the CButton's deleting destructor (its vtable's) */
void *TC fs98_simsui_cbutton_delete(void *button, uint32_t flags)
{
    CALL_THIS(MFC_609, void, (void *))(button);
    if (flags & 1) mfc_operator_delete(button);
    return button;
}
/* 0x20473720, CSimsSlider::_GetBaseMessageMap (and CSimsButton's GetMessageMap, folded) */
const void *SC fs98_simsui_slider_base_message_map(void)
{
    return IMAGE(MESSAGE_MAP_SLIDER_BASE);
}
/* 0x20473730, Init(id, CDIBPal *, HINSTANCE): the bitmap strip `id` loaded; the window
 * placed by its text ("name#x,y", else where it is, remapped), a frame in size, and shown */
int32_t TC fs98_simsui_button_init(void *button, int32_t id, void *pal, uint32_t instance)
{
    uint32_t text, name;
    int32_t point[2];
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_3874, void, (void *, void *))(button, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_point(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, point);
    else {
        fs98_simsui_remap_point(point, button);
        CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &text);
    }
    if (!fs98_simsui_dib_load(AT(button, 0x40), id, instance, pal, 0)) {
        CALL_THIS(MFC_800, void, (void *))(&name);
        CALL_THIS(MFC_800, void, (void *))(&text);
        return 0;
    }
    I32(button, 0xb4) = I32(button, 0x4c) / 4;
    I32(button, 0xb8) = I32(button, 0x48);
    PTR(button, 0xb0) = pal;
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        button, point[0], point[1], I32(button, 0xb8), I32(button, 0xb4), 1);
    CALL_THIS(MFC_6215, int32_t, (void *, int32_t))(button, 5);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return 1;
}
/* 0x20473880, DrawItem(DRAWITEMSTRUCT *): the frame for the item's state (selected, else
 * focused, else disabled, else normal) copied in; the bits given the palette's colours
 * first when its state has changed */
void TC fs98_simsui_button_draw_item(void *button, const uint8_t *item)
{
    uint8_t dc[0x10], memory[0x10];
    uint32_t state = D32(item, 0x10), active_process, foreground_process;
    void *old_pal, *old, *pal;
    int32_t row;
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_THIS(MFC_323, void *, (void *))(memory);
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, D32(item, 0x18));
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory,
            CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(D32(dc, 4)))) {
        void *bitmap = AT(button, 0x9c);
        old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(button, 0xb0), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        pal = PTR(button, 0xb0);
        if (D32(button, 0xbc) != D32(pal, 0x808)) {
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
            D32(button, 0xbc) = D32(PTR(button, 0xb0), 0x808);
        }
        if (state & 1) row = I32(button, 0xb4);
        else if (state & 0x10) row = I32(button, 0xb4) * 2;
        else if (state & 4) row = I32(button, 0xb4) * 3;
        else row = 0;
        CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(dc, 4), 0, 0, I32(button, 0xb8), I32(button, 0xb4), D32(memory, 4), 0, row, 0xcc0020);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
        if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 0);
    }
    CALL_THIS(MFC_2450, uint32_t, (void *))(dc);
    CALL_THIS(MFC_640, void, (void *))(memory);
    CALL_THIS(MFC_640, void, (void *))(dc);
}

/* CSims3PieceButton (0xfc bytes, vtable 0x15478): a CSimsButton whose four frames are
 * built at Init from a strip of pieces (a left cap, a middle repeated, a right cap: twelve
 * piece rows, three for each state) over the dialog's background, with its text drawn on, and optionally a
 * shadow, a mask of its shape (+0xe8) for hit tests, and a popup menu.
 *
 * Beyond CSimsButton: the mnemonic +0xc0 (byte); the menu +0xc4, its strings' instance
 * +0xc8 and item count +0xcc; pressed +0xd4; the text's rectangle when flags & 8 +0xd8;
 * the mask +0xe8; +0xec; flags +0xf0 (1 keyed by an unused colour, 2|4 keyed by the strip's
 * first pixel, 8 the given rectangle, 0x10 a blurred shadow, 0x20 a hard shadow, 0x40
 * focus on hover); the fonts, normal +0xf4 and hot +0xf8. */
#define VTABLE_3PIECE 0x15478
#define THREE_PIECE_SIZE 0xfc
#define BM_SETSTATE_ 0xf3

/* 0x20472390, _GetBaseClass: CButton's runtime class, MFC's */
void *SC fs98_simsui_3piece_base_class(void)
{
    return fs98_simsui_imports[SIMSUI_IMPORT_MFC_1795];
}
/* 0x204723a0 */
const void *TC fs98_simsui_3piece_runtime_class(const void *button)
{
    (void)button;
    return IMAGE(0x15398);
}
/* 0x204724f0 */
const void *TC fs98_simsui_3piece_message_map(const void *button)
{
    (void)button;
    return IMAGE(0x153b0);
}
/* 0x204723b0 */
void *TC fs98_simsui_3piece_construct(void *button)
{
    fs98_simsui_button_construct(button);
    D32(button, 0xf0) = 1;
    D8(button, 0xc0) = 0;
    D32(button, 0xc4) = 0;
    D32(button, 0xcc) = 0;
    D32(button, 0xd4) = 0;
    D32(button, 0xe8) = 0;
    D32(button, 0xec) = 0;
    PTR(button, 0) = IMAGE(VTABLE_3PIECE);
    return button;
}
/* 0x20472460 */
void TC fs98_simsui_3piece_destroy(void *button)
{
    PTR(button, 0) = IMAGE(VTABLE_3PIECE);
    if (D32(button, 0xe8)) {
        mfc_operator_delete(PTR(button, 0xe8));
        D32(button, 0xe8) = 0;
    }
    if (D32(button, 0xc4)) {
        CALL_STD(DestroyMenu, int32_t, (uint32_t))(D32(button, 0xc4));
        D32(button, 0xc4) = 0;
    }
    fs98_simsui_button_destroy(button);
}
/* 0x20472400 */
void *TC fs98_simsui_3piece_delete(void *button, uint32_t flags)
{
    return fs98_simsui_delete_object(button, flags, THREE_PIECE_SIZE, fs98_simsui_3piece_destroy, fs98_simsui_ui_delete);
}
/* a CString copied for a by-value argument */
static uint32_t cstring_copy(const void *text)
{
    uint32_t copy;
    CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, text);
    return copy;
}
/* 0x20472500, Init(font, hot font, strip id, background, CDIBPal *, flags, key, HINSTANCE,
 * hot x, hot y, CRect text): see above. The window's text is "name#x,y,width" (else where
 * it is, remapped); its '&' marks the mnemonic, a '\n' makes it two lines. */
int32_t TC fs98_simsui_3piece_init(void *button, void *font, void *hot_font, int32_t id, void *background,
                                   void *pal, uint32_t flags, int32_t key, uint32_t instance, int32_t hot_x,
                                   int32_t hot_y, struct fs98_simsui_rect text_rect)
{
    uint8_t face[0x70], shadow[0x70], strip[0x70], dc[0x10];
    static const int32_t mask_offsets[8] = {4, 6, 3, 4, 4, 6, 4, 6};
    uint32_t text, name;
    int32_t point[2], size[2], rect[4];
    int32_t piece, width, two_lines, keyed, count, rest, state, unused = 0, at;
    uint32_t has_shadow;
    uint16_t font_format, hot_format;
    uint8_t *info;
    void *old, *old_font;
    PTR(button, 0xf4) = font;
    PTR(button, 0xf8) = hot_font;
    I32(button, 0xd8) = text_rect.left;
    I32(button, 0xdc) = text_rect.top;
    I32(button, 0xe0) = text_rect.right;
    I32(button, 0xe4) = text_rect.bottom;
    fs98_simsui_dib_construct(face);
    fs98_simsui_dib_construct(shadow);
    fs98_simsui_dib_construct(strip);
    D32(button, 0xf0) = flags;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_3874, void, (void *, void *))(button, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_point_and(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, point,
                                    (int32_t *)AT(button, 0xb8));
    else {
        fs98_simsui_remap_point_width(point, (int32_t *)AT(button, 0xb8), button);
        CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &text);
    }
    at = CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&name, '&');
    if (at != -1) {
        char mnemonic = ((const char *)(uintptr_t)name)[at + 1];
        if (mnemonic != '&') D8(button, 0xc0) = (uint8_t)mnemonic;
    }
    two_lines = 0;
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&name, '\n') != -1) two_lines = 1;
    if (!instance) instance = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    PTR(button, 0xb0) = pal;
    fs98_simsui_dib_load(strip, id, instance, pal, 0);
    piece = I32(strip, 0xc) / 12;
    if (piece < 0) piece = -piece;
    width = I32(button, 0xb8);
    has_shadow = flags & 0x10;
    if (!has_shadow && !(flags & 0x20)) I32(button, 0xb4) = piece;
    else {
        I32(button, 0xb4) = piece + 7;
        I32(button, 0xb8) = width + 5;
    }
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0) = 0x28;
    I32(info, 4) = I32(button, 0xb8);
    I32(info, 8) = -piece;
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    D32(info, 0x10) = 0;
    for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(PTR(button, 0xb0), 8 + at * 4);
    I32(info, 4) = I32(button, 0xb8);
    I32(info, 8) = -I32(button, 0xb4);
    fs98_simsui_dib_create(face, info, PTR(button, 0xb0), 0);
    if (has_shadow || (flags & 0x20)) fs98_simsui_dib_create(shadow, info, PTR(button, 0xb0), 0);
    I32(info, 4) = I32(button, 0xb8);
    I32(info, 8) = -I32(button, 0xb4) * 4;
    fs98_simsui_dib_create(AT(button, 0x40), info, PTR(button, 0xb0), 0);
    mfc_operator_delete(info);
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    if (!CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0)))
        fs98_simsui_exit_cleanly();
    {
        void *bitmap = AT(button, 0x9c);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), bitmap ? D32(bitmap, 4) : 0);
    }
    old_font = CALL_THIS(MFC_5789, void *, (void *, void *))(dc, PTR(font, 4));
    CALL_STD(GetTextExtentPointA, int32_t, (uint32_t, const char *, int32_t, int32_t *))(
        D32(dc, 4), (const char *)(uintptr_t)name, I32((uintptr_t)name, -8), size);
    CALL_THIS(MFC_5789, void *, (void *, void *))(dc, old_font);
    hot_format = D16(hot_font, 0x58);
    font_format = D16(font, 0x58);
    if (two_lines) {
        D16(font, 0x58) = font_format | 0x10;
        D8(hot_font, 0x58) |= 0x10;
    }
    for (state = 0; state < 4; ++state)
        fs98_simsui_dib_copy(background, AT(button, 0x40), 0, I32(button, 0xb4) * state, I32(button, 0xb8),
                             I32(button, 0xb4), point[0], point[1]);
    for (state = 0; state < 4; ++state) {
        int32_t inner, x;
        keyed = flags & 1;
        piece = I32(strip, 8);
        if (!has_shadow && !(flags & 0x20)) inner = I32(button, 0xb8) - piece * 2;
        else inner = I32(button, 0xb8) - piece * 2 - 5;
        count = *(volatile int32_t *)&(volatile int32_t){inner / piece};    /* 0 faults, as retail's idiv */
        rest = *(volatile int32_t *)&(volatile int32_t){inner % piece};
        if (keyed) {
            unused = fs98_simsui_dib_unused_colour(strip);
            key = unused;
            fs98_simsui_dib_fill(face, (uint32_t)unused);
        } else if (flags & 6) fs98_simsui_dib_fill(face, *(uint8_t *)PTR(strip, 0x64));
        {
            int32_t height = I32(strip, 0xc) / 12;
            if (height < 0) height = -height;
            fs98_simsui_dib_copy(strip, face, 0, 0, piece, height, 0, state * height * 3);
            if (count > 0) {
                int32_t remaining = count;
                x = piece;
                do {
                    fs98_simsui_dib_copy(strip, face, x, 0, piece, height, 0, (state * 3 + 1) * height);
                    x += piece;
                } while (--remaining);
            }
            fs98_simsui_dib_copy(strip, face, (count + 1) * piece, 0, rest, height, 0, (state * 3 + 1) * height);
            fs98_simsui_dib_copy(strip, face, width - I32(strip, 8), 0, I32(strip, 8), height, 0,
                                 (state * 3 + 2) * height);
            if (has_shadow || (flags & 0x20)) {
                int32_t made = 1, mask_key = 0;
                if (keyed) { fs98_simsui_dib_fill(shadow, (uint32_t)unused); mask_key = 1; }
                else if (flags & 6) { fs98_simsui_dib_fill(shadow, 0xff); mask_key = -1; }
                else made = 0;
                if (made)
                    fs98_simsui_dib_mask(face, shadow, mask_offsets[state * 2], mask_offsets[state * 2 + 1], width,
                                         height, 0, 0, 0, mask_key, 0);
                if (has_shadow)
                    fs98_simsui_dib_blur_shadow(AT(button, 0x40), shadow, state * I32(button, 0xb4), PTR(button, 0xb0));
                else if (flags & 0x20) {
                    if (keyed)
                        fs98_simsui_dib_sprite(shadow, AT(button, 0x40), 0, state * I32(button, 0xb4), I32(shadow, 8),
                                               I32(shadow, 0xc), 0, 0, -1);
                    else
                        fs98_simsui_dib_sprite(shadow, AT(button, 0x40), 0, state * I32(button, 0xb4),
                                               I32(button, 0xb8), I32(button, 0xb4), 0, 0, -1);
                }
            }
            if (keyed)
                fs98_simsui_dib_sprite(face, AT(button, 0x40), 0, state * I32(button, 0xb4), I32(button, 0xb8),
                                       I32(button, 0xb4), 0, 0, unused);
            else if (flags & 6)
                fs98_simsui_dib_sprite(face, AT(button, 0x40), 0, state * I32(button, 0xb4), I32(button, 0xb8),
                                       I32(button, 0xb4), 0, 0, key);
            if (flags & 8)
                CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect,
                    I32(button, 0xd8), I32(button, 0xdc) + state * I32(button, 0xb4), I32(button, 0xe0),
                    I32(button, 0xe4) + state * I32(button, 0xb4));
            else if (!two_lines)
                CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect,
                    0, state * I32(button, 0xb4), width, height + state * I32(button, 0xb4));
            else {
                int32_t top = (I32(button, 0xb4) - (size[1] * 2 + 6)) / 2;
                if (top < 0) top = 0;
                CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect,
                    0, top + state * I32(button, 0xb4), width, height + state * I32(button, 0xb4));
            }
        }
        {
            struct fs98_simsui_rect area;
            void *with = font;
            int32_t drawn = 0;
            switch (state) {
            case 0: drawn = 0; break;
            case 1:
                CALL_STD(OffsetRect, int32_t, (void *, int32_t, int32_t))(rect, hot_x, hot_y);
                with = hot_font; drawn = 0;
                break;
            case 2: drawn = 1; break;
            case 3: drawn = 2; break;
            }
            area.left = rect[0]; area.top = rect[1]; area.right = rect[2]; area.bottom = rect[3];
            fs98_simsui_font_draw_text(with, dc, cstring_copy(&name), area, drawn);
        }
    }
    D16(font, 0x58) = font_format;
    D16(hot_font, 0x58) = hot_format;
    if (has_shadow || (flags & 0x20)) {
        uint32_t bytes;
        int32_t height = I32(strip, 0xc) / 12;
        if (height < 0) height = -height;
        fs98_simsui_dib_fill(shadow, 0xff);
        fs98_simsui_dib_mask(face, shadow, 0, 0, width, height, 0, 0, 0, -1, 0);
        bytes = ((D32(shadow, 8) + 3) & 0xfffffffc) * D32(shadow, 0xc);
        PTR(button, 0xe8) = mfc_operator_new(bytes);
        fs98_simsui_copy(PTR(button, 0xe8), PTR(shadow, 0x64), bytes);
    }
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), old ? D32(old, 4) : 0);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        button, point[0], point[1], I32(button, 0xb8), I32(button, 0xb4), 1);
    CALL_THIS(MFC_640, void, (void *))(dc);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    fs98_simsui_dib_destroy(strip);
    fs98_simsui_dib_destroy(shadow);
    fs98_simsui_dib_destroy(face);
    return 1;
}
/* whether a point is on the button: inside its first frame, and on its shape when it has
 * a mask (the top-left pixel always counts) */
static int32_t on_button(void *button, int32_t x, int32_t y)
{
    int32_t stride = (I32(button, 0x48) + 3) & ~3, height = I32(button, 0x4c) / 4;
    const uint8_t *mask;
    if (x < 0 || y < 0 || x >= stride || y >= height) return 0;
    mask = PTR(button, 0xe8);
    if (mask && mask[stride * y + x] && (x || y)) return 0;
    return 1;
}
/* 0x20472f30, OnLButtonDown: on the button, the mouse captured, the focus taken, pressed */
void TC fs98_simsui_3piece_button_down(void *button, uint32_t keys, int32_t x, int32_t y)
{
    (void)keys;
    if (!on_button(button, x, y)) { D32(button, 0xd4) = 0; return; }
    CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(SetCapture, uint32_t, (uint32_t))(D32(button, 0x20)));
    CALL_THIS(MFC_5981, void *, (void *))(button);
    D32(button, 0xd4) = 1;
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), BM_SETSTATE_, 1, 0);
}
/* 0x20472fe0, OnLButtonUp: released; on the button and pressed, its command posted to the
 * parent */
void TC fs98_simsui_3piece_button_up(void *button, uint32_t keys, int32_t x, int32_t y)
{
    (void)keys;
    CALL_STD(ReleaseCapture, int32_t, (void))();
    if (!OBJECT_COUNT) return;
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), BM_SETSTATE_, 0, 0);
    if (on_button(button, x, y) && D32(button, 0xd4)) {
        void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(button, 0x20)));
        uint32_t window = button ? D32(button, 0x20) : 0;
        uint32_t id = (uint32_t)CALL_THIS(MFC_3089, int32_t, (void *))(button) & 0xffff;
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x111, id, window);
        D32(button, 0xd4) = 0;
        return;
    }
    D32(button, 0xd4) = 0;
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x204730e0, OnMouseMove: pressed, shown pressed while on the button; not, the focus
 * taken on hover when asked (0x40) */
void TC fs98_simsui_3piece_mouse_move(void *button, uint32_t keys, int32_t x, int32_t y)
{
    int32_t on = on_button(button, x, y);
    (void)keys;
    if (!D32(button, 0xd4)) {
        if ((D8(button, 0xf0) & 0x40) && on &&
            CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetFocus, uint32_t, (void))()) != button)
            CALL_THIS(MFC_5981, void *, (void *))(button);
        if (!D32(button, 0xd4)) { CALL_THIS(MFC_2379, uint32_t, (void *))(button); return; }
    }
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), BM_SETSTATE_, on ? 1 : 0, 0);
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x204731c0, CreateMenu(HINSTANCE): a new popup menu, its strings from the instance */
uint32_t TC fs98_simsui_3piece_create_menu(void *button, uint32_t instance)
{
    uint32_t menu;
    if (D32(button, 0xc4)) CALL_STD(DestroyMenu, int32_t, (uint32_t))(D32(button, 0xc4));
    menu = CALL_STD(CreatePopupMenu, uint32_t, (void))();
    D32(button, 0xc4) = menu;
    D32(button, 0xc8) = instance;
    D32(button, 0xcc) = 0;
    return menu;
}
/* 0x20473200, AppendMenu(id, string, flags) */
int32_t TC fs98_simsui_3piece_append_menu(void *button, int16_t id, uint32_t string, int16_t flags)
{
    char text[0x34];
    if (!D32(button, 0xc4)) {
        D32(button, 0xc4) = CALL_STD(CreatePopupMenu, uint32_t, (void))();
        if (!D32(button, 0xc4)) return 0;
    }
    ++D32(button, 0xcc);
    CALL_STD(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(D32(button, 0xc8), string, text, 0x31);
    return CALL_STD(AppendMenuA, int32_t, (uint32_t, uint32_t, uint32_t, const char *))(
        D32(button, 0xc4), (uint32_t)(int32_t)flags, (uint32_t)(int32_t)id, text);
}
/* 0x20473280, CheckMenuItem(item, check) */
uint32_t TC fs98_simsui_3piece_check_menu(void *button, int16_t item, int16_t check)
{
    return CALL_STD(CheckMenuItem, uint32_t, (uint32_t, uint32_t, uint32_t))(D32(button, 0xc4), (uint32_t)(int32_t)item,
                                                                            (uint32_t)(int32_t)check);
}
/* 0x204732a0, DeleteMenu */
void TC fs98_simsui_3piece_delete_menu(void *button)
{
    if (D32(button, 0xc4)) CALL_STD(DestroyMenu, int32_t, (uint32_t))(D32(button, 0xc4));
    D32(button, 0xc4) = 0;
    D32(button, 0xcc) = 0;
}
/* 0x204732d0, TrackMenu: the menu below the button's left */
void TC fs98_simsui_3piece_track_menu(void *button)
{
    int32_t rect[4];
    uint32_t parent;
    CALL_STD(GetWindowRect, int32_t, (uint32_t, int32_t *))(D32(button, 0x20), rect);
    parent = CALL_STD(GetParent, uint32_t, (uint32_t))(D32(button, 0x20));
    CALL_STD(TrackPopupMenu, int32_t, (uint32_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, const void *))(
        D32(button, 0xc4), 0, rect[0] + 0xd, rect[3] - 5, 0, parent, 0);
}
/* 0x20473320, OnKillFocus */
void TC fs98_simsui_3piece_kill_focus(void *button, void *window)
{
    (void)window;
    if (D32(button, 0xd4)) {
        CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), BM_SETSTATE_, 0, 0);
        D32(button, 0xd4) = 0;
    }
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x20473360, OnGetDlgCode: CWnd::Default's */
uint32_t TC fs98_simsui_3piece_dialog_code(void *button)
{
    return CALL_THIS(MFC_2379, uint32_t, (void *))(button);
}
/* 0x20473370: a three-argument handler that only passes the message on */
uint32_t TC fs98_simsui_default_3(void *window, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    return CALL_THIS(MFC_2379, uint32_t, (void *))(window);
}

/* CSimsHelpButton and CSimsExitButton (0xc4 bytes): a CSimsButton from strip 0x516 / 0x515
 * of the main DLL, and the window that had the focus (+0xc0), given it back after a click */
#define VTABLE_HELP 0x15648
#define VTABLE_EXIT 0x15710
#define SMALL_BUTTON_SIZE 0xc4
#define MAIN_INSTANCE G32(0x213d0)
/* 0x20473380 */
void *TC fs98_simsui_help_construct(void *button)
{
    fs98_simsui_button_construct(button);
    PTR(button, 0) = IMAGE(VTABLE_HELP);
    D32(button, 0xc0) = 0;
    return button;
}
/* 0x20473400 */
void TC fs98_simsui_help_destroy(void *button)
{
    PTR(button, 0) = IMAGE(VTABLE_HELP);
    fs98_simsui_button_destroy(button);
}
/* 0x204733a0 */
void *TC fs98_simsui_help_delete(void *button, uint32_t flags)
{
    return fs98_simsui_delete_object(button, flags, SMALL_BUTTON_SIZE, fs98_simsui_help_destroy, fs98_simsui_ui_delete);
}
/* 0x20473410 */
const void *TC fs98_simsui_help_message_map(const void *button) { (void)button; return IMAGE(0x15540); }
/* 0x20473420, CSimsText::OnNcHitTest (and others'): HTCLIENT */
uint32_t TC fs98_simsui_hit_client(void *window, int32_t x, int32_t y) { (void)window; (void)x; (void)y; return 1; }
/* Init(CDIBPal *, CPoint) of both: the strip, then the window at the point */
static int32_t small_button_init(void *button, int32_t id, void *pal, int32_t x, int32_t y)
{
    int32_t result = fs98_simsui_button_init(button, id, pal, MAIN_INSTANCE);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        button, x, y, I32(button, 0xb8), I32(button, 0xb4), 1);
    return result;
}
/* 0x20473430 */
int32_t TC fs98_simsui_help_init(void *button, void *pal, int32_t x, int32_t y)
{
    return small_button_init(button, 0x516, pal, x, y);
}
/* 0x20473480, OnLButtonUp: the focus given back */
void TC fs98_simsui_help_button_up(void *button, uint32_t keys, int32_t x, int32_t y)
{
    void *window;
    (void)keys; (void)x; (void)y;
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
    window = PTR(button, 0xc0);
    if (window && D32(window, 0x20)) CALL_THIS(MFC_5981, void *, (void *))(window);
}
/* 0x204734b0 */
void *TC fs98_simsui_exit_construct(void *button)
{
    fs98_simsui_button_construct(button);
    PTR(button, 0) = IMAGE(VTABLE_EXIT);
    D32(button, 0xc0) = 0;
    return button;
}
/* 0x20473530 */
void TC fs98_simsui_exit_destroy(void *button)
{
    PTR(button, 0) = IMAGE(VTABLE_EXIT);
    fs98_simsui_button_destroy(button);
}
/* 0x204734d0 */
void *TC fs98_simsui_exit_delete(void *button, uint32_t flags)
{
    return fs98_simsui_delete_object(button, flags, SMALL_BUTTON_SIZE, fs98_simsui_exit_destroy, fs98_simsui_ui_delete);
}
/* 0x20473540 */
const void *TC fs98_simsui_exit_message_map(const void *button) { (void)button; return IMAGE(0x155a8); }
/* 0x20473550 */
int32_t TC fs98_simsui_exit_init(void *button, void *pal, int32_t x, int32_t y)
{
    return small_button_init(button, 0x515, pal, x, y);
}
/* 0x204735a0, OnLButtonUp: the focus given back, while the window lives, and forgotten */
void TC fs98_simsui_exit_button_up(void *button, uint32_t keys, int32_t x, int32_t y)
{
    void *window;
    (void)keys; (void)x; (void)y;
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
    if (!CALL_STD(IsWindow, int32_t, (uint32_t))(D32(button, 0x20))) return;
    window = PTR(button, 0xc0);
    if (window && D32(window, 0x20)) CALL_THIS(MFC_5981, void *, (void *))(window);
    D32(button, 0xc0) = 0;
}
/* 0x204735e0, OnSetFocus: the window that had it kept */
void TC fs98_simsui_small_button_set_focus(void *button, void *previous)
{
    CALL_THIS(MFC_2379, uint32_t, (void *))(button);
    PTR(button, 0xc0) = previous;
}
