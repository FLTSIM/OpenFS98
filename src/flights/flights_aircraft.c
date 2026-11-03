/* Select Aircraft, dialog 0x1201 (class at 0x223118a0, 0x1868 bytes).
 *
 * Members: OK 0x10dc, Cancel 0x11d8, labels 0x12d4 0x138c 0x1444, the list 0x14fc (window
 * +0x151c), the edits 0x1544 (+0x1564), 0x15ac (+0x15cc) and 0x1614 (+0x1634), Details
 * 0x167c, title art 0x1778. The preview picture is dialog item 0xc87. The answer state is
 * +0x1860, the descriptor +0x1864 (flights_internal.h), which every handler but the list's
 * frame and double click tests for null. */
#include "flights_internal.h"
#include "flights_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define SERVICES fs98_flights_services()
#define DESCRIPTOR(object) PTR(object, 0x1864)
/* LB_* and EM_LIMITTEXT come from windows.h, through flights_unwind.h */

/* The chosen record, through a reloaded descriptor each time, as retail indexes it */
static uint8_t *fs98_flights_aircraft_record(void *object)
{
    void *descriptor = DESCRIPTOR(object);
    return AT(PTR(descriptor, 0x1c), D32(descriptor, 0x14) * (uint32_t)0xa08);
}
/* RETAIL BUG (kept): FLT-2 (docs/retail-bugs.md), its callers store -1 and read the record there */
static int32_t fs98_flights_aircraft_selection(void *object)
{
    return SERVICES->send_message(D32(object, 0x151c), LB_GETCURSEL, 0, 0);
}
static void fs98_flights_aircraft_preview(void *object, uint32_t operation)
{
    void *descriptor = DESCRIPTOR(object);
    ((fs98_flights_aircraft_preview_fn)PTR(descriptor, 0x10))(operation, DESCRIPTOR(object), 0);
}

/* A CString local, destroyed if an exception passes through its frame */
struct fs98_flights_string_frame {
    struct fs98_flights_unwind frame;
    struct fs98_flights_string string;
    volatile uint32_t live;
};
static void fs98_flights_string_cleanup(struct fs98_flights_unwind *frame)
{
    struct fs98_flights_string_frame *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(&state->string); }
}
static void fs98_flights_string_enter(struct fs98_flights_string_frame *state)
{
    state->live = 0;
    fs98_flights_unwind_enter(&state->frame, fs98_flights_string_cleanup);
    SERVICES->string_construct(&state->string);
    state->live = 1;
}
static void fs98_flights_string_leave(struct fs98_flights_string_frame *state)
{
    state->live = 0;
    SERVICES->string_destroy(&state->string);
    fs98_flights_unwind_leave(&state->frame);
}

/* The chosen aircraft's texts, its title fetched back from the list, and Details */
static void fs98_flights_aircraft_show(void *object, struct fs98_flights_string *title)
{
    SERVICES->set_window_text(AT(object, 0x15ac), (char *)fs98_flights_aircraft_record(object) + 0x100);
    SERVICES->set_window_text(AT(object, 0x1614), (char *)fs98_flights_aircraft_record(object) + 0x500);
    SERVICES->list_text(AT(object, 0x14fc), (int32_t)D32(DESCRIPTOR(object), 0x14), title);
    SERVICES->set_window_text(AT(object, 0x1544), title->text);
    SERVICES->enable_control(AT(object, 0x167c), (int32_t)D32(fs98_flights_aircraft_record(object), 0xa04));
}

/* 0x22311ba0, OnCancel: the preview closed, then the vtable read */
void TC fs98_flights_aircraft_cancel(void *object)
{
    if (DESCRIPTOR(object)) {
        D32(DESCRIPTOR(object), 0) = 0;
        fs98_flights_aircraft_preview(object, 2);
    }
    D32(object, 0x1860) = 2;
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

/* 0x22311be0, OnOK */
void TC fs98_flights_aircraft_accept(void *object)
{
    if (DESCRIPTOR(object)) {
        int32_t selection;
        D32(DESCRIPTOR(object), 0) = 1;
        selection = fs98_flights_aircraft_selection(object);
        D32(DESCRIPTOR(object), 0x14) = (uint32_t)selection;
        fs98_flights_aircraft_preview(object, 2);
    }
    D32(object, 0x1860) = 1;
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

/* 0x22311c40, WM_USER+100: a preview frame of the aircraft chosen */
int32_t TC fs98_flights_aircraft_frame(void *object, uint32_t w_param, uint32_t l_param)
{
    int32_t selection = fs98_flights_aircraft_selection(object);
    void *descriptor;
    (void)w_param; (void)l_param;
    D32(DESCRIPTOR(object), 0x14) = (uint32_t)selection;
    descriptor = DESCRIPTOR(object);
    ((fs98_flights_aircraft_preview_fn)PTR(descriptor, 0x10))(3, descriptor, 0);
    return 0;
}

/* 0x22311f40, vtable +0xf0: the list, the chosen one selected and shown. The count and
 * the loop are signed. */
void TC fs98_flights_aircraft_populate(void *object)
{
    struct fs98_flights_string_frame title;
    int32_t index;
    uint32_t offset;
    SERVICES->send_message(D32(object, 0x151c), LB_RESETCONTENT, 0, 0);
    for (index = 0, offset = 0; index < (int32_t)D32(DESCRIPTOR(object), 0x18); ++index, offset += 0xa08)
        SERVICES->send_message(D32(object, 0x151c), LB_ADDSTRING, 0,
                               (uint32_t)(uintptr_t)AT(PTR(DESCRIPTOR(object), 0x1c), offset));
    fs98_flights_string_enter(&title);
    SERVICES->send_message(D32(object, 0x151c), LB_SETCURSEL, D32(DESCRIPTOR(object), 0x14), 0);
    fs98_flights_aircraft_show(object, &title.string);
    fs98_flights_string_leave(&title);
}

/* 0x223120a0: LBN_SELCHANGE */
void TC fs98_flights_aircraft_select(void *object)
{
    struct fs98_flights_string_frame title;
    int32_t selection;
    if (DESCRIPTOR(object) == 0) return;
    fs98_flights_string_enter(&title);
    selection = fs98_flights_aircraft_selection(object);
    D32(DESCRIPTOR(object), 0x14) = (uint32_t)selection;
    fs98_flights_aircraft_show(object, &title.string);
    fs98_flights_string_leave(&title);
}

/* 0x223121c0: LBN_DBLCLK */
void TC fs98_flights_aircraft_double_click(void *object)
{
    fs98_flights_aircraft_select(object);
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xcc))(object);
}

/* 0x22312220: Details. Its CString is made and never used. When the callback answers
 * zero it has changed the list, which is filled again. */
void TC fs98_flights_aircraft_details(void *object)
{
    struct fs98_flights_string_frame unused;
    int32_t selection;
    void *descriptor;
    if (DESCRIPTOR(object) == 0) return;
    fs98_flights_string_enter(&unused);
    selection = fs98_flights_aircraft_selection(object);
    D32(DESCRIPTOR(object), 0x14) = (uint32_t)selection;
    descriptor = DESCRIPTOR(object);
    if (((fs98_flights_aircraft_details_fn)PTR(descriptor, 8))(descriptor) == 0) {
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xf0))(object);
        selection = fs98_flights_aircraft_selection(object);
        D32(DESCRIPTOR(object), 0x14) = (uint32_t)selection;
        SERVICES->enable_control(AT(object, 0x167c), (int32_t)D32(fs98_flights_aircraft_record(object), 0xa04));
        SERVICES->set_focus(AT(object, 0x1544));
    }
    fs98_flights_string_leave(&unused);
}

static void fs98_flights_aircraft_edit(void *object, uint32_t label, uint32_t edit)
{
    SERVICES->init_text(AT(object, label), AT(object, 0x370));
    SERVICES->init_edit(AT(object, edit), SERVICES->base_font(2), 0xffffff, 0, 0xffffff, AT(object, label), 0);
}

/* 0x22311c80, OnInitDialog. The vtable is read once, before the skin. With a descriptor
 * it fills the list, fits the preview to its frame and opens it; it answers FALSE, the
 * focus on the list. */
int32_t TC fs98_flights_aircraft_init(void *object)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0}, rect;
    uint32_t module = fs98_flights_resource_module();
    void **vtable;
    SERVICES->base_init(object);
    vtable = PTR(object, 0);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))vtable[0xe4 / 4])(object, 0x14f, module, 0xf))
        return 0;
    D32(object, 0x84) = 0xb;
    SERVICES->init_art(AT(object, 0x1778), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x10dc), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    SERVICES->init_button(AT(object, 0x11d8), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    SERVICES->init_button(AT(object, 0x167c), AT(object, 0x680), AT(object, 0x6f0), 0x137,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    fs98_flights_aircraft_edit(object, 0x12d4, 0x1544);
    fs98_flights_aircraft_edit(object, 0x138c, 0x15ac);
    fs98_flights_aircraft_edit(object, 0x1444, 0x1614);
    SERVICES->init_list(AT(object, 0x14fc), AT(object, 0x450), AT(object, 0x12d4));
    SERVICES->enable_control(AT(object, 0x167c), 0);
    SERVICES->send_message(D32(object, 0x1564), EM_LIMITTEXT, 0x100, 0);
    SERVICES->send_message(D32(object, 0x15cc), EM_LIMITTEXT, 0x3ff, 0);
    SERVICES->send_message(D32(object, 0x1634), EM_LIMITTEXT, 0x3ff, 0);
    if (DESCRIPTOR(object)) {
        void *picture, *descriptor, *early;
        uint32_t window;
        ((void (TC *)(void *))vtable[0xf0 / 4])(object);
        picture = SERVICES->dialog_item(object, 0xc87);
        SERVICES->remap_rect(&rect, picture);
        SERVICES->move_window(picture, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 1);
        early = DESCRIPTOR(object);     /* the callback's descriptor, read before the item */
        window = SERVICES->dialog_item_window(D32(object, 0x20), 0xc87);
        descriptor = DESCRIPTOR(object);
        ((fs98_flights_aircraft_preview_fn)PTR(early, 0x10))(1, descriptor, window);
    }
    SERVICES->set_focus(AT(object, 0x14fc));
    return 0;
}
