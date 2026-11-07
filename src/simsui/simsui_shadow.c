/* CSimsShadow (0x88 bytes, vtable 0x16c60): a CObject holding an off-screen picture of a
 * bitmap cast onto a piece of the dialog's background, with a shadow, drawn at will.
 *
 * Its CDIBSection +4 (CBitmap +0x60); where it sits +0x74/+0x78 (remapped); its size
 * +0x7c/+0x80 (the bitmap's, grown by the shadow's offset); the CDIBPal +0x84 (none: not
 * made). */
#include "simsui_internal.h"

#define VTABLE_SHADOW 0x16c60
#define VTABLE_OBJECT 0x15018
#define SHADOW_SIZE 0x88

/* 0x2047d3c0 */
void *TC fs98_simsui_shadow_construct(void *shadow)
{
    PTR(shadow, 0) = IMAGE(VTABLE_OBJECT);
    fs98_simsui_dib_construct(AT(shadow, 4));
    PTR(shadow, 0) = IMAGE(VTABLE_SHADOW);
    D32(shadow, 0x84) = 0;
    return shadow;
}
/* 0x2047d480 */
void TC fs98_simsui_shadow_destroy(void *shadow)
{
    PTR(shadow, 0) = IMAGE(VTABLE_SHADOW);
    fs98_simsui_dib_destroy(AT(shadow, 4));
    PTR(shadow, 0) = IMAGE(VTABLE_OBJECT);
}
/* 0x2047d420 */
void *TC fs98_simsui_shadow_delete(void *shadow, uint32_t flags)
{
    return fs98_simsui_delete_object(shadow, flags, SHADOW_SIZE, fs98_simsui_shadow_destroy, fs98_simsui_ui_delete);
}
/* 0x2047d4d0, Init(id, background, CDIBPal *, HINSTANCE, x, y, flags, shadow x, shadow y):
 * the bitmap at (x, y) (remapped) over the background, its shadow cast (shadow x, shadow y)
 * away, blurred (flags & 0x10) or hard (0x20); its transparent colour the first pixel
 * (flags & 4) or one it does not use */
void TC fs98_simsui_shadow_init(void *shadow, int32_t id, void *background, void *pal, uint32_t instance, int32_t x,
                                int32_t y, uint32_t flags, int32_t shadow_x, int32_t shadow_y)
{
    uint8_t face[0x70], strip[0x70], cast[0x70], dc[0x10], *info;
    int32_t point[2], width, height, key = -1, at;
    uint32_t first_pixel;
    void *old;
    fs98_simsui_dib_construct(face);
    fs98_simsui_dib_construct(strip);
    fs98_simsui_dib_construct(cast);
    I32(shadow, 0x74) = x;
    point[0] = x;
    I32(shadow, 0x78) = y;
    D32(shadow, 0x84) = (uint32_t)(uintptr_t)pal;
    point[1] = y;
    fs98_simsui_remap_point(point, 0);
    I32(shadow, 0x74) = point[0];
    I32(shadow, 0x78) = point[1];
    if (!instance) instance = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    fs98_simsui_dib_load(strip, id, instance, pal, 0);
    height = I32(strip, 0xc);
    if (height < 0) height = -height;
    width = I32(strip, 8);
    I32(shadow, 0x80) = height + shadow_y;
    I32(shadow, 0x7c) = width + shadow_x;
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0) = 0x28;
    I32(info, 4) = I32(shadow, 0x7c);
    I32(info, 8) = -I32(shadow, 0x80);
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    D32(info, 0x10) = 0;
    for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(PTR(shadow, 0x84), 8 + at * 4);
    I32(info, 4) = I32(shadow, 0x7c);
    I32(info, 8) = -I32(shadow, 0x80);
    fs98_simsui_dib_create(face, info, PTR(shadow, 0x84), 0);
    I32(info, 4) = I32(shadow, 0x7c);
    I32(info, 8) = -I32(shadow, 0x80);
    fs98_simsui_dib_create(cast, info, PTR(shadow, 0x84), 0);
    I32(info, 4) = I32(shadow, 0x7c);
    I32(info, 8) = -I32(shadow, 0x80);
    fs98_simsui_dib_create(AT(shadow, 4), info, PTR(shadow, 0x84), 0);
    mfc_operator_delete(info);
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    if (!CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0)))
        fs98_simsui_exit_cleanly();
    {
        void *bitmap = AT(shadow, 0x60);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), bitmap ? D32(bitmap, 4) : 0);
    }
    fs98_simsui_dib_copy(background, AT(shadow, 4), 0, 0, I32(shadow, 0x7c), I32(shadow, 0x80), I32(shadow, 0x74),
                         I32(shadow, 0x78));
    first_pixel = flags & 4;
    if (first_pixel) {
        fs98_simsui_dib_fill(face, *(uint8_t *)PTR(strip, 0x64));
        fs98_simsui_dib_copy(strip, face, 0, 0, width, height, 0, 0);
        fs98_simsui_dib_fill(cast, 0xff);
        fs98_simsui_dib_mask(face, cast, shadow_x, shadow_y, width, height, 0, 0, 0, -1, 0);
    } else {
        int32_t unused = fs98_simsui_dib_unused_colour(strip);
        key = unused;
        fs98_simsui_dib_fill(face, (uint32_t)unused);
        fs98_simsui_dib_copy(strip, face, 0, 0, width, height, 0, 0);
        fs98_simsui_dib_fill(cast, (uint32_t)unused);
        fs98_simsui_dib_mask(face, cast, shadow_x, shadow_y, width, height, 0, 0, 0, 1, 0);
    }
    if (flags & 0x10) fs98_simsui_dib_blur_shadow(AT(shadow, 4), cast, 0, PTR(shadow, 0x84));
    else if (flags & 0x20) {
        if (!first_pixel)
            fs98_simsui_dib_sprite(cast, AT(shadow, 4), 0, 0, I32(cast, 8), I32(cast, 0xc), 0, 0, -1);
        else
            fs98_simsui_dib_sprite(cast, AT(shadow, 4), 0, 0, I32(shadow, 0x7c), I32(shadow, 0x80), 0, 0, -1);
    }
    fs98_simsui_dib_sprite(face, AT(shadow, 4), 0, 0, I32(shadow, 0x7c), I32(shadow, 0x80), 0, 0, key);
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), old ? D32(old, 4) : 0);
    CALL_THIS(MFC_640, void, (void *))(dc);
    fs98_simsui_dib_destroy(cast);
    fs98_simsui_dib_destroy(strip);
    fs98_simsui_dib_destroy(face);
}
/* 0x2047d940, Draw(CDC *, x, y): the picture copied to the DC, where it sits (x -1) or at
 * (x, y); BitBlt's answer, or 0 when there is none */
int32_t TC fs98_simsui_shadow_draw(void *shadow, void *dc, int32_t x, int32_t y)
{
    uint8_t memory[0x10];
    void *old_pal, *old, *bitmap;
    int32_t result;
    CALL_THIS(MFC_323, void *, (void *))(memory);
    if (!D32(shadow, 0x84) ||
        !CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(dc ? D32(dc, 4) : 0))) {
        CALL_THIS(MFC_640, void, (void *))(memory);
        return 0;
    }
    old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(shadow, 0x84), 0);
    CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
    bitmap = AT(shadow, 0x60);
    old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
    if (x == -1)
        result = CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(dc, 4), I32(shadow, 0x74), I32(shadow, 0x78), I32(shadow, 0x7c), I32(shadow, 0x80), D32(memory, 4), 0, 0,
            0xcc0020);
    else
        result = CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, int32_t, int32_t, uint32_t))(
            D32(dc, 4), x, y, I32(shadow, 0x7c), I32(shadow, 0x80), D32(memory, 4), 0, 0, 0xcc0020);
    if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 0);
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
    CALL_THIS(MFC_640, void, (void *))(memory);
    return result;
}
/* 0x2047dab0, GetOSBuffer: the section, once made */
void *TC fs98_simsui_shadow_buffer(void *shadow)
{
    return D32(shadow, 0x84) ? AT(shadow, 4) : 0;
}
