/* Save video and Flight video, DEMO's two larger dialogs, and the CNotify both make.
 *
 * Save video (0x14040001, template 0x3a1, class at 0x2239d510, 0x1900 bytes): the labels
 * 0x11e8 and 0x12a0, title art 0x1358, the name edit 0x1440 (window +0x1460) and the
 * description edit 0x14a8 (window +0x14c8), the buttons review 0x1510 (0x1165), go on
 * 0x160c (0x113b), save 0x1708 (0x116c) and Cancel 0x1804. +0x10dc holds the CNotify,
 * +0x10e0 the check's answer, +0x10e4 the name the user last agreed to overwrite; the
 * descriptor is at +0x11e4 (options_internal.h).
 *
 * Flight video (0x1404, template 0x3a2, class at 0x2239c800, 0x1a10 bytes): the labels
 * 0x10e8, 0x11a0 and 0x1258, title art 0x1310, the list 0x13f8 (window +0x1418), the name
 * edit 0x1440 (window +0x1460), the title edit 0x14a8 (window +0x14c8), the rate combo
 * 0x1510 (window +0x1530), the buttons record 0x1558 (0x1134), play 0x1654 (0x1133),
 * delete 0x1750 (0x1132) and OK 0x184c, and the loop box 0x1948 (state +0x1a04). +0x10dc
 * holds the CNotify, +0x10e0 the edit callback's answer; the descriptor is at +0x10e4. */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define VCALL(object, slot) ((void (TC *)(void *))PTR(PTR(object, 0), slot))(object)
/* EM_LIMITTEXT, LB_ and CB_ messages: windows.h, through options_unwind.h */
#define RECORD 0xa08
#define NOTIFY_SIZE 0x29c

static void *s_notify_vtable;
static struct fs98_options_video_state *s_video;
void fs98_options_set_notify_vtable(void *vtable) { s_notify_vtable = vtable; }
void fs98_options_set_video_state(struct fs98_options_video_state *state) { s_video = state; }

/* vtable +0xe4, CSimsDialog's skin */
static int32_t skin(void *object)
{
    return ((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
        object, 0x14f, fs98_options_resource_module(), 0xf);
}
/* a three-piece button: the dialog's own fonts (0x5a0, 0x610) and art 0x132, or the
 * second pair (0x680, 0x6f0) and art 0x137 */
static void button(void *object, uint32_t at, uint32_t fonts, uint32_t bitmap)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, fonts), AT(object, fonts + 0x70), bitmap,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                          fs98_options_resource_module(), 1, 1, empty);
}
/* an edit in the base font, labelled */
static void edit(void *object, uint32_t at, uint32_t label)
{
    SERVICES->init_edit(AT(object, at), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(object, label), 0);
}
/* vtable +0xe0: the focus to a control, by id */
static uint32_t focus(void *object, uint32_t control)
{
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, control);
}

/* ---- the CNotify ------------------------------------------------------------------------ */

/* 0x22394290, its vtable +4 */
void *TC fs98_options_notify_delete(void *notify, uint32_t flags)
{
    SERVICES->notify_destroy(notify);
    if (flags & 1) SERVICES->ui_delete(notify);
    return notify;
}
struct notify_unwind {
    struct fs98_options_unwind frame;
    void *notify;
    volatile uint32_t live;
};
static void notify_cleanup(struct fs98_options_unwind *frame)
{
    struct notify_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->notify_free(state->notify); }
}
/* OnInitDialog's: ui_new, CNotify(0x30, 1) and Options' vtable, freed through CNotify's
 * operator delete if the constructor throws; null if ui_new fails */
static FS98_OPTIONS_NOINLINE void *new_notify(void)
{
    struct notify_unwind state;
    void *notify;
    state.notify = 0; state.live = 0;
    fs98_options_unwind_enter(&state.frame, notify_cleanup);
    notify = SERVICES->allocate(NOTIFY_SIZE);
    state.notify = notify; state.live = 1;
    if (notify) {
        SERVICES->notify_construct(notify, 0x30, 1);
        PTR(notify, 0) = s_notify_vtable;
    }
    state.live = 0;
    fs98_options_unwind_leave(&state.frame);
    return notify;
}
/* 0x22396d70, vtable +0xec of Save video and others: the CNotify deleted, then Close */
void TC fs98_options_notify_close(void *object)
{
    void *notify = PTR(object, 0x10dc);
    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);
    SERVICES->close(object);
}

/* ---- Save video ------------------------------------------------------------------------- */

#define SAVE_DESCRIPTOR(object) PTR(object, 0x11e4)

/* 0x2239d7c0 (0x1165) and 0x2239d800 (0x113b): review it, or go on; then close */
void TC fs98_options_save_video_review(void *object)
{
    D32(SAVE_DESCRIPTOR(object), 8) = 0;
    D32(SAVE_DESCRIPTOR(object), 0xc) = 0;
    D32(SAVE_DESCRIPTOR(object), 0x10) = 1;
    D32(SAVE_DESCRIPTOR(object), 0x14) = 0;
    VCALL(object, 0xec);
}
void TC fs98_options_save_video_resume(void *object)
{
    D32(SAVE_DESCRIPTOR(object), 8) = 0;
    D32(SAVE_DESCRIPTOR(object), 0xc) = 1;
    D32(SAVE_DESCRIPTOR(object), 0x10) = 0;
    D32(SAVE_DESCRIPTOR(object), 0x14) = 0;
    VCALL(object, 0xec);
}
/* 0x2239d840 (0x116c, and OnOK through 0x2239d8b0): validate and report through the
 * vtable read once; saved, answered and closed unless the report holds it */
void TC fs98_options_save_video_save(void *object)
{
    void **vtable = PTR(object, 0);
    uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    D32(SAVE_DESCRIPTOR(object), 8) = 1;
    D32(SAVE_DESCRIPTOR(object), 0xc) = 0;
    D32(SAVE_DESCRIPTOR(object), 0x10) = 0;
    D32(SAVE_DESCRIPTOR(object), 0x14) = 0;
    D32(SAVE_DESCRIPTOR(object), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x2239d8c0, OnCancel: asks first (0x1011); yes discards and closes */
void TC fs98_options_save_video_cancel(void *object)
{
    uint32_t *descriptor;
    void *vtable;
    D32(object, 0x90) = 0xc0;
    D32(object, 0x94) = 1;
    if (SERVICES->error_dialog(object, AT(object, 0x1440), 0x1011) == 0) return;
    D32(SAVE_DESCRIPTOR(object), 8) = 0;
    D32(SAVE_DESCRIPTOR(object), 0xc) = 0;
    D32(SAVE_DESCRIPTOR(object), 0x10) = 0;
    descriptor = SAVE_DESCRIPTOR(object);
    vtable = PTR(object, 0);
    descriptor[5] = 1;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x2239ddc0: the name and the description from the descriptor */
void TC fs98_options_save_video_fill(void *object)
{
    SERVICES->set_window_text(AT(object, 0x1440), PTR(SAVE_DESCRIPTOR(object), 0x1c));
    SERVICES->set_window_text(AT(object, 0x14a8), PTR(SAVE_DESCRIPTOR(object), 0x30));
}
/* 0x2239ddf0 */
uint32_t TC fs98_options_save_video_has_descriptor(void *object) { return SAVE_DESCRIPTOR(object) != 0; }
/* 0x2239d930, OnInitDialog */
int32_t TC fs98_options_save_video_init(void *object)
{
    SERVICES->base_init(object);
    if (!skin(object)) return 0;
    SERVICES->init_art(AT(object, 0x1358), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    button(object, 0x1804, 0x5a0, 0x132);
    button(object, 0x1708, 0x5a0, 0x132);
    button(object, 0x160c, 0x5a0, 0x132);
    button(object, 0x1510, 0x680, 0x137);
    SERVICES->init_text(AT(object, 0x12a0), AT(object, 0x370));
    edit(object, 0x14a8, 0x12a0);
    SERVICES->send_message(D32(object, 0x14c8), EM_LIMITTEXT, D32(SAVE_DESCRIPTOR(object), 0x34) - 1, 0);
    SERVICES->init_text(AT(object, 0x11e8), AT(object, 0x370));
    edit(object, 0x1440, 0x11e8);
    SERVICES->send_message(D32(object, 0x1460), EM_LIMITTEXT, D32(SAVE_DESCRIPTOR(object), 0x20) - 1, 0);
    PTR(object, 0x10dc) = new_notify();
    if (!fs98_options_save_video_has_descriptor(object)) return 0;
    fs98_options_save_video_fill(object);
    return 1;
}
/* 0x2239dc10, vtable +0xd8: the edits into the descriptor, then DEMO's check. A failure
 * names the edit at fault, the name's when its status is set. */
uint32_t TC fs98_options_save_video_validate(void *object, uint32_t unused)
{
    void *descriptor;
    uint32_t result;
    (void)unused;
    descriptor = SAVE_DESCRIPTOR(object);
    SERVICES->get_window_text(AT(object, 0x14a8), PTR(descriptor, 0x30), (int32_t)D32(descriptor, 0x34));
    descriptor = SAVE_DESCRIPTOR(object);
    SERVICES->get_window_text(AT(object, 0x1440), PTR(descriptor, 0x1c), (int32_t)D32(descriptor, 0x20));
    descriptor = SAVE_DESCRIPTOR(object);
    result = ((fs98_options_check_fn)PTR(descriptor, 4))(descriptor);
    D32(object, 0x10e0) = result;
    if (result) return D32(SAVE_DESCRIPTOR(object), 0x18) ? 0x11a3 : 0x11a0;
    fs98_options_save_video_fill(object);
    return 0;
}
/* 0x2239dc80, vtable +0xdc: nothing while review or go on has the focus. A name already
 * there passes if it is the one already agreed to; otherwise the question is asked, and
 * an agreement remembers it and moves on to the description (0x5fa). */
uint32_t TC fs98_options_save_video_error(void *object, uint32_t code)
{
    void *descriptor;
    uint32_t status;
    if (code == 0) return 0;
    if (SERVICES->window_from_handle(SERVICES->get_focus()) == AT(object, 0x1510)) return 0;
    if (SERVICES->window_from_handle(SERVICES->get_focus()) == AT(object, 0x160c)) return 0;
    if (code != 0x11a3) {
        SERVICES->error_dialog(object, AT(object, 0x14a8), D32(SAVE_DESCRIPTOR(object), 0x2c));
        return focus(object, code);
    }
    descriptor = SAVE_DESCRIPTOR(object);
    status = D32(descriptor, 0x18);
    if (status != 0x1004) {
        SERVICES->error_dialog(object, AT(object, 0x1440), status);
        return focus(object, code);
    }
    if (SERVICES->compare_text((char *)AT(object, 0x10e4), PTR(descriptor, 0x1c)) == 0) return 0;
    descriptor = SAVE_DESCRIPTOR(object);
    D32(object, 0x90) = 0xc0;
    D32(object, 0x94) = 2;
    if (SERVICES->error_dialog(object, AT(object, 0x1440), D32(descriptor, 0x18)) == 0) {
        SERVICES->copy_text((char *)AT(object, 0x10e4), fs98_options_empty_text);
        return focus(object, 0x11a3);
    }
    SERVICES->get_window_text(AT(object, 0x1440), (char *)AT(object, 0x10e4), 0xff);
    SERVICES->post_message(D32(object, 0x20), 0x5fa, D32(object, 0x14c8), 0);
    return 0;
}

/* ---- Flight video ----------------------------------------------------------------------- */

#define VIDEO_DESCRIPTOR(object) PTR(object, 0x10e4)

/* 0x2239cb70, vtable +0xec: the CNotify deleted and forgotten, then Close */
void TC fs98_options_flight_video_close(void *object)
{
    void *notify = PTR(object, 0x10dc);
    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);
    PTR(object, 0x10dc) = 0;
    SERVICES->close(object);
}
/* 0x2239cba0, OnCancel */
void TC fs98_options_flight_video_cancel(void *object)
{
    D32(VIDEO_DESCRIPTOR(object), 0x10) = 0;
    D32(VIDEO_DESCRIPTOR(object), 0x14) = 0;
    D32(VIDEO_DESCRIPTOR(object), 0) = 0;
    VCALL(object, 0xec);
}
/* 0x2239cbd0, OnOK: validate and report through the vtable read once */
void TC fs98_options_flight_video_accept(void *object)
{
    void **vtable = PTR(object, 0);
    uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    D32(VIDEO_DESCRIPTOR(object), 0x10) = 0;
    D32(VIDEO_DESCRIPTOR(object), 0x14) = 0;
    D32(VIDEO_DESCRIPTOR(object), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x2239d0a0, the list's LBN_SELCHANGE: the pick stored, its name and title shown, the
 * edits, their label and delete usable as the record is writable */
void TC fs98_options_flight_video_select(void *object)
{
    uint8_t *item;
    D32(VIDEO_DESCRIPTOR(object), 0x28) = (uint32_t)SERVICES->send_message(D32(object, 0x1418), LB_GETCURSEL, 0, 0);
    item = (uint8_t *)(uintptr_t)(uint32_t)SERVICES->send_message(D32(object, 0x1418), LB_GETITEMDATA,
                                                                  D32(VIDEO_DESCRIPTOR(object), 0x28), 0);
    SERVICES->set_window_text(AT(object, 0x1440), (char *)item);
    SERVICES->set_window_text(AT(object, 0x14a8), (char *)item + 0x100);
    SERVICES->enable_control(AT(object, 0x1440), (int32_t)D32(item, 0xa04));
    SERVICES->enable_control(AT(object, 0x14a8), (int32_t)D32(item, 0xa04));
    SERVICES->enable_control(AT(object, 0x11a0), (int32_t)D32(item, 0xa04));
    SERVICES->enable_control(AT(object, 0x1750), (int32_t)D32(item, 0xa04));
}
/* 0x2239d400: record usable or not, the list refilled from the descriptor (its count
 * reloaded after every entry), the pick, the rate, the loop box, then the pick shown */
void TC fs98_options_flight_video_populate(void *object)
{
    void *descriptor;
    uint8_t *record;
    int32_t index, count;
    SERVICES->enable_control(AT(object, 0x1558), (int32_t)D32(VIDEO_DESCRIPTOR(object), 0x18));
    SERVICES->send_message(D32(object, 0x1418), LB_RESETCONTENT, 0, 0);
    descriptor = VIDEO_DESCRIPTOR(object);
    count = (int32_t)D32(descriptor, 0x2c);
    record = PTR(descriptor, 0x30);
    if (count > 0) {
        index = 0;
        do {
            uint32_t entry = (uint32_t)SERVICES->send_message(D32(object, 0x1418), LB_ADDSTRING, 0,
                                                              (uint32_t)(uintptr_t)record);
            SERVICES->send_message(D32(object, 0x1418), LB_SETITEMDATA, entry, (uint32_t)(uintptr_t)record);
            ++index;
            record += RECORD;
        } while (index < (int32_t)D32(VIDEO_DESCRIPTOR(object), 0x2c));
    }
    SERVICES->send_message(D32(object, 0x1418), LB_SETCURSEL, D32(VIDEO_DESCRIPTOR(object), 0x28), 0);
    SERVICES->send_message(D32(object, 0x1530), CB_SETCURSEL, D32(VIDEO_DESCRIPTOR(object), 0x20), 0);
    SERVICES->set_checked(AT(object, 0x1948), (int32_t)D32(VIDEO_DESCRIPTOR(object), 0x24));
    fs98_options_flight_video_select(object);
}
/* 0x2239d4d0 */
uint32_t TC fs98_options_flight_video_has_descriptor(void *object) { return VIDEO_DESCRIPTOR(object) != 0; }
/* 0x2239cc20, OnInitDialog */
int32_t TC fs98_options_flight_video_init(void *object)
{
    void *check;
    SERVICES->base_init(object);
    if (!skin(object)) return 0;
    D32(object, 0x84) = 0xb;
    SERVICES->init_art(AT(object, 0x1310), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    button(object, 0x184c, 0x5a0, 0x132);
    button(object, 0x1750, 0x680, 0x137);
    button(object, 0x1654, 0x680, 0x137);
    button(object, 0x1558, 0x680, 0x137);
    SERVICES->init_text(AT(object, 0x1258), AT(object, 0x370));
    SERVICES->init_combo(AT(object, 0x1510), AT(object, 0x3e0), AT(object, 0x1258));
    SERVICES->send_message(D32(object, 0x1530), CB_SETCURSEL, 0, 0);
    SERVICES->init_text(AT(object, 0x11a0), AT(object, 0x370));
    edit(object, 0x14a8, 0x11a0);
    SERVICES->send_message(D32(object, 0x14c8), EM_LIMITTEXT, 0xff, 0);
    SERVICES->init_text(AT(object, 0x10e8), AT(object, 0x370));
    SERVICES->init_list(AT(object, 0x13f8), AT(object, 0x450), AT(object, 0x10e8));
    edit(object, 0x1440, 0x10e8);
    SERVICES->send_message(D32(object, 0x1460), EM_LIMITTEXT, 0xff, 0);
    check = AT(object, 0x1948);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), fs98_options_resource_module());
    PTR(object, 0x10dc) = new_notify();
    if (!fs98_options_flight_video_has_descriptor(object)) return 0;
    fs98_options_flight_video_populate(object);
    return 1;
}
/* 0x2239cf90 (0x1132): delete the pick through DEMO; done, the list again, and the focus
 * to the list if delete is no longer usable */
void TC fs98_options_flight_video_delete_entry(void *object)
{
    void *descriptor;
    D32(VIDEO_DESCRIPTOR(object), 0x28) = (uint32_t)SERVICES->send_message(D32(object, 0x1418), LB_GETCURSEL, 0, 0);
    descriptor = VIDEO_DESCRIPTOR(object);
    if (((fs98_options_delete_fn)PTR(descriptor, 8))(descriptor) != 0) return;
    fs98_options_flight_video_populate(object);
    if (!SERVICES->window_enabled(AT(object, 0x1750))) SERVICES->set_focus(AT(object, 0x13f8));
}
/* 0x2239cff0 (0x1133): play the pick, looped as the box says */
void TC fs98_options_flight_video_play(void *object)
{
    void *vtable;
    uint32_t *descriptor;
    D32(VIDEO_DESCRIPTOR(object), 0x10) = 1;
    D32(VIDEO_DESCRIPTOR(object), 0x14) = 0;
    D32(VIDEO_DESCRIPTOR(object), 0x24) = D32(object, 0x1a04);
    descriptor = VIDEO_DESCRIPTOR(object);
    vtable = PTR(object, 0);
    descriptor[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x2239d030 (0x1134): record, at the rate chosen */
void TC fs98_options_flight_video_record(void *object)
{
    void *vtable;
    uint32_t *descriptor;
    D32(VIDEO_DESCRIPTOR(object), 0x10) = 0;
    D32(VIDEO_DESCRIPTOR(object), 0x14) = 1;
    D32(VIDEO_DESCRIPTOR(object), 0x20) = (uint32_t)SERVICES->send_message(D32(object, 0x1530), CB_GETCURSEL, 0, 0);
    D32(VIDEO_DESCRIPTOR(object), 0x24) = D32(object, 0x1a04);
    descriptor = VIDEO_DESCRIPTOR(object);
    vtable = PTR(object, 0);
    descriptor[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x2239d150, the list's double click: select, then play */
void TC fs98_options_flight_video_double_click(void *object)
{
    fs98_options_flight_video_select(object);
    fs98_options_flight_video_play(object);
}
/* 0x2239d4e0, the rate combo's CBN_SELCHANGE */
void TC fs98_options_flight_video_rate(void *object)
{
    D32(VIDEO_DESCRIPTOR(object), 0x20) = (uint32_t)SERVICES->send_message(D32(object, 0x1530), CB_GETCURSEL, 0, 0);
}
/* 0x2239d170, vtable +0xd8: for a writable pick, a copy of its record with the name typed
 * (or, for the title edit 0x11a1, no name and the title typed) goes to DEMO's edit. The
 * name edit alone (0x11a3) passes without it when the name is the one last seen. Kept or
 * renamed, the name is remembered; a refusal names the edit. */
uint32_t TC fs98_options_flight_video_validate(void *object, uint32_t control)
{
    char name[0x100];
    uint8_t record[RECORD];
    uint8_t *item;
    void *descriptor;
    uint32_t index, result;
    name[0] = fs98_options_empty_text[0];
    for (index = 1; index < sizeof(name); ++index) name[index] = 0;
    SERVICES->get_window_text(AT(object, 0x1440), name, 0xff);
    D32(VIDEO_DESCRIPTOR(object), 0x28) = (uint32_t)SERVICES->send_message(D32(object, 0x1418), LB_GETCURSEL, 0, 0);
    item = (uint8_t *)(uintptr_t)(uint32_t)SERVICES->send_message(D32(object, 0x1418), LB_GETITEMDATA,
                                                                  D32(VIDEO_DESCRIPTOR(object), 0x28), 0);
    if (D32(item, 0xa04) == 0) return 0;
    D32(VIDEO_DESCRIPTOR(object), 0x20) = (uint32_t)SERVICES->send_message(D32(object, 0x1530), CB_GETCURSEL, 0, 0);
    D32(VIDEO_DESCRIPTOR(object), 0x24) = D32(object, 0x1a04);
    descriptor = VIDEO_DESCRIPTOR(object);
    {
        const uint8_t *from = (const uint8_t *)PTR(descriptor, 0x30) + D32(descriptor, 0x28) * RECORD;
        for (index = 0; index < RECORD; ++index) record[index] = from[index];
    }
    if (control == 0x11a1) {
        SERVICES->get_window_text(AT(object, 0x14a8), (char *)record + 0x100, 0xff);
        record[0] = 0;
    } else {
        SERVICES->get_window_text(AT(object, 0x1440), (char *)record, 0xff);
        record[0x100] = 0;
    }
    if (control == 0x11a3 &&
        (SERVICES->host_strcmpi(s_video->last, name) == 0 || s_video->last[0] == 0)) {
        descriptor = VIDEO_DESCRIPTOR(object);
        SERVICES->host_strcpy(s_video->last, (const char *)PTR(descriptor, 0x30) + D32(descriptor, 0x28) * RECORD);
        return 0;
    }
    descriptor = VIDEO_DESCRIPTOR(object);
    result = ((fs98_options_edit_fn)PTR(descriptor, 0xc))(descriptor, record);
    D32(object, 0x10e0) = result;
    if (result == 0) {
        SERVICES->host_strcpy(s_video->last, name);
        fs98_options_flight_video_populate(object);
        return 0;
    }
    return control == 0x11a1 ? 0x11a1 : 0x11a3;
}
/* 0x2239d370, vtable +0xdc: the title's failure on the title edit; a name already there
 * asks, and an agreement refills the list; any other on the name edit */
uint32_t TC fs98_options_flight_video_error(void *object, uint32_t code)
{
    uint32_t status;
    if (code == 0) return 0;
    status = D32(object, 0x10e0);
    if (code == 0x11a1) {
        SERVICES->error_dialog(object, AT(object, 0x14a8), status);
        return focus(object, code);
    }
    if (status != 0x1004) {
        SERVICES->error_dialog(object, AT(object, 0x1440), status);
        return focus(object, code);
    }
    D32(object, 0x90) = 0xc0;
    D32(object, 0x94) = 2;
    if (SERVICES->error_dialog(object, AT(object, 0x1440), status) == 0) return focus(object, code);
    fs98_options_flight_video_populate(object);
    return 0;
}
