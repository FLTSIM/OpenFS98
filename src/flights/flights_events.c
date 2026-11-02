/* Flights' dialog handlers. Offsets are the retail PE32 object layouts; every MFC,
 * SIMSUI and Win32 call goes through the service table, in retail's order, so a
 * harness can run the original routines and these over the same logged stubs. */
#include "flights_internal.h"
#include "flights_tables.inc"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define VCALL(object, slot) ((void (TC *)(void *))PTR(PTR(object, 0), slot))(object)

#define LB_ADDSTRING 0x180
#define LB_RESETCONTENT 0x184
#define LB_SETCURSEL 0x186
#define LB_GETCURSEL 0x188
#define EM_LIMITTEXT 0xc5

static const struct fs98_flights_services *s_services;
static uint32_t s_resource_module;
static const char *s_help_file = "fs98.hlp";
const char fs98_flights_empty_text[] = "";

void fs98_flights_set_services(const struct fs98_flights_services *services) { s_services = services; }
const struct fs98_flights_services *fs98_flights_services(void) { return s_services; }
uint32_t fs98_flights_resource_module(void) { return s_resource_module; }
void fs98_flights_set_resource_module(uint32_t module) { s_resource_module = module; }
void fs98_flights_set_help_file(const char *file) { s_help_file = file; }

/* ---- shared by every class --------------------------------------------------------- */

/* 0x22312310 (vtable +0x8, +0x10), 0x22312320 (+0xc), 0x22312330/0x22312340 (+0x90/+0x94) */
void TC fs98_flights_empty_arg(void *object, uint32_t unused) { (void)object; (void)unused; }
void TC fs98_flights_empty(void *object) { (void)object; }
void TC fs98_flights_nothing(void *object) { (void)object; }
void TC fs98_flights_disable(void *object) { s_services->enable_window(D32(object, 0x20), 0); }
void TC fs98_flights_enable(void *object) { s_services->enable_window(D32(object, 0x20), 1); }
/* 0x223126a0 (+0xac): delete through the deleting destructor */
void TC fs98_flights_post_destroy(void *object)
{
    if (object) ((void *(TC *)(void *, uint32_t))PTR(PTR(object, 0), 4))(object, 1);
}

#define FLIGHTS_CLASS(name, base) \
int32_t TC fs98_flights_##name##_help(void *object, uint32_t unused, void *info) \
{ \
    (void)object; (void)unused; \
    return s_services->win_help(D32(info, 0xc), s_help_file, 0xc, s_help_##name); \
} \
int32_t TC fs98_flights_##name##_context(void *object, uint32_t window, uint32_t unused) \
{ \
    (void)object; (void)unused; \
    s_services->win_help(window, s_help_file, 0xa, s_help_##name); \
    return 1; \
} \
void TC fs98_flights_##name##_exchange(void *object, void *exchange) \
{ \
    uint32_t index; \
    if (base) s_services->base_exchange(object, exchange); \
    for (index = 0; (int32_t)index < (int32_t)s_ddx_count_##name; ++index) \
        s_services->ddx_control(exchange, s_ddx_##name[index][1], \
                                AT(object, s_ddx_##name[index][0])); \
}
FLIGHTS_CLASS(adventures, 1)
FLIGHTS_CLASS(challenges, 1)
FLIGHTS_CLASS(save, 0)          /* 0x22316140 binds its controls without CSimsDialog's */
FLIGHTS_CLASS(comms, 0)         /* 0x22315810 likewise */
FLIGHTS_CLASS(create, 0)        /* 0x22312590 likewise */
FLIGHTS_CLASS(aircraft, 1)
FLIGHTS_CLASS(modeless, 1)     /* its help pair is the welcome dialog's (0x22319e00) */
FLIGHTS_CLASS(opening, 1)
FLIGHTS_CLASS(flight_menu, 1)
FLIGHTS_CLASS(learning_menu, 1)
FLIGHTS_CLASS(select, 0)        /* 0x22316850 binds without CSimsDialog's */
FLIGHTS_CLASS(autopilot, 0)     /* 0x223134f0 likewise */
#undef FLIGHTS_CLASS

/* ---- the pickers: Adventures (0x1108) and Challenges (0x1107) ------------------------
 * The list box's window is at +0x1270, the description edit's at +0x12b8, the answer
 * state at +0x15e0: 1 accepted, 2 cancelled. */

/* 0x22315240: both pickers' OnCancel. The descriptor and vtable are read before either
 * write. */
void TC fs98_flights_picker_cancel(void *object)
{
    uint32_t *descriptor = PTR(object, 0x10dc);
    void *vtable = PTR(object, 0);
    D32(object, 0x15e0) = 2;
    descriptor[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}

/* 0x22313040: both pickers' LBN_SELCHANGE. The record is indexed through the selection
 * just stored, reloaded from a reloaded descriptor. */
void TC fs98_flights_picker_select(void *object)
{
    int32_t selection = s_services->send_message(D32(object, 0x1270), LB_GETCURSEL, 0, 0);
    void *descriptor;
    if (selection == -1) return;
    D32(PTR(object, 0x10dc), 0xc) = (uint32_t)selection;
    descriptor = PTR(object, 0x10dc);
    s_services->set_window_text(AT(object, 0x1298),
        (char *)AT(PTR(descriptor, 0x10), D32(descriptor, 0xc) * (uint32_t)0xa08 + 0x100));
}

/* 0x223154c0 */
void TC fs98_flights_picker_sync(void *object)
{
    int32_t selection = s_services->send_message(D32(object, 0x1270), LB_GETCURSEL, 0, 0);
    if (selection != -1) D32(PTR(object, 0x10dc), 0xc) = (uint32_t)selection;
}

/* 0x22315580: the error check both OnOKs call; it always answers no error, so the
 * second call in each OnOK is never reached. 0x22315590: the descriptor check. */
int32_t TC fs98_flights_picker_no_error(void *object) { (void)object; return 0; }
int32_t TC fs98_flights_picker_validate(void *object) { return PTR(object, 0x10dc) != 0; }

static void fs98_flights_picker_accept(void *object)
{
    D32(object, 0x15e0) = 1;
    fs98_flights_picker_sync(object);
    if (fs98_flights_picker_no_error(object) && fs98_flights_picker_no_error(object)) return;
    {
        uint32_t *descriptor = PTR(object, 0x10dc);
        void *vtable = PTR(object, 0);
        descriptor[0] = 1;
        ((void (TC *)(void *))PTR(vtable, 0xec))(object);
    }
}

/* 0x22312e50: with nothing selected Adventures cancels through the virtual OnCancel */
void TC fs98_flights_adventures_accept(void *object)
{
    if (s_services->send_message(D32(object, 0x1270), LB_GETCURSEL, 0, 0) == -1) {
        VCALL(object, 0xd0);
        return;
    }
    fs98_flights_picker_accept(object);
}

/* 0x22315260: Challenges cancels inline instead */
void TC fs98_flights_challenges_accept(void *object)
{
    if (s_services->send_message(D32(object, 0x1270), LB_GETCURSEL, 0, 0) == -1) {
        fs98_flights_picker_cancel(object);
        return;
    }
    fs98_flights_picker_accept(object);
}

/* Fill the list from the descriptor, reloading it after every message */
static void fs98_flights_picker_fill(void *object)
{
    void *descriptor;
    uint32_t index, offset;
    s_services->send_message(D32(object, 0x1270), LB_RESETCONTENT, 0, 0);
    descriptor = PTR(object, 0x10dc);
    for (index = 0, offset = 0; index < D32(descriptor, 8); ++index, offset += 0xa08) {
        s_services->send_message(D32(object, 0x1270), LB_ADDSTRING, 0,
                                 (uint32_t)(uintptr_t)AT(PTR(descriptor, 0x10), offset));
        descriptor = PTR(object, 0x10dc);
    }
}

/* 0x223130f0: select the saved entry if it is in range, then show its description
 * whether or not it was */
void TC fs98_flights_adventures_populate(void *object)
{
    void *descriptor;
    fs98_flights_picker_fill(object);
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 0xc) < D32(descriptor, 8))
        s_services->send_message(D32(object, 0x1270), LB_SETCURSEL, D32(descriptor, 0xc), 0);
    fs98_flights_picker_select(object);
}

/* 0x223154f0: with any entries, select the saved one, in range or not, and show it */
void TC fs98_flights_challenges_populate(void *object)
{
    void *descriptor;
    fs98_flights_picker_fill(object);
    descriptor = PTR(object, 0x10dc);
    if (D32(descriptor, 8) != 0) {
        s_services->send_message(D32(object, 0x1270), LB_SETCURSEL, D32(descriptor, 0xc), 0);
        fs98_flights_picker_select(object);
    }
}

/* 0x22313090, 0x22315460: LBN_DBLCLK selects and accepts through the virtual OnOK */
void TC fs98_flights_adventures_double_click(void *object)
{
    fs98_flights_picker_select(object);
    VCALL(object, 0xcc);
}
void TC fs98_flights_challenges_double_click(void *object)
{
    fs98_flights_picker_select(object);
    VCALL(object, 0xcc);
}

/* 0x22312ec0, 0x223152e0: OnInitDialog. A failed skin or a missing descriptor returns
 * zero after the members it has already set up. */
static int32_t fs98_flights_picker_init(void *object, int32_t skin, void (TC *populate)(void *))
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    s_services->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, skin, s_resource_module, 7)) return 0;
    s_services->init_art(AT(object, 0x14f8), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    s_services->init_button(AT(object, 0x1300), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                            s_resource_module, 1, 1, empty);
    s_services->init_button(AT(object, 0x13fc), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                            AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                            s_resource_module, 1, 1, empty);
    s_services->init_text(AT(object, 0x10e0), AT(object, 0x370));
    s_services->init_list(AT(object, 0x1250), AT(object, 0x450), AT(object, 0x10e0));
    s_services->init_text(AT(object, 0x1198), AT(object, 0x370));
    s_services->init_edit(AT(object, 0x1298), s_services->base_font(2), 0xffffff, 0, 0xff0000,
                          AT(object, 0x1198), 0);
    if (!fs98_flights_picker_validate(object)) return 0;
    s_services->send_message(D32(object, 0x12b8), EM_LIMITTEXT, 0x3ff, 0);
    populate(object);
    return 1;
}
int32_t TC fs98_flights_adventures_init(void *object)
{ return fs98_flights_picker_init(object, 0x13b, fs98_flights_adventures_populate); }
int32_t TC fs98_flights_challenges_init(void *object)
{ return fs98_flights_picker_init(object, 0x13c, fs98_flights_challenges_populate); }
