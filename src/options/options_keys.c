/* Customise controls' Keys page (0x3a4, class at 0x223a0ab0, 0x1b64 bytes): the commands of
 * the list shown (flight or slew) beside the keyboard's binding and one joystick's, a check
 * box per command for a joystick's axis, two scroll bars that move the four lists together
 * (vertical) and choose the joystick (horizontal), and a CSimsHotKey and a CSimsHotJoy made
 * at run time to capture a binding. The block is described in options_controls.c.
 *
 * Members: radios 0x1408 (flight, window +0x1428) and 0x14fc (slew, +0x151c), buttons 0x15f0
 * (assign), 0x16ec (reset) and 0x17e8 (clear), labels 0x1128, 0x11e0, 0x1298 and 0x1350 (the
 * joystick's name), the lists 0x18e4 (commands, window +0x1904), 0x192c (keys, +0x194c), 0x1974
 * (joystick, +0x1994) and the check list 0x19bc (+0x19dc), scroll bars 0x1a0c (+0x1a2c) and
 * 0x1a4c (+0x1a6c), the hot key 0x1a8c and hot joystick 0x1af0 (window +0x1b10). Its own words:
 * the device being assigned +0x1b50 (0 the keyboard), the list shown +0x1b54 (the block +0x30
 * or +0x40), each list's scroll position +0x1b58 and +0x1b5c, the joystick shown +0x1b60. */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SC FS98_OPTIONS_STDCALL
#define SERVICES fs98_options_services()
#define PART(page) ((uint8_t *)PTR(page, 0x1118))
#define WHOLE(page) ((uint8_t *)PTR(page, 0x111c))
#define LIST(page) ((uint8_t *)PTR(page, 0x1b54))
#define CHOSEN(page) D32(page, 0x1b50)
#define JOYSTICK(page) D32(page, 0x1b60)
#define LB_ADDSTRING_ 0x180
#define LB_INSERTSTRING_ 0x181
#define LB_DELETESTRING_ 0x182
#define LB_RESETCONTENT_ 0x184
#define LB_SETCURSEL_ 0x186
#define LB_GETCURSEL_ 0x188
#define LB_GETCOUNT_ 0x18b
#define LB_GETTOPINDEX_ 0x18e
#define LB_SETTOPINDEX_ 0x197
#define LB_GETITEMRECT_ 0x198
#define LB_GETITEMHEIGHT_ 0x1a1
#define SB_CTL_ 2
#define GWL_USERDATA_ (-21)
#define COMMANDS 0x1904
#define KEYS 0x194c
#define STICK 0x1994
#define CHECKS 0x19dc

typedef uint32_t (SC *fs98_options_reset_fn)(void *block, uint32_t mode, uint32_t joystick);

static int32_t message(void *page, uint32_t window, uint32_t id, uint32_t w_param, uint32_t l_param)
{
    return SERVICES->send_message(D32(page, window), id, w_param, l_param);
}
static void enable(void *page, uint32_t at, int32_t on) { SERVICES->enable_control(AT(page, at), on); }
static void buttons_off(void *page) { enable(page, 0x15f0, 0); enable(page, 0x17e8, 0); enable(page, 0x16ec, 0); }
/* a device of the list shown, its bindings (value, kind) and its axis settings */
static uint8_t *device_of(void *page, uint32_t index) { return (uint8_t *)PTR(LIST(page), 0xc) + index * 0x14; }
static uint32_t *bindings_of(void *page, uint32_t index) { return PTR(device_of(page, index), 0xc); }
static uint32_t *settings_of(void *page, uint32_t index) { return PTR(device_of(page, index), 0x10); }
static void command_name(void *page, uint32_t index, uint32_t *name) { *name = D32(PTR(LIST(page), 4), index * 8); }

/* 0x223a2420: every list to this first row, and the vertical bar with them */
void TC fs98_options_keys_top(void *page, int32_t top)
{
    SERVICES->set_scroll_pos_window(D32(page, 0x1a2c), SB_CTL_, top, 1);
    message(page, COMMANDS, LB_SETTOPINDEX_, (uint32_t)top, 0);
    message(page, KEYS, LB_SETTOPINDEX_, (uint32_t)top, 0);
    message(page, STICK, LB_SETTOPINDEX_, (uint32_t)top, 0);
    message(page, CHECKS, LB_SETTOPINDEX_, (uint32_t)top, 0);
}
struct text_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *text;
    volatile uint32_t live;
};
static void text_cleanup(struct fs98_options_unwind *frame)
{
    struct text_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(state->text); }
}
/* 0x223a2490: joystick j's bindings into the joystick list and check list; the label and the
 * horizontal bar only when there is more than one joystick */
FS98_OPTIONS_NOINLINE void TC fs98_options_keys_joystick(void *page, uint32_t joystick)
{
    uint32_t index;
    if (D32(LIST(page), 8) > 2) {
        struct text_unwind state;
        struct fs98_options_string text;
        SERVICES->set_scroll_pos_window(D32(page, 0x1a6c), SB_CTL_, (int32_t)joystick, 1);
        state.text = &text; state.live = 0;
        fs98_options_unwind_enter(&state.frame, text_cleanup);
        SERVICES->string_construct(&text);
        state.live = 1;
        SERVICES->string_format(&text, 0xf76d, joystick);
        SERVICES->set_sims_text(AT(page, 0x1350), text.text);
        text_cleanup(&state.frame);
        fs98_options_unwind_leave(&state.frame);
    }
    JOYSTICK(page) = joystick;
    message(page, STICK, LB_RESETCONTENT_, 0, 0);
    message(page, CHECKS, LB_RESETCONTENT_, 0, 0);
    if (D32(LIST(page), 8) <= 1) return;
    for (index = 0; index < D32(LIST(page), 0); ++index) {
        char name[0x34];
        uint32_t *binding;
        SERVICES->host_joystick_name(bindings_of(page, JOYSTICK(page))[index * 2], name, 0x32);
        message(page, STICK, LB_ADDSTRING_, 0, (uint32_t)(uintptr_t)name);
        message(page, CHECKS, LB_ADDSTRING_, 0, (uint32_t)(uintptr_t)fs98_options_empty_text);
        SERVICES->check_list_enable(AT(page, 0x19bc), (int32_t)index, 0);
        SERVICES->check_list_set_check(AT(page, 0x19bc), (int32_t)index, -1);
        binding = bindings_of(page, JOYSTICK(page)) + index * 2;
        if (binding[1] != 2 && binding[0] != 0) {
            SERVICES->check_list_enable(AT(page, 0x19bc), (int32_t)index, 1);
            SERVICES->check_list_set_check(AT(page, 0x19bc), (int32_t)index,
                                           (int32_t)settings_of(page, JOYSTICK(page))[index * 3 + 2]);
        }
    }
    if (message(page, COMMANDS, LB_GETCURSEL_, 0, 0) != -1) {
        message(page, STICK, LB_SETTOPINDEX_, (uint32_t)message(page, COMMANDS, LB_GETTOPINDEX_, 0, 0), 0);
        message(page, STICK, LB_SETCURSEL_, (uint32_t)message(page, COMMANDS, LB_GETCURSEL_, 0, 0), 0);
        message(page, CHECKS, LB_SETTOPINDEX_, (uint32_t)message(page, COMMANDS, LB_GETTOPINDEX_, 0, 0), 0);
        message(page, CHECKS, LB_SETCURSEL_, (uint32_t)message(page, COMMANDS, LB_GETCURSEL_, 0, 0), 0);
    }
    if ((int32_t)CHOSEN(page) > 0) CHOSEN(page) = joystick;
}
/* 0x223a1af0: the commands and the keyboard's bindings, the bars' ranges, the joystick shown */
void TC fs98_options_keys_populate(void *page)
{
    uint32_t index;
    message(page, COMMANDS, LB_RESETCONTENT_, 0, 0);
    message(page, KEYS, LB_RESETCONTENT_, 0, 0);
    for (index = 0; index < D32(LIST(page), 0); ++index) {
        char name[0x34];
        uint32_t command;
        command_name(page, index, &command);
        message(page, COMMANDS, LB_ADDSTRING_, 0, command);
        SERVICES->host_key_name(bindings_of(page, 0)[index * 2], name, 0x32);
        message(page, KEYS, LB_ADDSTRING_, 0, (uint32_t)(uintptr_t)name);
    }
    enable(page, 0x18e4, 0);
    SERVICES->set_scroll_range_window(D32(page, 0x1a2c), SB_CTL_, 0, (int32_t)D32(LIST(page), 0) - 1, 1);
    SERVICES->set_scroll_pos_window(D32(page, 0x1a2c), SB_CTL_, 0, 1);
    if (D32(LIST(page), 8) <= 1) {
        enable(page, 0x1974, 0);
        enable(page, 0x19bc, 0);
    } else {
        if (D32(LIST(page), 8) > 2) {
            SERVICES->enable_scroll_bar(D32(page, 0x1a6c), SB_CTL_, 0);
            SERVICES->set_scroll_range_window(D32(page, 0x1a6c), SB_CTL_, 1, (int32_t)D32(LIST(page), 8) - 1, 1);
        }
        fs98_options_keys_joystick(page, JOYSTICK(page));
    }
    message(page, COMMANDS, LB_SETCURSEL_, 0, 0);
    message(page, KEYS, LB_SETCURSEL_, 0, 0);
    message(page, STICK, LB_SETCURSEL_, 0, 0);
    message(page, CHECKS, LB_SETCURSEL_, 0, 0);
    buttons_off(page);
}
/* 0x223a0f30 (BN_CLICKED on 0x117d) and 0x223a0fb0 (on 0x117e): flight or slew; each list keeps
 * its scroll position. A radio is on when its window's user data is its own handle. */
static void choose_list(void *page, uint32_t radio, uint32_t part, uint32_t save, uint32_t restore, uint32_t list)
{
    int32_t data = SERVICES->get_window_long(D32(page, radio), GWL_USERDATA_);
    if (D32(page, radio) != (uint32_t)data) return;
    CHOSEN(page) = 0;
    PTR(page, 0x1b54) = PART(page) + part;
    D32(page, save) = (uint32_t)SERVICES->get_scroll_pos_window(D32(page, 0x1a2c), SB_CTL_);
    fs98_options_keys_populate(page);
    fs98_options_keys_top(page, (int32_t)D32(page, restore));
    D32(PART(page), 4) = list;
}
void TC fs98_options_keys_flight(void *page) { choose_list(page, 0x1428, 0xc, 0x1b5c, 0x1b58, 0); }
void TC fs98_options_keys_slew(void *page) { choose_list(page, 0x151c, 0x1c, 0x1b58, 0x1b5c, 1); }
/* 0x223a1030 (WM_VSCROLL): the lists by a line, a page, to a position or an end */
void TC fs98_options_keys_vertical(void *page, uint32_t code, uint32_t position, void *bar)
{
    int32_t top;
    if (bar != AT(page, 0x1a0c)) return;
    top = message(page, COMMANDS, LB_GETTOPINDEX_, 0, 0);
    if (top == -1) return;
    switch (code) {
    case 0: --top; break;
    case 1: ++top; break;
    case 2: case 3: {
        struct fs98_options_rect rect;
        int32_t height;
        SERVICES->get_window_rect(D32(page, COMMANDS), &rect);
        height = message(page, COMMANDS, LB_GETITEMHEIGHT_, 0, 0);
        if (code == 2) top -= (rect.bottom - rect.top) / height;
        else top += (rect.bottom - rect.top) / height;
        break;
    }
    case 4: case 5: top = (int32_t)position; break;
    case 6: top = 0; break;
    case 7: top = message(page, COMMANDS, LB_GETCOUNT_, 0, 0) - 1; break;
    default: break;
    }
    if (top < 0) top = 0;
    if (top >= message(page, COMMANDS, LB_GETCOUNT_, 0, 0)) top = message(page, COMMANDS, LB_GETCOUNT_, 0, 0) - 1;
    fs98_options_keys_top(page, top);
}
/* 0x223a1180 (WM_HSCROLL): another joystick */
void TC fs98_options_keys_horizontal(void *page, uint32_t code, uint32_t position, void *bar)
{
    int32_t current, low, high;
    if (bar != AT(page, 0x1a4c)) return;
    current = SERVICES->get_scroll_pos_window(D32(page, 0x1a6c), SB_CTL_);
    SERVICES->get_scroll_range_window(D32(page, 0x1a6c), SB_CTL_, &low, &high);
    switch (code) {
    case 0: --current; break;
    case 1: ++current; break;
    case 2: current += -1 - (high - low) / 4; break;
    case 3: current += (high - low) / 4 + 1; break;
    case 4: case 5: current = (int32_t)position; break;
    case 6: current = low; break;
    case 7: current = high; break;
    default: break;
    }
    if (current < low) current = low;
    if (current > high) current = high;
    fs98_options_keys_joystick(page, (uint32_t)current);
}
/* the other lists to the row picked in one */
static int32_t follow(void *page, uint32_t from, uint32_t a, uint32_t b, uint32_t c)
{
    int32_t row = message(page, from, LB_GETCURSEL_, 0, 0);
    if (row == -1) return -1;
    message(page, a, LB_SETCURSEL_, (uint32_t)row, 0);
    message(page, b, LB_SETCURSEL_, (uint32_t)row, 0);
    message(page, c, LB_SETCURSEL_, (uint32_t)row, 0);
    fs98_options_keys_top(page, message(page, from, LB_GETTOPINDEX_, 0, 0));
    return row;
}
/* 0x223a1250 (LBN_SELCHANGE on 0x122a): a command */
void TC fs98_options_keys_command(void *page)
{
    if (follow(page, COMMANDS, KEYS, STICK, CHECKS) == -1) return;
    buttons_off(page);
}
/* 0x223a12f0 (LBN_SELCHANGE and LBN_SETFOCUS on 0x122b): the keyboard's binding; only a
 * command's key (kind 0) can be assigned, and cleared when it has one */
void TC fs98_options_keys_key(void *page)
{
    int32_t row = follow(page, KEYS, COMMANDS, STICK, CHECKS);
    if (row == -1) return;
    if (bindings_of(page, 0)[row * 2 + 1] == 0) {
        enable(page, 0x15f0, 1);
        enable(page, 0x17e8, bindings_of(page, 0)[row * 2] != 0);
    } else {
        enable(page, 0x15f0, 0);
        enable(page, 0x17e8, 0);
    }
    enable(page, 0x16ec, 1);
    CHOSEN(page) = 0;
}
/* 0x223a13f0 (on 0x122c): the joystick's binding */
void TC fs98_options_keys_stick(void *page)
{
    int32_t row = follow(page, STICK, CHECKS, KEYS, COMMANDS);
    if (row == -1) return;
    enable(page, 0x15f0, 1);
    CHOSEN(page) = JOYSTICK(page);
    enable(page, 0x17e8, bindings_of(page, CHOSEN(page))[row * 2] != 0);
    enable(page, 0x16ec, 1);
}
/* 0x223a14d0 (on 0x1252): a check box: the axis setting's flags */
void TC fs98_options_keys_check(void *page)
{
    int32_t row = follow(page, CHECKS, STICK, KEYS, COMMANDS), check;
    if (row == -1) return;
    buttons_off(page);
    CHOSEN(page) = JOYSTICK(page);
    check = SERVICES->check_list_get_check(AT(page, 0x19bc), row);
    settings_of(page, CHOSEN(page))[row * 3 + 2] = (uint32_t)check;
}
/* 0x223a16b0 (BN_CLICKED on 0x1228): clear the binding */
void TC fs98_options_keys_clear(void *page)
{
    uint32_t window = CHOSEN(page) == 0 ? KEYS : STICK;
    int32_t row = message(page, window, LB_GETCURSEL_, 0, 0);
    if (row == -1) return;
    message(page, window, LB_DELETESTRING_, (uint32_t)row, 0);
    message(page, window, LB_INSERTSTRING_, (uint32_t)row, (uint32_t)(uintptr_t)fs98_options_empty_text);
    message(page, window, LB_SETCURSEL_, (uint32_t)row, 0);
    if (window == STICK) {
        SERVICES->check_list_enable(AT(page, 0x19bc), row, 0);
        SERVICES->check_list_set_check(AT(page, 0x19bc), row, -1);
    }
    bindings_of(page, CHOSEN(page))[row * 2] = 0;
}
/* the capture control over the row, shown and focused */
static void capture_over(void *page, uint32_t window, uint32_t control, int32_t row)
{
    struct fs98_options_rect rect;
    message(page, window, LB_GETITEMRECT_, (uint32_t)row, (uint32_t)(uintptr_t)&rect);
    SERVICES->client_to_screen(D32(page, window), &rect.left);
    SERVICES->client_to_screen(D32(page, window), &rect.right);
    SERVICES->screen_to_client(D32(page, 0x20), &rect.left);
    SERVICES->screen_to_client(D32(page, 0x20), &rect.right);
    SERVICES->move_window(AT(page, control), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 1);
    SERVICES->show_window(AT(page, control), 5);
    SERVICES->set_focus(AT(page, control));
}
/* 0x223a1cb0: capture a key for the command (kind 0 only) */
void TC fs98_options_keys_assign_key(void *page)
{
    int32_t row = message(page, KEYS, LB_GETCURSEL_, 0, 0);
    if (row == -1) return;
    if (bindings_of(page, 0)[row * 2 + 1] != 0) return;
    capture_over(page, KEYS, 0x1a8c, row);
    SERVICES->hot_key_set(AT(page, 0x1a8c), 0);
}
/* 0x223a1d90: capture a joystick's button (kind 2: an axis, 3: either) */
void TC fs98_options_keys_assign_stick(void *page)
{
    int32_t row = message(page, STICK, LB_GETCURSEL_, 0, 0);
    uint32_t chosen, kind, sense = 0;
    if (row == -1) return;
    capture_over(page, STICK, 0x1af0, row);
    SERVICES->update_window(D32(page, 0x1b10));
    chosen = CHOSEN(page);
    kind = bindings_of(page, chosen)[row * 2 + 1];
    if (kind == 2) sense = 1;
    else if (kind == 3) sense = 2;
    SERVICES->hot_joy_set(AT(page, 0x1af0), chosen - 1, sense);
}
/* 0x223a17c0 (BN_CLICKED on 0x1237) */
void TC fs98_options_keys_assign(void *page)
{
    if (CHOSEN(page) == 0) fs98_options_keys_assign_key(page);
    else fs98_options_keys_assign_stick(page);
}
/* 0x223a2720: "already bound: take it?" through the host's message dialog (2); its answer
 * (2 for yes) comes back in the block's +0xc */
static uint32_t ask(const char *format, const char *binding, uint32_t command)
{
    char text[0x140];
    uint32_t block[6];
    SERVICES->format_two(text, format, binding, (const char *)(uintptr_t)command);
    block[0] = 0; block[1] = 0; block[2] = 6; block[3] = 0; block[4] = 1; block[5] = (uint32_t)(uintptr_t)text;
    SERVICES->host_dialog(2, block);
    return block[3] == 2;
}
/* 0x223a1eb0 (on 0x1242: a key captured): a key bound elsewhere is offered back first */
FS98_OPTIONS_NOINLINE void TC fs98_options_keys_key_captured(void *page)
{
    int32_t row = message(page, KEYS, LB_GETCURSEL_, 0, 0);
    uint32_t index;
    char name[0x34];
    if (row == -1) return;
    for (index = 0; index < D32(LIST(page), 0); ++index) {
        struct text_unwind state;
        struct fs98_options_string text;
        uint32_t command;
        if (index == (uint32_t)row) continue;
        if (bindings_of(page, 0)[index * 2] != (uint32_t)SERVICES->hot_key_get(AT(page, 0x1a8c))) continue;
        state.text = &text; state.live = 0;
        fs98_options_unwind_enter(&state.frame, text_cleanup);
        SERVICES->string_construct(&text);
        state.live = 1;
        SERVICES->string_load(&text, 0xf769);
        SERVICES->host_key_name((uint32_t)SERVICES->hot_key_get(AT(page, 0x1a8c)), name, 0x32);
        command_name(page, index, &command);
        if (!ask(text.text, name, command)) {
            text_cleanup(&state.frame);
            fs98_options_unwind_leave(&state.frame);
            goto refresh;
        }
        bindings_of(page, 0)[index * 2] = 0;
        message(page, KEYS, LB_DELETESTRING_, index, 0);
        message(page, KEYS, LB_INSERTSTRING_, index, (uint32_t)(uintptr_t)fs98_options_empty_text);
        text_cleanup(&state.frame);
        fs98_options_unwind_leave(&state.frame);
    }
    bindings_of(page, 0)[row * 2] = (uint32_t)SERVICES->hot_key_get(AT(page, 0x1a8c));
refresh:
    SERVICES->host_key_name(bindings_of(page, 0)[row * 2], name, 0x32);
    message(page, KEYS, LB_DELETESTRING_, (uint32_t)row, 0);
    message(page, KEYS, LB_INSERTSTRING_, (uint32_t)row, (uint32_t)(uintptr_t)name);
    message(page, KEYS, LB_SETCURSEL_, (uint32_t)row, 0);
    SERVICES->set_focus(AT(page, 0x192c));
    SERVICES->show_window(AT(page, 0x1a8c), 0);
}
/* 0x223a20b0 (on 0x1243: a joystick input captured) */
FS98_OPTIONS_NOINLINE void TC fs98_options_keys_stick_captured(void *page)
{
    int32_t row = message(page, STICK, LB_GETCURSEL_, 0, 0);
    void *hot = AT(page, 0x1af0);
    uint32_t index;
    char name[0x34];
    if (row == -1) return;
    if (SERVICES->hot_joy_get(hot) == 0) goto tail;
    for (index = 0; index < D32(LIST(page), 0); ++index) {
        struct text_unwind state;
        struct fs98_options_string text;
        uint32_t command;
        if (index == (uint32_t)row) continue;
        if (bindings_of(page, CHOSEN(page))[index * 2] != SERVICES->hot_joy_get(hot)) continue;
        state.text = &text; state.live = 0;
        fs98_options_unwind_enter(&state.frame, text_cleanup);
        SERVICES->string_construct(&text);
        state.live = 1;
        SERVICES->string_load(&text, 0xf76a);
        SERVICES->host_joystick_name(SERVICES->hot_joy_get(hot), name, 0x32);
        command_name(page, index, &command);
        if (!ask(text.text, name, command)) {
            text_cleanup(&state.frame);
            fs98_options_unwind_leave(&state.frame);
            goto tail;
        }
        bindings_of(page, CHOSEN(page))[index * 2] = 0;
        message(page, STICK, LB_DELETESTRING_, index, 0);
        message(page, STICK, LB_INSERTSTRING_, index, (uint32_t)(uintptr_t)fs98_options_empty_text);
        SERVICES->check_list_enable(AT(page, 0x19bc), (int32_t)index, 0);
        SERVICES->check_list_set_check(AT(page, 0x19bc), (int32_t)index, -1);
        text_cleanup(&state.frame);
        fs98_options_unwind_leave(&state.frame);
    }
    if (index >= D32(LIST(page), 0)) {
        if (bindings_of(page, CHOSEN(page))[row * 2] == SERVICES->hot_joy_get(hot)) goto tail;
        bindings_of(page, CHOSEN(page))[row * 2] = SERVICES->hot_joy_get(hot);
    }
    SERVICES->host_joystick_name(bindings_of(page, CHOSEN(page))[row * 2], name, 0x32);
    message(page, STICK, LB_DELETESTRING_, (uint32_t)row, 0);
    message(page, STICK, LB_INSERTSTRING_, (uint32_t)row, (uint32_t)(uintptr_t)name);
    if (bindings_of(page, CHOSEN(page))[row * 2] != 0 && bindings_of(page, CHOSEN(page))[row * 2 + 1] != 2) {
        SERVICES->check_list_enable(AT(page, 0x19bc), row, 1);
        SERVICES->check_list_set_check(AT(page, 0x19bc), row, 0);
        settings_of(page, CHOSEN(page))[row * 3 + 2] = 0;
    } else {
        SERVICES->check_list_enable(AT(page, 0x19bc), row, 0);
        SERVICES->check_list_set_check(AT(page, 0x19bc), row, -1);
        settings_of(page, CHOSEN(page))[row * 3] = 0x64;
        settings_of(page, CHOSEN(page))[row * 3 + 1] = 0x24;
    }
tail:
    message(page, STICK, LB_SETCURSEL_, (uint32_t)row, 0);
    SERVICES->set_focus(AT(page, 0x1974));
    SERVICES->show_window(hot, 0);
}
/* 0x223a15c0 (BN_CLICKED on 0x1236): reset the device chosen, once the host's message
 * dialog answers yes (0x40) in the block's +0xc, which retail leaves unset beforehand */
FS98_OPTIONS_NOINLINE void TC fs98_options_keys_reset(void *page)
{
    struct text_unwind state;
    struct fs98_options_string text;
    uint32_t block[6];
    state.text = &text; state.live = 0;
    fs98_options_unwind_enter(&state.frame, text_cleanup);
    SERVICES->string_construct(&text);
    state.live = 1;
    SERVICES->string_load(&text, D32(device_of(page, CHOSEN(page)), 0) == 0 ? 0xf76b : 0xf76c);
    block[0] = 0; block[1] = 0; block[2] = 0xc0; block[4] = 1; block[5] = (uint32_t)(uintptr_t)text.text;
    block[3] = 0;
    SERVICES->host_dialog(2, block);
    if (block[3] == 0x40) {
        ((fs98_options_reset_fn)PTR(PART(page), 8))(WHOLE(page), (uint32_t)(uintptr_t)LIST(page), CHOSEN(page));
        fs98_options_keys_populate(page);
    }
    text_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
}
/* 0x223a23f0 (registered message 0x5f9) */
int32_t TC fs98_options_keys_changed(void *page, uint32_t w_param, uint32_t l_param)
{
    (void)w_param; (void)l_param;
    buttons_off(page);
    return 0;
}
/* 0x223a17e0, vtable +0xf0 */
void TC fs98_options_keys_init(void *page)
{
    static const uint32_t buttons[] = {0x16ec, 0x15f0, 0x17e8};
    struct fs98_options_rect empty = {0, 0, 0, 0}, rect;
    uint32_t index, bars[2] = {0x1a0c, 0x1a4c};
    CHOSEN(page) = 0;
    JOYSTICK(page) = 1;
    PTR(page, 0x1b54) = PART(page) + 0xc;
    D32(page, 0x1b58) = 0;
    D32(page, 0x1b5c) = 0;
    for (index = 0; index < 2; ++index) {
        void *radio = AT(page, index ? 0x14fc : 0x1408);
        uint32_t module = fs98_options_resource_module();
        ((int32_t (TC *)(void *, void *, uint32_t, void *, void *, uint32_t))PTR(PTR(radio, 0), 0xc4))(
            radio, AT(page, 0x4c0), 0x134, PTR(page, 0x10fc), PTR(page, 0x10dc), module);
    }
    SERVICES->set_radio(AT(page, 0x1408), 1);
    for (index = 0; index < 3; ++index) {
        SERVICES->init_button(AT(page, buttons[index]), AT(page, 0x680), AT(page, 0x6f0), 0x137, PTR(page, 0x10fc),
                              PTR(page, 0x10dc), 1, 0xffffffff, fs98_options_resource_module(), 1, 1, empty);
        enable(page, buttons[index], 0);
    }
    SERVICES->init_text(AT(page, 0x11e0), AT(page, 0x370));
    SERVICES->init_list(AT(page, 0x18e4), AT(page, 0x450), AT(page, 0x11e0));
    SERVICES->init_text(AT(page, 0x1298), AT(page, 0x370));
    SERVICES->init_list(AT(page, 0x192c), AT(page, 0x450), 0);
    SERVICES->init_text(AT(page, 0x1350), AT(page, 0x370));
    SERVICES->init_list(AT(page, 0x1974), AT(page, 0x450), 0);
    SERVICES->check_list_init(AT(page, 0x19bc), AT(page, 0x450), 0);
    for (index = 0; index < 2; ++index) {
        SERVICES->remap_rect(&rect, AT(page, bars[index]));
        SERVICES->move_window(AT(page, bars[index]), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 1);
    }
    SERVICES->init_text(AT(page, 0x1128), AT(page, 0x220));
    SERVICES->hot_key_create(AT(page, 0x1a8c), 0x40800000, &empty, page, 0x1242);
    SERVICES->hot_key_init(AT(page, 0x1a8c), AT(page, 0x450), 0);
    SERVICES->hot_joy_create(AT(page, 0x1af0), 0x40800000, &empty, page, 0x1243);
    SERVICES->hot_joy_init(AT(page, 0x1af0), AT(page, 0x450), 0);
    fs98_options_keys_populate(page);
}
