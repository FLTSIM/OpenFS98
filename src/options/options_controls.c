/* Customise controls (0x140a, template 0x3a5, class at 0x2239e470, 0x2178 bytes), CONTROLS'
 * key-assignment dialog (+0x58; docs/modules/controls.md), and its three tab pages:
 * Sensitivities (0x3a3), Keys (0x3a4, options_keys.c) and Force Feedback (0x3a7).
 *
 * The block is CONTROLS' D+0x240: answer +0, +4 a callback that does nothing, the tab shown
 * first +8, Sensitivities' part from +0xc (its +4: no joystick, +0xc: joysticks on, +0x10
 * reset a device's sensitivities (stdcall: block, mode 0 keyboard / 1 a joystick / 2 the
 * mouse, the joystick), +0x14 count the joysticks again (stdcall: block)), Keys' part from
 * +0x24, the list shown +0x28 (0 flight, 1 slew), two command lists +0x30 and +0x40 (the
 * count, the commands (name, id: 8 bytes each), the devices' count and the devices: 0x14
 * bytes each, +0 their kind (0 the keyboard, 1 a joystick), +8 the name, +0xc the bindings
 * (value, kind: 8 bytes a command), +0x10 a joystick's axis settings (scale, null zone, flags:
 * 0xc bytes a command)), the mouse's name +0x54, its three axes from +0x58 (name, word, flag:
 * 0x10 each; the word at +0x64), the keyboard's three axes from +0x90 (flight name, slew name,
 * flag at +0x98; the word at +0x9c), Force Feedback's part from +0xc0.
 *
 * The dialog's members: OK 0x10e0 and Cancel 0x11dc, the tab control 0x12d8, title art 0x1fa0,
 * three helpers 0x2088, 0x20d0, 0x2118; the pages at +0x2164, +0x2168, +0x216c, OK once it
 * exists +0x2170, the answer state +0x2174. No CNotify. */
#include "options_internal.h"
#include "options_objects.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define I32(object, offset) (*(int32_t *)((uint8_t *)(object) + (offset)))
#define W16(object, offset) (*(uint16_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SC FS98_OPTIONS_STDCALL
#define SERVICES fs98_options_services()
#define BLOCK(object) PTR(object, 0x10dc)
#define PART(page) ((uint8_t *)PTR(page, 0x1118))
#define WHOLE(page) ((uint8_t *)PTR(page, 0x111c))
#define LB_ADDSTRING_ 0x180
#define LB_RESETCONTENT_ 0x184
#define LB_SETCURSEL_ 0x186
#define LB_GETCURSEL_ 0x188
#define LB_GETCOUNT_ 0x18b
#define CB_ADDSTRING_ 0x143
#define CB_GETCURSEL_ 0x147
#define CB_RESETCONTENT_ 0x14b
#define CB_SETCURSEL_ 0x14e
#define VK_RETURN_ 0xd
#define TAB 0x12d8
#define RDW_ 0x105                  /* RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW */

typedef uint32_t (SC *fs98_options_block_fn)(void *block);
typedef uint32_t (SC *fs98_options_reset_fn)(void *block, uint32_t mode, uint32_t joystick);
typedef uint32_t (SC *fs98_options_applet_fn)(uint32_t window);

static void check_init(void *page, uint32_t at)
{
    void *check = AT(page, at);
    uint32_t module = fs98_options_resource_module();
    ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(check, 0), 0xc4))(
        check, AT(page, 0x4c0), 0x139, PTR(page, 0x10fc), PTR(page, 0x10dc), module);
}
static void page_button(void *page, uint32_t at)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(page, at), AT(page, 0x680), AT(page, 0x6f0), 0x137, PTR(page, 0x10fc),
                          PTR(page, 0x10dc), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
}
static void enable(void *page, uint32_t at, int32_t on) { SERVICES->enable_control(AT(page, at), on); }

/* ---- Force Feedback (0x3a7, class at 0x223a27c0, 0x168c bytes) --------------------------
 * Six boxes and a group 0x15d8. The first (force feedback on, its part's +8) makes the other
 * five usable; they are the part's +0x10, +0x18, +0x20, +0x28 and +0x30. */
static const uint32_t s_force[][2] = {
    {0x1128, 8}, {0x11f0, 0x10}, {0x12b8, 0x18}, {0x1380, 0x20}, {0x1448, 0x28}, {0x1510, 0x30}
};
/* 0x223a2bf0 (BN_CLICKED on 0x1249): on or off, and the others with it */
void TC fs98_options_controls_force_master(void *page)
{
    uint32_t on = D32(page, 0x11e4), index;
    D32(PART(page), 8) = on;
    for (index = 1; index < 6; ++index) enable(page, s_force[index][0], (int32_t)on);
}
/* 0x223a2c50..0x223a2c90 (BN_CLICKED on 0x124a..0x124e): one box into the part */
void TC fs98_options_controls_force_box(void *page, uint32_t index)
{
    D32(PART(page), s_force[index][1]) = D32(page, s_force[index][0] + 0xbc);
}
#define FORCE_BOX(n) void TC fs98_options_controls_force_box##n(void *page) { fs98_options_controls_force_box(page, n); }
FORCE_BOX(1) FORCE_BOX(2) FORCE_BOX(3) FORCE_BOX(4) FORCE_BOX(5)
#undef FORCE_BOX
/* 0x223a2b70 */
void TC fs98_options_controls_force_populate(void *page)
{
    uint32_t index;
    for (index = 0; index < 6; ++index)
        SERVICES->set_checked(AT(page, s_force[index][0]), (int32_t)D32(PART(page), s_force[index][1]));
    fs98_options_controls_force_master(page);
}
/* 0x223a2a30, vtable +0xf0 */
void TC fs98_options_controls_force_init(void *page)
{
    uint32_t index;
    for (index = 0; index < 6; ++index) check_init(page, s_force[index][0]);
    SERVICES->init_group(AT(page, 0x15d8), AT(page, 0x760));
    fs98_options_controls_force_populate(page);
}

/* ---- Sensitivities (0x3a3, class at 0x2239ec00, 0x2584 bytes) ---------------------------
 * Groups 0x1128 and 0x13a0, the joysticks box 0x11dc, the control panel button 0x12a4, the
 * devices list 0x150c (window +0x152c), the axes combo 0x160c (+0x162c), the sensitivity
 * slider 0x170c (+0x172c) and null zone slider 0x1db4 (+0x1dd4) with their labels, the
 * reverse box 0x23a4 and the reset button 0x246c. Its own words: the device chosen +0x2568,
 * the mouse's row in the list +0x256c and the keyboard's +0x2570, the axis chosen for a
 * joystick +0x2574, the keyboard +0x2578 and the mouse +0x257c, the axes combo's row +0x2580.
 * A joystick's axis is one of its device's commands bound with kind 2. */
#define DEVICE(page) D32(page, 0x2568)
#define MOUSE(page) D32(page, 0x256c)
#define KEYBOARD(page) D32(page, 0x2570)
#define LIST(page) D32(WHOLE(page), 0x28)
/* a device of the list shown */
static uint8_t *device(void *page, uint32_t list, uint32_t index)
{
    return (uint8_t *)PTR(WHOLE(page), list ? 0x4c : 0x3c) + index * 0x14;
}
static uint32_t command_count(void *page, uint32_t list) { return D32(WHOLE(page), list ? 0x40 : 0x30); }

/* 0x2239fc00: the keyboard pair for axis k bound by the device chosen */
static const uint32_t s_axis_pairs[2][3][2] = {
    {{0x1004b, 0x1004d}, {0x10048, 0x10050}, {0x10098, 0x1009a}},
    {{0x100cd, 0x100cf}, {0x100cb, 0x100d1}, {0x100d0, 0x100d2}}
};
static uint32_t bound(void *page, uint32_t at_device, uint32_t k, uint32_t list)
{
    uint32_t index, count;
    if (k > 2) return 0;
    count = command_count(page, list);
    for (index = 0; index < count; ++index) {
        uint32_t id = D32(PTR(WHOLE(page), list ? 0x44 : 0x34), index * 8 + 4);
        if ((id == s_axis_pairs[list][k][0] || id == s_axis_pairs[list][k][1]) &&
            D32(PTR(device(page, list, D32(page, at_device)), 0xc), index * 8) != 0)
            return 1;
    }
    return 0;
}
uint32_t TC fs98_options_controls_keyboard_axis(void *page, uint32_t k, uint32_t list)
{
    return bound(page, 0x2568, k, list != 0);
}
/* 0x2239fef0: a mouse axis is usable if the keyboard's pair is bound (the keyboard's row,
 * not the device chosen); its third if either of the first two is */
uint32_t TC fs98_options_controls_mouse_axis(void *page, uint32_t k, uint32_t list)
{
    if (k == 2) return D32(WHOLE(page), 0x60) == 1 || D32(WHOLE(page), 0x70) == 1;
    if (k > 2) return 0;
    return bound(page, 0x2570, k, list != 0);
}
/* 0x223a0360: row n of the axes combo to its axis; answers the count it reached */
uint32_t TC fs98_options_controls_row_axis(void *page, uint32_t n)
{
    uint32_t count = 0, index;
    if (DEVICE(page) == KEYBOARD(page) || DEVICE(page) == MOUSE(page)) {
        int keyboard = DEVICE(page) == KEYBOARD(page);
        for (index = 0; index < 3; ++index) {
            if (D32(WHOLE(page), (keyboard ? 0x98 : 0x60) + index * 0x10) == 0) continue;
            if (count == n) { D32(page, keyboard ? 0x2578 : 0x257c) = index; return count; }
            ++count;
        }
        return count;
    } else {
        uint32_t list = LIST(page), commands = command_count(page, list);
        uint32_t *binding = PTR(device(page, list, DEVICE(page)), 0xc);
        for (index = 0; index < commands; ++index) {
            if (binding[index * 2 + 1] != 2 || binding[index * 2] == 0) continue;
            if (count == n) { D32(page, 0x2574) = index; return count; }
            ++count;
        }
        return count;
    }
}
/* 0x223a0490: the row to show for the axis chosen, or 0 when it is no longer there.
 * RETAIL BUG (kept): OPT-11 (docs/retail-bugs.md): the mouse tests the keyboard's axis, and
 * a joystick tests the binding after its axis (after the list, when the axis is not the last). */
int32_t TC fs98_options_controls_axis_row(void *page)
{
    int32_t row = -1;
    uint32_t count = 0, k;
    if (DEVICE(page) == KEYBOARD(page) || DEVICE(page) == MOUSE(page)) {
        int keyboard = DEVICE(page) == KEYBOARD(page);
        uint32_t flags = keyboard ? 0x98 : 0x60, chosen = keyboard ? 0x2578 : 0x257c;
        k = 0;
        do {
            uint32_t found = keyboard ? fs98_options_controls_keyboard_axis(page, k, LIST(page))
                                      : fs98_options_controls_mouse_axis(page, k, LIST(page));
            D32(WHOLE(page), flags + k * 0x10) = found;
            if (D32(WHOLE(page), flags + k * 0x10) != 0) { ++count; ++row; }
            ++k;
        } while (k <= D32(page, chosen));
        for (k = D32(page, chosen) + 1; k < 3; ++k) {
            uint32_t found = keyboard ? fs98_options_controls_keyboard_axis(page, k, LIST(page))
                                      : fs98_options_controls_mouse_axis(page, k, LIST(page));
            D32(WHOLE(page), flags + k * 0x10) = found;
            if (D32(WHOLE(page), flags + k * 0x10) != 0) ++count;
        }
        if (count == 0) return row;
        if (row == -1) return 0;
        return D32(WHOLE(page), flags + D32(page, 0x2578) * 0x10) ? row : 0;
    } else {
        uint32_t list = LIST(page), axis = D32(page, 0x2574), at, commands;
        uint32_t *binding = PTR(device(page, list, DEVICE(page)), 0xc);
        for (at = 0; at <= axis; ++at)
            if (binding[at * 2 + 1] == 2 && binding[at * 2] != 0) { ++count; ++row; }
        commands = command_count(page, list);
        if (at < commands) {
            for (; at < commands; ++at)
                if (binding[at * 2 + 1] == 2 && binding[at * 2] != 0) ++count;
        }
        if (count == 0) return row;
        if (row == -1) return 0;
        return binding[at * 2] != 0 ? row : 0;
    }
}
/* 0x2239f8c0: the devices, then the mouse; the keyboard's row and the mouse's noted */
void TC fs98_options_controls_fill_devices(void *page)
{
    uint32_t index = 0, list;
    SERVICES->send_message(D32(page, 0x152c), LB_RESETCONTENT_, 0, 0);
    list = LIST(page);
    if (D32(WHOLE(page), list ? 0x48 : 0x38) != 0) {
        do {
            if (D32(device(page, list, index), 0) == 0) KEYBOARD(page) = index;
            SERVICES->send_message(D32(page, 0x152c), LB_ADDSTRING_, 0, D32(device(page, list, index), 8));
            ++index;
        } while (index < D32(WHOLE(page), list ? 0x48 : 0x38));
    }
    SERVICES->send_message(D32(page, 0x152c), LB_ADDSTRING_, 0, D32(WHOLE(page), 0x54));
    MOUSE(page) = index;
}
/* 0x2239f9a0: the axes of the device chosen. Its argument is not used. */
void TC fs98_options_controls_fill_axes(void *page, uint32_t unused)
{
    uint32_t k;
    (void)unused;
    SERVICES->send_message(D32(page, 0x152c), LB_GETCOUNT_, 0, 0);
    SERVICES->send_message(D32(page, 0x162c), CB_RESETCONTENT_, 0, 0);
    if (DEVICE(page) == MOUSE(page)) {
        for (k = 0; k < 3; ++k) {
            if (fs98_options_controls_mouse_axis(page, k, LIST(page))) {
                SERVICES->send_message(D32(page, 0x162c), CB_ADDSTRING_, 0, D32(WHOLE(page), 0x58 + k * 0x10));
                D32(WHOLE(page), 0x60 + k * 0x10) = 1;
            } else D32(WHOLE(page), 0x60 + k * 0x10) = 0;
        }
        return;
    } else {
        uint32_t list = LIST(page), kind = D32(device(page, list, DEVICE(page)), 0);
        if (kind == 1) {
            uint32_t index;
            for (index = 0; index < command_count(page, list); ++index) {
                uint32_t *binding = PTR(device(page, list, DEVICE(page)), 0xc);
                if (binding[index * 2 + 1] == 2 && binding[index * 2] != 0)
                    SERVICES->send_message(D32(page, 0x162c), CB_ADDSTRING_, 0,
                                           D32(PTR(WHOLE(page), list ? 0x44 : 0x34), index * 8));
            }
        } else if (kind == 0) {
            for (k = 0; k < 3; ++k) {
                if (fs98_options_controls_keyboard_axis(page, k, list)) {
                    SERVICES->send_message(D32(page, 0x162c), CB_ADDSTRING_, 0,
                                           D32(WHOLE(page), (list ? 0x94 : 0x90) + k * 0x10));
                    D32(WHOLE(page), 0x98 + k * 0x10) = 1;
                } else D32(WHOLE(page), 0x98 + k * 0x10) = 0;
            }
        }
    }
}
static const uint32_t s_sliders_first[] = {0x1654, 0x170c, 0x1b8c, 0x1c44};   /* sensitivity */
static const uint32_t s_sliders_second[] = {0x1cfc, 0x1db4, 0x2234, 0x22ec};  /* null zone */
/* 0x223a0120: the mouse's axis: the first two have a sensitivity, the third a null zone */
void TC fs98_options_controls_show_mouse(void *page)
{
    uint32_t axis = D32(page, 0x257c), index;
    if (axis <= 1) {
        SERVICES->set_scroll_pos(AT(page, 0x170c), W16(WHOLE(page), 0x64 + axis * 0x10));
        SERVICES->set_scroll_pos(AT(page, 0x1db4), 0);
        SERVICES->redraw_window(D32(page, 0x1dd4), 0, 0, RDW_);
        for (index = 0; index < 4; ++index) enable(page, s_sliders_second[index], 0);
        for (index = 0; index < 4; ++index) enable(page, s_sliders_first[index], 1);
    } else if (axis == 2) {
        SERVICES->set_scroll_pos(AT(page, 0x1db4), W16(WHOLE(page), 0x84));
        SERVICES->set_scroll_pos(AT(page, 0x170c), 0);
        SERVICES->redraw_window(D32(page, 0x172c), 0, 0, RDW_);
        for (index = 0; index < 4; ++index) enable(page, s_sliders_second[index], 1);
        for (index = 0; index < 4; ++index) enable(page, s_sliders_first[index], 0);
    }
}
/* 0x223a0290: the keyboard's axis: a sensitivity only */
void TC fs98_options_controls_show_keyboard(void *page)
{
    uint32_t index;
    SERVICES->set_scroll_pos(AT(page, 0x170c), W16(WHOLE(page), 0x9c + D32(page, 0x2578) * 0x10));
    SERVICES->redraw_window(D32(page, 0x172c), 0, 0, RDW_);
    enable(page, 0x1cfc, 0);
    SERVICES->set_scroll_pos(AT(page, 0x1db4), 1);
    SERVICES->redraw_window(D32(page, 0x1dd4), 0, 0, RDW_);
    for (index = 1; index < 4; ++index) enable(page, s_sliders_second[index], 0);
    for (index = 0; index < 4; ++index) enable(page, s_sliders_first[index], 1);
}
static uint32_t *settings_of(void *page, uint32_t list)
{
    return (uint32_t *)PTR(device(page, list, DEVICE(page)), 0x10) + D32(page, 0x2574) * 3;
}
static uint32_t *axis_settings(void *page) { return settings_of(page, LIST(page)); }
/* 0x223a07c0: a joystick's axis: its scale (negative when reversed) and null zone */
void TC fs98_options_controls_show_joystick(void *page)
{
    uint32_t list = LIST(page), index;      /* which list: decided once */
    int32_t scale = (int32_t)settings_of(page, list)[0];
    if (scale < 0) { scale = -scale; SERVICES->set_checked(AT(page, 0x23a4), 1); }
    else SERVICES->set_checked(AT(page, 0x23a4), 0);
    SERVICES->set_scroll_pos(AT(page, 0x170c), scale);
    SERVICES->set_scroll_pos(AT(page, 0x1db4), (int32_t)settings_of(page, list)[1]);
    for (index = 0; index < 4; ++index) enable(page, s_sliders_second[index], 1);
    for (index = 0; index < 4; ++index) enable(page, s_sliders_first[index], 1);
}
/* 0x2239f7e0 */
void TC fs98_options_controls_sensitivity_populate(void *page)
{
    SERVICES->set_checked(AT(page, 0x11dc), (int32_t)D32(PART(page), 0xc));
    enable(page, 0x12a4, D32(PART(page), 4) == 0);
    fs98_options_controls_fill_devices(page);
    SERVICES->send_message(D32(page, 0x152c), LB_SETCURSEL_, DEVICE(page), 0);
    fs98_options_controls_fill_axes(page, LIST(page));
    SERVICES->send_message(D32(page, 0x162c), CB_SETCURSEL_, D32(page, 0x2580), 0);
    if (DEVICE(page) == MOUSE(page)) {
        enable(page, 0x23a4, 0);
        fs98_options_controls_show_mouse(page);
    } else if (DEVICE(page) == KEYBOARD(page)) {
        enable(page, 0x23a4, 0);
        fs98_options_controls_show_keyboard(page);
    } else {
        enable(page, 0x23a4, 1);
        fs98_options_controls_show_joystick(page);
    }
}
/* 0x2239f3c0 (BN_CLICKED on 0x121c, reset): CONTROLS resets the device chosen */
void TC fs98_options_controls_reset(void *page)
{
    uint32_t reset = D32(PART(page), 0x10), chosen;
    if (reset == 0 || reset == 0xcdcdcdcd) {
        SERVICES->message_box("Bad m_pliveInfo->reset_defaults in OnBnResetDefaults", 0, 0);
        return;
    }
    chosen = DEVICE(page);
    if (chosen == KEYBOARD(page)) ((fs98_options_reset_fn)(uintptr_t)reset)(WHOLE(page), 0, chosen);
    else if (chosen == MOUSE(page)) ((fs98_options_reset_fn)(uintptr_t)reset)(WHOLE(page), 2, chosen);
    else ((fs98_options_reset_fn)(uintptr_t)reset)(WHOLE(page), 1, chosen - 1);
    fs98_options_controls_sensitivity_populate(page);
}
static void show_chosen(void *page)
{
    if (DEVICE(page) == MOUSE(page)) fs98_options_controls_show_mouse(page);
    else if (DEVICE(page) == KEYBOARD(page)) fs98_options_controls_show_keyboard(page);
    else fs98_options_controls_show_joystick(page);
}
/* 0x2239f430 (CBN_SELCHANGE on 0x1244): another axis */
void TC fs98_options_controls_axis(void *page)
{
    uint32_t row = (uint32_t)SERVICES->send_message(D32(page, 0x162c), CB_GETCURSEL_, 0, 0);
    D32(page, 0x2580) = fs98_options_controls_row_axis(page, row);
    show_chosen(page);
}
/* 0x2239f490 (LBN_SELCHANGE on 0x1b0f): another device, from its first axis */
void TC fs98_options_controls_device(void *page)
{
    uint32_t row;
    DEVICE(page) = (uint32_t)SERVICES->send_message(D32(page, 0x152c), LB_GETCURSEL_, 0, 0);
    row = fs98_options_controls_row_axis(page, 0);
    D32(page, 0x2580) = row;
    if (DEVICE(page) == MOUSE(page)) { D32(page, 0x2574) = 0; D32(page, 0x2578) = 0; }
    else if (DEVICE(page) == KEYBOARD(page)) { D32(page, 0x2574) = 0; D32(page, 0x257c) = 0; }
    else { D32(page, 0x257c) = 0; D32(page, 0x2578) = 0; }
    SERVICES->send_message(D32(page, 0x162c), CB_SETCURSEL_, row, 0);
    fs98_options_controls_sensitivity_populate(page);
}
/* 0x2239f520 (on 0x1230): the sensitivity slider; a joystick's negated while reversed */
void TC fs98_options_controls_scale(void *page)
{
    uint32_t reversed = D32(page, 0x2460);
    int32_t position;
    if (DEVICE(page) == MOUSE(page)) {
        position = SERVICES->get_scroll_pos(AT(page, 0x170c));
        W16(WHOLE(page), 0x64 + (D32(page, 0x257c) << 4)) = (uint16_t)position;
    } else if (DEVICE(page) == KEYBOARD(page)) {
        position = SERVICES->get_scroll_pos(AT(page, 0x170c));
        W16(WHOLE(page), 0x9c + (D32(page, 0x2578) << 4)) = (uint16_t)position;
    } else {
        uint32_t list = LIST(page);       /* read before the call */
        position = SERVICES->get_scroll_pos(AT(page, 0x170c));
        settings_of(page, list)[0] = (uint32_t)position;
        if (reversed) settings_of(page, list)[0] = (uint32_t)-(int32_t)settings_of(page, list)[0];
    }
}
/* 0x2239f660 (on 0x1232): the null zone slider */
void TC fs98_options_controls_null_zone(void *page)
{
    int32_t position;
    if (DEVICE(page) == MOUSE(page)) {
        position = SERVICES->get_scroll_pos(AT(page, 0x1db4));
        W16(WHOLE(page), 0x64 + (D32(page, 0x257c) << 4)) = (uint16_t)position;
    } else if (DEVICE(page) == KEYBOARD(page)) {
        position = SERVICES->get_scroll_pos(AT(page, 0x1db4));
        W16(WHOLE(page), 0x9c + (D32(page, 0x2578) << 4)) = (uint16_t)position;
    } else {
        uint32_t list = LIST(page);
        position = SERVICES->get_scroll_pos(AT(page, 0x1db4));
        settings_of(page, list)[1] = (uint32_t)position;
    }
}
/* 0x2239f730 (BN_CLICKED on 0x1251): reversed, the scale's sign to match */
void TC fs98_options_controls_reverse(void *page)
{
    int32_t scale = (int32_t)axis_settings(page)[0];
    if (D32(page, 0x2460) != 0) { if (scale > 0) axis_settings(page)[0] = (uint32_t)-scale; }
    else if (scale < 0) axis_settings(page)[0] = (uint32_t)-scale;
}
/* 0x223a0920 (BN_CLICKED on 0x122f): joysticks on */
void TC fs98_options_controls_joysticks(void *page) { D32(PART(page), 0xc) = D32(page, 0x1298); }
/* 0x223a0930 (BN_CLICKED on 0x121a): Windows' joystick control panel, then CONTROLS counts
 * the joysticks again */
void TC fs98_options_controls_calibrate(void *page)
{
    uint32_t recount = D32(PART(page), 0x14), library;
    fs98_options_applet_fn applet;
    if (recount == 0 || recount == 0xcdcdcdcd) {
        SERVICES->message_box("Bad m_pliveInfo->calibrate in OnBnControlsCalibrate", 0, 0);
        return;
    }
    library = SERVICES->load_library("joy.cpl");
    if (library == 0) return;
    applet = (fs98_options_applet_fn)SERVICES->get_proc_address(library, "ShowJoyCPL");
    if (applet && applet(D32(page, 0x20)) != 2) {
        ((fs98_options_block_fn)PTR(PART(page), 0x14))(WHOLE(page));
        if (SERVICES->host_joystick_count() != 0) {
            D32(PART(page), 4) = 0;
            enable(page, 0x11dc, 1);
        } else {
            D32(PART(page), 4) = 1;
            enable(page, 0x11dc, 0);
        }
        fs98_options_controls_sensitivity_populate(page);
    }
    SERVICES->free_library(library);
}
struct caption_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *caption;
    volatile uint32_t live;
};
static void caption_cleanup(struct fs98_options_unwind *frame)
{
    struct caption_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(state->caption); }
}
/* the second group's caption: flight's or slew's */
static void caption(void *page, struct fs98_options_string *text)
{
    SERVICES->string_load(text, LIST(page) ? 0xf768 : 0xf767);
    SERVICES->set_window_text(AT(page, 0x13a0), text->text);
}
/* 0x223a09f0 (registered message 0x5f9: the Keys page changed the list or a binding) */
FS98_OPTIONS_NOINLINE int32_t TC fs98_options_controls_changed(void *page, uint32_t w_param, uint32_t l_param)
{
    struct caption_unwind state;
    struct fs98_options_string text;
    int32_t row;
    (void)w_param; (void)l_param;
    state.caption = &text; state.live = 0;
    fs98_options_unwind_enter(&state.frame, caption_cleanup);
    SERVICES->string_construct(&text);
    state.live = 1;
    caption(page, &text);
    row = fs98_options_controls_axis_row(page);
    D32(page, 0x2580) = (uint32_t)row;
    SERVICES->send_message(D32(page, 0x162c), CB_SETCURSEL_, (uint32_t)row, 0);
    fs98_options_controls_sensitivity_populate(page);
    caption_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
    return 0;
}
/* 0x2239f060, vtable +0xf0 */
FS98_OPTIONS_NOINLINE void TC fs98_options_controls_sensitivity_init(void *page)
{
    static const uint32_t labels[] = {0x1654, 0x1b8c, 0x1c44};
    static const uint32_t zone_labels[] = {0x1cfc, 0x22ec, 0x2234};
    struct caption_unwind state;
    struct fs98_options_string text;
    uint32_t index, joystickless;
    state.caption = &text; state.live = 0;
    fs98_options_unwind_enter(&state.frame, caption_cleanup);
    SERVICES->string_construct(&text);
    state.live = 1;
    SERVICES->init_group(AT(page, 0x1128), AT(page, 0x220));
    check_init(page, 0x11dc);
    page_button(page, 0x12a4);
    page_button(page, 0x246c);
    SERVICES->init_group(AT(page, 0x13a0), AT(page, 0x220));
    SERVICES->init_text(AT(page, 0x1454), AT(page, 0x370));
    SERVICES->init_list(AT(page, 0x150c), AT(page, 0x450), AT(page, 0x1454));
    SERVICES->init_text(AT(page, 0x1554), AT(page, 0x370));
    SERVICES->init_combo(AT(page, 0x160c), AT(page, 0x3e0), AT(page, 0x1554));
    SERVICES->slider_init(AT(page, 0x170c), 0x135, 0x136, PTR(page, 0x10fc), PTR(page, 0x10dc),
                          6, 0x1a, 6, 8, 0, -1, fs98_options_resource_module());
    SERVICES->set_scroll_range(AT(page, 0x170c), 1, 0x7f);
    for (index = 0; index < 3; ++index) SERVICES->init_text(AT(page, labels[index]), AT(page, 0x220));
    SERVICES->slider_init(AT(page, 0x1db4), 0x135, 0x136, PTR(page, 0x10fc), PTR(page, 0x10dc),
                          6, 0x1a, 6, 8, 0, -1, fs98_options_resource_module());
    SERVICES->set_scroll_range(AT(page, 0x1db4), 1, 0x7f);
    for (index = 0; index < 3; ++index) SERVICES->init_text(AT(page, zone_labels[index]), AT(page, 0x220));
    check_init(page, 0x23a4);
    caption(page, &text);
    D32(page, 0x2568) = 0; D32(page, 0x2578) = 0; D32(page, 0x2574) = 0;
    D32(page, 0x257c) = 0; D32(page, 0x2580) = 0;
    joystickless = D32(PART(page), 4);
    enable(page, 0x11dc, joystickless == 0);
    fs98_options_controls_sensitivity_populate(page);
    caption_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
}

/* ---- the dialog ------------------------------------------------------------------------- */

/* 0x2239e6b0, OnCancel */
void TC fs98_options_controls_cancel(void *object)
{
    uint32_t *block = BLOCK(object);
    void *vtable = PTR(object, 0);
    D32(object, 0x2174) = 2;
    block[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x2239ead0: each page's read, all Options' empty one (0x22395380) */
void TC fs98_options_controls_read(void *object)
{
    fs98_options_analysis_nothing(PTR(object, 0x2164));
    fs98_options_analysis_nothing(PTR(object, 0x2168));
    fs98_options_analysis_nothing(PTR(object, 0x216c));
}
/* 0x2239ebe0 */
uint32_t TC fs98_options_controls_error(void *object)
{
    return fs98_options_no_error(PTR(object, 0x2164)) != 0;
}
/* 0x2239e6d0, OnOK: Options' empty check where the others test the block's, so OK always
 * closes */
void TC fs98_options_controls_accept(void *object)
{
    if (SERVICES->async_key_state(VK_RETURN_) & 1) SERVICES->set_focus(PTR(object, 0x2170));
    D32(object, 0x2174) = 1;
    fs98_options_controls_read(object);
    if (fs98_options_no_error(object) == 0 || fs98_options_controls_error(object) == 0) {
        D32(BLOCK(object), 0) = 1;
        ((void (TC *)(void *))PTR(PTR(object, 0), 0xec))(object);
    }
}
/* 0x2239e730, vtable +0xec */
void TC fs98_options_controls_close(void *object)
{
    ((void (TC *)(void *))PTR(PTR(object, TAB), 0xc4))(AT(object, TAB));
    SERVICES->close(object);
}
/* 0x2239eb00 */
void TC fs98_options_controls_attach(void *object)
{
    static const uint32_t parts[] = {0xc, 0x24, 0xc0};
    void *ok = AT(object, 0x10e0), *cancel = AT(object, 0x11dc);
    uint32_t index;
    for (index = 0; index < 3; ++index) {
        PTR(PTR(object, 0x2164 + index * 4), 0x1118) = (uint8_t *)BLOCK(object) + parts[index];
        PTR(PTR(object, 0x2164 + index * 4), 0x111c) = BLOCK(object);
        PTR(PTR(object, 0x2164 + index * 4), 0x1120) = cancel;
        PTR(PTR(object, 0x2164 + index * 4), 0x1124) = ok;
    }
    PTR(object, 0x2170) = ok;
}
struct page_unwind {
    struct fs98_options_unwind frame;
    void *page;
    volatile uint32_t live;
};
static void page_cleanup(struct fs98_options_unwind *frame)
{
    struct page_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; fs98_options_dialog_delete(state->page); }
}
typedef void *(TC *page_constructor)(void *page, void *parent);
/* as Display options' (options_display.c). RETAIL BUG (kept): FLT-3. */
static FS98_OPTIONS_NOINLINE void new_page(void *object, uint32_t size, page_constructor construct, uint32_t at)
{
    struct page_unwind state;
    void *page;
    state.page = 0; state.live = 0;
    fs98_options_unwind_enter(&state.frame, page_cleanup);
    page = SERVICES->allocate(size);
    state.page = page; state.live = 1;
    if (page) page = construct(page, object);
    state.live = 0;
    fs98_options_unwind_leave(&state.frame);
    PTR(object, at) = page;
    SERVICES->page_init(page, 9);
}
/* 0x2239e790, OnInitDialog */
int32_t TC fs98_options_controls_init(void *object)
{
    static const uint32_t titles[][3] = {{0xf763, 0xc, 0x2164}, {0xf764, 0x24, 0x2168}, {0xf766, 0xc0, 0x216c}};
    struct fs98_options_rect empty = {0, 0, 0, 0};
    char title[0x20];
    void *vtable, *font;
    uint32_t index, module;
    SERVICES->base_init(object);
    vtable = PTR(object, 0);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(vtable, 0xe4))(
            object, 0x14f, fs98_options_resource_module(), 0xf)) return 0;
    if (!fs98_options_has_descriptor(object)) return 0;
    SERVICES->init_art(AT(object, 0x1fa0), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    SERVICES->init_button(AT(object, 0x10e0), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 0, 0, empty);
    SERVICES->init_button(AT(object, 0x11dc), AT(object, 0x5a0), AT(object, 0x610), 0x132, AT(object, 0xff4),
                          AT(object, 0x7e8), 1, 0xffffffff, fs98_options_resource_module(), 0, 0, empty);
    module = fs98_options_resource_module();
    font = SERVICES->base_font(0xa);
    SERVICES->tab_init(AT(object, TAB), object, font, 0x148, AT(object, 0xff4), AT(object, 0x7e8), 0, 0xc4c4c4,
                       5, 0x1d, 0x47, -1, module, 0);
    ((void (TC *)(void *, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t))PTR(vtable, 0xe8))(
        object, 0x150, 0x1d, 0x47, 0x1c5, 0x136, fs98_options_resource_module());
    new_page(object, 0x2584, fs98_options_controls_sensitivity_construct, 0x2164);
    new_page(object, 0x1b64, fs98_options_controls_keys_construct, 0x2168);
    new_page(object, 0x168c, fs98_options_controls_force_construct, 0x216c);
    fs98_options_controls_attach(object);
    for (index = 0; index < 3; ++index) {
        SERVICES->load_string_a(D32(object, 0x80), titles[index][0], title, 0x1d);
        SERVICES->add_page(AT(object, TAB), PTR(object, titles[index][2]), title,
                           (int32_t)D32(BLOCK(object), titles[index][1]));
    }
    for (index = 0; index < 3; ++index) SERVICES->helper_init(AT(object, 0x2088 + index * 0x48), AT(object, TAB));
    SERVICES->set_last_page(AT(object, TAB), PTR(object, 0x216c));
    SERVICES->set_active_tab(AT(object, TAB), (int32_t)D32(BLOCK(object), 8), 0);
    PTR(object, 0x7c) = AT(object, TAB);
    SERVICES->show_window(object, 1);
    return 1;
}
