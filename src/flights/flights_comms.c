/* Communications, dialog 0x1203 (class at 0x223155a0, 0x1fe0 bytes).
 *
 * Members: title art 0x10dc, OK 0x11c4, Cancel 0x12c0, labels 0x13bc 0x1474 0x152c 0x15e4,
 * the rate slider 0x169c, the request radios none 0x1b1c (window +0x1b3c), take off 0x1c10
 * (+0x1c30) and land 0x1d04 (+0x1d24), the frequency edit 0x1df8 (+0x1e18), the "slow"
 * box 0x1e60 (state +0x1f1c), a group box 0x1f28. The descriptor is at +0x1fdc
 * (flights_internal.h). */
#include "flights_internal.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define EM_LIMITTEXT 0xc5
#define GWL_USERDATA (-21)
#define WM_FOCUS_NEXT 0x5fa
#define SERVICES fs98_flights_services()
#define DESCRIPTOR(object) PTR(object, 0x1fdc)

static const char *s_number_format = "%.3f";
void fs98_flights_set_number_format(const char *format) { s_number_format = format; }

/* 0x22315b80, 0x22315bc0, 0x22315bf0: a request radio clicked. Only "none" leaves the
 * box usable; each moves the focus to its radio. */
void TC fs98_flights_comms_land(void *object)
{
    SERVICES->set_checked(AT(object, 0x1e60), 0);
    SERVICES->enable_control(AT(object, 0x1e60), 0);
    SERVICES->post_message(D32(object, 0x20), WM_FOCUS_NEXT, D32(object, 0x1d24), 0);
}
void TC fs98_flights_comms_none(void *object)
{
    SERVICES->enable_control(AT(object, 0x1e60), 1);
    SERVICES->post_message(D32(object, 0x20), WM_FOCUS_NEXT, D32(object, 0x1b3c), 0);
}
void TC fs98_flights_comms_take_off(void *object)
{
    SERVICES->set_checked(AT(object, 0x1e60), 0);
    SERVICES->enable_control(AT(object, 0x1e60), 0);
    SERVICES->post_message(D32(object, 0x20), WM_FOCUS_NEXT, D32(object, 0x1c30), 0);
}

/* 0x22315c30, OnCancel */
void TC fs98_flights_comms_cancel(void *object)
{
    uint32_t *descriptor = DESCRIPTOR(object);
    void **vtable = PTR(object, 0);
    descriptor[0] = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x22315cd0: the controls into the descriptor. A radio is the checked one when its
 * user data is its own window, the window read again after the call. */
void TC fs98_flights_comms_read(void *object)
{
    int32_t data = SERVICES->get_window_long(D32(object, 0x1d24), GWL_USERDATA);
    if (D32(object, 0x1d24) == (uint32_t)data) {
        D32(DESCRIPTOR(object), 0x34) = 2;
    } else {
        data = SERVICES->get_window_long(D32(object, 0x1c30), GWL_USERDATA);
        if (D32(object, 0x1c30) == (uint32_t)data) D32(DESCRIPTOR(object), 0x34) = 1;
        else D32(DESCRIPTOR(object), 0x34) = 0;
    }
    D32(DESCRIPTOR(object), 0x2c) = D32(object, 0x1f1c);
    SERVICES->get_window_text(AT(object, 0x1df8), PTR(DESCRIPTOR(object), 0x10), 0xff);
    {
        int32_t position = SERVICES->slider_position(AT(object, 0x169c));
        D32(DESCRIPTOR(object), 0x3c) = (uint32_t)position;
    }
}

/* 0x22315c50, OnOK: read first, then the vtable is read once */
void TC fs98_flights_comms_accept(void *object)
{
    void **vtable;
    uint32_t failure;
    fs98_flights_comms_read(object);
    vtable = PTR(object, 0);
    failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    D32(DESCRIPTOR(object), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x22315d90: the descriptor into the controls; the focus is put back where it was */
void TC fs98_flights_comms_populate(void *object)
{
    uint32_t focus = SERVICES->get_focus();
    switch (D32(DESCRIPTOR(object), 0x34)) {
    case 0:
        SERVICES->set_radio(AT(object, 0x1b1c), 1);
        SERVICES->set_radio(AT(object, 0x1c10), 0);
        SERVICES->set_radio(AT(object, 0x1d04), 0);
        SERVICES->set_checked(AT(object, 0x1e60), (int32_t)D32(DESCRIPTOR(object), 0x2c));
        break;
    case 1:
    case 2: {
        int32_t land = D32(DESCRIPTOR(object), 0x34) == 2;
        SERVICES->set_radio(AT(object, 0x1b1c), 0);
        SERVICES->set_radio(AT(object, 0x1c10), !land);
        SERVICES->set_radio(AT(object, 0x1d04), land);
        SERVICES->set_checked(AT(object, 0x1e60), 0);
        SERVICES->enable_control(AT(object, 0x1e60), 0);
        break;
    }
    default:
        break;
    }
    SERVICES->set_window_text(AT(object, 0x1df8), PTR(DESCRIPTOR(object), 0x10));
    SERVICES->set_slider_position(AT(object, 0x169c), (int32_t)D32(DESCRIPTOR(object), 0x3c));
    SERVICES->post_message(D32(object, 0x20), WM_FOCUS_NEXT, focus, 0);
}

/* 0x22315e90, vtable +0xd8: read, run MAIN's check (its answer is not looked at) and
 * report a failure through the status. Only when asked about the frequency edit itself
 * is the frequency rewritten to three decimals. */
uint32_t TC fs98_flights_comms_validate(void *object, uint32_t control)
{
    uint32_t failure = 0;
    void *descriptor;
    fs98_flights_comms_read(object);
    descriptor = DESCRIPTOR(object);
    ((fs98_flights_comms_check_fn)PTR(descriptor, 4))(descriptor);
    if (D32(DESCRIPTOR(object), 0xc) != 0) failure = 0xcb3;
    if (control == 0xcb3) {
        char text[0x40];
        double value;
        SERVICES->get_window_text(AT(object, 0x1df8), text, 0x3f);
        value = SERVICES->to_number(text);
        SERVICES->format_number(text, s_number_format, value);
        SERVICES->set_window_text(AT(object, 0x1df8), text);
    }
    return failure;
}

/* 0x22315f20, vtable +0xdc. The error dialog is given the status's address, not its
 * value, as retail does; the answer is whatever vtable +0xe0 returns. */
uint32_t TC fs98_flights_comms_error(void *object, uint32_t code)
{
    uint32_t handled = 0;
    uint32_t *status = (uint32_t *)AT(DESCRIPTOR(object), 0xc);
    (void)code;
    if (*status != 0 &&
        SERVICES->error_dialog(object, AT(object, 0x1df8), (uint32_t)(uintptr_t)status))
        handled = 0xcb3;
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, handled);
}

/* 0x22315930, OnInitDialog */
int32_t TC fs98_flights_comms_init(void *object)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    uint32_t module = fs98_flights_resource_module();
    static const uint32_t radios[] = {0x1d04, 0x1b1c, 0x1c10};
    uint32_t index;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14f, module, 0xf)) return 0;
    SERVICES->init_art(AT(object, 0x10dc), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x11c4), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    SERVICES->init_button(AT(object, 0x12c0), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    SERVICES->init_slider(AT(object, 0x169c), 0x135, 0x136, AT(object, 0xff4), AT(object, 0x7e8),
                          6, 0x1a, 6, 8, 0, -1, module);
    SERVICES->slider_range(AT(object, 0x169c), 0, 0xe);
    SERVICES->init_text(AT(object, 0x152c), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x13bc), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x1474), AT(object, 0x220));
    for (index = 0; index < 3; ++index) {
        void *radio = AT(object, radios[index]);
        ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(radio, 0), 0xc4))(
            radio, AT(object, 0x530), 0x134, AT(object, 0xff4), AT(object, 0x7e8), module);
    }
    SERVICES->init_text(AT(object, 0x15e4), AT(object, 0x220));
    SERVICES->init_edit(AT(object, 0x1df8), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                        AT(object, 0x15e4), 5);
    {
        void *box = AT(object, 0x1e60);
        ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(box, 0), 0xc4))(
            box, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), module);
    }
    SERVICES->init_group(AT(object, 0x1f28), AT(object, 0x4c0));
    SERVICES->send_message(D32(object, 0x1e18), EM_LIMITTEXT, D32(DESCRIPTOR(object), 0x14) - 1, 0);
    fs98_flights_comms_populate(object);
    return 1;
}
