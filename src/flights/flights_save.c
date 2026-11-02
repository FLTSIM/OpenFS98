/* Save Flight, dialog 0x1104 (class at 0x22315f60, 0x17d0 bytes).
 *
 * Members: Cancel 0x11e4, title art 0x12e0, the labels 0x13c8 and 0x1480, the name edit
 * 0x1538 (window +0x1558) and the description edit 0x15a0 (window +0x15c0), OK 0x1608,
 * "make it the default" 0x1704, whose state is +0x17c0. +0x10dc holds the name the user
 * last agreed to overwrite; +0x11dc/+0x11e0 point at the two buttons. The descriptor is
 * at +0x17cc (flights_internal.h). */
#include "flights_internal.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define EM_LIMITTEXT 0xc5
#define SERVICES fs98_flights_services()

/* 0x22316490: the edits' limits from the descriptor's room, the description, the box */
void TC fs98_flights_save_populate(void *object)
{
    SERVICES->send_message(D32(object, 0x1558), EM_LIMITTEXT, D32(PTR(object, 0x17cc), 0x10) - 1, 0);
    SERVICES->send_message(D32(object, 0x15c0), EM_LIMITTEXT, D32(PTR(object, 0x17cc), 0x24) - 1, 0);
    SERVICES->set_window_text(AT(object, 0x15a0), PTR(PTR(object, 0x17cc), 0x20));
    SERVICES->set_checked(AT(object, 0x1704), (int32_t)D32(PTR(object, 0x17cc), 0x30));
}

/* 0x22316440: the edits and the box back into the descriptor */
void TC fs98_flights_save_read(void *object)
{
    void *descriptor = PTR(object, 0x17cc);
    SERVICES->get_window_text(AT(object, 0x1538), PTR(descriptor, 0xc), (int32_t)D32(descriptor, 0x10));
    descriptor = PTR(object, 0x17cc);
    SERVICES->get_window_text(AT(object, 0x15a0), PTR(descriptor, 0x20), (int32_t)D32(descriptor, 0x24));
    D32(PTR(object, 0x17cc), 0x30) = D32(object, 0x17c0);
}

/* 0x22316500, vtable +0xd8: read, then MAIN's check. A failure names the edit at fault,
 * the name's first; a failure with neither status set is the check's own answer. */
uint32_t TC fs98_flights_save_validate(void *object, uint32_t unused)
{
    uint32_t result;
    void *descriptor;
    (void)unused;
    fs98_flights_save_read(object);
    descriptor = PTR(object, 0x17cc);
    result = ((fs98_flights_save_check_fn)PTR(descriptor, 4))(descriptor);
    if (result == 0) return 0;
    descriptor = PTR(object, 0x17cc);
    if (D32(descriptor, 8) != 0) return 0xcc8;
    if (D32(descriptor, 0x1c) != 0) return 0xcbd;
    return result;
}

/* 0x22316540, vtable +0xdc: report a failure, answering zero when it is settled.
 * A name that is already there passes if it is the one already agreed to; otherwise the
 * question is asked, and an agreement remembers the name and moves to the description. */
uint32_t TC fs98_flights_save_error(void *object, uint32_t code)
{
    void *descriptor = PTR(object, 0x17cc);
    uint32_t status = D32(descriptor, 8);
    int32_t answer;
    (void)code;
    if (status == 0) {
        status = D32(descriptor, 0x1c);
        if (status == 0) return 0;
        return SERVICES->error_dialog(object, AT(object, 0x15a0), status) ? 0xcbd : 0;
    }
    if (status != 0x1004)
        return SERVICES->error_dialog(object, AT(object, 0x1538), status) ? 0xcc8 : 0;
    if (SERVICES->compare_text((char *)AT(object, 0x10dc), PTR(descriptor, 0xc)) == 0) return 0;
    D32(object, 0x90) = 0xc0;
    D32(object, 0x94) = 2;
    answer = SERVICES->error_dialog(object, AT(object, 0x1538), D32(PTR(object, 0x17cc), 8));
    D32(object, 0x8c) = answer == 0;
    if (answer == 0) {
        SERVICES->copy_text((char *)AT(object, 0x10dc), fs98_flights_empty_text);
        return 0xcc8;
    }
    SERVICES->get_window_text(AT(object, 0x1538), (char *)AT(object, 0x10dc), 0xff);
    SERVICES->post_message(D32(object, 0x20), 0x5fa, D32(object, 0x15c0), 0);
    return 0;
}

/* 0x22316390, OnOK: the vtable is read once and every call goes through that copy */
void TC fs98_flights_save_accept(void *object)
{
    void **vtable = PTR(object, 0);
    uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    D32(PTR(object, 0x17cc), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x223163e0, OnCancel */
void TC fs98_flights_save_cancel(void *object)
{
    uint32_t *descriptor = PTR(object, 0x17cc);
    void **vtable = PTR(object, 0);
    descriptor[0] = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x223163d0, vtable +0xec: CSimsDialog's Close, unchanged */
void TC fs98_flights_save_close(void *object) { SERVICES->close(object); }

/* 0x223161f0, OnInitDialog. No descriptor check, unlike the pickers. */
int32_t TC fs98_flights_save_init(void *object)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    uint32_t module = fs98_flights_resource_module();
    void *check;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14f, module, 0xf)) return 0;
    SERVICES->init_art(AT(object, 0x12e0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x1608), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 0, 0, empty);
    SERVICES->init_button(AT(object, 0x11e4), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                          fs98_flights_resource_module(), 0, 0, empty);
    check = AT(object, 0x1704);
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), fs98_flights_resource_module());
    SERVICES->init_text(AT(object, 0x1480), AT(object, 0x370));
    SERVICES->init_edit(AT(object, 0x1538), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                        AT(object, 0x1480), 0);
    SERVICES->init_text(AT(object, 0x13c8), AT(object, 0x370));
    SERVICES->init_edit(AT(object, 0x15a0), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                        AT(object, 0x13c8), 0);
    SERVICES->send_message(D32(object, 0x1558), EM_LIMITTEXT, 0x104, 0);
    PTR(object, 0x11dc) = AT(object, 0x11e4);
    PTR(object, 0x11e0) = AT(object, 0x1608);
    fs98_flights_save_populate(object);
    return 1;
}
