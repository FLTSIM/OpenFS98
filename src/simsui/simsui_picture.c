/* CSimsPicture (0xd0 bytes, vtable 0x167d0): a CStatic showing a bitmap over (or instead of)
 * the dialog's background, which reports keys and focus to its parent; and CSimsPictureView
 * (0x40 bytes, vtable 0x168b0), a bare CWnd.
 *
 * The picture: the CStatic (CWnd) to +0x40; the CDIBPal +0x40; the image's height +0x44
 * and width +0x48; its CDIBSection +0x4c (CBitmap +0xa8); no image (the window paints
 * itself) +0xbc; the palette state given to the bits +0xc0; the last key's character
 * +0xc4, repeat count +0xc8 and flags +0xcc. */
#include "simsui_internal.h"

#define VTABLE_STATIC_BASE 0x16328
#define VTABLE_PICTURE 0x167d0
#define VTABLE_VIEW 0x168b0
#define PICTURE_SIZE 0xd0
#define OPAQUE_KEY 0xf100                    /* the image as it is, no background */
#define FIRST_PIXEL_KEY 0xf200               /* the image's first pixel transparent */

/* 0x2047b140 */
void *TC fs98_simsui_picture_construct(void *picture)
{
    mfc_wnd_construct(picture);
    PTR(picture, 0) = IMAGE(VTABLE_STATIC_BASE);
    fs98_simsui_dib_construct(AT(picture, 0x4c));
    D32(picture, 0xbc) = 0;
    D32(picture, 0xc4) = 0;
    D32(picture, 0xc8) = 0;
    D32(picture, 0xcc) = 0;
    PTR(picture, 0) = IMAGE(VTABLE_PICTURE);
    return picture;
}
/* 0x2047b210 */
void TC fs98_simsui_picture_destroy(void *picture)
{
    PTR(picture, 0) = IMAGE(VTABLE_PICTURE);
    fs98_simsui_dib_destroy(AT(picture, 0x4c));
    CALL_THIS(MFC_795, void, (void *))(picture);
}
/* 0x2047b1b0 */
void *TC fs98_simsui_picture_delete(void *picture, uint32_t flags)
{
    return fs98_simsui_delete_object(picture, flags, PICTURE_SIZE, fs98_simsui_picture_destroy, fs98_simsui_ui_delete);
}
/* the image `id` made into the picture's section at (x, y) of the background: the strip
 * loaded, a section its size made, the background behind it (unless the key is
 * OPAQUE_KEY), and the strip copied or sprited over with the key; the window sized */
static void make_image(void *picture, uint8_t *strip, int32_t id, void *background, void *pal, uint32_t instance,
                       uint16_t key, int32_t *rect)
{
    uint8_t *info;
    int32_t at;
    PTR(picture, 0x40) = pal;
    fs98_simsui_dib_load(strip, id, instance, pal, 0);
    I32(picture, 0x44) = I32(strip, 0xc);
    I32(picture, 0x48) = I32(strip, 8);
    if (I32(picture, 0x44) < 0) I32(picture, 0x44) = -I32(picture, 0x44);
    rect[2] = rect[0] + I32(picture, 0x48);
    rect[3] = rect[1] + I32(picture, 0x44);
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0) = 0x28;
    I32(info, 4) = I32(picture, 0x48);
    I32(info, 8) = -I32(picture, 0x44);
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    D32(info, 0x10) = 0;
    for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(PTR(picture, 0x40), 8 + at * 4);
    I32(info, 4) = I32(picture, 0x48);
    I32(info, 8) = -I32(picture, 0x44);
    fs98_simsui_dib_create(AT(picture, 0x4c), info, PTR(picture, 0x40), 0);
    if (key != OPAQUE_KEY)
        fs98_simsui_dib_copy(background, AT(picture, 0x4c), 0, 0, I32(picture, 0x48), I32(picture, 0x44), rect[0], rect[1]);
    if (key == OPAQUE_KEY)
        fs98_simsui_dib_copy(strip, AT(picture, 0x4c), 0, 0, I32(picture, 0x48), I32(picture, 0x44), 0, 0);
    else
        fs98_simsui_dib_sprite(strip, AT(picture, 0x4c), 0, 0, I32(picture, 0x48), I32(picture, 0x44), 0, 0,
                               key == FIRST_PIXEL_KEY ? -1 : (int32_t)key);
    mfc_operator_delete(info);
    CALL_THIS(MFC_4284, int32_t, (void *, uint32_t, uint32_t, uint32_t))(picture, 0x200, 0xe, 0);
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        picture, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
}
/* 0x2047b260, SetImage(id, background, CDIBPal *, HINSTANCE, key): a new image where the
 * window is */
void TC fs98_simsui_picture_set_image(void *picture, int32_t id, void *background, void *pal, uint32_t instance,
                                      uint16_t key)
{
    uint8_t strip[0x70];
    int32_t rect[4];
    void *parent;
    if (!instance) instance = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    fs98_simsui_dib_construct(strip);
    CALL_STD(GetWindowRect, int32_t, (uint32_t, int32_t *))(D32(picture, 0x20), rect);
    parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(picture, 0x20)));
    CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect);
    CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect + 2);
    make_image(picture, strip, id, background, pal, instance, key, rect);
    fs98_simsui_dib_destroy(strip);
}
/* 0x2047b4a0, Init(id, background, CDIBPal *, HINSTANCE, key): placed by its text's spec
 * "name#l,t,r,b" (else where it is, remapped); with no id, background, palette or instance
 * there is no image and the window paints itself */
int32_t TC fs98_simsui_picture_init(void *picture, int32_t id, void *background, void *pal, uint32_t instance,
                                    uint16_t key)
{
    uint8_t strip[0x70];
    uint32_t text, name;
    int32_t rect[4];
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    fs98_simsui_dib_construct(strip);
    CALL_THIS(MFC_3874, void, (void *, void *))(picture, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_rect(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, rect);
    else fs98_simsui_remap_rect(rect, picture);
    if (!id && !background && !pal && !instance) {
        D32(picture, 0xbc) = 1;
        CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
            picture, rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1], 1);
    } else {
        if (!instance) instance = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
        make_image(picture, strip, id, background, pal, instance, key, rect);
    }
    fs98_simsui_dib_destroy(strip);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return 1;
}
/* 0x2047b760 */
const void *TC fs98_simsui_picture_message_map(const void *picture) { (void)picture; return IMAGE(0x16720); }
/* a notification to the parent: WM_COMMAND with the code in the high word */
static void notify_parent(void *picture, uint32_t code)
{
    uint32_t window = picture ? D32(picture, 0x20) : 0;
    void *parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(picture, 0x20)));
    uint32_t id = (uint32_t)CALL_THIS(MFC_3089, int32_t, (void *))(picture) & 0xffff;
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(parent, 0x20), 0x111, id | code, window);
    CALL_THIS(MFC_2379, uint32_t, (void *))(picture);
}
/* 0x2047b770, OnChar; 0x2047b7e0, OnKeyDown: the key kept and told (0x7e8, 0x7e9) */
void TC fs98_simsui_picture_on_char(void *picture, uint32_t key, uint32_t repeat, uint32_t flags)
{
    D32(picture, 0xc4) = key;
    D32(picture, 0xc8) = repeat;
    D32(picture, 0xcc) = flags;
    notify_parent(picture, 0x7e80000);
}
void TC fs98_simsui_picture_key_down(void *picture, uint32_t key, uint32_t repeat, uint32_t flags)
{
    D32(picture, 0xc4) = key;
    D32(picture, 0xc8) = repeat;
    D32(picture, 0xcc) = flags;
    notify_parent(picture, 0x7e90000);
}
/* 0x2047b850, OnSetFocus; 0x2047b8b0, OnKillFocus: told (1, 2) */
void TC fs98_simsui_picture_set_focus(void *picture, void *previous) { (void)previous; notify_parent(picture, 0x1000000); }
void TC fs98_simsui_picture_kill_focus(void *picture, void *next) { (void)next; notify_parent(picture, 0x2000000); }
/* 0x2047b910, OnEraseBkgnd: nothing to erase under an image */
int32_t TC fs98_simsui_picture_erase(void *picture, void *dc)
{
    (void)dc;
    if (D32(picture, 0xbc)) return (int32_t)CALL_THIS(MFC_2379, uint32_t, (void *))(picture);
    return 1;
}
/* 0x2047b930, OnPaint: the image copied in through the palette */
void TC fs98_simsui_picture_paint(void *picture)
{
    uint8_t dc[0x60], memory[0x10];
    void *pal, *bitmap;
    if (D32(picture, 0xbc)) {
        CALL_THIS(MFC_2379, uint32_t, (void *))(picture);
        return;
    }
    CALL_THIS(MFC_470, void *, (void *, void *))(dc, picture);
    CALL_THIS(MFC_323, void *, (void *))(memory);
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(D32(dc, 4)))) {
        CALL_THIS(MFC_5736, uint32_t, (void *))(dc);
        CALL_THIS(MFC_5736, uint32_t, (void *))(memory);
        bitmap = AT(picture, 0xa8);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(picture, 0x40), 0);
        CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(memory, PTR(picture, 0x40), 0);
        pal = PTR(picture, 0x40);
        if (D32(picture, 0xc0) != D32(pal, 0x808)) {
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
            D32(picture, 0xc0) = D32(PTR(picture, 0x40), 0x808);
        }
        CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(dc, 4), 0, 0, I32(picture, 0x48), I32(picture, 0x44), D32(memory, 4), 0, 0, 0xcc0020);
        CALL_THIS(MFC_5678, uint32_t, (void *, int32_t))(dc, -1);
        CALL_THIS(MFC_5678, uint32_t, (void *, int32_t))(memory, -1);
    }
    CALL_THIS(MFC_640, void, (void *))(memory);
    CALL_THIS(MFC_755, void, (void *))(dc);
}

/* 0x2047bb30 */
void *TC fs98_simsui_view_construct(void *view)
{
    mfc_wnd_construct(view);
    PTR(view, 0) = IMAGE(VTABLE_VIEW);
    return view;
}
/* 0x2047bba0 */
void TC fs98_simsui_view_destroy(void *view)
{
    PTR(view, 0) = IMAGE(VTABLE_VIEW);
    CALL_THIS(MFC_818, void, (void *))(view);
}
/* 0x2047bb50 */
void *TC fs98_simsui_view_delete(void *view, uint32_t flags)
{
    return fs98_simsui_delete_object(view, flags, 0x40, fs98_simsui_view_destroy, fs98_simsui_ui_delete);
}
/* 0x2047bbb0 */
const void *TC fs98_simsui_view_message_map(const void *view) { (void)view; return IMAGE(0x16890); }
