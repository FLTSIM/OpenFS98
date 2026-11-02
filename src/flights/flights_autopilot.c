/* Autopilot, dialog 0x1204 (class at 0x22313180, 0x2178 bytes).
 *
 * Members: OK 0x10dc, Cancel 0x11d8, title art 0x12d4, labels 0x13bc (heading) 0x1474
 * (altitude) 0x152c (navigation) 0x1f9c and 0x20bc (vertical speed); check boxes LVL 0x15e4,
 * HDG 0x16ac, ALT 0x1774, ATT 0x183c, yaw damper 0x1ae4, active 0x1a1c, ASH 0x1bac, MNH
 * 0x1c74, AAT 0x1d3c, TO/GA 0x1e04, each with its window at +0x20 and its state at
 * +0xbc; the navigation combo 0x1904 (window +0x1924); edits heading 0x194c (+0x196c),
 * altitude 0x19b4 (+0x19d4), airspeed 0x1ecc (+0x1eec), Mach 0x1f34 (+0x1f54), vertical
 * speed 0x2054 (+0x2074). The descriptor is +0x2174 (flights_internal.h). */
#include "flights_internal.h"
#include "flights_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_FLIGHTS_THISCALL
#define SERVICES fs98_flights_services()
#define DESCRIPTOR(object) PTR(object, 0x2174)
#define FIELD(object, at) D32(DESCRIPTOR(object), at)
#define FIELD_TEXT(object, at) ((char *)PTR(DESCRIPTOR(object), at))

/* the boxes, and where each keeps its state */
enum {
    LEVEL = 0x15e4, HEADING = 0x16ac, ALTITUDE = 0x1774, ATTITUDE = 0x183c, YAW = 0x1ae4,
    ACTIVE = 0x1a1c, AIRSPEED = 0x1bac, MACH = 0x1c74, THROTTLE = 0x1d3c, TAKEOFF = 0x1e04
};
#define ON(object, box) (D32(object, (box) + 0xbc) != 0)
/* the edits and labels */
enum {
    HEADING_EDIT = 0x194c, HEADING_LABEL = 0x13bc, ALTITUDE_EDIT = 0x19b4, ALTITUDE_LABEL = 0x1474,
    AIRSPEED_EDIT = 0x1ecc, MACH_EDIT = 0x1f34, CLIMB_EDIT = 0x2054, CLIMB_LABEL = 0x20bc,
    CLIMB_UNITS = 0x1f9c, NAVIGATION = 0x1904, NAVIGATION_LABEL = 0x152c
};
#define CB_SETCURSEL_ 0x14e
#define CB_GETCURSEL_ 0x147

static void enable(void *object, uint32_t at, int32_t on) { SERVICES->enable_control(AT(object, at), on); }
static void check(void *object, uint32_t at, int32_t on) { SERVICES->set_checked(AT(object, at), on); }
static void redraw(void *object, uint32_t box) { SERVICES->redraw_window(D32(object, box + 0x20), 0, 0, 0x105); }
static void text(void *object, uint32_t at, const char *value) { SERVICES->set_window_text(AT(object, at), value); }
/* clear a box the user has just overridden, and show it */
static void uncheck(void *object, uint32_t box) { check(object, box, 0); redraw(object, box); }

/* 0x22313af0, OnCancel */
void TC fs98_flights_autopilot_cancel(void *object)
{
    uint32_t *descriptor = DESCRIPTOR(object);
    void **vtable = PTR(object, 0);
    descriptor[0] = 0;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x22313b10, OnOK: the vtable read once */
void TC fs98_flights_autopilot_accept(void *object)
{
    void **vtable = PTR(object, 0);
    uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    FIELD(object, 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}

/* 0x22313b50: "Autopilot active". On, the boxes come alive as the other boxes and the
 * aircraft's capabilities allow; off, everything is cleared and disabled. */
void TC fs98_flights_autopilot_active(void *object)
{
    if (ON(object, ACTIVE)) {
        FIELD(object, 0x10) = 1;
        enable(object, LEVEL, 1);
        enable(object, HEADING, 1);
        enable(object, HEADING_EDIT, ON(object, HEADING));
        enable(object, HEADING_LABEL, ON(object, HEADING));
        enable(object, ALTITUDE, 1);
        enable(object, CLIMB_UNITS, 1);
        if (ON(object, ALTITUDE)) {
            enable(object, ALTITUDE_EDIT, 1);
            enable(object, ALTITUDE_LABEL, 1);
            if (FIELD(object, 0x5c) & 8) {
                enable(object, CLIMB_EDIT, 1);
                enable(object, CLIMB_LABEL, 1);
            } else {
                enable(object, CLIMB_EDIT, 0);
                enable(object, CLIMB_LABEL, 0);
                enable(object, CLIMB_UNITS, 0);
            }
        } else {
            enable(object, ALTITUDE_EDIT, 0);
            enable(object, ALTITUDE_LABEL, 0);
            enable(object, CLIMB_EDIT, 0);
            enable(object, CLIMB_LABEL, 0);
        }
        enable(object, ATTITUDE, 1);
        enable(object, NAVIGATION, 1);
        enable(object, NAVIGATION_LABEL, 1);
        if (FIELD(object, 0x5c) & 2) {
            enable(object, AIRSPEED, 1);
            if (ON(object, AIRSPEED)) enable(object, AIRSPEED_EDIT, 1);
        } else {
            enable(object, AIRSPEED, 0);
            enable(object, AIRSPEED_EDIT, 0);
        }
        if (FIELD(object, 0x5c) & 4) {
            enable(object, MACH, 1);
            if (ON(object, MACH)) enable(object, MACH_EDIT, 1);
        } else {
            enable(object, MACH, 0);
            enable(object, MACH_EDIT, 0);
        }
        if ((FIELD(object, 0x5c) & 0x20) && !ON(object, TAKEOFF)) {
            enable(object, AIRSPEED, 0);
            enable(object, AIRSPEED_EDIT, 0);
            enable(object, MACH, 0);
            enable(object, MACH_EDIT, 0);
        }
        return;
    }
    FIELD(object, 0x10) = 0;
    FIELD(object, 0x18) = 0;
    check(object, LEVEL, 0);
    enable(object, LEVEL, 0);
    check(object, HEADING, 0);
    enable(object, HEADING, 0);
    enable(object, HEADING_EDIT, 0);
    enable(object, HEADING_LABEL, 0);
    FIELD(object, 0x44) = 0;
    check(object, ALTITUDE, 0);
    enable(object, ALTITUDE, 0);
    enable(object, ALTITUDE_EDIT, 0);
    enable(object, ALTITUDE_LABEL, 0);
    check(object, ATTITUDE, 0);
    enable(object, ATTITUDE, 0);
    FIELD(object, 0xe0) = 0;
    SERVICES->send_message(D32(object, 0x1924), CB_SETCURSEL_, 0, 0);
    enable(object, NAVIGATION, 0);
    enable(object, NAVIGATION_LABEL, 0);
    FIELD(object, 0x10) = 0;
    check(object, AIRSPEED, 0);
    enable(object, AIRSPEED, 0);
    enable(object, AIRSPEED_EDIT, 0);
    FIELD(object, 0x10) = 0;
    check(object, MACH, 0);
    enable(object, MACH, 0);
    enable(object, MACH_EDIT, 0);
    enable(object, CLIMB_UNITS, 0);
    enable(object, CLIMB_EDIT, 0);
    enable(object, CLIMB_LABEL, 0);
}

/* 0x22313ea0: LVL turns HDG and ATT off */
void TC fs98_flights_autopilot_level(void *object)
{
    if (!ON(object, LEVEL)) return;
    if (ON(object, HEADING)) {
        check(object, HEADING, 0);
        enable(object, HEADING_EDIT, 0);
        enable(object, HEADING_LABEL, 0);
        redraw(object, HEADING);
    }
    if (ON(object, ATTITUDE)) uncheck(object, ATTITUDE);
}

/* 0x223142f0: HDG turns LVL and ATT off and opens its heading */
void TC fs98_flights_autopilot_heading(void *object)
{
    if (ON(object, HEADING)) {
        if (ON(object, LEVEL)) uncheck(object, LEVEL);
        if (ON(object, ATTITUDE)) uncheck(object, ATTITUDE);
        enable(object, HEADING_EDIT, 1);
        text(object, HEADING_EDIT, FIELD_TEXT(object, 0x20));
        enable(object, HEADING_LABEL, 1);
        return;
    }
    text(object, HEADING_EDIT, FIELD_TEXT(object, 0x20));
    enable(object, HEADING_EDIT, 0);
    enable(object, HEADING_LABEL, 0);
}

/* 0x223143c0: ALT turns ATT off and opens its altitude, and the climb rate when the
 * aircraft has one */
void TC fs98_flights_autopilot_altitude(void *object)
{
    if (ON(object, ALTITUDE)) {
        int32_t climb;
        if (ON(object, ATTITUDE)) uncheck(object, ATTITUDE);
        enable(object, ALTITUDE_EDIT, 1);
        text(object, ALTITUDE_EDIT, FIELD_TEXT(object, 0x4c));
        enable(object, ALTITUDE_LABEL, 1);
        climb = (FIELD(object, 0x5c) & 8) != 0;
        enable(object, CLIMB_EDIT, climb);
        enable(object, CLIMB_LABEL, climb);
        enable(object, CLIMB_UNITS, climb);
        return;
    }
    text(object, ALTITUDE_EDIT, FIELD_TEXT(object, 0x4c));
    enable(object, ALTITUDE_EDIT, 0);
    enable(object, ALTITUDE_LABEL, 0);
    enable(object, CLIMB_EDIT, 0);
    enable(object, CLIMB_LABEL, 0);
}

/* 0x223144e0: ATT turns LVL, HDG and ALT off */
void TC fs98_flights_autopilot_attitude(void *object)
{
    if (!ON(object, ATTITUDE)) return;
    if (ON(object, LEVEL)) uncheck(object, LEVEL);
    if (ON(object, HEADING)) {
        uncheck(object, HEADING);
        text(object, HEADING_EDIT, FIELD_TEXT(object, 0x20));
        enable(object, HEADING_EDIT, 0);
        enable(object, HEADING_LABEL, 0);
    }
    if (ON(object, ALTITUDE)) {
        uncheck(object, ALTITUDE);
        text(object, ALTITUDE_EDIT, FIELD_TEXT(object, 0x4c));
        enable(object, ALTITUDE_EDIT, 0);
        enable(object, ALTITUDE_LABEL, 0);
        enable(object, CLIMB_EDIT, 0);
        enable(object, CLIMB_LABEL, 0);
    }
}

/* 0x22313f30: ASH turns AAT and MNH off */
void TC fs98_flights_autopilot_airspeed(void *object)
{
    if (ON(object, AIRSPEED)) {
        if (ON(object, THROTTLE)) uncheck(object, THROTTLE);
        FIELD(object, 0xa0) = 0;
        enable(object, AIRSPEED_EDIT, 1);
        text(object, AIRSPEED_EDIT, FIELD_TEXT(object, 0x7c));
        check(object, MACH, 0);
        enable(object, MACH_EDIT, 0);
        return;
    }
    text(object, AIRSPEED_EDIT, FIELD_TEXT(object, 0x7c));
    enable(object, AIRSPEED_EDIT, 0);
    FIELD(object, 0xa4) = 0;
}

/* 0x22313ff0: MNH turns AAT and ASH off */
void TC fs98_flights_autopilot_mach(void *object)
{
    if (ON(object, MACH)) {
        if (ON(object, THROTTLE)) uncheck(object, THROTTLE);
        FIELD(object, 0xa4) = 0;
        enable(object, MACH_EDIT, 1);
        text(object, MACH_EDIT, FIELD_TEXT(object, 0x90));
        check(object, AIRSPEED, 0);
        enable(object, AIRSPEED_EDIT, 0);
        return;
    }
    text(object, MACH_EDIT, FIELD_TEXT(object, 0x90));
    enable(object, MACH_EDIT, 0);
    FIELD(object, 0xa0) = 0;
}

/* 0x223140c0: AAT turns the speed holds off; off again, it gives back the ones the
 * descriptor remembers */
void TC fs98_flights_autopilot_throttle(void *object)
{
    if (ON(object, THROTTLE)) {
        if (ON(object, AIRSPEED)) {
            uncheck(object, AIRSPEED);
            enable(object, AIRSPEED_EDIT, 0);
        }
        if (ON(object, MACH)) {
            uncheck(object, MACH);
            enable(object, MACH_EDIT, 0);
        }
        return;
    }
    if (FIELD(object, 0x5c) & 2) {
        check(object, AIRSPEED, (int32_t)FIELD(object, 0xa4));
        redraw(object, AIRSPEED);
        if (FIELD(object, 0x10) != 0 && ON(object, AIRSPEED)) enable(object, AIRSPEED_EDIT, 1);
        text(object, AIRSPEED_EDIT, FIELD_TEXT(object, 0x7c));
    }
    if (FIELD(object, 0x5c) & 4) {
        check(object, MACH, (int32_t)FIELD(object, 0xa0));
        redraw(object, MACH);
        if (FIELD(object, 0x10) != 0 && ON(object, MACH)) enable(object, MACH_EDIT, 1);
        text(object, MACH_EDIT, FIELD_TEXT(object, 0x90));
    }
}

/* 0x22314230: TO/GA enables AAT and, while active, the speed holds */
void TC fs98_flights_autopilot_takeoff(void *object)
{
    if (ON(object, TAKEOFF)) {
        enable(object, THROTTLE, 1);
        enable(object, AIRSPEED, ON(object, ACTIVE));
        enable(object, MACH, ON(object, ACTIVE));
        return;
    }
    check(object, THROTTLE, 0);
    enable(object, THROTTLE, 0);
    if (FIELD(object, 0x5c) & 0x20) {
        check(object, AIRSPEED, 0);
        enable(object, AIRSPEED, 0);
        check(object, MACH, 0);
        enable(object, MACH, 0);
    }
}

/* 0x22314630: the controls into the descriptor */
void TC fs98_flights_autopilot_read(void *object)
{
    FIELD(object, 0x10) = ON(object, ACTIVE) ? 1 : 0;
    if (ON(object, ALTITUDE)) {
        FIELD(object, 0x44) = 1;
        SERVICES->get_window_text(AT(object, ALTITUDE_EDIT), FIELD_TEXT(object, 0x4c), 0xff);
        if (FIELD(object, 0x5c) & 8) {
            FIELD(object, 0xbc) = 1;
            SERVICES->get_window_text(AT(object, CLIMB_EDIT), FIELD_TEXT(object, 0xc4), 0xff);
        } else {
            FIELD(object, 0xbc) = 0;
        }
    } else {
        FIELD(object, 0x44) = 0;
        FIELD(object, 0xbc) = 0;
    }
    FIELD(object, 0xd8) = D32(object, YAW + 0xbc);
    if (ON(object, LEVEL)) {
        FIELD(object, 0x18) = 1;
    } else if (ON(object, HEADING)) {
        FIELD(object, 0x18) = 3;
        SERVICES->get_window_text(AT(object, HEADING_EDIT), FIELD_TEXT(object, 0x20), 0xff);
    } else if (ON(object, ATTITUDE)) {
        FIELD(object, 0x18) = 2;
    } else {
        FIELD(object, 0x18) = 0;
    }
    {
        uint32_t mode = (uint32_t)SERVICES->send_message(D32(object, 0x1924), CB_GETCURSEL_, 0, 0);
        if (mode <= 4) FIELD(object, 0xe0) = mode;
    }
    FIELD(object, 0x64) = ON(object, TAKEOFF) ? 1 : 0;
    FIELD(object, 0x6c) = ON(object, THROTTLE) ? 1 : 0;
    if (ON(object, AIRSPEED)) {
        FIELD(object, 0x74) = 1;
        SERVICES->get_window_text(AT(object, AIRSPEED_EDIT), FIELD_TEXT(object, 0x7c), 0xff);
        FIELD(object, 0xa4) = 1;
        FIELD(object, 0xa0) = 0;
    } else if (ON(object, MACH)) {
        FIELD(object, 0x74) = 1;
        SERVICES->get_window_text(AT(object, MACH_EDIT), FIELD_TEXT(object, 0x90), 0xff);
        FIELD(object, 0xa0) = 1;
        FIELD(object, 0xa4) = 0;
    } else {
        FIELD(object, 0x74) = 0;
        FIELD(object, 0xa0) = 0;
        FIELD(object, 0xa4) = 0;
    }
}

/* 0x223148d0: the descriptor into the controls. With no airspeed hold (capability 2)
 * retail disables the Mach controls a second time, not the airspeed ones. */
void TC fs98_flights_autopilot_populate(void *object)
{
    uint32_t flags;
    check(object, ACTIVE, (int32_t)FIELD(object, 0x10));
    text(object, HEADING_EDIT, FIELD_TEXT(object, 0x20));
    text(object, ALTITUDE_EDIT, FIELD_TEXT(object, 0x4c));
    check(object, ALTITUDE, (int32_t)FIELD(object, 0x44));
    SERVICES->set_sims_text(AT(object, ALTITUDE_LABEL), (char *)AT(DESCRIPTOR(object), 0x30));
    text(object, CLIMB_EDIT, FIELD_TEXT(object, 0xc4));
    SERVICES->set_sims_text(AT(object, CLIMB_LABEL), (char *)AT(DESCRIPTOR(object), 0xa8));
    if (FIELD(object, 0x44) != 0) {
        enable(object, ALTITUDE_EDIT, 1);
        enable(object, ALTITUDE_LABEL, 1);
        if (FIELD(object, 0x5c) & 8) {
            enable(object, CLIMB_EDIT, 1);
            enable(object, CLIMB_LABEL, 1);
            enable(object, CLIMB_UNITS, 1);
            goto boxes;
        }
    } else {
        enable(object, ALTITUDE_EDIT, 0);
        enable(object, ALTITUDE_LABEL, 0);
    }
    enable(object, CLIMB_EDIT, 0);
    enable(object, CLIMB_LABEL, 0);
    enable(object, CLIMB_UNITS, 0);
boxes:
    check(object, YAW, (int32_t)FIELD(object, 0xd8));
    check(object, LEVEL, 0);
    check(object, HEADING, 0);
    check(object, ATTITUDE, 0);
    switch (FIELD(object, 0x18)) {
    case 1:
        check(object, LEVEL, 1);
        break;
    case 2:
        check(object, ATTITUDE, 1);
        enable(object, CLIMB_EDIT, 0);
        enable(object, CLIMB_LABEL, 0);
        break;
    case 3:
        check(object, HEADING, 1);
        enable(object, HEADING_EDIT, 1);
        enable(object, HEADING_LABEL, 1);
        break;
    default:
        break;
    }
    if (FIELD(object, 0xe0) <= 4)
        SERVICES->send_message(D32(object, 0x1924), CB_SETCURSEL_, FIELD(object, 0xe0), 0);
    text(object, AIRSPEED_EDIT, FIELD_TEXT(object, 0x7c));
    check(object, AIRSPEED, (int32_t)FIELD(object, 0xa4));
    text(object, MACH_EDIT, FIELD_TEXT(object, 0x90));
    check(object, MACH, (int32_t)FIELD(object, 0xa0));
    flags = FIELD(object, 0x5c);
    if (!(flags & 6)) {
        enable(object, AIRSPEED, 0);
        enable(object, AIRSPEED_EDIT, 0);
        enable(object, MACH_EDIT, 0);
        enable(object, MACH, 0);
    } else {
        if (!(flags & 4)) {
            enable(object, MACH, 0);
            enable(object, MACH_EDIT, 0);
        }
        if (!(FIELD(object, 0x5c) & 2)) {
            enable(object, MACH, 0);
            enable(object, MACH_EDIT, 0);
        }
    }
    check(object, TAKEOFF, (int32_t)FIELD(object, 0x64));
    check(object, THROTTLE, (int32_t)FIELD(object, 0x6c));
    flags = FIELD(object, 0x5c);
    if (!(flags & 0x10)) {
        enable(object, TAKEOFF, 0);
        enable(object, THROTTLE, 0);
    } else if (!ON(object, TAKEOFF)) {
        enable(object, THROTTLE, 0);
        if (FIELD(object, 0x5c) & 0x20) {
            enable(object, AIRSPEED, 0);
            enable(object, AIRSPEED_EDIT, 0);
            enable(object, MACH, 0);
            enable(object, MACH_EDIT, 0);
        }
    } else if (!(flags & 1)) {
        enable(object, THROTTLE, 0);
    } else if (ON(object, THROTTLE)) {
        check(object, AIRSPEED, 0);
        enable(object, AIRSPEED_EDIT, 0);
        check(object, MACH, 0);
        enable(object, MACH_EDIT, 0);
    }
    if (FIELD(object, 0x10) == 0) {
        static const uint32_t all[] = {
            LEVEL, HEADING, HEADING_EDIT, HEADING_LABEL, ALTITUDE, ALTITUDE_EDIT, ALTITUDE_LABEL,
            ATTITUDE, NAVIGATION, NAVIGATION_LABEL, AIRSPEED, MACH, AIRSPEED_EDIT, MACH_EDIT,
            CLIMB_EDIT, CLIMB_LABEL, CLIMB_UNITS
        };
        uint32_t index;
        for (index = 0; index < sizeof(all) / sizeof(all[0]); ++index) enable(object, all[index], 0);
    }
}

/* 0x22314db0, vtable +0xd8: read, run MAIN's check, and name the edit at fault. Retail
 * names the heading edit (0xcb1) for an altitude failure and the altitude edit (0xcb0)
 * for a heading one. Inactive, nothing is at fault; with nothing at fault the controls
 * are filled again. */
uint32_t TC fs98_flights_autopilot_validate(void *object, uint32_t control)
{
    uint32_t failure = 0;
    void *descriptor;
    (void)control;
    fs98_flights_autopilot_read(object);
    descriptor = DESCRIPTOR(object);
    ((fs98_flights_autopilot_check_fn)PTR(descriptor, 4))(descriptor);
    if (ON(object, THROTTLE) && FIELD(object, 0x68) != 0) failure = 0xd25;
    if (!ON(object, ACTIVE)) return 0;
    if (ON(object, ALTITUDE) && FIELD(object, 0x48) != 0) {
        failure = 0xcb1;
    } else if (ON(object, HEADING) && FIELD(object, 0x1c) != 0) {
        failure = 0xcb0;
    } else if (FIELD(object, 0x5c) != 0) {
        if (ON(object, AIRSPEED) && FIELD(object, 0x78) != 0) failure = 0xd26;
        if (ON(object, MACH) && FIELD(object, 0x8c) != 0) failure = 0xd27;
        if (ON(object, ALTITUDE) && FIELD(object, 0xc0) != 0) failure = 0xd28;
    }
    if (failure == 0) fs98_flights_autopilot_populate(object);
    return failure;
}

/* 0x22314e90, vtable +0xdc: the first failing field is reported, its status given to
 * the error dialog by address; the answer is vtable +0xe0's, told which edit it was */
uint32_t TC fs98_flights_autopilot_error(void *object, uint32_t code)
{
    static const struct { uint32_t box, status, edit, id; } fields[] = {
        {HEADING, 0x1c, HEADING_EDIT, 0xcb1}, {ALTITUDE, 0x48, ALTITUDE_EDIT, 0xcb0},
        {AIRSPEED, 0x78, AIRSPEED_EDIT, 0xd26}, {MACH, 0x8c, MACH_EDIT, 0xd27},
        {ALTITUDE, 0xc0, CLIMB_EDIT, 0xd28},
    };
    uint32_t handled = 0, index;
    if (code != 0 && ON(object, ACTIVE)) {
        for (index = 0; index < 5; ++index) {
            uint32_t *status = (uint32_t *)AT(DESCRIPTOR(object), fields[index].status);
            if (!ON(object, fields[index].box) || *status == 0) continue;
            if (SERVICES->error_dialog(object, AT(object, fields[index].edit), (uint32_t)(uintptr_t)status))
                handled = fields[index].id;
            break;
        }
    }
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, handled);
}

/* 0x223136c0, OnInitDialog */
int32_t TC fs98_flights_autopilot_init(void *object)
{
    struct fs98_flights_rect empty = {0, 0, 0, 0};
    uint32_t module = fs98_flights_resource_module();
    static const uint32_t first[] = {LEVEL, HEADING, ALTITUDE, ATTITUDE, YAW};
    static const uint32_t second[] = {AIRSPEED, MACH, THROTTLE, TAKEOFF};
    uint32_t index;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(object, 0x14f, module, 0xf))
        return 0;
    SERVICES->init_art(AT(object, 0x12d4), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x10dc), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
    SERVICES->init_button(AT(object, 0x11d8), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff, module, 1, 1, empty);
#define CHECK_BOX(at) do { \
        void *box = AT(object, at); \
        ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(box, 0), 0xc4))( \
            box, AT(object, 0x4c0), 0x139, AT(object, 0xff4), AT(object, 0x7e8), module); \
    } while (0)
    for (index = 0; index < 5; ++index) CHECK_BOX(first[index]);
    SERVICES->init_text(AT(object, ALTITUDE_LABEL), AT(object, 0x220));
    SERVICES->init_edit(AT(object, ALTITUDE_EDIT), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                        AT(object, ALTITUDE_LABEL), 0);
    SERVICES->init_text(AT(object, HEADING_LABEL), AT(object, 0x220));
    SERVICES->init_edit(AT(object, HEADING_EDIT), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                        AT(object, HEADING_LABEL), 0);
    for (index = 0; index < 4; ++index) CHECK_BOX(second[index]);
    SERVICES->init_edit(AT(object, AIRSPEED_EDIT), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, 0, 1);
    SERVICES->init_edit(AT(object, MACH_EDIT), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, 0, 5);
    SERVICES->init_text(AT(object, CLIMB_UNITS), AT(object, 0x220));
    SERVICES->init_text(AT(object, CLIMB_LABEL), AT(object, 0x220));
    SERVICES->init_edit(AT(object, CLIMB_EDIT), SERVICES->base_font(2), 0xffffff, 0, 0xff0000,
                        AT(object, CLIMB_LABEL), 3);
    SERVICES->init_text(AT(object, NAVIGATION_LABEL), AT(object, 0x370));
    SERVICES->init_combo(AT(object, NAVIGATION), AT(object, 0x3e0), AT(object, NAVIGATION_LABEL));
    SERVICES->send_message(D32(object, 0x1924), CB_SETCURSEL_, 0, 0);
    CHECK_BOX(ACTIVE);
#undef CHECK_BOX
    SERVICES->send_message(D32(object, 0x196c), EM_LIMITTEXT, FIELD(object, 0x24) - 1, 0);
    SERVICES->send_message(D32(object, 0x19d4), EM_LIMITTEXT, FIELD(object, 0x50) - 1, 0);
    SERVICES->send_message(D32(object, 0x2074), EM_LIMITTEXT, FIELD(object, 0xc8) - 1, 0);
    SERVICES->send_message(D32(object, 0x1eec), EM_LIMITTEXT, FIELD(object, 0x80) - 1, 0);
    SERVICES->send_message(D32(object, 0x1f54), EM_LIMITTEXT, FIELD(object, 0x94) - 1, 0);
    enable(object, HEADING_EDIT, 0);
    enable(object, HEADING_LABEL, 0);
    fs98_flights_autopilot_populate(object);
    return 1;
}
