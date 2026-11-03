/* The two small video dialogs DEMO opens.
 *
 * Video finished (0x14040003, template 0x39f, class at 0x2239de00, 0x1290 bytes): a
 * message 0x10dc and OK 0x1194; no descriptor.
 *
 * Video message (0x14040002, template 0x3a0, class at 0x2239e030, 0x13fc bytes): the edit
 * 0x10e0 (window +0x1100), its label 0x1148, Cancel 0x1200 and OK 0x12fc; the answer state
 * +0x13f8 (1 accepted, 2 cancelled) and the descriptor +0x10dc (options_internal.h). */
#include "options_internal.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define EM_LIMITTEXT 0xc5
#define MESSAGE_ROOM 0x37

/* vtable +0xe4, CSimsDialog's skin: the template's art from the resource module */
static int32_t skin(void *object, uint32_t module)
{
    return ((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
        object, 0x14f, module, 0xf);
}
/* a three-piece button in the dialog's fonts and art */
static void button(void *object, uint32_t at)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                          fs98_options_resource_module(), 1, 1, empty);
}

/* ---- Video finished ------------------------------------------------------------------ */

/* 0x2239df40, OnInitDialog */
int32_t TC fs98_options_video_finished_init(void *object)
{
    SERVICES->base_init(object);
    if (!skin(object, fs98_options_resource_module())) return 0;
    button(object, 0x1194);
    SERVICES->init_text(AT(object, 0x10dc), AT(object, 0x220));
    return 1;
}
/* 0x2239dfe0, OnOK: vtable +0xec, CSimsDialog's Close */
void TC fs98_options_video_finished_accept(void *object)
{
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

/* ---- Video message ------------------------------------------------------------------- */

/* 0x2239e400: with a descriptor, the edit starts empty */
uint32_t TC fs98_options_video_message_populate(void *object)
{
    if (PTR(object, 0x10dc) == 0) return 0;
    SERVICES->set_window_text(AT(object, 0x10e0), fs98_options_empty_text);
    return 1;
}
/* 0x2239e1d0, OnInitDialog. It answers 0 either way: the edit has the focus, or the skin
 * failed. */
int32_t TC fs98_options_video_message_init(void *object)
{
    SERVICES->base_init(object);
    if (!skin(object, fs98_options_resource_module())) return 0;
    button(object, 0x12fc);
    button(object, 0x1200);
    SERVICES->init_text(AT(object, 0x1148), AT(object, 0x220));
    SERVICES->init_edit(AT(object, 0x10e0), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                        AT(object, 0x1148), 0);
    SERVICES->send_message(D32(object, 0x1100), EM_LIMITTEXT, MESSAGE_ROOM, 0);
    if (fs98_options_video_message_populate(object)) SERVICES->set_focus(AT(object, 0x10e0));
    return 0;
}
/* 0x2239e300, OnOK: the message into the descriptor, cut to 0x35 characters and without
 * a lead byte left at its end, then always accepted: a failed check is followed by the
 * error check, which finds nothing.
 *
 * Retail's bug: with an empty message it looks at the byte before its buffer, the top
 * byte of the ebp it saved. A stack below 16 MB, as wine's and Windows' main threads have,
 * makes that 0, which is no lead byte, so nothing is written there; this passes 0. */
void TC fs98_options_video_message_accept(void *object)
{
    char buffer[0x800];
    uint32_t length;
    char *message;
    D32(object, 0x13f8) = 1;
    SERVICES->get_window_text(AT(object, 0x10e0), buffer, MESSAGE_ROOM);
    for (length = 0; buffer[length]; ++length) {}
    if (length >= 0x35) buffer[0x35] = 0;
    for (length = 0; buffer[length]; ++length) {}
    if (SERVICES->is_lead_byte(length ? (signed char)buffer[length - 1] : 0) && length)
        buffer[length - 1] = 0;
    message = (char *)PTR(object, 0x10dc) + 8;
    for (length = 0; (message[length] = buffer[length]) != 0; ++length) {}
    if (fs98_options_check(object) == 0 || fs98_options_no_error(object) == 0) {
        D32(PTR(object, 0x10dc), 0) = 1;
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
    }
}
/* 0x2239e3e0, OnCancel: the descriptor and the vtable read before either store */
void TC fs98_options_video_message_cancel(void *object)
{
    uint32_t *descriptor = PTR(object, 0x10dc);
    void *vtable = PTR(object, 0);
    D32(object, 0x13f8) = 2;
    descriptor[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
