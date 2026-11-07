/* CSimsTabCtrl (vtable 0x17238): SIMSUI's tab control, its pages in 0x160-byte entries
 * from +0x40 (the CSimsTabPage first), the last index +0xc20, the count +0xc24 and the
 * active index +0xc28. */
#include "simsui_internal.h"

#define ENTRY_SIZE 0x160

/* 0x204802b0, GetActivePage */
void *TC fs98_simsui_tabs_active_page(const void *tabs)
{
    return PTR(tabs, 0x40 + I32(tabs, 0xc28) * ENTRY_SIZE);
}

/* CSimsTabCtrlHelper (0x48 bytes, vtable 0x16eb8): an invisible 1x1 button whose text holds a
 * tab's mnemonic (+0x40), passing Alt keys on to the tab control (its window +0x44) */
#define VTABLE_HELPER 0x16eb8
/* 0x2047ef80 */
void *TC fs98_simsui_helper_construct(void *helper)
{
    mfc_wnd_construct(helper);
    PTR(helper, 0) = IMAGE(VTABLE_HELPER);
    D8(helper, 0x40) = 0;
    return helper;
}
/* 0x2047eff0 */
void TC fs98_simsui_helper_destroy(void *helper)
{
    PTR(helper, 0) = IMAGE(VTABLE_HELPER);
    CALL_THIS(MFC_609, void, (void *))(helper);
}
/* 0x2047efa0 */
void *TC fs98_simsui_helper_delete(void *helper, uint32_t flags)
{
    return fs98_simsui_delete_object(helper, flags, 0x48, fs98_simsui_helper_destroy, fs98_simsui_ui_delete);
}
/* 0x2047f000, Init(CSimsTabCtrl *): the tab control's window kept, the mnemonic taken from
 * the text, the window shrunk to a pixel where it is */
void TC fs98_simsui_helper_init(void *helper, void *tabs)
{
    uint32_t text;
    int32_t rect[4], at;
    void *parent;
    D32(helper, 0x44) = D32(tabs, 0x20);
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_3874, void, (void *, void *))(helper, &text);
    CALL_STD(GetWindowRect, int32_t, (uint32_t, int32_t *))(D32(helper, 0x20), rect);
    parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(helper, 0x20)));
    CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect);
    CALL_STD(ScreenToClient, int32_t, (uint32_t, int32_t *))(D32(parent, 0x20), rect + 2);
    at = CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '&');
    if (at != -1) {
        char mnemonic = ((const char *)(uintptr_t)text)[at + 1];
        if (mnemonic != '&') D8(helper, 0x40) = (uint8_t)mnemonic;
    }
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(helper, rect[0], rect[1], 1, 1, 1);
    CALL_THIS(MFC_800, void, (void *))(&text);
}
/* 0x2047f0e0 */
const void *TC fs98_simsui_helper_message_map(const void *helper) { (void)helper; return IMAGE(0x16e38); }
/* 0x2047f0f0, OnDrawItem; 0x2047f100, OnLButtonDown: nothing */
void TC fs98_simsui_helper_draw_item(void *helper, int32_t id, void *item) { (void)helper; (void)id; (void)item; }
void TC fs98_simsui_helper_button_down(void *helper, uint32_t keys, int32_t x, int32_t y)
{
    (void)helper; (void)keys; (void)x; (void)y;
}
/* 0x2047f110, OnSysKeyUp: with Alt, passed to the tab control */
void TC fs98_simsui_helper_sys_key_up(void *helper, uint32_t key, uint32_t repeat, uint32_t flags)
{
    (void)repeat;
    if (flags & 0x2000)
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(helper, 0x44), 0x105, key, 0x20000001);
}

/* CSimsTabPage (0x1118 bytes, vtable 0x17060): a CSimsDialog shown as a page of a tab
 * control: its owner +0x10f4, its template id +0x1104; the palette +0x10dc, its size
 * +0x10e8/+0x10e4, its instance +0x10f0, the background it sits on +0x10f8 and the
 * picture it paints +0x10fc; painting itself +0x1100. */
#define VTABLE_PAGE 0x17060
/* 0x2047f140, GetRuntimeClass */
const void *TC fs98_simsui_page_runtime_class(const void *page) { (void)page; return IMAGE(0x16f80); }
/* 0x2047f150, CSimsTabPage(id, owner) */
void *TC fs98_simsui_page_construct(void *page, uint32_t id, void *owner)
{
    fs98_simsui_dialog_construct(page, id, owner);
    PTR(page, 0x10f4) = owner;
    PTR(page, 0) = IMAGE(VTABLE_PAGE);
    D32(page, 0x84) = 0xb;
    D32(page, 0x1104) = id;
    D32(page, 0x1100) = 0;
    return page;
}
/* 0x2047f200 */
void TC fs98_simsui_page_destroy(void *page)
{
    PTR(page, 0) = IMAGE(VTABLE_PAGE);
    fs98_simsui_dialog_destroy(page);
}
/* 0x2047f1a0 */
void *TC fs98_simsui_page_delete(void *page, uint32_t flags)
{
    return fs98_simsui_delete_object(page, flags, 0x1118, fs98_simsui_page_destroy, fs98_simsui_ui_delete);
}
/* the page created and sized: what follows both Inits */
static void page_place(void *page)
{
    D32(page, 0x80) = D32(PTR(page, 0x10f4), 0x80);
    D32(page, 0x88) = 0xffffffffu;
    CALL_THIS(MFC_6197, int32_t, (void *, const void *, int32_t, int32_t, int32_t, int32_t, uint32_t))(
        page, 0, 0, 0, I32(page, 0x10e8), I32(page, 0x10e4), 6);
    D32(page, 0x228) = 0;
    D32(page, 0x4c8) = 0;
    D32(page, 0x538) = 0;
}
/* 0x2047f210, Init(width less): painted with the owner's tab strip, as wide as the strip
 * less the argument */
int32_t TC fs98_simsui_page_init(void *page, int32_t narrower)
{
    void *owner;
    CALL_THIS(MFC_2086, int32_t, (void *, uint32_t, void *))(page, D32(page, 0x1104) & 0xffff, PTR(page, 0x10f4));
    owner = PTR(page, 0x10f4);
    PTR(page, 0x10fc) = AT(owner, 0x1064);
    PTR(page, 0x10dc) = AT(owner, 0x7e8);
    I32(page, 0x10e8) = I32(PTR(page, 0x10fc), 8) - narrower;
    I32(page, 0x10e4) = I32(PTR(page, 0x10fc), 0xc);
    PTR(page, 0x10f8) = AT(owner, 0xff4);
    page_place(page);
    return 1;
}
/* 0x2047f2d0, Init(id, background, CDIBPal *, HINSTANCE): painted with a bitmap of its own */
int32_t TC fs98_simsui_page_init_with(void *page, int32_t id, void *background, void *pal, uint32_t instance)
{
    CALL_THIS(MFC_2086, int32_t, (void *, uint32_t, void *))(page, D32(page, 0x1104) & 0xffff, PTR(page, 0x10f4));
    PTR(page, 0x10dc) = pal;
    PTR(page, 0x10f8) = background;
    D32(page, 0x10f0) = instance;
    if (!instance) D32(page, 0x10f0) = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    if (!fs98_simsui_dib_load(AT(page, 0xff4), id, D32(page, 0x10f0), pal, 0)) return 0;
    PTR(page, 0x10fc) = AT(page, 0xff4);
    I32(page, 0x10e8) = I32(page, 0xffc);
    I32(page, 0x10e4) = I32(page, 0x1000);
    page_place(page);
    return 1;
}
/* 0x2047f3c0, DoDataExchange: CSimsDialog's */
void TC fs98_simsui_page_exchange(void *page, void *exchange) { fs98_simsui_dialog_exchange(page, exchange); }
/* 0x2047f3d0 */
const void *TC fs98_simsui_page_message_map(const void *page) { (void)page; return IMAGE(0x16f98); }
/* 0x2047f3e0, OnInitDialog: CSimsDialog's; true */
int32_t TC fs98_simsui_page_on_init(void *page)
{
    fs98_simsui_dialog_on_init(page);
    return 1;
}
/* 0x2047f3f0, OnQueryNewPalette: true */
int32_t TC fs98_simsui_page_query_palette(void *page) { (void)page; return 1; }
/* 0x2047f400, OnPaint: validated, nothing drawn */
void TC fs98_simsui_page_paint(void *page)
{
    uint8_t dc[0x54];
    CALL_THIS(MFC_470, void *, (void *, void *))(dc, page);
    CALL_THIS(MFC_755, void, (void *))(dc);
}
/* 0x2047f420, OnActivate: activated, the focus back to the edit being validated */
void TC fs98_simsui_page_activate(void *page, uint32_t state, void *other, int32_t minimised)
{
    (void)other; (void)minimised;
    if (state && D32(page, 0x7d0)) {
        void *control = CALL_THIS(MFC_3092, void *, (void *, int32_t))(page, I32(page, 0x7d0));
        if (control)
            CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(page, 0x20), 0x5fa,
                                                                                     D32(control, 0x20), 0);
    }
}
/* 0x2047f460, OnEraseBkgnd: its picture drawn, when it paints itself */
int32_t TC fs98_simsui_page_erase(void *page, void *dc)
{
    if (D32(page, 0x1100)) {
        int32_t rect[4];
        void *old = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(page, 0x10dc), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        CALL_STD(GetClientRect, int32_t, (uint32_t, int32_t *))(D32(page, 0x20), rect);
        fs98_simsui_dib_blit_rect(PTR(page, 0x10fc), dc, rect, 0xcc0020);
        if (old) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old, 1);
    }
    return 1;
}
/* 0x2047f4e0, OnGetDlgCode */
uint32_t TC fs98_simsui_page_dialog_code(void *page) { (void)page; return 6; }
/* 0x2047f4f0, PostNcDestroy: deleted */
void TC fs98_simsui_page_post_destroy(void *page)
{
    if (page) VIRTUAL(page, 4, void *, (void *, uint32_t))(page, 1);
}
/* 0x2047f500, PreTranslateMessage: a dialog message taken (Tab passed on), else CDialog's */
int32_t TC fs98_simsui_page_pre_translate(void *page, const uint32_t *message)
{
    if (CALL_THIS(MFC_4047, int32_t, (void *, const void *))(page, message)) {
        if (message[1] == 0x100 && message[2] == 9) CALL_THIS(MFC_2379, uint32_t, (void *))(page);
        return 1;
    }
    return CALL_THIS(MFC_5280, int32_t, (void *, const void *))(page, message);
}
/* 0x2047f540, an EnumChildWindows callback: the first and last focusable child (a tab stop,
 * enabled, visible, and not one of two to skip) */
int32_t SC fs98_simsui_find_focusable(uint32_t window, uint32_t *found)
{
    int32_t style = CALL_STD(GetWindowLongA, int32_t, (uint32_t, int32_t))(window, -16);
    if ((style & 0x10000) && !(style & 0x8000000) && CALL_STD(IsWindowVisible, int32_t, (uint32_t))(window) &&
        found[2] != window && found[3] != window) {
        if (!found[0]) found[0] = window;
        found[1] = window;
    }
    return 1;
}

/* CSimsTabCtrl (0xcc8 bytes, vtable 0x17238): a CButton drawn as a column of tabs, each a
 * CSimsTabPage, and the area under them.
 *
 * Seven entries of 0x160 bytes from +0x40: the page, its frames (+4), its face (+0x74)
 * and its shadow mask (+0xe4) as CDIBSections, two lines (+0x154), its mnemonic (+0x158)
 * and disabled (+0x15c). The background +0xa50; the tab strip +0x9e0; the area under the
 * tabs: its picture +0xa54, face +0xb34, shadow +0xac4, mask +0xba4; the mask of the
 * area under the tabs +0xc14; the colours of normal and current text +0xc18/+0xc1c; the
 * last index +0xc20, the count +0xc24, the active index +0xc28; the area page +0xc30; the
 * strip's first pixel +0xc34; the CDIBPal +0xc38; the strip id +0xc3c; a tab's height
 * +0xc40 and width +0xc44; the pages' total height +0xc48; the shadow's offset +0xc4c
 * (flags & 1); where the pages go +0xc50/+0xc54; the id +0xc5c; the CFont +0xc60; the
 * HINSTANCE +0xc64; the dialog +0xc68; the tab clicked +0xc6c; the rectangles of the old
 * and new active tabs +0xc70/+0xc90; its own +0xc80; drawing +0xcb4; the palette state
 * +0xcb8; a BITMAPINFO +0xcbc; flags +0xcc4. */
#define VTABLE_CBUTTON 0x158a0
#define VTABLE_TABS 0x17238
#define ENTRY(tabs, index) AT(tabs, 0x40 + (index) * ENTRY_SIZE)
#define PAGE_OF(tabs, index) PTR(tabs, 0x40 + (index) * ENTRY_SIZE)
#define DISABLED(tabs, index) D32(tabs, 0x40 + (index) * ENTRY_SIZE + 0x15c)
#define TWO_LINES(tabs, index) D32(tabs, 0x40 + (index) * ENTRY_SIZE + 0x154)
#define MNEMONIC(tabs, index) D8(tabs, 0x40 + (index) * ENTRY_SIZE + 0x158)


/* 0x2047f6d0, an entry's constructor */
void *TC fs98_simsui_tab_entry_construct(void *entry)
{
    fs98_simsui_dib_construct(AT(entry, 4));
    fs98_simsui_dib_construct(AT(entry, 0x74));
    fs98_simsui_dib_construct(AT(entry, 0xe4));
    return entry;
}
/* 0x2047f730, its destructor */
void TC fs98_simsui_tab_entry_destroy(void *entry)
{
    fs98_simsui_dib_destroy(AT(entry, 0xe4));
    fs98_simsui_dib_destroy(AT(entry, 0x74));
    fs98_simsui_dib_destroy(AT(entry, 4));
}
static const uint32_t tab_sections[5] = {0x9e0, 0xa54, 0xac4, 0xb34, 0xba4};
/* 0x2047f590 */
void *TC fs98_simsui_tabs_construct(void *tabs)
{
    int32_t at;
    mfc_wnd_construct(tabs);
    PTR(tabs, 0) = IMAGE(VTABLE_CBUTTON);
    fs98_simsui_vector_construct(AT(tabs, 0x40), ENTRY_SIZE, 7, fs98_simsui_tab_entry_construct,
                                 fs98_simsui_tab_entry_destroy);
    for (at = 0; at < 5; ++at) fs98_simsui_dib_construct(AT(tabs, tab_sections[at]));
    D32(tabs, 0xc6c) = 0xffffffffu;
    D32(tabs, 0xcb4) = 0;
    D32(tabs, 0xcbc) = 0;
    D32(tabs, 0xc30) = 0xffffffffu;
    D32(tabs, 0xc48) = 0;
    D32(tabs, 0xc14) = 0;
    D32(tabs, 0xc38) = 0;
    PTR(tabs, 0) = IMAGE(VTABLE_TABS);
    return tabs;
}
/* 0x2047f790 */
void TC fs98_simsui_tabs_destroy(void *tabs)
{
    int32_t at;
    PTR(tabs, 0) = IMAGE(VTABLE_TABS);
    D32(tabs, 0xc60) = 0;
    for (at = 4; at >= 0; --at) fs98_simsui_dib_destroy(AT(tabs, tab_sections[at]));
    fs98_simsui_vector_destroy(AT(tabs, 0x40), ENTRY_SIZE, 7, fs98_simsui_tab_entry_destroy);
    CALL_THIS(MFC_609, void, (void *))(tabs);
}
/* 0x2047f670 */
void *TC fs98_simsui_tabs_delete(void *tabs, uint32_t flags)
{
    return fs98_simsui_delete_object(tabs, flags, 0xcc8, fs98_simsui_tabs_destroy, fs98_simsui_ui_delete);
}
/* 0x2047f850, EnablePage(page, enable) */
void TC fs98_simsui_tabs_enable_page(void *tabs, void *page, int32_t enable)
{
    int32_t at;
    for (at = 0; at < I32(tabs, 0xc24); ++at)
        if (PAGE_OF(tabs, at) == page) DISABLED(tabs, at) = !enable;
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(tabs, 0x20), 0, 1);
}
/* the number of frames a tab has: 4 for a tall strip, else 2 */
static int32_t tab_frames(const void *tabs) { return I32(tabs, 0x9ec) >= 0xa0 ? 4 : 2; }
/* 0x2047f8b0, Init(dialog, CFont *, strip id, background, CDIBPal *, text colour, current
 * colour, shadow offset, page x, page y, unused, HINSTANCE, flags): placed by its text's
 * spec "name#x,y" (else where it is, remapped); a tab a third (a fifth, for a tall strip)
 * of the strip less the shadow offset high */
int32_t TC fs98_simsui_tabs_init(void *tabs, void *dialog, void *font, int32_t id, void *background, void *pal,
                                 uint32_t colour, uint32_t current, int32_t shadow, int32_t page_x, int32_t page_y,
                                 int32_t unused, uint32_t instance, uint32_t flags)
{
    uint32_t text, name;
    int32_t point[2], at, height;
    uint8_t *info;
    (void)unused;
    D32(tabs, 0xcc4) = flags;
    for (at = 0; at < 7; ++at) {
        PAGE_OF(tabs, at) = 0;
        MNEMONIC(tabs, at) = 0;
        DISABLED(tabs, at) = 0;
    }
    PTR(tabs, 0xa50) = background;
    PTR(tabs, 0xc60) = font;
    PTR(tabs, 0xc38) = pal;
    D32(tabs, 0xc14) = 0;
    D32(tabs, 0xc20) = 0xffffffffu;
    D32(tabs, 0xc2c) = 0;
    D32(tabs, 0xc24) = 0;
    D32(tabs, 0xc28) = 0;
    D32(tabs, 0xc18) = colour;
    D32(tabs, 0xc1c) = current;
    I32(tabs, 0xc4c) = (D8(tabs, 0xcc4) & 1) ? shadow : 0;
    I32(tabs, 0xc54) = page_y;
    I32(tabs, 0xc50) = page_x;
    I32(tabs, 0xc3c) = id;
    D32(tabs, 0xc5c) = (uint32_t)CALL_THIS(MFC_3089, int32_t, (void *))(tabs);
    D32(tabs, 0xc64) = instance;
    if (!instance) D32(tabs, 0xc64) = D32(CALL_STD(MFC_1168, void *, (void))(), 8);
    PTR(tabs, 0xc68) = dialog;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    CALL_THIS(MFC_540, void *, (void *))(&name);
    CALL_THIS(MFC_3874, void, (void *, void *))(tabs, &text);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '#') != -1)
        fs98_simsui_parse_point(CALL_THIS(MFC_2915, char *, (void *, int32_t))(&text, 0), &name, point);
    else {
        fs98_simsui_remap_rect((int32_t *)AT(tabs, 0xc80), tabs);
        point[0] = I32(tabs, 0xc80);
        point[1] = I32(tabs, 0xc84);
        CALL_THIS(MFC_858, void *, (void *, const void *))(&name, &text);
    }
    fs98_simsui_dib_load(AT(tabs, 0x9e0), I32(tabs, 0xc3c), D32(tabs, 0xc64), PTR(tabs, 0xc38), 0);
    height = I32(tabs, 0x9ec);
    I32(tabs, 0xc40) = height < 0xa0 ? (height - shadow) / 3 : (height - shadow) / 5;
    I32(tabs, 0xc44) = I32(tabs, 0x9e8);
    if (I32(tabs, 0xc40) < 0) I32(tabs, 0xc40) = -I32(tabs, 0xc40);
    CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(
        (int32_t *)AT(tabs, 0xc80), point[0], point[1], point[0] + I32(tabs, 0xc44), point[1] + I32(tabs, 0xc40));
    for (at = 0; at < 4; ++at) D32(tabs, 0xc90 + at * 4) = D32(tabs, 0xc80 + at * 4);
    for (at = 0; at < 4; ++at) D32(tabs, 0xc70 + at * 4) = D32(tabs, 0xc80 + at * 4);
    info = mfc_operator_new(0x428);
    PTR(tabs, 0xcbc) = info;
    fs98_simsui_fill(info, 0, 0x428);
    D32(PTR(tabs, 0xcbc), 0) = 0x28;
    I32(PTR(tabs, 0xcbc), 4) = I32(tabs, 0xc44);
    I32(PTR(tabs, 0xcbc), 8) = -5 - I32(tabs, 0xc40);
    D16(PTR(tabs, 0xcbc), 0xc) = 1;
    D16(PTR(tabs, 0xcbc), 0xe) = 8;
    D32(PTR(tabs, 0xcbc), 0x10) = 0;
    for (at = 0; at < 0x100; ++at) D32(PTR(tabs, 0xcbc), 0x28 + at * 4) = D32(PTR(tabs, 0xc38), 8 + at * 4);
    D8(tabs, 0xc34) = *(uint8_t *)PTR(tabs, 0xa44);
    CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(
        D32(CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(tabs, 0x20))), 0x20)));
    D32(tabs, 0xcb4) = 0;
    CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
        tabs, point[0], point[1], I32(tabs, 0xc44), I32(tabs, 0xc40), 1);
    CALL_THIS(MFC_800, void, (void *))(&name);
    CALL_THIS(MFC_800, void, (void *))(&text);
    return 1;
}
/* 0x2047fc40, AddPage(page, text, disabled): a tab for the page, its frames drawn from the
 * strip over the background with the text (two lines when it holds '\n'; its mnemonic after
 * the mark) in the colour of each state; the page placed */
void TC fs98_simsui_tabs_add_page(void *tabs, void *page, const char *title, int32_t disabled)
{
    uint8_t spare[0x70], dc[0x10];
    uint32_t text;
    int32_t frames = tab_frames(tabs), index, at, area[4];
    void *old, *old_font, *entry;
    CALL_THIS(MFC_540, void *, (void *))(&text);
    fs98_simsui_dib_construct(spare);
    ++I32(tabs, 0xc20);
    ++I32(tabs, 0xc24);
    I32(tabs, 0xc48) += I32(tabs, 0xc40);
    PAGE_OF(tabs, I32(tabs, 0xc20)) = page;
    D32(page, 0x1108) = D32(tabs, 0xc20);
    TWO_LINES(tabs, I32(tabs, 0xc20)) = 0;
    CALL_THIS(MFC_860, void *, (void *, const char *))(&text, title);
    if (CALL_THIS(MFC_2763, int32_t, (void *, int32_t))(&text, '\n') != -1) TWO_LINES(tabs, I32(tabs, 0xc20)) = 1;
    at = CALL_THIS(MFC_2784, int32_t, (void *, const char *))(&text, IMAGE(0x200a8));
    if (at != -1) {
        uint32_t key = ((uint32_t)(at + 1) & 0xffffff00u) | (uint8_t)((const char *)(uintptr_t)text)[at + 1];
        if ((uint8_t)key != '&')
            MNEMONIC(tabs, I32(tabs, 0xc20)) = (uint8_t)CALL_STD(VkKeyScanA, int16_t, (uint32_t))(key);
    }
    DISABLED(tabs, I32(tabs, 0xc20)) = (uint32_t)disabled;
    D32(tabs, 0xcb4) = 0;
    I32(tabs, 0xc8c) += I32(tabs, 0xc40);
    PTR(page, 0x110c) = tabs;
    D32(page, 0x1110) = D32(tabs, 0xc50);
    D32(page, 0x1114) = D32(tabs, 0xc54);
    CALL_THIS(MFC_6197, int32_t, (void *, const void *, int32_t, int32_t, int32_t, int32_t, uint32_t))(
        page, 0, I32(tabs, 0xc50), I32(tabs, 0xc54), 0, 0, 5);
    index = I32(tabs, 0xc20);
    entry = ENTRY(tabs, index);
    I32(PTR(tabs, 0xcbc), 4) = I32(tabs, 0xc44);
    I32(PTR(tabs, 0xcbc), 8) = -I32(tabs, 0xc40);
    fs98_simsui_dib_create(AT(entry, 0x74), PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    I32(PTR(tabs, 0xcbc), 4) = I32(tabs, 0xc44);
    I32(PTR(tabs, 0xcbc), 8) = -I32(tabs, 0xc40);
    fs98_simsui_dib_create(AT(entry, 0xe4), PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    if (D8(tabs, 0xcc4) & 1) fs98_simsui_dib_create(spare, PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    I32(PTR(tabs, 0xcbc), 4) = I32(tabs, 0xc44);
    I32(PTR(tabs, 0xcbc), 8) = -(frames * I32(tabs, 0xc40));
    fs98_simsui_dib_create(AT(entry, 4), PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_STD(SetLastError, void, (uint32_t))(0);
    if (!CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(0)))
        fs98_simsui_exit_cleanly();
    {
        void *bitmap = AT(entry, 0x60);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), bitmap ? D32(bitmap, 4) : 0);
    }
    CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(area, 0, 0, I32(tabs, 0xc44) - 0x14,
                                                                                I32(tabs, 0xc40));
    if (TWO_LINES(tabs, I32(tabs, 0xc20))) area[1] += 5;
    CALL_THIS(MFC_5875, int32_t, (void *, int32_t))(dc, 1);
    old_font = CALL_THIS(MFC_5789, void *, (void *, void *))(dc, PTR(tabs, 0xc60));
    for (at = 0; at < frames; ++at)
        fs98_simsui_dib_copy(PTR(tabs, 0xa50), AT(ENTRY(tabs, I32(tabs, 0xc20)), 4), 0, at * I32(tabs, 0xc40),
                             I32(tabs, 0xc44), I32(tabs, 0xc40), I32(tabs, 0xc80),
                             I32(tabs, 0xc40) * I32(tabs, 0xc20) + I32(tabs, 0xc84));
    for (at = 0; at < frames; ++at) {
        uint32_t copy;
        entry = ENTRY(tabs, I32(tabs, 0xc20));
        fs98_simsui_dib_fill(AT(entry, 0x74), 0);
        fs98_simsui_dib_copy(AT(tabs, 0x9e0), AT(entry, 0x74), 0, 0, I32(tabs, 0xc44), I32(tabs, 0xc40), 0,
                             at * I32(tabs, 0xc40));
        if (D8(tabs, 0xcc4) & 1) {
            entry = ENTRY(tabs, I32(tabs, 0xc20));
            fs98_simsui_dib_fill(AT(entry, 0xe4), 0xff);
            entry = ENTRY(tabs, I32(tabs, 0xc20));
            fs98_simsui_dib_mask(AT(entry, 0x74), AT(entry, 0xe4), I32(tabs, 0xc4c) - 1, I32(tabs, 0xc4c),
                                 I32(tabs, 0xc44), I32(tabs, 0xc40), 0, 0, 0, -1, 0);
            entry = ENTRY(tabs, I32(tabs, 0xc20));
            fs98_simsui_dib_blur_shadow(AT(entry, 4), AT(entry, 0xe4), at * I32(tabs, 0xc40), PTR(tabs, 0xc38));
        }
        entry = ENTRY(tabs, I32(tabs, 0xc20));
        fs98_simsui_dib_sprite(AT(entry, 0x74), AT(entry, 4), 0, at * I32(tabs, 0xc40), I32(tabs, 0xc44),
                               I32(tabs, 0xc40), 0, 0, 0);
        if (frames == 2)
            CALL_THIS(MFC_6172, uint32_t, (void *, uint32_t))(
                dc, at && !DISABLED(tabs, I32(tabs, 0xc20)) ? D32(tabs, 0xc1c) : D32(tabs, 0xc18));
        else CALL_THIS(MFC_6172, uint32_t, (void *, uint32_t))(dc, at == 1 ? D32(tabs, 0xc1c) : D32(tabs, 0xc18));
        CALL_THIS(MFC_537, void *, (void *, const char *))(&copy, title);
        VIRTUAL(dc, 0x70, int32_t, (void *, uint32_t, int32_t, void *, uint32_t))(
            dc, copy, I32((uintptr_t)copy, -8), area, TWO_LINES(tabs, I32(tabs, 0xc20)) ? 1 : 0x25);
        CALL_THIS(MFC_800, void, (void *))(&copy);
        area[1] += I32(tabs, 0xc40);
        area[3] += I32(tabs, 0xc40);
    }
    CALL_THIS(MFC_5789, void *, (void *, void *))(dc, old_font);
    CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(dc, 4), old ? D32(old, 4) : 0);
    CALL_THIS(MFC_640, void, (void *))(dc);
    fs98_simsui_dib_destroy(spare);
    CALL_THIS(MFC_800, void, (void *))(&text);
}
/* 0x204802d0, SetActivePage */
int32_t TC fs98_simsui_tabs_set_active_page(void *tabs, void *page)
{
    int32_t at;
    for (at = 0; at < I32(tabs, 0xc24); ++at)
        if (PAGE_OF(tabs, at) == page) {
            fs98_simsui_tabs_set_active_tab(tabs, at, 0);
            return 1;
        }
    return 0;
}
/* 0x20480320, SetLastPage(page): the area under the tabs drawn (the strip's last frame
 * over the background, its mask kept), and the page shown there */
int32_t TC fs98_simsui_tabs_set_last_page(void *tabs, void *page)
{
    int32_t frames = tab_frames(tabs), height = I32(tabs, 0xc4c) + I32(tabs, 0xc40), at;
    uint32_t bytes;
    I32(PTR(tabs, 0xcbc), 4) = I32(tabs, 0xc44);
    I32(PTR(tabs, 0xcbc), 8) = -height;
    fs98_simsui_dib_create(AT(tabs, 0xb34), PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    if (D8(tabs, 0xcc4) & 1) {
        I32(PTR(tabs, 0xcbc), 8) = -(height + I32(tabs, 0xc4c));
        fs98_simsui_dib_create(AT(tabs, 0xac4), PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    }
    I32(PTR(tabs, 0xcbc), 8) = -height;
    fs98_simsui_dib_create(AT(tabs, 0xa54), PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    I32(PTR(tabs, 0xcbc), 8) = -I32(tabs, 0xc40);
    fs98_simsui_dib_create(AT(tabs, 0xba4), PTR(tabs, 0xcbc), PTR(tabs, 0xc38), 0);
    fs98_simsui_dib_copy(PTR(tabs, 0xa50), AT(tabs, 0xa54), 0, 0, I32(tabs, 0xc44), height, I32(tabs, 0xc80),
                         I32(tabs, 0xc20) * I32(tabs, 0xc40) + I32(tabs, 0xc84));
    fs98_simsui_dib_fill(AT(tabs, 0xb34), D8(tabs, 0xc34));
    fs98_simsui_dib_copy(AT(tabs, 0x9e0), AT(tabs, 0xb34), 0, 0, I32(tabs, 0xc44), height, 0, frames * I32(tabs, 0xc40));
    if (D8(tabs, 0xcc4) & 1) {
        fs98_simsui_dib_fill(AT(tabs, 0xac4), 0xff);
        fs98_simsui_dib_mask(AT(tabs, 0xb34), AT(tabs, 0xac4), I32(tabs, 0xc4c) - 1, I32(tabs, 0xc4c), I32(tabs, 0xc44),
                             height + I32(tabs, 0xc4c), 0, 0, 0, -1, 0);
        fs98_simsui_dib_blur_shadow(AT(tabs, 0xa54), AT(tabs, 0xac4), 0, PTR(tabs, 0xc38));
    }
    fs98_simsui_dib_sprite(AT(tabs, 0xb34), AT(tabs, 0xa54), 0, 0, I32(tabs, 0xc44), height, 0, 0, -1);
    fs98_simsui_dib_fill(AT(tabs, 0xba4), 0xff);
    fs98_simsui_dib_mask(AT(tabs, 0xb34), AT(tabs, 0xba4), 0, 0, I32(tabs, 0xc44), I32(tabs, 0xc40), 0, 0, 0, -1, 0);
    bytes = (uint32_t)(I32(tabs, 0xc44) * I32(tabs, 0xc40));
    PTR(tabs, 0xc14) = mfc_operator_new(bytes);
    fs98_simsui_copy(PTR(tabs, 0xc14), PTR(tabs, 0xc08), bytes);
    mfc_operator_delete(PTR(tabs, 0xcbc));
    D32(tabs, 0xcbc) = 0;
    for (at = 0; at < I32(tabs, 0xc24); ++at)
        if (PAGE_OF(tabs, at) == page) {
            void *parent;
            D32(tabs, 0xc30) = (uint32_t)at;
            I32(tabs, 0xc8c) += I32(tabs, 0xc4c) - I32(tabs, 0xc40);
            CALL_THIS(MFC_4299, int32_t, (void *, int32_t, int32_t, int32_t, int32_t, int32_t))(
                tabs, I32(tabs, 0xc80), I32(tabs, 0xc84), I32(tabs, 0xc88) - I32(tabs, 0xc80),
                I32(tabs, 0xc8c) - I32(tabs, 0xc84), 1);
            parent = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(tabs, 0x20)));
            CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(parent, 0x20), 0, 1);
            return 1;
        }
    D32(tabs, 0xc30) = 0xffffffffu;
    CALL_STD(MessageBeep, int32_t, (uint32_t))(0);
    return 0;
}
/* 0x20480630, GetActiveTab */
int32_t TC fs98_simsui_tabs_active_tab(const void *tabs) { return I32(tabs, 0xc28); }
/* 0x20480640, SetActiveTab(index, focus window): leaving a page whose edit will not
 * validate refused; the tabs redrawn, the old page hidden and the new one shown (made the
 * first time); the page told (0x5f9: 4 changed, 2 first shown, 1 from the tab control) */
int32_t TC fs98_simsui_tabs_set_active_tab(void *tabs, int32_t index, uint32_t focus)
{
    int32_t current, changed, first, from_self;
    void *page;
    if (index < 0 || index > I32(tabs, 0xc20) || DISABLED(tabs, index)) return 0;
    current = I32(tabs, 0xc28);
    page = PAGE_OF(tabs, current);
    first = D32(page, 0x1100) == 0;
    changed = current != index;
    from_self = focus == D32(tabs, 0x20);
    if (current != index) {
        void *focused = CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetFocus, uint32_t, (void))());
        int32_t id, failed;
        if (focused && CALL_THIS(MFC_4083, int32_t, (void *, const void *))(focused, fs98_simsui_imports[SIMSUI_IMPORT_MFC_1829]))
            id = CALL_THIS(MFC_3089, int32_t, (void *))(focused);
        else id = I32(page, 0x7d0);
        failed = VIRTUAL(PAGE_OF(tabs, I32(tabs, 0xc28)), 0xd8, int32_t, (void *, int32_t))(PAGE_OF(tabs, I32(tabs, 0xc28)), id);
        if (failed && VIRTUAL(PAGE_OF(tabs, I32(tabs, 0xc28)), 0xdc, int32_t, (void *, int32_t))(PAGE_OF(tabs, I32(tabs, 0xc28)), failed))
            return 0;
    }
    D32(tabs, 0xcb4) = 1;
    CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(
        (int32_t *)AT(tabs, 0xc70), 0, I32(tabs, 0xc40) * I32(tabs, 0xc28), I32(tabs, 0xc44),
        (I32(tabs, 0xc28) + 1) * I32(tabs, 0xc40));
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(tabs, 0x20), AT(tabs, 0xc70), 1);
    if (D32(PAGE_OF(tabs, I32(tabs, 0xc28)), 0x1100))
        CALL_THIS(MFC_6215, int32_t, (void *, int32_t))(PAGE_OF(tabs, I32(tabs, 0xc28)), 0);
    {
        int32_t top = index * I32(tabs, 0xc40), bottom = (index + 1) * I32(tabs, 0xc40), width = I32(tabs, 0xc44);
        I32(tabs, 0xc28) = index;
        CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))((int32_t *)AT(tabs, 0xc90), 0, top,
                                                                                    width, bottom);
    }
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(tabs, 0x20), AT(tabs, 0xc90), 1);
    page = PAGE_OF(tabs, I32(tabs, 0xc28));
    if (!D32(page, 0x1100)) {
        VIRTUAL(page, 0xf0, void, (void *))(page);
        D32(PAGE_OF(tabs, I32(tabs, 0xc28)), 0x1100) = 1;
        CALL_THIS(MFC_6215, int32_t, (void *, int32_t))(PAGE_OF(tabs, I32(tabs, 0xc28)), 5);
        CALL_THIS(MFC_6197, int32_t, (void *, const void *, int32_t, int32_t, int32_t, int32_t, uint32_t))(
            PAGE_OF(tabs, I32(tabs, 0xc28)), fs98_simsui_imports[SIMSUI_IMPORT_MFC_6379], I32(tabs, 0xc50),
            I32(tabs, 0xc54), 0, 0, 0x41);
    } else {
        CALL_THIS(MFC_6215, int32_t, (void *, int32_t))(page, 5);
        CALL_THIS(MFC_6197, int32_t, (void *, const void *, int32_t, int32_t, int32_t, int32_t, uint32_t))(
            PAGE_OF(tabs, I32(tabs, 0xc28)), fs98_simsui_imports[SIMSUI_IMPORT_MFC_6379], 0, 0, 0, 0, 0x43);
    }
    if (!first && changed) {
        uint32_t how = (changed ? 4 : 0) | (first ? 2 : 0) | (from_self ? 1 : 0);
        uint32_t which = ((uint32_t)index & 0xffff) << 16 | ((uint32_t)current & 0xffff);
        CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(
            D32(PAGE_OF(tabs, I32(tabs, 0xc28)), 0x20), 0x5f9, how, which);
    }
    return 1;
}
/* 0x20480920 */
const void *TC fs98_simsui_tabs_message_map(const void *tabs) { (void)tabs; return IMAGE(0x17158); }
/* 0x20480930, OnDrawItem: DrawItem (virtual) */
void TC fs98_simsui_tabs_on_draw_item(void *tabs, int32_t id, void *item)
{
    (void)id;
    VIRTUAL(tabs, 0xc0, void, (void *, void *))(tabs, item);
}
/* the palette's colours given to a DC's sections */
static void give_colours(void *pal, const uint8_t *memory)
{
    uint32_t active_process = 0, foreground_process = 0;
    uint32_t active = CALL_STD(GetActiveWindow, uint32_t, (void))();
    uint32_t foreground = CALL_STD(GetForegroundWindow, uint32_t, (void))();
    void *table;
    if (active) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(active, &active_process);
    if (foreground) CALL_STD(GetWindowThreadProcessId, uint32_t, (uint32_t, uint32_t *))(foreground, &foreground_process);
    if (!D32(pal, 0x808) && (CALL_STD(GetDeviceCaps, int32_t, (uint32_t, int32_t))(D32(memory, 8), 0x26) & 0x100))
        table = AT(pal, 0x408);
    else table = AT(pal, 8);
    CALL_STD(SetDIBColorTable, uint32_t, (uint32_t, uint32_t, uint32_t, void *))(D32(memory, 4), 0, 0x100, table);
}
#define BLIT(dc_handle, x, y, cx, cy, from, sx, sy) CALL_STD(BitBlt, int32_t, (uint32_t, int32_t, int32_t, int32_t, \
        int32_t, uint32_t, int32_t, int32_t, uint32_t))(dc_handle, x, y, cx, cy, from, sx, sy, 0xcc0020)
/* the active tab's rectangle kept as both old and new */
static void keep_active_rect(void *tabs, int32_t top, int32_t bottom)
{
    int32_t at;
    CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))((int32_t *)AT(tabs, 0xc90), 0, top,
                                                                                I32(tabs, 0xc44), bottom);
    for (at = 0; at < 4; ++at) D32(tabs, 0xc70 + at * 4) = D32(tabs, 0xc90 + at * 4);
}
/* 0x20480940, DrawItem: each tab's frame for its state (current; pressed, disabled), and
 * the area under them */
void TC fs98_simsui_tabs_draw_item(void *tabs, const uint8_t *item)
{
    uint8_t dc[0x10], memory[0x10];
    int32_t frames = 4, area[4], at;
    uint32_t state;
    void *old_pal, *old, *bitmap;
    CALL_THIS(MFC_323, void *, (void *))(dc);
    CALL_THIS(MFC_323, void *, (void *))(memory);
    state = D32(item, 0x10);
    if (!PTR(tabs, 0xc38)) {
        CALL_THIS(MFC_640, void, (void *))(memory);
        CALL_THIS(MFC_640, void, (void *))(dc);
        return;
    }
    if (I32(tabs, 0x9ec) < 0xa0) frames = 2;
    CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(area, 0, 0, I32(tabs, 0xc44) - 0x14,
                                                                                I32(tabs, 0xc40));
    CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(dc, D32(item, 0x18));
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(D32(dc, 4)))) {
        old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(tabs, 0xc38), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        bitmap = AT(tabs, 0xab0);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        give_colours(PTR(tabs, 0xc38), memory);
        D32(tabs, 0xcb8) = D32(PTR(tabs, 0xc38), 0x808);
        for (at = 0; at < I32(tabs, 0xc24); ++at) {
            bitmap = AT(ENTRY(tabs, at), 0x60);
            CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
            if (frames == 2) {
                if (at == I32(tabs, 0xc28)) {
                    BLIT(D32(dc, 4), 0, area[1], I32(tabs, 0xc44), I32(tabs, 0xc40), D32(memory, 4), 0, 0);
                    keep_active_rect(tabs, area[1], area[3]);
                } else BLIT(D32(dc, 4), 0, area[1], I32(tabs, 0xc44), I32(tabs, 0xc40), D32(memory, 4), 0, I32(tabs, 0xc40));
            } else if (at == I32(tabs, 0xc28)) {
                int32_t row = (state & 0x10) ? I32(tabs, 0xc40) * 2 : 0;
                BLIT(D32(dc, 4), 0, area[1], I32(tabs, 0xc44), I32(tabs, 0xc40), D32(memory, 4), 0, row);
                keep_active_rect(tabs, area[1], area[3]);
            } else {
                int32_t row = I32(tabs, 0xc40);
                if ((state & 4) || DISABLED(tabs, at)) row = I32(tabs, 0xc40) * 3;
                BLIT(D32(dc, 4), 0, area[1], I32(tabs, 0xc44), I32(tabs, 0xc40), D32(memory, 4), 0, row);
            }
            area[1] += I32(tabs, 0xc40);
            area[3] += I32(tabs, 0xc40);
        }
        bitmap = AT(tabs, 0xab0);
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        BLIT(D32(dc, 4), 0, I32(tabs, 0xc40) * I32(tabs, 0xc24), I32(tabs, 0xc44), I32(tabs, 0xc4c), D32(memory, 4), 0,
             I32(tabs, 0xc40));
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
        if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 0);
    }
    CALL_THIS(MFC_2450, uint32_t, (void *))(dc);
    D32(tabs, 0xcb4) = 0;
    CALL_STD(ValidateRect, int32_t, (uint32_t, const void *))(D32(tabs, 0x20), 0);
    CALL_THIS(MFC_640, void, (void *))(memory);
    CALL_THIS(MFC_640, void, (void *))(dc);
}
/* 0x20480de0, OnLButtonDown: the tab under the point kept, when on its shape */
void TC fs98_simsui_tabs_button_down(void *tabs, uint32_t keys, int32_t x, int32_t y)
{
    int32_t index = *(volatile int32_t *)&(volatile int32_t){y / I32(tabs, 0xc40)};
    (void)keys;
    FS98_SIMSUI_BARRIER();
    D32(tabs, 0xc6c) = 0xffffffffu;
    if (index > I32(tabs, 0xc20)) return;
    CALL_THIS(MFC_5981, void *, (void *))(tabs);
    if (CALL_STD(PtInRect, int32_t, (const int32_t *, int32_t, int32_t))((const int32_t *)AT(tabs, 0xc90), x, y)) return;
    {
        int32_t into = y % I32(tabs, 0xc40);
        int32_t stride = (I32(tabs, 0x4c) + 3) & ~3;
        if (!*((const uint8_t *)PTR(tabs, 0xc14) + into * stride + x)) I32(tabs, 0xc6c) = index;
    }
}
/* 0x20480e60, OnLButtonUp: released on the tab pressed, it is made active */
void TC fs98_simsui_tabs_button_up(void *tabs, uint32_t keys, int32_t x, int32_t y)
{
    int32_t pressed = I32(tabs, 0xc6c);
    (void)keys; (void)x;
    if (pressed == -1) return;
    D32(tabs, 0xc6c) = 0xffffffffu;
    if (*(volatile int32_t *)&(volatile int32_t){y / I32(tabs, 0xc40)} == pressed)
        fs98_simsui_tabs_set_active_tab(tabs, pressed, 0);
}
/* 0x20480ea0, OnEraseBkgnd: the tabs' resting frames */
int32_t TC fs98_simsui_tabs_erase(void *tabs, void *dc)
{
    uint8_t memory[0x10];
    int32_t area[4], at;
    void *old_pal, *old, *bitmap;
    CALL_THIS(MFC_323, void *, (void *))(memory);
    if (CALL_THIS(MFC_1640, int32_t, (void *, uint32_t))(memory, CALL_STD(CreateCompatibleDC, uint32_t, (uint32_t))(dc ? D32(dc, 4) : 0))) {
        CALL_STD(SetRect, int32_t, (int32_t *, int32_t, int32_t, int32_t, int32_t))(area, 0, 0, I32(tabs, 0xc44) - 0x14,
                                                                                    I32(tabs, 0xc40));
        old_pal = CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, PTR(tabs, 0xc38), 0);
        CALL_STD(RealizePalette, uint32_t, (uint32_t))(D32(dc, 4));
        bitmap = AT(tabs, 0xa0);
        old = CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
        for (at = 0; at < I32(tabs, 0xc24); ++at) {
            bitmap = AT(ENTRY(tabs, at), 0x60);
            CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), bitmap ? D32(bitmap, 4) : 0);
            give_colours(PTR(tabs, 0xc38), memory);
            if (at == I32(tabs, 0xc28)) {
                BLIT(D32(dc, 4), 0, area[1], I32(tabs, 0xc44), I32(tabs, 0xc40), D32(memory, 4), 0, 0);
                keep_active_rect(tabs, area[1], area[3]);
            } else BLIT(D32(dc, 4), 0, area[1], I32(tabs, 0xc44), I32(tabs, 0xc40), D32(memory, 4), 0, I32(tabs, 0xc40));
            area[1] += I32(tabs, 0xc40);
            area[3] += I32(tabs, 0xc40);
        }
        CALL_STD(MFC_5785, void *, (uint32_t, uint32_t))(D32(memory, 4), old ? D32(old, 4) : 0);
        if (old_pal) CALL_THIS(MFC_5791, void *, (void *, void *, int32_t))(dc, old_pal, 0);
    }
    CALL_THIS(MFC_640, void, (void *))(memory);
    return 1;
}
#undef BLIT
/* 0x20481170, OnGetDlgCode */
uint32_t TC fs98_simsui_tabs_dialog_code(void *tabs) { (void)tabs; return 0x2003; }
/* 0x20481180, OnNcPaint: unless drawing, repainted */
void TC fs98_simsui_tabs_nc_paint(void *tabs)
{
    if (D32(tabs, 0xcb4) == 1) return;
    CALL_STD(InvalidateRect, int32_t, (uint32_t, const void *, int32_t))(D32(tabs, 0x20), 0, 1);
}
/* the next enabled tab after (step 1) or before (step -1) `from`, wrapping */
static int32_t enabled_tab(void *tabs, int32_t from, int32_t step)
{
    int32_t at = from;
    do {
        at += step;
        if (at > I32(tabs, 0xc20)) at = 0;
        if (at < 0) at = I32(tabs, 0xc20);
    } while (DISABLED(tabs, at));
    return at;
}
/* 0x204811a0, OnKeyDown: Home and End the first and last enabled tab, Left and Down the
 * next, Up and Right the one before; Tab moves the focus into the page (Shift: out of
 * it); other keys go to the parent */
void TC fs98_simsui_tabs_key_down(void *tabs, uint32_t key, uint32_t repeat, uint32_t flags)
{
    uint32_t inside[4] = {0, 0, 0, 0}, around[4] = {0, 0, 0, 0}, target;
    int32_t current = I32(tabs, 0xc28);
    switch (key) {
    case 0x23: fs98_simsui_tabs_set_active_tab(tabs, enabled_tab(tabs, I32(tabs, 0xc20) + 1, -1), D32(tabs, 0x20)); return;
    case 0x24: fs98_simsui_tabs_set_active_tab(tabs, enabled_tab(tabs, -1, 1), D32(tabs, 0x20)); return;
    case 0x25: case 0x28: fs98_simsui_tabs_set_active_tab(tabs, enabled_tab(tabs, current, 1), D32(tabs, 0x20)); return;
    case 0x26: case 0x27: fs98_simsui_tabs_set_active_tab(tabs, enabled_tab(tabs, current, -1), D32(tabs, 0x20)); return;
    case 9: {
        uint8_t shift = (uint8_t)((uint16_t)CALL_STD(GetAsyncKeyState, int16_t, (int32_t))(0x10) >> 8);
        void *page = PAGE_OF(tabs, I32(tabs, 0xc28));
        around[3] = D32(page, 0x20);
        around[2] = D32(tabs, 0x20);
        inside[2] = D32(tabs, 0x20);
        inside[3] = D32(PAGE_OF(tabs, I32(tabs, 0xc28)), 0x20);
        CALL_STD(EnumChildWindows, int32_t, (uint32_t, const void *, void *))(
            D32(PAGE_OF(tabs, I32(tabs, 0xc28)), 0x20), (const void *)fs98_simsui_find_focusable, inside);
        CALL_STD(EnumChildWindows, int32_t, (uint32_t, const void *, void *))(
            D32(CALL_STD(MFC_2864, void *, (uint32_t))(CALL_STD(GetParent, uint32_t, (uint32_t))(D32(tabs, 0x20))), 0x20),
            (const void *)fs98_simsui_find_focusable, around);
        if (shift) target = around[1] ? around[1] : D32(tabs, 0x20);
        else target = inside[0] ? inside[0] : around[0] ? around[0] : D32(tabs, 0x20);
        if (target) {
            CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(
                D32(PAGE_OF(tabs, I32(tabs, 0xc28)), 0x20), 0x5fa, target, 0);
            return;
        }
        break;
    }
    default: break;
    }
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(
        CALL_STD(GetParent, uint32_t, (uint32_t))(D32(tabs, 0x20)), 0x100, key,
        (flags & 0xffff) << 16 | (repeat & 0xffff));
}
/* 0x20481410, NextTab; 0x204814c0, PrevTab: the next (or previous) enabled tab made
 * active, and the focus to its first focusable child (or the tab control) */
static void step_tab(void *tabs, int32_t step)
{
    uint32_t found[4] = {0, 0, 0, 0}, target;
    int32_t index = enabled_tab(tabs, I32(tabs, 0xc28), step);
    void *page;
    fs98_simsui_tabs_set_active_tab(tabs, index, 0);
    page = PAGE_OF(tabs, index);
    found[2] = D32(tabs, 0x20);
    found[3] = D32(page, 0x20);
    CALL_STD(EnumChildWindows, int32_t, (uint32_t, const void *, void *))(D32(PAGE_OF(tabs, index), 0x20),
                                                                         (const void *)fs98_simsui_find_focusable, found);
    target = found[0] ? found[0] : D32(tabs, 0x20);
    CALL_STD(PostMessageA, int32_t, (uint32_t, uint32_t, uint32_t, uint32_t))(D32(PAGE_OF(tabs, index), 0x20), 0x5fa,
                                                                             target, 0);
}
void TC fs98_simsui_tabs_next(void *tabs) { step_tab(tabs, 1); }
void TC fs98_simsui_tabs_previous(void *tabs) { step_tab(tabs, -1); }
/* 0x20481560, Close: each page's (empty, 0x204716a0, the same as 0x204710b0) close, the mask freed */
void TC fs98_simsui_tabs_close(void *tabs)
{
    int32_t at;
    for (at = 0; at < I32(tabs, 0xc24); ++at)
        if (PAGE_OF(tabs, at)) fs98_simsui_nothing(PAGE_OF(tabs, at));
    if (D32(tabs, 0xc14)) mfc_operator_delete(PTR(tabs, 0xc14));
    D32(tabs, 0xc14) = 0;
}
/* 0x204815c0, OnSysKeyUp: Alt and a tab's mnemonic make it active */
void TC fs98_simsui_tabs_sys_key_up(void *tabs, uint32_t key, uint32_t repeat, uint32_t flags)
{
    int32_t at;
    (void)repeat;
    for (at = 0; at < I32(tabs, 0xc24); ++at)
        if (MNEMONIC(tabs, at) == (uint8_t)key && (flags & 0x2000)) {
            fs98_simsui_tabs_set_active_tab(tabs, at, 0);
            return;
        }
    CALL_THIS(MFC_2379, uint32_t, (void *))(tabs);
}
