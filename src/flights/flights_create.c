/* Create a Flight, dialog 0x1101 (class at 0x22312350, 0x1c64 bytes).
 *
 * Members: OK 0x10e4, Cancel 0x11e0, the four summaries 0x12dc 0x1394 0x144c 0x1504, title
 * art 0x15bc, a picture 0x16a4, Select Aircraft 0x1774, Airport 0x1870, Weather 0x196c,
 * Time 0x1a68, Save 0x1b64. +0x10dc is cleared at the end of OnInitDialog; the answer
 * state is +0x1c60, 1 accepted, 2 cancelled. The descriptor is at +0x10e0 and may be null:
 * every handler but Save tests it. */
#include "flights_internal.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define SERVICES fs98_flights_services()
#define DESCRIPTOR(object) PTR(object, 0x10e0)

static struct fs98_flights_create_state s_own_state;
static struct fs98_flights_create_state *s_state = &s_own_state;
void fs98_flights_set_create_state(struct fs98_flights_create_state *state) { s_state = state; }

/* 0x223129c0, registered with the CRT on first use */
void fs98_flights_create_font_destroy(void) { SERVICES->font_destroy(s_state->font); }

/* 0x22312b50: an empty member, called before each choice and on OK */
static void fs98_flights_create_nothing(void *object) { (void)object; }

/* 0x22312b60: the summaries, and OK and Save usable once there is something to fly */
void TC fs98_flights_create_refresh(void *object)
{
    int32_t ready;
    if (DESCRIPTOR(object) == 0) return;
    ready = D32(DESCRIPTOR(object), 0x2c) != 0;
    SERVICES->enable_control(AT(object, 0x1b64), ready);
    SERVICES->enable_control(AT(object, 0x10e4), ready);
    SERVICES->set_sims_text(AT(object, 0x12dc), PTR(DESCRIPTOR(object), 0x1c));
    SERVICES->set_sims_text(AT(object, 0x1394), PTR(DESCRIPTOR(object), 0x20));
    SERVICES->set_sims_text(AT(object, 0x144c), PTR(DESCRIPTOR(object), 0x24));
    SERVICES->set_sims_text(AT(object, 0x1504), PTR(DESCRIPTOR(object), 0x28));
}

/* 0x223129d0: Save. No test of the descriptor, and no refresh after. */
void TC fs98_flights_create_save(void *object)
{
    void *descriptor;
    fs98_flights_create_nothing(object);
    descriptor = DESCRIPTOR(object);
    if (PTR(descriptor, 8)) ((fs98_flights_create_fn)PTR(descriptor, 8))(descriptor);
}

/* 0x223129f0, 0x22312a20, 0x22312a50, 0x22312a80: a choice, then the summaries again */
static void fs98_flights_create_choose(void *object, uint32_t slot)
{
    void *descriptor;
    fs98_flights_create_nothing(object);
    descriptor = DESCRIPTOR(object);
    if (PTR(descriptor, slot)) ((fs98_flights_create_fn)PTR(descriptor, slot))(descriptor);
    fs98_flights_create_refresh(object);
}
void TC fs98_flights_create_aircraft(void *object) { fs98_flights_create_choose(object, 0xc); }
void TC fs98_flights_create_location(void *object) { fs98_flights_create_choose(object, 0x10); }
void TC fs98_flights_create_weather(void *object) { fs98_flights_create_choose(object, 0x14); }
void TC fs98_flights_create_time(void *object) { fs98_flights_create_choose(object, 0x18); }

/* 0x22312ab0, OnCancel */
void TC fs98_flights_create_cancel(void *object)
{
    if (DESCRIPTOR(object)) D32(DESCRIPTOR(object), 0) = 0;
    D32(object, 0x1c60) = 2;
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

/* 0x22312ae0, OnOK */
void TC fs98_flights_create_accept(void *object)
{
    if (DESCRIPTOR(object)) D32(DESCRIPTOR(object), 0) = 1;
    D32(object, 0x1c60) = 1;
    fs98_flights_create_nothing(object);
    ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
}

/* 0x22312c00: WM_PAINT goes straight to MFC's */
void TC fs98_flights_create_paint(void *object) { SERVICES->default_paint(object); }

static void fs98_flights_create_button(void *object, uint32_t at, uint32_t font, uint32_t alternate,
                                       uint32_t bitmap)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, font), AT(object, alternate), bitmap,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                          fs98_flights_resource_module(), 1, 1, empty);
}

/* 0x223126b0, OnInitDialog. It answers FALSE: it has put the focus on Select Aircraft. */
int32_t TC fs98_flights_create_init(void *object)
{
    uint8_t *font = s_state->font;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14f, fs98_flights_resource_module(), 0xf)) return 0;
    if (!(s_state->guard & 1)) {
        s_state->guard |= 1;
        SERVICES->font_construct(font, SERVICES->base_font(8));
        SERVICES->register_exit(fs98_flights_create_font_destroy);
    }
    D32(font, 4) = D32(object, 0x224);
    D32(font, 8) = 0xff00;
    D32(font, 0xc) = 0;
    D32(font, 0x50) = D32(object, 0x270);
    D32(font, 0x54) = D32(object, 0x274);
    *(uint16_t *)(font + 0x58) = 0x410;
    SERVICES->init_art(AT(object, 0x15bc), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_picture(AT(object, 0x16a4), 0x154, AT(object, 0xff4), AT(object, 0x7e8),
                           fs98_flights_resource_module(), 0xf100);
    fs98_flights_create_button(object, 0x10e4, 0x5a0, 0x610, 0x132);
    fs98_flights_create_button(object, 0x11e0, 0x5a0, 0x610, 0x132);
    fs98_flights_create_button(object, 0x1b64, 0x680, 0x6f0, 0x137);
    fs98_flights_create_button(object, 0x1774, 0x680, 0x6f0, 0x137);
    fs98_flights_create_button(object, 0x1870, 0x680, 0x6f0, 0x137);
    fs98_flights_create_button(object, 0x196c, 0x680, 0x6f0, 0x137);
    fs98_flights_create_button(object, 0x1a68, 0x680, 0x6f0, 0x137);
    SERVICES->init_text(AT(object, 0x12dc), font);
    SERVICES->init_text(AT(object, 0x1394), font);
    SERVICES->init_text(AT(object, 0x144c), font);
    SERVICES->init_text(AT(object, 0x1504), font);
    SERVICES->set_focus(AT(object, 0x1774));
    fs98_flights_create_refresh(object);
    D32(object, 0x10dc) = 0;
    return 0;
}
