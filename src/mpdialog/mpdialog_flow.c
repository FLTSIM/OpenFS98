/* Isolated replacement for MP_DIALOG registry entry 0x110a.
 * The five page classes borrow their owner descriptor; HOST owns the HWND
 * array's dispatch and teardown loop. */
#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mpdialog_internal.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define S32(object, offset) (*(int32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_MPDIALOG_THISCALL
#define SC FS98_MPDIALOG_STDCALL

struct fs98_flow_rect { int32_t left, top, right, bottom; };
struct fs98_flow_control { uint16_t offset; const char *ctor; const char *dtor; };

extern void *fs98_mpdialog_simsui(const char *name);
extern void *fs98_mpdialog_mfc(uint32_t ordinal);
extern void *fs98_mpdialog_import_va(uint32_t iat_va);
extern void *fs98_mpdialog_allocate(uint32_t size);
extern void fs98_mpdialog_free(void *object);
extern uint32_t fs98_mpdialog_instance(void);
extern uint32_t fs98_mpdialog_resource_module(void);
extern int32_t fs98_mpdialog_create_window(void *object, uint32_t template_id, void *parent);
extern int32_t fs98_mpdialog_show_window(void *object, int32_t show);
extern int32_t fs98_mpdialog_base_init(void *object);
extern void fs98_mpdialog_ddx_control(void *exchange, uint32_t id, void *control);
extern int32_t fs98_mpdialog_enable(void *control, int32_t value);
extern int32_t fs98_mpdialog_show_control(void *control, int32_t value);
extern void fs98_mpdialog_focus_control(void *control);
static void flow_capture_text(void *control, char *buffer, uint32_t capacity);
extern void *fs98_mpdialog_flow_vtables[5][62];
extern struct fs98_mpdialog_message_map fs98_mpdialog_maps[7];

static const uint32_t s_flow_sizes[5] = {0x19f0, 0x1cb0, 0x1d5c, 0x18fc, 0x1af0};
static const uint32_t s_flow_descriptor_offsets[5] = {0x19ec, 0x1cac, 0x1d58, 0x18f8, 0x1ae8};
static const uint32_t s_flow_templates[5] = {0x65, 0x66, 0x67, 0x68, 0x69};
static uint32_t s_flow_hwnds[6];
static void *s_flow_objects[5];
static uint32_t s_flow_current, s_flow_previous;
static void flow_connection_mode(void *object, uint32_t mode);

#define CTOR(type) "??0" type "@@QAE@XZ"
#define DTOR(type) "??1" type "@@UAE@XZ"
#define FLOW_BTN(offset) {offset, CTOR("CSims3PieceButton"), DTOR("CSims3PieceButton")}
#define FLOW_TEXT(offset) {offset, CTOR("CSimsText"), DTOR("CSimsText")}
#define FLOW_RADIO(offset) {offset, CTOR("CSimsRadioButton"), DTOR("CSimsRadioButton")}
#define FLOW_EDIT(offset) {offset, CTOR("CSimsEdit"), DTOR("CSimsEdit")}
#define FLOW_CHECK(offset) {offset, CTOR("CSimsCheckButton"), DTOR("CSimsCheckButton")}
#define FLOW_PICTURE(offset) {offset, CTOR("CSimsPicture"), DTOR("CSimsPicture")}
#define FLOW_ART(offset) {offset, CTOR("CSimsTextArt"), DTOR("CSimsTextArt")}
#define FLOW_LIST(offset) {offset, CTOR("CSimsListBox"), DTOR("CSimsListBox")}

static const struct fs98_flow_control s_flow_controls[5][20] = {
    {
        FLOW_BTN(0x10dc), FLOW_BTN(0x11d8), FLOW_BTN(0x12d4),
        FLOW_TEXT(0x13d0), FLOW_TEXT(0x1488), FLOW_TEXT(0x1540), FLOW_TEXT(0x15f8), FLOW_TEXT(0x16b0),
        FLOW_CHECK(0x1768), FLOW_PICTURE(0x1830), FLOW_ART(0x1900), {0, 0, 0}
    },
    {
        FLOW_BTN(0x10dc), FLOW_BTN(0x11d8), FLOW_BTN(0x12d4), FLOW_BTN(0x13d0),
        FLOW_TEXT(0x14cc), FLOW_TEXT(0x1584), FLOW_TEXT(0x163c), FLOW_TEXT(0x16f4),
        FLOW_EDIT(0x17ac), FLOW_RADIO(0x1814), FLOW_RADIO(0x1908), FLOW_RADIO(0x19fc),
        FLOW_PICTURE(0x1af0), FLOW_ART(0x1bc0), {0, 0, 0}
    },
    {
        FLOW_BTN(0x10dc), FLOW_BTN(0x11d8), FLOW_BTN(0x12d4), FLOW_BTN(0x13d0),
        FLOW_TEXT(0x14cc), FLOW_TEXT(0x1584), FLOW_TEXT(0x163c),
        {0x16f4, 0, 0}, {0x1734, 0, 0}, FLOW_TEXT(0x1774), FLOW_TEXT(0x182c),
        FLOW_EDIT(0x18e4), FLOW_EDIT(0x194c), FLOW_CHECK(0x19b4), FLOW_TEXT(0x1a7c),
        FLOW_EDIT(0x1b34), FLOW_PICTURE(0x1b9c), FLOW_ART(0x1c6c), {0, 0, 0}
    },
    {
        FLOW_BTN(0x10dc), FLOW_BTN(0x11d8), FLOW_BTN(0x12d4), FLOW_BTN(0x13d0),
        FLOW_TEXT(0x14cc), FLOW_TEXT(0x1584), FLOW_TEXT(0x163c), FLOW_LIST(0x16f4),
        FLOW_PICTURE(0x173c), FLOW_ART(0x180c), {0, 0, 0}
    },
    {
        FLOW_BTN(0x10dc), FLOW_BTN(0x11d8), FLOW_BTN(0x12d4), FLOW_BTN(0x13d0),
        FLOW_BTN(0x14cc), FLOW_BTN(0x15c8), FLOW_TEXT(0x16c4), FLOW_TEXT(0x177c),
        FLOW_TEXT(0x1834), FLOW_LIST(0x18ec), FLOW_ART(0x1934), FLOW_CHECK(0x1a1c), {0, 0, 0}
    }
};

static void flow_call_control(const char *symbol, void *object, uint32_t offset)
{
    ((void (TC *)(void *))fs98_mpdialog_simsui(symbol))(AT(object, offset));
}

static void flow_construct(void *object, uint32_t page, uint32_t parent)
{
    (void)parent;
    uint32_t index;
    ((void *(TC *)(void *, int32_t, void *))
        fs98_mpdialog_simsui("??0CSimsDialog@@QAE@HPAVCWnd@@@Z"))(
            object, (int32_t)s_flow_templates[page], 0);
    for (index = 0; index < 20 && s_flow_controls[page][index].offset; ++index) {
        const struct fs98_flow_control *control = &s_flow_controls[page][index];
        if (page == 2 && (control->offset == 0x16f4 || control->offset == 0x1734)) {
            ((void (TC *)(void *))fs98_mpdialog_mfc(567))(AT(object, control->offset));
            *(void **)AT(object, control->offset) = fs98_mpdialog_import_va(0x2234b42c);
        } else if (control->ctor) {
            flow_call_control(control->ctor, object, control->offset);
        }
    }
    PTR(object, 0) = fs98_mpdialog_flow_vtables[page];
}

static void flow_destroy_body(void *object, uint32_t page)
{
    uint32_t index = 20;
    while (index) {
        const struct fs98_flow_control *control;
        --index;
        control = &s_flow_controls[page][index];
        if (!control->offset) continue;
        if (page == 2 && (control->offset == 0x16f4 || control->offset == 0x1734))
            ((void (TC *)(void *))fs98_mpdialog_mfc(793))(AT(object, control->offset));
        else if (control->dtor)
            flow_call_control(control->dtor, object, control->offset);
    }
    ((void (TC *)(void *))fs98_mpdialog_simsui("??1CSimsDialog@@UAE@XZ"))(object);
}

#define FLOW_DELETE_BODY(name, page) \
    void TC fs98_mpdialog_flow_delete_body_##name(void *object) { flow_destroy_body(object, page); }
#define FLOW_DELETE(name, page) \
    void *TC fs98_mpdialog_flow_delete_##name(void *object, uint32_t flags) { \
        flow_destroy_body(object, page); if (flags & 1) fs98_mpdialog_free(object); return object; }
FLOW_DELETE_BODY(intro, 0) FLOW_DELETE_BODY(choice, 1) FLOW_DELETE_BODY(host, 2)
FLOW_DELETE_BODY(connection, 3) FLOW_DELETE_BODY(browser, 4)
FLOW_DELETE(intro, 0) FLOW_DELETE(choice, 1) FLOW_DELETE(host, 2)
FLOW_DELETE(connection, 3) FLOW_DELETE(browser, 4)

static uint32_t flow_create_page(uint32_t page, uint32_t parent, void *descriptor)
{
    void *object = fs98_mpdialog_allocate(s_flow_sizes[page]);
    flow_construct(object, page, parent);
    PTR(object, s_flow_descriptor_offsets[page]) = descriptor;
    D32(object, 0x80) = fs98_mpdialog_instance();
    (void)fs98_mpdialog_create_window(object, s_flow_templates[page], 0);
    (void)fs98_mpdialog_show_window(object, SW_HIDE);
    s_flow_objects[page] = object;
    return D32(object, 0x20);
}

uint32_t SC fs98_mpdialog_flow_create(uint32_t parent, void *descriptor)
{
    memset(s_flow_hwnds, 0, sizeof(s_flow_hwnds));
    memset(s_flow_objects, 0, sizeof(s_flow_objects));
    s_flow_current = s_flow_previous = 0;
    s_flow_hwnds[3] = flow_create_page(3, parent, descriptor);
    s_flow_hwnds[2] = flow_create_page(2, parent, descriptor);
    s_flow_hwnds[4] = flow_create_page(4, parent, descriptor);
    s_flow_hwnds[1] = flow_create_page(1, parent, descriptor);
    s_flow_hwnds[0] = flow_create_page(0, parent, descriptor);
    s_flow_hwnds[5] = 0;
    if (D32(descriptor, 0x1c) == 0)
        PostMessageA((HWND)s_flow_hwnds[0], WM_COMMAND, 0x3e8, 0);
    return (uint32_t)(uintptr_t)s_flow_hwnds;
}

static void *flow_block(void *object, uint32_t page)
{ return PTR(object, s_flow_descriptor_offsets[page]); }

static void flow_display(void *descriptor)
{
    uint32_t current = s_flow_current, previous = s_flow_previous;
    (void)descriptor;
    if (current < 5) (void)ShowWindow((HWND)s_flow_hwnds[current], SW_SHOW);
    if (current != previous && previous < 5)
        (void)ShowWindow((HWND)s_flow_hwnds[previous], SW_HIDE);
}

void TC fs98_mpdialog_flow_close(void *object)
{
    ((void (TC *)(void *))fs98_mpdialog_simsui(
        "?Close@CSimsDialog@@UAEXXZ"))(object);
}

static int32_t flow_set_state(void *descriptor, uint32_t target)
{
    s_flow_previous = s_flow_current;
    if (target >= 5) return -1;
    s_flow_current = target;
    if (s_flow_current != s_flow_previous) flow_display(descriptor);
    return 0;
}

static int32_t flow_advance(void *descriptor)
{
    uint32_t target;
    switch (s_flow_current) {
    case 0: target = 1; break;
    case 1: target = D32(descriptor, 0x24) == 1 ? 2 : 3; break;
    case 2: target = 3; break;
    case 3: target = 4; break;
    default: s_flow_previous = s_flow_current; return -1;
    }
    return flow_set_state(descriptor, target);
}

static int32_t flow_back(void *descriptor)
{
    uint32_t target;
    switch (s_flow_current) {
    case 0: s_flow_previous = s_flow_current; return -1;
    case 1: target = 0; break;
    case 2: target = 1; break;
    case 3: target = D32(descriptor, 0x24) == 1 ? 2 : 1; break;
    case 4: target = 3; break;
    default: s_flow_previous = s_flow_current; return -1;
    }
    return flow_set_state(descriptor, target);
}

static void flow_finish(void *descriptor, uint32_t result)
{
    static const uint32_t s_pages[] = {0, 1, 4, 2, 3};
    static const uint32_t s_state_offsets[] = {0x19e8, 0x1ca8, 0x1ae4, 0x1d54, 0x18f4};
    uint32_t index;
    D32(descriptor, 0) = result;
    for (index = 0; index < 5; ++index) {
        uint32_t page = s_pages[index];
        void *object = s_flow_objects[page];
        void **vtable = PTR(object, 0);
        D32(object, s_state_offsets[page]) = result ? 1 : 2;
        ((void (TC *)(void *))vtable[0xec / 4])(object);
    }
}

static void flow_capture(void *object)
{
    void **vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xf4 / 4])(object);
}

#define FLOW_MAP(name, index) \
    void *TC fs98_mpdialog_flow_message_map_##name(void *object) { \
        (void)object; return &fs98_mpdialog_maps[index]; }
FLOW_MAP(intro, 2) FLOW_MAP(choice, 3) FLOW_MAP(host, 4)
FLOW_MAP(connection, 5) FLOW_MAP(browser, 6)

static void flow_exchange(void *object, void *exchange, const uint32_t (*ddx)[2], uint32_t count)
{
    uint32_t index;
    ((void (TC *)(void *, void *))fs98_mpdialog_simsui(
        "?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z"))(object, exchange);
    for (index = 0; index < count; ++index)
        fs98_mpdialog_ddx_control(exchange, ddx[index][1], AT(object, ddx[index][0]));
}

#define FLOW_EXCHANGE(name, table) \
    void TC fs98_mpdialog_flow_exchange_##name(void *object, void *exchange) { \
        flow_exchange(object, exchange, table, sizeof(table) / sizeof((table)[0])); }
static const uint32_t s_intro_ddx[][2] = {
    {0x10dc, 2}, {0x11d8, 0x3e8}, {0x12d4, 0x3e9}, {0x1900, 0x427},
    {0x13d0, 0x421}, {0x1488, 0x422}, {0x1540, 0x423}, {0x15f8, 0x424},
    {0x16b0, 0x425}, {0x1768, 0x42b}, {0x1830, 0x434}
};
static const uint32_t s_choice_ddx[][2] = {
    {0x10dc, 2}, {0x11d8, 0x3e8}, {0x13d0, 1}, {0x12d4, 0x3e9},
    {0x1bc0, 0x3f0}, {0x14cc, 0x3f1}, {0x1584, 0x3f2}, {0x163c, 0x42e},
    {0x16f4, 0x41f}, {0x17ac, 0x419}, {0x1814, 0x41c}, {0x1908, 0x41e},
    {0x19fc, 0x41d}, {0x1af0, 0x434}
};
static const uint32_t s_host_ddx[][2] = {
    {0x10dc, 2}, {0x11d8, 0x3e8}, {0x12d4, 0x3e9}, {0x13d0, 0x426},
    {0x1c6c, 0x3f5}, {0x14cc, 0x3f4}, {0x1584, 0x3f3}, {0x163c, 0x42f},
    {0x1774, 0x413}, {0x182c, 0x414}, {0x18e4, 0x40f}, {0x194c, 0x410},
    {0x16f4, 0x411}, {0x1734, 0x412}, {0x19b4, 0x3ed}, {0x1a7c, 0x420},
    {0x1b34, 0x41a}, {0x1b9c, 0x434}
};
static const uint32_t s_connection_ddx[][2] = {
    {0x10dc, 2}, {0x11d8, 0x3e8}, {0x12d4, 0x433}, {0x13d0, 0x3e9},
    {0x180c, 0x3f6}, {0x14cc, 0x3f7}, {0x1584, 0x3f8}, {0x163c, 0x3f9},
    {0x16f4, 0x41b}, {0x173c, 0x434}
};
static const uint32_t s_browser_ddx[][2] = {
    {0x10dc, 2}, {0x11d8, 0x3ff}, {0x12d4, 0x3e9}, {0x14cc, 0x3ec},
    {0x15c8, 0x401}, {0x13d0, 0x400}, {0x1934, 0x3fa}, {0x16c4, 0x3fb},
    {0x177c, 0x3fc}, {0x1834, 0x3fe}, {0x18ec, 0x3fd}, {0x1a1c, 0x430}
};
FLOW_EXCHANGE(intro, s_intro_ddx)
FLOW_EXCHANGE(choice, s_choice_ddx)
FLOW_EXCHANGE(host, s_host_ddx)
FLOW_EXCHANGE(connection, s_connection_ddx)
FLOW_EXCHANGE(browser, s_browser_ddx)

static void flow_init_button(void *object, uint32_t offset, uint32_t bitmap)
{
    struct fs98_flow_rect rect = {0, 0, 0, 0};
    void *init = fs98_mpdialog_simsui(
        "?Init@CSims3PieceButton@@QAEHPAVCSimsFont@@0IPAVCDIBSection@@PAVCDIBPal@@KHPAUHINSTANCE__@@HHVCRect@@@Z");
    ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t,
        uint32_t, uint32_t, int32_t, int32_t, struct fs98_flow_rect))init)(
            AT(object, offset), AT(object, 0x5a0), AT(object, 0x610), bitmap,
            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
            fs98_mpdialog_resource_module(), 1, 1, rect);
}

static void flow_init_art(void *object, uint32_t offset)
{
    ((int32_t (TC *)(void *, int32_t, void *, void *, uint32_t))fs98_mpdialog_simsui(
        "?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z"))(
            AT(object, offset), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
}

static void flow_init_text(void *object, uint32_t offset, uint32_t font_offset)
{
    ((void (TC *)(void *, void *))fs98_mpdialog_simsui(
        "?Init@CSimsText@@QAEXPAVCSimsFont@@@Z"))(AT(object, offset), AT(object, font_offset));
}

static void flow_init_picture(void *object, uint32_t offset, uint32_t bitmap)
{
    ((int32_t (TC *)(void *, int32_t, void *, void *, uint32_t, uint16_t))
        fs98_mpdialog_simsui("?Init@CSimsPicture@@QAEHIPAVCDIBSection@@PAVCDIBPal@@PAUHINSTANCE__@@G@Z"))(
            AT(object, offset), (int32_t)bitmap, AT(object, 0xff4), AT(object, 0x7e8),
            D32(object, 0x80), 0xf200);
}

static void flow_init_edit(void *object, uint32_t offset, uint32_t label_offset)
{
    void *font = ((void *(SC *)(int32_t))fs98_mpdialog_simsui(
        "?GetSimsBaseFont@@YGPAVCFont@@H@Z"))(2);
    ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))
        fs98_mpdialog_simsui("?Init@CSimsEdit@@QAEXPAVCFont@@KKKPAVCSimsText@@K@Z"))(
            AT(object, offset), font,
            0xffffff, 0, 0xff0000, AT(object, label_offset), 0);
}

static void flow_remap_spin_control(void *object, uint32_t offset)
{
    struct fs98_flow_rect rect = {0, 0, 0, 0};
    void (SC *remap)(struct fs98_flow_rect *, void *) =
        (void *)fs98_mpdialog_simsui("?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z");
    void (TC *move_window)(void *, int32_t, int32_t, int32_t, int32_t,
                           int32_t) = (void *)fs98_mpdialog_mfc(4299);
    remap(&rect, AT(object, offset));
    move_window(AT(object, offset), rect.left, rect.top,
        rect.right - rect.left, rect.bottom - rect.top, 1);
}

static void flow_init_check(void *object, uint32_t offset)
{
    void *control = AT(object, offset);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))
        PTR(PTR(control, 0), 0xc4))(control, AT(object, 0x4c0), 0x139,
            AT(object, 0xff4), AT(object, 0x7e8), fs98_mpdialog_resource_module());
}

static void flow_init_radio(void *object, uint32_t offset)
{
    void *control = AT(object, offset);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))
        PTR(PTR(control, 0), 0xc4))(control, AT(object, 0x530), 0x134,
            AT(object, 0xff4), AT(object, 0x7e8), fs98_mpdialog_resource_module());
}

static int32_t flow_base_page_init(void *object)
{
    int32_t initialized = fs98_mpdialog_base_init(object);
    int32_t (TC *base_init)(void *, int32_t, uint32_t, uint32_t);
    if (!initialized) return 0;
    base_init = (void *)PTR(PTR(object, 0), 0xe4);
    if (!base_init(object, 0x14f, fs98_mpdialog_resource_module(), 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    return 1;
}

static void flow_init_buttons(void *object, const uint32_t *offsets, const uint32_t *bitmaps,
                              uint32_t count)
{
    uint32_t index;
    for (index = 0; index < count; ++index) flow_init_button(object, offsets[index], bitmaps[index]);
}

static void flow_focus(void *object, uint32_t offset)
{ fs98_mpdialog_focus_control(AT(object, offset)); }

int32_t TC fs98_mpdialog_flow_on_init_intro(void *object)
{
    static const uint32_t s_buttons[] = {0x12d4, 0x11d8, 0x10dc};
    static const uint32_t s_bitmaps[] = {0x132, 0x132, 0x132};
    static const uint32_t s_texts[] = {0x13d0, 0x1488, 0x1540, 0x15f8, 0x16b0};
    uint32_t index;
    if (!flow_base_page_init(object)) return 0;
    flow_init_art(object, 0x1900);
    for (index = 0; index < sizeof(s_texts) / sizeof(s_texts[0]); ++index)
        flow_init_text(object, s_texts[index], 0x220);
    flow_init_check(object, 0x1768);
    flow_init_buttons(object, s_buttons, s_bitmaps, 3);
    flow_init_picture(object, 0x1830, 0x6f);
    fs98_mpdialog_enable(AT(object, 0x12d4), 0);
    fs98_mpdialog_enable(AT(object, 0x11d8), 1);
    fs98_mpdialog_enable(AT(object, 0x10dc), 1);
    fs98_mpdialog_flow_update_intro(object);
    flow_focus(object, 0x11d8);
    return 0;
}

void TC fs98_mpdialog_flow_update_intro(void *object)
{
    void *block = flow_block(object, 0);
    int32_t (TC *check)(void *, int32_t) =
        (void *)fs98_mpdialog_simsui("?SetChecked@CSimsCheckButton@@QAEHH@Z");
    if (block) check(AT(object, 0x1768), D32(block, 0x1c) != 0);
}

void TC fs98_mpdialog_flow_capture_intro(void *object)
{
    void *block = flow_block(object, 0);
    if (!block) return;
    D32(block, 0x1c) = D32(object, 0x1824) != 0;
}

int32_t TC fs98_mpdialog_flow_on_init_choice(void *object)
{
    static const uint32_t s_buttons[] = {0x12d4, 0x11d8, 0x13d0, 0x10dc};
    static const uint32_t s_bitmaps[] = {0x132, 0x132, 0x132, 0x132};
    static const uint32_t s_texts[] = {0x14cc, 0x1584, 0x163c, 0x16f4};
    uint32_t index;
    if (!flow_base_page_init(object)) return 0;
    flow_init_art(object, 0x1bc0);
    for (index = 0; index < 4; ++index) flow_init_text(object, s_texts[index], 0x220);
    flow_init_edit(object, 0x17ac, 0x16f4);
    flow_init_buttons(object, s_buttons, s_bitmaps, 4);
    flow_init_picture(object, 0x1af0, 0x6e);
    fs98_mpdialog_enable(AT(object, 0x12d4), 1);
    fs98_mpdialog_enable(AT(object, 0x11d8), 1);
    fs98_mpdialog_enable(AT(object, 0x10dc), 1);
    flow_init_radio(object, 0x1814);
    flow_init_radio(object, 0x1908);
    flow_init_radio(object, 0x19fc);
    ((int32_t (TC *)(void *, int32_t))fs98_mpdialog_simsui(
        "?SetChecked@CSimsRadioButton@@QAEHH@Z"))(AT(object, 0x1814), 1);
    {
        void *block = flow_block(object, 1);
        if (block)
            SendMessageA((HWND)D32(object, 0x17cc), EM_LIMITTEXT,
                         D32(block, 0x30) - 1, 0);
    }
    fs98_mpdialog_flow_update_choice(object);
    flow_focus(object, 0x17ac);
    return 0;
}

void TC fs98_mpdialog_flow_update_choice(void *object)
{
    void *block = flow_block(object, 1);
    int32_t (TC *set_checked)(void *, int32_t) =
        (void *)fs98_mpdialog_simsui("?SetChecked@CSimsRadioButton@@QAEHH@Z");
    uint32_t mode;
    if (!block) return;
    mode = D32(block, 0x24);
    if (D32(block, 0x2c)) SetWindowTextA((HWND)D32(object, 0x17cc), (const char *)D32(block, 0x2c));
    set_checked(AT(object, mode == 1 ? 0x1908 : (mode == 2 ? 0x19fc : 0x1814)), 1);
    if (mode == 2) {
        fs98_mpdialog_enable(AT(object, 0x13d0), 1);
        fs98_mpdialog_show_control(AT(object, 0x13d0), SW_SHOW);
        fs98_mpdialog_enable(AT(object, 0x11d8), 0);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_HIDE);
        fs98_mpdialog_enable(AT(object, 0x17ac), 0);
    } else {
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_SHOW);
        fs98_mpdialog_enable(AT(object, 0x11d8), 1);
        fs98_mpdialog_enable(AT(object, 0x13d0), 0);
        fs98_mpdialog_show_control(AT(object, 0x13d0), SW_HIDE);
        fs98_mpdialog_enable(AT(object, 0x17ac), 1);
    }
}

void TC fs98_mpdialog_flow_capture_choice(void *object)
{
    void *block = flow_block(object, 1);
    uint32_t button;
    if (!block) return;
    button = D32(object, 0x1834);
    if ((uint32_t)GetWindowLongA((HWND)button, GWL_USERDATA) == button) D32(block, 0x24) = 0;
    button = D32(object, 0x1928);
    if ((uint32_t)GetWindowLongA((HWND)button, GWL_USERDATA) == button) D32(block, 0x24) = 1;
    button = D32(object, 0x1a1c);
    if ((uint32_t)GetWindowLongA((HWND)button, GWL_USERDATA) == button) D32(block, 0x24) = 2;
    if ((uint32_t)GetWindowLongA((HWND)button, GWL_USERDATA) == button) D32(block, 0x24) = 2;
    flow_capture_text(AT(object, 0x17ac), (char *)(uintptr_t)D32(block, 0x2c), D32(block, 0x30));
}

int32_t TC fs98_mpdialog_flow_validate_choice(void *object, uint32_t control_id)
{
    void *block = flow_block(object, 1);
    int32_t result;
    (void)control_id;
    if (!IsWindowVisible((HWND)D32(object, 0x20)) || !block) return 0;
    fs98_mpdialog_flow_capture_choice(object);
    result = ((int32_t (SC *)(void *))D32(block, 0x10))(block);
    return result && D32(block, 0x28) ? 0x41a : 0;
}

int32_t TC fs98_mpdialog_flow_error_choice(void *object, int32_t field)
{
    void *block = flow_block(object, 1);
    uint32_t result = 0;
    if (!field) return 0;
    if (block && D32(block, 0x28)) {
        result = ((int32_t (TC *)(void *, void *, uint32_t))
            fs98_mpdialog_import_va(0x2234b414))(
                object, AT(object, 0x17ac), (uint32_t)(uintptr_t)AT(block, 0x28));
        D32(object, 0x8c) = result;
        if (result) field = 0x419;
    } else field = 0;
    return ((int32_t (TC *)(void *, int32_t))PTR(PTR(object, 0), 0xe0))(object, field);
}

static void flow_choice_route_update(void *object, uint32_t online)
{
    if (online) {
        fs98_mpdialog_enable(AT(object, 0x13d0), 1);
        fs98_mpdialog_show_control(AT(object, 0x13d0), SW_SHOW);
        fs98_mpdialog_enable(AT(object, 0x11d8), 0);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_HIDE);
        fs98_mpdialog_enable(AT(object, 0x17ac), 0);
    } else {
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_SHOW);
        fs98_mpdialog_enable(AT(object, 0x11d8), 1);
        fs98_mpdialog_enable(AT(object, 0x13d0), 0);
        fs98_mpdialog_show_control(AT(object, 0x13d0), SW_HIDE);
        fs98_mpdialog_enable(AT(object, 0x17ac), 1);
    }
}

void TC fs98_mpdialog_flow_route_standard(void *object)
{ fs98_mpdialog_flow_capture_choice(object); flow_choice_route_update(object, 0); }
static void flow_route_choice(void *object, uint32_t mode)
{
    uint32_t selected = D32(object, mode == 1 ? 0x1928 : (mode == 2 ? 0x1a1c : 0x1834));
    void *block = flow_block(object, 1);
    SetWindowLongA((HWND)D32(object, 0x1834), GWL_USERDATA, (LONG)selected);
    SetWindowLongA((HWND)D32(object, 0x1928), GWL_USERDATA, (LONG)selected);
    SetWindowLongA((HWND)D32(object, 0x1a1c), GWL_USERDATA, (LONG)selected);
    if (block) D32(block, 0x24) = mode;
    flow_choice_route_update(object, mode == 2);
}
void TC fs98_mpdialog_flow_route_join(void *object) { flow_route_choice(object, 0); }
void TC fs98_mpdialog_flow_route_host(void *object) { flow_route_choice(object, 1); }
void TC fs98_mpdialog_flow_route_online(void *object)
{ flow_route_choice(object, 2); }

void TC fs98_mpdialog_flow_next_intro(void *object)
{ flow_capture(object); (void)flow_advance(flow_block(object, 0)); }
void TC fs98_mpdialog_flow_back_intro(void *object)
{ (void)flow_back(flow_block(object, 0)); }
void TC fs98_mpdialog_flow_cancel_intro(void *object)
{ flow_finish(flow_block(object, 0), 0); }

void TC fs98_mpdialog_flow_next_choice(void *object)
{
    void *block = flow_block(object, 1);
    flow_capture(object);
    if (D32(block, 0x24) == 2) {
        D32(block, 0) = 1;
        if (((int32_t (SC *)(void *))D32(block, 0x0c))(block) == 0) flow_finish(block, 1);
    } else (void)flow_advance(block);
}
void TC fs98_mpdialog_flow_back_choice(void *object)
{ flow_capture(object); (void)flow_back(flow_block(object, 1)); }
void TC fs98_mpdialog_flow_cancel_choice(void *object)
{ flow_finish(flow_block(object, 1), 0); }

static uint32_t flow_atoi_pointer(void *pointer)
{ return pointer ? (uint32_t)strtoul((const char *)pointer, 0, 10) : 0; }

static void flow_capture_text(void *control, char *buffer, uint32_t capacity)
{
    if (buffer && capacity)
        (void)((int32_t (TC *)(void *, char *, int32_t))fs98_mpdialog_mfc(3873))(
            control, buffer, (int32_t)capacity);
}

void TC fs98_mpdialog_flow_capture_host(void *object)
{
    void *block = flow_block(object, 2);
    if (!block) return;
    flow_capture_text(AT(object, 0x1b34), (char *)(uintptr_t)D32(block, 0x4c), D32(block, 0x50));
    flow_capture_text(AT(object, 0x18e4), (char *)(uintptr_t)D32(block, 0x60), D32(block, 0x64));
    flow_capture_text(AT(object, 0x194c), (char *)(uintptr_t)D32(block, 0x74), D32(block, 0x78));
    D32(block, 0x40) = D32(object, 0x1a70) ? 2 : 1;
}

void TC fs98_mpdialog_flow_update_host(void *object)
{
    void *block = flow_block(object, 2);
    int32_t (TC *set_checked)(void *, int32_t) =
        (void *)fs98_mpdialog_simsui("?SetChecked@CSimsCheckButton@@QAEHH@Z");
    if (!block) return;
    if (D32(block, 0x4c)) SetWindowTextA((HWND)D32(object, 0x1b54), (const char *)D32(block, 0x4c));
    if (D32(block, 0x60)) SetWindowTextA((HWND)D32(object, 0x1904), (const char *)D32(block, 0x60));
    if (D32(block, 0x74)) SetWindowTextA((HWND)D32(object, 0x196c), (const char *)D32(block, 0x74));
    SendMessageA((HWND)D32(object, 0x1714), UDM_SETRANGE, 0,
        MAKELPARAM(flow_atoi_pointer((void *)(uintptr_t)D32(block, 0x6c)),
                   flow_atoi_pointer((void *)(uintptr_t)D32(block, 0x68))));
    SendMessageA((HWND)D32(object, 0x1754), UDM_SETRANGE, 0,
        MAKELPARAM(flow_atoi_pointer((void *)(uintptr_t)D32(block, 0x80)),
                   flow_atoi_pointer((void *)(uintptr_t)D32(block, 0x7c))));
    set_checked(AT(object, 0x19b4), D32(block, 0x40) == 2);
}

int32_t TC fs98_mpdialog_flow_validate_host(void *object, uint32_t control_id)
{
    void *block = flow_block(object, 2);
    int32_t result;
    (void)control_id;
    if (!IsWindowVisible((HWND)D32(object, 0x20)) || !block) return 0;
    fs98_mpdialog_flow_capture_host(object);
    result = ((int32_t (SC *)(void *))D32(block, 0x10))(block);
    if (!result) return 0;
    if (D32(block, 0x48)) return 0x41a;
    if (D32(block, 0x5c)) return 0x40f;
    if (D32(block, 0x70)) return 0x410;
    return 0;
}

int32_t TC fs98_mpdialog_flow_error_host(void *object, int32_t field)
{
    void *block = flow_block(object, 2);
    uint32_t status_offset = 0, control_offset = 0, result = 0;
    int32_t original_field = field;
    if (!field) return 0;
    if (block && D32(block, 0x48)) { status_offset = 0x48; control_offset = 0x1b34; field = 0x41a; }
    else if (block && D32(block, 0x5c)) { status_offset = 0x5c; control_offset = 0x18e4; field = 0x40f; }
    else if (block && D32(block, 0x70)) { status_offset = 0x70; control_offset = 0x194c; field = 0x410; }
    if (status_offset) {
        result = ((int32_t (TC *)(void *, void *, uint32_t))
            fs98_mpdialog_import_va(0x2234b414))(
                object, AT(object, control_offset), (uint32_t)(uintptr_t)AT(block, status_offset));
        D32(object, 0x8c) = result;
        if (!result) field = original_field;
    } else field = 0;
    return ((int32_t (TC *)(void *, int32_t))PTR(PTR(object, 0), 0xe0))(object, field);
}

void TC fs98_mpdialog_flow_next_host(void *object)
{
    int32_t field;
    flow_capture(object);
    field = ((int32_t (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xd8))(object, 0);
    if (!field) (void)flow_advance(flow_block(object, 2));
    else (void)fs98_mpdialog_flow_error_host(object, field);
}
void TC fs98_mpdialog_flow_back_host(void *object)
{ flow_capture(object); (void)flow_back(flow_block(object, 2)); }
void TC fs98_mpdialog_flow_cancel_host(void *object)
{ flow_finish(flow_block(object, 2), 0); }
void TC fs98_mpdialog_flow_settings_host(void *object)
{
    void *block = flow_block(object, 2);
    if (block) ((int32_t (SC *)(void *))D32(block, 0x44))(block);
}

int32_t TC fs98_mpdialog_flow_on_init_host(void *object)
{
    static const uint32_t s_buttons[] = {0x12d4, 0x11d8, 0x10dc, 0x13d0};
    static const uint32_t s_bitmaps[] = {0x132, 0x132, 0x132, 0x137};
    static const uint32_t s_texts[] = {0x14cc, 0x1584, 0x163c, 0x1774, 0x182c, 0x1a7c};
    uint32_t index;
    if (!flow_base_page_init(object)) return 0;
    flow_init_art(object, 0x1c6c);
    for (index = 0; index < sizeof(s_texts) / sizeof(s_texts[0]); ++index)
        flow_init_text(object, s_texts[index], 0x290);
    flow_init_edit(object, 0x1b34, 0x1a7c);
    flow_init_edit(object, 0x18e4, 0x1774);
    flow_init_edit(object, 0x194c, 0x182c);
    flow_remap_spin_control(object, 0x16f4);
    flow_remap_spin_control(object, 0x1734);
    flow_init_check(object, 0x19b4);
    flow_init_buttons(object, s_buttons, s_bitmaps, 4);
    flow_init_picture(object, 0x1b9c, 0x6d);
    fs98_mpdialog_enable(AT(object, 0x12d4), 1);
    fs98_mpdialog_enable(AT(object, 0x11d8), 1);
    fs98_mpdialog_enable(AT(object, 0x10dc), 1);
    fs98_mpdialog_enable(AT(object, 0x13d0), 1);
    {
        void *block = flow_block(object, 2);
        SendMessageA((HWND)D32(object, 0x1904), EM_SETLIMITTEXT, D32(block, 0x64) - 1, 0);
        SendMessageA((HWND)D32(object, 0x196c), EM_SETLIMITTEXT, D32(block, 0x78) - 1, 0);
        SendMessageA((HWND)D32(object, 0x1b54), EM_SETLIMITTEXT, D32(block, 0x50) - 1, 0);
    }
    fs98_mpdialog_flow_update_host(object);
    flow_focus(object, 0x1b34);
    return 0;
}

void TC fs98_mpdialog_flow_capture_connection(void *object)
{
    void *block = flow_block(object, 3);
    if (block) D32(block, 0x90) = (uint32_t)SendMessageA((HWND)D32(object, 0x1714), LB_GETCURSEL, 0, 0);
}

void TC fs98_mpdialog_flow_update_connection(void *object)
{
    void *block = flow_block(object, 3);
    HWND combo = (HWND)D32(object, 0x1714);
    uint32_t index, count;
    if (!block) return;
    SendMessageA(combo, LB_RESETCONTENT, 0, 0);
    count = D32(block, 0x88);
    for (index = 0; index < count; ++index) {
        char **providers = (char **)(uintptr_t)D32(block, 0x84);
        if (providers) SendMessageA(combo, LB_ADDSTRING, 0, (LPARAM)providers[index * 7]);
    }
    SendMessageA(combo, LB_SETCURSEL, D32(block, 0x90), 0);
    if (D32(block, 0x24) == 1) {
        fs98_mpdialog_show_control(AT(object, 0x12d4), SW_SHOW);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_HIDE);
    } else {
        fs98_mpdialog_show_control(AT(object, 0x12d4), SW_HIDE);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_SHOW);
    }
}

void TC fs98_mpdialog_flow_provider_connection(void *object)
{
    void **vtable = PTR(object, 0);
    ((void (TC *)(void *))vtable[0xcc / 4])(object);
}

void TC fs98_mpdialog_flow_activate_connection(void *object, uint32_t state,
                                               uint32_t window, uint32_t minimized)
{
    ((void (TC *)(void *, uint32_t, uint32_t, uint32_t))fs98_mpdialog_simsui(
        "?OnActivate@CSimsDialog@@IAEXIPAVCWnd@@H@Z"))(object, state, window, minimized);
    flow_connection_mode(object, 1);
}

void TC fs98_mpdialog_flow_next_connection(void *object)
{
    void *block = flow_block(object, 3);
    int32_t result;
    if (!block) return;
    flow_capture(object);
    if (D32(block, 0x24) == 1) {
        D32(block, 0) = 1;
        if (((int32_t (SC *)(void *))D32(block, 0x0c))(block) == 0) flow_finish(block, 1);
        return;
    }
    if (D32(block, 0x24) != 0) return;
    D32(block, 0x14) = D32(object, 0x20);
    fs98_mpdialog_enable(object, FALSE);
    result = ((int32_t (SC *)(void *))D32(block, 4))(block);
    fs98_mpdialog_enable(object, TRUE);
    if (!result) (void)flow_advance(block);
    else ((int32_t (SC *)(void *))D32(block, 8))(block);
}
void TC fs98_mpdialog_flow_back_connection(void *object)
{ flow_capture(object); (void)flow_back(flow_block(object, 3)); }
void TC fs98_mpdialog_flow_cancel_connection(void *object)
{ flow_finish(flow_block(object, 3), 0); }

static void flow_connection_mode(void *object, uint32_t focus)
{
    void *block = flow_block(object, 3);
    uint32_t mode;
    if (!block) return;
    mode = D32(block, 0x24);
    if (mode == 1) {
        fs98_mpdialog_enable(AT(object, 0x11d8), 0);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_HIDE);
        fs98_mpdialog_enable(AT(object, 0x12d4), 1);
        fs98_mpdialog_show_control(AT(object, 0x12d4), SW_SHOW);
        if (focus) flow_focus(object, 0x12d4);
    } else if (mode == 0 || mode == 2) {
        fs98_mpdialog_enable(AT(object, 0x11d8), 1);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_SHOW);
        fs98_mpdialog_enable(AT(object, 0x12d4), 0);
        fs98_mpdialog_show_control(AT(object, 0x12d4), SW_HIDE);
        if (focus) flow_focus(object, 0x11d8);
    }
}

int32_t TC fs98_mpdialog_flow_on_init_connection(void *object)
{
    static const uint32_t s_buttons[] = {0x12d4, 0x11d8, 0x10dc, 0x13d0};
    static const uint32_t s_bitmaps[] = {0x132, 0x132, 0x132, 0x132};
    static const uint32_t s_texts[] = {0x14cc, 0x1584, 0x163c};
    uint32_t index;
    if (!flow_base_page_init(object)) return 0;
    flow_init_art(object, 0x180c);
    for (index = 0; index < 3; ++index) flow_init_text(object, s_texts[index], 0x220);
    ((void (TC *)(void *, void *, void *))fs98_mpdialog_simsui(
        "?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z"))(
            AT(object, 0x16f4), AT(object, 0x450), AT(object, 0x163c));
    flow_init_buttons(object, s_buttons, s_bitmaps, 4);
    flow_init_picture(object, 0x173c, 0x6c);
    fs98_mpdialog_enable(AT(object, 0x12d4), 1);
    fs98_mpdialog_enable(AT(object, 0x11d8), 1);
    fs98_mpdialog_enable(AT(object, 0x10dc), 1);
    fs98_mpdialog_enable(AT(object, 0x13d0), 1);
    fs98_mpdialog_flow_update_connection(object);
    flow_connection_mode(object, 1);
    flow_focus(object, 0x11d8);
    return 0;
}

static void flow_browser_update_join(void *object);

static void flow_browser_rows(void *object)
{
    void *block = flow_block(object, 4);
    HWND list = (HWND)D32(object, 0x190c);
    uint8_t *rows;
    uint32_t count, index;
    if (!block || !list) return;
    SendMessageA(list, LB_RESETCONTENT, 0, 0);
    for (index = 0; index < D32(block, 0xb0); ++index) {
        uint8_t *row;
        rows = (uint8_t *)(uintptr_t)D32(block, 0xac);
        row = rows + index * 84;
        const char *name = (const char *)(uintptr_t)D32(row, 0);
        char *text;
        if (!name) name = "";
        text = malloc(strlen(name) + 64);
        strcpy(text, name);
        wsprintfA(text + strlen(name), "\t%d of %d\t%d of %d",
            S32(row, 0x28), S32(row, 0x20), S32(row, 0x2c), S32(row, 0x24));
        SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)text);
        free(text);
    }
    count = D32(block, 0xb0);
    SendMessageA(list, LB_SETCURSEL, count ? 0 : (WPARAM)-1, 0);
    fs98_mpdialog_enable(AT(object, 0x11d8), count != 0);
    fs98_mpdialog_enable(AT(object, 0x13d0), count != 0);
    flow_browser_update_join(object);
}

static void flow_browser_update_join(void *object)
{
    void *block = flow_block(object, 4);
    HWND list = (HWND)D32(object, 0x190c);
    uint32_t selected = (uint32_t)SendMessageA(list, LB_GETCURSEL, 0, 0);
    uint8_t *rows;
    uint8_t *row;
    uint32_t current_offset, capacity_offset;
    if (!block || selected == (uint32_t)-1 || selected >= D32(block, 0xb0)) {
        fs98_mpdialog_enable(AT(object, 0x11d8), 0);
        return;
    }
    rows = (uint8_t *)(uintptr_t)D32(block, 0xac);
    if (!rows) {
        fs98_mpdialog_enable(AT(object, 0x11d8), 0);
        return;
    }
    row = rows + selected * 84;
    current_offset = D32(object, 0x1ad8) ? 0x24 : 0x20;
    capacity_offset = D32(object, 0x1ad8) ? 0x2c : 0x28;
    fs98_mpdialog_enable(AT(object, 0x11d8),
        D32(row, current_offset) != D32(row, capacity_offset));
}

void TC fs98_mpdialog_flow_capture_browser(void *object)
{
    void *block = flow_block(object, 4);
    if (!block) return;
    D32(block, 0xb8) = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);
    D32(block, 0x40) = D32(object, 0x1ad8) ? 2 : 1;
}

void TC fs98_mpdialog_flow_update_browser(void *object)
{
    void *block = flow_block(object, 4);
    int32_t (TC *check)(void *, int32_t) =
        (void *)fs98_mpdialog_simsui("?SetChecked@CSimsCheckButton@@QAEHH@Z");
    if (!block) return;
    flow_browser_rows(object);
    fs98_mpdialog_enable(AT(object, 0x1a1c), D32(block, 0xa8) != 0);
    check(AT(object, 0x1a1c), D32(block, 0x40) == 2);
    flow_browser_update_join(object);
}

static void flow_browser_selection(void *object)
{
    void *block = flow_block(object, 4);
    uint32_t selection;
    if (!block) return;
    selection = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);
    fs98_mpdialog_enable(AT(object, 0x11d8), selection != (uint32_t)-1);
    selection = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);
    ((int32_t (SC *)(void *, uint32_t))D32(block, 0x98))(block, selection);
}

void TC fs98_mpdialog_flow_select_browser(void *object) { flow_browser_selection(object); }
void TC fs98_mpdialog_flow_select_type_browser(void *object)
{ flow_browser_selection(object); flow_capture(object); }

void TC fs98_mpdialog_flow_next_browser(void *object)
{
    void *block = flow_block(object, 4);
    flow_capture(object);
    if (!block) return;
    ((int32_t (SC *)(void *))D32(block, 0x0c))(block);
    flow_finish(block, 1);
}
void TC fs98_mpdialog_flow_back_browser(void *object)
{
    void *block = flow_block(object, 4);
    HWND list = (HWND)D32(object, 0x190c);
    if (!block) return;
    SendMessageA(list, LB_RESETCONTENT, 0, 0);
    SendMessageA(list, LB_SETCURSEL, (WPARAM)-1, 0);
    fs98_mpdialog_enable(AT(object, 0x11d8), 0);
    ((int32_t (SC *)(void *))D32(block, 8))(block);
    (void)flow_back(block);
}
void TC fs98_mpdialog_flow_cancel_browser(void *object)
{
    void *block = flow_block(object, 4);
    if (!block) return;
    D32(block, 0) = 0;
    ((int32_t (SC *)(void *))D32(block, 8))(block);
    flow_finish(block, 0);
}
void TC fs98_mpdialog_flow_details_browser(void *object)
{
    void *block = flow_block(object, 4);
    if (!block) return;
    D32(block, 0xb8) = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);
    if (D32(block, 0xb8) != (uint32_t)-1)
        ((int32_t (SC *)(void *))D32(block, 0x9c))(block);
}
void TC fs98_mpdialog_flow_settings_browser(void *object)
{
    void *block = flow_block(object, 4);
    if (block) ((int32_t (SC *)(void *))D32(block, 0xa4))(block);
}
void TC fs98_mpdialog_flow_refresh_browser(void *object)
{
    void *block = flow_block(object, 4);
    if (!block || D32(object, 0x1aec)) return;
    D32(object, 0x1aec) = 1;
    D32(block, 0xb8) = (uint32_t)SendMessageA((HWND)D32(object, 0x190c), LB_GETCURSEL, 0, 0);
    if (((int32_t (SC *)(void *))D32(block, 0xa0))(block))
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xf0))(object);
    D32(object, 0x1aec) = 0;
}
void TC fs98_mpdialog_flow_observer_browser(void *object)
{
    void *block = flow_block(object, 4);
    flow_browser_update_join(object);
    if (block) D32(block, 0x40) = D32(object, 0x1ad8) ? 2 : 1;
}
void TC fs98_mpdialog_flow_idle_browser(void *object, uint32_t wparam, uint32_t lparam)
{
    void *block = flow_block(object, 4);
    (void)wparam; (void)lparam;
    if (block) D32(block, 0x14) = 0;
}

int32_t TC fs98_mpdialog_flow_mouse_activate_browser(void *object, uint32_t active,
                                                     uint32_t hit_test, uint32_t message)
{
    int32_t result = ((int32_t (TC *)(void *))fs98_mpdialog_mfc(2379))(object);
    (void)active;
    (void)hit_test;
    (void)message;
    if (D32(object, 0x1aec)) return 4;
    return result;
}

void TC fs98_mpdialog_flow_show_browser(void *object, uint32_t show, uint32_t status)
{
    HWND window = (HWND)D32(object, 0x20);
    (void)status;
    ((void (TC *)(void *))fs98_mpdialog_mfc(2379))(object);
    (void)KillTimer(window, 1);
    if (show) (void)SetTimer(window, 1, 0x1f4, 0);
}

void TC fs98_mpdialog_flow_timer_browser(void *object, uint32_t timer_id)
{
    (void)KillTimer((HWND)D32(object, 0x20), timer_id);
    fs98_mpdialog_flow_refresh_browser(object);
}

int32_t TC fs98_mpdialog_flow_on_init_browser(void *object)
{
    static const uint32_t s_buttons[] = {0x12d4, 0x11d8, 0x10dc, 0x13d0, 0x14cc, 0x15c8};
    static const uint32_t s_bitmaps[] = {0x132, 0x132, 0x132, 0x137, 0x137, 0x137};
    static const uint32_t s_texts[] = {0x16c4, 0x177c, 0x1834};
    uint32_t index;
    if (!flow_base_page_init(object)) return 0;
    D32(object, 0x1aec) = 0;
    flow_init_art(object, 0x1934);
    for (index = 0; index < 3; ++index) flow_init_text(object, s_texts[index], 0x220);
    ((void (TC *)(void *, void *, void *))fs98_mpdialog_simsui(
        "?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z"))(
            AT(object, 0x18ec), AT(object, 0x450), AT(object, 0x1834));
    flow_init_check(object, 0x1a1c);
    flow_init_buttons(object, s_buttons, s_bitmaps, 6);
    fs98_mpdialog_enable(AT(object, 0x12d4), 1);
    fs98_mpdialog_enable(AT(object, 0x11d8), 0);
    fs98_mpdialog_enable(AT(object, 0x10dc), 1);
    fs98_mpdialog_enable(AT(object, 0x14cc), 1);
    fs98_mpdialog_flow_update_browser(object);
    flow_focus(object, 0x11d8);
    return 0;
}

