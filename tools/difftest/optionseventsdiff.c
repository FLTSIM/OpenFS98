/* Options' dialog routines, the original PE32 code against C (src/options/).
 *
 *   optionseventsdiff.exe [scenarios] [seed] [normal|unwind]
 *
 * As tools/difftest/flightseventsdiff.c does for Flights: retail Options.DLL is mapped
 * without resolving its imports, and every import slot the tested routines use is pointed
 * at a logging stub. Each operation then runs twice from the same randomised fixture,
 * retail then ours, and the call logs, return values and every fixture byte must agree. A
 * stub may swap the descriptor, replace the vtable or alter a window handle mid-call, at a
 * position the scenario chooses. In unwind mode each operation is repeated with a
 * C++-style throw at every call position, comparing what the cleanup frames destroy.
 *
 * Covered: the helpers every class shares, Video finished (0x14040003), Video message
 * (0x14040002), Save video (0x14040001), Flight video (0x1404) and their CNotify, Add
 * logbook (0x14070001), Logbook entry (0x14070002) and the Logbook (0x1407) with its rows
 * list's class. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../../src/options/options_internal.h"
#include "../../src/options/options_objects.h"
#include "../../src/options/options_objects_tables.inc"

#define TC FS98_OPTIONS_THISCALL
#define SC FS98_OPTIONS_STDCALL
#define D32(object, offset) (*(uint32_t *)((uint8_t *)(object) + (offset)))
#define PTR(object, offset) (*(void **)((uint8_t *)(object) + (offset)))
#define TEST_THROW 0x0e041a22
/* a harness limit reached: say which, and where */
#define GIVE_UP() do { fprintf(stderr, "GIVE UP line %d scenario=%u op=%u side=%d\n", __LINE__, s_scenario, s_operation, s_side); exit(2); } while (0)
#define LOG_SIZE 32768
#define BASE 0x22390000
#define RESOURCE_MODULE 0x31415926
#define OBJECT_MODULE 0x27182818
#define RETAIL_RESOURCE_MODULE 0x1c344     /* .data: the skin's and buttons' instance */
#define RETAIL_OBJECT_MODULE 0x1c35c       /* .data: Options' own, stored at +0x80 */
#define RETAIL_HELP_FILE 0x1c168           /* "fs98.hlp" */
#define RETAIL_HOST 0x1c0ec                /* the HOST import link: its service table */
#define RETAIL_VIDEO 0x1c450               /* the name Flight video last saw */
#define RETAIL_NOTIFY_VTABLE 0x16ab0       /* Options' CNotify */
#define RETAIL_BOOK 0x1c368                /* the Logbook's static fonts and their guard */
#define RETAIL_ONEXIT 0x1c564              /* -1: atexit goes to _onexit */
#define RETAIL_LIST_VTABLE 0x17bb0         /* the Logbook's rows list's class */

struct fixture {
    uint8_t guard_before[32];
    uint8_t object[0x2400];
    uint8_t descriptor[2][0x200];   /* MAIN's logbook block reaches +0x154 */
    uint8_t notify[0x2a0];
    uint8_t record_before[0xa08];   /* record -1: with no selection the pick is -1 */
    uint8_t records[5][0xa08];
    char texts[4][0x110];
    struct fs98_options_video_state video;
    char strings[8][0x100];         /* what the CStrings hold */
    uint8_t books[5][0x124];        /* MAIN's logbooks */
    uint8_t rows[6][0x4f];          /* and a logbook's rows */
    char fields[7][0x50];           /* what a row opens with */
    struct fs98_options_book_state book;
    char empty[4];
    uint8_t info[32];
    uint8_t exchange[32];
    uint8_t font[32];
    uint8_t guard_after[32];
};
static struct fixture s_fixture, s_initial, s_retail_result;
static HMODULE s_retail;
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_script;
static uint32_t s_calls, s_log_length, s_throw_at, s_saved_seh;
static uint32_t s_max_calls, s_throws, s_comparisons, s_swaps, s_allocation_faults, s_no_descriptor;
static int s_side;
static char s_logs[2][LOG_SIZE];
static void *s_vtable[64], *s_other_vtable[64], *s_control_vtable[64], *s_jump[5];
static void *s_host_table[0x300 / 4];

enum signature {
    SIG_EVENT, SIG_HELP, SIG_CONTEXT, SIG_EXCHANGE, SIG_INT, SIG_EMPTY_ARG, SIG_INT_ARG, SIG_NOTIFY_DELETE,
    SIG_EXIT, SIG_SCROLL, SIG_LIST_DELETE,
    SIG_CONSTRUCT, SIG_DESTROY, SIG_DELETE, SIG_CREATE, SIG_ROUNDTRIP, SIG_CONSTRUCT_DELETE
};
struct operation { uint32_t rva; void *ours; enum signature signature; uint32_t kind; const char *name; };
#define SHARED(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_VIDEO_MESSAGE, #name}
#define VIDEO_FINISHED(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_VIDEO_FINISHED, #name}
#define VIDEO_MESSAGE(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_VIDEO_MESSAGE, #name}
#define SAVE_VIDEO(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_SAVE_VIDEO, #name}
#define FLIGHT_VIDEO(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_FLIGHT_VIDEO, #name}
#define ADD_LOGBOOK(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_ADD_LOGBOOK, #name}
#define LOGBOOK_ENTRY(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_LOGBOOK_ENTRY, #name}
#define BOOK(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_BOOK, #name}
#define CLASS(help, context, ddx, name, kind) \
    {help, (void *)fs98_options_##name##_help, SIG_HELP, kind, #name "_help"}, \
    {context, (void *)fs98_options_##name##_context, SIG_CONTEXT, kind, #name "_context"}, \
    {ddx, (void *)fs98_options_##name##_exchange, SIG_EXCHANGE, kind, #name "_exchange"}
#define LIFETIME(kind, name) \
    {0, (void *)fs98_options_##name##_construct, SIG_CONSTRUCT, kind, #name "_construct"}, \
    {0, (void *)fs98_options_##name##_destroy, SIG_DESTROY, kind, #name "_destroy"}, \
    {0, (void *)fs98_options_##name##_delete, SIG_DELETE, kind, #name "_delete"}, \
    {0, (void *)fs98_options_##name##_create, SIG_CREATE, kind, #name "_create"}, \
    {0, (void *)fs98_options_##name##_construct, SIG_ROUNDTRIP, kind, #name "_roundtrip"}, \
    {0, (void *)fs98_options_##name##_construct, SIG_CONSTRUCT_DELETE, kind, #name "_construct_delete"}
static const struct operation s_operations[] = {
    SHARED(0x2400, empty, SIG_EVENT), SHARED(0x2410, disable, SIG_EVENT), SHARED(0x2420, enable, SIG_EVENT),
    SHARED(0x43d0, empty_arg, SIG_EMPTY_ARG), SHARED(0x7c30, post_destroy, SIG_EVENT),
    SHARED(0x2f20, no_error, SIG_INT), SHARED(0x43c0, check, SIG_INT),
    CLASS(0xdff0, 0xe010, 0xdf00, video_finished, FS98_OPTIONS_VIDEO_FINISHED),
    VIDEO_FINISHED(0xdf40, video_finished_init, SIG_INT),
    VIDEO_FINISHED(0xdfe0, video_finished_accept, SIG_EVENT),
    LIFETIME(FS98_OPTIONS_VIDEO_FINISHED, video_finished),
    CLASS(0xe430, 0xe450, 0xe170, video_message, FS98_OPTIONS_VIDEO_MESSAGE),
    VIDEO_MESSAGE(0xe1d0, video_message_init, SIG_INT),
    VIDEO_MESSAGE(0xe300, video_message_accept, SIG_EVENT),
    VIDEO_MESSAGE(0xe3e0, video_message_cancel, SIG_EVENT),
    VIDEO_MESSAGE(0xe400, video_message_populate, SIG_INT),
    LIFETIME(FS98_OPTIONS_VIDEO_MESSAGE, video_message),
    SAVE_VIDEO(0x4290, notify_delete, SIG_NOTIFY_DELETE),
    SAVE_VIDEO(0x6d70, notify_close, SIG_EVENT),
    CLASS(0xdbd0, 0xdbf0, 0xd6f0, save_video, FS98_OPTIONS_SAVE_VIDEO),
    SAVE_VIDEO(0xd7c0, save_video_review, SIG_EVENT), SAVE_VIDEO(0xd800, save_video_resume, SIG_EVENT),
    SAVE_VIDEO(0xd840, save_video_save, SIG_EVENT), SAVE_VIDEO(0xd8b0, save_video_save, SIG_EVENT),
    SAVE_VIDEO(0xd8c0, save_video_cancel, SIG_EVENT), SAVE_VIDEO(0xd930, save_video_init, SIG_INT),
    SAVE_VIDEO(0xdc10, save_video_validate, SIG_INT_ARG), SAVE_VIDEO(0xdc80, save_video_error, SIG_INT_ARG),
    SAVE_VIDEO(0xddc0, save_video_fill, SIG_EVENT), SAVE_VIDEO(0xddf0, save_video_has_descriptor, SIG_INT),
    LIFETIME(FS98_OPTIONS_SAVE_VIDEO, save_video),
    CLASS(0xcf50, 0xcf70, 0xca60, flight_video, FS98_OPTIONS_FLIGHT_VIDEO),
    FLIGHT_VIDEO(0xcb70, flight_video_close, SIG_EVENT), FLIGHT_VIDEO(0xcba0, flight_video_cancel, SIG_EVENT),
    FLIGHT_VIDEO(0xcbd0, flight_video_accept, SIG_EVENT), FLIGHT_VIDEO(0xcc20, flight_video_init, SIG_INT),
    FLIGHT_VIDEO(0xcf90, flight_video_delete_entry, SIG_EVENT), FLIGHT_VIDEO(0xcff0, flight_video_play, SIG_EVENT),
    FLIGHT_VIDEO(0xd030, flight_video_record, SIG_EVENT), FLIGHT_VIDEO(0xd0a0, flight_video_select, SIG_EVENT),
    FLIGHT_VIDEO(0xd150, flight_video_double_click, SIG_EVENT),
    FLIGHT_VIDEO(0xd170, flight_video_validate, SIG_INT_ARG), FLIGHT_VIDEO(0xd370, flight_video_error, SIG_INT_ARG),
    FLIGHT_VIDEO(0xd400, flight_video_populate, SIG_EVENT), FLIGHT_VIDEO(0xd4d0, flight_video_has_descriptor, SIG_INT),
    FLIGHT_VIDEO(0xd4e0, flight_video_rate, SIG_EVENT),
    LIFETIME(FS98_OPTIONS_FLIGHT_VIDEO, flight_video),
    CLASS(0xab50, 0xab70, 0xa7d0, add_logbook, FS98_OPTIONS_ADD_LOGBOOK),
    ADD_LOGBOOK(0xa850, add_logbook_cancel, SIG_EVENT), ADD_LOGBOOK(0xa870, add_logbook_accept, SIG_EVENT),
    ADD_LOGBOOK(0xa8b0, add_logbook_init, SIG_INT), ADD_LOGBOOK(0xaa70, add_logbook_read, SIG_EVENT),
    ADD_LOGBOOK(0xaa90, add_logbook_clear, SIG_EVENT), ADD_LOGBOOK(0xaab0, add_logbook_error, SIG_INT),
    ADD_LOGBOOK(0xab20, add_logbook_has_descriptor, SIG_INT), ADD_LOGBOOK(0xab40, add_logbook_check, SIG_INT),
    LIFETIME(FS98_OPTIONS_ADD_LOGBOOK, add_logbook),
    CLASS(0x9d40, 0x9d60, 0x9ae0, logbook_entry, FS98_OPTIONS_LOGBOOK_ENTRY),
    LOGBOOK_ENTRY(0x9cd0, logbook_entry_close, SIG_EVENT), LOGBOOK_ENTRY(0x9ce0, logbook_entry_cancel, SIG_EVENT),
    LOGBOOK_ENTRY(0x9d00, logbook_entry_accept, SIG_EVENT), LOGBOOK_ENTRY(0x9d80, logbook_entry_init, SIG_INT),
    LOGBOOK_ENTRY(0xa150, logbook_entry_validate, SIG_INT_ARG), LOGBOOK_ENTRY(0xa300, logbook_entry_error, SIG_INT_ARG),
    LOGBOOK_ENTRY(0xa500, logbook_entry_fill, SIG_EVENT), LOGBOOK_ENTRY(0xa630, logbook_entry_has_descriptor, SIG_INT),
    LIFETIME(FS98_OPTIONS_LOGBOOK_ENTRY, logbook_entry),
    CLASS(0x8a50, 0x8a70, 0x83a0, book, FS98_OPTIONS_BOOK),
    BOOK(0x84d0, book_close, SIG_EVENT), BOOK(0x84f0, book_cancel, SIG_EVENT), BOOK(0x8510, book_accept, SIG_EVENT),
    BOOK(0x8550, book_init, SIG_INT), BOOK(0x8a90, book_new_row, SIG_EVENT), BOOK(0x8c50, book_edit_row, SIG_EVENT),
    BOOK(0x93f0, book_edit_row, SIG_EVENT), BOOK(0x8e50, book_delete_row, SIG_EVENT),
    BOOK(0x8ea0, book_new_book, SIG_EVENT), BOOK(0x8ed0, book_delete_book, SIG_EVENT),
    BOOK(0x8f40, book_show_book, SIG_EVENT), BOOK(0x9400, book_choose_row, SIG_EVENT),
    BOOK(0x9440, book_read, SIG_EVENT), BOOK(0x94a0, book_fill_books, SIG_EVENT),
    BOOK(0x9540, book_refresh, SIG_EVENT), BOOK(0x95e0, book_has_descriptor, SIG_INT),
    BOOK(0x95f0, book_check, SIG_INT), BOOK(0x9600, book_rename, SIG_EVENT),
    BOOK(0x96f0, book_scroll, SIG_SCROLL), BOOK(0x8380, book_list_delete, SIG_LIST_DELETE),
    {0x8a40, (void *)fs98_options_book_total_font_destroy, SIG_EXIT, FS98_OPTIONS_BOOK, "book_total_font_destroy"},
    {0x8a30, (void *)fs98_options_book_rows_font_destroy, SIG_EXIT, FS98_OPTIONS_BOOK, "book_rows_font_destroy"},
    LIFETIME(FS98_OPTIONS_BOOK, book),
};
#define OP_COUNT (sizeof(s_operations) / sizeof(s_operations[0]))
#define OP s_operations[s_operation]

static uint32_t next_random(void)
{ s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static uint32_t offset(const void *pointer)
{
    if (s_retail && (const uint8_t *)pointer >= (uint8_t *)s_retail + RETAIL_BOOK &&
        (const uint8_t *)pointer < (uint8_t *)s_retail + RETAIL_BOOK + sizeof(struct fs98_options_book_state))
        pointer = (const uint8_t *)&s_fixture.book + ((const uint8_t *)pointer - ((uint8_t *)s_retail + RETAIL_BOOK));
    if (s_retail && (const uint8_t *)pointer >= (uint8_t *)s_retail + RETAIL_VIDEO &&
        (const uint8_t *)pointer < (uint8_t *)s_retail + RETAIL_VIDEO + sizeof(struct fs98_options_video_state))
        pointer = (const uint8_t *)&s_fixture.video + ((const uint8_t *)pointer - ((uint8_t *)s_retail + RETAIL_VIDEO));
    return (uint32_t)((const uint8_t *)pointer - (const uint8_t *)&s_fixture);
}
/* where a pointer is: in the fixture, by its offset; a stack local, as such */
static uint32_t where(const void *pointer)
{
    uint32_t at = offset(pointer);
    return at < sizeof(s_fixture) ? at : 0xffffffff;
}
/* a string the stubs print: a fixture one by its offset and text, anything else by text */
static const char *shown(const char *text) { return text ? text : "(null)"; }
static void put(const char *format, ...)
{
    va_list args;
    int length;
    if (s_log_length > LOG_SIZE - 2048) GIVE_UP();
    va_start(args, format);
    length = vsnprintf(s_logs[s_side] + s_log_length, LOG_SIZE - s_log_length, format, args);
    va_end(args);
    if (length < 0 || (uint32_t)length >= LOG_SIZE - s_log_length) GIVE_UP();
    s_log_length += (uint32_t)length;
    ++s_calls;
    if (s_throw_at && s_calls == s_throw_at) RaiseException(TEST_THROW, 0, 0, 0);
}
/* At two call points per operation: swap the descriptor for the other, take it away,
 * replace the vtable, change the edit's window. */
static uint32_t descriptor_offset(void)
{
    uint32_t at = s_classes[OP.kind].descriptor;
    return at ? at : 0x10dc;
}
static void mutate(void)
{
    uint8_t *object = s_fixture.object;
    void *descriptor = PTR(object, descriptor_offset());
    /* one point among the first calls and one anywhere in the first 32, any script */
    if (s_calls != 1 + ((s_scenario ^ (s_seed >> 7)) % 4) && s_calls != 5 + ((s_seed >> 12) % 28)) return;
    if (OP.kind != FS98_OPTIONS_VIDEO_FINISHED && (s_script & 1) &&
        (descriptor == s_fixture.descriptor[0] || descriptor == s_fixture.descriptor[1])) {
        PTR(object, descriptor_offset()) = descriptor == s_fixture.descriptor[0] ? s_fixture.descriptor[1]
                                                                                 : s_fixture.descriptor[0];
        if (!s_side) ++s_swaps;
    }
    if (s_script & 2) {
        descriptor = PTR(object, descriptor_offset());
        if (descriptor == s_fixture.descriptor[0] || descriptor == s_fixture.descriptor[1]) {
            D32(descriptor, 0) ^= 0x10;
            if (OP.kind == FS98_OPTIONS_SAVE_VIDEO) D32(descriptor, 0x18) ^= 0x1004;
            if (OP.kind == FS98_OPTIONS_FLIGHT_VIDEO) {
                if (D32(descriptor, 0x28) < 4) D32(descriptor, 0x28) ^= 1;     /* another pick, never before the list */
                D32(descriptor, 0x2c) = (D32(descriptor, 0x2c) + 1) % 5;
            }
        }
        if (OP.kind == FS98_OPTIONS_FLIGHT_VIDEO) D32(object, 0x10e0) ^= 0x1004;
    }
    /* the logbook dialogs' part of the block follows the block */
    if (OP.kind == FS98_OPTIONS_ADD_LOGBOOK || OP.kind == FS98_OPTIONS_LOGBOOK_ENTRY) {
        uint32_t part = OP.kind == FS98_OPTIONS_ADD_LOGBOOK ? 0x10e0 : 0x10dc;
        uint32_t into = OP.kind == FS98_OPTIONS_ADD_LOGBOOK ? 0x28 : 0x54;
        uint8_t *now = PTR(object, part);
        if ((s_script & 1) && (now == s_fixture.descriptor[0] + into || now == s_fixture.descriptor[1] + into))
            PTR(object, part) = now == s_fixture.descriptor[0] + into ? s_fixture.descriptor[1] + into
                                                                    : s_fixture.descriptor[0] + into;
        if ((s_script & 2) && OP.kind == FS98_OPTIONS_LOGBOOK_ENTRY &&
            (now == s_fixture.descriptor[0] + into || now == s_fixture.descriptor[1] + into)) {
            D32(PTR(object, part), 0xa4) ^= 1;
            D32(PTR(object, part), 0x44) ^= 0x1003;
        }
    }
    if (OP.kind == FS98_OPTIONS_BOOK && (s_script & 2)) {
        uint8_t *block = PTR(object, 0x10fc);
        if (block == s_fixture.descriptor[0] || block == s_fixture.descriptor[1]) {
            D32(block, 0x48) = (D32(block, 0x48) + 1) % 5;
            D32(block, 0x1c) = (D32(block, 0x1c) + 3) % 5;
        }
    }
    if (s_script & 4) PTR(object, 0) = s_other_vtable;
    if (s_script & 8) D32(object, OP.kind == FS98_OPTIONS_BOOK ? 0x15dc : OP.kind == FS98_OPTIONS_SAVE_VIDEO ? 0x1460 :
                              OP.kind == FS98_OPTIONS_FLIGHT_VIDEO ? 0x1418 : 0x1100) = s_seed ^ 0x12345678;
}

/* ---- the object's own virtuals --------------------------------------------------------- */
static void TC virtual_close(void *object)
{
    put("virtual.close %u state=%u answers=%u/%u\n", offset(object), D32(object, 0x13f8),
        D32(s_fixture.descriptor[0], 0), D32(s_fixture.descriptor[1], 0));
    mutate();
}
static void TC virtual_close_other(void *object) { put("other.close\n"); virtual_close(object); }
static void *TC virtual_delete(void *object, uint32_t flags)
{ put("virtual.delete %u flags=%u\n", offset(object), flags); mutate(); return object; }
#define OTHER_SLOT(n) static void TC virtual_slot_##n(void *object) \
{ put("virtual.slot %u %u\n", n, offset(object)); mutate(); }
#define OTHER_SLOTS(n) OTHER_SLOT(n##0) OTHER_SLOT(n##1) OTHER_SLOT(n##2) OTHER_SLOT(n##3) \
                       OTHER_SLOT(n##4) OTHER_SLOT(n##5) OTHER_SLOT(n##6) OTHER_SLOT(n##7)
OTHER_SLOTS(1) OTHER_SLOTS(2) OTHER_SLOTS(3) OTHER_SLOTS(4) OTHER_SLOTS(5) OTHER_SLOTS(6) OTHER_SLOTS(7) OTHER_SLOTS(8)
#undef OTHER_SLOT
#define SLOT_ROW(n) virtual_slot_##n##0, virtual_slot_##n##1, virtual_slot_##n##2, virtual_slot_##n##3, \
                    virtual_slot_##n##4, virtual_slot_##n##5, virtual_slot_##n##6, virtual_slot_##n##7
static void (TC *const s_other_slots[64])(void *) = {
    SLOT_ROW(1), SLOT_ROW(2), SLOT_ROW(3), SLOT_ROW(4), SLOT_ROW(5), SLOT_ROW(6), SLOT_ROW(7), SLOT_ROW(8)
};
#undef SLOT_ROW
#define OTHER_SLOT(n) static void TC replaced_slot_##n(void *object) \
{ put("other.slot %u %u\n", n, offset(object)); mutate(); }
OTHER_SLOTS(1) OTHER_SLOTS(2) OTHER_SLOTS(3) OTHER_SLOTS(4) OTHER_SLOTS(5) OTHER_SLOTS(6) OTHER_SLOTS(7) OTHER_SLOTS(8)
#undef OTHER_SLOT
#define SLOT_ROW(n) replaced_slot_##n##0, replaced_slot_##n##1, replaced_slot_##n##2, replaced_slot_##n##3, \
                    replaced_slot_##n##4, replaced_slot_##n##5, replaced_slot_##n##6, replaced_slot_##n##7
static void (TC *const s_replaced_slots[64])(void *) = {
    SLOT_ROW(1), SLOT_ROW(2), SLOT_ROW(3), SLOT_ROW(4), SLOT_ROW(5), SLOT_ROW(6), SLOT_ROW(7), SLOT_ROW(8)
};
#undef SLOT_ROW
#undef OTHER_SLOTS
/* vtable +0xe4, the skin: fails one time in eight */
static int32_t TC virtual_skin(void *object, int32_t resource, uint32_t module, uint32_t flags);
static int32_t TC replaced_skin(void *object, int32_t resource, uint32_t module, uint32_t flags)
{ put("other.skin\n"); return virtual_skin(object, resource, module, flags); }
static int32_t TC virtual_skin(void *object, int32_t resource, uint32_t module, uint32_t flags)
{
    put("virtual.skin %u resource=%x module=%x flags=%x\n", offset(object), resource, module, flags);
    mutate();
    return (s_seed >> 20) % 8 ? (int32_t)(s_seed | 1) : 0;
}
static uint32_t TC virtual_validate(void *object, uint32_t argument)
{ put("virtual.validate %u %x\n", offset(object), argument); mutate(); return (s_seed >> 5) % 3 ? 0 : 0x11a3; }
static uint32_t TC virtual_error(void *object, uint32_t code)
{ put("virtual.error %u %x\n", offset(object), code); mutate(); return (s_seed >> 6) & 1; }
static uint32_t TC virtual_focus(void *object, uint32_t control)
{ put("virtual.focus %u %x\n", offset(object), control); mutate(); return s_seed ^ control; }
static int32_t TC virtual_check(void *control, void *font, uint32_t bitmap, void *dib, void *palette, uint32_t module)
{
    put("check.init %u font=%u bitmap=%x dib=%u palette=%u module=%x\n", offset(control), offset(font), bitmap,
        offset(dib), offset(palette), module);
    mutate(); return (int32_t)s_seed;
}
/* DEMO's delete and edit, given the descriptor (and the record): refused one time in three */
static uint32_t SC descriptor_delete(void *descriptor)
{ put("delete %u pick=%u\n", offset(descriptor), D32(descriptor, 0x28)); mutate(); return (s_seed >> 10) % 3 ? 0 : s_seed; }
static uint32_t SC descriptor_edit(void *descriptor, void *record)
{
    put("edit %u name=[%.40s] title=[%.40s] writable=%u\n", offset(descriptor), (char *)record,
        (char *)record + 0x100, D32(record, 0xa04));
    mutate();
    return (s_seed >> 11) % 3 == 0 ? 0x1004 : (s_seed >> 11) % 3 == 1 ? 0 : 0x100d;
}
/* MAIN's lb_button: an operation, an index, the block */
static uint32_t SC book_button(uint32_t operation, uint32_t index, void *block)
{
    put("button %u %d block=%u\n", operation, (int32_t)index, offset(block));
    mutate();
    return (s_seed >> (operation + 3)) % 3 == 0 ? 0xffffffff : 0;
}
/* MAIN's logbook checks, given their part */
static uint32_t SC part_check(void *part)
{ put("part.check %u\n", offset(part)); mutate(); return (s_seed >> 9) % 3 == 0 ? s_seed : 0; }
/* DEMO's check, given the descriptor: nonzero (a failure) one time in three */
static uint32_t SC descriptor_check(void *descriptor)
{
    put("check %u text=[%s]\n", offset(descriptor), (char *)descriptor + 8);
    mutate();
    return (s_seed >> 9) % 3 == 0 ? s_seed : 0;
}

/* ---- the services ---------------------------------------------------------------------- */
static int32_t send_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{
    if (message == 0x148) {     /* CB_GETLBTEXT: the aircraft's name, long enough to be cut */
        put("message %u %x %d\n", window, message, (int32_t)w_param); mutate();
        snprintf((char *)(uintptr_t)l_param, 0x40, "Aircraft %d of the logbook list", (int32_t)w_param);
        return 20;
    }
    if (message == 0x143) { put("message %u %x %u record=%u\n", window, message, w_param, offset((void *)(uintptr_t)l_param)); mutate(); return (int32_t)s_seed; }
    if (message == 0x180 || message == 0x19a) put("message %u %x %u record=%u\n", window, message, w_param, offset((void *)(uintptr_t)l_param));
    else put("message %u %x %u %u\n", window, message, w_param, l_param);
    mutate();
    if (message == 0x188)   /* LB_GETCURSEL: none (for a whole scenario, or this call alone), or one of the five */
        return s_scenario % 7 == 0 || (s_seed >> (s_calls % 16)) % 9 == 0 ? -1 : (int32_t)((s_scenario + s_calls) % 5);
    if (message == 0x199)   /* LB_GETITEMDATA: the record the item was tagged with */
        return OP.kind == FS98_OPTIONS_BOOK ? (int32_t)(uintptr_t)s_fixture.books[w_param % 5]
                                            : (int32_t)(uintptr_t)s_fixture.records[w_param % 5];
    if (message == 0x147)   /* CB_GETCURSEL */
        return (int32_t)((s_seed >> (s_calls % 8)) % 5) - 1;
    if (message == 0x180)   /* LB_ADDSTRING: its index, or an error */
        return (s_seed >> (s_calls % 16)) % 8 == 0 ? -1 : (int32_t)(s_calls % 9);
    return (int32_t)s_seed;
}
static int32_t win_help(uint32_t window, const char *file, uint32_t command, const uint32_t *table)
{
    uint32_t index = 0;
    char pairs[1024];
    int length = 0;
    do {
        if (index >= 128 || length > 900) GIVE_UP();
        length += snprintf(pairs + length, sizeof(pairs) - length, "%x:%x,", table[index], table[index + 1]);
        index += 2;
    } while (table[index - 2] || table[index - 1]);
    put("help %u %s %u [%s]\n", window, file, command, pairs);
    mutate(); return (int32_t)s_seed;
}
static int32_t enable_window(uint32_t window, int32_t enable)
{ put("enable.window %u %d\n", window, enable); mutate(); return (int32_t)s_seed; }
static void ddx_control(void *exchange, uint32_t id, void *control)
{ put("ddx %u %x %u\n", offset(exchange), id, offset(control)); mutate(); }
static int32_t base_init(void *object) { put("base.init %u\n", offset(object)); mutate(); return (int32_t)s_seed; }
static int32_t init_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                           void *dib, void *palette, uint32_t flags, uint32_t color,
                           uint32_t module, int32_t x, int32_t y, struct fs98_options_rect rect)
{
    put("init.button %u font=%u alt=%u bitmap=%x dib=%u palette=%u flags=%x color=%x module=%x %d,%d rect=%d,%d,%d,%d\n",
        offset(control), offset(font), offset(alternate_font), bitmap, offset(dib), offset(palette), flags, color,
        module, x, y, rect.left, rect.top, rect.right, rect.bottom);
    mutate(); return (int32_t)s_seed;
}
static void init_text(void *control, void *font)
{ put("init.text %u font=%u\n", offset(control), offset(font)); mutate(); }
static void init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                      uint32_t error_color, void *label, uint32_t flags)
{
    put("init.edit %u font=%u %x %x %x label=%u flags=%x\n", offset(control), offset(font), foreground,
        background, error_color, offset(label), flags);
    mutate();
}
static void *base_font(int32_t id) { put("base.font %d\n", id); mutate(); return s_fixture.font; }
static void set_window_text(void *window, const char *text)
{ put("set.text %u [%s]\n", offset(window), text); mutate(); }
/* the edit's text: empty, short, up to the room, or one that ends in a lead byte */
static int32_t get_window_text(void *window, char *buffer, int32_t count)
{
    static const char *const texts[] = {
        "", "Approach", "Turning base at five hundred feet with the gear down and three greens",
        "Lead \x81", "\x82", "Fifty-three characters exactly, give or take, one more!"
    };
    const char *text = texts[(s_seed >> (s_calls % 8)) % 6];
    int32_t length = 0;
    put("get.text %u count=%d\n", offset(window), count);
    mutate();
    if (count <= 0) return 0;
    while (text[length] && length < count - 1) { buffer[length] = text[length]; ++length; }
    buffer[length] = 0;
    return length;
}
static void *set_focus(void *window) { put("set.focus %u\n", offset(window)); mutate(); return s_fixture.info; }
/* a lead byte: the high half, as the code page says, never 0 */
static int32_t is_lead_byte(int32_t character)
{
    put("isleadbyte %d\n", character);
    mutate();
    return (character & 0x80) && ((s_seed >> 3) & 1) ? 0x8000 : 0;
}
static void base_exchange(void *object, void *exchange)
{ put("base.exchange %u %u\n", offset(object), offset(exchange)); mutate(); }
static int32_t init_art(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{
    put("init.art %u bitmap=%x dib=%u palette=%u flags=%x\n", offset(control), bitmap, offset(dib), offset(palette), flags);
    mutate(); return (int32_t)s_seed;
}
static void init_list(void *control, void *font, void *label)
{ put("init.list %u font=%u label=%u\n", offset(control), offset(font), offset(label)); mutate(); }
static void init_combo(void *control, void *font, void *label)
{ put("init.combo %u font=%u label=%u\n", offset(control), offset(font), offset(label)); mutate(); }
static int32_t error_dialog(void *object, void *control, uint32_t code)
{
    put("error.dialog %u control=%u code=%x style=%x/%x\n", offset(object), offset(control), code,
        D32(object, 0x90), D32(object, 0x94));
    mutate(); return (s_seed >> (s_calls % 16)) & 1 ? (int32_t)s_seed : 0;
}
static uint32_t get_focus(void) { put("get.focus\n"); mutate(); return s_seed ^ 0xf0c5; }
/* the focus on review, on go on, or elsewhere: each call its own seed bits */
static uint32_t s_from_handle_calls, s_string_count;
static void *window_from_handle(uint32_t window)
{
    static const uint32_t at[] = {0x1510, 0x160c, 0x1440, 0x1440};
    uint32_t call = s_from_handle_calls++;
    put("from.handle %x\n", window); mutate();
    return s_fixture.object + at[(s_seed >> (14 + 2 * (call % 4))) % 4];
}
static int32_t compare_text(const char *a, const char *b)
{ put("compare [%s] [%s]\n", shown(a), shown(b)); mutate(); return strcmp(a, b); }
static char *copy_text(char *out, const char *text)
{ put("copy %u [%s]\n", offset(out), shown(text)); mutate(); strcpy(out, text); return out; }
static int32_t post_message(uint32_t window, uint32_t message, uint32_t w_param, uint32_t l_param)
{ put("post %u %x %u %u\n", window, message, w_param, l_param); mutate(); return (int32_t)s_seed; }
static int32_t enable_control(void *window, int32_t enable)
{ put("enable.control %u %d\n", offset(window), enable); mutate(); return (int32_t)s_seed; }
static int32_t window_enabled(void *window)
{ put("window.enabled %u\n", offset(window)); mutate(); return (s_seed >> 15) & 1; }
static int32_t set_checked(void *control, int32_t checked)
{ put("set.checked %u %d\n", offset(control), checked); mutate(); return (int32_t)s_seed; }
static char *host_strcpy(char *out, const char *text)
{ put("host.strcpy %u [%s]\n", offset(out), shown(text)); mutate(); strcpy(out, text); return out; }
static int32_t host_strcmpi(const char *a, const char *b)
{ put("host.strcmpi %u [%s] [%s]\n", offset(a), shown(a), shown(b)); mutate(); return _stricmp(a, b); }
static void *allocate_object(uint32_t size);
static void notify_construct(void *notify, uint32_t a, uint32_t b)
{ put("notify.construct %u %x %x\n", offset(notify), a, b); D32(notify, 0) = 0x2071f700; mutate(); }
static void notify_free(void *notify) { put("notify.free %u\n", offset(notify)); mutate(); }
static void notify_destroy(void *notify)
{ put("notify.destroy %u vtable=%x\n", offset(notify), D32(notify, 0)); D32(notify, 0) = 0xdead2222; mutate(); }
static void ui_delete(void *object);
static void close_dialog(void *object) { put("base.close %u notify=%x\n", offset(object), D32(object, 0x10dc)); mutate(); }
static void string_construct(void *string)
{
    put("string.construct %u\n", where(string));
    ((struct fs98_options_string *)string)->text = s_fixture.empty;
    mutate();
}
static void string_destroy(void *string)
{
    put("string.destroy %u text=%u [%s]\n", where(string), offset(((struct fs98_options_string *)string)->text),
        ((struct fs98_options_string *)string)->text);
    mutate();
}
/* the text into a fixture slot of its own */
static void string_set(void *string, const char *text)
{
    char *slot = s_fixture.strings[s_string_count++ % 8];
    snprintf(slot, sizeof(s_fixture.strings[0]), "%s", text);
    ((struct fs98_options_string *)string)->text = slot;
}
static void string_assign(void *string, const char *text)
{ put("string.assign %u [%s]\n", where(string), shown(text)); string_set(string, text); mutate(); }
static void ddx_text(void *exchange, uint32_t id, void *string)
{
    put("ddx.text %u %x %u [%s]\n", offset(exchange), id, where(string), ((struct fs98_options_string *)string)->text);
    mutate();
}
static void ddv_max_chars(void *exchange, void *string, int32_t count)
{ put("ddv.max %u %u %d\n", offset(exchange), where(string), count); mutate(); }
static int32_t show_dialog(void *object, int32_t show);
static int32_t load_string_a(uint32_t instance, uint32_t id, char *buffer, int32_t size)
{
    put("load.string %x %x size=%d\n", instance, id, size); mutate();
    return snprintf(buffer, (size_t)size, "Title %x", id);
}
static void init_group(void *control, void *font)
{ put("init.group %u font=%u\n", offset(control), offset(font)); mutate(); }
static void set_sims_text(void *control, const char *text)
{ put("sims.text %u [%s]\n", offset(control), shown(text)); mutate(); }
static void get_sims_text(void *control, void *string)
{
    char text[32];
    put("sims.get %u into %u\n", offset(control), where(string));
    snprintf(text, sizeof(text), "Label %x", s_seed & 0xfff);
    string_set(string, text); mutate();
}
static void window_text_string(void *window, void *string)
{
    char text[32];
    put("get.string %u into %u\n", offset(window), where(string));
    snprintf(text, sizeof(text), "%u.%u", (s_seed >> 4) % 100, s_seed % 10);
    string_set(string, text); mutate();
}
static uint8_t s_guard_seen(void)
{ return s_side ? s_fixture.book.guard : *((uint8_t *)s_retail + RETAIL_BOOK + 0xe0); }
static void font_construct(void *font, void *base_font)
{
    put("font.construct %u base=%u guard=%u\n", offset(font), offset(base_font), s_guard_seen());
    memset(font, s_seed & 255, 0x70); mutate();
}
static void font_destroy(void *font)
{ put("font.destroy %u value=%x/%x\n", offset(font), D32(font, 8), D32(font, 0x50)); mutate(); }
static void register_exit(void (*handler)(void))
{
    uint32_t which = handler == (s_side ? fs98_options_book_total_font_destroy : (void (*)(void))((uint8_t *)s_retail + 0x8a40)) ? 1 :
                     handler == (s_side ? fs98_options_book_rows_font_destroy : (void (*)(void))((uint8_t *)s_retail + 0x8a30)) ? 2 : 0;
    put("register.exit which=%u\n", which); mutate();
}
static int32_t set_tab_stops(void *font, int32_t count, int32_t *stops)
{
    put("tab.stops %u %d [%d %d %d %d] at=%u\n", offset(font), count, stops[0], stops[1], stops[2], stops[3], offset(stops));
    mutate(); return (int32_t)s_seed;
}
static int32_t get_client_rect(uint32_t window, struct fs98_options_rect *rect)
{
    put("client.rect %u\n", window); mutate();
    rect->left = 0; rect->top = 0; rect->right = (int32_t)(s_seed % 700) - 50; rect->bottom = 120;
    return 1;
}
static int32_t get_sims_text_buffer(void *control, char *buffer, int32_t count)
{
    put("sims.get.text %u count=%d into %u\n", offset(control), count, offset(buffer)); mutate();
    return snprintf(buffer, (size_t)count, "Rows %x", s_seed & 0xffff);
}
static int32_t text_length(const char *text)
{ put("lstrlen [%s]\n", shown(text)); mutate(); return (int32_t)strlen(text); }
static void string_append(void *string, const char *text)
{
    char joined[0x200];
    put("string.append %u [%s]\n", where(string), shown(text));
    snprintf(joined, sizeof(joined), "%s%s", ((struct fs98_options_string *)string)->text, text);
    string_set(string, joined); mutate();
}
static void string_append_char(void *string, char character)
{
    char joined[0x200];
    put("string.append.char %u %d\n", where(string), character);
    snprintf(joined, sizeof(joined), "%s%c", ((struct fs98_options_string *)string)->text, character);
    string_set(string, joined); mutate();
}
static void list_text(void *list, int32_t index, void *string)
{
    char text[32];
    put("list.text %u %d into %u\n", offset(list), index, where(string));
    snprintf(text, sizeof(text), "Logbook %d", index);
    string_set(string, text); mutate();
}
static int32_t format_text(char *out, const char *format, const char *text)
{ put("sprintf [%s] [%s]\n", format, shown(text)); mutate(); return sprintf(out, format, text); }
static void destroy_control(void *control, uint32_t import);
static void list_destroy(void *list) { destroy_control(list, 0x1d6ac); }
static const struct fs98_options_services s_services = {
    send_message, win_help, enable_window, ddx_control, base_init, init_button, init_text,
    init_edit, base_font, set_window_text, get_window_text, set_focus, is_lead_byte,
    base_exchange, init_art, init_list, init_combo, error_dialog, get_focus, window_from_handle,
    compare_text, copy_text, post_message, enable_control, window_enabled, set_checked,
    host_strcpy, host_strcmpi, allocate_object, notify_construct, notify_free, notify_destroy,
    ui_delete, close_dialog, string_construct, string_destroy, string_assign, ddx_text, ddv_max_chars,
    show_dialog, load_string_a, init_group, set_sims_text, get_sims_text, window_text_string,
    font_construct, font_destroy, register_exit, set_tab_stops, get_client_rect, get_sims_text_buffer,
    text_length, string_append, string_append_char, list_text, format_text, list_destroy
};

static void *dialog_construct(void *object, int32_t resource, void *parent)
{
    put("dialog.construct %u resource=%x parent=%u\n", offset(object), resource, (uint32_t)(uintptr_t)parent);
    D32(object, 0) = 0xba5e0000; D32(object, 0x20) = s_seed;
    mutate(); return object;
}
static void dialog_destroy(void *object)
{ put("dialog.destroy %u vtable=%x\n", offset(object), D32(object, 0)); D32(object, 0) = 0xdead0000; mutate(); }
static void *page_construct(void *object, int32_t resource, void *parent)
{
    put("page.construct %u resource=%x parent=%u\n", offset(object), resource, (uint32_t)(uintptr_t)parent);
    D32(object, 0) = 0xba5e1111; mutate(); return object;
}
static void page_destroy(void *object)
{ put("page.destroy %u vtable=%x\n", offset(object), D32(object, 0)); D32(object, 0) = 0xdead1111; mutate(); }
static void construct_control(void *control, uint32_t import)
{
    if (import == 0x1d530) { string_construct(control); return; }    /* a CString member */
    put("control.construct %u import=%x\n", offset(control), import);
    D32(control, 0) = import; D32(control, 4) = s_seed;
    mutate();
}
static void destroy_control(void *control, uint32_t import)
{
    if (import == 0x1d53c) { string_destroy(control); return; }
    put("control.destroy %u import=%x vtable=%x\n", offset(control), import, D32(control, 0));
    D32(control, 0) = import; mutate();
}
static void *allocate_object(uint32_t size)
{
    put("allocate %x\n", size); mutate();
    if (size == 0x29c) return (s_seed >> 21) % 9 ? s_fixture.notify : 0;   /* the CNotify */
    return s_scenario % 11 ? s_fixture.object : 0;
}
static void ui_delete(void *object) { put("ui.delete %u\n", offset(object)); mutate(); }
static void dialog_delete(void *object) { put("dialog.delete %u\n", offset(object)); mutate(); }
static int32_t create_dialog(void *object, uint32_t resource, void *parent)
{
    put("dialog.create %u resource=%x parent=%u module=%x descriptor=%u\n", offset(object), resource,
        (uint32_t)(uintptr_t)parent, D32(object, 0x80), offset(PTR(object, 0x10dc)));
    mutate(); return s_scenario & 1 ? 0 : (int32_t)s_seed;
}
static int32_t show_dialog(void *object, int32_t show)
{ put("dialog.show %u %d\n", offset(object), show); mutate(); return s_scenario & 2 ? 0 : (int32_t)s_seed; }
static const struct fs98_options_object_services s_object_services = {
    dialog_construct, dialog_destroy, construct_control, destroy_control, allocate_object,
    ui_delete, dialog_delete, create_dialog, show_dialog, page_construct, page_destroy, copy_text,
    string_assign
};

/* ---- the same stubs behind retail's import ABIs ----------------------------------------- */
#define CONTROL_HOOK(name, ctor, dtor) \
static void *TC hook_construct_##name(void *control) { construct_control(control, ctor); return control; } \
static void TC hook_destroy_##name(void *control) { destroy_control(control, dtor); }
CONTROL_HOOK(text, 0x1d78c, 0x1d79c)
CONTROL_HOOK(button, 0x1d760, 0x1d758)
CONTROL_HOOK(edit, 0x1d6c8, 0x1d6d0)
CONTROL_HOOK(art, 0x1d724, 0x1d734)
CONTROL_HOOK(list, 0x1d6a8, 0x1d6ac)
CONTROL_HOOK(combo, 0x1d75c, 0x1d764)
CONTROL_HOOK(check, 0x1d790, 0x1d7a0)
CONTROL_HOOK(group, 0x1d748, 0x1d744)
CONTROL_HOOK(string, 0x1d530, 0x1d53c)
#undef CONTROL_HOOK
static void *TC hook_dialog_construct(void *object, int32_t resource, void *parent)
{ return dialog_construct(object, resource, parent); }
static void TC hook_dialog_destroy(void *object) { dialog_destroy(object); }
static void SC hook_dialog_delete(void *object) { dialog_delete(object); }
static void *hook_allocate(uint32_t size) { return allocate_object(size); }
static void hook_ui_delete(void *object) { ui_delete(object); }
static int32_t TC hook_create_dialog(void *object, uint32_t resource, void *parent)
{ return create_dialog(object, resource, parent); }
static int32_t TC hook_show_dialog(void *object, int32_t show) { return show_dialog(object, show); }
static int32_t SC hook_send(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return send_message(a, b, c, d); }
static int32_t SC hook_help(uint32_t a, const char *b, uint32_t c, const uint32_t *d) { return win_help(a, b, c, d); }
static int32_t SC hook_enable(uint32_t a, int32_t b) { return enable_window(a, b); }
static void SC hook_ddx(void *a, uint32_t b, void *c) { ddx_control(a, b, c); }
static int32_t TC hook_base_init(void *object) { return base_init(object); }
static int32_t TC hook_button(void *control, void *font, void *alternate_font, uint32_t bitmap,
                              void *dib, void *palette, uint32_t flags, uint32_t color,
                              uint32_t module, int32_t x, int32_t y, struct fs98_options_rect rect)
{ return init_button(control, font, alternate_font, bitmap, dib, palette, flags, color, module, x, y, rect); }
static void TC hook_init_text(void *control, void *font) { init_text(control, font); }
static void TC hook_init_edit(void *control, void *font, uint32_t foreground, uint32_t background,
                              uint32_t error_color, void *label, uint32_t flags)
{ init_edit(control, font, foreground, background, error_color, label, flags); }
static void *SC hook_base_font(int32_t id) { return base_font(id); }
static void TC hook_set_text(void *window, const char *text) { set_window_text(window, text); }
static int32_t TC hook_get_text(void *window, char *buffer, int32_t count) { return get_window_text(window, buffer, count); }
static void *TC hook_set_focus(void *window) { return set_focus(window); }
static int hook_is_lead_byte(int character) { return is_lead_byte(character); }
static void TC hook_exchange(void *a, void *b) { base_exchange(a, b); }
static int32_t TC hook_art(void *control, uint32_t bitmap, void *dib, void *palette, uint32_t flags)
{ return init_art(control, bitmap, dib, palette, flags); }
static void TC hook_init_list(void *control, void *font, void *label) { init_list(control, font, label); }
static void TC hook_init_combo(void *control, void *font, void *label) { init_combo(control, font, label); }
static int32_t TC hook_error_dialog(void *object, void *control, uint32_t code) { return error_dialog(object, control, code); }
static uint32_t SC hook_get_focus(void) { return get_focus(); }
static void *SC hook_from_handle(uint32_t window) { return window_from_handle(window); }
static int32_t SC hook_compare(const char *a, const char *b) { return compare_text(a, b); }
static char *SC hook_copy(char *out, const char *text) { return copy_text(out, text); }
static int32_t SC hook_post(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return post_message(a, b, c, d); }
static int32_t TC hook_enable_control(void *window, int32_t enable) { return enable_control(window, enable); }
static int32_t TC hook_window_enabled(void *window) { return window_enabled(window); }
static int32_t TC hook_set_checked(void *control, int32_t checked) { return set_checked(control, checked); }
static char *SC hook_host_strcpy(char *out, const char *text) { return host_strcpy(out, text); }
static int32_t SC hook_host_strcmpi(const char *a, const char *b) { return host_strcmpi(a, b); }
static void TC hook_notify_construct(void *notify, uint32_t a, uint32_t b) { notify_construct(notify, a, b); }
static void SC hook_notify_free(void *notify) { notify_free(notify); }
static void TC hook_notify_destroy(void *notify) { notify_destroy(notify); }
static void TC hook_close(void *object) { close_dialog(object); }
static void *TC hook_string_assign(void *string, const char *text) { string_assign(string, text); return string; }
static void SC hook_ddx_text(void *exchange, uint32_t id, void *string) { ddx_text(exchange, id, string); }
static void SC hook_ddv_max(void *exchange, void *string, int32_t count) { ddv_max_chars(exchange, string, count); }
static int32_t SC hook_load_string_a(uint32_t instance, uint32_t id, char *buffer, int32_t size)
{ return load_string_a(instance, id, buffer, size); }
static void TC hook_init_group(void *control, void *font) { init_group(control, font); }
static void TC hook_set_sims_text(void *control, const char *text) { set_sims_text(control, text); }
static void TC hook_get_sims_text(void *control, void *string) { get_sims_text(control, string); }
static void TC hook_window_text_string(void *window, void *string) { window_text_string(window, string); }
static void *TC hook_font_construct(void *font, void *base_font) { font_construct(font, base_font); return font; }
static void TC hook_font_destroy(void *font) { font_destroy(font); }
static void *hook_onexit(void (*handler)(void)) { register_exit(handler); return (void *)handler; }
static void *hook_dll_onexit(void (*handler)(void), void *begin, void *end)
{ (void)begin; (void)end; register_exit(handler); return (void *)handler; }
static int32_t TC hook_tab_stops(void *font, int32_t count, int32_t *stops) { return set_tab_stops(font, count, stops); }
static int32_t SC hook_client_rect(uint32_t window, struct fs98_options_rect *rect) { return get_client_rect(window, rect); }
static int32_t TC hook_sims_get_text(void *control, char *buffer, int32_t count) { return get_sims_text_buffer(control, buffer, count); }
static int32_t SC hook_lstrlen(const char *text) { return text_length(text); }
static void *TC hook_string_append(void *string, const char *text) { string_append(string, text); return string; }
static void *TC hook_string_append_char(void *string, char character) { string_append_char(string, character); return string; }
static void TC hook_list_text(void *list, int32_t index, void *string) { list_text(list, index, string); }
static int hook_sprintf(char *out, const char *format, ...)
{
    va_list args;
    const char *text;
    va_start(args, format);
    text = va_arg(args, const char *);
    va_end(args);
    return format_text(out, format, text);
}

static void patch(uint32_t rva, void *target)
{
    DWORD old;
    void **slot = (void **)((uint8_t *)s_retail + rva);
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &old)) GIVE_UP();
    *slot = target;
    VirtualProtect(slot, 4, old, &old);
}
static void setup_imports(void)
{
    void *vtables[FS98_OPTIONS_CLASS_COUNT];
    uint32_t index;
    HMODULE crt = LoadLibraryA("msvcrt.dll");
    void *handler = (void *)GetProcAddress(crt, "__CxxFrameHandler");
    if (!handler) GIVE_UP();
    patch(0x1d644, handler);
    patch(0x1d834, hook_send); patch(0x1d810, hook_help); patch(0x1d838, hook_enable);
    patch(0x1d56c, hook_ddx); patch(0x1d538, hook_set_text); patch(0x1d4c0, hook_get_text);
    patch(0x1d488, hook_set_focus); patch(0x1d634, hook_is_lead_byte);
    patch(0x1d4a0, hook_create_dialog); patch(0x1d49c, hook_show_dialog);
    patch(0x1d6e8, hook_base_init); patch(0x1d750, hook_button); patch(0x1d774, hook_init_text);
    patch(0x1d6bc, hook_init_edit); patch(0x1d6f4, hook_base_font);
    patch(0x1d7d4, hook_allocate); patch(0x1d784, hook_ui_delete); patch(0x1d7d8, hook_dialog_delete);
    patch(0x1d71c, hook_dialog_construct); patch(0x1d72c, hook_dialog_destroy);
#define PATCH_CONTROL(name, ctor, dtor) patch(ctor, hook_construct_##name); patch(dtor, hook_destroy_##name)
    PATCH_CONTROL(text, 0x1d78c, 0x1d79c); PATCH_CONTROL(button, 0x1d760, 0x1d758);
    PATCH_CONTROL(edit, 0x1d6c8, 0x1d6d0);
    PATCH_CONTROL(art, 0x1d724, 0x1d734); PATCH_CONTROL(list, 0x1d6a8, 0x1d6ac);
    PATCH_CONTROL(combo, 0x1d75c, 0x1d764); PATCH_CONTROL(check, 0x1d790, 0x1d7a0);
#define PATCH_STRING() do { PATCH_CONTROL(group, 0x1d748, 0x1d744); PATCH_CONTROL(string, 0x1d530, 0x1d53c); } while (0)
    patch(0x1d718, hook_exchange); patch(0x1d6ec, hook_art); patch(0x1d6a0, hook_init_list);
    patch(0x1d754, hook_init_combo); patch(0x1d6d4, hook_error_dialog); patch(0x1d814, hook_get_focus);
    patch(0x1d4bc, hook_from_handle); patch(0x1d460, hook_compare); patch(0x1d464, hook_copy);
    patch(0x1d824, hook_post); patch(0x1d568, hook_enable_control); patch(0x1d4cc, hook_window_enabled);
    patch(0x1d76c, hook_set_checked); patch(0x1d6f0, hook_notify_construct); patch(0x1d64c, hook_notify_free);
    patch(0x1d6e4, hook_notify_destroy); patch(0x1d7d0, hook_close);
    PATCH_STRING();
#undef PATCH_STRING
#undef PATCH_CONTROL
    patch(0x1d4f8, hook_string_assign); patch(0x1d4d0, hook_ddx_text); patch(0x1d4d4, hook_ddv_max);
    patch(0x1d82c, hook_load_string_a); patch(0x1d740, hook_init_group); patch(0x1d6b8, hook_set_sims_text);
    patch(0x1d68c, hook_get_sims_text); patch(0x1d534, hook_window_text_string);
    patch(0x1d698, hook_font_construct); patch(0x1d694, hook_font_destroy); patch(0x1d63c, hook_onexit);
    patch(0x1d624, hook_dll_onexit); patch(RETAIL_ONEXIT, (void *)0xffffffff);
    patch(0x1d69c, hook_tab_stops); patch(0x1d818, hook_client_rect); patch(0x1d6a4, hook_sims_get_text);
    patch(0x1d468, hook_lstrlen); patch(0x1d4fc, hook_string_append); patch(0x1d4c8, hook_string_append_char);
    patch(0x1d4c4, hook_list_text); patch(0x1d620, hook_sprintf);
    fs98_options_set_book_state(&s_fixture.book);
    fs98_options_set_book_list_vtable((uint8_t *)s_retail + RETAIL_LIST_VTABLE);
    fs98_options_set_object_list_vtable((uint8_t *)s_retail + RETAIL_LIST_VTABLE);
    s_host_table[0x2a8 / 4] = (void *)hook_host_strcpy; s_host_table[0x2c0 / 4] = (void *)hook_host_strcmpi;
    patch(RETAIL_HOST, s_host_table);
    fs98_options_set_notify_vtable((uint8_t *)s_retail + RETAIL_NOTIFY_VTABLE);
    fs98_options_set_video_state(&s_fixture.video);
    patch(RETAIL_RESOURCE_MODULE, (void *)RESOURCE_MODULE); fs98_options_set_resource_module(RESOURCE_MODULE);
    patch(RETAIL_OBJECT_MODULE, (void *)OBJECT_MODULE); fs98_options_set_object_module(OBJECT_MODULE);
    for (index = 0; index < FS98_OPTIONS_CLASS_COUNT; ++index)
        vtables[index] = (uint8_t *)s_retail + s_classes[index].vtable;
    fs98_options_set_object_services(&s_object_services, vtables);
    fs98_options_set_services(&s_services);
}

static void initialize(void)
{
    uint32_t index;
    uint8_t *object = s_fixture.object;
    for (index = 0; index < sizeof(s_fixture); ++index) ((uint8_t *)&s_fixture)[index] = (uint8_t)next_random();
    PTR(object, 0) = s_vtable;
    D32(object, 0x20) = s_seed;
    D32(object, 0x1100) = s_seed ^ 0x1100;
    PTR(object, descriptor_offset()) = s_fixture.descriptor[0];
    for (index = 0; index < 2; ++index) {
        uint8_t *descriptor = s_fixture.descriptor[index];
        D32(descriptor, 0) = (s_seed >> (4 + index)) & 1;
        PTR(descriptor, 4) = descriptor_check;
        snprintf((char *)descriptor + 8, 0x38, "Before %u", index);
    }
    for (index = 0; index < 4; ++index)
        snprintf(s_fixture.texts[index], 0x40, "%s %u%02u", index & 1 ? "Approach" : "Video", index, (s_seed >> index) % 100);
    for (index = 0; index < 5; ++index) {
        uint8_t *record = s_fixture.records[index];
        snprintf((char *)record, 0x40, "Video %u of %u", index, s_scenario % 3);
        snprintf((char *)record + 0x100, 0x40, "Title %u seed %x", index, s_seed);
        snprintf((char *)record + 0x500, 0x40, "Description %u", index);
        snprintf((char *)record + 0x900, 0x40, "C:\\videos\\video%u.vid", index);
        D32(record, 0xa04) = (s_seed >> (index + 3)) % 3 != 0;
    }
    snprintf((char *)s_fixture.record_before, 0x40, "Before %u", s_scenario);
    snprintf((char *)s_fixture.record_before + 0x100, 0x40, "Before title");
    D32(s_fixture.record_before, 0xa04) = (s_seed >> 2) & 1;
    /* the name last seen: none, one of the records', or another */
    snprintf(s_fixture.video.last, sizeof(s_fixture.video.last), "%s",
             (s_seed >> 16) % 3 == 0 ? "" : (s_seed >> 16) % 3 == 1 ? "video 1 OF 0" : "Other");
    PTR(s_fixture.notify, 0) = s_vtable;
    PTR(object, 0x1948) = s_control_vtable;
    if (OP.kind == FS98_OPTIONS_SAVE_VIDEO || OP.kind == FS98_OPTIONS_FLIGHT_VIDEO) {
        static const uint32_t statuses[] = {0, 0x1004, 0x100d, 0x1004};
        /* the CNotify, sometimes none */
        PTR(object, 0x10dc) = (s_seed >> 22) % 4 ? (void *)s_fixture.notify : 0;
        D32(object, 0x10e0) = statuses[(s_seed >> 23) % 4];
        D32(object, 0x1a04) = (s_seed >> 25) & 1;
        D32(object, 0x1418) = s_seed ^ 0x1418;
        D32(object, 0x1460) = s_seed ^ 0x1460;
        D32(object, 0x14c8) = s_seed ^ 0x14c8;
        D32(object, 0x1530) = s_seed ^ 0x1530;
        for (index = 0; index < 2; ++index) {
            uint8_t *descriptor = s_fixture.descriptor[index];
            if (OP.kind == FS98_OPTIONS_SAVE_VIDEO) {
                D32(descriptor, 0x18) = statuses[(s_seed >> (5 + index)) % 4];
                PTR(descriptor, 0x1c) = s_fixture.texts[index];
                D32(descriptor, 0x20) = 1 + (s_scenario + index * 0x13) % 0x40;     /* the two differ */
                D32(descriptor, 0x2c) = statuses[(s_seed >> (8 + index)) % 4] ^ 1;
                PTR(descriptor, 0x30) = s_fixture.texts[2 + index];
                D32(descriptor, 0x34) = 1 + (s_scenario + index * 0x29) % 0x40;
            } else {
                static const int32_t counts[] = {0, 1, 3, 5, -1};
                PTR(descriptor, 8) = descriptor_delete;
                PTR(descriptor, 0xc) = descriptor_edit;
                D32(descriptor, 0x18) = (s_seed >> (5 + index)) & 1;
                D32(descriptor, 0x20) = (s_seed >> (7 + index)) % 5;
                D32(descriptor, 0x24) = (s_seed >> (9 + index)) & 1;
                D32(descriptor, 0x28) = (s_seed >> (11 + index)) % 5;
                D32(descriptor, 0x2c) = (uint32_t)counts[(s_scenario + index) % 5];
                PTR(descriptor, 0x30) = s_fixture.records[index];   /* the second's list starts one on */
            }
        }
        /* the name already agreed to: often the one typed */
        snprintf((char *)object + 0x10e4, 0x100, "%s", (s_seed >> 3) % 3 ? s_fixture.texts[0] : "Other");
        if (OP.kind == FS98_OPTIONS_FLIGHT_VIDEO) PTR(object, 0x10e4) = s_fixture.descriptor[0];
    }
    snprintf(s_fixture.empty, sizeof(s_fixture.empty), "%s", "");
    for (index = 0; index < 8; ++index) snprintf(s_fixture.strings[index], 0x40, "String %u", index);
    if (OP.kind == FS98_OPTIONS_ADD_LOGBOOK || OP.kind == FS98_OPTIONS_LOGBOOK_ENTRY) {
        static const uint32_t statuses[] = {0, 0x1004, 0x100d, 0x1013, 0, 0};
        int entry = OP.kind == FS98_OPTIONS_LOGBOOK_ENTRY;
        uint32_t into = entry ? 0x54 : 0x28;
        PTR(object, entry ? 0x10e0 : 0x10e4) = s_fixture.descriptor[0];
        PTR(object, entry ? 0x10dc : 0x10e0) = s_fixture.descriptor[0] + into;
        if (!entry) PTR(object, 0x10dc) = (s_seed >> 22) % 4 ? (void *)s_fixture.notify : 0;
        for (index = 0; index < 5; ++index) PTR(object, 0x1c90 + index * 4) = s_fixture.strings[index];
        PTR(object, 0x1400) = s_fixture.strings[5];
        D32(object, 0x13b8) = s_seed ^ 0x13b8;
        D32(object, 0x1c68) = s_seed ^ 0x1c68;
        D32(object, 0x1ac8) = s_seed ^ 0x1ac8;
        for (index = 0; index < 2; ++index) {
            uint8_t *part = s_fixture.descriptor[index] + into;
            PTR(part, 4) = part_check;
            D32(part, 8) = statuses[(s_seed >> (3 + index)) % 6];
            PTR(part, 0xc) = s_fixture.texts[index];
            D32(part, 0x10) = 1 + (s_scenario + index * 0x13) % 0x40;
            if (!entry) {
                D32(part, 0x1c) = (s_seed >> (5 + index)) & 1;
                continue;
            }
            PTR(part, 0x20) = s_fixture.texts[2 + index];
            {
                static const uint32_t fields[] = {0x30, 0x44, 0x58, 0x6c};
                uint32_t field;
                for (field = 0; field < 4; ++field) {     /* each status from its own bits */
                    D32(part, fields[field]) = statuses[(s_seed >> (7 + field * 3 + index)) % 6];
                    PTR(part, fields[field] + 4) = s_fixture.strings[(field + index) % 8];
                    D32(part, fields[field] + 8) = 4 + (s_scenario + field + index) % 0x30;
                }
            }
            if ((s_seed >> 29) % 4 == 0)     /* nothing failed, one time in four: the check's own answer */
                D32(part, 8) = D32(part, 0x30) = D32(part, 0x44) = D32(part, 0x58) = D32(part, 0x6c) = 0;
            PTR(part, 0x98) = s_fixture.records[index];
            D32(part, 0x9c) = (s_scenario + index) % 4 == 3 ? 5 : (s_scenario + index) % 4;
            D32(part, 0xa0) = (s_seed >> (20 + index)) % 5;
            D32(part, 0xa4) = (s_seed >> (24 + index)) % 3 == 0;
        }
    }
    if (OP.kind == FS98_OPTIONS_BOOK) {
        static const char *const remarks[] = {"", "XC", "Pattern work", "Night currency at KSEA, three landings"};
        PTR(object, 0x10fc) = s_fixture.descriptor[0];
        PTR(object, 0x10f8) = (s_seed >> 22) % 4 ? (void *)s_fixture.notify : 0;
        PTR(object, 0x1af0) = s_fixture.strings[6];
        D32(object, 0x152c) = s_seed ^ 0x152c;
        D32(object, 0x15dc) = s_seed ^ 0x15dc;
        D32(object, 0x1574) = s_seed ^ 0x1574;
        D32(object, 0x1720) = s_seed ^ 0x1720;
        s_fixture.book.guard = (uint8_t)((s_seed >> 13) & 3);     /* each font already built or not */
        for (index = 0; index < 5; ++index) {
            snprintf((char *)s_fixture.books[index], 0x1e, "Logbook %u", index);
            snprintf((char *)s_fixture.books[index] + 0x1f, 0x40, "LOG%u.LOG", index);
            D32(s_fixture.books[index], 0x120) = (s_seed >> (index + 5)) & 1;
        }
        for (index = 0; index < 6; ++index) {
            uint8_t *row = s_fixture.rows[index];
            memset(row, 0, 0x4f);
            snprintf((char *)row, 9, "0%u-1%u-98", index % 9, index);
            snprintf((char *)row + 9, 17, "%s", index & 1 ? "Cessna 182" : "Learjet");
            snprintf((char *)row + 0x1a, 25, "%s", remarks[(s_seed >> index) % 4 == 3 && index > 2 ? 3 % 4 : (s_seed >> index) % 3]);
            snprintf((char *)row + 0x33, 7, "%u.%u", index, (s_seed >> index) % 10);
            snprintf((char *)row + 0x3a, 7, "0.%u", index);
            snprintf((char *)row + 0x41, 7, "1.%u", index);
            snprintf((char *)row + 0x48, 7, "%u.0", index + 2);
        }
        for (index = 0; index < 7; ++index) snprintf(s_fixture.fields[index], 0x50, "Field %u", index);
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            static const uint32_t opens[] = {0x60, 0x88, 0x9c, 0xb0, 0xc4, 0xd8, 0x74};
            uint32_t field;
            PTR(block, 4) = part_check;
            D32(block, 8) = (s_seed >> (3 + index)) % 3 ? 0 : 0x1004;
            PTR(block, 0xc) = s_fixture.texts[index];
            D32(block, 0x10) = 1 + (s_scenario + index) % 0x30;
            D32(block, 0x1c) = (s_scenario + index) % 5;
            D32(block, 0x20) = (s_seed >> (16 + index)) % 4;
            PTR(block, 0x24) = s_fixture.books[index];
            D32(block, 0x48) = (s_scenario / 5 + index) % 5;
            D32(block, 0x4c) = (s_seed >> (18 + index)) % 4;
            PTR(block, 0x50) = s_fixture.rows[index];
            for (field = 0; field < 7; ++field) PTR(block, opens[field]) = s_fixture.fields[field];
            D32(block, 0xe8) = 0;
            for (field = 0; field < 4; ++field) PTR(block, 0x100 + field * 0x14) = s_fixture.texts[(field + index) % 4];
            PTR(block, 0x150) = book_button;
        }
    }
    /* the handlers that test for no descriptor */
    if ((OP.rva == 0xe400 || OP.rva == 0xe1d0 || OP.rva == 0xddf0 || OP.rva == 0xd930 ||
         OP.rva == 0xd4d0 || OP.rva == 0xcc20 || OP.rva == 0x8550 || OP.rva == 0x95e0) && (s_seed >> 27) % 4 == 0) {
        PTR(object, descriptor_offset()) = 0;
        ++s_no_descriptor;
    }
    s_initial = s_fixture;
}
static LONG WINAPI fatal(EXCEPTION_POINTERS *exception)
{
    fprintf(stderr, "FAULT scenario=%u op=%u (%s) side=%d code=%lx address=%p\n", s_scenario,
        s_operation, OP.name, s_side, exception->ExceptionRecord->ExceptionCode,
        exception->ExceptionRecord->ExceptionAddress);
    fflush(stderr); ExitProcess(2); return EXCEPTION_EXECUTE_HANDLER;
}
static void land(void)
{
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    __builtin_longjmp(s_jump, 1);
}
/* A fault inside an operation is an outcome to compare, not the end of the run: a failed
 * allocation makes the factory store through null on both sides, and a break that faults
 * on one side only must show up as a difference. Addresses above the first page are where
 * each side's code or stack happens to be, so only low ones are logged. */
static int s_running;
static LONG CALLBACK expected_fault(EXCEPTION_POINTERS *exception)
{
    EXCEPTION_RECORD *record = exception->ExceptionRecord;
    uint32_t address;
    if (!s_running || (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
                       record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
                       record->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION))
        return EXCEPTION_CONTINUE_SEARCH;
    s_throw_at = 0;
    address = record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? (uint32_t)record->ExceptionInformation[1] : 0;
    put("fault code=%lx access=%u address=%x\n", record->ExceptionCode,
        record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ? (uint32_t)record->ExceptionInformation[0] : 0,
        address < 0x10000 ? address : 0xffffffff);
    if (!s_side && OP.signature == SIG_CREATE) ++s_allocation_faults;
    exception->ContextRecord->Eip = (DWORD)land;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static EXCEPTION_DISPOSITION __cdecl catch_throw(EXCEPTION_RECORD *record, void *frame,
                                                 CONTEXT *context, void *dispatcher)
{
    (void)context; (void)dispatcher;
    if (record->ExceptionCode == TEST_THROW &&
        !(record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))) {
        RtlUnwind(frame, 0, record, 0);
        s_throw_at = 0; put("CAUGHT\n"); if (!s_side) ++s_throws;
        land();
    }
    return ExceptionContinueSearch;
}
static __attribute__((noinline)) int32_t run(uint32_t throw_at)
{
    struct { void *previous; void *handler; } catcher;
    const struct fs98_options_class *class_info = &s_classes[OP.kind];
    void *entry = s_side ? OP.ours : (uint8_t *)s_retail + OP.rva;
    void *object = s_fixture.object;
    volatile int32_t result = 0;
    s_fixture = s_initial;
    if (!s_side) memcpy((uint8_t *)s_retail + RETAIL_VIDEO, &s_fixture.video, sizeof(s_fixture.video));
    if (!s_side) memcpy((uint8_t *)s_retail + RETAIL_BOOK, &s_fixture.book, sizeof(s_fixture.book));
    s_log_length = s_calls = 0; s_logs[s_side][0] = 0; s_throw_at = throw_at; s_from_handle_calls = 0; s_string_count = 0;
    if (OP.signature >= SIG_CONSTRUCT && !s_side) {
        uint32_t rva = OP.signature == SIG_DESTROY ? class_info->destructor :
                       OP.signature == SIG_DELETE ? class_info->delete :
                       OP.signature == SIG_CREATE ? class_info->factory : class_info->constructor;
        entry = (uint8_t *)s_retail + rva;
    }
    __asm__ volatile("movl %%fs:0, %0" : "=r"(s_saved_seh));
    s_running = 1;
    if (__builtin_setjmp(s_jump) == 0) {
        catcher.previous = (void *)s_saved_seh; catcher.handler = catch_throw;
        __asm__ volatile("movl %0, %%fs:0" : : "r"(&catcher) : "memory");
        switch (OP.signature) {
        case SIG_EVENT:
            if (OP.rva == 0x7c30 && s_scenario % 2 == 0) object = 0;
            ((void (TC *)(void *))entry)(object);
            break;
        case SIG_HELP:
            result = ((int32_t (TC *)(void *, uint32_t, void *))entry)(object, s_seed, s_fixture.info);
            break;
        case SIG_CONTEXT:
            result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 1);
            break;
        case SIG_EXCHANGE:
            ((void (TC *)(void *, void *))entry)(object, s_fixture.exchange);
            break;
        case SIG_INT:
            result = ((int32_t (TC *)(void *))entry)(object);
            break;
        case SIG_EMPTY_ARG:
            ((void (TC *)(void *, uint32_t))entry)(object, s_seed);
            break;
        case SIG_INT_ARG: {     /* each class's own edits' ids, none, or another */
            static const uint32_t ids[] = {0, 0x11a1, 0x11a3, 0x11a0, 0x1234};
            result = ((int32_t (TC *)(void *, uint32_t))entry)(object, ids[(s_seed >> 3) % 5]);
            break;
        }
        case SIG_EXIT:
            ((void (*)(void))entry)();
            break;
        case SIG_SCROLL:
            ((void (TC *)(void *, uint32_t, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 1, s_seed ^ 2);
            break;
        case SIG_LIST_DELETE: {
            void *returned = ((void *(TC *)(void *, uint32_t))entry)(s_fixture.object + 0x150c, s_seed);
            put("return.object %u\n", offset(returned));
            break;
        }
        case SIG_NOTIFY_DELETE: {
            void *returned = ((void *(TC *)(void *, uint32_t))entry)(s_fixture.notify, s_seed);
            put("return.object %u\n", offset(returned));
            break;
        }
        default: {
            void *returned = 0;
            const struct operation *ours = &OP - (OP.signature == SIG_ROUNDTRIP ? 4 :
                                                  OP.signature == SIG_CONSTRUCT_DELETE ? 5 : 0);
            if (OP.signature == SIG_CONSTRUCT || OP.signature >= SIG_ROUNDTRIP)
                returned = ((void *(TC *)(void *, void *))entry)(object, (void *)(uintptr_t)s_seed);
            else if (OP.signature == SIG_DESTROY) ((void (TC *)(void *))entry)(object);
            else if (OP.signature == SIG_DELETE) returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
            else result = ((int32_t (SC *)(uint32_t, void *))entry)(s_seed, s_fixture.descriptor[0]);
            if (returned) put("return.object %u\n", offset(returned));
            /* a whole lifetime: built, then torn down by the destructor or the deleting one */
            if (OP.signature == SIG_ROUNDTRIP) {
                entry = s_side ? (ours + 1)->ours : (uint8_t *)s_retail + class_info->destructor;
                ((void (TC *)(void *))entry)(object);
            } else if (OP.signature == SIG_CONSTRUCT_DELETE) {
                entry = s_side ? (ours + 2)->ours : (uint8_t *)s_retail + class_info->delete;
                returned = ((void *(TC *)(void *, uint32_t))entry)(object, s_seed);
                put("return.object %u\n", offset(returned));
            }
            break;
        }
        }
    }
    __asm__ volatile("movl %0, %%fs:0" : : "r"(s_saved_seh) : "memory");
    s_running = 0;
    if (!s_side) memcpy(&s_fixture.video, (uint8_t *)s_retail + RETAIL_VIDEO, sizeof(s_fixture.video));
    if (!s_side) memcpy(&s_fixture.book, (uint8_t *)s_retail + RETAIL_BOOK, sizeof(s_fixture.book));
    if (s_calls > s_max_calls) s_max_calls = s_calls;
    return result;
}
static int compare(uint32_t throw_at)
{
    int32_t retail, ours;
    uint32_t at;
    s_side = 0; retail = run(throw_at); s_retail_result = s_fixture;
    s_side = 1; ours = run(throw_at);
    ++s_comparisons;
    if (retail == ours && !memcmp(&s_retail_result, &s_fixture, sizeof(s_fixture)) &&
        !strcmp(s_logs[0], s_logs[1])) return 0;
    fprintf(stderr, "DIFFER scenario=%u op=%u (%s) throw=%u seed=%x script=%u returns=%d/%d\n",
        s_scenario, s_operation, OP.name, throw_at, s_seed, s_script, retail, ours);
    for (at = 0; at < sizeof(s_fixture); ++at)
        if (((uint8_t *)&s_retail_result)[at] != ((uint8_t *)&s_fixture)[at]) {
            fprintf(stderr, "byte offset=%x retail=%02x ours=%02x\n", at,
                ((uint8_t *)&s_retail_result)[at], ((uint8_t *)&s_fixture)[at]); break;
        }
    fprintf(stderr, "RETAIL:\n%sOURS:\n%s", s_logs[0], s_logs[1]);
    return 1;
}
int main(int argc, char **argv)
{
    uint32_t count = argc > 1 ? strtoul(argv[1], 0, 0) : 20000;
    uint32_t initial_seed = argc > 2 ? strtoul(argv[2], 0, 0) : 0x5f1a6c2d;
    int unwind = argc > 3 && !strcmp(argv[3], "unwind");
    uint32_t throw, limit, slot;
    if (!count || !initial_seed) return 2;
    s_rng = initial_seed;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal);
    AddVectoredExceptionHandler(1, expected_fault);
    s_retail = LoadLibraryExA("orig/English/Options.DLL", 0, DONT_RESOLVE_DLL_REFERENCES);
    if (!s_retail || (uint32_t)(uintptr_t)s_retail != BASE || !GetProcAddress(s_retail, "Linkage")) {
        fprintf(stderr, "orig/English/Options.DLL not mapped at its base: %p, error %lu\n",
                (void *)s_retail, GetLastError());
        return 2;
    }
    fs98_options_set_help_file((const char *)s_retail + RETAIL_HELP_FILE);
    setup_imports();
    for (slot = 0; slot < 64; ++slot) {
        s_vtable[slot] = (void *)s_other_slots[slot];
        s_other_vtable[slot] = (void *)s_replaced_slots[slot];
    }
    s_vtable[0xec / 4] = virtual_close; s_other_vtable[0xec / 4] = virtual_close_other;
    s_vtable[1] = s_other_vtable[1] = virtual_delete;
    s_vtable[0xe4 / 4] = virtual_skin; s_other_vtable[0xe4 / 4] = replaced_skin;
    s_vtable[0xd8 / 4] = s_other_vtable[0xd8 / 4] = virtual_validate;
    s_vtable[0xdc / 4] = s_other_vtable[0xdc / 4] = virtual_error;
    s_vtable[0xe0 / 4] = s_other_vtable[0xe0 / 4] = virtual_focus;
    for (slot = 0; slot < 64; ++slot) s_control_vtable[slot] = (void *)s_other_slots[slot];
    s_control_vtable[0xc4 / 4] = virtual_check;
    for (s_scenario = 0; s_scenario < count; ++s_scenario) {
        s_seed = next_random(); s_script = s_scenario & 15;
        for (s_operation = 0; s_operation < OP_COUNT; ++s_operation) {
            initialize();
            if (compare(0)) return 1;
            limit = s_calls;
            if (unwind) for (throw = 1; throw <= limit; ++throw)
                if (compare(throw)) return 1;
        }
    }
    printf("Options events: %u scenarios, %u operations, %u comparisons, 0 differ; seed=%x swaps=%u "
           "no_descriptor=%u throws=%u max_calls=%u allocation_faults=%u\n",
        count, (uint32_t)OP_COUNT, s_comparisons, initial_seed, s_swaps, s_no_descriptor, s_throws,
        s_max_calls, s_allocation_faults);
    FreeLibrary(s_retail); return 0;
}
