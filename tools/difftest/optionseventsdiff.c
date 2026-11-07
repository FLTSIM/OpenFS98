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
 * list's class, and ANALYSIS's landing (0x14050002) and manoeuvre (0x14050003) graphs and
 * its options, Flight analysis (0x1405), DEMO's Instant replay (0x1403), and MAIN's Select
 * demo (0x14090002), and Display options (0x14090001), Preferences (0x1409) and Customise
 * controls (0x140a) with their tab pages. */
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
    uint8_t object[0x3000];         /* up to the Sound page's 0x2dbc */
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
    uint8_t graph[0x40];            /* the graph control GetDlgItem finds */
    uint8_t dc[0x60];               /* a paint DC given to the landing's scale */
    char buffer[0x110];             /* a CString's GetBuffer */
    char empty[4];
    uint8_t info[32];
    uint8_t exchange[32];
    uint8_t font[32];
    uint8_t pages[6][0x2e00];       /* the pages Display options and Preferences make */
    char *names[2][7];              /* MAIN's driver and mode lists */
    uint32_t commands[2][8][2];     /* CONTROLS' lists: each command's name and id */
    uint8_t devices[2][6][0x14];    /* their devices */
    uint32_t bindings[2][6][16];    /* each device's bindings (value, kind) */
    uint32_t settings[2][6][24];    /* and a joystick's axis settings */
    uint8_t guard_after[32];
};
static struct fixture s_fixture, s_initial, s_retail_result;
static HMODULE s_retail;
static uint32_t s_seed, s_rng, s_scenario, s_operation, s_script;
static uint32_t s_calls, s_log_length, s_throw_at, s_saved_seh;
static uint32_t s_max_calls, s_throws, s_comparisons, s_swaps, s_allocation_faults, s_no_descriptor;
static int s_side;
static char s_logs[2][LOG_SIZE];
static void *s_vtable[64], *s_other_vtable[64], *s_control_vtable[64], *s_dc_vtable[64], *s_jump[5];
static void *s_host_table[0x300 / 4];

enum signature {
    SIG_EVENT, SIG_HELP, SIG_CONTEXT, SIG_EXCHANGE, SIG_INT, SIG_EMPTY_ARG, SIG_INT_ARG, SIG_NOTIFY_DELETE,
    SIG_EXIT, SIG_SCROLL, SIG_LIST_DELETE, SIG_DRAW, SIG_SCALE, SIG_INT_PAIR, SIG_INT_SMALL, SIG_VOID_SMALL, SIG_BAR,
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
#define LANDING(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_LANDING, #name}
#define MANOEUVRE(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_MANOEUVRE, #name}
#define ANALYSIS(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_ANALYSIS, #name}
#define REPLAY(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_REPLAY, #name}
#define DEMO(rva, name, signature) {rva, (void *)fs98_options_##name, signature, FS98_OPTIONS_DEMO, #name}
#define DISPLAY(rva, name, signature, kind) {rva, (void *)fs98_options_##name, signature, kind, #name}
#define SPIN_VTABLE 0x5b1a0000      /* SIMSUI's CSimsSpinButtonCtrl vtable, a data import */
#define SCROLL_VTABLE 0x5b1b0000    /* and its CSimsScrollBar's */
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
/* a tab page has no factory: its place in the six is a second construction */
#define PAGE_LIFETIME(kind, name) \
    {0, (void *)fs98_options_##name##_construct, SIG_CONSTRUCT, kind, #name "_construct"}, \
    {0, (void *)fs98_options_##name##_destroy, SIG_DESTROY, kind, #name "_destroy"}, \
    {0, (void *)fs98_options_##name##_delete, SIG_DELETE, kind, #name "_delete"}, \
    {0, (void *)fs98_options_##name##_construct, SIG_CONSTRUCT, kind, #name "_construct_again"}, \
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
    CLASS(0x7fd0, 0x7ff0, 0x7ab0, landing, FS98_OPTIONS_LANDING),
    LANDING(0x7b70, landing_draw, SIG_DRAW), LANDING(0x7ba0, landing_accept, SIG_EVENT),
    LANDING(0x7bf0, landing_cancel, SIG_EVENT), LANDING(0x7c40, landing_init, SIG_INT),
    LANDING(0x7d90, landing_scale, SIG_SCALE), LANDING(0x8060, landing_paint, SIG_EVENT),
    LANDING(0x8010, landing_set_up, SIG_EVENT), LANDING(0x8030, landing_has_descriptor, SIG_INT),
    LIFETIME(FS98_OPTIONS_LANDING, landing),
    CLASS(0xb050, 0xb070, 0xace0, manoeuvre, FS98_OPTIONS_MANOEUVRE),
    MANOEUVRE(0xad50, manoeuvre_draw, SIG_DRAW), MANOEUVRE(0xae90, manoeuvre_accept, SIG_EVENT),
    MANOEUVRE(0xaee0, manoeuvre_cancel, SIG_EVENT), MANOEUVRE(0xaf20, manoeuvre_init, SIG_INT),
    MANOEUVRE(0xb090, manoeuvre_set_up, SIG_EVENT), MANOEUVRE(0xb0b0, manoeuvre_has_descriptor, SIG_INT),
    LIFETIME(FS98_OPTIONS_MANOEUVRE, manoeuvre),
    CLASS(0x5140, 0x5160, 0x4d00, analysis, FS98_OPTIONS_ANALYSIS),
    ANALYSIS(0x4e10, analysis_cancel, SIG_EVENT), ANALYSIS(0x4e30, analysis_accept, SIG_EVENT),
    ANALYSIS(0x4e70, analysis_init, SIG_INT), ANALYSIS(0x5180, analysis_off, SIG_EVENT),
    ANALYSIS(0x5200, analysis_third, SIG_EVENT), ANALYSIS(0x5280, analysis_first, SIG_EVENT),
    ANALYSIS(0x5300, analysis_second, SIG_EVENT), ANALYSIS(0x5380, analysis_nothing, SIG_EVENT),
    ANALYSIS(0x5390, analysis_read, SIG_EVENT), ANALYSIS(0x54e0, analysis_populate, SIG_EVENT),
    ANALYSIS(0x5620, analysis_error, SIG_INT), ANALYSIS(0x57e0, has_descriptor, SIG_INT),
    LIFETIME(FS98_OPTIONS_ANALYSIS, analysis),
    CLASS(0x7110, 0x7130, 0x6c30, replay, FS98_OPTIONS_REPLAY),
    REPLAY(0x6d70, notify_close, SIG_EVENT), REPLAY(0x6d90, replay_cancel, SIG_EVENT),
    REPLAY(0x6db0, replay_accept, SIG_EVENT), REPLAY(0x6df0, replay_init, SIG_INT),
    REPLAY(0x7150, replay_populate, SIG_EVENT), REPLAY(0x72d0, replay_has_descriptor, SIG_INT),
    REPLAY(0x72e0, replay_validate, SIG_INT_ARG), REPLAY(0x73d0, replay_error, SIG_INT_ARG),
    LIFETIME(FS98_OPTIONS_REPLAY, replay),
    CLASS(0xc6c0, 0xc6e0, 0xc300, demo, FS98_OPTIONS_DEMO),
    DEMO(0x6d70, notify_close, SIG_EVENT), DEMO(0xc660, demo_cancel, SIG_EVENT),
    DEMO(0xc690, demo_accept, SIG_EVENT), DEMO(0xc3b0, demo_init, SIG_INT),
    DEMO(0xc5a0, demo_select, SIG_EVENT), DEMO(0xc700, demo_read, SIG_EVENT),
    DEMO(0xc730, demo_populate, SIG_EVENT), DEMO(0xc7c0, demo_has_descriptor, SIG_INT),
    LIFETIME(FS98_OPTIONS_DEMO, demo),
    CLASS(0x3e60, 0x3e80, 0x3cf0, display, FS98_OPTIONS_DISPLAY),
    DISPLAY(0x3da0, display_cancel, SIG_EVENT, FS98_OPTIONS_DISPLAY), DISPLAY(0x3dc0, display_accept, SIG_EVENT, FS98_OPTIONS_DISPLAY),
    DISPLAY(0x3e20, display_close, SIG_EVENT, FS98_OPTIONS_DISPLAY), DISPLAY(0x3ea0, display_init, SIG_INT, FS98_OPTIONS_DISPLAY),
    DISPLAY(0x42b0, display_read, SIG_EVENT, FS98_OPTIONS_DISPLAY), DISPLAY(0x42f0, display_attach, SIG_EVENT, FS98_OPTIONS_DISPLAY),
    DISPLAY(0x4360, display_error, SIG_INT, FS98_OPTIONS_DISPLAY),
    LIFETIME(FS98_OPTIONS_DISPLAY, display),
    CLASS(0x2e30, 0x2e50, 0x2c30, display_aircraft, FS98_OPTIONS_DISPLAY_AIRCRAFT),
    DISPLAY(0x2d50, display_aircraft_init, SIG_EVENT, FS98_OPTIONS_DISPLAY_AIRCRAFT),
    DISPLAY(0x2e70, display_aircraft_read, SIG_EVENT, FS98_OPTIONS_DISPLAY_AIRCRAFT),
    DISPLAY(0x2ec0, display_aircraft_populate, SIG_EVENT, FS98_OPTIONS_DISPLAY_AIRCRAFT),
    PAGE_LIFETIME(FS98_OPTIONS_DISPLAY_AIRCRAFT, display_aircraft),
    CLASS(0x3180, 0x31a0, 0x3070, display_map, FS98_OPTIONS_DISPLAY_MAP),
    DISPLAY(0x30e0, display_map_init, SIG_EVENT, FS98_OPTIONS_DISPLAY_MAP),
    DISPLAY(0x31c0, display_map_read, SIG_EVENT, FS98_OPTIONS_DISPLAY_MAP),
    DISPLAY(0x3200, display_map_populate, SIG_EVENT, FS98_OPTIONS_DISPLAY_MAP),
    PAGE_LIFETIME(FS98_OPTIONS_DISPLAY_MAP, display_map),
    CLASS(0x48a0, 0x48c0, 0x45d0, display_scenery, FS98_OPTIONS_DISPLAY_SCENERY),
    DISPLAY(0x46c0, display_scenery_init, SIG_EVENT, FS98_OPTIONS_DISPLAY_SCENERY),
    DISPLAY(0x48e0, display_scenery_read, SIG_EVENT, FS98_OPTIONS_DISPLAY_SCENERY),
    DISPLAY(0x4990, display_scenery_populate, SIG_EVENT, FS98_OPTIONS_DISPLAY_SCENERY),
    DISPLAY(0x4a60, display_scenery_enable, SIG_EVENT, FS98_OPTIONS_DISPLAY_SCENERY),
    PAGE_LIFETIME(FS98_OPTIONS_DISPLAY_SCENERY, display_scenery),
    CLASS(0x3710, 0x3730, 0x3450, display_hardware, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3540, display_hardware_init, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3750, display_hardware_read, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x37e0, display_hardware_populate, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x38d0, display_hardware_fill_drivers, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3950, display_hardware_list_drivers, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3990, display_hardware_fill_modes, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3a10, display_hardware_list_modes, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3a80, display_hardware_accelerate, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3ac0, display_hardware_refresh, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3af0, display_hardware_driver, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    DISPLAY(0x3b10, display_hardware_changed, SIG_EVENT, FS98_OPTIONS_DISPLAY_HARDWARE),
    PAGE_LIFETIME(FS98_OPTIONS_DISPLAY_HARDWARE, display_hardware),
    CLASS(0xb840, 0xb860, 0xb690, prefs, FS98_OPTIONS_PREFS),
    DISPLAY(0xb780, prefs_cancel, SIG_EVENT, FS98_OPTIONS_PREFS), DISPLAY(0xb7a0, prefs_accept, SIG_EVENT, FS98_OPTIONS_PREFS),
    DISPLAY(0xb800, prefs_close, SIG_EVENT, FS98_OPTIONS_PREFS), DISPLAY(0xb880, prefs_defaults, SIG_EVENT, FS98_OPTIONS_PREFS),
    DISPLAY(0xb9d0, prefs_init, SIG_INT, FS98_OPTIONS_PREFS), DISPLAY(0xbef0, prefs_read, SIG_EVENT, FS98_OPTIONS_PREFS),
    DISPLAY(0xbf40, prefs_attach, SIG_EVENT, FS98_OPTIONS_PREFS), DISPLAY(0xc0b0, prefs_error, SIG_INT, FS98_OPTIONS_PREFS),
    LIFETIME(FS98_OPTIONS_PREFS, prefs),
    CLASS(0x5ea0, 0x5ec0, 0x5a80, prefs_general, FS98_OPTIONS_PREFS_GENERAL),
    DISPLAY(0x6390, prefs_general_choose, SIG_EVENT, FS98_OPTIONS_PREFS_GENERAL),
    DISPLAY(0x5bc0, prefs_general_first, SIG_EVENT, FS98_OPTIONS_PREFS_GENERAL),
    DISPLAY(0x5be0, prefs_general_second, SIG_EVENT, FS98_OPTIONS_PREFS_GENERAL),
    DISPLAY(0x5ee0, prefs_general_read, SIG_EVENT, FS98_OPTIONS_PREFS_GENERAL),
    DISPLAY(0x5fd0, prefs_general_populate, SIG_EVENT, FS98_OPTIONS_PREFS_GENERAL),
    DISPLAY(0x5c00, prefs_general_init, SIG_EVENT, FS98_OPTIONS_PREFS_GENERAL),
    DISPLAY(0x60d0, prefs_general_error, SIG_INT, FS98_OPTIONS_PREFS_GENERAL),
    PAGE_LIFETIME(FS98_OPTIONS_PREFS_GENERAL, prefs_general),
    CLASS(0x77d0, 0x77f0, 0x7650, prefs_instrument, FS98_OPTIONS_PREFS_INSTRUMENT),
    DISPLAY(0x7810, prefs_instrument_read, SIG_EVENT, FS98_OPTIONS_PREFS_INSTRUMENT),
    DISPLAY(0x7880, prefs_instrument_populate, SIG_EVENT, FS98_OPTIONS_PREFS_INSTRUMENT),
    DISPLAY(0x76d0, prefs_instrument_init, SIG_EVENT, FS98_OPTIONS_PREFS_INSTRUMENT),
    PAGE_LIFETIME(FS98_OPTIONS_PREFS_INSTRUMENT, prefs_instrument),
    CLASS(0x67e0, 0x6800, 0x65d0, prefs_international, FS98_OPTIONS_PREFS_INTERNATIONAL),
    DISPLAY(0x6820, prefs_international_read, SIG_EVENT, FS98_OPTIONS_PREFS_INTERNATIONAL),
    DISPLAY(0x68e0, prefs_international_populate, SIG_EVENT, FS98_OPTIONS_PREFS_INTERNATIONAL),
    DISPLAY(0x66b0, prefs_international_init, SIG_EVENT, FS98_OPTIONS_PREFS_INTERNATIONAL),
    PAGE_LIFETIME(FS98_OPTIONS_PREFS_INTERNATIONAL, prefs_international),
    CLASS(0x2850, 0x2870, 0x2620, prefs_display, FS98_OPTIONS_PREFS_DISPLAY),
    DISPLAY(0x2890, prefs_display_read, SIG_EVENT, FS98_OPTIONS_PREFS_DISPLAY),
    DISPLAY(0x2910, prefs_display_populate, SIG_EVENT, FS98_OPTIONS_PREFS_DISPLAY),
    DISPLAY(0x26d0, prefs_display_button, SIG_EVENT, FS98_OPTIONS_PREFS_DISPLAY),
    DISPLAY(0x29d0, prefs_display_preset, SIG_EVENT, FS98_OPTIONS_PREFS_DISPLAY),
    DISPLAY(0x2a10, prefs_display_other, SIG_EVENT, FS98_OPTIONS_PREFS_DISPLAY),
    DISPLAY(0x2a60, prefs_display_box, SIG_EVENT, FS98_OPTIONS_PREFS_DISPLAY),
    DISPLAY(0x2700, prefs_display_init, SIG_EVENT, FS98_OPTIONS_PREFS_DISPLAY),
    PAGE_LIFETIME(FS98_OPTIONS_PREFS_DISPLAY, prefs_display),
    CLASS(0x1f70, 0x1f90, 0x1c00, prefs_sound, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x2050, prefs_sound_read, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x2190, prefs_sound_populate, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x1fb0, prefs_sound_box0, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x1fd0, prefs_sound_box1, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x1ff0, prefs_sound_box2, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x2010, prefs_sound_box3, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x2030, prefs_sound_box4, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    DISPLAY(0x1d10, prefs_sound_init, SIG_EVENT, FS98_OPTIONS_PREFS_SOUND),
    PAGE_LIFETIME(FS98_OPTIONS_PREFS_SOUND, prefs_sound),
    CLASS(0xb390, 0xb3b0, 0xb230, prefs_flight, FS98_OPTIONS_PREFS_FLIGHT),
    DISPLAY(0xb3d0, prefs_flight_read, SIG_EVENT, FS98_OPTIONS_PREFS_FLIGHT),
    DISPLAY(0xb420, prefs_flight_populate, SIG_EVENT, FS98_OPTIONS_PREFS_FLIGHT),
    DISPLAY(0xb2b0, prefs_flight_init, SIG_EVENT, FS98_OPTIONS_PREFS_FLIGHT),
    PAGE_LIFETIME(FS98_OPTIONS_PREFS_FLIGHT, prefs_flight),
    CLASS(0xe750, 0xe770, 0xe610, controls, FS98_OPTIONS_CONTROLS),
    DISPLAY(0xe6b0, controls_cancel, SIG_EVENT, FS98_OPTIONS_CONTROLS), DISPLAY(0xe6d0, controls_accept, SIG_EVENT, FS98_OPTIONS_CONTROLS),
    DISPLAY(0xe730, controls_close, SIG_EVENT, FS98_OPTIONS_CONTROLS), DISPLAY(0xe790, controls_init, SIG_INT, FS98_OPTIONS_CONTROLS),
    DISPLAY(0xead0, controls_read, SIG_EVENT, FS98_OPTIONS_CONTROLS), DISPLAY(0xeb00, controls_attach, SIG_EVENT, FS98_OPTIONS_CONTROLS),
    DISPLAY(0xebe0, controls_error, SIG_INT, FS98_OPTIONS_CONTROLS),
    LIFETIME(FS98_OPTIONS_CONTROLS, controls),
    CLASS(0xf380, 0xf3a0, 0xeef0, controls_sensitivity, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf060, controls_sensitivity_init, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf7e0, controls_sensitivity_populate, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf8c0, controls_fill_devices, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf9a0, controls_fill_axes, SIG_EMPTY_ARG, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xfc00, controls_keyboard_axis, SIG_INT_PAIR, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xfef0, controls_mouse_axis, SIG_INT_PAIR, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x10120, controls_show_mouse, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x10290, controls_show_keyboard, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x10360, controls_row_axis, SIG_INT_SMALL, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x10490, controls_axis_row, SIG_INT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x107c0, controls_show_joystick, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x10920, controls_joysticks, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x10930, controls_calibrate, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0x109f0, controls_changed, SIG_DRAW, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf3c0, controls_reset, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf430, controls_axis, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf490, controls_device, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf520, controls_scale, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf660, controls_null_zone, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    DISPLAY(0xf730, controls_reverse, SIG_EVENT, FS98_OPTIONS_CONTROLS_SENSITIVITY),
    PAGE_LIFETIME(FS98_OPTIONS_CONTROLS_SENSITIVITY, controls_sensitivity),
    CLASS(0x10ef0, 0x10f10, 0x10db0, controls_keys, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x10f30, keys_flight, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x10fb0, keys_slew, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x11030, keys_vertical, SIG_BAR, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x11180, keys_horizontal, SIG_BAR, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x11250, keys_command, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x112f0, keys_key, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x113f0, keys_stick, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x114d0, keys_check, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x115c0, keys_reset, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x116b0, keys_clear, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x117c0, keys_assign, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x117e0, keys_init, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x11af0, keys_populate, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x11cb0, keys_assign_key, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x11d90, keys_assign_stick, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x11eb0, keys_key_captured, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x120b0, keys_stick_captured, SIG_EVENT, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x123f0, keys_changed, SIG_DRAW, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x12420, keys_top, SIG_VOID_SMALL, FS98_OPTIONS_CONTROLS_KEYS),
    DISPLAY(0x12490, keys_joystick, SIG_VOID_SMALL, FS98_OPTIONS_CONTROLS_KEYS),
    PAGE_LIFETIME(FS98_OPTIONS_CONTROLS_KEYS, controls_keys),
    CLASS(0x129f0, 0x12a10, 0x12950, controls_force, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12bf0, controls_force_master, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12c50, controls_force_box1, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12c60, controls_force_box2, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12c70, controls_force_box3, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12c80, controls_force_box4, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12c90, controls_force_box5, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12b70, controls_force_populate, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    DISPLAY(0x12a30, controls_force_init, SIG_EVENT, FS98_OPTIONS_CONTROLS_FORCE),
    PAGE_LIFETIME(FS98_OPTIONS_CONTROLS_FORCE, controls_force),
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
    if (OP.kind == FS98_OPTIONS_PREFS_SOUND || OP.kind == FS98_OPTIONS_CONTROLS_SENSITIVITY ||
        OP.kind == FS98_OPTIONS_CONTROLS_KEYS) return 0x111c;
    if (s_classes[OP.kind].page) return 0x1118;     /* a page's part of the block */
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
            /* the answer; on the Hardware page MAIN's callbacks, so another word */
            if (OP.kind == FS98_OPTIONS_DISPLAY_HARDWARE) D32(descriptor, 0x14) ^= 0x10;
            else D32(descriptor, 0) ^= 0x10;
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
    if (OP.kind == FS98_OPTIONS_DISPLAY_HARDWARE && (s_script & 2)) {      /* the lists' rooms */
        uint8_t *block = PTR(object, 0x1118);
        if (block == s_fixture.descriptor[0] || block == s_fixture.descriptor[1]) {
            D32(block, 0x18) ^= 1;
            D32(block, 0x24) ^= 2;
        }
    }
    if (OP.kind == FS98_OPTIONS_DEMO && (s_script & 2)) {
        uint8_t *block = PTR(object, 0x10e0);
        if (block == s_fixture.descriptor[0] || block == s_fixture.descriptor[1]) {
            D32(block, 0x14) = (D32(block, 0x14) + 1) % 5;
            D32(block, 0x10) ^= 1;
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
                              OP.kind == FS98_OPTIONS_FLIGHT_VIDEO ? 0x1418 :
                              OP.kind == FS98_OPTIONS_DEMO ? 0x1554 :
                              OP.kind == FS98_OPTIONS_DISPLAY_MAP || OP.kind == FS98_OPTIONS_DISPLAY_HARDWARE ? 0x12b4 : 0x1100) = s_seed ^ 0x12345678;
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
/* vtable +0xe8, the dialog's frame: six arguments */
static int32_t TC virtual_frame(void *object, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, uint32_t module)
{ put("virtual.frame %u %d %d %d %d %d %x\n", offset(object), a, b, c, d, e, module); mutate(); return (int32_t)s_seed; }
static int32_t TC replaced_frame(void *object, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, uint32_t module)
{ put("other.frame\n"); return virtual_frame(object, a, b, c, d, e, module); }
/* MAIN's driver block's callbacks, given the block: list the drivers, the modes, refresh */
static uint32_t SC driver_list(void *block)
{
    put("drivers %u chosen=%u\n", offset(block), D32(block, 0x20)); mutate();
    D32(block, 0x20) = (s_seed >> 3) % 4; D32(block, 0x30) = (s_seed >> 7) & 1;
    return 0;
}
static uint32_t SC driver_modes(void *block)
{
    put("modes %u chosen=%u\n", offset(block), D32(block, 0x2c)); mutate();
    D32(block, 0x2c) = (s_seed >> 4) % 5; D32(block, 0x24) = (D32(block, 0x24) + 1) % 8;
    return 0;
}
static uint32_t SC driver_refresh(void *block)
{ put("refresh %u\n", offset(block)); mutate(); D32(block, 0x18) = (s_seed >> 9) % 8; return 0; }
/* MAIN's Preferences callbacks, each given the block */
#define BLOCK_CALLBACK(n) static uint32_t SC block_callback_##n(void *block) \
{ put("callback %u %u\n", n, offset(block)); mutate(); return s_seed; }
BLOCK_CALLBACK(0) BLOCK_CALLBACK(1) BLOCK_CALLBACK(2) BLOCK_CALLBACK(3) BLOCK_CALLBACK(4)
#undef BLOCK_CALLBACK
/* CONTROLS' callbacks: reset a device's sensitivities, count the joysticks again; and the
 * joystick control panel's ShowJoyCPL */
static uint32_t SC controls_reset(void *block, uint32_t mode, uint32_t joystick)
{
    put("controls.reset %u %u %u\n", offset(block), mode, joystick); mutate();
    *(uint16_t *)((uint8_t *)block + 0x64) ^= 7;
    return s_seed;
}
static uint32_t SC controls_reset_device(void *block, void *list, uint32_t device)
{ put("controls.reset.device %u %u %u\n", offset(block), offset(list), device); mutate(); return s_seed; }
static uint32_t SC controls_recount(void *block) { put("controls.recount %u\n", offset(block)); mutate(); return s_seed; }
static uint32_t SC joystick_applet(uint32_t window)
{ put("applet %x\n", window); mutate(); return (s_seed >> (s_calls % 8)) % 3; }
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
/* ANALYSIS's graph control: an operation, the block, the graph's window */
static uint32_t SC graph_control(uint32_t operation, void *block, uint32_t window)
{ put("graph %u block=%u window=%u\n", operation, offset(block), window); mutate(); return s_seed; }
/* the paint DC's own virtuals */
static uint32_t TC dc_text_color(void *dc, uint32_t color)
{ put("dc.color %u %x\n", where(dc), color); mutate(); return 0; }
static int32_t TC dc_text_out(void *dc, int32_t x, int32_t y, const char *text, int32_t length)
{ put("dc.text %u %d,%d [%.*s] %d\n", where(dc), x, y, length, text, length); mutate(); return 1; }
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
    if ((message == 0x180 || message == 0x181 || message == 0x143) && where((void *)(uintptr_t)l_param) == 0xffffffff &&
        l_param > 0x10000) put("message %u %x %u text=[%s]\n", window, message, w_param, (const char *)(uintptr_t)l_param);
    else if (message == 0x180 || message == 0x19a) put("message %u %x %u record=%u\n", window, message, w_param, offset((void *)(uintptr_t)l_param));
    else if (message == 0x198) { put("message %u %x %u rect\n", window, message, w_param);
        ((int32_t *)(uintptr_t)l_param)[0] = 3; ((int32_t *)(uintptr_t)l_param)[1] = (int32_t)w_param * 12;
        ((int32_t *)(uintptr_t)l_param)[2] = 200; ((int32_t *)(uintptr_t)l_param)[3] = (int32_t)w_param * 12 + 11; }
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
    if (where((void *)(uintptr_t)code) != 0xffffffff)   /* an error block: its status, text and limit */
        put("  block %x [%s] %d\n", D32((void *)(uintptr_t)code, 0), (char *)PTR((void *)(uintptr_t)code, 4),
            (int32_t)D32((void *)(uintptr_t)code, 8));
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
static void *dialog_item(void *object, int32_t id)
{ put("dialog.item %u %x\n", offset(object), id); mutate(); return s_fixture.graph; }
static void remap_rect(struct fs98_options_rect *rect, void *control)
{
    put("remap.rect %u\n", offset(control)); mutate();
    rect->left = 10 + (int32_t)(s_seed % 30); rect->top = 20; rect->right = 300 + (int32_t)(s_seed % 50); rect->bottom = 200;
}
static void move_window(void *window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ put("move.window %u %d,%d %dx%d %d\n", offset(window), x, y, width, height, repaint); mutate(); }
static char *get_buffer(void *string, int32_t size)
{ put("string.buffer %u %d\n", where(string), size); mutate(); s_fixture.buffer[0] = 0; return s_fixture.buffer; }
static void release_buffer(void *string, int32_t size)
{ put("string.release %u %d [%s]\n", where(string), size, s_fixture.buffer); string_set(string, s_fixture.buffer); mutate(); }
static void string_append_string(void *string, const void *other)
{ string_append(string, ((const struct fs98_options_string *)other)->text); }
static int32_t message_box(const char *text, uint32_t type, uint32_t help)
{ put("message.box [%s] %x %x\n", text, type, help); mutate(); return 1; }
static void paint_construct(void *dc, void *window)
{ put("paint.construct %u window=%u\n", where(dc), offset(window)); PTR(dc, 0) = s_dc_vtable; mutate(); }
static void paint_destroy(void *dc) { put("paint.destroy %u\n", where(dc)); mutate(); }
static int32_t dc_mode(void *dc, int32_t mode) { put("dc.mode %u %d\n", where(dc), mode); mutate(); return 2; }
static uint32_t dc_align(void *dc, uint32_t align) { put("dc.align %u %u\n", where(dc), align); mutate(); return 0; }
static int32_t get_window_rect(uint32_t window, struct fs98_options_rect *rect)
{
    put("window.rect %u\n", window); mutate();
    rect->left = 400; rect->top = 300 - (int32_t)(s_seed % 40); rect->right = 700;
    rect->bottom = (s_seed >> 3) % 4 == 0 ? 250 - (int32_t)(s_seed % 90) : 420 + (int32_t)(s_seed % 100);   /* sometimes upside down */
    return 1;
}
static int32_t screen_to_client(uint32_t window, void *point)
{
    put("screen.to.client %u %d,%d\n", window, ((int32_t *)point)[0], ((int32_t *)point)[1]); mutate();
    ((int32_t *)point)[0] -= 100; ((int32_t *)point)[1] -= 60;
    return 1;
}
static int32_t to_int(const char *text) { put("atoi [%s]\n", shown(text)); mutate(); return atoi(text); }
static int32_t format_int(char *out, const char *format, int32_t value)
{ put("sprintf [%s] %d\n", format, value); mutate(); return sprintf(out, format, value); }
/* a radio on (its user data is its own handle) or off, each window from its own bits */
static int32_t get_window_long(uint32_t window, int32_t index)
{
    put("window.long %u %d\n", window, index); mutate();
    return (s_seed >> (window % 29)) & 1 ? (int32_t)window : (int32_t)(window ^ 0x40);
}
static int32_t set_radio(void *control, int32_t checked)
{ put("set.radio %u %d\n", offset(control), checked); mutate(); return (int32_t)s_seed; }
static char *long_to_text(int32_t value, char *out, int32_t radix)
{ put("ltoa %d %d\n", value, radix); mutate(); sprintf(out, "%ld", (long)value); return out; }
static int32_t to_long(const char *text) { put("atol [%s]\n", shown(text)); mutate(); return atol(text); }
static void ddx_int(void *exchange, uint32_t id, int32_t *value)
{ put("ddx.int %u %x %u %d\n", offset(exchange), id, offset(value), *value); mutate(); }
static void ddv_min_max(void *exchange, int32_t value, int32_t low, int32_t high)
{ put("ddv.minmax %u %d %d %d\n", offset(exchange), value, low, high); mutate(); }
static int32_t notify_send(void *notify, const char *title, const char *format, void *value)
{ put("notify.send %u [%s] [%s] %x\n", offset(notify), shown(title), shown(format), (uint32_t)(uintptr_t)value); mutate(); return (int32_t)s_seed; }
static int16_t async_key_state(int32_t key)
{   /* pressed since the last call (bit 0) and down now (bit 15), each its own bit */
    static const uint16_t states[] = {0, 1, 0x8000, 0x8001};
    put("async.key %d\n", key); mutate(); return (int16_t)states[(s_seed >> (s_calls % 8)) % 4];
}
static int32_t tab_init(void *tab, void *dialog, void *font, uint32_t bitmap, void *dib, void *palette,
                        uint32_t a, uint32_t color, int32_t x, int32_t y, int32_t width, int32_t height,
                        uint32_t module, uint32_t b)
{
    put("tab.init %u dialog=%u font=%u bitmap=%x dib=%u palette=%u %x %x %d,%d %dx%d module=%x %x\n", offset(tab),
        offset(dialog), offset(font), bitmap, offset(dib), offset(palette), a, color, x, y, width, height, module, b);
    mutate(); return (int32_t)s_seed;
}
static int32_t page_init(void *page, int32_t value)
{ put("page.init %u %d\n", offset(page), value); mutate(); return (int32_t)s_seed; }
static void add_page(void *tab, void *page, char *title, int32_t value)
{ put("tab.add %u page=%u [%s] %d\n", offset(tab), offset(page), shown(title), value); mutate(); }
static void helper_init(void *helper, void *tab)
{ put("helper.init %u tab=%u\n", offset(helper), offset(tab)); mutate(); }
static int32_t set_last_page(void *tab, void *page)
{ put("tab.last %u page=%u\n", offset(tab), offset(page)); mutate(); return (int32_t)s_seed; }
static int32_t set_active_tab(void *tab, int32_t index, uint32_t window)
{ put("tab.active %u %d %x\n", offset(tab), index, window); mutate(); return (int32_t)s_seed; }
static int32_t get_active_tab(void *tab)
{ put("tab.get %u\n", offset(tab)); mutate(); return (int32_t)((s_seed >> (s_calls % 8)) % 8) - 1; }
static int32_t slider_init(void *slider, uint32_t bitmap, uint32_t thumb, void *dib, void *palette, uint32_t a,
                           uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f, uint32_t module)
{
    put("slider.init %u %x %x dib=%u palette=%u %u %u %u %u %d %d %x\n", offset(slider), bitmap, thumb, offset(dib),
        offset(palette), a, b, c, d, e, f, module);
    mutate(); return (int32_t)s_seed;
}
static void set_scroll_range(void *slider, int32_t low, int32_t high)
{ put("slider.range %u %d %d\n", offset(slider), low, high); mutate(); }
static int32_t get_scroll_pos(void *slider)
{ put("slider.get %u\n", offset(slider)); mutate(); return (int32_t)((s_seed >> (s_calls % 16)) % 103) - 1; }
static int32_t set_scroll_pos(void *slider, int32_t position)
{ put("slider.set %u %d\n", offset(slider), position); mutate(); return (int32_t)s_seed; }
static int32_t string_load(void *string, uint32_t id)
{
    char text[32];
    put("string.load %u %x\n", where(string), id);
    snprintf(text, sizeof(text), "Caption %x", id);
    string_set(string, text); mutate(); return 1;
}
static int32_t redraw_window(uint32_t window, const void *rect, uint32_t region, uint32_t flags)
{ put("redraw %u %u %u %x\n", window, where(rect), region, flags); mutate(); return 1; }
static uint32_t load_library(const char *name)
{ put("load.library [%s]\n", shown(name)); mutate(); return (s_seed >> 14) % 5 ? 0x5a5a0000 : 0; }
static void *get_proc_address(uint32_t library, const char *name)
{ put("get.proc %x [%s]\n", library, shown(name)); mutate(); return (s_seed >> 15) % 5 ? (void *)joystick_applet : 0; }
static int32_t free_library(uint32_t library) { put("free.library %x\n", library); mutate(); return 1; }
static uint32_t host_joystick_count(void) { put("host.joysticks\n"); mutate(); return (s_seed >> 16) % 3; }
static int32_t set_scroll_pos_window(uint32_t window, int32_t bar, int32_t position, int32_t redraw)
{ put("scroll.pos %u %d %d %d\n", window, bar, position, redraw); mutate(); return (int32_t)(s_seed % 7); }
static int32_t get_scroll_pos_window(uint32_t window, int32_t bar)
{ put("scroll.get %u %d\n", window, bar); mutate(); return (int32_t)((s_seed >> (s_calls % 8)) % 6) - 1; }
static int32_t set_scroll_range_window(uint32_t window, int32_t bar, int32_t low, int32_t high, int32_t redraw)
{ put("scroll.range %u %d %d %d %d\n", window, bar, low, high, redraw); mutate(); return 1; }
static int32_t get_scroll_range_window(uint32_t window, int32_t bar, int32_t *low, int32_t *high)
{ put("scroll.get.range %u %d\n", window, bar); mutate(); *low = (int32_t)(s_seed % 3); *high = *low + (int32_t)((s_seed >> 4) % 9); return 1; }
static int32_t enable_scroll_bar(uint32_t window, uint32_t bar, uint32_t arrows)
{ put("scroll.enable %u %u %u\n", window, bar, arrows); mutate(); return 1; }
static int32_t client_to_screen(uint32_t window, void *point)
{ put("client.to.screen %u %d,%d\n", window, ((int32_t *)point)[0], ((int32_t *)point)[1]); mutate(); ((int32_t *)point)[0] += 50; ((int32_t *)point)[1] += 40; return 1; }
static int32_t update_window(uint32_t window) { put("update %u\n", window); mutate(); return 1; }
static int32_t format_two(char *out, const char *format, const char *a, const char *b)
{ put("wsprintf [%s] [%s] [%s]\n", shown(format), shown(a), shown(b)); mutate(); return sprintf(out, "%s|%s|%s", format, a, b); }
static void string_format(void *string, uint32_t id, uint32_t value)
{
    char text[32];
    put("string.format %u %x %u\n", where(string), id, value);
    snprintf(text, sizeof(text), "Joystick %u (%x)", value, id);
    string_set(string, text); mutate();
}
static void check_list_init(void *list, void *font, void *label)
{ put("checklist.init %u font=%u label=%u\n", offset(list), offset(font), offset(label)); mutate(); }
static void check_list_enable(void *list, int32_t index, int32_t on)
{ put("checklist.enable %u %d %d\n", offset(list), index, on); mutate(); }
static void check_list_set_check(void *list, int32_t index, int32_t check)
{ put("checklist.set %u %d %d\n", offset(list), index, check); mutate(); }
static int32_t check_list_get_check(void *list, int32_t index)
{ put("checklist.get %u %d\n", offset(list), index); mutate(); return (int32_t)((s_seed >> (s_calls % 8)) % 3); }
static int32_t hot_create(const char *kind, void *control, uint32_t style, const struct fs98_options_rect *rect, void *parent, uint32_t id)
{
    put("%s.create %u %x rect=%d,%d,%d,%d parent=%u %x\n", kind, offset(control), style, rect->left, rect->top,
        rect->right, rect->bottom, offset(parent), id);
    mutate(); return 1;
}
static int32_t hot_key_create(void *control, uint32_t style, const struct fs98_options_rect *rect, void *parent, uint32_t id)
{ return hot_create("hotkey", control, style, rect, parent, id); }
static int32_t hot_joy_create(void *control, uint32_t style, const struct fs98_options_rect *rect, void *parent, uint32_t id)
{ return hot_create("hotjoy", control, style, rect, parent, id); }
static void hot_key_init(void *control, void *font, uint32_t flags)
{ put("hotkey.init %u %u %x\n", offset(control), offset(font), flags); mutate(); }
static void hot_joy_init(void *control, void *font, uint32_t flags)
{ put("hotjoy.init %u %u %x\n", offset(control), offset(font), flags); mutate(); }
/* the captured key or input: one of the fixture's bindings now and then, so conflicts happen */
static int32_t hot_key_get(void *control)
{ put("hotkey.get %u\n", offset(control)); mutate(); return (s_seed >> (s_calls % 8)) % 3 ? 0x11 + (int32_t)((s_seed >> 5) % 8) : 0x41; }
static uint32_t hot_joy_get(void *control)
{ put("hotjoy.get %u\n", offset(control)); mutate(); return (s_seed >> (s_calls % 8)) % 4 ? 0x11 + (s_seed >> 6) % 8 : 0; }
static void hot_key_set(void *control, int32_t key) { put("hotkey.set %u %d\n", offset(control), key); mutate(); }
static void hot_joy_set(void *control, uint32_t joystick, uint32_t sense)
{ put("hotjoy.set %u %d %u\n", offset(control), (int32_t)joystick, sense); mutate(); }
static void host_key_name(uint32_t key, char *out, uint32_t size)
{ put("host.keyname %x %u\n", key, size); mutate(); snprintf(out, size, "Key %x", key); }
static void host_joystick_name(uint32_t input, char *out, uint32_t size)
{ put("host.joyname %x %u\n", input, size); mutate(); snprintf(out, size, "Button %x", input); }
static uint32_t host_dialog(uint32_t id, void *block)
{
    uint32_t *words = block;
    put("host.dialog %u [%x %x %x %x %x] [%s]\n", id, words[0], words[1], words[2], words[4], where((void *)(uintptr_t)words[5]) == 0xffffffff ? 0 : 1,
        (const char *)(uintptr_t)words[5]);
    mutate();
    words[3] = (s_seed >> (s_calls % 8)) % 3 == 0 ? 2 : (s_seed >> (s_calls % 8)) % 3 == 1 ? 0x40 : 0;
    return s_seed;
}
static const struct fs98_options_services s_services = {
    send_message, win_help, enable_window, ddx_control, base_init, init_button, init_text,
    init_edit, base_font, set_window_text, get_window_text, set_focus, is_lead_byte,
    base_exchange, init_art, init_list, init_combo, error_dialog, get_focus, window_from_handle,
    compare_text, copy_text, post_message, enable_control, window_enabled, set_checked,
    host_strcpy, host_strcmpi, allocate_object, notify_construct, notify_free, notify_destroy,
    ui_delete, close_dialog, string_construct, string_destroy, string_assign, ddx_text, ddv_max_chars,
    show_dialog, load_string_a, init_group, set_sims_text, get_sims_text, window_text_string,
    font_construct, font_destroy, register_exit, set_tab_stops, get_client_rect, get_sims_text_buffer,
    text_length, string_append, string_append_char, list_text, format_text, list_destroy,
    dialog_item, remap_rect, move_window, get_buffer, release_buffer, string_append_string, message_box,
    paint_construct, paint_destroy, dc_mode, dc_align, get_window_rect, screen_to_client, to_int, format_int,
    get_window_long, set_radio, long_to_text, to_long, ddx_int, ddv_min_max, notify_send,
    async_key_state, tab_init, page_init, add_page, helper_init, set_last_page, set_active_tab,
    get_active_tab, slider_init, set_scroll_range, get_scroll_pos, set_scroll_pos,
    string_load, redraw_window, load_library, get_proc_address, free_library, host_joystick_count,
    set_scroll_pos_window, get_scroll_pos_window, set_scroll_range_window, get_scroll_range_window, enable_scroll_bar,
    client_to_screen, update_window, format_two, string_format, check_list_init, check_list_enable, check_list_set_check,
    check_list_get_check, hot_key_create, hot_key_init, hot_key_get, hot_key_set, hot_joy_create, hot_joy_init,
    hot_joy_get, hot_joy_set, host_key_name, host_joystick_name, host_dialog
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
    {   /* Display options' pages, each failing now and then */
        static const uint32_t sizes[] = {0x168c, 0x13a4, 0x19ac, 0x18ac};
        static const uint32_t prefs_sizes[] = {0x1c4c, 0x150c, 0x1714, 0x16ec, 0x2dbc, 0x14f8};
        uint32_t index;
        for (index = 0; index < 4; ++index)
            if (size == sizes[index] && OP.kind == FS98_OPTIONS_DISPLAY)
                return (s_seed >> (24 + index)) % 11 ? s_fixture.pages[index] : 0;
        for (index = 0; index < 6; ++index)
            if (size == prefs_sizes[index] && OP.kind == FS98_OPTIONS_PREFS)
                return (s_seed >> (23 + index)) % 11 ? s_fixture.pages[index] : 0;
        {
            static const uint32_t controls_sizes[] = {0x2584, 0x1b64, 0x168c};
            for (index = 0; index < 3; ++index)
                if (size == controls_sizes[index] && OP.kind == FS98_OPTIONS_CONTROLS)
                    return (s_seed >> (23 + index)) % 11 ? s_fixture.pages[index] : 0;
        }
    }
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
    string_assign, message_box
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
CONTROL_HOOK(radio, 0x1d6dc, 0x1d6e0)
CONTROL_HOOK(spin, 0x1d528, 0x1d524)
CONTROL_HOOK(tab, 0x1d720, 0x1d730)
CONTROL_HOOK(helper, 0x1d728, 0x1d738)
CONTROL_HOOK(slider, 0x1d794, 0x1d7a4)
CONTROL_HOOK(check_list, 0x1d7e4, 0x1d678)
CONTROL_HOOK(hot_key, 0x1d670, 0x1d680)
CONTROL_HOOK(hot_joy, 0x1d674, 0x1d668)
static void TC hook_destroy_scroll(void *control) { destroy_control(control, 0x1d4b0); }
static void TC hook_destroy_scroll_unwind(void *control) { destroy_control(control, 0x1d67c); }
static void TC hook_destroy_spin_unwind(void *control) { destroy_control(control, 0x1d6cc); }
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
    uint32_t value;
    va_start(args, format);
    value = va_arg(args, uint32_t);
    va_end(args);
    if (strchr(format, 'd')) return format_int(out, format, (int32_t)value);
    return format_text(out, format, (const char *)(uintptr_t)value);
}
static void *TC hook_dialog_item(void *object, int32_t id) { return dialog_item(object, id); }
static void SC hook_remap_rect(struct fs98_options_rect *rect, void *control) { remap_rect(rect, control); }
static int32_t TC hook_move_window(void *window, int32_t x, int32_t y, int32_t width, int32_t height, int32_t repaint)
{ move_window(window, x, y, width, height, repaint); return 1; }
static char *TC hook_get_buffer(void *string, int32_t size) { return get_buffer(string, size); }
static void TC hook_release_buffer(void *string, int32_t size) { release_buffer(string, size); }
static void *TC hook_append_string(void *string, const void *other) { string_append_string(string, other); return string; }
static int32_t SC hook_message_box(const char *text, uint32_t type, uint32_t help) { return message_box(text, type, help); }
static void *TC hook_paint_construct(void *dc, void *window) { paint_construct(dc, window); return dc; }
static void TC hook_paint_destroy(void *dc) { paint_destroy(dc); }
static int32_t TC hook_dc_mode(void *dc, int32_t mode) { return dc_mode(dc, mode); }
static uint32_t TC hook_dc_align(void *dc, uint32_t align) { return dc_align(dc, align); }
static int32_t SC hook_window_rect(uint32_t window, struct fs98_options_rect *rect) { return get_window_rect(window, rect); }
static int32_t SC hook_screen_to_client(uint32_t window, void *point) { return screen_to_client(window, point); }
static int hook_atoi(const char *text) { return to_int(text); }
static int32_t SC hook_window_long(uint32_t window, int32_t index) { return get_window_long(window, index); }
static int32_t TC hook_set_radio(void *control, int32_t checked) { return set_radio(control, checked); }
static char *hook_ltoa(long value, char *out, int radix) { return long_to_text((int32_t)value, out, radix); }
static long hook_atol(const char *text) { return to_long(text); }
static void SC hook_ddx_int(void *exchange, uint32_t id, int32_t *value) { ddx_int(exchange, id, value); }
static int hook_notify_send(void *notify, const char *title, const char *format, ...)
{
    va_list args;
    void *value;
    va_start(args, format);
    value = va_arg(args, void *);
    va_end(args);
    return notify_send(notify, title, format, value);
}
static int16_t SC hook_async_key(int32_t key) { return async_key_state(key); }
static int32_t TC hook_tab_init(void *tab, void *dialog, void *font, uint32_t bitmap, void *dib, void *palette,
                                uint32_t a, uint32_t color, int32_t x, int32_t y, int32_t width, int32_t height,
                                uint32_t module, uint32_t b)
{ return tab_init(tab, dialog, font, bitmap, dib, palette, a, color, x, y, width, height, module, b); }
static int32_t TC hook_page_init(void *page, int32_t value) { return page_init(page, value); }
static void TC hook_add_page(void *tab, void *page, char *title, int32_t value) { add_page(tab, page, title, value); }
static void TC hook_helper_init(void *helper, void *tab) { helper_init(helper, tab); }
static int32_t TC hook_set_last_page(void *tab, void *page) { return set_last_page(tab, page); }
static int32_t TC hook_set_active_tab(void *tab, int32_t index, uint32_t window) { return set_active_tab(tab, index, window); }
static void *TC hook_page_construct(void *object, int32_t resource, void *parent) { return page_construct(object, resource, parent); }
static void TC hook_page_destroy(void *object) { page_destroy(object); }
static int32_t TC hook_get_active_tab(void *tab) { return get_active_tab(tab); }
static int32_t TC hook_slider_init(void *slider, uint32_t bitmap, uint32_t thumb, void *dib, void *palette, uint32_t a,
                                   uint32_t b, uint32_t c, uint32_t d, int32_t e, int32_t f, uint32_t module)
{ return slider_init(slider, bitmap, thumb, dib, palette, a, b, c, d, e, f, module); }
static void TC hook_set_scroll_range(void *slider, int32_t low, int32_t high) { set_scroll_range(slider, low, high); }
static int32_t TC hook_get_scroll_pos(void *slider) { return get_scroll_pos(slider); }
static int32_t TC hook_set_scroll_pos(void *slider, int32_t position) { return set_scroll_pos(slider, position); }
static int32_t TC hook_string_load(void *string, uint32_t id) { return string_load(string, id); }
static int32_t SC hook_redraw(uint32_t window, const void *rect, uint32_t region, uint32_t flags)
{ return redraw_window(window, rect, region, flags); }
static uint32_t SC hook_load_library(const char *name) { return load_library(name); }
static void *SC hook_get_proc(uint32_t library, const char *name) { return get_proc_address(library, name); }
static int32_t SC hook_free_library(uint32_t library) { return free_library(library); }
static uint32_t SC hook_host_joysticks(void) { return host_joystick_count(); }
static int32_t SC hook_window_set_scroll_pos(uint32_t w, int32_t b, int32_t p, int32_t r) { return set_scroll_pos_window(w, b, p, r); }
static int32_t SC hook_window_get_scroll_pos(uint32_t w, int32_t b) { return get_scroll_pos_window(w, b); }
static int32_t SC hook_window_set_scroll_range(uint32_t w, int32_t b, int32_t l, int32_t h, int32_t r) { return set_scroll_range_window(w, b, l, h, r); }
static int32_t SC hook_get_scroll_range(uint32_t w, int32_t b, int32_t *l, int32_t *h) { return get_scroll_range_window(w, b, l, h); }
static int32_t SC hook_enable_scroll_bar(uint32_t w, uint32_t b, uint32_t a) { return enable_scroll_bar(w, b, a); }
static int32_t SC hook_client_to_screen(uint32_t w, void *p) { return client_to_screen(w, p); }
static int32_t SC hook_update_window(uint32_t w) { return update_window(w); }
static int hook_wsprintf(char *out, const char *format, ...)
{
    va_list args;
    const char *a, *b;
    va_start(args, format); a = va_arg(args, const char *); b = va_arg(args, const char *); va_end(args);
    return format_two(out, format, a, b);
}
static void hook_string_format(void *string, uint32_t id, ...)
{
    va_list args;
    uint32_t value;
    va_start(args, id); value = va_arg(args, uint32_t); va_end(args);
    string_format(string, id, value);
}
static void TC hook_check_list_init(void *l, void *f, void *t) { check_list_init(l, f, t); }
static void TC hook_check_list_enable(void *l, int32_t i, int32_t e) { check_list_enable(l, i, e); }
static void TC hook_check_list_set(void *l, int32_t i, int32_t c) { check_list_set_check(l, i, c); }
static int32_t TC hook_check_list_get(void *l, int32_t i) { return check_list_get_check(l, i); }
static int32_t TC hook_hot_key_create(void *c, uint32_t s2, const struct fs98_options_rect *r, void *p, uint32_t i) { return hot_key_create(c, s2, r, p, i); }
static int32_t TC hook_hot_joy_create(void *c, uint32_t s2, const struct fs98_options_rect *r, void *p, uint32_t i) { return hot_joy_create(c, s2, r, p, i); }
static void TC hook_hot_key_init(void *c, void *f, uint32_t x) { hot_key_init(c, f, x); }
static void TC hook_hot_joy_init(void *c, void *f, uint32_t x) { hot_joy_init(c, f, x); }
static int32_t TC hook_hot_key_get(void *c) { return hot_key_get(c); }
static uint32_t TC hook_hot_joy_get(void *c) { return hot_joy_get(c); }
static void TC hook_hot_key_set(void *c, int32_t k) { hot_key_set(c, k); }
static void TC hook_hot_joy_set(void *c, uint32_t j, uint32_t s2) { hot_joy_set(c, j, s2); }
static void SC hook_host_key_name(uint32_t k, char *o, uint32_t n) { host_key_name(k, o, n); }
static void SC hook_host_joy_name(uint32_t k, char *o, uint32_t n) { host_joystick_name(k, o, n); }
static uint32_t SC hook_host_dialog(uint32_t id, void *block) { return host_dialog(id, block); }
static void SC hook_ddv_min_max(void *exchange, int32_t value, int32_t low, int32_t high) { ddv_min_max(exchange, value, low, high); }

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
    patch(0x1d510, hook_dialog_item); patch(0x1d6c0, hook_remap_rect); patch(0x1d518, hook_move_window);
    patch(0x1d514, hook_get_buffer); patch(0x1d500, hook_release_buffer); patch(0x1d504, hook_append_string);
    patch(0x1d4a8, hook_message_box); patch(0x1d4f0, hook_paint_construct); patch(0x1d4f4, hook_paint_destroy);
    patch(0x1d508, hook_dc_mode); patch(0x1d50c, hook_dc_align); patch(0x1d81c, hook_window_rect);
    patch(0x1d820, hook_screen_to_client); patch(0x1d628, hook_atoi);
    patch(0x1d828, hook_window_long); patch(0x1d6d8, hook_set_radio);
    patch(0x1d6dc, hook_construct_radio); patch(0x1d6e0, hook_destroy_radio);
    patch(0x1d528, hook_construct_spin); patch(0x1d524, hook_destroy_spin); patch(0x1d6cc, hook_destroy_spin_unwind);
    patch(0x1d6c4, (void *)SPIN_VTABLE); fs98_options_set_spin_vtable((void *)SPIN_VTABLE);
    patch(0x1d618, hook_ltoa); patch(0x1d61c, hook_atol); patch(0x1d51c, hook_ddx_int); patch(0x1d520, hook_ddv_min_max);
    patch(0x1d684, hook_notify_send);
    patch(0x1d830, hook_async_key); patch(0x1d6f8, hook_tab_init); patch(0x1d6fc, hook_page_init);
    patch(0x1d700, hook_add_page); patch(0x1d704, hook_helper_init); patch(0x1d708, hook_set_last_page);
    patch(0x1d70c, hook_set_active_tab); patch(0x1d788, hook_page_construct); patch(0x1d798, hook_page_destroy);
    patch(0x1d720, hook_construct_tab); patch(0x1d730, hook_destroy_tab);
    patch(0x1d728, hook_construct_helper); patch(0x1d738, hook_destroy_helper);
    patch(0x1d688, hook_get_active_tab); patch(0x1d778, hook_slider_init); patch(0x1d77c, hook_set_scroll_range);
    patch(0x1d770, hook_get_scroll_pos); patch(0x1d768, hook_set_scroll_pos);
    patch(0x1d794, hook_construct_slider); patch(0x1d7a4, hook_destroy_slider);
    patch(0x1d614, (void *)GetProcAddress(crt, "_ftol"));        /* the real one: its argument is in st0 */
    patch(0x1d4b8, hook_string_load); patch(0x1d7ec, hook_redraw); patch(0x1d470, hook_load_library);
    patch(0x1d45c, hook_get_proc); patch(0x1d46c, hook_free_library);
    patch(0x1d7e4, hook_construct_check_list); patch(0x1d678, hook_destroy_check_list);
    patch(0x1d670, hook_construct_hot_key); patch(0x1d680, hook_destroy_hot_key);
    patch(0x1d674, hook_construct_hot_joy); patch(0x1d668, hook_destroy_hot_joy);
    patch(0x1d4b0, hook_destroy_scroll); patch(0x1d67c, hook_destroy_scroll_unwind);
    patch(0x1d66c, (void *)SCROLL_VTABLE); fs98_options_set_scroll_vtable((void *)SCROLL_VTABLE);
    s_host_table[0x1c0 / 4] = (void *)hook_host_joysticks;
    s_host_table[0x200 / 4] = (void *)hook_host_key_name; s_host_table[0x1d0 / 4] = (void *)hook_host_joy_name;
    s_host_table[0x10c / 4] = (void *)hook_host_dialog;
    patch(0x1d800, hook_window_set_scroll_pos); patch(0x1d80c, hook_window_get_scroll_pos); patch(0x1d7fc, hook_window_set_scroll_range);
    patch(0x1d808, hook_get_scroll_range); patch(0x1d804, hook_enable_scroll_bar); patch(0x1d7f8, hook_client_to_screen);
    patch(0x1d7f4, hook_update_window); patch(0x1d7f0, hook_wsprintf); patch(0x1d4a4, hook_string_format);
    patch(0x1d65c, hook_check_list_init); patch(0x1d478, hook_check_list_enable); patch(0x1d47c, hook_check_list_set);
    patch(0x1d4ac, hook_check_list_get); patch(0x1d660, hook_hot_key_create); patch(0x1d7dc, hook_hot_joy_create);
    patch(0x1d7e0, hook_hot_key_init); patch(0x1d664, hook_hot_joy_init); patch(0x1d650, hook_hot_key_get);
    patch(0x1d710, hook_hot_joy_get); patch(0x1d658, hook_hot_key_set); patch(0x1d654, hook_hot_joy_set);
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
    if (OP.kind == FS98_OPTIONS_LANDING || OP.kind == FS98_OPTIONS_MANOEUVRE) {
        int landing = OP.kind == FS98_OPTIONS_LANDING;
        static const char *const numbers[] = {"120", "-45", "0", "9999", "abc"};
        PTR(object, landing ? 0x10e4 : 0x10e0) = s_fixture.descriptor[0];
        if (landing) D32(object, 0x10e0) = (s_seed >> 22) & 1;     /* the caption still to make */
        else D32(object, 0x1440) = (s_seed >> 22) & 1;             /* the scale's label made */
        D32(object, 0x10dc) = s_seed ^ 0x10dc;
        D32(s_fixture.graph, 0x20) = s_seed ^ 0x6a;
        PTR(s_fixture.dc, 0) = s_dc_vtable;
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            PTR(block, 8) = graph_control;
            D32(block, 0xc) = (s_seed >> (5 + index)) & 1;
            if (landing) {
                PTR(block, 0x10) = s_fixture.texts[index];
                PTR(block, 0x14) = (void *)numbers[(s_seed >> (7 + index)) % 5];
                PTR(block, 0x18) = (void *)numbers[(s_seed >> (10 + index)) % 5];
                PTR(block, 0x1c) = s_fixture.texts[2 + index];
            } else {
                D32(block, 0x10) = (s_seed >> (7 + index)) & 1;
                PTR(block, 0x14) = s_fixture.texts[index];
            }
        }
    }
    if (OP.kind == FS98_OPTIONS_REPLAY) {
        PTR(object, 0x10e0) = s_fixture.descriptor[0];
        PTR(object, 0x10dc) = (s_seed >> 22) % 4 ? (void *)s_fixture.notify : 0;
        PTR(object, 0x1774) = s_control_vtable;
        D32(object, 0x1830) = (s_seed >> 3) & 1;
        D32(object, 0x1a34) = (s_seed >> 4) % 120;
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            PTR(block, 4) = part_check;
            D32(block, 8) = (s_seed >> (5 + index)) % 4 == 0;          /* a replay running */
            D32(block, 0xc) = 20 + (s_seed >> 8) % 40;
            D32(block, 0x10) = (s_seed >> (10 + index)) % 3 == 0 ? 5 + index : 0;
            D32(block, 0x14) = (s_seed >> 12) % 300 + ((s_seed >> 25) & 1 ? 0x10000 : 0);   /* past 16 bits now and then */
            D32(block, 0x2c) = (s_seed >> (14 + index)) % 3 == 0 ? 6 : 0;
            D32(block, 0x30) = (s_seed >> 16) % 150 + ((s_seed >> 26) & 1 ? 0x30000 : 0);
            D32(block, 0x4c) = (s_seed >> (18 + index)) % 3;
        }
    }
    if (OP.kind == FS98_OPTIONS_DEMO) {      /* MAIN's block: the pick and the demos */
        static const int32_t counts[] = {0, 1, 3, 5, -1};
        PTR(object, 0x10e0) = s_fixture.descriptor[0];
        PTR(object, 0x10dc) = (s_seed >> 22) % 4 ? (void *)s_fixture.notify : 0;
        D32(object, 0x1554) = s_seed ^ 0x1554;
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            D32(block, 0x10) = (uint32_t)((int32_t)((s_seed >> (5 + index)) % 7) - 1);   /* -1..5 */
            D32(block, 0x14) = (uint32_t)counts[(s_scenario + index) % 5];
            PTR(block, 0x18) = s_fixture.records[index];
        }
    }
    if (OP.kind >= FS98_OPTIONS_DISPLAY && OP.kind <= FS98_OPTIONS_DISPLAY_HARDWARE) {
        /* each page's check boxes, the parts' words, MAIN's driver block and its lists */
        static const uint32_t checks[][11] = {
            {0},
            {0x11dc, 0x12a4, 0x136c, 0x1434, 0x14fc, 0x15c4},
            {0x12dc},
            {0x11dc, 0x12a4, 0x136c, 0x1434, 0x14fc, 0x15c4, 0x168c, 0x1754, 0x181c, 0x18e4},
            {0x13dc, 0x15a0, 0x1668, 0x1730},
        };
        static const uint32_t parts[] = {0xc, 0x38, 0x4c, 0xa0};
        uint32_t page, list, at;
        for (list = 0; list < 2; ++list) {
            for (at = 0; at < 7; ++at) s_fixture.names[list][at] = s_fixture.strings[(at + list) % 8];
            s_fixture.names[list][(s_seed >> (10 + list)) % 7] = 0;     /* the list's end */
        }
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            D32(block, 8) = (s_seed >> (5 + index)) % 5;               /* the tab shown first */
            for (page = 0; page < 4; ++page) D32(block, parts[page]) = (s_seed >> (page + index)) & 1;
            D32(block, 0xac) = (s_seed >> 13) % 3;
            if (OP.kind == FS98_OPTIONS_DISPLAY_HARDWARE || OP.kind == FS98_OPTIONS_DISPLAY) {
                uint8_t *driver = OP.kind == FS98_OPTIONS_DISPLAY ? block + 0xa0 : block;
                static const uint32_t rooms[] = {0, 1, 3, 7, 0x80000001};
                PTR(driver, 0) = driver_list; PTR(driver, 4) = driver_modes; PTR(driver, 8) = driver_refresh;
                D32(driver, 0x10) = (s_seed >> (14 + index)) & 1;
                D32(driver, 0x14) = s_seed & 0xff;
                D32(driver, 0x18) = rooms[(s_scenario + index) % 5];
                PTR(driver, 0x1c) = s_fixture.names[0];
                D32(driver, 0x20) = (s_seed >> 16) % 5;
                D32(driver, 0x24) = rooms[(s_scenario + index + 2) % 5];
                PTR(driver, 0x28) = s_fixture.names[1];
                D32(driver, 0x2c) = (s_seed >> 19) % 5;
                D32(driver, 0x30) = (s_seed >> (21 + index)) & 1;
            }
        }
        if (OP.kind == FS98_OPTIONS_DISPLAY) {
            PTR(object, 0x12dc) = s_other_vtable;                       /* the tab control's +0xc4 */
            PTR(object, 0x21ac) = (s_seed >> 22) % 4 ? (void *)s_fixture.notify : 0;
            for (page = 0; page < 4; ++page) {
                uint8_t *at_page = s_fixture.pages[page];
                PTR(object, 0x21b0 + page * 4) = at_page;
                PTR(at_page, 0x1118) = s_fixture.descriptor[0] + parts[page];
                D32(at_page, 0x1100) = (s_seed >> (9 + page)) % 4 != 0;
            }
        } else {
            uint32_t kind = OP.kind - FS98_OPTIONS_DISPLAY;
            D32(object, 0x1100) = (s_seed >> 9) % 4 != 0;               /* the page shown */
            for (at = 0; at < 11 && checks[kind][at]; ++at) {     /* each box off, on, or another true */
                PTR(object, checks[kind][at]) = s_control_vtable;
                D32(object, checks[kind][at] + 0xbc) = (s_seed >> (at + 3)) % 3;
            }
            D32(object, 0x12b4) = s_seed ^ 0x12b4;
            D32(object, 0x13b4) = s_seed ^ 0x13b4;
        }
    }
    if (OP.kind >= FS98_OPTIONS_PREFS && OP.kind <= FS98_OPTIONS_PREFS_FLIGHT) {
        /* the pages' boxes, MAIN's callbacks, the presets, the volumes */
        static const uint32_t checks[][8] = {
            {0},
            {0x1128, 0x15a4, 0x1864, 0x192c, 0x19f4, 0x1abc, 0x1b84},
            {0x1124, 0x11ec, 0x12b4, 0x137c, 0x1444},
            {0x164c},
            {0x1128},
            {0x1354, 0x189c, 0x1de4, 0x232c, 0x2874},
            {0x11d8, 0x12a0, 0x1368, 0x1430},
        };
        /* 0.29 times 100 truncates to 28 in extended precision, to 29 rounded to double first */
        static const double volumes[] = {0.0, 0.5, -0.25, 1.0, -1.0, 0.333, 0.999, -0.001, 1e10, __builtin_nan(""),
                                         0.29, -0.57};
        static const uint32_t defaults[] = {0, 0xcdcdcdcd, 1, 1};
        static const int32_t counts[] = {0, 1, 3, 5, -1};
        static const uint32_t pages[] = {0x2338, 0x233c, 0x2340, 0x2344, 0x2348, 0x234c};
        uint32_t page, at, kind = OP.kind - FS98_OPTIONS_PREFS;
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            static const uint32_t statuses[] = {0x34, 0x3c, 0x44, 0x4c, 0x54, 0x5c, 0x64};
            D32(block, 8) = (s_seed >> (5 + index)) % 7;
            at = defaults[(s_seed >> (12 + index)) % 4];
            D32(block, 0xc) = at == 1 ? (uint32_t)(uintptr_t)block_callback_0 : at;
            PTR(block, 0x14) = block_callback_1; PTR(block, 0x18) = block_callback_2;   /* General's buttons */
            for (at = 0; at < 7; ++at)       /* General's statuses, each set one time in three */
                D32(block, statuses[at]) = (s_seed >> (at * 2 + index)) % 3 == 0 ? 0x1000 + at : 0;
            PTR(block, 0xc4) = block_callback_3; PTR(block, 0xd4) = block_callback_4;   /* Display's */
            PTR(block, 0xc8) = s_fixture.records[0];
            D32(block, 0xcc) = (uint32_t)counts[(s_scenario + index) % 5];
            D32(block, 0xb4) = (s_seed >> (16 + index)) % 3;          /* International's codes */
            D32(block, 0xa8) = (s_seed >> (18 + index)) % 4 + 1;
            D32(block, 0xb0) = (s_seed >> (20 + index)) & 1;
            for (at = 0; at < 5; ++at)
                *(double *)(block + 0xfc + at * 8) = volumes[(s_seed >> (at * 3 + index)) % 12];
        }
        if (OP.kind == FS98_OPTIONS_PREFS) {
            PTR(object, 0x13d4) = s_other_vtable;
            PTR(object, 0x2334) = (s_seed >> 22) % 4 ? (void *)s_fixture.notify : 0;
            for (page = 0; page < 6; ++page) {
                static const uint32_t parts[] = {0x10, 0x6c, 0x98, 0xc0, 0xf8, 0x124};
                uint8_t *at_page = s_fixture.pages[page];
                PTR(object, pages[page]) = at_page;
                PTR(at_page, page == 4 ? 0x111c : 0x1118) = s_fixture.descriptor[0] + parts[page];
                PTR(at_page, page == 4 ? 0x1120 : 0x111c) = s_fixture.descriptor[0];
                D32(at_page, 0x1100) = (s_seed >> (9 + page)) % 4 != 0;
            }
        } else {
            D32(object, 0x1100) = (s_seed >> 9) % 4 != 0;
            if (OP.kind == FS98_OPTIONS_PREFS_GENERAL) {
                PTR(object, 0x1118) = s_fixture.descriptor[0] + 0x10;
                PTR(object, 0x111c) = s_fixture.descriptor[0];
            }
            if (OP.kind == FS98_OPTIONS_PREFS_DISPLAY) {
                PTR(object, 0x1118) = s_fixture.descriptor[0] + 0xc0;
                PTR(object, 0x111c) = s_fixture.descriptor[0];
            }
            if (OP.kind == FS98_OPTIONS_PREFS_INTERNATIONAL) PTR(object, 0x1118) = s_fixture.descriptor[0] + 0x98;
            if (OP.kind == FS98_OPTIONS_PREFS_SOUND) PTR(object, 0x111c) = s_fixture.descriptor[0] + 0xf8;
            for (at = 0; at < 8 && checks[kind][at]; ++at) {
                PTR(object, checks[kind][at]) = s_control_vtable;
                D32(object, checks[kind][at] + 0xbc) = (s_seed >> (at + 3)) % 3;     /* off, on, or another true */
            }
        }
    }
    if (OP.kind >= FS98_OPTIONS_CONTROLS) {
        /* CONTROLS' block: two command lists with their devices; the mouse and keyboard axes */
        static const uint32_t ids[2][8] = {
            {0x1004b, 0x10050, 0x10200, 0x1009a, 0x1004d, 0x10048, 0x100cd, 0x10098},   /* one of slew's too */
            {0x100cd, 0x100d1, 0x10200, 0x100d2, 0x100cf, 0x100cb, 0x10301, 0x100d0}
        };
        static const uint32_t counts[] = {0, 2, 5, 8};
        static const uint32_t callbacks[] = {0, 0xcdcdcdcd, 1, 1};
        uint32_t list, at, device;
        for (list = 0; list < 2; ++list) {
            for (at = 0; at < 8; ++at) {
                s_fixture.commands[list][at][0] = (uint32_t)(uintptr_t)s_fixture.strings[at];
                s_fixture.commands[list][at][1] = ids[list][(at + (s_seed >> 3)) % 8];
            }
            for (device = 0; device < 6; ++device) {
                uint8_t *entry = s_fixture.devices[list][device];
                D32(entry, 0) = device == 0 ? 0 : (s_seed >> (device + list)) % 4 ? 1 : 2;
                PTR(entry, 8) = s_fixture.texts[device % 4];
                PTR(entry, 0xc) = s_fixture.bindings[list][device];
                PTR(entry, 0x10) = s_fixture.settings[list][device];
                for (at = 0; at < 8; ++at) {     /* bound or not, an axis (2) or a button */
                    s_fixture.bindings[list][device][at * 2] = (s_seed >> ((at + device) % 16)) % 3 ? 0x11 + at : 0;
                    s_fixture.bindings[list][device][at * 2 + 1] =      /* the keyboard's mostly keys (0) */
                        device == 0 ? (uint32_t[]){0, 0, 1, 2}[(s_seed >> ((at * 3) % 16)) % 4] : (s_seed >> ((at * 3 + device) % 16)) % 3 ? 2 : 1;
                }
                for (at = 0; at < 24; ++at)
                    s_fixture.settings[list][device][at] = (uint32_t)((int32_t)((s_seed >> (at % 16)) % 255) - 127);
            }
        }
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            D32(block, 8) = (s_seed >> (5 + index)) % 4;
            D32(block, 0x10) = (s_seed >> (6 + index)) & 1;                 /* no joystick */
            D32(block, 0x18) = (s_seed >> (7 + index)) & 1;
            at = callbacks[(s_seed >> (8 + index)) % 4];
            D32(block, 0x1c) = at == 1 ? (uint32_t)(uintptr_t)controls_reset : at;
            at = callbacks[(s_seed >> (10 + index)) % 4];
            D32(block, 0x20) = at == 1 ? (uint32_t)(uintptr_t)controls_recount : at;
            D32(block, 0x28) = (s_seed >> (12 + index)) & 1;
            PTR(block, 0x2c) = controls_reset_device;
            for (list = 0; list < 2; ++list) {
                D32(block, 0x30 + list * 0x10) = counts[(s_scenario + list + index) % 4];
                PTR(block, 0x34 + list * 0x10) = s_fixture.commands[list];
                D32(block, 0x38 + list * 0x10) = (s_scenario / 4 + list) % 5;
                PTR(block, 0x3c + list * 0x10) = s_fixture.devices[list];
            }
            PTR(block, 0x54) = s_fixture.texts[3];
            for (at = 0; at < 3; ++at) {
                PTR(block, 0x58 + at * 0x10) = s_fixture.strings[at];
                D32(block, 0x60 + at * 0x10) = (s_seed >> (14 + at)) & 1;
                PTR(block, 0x90 + at * 0x10) = s_fixture.strings[at + 3];
                PTR(block, 0x94 + at * 0x10) = s_fixture.strings[at + 5];
                D32(block, 0x98 + at * 0x10) = (s_seed >> (17 + at)) & 1;
            }
        }
        if (OP.kind == FS98_OPTIONS_CONTROLS) {
            static const uint32_t parts[] = {0xc, 0x24, 0xc0};
            PTR(object, 0x12d8) = s_other_vtable;
            for (at = 0; at < 3; ++at) {
                PTR(object, 0x2164 + at * 4) = s_fixture.pages[at];
                PTR(s_fixture.pages[at], 0x1118) = s_fixture.descriptor[0] + parts[at];
                PTR(s_fixture.pages[at], 0x111c) = s_fixture.descriptor[0];
            }
        } else {
            static const uint32_t checks[] = {0x11dc, 0x23a4, 0x1128, 0x11f0, 0x12b8, 0x1380, 0x1448, 0x1510};
            for (at = 0; at < 8; ++at) {
                PTR(object, checks[at]) = s_control_vtable;
                D32(object, checks[at] + 0xbc) = (s_seed >> (at + 3)) % 3;
            }
            if (OP.kind == FS98_OPTIONS_CONTROLS_KEYS) {
                PTR(object, 0x1118) = (uint8_t *)PTR(object, 0x111c) + 0x24;
                PTR(object, 0x1b54) = (uint8_t *)PTR(object, 0x111c) + ((s_seed >> 3) & 1 ? 0x40 : 0x30);
                D32(object, 0x1b50) = (s_seed >> 4) % 4;
                D32(object, 0x1b60) = 1 + (s_seed >> 6) % 4;
                D32(object, 0x1b58) = (s_seed >> 8) % 5;
                D32(object, 0x1b5c) = (s_seed >> 10) % 5;
                PTR(object, 0x1408) = s_control_vtable; PTR(object, 0x14fc) = s_control_vtable;
            }
            if (OP.kind == FS98_OPTIONS_CONTROLS_SENSITIVITY) {
                PTR(object, 0x1118) = (uint8_t *)PTR(object, 0x111c) + 0xc;
                D32(object, 0x2568) = (s_seed >> 3) % 6;                     /* the device */
                D32(object, 0x256c) = (s_seed >> 6) % 6;                     /* the mouse's row */
                D32(object, 0x2570) = (s_seed >> 9) % 3;                     /* the keyboard's */
                D32(object, 0x2574) = (s_seed >> 11) % 6;
                D32(object, 0x2578) = (s_seed >> 13) % 3;
                D32(object, 0x257c) = (s_seed >> 15) % 4;     /* 0..2 as the page sets it, or past */
                D32(object, 0x2580) = (s_seed >> 17) % 4;
                if ((s_seed >> 19) % 3 == 0) D32(object, 0x2568) = D32(object, 0x256c);
                else if ((s_seed >> 19) % 3 == 1) D32(object, 0x2568) = D32(object, 0x2570);
            }
        }
    }
    if (OP.kind == FS98_OPTIONS_ANALYSIS) {
        static const uint32_t controls[] = {0x1834, 0x14bc, 0x13c8, 0x1678, 0x15b0, 0x176c};
        static const uint32_t windows[] = {0x1854, 0x14dc, 0x13e8, 0x1698, 0x15d0, 0x178c, 0x1358, 0x13a0};
        static const uint32_t statuses[] = {8, 0x10, 0x18, 0x28, 0x30, 0x38};
        for (index = 0; index < 6; ++index) PTR(object, controls[index]) = s_control_vtable;
        for (index = 0; index < 8; ++index) D32(object, windows[index]) = (s_seed ^ (index * 0x111)) & 0xffff0;
        D32(object, 0x166c) = (s_seed >> 3) & 1;
        D32(object, 0x1828) = (s_seed >> 4) & 1;
        for (index = 0; index < 2; ++index) {
            uint8_t *block = s_fixture.descriptor[index];
            uint32_t field;
            PTR(block, 4) = part_check;
            for (field = 0; field < 6; ++field)      /* each status set one time in three, from its own bits */
                D32(block, statuses[field]) = (s_seed >> (field * 2 + index)) % 3 == 0 ? 0x1000 + field : 0;
            if ((s_seed >> 29) % 4 == 0) for (field = 0; field < 6; ++field) D32(block, statuses[field]) = 0;
            D32(block, 0xc) = (s_seed >> (14 + index)) & 1;
            D32(block, 0x14) = (uint32_t)((int32_t)((s_seed >> (16 + index)) % 5) - 1);   /* -1..3 */
            D32(block, 0x1c) = (s_seed >> 19) & 1;
            D32(block, 0x2c) = (s_seed >> 20) & 1;
            D32(block, 0x34) = (s_seed >> 21) % 4;
            D32(block, 0x3c) = (s_seed >> 23) % 4;
        }
    }
    /* the handlers that test for no descriptor; the graphs' also for MSVC's debug fill */
    if ((OP.rva == 0x7c40 || OP.rva == 0x8030 || OP.rva == 0xaf20 || OP.rva == 0xb0b0 ||
         OP.rva == 0xc3b0 || OP.rva == 0xc7c0) && OP.kind < FS98_OPTIONS_DISPLAY && (s_seed >> 27) % 4 == 1)
        PTR(object, descriptor_offset()) = (void *)(uintptr_t)0xcdcdcdcd;
    if ((OP.rva == 0xe400 || OP.rva == 0xe1d0 || OP.rva == 0xddf0 || OP.rva == 0xd930 ||
         OP.rva == 0xd4d0 || OP.rva == 0xcc20 || OP.rva == 0x8550 || OP.rva == 0x95e0 || OP.rva == 0x7c40 ||
         OP.rva == 0x8030 || OP.rva == 0xaf20 || OP.rva == 0xb0b0 || OP.rva == 0x4e70 || OP.rva == 0x57e0 ||
         OP.rva == 0x6df0 || OP.rva == 0x72d0 || OP.rva == 0xc3b0 || OP.rva == 0xc7c0 ||
         OP.rva == 0xc660 || OP.rva == 0xc690 || (OP.rva == 0x3ea0 && OP.kind == FS98_OPTIONS_DISPLAY) ||
         (OP.rva == 0xb9d0 && OP.kind == FS98_OPTIONS_PREFS) || (OP.rva == 0xe790 && OP.kind == FS98_OPTIONS_CONTROLS)) &&
        (s_seed >> 27) % 4 == 0) {
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
        case SIG_INT_PAIR:      /* an axis 0..4, a list 0, 1 or 2 */
            result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, (s_seed >> 3) % 5, (s_seed >> 6) % 3);
            break;
        case SIG_INT_SMALL:
            result = ((int32_t (TC *)(void *, uint32_t))entry)(object, (s_seed >> 3) % 6);
            break;
        case SIG_VOID_SMALL:
            ((void (TC *)(void *, uint32_t))entry)(object, (s_seed >> 3) % 5);
            break;
        case SIG_BAR: {         /* a scroll code, a position, and the bar or another */
            void *bars[] = {s_fixture.object + 0x1a0c, s_fixture.object + 0x1a4c, s_fixture.info};
            ((void (TC *)(void *, uint32_t, uint32_t, void *))entry)(object, (s_seed >> 3) % 9, (s_seed >> 7) % 6,
                                                                      bars[(s_seed >> 11) % 3]);
            break;
        }
        case SIG_DRAW:
            result = ((int32_t (TC *)(void *, uint32_t, uint32_t))entry)(object, s_seed, s_seed ^ 3);
            break;
        case SIG_SCALE:
            ((void (TC *)(void *, void *))entry)(object, s_fixture.dc);
            break;
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
    if (OP.kind == FS98_OPTIONS_REPLAY) {      /* OPT-8: each side's own stack, by address */
        uint32_t index;
        for (index = 0; index < 2; ++index) {
            static const uint32_t at[] = {0x1c, 0x38};
            uint32_t field;
            for (field = 0; field < 2; ++field) {
                uint8_t *text = PTR(s_fixture.descriptor[index], at[field]);
                if (where(text) == 0xffffffff && (uintptr_t)text > 0x10000 && (uintptr_t)text < 0x10000000 &&
                    (uintptr_t)text != (uintptr_t)s_initial.descriptor[index][at[field]])
                    PTR(s_fixture.descriptor[index], at[field]) = (void *)(uintptr_t)0x5ac50000;
            }
        }
    }
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
    s_vtable[0xe8 / 4] = virtual_frame; s_other_vtable[0xe8 / 4] = replaced_frame;
    s_vtable[0xd8 / 4] = s_other_vtable[0xd8 / 4] = virtual_validate;
    s_vtable[0xdc / 4] = s_other_vtable[0xdc / 4] = virtual_error;
    s_vtable[0xe0 / 4] = s_other_vtable[0xe0 / 4] = virtual_focus;
    for (slot = 0; slot < 64; ++slot) s_control_vtable[slot] = (void *)s_other_slots[slot];
    s_control_vtable[0xc4 / 4] = virtual_check;
    for (slot = 0; slot < 64; ++slot) s_dc_vtable[slot] = (void *)s_replaced_slots[slot];
    s_dc_vtable[0x38 / 4] = dc_text_color; s_dc_vtable[0x64 / 4] = dc_text_out;
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
