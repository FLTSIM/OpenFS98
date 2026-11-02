/* Select Flight, dialog 0x1102 (class at 0x22316650, 0x1810 bytes).
 *
 * Members: OK 0x10e0, Cancel 0x11dc, labels 0x12d8 and 0x1390, title art 0x1448, the list
 * 0x1530 (window +0x1550), the description edit 0x1578 (window +0x1598) and the title edit
 * 0x15e0, Delete 0x1648, "make it the default" 0x1744 (state +0x1800). +0x10dc holds the
 * edit callback's last answer; the descriptor is +0x180c (flights_internal.h). Each list
 * item carries its record's address as its item data. */
#include "flights_internal.h"
#include "flights_unwind.h"
#include <string.h>

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define SERVICES fs98_flights_services()
#define DESCRIPTOR(object) PTR(object, 0x180c)
#define LIST(object) D32(object, 0x1550)
#define RECORD(descriptor) AT(PTR(descriptor, 0x1c), D32(descriptor, 0x14) * (uint32_t)0xa08)
/* LB_* and EM_* come from windows.h */

/* 0x22316b60, OnCancel */
void TC fs98_flights_select_cancel(void *object)
{
    uint32_t *descriptor = DESCRIPTOR(object);
    void **vtable = PTR(object, 0);
    descriptor[0] = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x22316b80, OnOK: the vtable read once; "make it the default" into the descriptor */
void TC fs98_flights_select_accept(void *object)
{
    void **vtable = PTR(object, 0);
    uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    D32(DESCRIPTOR(object), 0x10) = D32(object, 0x1800);
    D32(DESCRIPTOR(object), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x22316c40: the chosen flight's title and description. The protected entry, and any
 * flagged at record +0xa04, cannot be renamed or deleted. */
void TC fs98_flights_select_choose(void *object)
{
    int32_t selection = SERVICES->send_message(LIST(object), LB_GETCURSEL, 0, 0);
    uint8_t *record;
    D32(DESCRIPTOR(object), 0x14) = (uint32_t)selection;
    record = (uint8_t *)(uintptr_t)(uint32_t)SERVICES->send_message(LIST(object), LB_GETITEMDATA,
                                                                    D32(DESCRIPTOR(object), 0x14), 0);
    SERVICES->set_window_text(AT(object, 0x15e0), (char *)record);
    SERVICES->set_window_text(AT(object, 0x1578), (char *)record + 0x100);
    SERVICES->send_message(D32(object, 0x1598), EM_SETREADONLY, D32(record, 0xa04) == 0, 0);
    SERVICES->enable_control(AT(object, 0x15e0), (int32_t)D32(record, 0xa04));
    SERVICES->enable_control(AT(object, 0x1648), (int32_t)D32(record, 0xa04));
    if (D32(DESCRIPTOR(object), 0x14) == D32(DESCRIPTOR(object), 0x20)) {
        SERVICES->send_message(D32(object, 0x1598), EM_SETREADONLY, 1, 0);
        SERVICES->enable_control(AT(object, 0x15e0), 0);
        SERVICES->enable_control(AT(object, 0x1648), 0);
    }
}

/* 0x22316d30: LBN_DBLCLK */
void TC fs98_flights_select_double_click(void *object)
{
    fs98_flights_select_choose(object);
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xcc))(object);
}

/* 0x22316d50: the list, each item tagged with its record; the records' base is read
 * once. A selection out of range falls back to the first. */
void TC fs98_flights_select_populate(void *object)
{
    uint8_t *record;
    uint32_t index;
    SERVICES->send_message(LIST(object), LB_RESETCONTENT, 0, 0);
    record = PTR(DESCRIPTOR(object), 0x1c);
    for (index = 0; index < D32(DESCRIPTOR(object), 0x18); ++index, record += 0xa08) {
        int32_t item = SERVICES->send_message(LIST(object), LB_ADDSTRING, 0, (uint32_t)(uintptr_t)record);
        SERVICES->send_message(LIST(object), LB_SETITEMDATA, (uint32_t)item, (uint32_t)(uintptr_t)record);
    }
    SERVICES->send_message(LIST(object), LB_SETCURSEL, D32(DESCRIPTOR(object), 0x14), 0);
    if (SERVICES->send_message(LIST(object), LB_GETCURSEL, 0, 0) == -1) {
        SERVICES->send_message(LIST(object), LB_SETCURSEL, 0, 0);
        D32(DESCRIPTOR(object), 0x14) = 0;
    }
    SERVICES->set_checked(AT(object, 0x1744), (int32_t)D32(DESCRIPTOR(object), 0x10));
    fs98_flights_select_choose(object);
}

/* 0x22316bd0: Delete. An entry before the protected one moves it down. If Delete is no
 * longer usable the focus goes to OK. */
void TC fs98_flights_select_remove(void *object)
{
    int32_t selection = SERVICES->send_message(LIST(object), LB_GETCURSEL, 0, 0);
    void *descriptor;
    D32(DESCRIPTOR(object), 0x14) = (uint32_t)selection;
    descriptor = DESCRIPTOR(object);
    ((fs98_flights_select_delete_fn)PTR(descriptor, 8))(descriptor);
    descriptor = DESCRIPTOR(object);
    if (D32(descriptor, 0x14) < D32(descriptor, 0x20)) D32(descriptor, 0x20) -= 1;
    fs98_flights_select_populate(object);
    if (!SERVICES->window_enabled(AT(object, 0x1648))) SERVICES->set_focus(AT(object, 0x10e0));
}

/* 0x22316e30, vtable +0xd8. Only while Delete is usable, and only for OK (0) or the title
 * edit (0xcbe). A changed title goes to the edit callback with a copy of the record;
 * otherwise the description does, with the copy's title cleared. */
uint32_t TC fs98_flights_select_validate(void *object, uint32_t control)
{
    uint8_t record[0xa08];
    if (!SERVICES->window_enabled(AT(object, 0x1648))) return 0;
    if (control != 0 && control != 0xcbe) return 0;
    memcpy(record, RECORD(DESCRIPTOR(object)), sizeof(record));
    SERVICES->get_window_text(AT(object, 0x15e0), (char *)record, 0xff);
    record[0x100] = 0;
    D32(object, 0x10dc) = 0;
    if (SERVICES->compare_text((char *)record, (char *)RECORD(DESCRIPTOR(object))) != 0) {
        void *descriptor = DESCRIPTOR(object);
        D32(object, 0x10dc) = ((fs98_flights_select_edit_fn)PTR(descriptor, 0xc))(descriptor, record);
    }
    if (D32(object, 0x10dc) != 0) {
        if (control == 0 || D32(object, 0x10dc) != 0x1004) return 0xcbe;
        return 0;
    }
    SERVICES->get_window_text(AT(object, 0x1578), (char *)record + 0x100, 0x3ff);
    {
        void *descriptor = DESCRIPTOR(object);
        record[0] = 0;
        ((fs98_flights_select_edit_fn)PTR(descriptor, 0xc))(descriptor, record);
    }
    return 0;
}

/* 0x22316f50, vtable +0xdc. A title that exists already is asked about; agreeing keeps
 * the old title and saves the new description. Anything else is reported, and the answer
 * is vtable +0xe0's. */
uint32_t TC fs98_flights_select_error(void *object, uint32_t code)
{
    uint32_t status;
    if (code == 0) return 0;
    status = D32(object, 0x10dc);
    if (status == 0x1004) {
        D32(object, 0x90) = 0xc0;
        D32(object, 0x94) = 2;
        if (SERVICES->error_dialog(object, AT(object, 0x15e0), status) != 0) {
            uint8_t record[0xa08];
            void *descriptor;
            memcpy(record, RECORD(DESCRIPTOR(object)), sizeof(record));
            SERVICES->get_window_text(AT(object, 0x1578), (char *)record + 0x100, 0x3ff);
            descriptor = DESCRIPTOR(object);
            record[0] = 0;
            ((fs98_flights_select_edit_fn)PTR(descriptor, 0xc))(descriptor, record);
            return 0;
        }
    } else {
        SERVICES->error_dialog(object, AT(object, 0x15e0), status);
    }
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, code);
}

/* 0x22316960, OnInitDialog. It answers FALSE, with the focus on the list when there is a
 * descriptor. */
int32_t TC fs98_flights_select_init(void *object)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    uint32_t module = fs98_flights_resource_module();
    void *check;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(object, 0x14f, module, 0xf))
        return 0;
    SERVICES->init_art(AT(object, 0x1448), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x10e0), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 0, 0, empty);
    SERVICES->init_button(AT(object, 0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 0, 0, empty);
    SERVICES->init_button(AT(object, 0x1648), AT(object, 0x680), AT(object, 0x6f0), 0x137,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    SERVICES->init_text(AT(object, 0x1390), AT(object, 0x370));
    SERVICES->init_edit(AT(object, 0x15e0), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(object, 0x1390), 0x10);
    SERVICES->init_list(AT(object, 0x1530), AT(object, 0x450), 0);
    SERVICES->init_text(AT(object, 0x12d8), AT(object, 0x370));
    SERVICES->init_edit(AT(object, 0x1578), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(object, 0x12d8), 0x10);
    check = AT(object, 0x1744);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), module);
    D32(object, 0x1644) = 0xff;
    D32(object, 0x15dc) = 0x3ff;
    if (DESCRIPTOR(object)) {
        fs98_flights_select_populate(object);
        SERVICES->set_focus(AT(object, 0x1530));
    }
    return 0;
}
