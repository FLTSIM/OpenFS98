/* The Logbook (0x1407, template 0x392, class at 0x223980f0, 0x1c00 bytes), MAIN's command
 * 0x1407 (main_logbook.inc), whose block is described in options_internal.h.
 *
 * Members: OK 0x1100, title art 0x11fc, the labels 0x12e4, 0x139c (the rows') and 0x1454
 * (the totals line), the rows list 0x150c (window +0x152c, Options' own list box class),
 * the title edit 0x1554 (window +0x1574), the logbooks list 0x15bc (window +0x15dc), the
 * buttons delete logbook 0x1604 (0x115c), new logbook 0x1700 (0x115b, window +0x1720),
 * delete row 0x17fc (0x1159), edit row 0x18f8 (0x1158) and new row 0x19f4 (0x1157), and a
 * CString 0x1af0 bound to the title. +0x10dc keeps the logbook last shown, +0x10e0..+0x10f4
 * the six tab stops, +0x10f8 the CNotify, +0x10fc MAIN's block, +0x1af4 the answer state
 * (1 accepted, 2 cancelled), +0x1af8/+0x1afc two measures and +0x1b00 the rows label's
 * text. Every button goes through MAIN's lb_button (block +0x150) and then refreshes. */
#include "options_internal.h"
#include "options_unwind.h"

#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define AT(object, offset) ((uint8_t *)(object) + (offset))
#define TC FS98_OPTIONS_THISCALL
#define SERVICES fs98_options_services()
#define BLOCK(object) PTR(object, 0x10fc)
#define BOOKS(object) D32(object, 0x15dc)
#define ROWS(object) D32(object, 0x152c)
#define ROW 0x4f
#define BOOK 0x124
/* EM_, LB_ messages: windows.h, through options_unwind.h */

static struct fs98_options_book_state s_own_state;
static struct fs98_options_book_state *s_state = &s_own_state;
static void *s_list_vtable;
void fs98_options_set_book_state(struct fs98_options_book_state *state) { s_state = state; }
void fs98_options_set_book_list_vtable(void *vtable) { s_list_vtable = vtable; }
/* 0x22398a40, 0x22398a30: their atexit handlers */
void fs98_options_book_total_font_destroy(void) { SERVICES->font_destroy(s_state->total_font); }
void fs98_options_book_rows_font_destroy(void) { SERVICES->font_destroy(s_state->rows_font); }

static uint32_t call(void *object, uint32_t operation, uint32_t index)
{
    void *block = BLOCK(object);
    return ((fs98_options_book_fn)PTR(block, 0x150))(operation, index, block);
}
static int32_t selection(uint32_t window)
{
    return SERVICES->send_message(window, LB_GETCURSEL, 0, 0);
}
static void button(void *object, uint32_t at, uint32_t fonts, uint32_t bitmap)
{
    struct fs98_options_rect empty = {0, 0, 0, 0};
    SERVICES->init_button(AT(object, at), AT(object, fonts), AT(object, fonts + 0x70), bitmap,
                          AT(object, 0xff4), AT(object, 0x7e8), 1, 0xffffffff,
                          fs98_options_resource_module(), 1, 1, empty);
}

/* ---- the rows list's class ---------------------------------------------------------------- */

/* 0x223996f0: its WM_HSCROLL, which does nothing */
void TC fs98_options_book_scroll(void *object, uint32_t a, uint32_t b, uint32_t c)
{ (void)object; (void)a; (void)b; (void)c; }
/* 0x22398380, its deleting destructor (0x22398250 is CSimsListBox's) */
void *TC fs98_options_book_list_delete(void *list, uint32_t flags)
{
    SERVICES->list_destroy(list);
    if (flags & 1) SERVICES->ui_delete(list);
    return list;
}

/* ---- the dialog --------------------------------------------------------------------------- */

/* 0x223984d0, vtable +0xec: the CNotify deleted, then Close */
void TC fs98_options_book_close(void *object)
{
    void *notify = PTR(object, 0x10f8);
    if (notify) ((void *(TC *)(void *, uint32_t))PTR(PTR(notify, 0), 4))(notify, 1);
    SERVICES->close(object);
}
/* 0x223984f0, OnCancel: the block and the vtable read before either store */
void TC fs98_options_book_cancel(void *object)
{
    uint32_t *block = BLOCK(object);
    void *vtable = PTR(object, 0);
    D32(object, 0x1af4) = 2;
    block[0] = 0;
    ((void (TC *)(void *))PTR(vtable, 0xec))(object);
}
/* 0x22399440: both lists' choices into the block, -1 as it is */
void TC fs98_options_book_read(void *object)
{
    D32(BLOCK(object), 0x20) = (uint32_t)selection(BOOKS(object));
    D32(BLOCK(object), 0x4c) = (uint32_t)selection(ROWS(object));
}
/* 0x223995f0: MAIN's check, given the block */
uint32_t TC fs98_options_book_check(void *object)
{
    void *block = BLOCK(object);
    return ((fs98_options_check_fn)PTR(block, 4))(block);
}
/* 0x22398510, OnOK: always accepted, as Video message is */
void TC fs98_options_book_accept(void *object)
{
    D32(object, 0x1af4) = 1;
    fs98_options_book_read(object);
    if (fs98_options_book_check(object) == 0 || fs98_options_no_error(object) == 0) {
        uint32_t *block = BLOCK(object);
        void *vtable = PTR(object, 0);
        block[0] = 1;
        ((void (TC *)(void *))PTR(vtable, 0xec))(object);
    }
}
/* 0x223995e0 */
uint32_t TC fs98_options_book_has_descriptor(void *object) { return BLOCK(object) != 0; }
/* 0x223994a0: the logbooks, each with its own record as its data, the count reloaded after
 * each; the one chosen, kept at +0x10dc */
void TC fs98_options_book_fill_books(void *object)
{
    uint8_t *book = PTR(BLOCK(object), 0x24);
    uint32_t index;
    SERVICES->send_message(BOOKS(object), LB_RESETCONTENT, 0, 0);
    for (index = 0; index < D32(BLOCK(object), 0x1c); ++index, book += BOOK) {
        uint32_t entry = (uint32_t)SERVICES->send_message(BOOKS(object), LB_ADDSTRING, 0, (uint32_t)(uintptr_t)book);
        SERVICES->send_message(BOOKS(object), LB_SETITEMDATA, entry, (uint32_t)(uintptr_t)book);
    }
    SERVICES->send_message(BOOKS(object), LB_SETCURSEL, D32(BLOCK(object), 0x20), 0);
    D32(object, 0x10dc) = D32(BLOCK(object), 0x20);
}
/* a field in the rows' or the totals' format, onto the line after a tab */
static void column(void *line, const char *format, const char *text)
{
    char field[0x1c];
    SERVICES->format_text(field, format, text);
    SERVICES->string_append(line, field);
}
struct show_unwind {
    struct fs98_options_unwind frame;
    struct fs98_options_string *line;
    volatile uint32_t live;
};
static void show_cleanup(struct fs98_options_unwind *frame)
{
    struct show_unwind *state = (void *)frame;
    if (state->live) { state->live = 0; SERVICES->string_destroy(state->line); }
}
/* 0x22398f40, the logbooks list's LBN_SELCHANGE: the logbook chosen, its title in the
 * edit, its rows as tab-separated lines (a short aircraft gets a second tab), and its
 * totals line; with no rows the row buttons and the rows list are disabled. Delete
 * logbook is usable as the logbook is deletable. */
FS98_OPTIONS_NOINLINE void TC fs98_options_book_show_book(void *object)
{
    struct show_unwind state;
    struct fs98_options_string line;
    char buffer[0x1c];
    int32_t chosen;
    uint8_t *row, *remarks;
    uint32_t index;
    state.line = &line; state.live = 0;
    fs98_options_unwind_enter(&state.frame, show_cleanup);
    SERVICES->string_construct(&line);
    state.live = 1;
    chosen = selection(BOOKS(object));
    if (chosen == -1) {
        show_cleanup(&state.frame);
        fs98_options_unwind_leave(&state.frame);
        return;
    }
    D32(BLOCK(object), 0x20) = (uint32_t)chosen;
    SERVICES->list_text(AT(object, 0x15bc), (int32_t)D32(BLOCK(object), 0x20), &line);
    SERVICES->set_window_text(AT(object, 0x1554), line.text);
    call(object, 3, D32(BLOCK(object), 0x20));
    row = PTR(BLOCK(object), 0x50);
    SERVICES->enable_control(AT(object, 0x150c), 1);
    SERVICES->enable_control(AT(object, 0x139c), 1);
    SERVICES->send_message(ROWS(object), LB_RESETCONTENT, 0, 0);
    if (D32(BLOCK(object), 0x48) != 0) {
        remarks = row + 0x1a;
        index = 0;
        do {
            SERVICES->string_assign(&line, (const char *)row);
            SERVICES->string_append(&line, "\t");
            SERVICES->format_text(buffer, "%-16s", (const char *)remarks - 0x11);
            SERVICES->string_append(&line, buffer);
            SERVICES->string_append(&line, "\t");
            SERVICES->format_text(buffer, "%-24s", (const char *)remarks);
            SERVICES->string_append(&line, buffer);
            SERVICES->string_append(&line, "\t");
            if (SERVICES->text_length((const char *)remarks) < 3) SERVICES->string_append_char(&line, '\t');
            SERVICES->format_text(buffer, "%6s", (const char *)remarks + 0x19);
            SERVICES->string_append(&line, buffer);
            SERVICES->string_append(&line, "\t");
            SERVICES->format_text(buffer, "%6s", (const char *)remarks + 0x20);
            SERVICES->string_append(&line, buffer);
            SERVICES->string_append(&line, "\t");
            SERVICES->format_text(buffer, "%6s", (const char *)remarks + 0x27);
            SERVICES->string_append(&line, buffer);
            SERVICES->string_append(&line, "\t");
            SERVICES->format_text(buffer, "%6s", (const char *)remarks + 0x2e);
            SERVICES->string_append(&line, buffer);
            {
                uint32_t entry = (uint32_t)SERVICES->send_message(ROWS(object), LB_ADDSTRING, 0,
                                                                  (uint32_t)(uintptr_t)line.text);
                SERVICES->send_message(ROWS(object), LB_SETITEMDATA, entry, (uint32_t)(uintptr_t)row);
            }
            ++index;
            row += ROW;
            remarks += ROW;
        } while (index < D32(BLOCK(object), 0x48));
    }
    if (D32(BLOCK(object), 0x48) < 1) {
        SERVICES->set_sims_text(AT(object, 0x1454), fs98_options_empty_text);
        SERVICES->enable_control(AT(object, 0x18f8), 0);
        SERVICES->enable_control(AT(object, 0x17fc), 0);
        SERVICES->enable_control(AT(object, 0x150c), 0);
        SERVICES->enable_control(AT(object, 0x139c), 0);
    } else {
        SERVICES->load_string_a(D32(object, 0x80), 0xf759, buffer, 0x18);
        SERVICES->string_assign(&line, buffer);
        SERVICES->string_append(&line, "\t");
        column(&line, "%6s", PTR(BLOCK(object), 0x100));
        SERVICES->string_append(&line, "\t");
        column(&line, "%6s", PTR(BLOCK(object), 0x114));
        SERVICES->string_append(&line, "\t");
        column(&line, "%6s", PTR(BLOCK(object), 0x128));
        SERVICES->string_append(&line, "\t");
        column(&line, "%6s", PTR(BLOCK(object), 0x13c));
        SERVICES->set_sims_text(AT(object, 0x1454), line.text);
        SERVICES->enable_control(AT(object, 0x18f8), 1);
        SERVICES->enable_control(AT(object, 0x17fc), 1);
        D32(BLOCK(object), 0x4c) = 0;
        SERVICES->send_message(ROWS(object), LB_SETCURSEL, 0, 0);
        call(object, 7, 0);
    }
    {
        uint8_t *book = (uint8_t *)(uintptr_t)(uint32_t)SERVICES->send_message(BOOKS(object), LB_GETITEMDATA,
                                                                                D32(BLOCK(object), 0x20), 0);
        SERVICES->enable_control(AT(object, 0x1604), D32(book, 0x120) ? 1 : 0);
    }
    show_cleanup(&state.frame);
    fs98_options_unwind_leave(&state.frame);
}
/* 0x22399540: with logbooks, the list and the one chosen; with none, everything off */
void TC fs98_options_book_refresh(void *object)
{
    if (D32(BLOCK(object), 0x1c) != 0) {
        SERVICES->enable_control(AT(object, 0x19f4), 1);
        fs98_options_book_fill_books(object);
        fs98_options_book_show_book(object);
        return;
    }
    SERVICES->enable_control(AT(object, 0x1604), 0);
    SERVICES->enable_control(AT(object, 0x17fc), 0);
    SERVICES->enable_control(AT(object, 0x19f4), 0);
    SERVICES->enable_control(AT(object, 0x18f8), 0);
    SERVICES->set_window_text(AT(object, 0x1554), fs98_options_empty_text);
    SERVICES->send_message(BOOKS(object), LB_RESETCONTENT, 0, 0);
    SERVICES->send_message(ROWS(object), LB_RESETCONTENT, 0, 0);
}
/* a row's fields into what the entry dialog opens with */
static void open_row(void *object, uint32_t at)
{
    static const uint32_t fields[][2] = {
        {0, 0x60}, {0x1a, 0x88}, {0x33, 0x9c}, {0x3a, 0xb0}, {0x41, 0xc4}, {0x48, 0xd8}, {9, 0x74}
    };
    uint32_t field, index;
    for (field = 0; field < 7; ++field) {
        void *block = BLOCK(object);
        const char *from = (const char *)PTR(block, 0x50) + at + fields[field][0];
        char *to = PTR(block, fields[field][1]);
        for (index = 0; (to[index] = from[index]) != 0; ++index) {}
    }
}
/* 0x22398a90 (0x1157): a new row in the logbook chosen, opening with the first row's
 * fields when there is one */
void TC fs98_options_book_new_row(void *object)
{
    int32_t chosen = selection(BOOKS(object));
    if (chosen == -1) return;
    D32(BLOCK(object), 0x20) = (uint32_t)chosen;
    call(object, 3, (uint32_t)chosen);
    D32(BLOCK(object), 0xe8) = 4;
    if (D32(BLOCK(object), 0x48) != 0) open_row(object, 0);
    call(object, 4, (uint32_t)chosen);
    fs98_options_book_refresh(object);
    SERVICES->set_focus(AT(object, 0x19f4));
}
/* 0x22398c50 (0x1158, and the rows' double click): the row chosen, edited */
void TC fs98_options_book_edit_row(void *object)
{
    int32_t chosen = selection(BOOKS(object)), row;
    if (chosen == -1) return;
    D32(BLOCK(object), 0x20) = (uint32_t)chosen;
    call(object, 3, (uint32_t)chosen);
    row = selection(ROWS(object));
    if (row == -1) return;
    D32(BLOCK(object), 0x4c) = (uint32_t)row;
    call(object, 7, (uint32_t)row);
    D32(BLOCK(object), 0xe8) = 5;
    open_row(object, (uint32_t)row * ROW);
    call(object, 5, D32(BLOCK(object), 0x4c));
    fs98_options_book_refresh(object);
    SERVICES->set_focus(AT(object, 0x18f8));
}
/* 0x22398e50 (0x1159) */
void TC fs98_options_book_delete_row(void *object)
{
    int32_t row = selection(ROWS(object));
    if (row == -1) return;
    D32(BLOCK(object), 0x4c) = (uint32_t)row;
    call(object, 6, D32(BLOCK(object), 0x4c));
    fs98_options_book_refresh(object);
}
/* 0x22398ea0 (0x115b) */
void TC fs98_options_book_new_book(void *object)
{
    call(object, 1, D32(BLOCK(object), 0x20));
    fs98_options_book_refresh(object);
    SERVICES->set_focus(AT(object, 0x1700));
}
/* 0x22398ed0 (0x115c): and with no rows left to delete, the focus moves to new logbook */
void TC fs98_options_book_delete_book(void *object)
{
    int32_t chosen = selection(BOOKS(object));
    if (chosen == -1) return;
    D32(BLOCK(object), 0x20) = (uint32_t)chosen;
    call(object, 2, D32(BLOCK(object), 0x20));
    fs98_options_book_refresh(object);
    if (SERVICES->window_enabled(AT(object, 0x17fc)) == 0)
        SERVICES->post_message(D32(object, 0x20), 0x5fa, D32(object, 0x1720), 0);
}
/* 0x22399400, the rows list's LBN_SELCHANGE */
void TC fs98_options_book_choose_row(void *object)
{
    int32_t row = selection(ROWS(object));
    if (row == -1) return;
    D32(BLOCK(object), 0x4c) = (uint32_t)row;
    call(object, 7, D32(BLOCK(object), 0x4c));
}
/* 0x22399600, the title edit's EN_KILLFOCUS: a changed title renames the logbook; a
 * failure MAIN reports is shown and the title selected for retyping, otherwise the list
 * is refilled */
void TC fs98_options_book_rename(void *object)
{
    char title[0x20];
    int32_t chosen;
    const char *book;
    SERVICES->get_window_text(AT(object, 0x1554), title, 0x1f);
    chosen = selection(BOOKS(object));
    if (chosen == -1) return;
    book = (const char *)(uintptr_t)(uint32_t)SERVICES->send_message(BOOKS(object), LB_GETITEMDATA, (uint32_t)chosen, 0);
    if (SERVICES->compare_text(title, book) == 0) return;
    D32(BLOCK(object), 0x20) = (uint32_t)chosen;
    SERVICES->copy_text(PTR(BLOCK(object), 0xc), title);
    if (call(object, 8, (uint32_t)chosen) != 0 && D32(BLOCK(object), 8) != 0) {
        D32(object, 0x8c) = (uint32_t)SERVICES->error_dialog(object, AT(object, 0x1554), D32(BLOCK(object), 8));
        if (D32(object, 0x8c) != 0) {
            SERVICES->set_focus(AT(object, 0x1554));
            SERVICES->post_message(D32(object, 0x1574), EM_SETSEL, 0, 0xffffffff);
            return;
        }
    }
    fs98_options_book_fill_books(object);
}
/* 0x22398550, OnInitDialog. The two static fonts are built once each and given the
 * dialog's own fonts' fields and their tab stops every time.
 *
 * RETAIL BUG (kept): OPT-6 (docs/retail-bugs.md). The rows list's width and a quarter
 * more is stored at +0x1af8 and at once overwritten with 0x299. */
int32_t TC fs98_options_book_init(void *object)
{
    struct fs98_options_rect rect;
    uint8_t *font;
    SERVICES->base_init(object);
    if (!((int32_t (TC *)(void *, int32_t, uint32_t, uint32_t))PTR(PTR(object, 0), 0xe4))(
            object, 0x14f, fs98_options_resource_module(), 0xf)) return 0;
    D32(object, 0x84) = 0xb;
    D32(object, 0x10e4) = 0xa3;
    D32(object, 0x10e0) = 0x3b;
    D32(object, 0x10e8) = 0x125;
    D32(object, 0x10ec) = 0x14a;
    D32(object, 0x10f0) = 0x172;
    D32(object, 0x10f4) = 0x199;
    font = s_state->total_font;
    if (!(s_state->guard & 1)) {
        s_state->guard |= 1;
        SERVICES->font_construct(font, SERVICES->base_font(1));
        SERVICES->register_exit(fs98_options_book_total_font_destroy);
    }
    D32(font, 8) = D32(object, 0x378);
    D32(font, 0xc) = D32(object, 0x37c);
    D32(font, 0x50) = 0;
    D32(font, 0x54) = 1;
    SERVICES->set_tab_stops(font, 4, (int32_t *)AT(object, 0x10e8));
    font = s_state->rows_font;
    if (!(s_state->guard & 2)) {
        s_state->guard |= 2;
        SERVICES->font_construct(font, SERVICES->base_font(3));
        SERVICES->register_exit(fs98_options_book_rows_font_destroy);
    }
    D32(font, 8) = D32(object, 0x458);
    D32(font, 0xc) = D32(object, 0x45c);
    D32(font, 0x14) = D32(object, 0x464);
    D32(font, 0x18) = D32(object, 0x468);
    D32(font, 0x1c) = D32(object, 0x46c);
    D32(font, 0x24) = D32(object, 0x474);
    D32(font, 0x4c) = 1;
    D32(font, 0x54) = 0;
    SERVICES->set_tab_stops(font, 6, (int32_t *)AT(object, 0x10e0));
    SERVICES->init_art(AT(object, 0x11fc), 0xf831, AT(object, 0xff4), AT(object, 0x7e8), 2);
    button(object, 0x1100, 0x5a0, 0x132);
    button(object, 0x19f4, 0x680, 0x137);
    button(object, 0x18f8, 0x680, 0x137);
    button(object, 0x17fc, 0x680, 0x137);
    button(object, 0x1700, 0x680, 0x137);
    button(object, 0x1604, 0x680, 0x137);
    SERVICES->init_text(AT(object, 0x12e4), AT(object, 0x370));
    SERVICES->init_list(AT(object, 0x15bc), AT(object, 0x3e0), 0);
    SERVICES->send_message(BOOKS(object), LB_SETCURSEL, 0, 0);
    SERVICES->init_edit(AT(object, 0x1554), SERVICES->base_font(2), 0xffffff, 0, 0xff0000, AT(object, 0x12e4), 0);
    SERVICES->init_text(AT(object, 0x1454), s_state->total_font);
    SERVICES->init_text(AT(object, 0x139c), AT(object, 0x370));
    SERVICES->init_list(AT(object, 0x150c), s_state->rows_font, AT(object, 0x139c));
    SERVICES->get_client_rect(ROWS(object), &rect);
    D32(object, 0x1af8) = (uint32_t)(rect.right + rect.right / 4);
    SERVICES->get_sims_text_buffer(AT(object, 0x139c), (char *)AT(object, 0x1b00), 0xff);
    {
        int32_t length = SERVICES->text_length((const char *)AT(object, 0x1b00));
        D32(object, 0x1af8) = 0x299;
        D32(object, 0x1afc) = (uint32_t)(length * 10 / 5);
    }
    PTR(object, 0x10f8) = fs98_options_new_notify();
    if (!fs98_options_book_has_descriptor(object)) return 0;
    SERVICES->send_message(D32(object, 0x1574), EM_LIMITTEXT, D32(BLOCK(object), 0x10) - 1, 0);
    fs98_options_book_refresh(object);
    return 1;
}
