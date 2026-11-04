/* Select demo (0x14090002, template 0x39c, class CSelectDemo at 0x2239c140, 0x1650 bytes),
 * MAIN's pf_aircraft, its block described in options_internal.h.
 *
 * Members: OK 0x10e4 and Cancel 0x11e0, the labels 0x12dc and 0x1394, title art 0x144c,
 * the demos list 0x1534 (window +0x1554), and two edits 0x157c and 0x15e4 that show the
 * pick (its record's text, and the list's text). +0x10dc holds the CNotify, +0x10e0 the
 * block, +0x164c the answer state (1 OK, 2 Cancel). */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define BLOCK(object) PTR(object, 0x10e0)
#define LIST(object) D32(object, 0x1554)
#define LB_ADDSTRING_ 0x180
#define LB_RESETCONTENT_ 0x184
#define LB_SETCURSEL_ 0x186
#define LB_GETCURSEL_ 0x188
#define LB_GETITEMDATA_ 0x199
#define LB_SETITEMDATA_ 0x19a
#define RECORD_SIZE 0xa08

static void button(void *object, uint32_t at)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 0, 0, empty);
}
static void edit(void *object, uint32_t at)
{
    SERVICES->init_edit(AT(object, at), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, 0, 0);
}

/* 0x2239c660, OnCancel: the vtable read after the stores */
void TC fs98_options_demo_cancel(void *object)
{
    uint32_t *block = BLOCK(object);
    D32(object, 0x164c) = 2;
    if (block) block[0] = 0;
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
/* 0x2239c700: the pick into the block */
void TC fs98_options_demo_read(void *object)
{
    uint32_t pick = (uint32_t)SERVICES->send_message(LIST(object), LB_GETCURSEL_, 0, 0);
    D32(BLOCK(object), 0x10) = pick;
}
/* 0x2239c690, OnOK */
void TC fs98_options_demo_accept(void *object)
{
    uint32_t *block;
    D32(object, 0x164c) = 1;
    fs98_options_demo_read(object);
    block = BLOCK(object);
    if (block) block[0] = 1;
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}
struct text_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *text;
    volatile uint32_t live;
};
static void text_cleanup(struct fs98_options_unwind *frame)
{
    struct text_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(state->text); }
}
/* 0x2239c5a0 (LBN_SELCHANGE on 0x11ac): the pick's list text into 0x15e4, the text of the
 * record it was tagged with into 0x157c. RETAIL BUG (kept): OPT-9 (docs/retail-bugs.md):
 * with nothing picked the list answers LB_ERR, and that is shown as a text pointer. */
FS98_OPTIONS_NOINLINE void TC fs98_options_demo_select(void *object)
{
    struct text_unwind state;
    struct fs98_options_string text;
    int32_t pick;
    uint32_t record;
    state.text = &text; state.live = 0;
    fs98_options_unwind_enter(&state.frame, text_cleanup);
    SERVICES->string_construct(&text);
    state.live = 1;
    pick = SERVICES->send_message(LIST(object), LB_GETCURSEL_, 0, 0);
    SERVICES->list_text(AT(object, 0x1534), pick, &text);
    SERVICES->set_window_text(AT(object, 0x15e4), text.text);
    pick = SERVICES->send_message(LIST(object), LB_GETCURSEL_, 0, 0);
    record = (uint32_t)SERVICES->send_message(LIST(object), LB_GETITEMDATA_, (uint32_t)pick, 0);
    SERVICES->set_window_text(AT(object, 0x157c), (const char *)(uintptr_t)record);
    text_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
}
/* 0x2239c730: the demos into the list, each tagged with its record; the count read again
 * after each, then MAIN's pick and what it shows */
void TC fs98_options_demo_populate(void *object)
{
    uint8_t *block, *record;
    int32_t index = 0;
    SERVICES->send_message(LIST(object), LB_RESETCONTENT_, 0, 0);
    block = BLOCK(object);
    record = PTR(block, 0x18);
    if ((int32_t)D32(block, 0x14) > 0) {
        do {
            int32_t at = SERVICES->send_message(LIST(object), LB_ADDSTRING_, 0, (uint32_t)(uintptr_t)record);
            SERVICES->send_message(LIST(object), LB_SETITEMDATA_, (uint32_t)at, (uint32_t)(uintptr_t)record);
            ++index;
            record += RECORD_SIZE;
        } while (index < (int32_t)D32(BLOCK(object), 0x14));
    }
    SERVICES->send_message(LIST(object), LB_SETCURSEL_, D32(BLOCK(object), 0x10), 0);
    fs98_options_demo_select(object);
}
/* 0x2239c7c0: a block that is neither null nor MSVC's debug fill; otherwise said through the
 * CNotify */
uint32_t TC fs98_options_demo_has_descriptor(void *object)
{
    void *block = BLOCK(object);
    if (block != 0 && block != (void *)(uintptr_t)0xcdcdcdcd) return 1;
    SERVICES->notify_send(PTR(object, 0x10dc), "In CSelectDemo::InitializeData",
                          "Bad m_pliveInfo = %lx\npassed to dialog by core code", block);
    return 0;
}
/* 0x2239c3b0, OnInitDialog */
int32_t TC fs98_options_demo_init(void *object)
{
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14f, fs98_options_resource_module(), 0xf)) return 0;
    SERVICES->init_art(AT(object, 0x144c), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    button(object, 0x10e4);
    button(object, 0x11e0);
    SERVICES->init_text(AT(object, 0x12dc), AT(object, 0x370));
    edit(object, 0x157c);
    SERVICES->init_text(AT(object, 0x1394), AT(object, 0x370));
    edit(object, 0x15e4);
    SERVICES->init_list(AT(object, 0x1534), AT(object, 0x450), AT(object, 0x1394));
    PTR(object, 0x10dc) = fs98_options_new_notify();
    if (!fs98_options_demo_has_descriptor(object)) return 0;
    fs98_options_demo_populate(object);
    return 1;
}
