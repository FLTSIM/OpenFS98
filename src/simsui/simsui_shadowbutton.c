/* CSimsShadowButton (0xf4 bytes, vtable 0x16b98): a CSimsButton whose four frames are built
 * at Init from a strip of four (one per state) over the dialog's background, with its
 * text drawn on, and optionally a shadow and a mask of its shape (+0xe0) for hit tests.
 *
 * Beyond CSimsButton: its window +0xc0 and id +0xc4, kept for the command it posts;
 * pressed +0xcc; the text's rectangle when flags & 8 +0xd0; the mask +0xe0; +0xe4; flags
 * +0xe8 (as CSims3PieceButton's); the fonts, normal +0xec and hot +0xf0. */
#include "simsui_internal.h"

#define VTABLE_SHADOW_BUTTON 0x16b98
#define SHADOW_BUTTON_SIZE 0xf4
#define OBJECT_COUNT I32(fs98_simsui_image, 0x213cc)
#define BM_SETSTATE_ 0xf3

/* 0x2047c830 */
void *TC fs98_simsui_shadow_button_construct(void *button)
{
    fs98_simsui_button_construct(button);
    PTR(button, 0) = IMAGE(VTABLE_SHADOW_BUTTON);
    D32(button, 0xcc) = 0;
    D32(button, 0xe0) = 0;
    D32(button, 0xe4) = 0;
    return button;
}
/* 0x2047c8c0 */
void TC fs98_simsui_shadow_button_destroy(void *button)
{
    PTR(button, 0) = IMAGE(VTABLE_SHADOW_BUTTON);
    if (D32(button, 0xe0)) mfc_operator_delete(PTR(button, 0xe0));
    D32(button, 0xe0) = 0;
    fs98_simsui_button_destroy(button);
}
/* 0x2047c860 */
void *TC fs98_simsui_shadow_button_delete(void *button, uint32_t flags)
{
    return fs98_simsui_delete_object(button, flags, SHADOW_BUTTON_SIZE, fs98_simsui_shadow_button_destroy,
                                     fs98_simsui_ui_delete);
}
/* 0x2047c930 */
const void *TC fs98_simsui_shadow_button_message_map(const void *button) { (void)button; return IMAGE(0x16b30); }
/* 0x2047c940, Init(font, hot font, strip id, background, CDIBPal *, flags, key, HINSTANCE,
 * hot x, hot y, CRect text): the window's text is "name#x,y" (else where it is,
 * remapped); a frame is the strip's width plus 5 by a quarter of its height plus 7 */
int32_t TC fs98_simsui_shadow_button_init(void *button, void *font, void *hot_font, int32_t id, void *background,
                                          void *pal, uint32_t flags, int32_t key, uint32_t instance, int32_t hot_x,
                                          int32_t hot_y, struct fs98_simsui_rect text_rect)
{
    uint8_t face[0x70], shadow[0x70], strip[0x70], dc[0x10], *info;
    static const int32_t mask_offsets[8] = {4, 6, 3, 4, 4, 6, 4, 6};
    uint32_t text, name, has_shadow, keyed;
    int32_t point[2], rect[4], piece, width, state, unused = 0, at;
    void *old;
    D32(button, 0xc4) = (uint32_t)CALL_THIS(MFC_3089, int32_t, (void *))(button);
    D32(button, 0xc0) = button ? D32(button, 0x20) : 0;
    PTR(button, 0xec) = font;
    PTR(button, 0xf0) = hot_font;
    I32(button, 0xd0) = text_rect.left;
    I32(button, 0xd4) = text_rect.top;
    I32(button, 0xd8) = text_rect.right;
    I32(button, 0xdc) = text_rect.bottom;
    fs98_simsui_dib_construct(face);
    fs98_simsui_dib_construct(shadow);
    fs98_simsui_dib_construct(strip);
    D32(button, 0xe8) = flags;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_3874, void, (void *, void *))(button, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_point(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, point);
    else {
        fs98_simsui_remap_point(point, button);
        CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &text);
    }
    if (!instance) instance = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    PTR(button, 0xb0) = pal;
    fs98_simsui_dib_load(strip, id, instance, pal, 0);
    I32(button, 0xb8) = I32(strip, 8);
    width = I32(strip, 8);
    piece = I32(strip, 0xc) / 4;
    if (piece < 0) piece = -piece;
    I32(button, 0xb8) = width + 5;
    I32(button, 0xb4) = piece + 7;
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0) = 0x28;
    D16(info, 0xc) = 1;
    I32(info, 4) = I32(button, 0xb8);
    I32(info, 8) = -piece;
    D16(info, 0xe) = 8;
    D32(info, 0x10) = 0;
    for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(PTR(button, 0xb0), 8 + at * 4);
    I32(info, 4) = I32(button, 0xb8);
    I32(info, 8) = -I32(button, 0xb4);
    fs98_simsui_dib_create(face, info, PTR(button, 0xb0), 0);
    has_shadow = flags & 0x10;
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
    for (state = 0; state < 4; ++state)
        fs98_simsui_dib_copy(background, AT(button, 0x40), 0, state * I32(button, 0xb4), I32(button, 0xb8),
                             I32(button, 0xb4), point[0], point[1]);
    keyed = flags & 1;
    for (state = 0; state < 4; ++state) {
        if (keyed) {
            unused = fs98_simsui_dib_unused_colour(strip);
            key = unused;
            fs98_simsui_dib_fill(face, (uint32_t)unused);
        } else if (flags & 6) fs98_simsui_dib_fill(face, *(uint8_t *)PTR(strip, 0x64));
        fs98_simsui_dib_copy(strip, face, 0, 0, width, piece, 0, state * piece);
        if (has_shadow || (flags & 0x20)) {
            int32_t made = 1, mask_key = 0;
            if (keyed) { fs98_simsui_dib_fill(shadow, (uint32_t)unused); mask_key = 1; }
            else if (flags & 6) { fs98_simsui_dib_fill(shadow, 0xff); mask_key = -1; }
            else made = 0;
            if (made)
                fs98_simsui_dib_mask(face, shadow, mask_offsets[state * 2], mask_offsets[state * 2 + 1], width, piece, 0,
                                     0, 0, mask_key, 0);
            if (has_shadow)
                fs98_simsui_dib_blur_shadow(AT(button, 0x40), shadow, state * I32(button, 0xb4), PTR(button, 0xb0));
            else if (flags & 0x20) {
                if (keyed)
                    fs98_simsui_dib_sprite(shadow, AT(button, 0x40), 0, state * I32(button, 0xb4), I32(shadow, 8),
                                           I32(shadow, 0xc), 0, 0, -1);
                else
                    fs98_simsui_dib_sprite(shadow, AT(button, 0x40), 0, state * I32(button, 0xb4), I32(button, 0xb8),
                                           I32(button, 0xb4), 0, 0, -1);
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
                I32(button, 0xd0), I32(button, 0xd4) + state * I32(button, 0xb4), I32(button, 0xd8),
                I32(button, 0xdc) + state * I32(button, 0xb4));
        else
            CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect,
                0, state * I32(button, 0xb4), width, piece + state * I32(button, 0xb4));
        {
            struct fs98_simsui_rect area;
            void *with = font;
            int32_t drawn = 0;
            uint32_t copy;
            if (state == 1) {
                CALL_STD(OffsetRect, int32_t, (void *, int32_t, int32_t))(rect, hot_x, hot_y);
                with = hot_font;
            } else if (state == 2) drawn = 1;
            else if (state == 3) drawn = 2;
            area.left = rect[0]; area.top = rect[1]; area.right = rect[2]; area.bottom = rect[3];
            CALL_THIS(MFC_535, void *, (void *, const void *))(&copy, &name);
            fs98_simsui_font_draw_text(state == 1 ? PTR(button, 0xf0) : PTR(button, 0xec), dc, copy, area, drawn);
            (void)with;
        }
    }
    if (has_shadow || (flags & 0x20)) {
        uint32_t bytes;
        fs98_simsui_dib_fill(shadow, 0xff);
        fs98_simsui_dib_mask(face, shadow, 0, 0, width, piece, 0, 0, 0, -1, 0);
        bytes = ((D32(shadow, 8) + 3) & 0xfffffffc) * D32(shadow, 0xc);
        PTR(button, 0xe0) = mfc_operator_new(bytes);
        fs98_simsui_copy(PTR(button, 0xe0), PTR(shadow, 0x64), bytes);
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
    mask = PTR(button, 0xe0);
    if (mask && mask[stride * y + x] && (x || y)) return 0;
    return 1;
}
/* 0x2047d150, OnLButtonDown: on the button, the mouse captured, the focus taken, pressed */
void TC fs98_simsui_shadow_button_down(void *button, uint32_t keys, int32_t x, int32_t y)
{
    (void)keys;
    if (!on_button(button, x, y)) { D32(button, 0xcc) = 0; return; }
    CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(SetCapture, uint32_t, (uint32_t))(D32(button, 0x20)));
    CALL_THIS(MFC_5981, void *, (void *))(button);
    D32(button, 0xcc) = 1;
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), BM_SETSTATE_, 1, 0);
}
/* 0x2047d200, OnLButtonUp: released; on the button and pressed, its command posted */
void TC fs98_simsui_shadow_button_up(void *button, uint32_t keys, int32_t x, int32_t y)
{
    (void)keys;
    if (!OBJECT_COUNT) return;
    CALL_STD(ReleaseCapture, int32_t, (void))();
    CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), BM_SETSTATE_, 0, 0);
    if (on_button(button, x, y) && D32(button, 0xcc)) {
        void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(button, 0x20)));
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x111,
                                                                                 D16(button, 0xc4), D32(button, 0xc0));
    }
    D32(button, 0xcc) = 0;
}
/* 0x2047d2d0, OnMouseMove: only while the parent is active: the focus taken on hover when
 * asked (0x40), and pressed shown while on the button */
void TC fs98_simsui_shadow_button_move(void *button, uint32_t keys, int32_t x, int32_t y)
{
    int32_t on;
    void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(button, 0x20)));
    (void)keys;
    if (parent != CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetActiveWindow, uint32_t, (void))())) return;
    on = on_button(button, x, y);
    if ((D8(button, 0xe8) & 0x40) && on &&
        CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetFocus, uint32_t, (void))()) != button)
        CALL_THIS(MFC_5981, void *, (void *))(button);
    if (D32(button, 0xcc))
        CALL_STD(SendMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(button, 0x20), BM_SETSTATE_,
                                                                                 on ? 1 : 0, 0);
}
