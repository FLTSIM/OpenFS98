/* Add logbook and Logbook entry, two of MAIN's three logbook dialogs (main_logbook.inc).
 *
 * Add logbook (0x14070001, template 0x394, class at 0x2239a650, 0x1408 bytes): OK 0x10e8,
 * Cancel 0x11e4, the label 0x12e0, the name edit 0x1398 (window +0x13b8) and a CString
 * 0x1400 that DDX_Text binds to it; the answer state +0x1404 (1 accepted, 2 cancelled).
 * MAIN's block is at +0x10e4, its part from +0x28 at +0x10e0, the CNotify at +0x10dc.
 *
 * Logbook entry (0x14070002, template 0x393, class at 0x22399720, 0x1ca4 bytes): the
 * labels 0x10ec and 0x11a4 (the totals row's), OK 0x125c, Cancel 0x1358, title art
 * 0x1454, the labels 0x153c, 0x15f4 (the aircraft's), 0x1760, 0x1818, 0x18d0 and 0x1988,
 * the group box 0x16ac, the edits remarks 0x1a40 (window +0x1a60), date 0x1aa8 (+0x1ac8),
 * night 0x1b10 (+0x1b30), instrument 0x1b78 (+0x1b98) and day 0x1be0 (+0x1c00), the
 * aircraft combo 0x1c48 (+0x1c68) and five CStrings 0x1c90..0x1ca0 for DDX_Text. MAIN's
 * block is at +0x10e0, its part from +0x54 at +0x10dc (options_internal.h). */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define RECORD 0xa08
/* EM_ and CB_ messages: windows.h, through options_unwind.h */

static int32_t skin(void *object, uint32_t flags)
{
    return ((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
        object, 0x14f, fs98_options_resource_module(), flags);
}
static void button(void *object, uint32_t at)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, 0x5a0), AT(object, 0x610), 0x132,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                          fs98_options_resource_module(), 1, 1, empty);
}
static void edit(void *object, uint32_t at, uint32_t label, uint32_t flags)
{
    SERVICES->init_edit(AT(object, at), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(object, label), flags);
}
static uint32_t focus(void *object, uint32_t control)
{
    return ((uint32_t (TC *)(void *, uint32_t))PTR(PTR(object, 0), 0xe0))(object, control);
}

/* ---- Add logbook ------------------------------------------------------------------------ */

#define NEW_BOOK(object) PTR(object, 0x10e0)

/* 0x2239a850, OnCancel: the part and the vtable read before either store */
void TC fs98_options_add_logbook_cancel(void *object)
{
    uint32_t *part = NEW_BOOK(object);
    void *vtable = PTR(object, 0);
    D32(object, 0x1404) = 2;
    part[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x2239aa70: the name into the part, at most 0x100 whatever its room */
void TC fs98_options_add_logbook_read(void *object)
{
    SERVICES->get_window_text(AT(object, 0x1398), PTR(NEW_BOOK(object), 0xc), 0x100);
}
/* 0x2239ab40: MAIN's check, given the part */
uint32_t TC fs98_options_add_logbook_check(void *object)
{
    void *part = NEW_BOOK(object);
    return ((fs98_options_check_fn)PTR(part, 4))(part);
}
/* 0x2239aab0: a name already there passes when +0x1c is zero; any other failure is
 * reported, and a report the user dismisses selects the name for retyping */
uint32_t TC fs98_options_add_logbook_error(void *object)
{
    void *part = NEW_BOOK(object);
    uint32_t status = D32(part, 8);
    if (status == 0x1004) return D32(part, 0x1c) == 0;
    if (status == 0) return 0;
    D32(object, 0x8c) = (uint32_t)SERVICES->error_dialog(object, AT(object, 0x1398), status);
    if (D32(object, 0x8c) == 0) return 0;
    SERVICES->set_focus(AT(object, 0x1398));
    SERVICES->post_message(D32(object, 0x13b8), EM_SETSEL, 0, 0xffffffff);
    return D32(object, 0x8c);
}
/* 0x2239a870, OnOK */
void TC fs98_options_add_logbook_accept(void *object)
{
    D32(object, 0x1404) = 1;
    fs98_options_add_logbook_read(object);
    if (fs98_options_add_logbook_check(object) == 0 || fs98_options_add_logbook_error(object) == 0) {
        uint32_t *part = NEW_BOOK(object);
        void *vtable = PTR(object, 0);
        part[0] = 1;
        ((void (TC *)(void *))PTR(vtable, 0xec))(object);
    }
}
/* 0x2239aa90 */
void TC fs98_options_add_logbook_clear(void *object)
{
    SERVICES->set_window_text(AT(object, 0x1398), fs98_options_empty_text);
}
/* 0x2239ab20: the part is MAIN's block + 0x28, so it is never null unless the block is
 * -0x28; a null block gives 0x28, and OnInitDialog reads through it */
uint32_t TC fs98_options_add_logbook_has_descriptor(void *object)
{
    NEW_BOOK(object) = (uint8_t *)PTR(object, 0x10e4) + 0x28;
    return NEW_BOOK(object) != 0;
}
/* 0x2239a8b0, OnInitDialog: its skin's flags are 9, not 0xf */
int32_t TC fs98_options_add_logbook_init(void *object)
{
    SERVICES->base_init(object);
    if (!skin(object, 9)) return 0;
    button(object, 0x10e8);
    button(object, 0x11e4);
    SERVICES->init_text(AT(object, 0x12e0), AT(object, 0x220));
    edit(object, 0x1398, 0x12e0, 0);
    PTR(object, 0x10dc) = fs98_options_new_notify();
    if (!fs98_options_add_logbook_has_descriptor(object)) return 0;
    SERVICES->send_message(D32(object, 0x13b8), EM_LIMITTEXT, D32(NEW_BOOK(object), 0x10) - 1, 0);
    fs98_options_add_logbook_clear(object);
    return 1;
}

/* ---- Logbook entry ---------------------------------------------------------------------- */

#define ENTRY(object) PTR(object, 0x10dc)

/* 0x22399cd0, vtable +0xec: CSimsDialog's Close */
void TC fs98_options_logbook_entry_close(void *object) { SERVICES->close(object); }
/* 0x22399ce0, OnCancel */
void TC fs98_options_logbook_entry_cancel(void *object)
{
    uint32_t *part = ENTRY(object);
    void *vtable = PTR(object, 0);
    part[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x22399d00, OnOK: validate and report through the vtable read once */
void TC fs98_options_logbook_entry_accept(void *object)
{
    void **vtable = PTR(object, 0);
    uint32_t failure = ((uint32_t (TC *)(void *, uint32_t))vtable[0xd8 / 4])(object, 0);
    if (failure != 0 && ((uint32_t (TC *)(void *, uint32_t))vtable[0xdc / 4])(object, failure) != 0)
        return;
    D32(ENTRY(object), 0) = 1;
    ((void (TC *)(void *))vtable[0xec / 4])(object);
}
/* 0x2239a630: the part is MAIN's block + 0x54 */
uint32_t TC fs98_options_logbook_entry_has_descriptor(void *object)
{
    ENTRY(object) = (uint8_t *)PTR(object, 0x10e0) + 0x54;
    return ENTRY(object) != 0;
}
/* 0x2239a500: the aircraft (the totals row's label, or the combo refilled with the count
 * reloaded after each), then the five fields */
void TC fs98_options_logbook_entry_fill(void *object)
{
    void *part = ENTRY(object);
    if (D32(part, 0xa4)) {
        SERVICES->set_sims_text(AT(object, 0x10ec), (const char *)PTR(part, 0x98) + D32(part, 0xa0) * RECORD);
    } else {
        uint32_t index, offset;
        SERVICES->send_message(D32(object, 0x1c68), CB_RESETCONTENT, 0, 0);
        part = ENTRY(object);
        if (D32(part, 0x9c) != 0) {
            index = 0; offset = 0;
            do {
                SERVICES->send_message(D32(object, 0x1c68), CB_ADDSTRING, 0,
                                       (uint32_t)(uintptr_t)((uint8_t *)PTR(part, 0x98) + offset));
                part = ENTRY(object);
                ++index;
                offset += RECORD;
            } while (index < D32(part, 0x9c));
        }
        SERVICES->send_message(D32(object, 0x1c68), CB_SETCURSEL, D32(ENTRY(object), 0xa0), 0);
    }
    SERVICES->set_window_text(AT(object, 0x1aa8), PTR(ENTRY(object), 0xc));
    SERVICES->set_window_text(AT(object, 0x1a40), PTR(ENTRY(object), 0x34));
    SERVICES->set_window_text(AT(object, 0x1be0), PTR(ENTRY(object), 0x48));
    SERVICES->set_window_text(AT(object, 0x1b10), PTR(ENTRY(object), 0x5c));
    SERVICES->set_window_text(AT(object, 0x1b78), PTR(ENTRY(object), 0x70));
}
/* 0x22399d80, OnInitDialog. The totals row hides the aircraft and shows its own label;
 * its title is string 0xf744, an entry's 0xf74f. The totals row can only have its
 * remarks changed, so the other four edits are disabled and the remarks take the focus. */
int32_t TC fs98_options_logbook_entry_init(void *object)
{
    char title[0x28];
    SERVICES->base_init(object);
    if (!skin(object, 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    if (!fs98_options_logbook_entry_has_descriptor(object)) return 0;
    if (D32(ENTRY(object), 0xa4)) {
        SERVICES->show_window(AT(object, 0x15f4), 0);
        SERVICES->show_window(AT(object, 0x1c48), 0);
        SERVICES->init_text(AT(object, 0x11a4), AT(object, 0x220));
        SERVICES->init_text(AT(object, 0x10ec), AT(object, 0x220));
        SERVICES->load_string_a(D32(object, 0x80), 0xf744, title, 0x27);
    } else {
        SERVICES->init_text(AT(object, 0x15f4), AT(object, 0x370));
        SERVICES->init_combo(AT(object, 0x1c48), AT(object, 0x3e0), AT(object, 0x15f4));
        SERVICES->show_window(AT(object, 0x11a4), 0);
        SERVICES->show_window(AT(object, 0x10ec), 0);
        SERVICES->load_string_a(D32(object, 0x80), 0xf74f, title, 0x27);
    }
    SERVICES->set_window_text(AT(object, 0x1454), title);
    SERVICES->init_art(AT(object, 0x1454), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    button(object, 0x125c);
    button(object, 0x1358);
    SERVICES->init_group(AT(object, 0x16ac), AT(object, 0x220));
    SERVICES->init_text(AT(object, 0x1988), AT(object, 0x220));
    edit(object, 0x1be0, 0x1988, 5);
    SERVICES->init_text(AT(object, 0x18d0), AT(object, 0x220));
    edit(object, 0x1b78, 0x18d0, 5);
    SERVICES->init_text(AT(object, 0x1818), AT(object, 0x220));
    edit(object, 0x1b10, 0x1818, 5);
    SERVICES->init_text(AT(object, 0x153c), AT(object, 0x220));
    edit(object, 0x1aa8, 0x153c, 0x80);
    SERVICES->init_text(AT(object, 0x1760), AT(object, 0x220));
    edit(object, 0x1a40, 0x1760, 0);
    SERVICES->send_message(D32(object, 0x1ac8), EM_LIMITTEXT, D32(ENTRY(object), 0x10) - 1, 0);
    SERVICES->send_message(D32(object, 0x1a60), EM_LIMITTEXT, D32(ENTRY(object), 0x38) - 1, 0);
    SERVICES->send_message(D32(object, 0x1c00), EM_LIMITTEXT, D32(ENTRY(object), 0x4c) - 3, 0);
    SERVICES->send_message(D32(object, 0x1b30), EM_LIMITTEXT, D32(ENTRY(object), 0x60) - 3, 0);
    SERVICES->send_message(D32(object, 0x1b98), EM_LIMITTEXT, D32(ENTRY(object), 0x74) - 3, 0);
    fs98_options_logbook_entry_fill(object);
    if (D32(ENTRY(object), 0xa4)) {
        SERVICES->enable_control(AT(object, 0x1aa8), 0);
        SERVICES->enable_control(AT(object, 0x1b10), 0);
        SERVICES->enable_control(AT(object, 0x1b78), 0);
        SERVICES->enable_control(AT(object, 0x1be0), 0);
        SERVICES->set_focus(AT(object, 0x1a40));
    }
    return 1;
}
/* 0x2239a150, vtable +0xd8: the edits and the aircraft into MAIN's part, then its check.
 * A failure names the first field whose status is set, in this order; with none, the
 * check's own answer. The totals row reads its remarks alone and always passes.
 *
 * Retail's: the aircraft's name is CB_GETLBTEXT's into a stack buffer it does not
 * initialise, cut to 16 characters; with no pick (CB_ERR) nothing is written there and
 * whatever the stack held is copied. Ours copies what its own stack holds. */
uint32_t TC fs98_options_logbook_entry_validate(void *object, uint32_t unused)
{
    char name[0x100];
    void *part = ENTRY(object);
    uint32_t result, index;
    char *out;
    (void)unused;
    if (D32(part, 0xa4)) {
        SERVICES->get_window_text(AT(object, 0x1a40), PTR(part, 0x34), (int32_t)D32(part, 0x38));
        return 0;
    }
    SERVICES->get_window_text(AT(object, 0x1aa8), PTR(part, 0xc), (int32_t)D32(part, 0x10));
    part = ENTRY(object);
    SERVICES->get_window_text(AT(object, 0x1a40), PTR(part, 0x34), (int32_t)D32(part, 0x38));
    part = ENTRY(object);
    SERVICES->get_window_text(AT(object, 0x1be0), PTR(part, 0x48), (int32_t)D32(part, 0x4c));
    part = ENTRY(object);
    SERVICES->get_window_text(AT(object, 0x1b10), PTR(part, 0x5c), (int32_t)D32(part, 0x60));
    part = ENTRY(object);
    SERVICES->get_window_text(AT(object, 0x1b78), PTR(part, 0x70), (int32_t)D32(part, 0x74));
    D32(ENTRY(object), 0xa0) = (uint32_t)SERVICES->send_message(D32(object, 0x1c68), CB_GETCURSEL, 0, 0);
    SERVICES->send_message(D32(object, 0x1c68), CB_GETLBTEXT, D32(ENTRY(object), 0xa0),
                           (uint32_t)(uintptr_t)name);
    name[0x10] = 0;
    out = PTR(ENTRY(object), 0x20);
    for (index = 0; (out[index] = name[index]) != 0; ++index) {}
    part = ENTRY(object);
    result = ((fs98_options_check_fn)PTR(part, 4))(part);
    part = ENTRY(object);
    if (D32(part, 8)) return 0x1199;
    if (D32(part, 0x30)) return 0x1198;
    if (D32(part, 0x44)) return 0x1194;
    if (D32(part, 0x58)) return 0x1196;
    if (D32(part, 0x6c)) return 0x1195;
    return result;
}
struct entry_error_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *first, *second;
    volatile uint32_t live;
};
static void entry_error_cleanup(struct fs98_options_unwind *frame)
{
    struct entry_error_unwind *state = (void *)frame;
    if (state->live > 1) { state->live = 1; SERVICES->string_destroy(state->second); }
    if (state->live > 0) { state->live = 0; SERVICES->string_destroy(state->first); }
}
/* 0x2239a300, vtable +0xdc: the first field whose status is set is reported on its edit,
 * and the focus goes to it when the report is accepted. An instrument time over the
 * total (0x1013) is shown in its label while it is reported.
 *
 * Retail's: the day and night times, and an instrument time failing otherwise, are
 * reported with the address of their status rather than the status. */
FS98_OPTIONS_NOINLINE uint32_t TC fs98_options_logbook_entry_error(void *object, uint32_t code)
{
    struct entry_error_unwind state;
    struct fs98_options_string first, second;
    uint8_t *part;
    uint32_t target = code, result;
    state.first = &first; state.second = &second; state.live = 0;
    fs98_options_unwind_enter(&state.frame, entry_error_cleanup);
    SERVICES->string_construct(&first);
    state.live = 1;
    SERVICES->string_construct(&second);
    state.live = 2;
    if (code == 0) {
        entry_error_cleanup(&state.frame);
        fs98_options_unwind_leave(&state.frame);
        return 0;
    }
    part = ENTRY(object);
    if (D32(part, 8)) {
        if (SERVICES->error_dialog(object, AT(object, 0x1aa8), D32(part, 8))) target = 0x1199;
    } else if (D32(part, 0x30)) {
        if (SERVICES->error_dialog(object, AT(object, 0x1a40), D32(part, 0x30))) target = 0x1198;
    } else if (D32(part, 0x44)) {
        if (SERVICES->error_dialog(object, AT(object, 0x1be0), (uint32_t)(uintptr_t)(part + 0x44))) target = 0x1194;
    } else if (D32(part, 0x58)) {
        if (SERVICES->error_dialog(object, AT(object, 0x1b10), (uint32_t)(uintptr_t)(part + 0x58))) target = 0x1196;
    } else if (D32(part, 0x6c) == 0x1013) {
        SERVICES->get_sims_text(AT(object, 0x18d0), &second);
        SERVICES->window_text_string(AT(object, 0x1b78), &first);
        SERVICES->set_sims_text(AT(object, 0x18d0), first.text);
        if (SERVICES->error_dialog(object, AT(object, 0x1b78), D32(ENTRY(object), 0x6c))) target = 0x1195;
        SERVICES->set_sims_text(AT(object, 0x18d0), second.text);
    } else if (D32(part, 0x6c)) {
        if (SERVICES->error_dialog(object, AT(object, 0x1b78), (uint32_t)(uintptr_t)(part + 0x6c))) target = 0x1195;
    }
    result = focus(object, target);
    entry_error_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
    return result;
}
