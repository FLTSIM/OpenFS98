/* SIMSUI's fonts, its dialog-unit remapping, and the module's start and end.
 *
 * The fonts are a CPtrArray at .data +0x21390 (its data +0x21394, its size +0x21398) of
 * CFonts made from string resources ("face,height,charset,weight"). The remapping scales
 * a control's rectangle, in its parent's client co-ordinates, by two 16.16 factors
 * (+0x213a8 across, +0x213ac down) that DllMain measures with a hidden dialog
 * (template 0x7530, class at vtable 0x152c0). */
#include "simsui_internal.h"

#define FONTS IMAGE(0x21390)
#define FONT_DATA ((void **)G_PTR(0x21394))
#define FONT_COUNT I32(fs98_simsui_image, 0x21398)
#define SCALE_X I32(fs98_simsui_image, 0x213a8)
#define SCALE_Y I32(fs98_simsui_image, 0x213ac)
#define EXTENSION IMAGE(0x213b8)            /* the AFX_EXTENSION_MODULE */
#define INSTANCE G32(0x213b0)
#define MAIN_INSTANCE G32(0x213d0)           /* CSimsDialog::m_hInstMainDLL */
#define ERROR_TEXT ((char *)IMAGE(0x20b90))  /* 0x200 bytes */
#define LINE_BREAK ((const char *)IMAGE(0x20058))
#define EXIT_TEXT ((char *)IMAGE(0x20f90))   /* string 0xf84e, loaded at the start */
#define VTABLE_FONT 0x152a8
#define VTABLE_REMAP_DIALOG 0x152c0
#define MESSAGE_MAP_REMAP_DIALOG 0x15288

/* 0x204716f0: a CFont made and kept; its index */
int32_t SC fs98_simsui_create_font(const char *face, int32_t height, uint32_t charset, int32_t weight)
{
    void *font = mfc_operator_new(8);
    uint32_t handle;
    int32_t index;
    if (font) {
        D32(font, 4) = 0;
        PTR(font, 0) = IMAGE(VTABLE_FONT);
    } else font = 0;
    handle = CALL_STD(CreateFontA, uint32_t, (int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t,
                                               uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                               const char *))(height, 0, 0, 0, weight, 0, 0, 0, charset & 0xff,
                                                               4, 2, 2, 0, face);
    CALL_THIS(MFC_1641, int32_t, (void *, uint32_t))(font, handle);
    index = FONT_COUNT;
    CALL_THIS(MFC_5859, void, (void *, int32_t, void *))(FONTS, index, font);
    return index;
}
/* 0x20471770, AddFont: string `id`'s font. RETAIL BUG (kept): SUI-1 (docs/retail-bugs.md):
 * when the string will not load or holds no ',' the font is made from three locals never
 * set (here, whatever the last call left). */
int32_t SC fs98_simsui_add_font(uint32_t id)
{
    uint8_t name[4];
    volatile int32_t weight;
    volatile int32_t height;
    volatile union { uint8_t byte; uint32_t word; } charset;
    int32_t index;
    CALL_THIS(MFC_540, void, (void *))(name);
    fs98_simsui_parse_font_spec(id, name, (int32_t *)&height, (uint8_t *)&charset.byte, (int32_t *)&weight);
    index = fs98_simsui_create_font(CALL_THIS(MFC_2915, char *, (void *, int32_t))(name, 0), height,
                                    charset.word, weight);
    CALL_THIS(MFC_800, void, (void *))(name);
    return index;
}
/* 0x20471810, ClearSimsFonts: each font deleted, last first, and the array emptied */
void SC fs98_simsui_clear_fonts(void)
{
    int32_t index;
    for (index = FONT_COUNT - 1; index >= 0; --index) {
        void *font = FONT_DATA[index];
        if (font) VIRTUAL(font, 4, void *, (void *, uint32_t))(font, 1);
    }
    CALL_THIS(MFC_6141, void, (void *, int32_t, int32_t))(FONTS, 0, -1);
}
/* 0x20472100, GetSimsBaseFont */
void *SC fs98_simsui_base_font(int32_t index)
{
    if (index >= FONT_COUNT) return 0;
    return FONT_DATA[index];
}
/* 0x20471850, SIMSUI_ExitCleanly (and SIMSUI_ExitCleanlyE): the last error's text, if any,
 * and the farewell, in a message box; then abort */
void fs98_simsui_exit_cleanly(void)
{
    uint32_t length = 0;
    uint32_t error = CALL_STD(GetLastError, uint32_t, (void))();
    ERROR_TEXT[0] = 0;
    if (error)
        length = CALL_STD(FormatMessageA, uint32_t, (uint32_t, const void *, uint32_t, uint32_t, char *, uint32_t,
                                                      void *))(0x1000, 0, error, 0, ERROR_TEXT, 0x200, 0);
    if (!length || length > 0x200) ERROR_TEXT[0] = 0;
    if (length) fs98_simsui_append(ERROR_TEXT, LINE_BREAK);
    fs98_simsui_append(ERROR_TEXT, EXIT_TEXT);
    fs98_simsui_append(ERROR_TEXT, LINE_BREAK);
    CALL_STD(MessageBoxA, int32_t, (uint32_t, const char *, const char *, uint32_t))(0, ERROR_TEXT, 0, 0x1000);
    CALL_C(abort, void, (void))();
}
/* strcat, as MSVC inlines it */
void fs98_simsui_append(char *text, const char *more)
{
    while (*text) ++text;
    while ((*text++ = *more++) != 0) {}
}

/* 0x20472120: the measuring dialog */
void *TC fs98_simsui_remap_dialog_construct(void *dialog, void *parent)
{
    CALL_THIS(MFC_324, void *, (void *, uint32_t, void *))(dialog, 0x7530, parent);
    PTR(dialog, 0) = IMAGE(VTABLE_REMAP_DIALOG);
    return dialog;
}
/* 0x20472140 */
void *TC fs98_simsui_remap_dialog_delete(void *dialog, uint32_t flags)
{
    CALL_THIS(MFC_641, void, (void *))(dialog);
    if (flags & 1) mfc_operator_delete(dialog);
    return dialog;
}
/* 0x20472160 */
const void *TC fs98_simsui_remap_dialog_message_map(const void *dialog)
{
    (void)dialog;
    return IMAGE(MESSAGE_MAP_REMAP_DIALOG);
}
/* 0x20472170, its OnInitDialog: the factors from its client area, then done */
int32_t TC fs98_simsui_remap_dialog_init(void *dialog)
{
    int32_t rect[4];
    CALL_THIS(MFC_4710, int32_t, (void *))(dialog);
    CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(dialog, 0x20), rect);
    SCALE_X = 0x1000000 / (rect[2] - rect[0]);
    SCALE_Y = 0x1000000 / (rect[3] - rect[1]);
    CALL_THIS(MFC_2645, void, (void *, int32_t))(dialog, 0);
    return 0;
}
/* 0x204721d0, RemapCtrlRect: a control's rectangle in its parent (when given one), scaled */
void SC fs98_simsui_remap_rect(int32_t *rect, void *control)
{
    if (control) {
        void *parent;
        CALL_STD(GetWindowRect, int32_t, (uint32_t, int32_t *))(D32(control, 0x20), rect);
        parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(control, 0x20)));
        CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect);
        CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect + 2);
    }
    rect[0] = (rect[0] * SCALE_X) >> 16;
    rect[1] = (SCALE_Y * rect[1]) >> 16;
    rect[2] = (SCALE_X * rect[2]) >> 16;
    rect[3] = (SCALE_Y * rect[3]) >> 16;
}
/* 0x20472260, RemapCtrlPt: a control's top left, or a point, scaled */
void SC fs98_simsui_remap_point(int32_t *point, void *control)
{
    int32_t rect[4];
    if (!control)
        CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect, point[0], point[1],
                                                                                     point[0], point[1]);
    fs98_simsui_remap_rect(rect, control);
    point[0] = rect[0];
    point[1] = rect[1];
}
/* 0x204722b0, RemapCtrlPtWth: a control's top left and width, or a point and a width */
void SC fs98_simsui_remap_point_width(int32_t *point, int32_t *width, void *control)
{
    int32_t rect[4];
    if (!control)
        CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(rect, point[0], point[1],
                                                                                     *width + point[0], point[1]);
    fs98_simsui_remap_rect(rect, control);
    point[0] = rect[0];
    point[1] = rect[1];
    *width = rect[2] - rect[0];
}
/* 0x20472310: CFont's operator delete */
void SC fs98_simsui_font_free(void *font)
{
    mfc_operator_delete(font);
}
/* 0x20472320: CFont's deleting destructor */
void *TC fs98_simsui_font_delete(void *font, uint32_t flags)
{
    fs98_simsui_pal_destroy(font);
    if (flags & 1) mfc_operator_delete(font);
    return font;
}

/* 0x20471950, DllMain: on attach, the extension module, the fonts, the farewell text and
 * the factors (1.0 when the dialog would not say); on detach, the fonts and the module */
int32_t SC fs98_simsui_dll_main(uint32_t instance, uint32_t reason, void *reserved)
{
    (void)reserved;
    if (reason == 1) {
        uint8_t dialog[0x60];
        void *library;
        CALL_STD(MFC_1182, int32_t, (void *, uint32_t))(EXTENSION, instance);
        MAIN_INSTANCE = instance;
        INSTANCE = instance;
        library = mfc_operator_new(0x40);
        if (library) CALL_THIS(MFC_342, void *, (void *, void *, int32_t))(library, EXTENSION, 0);
        fs98_simsui_clear_fonts();
        fs98_simsui_add_font(0xf836); fs98_simsui_add_font(0xf833); fs98_simsui_add_font(0xf832);
        fs98_simsui_add_font(0xf830); fs98_simsui_add_font(0xf82f); fs98_simsui_add_font(0xf837);
        fs98_simsui_add_font(0xf839); fs98_simsui_add_font(0xf838); fs98_simsui_add_font(0xf83a);
        fs98_simsui_add_font(0xf834); fs98_simsui_add_font(0xf83b); fs98_simsui_add_font(0xf835);
        fs98_simsui_add_font(0xf84f);
        EXIT_TEXT[0] = 0;
        CALL_STD(LoadStringA, int32_t, (uint32_t, uint32_t, char *, int32_t))(INSTANCE, 0xf84e, EXIT_TEXT, 0x100);
        fs98_simsui_remap_dialog_construct(dialog, 0);
        CALL_THIS(MFC_2514, int32_t, (void *))(dialog);
        if (!SCALE_X) SCALE_X = 0x10000;
        if (!SCALE_Y) SCALE_Y = 0x10000;
        CALL_THIS(MFC_641, void, (void *))(dialog);
    } else if (reason == 0) {
        MAIN_INSTANCE = 0;
        fs98_simsui_clear_fonts();
        CALL_STD(MFC_1253, void, (void *, int32_t))(EXTENSION, 0);
    }
    return 1;
}
/* 0x20471670, the MFC extension's raw DllMain: the module state's resource chains moved on
 * attach */
int32_t SC fs98_simsui_raw_dll_main(uint32_t instance, uint32_t reason, void *reserved)
{
    (void)instance; (void)reserved;
    if (reason == 1) {
        uint8_t *state = CALL_STD(MFC_1168, uint8_t *, (void))();
        D32(state, 0x1c) = D32(state, 0x20);
        D32(state, 0x28) = D32(state, 0x2c);
        D32(state, 0x20) = 0;
        D32(state, 0x2c) = 0;
    }
    return 1;
}
