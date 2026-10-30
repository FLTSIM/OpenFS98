/* Reimplementation of registry slice 0x110b. Addresses and field offsets
 * correspond to the retail MP_DIALOG.DLL image at 0x22340000. */
#include <windows.h>
#include <string.h>
#include "mpdialog_internal.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))

struct fs98_mpdialog_rect { int32_t left, top, right, bottom; };

extern void *fs98_mpdialog_simsui(const char *name);
extern void *fs98_mpdialog_mfc(uint32_t ordinal);
extern void *fs98_mpdialog_allocate(uint32_t size);
extern uint32_t fs98_mpdialog_instance(void);
extern uint32_t fs98_mpdialog_resource_module(void);
extern void fs98_mpdialog_free(void *object);
extern int32_t fs98_mpdialog_create_window(void *object, uint32_t template_id, void *parent);
extern int32_t fs98_mpdialog_show_window(void *object, int32_t show);
extern int32_t fs98_mpdialog_base_init(void *object);
extern void fs98_mpdialog_ddx_control(void *exchange, uint32_t id, void *control);
extern int32_t fs98_mpdialog_check(void *control, int32_t value);
extern int32_t fs98_mpdialog_enable(void *control, int32_t value);
extern int32_t fs98_mpdialog_show_control(void *control, int32_t value);
extern void fs98_mpdialog_focus_control(void *control);
extern void *fs98_mpdialog_vtables[62];

static const uint32_t s_control_offsets[20] = {
    0x10dc, 0x11d8, 0x12d4, 0x13d0, 0x14cc, 0x15c8, 0x1680, 0x1734,
    0x17e8, 0x18a0, 0x1958, 0x1a10, 0x1ac8, 0x1b80, 0x1c48, 0x1d10,
    0x1dd8, 0x1ea0, 0x1f68, 0x2030
};
static const uint32_t s_checkbox_init_offsets[6] = {
    0x1b80, 0x1c48, 0x1d10, 0x1dd8, 0x1ea0, 0x1f68
};

static const char *const s_control_ctors[20] = {
    "??0CSims3PieceButton@@QAE@XZ", "??0CSims3PieceButton@@QAE@XZ",
    "??0CSims3PieceButton@@QAE@XZ", "??0CSims3PieceButton@@QAE@XZ",
    "??0CSims3PieceButton@@QAE@XZ", "??0CSimsText@@QAE@XZ",
    "??0CSimsGroupbox@@QAE@XZ", "??0CSimsGroupbox@@QAE@XZ",
    "??0CSims3PieceButton@@QAE@XZ", "??0CSims3PieceButton@@QAE@XZ",
    "??0CSims3PieceButton@@QAE@XZ", "??0CSims3PieceButton@@QAE@XZ",
    "??0CSims3PieceButton@@QAE@XZ", "??0CSimsCheckButton@@QAE@XZ",
    "??0CSimsCheckButton@@QAE@XZ", "??0CSimsCheckButton@@QAE@XZ",
    "??0CSimsCheckButton@@QAE@XZ", "??0CSimsCheckButton@@QAE@XZ",
    "??0CSimsCheckButton@@QAE@XZ", "??0CSimsTextArt@@QAE@XZ"
};

static const char *const s_control_dtors[20] = {
    "??1CSims3PieceButton@@UAE@XZ", "??1CSims3PieceButton@@UAE@XZ",
    "??1CSims3PieceButton@@UAE@XZ", "??1CSims3PieceButton@@UAE@XZ",
    "??1CSims3PieceButton@@UAE@XZ", "??1CSimsText@@UAE@XZ",
    "??1CSimsGroupbox@@UAE@XZ", "??1CSimsGroupbox@@UAE@XZ",
    "??1CSims3PieceButton@@UAE@XZ", "??1CSims3PieceButton@@UAE@XZ",
    "??1CSims3PieceButton@@UAE@XZ", "??1CSims3PieceButton@@UAE@XZ",
    "??1CSims3PieceButton@@UAE@XZ", "??1CSimsCheckButton@@UAE@XZ",
    "??1CSimsCheckButton@@UAE@XZ", "??1CSimsCheckButton@@UAE@XZ",
    "??1CSimsCheckButton@@UAE@XZ", "??1CSimsCheckButton@@UAE@XZ",
    "??1CSimsCheckButton@@UAE@XZ", "??1CSimsTextArt@@UAE@XZ"
};

static void mpdialog_construct(void *object)
{
    uint32_t index;
    ((void *(FS98_MPDIALOG_THISCALL *)(void *, int32_t, void *))
        fs98_mpdialog_simsui("??0CSimsDialog@@QAE@HPAVCWnd@@@Z"))(object, 0x6a, 0);
    for (index = 0; index < 20; ++index)
        ((void (FS98_MPDIALOG_THISCALL *)(void *))
            fs98_mpdialog_simsui(s_control_ctors[index]))(AT(object, s_control_offsets[index]));
    PTR(object, 0) = fs98_mpdialog_vtables;
}

static void mpdialog_destroy_controls(void *object)
{
    uint32_t index = 20;
    while (index != 0) {
        --index;
        ((void (FS98_MPDIALOG_THISCALL *)(void *))
            fs98_mpdialog_simsui(s_control_dtors[index]))(AT(object, s_control_offsets[index]));
    }
    ((void (FS98_MPDIALOG_THISCALL *)(void *))
        fs98_mpdialog_simsui("??1CSimsDialog@@UAE@XZ"))(object);
}

void *FS98_MPDIALOG_THISCALL fs98_mpdialog_delete(void *object, uint32_t flags)
{
    mpdialog_destroy_controls(object);
    if (flags & 1) fs98_mpdialog_free(object);
    return object;
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_post_destroy(void *object)
{
    if (object)
        ((void *(FS98_MPDIALOG_THISCALL *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_exchange(void *object, void *exchange)
{
    static const uint32_t s_ddx[][2] = {
        {0x10dc, 2}, {0x11d8, 1}, {0x12d4, 0x403}, {0x13d0, 0x3eb},
        {0x14cc, 0x3ee}, {0x2030, 0x401}, {0x15c8, 0x402}, {0x1680, 0x431},
        {0x1734, 0x432}, {0x1b80, 0x404}, {0x1c48, 0x415}, {0x1d10, 0x416},
        {0x1dd8, 0x417}, {0x1ea0, 0x418}, {0x1f68, 0x419}
    };
    uint32_t index;
    ((void (FS98_MPDIALOG_THISCALL *)(void *, void *))
        fs98_mpdialog_simsui("?DoDataExchange@CSimsDialog@@MAEXPAVCDataExchange@@@Z"))(object, exchange);
    for (index = 0; index < sizeof(s_ddx) / sizeof(s_ddx[0]); ++index)
        fs98_mpdialog_ddx_control(exchange, s_ddx[index][1], AT(object, s_ddx[index][0]));
}

static void mpdialog_init_button(void *object, uint32_t offset, uint32_t bitmap)
{
    struct fs98_mpdialog_rect rect = {0, 0, 0, 0};
    void *init = fs98_mpdialog_simsui(
        "?Init@CSims3PieceButton@@QAEHPAVCSimsFont@@0IPAVCDIBSection@@PAVCDIBPal@@KHPAUHINSTANCE__@@HHVCRect@@@Z");
    ((int32_t (FS98_MPDIALOG_THISCALL *)(void *, void *, void *, uint32_t, void *, void *,
        uint32_t, uint32_t, uint32_t, int32_t, int32_t, struct fs98_mpdialog_rect))init)(
        AT(object, offset), AT(object, 0x5a0), AT(object, 0x610), bitmap,
            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
            fs98_mpdialog_resource_module(), 1, 1, rect);
}

int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_on_init(void *object)
{
    uint32_t index;
    int32_t initialized;
    int32_t (FS98_MPDIALOG_THISCALL *base_init)(void *, int32_t, uint32_t, uint32_t) =
        (void *)PTR(PTR(object, 0), 0xe4);
    struct fs98_mpdialog_rect rect = {0, 0, 0, 0};
    int32_t (FS98_MPDIALOG_THISCALL *art_init)(void *, int32_t, void *, void *, uint32_t);

    initialized = fs98_mpdialog_base_init(object);
    if (!initialized) return 0;
    if (!base_init(object, 0x14f, fs98_mpdialog_resource_module(), 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    art_init = (void *)fs98_mpdialog_simsui(
        "?Init@CSimsTextArt@@QAEHIPAVCDIBSection@@PAVCDIBPal@@K@Z");
    art_init(AT(object, 0x2030), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    ((void (FS98_MPDIALOG_THISCALL *)(void *, void *))
        fs98_mpdialog_simsui("?Init@CSimsText@@QAEXPAVCSimsFont@@@Z"))(AT(object, 0x15c8), AT(object, 0x220));
    ((void (FS98_MPDIALOG_THISCALL *)(void *, void *))
        fs98_mpdialog_simsui("?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z"))(AT(object, 0x1680), AT(object, 0x220));
    ((void (FS98_MPDIALOG_THISCALL *)(void *, void *))
        fs98_mpdialog_simsui("?Init@CSimsGroupbox@@QAEXPAVCSimsFont@@@Z"))(AT(object, 0x1734), AT(object, 0x220));

    mpdialog_init_button(object, 0x11d8, 0x132);
    mpdialog_init_button(object, 0x12d4, 0x137);
    mpdialog_init_button(object, 0x13d0, 0x137);
    mpdialog_init_button(object, 0x14cc, 0x137);
    mpdialog_init_button(object, 0x10dc, 0x132);

    /* CSimsCheckButton::Init is a virtual at member +0xc4 in the retail
     * control classes. The sixth member is the owner-drawn player-name toggle. */
    for (index = 0; index < sizeof(s_checkbox_init_offsets) / sizeof(s_checkbox_init_offsets[0]); ++index) {
        void *control = AT(object, s_checkbox_init_offsets[index]);
        ((int32_t (FS98_MPDIALOG_THISCALL *)(void *, void *, uint32_t, void *, void *, uint32_t))
            PTR(PTR(control, 0), 0xc4))(control, AT(object, 0x4c0), 0x139,
                AT(object, 0xff4), AT(object, 0x7e8), fs98_mpdialog_resource_module());
    }
    (void)rect;
    {
        static const uint32_t s_button_offsets[] = {0x11d8, 0x10dc, 0x12d4, 0x13d0, 0x14cc};
        for (index = 0; index < sizeof(s_button_offsets) / sizeof(s_button_offsets[0]); ++index)
            fs98_mpdialog_enable(AT(object, s_button_offsets[index]), 1);
    }
    if (PTR(object, 0x211c)) fs98_mpdialog_update(object);
    fs98_mpdialog_focus_control(AT(object, 0x11d8));
    return 0;
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_update(void *object)
{
    struct fs98_mpdialog_owner_block *block = PTR(object, 0x211c);
    if (!block) return;
    fs98_mpdialog_check(AT(object, 0x1f68), block->player_names == 1);
    fs98_mpdialog_check(AT(object, 0x1b80), block->dme_lock != 0);
    fs98_mpdialog_check(AT(object, 0x1c48), block->autopilot_lock != 0 && block->reserved30 != 0);
    fs98_mpdialog_enable(AT(object, 0x1c48), block->reserved30 != 0);
    fs98_mpdialog_check(AT(object, 0x1d10), block->adf_lock != 0);
    fs98_mpdialog_check(AT(object, 0x1dd8), block->send_details != 0);
    fs98_mpdialog_check(AT(object, 0x1ea0), block->receive_details != 0);

    if (block->flight_conditions == 0) {
        fs98_mpdialog_enable(AT(object, 0x12d4), 0);
        fs98_mpdialog_show_control(AT(object, 0x14cc), 1);
        fs98_mpdialog_show_control(AT(object, 0x13d0), 0);
    } else {
        fs98_mpdialog_enable(AT(object, 0x12d4), 1);
        fs98_mpdialog_show_control(AT(object, 0x13d0), 1);
        fs98_mpdialog_show_control(AT(object, 0x14cc), 0);
    }
    if (block->reserved18 == 0) {
        fs98_mpdialog_enable(AT(object, 0x13d0), 0);
        fs98_mpdialog_enable(AT(object, 0x14cc), 0);
        fs98_mpdialog_enable(AT(object, 0x12d4), 0);
    } else if (block->flight_conditions == 0) {
        fs98_mpdialog_show_control(AT(object, 0x13d0), 1);
        fs98_mpdialog_show_control(AT(object, 0x14cc), 0);
    } else {
        fs98_mpdialog_show_control(AT(object, 0x14cc), 1);
        fs98_mpdialog_show_control(AT(object, 0x13d0), 0);
    }
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_save(void *object)
{
    struct fs98_mpdialog_owner_block *block = PTR(object, 0x211c);
    if (!block) return;
    block->player_names = D32(object, 0x2024);
    block->dme_lock = D32(object, 0x1c3c) != 0;
    block->autopilot_lock = D32(object, 0x1d04) != 0;
    block->adf_lock = D32(object, 0x1dcc) != 0;
    block->send_details = D32(object, 0x1e94) != 0;
    block->receive_details = D32(object, 0x1f5c) != 0;
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_on_ok(void *object)
{
    struct fs98_mpdialog_owner_block *block = PTR(object, 0x211c);
    ((void (FS98_MPDIALOG_THISCALL *)(void *))PTR(PTR(object, 0), 0xf4))(object);
    block->result = 1;
    ((fs98_mpdialog_callback)block->accept)(block);
    ((void (FS98_MPDIALOG_THISCALL *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_on_cancel(void *object)
{
    struct fs98_mpdialog_owner_block *block = PTR(object, 0x211c);
    block->result = 0;
    ((void (FS98_MPDIALOG_THISCALL *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

static void mpdialog_call_owner(void *object, uint32_t offset)
{
    struct fs98_mpdialog_owner_block *block = PTR(object, 0x211c);
    fs98_mpdialog_callback callback = *(fs98_mpdialog_callback *)AT(block, offset);
    callback(block);
}

void FS98_MPDIALOG_THISCALL fs98_mpdialog_host_settings(void *object)
{ mpdialog_call_owner(object, 0x0c); }
void FS98_MPDIALOG_THISCALL fs98_mpdialog_get_conditions(void *object)
{ mpdialog_call_owner(object, 0x10); }
void FS98_MPDIALOG_THISCALL fs98_mpdialog_set_conditions(void *object)
{ mpdialog_call_owner(object, 0x14); }

void FS98_MPDIALOG_THISCALL fs98_mpdialog_empty(void *object) { (void)object; }
void FS98_MPDIALOG_THISCALL fs98_mpdialog_empty_arg(void *object, uint32_t argument)
{ (void)object; (void)argument; }
void FS98_MPDIALOG_THISCALL fs98_mpdialog_hide(void *object)
{ EnableWindow((HWND)D32(object, 0x20), FALSE); }
void FS98_MPDIALOG_THISCALL fs98_mpdialog_show(void *object)
{ EnableWindow((HWND)D32(object, 0x20), TRUE); }

extern struct fs98_mpdialog_message_map fs98_mpdialog_maps[7];
void *FS98_MPDIALOG_THISCALL fs98_mpdialog_message_map(void *object)
{ (void)object; return &fs98_mpdialog_maps[0]; }

int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_help(void *object, void *info)
{ (void)object; (void)info; return 0; }
int32_t FS98_MPDIALOG_THISCALL fs98_mpdialog_context(void *object, uint32_t window, uint32_t unused)
{ (void)object; (void)unused; return (int32_t)WinHelpA((HWND)window, "fs98.hlp", HELP_CONTEXTPOPUP, 0); }

uint32_t FS98_MPDIALOG_STDCALL fs98_mpdialog_create(uint32_t parent, void *descriptor)
{
    void *object = fs98_mpdialog_allocate(0x2120);
    (void)parent;
    if (object) mpdialog_construct(object);
    PTR(object, 0x211c) = descriptor;
    D32(object, 0x80) = fs98_mpdialog_instance();
    fs98_mpdialog_create_window(object, 0x6a, 0);
    fs98_mpdialog_show_window(object, 1);
    return D32(object, 0x20);
}
