/* Isolated implementation of MP_DIALOG registry entry 0x110c.  The object
 * borrows the MULTIP-owned descriptor/player block at +0x1d78. */
#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "mpdialog_internal.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define S32(object, offset) (*(int32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_MPDIALOG_THISCALL
#define SC FS98_MPDIALOG_STDCALL

struct fs98_details_rect { int32_t left, top, right, bottom; };

extern void *fs98_mpdialog_simsui(const char *name);
extern void *fs98_mpdialog_mfc(uint32_t ordinal);
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
extern void *fs98_mpdialog_import_va(uint32_t iat_va);
extern void *fs98_mpdialog_details_vtables[64];
extern struct fs98_mpdialog_message_map fs98_mpdialog_maps[7];

static const uint32_t s_offsets[] = {
    0x10dc, 0x11d8, 0x12d4, 0x13d0, 0x14cc, 0x150c, 0x154c, 0x1604,
    0x16bc, 0x1724, 0x178c, 0x1844, 0x18ac, 0x1964, 0x19ac, 0x1a64,
    0x1b1c, 0x1bd4, 0x1c8c
};

static void details_call_ctor(const char *symbol, void *object, uint32_t offset)
{
    ((void (TC *)(void *))fs98_mpdialog_simsui(symbol))(AT(object, offset));
}

static void details_construct(void *object)
{
    uint32_t index;
    ((void *(TC *)(void *, int32_t, void *))
        fs98_mpdialog_simsui("??0CSimsDialog@@QAE@HPAVCWnd@@@Z"))(object, 0x6b, 0);
    for (index = 0; index < 4; ++index)
        details_call_ctor("??0CSims3PieceButton@@QAE@XZ", object, s_offsets[index]);
    for (index = 4; index <= 5; ++index) {
        void *spin = AT(object, s_offsets[index]);
        ((void (TC *)(void *))fs98_mpdialog_mfc(567))(spin);
        *(void **)spin = fs98_mpdialog_import_va(0x2234b42c);
    }
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x154c);
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x1604);
    details_call_ctor("??0CSimsEdit@@QAE@XZ", object, 0x16bc);
    details_call_ctor("??0CSimsEdit@@QAE@XZ", object, 0x1724);
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x178c);
    details_call_ctor("??0CSimsEdit@@QAE@XZ", object, 0x1844);
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x18ac);
    details_call_ctor("??0CSimsListBox@@QAE@XZ", object, 0x1964);
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x19ac);
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x1a64);
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x1b1c);
    details_call_ctor("??0CSimsText@@QAE@XZ", object, 0x1bd4);
    details_call_ctor("??0CSimsTextArt@@QAE@XZ", object, 0x1c8c);
    PTR(object, 0) = fs98_mpdialog_details_vtables;
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_delete_body(void *object)
{
    details_call_ctor("??1CSimsTextArt@@UAE@XZ", object, 0x1c8c);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x1bd4);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x1b1c);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x1a64);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x19ac);
    details_call_ctor("??1CSimsListBox@@UAE@XZ", object, 0x1964);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x18ac);
    details_call_ctor("??1CSimsEdit@@UAE@XZ", object, 0x1844);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x178c);
    details_call_ctor("??1CSimsEdit@@UAE@XZ", object, 0x1724);
    details_call_ctor("??1CSimsEdit@@UAE@XZ", object, 0x16bc);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x1604);
    details_call_ctor("??1CSimsText@@UAE@XZ", object, 0x154c);
    /* CSimsSpinButtonCtrl uses its retail imported vtable and destructor. */
    ((void (TC *)(void *))fs98_mpdialog_mfc(793))(AT(object, 0x150c));
    ((void (TC *)(void *))fs98_mpdialog_mfc(793))(AT(object, 0x14cc));
    details_call_ctor("??1CSims3PieceButton@@UAE@XZ", object, 0x13d0);
    details_call_ctor("??1CSims3PieceButton@@UAE@XZ", object, 0x12d4);
    details_call_ctor("??1CSims3PieceButton@@UAE@XZ", object, 0x11d8);
    details_call_ctor("??1CSims3PieceButton@@UAE@XZ", object, 0x10dc);
    ((void (TC *)(void *))fs98_mpdialog_simsui("??1CSimsDialog@@UAE@XZ"))(object);
}

void *FS98_MPDIALOG_THISCALL fs98_mpdialog_details_delete(void *object, uint32_t flags)
{
    fs98_mpdialog_details_delete_body(object);
    if (flags & 1) fs98_mpdialog_free(object);
    return object;
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_post_destroy(void *object)
{
    if (object)
        ((void *(TC *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_exchange(void *object, void *exchange)
{
    static const uint32_t s_ddx[][2] = {
        {0x11d8, 0x3ea}, {0x10dc, 2}, {0x12d4, 1}, {0x13d0, 0x3ef},
        {0x1c8c, 0x408}, {0x19ac, 0x42c}, {0x1a64, 0x42d},
        {0x1b1c, 0x429}, {0x1bd4, 0x42a}, {0x178c, 0x428},
        {0x1844, 0x405}, {0x154c, 0x409}, {0x1604, 0x40a},
        {0x16bc, 0x40b}, {0x1724, 0x40d}, {0x14cc, 0x40c},
        {0x150c, 0x40e}, {0x18ac, 0x407}, {0x1964, 0x406}
    };
    uint32_t index;
    ((void (TC *)(void *, void *))fs98_mpdialog_simsui(
        "?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z"))(object, exchange);
    for (index = 0; index < sizeof(s_ddx) / sizeof(s_ddx[0]); ++index)
        fs98_mpdialog_ddx_control(exchange, s_ddx[index][1], AT(object, s_ddx[index][0]));
}

void *FS98_MPDIALOG_THISCALL fs98_mpdialog_details_message_map(void *object)
{ (void)object; return &fs98_mpdialog_maps[1]; }

static void details_init_button(void *object, uint32_t offset, uint32_t bitmap)
{
    struct fs98_details_rect rect = {0, 0, 0, 0};
    void *init = fs98_mpdialog_simsui(
        "?Init@CSims3PieceButton@@QAEHPAVCSimsFont@@0IPAVCDIBSection@@PAVCDIBPal@@KHPAUHINSTANCE__@@HHVCRect@@@Z");
    ((int32_t (TC *)(void *, void *, void *, uint32_t, void *, void *, uint32_t,
        uint32_t, uint32_t, int32_t, int32_t, struct fs98_details_rect))init)(
        AT(object, offset), AT(object, 0x5a0), AT(object, 0x610), bitmap,
        AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
        fs98_mpdialog_resource_module(), 1, 1, rect);
}

static void details_remap_spin_control(void *object, uint32_t offset)
{
    struct fs98_details_rect rect = {0, 0, 0, 0};
    void (SC *remap)(struct fs98_details_rect *, void *) =
        (void *)fs98_mpdialog_simsui(
            "?RemapCtrlRect@@YGXAAVCRect@@PAVCWnd@@@Z");
    void (TC *move_window)(void *, int32_t, int32_t, int32_t, int32_t,
                           int32_t) = (void *)fs98_mpdialog_mfc(4299);
    remap(&rect, AT(object, offset));
    move_window(AT(object, offset), rect.left, rect.top,
        rect.right - rect.left, rect.bottom - rect.top, 1);
}

int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_init(void *object)
{
    int32_t initialized;
    void *font;
    uint8_t *block = PTR(object, 0x1d78);
    int32_t (TC *base_init)(void *, int32_t, uint32_t, uint32_t) =
        (void *)PTR(PTR(object, 0), 0xe4);
    uint32_t index;
    static const uint32_t s_text_offsets[] = {0x1b1c, 0x1bd4, 0x19ac, 0x1a64, 0x154c, 0x1604};
    static const uint32_t s_button_offsets[] = {0x10dc, 0x11d8, 0x12d4, 0x13d0};
    initialized = fs98_mpdialog_base_init(object);
    if (!initialized) return 0;
    if (!base_init(object, 0x14f, fs98_mpdialog_resource_module(), 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    if (D32(block, 0x14)) {
        char title[0xff];
        LoadStringA((HINSTANCE)fs98_mpdialog_instance(), 1, title, sizeof(title));
        ((void (TC *)(void *, const char *))fs98_mpdialog_mfc(6199))(
            AT(object, 0x1c8c), title);
    }
    ((int32_t (TC *)(void *, int32_t, void *, void *, uint32_t))
        fs98_mpdialog_simsui("?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z"))(
            AT(object, 0x1c8c), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    font = ((void *(SC *)(int32_t))fs98_mpdialog_simsui("?GetSimsBaseFont@@YGPAVCFont@@H@Z"))(2);
    for (index = 0; index < sizeof(s_text_offsets) / sizeof(s_text_offsets[0]); ++index)
        ((void (TC *)(void *, void *))fs98_mpdialog_simsui("?Init@CSimsText@@QAEXPAVCSimsFont@@@Z"))(
            AT(object, s_text_offsets[index]), AT(object, 0x220));
    ((void (TC *)(void *, void *))fs98_mpdialog_simsui("?Init@CSimsText@@QAEXPAVCSimsFont@@@Z"))(
        AT(object, 0x178c), AT(object, 0x370));
    ((void (TC *)(void *, void *))fs98_mpdialog_simsui("?Init@CSimsText@@QAEXPAVCSimsFont@@@Z"))(
        AT(object, 0x18ac), AT(object, 0x370));
    ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))
        fs98_mpdialog_simsui("?Init@CSimsEdit@@QAEXPAVCFont@@KKKPAVCSimsText@@K@Z"))(
            AT(object, 0x1844), font, 0xffffff, 0, 0xff0000, AT(object, 0x178c), 0);
    ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))
        fs98_mpdialog_simsui("?Init@CSimsEdit@@QAEXPAVCFont@@KKKPAVCSimsText@@K@Z"))(
            AT(object, 0x16bc), font, 0xffffff, 0, 0xff0000, AT(object, 0x154c), 0);
    ((void (TC *)(void *, void *, uint32_t, uint32_t, uint32_t, void *, uint32_t))
        fs98_mpdialog_simsui("?Init@CSimsEdit@@QAEXPAVCFont@@KKKPAVCSimsText@@K@Z"))(
            AT(object, 0x1724), font, 0xffffff, 0, 0xff0000, AT(object, 0x1604), 0);
    details_remap_spin_control(object, 0x14cc);
    details_remap_spin_control(object, 0x150c);
    ((void (TC *)(void *, void *, void *))fs98_mpdialog_simsui(
        "?Init@CSimsListBox@@QAEXPAVCSimsFont@@PAVCSimsText@@@Z"))(
            AT(object, 0x1964), AT(object, 0x450), AT(object, 0x18ac));
    for (index = 0; index < 4; ++index)
        details_init_button(object, s_button_offsets[index], index < 3 ? 0x132 : 0x137);
    if (D32(block, 0x14)) {
        fs98_mpdialog_show_control(AT(object, 0x12d4), SW_SHOW);
        fs98_mpdialog_show_control(AT(object, 0x10dc), SW_SHOW);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_HIDE);
        fs98_mpdialog_enable(AT(object, 0x16bc), 1);
        fs98_mpdialog_enable(AT(object, 0x14cc), 1);
        fs98_mpdialog_enable(AT(object, 0x1724), 1);
        fs98_mpdialog_enable(AT(object, 0x150c), 1);
    } else {
        fs98_mpdialog_show_control(AT(object, 0x12d4), SW_HIDE);
        fs98_mpdialog_show_control(AT(object, 0x10dc), SW_HIDE);
        fs98_mpdialog_show_control(AT(object, 0x11d8), SW_SHOW);
        fs98_mpdialog_enable(AT(object, 0x16bc), 0);
        fs98_mpdialog_enable(AT(object, 0x14cc), 0);
        fs98_mpdialog_enable(AT(object, 0x1724), 0);
        fs98_mpdialog_enable(AT(object, 0x150c), 0);
    }
    fs98_mpdialog_enable(AT(object, 0x12d4), 1);
    fs98_mpdialog_enable(AT(object, 0x10dc), 1);
    fs98_mpdialog_enable(AT(object, 0x11d8), 1);
    fs98_mpdialog_enable(AT(object, 0x13d0), 0);
    fs98_mpdialog_details_update(object);
    D32(object, 0x1d7c) = 1;
    if (D32(block, 0x14))
        fs98_mpdialog_focus_control(AT(object, 0x10dc));
    else
        fs98_mpdialog_focus_control(AT(object, 0x11d8));
    return 0;
}

static int32_t details_atoi(const char *value)
{ return value ? atoi(value) : 0; }

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_update(void *object)
{
    uint8_t *block = PTR(object, 0x1d78);
    HWND list = (HWND)D32(object, 0x1984);
    uint32_t count, index, old_selection;
    uint8_t *rows;
    LPARAM range;
    if (!block || !list) return;
    old_selection = (uint32_t)SendMessageA(list, LB_GETCURSEL, 0, 0);
    SendMessageA(list, LB_RESETCONTENT, 0, 0);
    rows = (uint8_t *)D32(block, 0x4c);
    count = D32(block, 0x50);
    for (index = 0; rows && index < count; ++index)
        SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)*(char **)(rows + index * 16 + 12));
    SendMessageA(list, LB_SETCURSEL, count ? old_selection : (WPARAM)-1, 0);
    S32(block, 0x58) = (int32_t)SendMessageA(list, LB_GETCURSEL, 0, 0);
    if (!count) fs98_mpdialog_enable(AT(object, 0x13d0), 0);
    if (D32(block, 0x18)) SetWindowTextA((HWND)D32(object, 0x1844 + 0x20), (const char *)D32(block, 0x18));
    if (D32(block, 0x44)) SetWindowTextA((HWND)D32(object, 0x19ac + 0x20), (const char *)D32(block, 0x44));
    if (D32(block, 0x48)) SetWindowTextA((HWND)D32(object, 0x1a64 + 0x20), (const char *)D32(block, 0x48));
    if (!D32(object, 0x1d7c) || !D32(block, 0x14)) {
        if (D32(block, 0x20)) SetWindowTextA((HWND)D32(object, 0x16bc + 0x20), (const char *)D32(block, 0x20));
        if (D32(block, 0x34)) SetWindowTextA((HWND)D32(object, 0x1724 + 0x20), (const char *)D32(block, 0x34));
    }
    range = MAKELPARAM((uint16_t)details_atoi((const char *)D32(block, 0x2c)),
                       (uint16_t)details_atoi((const char *)D32(block, 0x28)));
    SendMessageA((HWND)D32(object, 0x14cc + 0x20), UDM_SETRANGE, 0, range);
    range = MAKELPARAM((uint16_t)details_atoi((const char *)D32(block, 0x40)),
                       (uint16_t)details_atoi((const char *)D32(block, 0x3c)));
    SendMessageA((HWND)D32(object, 0x150c + 0x20), UDM_SETRANGE, 0, range);
    if (D32(block, 0x14) && count > 1) fs98_mpdialog_enable(AT(object, 0x13d0), 1);
    fs98_mpdialog_details_on_selection(object);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_capture(void *object)
{
    uint8_t *block = PTR(object, 0x1d78);
    HWND list = (HWND)D32(object, 0x1984);
    if (!block) return;
    S32(block, 0x58) = list ? (int32_t)SendMessageA(list, LB_GETCURSEL, 0, 0) : -1;
    GetWindowTextA((HWND)D32(object, 0x16bc + 0x20), (char *)D32(block, 0x20), (int)D32(block, 0x24));
    GetWindowTextA((HWND)D32(object, 0x1724 + 0x20), (char *)D32(block, 0x34), (int)D32(block, 0x38));
}

int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_details_validate(void *object, uint32_t control_id)
{
    uint8_t *block = PTR(object, 0x1d78);
    (void)control_id;
    if (!IsWindowVisible((HWND)D32(object, 0x20))) return 0;
    fs98_mpdialog_details_capture(object);
    if (!block || !D32(block, 4)) return 0;
    if (((fs98_mpdialog_callback)D32(block, 4))(block)) {
        if (D32(block, 0x1c)) return 0x40b;
        if (D32(block, 0x30)) return 0x40d;
    }
    return 0;
}

int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_details_validation_result(void *object, int32_t result)
{
    if (!result) return 0;
    SetFocus(GetDlgItem((HWND)D32(object, 0x20), result));
    return 1;
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_ok(void *object)
{
    uint8_t *block = PTR(object, 0x1d78);
    void **vtable = PTR(object, 0);
    int32_t result;
    fs98_mpdialog_details_capture(object);
    result = ((int32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (((int32_t (TC *)(void *, int32_t))vtable[0xdc / 4])(object, result)) return;
    D32(block, 0) = 1;
    ((fs98_mpdialog_callback)D32(block, 8))(block);
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_cancel(void *object)
{
    uint8_t *block = PTR(object, 0x1d78);
    D32(block, 0) = 0;
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_remove(void *object)
{
    uint8_t *block = PTR(object, 0x1d78);
    fs98_mpdialog_details_capture(object);
    ((fs98_mpdialog_callback)D32(block, 0x0c))(block);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_refresh(void *object, uint32_t wparam, uint32_t lparam)
{
    uint8_t *block = PTR(object, 0x1d78);
    (void)wparam; (void)lparam;
    if (!IsWindowVisible((HWND)D32(object, 0x20))) return;
    if (((fs98_mpdialog_callback)D32(block, 0x10))(block))
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xf8))(object);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_details_on_selection(void *object)
{
    uint8_t *block = PTR(object, 0x1d78);
    int32_t selected = (int32_t)SendMessageA((HWND)D32(object, 0x1984), LB_GETCURSEL, 0, 0);
    uint32_t count = D32(block, 0x50);
    uint8_t *rows = (uint8_t *)D32(block, 0x4c);
    int32_t disabled = !D32(block, 0x14) || count <= 1;
    if (!disabled && selected >= 0 && (uint32_t)selected < count && rows &&
        D32(rows + selected * 16, 8)) disabled = 1;
    fs98_mpdialog_enable(AT(object, 0x13d0), !disabled);
}

uint32_t FS98_MPDIALOG_STDCALL fs98_mpdialog_details_create(uint32_t parent, void *descriptor)
{
    void *object = fs98_mpdialog_allocate(0x1d80);
    (void)parent;
    if (!object) return 0;
    details_construct(object);
    PTR(object, 0x1d78) = descriptor;
    D32(object, 0x80) = fs98_mpdialog_instance();
    fs98_mpdialog_create_window(object, 0x6b, 0);
    fs98_mpdialog_show_window(object, 1);
    return D32(object, 0x20);
}
