/* CSimsTextArt (0xe8 bytes, vtable 0x174c0): a CStatic showing a string resource drawn once
 * into a section over the dialog's background, in a font from a font spec, with a blurred
 * shadow (flags & 1), a plain face (2), an outline (4), and a vertical gradient (8) or a
 * flat colour.
 *
 * The CStatic (CWnd) to +0x40; the shadow's offset +0x40/+0x44; the outline colour +0x48,
 * the flat colour +0x4c, the face colour +0x50; its rectangle +0x54; the CDIBSection +0x64
 * (CBitmap +0xc0, bits +0xc8); a CFont +0xd4; the CDIBPal +0xdc; flags +0xe4. */
#include "simsui_internal.h"

#define VTABLE_STATIC_BASE 0x16328
#define VTABLE_TEXT_ART 0x174c0
#define VTABLE_FONT 0x152a8
#define GRADIENT 0x20b08                     /* thirty colours, a dword each */
#define GRADIENT_STEPS 0x174b8               /* 29.0 */

/* 0x20481bd0 */
void *TC fs98_simsui_text_art_construct(void *art)
{
    mfc_wnd_construct(art);
    PTR(art, 0) = IMAGE(VTABLE_STATIC_BASE);
    D32(art, 0x40) = 2;
    D32(art, 0x44) = 2;
    D32(art, 0x48) = 0x202020;
    D32(art, 0x4c) = 0xcea262;
    D32(art, 0x50) = 0;
    fs98_simsui_dib_construct(AT(art, 0x64));
    D32(art, 0xd8) = 0;
    PTR(art, 0xd4) = IMAGE(0x16090);
    D32(art, 0xdc) = 0;
    PTR(art, 0) = IMAGE(VTABLE_TEXT_ART);
    return art;
}
/* 0x20481cc0 */
void TC fs98_simsui_text_art_destroy(void *art)
{
    PTR(art, 0) = IMAGE(VTABLE_TEXT_ART);
    PTR(art, 0xd4) = IMAGE(0x15030);
    mfc_gdi_delete_object(AT(art, 0xd4));
    PTR(art, 0xd4) = IMAGE(0x15018);
    fs98_simsui_dib_destroy(AT(art, 0x64));
    CALL_THIS(MFC_795, void, (void *))(art);
}
/* 0x20481c60 */
void *TC fs98_simsui_text_art_delete(void *art, uint32_t flags)
{
    return fs98_simsui_delete_object(art, flags, 0xe8, fs98_simsui_text_art_destroy, fs98_simsui_ui_delete);
}
/* 0x204824c0 */
const void *TC fs98_simsui_text_art_message_map(const void *art) { (void)art; return IMAGE(0x17480); }
/* MSVCRT's _ftol, which takes its value in st(0) */
static int32_t call_ftol(long double value)
{
    int32_t result;
    __asm__ volatile("fldt %1\n\tcall *%2" : "=a"(result) : "m"(value), "r"(fs98_simsui_imports[SIMSUI_IMPORT__ftol])
                     : "ecx", "edx", "memory");
    return result;
}
/* a GDI object on the stack torn down */
static void gdi_destroy(void *object)
{
    PTR(object, 0) = IMAGE(0x15030);
    mfc_gdi_delete_object(object);
    PTR(object, 0) = IMAGE(0x15018);
}
#define TEXT_OUT(dc_handle, x, y) CALL_STD(TextOutA, int32_t, (uint32_t, int32_t, int32_t, const char *, int32_t))( \
        dc_handle, x, y, (const char *)(uintptr_t)name, I32((uintptr_t)name, -8))
/* 0x20481d40, Init(font spec id, background, CDIBPal *, flags): placed by its text's spec
 * "name#l,t,r,b" (else where it is, remapped; its text the string), sized to the string
 * in the font, and drawn */
int32_t TC fs98_simsui_text_art_init(void *art, uint32_t spec_id, void *background, void *pal, uint32_t flags)
{
    uint8_t dc[0x10], font[8], logfont[0x3c], metrics[0x38];
    uint32_t text, name, face, charset_word = 0;
    int32_t size[2], height_spec = 0, weight = 0, width, height, average, at;
    uint8_t charset = 0;
    uint8_t *info;
    void *old_font, *old;
    (void)charset_word;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    D32(art, 0xe4) = flags;
    PTR(art, 0xdc) = pal;
    CALL_THIS(MFC_3874, void, (void *, void *))(art, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_rect(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, (int32_t *)AT(art, 0x54));
    else {
        fs98_simsui_remap_rect((int32_t *)AT(art, 0x54), art);
        CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &text);
    }
    CALL_THIS(MFC_323, void *, (void *))(dc);
    if (!CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0)))
        goto done;
    D32(font, 4) = 0;
    PTR(font, 0) = IMAGE(VTABLE_FONT);
    CALL_THIS(MFC_540, void *, (void *))(&face);
    fs98_simsui_fill(logfont, 0, sizeof(logfont));
    fs98_simsui_parse_font_spec(spec_id, &face, &height_spec, &charset, &weight);
    I32(logfont, 0) = -height_spec;
    I32(logfont, 0x10) = weight;
    D8(logfont, 0x17) = charset;
    D8(logfont, 0x18) = 4;
    D8(logfont, 0x19) = 0;
    D8(logfont, 0x1a) = 2;
    D8(logfont, 0x1b) = 0;
    {
        const char *source = CALL_THIS(MFC_2915, char *, (void *, int32_t))(&face, 0);
        uint32_t length = 0;
        while (source[length]) ++length;
        fs98_simsui_copy(logfont + 0x1c, source, length + 1);
    }
    if (!CALL_THIS(MFC_1641, int32_t, (void *, uint32_t))(font, CALL_STD(CreateFontIndirectA, uint32_t, (const void *))(logfont))) {
        CALL_THIS(MFC_800, void, (void *))(&face);
        gdi_destroy(font);
        CALL_THIS(MFC_640, void, (void *))(dc);
        CALL_THIS(MFC_800, void, (void *))(&name);
        CALL_THIS(MFC_800, void, (void *))(&text);
        return 0;
    }
    old_font = CALL_THIS(MFC_5789, void *, (void *, void *))(dc, font);
    CALL_STD(GetTextExtentPointA, int32_t, (uint32_t, const char *, int32_t, int32_t *))(
        D32(dc, 4), (const char *)(uintptr_t)name, I32((uintptr_t)name, -8), size);
    CALL_STD(GetTextMetricsA, int32_t, (uint32_t, void *))(D32(dc, 4), metrics);
    average = I32(metrics, 0x14);
    width = size[0] + average + I32(art, 0x40) + 1;
    height = size[1] + I32(art, 0x44) + 0x15;
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        art, I32(art, 0x54), I32(art, 0x58), I32(art, 0x5c) - I32(art, 0x54), I32(art, 0x60) - I32(art, 0x58), 0);
    info = mfc_operator_new(0x428);
    fs98_simsui_fill(info, 0, 0x428);
    D32(info, 0) = 0x28;
    I32(info, 4) = width;
    I32(info, 8) = -height;
    D16(info, 0xc) = 1;
    D16(info, 0xe) = 8;
    D32(info, 0x10) = 0;
    for (at = 0; at < 0x100; ++at) D32(info, 0x28 + at * 4) = D32(PTR(art, 0xdc), 8 + at * 4);
    fs98_simsui_dib_create(AT(art, 0x64), info, PTR(art, 0xdc), 0);
    {
        void *bitmap = AT(art, 0xc0);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), bitmap ? D32(bitmap, 4) : 0);
    }
    fs98_simsui_dib_copy(background, AT(art, 0x64), 0, 0, width, height, I32(art, 0x54), I32(art, 0x58));
    if (flags & 1) {
        uint8_t shade[0x10], cast[0x70];
        void *old_cast, *old_shade_font, *bitmap;
        CALL_THIS(MFC_323, void *, (void *))(shade);
        fs98_simsui_dib_construct(cast);
        CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(shade, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0));
        fs98_simsui_dib_create(cast, info, PTR(art, 0xdc), 0);
        bitmap = AT(cast, 0x5c);
        old_cast = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(shade, 4), bitmap ? D32(bitmap, 4) : 0);
        old_shade_font = CALL_THIS(MFC_5789, void *, (void *, void *))(shade, font);
        CALL_STD(PatBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))(D32(shade, 4), 0, 0, width, height,
                                                                                          0xff0062);
        CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(shade, 1);
        CALL_THIS(MFC_6172, uint32_t, (void *, uint32_t))(shade, 0);
        TEXT_OUT(D32(shade, 4), average + I32(art, 0x40), I32(art, 0x44));
        CALL_STD(GdiFlush, int32_t, (void))();
        fs98_simsui_dib_blur_shadow(AT(art, 0x64), cast, 0, PTR(art, 0xdc));
        CALL_THIS(MFC_5789, void *, (void *, void *))(shade, old_shade_font);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(shade, 4), old_cast ? D32(old_cast, 4) : 0);
        fs98_simsui_dib_destroy(cast);
        CALL_THIS(MFC_640, void, (void *))(shade);
    }
    mfc_operator_delete(info);
    if (flags & 2) {
        CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(dc, 1);
        CALL_THIS(MFC_6172, uint32_t, (void *, uint32_t))(dc, D32(art, 0x50));
        TEXT_OUT(D32(dc, 4), average + I32(art, 0x40), I32(art, 0x44));
    }
    if (flags & 4) {
        CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(dc, 1);
        CALL_THIS(MFC_6172, uint32_t, (void *, uint32_t))(dc, D32(art, 0x48));
        TEXT_OUT(D32(dc, 4), average, 0);
        TEXT_OUT(D32(dc, 4), average, 2);
        TEXT_OUT(D32(dc, 4), average + 2, 0);
        TEXT_OUT(D32(dc, 4), average + 2, 2);
    }
    if (flags & 8) {
        uint32_t key;
        uint8_t *bits, *at_byte;
        int32_t first = -1, last = -1, row, column, count;
        long double step;
        CALL_THIS(MFC_6172, uint32_t, (void *, uint32_t))(dc, 0xff00ff);
        TEXT_OUT(D32(dc, 4), average + 1, 1);
        key = CALL_STD(GetNearestPaletteIndex, uint32_t, (uint32_t, uint32_t))(D32(PTR(art, 0xdc), 4), 0xff00ff);
        bits = PTR(art, 0xc8);
        CALL_STD(GdiFlush, int32_t, (void))();
        at_byte = bits;
        for (row = 0; row < height; ++row)
            for (column = 0; column < width; ++column, ++at_byte)
                if (*at_byte == key) {
                    if (first == -1) first = row;
                    last = row;
                    at_byte += width - column;
                    break;
                }
        if (first == -1) first = 0;
        if (last == -1) last = 0;
        at_byte = bits + first * width;
        count = last - first;
        step = (long double)*(const double *)IMAGE(GRADIENT_STEPS) / (long double)count;
        for (row = 0; first <= last && row <= count; ++row) {
            int32_t index = call_ftol((long double)row * step);
            for (column = 0; column < width; ++column, ++at_byte)
                if (*at_byte == key) *at_byte = D8(fs98_simsui_image, GRADIENT + index * 4);
        }
    } else {
        CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(dc, 1);
        CALL_THIS(MFC_6172, uint32_t, (void *, uint32_t))(dc, D32(art, 0x4c));
        TEXT_OUT(D32(dc, 4), average, 0);
    }
    CALL_THIS(MFC_5789, void *, (void *, void *))(dc, old_font);
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), old ? D32(old, 4) : 0);
    CALL_THIS(MFC_800, void, (void *))(&face);
    gdi_destroy(font);
done:
    CALL_THIS(MFC_640, void, (void *))(dc);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return 1;
}
#undef TEXT_OUT
/* 0x204824d0, OnPaint: the picture copied in through the palette */
void TC fs98_simsui_text_art_paint(void *art)
{
    uint8_t dc[0x54];
    void *old;
    if (!PTR(art, 0xdc)) return;
    CALL_THIS(MFC_470, void *, (void *, void *))(dc, art);
    old = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(art, 0xdc), 0);
    CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
    fs98_simsui_dib_blit_at(AT(art, 0x64), dc, 0, 0, I32(art, 0x5c) - I32(art, 0x54), I32(art, 0x60) - I32(art, 0x58),
                            0xcc0020);
    if (old) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old, 1);
    CALL_THIS(MFC_755, void, (void *))(dc);
}
